#include "lv_port_disp.h"
#include "lvgl.h"
#include "bsp_nt35510_lcd.h"
#include "bsp_dma.h"
#include <stdio.h>

#define MY_DISP_HOR_RES 480
#define MY_DISP_VER_RES 800

lv_disp_drv_t * g_disp_drv = NULL;

static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p)
{
    g_disp_drv = disp_drv;

    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    uint32_t len = w * h;

    NT35510_OpenWindow(area->x1, area->y1, w, h);
    NT35510_Write_Cmd(CMD_SetPixel);

    /* 强行复位 DMA 句柄锁，确保 HAL_DMA_Start_IT 100% 能成功启动 */
    hdma_memtomem_dma2_stream0.State = HAL_DMA_STATE_READY;
    hdma_memtomem_dma2_stream0.Lock = HAL_UNLOCKED;

    HAL_StatusTypeDef status = HAL_DMA_Start_IT(&hdma_memtomem_dma2_stream0, 
                                               (uint32_t)color_p, 
                                               (uint32_t)FSMC_Addr_NT35510_DATA, 
                                               len);

    /* 如果 DMA 依然启动失败，通过串口打印错误码 */
    if (status != HAL_OK)
    {
        // 若打印 status == 1 (HAL_ERROR) 或 2 (HAL_BUSY)，说明 DMA 被拒绝
        printf("DMA Fail Status: %d\r\n", status);

        for(uint32_t i = 0; i < len; i++) {
            *(__IO uint16_t *)(FSMC_Addr_NT35510_DATA) = color_p->full;
            color_p++;
        }
        lv_disp_flush_ready(disp_drv);
    }
}

void lv_port_disp_init(void)
{
    NT35510_Init();

    static lv_disp_draw_buf_t draw_buf_dsc_1;
	/* 1. 定义双缓冲区 */
    static lv_color_t buf_1[MY_DISP_HOR_RES * 20]; 
	  static lv_color_t buf_2[MY_DISP_HOR_RES * 20];
	/* 2. 传入 buf_1 和 buf_2 启动双缓冲 */
   lv_disp_draw_buf_init(&draw_buf_dsc_1, buf_1, buf_2, MY_DISP_HOR_RES * 20);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);

    disp_drv.hor_res = MY_DISP_HOR_RES;
    disp_drv.ver_res = MY_DISP_VER_RES;
    disp_drv.flush_cb = disp_flush;
    disp_drv.draw_buf = &draw_buf_dsc_1;

    lv_disp_drv_register(&disp_drv);
}

