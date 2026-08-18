/*-----------------------------------------------------------------------*/
/* bsp_sdio.h - SDIO BSP Driver for STM32F407 + SD Card (4-bit mode)    */
/*-----------------------------------------------------------------------*/
#ifndef __BSP_SDIO_H
#define __BSP_SDIO_H

#include "stm32f4xx_hal.h"

/* Exported handle — used by diskio.c and anywhere else */
extern SD_HandleTypeDef hsd1;

/* HAL_SD_MspInit/MspDeInit — weak overrides, called by HAL internally */
void HAL_SD_MspInit(SD_HandleTypeDef *hsd);
void HAL_SD_MspDeInit(SD_HandleTypeDef *hsd);

/* User-facing init */
int BSP_SD_Init(void);
int BSP_SD_DeInit(void);

/* Non-blocking card detect — safe to call from FreeRTOS tasks */
int BSP_SD_IsCardPresent(void);

/* BSP IRQ helpers — called from stm32f4xx_it.c interrupt handlers */
void BSP_SDIO_IRQHandler(void);
void BSP_SDIO_DMA_Tx_IRQHandler(void);
void BSP_SDIO_DMA_Rx_IRQHandler(void);

#endif /* __BSP_SDIO_H */
