#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>

// Identificatori 0..7 invariati per leggere anche i vecchi log.
enum class Scenario { Reale, Nominale, Rumore, PiccoBaro, BucoImu, BucoBaro, Timeout, FalsoDecollo,
    BucoImuBurnout, BucoBaroApogeo, CoastZero, PiccoBaroLungo, SensoriAssenti,
    PadRumoroso, Jitter, Paracadute, QuotaNegativa, FalsoLanded, BaroCongelato };
constexpr unsigned scenarioCount=18;
inline const char* scenarioName(Scenario s) {
    const char* names[]={"REALE","NOMINALE","RUMORE","PICCO_BARO","BUCO_IMU","BUCO_BARO","TIMEOUT","FALSO_DECOLLO",
        "BUCO_IMU_BURNOUT","BUCO_BARO_APOGEO","COAST_ZERO","PICCO_BARO_LUNGO","SENSORI_ASSENTI",
        "PAD_RUMOROSO","JITTER","PARACADUTE","QUOTA_NEGATIVA","FALSO_LANDED","BARO_CONGELATO"};
    return unsigned(s)<=scenarioCount?names[unsigned(s)]:"INVALID";
}
inline bool parseScenario(const char* name,Scenario& result) {
    for(unsigned i=1;i<=scenarioCount;++i) if(!strcmp(name,scenarioName(Scenario(i)))) {result=Scenario(i);return true;}
    return false;
}
inline bool padScenario(Scenario s) {return s==Scenario::FalsoDecollo || s==Scenario::PadRumoroso;}
inline bool chuteScenario(Scenario s) {return s==Scenario::Paracadute || s==Scenario::QuotaNegativa || s==Scenario::FalsoLanded;}
inline uint32_t scenarioDurationMs(Scenario s) {return chuteScenario(s)?75000:35000;}
inline uint32_t modelStepUs(Scenario s,uint32_t tick) {
    // Ritardi RIPRODUCIBILI nel tempo matematico, senza bloccare il processore.
    return s==Scenario::Jitter && tick%125==100?30000:10000;
}
struct Truth {double h=0,v=0,a=0;bool ground=true;};
inline Truth truthAt(Scenario s,double t) {
    Truth q;
    if(padScenario(s) || t<1) return q;
    q.ground=false;
    if(t<3) {double u=t-1;q.a=25;q.v=25*u;q.h=12.5*u*u;return q;}
    const double gravity=s==Scenario::CoastZero?9.80665:10.3;
    double u=t-3;q.a=-gravity;q.v=50-gravity*u;q.h=50+50*u-.5*gravity*u*u;
    if(chuteScenario(s) && t>=8.1) {
        // Profilo prescritto, indipendente dal comando firmware: NON simula il servo.
        const double h0=50+50*5.1-5.15*5.1*5.1, v0=50-10.3*5.1;
        const double ramp=.5, acc=(-5-v0)/ramp, endH=h0+v0*ramp+.5*acc*ramp*ramp;
        u=t-8.1;
        if(u<ramp) {q.a=acc;q.v=v0+acc*u;q.h=h0+v0*u+.5*acc*u*u;}
        else {
            const double pauseStart=8.6+(endH-5)/5;
            double travel=t-8.6;
            if(s==Scenario::FalsoLanded && t>=pauseStart) {
                if(t<pauseStart+.3) {q.h=5;q.v=0;q.a=0;return q;}
                travel-=.3;
            }
            q.h=endH-5*travel;q.v=-5;q.a=0;
        }
    }
    const double terrain=s==Scenario::QuotaNegativa?-25:0;
    if(q.h<=terrain) {q.h=terrain;q.v=q.a=0;q.ground=true;}
    return q;
}
struct Sample {
    double forza=NAN,quota=NAN,pressione=NAN;
    int16_t raw[6]={INT16_MIN,INT16_MIN,INT16_MIN,INT16_MIN,INT16_MIN,INT16_MIN};
    bool fresh=false,imuOk=false;
};
inline Sample syntheticAt(Scenario scenario,uint32_t timeUs,uint32_t previousUs) {
    const double t=timeUs*1e-6,g=9.80665;
    const uint32_t tick=timeUs/10000;
    Truth q=truthAt(scenario,t);Sample s;s.forza=q.a+g;s.quota=q.h;
    s.fresh=timeUs==0 || uint64_t(timeUs)*38/1000000!=uint64_t(previousUs)*38/1000000;
    if(scenario==Scenario::Rumore) {s.forza+=.08+.10*std::sin(tick*1.71);s.quota+=.30*std::sin(tick*.73);}
    if(scenario==Scenario::PiccoBaro && tick>=500 && tick<503) s.quota+=150;
    if(scenario==Scenario::BucoImu && tick>=410 && tick<440) s.forza=NAN;
    if(scenario==Scenario::BucoBaro && tick>=400 && tick<500) {s.fresh=false;s.quota=NAN;}
    if(scenario==Scenario::Timeout && t>=3) s.forza=g;
    if(scenario==Scenario::FalsoDecollo && (tick==100 || tick==101)) s.forza=35;
    if(scenario==Scenario::BucoImuBurnout && t>=2.85 && t<3.35) s.forza=NAN;
    if(scenario==Scenario::BucoBaroApogeo && t>=7.4 && t<8.5) {s.fresh=false;s.quota=NAN;}
    if(scenario==Scenario::PiccoBaroLungo && t>=4.8 && t<5.8) s.quota+=150;
    if(scenario==Scenario::SensoriAssenti && t>=4) {s.forza=NAN;s.quota=NAN;s.fresh=false;}
    if(scenario==Scenario::BaroCongelato && t>=4) s.quota=truthAt(scenario,4).h;
    if(scenario==Scenario::PadRumoroso) {
        s.forza+=.5*std::sin(tick*1.71);s.quota+=.8*std::sin(tick*.73);
        if(tick%400>=100 && tick%400<104) s.forza=35; // quattro campioni, soglia cinque
    }
    s.imuOk=std::isfinite(s.forza);
    // raw sintetici su Z per compatibilita' con v2; forza gia' verticale.
    if(s.imuOk) {s.raw[0]=s.raw[1]=0;s.raw[2]=int16_t(s.forza/g*2048);}
    s.raw[3]=s.raw[4]=s.raw[5]=0;
    if(std::isfinite(s.quota)) s.pressione=101325*std::pow(1-s.quota/44330,1/.190295);
    return s;
}
inline Sample syntheticSample(Scenario s,uint32_t tick) {return syntheticAt(s,tick*10000,tick? (tick-1)*10000:0);}
