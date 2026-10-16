#pragma once
#include <cmath>
#include <cstdint>

// Algoritmi senza dipendenze Arduino: verificabili anche sul PC.
namespace volo {
struct Parametri {
    double g = 9.80665;
    double rumoreAcc = 0.012;
    double rumoreBaro = 2.97 * 0.083;
    double fattoreDisturbi = 10.0;
    double qBiasPerCampione = 1e-4; // MATLAB: a dt nominale di 0.01 s
    unsigned scartiMax = 5;
    double sogliaDecollo = 30.0;
    unsigned conferme = 5;
    double quotaDecolloBaro = 18.0;
    double lunghezzaRampa = 1.5;
    double quotaMinDeploy = 30.0;
    double tempoMinDeploy = 1.8;
    double tempoMaxDeploy = 8.0;
};  

enum class Correzione { Assente, Scartata, Accettata, ErroreNumerico };

struct Kalman {
    // MATLAB x(1), x(2), x(3) -> C++ x[0], x[1], x[2].
    double x[3] = {0.0, 0.0, 0.0}; // h [m], v [m/s], bias [m/s^2]
    double P[3][3] = {{0.25, 0, 0}, {0, 0.01, 0}, {0, 0, 0.16}};
    double accPrecedente = 9.80665;
    unsigned scarti = 0;

    Correzione step(double acc, double baro, bool fresh, double dt,
                    const Parametri& p) {
        if (!std::isfinite(dt) || dt <= 0) return Correzione::ErroreNumerico;
        if (std::isfinite(acc)) accPrecedente = acc;
        else acc = accPrecedente; // stesso fallback di kalman.m
        const double dt2 = 0.5 * dt * dt;
        const double F[3][3] = {{1, dt, -dt2}, {0, 1, -dt}, {0, 0, 1}};
        const double B[3] = {dt2, dt, 0};
        const double a = acc - p.g - x[2];
        // F*x+B*u, espanso: usare la vecchia velocita' per aggiornare h!
        x[0] += dt * x[1] + dt2 * a;
        x[1] += dt * a;

        double FP[3][3] = {};
        double pred[3][3] = {};
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                for (int k = 0; k < 3; ++k)
                    FP[i][j] += F[i][k] * P[k][j];
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                for (int k = 0; k < 3; ++k)
                    pred[i][j] += FP[i][k] * F[j][k]; // F trasposta
                pred[i][j] += B[i] * B[j] * p.rumoreAcc * p.rumoreAcc;
            }
        }
        // Identico al MATLAB a 100 Hz; proporzionale al tempo se il loop ritarda.
        pred[2][2] += p.qBiasPerCampione * dt / 0.01;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) P[i][j] = pred[i][j];

        if (!fresh || !std::isfinite(baro)) return Correzione::Assente;
        const double sigma = p.rumoreBaro * p.fattoreDisturbi;
        const double R = sigma * sigma;
        // H=[1 0 0]: y, S e K non richiedono algebra matriciale generale.
        const double y = baro - x[0];
        const double S = P[0][0] + R;
        if (!std::isfinite(S) || S <= 0 || !std::isfinite(y))
            return Correzione::ErroreNumerico;
        if (std::fabs(y) > 4 * std::sqrt(S) && scarti < p.scartiMax) {
            ++scarti;
            return Correzione::Scartata;
        }
        scarti = 0;
        double K[3];
        double A[3][3] = {};
        double AP[3][3] = {};
        double post[3][3] = {};
        for (int i = 0; i < 3; ++i) {
            K[i] = P[i][0] / S; // S e' scalare: NESSUNA inversione di matrice
            x[i] += K[i] * y;
            for (int j = 0; j < 3; ++j)
                A[i][j] = (i == j ? 1.0 : 0.0) - (j == 0 ? K[i] : 0.0);
        }
        // Forma di Joseph, equivalente a (I-KH)*P in aritmetica esatta,
        // piu' robusta agli arrotondamenti: A*P*A' + K*R*K'.
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                for (int k = 0; k < 3; ++k) AP[i][j] += A[i][k] * P[k][j];
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) {
                for (int k = 0; k < 3; ++k) post[i][j] += AP[i][k] * A[j][k];
                post[i][j] += K[i] * R * K[j];
            }
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) P[i][j] = 0.5 * (post[i][j] + post[j][i]);
        return Correzione::Accettata;
    }
};

enum class Fase { PAD, BOOST, COAST, DESCENT, LANDED };
enum class MotivoDeploy { Nessuno, Apogeo, Timeout };
inline const char* nomeFase(Fase f) {
    switch (f) {
        case Fase::PAD: return "PAD";
        case Fase::BOOST: return "BOOST";
        case Fase::COAST: return "COAST";
        case Fase::DESCENT: return "DESCENT";
        case Fase::LANDED: return "LANDED";
    }
    return "ERRORE";
}
struct Eventi {
    bool decollo = false; // flag_t: impulso di un solo ciclo
    bool apertura = false; // flag: impulso di un solo ciclo
    bool atterraggio = false;
};
struct MacchinaStati {
    Fase fase = Fase::PAD;
    unsigned count = 0, countBaro = 0;
    double velocitaPrecedente = 0.0;
    uint32_t decolloMs = 0;
    bool deploy = false; // rimane true anche dopo l'impulso apertura
    MotivoDeploy motivo = MotivoDeploy::Nessuno;

    Eventi step(double v, double forza, double h, uint32_t oraMs,
                const Parametri& p) {
        Eventi e;
        // Sottrazione unsigned: funziona anche al rollover di millis().
        const double tempoVolo = uint32_t(oraMs - decolloMs) * 0.001;
        switch (fase) {
            case Fase::PAD:
                count = forza > p.sogliaDecollo ? count + 1 : 0;
                countBaro = h > p.quotaDecolloBaro ? countBaro + 1 : 0;
                if (count >= p.conferme || countBaro >= p.conferme) {
                    fase = Fase::BOOST;
                    count = countBaro = 0;
                    decolloMs = oraMs;
                    e.decollo = true;
                }
                break;
            case Fase::BOOST:
                count = (v < velocitaPrecedente && forza < 0 && h > p.lunghezzaRampa)
                        ? count + 1 : 0;
                if (count >= p.conferme) { fase = Fase::COAST; count = 0; }
                if (tempoVolo > p.tempoMaxDeploy && !deploy)
                    apri(e, MotivoDeploy::Timeout);
                break;
            case Fase::COAST:
                count = (v < 0 && h > p.quotaMinDeploy && tempoVolo > p.tempoMinDeploy)
                        ? count + 1 : 0;
                if (count >= p.conferme) apri(e, MotivoDeploy::Apogeo);
                if (tempoVolo > p.tempoMaxDeploy && !deploy)
                    apri(e, MotivoDeploy::Timeout);
                break;
            case Fase::DESCENT:
                count = (std::fabs(v) < 0.4 && h < 10) ? count + 1 : 0;
                if (count >= p.conferme) {
                    fase = Fase::LANDED; count = 0; e.atterraggio = true;
                }
                break;
            case Fase::LANDED: break;
        }
        velocitaPrecedente = v;
        return e;
    }
private:
    void apri(Eventi& e, MotivoDeploy causa) {
        e.apertura = true; deploy = true; fase = Fase::DESCENT;
        count = 0; motivo = causa;
    }
};
} // namespace volo
