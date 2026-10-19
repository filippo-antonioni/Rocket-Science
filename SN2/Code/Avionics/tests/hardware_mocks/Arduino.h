#pragma once
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define INPUT_PULLUP 2
uint32_t millis();uint32_t micros();void delay(uint32_t);
void pinMode(int,int);void digitalWrite(int,int);int digitalRead(int);
struct SerialMock {
 void begin(int);int available();int read();size_t write(const uint8_t*,size_t);
 void println(const char*);
 void flush();
 int printf(const char*,...) __attribute__((format(printf,2,3)));
};
extern SerialMock Serial;
struct EspMock {uint32_t getFlashChipSize();uint32_t getPsramSize();};extern EspMock ESP;
