#include "../src/flight_core.h"
#include "../src/scenarios.h"
#include "../src/log_format.h"
#include <cassert>
#include <iostream>
int main() {
    Ring<int,3> r;for(int i=0;i<8;++i) r.push(i);
    assert(r.size()==3 && r.oldest(0)==5 && r.oldest(2)==7);
    r.clear();r.push(9);assert(r.size()==1 && r.oldest(0)==9);
    assert(crc32("123456789",9)==0xcbf43926u);
    for(unsigned id=1;id<=7;++id) {
        Scenario scenario=static_cast<Scenario>(id);
        volo::Kalman k;volo::MacchinaStati m;volo::Parametri p;
        unsigned launches=0,deploys=0,deployMs=0,rejected=0,bad=0,mask=1;
        for(uint32_t tick=0;tick<3500;++tick) {
            Sample s=syntheticSample(scenario,tick);
            auto c=k.step(s.forza,s.quota,s.fresh,.01,p);
            auto e=m.step(k.x[1],s.forza,k.x[0],tick*10,p);
            if(e.decollo) ++launches;
            if(e.apertura) {++deploys;deployMs=tick*10;}
            if(c==volo::Correzione::Scartata) ++rejected;
            if(!s.imuOk) ++bad;
            mask|=1u<<unsigned(m.fase);
            for(double x:k.x) assert(std::isfinite(x));
        }
        std::cout<<scenarioName(scenario)<<" deploy_ms="<<deployMs<<" launches="<<launches
                 <<" deploys="<<deploys<<" mask="<<mask<<" rejected="<<rejected<<"\n";
        if(scenario==Scenario::FalsoDecollo) assert(!launches && !deploys && m.fase==volo::Fase::PAD);
        else if(scenario==Scenario::Timeout) assert(launches==1 && deploys==1 && m.motivo==volo::MotivoDeploy::Timeout && deployMs>=9000 && deployMs<=9200);
        else assert(launches==1 && deploys==1 && m.motivo==volo::MotivoDeploy::Apogeo && deployMs>=7400 && deployMs<=8300 && mask==31);
        if(scenario==Scenario::PiccoBaro) assert(rejected>0);
        if(scenario==Scenario::BucoImu) assert(bad==30);
    }
}
