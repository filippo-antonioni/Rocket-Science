#pragma once
#include "FreeRTOS.h"
#include <cstddef>
QueueHandle_t xQueueCreate(unsigned,size_t);int xQueueReceive(QueueHandle_t,void*,unsigned);
int xQueueSend(QueueHandle_t,const void*,unsigned);unsigned uxQueueMessagesWaiting(QueueHandle_t);
void xQueueReset(QueueHandle_t);
