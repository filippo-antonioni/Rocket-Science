#pragma once
#include "FreeRTOS.h"
void vTaskDelay(unsigned);
int xTaskCreatePinnedToCore(void(*)(void*),const char*,unsigned,void*,int,void*,int);
