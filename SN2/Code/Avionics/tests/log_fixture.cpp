#include "../src/log_format.h"
#include <fstream>
#include <limits>
int main(int argc,char** argv) {
    if(argc!=2) return 1;
    std::ofstream out(argv[1],std::ios::binary);
    LogHeader h;h.sorgente=3;out.write(reinterpret_cast<const char*>(&h),sizeof(h));
    for(uint32_t i=0;i<3;++i) {
        Record r={};r.tMs=1000+i*10;r.dtUs=10000;r.sequenza=100+i;
        r.forza=i==1?std::numeric_limits<float>::quiet_NaN():9.80665f;
        r.baro=12.5f;r.h=12;r.v=-.1f;r.bias=.08f;r.pressione=100000;
        r.raw[2]=2048;r.fase=3;r.flags=23;r.motivo=1;r.ritardi=2;
        r.crc=crc32(&r,offsetof(Record,crc));
        out.write(reinterpret_cast<const char*>(&r),sizeof(r));
    }
}
