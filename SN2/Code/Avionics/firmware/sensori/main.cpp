#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "sensors.h"
#include "logger.h"
#include "sensor_capture.h"

Sensors sensors;
Logger logger;
SensorCapture capture;
Adafruit_NeoPixel rgb(1,RGB_PIN,NEO_GRB+NEO_KHZ800);
bool loggerReady=false,acquiring=false,reportPending=false,stopped=false;
uint32_t startUs=0,lastUs=0,elapsedFinalUs=0;
char lineBuffer[64];size_t lineUsed=0;bool lineOverflow=false;

void color(uint8_t r,uint8_t g,uint8_t b) {rgb.setPixelColor(0,rgb.Color(r,g,b));rgb.show();}
bool idle() {return !acquiring && !reportPending && !logger.active();}
bool sensorName(const char* path) {
    if(strlen(path)!=13 || strncmp(path,"/sens",5) || strcmp(path+9,".bin")) return false;
    for(unsigned i=5;i<9;++i) if(path[i]<'0'||path[i]>'9') return false;
    return true;
}
void info() {
    Serial.printf("INFO firmware=SENSORI revision=1 psram_bytes=%lu ram_buffer_bytes=%lu ram_test_ok=%u\n",
        (unsigned long)ESP.getPsramSize(),(unsigned long)logger.ramBytes(),unsigned(logger.ramReady()));
    if(idle()) Serial.printf("INFO fs_mounted=%u free=%lu\n",unsigned(logger.mounted),
        (unsigned long)(logger.mounted?LittleFS.totalBytes()-LittleFS.usedBytes():0));
    Serial.printf("STATUS armed=%u log_active=%u pending_report=%u ram_pending=%u rows=%lu ram_records=%lu file=%s log_error=%s\n",
        unsigned(acquiring),unsigned(logger.active()),unsigned(reportPending),unsigned(logger.unsaved()),
        (unsigned long)logger.rows(),(unsigned long)logger.ramRows(),logger.name(),logger.lastError());
}
void finishTest(bool manual) {
    if(!acquiring) return;
    elapsedFinalUs=uint32_t(micros()-startUs);acquiring=false;stopped=manual;
    logger.finish();reportPending=true;color(24,8,0);
    Serial.println("SAVING SENSORI: acquisizione finita; attendere, non scollegare USB");
}
void report() {
    if(!reportPending || logger.active()) return;
    bool logOk=logger.ok() && logger.rows()==capture.rows && logger.dropped()==0 && logger.flashOps()==0;
    Serial.printf("RESULT SENSORI complete=%u elapsed_ms=%lu log=%s timing=%s file=%s rows=%lu ram_records=%lu dropped=%lu flash_ops_capture=%lu save_ms=%lu ram_pending=%u max_dt_us=%lu late=%lu max_poll_us=%lu max_read_us=%lu max_core_us=%lu imu_invalid=%lu baro_fresh=%lu max_imu_gap_us=%lu max_baro_gap_us=%lu numerical=%lu log_error=%s\n",
        unsigned(!stopped),(unsigned long)(elapsedFinalUs/1000),logOk?"PASS":"FAIL",capture.maxDtUs<=20000?"PASS":"CHECK",logger.name(),
        (unsigned long)logger.rows(),(unsigned long)logger.ramRows(),(unsigned long)logger.dropped(),
        (unsigned long)logger.flashOps(),(unsigned long)logger.saveMs(),unsigned(logger.unsaved()),
        (unsigned long)capture.maxDtUs,(unsigned long)capture.late,(unsigned long)capture.maxPollUs,
        (unsigned long)capture.maxReadUs,(unsigned long)capture.maxCoreUs,(unsigned long)capture.imuInvalid,
        (unsigned long)capture.baroFresh,(unsigned long)capture.maxImuGapUs,(unsigned long)capture.maxBaroGapUs,
        (unsigned long)capture.numerical,logger.lastError());
    reportPending=false;color(logOk?0:24,logOk?16:0,logOk?16:0);
}
void beginTest(unsigned seconds) {
    if(!idle()) {Serial.println("ERR BUSY");return;}
    if(logger.unsaved()) {Serial.println("ERR RAM_UNSAVED: GET RAM oppure SAVE prima");return;}
    if(seconds<5 || seconds>180) {Serial.println("ERR DURATA: da 5 a 180 secondi");return;}
    if(!loggerReady) {Serial.println("ERR LOGGER_INIT");return;}
    if(!logger.mounted) {Serial.println("ERR FS_NOT_MOUNTED: non formattare, conserva output");return;}
    if(!logger.ramReady()) {
        Serial.println("PREPARING RAMTEST: controllo buffer PSRAM");
        if(!logger.testRam()) {Serial.printf("ERR RAMTEST %s\n",logger.lastError());return;}
    }
    Serial.println("CALIBRAZIONE: tieni la scheda ferma nella posizione di rampa fino a OK TEST");
    if(!sensors.begin()) {color(24,0,0);Serial.println("ERR SENSORI: controllare I2C; nessuna registrazione avviata");return;}
    // 100 = sensori reali diagnostici; raccolta completa, nessuna finestra PAD.
    if(!logger.open(100,LogMode::Ram,2,1,seconds*1000u)) {Serial.printf("ERR LOG %s\n",logger.lastError());return;}
    capture.reset(seconds);stopped=false;elapsedFinalUs=0;color(0,24,0);
    Serial.printf("OK TEST seconds=%u file=%s; registrazione continua, nessuna macchina a stati\n",seconds,logger.name());
    Serial.flush(); // completa il messaggio PRIMA di far partire il cronometro
    startUs=lastUs=micros();acquiring=true;
}
void download(const char* name) {
    uint8_t buffer[256];
    if(!strcmp(name,"RAM")) {
        size_t size=logger.ramFileSize();if(!size) {Serial.println("ERR NO_RAM_LOG");return;}
        Serial.printf("BEGIN RAM %lu\n",(unsigned long)size);
        for(size_t offset=0;offset<size;) {
            size_t n=logger.readRamFile(offset,buffer,sizeof(buffer));if(!n) break;
            Serial.write(buffer,n);offset+=n;delay(1);
        }
    } else {
        if(!sensorName(name) || !LittleFS.exists(name)) {Serial.println("ERR FILE");return;}
        File file=LittleFS.open(name,FILE_READ);if(!file) {Serial.println("ERR OPEN");return;}
        Serial.printf("BEGIN %s %lu\n",name,(unsigned long)file.size());
        while(file.available()) {size_t n=file.read(buffer,sizeof(buffer));if(!n) break;Serial.write(buffer,n);}
        file.close();
    }
    Serial.println("\nEND");
}
void listFiles() {
    File root=LittleFS.open("/");File file=root.openNextFile();
    while(file) {
        char name[24];const char* raw=file.name();snprintf(name,sizeof(name),"%s%s",raw[0]=='/'?"":"/",raw);
        if(!file.isDirectory() && sensorName(name)) Serial.printf("FILE %s %lu\n",name,(unsigned long)file.size());
        file.close();file=root.openNextFile();
    }
    root.close();Serial.println("END LIST");
}
void command(const char* line) {
    if(!strcmp(line,"INFO") || !strcmp(line,"STATUS")) {info();return;}
    if(!strcmp(line,"STOP")) {finishTest(true);Serial.println("OK STOP: attendere log_active=0");return;}
    if(!idle()) {Serial.println("ERR BUSY: attendere la fine oppure STOP");return;}
    if(!strncmp(line,"TEST ",5)) {
        // Solo interi positivi: niente -1, decimali, overflow o argomenti extra.
        unsigned value=0;const char* p=line+5;
        if(!*p) {Serial.println("ERR DURATA");return;}
        for(;*p;++p) {
            if(*p<'0'||*p>'9'||value>180) {Serial.println("ERR DURATA");return;}
            value=value*10+unsigned(*p-'0');
        }
        beginTest(value);return;
    }
    if(!strcmp(line,"SAVE")) {
        if(logger.retrySave()) Serial.println("OK SAVE: attendere log_active=0");
        else Serial.printf("ERR SAVE %s\n",logger.lastError());
        return;
    }
    if(!strncmp(line,"GET ",4)) {download(line+4);return;}
    if(!strncmp(line,"DELETE ",7)) {
        if(!sensorName(line+7) || !LittleFS.remove(line+7)) Serial.println("ERR DELETE");
        else Serial.println("OK DELETE");
        return;
    }
    if(!strcmp(line,"LIST")) {listFiles();return;}
    Serial.println("ERR COMANDO: INFO | TEST 60 | STOP | LIST | GET /sens0000.bin | GET RAM | SAVE");
}
void console() {
    for(unsigned n=0;n<64 && Serial.available();++n) {
        char c=Serial.read();if(c=='\r') continue;
        if(c=='\n') {
            lineBuffer[lineUsed]=0;
            if(lineOverflow) Serial.println("ERR LINE_TOO_LONG");else if(lineUsed) command(lineBuffer);
            lineUsed=0;lineOverflow=false;
        } else if(lineUsed<sizeof(lineBuffer)-1 && !lineOverflow) lineBuffer[lineUsed++]=c;
        else lineOverflow=true;
    }
}
void setup() {
    Serial.begin(115200);pinMode(cfg::ledDeploy,OUTPUT);digitalWrite(cfg::ledDeploy,LOW);
    rgb.begin();color(0,0,8);loggerReady=logger.init();if(!loggerReady) color(24,0,0);
    Serial.println("READY SENSORI revision=1: TEST 60; acquisizione solo su comando");
}
void loop() {
    if(acquiring) {
        uint32_t before=micros();sensors.poll();capture.observePoll(uint32_t(micros()-before));
        uint32_t now=micros(),wall=uint32_t(now-startUs),dt=uint32_t(now-lastUs);
        if(capture.expired(wall)) finishTest(false);
        else if(dt>=cfg::periodoUs) {
            lastUs=now;
            before=micros();Sample sample=sensors.sample();capture.observeRead(uint32_t(micros()-before));
            before=micros();Record record=capture.step(sample,wall,dt);
            record.coreUs=uint32_t(micros()-before);capture.observeCore(record.coreUs);
            logger.feed(record,false,false);
        }
    }
    console();report();delay(1);
}
