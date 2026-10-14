#pragma once
// =====================================================================
//  SEZIONE 2 - Logger su LittleFS / SD (BOOST -> LANDED)
//  Control loop (core 1) --push()--> ring buffer SPSC lock-free --> task writer (core 0)
//  Il loop di controllo non fa mai I/O su memoria e non si blocca mai.
// =====================================================================
#include <Arduino.h>
#include <FS.h>
#include <atomic>
#include "config.h"

static_assert((cfg::LOG_RING_N & (cfg::LOG_RING_N - 1)) == 0, "LOG_RING_N deve essere potenza di 2");

struct LogRecord {
    uint32_t t_ms;
    float    p_pa, baro_h, acc;
    float    gyr[3];
    float    h_kal, v_kal, bias_kal;
    uint8_t  fase;
    uint8_t  flags;    // bit0 baro fresco, bit1 IMU non valida, bit2 deploy comandato
};

class FlightLogger {
public:
    bool begin(fs::FS& fs, bool resume = false); 
    void start();                      
    void stop();                       
    void push(const LogRecord& r);     
    bool active() const { return state_.load() == ACTIVE; }
    uint32_t dropped() const { return dropped_; }
    const char* path() const { return path_; }

private:
    enum State : uint8_t { LOG_DISABLED, IDLE, ACTIVE, CLOSING, DONE }; // <--- Modificato qui
    static void taskEntry(void* self) { static_cast<FlightLogger*>(self)->writerLoop(); }
    void writerLoop();
    bool ringPush(const LogRecord& r);
    uint16_t nextFileIndex();
    static int formatCsv(const LogRecord& r, char* buf, size_t n);

    fs::FS* fs_ = nullptr;
    bool resume_ = false;
    char path_[32] = {0};
    std::atomic<uint8_t> state_{LOG_DISABLED}; // <--- Modificato qui

    LogRecord ring_[cfg::LOG_RING_N];
    std::atomic<uint32_t> head_{0};    
    std::atomic<uint32_t> tail_{0};    
    volatile uint32_t dropped_ = 0;

    LogRecord pre_[cfg::LOG_PREROLL_N];  
    uint32_t pre_idx_ = 0;

    char chunk_[cfg::LOG_CHUNK_B];
};