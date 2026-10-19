#pragma once
#include "scenarios.h"
#include "flight_core.h"
// Esiti distinti: sequenza stati, accuratezza prima dell'apertura, atterraggio.
// Le soglie sono criteri di regressione al banco, NON requisiti di certificazione.
struct BenchMetrics {
    unsigned launches=0,deploys=0,numerical=0,invalidImu=0,rejected=0;
    uint32_t deployMs=0,landMs=0;
    double deployHError=NAN,deployVError=NAN,maxPreHError=0,maxPreVError=0,landHError=NAN;
    bool landed=false,earlyLanding=false;
    void update(Scenario s,uint32_t ms,const Sample& sample,const volo::Kalman& k,
                const volo::MacchinaStati& f,const volo::Eventi& e,volo::Correzione correction) {
        const Truth truth=truthAt(s,ms*.001);
        if(!sample.imuOk) ++invalidImu;
        if(correction==volo::Correzione::Scartata) ++rejected;
        if(correction==volo::Correzione::ErroreNumerico || !std::isfinite(k.x[0]) || !std::isfinite(k.x[1]) || !std::isfinite(k.x[2])) ++numerical;
        if(!deploys) {
            double h=std::fabs(k.x[0]-truth.h),v=std::fabs(k.x[1]-truth.v);
            if(h>maxPreHError) maxPreHError=h;
            if(v>maxPreVError) maxPreVError=v;
        }
        if(e.decollo) ++launches;
        if(e.apertura) {++deploys;deployMs=ms;deployHError=k.x[0]-truth.h;deployVError=k.x[1]-truth.v;}
        if(e.atterraggio) {landed=true;landMs=ms;earlyLanding=!truth.ground;landHError=k.x[0]-truth.h;}
        (void)f;
    }
    bool algorithmOk(Scenario s,const volo::MacchinaStati& f) const {
        if(numerical) return false;
        if(padScenario(s)) return launches==0 && deploys==0 && f.fase==volo::Fase::PAD;
        if(launches!=1 || deploys!=1) return false;
        if(s==Scenario::Timeout) return f.motivo==volo::MotivoDeploy::Timeout && deployMs>=9000 && deployMs<=9300;
        const double apogee=3+50/(s==Scenario::CoastZero?9.80665:10.3);
        // Non prima dell'apogeo fisico, non oltre 0.5 s: criterio esplicito.
        return f.motivo==volo::MotivoDeploy::Apogeo && deployMs>=uint32_t(apogee*1000) && deployMs<=uint32_t(apogee*1000)+500;
    }
    bool accuracyOk() const {return !numerical && maxPreHError<=5 && maxPreVError<=5;}
    bool landingOk(Scenario s) const {
        if(padScenario(s)) return !landed;
        if(!landed || earlyLanding) return false;
        // Deve riconoscere la quiete entro 5 s dal contatto prescritto.
        return landMs<5000 || !truthAt(s,(landMs-5000)*.001).ground;
    }
};
