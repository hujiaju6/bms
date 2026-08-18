#include "led.h"
#include "stdio.h"

void LED_Ctrl(uint8_t Ctrl)
{
	if(Ctrl == On)
	{
		HAL_GPIO_WritePin(GPIOF,GPIO_PIN_9,GPIO_PIN_RESET);
	}
	else if(Ctrl == Off)
	{
		HAL_GPIO_WritePin(GPIOF,GPIO_PIN_9,GPIO_PIN_SET);
	}
}

void LED_Process(void *params)
{
	while(1)
	{
		
//		LED_Ctrl(On);
//		vTaskDelay(500);
//		LED_Ctrl(Off);
		vTaskDelay(500);
	}
}
