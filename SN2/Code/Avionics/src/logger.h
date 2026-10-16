#pragma once
#include <Arduino.h>
#include <LittleFS.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include "config.h"
#include "log_format.h"

// Il task scrittore possiede il file durante la registrazione.
// Il loop di volo mette record in una coda RAM con attesa ZERO.
// Flash/cache restano risorse condivise: misurare sempre i ritardi sul dispositivo.
class Logger {
    Ring<Record,cfg::preCampioni> pre;
    QueueHandle_t queue=nullptr;
    File file;
    std::atomic<bool> running{false}, closing{false}, healthy{true};
    std::atomic<uint32_t> lost{0}, written{0}, maxWriteUs{0}, maxFlushUs{0};
    bool launched=false, stopRequested=false;
    uint32_t launchMs=0;
    char path[24]={};
    static void taskEntry(void* arg) { static_cast<Logger*>(arg)->writer(); }
    void writer() {
        uint32_t lastFlush=millis();
        for(;;) {
            if (!running.load()) {vTaskDelay(pdMS_TO_TICKS(10));continue;}
            Record r;
            if (xQueueReceive(queue,&r,pdMS_TO_TICKS(10))==pdTRUE) {
                if (healthy.load()) {
                    uint32_t start=micros();size_t n=file.write(reinterpret_cast<const uint8_t*>(&r),sizeof(r));
                    uint32_t cost=uint32_t(micros()-start);
                    if(cost>maxWriteUs.load()) maxWriteUs=cost;
                    if(n!=sizeof(r)) {healthy=false;++lost;} else ++written;
                } else ++lost;
            }
            if (written.load()>0 && uint32_t(millis()-lastFlush)>=1000) {
                uint32_t start=micros();file.flush();uint32_t cost=uint32_t(micros()-start);
                if(cost>maxFlushUs.load()) maxFlushUs=cost;
                lastFlush=millis();
            }
            if (closing.load() && uxQueueMessagesWaiting(queue)==0) {
                uint32_t start=micros();file.flush();file.close();
                uint32_t cost=uint32_t(micros()-start);if(cost>maxFlushUs.load()) maxFlushUs=cost;
                running=false; // pubblicato solo DOPO flush/close
            }
        }
    }
    void enqueue(Record r) {
        r.persi=lost.load();r.crc=crc32(&r,offsetof(Record,crc));
        if (xQueueSend(queue,&r,0)!=pdTRUE) {++lost;healthy=false;}
    }
public:
    bool mounted=false;
    bool init() {
        mounted=LittleFS.begin(false); // mai formattare automaticamente
        queue=xQueueCreate(cfg::codaCampioni,sizeof(Record));
        if(!queue) return false;
        return xTaskCreatePinnedToCore(taskEntry,"log_writer",4096,this,1,nullptr,0)==pdPASS;
    }
    bool active() const {return running.load();}
    bool ok() const {return healthy.load();}
    uint32_t dropped() const {return lost.load();}
    uint32_t rows() const {return written.load();}
    uint32_t writeUs() const {return maxWriteUs.load();}
    uint32_t flushUs() const {return maxFlushUs.load();}
    const char* name() const {return path;}
    bool open(uint32_t source) {
        if(active() || !mounted || !queue || LittleFS.totalBytes()-LittleFS.usedBytes()<cfg::riservaByte) return false;
        path[0]=0;
        for(unsigned n=0;n<10000;++n) {
            snprintf(path,sizeof(path),"/log%04u.bin",n);
            if(!LittleFS.exists(path)) {file=LittleFS.open(path,FILE_WRITE);break;}
            path[0]=0;
        }
        if(!file || !path[0]) return false;
        LogHeader header;header.sorgente=source;header.asse=cfg::asse;header.segno=cfg::segno;
        if(file.write(reinterpret_cast<const uint8_t*>(&header),sizeof(header))!=sizeof(header)) {file.close();return false;}
        file.flush(); // prima di armare, non al decollo
        pre.clear();xQueueReset(queue);launched=stopRequested=false;
        lost=written=maxWriteUs=maxFlushUs=0;healthy=true;closing=false;running=true;
        return true;
    }
    void feed(const Record& r,bool liftoff,bool landed) {
        if(!active() || stopRequested) return;
        if(!launched) {
            if(!liftoff) {pre.push(r);return;}
            launched=true;launchMs=r.tMs;
            // 300 copie in RAM: nessuna scrittura flash sincrona nel loop.
            for(size_t i=0;i<pre.size();++i) enqueue(pre.oldest(i));
            pre.clear();
        }
        enqueue(r);
        if(landed || uint32_t(r.tMs-launchMs)>=cfg::maxLogMs) finish();
    }
    void finish(bool savePad=false) {
        if(savePad && !launched && active() && !stopRequested) {
            for(size_t i=0;i<pre.size();++i) enqueue(pre.oldest(i));
            pre.clear();
        }
        stopRequested=true;closing=true;
    }
};
