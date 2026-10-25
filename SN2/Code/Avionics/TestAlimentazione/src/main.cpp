#include <Arduino.h>
#include <LittleFS.h> // Explicit include for PlatformIO's dependency finder.
#include <Adafruit_NeoPixel.h>
#include <esp_system.h>
#include "sensors.h"
#include "logger.h"
#include "sensor_capture.h"
#include "power_sequence.h"
#include "power_servo.h"

enum class Stage {Wait,Held,Prepare,Capture,Saving,Done,Error};
Sensors sensors;
Logger logger;
SensorCapture capture;
PowerServo servo;
powerTest::Sequence sequence;
Adafruit_NeoPixel rgb(1,RGB_PIN,NEO_GRB+NEO_KHZ800);
Stage stage=Stage::Wait;
bool loggerReady=false,servoReady=false,servoOk=true,reportPending=false,manualStop=false;
uint32_t bootMs=0,startUs=0,lastUs=0,elapsedFinalUs=0,maxServoUs=0;
uint32_t eventWallUs[powerTest::actionCount]={};
uint32_t eventCostUs[powerTest::actionCount]={};
esp_reset_reason_t bootReason;
char commandLine[64]={};size_t commandUsed=0;bool commandOverflow=false;
char summary[2500]={};

const char* stageName() {
    switch(stage) {
    case Stage::Wait:return "WAIT";case Stage::Held:return "HOLD";
    case Stage::Prepare:return "PREPARE";case Stage::Capture:return "CAPTURE";
    case Stage::Saving:return "SAVING";case Stage::Done:return "DONE";
    default:return "ERROR";
    }
}
const char* resetName() {
    switch(bootReason) {
    case ESP_RST_POWERON:return "POWERON";case ESP_RST_BROWNOUT:return "BROWNOUT";
    case ESP_RST_SW:return "SOFTWARE";case ESP_RST_PANIC:return "PANIC";
    case ESP_RST_INT_WDT:case ESP_RST_TASK_WDT:case ESP_RST_WDT:return "WATCHDOG";
    default:return "OTHER";
    }
}
void color(uint8_t r,uint8_t g,uint8_t b) {
    rgb.setPixelColor(0,rgb.Color(r,g,b));rgb.show();
}
bool idle() {return stage!=Stage::Prepare && stage!=Stage::Capture && !logger.active();}
bool ownPath(const char* name) {
    size_t n=strlen(name);
    if((n!=12 && n!=13) || strncmp(name,"/pwr",4)) return false;
    for(unsigned i=4;i<8;++i) if(name[i]<'0'||name[i]>'9') return false;
    return !strcmp(name+8,".bin") || !strcmp(name+8,".json");
}
void error(const char* message) {
    stage=Stage::Error;color(24,0,0);Serial.printf("ERR %s\n",message);
}
void info() {
    Serial.printf("INFO firmware=POWER_TEST revision=1 reset_reason=%s reset_code=%d psram_bytes=%lu servo_pin=%d axis=%d sign=%d voltage_measured=0\n",
        resetName(),int(bootReason),(unsigned long)ESP.getPsramSize(),cfg::servoPin,cfg::asse,cfg::segno);
    Serial.printf("STATUS stage=%s armed=%u log_active=%u ram_pending=%u rows=%lu ram_records=%lu commands=%u file=%s log_error=%s\n",
        stageName(),unsigned(stage==Stage::Capture),unsigned(logger.active()),unsigned(logger.unsaved()),
        (unsigned long)logger.rows(),(unsigned long)logger.ramRows(),sequence.completed(),logger.name(),logger.lastError());
}
void finish(bool manually) {
    if(stage!=Stage::Capture) return;
    elapsedFinalUs=uint32_t(micros()-startUs);manualStop=manually;
    // PWM remains active at the last commanded position during flash saving.
    logger.finish();stage=Stage::Saving;reportPending=true;color(24,8,0);
    Serial.println("SAVING POWER: acquisizione finita; non spegnere fino al LED verde o rosso");
}
bool writeSummary() {
    char path[24];size_t base=strlen(logger.name());
    if(base<4) return false;
    snprintf(path,sizeof(path),"%.*s.json",int(base-4),logger.name());
    File out=LittleFS.open(path,FILE_WRITE);
    if(!out) return false;
    size_t n=strlen(summary);bool ok=out.write(reinterpret_cast<const uint8_t*>(summary),n)==n;
    out.flush();ok=ok && out.size()==n;out.close();return ok;
}
void report() {
    if(!reportPending || logger.active()) return;
    bool logOk=logger.ok() && !logger.unsaved() && logger.rows()==capture.rows &&
               logger.dropped()==0 && logger.flashOps()==0;
    bool complete=!manualStop && sequence.completed()==powerTest::actionCount;
    int n=snprintf(summary,sizeof(summary),
        "{\"firmware\":\"POWER_TEST\",\"revision\":1,\"file\":\"%s\",\"complete\":%u,"
        "\"elapsed_ms\":%lu,\"log\":\"%s\",\"timing\":\"%s\",\"servo\":\"%s\","
        "\"voltage_measured\":false,\"reset_reason\":\"%s\",\"reset_code\":%d,"
        "\"rows\":%lu,\"ram_records\":%lu,\"dropped\":%lu,\"flash_ops_capture\":%lu,"
        "\"save_ms\":%lu,\"ram_pending\":%u,\"max_dt_us\":%lu,\"late\":%lu,"
        "\"max_poll_us\":%lu,\"max_read_us\":%lu,\"max_core_us\":%lu,\"max_servo_us\":%lu,"
        "\"imu_invalid\":%lu,\"imu_driver_errors\":%lu,\"imu_empty\":%lu,\"last_imu_error\":%d,"
        "\"baro_fresh\":%lu,\"max_imu_gap_us\":%lu,\"max_baro_gap_us\":%lu,\"numerical\":%lu,"
        "\"commands\":%u,\"event_wall_us\":[%lu,%lu,%lu],\"event_cost_us\":[%lu,%lu,%lu],"
        "\"log_error\":\"%s\"}\n",
        logger.name(),unsigned(complete),(unsigned long)(elapsedFinalUs/1000),logOk?"PASS":"FAIL",
        capture.maxDtUs<=20000?"PASS":"CHECK",servoOk && complete?"PASS":"CHECK",
        resetName(),int(bootReason),(unsigned long)logger.rows(),(unsigned long)logger.ramRows(),
        (unsigned long)logger.dropped(),(unsigned long)logger.flashOps(),(unsigned long)logger.saveMs(),
        unsigned(logger.unsaved()),(unsigned long)capture.maxDtUs,(unsigned long)capture.late,
        (unsigned long)capture.maxPollUs,(unsigned long)capture.maxReadUs,(unsigned long)capture.maxCoreUs,
        (unsigned long)maxServoUs,(unsigned long)capture.imuInvalid,(unsigned long)sensors.imuDriverErrors,
        (unsigned long)sensors.imuEmpty,sensors.lastImuError,(unsigned long)capture.baroFresh,
        (unsigned long)capture.maxImuGapUs,(unsigned long)capture.maxBaroGapUs,(unsigned long)capture.numerical,
        sequence.completed(),(unsigned long)eventWallUs[0],(unsigned long)eventWallUs[1],(unsigned long)eventWallUs[2],
        (unsigned long)eventCostUs[0],(unsigned long)eventCostUs[1],(unsigned long)eventCostUs[2],logger.lastError());
    bool summaryOk=n>0 && size_t(n)<sizeof(summary) && writeSummary();
    reportPending=false;
    bool ok=logOk && complete && servoOk && summaryOk;
    stage=ok?Stage::Done:Stage::Error;color(ok?0:24,ok?20:0,0);
    Serial.printf("RESULT POWER %s",summary);
    if(!summaryOk) Serial.println("ERR SUMMARY_SAVE: scaricare INFO, GET RAM e file binario prima di spegnere");
    Serial.printf("FINISHED POWER stage=%s; il risultato non e una misura dei 5V\n",stageName());
}
void beginTest() {
    if(!idle() || stage==Stage::Done || capture.rows) {Serial.println("ERR SINGLE_RUN_PER_BOOT");return;}
    stage=Stage::Prepare;color(12,0,12);
    if(!loggerReady || !logger.mounted) {error("LOGGER_OR_FS: non formattare");return;}
    if(logger.unsaved()) {error("RAM_UNSAVED");return;}
    if(!logger.testRam()) {error(logger.lastError());return;}
    if(!servoReady || !servo.write(false)) {servoOk=false;error("PWM_INIT_OR_CLOSE");return;}
    delay(500); // Initial closing is OUTSIDE acquisition and timing statistics.
    Serial.println("CALIBRAZIONE: scheda ferma, asse -X verticale come in rampa");
    if(!sensors.begin()) {error("SENSORI_I2C");return;}
    if(!logger.open(101,LogMode::Ram,2,1,powerTest::durationSeconds*1000u)) {error(logger.lastError());return;}
    capture.reset(powerTest::durationSeconds);sequence=powerTest::Sequence{};
    Serial.println("OK POWER: 180 secondi; APRI a 60, CHIUDI a 90, APRI a 120; zero scritture flash durante acquisizione");
    color(0,0,20);Serial.flush();startUs=lastUs=micros();stage=Stage::Capture;
}
void download(const char* path) {
    uint8_t buffer[256];
    if(!strcmp(path,"RAM")) {
        size_t size=logger.ramFileSize();if(!size) {Serial.println("ERR NO_RAM");return;}
        Serial.printf("BEGIN RAM %lu\n",(unsigned long)size);
        for(size_t offset=0;offset<size;) {
            size_t n=logger.readRamFile(offset,buffer,sizeof(buffer));if(!n) break;
            Serial.write(buffer,n);offset+=n;delay(1);
        }
    } else {
        if(!ownPath(path) || !LittleFS.exists(path)) {Serial.println("ERR FILE");return;}
        File file=LittleFS.open(path,FILE_READ);if(!file) {Serial.println("ERR OPEN");return;}
        Serial.printf("BEGIN %s %lu\n",path,(unsigned long)file.size());
        while(file.available()) {size_t n=file.read(buffer,sizeof(buffer));if(!n) break;Serial.write(buffer,n);}
        file.close();
    }
    Serial.println("\nEND");
}
void listFiles() {
    File root=LittleFS.open("/");File file=root.openNextFile();
    while(file) {
        char name[24];const char* raw=file.name();snprintf(name,sizeof(name),"%s%s",raw[0]=='/'?"":"/",raw);
        if(!file.isDirectory() && ownPath(name)) Serial.printf("FILE %s %lu\n",name,(unsigned long)file.size());
        file.close();file=root.openNextFile();
    }
    root.close();Serial.println("END LIST");
}
void command(const char* line) {
    if(!strcmp(line,"INFO")) {info();return;}
    if(!strcmp(line,"HOLD")) {
        if(stage==Stage::Wait) {stage=Stage::Held;color(4,4,4);}
        if(!idle()) Serial.println("ERR BUSY: attendere fine test e salvataggio");
        else Serial.println("OK HOLD: avvio automatico disattivato fino a RESET");
        return;
    }
    if(!strcmp(line,"STOP")) {finish(true);Serial.println("OK STOP: attendere salvataggio");return;}
    if(!idle()) {Serial.println("ERR BUSY");return;}
    if(!strcmp(line,"START")) {beginTest();return;}
    if(!strcmp(line,"LIST")) {listFiles();return;}
    if(!strncmp(line,"GET ",4)) {download(line+4);return;}
    if(!strcmp(line,"SAVE")) {
        if(logger.retrySave()) {stage=Stage::Saving;reportPending=true;color(24,8,0);Serial.println("OK SAVE");}
        else Serial.printf("ERR SAVE %s\n",logger.lastError());
        return;
    }
    if(!strncmp(line,"DELETE ",7)) {
        if(!ownPath(line+7) || !LittleFS.remove(line+7)) Serial.println("ERR DELETE");
        else Serial.println("OK DELETE");
        return;
    }
    Serial.println("ERR COMANDO: HOLD INFO LIST GET /pwr0000.bin GET RAM SAVE STOP START");
}
void console() {
    for(unsigned n=0;n<64 && Serial.available();++n) {
        char c=Serial.read();if(c=='\r') continue;
        if(c=='\n') {
            commandLine[commandUsed]=0;
            if(commandOverflow) Serial.println("ERR LINE_TOO_LONG");else if(commandUsed) command(commandLine);
            commandUsed=0;commandOverflow=false;
        } else if(commandUsed<sizeof(commandLine)-1 && !commandOverflow) commandLine[commandUsed++]=c;
        else commandOverflow=true;
    }
}
void setup() {
    Serial.begin(115200);bootReason=esp_reset_reason();
    pinMode(cfg::ledDeploy,OUTPUT);digitalWrite(cfg::ledDeploy,LOW);
    rgb.begin();color(12,8,0);loggerReady=logger.init();servoReady=servo.begin();bootMs=millis();
    Serial.println("READY POWER_TEST revision=1: avvio automatico tra 30 s; HOLD annulla; nessuna misura ADC dei 5V");
    info();
}
void loop() {
    console();
    if(stage==Stage::Wait && uint32_t(millis()-bootMs)>=powerTest::startupMs) beginTest();
    if(stage==Stage::Capture) {
        uint32_t before=micros();sensors.poll();capture.observePoll(uint32_t(micros()-before));
        uint32_t now=micros(),wall=uint32_t(now-startUs),dt=uint32_t(now-lastUs);
        if(capture.expired(wall)) finish(false);
        else if(dt>=cfg::periodoUs) {
            lastUs=now;
            if(const auto* action=sequence.due(wall/1000)) {
                before=micros();bool ok=servo.write(action->open);uint32_t cost=uint32_t(micros()-before);
                if(cost>maxServoUs) maxServoUs=cost;
                if(!ok) {servoOk=false;finish(true);}
                else {
                    unsigned i=sequence.completed();eventWallUs[i]=uint32_t(micros()-startUs);eventCostUs[i]=cost;
                    sequence.acknowledge();color(0,action->open?16:0,20);
                }
            }
            if(stage==Stage::Capture) {
                before=micros();Sample sample=sensors.sample();capture.observeRead(uint32_t(micros()-before));
                before=micros();Record record=capture.step(sample,wall,dt);
                record.coreUs=uint32_t(micros()-before);capture.observeCore(record.coreUs);
                record.flags|=sequence.isOpen()?8:0;
                record.motivo=uint8_t(sequence.completed()); // 0 baseline; 1 open; 2 closed; 3 open.
                logger.feed(record,false,false);
            }
        }
    }
    report();delay(1);
}
