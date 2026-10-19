#pragma once
#include <cmath>
#include <cstdint>

// Evidenza indipendente dal segno della quota relativa: terreno anche sotto la rampa.
// Solo dopo DEPLOY. Nessun reset del Kalman, nessun cambiamento al comando di apertura.
struct LandingDetector {
    bool tracking=false,haveBaro=false;
    uint32_t sinceMs=0,lastBaroMs=0;
    double minimum=0,maximum=0;
    unsigned samples=0;
    void reset() {*this=LandingDetector{};}
    bool update(uint32_t ms,bool descent,double force,double baro,bool fresh) {
        if(!descent) {reset();return false;}
        if(fresh && std::isfinite(baro)) {lastBaroMs=ms;haveBaro=true;}
        const bool plausible=std::isfinite(force) && std::fabs(force-9.80665)<1.0 &&
            haveBaro && uint32_t(ms-lastBaroMs)<=150;
        if(!plausible) {tracking=false;samples=0;return false;}
        if(!tracking) {
            if(!fresh || !std::isfinite(baro)) return false;
            tracking=true;sinceMs=ms;minimum=maximum=baro;samples=1;
        } else if(fresh && std::isfinite(baro)) {
            if(baro<minimum) minimum=baro;
            if(baro>maximum) maximum=baro;
            ++samples;
            if(maximum-minimum>1.0) {sinceMs=ms;minimum=maximum=baro;samples=1;}
        }
        return uint32_t(ms-sinceMs)>=2000 && samples>=20;
    }
};
