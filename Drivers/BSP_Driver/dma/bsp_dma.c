#include "bsp_dma.h"

DMA_HandleTypeDef hdma_memtomem_dma2_stream0;

void LCD_DMA2_Init(void)
{
    /* 1. 使能 DMA2 时钟 (内存到内存模式必须用 DMA2) */
    __HAL_RCC_DMA2_CLK_ENABLE();

    /* 2. 配置 DMA2 Stream0 参数 */
    hdma_memtomem_dma2_stream0.Instance = DMA2_Stream0;
    hdma_memtomem_dma2_stream0.Init.Channel = DMA_CHANNEL_0;                     // Channel 选择 0
    hdma_memtomem_dma2_stream0.Init.Direction = DMA_MEMORY_TO_MEMORY;           // M2M 模式
    hdma_memtomem_dma2_stream0.Init.PeriphInc = DMA_PINC_ENABLE;                 // 源地址 (SRAM 缓冲区) 递增
    hdma_memtomem_dma2_stream0.Init.MemInc = DMA_MINC_DISABLE;                   // 目的地址 (FSMC LCD_RAM) 固定不递增
    hdma_memtomem_dma2_stream0.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;// 16 位 (RGB565)
    hdma_memtomem_dma2_stream0.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;   // 16 位 (RGB565)
    hdma_memtomem_dma2_stream0.Init.Mode = DMA_NORMAL;                            // 单次模式
    hdma_memtomem_dma2_stream0.Init.Priority = DMA_PRIORITY_HIGH;                 // 高优先级
    hdma_memtomem_dma2_stream0.Init.FIFOMode = DMA_FIFOMODE_ENABLE;              // 开启 FIFO 加速
    hdma_memtomem_dma2_stream0.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;
    hdma_memtomem_dma2_stream0.Init.MemBurst = DMA_MBURST_SINGLE;
    hdma_memtomem_dma2_stream0.Init.PeriphBurst = DMA_PBURST_SINGLE;

    HAL_DMA_Init(&hdma_memtomem_dma2_stream0);

    /* 3. 使能 DMA2 Stream0 中断，优先级设置为 6 */
    HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);
}

/**
  * @brief DMA2 Stream0 中断服务入口
  */
void DMA2_Stream0_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_memtomem_dma2_stream0);
}