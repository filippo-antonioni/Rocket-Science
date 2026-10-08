#include "flight_logger.h"
#include <stdio.h>
#include <string.h>

static const char CSV_HEADER[] =
    "t_ms,fase,p_pa,baro_h_m,acc_ms2,gyr_x_dps,gyr_y_dps,gyr_z_dps,h_kal_m,v_kal_ms,bias_kal_ms2,flags\n";

bool FlightLogger::begin(fs::FS& fs, bool resume) {
    fs_ = &fs;
    resume_ = resume;
    unsigned idx = nextFileIndex();              
    if (resume) { if (idx == 0) idx = 1; } else idx++;   
    snprintf(path_, sizeof path_, "/flight_%03u.csv", idx);
    state_.store(IDLE);
    if (xTaskCreatePinnedToCore(&FlightLogger::taskEntry, "logger", 8192, this, 2, nullptr, 0) != pdPASS) {
        state_.store(LOG_DISABLED); // <--- Modificato qui
        return false;
    }
    return true;
}

uint16_t FlightLogger::nextFileIndex() {
    unsigned maxIdx = 0;
    File root = fs_->open("/");
    if (root && root.isDirectory()) {
        File e = root.openNextFile();
        while (e) {
            const char* nm = e.name();
            if (nm[0] == '/') nm++;
            unsigned idx;
            if (sscanf(nm, "flight_%u.csv", &idx) == 1 && idx > maxIdx) maxIdx = idx;
            e = root.openNextFile();
        }
    }
    return (uint16_t)maxIdx;
}

bool FlightLogger::ringPush(const LogRecord& r) {
    const uint32_t h = head_.load(std::memory_order_relaxed);
    const uint32_t t = tail_.load(std::memory_order_acquire);
    if (h - t >= cfg::LOG_RING_N) { dropped_++; return false; }   
    ring_[h & (cfg::LOG_RING_N - 1)] = r;
    head_.store(h + 1, std::memory_order_release);
    return true;
}

void FlightLogger::push(const LogRecord& r) {
    switch (state_.load()) {
    case IDLE:    
        pre_[pre_idx_ % cfg::LOG_PREROLL_N] = r;
        pre_idx_++;
        break;
    case ACTIVE:
        ringPush(r);
        break;
    default:
        break;
    }
}

void FlightLogger::start() {
    if (state_.load() != IDLE) return;
    const uint32_t n = pre_idx_ < cfg::LOG_PREROLL_N ? pre_idx_ : cfg::LOG_PREROLL_N;
    for (uint32_t k = 0; k < n; k++)
        ringPush(pre_[(pre_idx_ - n + k) % cfg::LOG_PREROLL_N]);
    state_.store(ACTIVE);     
}

void FlightLogger::stop() {
    uint8_t exp = ACTIVE;
    state_.compare_exchange_strong(exp, (uint8_t)CLOSING);
}

int FlightLogger::formatCsv(const LogRecord& r, char* b, size_t n) {
    const int k = snprintf(b, n, "%lu,%u,%.1f,%.3f,%.4f,%.3f,%.3f,%.3f,%.3f,%.3f,%.4f,%u\n",
                           (unsigned long)r.t_ms, (unsigned)r.fase, r.p_pa, r.baro_h, r.acc,
                           r.gyr[0], r.gyr[1], r.gyr[2], r.h_kal, r.v_kal, r.bias_kal,
                           (unsigned)r.flags);
    return (k < 0 || (size_t)k >= n) ? 0 : k;
}

void FlightLogger::writerLoop() {
    File f;
    bool opened = false;
    size_t clen = 0;
    uint32_t t_flush = 0;
    char line[cfg::LOG_LINE_B];

    for (;;) {
        const uint8_t s = state_.load();   
        if (s == LOG_DISABLED || s == IDLE || s == DONE) { // <--- Modificato qui
            vTaskDelay(pdMS_TO_TICKS(s == DONE ? 500 : 20));
            continue;
        }

        if (!opened) {                      
            f = fs_->open(path_, resume_ ? FILE_APPEND : FILE_WRITE);
            if (!f) { state_.store(DONE); continue; }
            if (!resume_) f.print(CSV_HEADER);
            opened = true;
            t_flush = millis();
        }

        uint32_t t = tail_.load(std::memory_order_relaxed);
        const uint32_t h = head_.load(std::memory_order_acquire);
        bool err = false;

        while (t != h) {
            const int n = formatCsv(ring_[t & (cfg::LOG_RING_N - 1)], line, sizeof line);
            if (n > 0) {
                if (clen + (size_t)n > sizeof chunk_) {
                    if (f.write(reinterpret_cast<const uint8_t*>(chunk_), clen) != clen) err = true;
                    clen = 0;
                }
                memcpy(chunk_ + clen, line, n);
                clen += n;
            }
            t++;
            tail_.store(t, std::memory_order_release);   
        }
        if (clen) {
            if (f.write(reinterpret_cast<const uint8_t*>(chunk_), clen) != clen) err = true;
            clen = 0;
        }

        if (s == CLOSING || err || (millis() - t_flush) >= cfg::LOG_FLUSH_MS) {
            f.flush();
            t_flush = millis();
        }
        if (s == CLOSING || err) {          
            f.close();
            opened = false;
            state_.store(DONE);
            continue;
        }
        vTaskDelay(pdMS_TO_TICKS(cfg::LOG_WRITER_PERIOD_MS));
    }
}