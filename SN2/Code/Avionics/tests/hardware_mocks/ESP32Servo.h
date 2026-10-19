#pragma once
struct ESP32PWM {static void allocateTimer(int);};
struct Servo {bool mockAttached=false;void setPeriodHertz(int);int attach(int,int,int);bool attached();void write(int);void detach();};
