#include "../firmware/banco/landing.h"
#include "../firmware/banco/descent_evidence.h"
#include "../firmware/banco/flight_core.h"
#include <cassert>
#include <iostream>
int main() {
    LandingDetector land;
    // Una breve quiete non basta; sotto la quota di rampa non e' un errore.
    for(unsigned ms=0;ms<300;++ms) assert(!land.update(ms,true,9.80665,-25,ms%30==0));
    assert(!land.update(300,true,0,-25,true)); // movimento: interrompe continuita'
    for(unsigned ms=310;ms<2310;ms+=10) assert(!land.update(ms,true,9.80665,-25,ms%30==0));
    assert(land.update(2340,true,9.80665,-25,true));
    assert(!land.update(2500,true,9.80665,-25,false)); // baro vecchio
    land.reset();
    for(unsigned ms=0;ms<5000;ms+=10) assert(!land.update(ms,true,9.80665,100-ms*.005,ms%30==0)); // discesa -5 m/s
    land.reset();
    for(unsigned ms=0;ms<3000;ms+=10) assert(!land.update(ms,false,9.80665,0,true)); // PAD
    const uint32_t base=UINT32_MAX-1000;land.reset();bool reached=false;
    for(unsigned ms=0;ms<2500;ms+=10) reached=land.update(base+ms,true,9.80665,0,ms%30==0)||reached;
    assert(reached); // rollover millis
    DescentEvidence d;
    for(unsigned ms=0;ms<3000;ms+=10) assert(!d.update(ms,100,ms%30==0)); // congelato
    d.reset();bool descent=false;
    for(unsigned ms=0;ms<1500;ms+=10) descent=d.update(ms,100-ms*.005,ms%30==0);
    assert(descent && !d.update(2000,NAN,false));
    // Dimostrazione diretta del problema LANDED originale: sola stima falsa.
    volo::MacchinaStati original,candidate;volo::Parametri p;
    original.fase=candidate.fase=volo::Fase::DESCENT;
    for(unsigned ms=0;ms<50;ms+=10) original.step(0,p.g,-32,ms,p);
    assert(original.fase==volo::Fase::LANDED);
    p.robusto=true;
    for(unsigned ms=0;ms<1000;ms+=10) candidate.step(0,p.g,-32,ms,p,false);
    assert(candidate.fase==volo::Fase::DESCENT);
    assert(candidate.step(0,p.g,-32,2000,p,true).atterraggio);
    std::cout<<"PASS: durata quiete, quote negative, dropout, discesa, rollover, baro congelato, LANDED spurio\n";
}
