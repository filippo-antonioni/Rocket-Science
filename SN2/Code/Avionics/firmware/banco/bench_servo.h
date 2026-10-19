#pragma once
#include <ESP32Servo.h>
#include "config.h"

// Contatore dei comandi inviati, non feedback della posizione fisica.
class BenchServo {
    Servo actuator;
    bool opened=false;
    unsigned openings=0;
    bool connect() {
        if(actuator.attached()) return true;
        ESP32PWM::allocateTimer(0);actuator.setPeriodHertz(50);
        actuator.attach(cfg::servoPin,500,2400);
        return actuator.attached();
    }
public:
    bool close() {
        if(!connect()) return false;
        actuator.write(cfg::servoChiuso);opened=false;openings=0;return true;
    }
    bool open() {
        if(!actuator.attached()) return false;
        if(!opened) {actuator.write(cfg::servoAperto);opened=true;++openings;}
        return true;
    }
    void off() {actuator.detach();opened=false;}
    bool attached() {return actuator.attached();}
    unsigned count() const {return openings;}
};
