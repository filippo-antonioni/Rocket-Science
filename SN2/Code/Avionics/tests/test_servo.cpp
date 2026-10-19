#include "../firmware/banco/bench_servo.h"
#include "../firmware/banco/flight_core.h"
#include "../firmware/banco/scenarios.h"
#include "../firmware/banco/landing.h"
#include "../firmware/banco/descent_evidence.h"
#include <cassert>
#include <vector>
#include <iostream>
std::vector<int> angles;
bool failAttach=false;
void ESP32PWM::allocateTimer(int n){assert(n==0);}
void Servo::setPeriodHertz(int n){assert(n==50);}
int Servo::attach(int pin,int low,int high){assert(pin==18 && low==500 && high==2400);mockAttached=!failAttach;return mockAttached;}
bool Servo::attached(){return mockAttached;}
void Servo::write(int n){assert(mockAttached);angles.push_back(n);}
void Servo::detach(){mockAttached=false;}
int main(){
    BenchServo servo;assert(!servo.open() && angles.empty());
    assert(servo.close() && angles.back()==130);
    assert(servo.open() && servo.open() && angles.size()==2 && angles.back()==90 && servo.count()==1);
    servo.off();assert(!servo.attached() && !servo.open());
    failAttach=true;assert(!servo.close());failAttach=false;
    for(unsigned id=1;id<=scenarioCount;++id){
        Scenario scenario=Scenario(id);angles.clear();assert(servo.close());
        volo::Kalman kalman;volo::MacchinaStati fsm;volo::Parametri p;
        LandingDetector landing;DescentEvidence descent;
        unsigned deploys=0;
        for(unsigned ms=0;ms<scenarioDurationMs(scenario);ms+=10){
            auto sample=syntheticAt(scenario,ms*1000,ms? (ms-10)*1000:0);
            kalman.step(sample.forza,sample.quota,sample.fresh,.01,p);
            bool landed=landing.update(ms,fsm.fase==volo::Fase::DESCENT,sample.forza,sample.quota,sample.fresh);
            bool descending=descent.update(ms,sample.quota,sample.fresh);
            auto event=fsm.step(kalman.x[1],sample.forza,kalman.x[0],ms,p,landed,descending);
            if(event.apertura){assert(servo.open());++deploys;}
        }
        assert(servo.count()==deploys);
        assert(angles.size()==1+deploys && deploys==(padScenario(scenario)?0u:1u));
    }
    std::cout<<"PASS: manuale 130/90, nessun doppio comando, OFF, attach fallito, 18 replay collegati al servo simulato\n";
}
