#include "flight_core.h"
#include <cmath>
#include <string.h>

namespace {
constexpr float S2 = cfg::IMU_RUMORE_ACC * cfg::IMU_RUMORE_ACC;
constexpr float R  = (cfg::BARO_RUMORE_M * cfg::BARO_FATTORE_DISTURBI) *
                     (cfg::BARO_RUMORE_M * cfg::BARO_FATTORE_DISTURBI);
}

void FlightCore::init() {
    memset(x_, 0, sizeof x_);
    memset(P_, 0, sizeof P_);
    P_[0][0] = cfg::KF_P0_H;
    P_[1][1] = cfg::KF_P0_V;
    P_[2][2] = cfg::KF_P0_B;
    acc_prec_ = cfg::G;
    n_scarti_ = 0;
    reseed_ = false;

    memset(win_, 0, sizeof win_);
    win_n_ = 0;

    fase_ = Fase::PAD;
    count_ = count_baro_ = 0;
    t_start_ms_ = 0;
    memset(x_prev_, 0, sizeof x_prev_);
    deploy_ = false;

    restore_pending_ = false;
    restore_elapsed_ = 0;
    settling_ = false;
    settle_until_ = 0;

    cal_sum_ = 0.0;
    cal_n_ = 0;
    cal_min_ = cal_max_ = 0.f;
    ground_ready_ = false;
    p_ground_ = 101325.0f;

    st_ = FlightStatus();
}

// ---------------------------------------------------------------- recupero dopo reset
void FlightCore::restore(const FlightSnapshot& s) {
    if (s.valid_ground && s.p_ground > cfg::BARO_P_MIN_PA && s.p_ground < cfg::BARO_P_MAX_PA) {
        p_ground_ = s.p_ground;          // NON ricalibrare: in volo la pressione non e' quella del suolo
        ground_ready_ = true;
    }
    if (std::isfinite(s.bias)) { x_[2] = s.bias; P_[2][2] = 0.01f; }

    if (s.fase > (uint8_t)Fase::LANDED) return;
    fase_ = (Fase)s.fase;
    deploy_ = s.deploy != 0;

    if (fase_ != Fase::PAD) {
        restore_pending_ = true;          // t_start viene ricostruito al primo update
        restore_elapsed_ = s.elapsed_ms;
        settling_ = true;
        reseed_ = true;                   // h dal primo dato baro, v incognita con grande incertezza
        P_[1][1] = cfg::RECOVERY_P0_V;
    }
}

FlightSnapshot FlightCore::snapshot(uint32_t t_ms) const {
    FlightSnapshot s;
    memset(&s, 0, sizeof s);
    s.p_ground = p_ground_;
    s.valid_ground = ground_ready_ ? 1 : 0;
    s.bias = x_[2];
    s.fase = (uint8_t)fase_;
    s.deploy = deploy_ ? 1 : 0;
    s.elapsed_ms = (fase_ != Fase::PAD) ? (t_ms - t_start_ms_) : 0;
    return s;
}

// ---------------------------------------------------------------- filtro glitch IMU
float FlightCore::filterAcc(float a) {
    if (!std::isfinite(a) || fabsf(a) >= cfg::IMU_SAT_MS2) return NAN;   // invalido: non entra nella finestra
    if (!cfg::IMU_MEDIAN3) return a;
    win_[2] = win_[1]; win_[1] = win_[0]; win_[0] = a;
    if (win_n_ < 3) { win_n_++; return a; }
    const float x = win_[0], y = win_[1], z = win_[2];
    return fmaxf(fminf(x, y), fminf(fmaxf(x, y), z));                    // mediana di 3
}

// ---------------------------------------------------------------- Kalman (kalman.m, dt variabile)
void FlightCore::kalmanStep(float acc, float baro_h, bool fresh, float dt) {
    if (std::isfinite(acc)) acc_prec_ = acc; else acc = acc_prec_;

    const float ba = 0.5f * dt * dt, bv = dt;
    const float F[3][3] = {{1.f, dt, -ba}, {0.f, 1.f, -dt}, {0.f, 0.f, 1.f}};
    const float Q[3][3] = {{ba * ba * S2, ba * bv * S2, 0.f},
                           {ba * bv * S2, bv * bv * S2, 0.f},
                           {0.f, 0.f, cfg::KF_Q_BIAS}};

    const float u = acc - cfg::G;
    const float x0 = x_[0], x1 = x_[1], x2 = x_[2];
    x_[0] = x0 + dt * x1 - ba * x2 + ba * u;
    x_[1] = x1 - dt * x2 + bv * u;

    float FP[3][3], Pn[3][3];
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) {
            float s = 0;
            for (int k = 0; k < 3; k++) s += F[i][k] * P_[k][j];
            FP[i][j] = s;
        }
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) {
            float s = 0;
            for (int k = 0; k < 3; k++) s += FP[i][k] * F[j][k];
            Pn[i][j] = s + Q[i][j];
        }
    memcpy(P_, Pn, sizeof P_);

    if (fresh && std::isfinite(baro_h)) {
        if (reseed_) {                    // riavvio in volo: aggancio la quota al baro, niente gating
            x_[0] = baro_h;
            P_[0][0] = R;
            P_[0][1] = P_[1][0] = P_[0][2] = P_[2][0] = 0.f;
            reseed_ = false;
            fresh = false;
        }
    }
    if (fresh && std::isfinite(baro_h)) {
        const float y = baro_h - x_[0];
        const float S = P_[0][0] + R;
        if (fabsf(y) > 4.0f * sqrtf(S) && n_scarti_ < cfg::KF_SCARTI) {
            n_scarti_++;
        } else {
            n_scarti_ = 0;
            float K[3], row0[3];
            for (int i = 0; i < 3; i++) { K[i] = P_[i][0] / S; row0[i] = P_[0][i]; }
            for (int i = 0; i < 3; i++) x_[i] += K[i] * y;
            for (int i = 0; i < 3; i++)
                for (int j = 0; j < 3; j++) P_[i][j] -= K[i] * row0[j];
        }
    }

    for (int i = 0; i < 3; i++)
        for (int j = i + 1; j < 3; j++) {
            const float m = 0.5f * (P_[i][j] + P_[j][i]);
            P_[i][j] = P_[j][i] = m;
        }
}

// ---------------------------------------------------------------- FSM (macchina_stati.m)
void FlightCore::fsmStep(float v, float f, float h, uint32_t t_ms) {
    using namespace cfg;
    if (settling_ && (int32_t)(t_ms - settle_until_) >= 0) settling_ = false;
    const bool hold = settling_;   // dopo un riavvio: v non ancora affidabile, niente conferme burnout/apogeo

    switch (fase_) {
    case Fase::PAD:
        if (f > SOGLIA_DECOLLO) count_++; else count_ = 0;
        if (h > QUOTA_BARO_DECOLLO_M) count_baro_++; else count_baro_ = 0;
        if (count_ >= N_CONFERME || count_baro_ >= N_CONFERME) {
            fase_ = Fase::BOOST;
            count_ = count_baro_ = 0;
            st_.ev_boost = true;
            t_start_ms_ = t_ms;
        }
        break;

    case Fase::BOOST:
        if (!hold && v < x_prev_[1] && f < 0 && h > LUNGHEZZA_RAMPA_M) {
            if (++count_ >= N_CONFERME) { fase_ = Fase::COAST; count_ = 0; }
        } else {
            count_ = 0;
        }
        if ((t_ms - t_start_ms_) > T_MAX_DEPLOY_MS && !deploy_) {
            st_.ev_deploy = true; fase_ = Fase::DESCENT; count_ = 0; deploy_ = true;
        }
        break;

    case Fase::COAST:
        if (!hold && v < 0 && h > QUOTA_MIN_DEPLOY_M && (t_ms - t_start_ms_) > T_MIN_DEPLOY_MS) {
            if (++count_ >= N_CONFERME) {
                st_.ev_deploy = true; fase_ = Fase::DESCENT; count_ = 0; deploy_ = true;
            }
        } else {
            count_ = 0;
        }
        if ((t_ms - t_start_ms_) > T_MAX_DEPLOY_MS && !deploy_) {
            st_.ev_deploy = true; fase_ = Fase::DESCENT; count_ = 0; deploy_ = true;
        }
        break;

    case Fase::DESCENT:
        if (fabsf(v) < LANDED_V_MAX && h < LANDED_H_MAX) {
            if (++count_ >= N_CONFERME) { fase_ = Fase::LANDED; count_ = 0; st_.ev_landed = true; }
        } else {
            count_ = 0;
        }
        break;

    case Fase::LANDED:
        break;
    }
    x_prev_[0] = h; x_prev_[1] = v; x_prev_[2] = f;
}

// ---------------------------------------------------------------- update()
const FlightStatus& FlightCore::update(const SensorData& d, uint32_t t_ms) {
    st_.ev_boost = st_.ev_deploy = st_.ev_landed = false;

    if (restore_pending_) {
        t_start_ms_ = t_ms - restore_elapsed_;
        settle_until_ = t_ms + cfg::RECOVERY_SETTLE_MS;
        restore_pending_ = false;
    }

    float dt = cfg::KF_DT;
    if (d.dt_s > 0.f) dt = fminf(fmaxf(d.dt_s, 0.5f * cfg::KF_DT), 10.f * cfg::KF_DT);

    float baro_h = NAN;
    bool fresh = false;
    const bool p_ok = d.baro_fresh && std::isfinite(d.baro_pa) &&
                      d.baro_pa > cfg::BARO_P_MIN_PA && d.baro_pa < cfg::BARO_P_MAX_PA;
    if (p_ok) {
        if (!ground_ready_) {
            // Calibrazione solo a razzo fermo: se la pressione si muove troppo si riparte
            if (cal_n_ == 0) cal_min_ = cal_max_ = d.baro_pa;
            cal_min_ = fminf(cal_min_, d.baro_pa);
            cal_max_ = fmaxf(cal_max_, d.baro_pa);
            if (cal_max_ - cal_min_ > cfg::BARO_CAL_MAX_RANGE_PA) {
                cal_sum_ = 0.0; cal_n_ = 0;
            } else {
                cal_sum_ += d.baro_pa;
                if (++cal_n_ >= cfg::BARO_N_CAL) {
                    p_ground_ = (float)(cal_sum_ / cal_n_);
                    ground_ready_ = true;
                }
            }
        } else {
            baro_h = 44330.0f * (1.0f - powf(d.baro_pa / p_ground_, 0.190295f));
            fresh = true;
        }
    }

    const float acc = filterAcc(d.acc_ms2);

    st_.ground_ready = ground_ready_;
    st_.baro_h = baro_h;
    if (!ground_ready_) {
        st_.fase = fase_;
        return st_;
    }

    kalmanStep(acc, baro_h, fresh, dt);
    fsmStep(x_[1], acc, x_[0], t_ms);

    st_.fase = fase_;
    st_.h = x_[0];
    st_.v = x_[1];
    st_.bias = x_[2];
    st_.deploy = deploy_;
    return st_;
}
