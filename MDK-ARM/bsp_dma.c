#include "bsp_dma.h"
#include "lvgl.h"

DMA_HandleTypeDef hdma_memtomem_dma2_stream0;

/* 声明来自 lv_port_disp.c 的显示驱动指针[cite: 13] */
extern lv_disp_drv_t * g_disp_drv;

/* DMA 传输完成回调函数 */
static void DMA_Complete_Callback(DMA_HandleTypeDef *hdma)
{
    if (g_disp_drv != NULL)
    {
        /* 告诉 LVGL 这一帧 DMA 搬运已经完成，放行下一帧渲染[cite: 13] */
        lv_disp_flush_ready(g_disp_drv);
    }
}

void LCD_DMA2_Init(void)
{
    /* 1. 使能 DMA2 时钟 (内存到内存模式必须用 DMA2)[cite: 8] */
    __HAL_RCC_DMA2_CLK_ENABLE();

    /* 2. 配置 DMA2 Stream0 参数[cite: 8] */
    hdma_memtomem_dma2_stream0.Instance = DMA2_Stream0;
    hdma_memtomem_dma2_stream0.Init.Channel = DMA_CHANNEL_0;                     
    hdma_memtomem_dma2_stream0.Init.Direction = DMA_MEMORY_TO_MEMORY;           
    hdma_memtomem_dma2_stream0.Init.PeriphInc = DMA_PINC_ENABLE;                 
    hdma_memtomem_dma2_stream0.Init.MemInc = DMA_MINC_DISABLE;                   
    hdma_memtomem_dma2_stream0.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_memtomem_dma2_stream0.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;   
    hdma_memtomem_dma2_stream0.Init.Mode = DMA_NORMAL;                            
    hdma_memtomem_dma2_stream0.Init.Priority = DMA_PRIORITY_HIGH;                 
    hdma_memtomem_dma2_stream0.Init.FIFOMode = DMA_FIFOMODE_ENABLE;              
    hdma_memtomem_dma2_stream0.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;
    hdma_memtomem_dma2_stream0.Init.MemBurst = DMA_MBURST_SINGLE;
    hdma_memtomem_dma2_stream0.Init.PeriphBurst = DMA_PBURST_SINGLE;

    HAL_DMA_Init(&hdma_memtomem_dma2_stream0);

    /* 【核心修复】注册 DMA 传输完成回调函数，将 lv_disp_flush_ready 关联进去[cite: 8, 13] */
    HAL_DMA_RegisterCallback(&hdma_memtomem_dma2_stream0, HAL_DMA_XFER_CPLT_CB_ID, DMA_Complete_Callback);

    /* 3. 使能 DMA2 Stream0 中断，优先级设置为 6[cite: 8] */
    HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);
}

/**
  * @brief DMA2 Stream0 中断服务入口[cite: 8]
  */
void DMA2_Stream0_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_memtomem_dma2_stream0);
}