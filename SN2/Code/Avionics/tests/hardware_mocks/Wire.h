#pragma once
#include <cstdint>
struct TwoWire {
 void begin(int,int);void setClock(int);void setTimeOut(int);
 void beginTransmission(uint8_t);void write(uint8_t);int endTransmission(bool=true);
 int requestFrom(uint8_t,uint8_t);int read();
};extern TwoWire Wire;
