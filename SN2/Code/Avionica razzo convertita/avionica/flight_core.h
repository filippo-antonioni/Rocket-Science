#pragma once
// =====================================================================
//  SEZIONE 1 - Core v2: calibrazione baro + filtro glitch IMU + Kalman (dt variabile) + FSM
//  + salvataggio/ripristino dello stato per i reset in volo. Nessuna dipendenza Arduino.
// =====================================================================
#include <stdint.h>
#include <math.h>
#include "config.h"

enum class Fase : uint8_t { PAD = 0, BOOST, COAST, DESCENT, LANDED };

struct SensorData {
    float acc_ms2    = NAN;               // forza specifica asse verticale [m/s^2], +g a riposo; NaN = non valida
    float gyr_dps[3] = {NAN, NAN, NAN};   // solo per il log
    float baro_pa    = NAN;               // [Pa]; NaN = non valida
    bool  baro_fresh = false;             // true solo con dato NUOVO
    float dt_s       = 0.0f;              // tempo trascorso dal campione IMU precedente; <=0 -> nominale
};

struct FlightStatus {
    Fase  fase = Fase::PAD;
    float h = 0, v = 0, bias = 0;
    float baro_h = NAN;
    bool  ground_ready = false;
    bool  deploy = false;                 // latch: paracadute comandato
    bool  ev_boost = false;               // impulsi a un solo campione
    bool  ev_deploy = false;
    bool  ev_landed = false;
};

// Istantanea dello stato per il recupero dopo reset (salvata in RTC RAM dal chiamante, vedi flight_persist.h)
struct FlightSnapshot {
    uint32_t magic;
    float    p_ground;
    float    bias;
    uint32_t elapsed_ms;      // tempo dal BOOST (valido se fase != PAD)
    uint8_t  fase;
    uint8_t  deploy;
    uint8_t  valid_ground;
    uint8_t  pad_;
    uint32_t crc;
};
constexpr uint32_t SNAPSHOT_MAGIC = 0xF11647A5u;

class FlightCore {
public:
    void init();
    void restore(const FlightSnapshot& s);                 // dopo init(), solo in caso di recupero
    FlightSnapshot snapshot(uint32_t t_ms) const;          // magic/crc li imposta il livello persistenza
    const FlightStatus& update(const SensorData& data, uint32_t timestamp_ms);
    const FlightStatus& status() const { return st_; }

private:
    float filterAcc(float a);
    void kalmanStep(float acc, float baro_h, bool fresh, float dt);
    void fsmStep(float v, float f, float h, uint32_t t_ms);

    float   x_[3];
    float   P_[3][3];
    float   acc_prec_;
    uint8_t n_scarti_;
    bool    reseed_;               // dopo un riavvio: la quota si riaggancia al primo dato baro

    float   win_[3];               // finestra del filtro mediano
    uint8_t win_n_;

    Fase     fase_;
    uint8_t  count_, count_baro_;
    uint32_t t_start_ms_;
    float    x_prev_[3];
    bool     deploy_;

    bool     restore_pending_;
    uint32_t restore_elapsed_;
    bool     settling_;            // attesa convergenza dopo un riavvio in volo
    uint32_t settle_until_;

    double   cal_sum_;
    uint16_t cal_n_;
    float    cal_min_, cal_max_;
    bool     ground_ready_;
    float    p_ground_;

    FlightStatus st_;
};
