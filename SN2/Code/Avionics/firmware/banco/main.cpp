#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <ESP32Servo.h>
#include <cstdlib>
#include "config.h"
#include "flight_core.h"
#include "sensors.h"
#include "logger.h"
#include "landing.h"
#include "descent_evidence.h"
#include "bench_metrics.h"
#ifndef BENCH_SERVO
#define BENCH_SERVO 0
#endif
#if BENCH_SERVO
#include "bench_servo.h"
BenchServo benchServo;
#endif
const char* firmwareName() {return FIRMWARE_VOLO?"VOLO":BENCH_SERVO?"BANCO_SERVO":"BANCO";}

Sensors sensori;
Logger logger;
Servo servo;
Adafruit_NeoPixel rgb(1,RGB_PIN,NEO_GRB+NEO_KHZ800);
volo::Parametri parametri;
volo::Kalman kalman;
volo::MacchinaStati macchina;
Scenario scenario=Scenario::Reale;
LogMode logMode=LogMode::Legacy;
bool wallClock=false;
LandingDetector landing;
DescentEvidence descent;
BenchMetrics metrics;
uint32_t startUs=0,modelUs=0,previousModelUs=0,deployWallUs=0,maxCoreUs=0,preDeployMaxDtUs=0,preDeployLate=0;
uint32_t maxImuGapUs=0,maxBaroGapUs=0,lastImuUs=0,lastBaroUs=0;
bool armed=false, orient=false, deployed=false, servoCommand=false, fault=false;
bool loggerReady=false, reportPending=false;
uint32_t lastUs=0, startMs=0, tick=0, late=0, maxDtUs=0, deployMs=0;
unsigned launchCount=0, deployCount=0, rejected=0, badImu=0, phaseMask=1;
char consoleLine[96];size_t consoleUsed=0;bool consoleOverflow=false;

void color(uint8_t r,uint8_t g,uint8_t b) {
    uint32_t next=rgb.Color(r,g,b);
    if(rgb.getPixelColor(0)!=next) {rgb.setPixelColor(0,next);rgb.show();}
}
void showState() {
    if(fault || (armed && !logger.ok())) {color((millis()/150)%2?32:0,0,0);return;}
    if(orient) {color(0,16,16);return;}
    if(!armed && !deployed) {color(0,0,8);return;}
    switch(macchina.fase) {
        case volo::Fase::PAD: color(0,(millis()/500)%2?24:0,0);break;
        case volo::Fase::BOOST: color(30,9,0);break;
        case volo::Fase::COAST: color(25,25,0);break;
        case volo::Fase::DESCENT: color(24,0,24);break;
        case volo::Fase::LANDED: color(0,24,24);break;
    }
}
void deploy() {
    if(deployed) return;
    deployed=true;digitalWrite(cfg::ledDeploy,HIGH);
#if FIRMWARE_VOLO
    if(cfg::abilitaServo && servo.attached()) {servo.write(cfg::servoAperto);servoCommand=true;}
#endif
#if BENCH_SERVO
    servoCommand=benchServo.open();
    if(!servoCommand) fault=true;
#endif
}
void resetAlgorithm() {
    kalman=volo::Kalman{};macchina=volo::MacchinaStati{};landing.reset();descent.reset();metrics=BenchMetrics{};
    modelUs=previousModelUs=deployWallUs=maxCoreUs=preDeployMaxDtUs=preDeployLate=0;
    maxImuGapUs=maxBaroGapUs=lastImuUs=lastBaroUs=0;
    tick=late=maxDtUs=deployMs=0;launchCount=deployCount=rejected=badImu=0;phaseMask=1;
    deployed=servoCommand=fault=false;digitalWrite(cfg::ledDeploy,LOW);
}
bool prepareRun(Scenario selected,LogMode mode=LogMode::Legacy,bool robust=false,bool wall=false) {
    if(armed || logger.active() || reportPending) {Serial.println("ERR BUSY");return false;}
    if(logger.unsaved()) {Serial.println("ERR RAM_UNSAVED");return false;}
#if BENCH_SERVO
    if(selected==Scenario::Reale) {Serial.println("ERR BANCO_SERVO: solo replay sintetici");return false;}
    // La richiusura e l'assestamento precedono acquisizione e cronometro.
    if(!benchServo.close()) {fault=true;Serial.println("ERR SERVO_ATTACH");return false;}
    delay(400);
#endif
    if(!loggerReady || !logger.open(unsigned(selected),mode,unsigned(robust),unsigned(wall),scenarioDurationMs(selected))) {
        Serial.printf("ERR LOG %s; usare INFO e leggere INIZIA_QUI.md\n",logger.lastError());fault=true;return false;
    }
    resetAlgorithm();scenario=selected;orient=false;logMode=mode;wallClock=wall;parametri.robusto=robust;
    Serial.printf("OK RUN %s file=%s mode=%s core=%s clock=%s\n",scenarioName(scenario),logger.name(),logModeName(logMode),robust?"CANDIDATO":"BASE",wall?"WALL":"MODEL");
    armed=true;startMs=millis();startUs=lastUs=micros();return true;
}
bool idle() {return !armed && !orient && !logger.active() && !reportPending;}
bool logName(const char* p) {
    if(strlen(p)!=12 || strncmp(p,"/log",4)!=0 || strcmp(p+8,".bin")!=0) return false;
    for(int i=4;i<8;++i) if(p[i]<'0'||p[i]>'9') return false;
    return true;
}
void info() {
    Serial.printf("INFO revision=4 firmware=%s flash_bytes=%lu psram_bytes=%lu rgb_pin=%d servo_enabled=%u\n",
        firmwareName(),(unsigned long)ESP.getFlashChipSize(),
        (unsigned long)ESP.getPsramSize(),rgb.getPin(),unsigned(BENCH_SERVO || (FIRMWARE_VOLO && cfg::abilitaServo)));
#if BENCH_SERVO
    Serial.printf("INFO servo_pin=%d servo_closed=%d servo_open=%d servo_attached=%u servo_open_commands=%u\n",
        cfg::servoPin,cfg::servoChiuso,cfg::servoAperto,unsigned(benchServo.attached()),benchServo.count());
#endif
    // Anche INFO evita accessi al filesystem durante acquisizione/salvataggio.
    if(armed || logger.active()) Serial.println("INFO fs_stats_deferred=1");
    else Serial.printf("INFO fs_mounted=%u total=%u used=%u free=%u\n",unsigned(logger.mounted),
        unsigned(logger.mounted?LittleFS.totalBytes():0),unsigned(logger.mounted?LittleFS.usedBytes():0),
        unsigned(logger.mounted?LittleFS.totalBytes()-LittleFS.usedBytes():0));
    Serial.printf("INFO psram_free=%lu psram_largest=%lu ram_buffer_bytes=%lu ram_capacity_records=%lu ram_test_ok=%u\n",
        (unsigned long)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
        (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM),
        (unsigned long)logger.ramBytes(),(unsigned long)(logger.ramBytes()/sizeof(Record)),unsigned(logger.ramReady()));
    Serial.printf("STATUS armed=%u phase=%s deploy=%u log_active=%u log_ok=%u rows=%lu dropped=%lu max_dt_us=%lu max_write_us=%lu max_flush_us=%lu max_queue=%lu late=%lu max_core_us=%lu predeploy_max_dt_us=%lu predeploy_late=%lu mode=%s core=%s clock=%s ram_records=%lu ram_pending=%u flash_ops_capture=%lu save_ms=%lu log_error=%s\n",
        unsigned(armed),volo::nomeFase(macchina.fase),unsigned(deployed),unsigned(logger.active()),unsigned(logger.ok()),
        (unsigned long)logger.rows(),(unsigned long)logger.dropped(),(unsigned long)maxDtUs,
        (unsigned long)logger.writeUs(),(unsigned long)logger.flushUs(),(unsigned long)logger.peakQueue(),
        (unsigned long)late,(unsigned long)maxCoreUs,(unsigned long)preDeployMaxDtUs,(unsigned long)preDeployLate,
        logModeName(logMode),parametri.robusto?"CANDIDATO":"BASE",wallClock?"WALL":"MODEL",
        (unsigned long)logger.ramRows(),unsigned(logger.unsaved()),(unsigned long)logger.flashOps(),
        (unsigned long)logger.saveMs(),logger.lastError());
}
void listLogs() {
    File root=LittleFS.open("/");File f=root.openNextFile();
    while(f) {if(!f.isDirectory()) Serial.printf("FILE %s %lu\n",f.name(),(unsigned long)f.size());f.close();f=root.openNextFile();}
    root.close();Serial.println("END LIST");
}
void download(const char* name) {
    if(strcmp(name,"RAM")==0) {
        size_t size=logger.ramFileSize();
        if(!size) {Serial.println("ERR NO_RAM_LOG");return;}
        Serial.printf("BEGIN RAM %lu\n",(unsigned long)size);
        uint8_t buffer[256];
        for(size_t offset=0;offset<size;) {
            size_t n=logger.readRamFile(offset,buffer,sizeof(buffer));
            if(!n) break;
            Serial.write(buffer,n);offset+=n;delay(1);
        }
        Serial.println("\nEND");return;
    }
    if(!logName(name) || !LittleFS.exists(name)) {Serial.println("ERR FILE");return;}
    File f=LittleFS.open(name,FILE_READ);
    if(!f) {Serial.println("ERR OPEN");return;}
    Serial.printf("BEGIN %s %lu\n",name,(unsigned long)f.size());
    uint8_t buffer[256];
    while(f.available()) {
        size_t n=f.read(buffer,sizeof(buffer));if(!n) break;
        Serial.write(buffer,n);
    }
    f.close();Serial.println("\nEND");
}
void eraseLogs() {
    // Riapre la directory dopo ogni rimozione, senza invalidare un iteratore.
    unsigned count=0;
    for(;;) {
        char target[24]={};File root=LittleFS.open("/");File f=root.openNextFile();
        while(f) {
            const char* name=f.name();
            char candidate[24];snprintf(candidate,sizeof(candidate),"%s%s",name[0]=='/'?"":"/",name);
            if(!f.isDirectory() && logName(candidate)) {strncpy(target,candidate,sizeof(target)-1);f.close();break;}
            f.close();f=root.openNextFile();
        }
        root.close();
        if(!target[0]) break;
        if(!LittleFS.remove(target)) {Serial.println("ERR ERASE");return;}
        ++count;
    }
    Serial.printf("OK ERASED %u\n",count);
}
void command(const char* line) {
    if(strcmp(line,"HELP")==0) {
        Serial.println("HELP INFO STATUS LIST GET /log0000.bin ERASE CONFIRM FORMAT CONFIRM");
        Serial.println("RGB 48 | RGB 38 | LED ON | LED OFF | ORIENT | STOP | ARM");
        Serial.println("RAMTEST | SAVE | GET RAM | ARM RAM");
        Serial.println("BANCO_SERVO: SERVO CLOSE | SERVO OPEN | SERVO OFF; ogni RUN richiude prima di partire");
        Serial.println("BANCO: RUN scenario OFF|LEGACY|BATCH|RAM BASE|CANDIDATO MODEL|WALL; DELETE /log0000.bin");
        for(unsigned i=1;i<=scenarioCount;++i) Serial.println(scenarioName(Scenario(i)));
        return;
    }
    if(strcmp(line,"INFO")==0 || strcmp(line,"STATUS")==0) {info();return;}
    if(strcmp(line,"STOP")==0) {
        if(armed && scenario==Scenario::Reale && macchina.fase!=volo::Fase::PAD && macchina.fase!=volo::Fase::LANDED) {
            Serial.println("ERR STOP: volo rilevato");return;
        }
        orient=false;armed=false;logger.finish(true);reportPending=false;
        Serial.println("OK STOP: attendere log_active=0");return;
    }
    if(!idle()) {Serial.println("ERR BUSY: STOP prima, solo PAD/replay");return;}
    if(!strncmp(line,"SERVO ",6)) {
#if BENCH_SERVO
        bool ok=true;
        if(!strcmp(line+6,"CLOSE")) ok=benchServo.close();
        else if(!strcmp(line+6,"OPEN")) {
            if(!benchServo.attached()) {Serial.println("ERR SERVO: eseguire SERVO CLOSE prima");return;}
            ok=benchServo.open();
        } else if(!strcmp(line+6,"OFF")) benchServo.off();
        else {Serial.println("ERR SERVO: CLOSE OPEN OFF");return;}
        if(ok) Serial.printf("OK SERVO attached=%u open_commands=%u\n",unsigned(benchServo.attached()),benchServo.count());
        else Serial.println("ERR SERVO_ATTACH");
#else
        Serial.println("ERR SERVO: caricare banco_servo_psram");
#endif
        return;
    }
    if(strcmp(line,"RAMTEST")==0) {
        Serial.println("RAMTEST in corso: controllo lettura/scrittura 2 MiB");
        if(logger.testRam()) Serial.printf("OK RAMTEST bytes=%lu patterns=4\n",(unsigned long)logger.ramBytes());
        else Serial.printf("ERR RAMTEST %s\n",logger.lastError());
        return;
    }
    if(strcmp(line,"SAVE")==0) {
        if(logger.retrySave()) Serial.println("OK SAVE: attendere log_active=0, poi INFO");
        else Serial.printf("ERR SAVE %s\n",logger.lastError());
        return;
    }
    if(strcmp(line,"GET RAM")==0) {download("RAM");return;}
    if(strcmp(line,"FORMAT CONFIRM")==0) {
        LittleFS.end();bool ok=LittleFS.format();logger.mounted=ok && LittleFS.begin(false);
        fault=!logger.mounted;Serial.println(logger.mounted?"OK FORMAT":"ERR FORMAT");return;
    }
    if(strncmp(line,"DELETE ",7)==0) {
        if(!logger.mounted || !logName(line+7) || !LittleFS.exists(line+7) || !LittleFS.remove(line+7)) Serial.println("ERR DELETE");
        else Serial.println("OK DELETE");
        return;
    }
    if(strcmp(line,"LIST")==0 || strcmp(line,"ERASE CONFIRM")==0 || strncmp(line,"GET ",4)==0) {
        if(!logger.mounted) {Serial.println("ERR FS");return;}
        if(strcmp(line,"LIST")==0) listLogs();
        else if(strcmp(line,"ERASE CONFIRM")==0) eraseLogs();
        else download(line+4);
        return;
    }
    if(strcmp(line,"RGB 38")==0 || strcmp(line,"RGB 48")==0) {
        rgb.clear();rgb.show();rgb.setPin(atoi(line+4));color(24,24,24);
        Serial.println("OK RGB: bianco, poi torna blu; salvare RGB_PIN nel .ini");return;
    }
    if(strcmp(line,"LED ON")==0 || strcmp(line,"LED OFF")==0) {
        digitalWrite(cfg::ledDeploy,strcmp(line,"LED ON")==0?HIGH:LOW);Serial.println("OK LED");return;
    }
    if(strcmp(line,"ORIENT")==0 || strcmp(line,"ARM")==0 || strcmp(line,"ARM RAM")==0) {
#if BENCH_SERVO
        if(strcmp(line,"ORIENT")) {Serial.println("ERR BANCO_SERVO: ARM disabilitato; usare RUN");return;}
#endif
        LogMode realMode=LogMode::Legacy;
#ifdef BOARD_HAS_PSRAM
        realMode=LogMode::Ram;
#endif
        if(strcmp(line,"ARM RAM")==0) realMode=LogMode::Ram;
        if(strcmp(line,"ORIENT")!=0 && logger.unsaved()) {Serial.println("ERR RAM_UNSAVED: SAVE oppure GET RAM");return;}
        if(strcmp(line,"ORIENT")!=0 && realMode==LogMode::Ram && !logger.ramReady()) {
            Serial.println("RAMTEST prima della calibrazione");
            if(!logger.testRam()) {Serial.printf("ERR RAMTEST %s\n",logger.lastError());return;}
        }
        Serial.println("CALIBRAZIONE: fermo nella posizione di rampa, circa 3 secondi");
        if(!sensori.begin()) {fault=true;Serial.println("ERR SENSORI");return;}
        fault=false;
        if(strcmp(line,"ORIENT")==0) {orient=true;lastUs=micros();Serial.println("OK ORIENT: ax ay az forza h; STOP termina");return;}
        // Campioni freschi prima di armare, senza riusare un'accelerazione invalida.
        Sample s;double sum=0;unsigned good=0;uint32_t start=millis();
        while(uint32_t(millis()-start)<600) {sensori.poll();s=sensori.sample();if(s.imuOk){sum+=s.forza;++good;}delay(10);}
        if(good<30 || std::fabs(sum/good-9.80665)>1.0 || uint32_t(millis()-sensori.lastBaroMs)>200) {
            fault=true;Serial.println("ERR ORIENTAMENTO/SENSORI: controllare asse, segno e posizione ferma");return;
        }
#if FIRMWARE_VOLO
        if(cfg::abilitaServo) {
            ESP32PWM::allocateTimer(0);servo.setPeriodHertz(50);servo.attach(cfg::servoPin,500,2400);
            if(!servo.attached()) {fault=true;Serial.println("ERR SERVO");return;}
            servo.write(cfg::servoChiuso);
        }
#endif
        // Mantiene BASE in acquisizione reale; CANDIDATO e' una variante da valutare al banco.
        prepareRun(Scenario::Reale,realMode,false,true);return;
    }
    if(strncmp(line,"RUN ",4)==0) {
#if FIRMWARE_VOLO
        Serial.println("ERR RUN disabilitato nel firmware VOLO");
#else
        Scenario selected;char name[32],mode[12]="LEGACY",core[12]="BASE",clock[12]="MODEL",extra[2];
        int fields=sscanf(line+4,"%31s %11s %11s %11s %1s",name,mode,core,clock,extra);
        if(fields<1 || fields>4 || !parseScenario(name,selected)) {Serial.println("ERR SCENARIO/ARGOMENTI");return;}
        if((strcmp(mode,"OFF") && strcmp(mode,"LEGACY") && strcmp(mode,"BATCH") && strcmp(mode,"RAM")) ||
           (strcmp(core,"BASE") && strcmp(core,"CANDIDATO")) || (strcmp(clock,"MODEL") && strcmp(clock,"WALL"))) {Serial.println("ERR OPZIONI");return;}
        prepareRun(selected,!strcmp(mode,"OFF")?LogMode::Off:!strcmp(mode,"BATCH")?LogMode::Batch:!strcmp(mode,"RAM")?LogMode::Ram:LogMode::Legacy,
                   !strcmp(core,"CANDIDATO"),!strcmp(clock,"WALL"));
#endif
        return;
    }
    Serial.println("ERR COMANDO: HELP (maiuscole, invio finale)");
}
void console() {
    // Limite per ciclo, niente String e niente letture seriali con timeout bloccante.
    for(int n=0;n<64 && Serial.available();++n) {
        char c=Serial.read();if(c=='\r') continue;
        if(c=='\n') {
            consoleLine[consoleUsed]=0;
            if(consoleOverflow) Serial.println("ERR LINE TOO LONG");
            else if(consoleUsed) command(consoleLine);
            consoleUsed=0;consoleOverflow=false;
        } else if(consoleUsed<sizeof(consoleLine)-1 && !consoleOverflow) consoleLine[consoleUsed++]=c;
        else consoleOverflow=true;
    }
}
void testReport() {
    if(!reportPending || logger.active()) return;
    const bool pass=metrics.algorithmOk(scenario,macchina);
    const bool logOk=logger.ok() && logger.dropped()==0;
    Serial.printf("RESULT %s algorithm=%s accuracy=%s landing=%s log=%s timing=%s deploy_ms=%lu deploy_wall_us=%lu count=%u phase=%s file=%s mode=%s core=%s clock=%s max_dt_us=%lu predeploy_max_dt_us=%lu late=%lu predeploy_late=%lu max_core_us=%lu max_write_us=%lu max_flush_us=%lu max_queue=%lu rows=%lu dropped=%lu h_error_deploy=%.3f v_error_deploy=%.3f max_pre_h_error=%.3f max_pre_v_error=%.3f land_ms=%lu land_h_error=%.3f early_landing=%u max_imu_gap_us=%lu max_baro_gap_us=%lu numerical=%u ram_records=%lu ram_pending=%u flash_ops_capture=%lu save_ms=%lu log_error=%s servo_command=%u\n",
        scenarioName(scenario),pass?"PASS":"CHECK",metrics.accuracyOk()?"PASS":"CHECK",
        padScenario(scenario)?"SKIP":metrics.landingOk(scenario)?"PASS":"CHECK",
        logMode==LogMode::Off?"SKIP":logOk?"PASS":"FAIL",maxDtUs<=20000?"PASS":"CHECK",
        (unsigned long)deployMs,(unsigned long)deployWallUs,deployCount,volo::nomeFase(macchina.fase),logger.name(),
        logModeName(logMode),parametri.robusto?"CANDIDATO":"BASE",wallClock?"WALL":"MODEL",
        (unsigned long)maxDtUs,(unsigned long)preDeployMaxDtUs,(unsigned long)late,(unsigned long)preDeployLate,
        (unsigned long)maxCoreUs,(unsigned long)logger.writeUs(),(unsigned long)logger.flushUs(),(unsigned long)logger.peakQueue(),
        (unsigned long)logger.rows(),(unsigned long)logger.dropped(),metrics.deployHError,metrics.deployVError,
        metrics.maxPreHError,metrics.maxPreVError,(unsigned long)metrics.landMs,metrics.landHError,unsigned(metrics.earlyLanding),
        (unsigned long)maxImuGapUs,(unsigned long)maxBaroGapUs,metrics.numerical,
        (unsigned long)logger.ramRows(),unsigned(logger.unsaved()),(unsigned long)logger.flashOps(),
        (unsigned long)logger.saveMs(),logger.lastError(),unsigned(servoCommand));
    reportPending=false;
}
void setup() {
    Serial.begin(115200);pinMode(cfg::ledDeploy,OUTPUT);digitalWrite(cfg::ledDeploy,LOW);
    pinMode(0,INPUT_PULLUP); // BOOT, usato soltanto dopo l'avvio nel firmware VOLO
    rgb.begin();color(0,0,8);
    loggerReady=logger.init();fault=!loggerReady;
    Serial.printf("READY %s; HELP per comandi. Disarmato, PWM servo inattivo.\n",firmwareName());
    lastUs=micros();
}
void loop() {
#if FIRMWARE_VOLO
    // Sulla rampa: dopo il boot normale, tenere BOOT premuto 2 s per ARM.
    // Mai tenere BOOT premuto durante reset/accensione: entrerebbe nel bootloader.
    static bool pressing=false,handled=false;
    static uint32_t pressedMs=0;
    if(digitalRead(0)==LOW) {
        if(!pressing) {pressing=true;pressedMs=millis();}
        if(!handled && uint32_t(millis()-pressedMs)>=2000) {
            handled=true;if(idle()) command("ARM");
        }
    } else {pressing=handled=false;}
#endif
    if((armed && scenario==Scenario::Reale) || orient) sensori.poll();
    uint32_t now=micros(),elapsed=uint32_t(now-lastUs);
    if((armed || orient) && elapsed>=cfg::periodoUs) {
        lastUs=now;
        if(orient) {
            Sample s=sensori.sample();
            static unsigned n=0;
            if(++n%20==0) Serial.printf("ORIENT ax=%.3f ay=%.3f az=%.3f forza=%.3f h=%.2f valid=%u\n",
                s.raw[0]/2048.0*parametri.g,s.raw[1]/2048.0*parametri.g,s.raw[2]/2048.0*parametri.g,
                s.forza,s.quota,unsigned(s.imuOk));
        } else {
            const bool synthetic=scenario!=Scenario::Reale;
            const uint32_t wallUs=uint32_t(now-startUs);
            uint32_t dtUs=synthetic && !wallClock?modelStepUs(scenario,tick):elapsed;
            previousModelUs=modelUs;
            if(synthetic && !wallClock) {if(tick) modelUs+=dtUs;}
            else modelUs=wallUs;
            const uint32_t timeMs=modelUs/1000;
            if(elapsed>15000) {++late;if(!deployed) ++preDeployLate;}
            if(elapsed>maxDtUs) maxDtUs=elapsed;
            if(!deployed && elapsed>preDeployMaxDtUs) preDeployMaxDtUs=elapsed;
            Sample s=synthetic?syntheticAt(scenario,modelUs,previousModelUs):sensori.sample();
            if(s.imuOk) lastImuUs=modelUs;
            if(s.fresh && std::isfinite(s.quota)) lastBaroUs=modelUs;
            if(modelUs-lastImuUs>maxImuGapUs) maxImuGapUs=modelUs-lastImuUs;
            if(modelUs-lastBaroUs>maxBaroGapUs) maxBaroGapUs=modelUs-lastBaroUs;
            uint32_t coreStart=micros();
            auto correction=kalman.step(s.forza,s.quota,s.fresh,dtUs*1e-6,parametri);
            auto before=macchina.fase;
            bool landedEvidence=landing.update(timeMs,before==volo::Fase::DESCENT,s.forza,s.quota,s.fresh);
            bool descentConfirmed=descent.update(timeMs,s.quota,s.fresh);
            auto event=macchina.step(kalman.x[1],s.forza,kalman.x[0],timeMs,parametri,landedEvidence,descentConfirmed);
            if(event.decollo) ++launchCount;
            if(event.apertura) {deploy();deployMs=timeMs;deployWallUs=uint32_t(micros()-startUs);++deployCount;}
            uint32_t coreUs=uint32_t(micros()-coreStart);if(coreUs>maxCoreUs) maxCoreUs=coreUs;
            if(correction==volo::Correzione::Scartata) ++rejected;
            if(!s.imuOk) ++badImu;
            phaseMask |= 1u<<unsigned(macchina.fase);
            if(synthetic) metrics.update(scenario,timeMs,s,kalman,macchina,event,correction);
            Truth truth=truthAt(scenario,timeMs*.001);
            Record r={};r.tMs=timeMs;r.dtUs=dtUs;r.sequenza=tick;
            r.wallUs=wallUs;r.realDtUs=elapsed;r.coreUs=coreUs;r.queueDepth=logger.depth();
            r.trueH=synthetic?truth.h:NAN;r.trueV=synthetic?truth.v:NAN;
            r.forza=s.forza;r.baro=s.quota;r.pressione=s.pressione;
            r.h=kalman.x[0];r.v=kalman.x[1];r.bias=kalman.x[2];
            memcpy(r.raw,s.raw,sizeof(r.raw));r.fase=unsigned(macchina.fase);
            r.flags=(s.imuOk?1:0)|(s.fresh?2:0)|(deployed?4:0)|(servoCommand?8:0)|(synthetic?16:0)|
                (correction==volo::Correzione::ErroreNumerico?32:0);
            r.motivo=unsigned(macchina.motivo);r.correzione=unsigned(correction);r.ritardi=late;
            logger.feed(r,event.decollo,event.atterraggio);
            if(before!=macchina.fase) Serial.printf("EVENT t=%lu wall_us=%lu phase=%s h=%.2f v=%.2f deploy=%u dt_real_us=%lu\n",
                (unsigned long)timeMs,(unsigned long)wallUs,volo::nomeFase(macchina.fase),kalman.x[0],kalman.x[1],unsigned(deployed),(unsigned long)elapsed);
            ++tick;
            if(synthetic && modelUs+modelStepUs(scenario,tick)>=scenarioDurationMs(scenario)*1000) {armed=false;logger.finish(true);reportPending=true;}
            if(!synthetic && event.atterraggio) armed=false;
        }
    }
    static bool savingNotice=false;
    if(logger.savingRam() && !savingNotice) {Serial.println("SAVING RAM: acquisizione terminata, non premere RESET");savingNotice=true;}
    if(savingNotice && !logger.active()) {
        Serial.printf("SAVE_DONE ok=%u ram_pending=%u file=%s\n",unsigned(logger.ok()),unsigned(logger.unsaved()),logger.name());savingNotice=false;
    }
    console();testReport();showState();
    // Evita un busy-loop continuo sul banco; non blocca il passo di 10 ms.
    delay(1);
}
