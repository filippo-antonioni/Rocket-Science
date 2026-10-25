#pragma once
#include <cstdint>
#include <cstddef>

namespace powerTest {
constexpr uint32_t startupMs=30000;
constexpr unsigned durationSeconds=180;
struct Action {uint32_t atMs;bool open;};
constexpr Action actions[]={{60000,true},{90000,false},{120000,true}};
constexpr size_t actionCount=sizeof(actions)/sizeof(actions[0]);
static_assert(actionCount==3,"Riepilogo e analisi prevedono tre comandi");
static_assert(actions[actionCount-1].atMs<durationSeconds*1000u,"Evento dopo fine test");
class Sequence {
    size_t next_=0;
public:
    // An event is consumed only AFTER a successful PWM write.
    const Action* due(uint32_t elapsedMs) const {
        return next_<actionCount && elapsedMs>=actions[next_].atMs ? &actions[next_] : nullptr;
    }
    void acknowledge() {if(next_<actionCount) ++next_;}
    unsigned completed() const {return unsigned(next_);}
    bool isOpen() const {return next_ && actions[next_-1].open;}
};
}
