#pragma once
// =====================================================================
//  SEZIONE 3 - Servo paracadute: 135 deg (armato) -> 90 deg (aperto)
//  Il codice MATLAB NON pilotava alcun servo (esponeva solo flag/deploy).
// =====================================================================
#include <Arduino.h>
#include <ESP32Servo.h>
#include "config.h"

#if SERVO_PIN < 0
#error "DA COMPLETARE: imposta SERVO_PIN in config.h"
#endif

class ParachuteServo {
public:
    // already_deployed = true dopo un reset in volo con paracadute gia' comandato:
    // il servo NON deve tornare a 135 (richiuderebbe il rilascio).
    void begin(bool already_deployed = false) {
        ESP32PWM::allocateTimer(0);
        servo_.setPeriodHertz(50);
        servo_.attach(SERVO_PIN, cfg::SERVO_US_MIN, cfg::SERVO_US_MAX);
        servo_.write(already_deployed ? cfg::SERVO_ANGOLO_APERTO : cfg::SERVO_ANGOLO_ARMATO);
        deployed_ = already_deployed;
    }
    void deploy() {
        if (deployed_) return;
        servo_.write(cfg::SERVO_ANGOLO_APERTO);
        deployed_ = true;
    }
    bool deployed() const { return deployed_; }

private:
    Servo servo_;
    volatile bool deployed_ = false;
};
