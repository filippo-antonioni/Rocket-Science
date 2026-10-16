#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>

// Gli stessi ingressi (forza specifica e quota) dei sensori reali.
// Il Kalman e la macchina a stati NON vengono saltati ne' forzati.
enum class Scenario { Reale, Nominale, Rumore, PiccoBaro, BucoImu, BucoBaro, Timeout, FalsoDecollo };
inline const char* scenarioName(Scenario s) {
    const char* names[] = {"REALE","NOMINALE","RUMORE","PICCO_BARO","BUCO_IMU","BUCO_BARO","TIMEOUT","FALSO_DECOLLO"};
    return names[static_cast<unsigned>(s)];
}
inline bool parseScenario(const char* name, Scenario& result) {
    for (unsigned i=1;i<=7;++i) if (strcmp(name,scenarioName(static_cast<Scenario>(i)))==0) {
        result=static_cast<Scenario>(i); return true;
    }
    return false;
}
struct Sample {
    double forza = NAN, quota = NAN, pressione = NAN;
    int16_t raw[6] = {INT16_MIN,INT16_MIN,INT16_MIN,INT16_MIN,INT16_MIN,INT16_MIN};
    bool fresh = false, imuOk = false;
};
inline Sample syntheticSample(Scenario scenario, uint32_t tick) {
    const double t=tick*.01, g=9.80665;
    double h=0, a=0;
    if (t>=1 && t<3) { double u=t-1; a=25; h=12.5*u*u; }
    else if (t>=3) { double u=t-3; a=-10.3; h=50+50*u-5.15*u*u; if (h<=0) {h=0;a=0;} }
    Sample s;
    s.forza=a+g;
    s.quota=h;
    // 38 nuovi campioni ogni 100 tick, senza confrontare valori ripetuti.
    s.fresh=tick==0 || (tick*38/100 != (tick-1)*38/100);
    if (scenario==Scenario::Rumore) {
        s.forza += .08 + .10*std::sin(tick*1.71);
        s.quota += .30*std::sin(tick*.73);
    }
    if (scenario==Scenario::PiccoBaro && tick>=500 && tick<503) s.quota+=150;
    if (scenario==Scenario::BucoImu && tick>=410 && tick<440) s.forza=NAN;
    if (scenario==Scenario::BucoBaro && tick>=400 && tick<500) {s.fresh=false;s.quota=NAN;}
    // IMU bloccata a +g dopo burnout: forza<0 non arriva, rimane BOOST.
    if (scenario==Scenario::Timeout && t>=3) s.forza=g;
    if (scenario==Scenario::FalsoDecollo) {
        s.quota=0; s.forza=g;
        if (tick==100 || tick==101) s.forza=35; // 2 campioni, non 5
    }
    s.imuOk=std::isfinite(s.forza);
    // raw sintetico asse Z; forza e' gia' un ingresso verticale, indipendente da cfg::asse.
    if (s.imuOk) {s.raw[0]=s.raw[1]=0;s.raw[2]=int16_t(s.forza/g*2048);}
    s.raw[3]=s.raw[4]=s.raw[5]=0;
    if (std::isfinite(s.quota)) s.pressione=101325*std::pow(1-s.quota/44330,1/.190295);
    return s;
}
