#pragma once
#include <cstdint>
#define NEO_GRB 1
#define NEO_KHZ800 2
struct Adafruit_NeoPixel {
 Adafruit_NeoPixel(int,int,int);void begin();void show();void clear();int getPin();void setPin(int);
 uint32_t Color(uint8_t,uint8_t,uint8_t);uint32_t getPixelColor(int);void setPixelColor(int,uint32_t);
};
