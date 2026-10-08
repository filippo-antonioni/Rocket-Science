// =====================================================================
//  avionica.ino v2 - sensori -> Kalman -> FSM -> servo + logger
//  ESP32-S3-WROOM-1 N16R8 | BMP280 | ICM-42670-P
//  Core 1: task di controllo (priorita' alta). Core 0: task writer del logger.
//  Librerie: Adafruit BMP280, TDK ICM42670P, ESP32Servo (+ SD se LOG_BACKEND_SD).
// =====================================================================
#include <Arduino.h>
#include "config.h"          // PRIMA di tutto: le macro (LOG_BACKEND_SD, IMU_INT_PIN...) servono ai #if sotto
#include <Wire.h>
#include <esp_task_wdt.h>
#include <Adafruit_BMP280.h>
#include "ICM42670P.h"
#if LOG_BACKEND_SD
  #include <SD.h>
  #include <SPI.h>
#else
  #include <LittleFS.h>
#endif

#include "flight_core.h"
#include "flight_logger.h"
#include "flight_persist.h"
#include "parachute_servo.h"

static FlightCore      core;
static FlightLogger    logger;
static ParachuteServo  servo;
static Adafruit_BMP280 bmp(&Wire);
static ICM42670        imu(Wire, ICM_ADDR_LSB);
static bool imu_ok = false, bmp_ok = false;

struct RawImu { float acc; float gyr[3]; };

static RawImu convertImu(const inv_imu_sensor_event_t& ev) {
    RawImu o = {NAN, {NAN, NAN, NAN}};
    const int16_t a = ev.accel[cfg::IMU_ASSE_VERT];
    if (a != -32768) o.acc = cfg::IMU_SEGNO * ((float)a / cfg::IMU_ACC_LSB_PER_G) * cfg::G;   // -32768 = invalido
    for (int k = 0; k < 3; k++)
        if (ev.gyro[k] != -32768) o.gyr[k] = (float)ev.gyro[k] / cfg::IMU_GYR_LSB_PER_DPS;
    return o;
}

#if IMU_INT_PIN >= 0
// ---- acquisizione via FIFO + interrupt: un campione per ogni dato prodotto dal sensore, nessun aliasing ----
static TaskHandle_t g_ctrl = nullptr;
static RawImu  g_fifo[cfg::IMU_BATCH_MAX];
static uint8_t g_fifo_n = 0;

static void IRAM_ATTR imuIsr() {
    BaseType_t woken = pdFALSE;
    if (g_ctrl) vTaskNotifyGiveFromISR(g_ctrl, &woken);
    if (woken) portYIELD_FROM_ISR();
}
static void fifoCb(inv_imu_sensor_event_t* ev) {      // chiamata in modo sincrono da getDataFromFifo()
    if (g_fifo_n < cfg::IMU_BATCH_MAX) g_fifo[g_fifo_n++] = convertImu(*ev);
}
#endif

static void controlTask(void*) {
    esp_task_wdt_add(nullptr);          // se il loop si blocca: reset -> recupero dello stato (vedi flight_persist.h)
#if IMU_INT_PIN >= 0
    g_ctrl = xTaskGetCurrentTaskHandle();
#endif
    TickType_t last_tick = xTaskGetTickCount();
    uint32_t last_us = micros(), last_baro_us = micros();
    RawImu batch[cfg::IMU_BATCH_MAX];

    for (;;) {
        esp_task_wdt_reset();
        uint8_t n = 0;
        float dt = cfg::KF_DT;

#if IMU_INT_PIN >= 0
        const bool got = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(2 * cfg::IMU_PERIOD_MS)) > 0;
        const uint32_t now_us = micros();
        if (got && imu_ok) {
            g_fifo_n = 0;
            imu.getDataFromFifo(fifoCb);
            n = g_fifo_n;
            for (uint8_t i = 0; i < n; i++) batch[i] = g_fifo[i];
            dt = cfg::KF_DT;                                 // base tempi del sensore: ODR esatto
        }
        if (n == 0) {                                        // timeout o FIFO vuota: IMU assente, il filtro prosegue col baro
            batch[0] = RawImu{NAN, {NAN, NAN, NAN}};
            n = 1;
            dt = (now_us - last_us) * 1e-6f;
        }
        last_us = now_us;
#else
        vTaskDelayUntil(&last_tick, pdMS_TO_TICKS(cfg::IMU_PERIOD_MS));
        const uint32_t now_us = micros();
        dt = (now_us - last_us) * 1e-6f;                     // dt reale (il core lo limita a 5..100 ms)
        last_us = now_us;
        batch[0] = RawImu{NAN, {NAN, NAN, NAN}};
        if (imu_ok) {
            inv_imu_sensor_event_t ev;
            if (imu.getDataFromRegisters(ev) == 0) batch[0] = convertImu(ev);
        }
        n = 1;
#endif
        (void)last_tick;

        // Barometro: a tempo (38 Hz), UNA lettura per ciclo e il dato "nuovo" va all'ultimo campione del batch
        float baro_pa = NAN;
        bool baro_fresh = false;
        if (bmp_ok) {
            const uint32_t b = micros();
            if ((int32_t)(b - last_baro_us) >= (int32_t)cfg::BARO_PERIOD_US) {
                last_baro_us += cfg::BARO_PERIOD_US;
                if ((int32_t)(b - last_baro_us) >= (int32_t)cfg::BARO_PERIOD_US) last_baro_us = b;  // resync dopo uno stallo
                baro_pa = bmp.readPressure();                // NaN se la lettura fallisce
                baro_fresh = true;
            }
        }

        const uint32_t t_now = millis();
        for (uint8_t i = 0; i < n; i++) {
            const bool last = (i == n - 1);
            const uint32_t t_ms = t_now - (uint32_t)(n - 1 - i) * cfg::IMU_PERIOD_MS;

            SensorData d;
            d.acc_ms2 = batch[i].acc;
            for (int k = 0; k < 3; k++) d.gyr_dps[k] = batch[i].gyr[k];
            d.dt_s = dt;
            if (last) { d.baro_pa = baro_pa; d.baro_fresh = baro_fresh; }

            const FlightStatus& s = core.update(d, t_ms);

            if (s.deploy) servo.deploy();                    // 1) attuazione: priorita' assoluta (idempotente)

            if (s.ev_boost) logger.start();                  // 2) log
            LogRecord r;
            r.t_ms = t_ms;
            r.p_pa = d.baro_pa;  r.baro_h = s.baro_h;  r.acc = d.acc_ms2;
            for (int k = 0; k < 3; k++) r.gyr[k] = d.gyr_dps[k];
            r.h_kal = s.h;  r.v_kal = s.v;  r.bias_kal = s.bias;
            r.fase = (uint8_t)s.fase;
            r.flags = (d.baro_fresh ? 1 : 0) | (isfinite(d.acc_ms2) ? 0 : 2) | (s.deploy ? 4 : 0);
            logger.push(r);
            if (s.ev_landed) { logger.stop(); persistInvalidate(); }
        }

        if (core.status().fase != Fase::LANDED) persistSave(core.snapshot(t_now));   // RAM RTC: costo trascurabile
    }
}

void setup() {
    // 0) Recupero: se il reset e' avvenuto in volo NON si deve richiudere il servo ne' ricalibrare il suolo
    FlightSnapshot snap;
    const bool recover = persistLoadForRecovery(snap);
    const bool in_flight = recover && snap.fase >= (uint8_t)Fase::BOOST;

    servo.begin(recover && snap.deploy);          // armato a 135 (o gia' a 90 se era stato comandato)
    Serial.begin(115200);

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.setClock(400000);
    Wire.setTimeOut(5);                           // un I2C bloccato non deve fermare il loop oltre 5 ms

    imu_ok = (imu.begin() == 0);
    if (imu_ok) {
#if IMU_INT_PIN >= 0
        // DA VERIFICARE con la tua versione della libreria TDK: segui l'ordine/firma dell'esempio FIFO
        imu_ok = (imu.enableFifoInterrupt(IMU_INT_PIN, imuIsr, 1) == 0);
#endif
        imu_ok = imu_ok && (imu.startAccel(cfg::IMU_FREQ_INT, 16) == 0) && (imu.startGyro(100, 2000) == 0);
    }
    bmp_ok = bmp.begin(BMP280_I2C_ADDR);
    if (bmp_ok)
        bmp.setSampling(Adafruit_BMP280::MODE_NORMAL, Adafruit_BMP280::SAMPLING_X1,
                        Adafruit_BMP280::SAMPLING_X8, Adafruit_BMP280::FILTER_X4,
                        Adafruit_BMP280::STANDBY_MS_1);

#if LOG_BACKEND_SD
    SPI.begin(SD_PIN_SCK, SD_PIN_MISO, SD_PIN_MOSI, SD_PIN_CS);
    const bool fs_ok = SD.begin(SD_PIN_CS, SPI, 20000000) && logger.begin(SD, in_flight);
#else
    const bool fs_ok = LittleFS.begin(true) && logger.begin(LittleFS, in_flight);
#endif
    if (fs_ok && in_flight) logger.start();       // il log riprende subito in append sullo stesso file

    core.init();
    if (recover) core.restore(snap);
    Serial.printf("IMU:%d BARO:%d FS:%d RECOVER:%d fase:%d file:%s\n", imu_ok, bmp_ok, fs_ok, recover,
                  recover ? snap.fase : 0, fs_ok ? logger.path() : "-");

    xTaskCreatePinnedToCore(controlTask, "ctrl", 8192, nullptr, 5, nullptr, 1);
}

void loop() {
#if DEBUG_SERIAL
    // Lettura non sincronizzata: solo diagnostica.
    const FlightStatus& s = core.status();
    Serial.printf("fase=%u cal=%d h=%.1f v=%.2f bias=%.3f deploy=%d log=%d drop=%lu\n",
                  (unsigned)s.fase, s.ground_ready, s.h, s.v, s.bias, s.deploy,
                  logger.active(), (unsigned long)logger.dropped());
#endif
    vTaskDelay(pdMS_TO_TICKS(1000));
}
