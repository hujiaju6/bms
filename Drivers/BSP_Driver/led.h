#ifndef __LED_H__
#define __LED_H__
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"

typedef enum{
	On = 1,
	Off = 2
}LED_STATE;

void LED_Ctrl(uint8_t Ctrl);
void LED_Process(void *params);

#endif
