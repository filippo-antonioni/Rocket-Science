#pragma once
#include <Arduino.h>
#include <LittleFS.h>
#include <esp_heap_caps.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include "config.h"
#include "log_format.h"
#include "ram_capture.h"

enum class LogMode {Off=0,Legacy=1,Batch=2,Ram=3};
inline const char* logModeName(LogMode mode) {
    return mode==LogMode::Off?"OFF":mode==LogMode::Legacy?"LEGACY":mode==LogMode::Batch?"BATCH":"RAM";
}
static_assert((cfg::maxLogMs/10+cfg::preCampioni+1)*sizeof(Record)<cfg::ramBufferBytes,"Buffer RAM insufficiente");

class Logger {
    Ring<Record,cfg::preCampioni> pre;
    QueueHandle_t queue=nullptr;
    File file;
    std::atomic<bool> running{false},closing{false},healthy{true},pending{false},capturing{false};
    std::atomic<uint32_t> lost{0},written{0},maxWriteUs{0},maxFlushUs{0},maxQueue{0};
    std::atomic<uint32_t> stored{0},captureFlashOps{0},saveTimeMs{0};
    bool launched=false,stopRequested=false,captureAll=false,limitReached=false;
    bool ramTestPassed=false;
    LogMode mode=LogMode::Legacy;
    uint32_t launchMs=0;
    char path[24]={};
    std::atomic<const char*> error{"NONE"};
    Record batch[16]; // SRAM interna (Logger globale), mai puntatore PSRAM a file.write
    Record* ram=nullptr;
    RamCapture capture;
    LogHeader header;
    static void taskEntry(void* arg) {static_cast<Logger*>(arg)->writer();}
    size_t writeFile(const void* data,size_t size) {
        if(capturing.load()) ++captureFlashOps;
        uint32_t start=micros();size_t n=file.write(static_cast<const uint8_t*>(data),size);
        uint32_t cost=uint32_t(micros()-start);
        if(cost>maxWriteUs.load()) maxWriteUs=cost;
        return n;
    }
    void writeBatch(unsigned count) {
        if(!count) return;
        if(!healthy.load()) {lost+=count;return;}
        size_t size=count*sizeof(Record);
        if(writeFile(batch,size)!=size) {healthy=false;lost+=count;} else written+=count;
    }
    void flushFile(bool close=false) {
        if(capturing.load()) ++captureFlashOps;
        uint32_t start=micros();file.flush();if(close) file.close();
        uint32_t cost=uint32_t(micros()-start);if(cost>maxFlushUs.load()) maxFlushUs=cost;
    }
    bool createFile() {
        if(!mounted) {error="FS_NOT_MOUNTED";return false;}
        uint32_t reserve=mode==LogMode::Ram?cfg::ramRiservaByte:cfg::riservaByte;
        if(LittleFS.totalBytes()-LittleFS.usedBytes()<reserve) {error="FLASH_SPACE";return false;}
        path[0]=0;
        for(unsigned n=0;n<10000;++n) {
            snprintf(path,sizeof(path),"/log%04u.bin",n);
            if(!LittleFS.exists(path)) {file=LittleFS.open(path,FILE_WRITE);break;}
            path[0]=0;
        }
        if(!file || !path[0]) {error="FILE_CREATE";return false;}
        if(file.write(reinterpret_cast<const uint8_t*>(&header),sizeof(header))!=sizeof(header)) {
            file.close();error="HEADER_WRITE";return false;
        }
        file.flush();return true; // creazione/flush PRIMA di avviare l'acquisizione
    }
    void saveRam() {
        uint32_t start=millis();bool ioOk=true;
        size_t count=capture.size();
        for(size_t offset=0;offset<count && ioOk;) {
            unsigned n=0;
            while(n<16 && offset+n<count) {
                batch[n]=capture.at(offset+n);
                if(batch[n].crc!=crc32(&batch[n],offsetof(Record,crc))) {ioOk=false;error="RAM_CRC";break;}
                ++n;
            }
            if(!ioOk) break;
            if(writeFile(batch,n*sizeof(Record))!=n*sizeof(Record)) {ioOk=false;error="RAM_SAVE_WRITE";break;}
            written+=n;offset+=n;vTaskDelay(pdMS_TO_TICKS(1));
        }
        // flush Arduino non restituisce esito: verifica dimensione e poi CRC sul PC.
        flushFile();
        if(ioOk && file.size()!=sizeof(header)+count*sizeof(Record)) {ioOk=false;error="RAM_SAVE_SIZE";}
        file.close();saveTimeMs=uint32_t(millis()-start);
        healthy=ioOk && lost.load()==0;
        pending=!ioOk; // in errore i dati RAM NON vengono riciclati
        running=false; // pubblica completamento solo dopo close
    }
    void writer() {
        uint32_t lastFlush=millis(),batchStart=millis();unsigned count=0;
        for(;;) {
            if(!running.load()) {count=0;lastFlush=batchStart=millis();vTaskDelay(pdMS_TO_TICKS(10));continue;}
            if(mode==LogMode::Ram) {
                if(closing.load()) saveRam();
                else vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }
            if(xQueueReceive(queue,&batch[count],pdMS_TO_TICKS(10))==pdTRUE) {
                if(!count) batchStart=millis();
                ++count;
            }
            if(count && (mode==LogMode::Legacy || count==16 || uint32_t(millis()-batchStart)>=250 || closing.load())) {
                writeBatch(count);count=0;
            }
            const uint32_t interval=mode==LogMode::Legacy?1000:5000;
            if(written.load()>0 && uint32_t(millis()-lastFlush)>=interval) {
                writeBatch(count);count=0;flushFile();lastFlush=millis();
            }
            if(closing.load() && uxQueueMessagesWaiting(queue)==0) {
                writeBatch(count);count=0;flushFile(true);running=false;
            }
        }
    }
    void enqueue(Record r) {
        r.persi=lost.load();r.crc=crc32(&r,offsetof(Record,crc));
        if(xQueueSend(queue,&r,0)!=pdTRUE) {++lost;healthy=false;}
        uint32_t depth=uxQueueMessagesWaiting(queue);if(depth>maxQueue.load()) maxQueue=depth;
    }
public:
    bool mounted=false;
    bool init() {
        mounted=LittleFS.begin(false);
        queue=xQueueCreate(cfg::codaCampioni,sizeof(Record));
        if(!queue) return false;
        return xTaskCreatePinnedToCore(taskEntry,"log_writer",4096,this,1,nullptr,0)==pdPASS;
    }
    bool active() const {return running.load();}
    bool savingRam() const {return mode==LogMode::Ram && active() && closing.load();}
    bool ok() const {return healthy.load();}
    bool unsaved() const {return pending.load();}
    bool ramReady() const {return ramTestPassed;}
    uint32_t ramBytes() const {return ram?cfg::ramBufferBytes:0;}
    uint32_t ramRows() const {return stored.load();}
    uint32_t flashOps() const {return captureFlashOps.load();}
    uint32_t saveMs() const {return saveTimeMs.load();}
    uint32_t dropped() const {return lost.load();}
    uint32_t rows() const {return written.load();}
    uint32_t writeUs() const {return maxWriteUs.load();}
    uint32_t flushUs() const {return maxFlushUs.load();}
    uint32_t depth() const {return queue?uxQueueMessagesWaiting(queue):0;}
    uint32_t peakQueue() const {return maxQueue.load();}
    const char* name() const {return path;}
    const char* lastError() const {return error.load();}
    bool testRam() {
        if(active() || unsaved()) {error="RAM_BUSY_OR_UNSAVED";return false;}
        ramTestPassed=false;stored=0;
        if(!ram) ram=static_cast<Record*>(heap_caps_malloc(cfg::ramBufferBytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
        if(!ram) {error="NO_PSRAM_2MIB";return false;}
        volatile uint32_t* words=reinterpret_cast<volatile uint32_t*>(ram);
        const uint32_t patterns[]={0u,0xffffffffu,0x55555555u,0xaaaaaaaau};
        for(uint32_t pattern:patterns) {
            for(size_t i=0;i<cfg::ramBufferBytes/4;++i) {
                words[i]=pattern^(uint32_t(i)*2654435761u);
                if(i%4096==0) vTaskDelay(pdMS_TO_TICKS(1));
            }
            for(size_t i=0;i<cfg::ramBufferBytes/4;++i) {
                if(words[i]!=(pattern^(uint32_t(i)*2654435761u))) {error="RAM_TEST_MISMATCH";return false;}
                if(i%4096==0) vTaskDelay(pdMS_TO_TICKS(1));
            }
        }
        capture.reset(ram,cfg::ramBufferBytes/sizeof(Record),cfg::preCampioni,false);
        ramTestPassed=true;error="NONE";return true;
    }
    bool open(uint32_t source,LogMode selected=LogMode::Legacy,uint32_t coreMode=0,uint32_t clockMode=0,uint32_t duration=35000) {
        if(active() || unsaved() || !queue) {error="BUSY_OR_RAM_UNSAVED";return false;}
        if(selected==LogMode::Ram && !ramReady()) {error="RUN_RAMTEST_FIRST";return false;}
        mode=selected;captureAll=source!=0;pre.clear();xQueueReset(queue);launched=stopRequested=limitReached=false;
        lost=written=maxWriteUs=maxFlushUs=maxQueue=stored=captureFlashOps=saveTimeMs=0;
        healthy=true;closing=false;capturing=false;error="NONE";strcpy(path,"-");
        if(mode==LogMode::Off) return true;
        header=LogHeader{};header.sorgente=source;header.asse=cfg::asse;header.segno=cfg::segno;
        header.logMode=unsigned(mode);header.coreMode=coreMode;header.clockMode=clockMode;header.durataMs=duration;
        if(!createFile()) return false;
        if(mode==LogMode::Ram) capture.reset(ram,cfg::ramBufferBytes/sizeof(Record),cfg::preCampioni,captureAll);
        capturing=true;running=true;return true;
    }
    void feed(const Record& r,bool liftoff,bool landed) {
        if(!active() || stopRequested) return;
        if(mode==LogMode::Ram) {
            if(liftoff && !launched) {launched=true;launchMs=r.tMs;}
            // Superato il limite: algoritmo continua, MAI salvataggio flash in aria.
            if(!captureAll && launched && uint32_t(r.tMs-launchMs)>=cfg::maxLogMs) limitReached=true;
            Record copy=r;copy.persi=lost.load();copy.crc=crc32(&copy,offsetof(Record,crc));
            if(limitReached || !capture.push(copy,liftoff)) {++lost;healthy=false;}
            stored=capture.size();
            if(!captureAll && landed) finish();
            return;
        }
        if(captureAll) {enqueue(r);return;}
        if(!launched) {
            if(!liftoff) {pre.push(r);return;}
            launched=true;launchMs=r.tMs;
            for(size_t i=0;i<pre.size();++i) enqueue(pre.oldest(i));
            pre.clear();
        }
        enqueue(r);
        if(landed || uint32_t(r.tMs-launchMs)>=cfg::maxLogMs) finish();
    }
    void finish(bool savePad=false) {
        if(!active() || stopRequested) return;
        if(mode!=LogMode::Ram && savePad && !captureAll && !launched) {
            for(size_t i=0;i<pre.size();++i) enqueue(pre.oldest(i));
            pre.clear();
        }
        stopRequested=true;capturing=false;
        if(mode==LogMode::Ram) pending=true;
        closing=true;
    }
    bool retrySave() {
        if(active() || mode!=LogMode::Ram || !unsaved()) {error="NO_PENDING_RAM";return false;}
        if(!createFile()) return false;
        error="NONE";written=0;healthy=true;closing=true;running=true;return true;
    }
    size_t ramFileSize() const {
        return !active() && mode==LogMode::Ram && ramReady()?sizeof(header)+stored.load()*sizeof(Record):0;
    }
    size_t readRamFile(size_t offset,uint8_t* out,size_t size) const {
        size_t total=ramFileSize();if(offset>=total) return 0;
        if(size>total-offset) size=total-offset;
        for(size_t n=0;n<size;++n) {
            size_t pos=offset+n;
            if(pos<sizeof(header)) out[n]=reinterpret_cast<const uint8_t*>(&header)[pos];
            else {pos-=sizeof(header);out[n]=reinterpret_cast<const uint8_t*>(&capture.at(pos/sizeof(Record)))[pos%sizeof(Record)];}
        }
        return size;
    }
};
