#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <ESP32Servo.h>
#include <cstdlib>
#include "config.h"
#include "flight_core.h"
#include "sensors.h"
#include "logger.h"

Sensors sensori;
Logger logger;
Servo servo;
Adafruit_NeoPixel rgb(1,RGB_PIN,NEO_GRB+NEO_KHZ800);
volo::Parametri parametri;
volo::Kalman kalman;
volo::MacchinaStati macchina;
Scenario scenario=Scenario::Reale;
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
}
void resetAlgorithm() {
    kalman=volo::Kalman{};macchina=volo::MacchinaStati{};
    tick=late=maxDtUs=deployMs=0;launchCount=deployCount=rejected=badImu=0;phaseMask=1;
    deployed=servoCommand=fault=false;digitalWrite(cfg::ledDeploy,LOW);
}
bool prepareRun(Scenario selected) {
    if(armed || logger.active() || reportPending) {Serial.println("ERR BUSY");return false;}
    if(!loggerReady || !logger.open(unsigned(selected))) {
        Serial.println("ERR LOG: INFO; se non montata FORMAT CONFIRM; servono 1300000 byte liberi");fault=true;return false;
    }
    resetAlgorithm();scenario=selected;orient=false;
    armed=true;startMs=millis();lastUs=micros();
    Serial.printf("OK RUN %s file=%s\n",scenarioName(scenario),logger.name());return true;
}
bool idle() {return !armed && !orient && !logger.active() && !reportPending;}
bool logName(const char* p) {
    if(strlen(p)!=12 || strncmp(p,"/log",4)!=0 || strcmp(p+8,".bin")!=0) return false;
    for(int i=4;i<8;++i) if(p[i]<'0'||p[i]>'9') return false;
    return true;
}
void info() {
    Serial.printf("INFO firmware=%s flash_bytes=%lu psram_bytes=%lu rgb_pin=%d servo_enabled=%u\n",
        FIRMWARE_VOLO?"VOLO":"BANCO",(unsigned long)ESP.getFlashChipSize(),
        (unsigned long)ESP.getPsramSize(),rgb.getPin(),unsigned(FIRMWARE_VOLO && cfg::abilitaServo));
    Serial.printf("INFO fs_mounted=%u total=%u used=%u free=%u\n",unsigned(logger.mounted),
        unsigned(logger.mounted?LittleFS.totalBytes():0),unsigned(logger.mounted?LittleFS.usedBytes():0),
        unsigned(logger.mounted?LittleFS.totalBytes()-LittleFS.usedBytes():0));
    Serial.printf("STATUS armed=%u phase=%s deploy=%u log_active=%u log_ok=%u rows=%lu dropped=%lu max_dt_us=%lu max_write_us=%lu max_flush_us=%lu\n",
        unsigned(armed),volo::nomeFase(macchina.fase),unsigned(deployed),unsigned(logger.active()),unsigned(logger.ok()),
        (unsigned long)logger.rows(),(unsigned long)logger.dropped(),(unsigned long)maxDtUs,
        (unsigned long)logger.writeUs(),(unsigned long)logger.flushUs());
}
void listLogs() {
    File root=LittleFS.open("/");File f=root.openNextFile();
    while(f) {if(!f.isDirectory()) Serial.printf("FILE %s %lu\n",f.name(),(unsigned long)f.size());f.close();f=root.openNextFile();}
    root.close();Serial.println("END LIST");
}
void download(const char* name) {
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
        Serial.println("BANCO: RUN NOMINALE|RUMORE|PICCO_BARO|BUCO_IMU|BUCO_BARO|TIMEOUT|FALSO_DECOLLO");return;
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
    if(strcmp(line,"FORMAT CONFIRM")==0) {
        LittleFS.end();bool ok=LittleFS.format();logger.mounted=ok && LittleFS.begin(false);
        fault=!logger.mounted;Serial.println(logger.mounted?"OK FORMAT":"ERR FORMAT");return;
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
    if(strcmp(line,"ORIENT")==0 || strcmp(line,"ARM")==0) {
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
        prepareRun(Scenario::Reale);return;
    }
    if(strncmp(line,"RUN ",4)==0) {
#if FIRMWARE_VOLO
        Serial.println("ERR RUN disabilitato nel firmware VOLO");
#else
        Scenario selected;
        if(!parseScenario(line+4,selected)) {Serial.println("ERR SCENARIO");return;}
        prepareRun(selected);
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
    bool pass;
    if(scenario==Scenario::FalsoDecollo) pass=launchCount==0 && deployCount==0 && macchina.fase==volo::Fase::PAD;
    else if(scenario==Scenario::Timeout) pass=launchCount==1 && deployCount==1 && macchina.motivo==volo::MotivoDeploy::Timeout && deployMs>=9000 && deployMs<=9200;
    else pass=launchCount==1 && deployCount==1 && macchina.motivo==volo::MotivoDeploy::Apogeo && deployMs>=7400 && deployMs<=8300 && phaseMask==31;
    if(scenario==Scenario::PiccoBaro) pass=pass && rejected>0;
    if(scenario==Scenario::BucoImu) pass=pass && badImu==30;
    bool logOk=logger.ok() && logger.dropped()==0;
    // Un test matematico PASS non nasconde perdite di log o blocchi lunghi.
    Serial.printf("RESULT %s algorithm=%s log=%s timing=%s deploy_ms=%lu count=%u phase=%s file=%s\n",
        scenarioName(scenario),pass?"PASS":"FAIL",logOk?"PASS":"FAIL",maxDtUs<=20000?"PASS":"CHECK",
        (unsigned long)deployMs,deployCount,volo::nomeFase(macchina.fase),logger.name());
    reportPending=false;
}
void setup() {
    Serial.begin(115200);pinMode(cfg::ledDeploy,OUTPUT);digitalWrite(cfg::ledDeploy,LOW);
    pinMode(0,INPUT_PULLUP); // BOOT, usato soltanto dopo l'avvio nel firmware VOLO
    rgb.begin();color(0,0,8);
    loggerReady=logger.init();fault=!loggerReady;
    Serial.printf("READY %s; HELP per comandi. Disarmato, PWM servo inattivo.\n",FIRMWARE_VOLO?"VOLO":"BANCO");
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
            const uint32_t timeMs=synthetic?tick*10:uint32_t(millis()-startMs);
            const uint32_t dtUs=synthetic?cfg::periodoUs:elapsed;
            if(elapsed>15000) ++late;
            if(elapsed>maxDtUs) maxDtUs=elapsed;
            Sample s=synthetic?syntheticSample(scenario,tick):sensori.sample();
            auto correction=kalman.step(s.forza,s.quota,s.fresh,dtUs*1e-6,parametri);
            auto before=macchina.fase;
            auto event=macchina.step(kalman.x[1],s.forza,kalman.x[0],timeMs,parametri);
            if(event.decollo) ++launchCount;
            if(event.apertura) {deploy();deployMs=timeMs;++deployCount;}
            if(correction==volo::Correzione::Scartata) ++rejected;
            if(!s.imuOk) ++badImu;
            phaseMask |= 1u<<unsigned(macchina.fase);
            Record r={};r.tMs=timeMs;r.dtUs=dtUs;r.sequenza=tick;
            r.forza=s.forza;r.baro=s.quota;r.pressione=s.pressione;
            r.h=kalman.x[0];r.v=kalman.x[1];r.bias=kalman.x[2];
            memcpy(r.raw,s.raw,sizeof(r.raw));r.fase=unsigned(macchina.fase);
            r.flags=(s.imuOk?1:0)|(s.fresh?2:0)|(deployed?4:0)|(servoCommand?8:0)|(synthetic?16:0)|
                (correction==volo::Correzione::ErroreNumerico?32:0);
            r.motivo=unsigned(macchina.motivo);r.correzione=unsigned(correction);r.ritardi=late;
            logger.feed(r,event.decollo,event.atterraggio);
            if(before!=macchina.fase) Serial.printf("EVENT t=%lu phase=%s h=%.2f v=%.2f deploy=%u\n",
                (unsigned long)timeMs,volo::nomeFase(macchina.fase),kalman.x[0],kalman.x[1],unsigned(deployed));
            ++tick;
            if(synthetic && tick>=3500) {armed=false;logger.finish(true);reportPending=true;}
            if(!synthetic && event.atterraggio) armed=false;
        }
    }
    console();testReport();showState();
    // Evita un busy-loop continuo sul banco; non blocca il passo di 10 ms.
    delay(1);
}
