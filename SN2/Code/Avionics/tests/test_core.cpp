#include "../firmware/banco/flight_core.h"
#include <cassert>
#include <iostream>
using namespace volo;

int main() {
    Parametri p;
    Kalman k;
    for (int i=0;i<1000;++i) k.step(p.g,0,i%3==0,.01,p);
    assert(std::fabs(k.x[0])<1e-12 && std::fabs(k.x[1])<1e-12);
    assert(k.step(NAN,NAN,true,.01,p)==Correzione::Assente);
    assert(k.step(p.g,0,true,0,p)==Correzione::ErroreNumerico);
    for (int i=0;i<5;++i) assert(k.step(p.g,10000,true,.01,p)==Correzione::Scartata);
    assert(k.step(p.g,10000,true,.01,p)==Correzione::Accettata);
    assert(k.scarti==0);

    MacchinaStati m;
    for (int i=0;i<4;++i) assert(!m.step(0,31,0,i*10,p).decollo);
    m.step(0,29,0,40,p); // interrompe la consecutivita'
    for (int i=0;i<4;++i) assert(!m.step(0,31,0,50+i*10,p).decollo);
    assert(m.step(20,31,0,90,p).decollo);
    assert(m.fase==Fase::BOOST);
    for (int i=0;i<5;++i) m.step(19-i,-1,40,100+i*10,p);
    assert(m.fase==Fase::COAST);
    for (int i=0;i<10;++i) assert(!m.step(-1,-1,50,200+i*10,p).apertura);
    for (int i=0;i<10;++i) assert(!m.step(-1,-1,29,2000+i*10,p).apertura);
    for (int i=0;i<4;++i) assert(!m.step(-1,-1,50,2200+i*10,p).apertura);
    assert(m.step(-1,-1,50,2240,p).apertura);
    assert(m.deploy && m.motivo==MotivoDeploy::Apogeo);
    for (int i=0;i<4;++i) assert(!m.step(0,p.g,0,3000+i*10,p).atterraggio);
    assert(m.step(0,p.g,0,3040,p).atterraggio);
    assert(!m.step(0,p.g,0,3050,p).apertura && m.fase==Fase::LANDED);

    // Decollo di riserva con IMU NaN, poi timer anche con tutte le stime NaN.
    MacchinaStati riserva;
    for (int i=0;i<5;++i) riserva.step(0,NAN,19,100+i*10,p);
    assert(riserva.fase==Fase::BOOST);
    assert(!riserva.step(NAN,NAN,NAN,8140,p).apertura); // confronto strettamente >
    assert(riserva.step(NAN,NAN,NAN,8141,p).apertura);
    assert(riserva.motivo==MotivoDeploy::Timeout);
    assert(!riserva.step(NAN,NAN,NAN,8150,p).apertura);
    MacchinaStati coastTimer;
    coastTimer.fase=Fase::COAST;
    assert(coastTimer.step(NAN,NAN,NAN,8001,p).apertura);
    MacchinaStati pad;
    assert(!pad.step(0,p.g,0,100000,p).apertura); // timer non parte all'accensione

    MacchinaStati wrap;
    const uint32_t inizio=UINT32_MAX-100;
    for (unsigned i=0;i<5;++i) wrap.step(0,31,0,inizio+i*10,p);
    assert(wrap.step(NAN,NAN,NAN,uint32_t(wrap.decolloMs+8001),p).apertura);
    std::cout << "PASS: Kalman, dropout, gating, conferme, apogeo, timeout, rollover, atterraggio\n";
}
