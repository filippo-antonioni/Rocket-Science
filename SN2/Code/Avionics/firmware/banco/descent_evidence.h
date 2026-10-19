#pragma once
#include <cmath>
#include <cstdint>
// Variante sperimentale: conferma indipendente di discesa dal barometro.
// 24 letture fresche (~0.6 s a 38 Hz), regressione lineare e margine sul rumore.
// Non risolve un guasto comune ai sensori e puo' ritardare l'apertura se manca il baro.
struct DescentEvidence {
    double heights[24]={};uint32_t times[24]={};
    unsigned next=0,count=0;uint32_t lastMs=0;bool descending=false;
    void reset() {*this=DescentEvidence{};}
    bool update(uint32_t ms,double height,bool fresh) {
        if(count && uint32_t(ms-lastMs)>150) reset();
        if(fresh && std::isfinite(height)) {
            heights[next]=height;times[next]=ms;next=(next+1)%24;if(count<24) ++count;lastMs=ms;
            descending=false;
            if(count==24) {
                uint32_t span=uint32_t(ms-times[next]);
                if(span>=400 && span<=900) {
                    double meanT=0,meanH=0;
                    for(unsigned i=0;i<24;++i) {meanT+=uint32_t(ms-times[i])*.001;meanH+=heights[i];}
                    meanT/=24;meanH/=24;
                    double xx=0,xy=0;
                    for(unsigned i=0;i<24;++i) {double x=uint32_t(ms-times[i])*.001-meanT;xx+=x*x;xy+=x*(heights[i]-meanH);}
                    double slope=-xy/xx,rss=0;
                    for(unsigned i=0;i<24;++i) {double x=uint32_t(ms-times[i])*.001-meanT;double residual=heights[i]-meanH+slope*x;rss+=residual*residual;}
                    const double uncertainty=std::sqrt(rss/22/xx);
                    descending=slope+2*uncertainty<-.3;
                }
            }
        }
        return descending;
    }
};
