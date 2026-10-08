#pragma once
// =====================================================================
//  config.h - parametri avionica (traduzione di parameters.m) - v2
//  Le voci marcate "DA COMPLETARE" vanno impostate/verificate da te.
// =====================================================================
#include <stdint.h>

// ---------------- HARDWARE (macro, cosi' il preprocessore le controlla) ----------------
#define SERVO_PIN        (-1)   // DA COMPLETARE: GPIO servo (evita 26-32 flash, 35-37 PSRAM ottale, 0/3/45/46 strapping)
#define PIN_I2C_SDA      8      // DA COMPLETARE
#define PIN_I2C_SCL      9      // DA COMPLETARE
#define BMP280_I2C_ADDR  0x76   // DA COMPLETARE: 0x76 (SDO a GND) oppure 0x77
#define ICM_ADDR_LSB     0      // DA COMPLETARE: 0 -> 0x68, 1 -> 0x69
#define IMU_INT_PIN      (-1)   // DA COMPLETARE: GPIO collegato a INT1 dell'ICM-42670-P.
                                //  >=0 : acquisizione via FIFO + interrupt (consigliata, nessun aliasing/campione perso)
                                //  -1  : polling a 100 Hz con dt misurato (degradata: puo' ripetere/saltare campioni)
#define DEBUG_SERIAL     1

// Backend di log. 0 = LittleFS (flash interna), 1 = SD su SPI.
// ATTENZIONE: ogni scrittura/cancellazione sulla flash interna ferma la cache e quindi anche il task
// di controllo (decine di ms). La SD non ha questo problema: e' la scelta consigliata per il volo.
#define LOG_BACKEND_SD   0
#define SD_PIN_CS        (-1)   // DA COMPLETARE (solo se LOG_BACKEND_SD=1)
#define SD_PIN_SCK       (-1)   // DA COMPLETARE
#define SD_PIN_MISO      (-1)   // DA COMPLETARE
#define SD_PIN_MOSI      (-1)   // DA COMPLETARE

// Recupero dopo reset in volo (brownout / watchdog / panic). 0 = disabilitato.
#define FLIGHT_RECOVERY  1

namespace cfg {

// ---------------- Ambiente ----------------
constexpr float G                 = 9.80665f;
constexpr float LUNGHEZZA_RAMPA_M = 1.5f;

// ---------------- IMU ICM-42670-P ----------------
constexpr uint32_t IMU_FREQ_INT        = 100;
constexpr float    IMU_FREQ_HZ         = 100.0f;
constexpr uint32_t IMU_PERIOD_MS       = 1000 / IMU_FREQ_INT;
constexpr float    IMU_ACC_LSB_PER_G   = 2048.0f;     // +-16 g
constexpr float    IMU_GYR_LSB_PER_DPS = 16.4f;       // 2000 dps
constexpr float    IMU_RUMORE_ACC      = 0.012f;
constexpr float    IMU_SAT_MS2         = 0.98f * 16.0f * G;  // oltre = saturato/glitch -> NaN
constexpr bool     IMU_MEDIAN3         = true;        // mediana su 3 campioni: elimina glitch singoli (+1 campione di ritardo)
constexpr uint8_t  IMU_BATCH_MAX       = 16;          // campioni FIFO elaborabili per ciclo
// DA COMPLETARE: asse IMU parallelo all'asse del razzo e verso (+g a razzo fermo in verticale)
constexpr uint8_t  IMU_ASSE_VERT       = 2;
constexpr float    IMU_SEGNO           = 1.0f;

// ---------------- Barometro BMP280 ----------------
constexpr uint32_t BARO_FREQ_INT         = 38;
constexpr uint32_t BARO_PERIOD_US        = 1000000UL / BARO_FREQ_INT;
constexpr float    BARO_RUMORE_M         = 2.97f * 0.083f;
constexpr float    BARO_FATTORE_DISTURBI = 10.0f;
constexpr uint16_t BARO_N_CAL            = 200;
constexpr float    BARO_CAL_MAX_RANGE_PA = 40.0f;     // se in calibrazione max-min supera questo: razzo non fermo, si riparte
constexpr float    BARO_P_MIN_PA         = 30000.0f;
constexpr float    BARO_P_MAX_PA         = 120000.0f;

// ---------------- Kalman ----------------
constexpr float   KF_DT       = 1.0f / IMU_FREQ_HZ;   // dt nominale (il dt reale viene passato dal chiamante)
constexpr float   KF_P0_H     = 0.5f * 0.5f;          // DA VALIDARE
constexpr float   KF_P0_V     = 0.1f * 0.1f;          // DA VALIDARE
constexpr float   KF_P0_B     = 0.4f * 0.4f;          // DA VALIDARE
constexpr float   KF_Q_BIAS   = 1e-4f;
constexpr uint8_t KF_SCARTI   = 5;

// ---------------- Macchina a stati (P.logica) ----------------
constexpr float    SOGLIA_DECOLLO       = 30.0f;
constexpr uint8_t  N_CONFERME           = 5;
constexpr float    QUOTA_BARO_DECOLLO_M = 18.0f;
constexpr float    QUOTA_MIN_DEPLOY_M   = 30.0f;
constexpr uint32_t T_MIN_DEPLOY_MS      = 1800;
constexpr uint32_t T_MAX_DEPLOY_MS      = 8000;
constexpr float    LANDED_V_MAX         = 0.4f;
constexpr float    LANDED_H_MAX         = 10.0f;

// ---------------- Recupero dopo reset in volo ----------------
constexpr float    RECOVERY_P0_V        = 30.0f * 30.0f;  // incertezza velocita' dopo il riavvio
constexpr uint32_t RECOVERY_SETTLE_MS   = 1500;           // dopo il riavvio il rilevamento apogeo/burnout resta in attesa che il filtro converga

// ---------------- Servo paracadute ----------------
constexpr int SERVO_ANGOLO_ARMATO = 135;
constexpr int SERVO_ANGOLO_APERTO = 90;
constexpr int SERVO_US_MIN        = 500;    // DA COMPLETARE
constexpr int SERVO_US_MAX        = 2400;   // DA COMPLETARE

// ---------------- Logger ----------------
constexpr uint32_t LOG_RING_N           = 1024;   // POTENZA DI 2
constexpr uint32_t LOG_PREROLL_N        = 50;
constexpr uint32_t LOG_CHUNK_B          = 2048;
constexpr uint32_t LOG_LINE_B           = 160;
constexpr uint32_t LOG_FLUSH_MS         = 1000;
constexpr uint32_t LOG_WRITER_PERIOD_MS = 20;

} // namespace cfg
