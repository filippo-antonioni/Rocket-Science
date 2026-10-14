#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <ICM42670P.h>
#include <LittleFS.h>
#include <ESP32Servo.h>
#include "flight_core.h"

// ================= CONFIGURAZIONE DA CONTROLLARE =================
constexpr int SDA_PIN = 8;
constexpr int SCL_PIN = 9;
constexpr int SERVO_PIN = 18; // GPIO, NON numero fisico del piedino sul connettore
constexpr int SERVO_CHIUSO = 130;
constexpr int SERVO_APERTO = 90;
constexpr int SERVO_MIN_US = 500;
constexpr int SERVO_MAX_US = 2400;
constexpr int ASSE_VERTICALE = 2; // 0=X, 1=Y, 2=Z; allineato all'asse del razzo
constexpr float SEGNO_ASSE = +1.0f; // a riposo sulla rampa deve dare circa +9.81

enum Modalita { BANCO, TEST_SERVO, VOLO };
constexpr Modalita MODALITA = BANCO; // come nell'originale: default solo acquisizione
// BANCO: CSV grezzi a 8 colonne; servo scollegato dal PWM.
// TEST_SERVO: soltanto prova manuale A/C, non richiede i sensori.
// VOLO: filtro + macchina a stati + servo + log esteso su LittleFS.

constexpr double ACC_LSB_PER_G = 2048.0;
constexpr uint32_t PERIODO_IMU_US = 10000;
constexpr uint32_t PERIODO_BARO_US = 26316; // obiettivo ~38 Hz
constexpr int N_CALIBRAZIONE = 50;
constexpr int RIGHE_PER_FLUSH = 25;
static_assert(ASSE_VERTICALE >= 0 && ASSE_VERTICALE < 3, "Asse IMU non valido");
static_assert(SERVO_PIN != SDA_PIN && SERVO_PIN != SCL_PIN, "Conflitto servo/I2C");

Adafruit_BMP280 bmp;
ICM42670 IMU(Wire, 0);
Servo servoParacadute;
volo::Parametri parametri;
volo::Kalman kalman;
volo::MacchinaStati macchina;
File fileLog;
bool pronto = false, servoComandatoAperto = false;
double altitudineZero = 0;
uint32_t ultimoGiroUs = 0;
int righeDaFlush = 0;
uint32_t cicliInRitardo = 0;

// Il BMP280 in NORMAL non espone un contatore dei campioni. Per avere un
// fresh reale avviamo UNA conversione FORCED e la consumiamo UNA volta.
// Non usiamo takeForcedMeasurement(): attende la conversione bloccando il loop.
uint8_t indirizzoBaro = 0;
bool baroInCorso = false, baroNuovo = false;
uint32_t inizioBaroUs = 0, ultimoAvvioBaroUs = 0, ultimoPollBaroUs = 0;
double pressionePa = NAN, quotaBaro = NAN;

bool leggiRegistro(uint8_t addr, uint8_t reg, uint8_t& valore) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(addr, uint8_t(1)) != 1) return false;
    valore = Wire.read();
    return true;
}
bool scriviRegistro(uint8_t addr, uint8_t reg, uint8_t valore) {
    Wire.beginTransmission(addr);
    Wire.write(reg); Wire.write(valore);
    return Wire.endTransmission() == 0;
}
double quotaDaPressione(double pressione) {
    if (!std::isfinite(pressione) || pressione < 30000 || pressione > 110000)
        return NAN;
    return 44330.0 * (1.0 - std::pow(pressione / 101325.0, 0.190295));
}
void aggiornaBarometro() {
    const uint32_t ora = micros();
    if (!baroInCorso) {
        if (uint32_t(ora - ultimoAvvioBaroUs) < PERIODO_BARO_US) return;
        ultimoAvvioBaroUs = ora;
        // ctrl_meas: temp x1 (001), pressione x8 (100), forced (01).
        if (scriviRegistro(indirizzoBaro, 0xF4, 0x31)) {
            baroInCorso = true;
            inizioBaroUs = micros();
            ultimoPollBaroUs = inizioBaroUs;
        }
        return;
    }
    const uint32_t attesa = uint32_t(ora - inizioBaroUs);
    if (attesa > 100000) { baroInCorso = false; return; } // conversione guasta
    if (attesa < 23000 || uint32_t(ora - ultimoPollBaroUs) < 1000) return;
    ultimoPollBaroUs = ora;
    uint8_t stato;
    if (!leggiRegistro(indirizzoBaro, 0xF3, stato)) { baroInCorso = false; return; }
    if (stato & 0x09) return; // measuring o copia coefficienti ancora in corso
    const double pressione = bmp.readPressure();
    // Controllo di presenza aggiuntivo: la libreria Adafruit non espone
    // direttamente l'esito delle transazioni interne di readPressure().
    uint8_t chip;
    const bool presente = leggiRegistro(indirizzoBaro, 0xD0, chip) && chip == 0x58;
    const double h = presente ? quotaDaPressione(pressione) : NAN;
    baroInCorso = false;
    if (std::isfinite(h)) {
        pressionePa = pressione;
        quotaBaro = h - altitudineZero;
        baroNuovo = true;
    }
}

bool inizializzaServo() {
    ESP32PWM::allocateTimer(0); // un solo servo: basta un timer
    servoParacadute.setPeriodHertz(50);
    servoParacadute.attach(SERVO_PIN, SERVO_MIN_US, SERVO_MAX_US);
    if (!servoParacadute.attached()) return false;
    servoParacadute.write(SERVO_CHIUSO);
    return true;
}
void apriParacadute() {
    if (servoComandatoAperto) return;
    servoParacadute.write(SERVO_APERTO);
    servoComandatoAperto = true;
    // Non detach e non richiudere: il PWM mantiene la posizione aperta.
    // Questo flag prova l'invio del comando, NON l'apertura meccanica.
}
void testServo() {
    while (Serial.available()) {
        const char c = Serial.read();
        if (c == 'A' || c == 'a') {
            apriParacadute(); Serial.println("Servo: comando APERTO");
        } else if (c == 'C' || c == 'c') {
            servoParacadute.write(SERVO_CHIUSO);
            servoComandatoAperto = false;
            Serial.println("Servo: comando CHIUSO");
        }
    }
}
void erroreAvvio(const char* testo) {
    Serial.println(testo);
    pronto = false; // loop non deve partire con setup incompleto
}
void apriLog() {
    if (!LittleFS.begin(false)) { // NON formattare e cancellare log in automatico
        Serial.println("LittleFS non montata: algoritmi attivi, log disabilitato");
        return;
    }
    char nome[32];
    for (unsigned n = 0; n < 10000; ++n) {
        snprintf(nome, sizeof(nome), "/volo_%04u.csv", n);
        if (!LittleFS.exists(nome)) {
            fileLog = LittleFS.open(nome, FILE_WRITE);
            break;
        }
    }
    if (!fileLog) { Serial.println("Impossibile creare log"); return; }
    Serial.printf("Log: %s\n", nome);
    fileLog.println("tempo_ms,pressione_pa,ax_raw,ay_raw,az_raw,gx_raw,gy_raw,gz_raw,imu_ok,baro_fresh,dt_s,h_baro,h_kalman,v_kalman,bias,fase,deploy,servo_comandato,motivo,correzione,cicli_ritardo");
    fileLog.flush();
}

void setup() {
    Serial.begin(115200);
    delay(MODALITA == VOLO ? 200 : 4000);
    if (MODALITA == TEST_SERVO) {
        pronto = inizializzaServo();
        Serial.println(pronto ? "Test servo: A apre, C chiude" : "ERRORE servo");
        return;
    }
    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(100000);
    Wire.setTimeOut(10); // limita l'attesa per singola transazione I2C
    if (bmp.begin(0x76)) indirizzoBaro = 0x76;
    else if (bmp.begin(0x77)) indirizzoBaro = 0x77;
    else { erroreAvvio("ERRORE: BMP280 non trovato"); return; }
    // begin() puo' aver avviato NORMAL: prima entra in sleep e lascia finire
    // la misura. Il registro config deve essere scritto con sensore inattivo.
    if (!scriviRegistro(indirizzoBaro, 0xF4, 0x00)) {
        erroreAvvio("ERRORE configurazione BMP280"); return;
    }
    delay(30);
    // Sleep per configurare; aggiornaBarometro avvia poi le conversioni.
    bmp.setSampling(Adafruit_BMP280::MODE_SLEEP,
                    Adafruit_BMP280::SAMPLING_X1,
                    Adafruit_BMP280::SAMPLING_X8,
                    Adafruit_BMP280::FILTER_X4,
                    Adafruit_BMP280::STANDBY_MS_1);
    if (IMU.begin() != 0 || IMU.startAccel(100, 16) != 0 || IMU.startGyro(100, 2000) != 0) {
        erroreAvvio("ERRORE inizializzazione ICM42670P"); return;
    }
    Serial.println("Calibrazione: mantieni fermo il razzo sulla rampa");
    // Consuma campioni distinti, scartando i primi 40 per l'assestamento IIR.
    double somma = 0;
    int campioni = 0;
    const uint32_t inizio = millis();
    ultimoAvvioBaroUs = micros() - PERIODO_BARO_US;
    while (campioni < N_CALIBRAZIONE + 40 && uint32_t(millis() - inizio) < 10000) {
        aggiornaBarometro();
        if (baroNuovo) {
            baroNuovo = false;
            if (campioni >= 40) somma += quotaBaro;
            ++campioni;
        }
        delay(1);
    }
    if (campioni < N_CALIBRAZIONE + 40) {
        erroreAvvio("ERRORE: campioni barometro insufficienti per lo zero"); return;
    }
    altitudineZero = somma / N_CALIBRAZIONE;
    quotaBaro -= altitudineZero;
    Serial.printf("Zero altimetro: %.3f m\n", altitudineZero);
    if (MODALITA == VOLO) {
        if (!inizializzaServo()) { erroreAvvio("ERRORE servo"); return; }
        apriLog();
        Serial.println("VOLO attivo: macchina PAD, servo chiuso");
    } else {
        Serial.println("tempo_ms,pressione_pa,ax_raw,ay_raw,az_raw,gx_raw,gy_raw,gz_raw");
    }
    ultimoGiroUs = micros();
    pronto = true;
}

void loop() {
    if (!pronto) { delay(1); return; }
    if (MODALITA == TEST_SERVO) { testServo(); return; }
    const uint32_t oraUs = micros();
    const uint32_t trascorsi = uint32_t(oraUs - ultimoGiroUs);
    if (trascorsi < PERIODO_IMU_US) {
        aggiornaBarometro(); // conversione indipendente dal passo del filtro
        return;
    }
    ultimoGiroUs = oraUs;
    const uint32_t oraMs = millis();
    const double dt = trascorsi * 1e-6;
    if (trascorsi > 15000) ++cicliInRitardo;
    // Niente cicli di recupero con lo stesso campione: integra il dt reale.
    inv_imu_sensor_event_t dati = {};
    for (int i = 0; i < 3; ++i) dati.accel[i] = dati.gyro[i] = INT16_MIN;
    const int imuStatus = IMU.getDataFromRegisters(dati);
    const bool imuOk = imuStatus == 0 && dati.accel[ASSE_VERTICALE] != INT16_MIN;
    const double forza = imuOk
        ? SEGNO_ASSE * dati.accel[ASSE_VERTICALE] / ACC_LSB_PER_G * parametri.g : NAN;
    // Se il driver non riempie l'evento rimangono sentinel: mai dati non inizializzati.
    const bool fresh = baroNuovo;
    baroNuovo = false;

    if (MODALITA == BANCO) {
        Serial.printf("%lu,%.2f,%d,%d,%d,%d,%d,%d\n", (unsigned long)oraMs,
                      pressionePa, dati.accel[0], dati.accel[1], dati.accel[2],
                      dati.gyro[0], dati.gyro[1], dati.gyro[2]);
        aggiornaBarometro();
        return;
    }
    const volo::Correzione correzione = kalman.step(forza, quotaBaro, fresh, dt, parametri);
    const volo::Eventi eventi = macchina.step(kalman.x[1], forza, kalman.x[0], oraMs, parametri);
    if (eventi.apertura) apriParacadute(); // PRIMA della scrittura su flash
    aggiornaBarometro(); // viene servito anche se il loop non ha tempo libero

    if (fileLog) {
        const size_t scritti = fileLog.printf(
            "%lu,%.2f,%d,%d,%d,%d,%d,%d,%u,%u,%.6f,%.3f,%.3f,%.3f,%.5f,%s,%u,%u,%u,%u,%lu\n",
            (unsigned long)oraMs, pressionePa,
            dati.accel[0], dati.accel[1], dati.accel[2],
            dati.gyro[0], dati.gyro[1], dati.gyro[2],
            unsigned(imuOk), unsigned(fresh), dt, quotaBaro,
            kalman.x[0], kalman.x[1], kalman.x[2], volo::nomeFase(macchina.fase),
            unsigned(macchina.deploy), unsigned(servoComandatoAperto),
            unsigned(macchina.motivo), unsigned(correzione), (unsigned long)cicliInRitardo);
        if (scritti == 0) {
            fileLog.close(); // l'algoritmo continua anche senza log
        } else if (++righeDaFlush >= RIGHE_PER_FLUSH || eventi.apertura || eventi.atterraggio) {
            fileLog.flush(); righeDaFlush = 0;
        }
        if (eventi.atterraggio) fileLog.close();
    }
}
