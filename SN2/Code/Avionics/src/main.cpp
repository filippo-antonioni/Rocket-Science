#include <Arduino.h>
#include <Wire.h>             // per I2C
#include <Adafruit_BMP280.h>  // per barometro, correggi con 380/390 eventualmente
#include "ICM42670P.h"        // accelerometro
#include <LittleFS.h>         // per memoria esp

// ------------------------- PIN -------------------------
#define SDA_PIN 8
#define SCL_PIN 9
// #define PIN_MOSFET 4  // FUTURO: gate dell'IRLZ44N per l'accenditore.
//                          Scegli un GPIO che sta LOW durante il boot (NON usare
//                          gli strapping pin 0/3/45/46!) e metti una resistenza
//                          fisica di pull-down ~10k tra gate e GND: il pin non
//                          deve MAI poter andare alto da solo all'accensione.

// ---------------------- COSTANTI ----------------------
#define P0_HPA 1013.25  // pressione riferimento livello mare [hPa], come readAltitude vuole

// >>> RICORDA: CONVERSIONE UNITA' IMU <<<
// getDataFromRegisters() restituisce numeri GREZZI (LSB, interi), NON m/s^2!
// Con fondoscala +/-16 g  -> 2048 LSB per ogni g      (32768 / 16)
// Con fondoscala +/-2000 dps -> 16.4 LSB per ogni dps (32768 / 2000)
// Se cambi fondoscala in startAccel()/startGyro() DEVI aggiornare questi numeri.
constexpr float ACC_LSB_PER_G    = 2048.0f;
constexpr float GYRO_LSB_PER_DPS = 16.4f;
constexpr float G_MS2            = 9.80665f;

constexpr uint32_t PERIODO_LOOP_MS = 10;  // 10 ms = loop a 100 Hz (prima con delay(50) eri a ~20 Hz)
constexpr int      RIGHE_PER_FLUSH = 25;  // ogni quante righe forzare la scrittura su flash
constexpr int      N_CALIBRAZIONE  = 50;  // letture baro da mediare per lo zero

Adafruit_BMP280 bmp;      // eventualmente cambia con 380/390
ICM42670 IMU(Wire, 0);    // 0 indica l'indirizzo I2C standard 0x68

float altitudineZero = 0;
// ===== MODALITA' DI FUNZIONAMENTO =====
// BANCO: prove da scrivania -> logga i grezzi sul PC (seriale), NESSUN calcolo di volo.
// VOLO : missione -> logga i grezzi sulla flash (scatola nera) E gira l'algoritmo
//        in tempo reale che decide quando aprire il paracadute.
enum Modalita { BANCO, VOLO };
const Modalita MODALITA = BANCO;   // <<< METTI VOLO PRIMA DEL LANCIO
File  fileLog;            // tenuto APERTO per tutto il volo
int   righeDaFlush = 0;

void setup() {
    Serial.begin(115200);
    // Pausa lunga per permettere alla USB nativa del PC di ricollegarsi e al monitor Seriale di aprirsi
    delay(4000);

    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(100000);   // 100 kHz (standard)
    Serial.println("Inizializzazione...");
    
    Serial.println("Scanning I2C bus...");
    byte error, address;
    int nDevices = 0;
    for(address = 1; address < 127; address++ ) {
        Wire.beginTransmission(address);
        error = Wire.endTransmission();
        if (error == 0) {
            Serial.print("I2C device found at address 0x");
            if (address < 16) Serial.print("0");
            Serial.println(address, HEX);
            nDevices++;
        }
    }
    if (nDevices == 0) Serial.println("No I2C devices found\n");
    else Serial.println("done\n");
    
    // Check BMP280 chip ID directly
    Wire.beginTransmission(0x76);
    if(Wire.endTransmission() == 0) {
        Wire.beginTransmission(0x76);
        Wire.write(0xD0);
        Wire.endTransmission(false);
        Wire.requestFrom(0x76, 1);
        if(Wire.available()) {
            Serial.printf("Chip ID at 0x76: 0x%02X\n", Wire.read());
        }
    }
    Wire.beginTransmission(0x77);
    if(Wire.endTransmission() == 0) {
        Wire.beginTransmission(0x77);
        Wire.write(0xD0);
        Wire.endTransmission(false);
        Wire.requestFrom(0x77, 1);
        if(Wire.available()) {
            Serial.printf("Chip ID at 0x77: 0x%02X\n", Wire.read());
        }
    }
    // Loop di test continuo per l'IMU, così puoi toccare/sistemare i cavi a caldo
    Serial.println("\n--- TEST CONNESSIONE IMU ICM-42670-P ---");
    Serial.println("Se vedi 'Errore di lettura', controlla i cavi CS (deve andare a + 3.3V) e SDO/AD0 (deve andare a GND).");
    while (true) {
        Wire.beginTransmission(0x68);
        Wire.write(0x75); // Registro WHO_AM_I
        byte err = Wire.endTransmission(false); 
        
        if (err == 0) {
            byte rx = Wire.requestFrom(0x68, 1);
            if (rx == 1) {
                byte whoami = Wire.read();
                Serial.printf("SUCCESS! WHO_AM_I a 0x68: 0x%02X\n", whoami);
                if (whoami == 0x67) {
                    Serial.println("Sensore ICM-42670-P riconosciuto correttamente!");
                    break; // Esci dal loop e continua l'avvio
                } else {
                    Serial.println("Attenzione: Il chip ha risposto, ma il WHO_AM_I non e' 0x67. Forse e' un MPU6050?");
                    break;
                }
            } else {
                Serial.println("Errore: Il sensore rifiuta la lettura (NACK sul Read). AD0/SDO o CS potrebbero essere scollegati/fluttuanti.");
            }
        } else {
            Serial.printf("Errore: Nessuna risposta all'indirizzo 0x68 (codice %d). Il sensore e' scollegato o in modalita' SPI.\n", err);
        }
        delay(1500); // Aspetta prima di riprovare
    }
    Serial.println("--- FINE TEST IMU ---\n");


    if (!LittleFS.begin(true)) {
        Serial.println("Errore: Memoria Flash non montata!");
        return;
    }

    // --- SETUP BMP280 ---
    if (!bmp.begin(0x76) && !bmp.begin(0x77)) {
        Serial.println("ERRORE: BMP280 non trovato!");
        while (1) delay(10);
    }

    // Oversampling e filtro ridotti rispetto a prima (era tutto X16):
    // X16 liscia tanto ma rende il sensore LENTO e in ritardo. Per un razzo
    // meglio dati piu' rapidi e un filtro software (Kalman) a valle.
    bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                    Adafruit_BMP280::SAMPLING_X1,   // Temp: basta poco
                    Adafruit_BMP280::SAMPLING_X8,   // Pressione: compromesso rumore/velocita'
                    Adafruit_BMP280::FILTER_X4,     // Filtro IIR leggero = poco ritardo
                    Adafruit_BMP280::STANDBY_MS_1);

    int imu_status = IMU.begin();
    if (imu_status != 0) {
        Serial.printf("ERRORE: ICM-42670-P non trovato! (Codice: %d)\n", imu_status);
        while (1) delay(10);
    }

    IMU.startAccel(100, 16);    // 100 Hz, fondoscala 16 g  -> ACC_LSB_PER_G = 2048
    IMU.startGyro(100, 2000);   // 100 Hz, fondoscala 2000 dps -> GYRO_LSB_PER_DPS = 16.4
    Serial.println("IMU ICM-42670-P: OK (fondoscala 16 g)");

    // --- CALIBRAZIONE ZERO ---
    // Una lettura singola e' rumorosa (+/-0.5 m): meglio la media di tante.
    Serial.println("Calibrazione altimetro in corso...");
    delay(1000);  // tempo per far assestare il sensore

    float somma = 0;
    for (int i = 0; i < N_CALIBRAZIONE; i++) {
        somma += bmp.readAltitude(P0_HPA);
        delay(20);
    }
    altitudineZero = somma / N_CALIBRAZIONE;
    Serial.printf("Quota terra (media di %d letture): %.2f m\n", N_CALIBRAZIONE, altitudineZero);

    // --- FILE DI LOG (solo in VOLO, per non usurare la flash sul banco) ---
    if (MODALITA == VOLO) {
        fileLog = LittleFS.open("/volo.csv", FILE_WRITE);
        if (fileLog) {
            fileLog.println("tempo_ms,pressione_pa,ax_raw,ay_raw,az_raw,gx_raw,gy_raw,gz_raw");
        } else {
            Serial.println("ERRORE: impossibile creare /volo.csv");
        }
    }
    
    // Intestazione CSV anche su Seriale per il PC
    Serial.println("tempo_ms,pressione_pa,ax_raw,ay_raw,az_raw,gx_raw,gy_raw,gz_raw");
}

void loop() {
    // Cadenza fissa a 100 Hz senza delay bloccante
    static uint32_t prossimoGiro = 0;
    uint32_t ora = millis();
    if (ora < prossimoGiro) return;
    prossimoGiro = ora + PERIODO_LOOP_MS;

    // --- LETTURA IMU (valori GREZZI, nessuna conversione: si fa in MATLAB) ---
    inv_imu_sensor_event_t datiVolo;
    IMU.getDataFromRegisters(datiVolo);

    int16_t ax = datiVolo.accel[0];   // LSB grezzi accelerometro
    int16_t ay = datiVolo.accel[1];
    int16_t az = datiVolo.accel[2];
    int16_t gx = datiVolo.gyro[0];    // LSB grezzi giroscopio
    int16_t gy = datiVolo.gyro[1];
    int16_t gz = datiVolo.gyro[2];

    // --- LETTURA BAROMETRO ---
    // Pressione grezza in Pa (uscita nativa): la quota la calcoliamo in MATLAB
    float pressione = bmp.readPressure();

    // --- LOGGING DEI GREZZI (in ENTRAMBE le modalita': e' la "scatola nera") ---
    if (MODALITA == BANCO) {
        // Banco: al PC via seriale, per l'analisi in MATLAB
        Serial.printf("%lu,%.2f,%d,%d,%d,%d,%d,%d\n",
                      ora, pressione, ax, ay, az, gx, gy, gz);
    } else { // VOLO
        // Volo: sulla flash a bordo (nessun PC collegato)
        if (fileLog) {
            fileLog.printf("%lu,%.2f,%d,%d,%d,%d,%d,%d\n",
                           ora, pressione, ax, ay, az, gx, gy, gz);
            if (++righeDaFlush >= RIGHE_PER_FLUSH) { fileLog.flush(); righeDaFlush = 0; }
        }
    }

    // ============================================================
    //  ALGORITMO DI VOLO — gira SOLO in modalita' VOLO, in tempo reale.
    //  In volo NON puoi "elaborare dopo": la decisione di aprire il
    //  paracadute va presa live, campione per campione.
    // ============================================================
    if (MODALITA == VOLO) {
        // 1) Converti i grezzi in unita' fisiche (servono all'algoritmo)
        float accZ_ms2 = az / ACC_LSB_PER_G * G_MS2;
        float quota_m  = 44330.0f * (1.0f - powf(pressione / 101325.0f, 0.190295f));

        // 2) >>> QUI VA IL TUO CODICE DI VOLO (tradotto dal simulatore MATLAB) <<<
        //    - kalman_step(...)     -> stima quota e velocita'
        //    - macchina_stati(...)  -> PAD / BOOST / COAST / DISCESA
        //    - all'apogeo:          digitalWrite(PIN_MOSFET, HIGH); // apre il paracadute
        (void)accZ_ms2;   // placeholder finche' il blocco e' vuoto
        (void)quota_m;
    }
                  // ora è in ms occhio conversione in matlab, cifre significative per acc e baro prese da scheda tecnica pdf 
                  // per acc S = 2048LSB/g quindi 1LSB = 1/2048 g = 9.80665/2048 m/s^2 = 0.00478515625 m/s^2 quindi 3 cifre decimali sono sufficienti
                  // per baro S=0.16 Pa, 1 Pa =0.083 m quindi risoluzione 0.16 * 0.083 = 1.3 cm quindi 2  cifre 
                  // gyro 16.4 LSB/dps quindi 1LSB = 0.061 dps quindi 2 cifre 
                
}
