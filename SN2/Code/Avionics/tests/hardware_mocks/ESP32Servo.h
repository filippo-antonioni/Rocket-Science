#pragma once
struct ESP32PWM {static void allocateTimer(int);};
struct Servo {void setPeriodHertz(int);int attach(int,int,int);bool attached();void write(int);};
