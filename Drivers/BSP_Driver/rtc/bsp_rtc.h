#ifndef __BSP_RTC_H
#define __BSP_RTC_H

#include "stm32f4xx_hal.h"

/* Exported handle so FatFs get_fattime() can read RTC */
extern RTC_HandleTypeDef hrtc;

void BSP_RTC_Init(void);
void BSP_RTC_GetTime(uint8_t *hours, uint8_t *minutes, uint8_t *seconds);
void BSP_RTC_SetTime(uint8_t hours, uint8_t minutes, uint8_t seconds);

#endif /* __BSP_RTC_H */
