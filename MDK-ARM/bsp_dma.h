#ifndef __BSP_DMA_H
#define __BSP_DMA_H

#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_dma.h"

// 声明全局 DMA 句柄
extern DMA_HandleTypeDef hdma_memtomem_dma2_stream0;

// 初始化函数声明
void LCD_DMA2_Init(void);

#endif