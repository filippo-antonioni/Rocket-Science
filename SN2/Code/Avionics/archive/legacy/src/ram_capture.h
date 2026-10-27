#pragma once
#include "log_format.h"

// Un solo produttore. Il lettore accede soltanto dopo closing (atomic nel Logger).
// PAD circolare nei primi preLimit slot; BOOST continua dopo tale area.
// Non copia il prebuffer al decollo, non alloca e non sovrascrive il volo.
class RamCapture {
    Record* data=nullptr;
    size_t capacity=0,preLimit=0,preUsed=0,preNext=0,postUsed=0;
    bool all=false,launched=false;
public:
    void reset(Record* buffer,size_t slots,size_t pre,bool captureAll) {
        data=buffer;capacity=slots;preLimit=pre;all=captureAll;
        preUsed=preNext=postUsed=0;launched=false;
    }
    bool push(const Record& record,bool liftoff) {
        if(!data || capacity<=preLimit || !preLimit) return false;
        if(all) {
            if(postUsed==capacity) return false;
            data[postUsed++]=record;return true;
        }
        if(!launched && !liftoff) {
            data[preNext]=record;preNext=(preNext+1)%preLimit;
            if(preUsed<preLimit) ++preUsed;
            return true;
        }
        launched=true;
        if(preLimit+postUsed==capacity) return false;
        data[preLimit+postUsed++]=record;return true;
    }
    size_t size() const {return all?postUsed:preUsed+postUsed;}
    const Record& at(size_t i) const {
        if(all) return data[i];
        if(i<preUsed) return data[(preNext+preLimit-preUsed+i)%preLimit];
        return data[preLimit+i-preUsed];
    }
};
