#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <ICM42670P.h>
#include "config.h"
#include "scenarios.h"

class Sensors {
    Adafruit_BMP280 bmp;
    ICM42670 imu{Wire,0};
    uint8_t address = 0;
    bool pending = false, fresh = false;
    uint32_t startUs = 0, lastStart = 0, lastPoll = 0;
    double zero = 0, pressure = NAN, altitude = NAN;
    bool read(uint8_t reg, uint8_t& value) {
        Wire.beginTransmission(address); Wire.write(reg);
        if (Wire.endTransmission(false)!=0 || Wire.requestFrom(address,uint8_t(1))!=1) return false;
        value=Wire.read(); return true;
    }
    bool write(uint8_t reg,uint8_t value) {
        Wire.beginTransmission(address); Wire.write(reg); Wire.write(value);
        return Wire.endTransmission()==0;
    }
public:
    bool ready = false;
    uint32_t lastImuMs=0, lastBaroMs=0;
    void poll() {
        if (!address) return;
        uint32_t now=micros();
        if (!pending) {
            if (uint32_t(now-lastStart)<26316) return;
            lastStart=now;
            if (write(0xF4,0x31)) {pending=true;startUs=lastPoll=micros();}
            return;
        }
        if (uint32_t(now-startUs)>100000) {pending=false;return;}
        if (uint32_t(now-startUs)<23000 || uint32_t(now-lastPoll)<1000) return;
        lastPoll=now;
        uint8_t status;
        if (!read(0xF3,status)) {pending=false;return;}
        if (status&0x09) return;
        double p=bmp.readPressure();
        uint8_t chip;
        bool valid=read(0xD0,chip) && chip==0x58 && std::isfinite(p) && p>=30000 && p<=110000;
        pending=false;
        if (valid) {
            pressure=p;altitude=44330*(1-std::pow(p/101325,.190295))-zero;
            fresh=true;lastBaroMs=millis();
        }
    }
    bool begin() {
        ready=false;address=0;zero=0;fresh=pending=false;
        Wire.begin(cfg::sda,cfg::scl);Wire.setClock(100000);Wire.setTimeOut(10);
        if (bmp.begin(0x76)) address=0x76;
        else if (bmp.begin(0x77)) address=0x77;
        else return false;
        if (!write(0xF4,0)) return false;
        delay(30);
        bmp.setSampling(Adafruit_BMP280::MODE_SLEEP,Adafruit_BMP280::SAMPLING_X1,
            Adafruit_BMP280::SAMPLING_X8,Adafruit_BMP280::FILTER_X4,Adafruit_BMP280::STANDBY_MS_1);
        if (imu.begin()!=0 || imu.startAccel(100,16)!=0 || imu.startGyro(100,2000)!=0) return false;
        lastStart=micros()-26316;
        unsigned count=0;double sum=0;uint32_t start=millis();
        while (count<90 && uint32_t(millis()-start)<10000) {
            poll();
            if (fresh) {fresh=false;if (count>=40) sum+=altitude;++count;}
            delay(1);
        }
        if (count!=90) return false;
        zero=sum/50;altitude-=zero;ready=true;
        return true;
    }
    Sample sample() {
        Sample s;
        if (!ready) return s;
        inv_imu_sensor_event_t event={};
        for(int i=0;i<3;++i) event.accel[i]=event.gyro[i]=INT16_MIN;
        int result=imu.getDataFromRegisters(event);
        for(int i=0;i<3;++i) {s.raw[i]=event.accel[i];s.raw[i+3]=event.gyro[i];}
        s.imuOk=result==0 && (event.sensor_mask & (1 << INV_SENSOR_ACCEL)) && event.accel[cfg::asse]!=INT16_MIN;
        s.forza=s.imuOk ? cfg::segno*event.accel[cfg::asse]/2048.0*9.80665 : NAN;
        if(s.imuOk) lastImuMs=millis();
        s.quota=altitude;s.pressione=pressure;s.fresh=fresh;fresh=false;
        return s;
    }
};
