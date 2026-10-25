#pragma once
#include "config.h"
#include "flight_core.h"
#include "scenarios.h"
#include "log_format.h"

// Filtro monitorato, nessuna MacchinaStati, nessuna decisione di deploy/LANDED.
class SensorCapture {
    volo::Kalman kalman;
    volo::Parametri params;
    uint32_t lastImuUs=0,lastBaroUs=0;
public:
    uint32_t durationUs=0,rows=0,late=0,maxDtUs=0,maxPollUs=0,maxReadUs=0,maxCoreUs=0;
    uint32_t imuInvalid=0,baroFresh=0,maxImuGapUs=0,maxBaroGapUs=0,numerical=0;
    void reset(unsigned seconds) {
        kalman=volo::Kalman{};params=volo::Parametri{};
        durationUs=seconds*1000000u;rows=late=maxDtUs=maxPollUs=maxReadUs=maxCoreUs=0;
        imuInvalid=baroFresh=maxImuGapUs=maxBaroGapUs=numerical=lastImuUs=lastBaroUs=0;
    }
    bool expired(uint32_t elapsedUs) const {return elapsedUs>=durationUs;}
    void observePoll(uint32_t us) {if(us>maxPollUs) maxPollUs=us;}
    void observeRead(uint32_t us) {if(us>maxReadUs) maxReadUs=us;}
    void observeCore(uint32_t us) {if(us>maxCoreUs) maxCoreUs=us;}
    Record step(const Sample& sample,uint32_t wallUs,uint32_t dtUs) {
        if(dtUs>maxDtUs) maxDtUs=dtUs;
        if(dtUs>15000) ++late;
        // Intervalli tra letture valide includono anche la lettura che chiude un buco.
        uint32_t imuGap=wallUs-lastImuUs,baroGap=wallUs-lastBaroUs;
        if(imuGap>maxImuGapUs) maxImuGapUs=imuGap;
        if(baroGap>maxBaroGapUs) maxBaroGapUs=baroGap;
        if(sample.imuOk) lastImuUs=wallUs;else ++imuInvalid;
        if(sample.fresh && std::isfinite(sample.quota)) {lastBaroUs=wallUs;++baroFresh;}
        const auto correction=kalman.step(sample.forza,sample.quota,sample.fresh,dtUs*1e-6,params);
        if(correction==volo::Correzione::ErroreNumerico) ++numerical;
        Record r={};r.tMs=wallUs/1000;r.dtUs=dtUs;r.sequenza=rows++;
        r.forza=sample.forza;r.baro=sample.quota;r.pressione=sample.pressione;
        r.h=kalman.x[0];r.v=kalman.x[1];r.bias=kalman.x[2];
        memcpy(r.raw,sample.raw,sizeof(r.raw));
        r.fase=0; // campo non applicabile: il client lo mostra come TEST_SENSORI
        r.flags=(sample.imuOk?1:0)|(sample.fresh?2:0)|(correction==volo::Correzione::ErroreNumerico?32:0);
        r.correzione=unsigned(correction);r.ritardi=late;
        r.wallUs=wallUs;r.realDtUs=dtUs;r.trueH=r.trueV=NAN;
        return r;
    }
};
