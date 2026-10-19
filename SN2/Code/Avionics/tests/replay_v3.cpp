#include "../firmware/banco/bench_metrics.h"
#include "../firmware/banco/landing.h"
#include "../firmware/banco/descent_evidence.h"
#include "../firmware/banco/log_format.h"
#include <cstdio>
#include <cassert>
#include <fstream>
#include <string>
int main(int argc,char** argv) {
    std::puts("scenario,core,algorithm,accuracy,landing,deploy_ms,land_ms,h_error_deploy,v_error_deploy,max_pre_h_error,max_pre_v_error,early_landing,land_h_error");
    for(unsigned id=1;id<=scenarioCount;++id) for(unsigned robust=0;robust<2;++robust) {
        Scenario s=Scenario(id);volo::Kalman k;volo::MacchinaStati f;volo::Parametri p;p.robusto=robust;
        LandingDetector detector;DescentEvidence descent;BenchMetrics metrics;
        std::ofstream binary;
        if(argc>1) {
            binary.open(std::string(argv[1])+"/"+scenarioName(s)+"_"+(robust?"CANDIDATO":"BASE")+".bin",std::ios::binary);
            assert(binary.good());LogHeader header;header.sorgente=id;header.coreMode=robust;header.durataMs=scenarioDurationMs(s);
            binary.write(reinterpret_cast<const char*>(&header),sizeof(header));
        }
        uint32_t us=0,prev=0,tick=0;
        for(;;++tick) {
            const uint32_t dt=modelStepUs(s,tick);prev=us;if(tick) us+=dt;
            if(us>=scenarioDurationMs(s)*1000) break;
            Sample sample=syntheticAt(s,us,prev);
            auto correction=k.step(sample.forza,sample.quota,sample.fresh,dt*1e-6,p);
            bool landed=detector.update(us/1000,f.fase==volo::Fase::DESCENT,sample.forza,sample.quota,sample.fresh);
            bool confirmation=descent.update(us/1000,sample.quota,sample.fresh);
            auto e=f.step(k.x[1],sample.forza,k.x[0],us/1000,p,landed,confirmation);
            metrics.update(s,us/1000,sample,k,f,e,correction);
            for(double x:k.x) assert(std::isfinite(x));
            if(binary.is_open()) {
                Truth truth=truthAt(s,us*1e-6);Record r={};r.tMs=us/1000;r.dtUs=dt;r.sequenza=tick;
                r.forza=sample.forza;r.baro=sample.quota;r.pressione=sample.pressione;r.h=k.x[0];r.v=k.x[1];r.bias=k.x[2];
                memcpy(r.raw,sample.raw,sizeof(r.raw));r.fase=unsigned(f.fase);r.flags=(sample.imuOk?1:0)|(sample.fresh?2:0)|(f.deploy?4:0)|16;
                r.correzione=unsigned(correction);r.motivo=unsigned(f.motivo);r.trueH=truth.h;r.trueV=truth.v;
                r.wallUs=us;r.realDtUs=dt;r.crc=crc32(&r,offsetof(Record,crc));binary.write(reinterpret_cast<const char*>(&r),sizeof(r));
            }
        }
        std::printf("%s,%s,%s,%s,%s,%u,%u,%.6f,%.6f,%.6f,%.6f,%u,%.6f\n",scenarioName(s),robust?"CANDIDATO":"BASE",
            metrics.algorithmOk(s,f)?"PASS":"CHECK",metrics.accuracyOk()?"PASS":"CHECK",
            padScenario(s)?"SKIP":metrics.landingOk(s)?"PASS":"CHECK",metrics.deployMs,metrics.landMs,
            metrics.deployHError,metrics.deployVError,metrics.maxPreHError,metrics.maxPreVError,unsigned(metrics.earlyLanding),metrics.landHError);
        assert(!metrics.numerical && metrics.deploys<=1);
        if(id<=7) assert(metrics.algorithmOk(s,f));
        if(padScenario(s)) assert(!metrics.launches && !metrics.deploys);
        if(s==Scenario::CoastZero) assert(metrics.algorithmOk(s,f)==bool(robust));
        if(robust && (s==Scenario::Paracadute || s==Scenario::QuotaNegativa || s==Scenario::FalsoLanded)) assert(metrics.landingOk(s));
    }
}
