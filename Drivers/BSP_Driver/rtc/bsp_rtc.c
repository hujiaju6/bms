#include "bsp_rtc.h"
#include <stdio.h>

RTC_HandleTypeDef hrtc;
/* Note: no longer static — FatFs diskio.c uses it for get_fattime() */

/*
 * LSI 标称 32kHz，但实际在 17~47kHz 之间。
 * 预分频: (99+1)*(319+1) = 32000 → 1Hz
 * 如需更高精度可实测 LSI 频率后微调。
 */
#define RTC_ASYNCH_PREDIV  99
#define RTC_SYNCH_PREDIV  319

void BSP_RTC_Init(void)
{
    printf("[RTC] === Init Start ===\r\n");

    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();

    /* 配置 LSI 为 RTC 时钟源 */
    RCC_OscInitTypeDef osc = {0};
    osc.OscillatorType = RCC_OSCILLATORTYPE_LSI;
    osc.LSIState = RCC_LSI_ON;
    osc.PLL.PLLState = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        printf("[RTC] ERROR: LSI start failed!\r\n");
    } else {
        printf("[RTC] LSI configured OK.\r\n");
    }
    __HAL_RCC_RTC_CONFIG(RCC_RTCCLKSOURCE_LSI);
    __HAL_RCC_RTC_ENABLE();
    printf("[RTC] RTC clock source set to LSI, RTC enabled.\r\n");

    /* 绕过 HAL_RTC_Init 直接写寄存器，确保预分频器始终生效 */
    RTC->WPR = 0xCA;
    RTC->WPR = 0x53;

    /* 关键：如果上次遗留 INITF=1，必须先强制退出 */
    if (RTC->ISR & RTC_ISR_INITF) {
        printf("[RTC] WARNING: Stuck in INIT mode, forcing exit...\r\n");
        RTC->ISR &= ~((uint32_t)RTC_ISR_INIT);
        volatile uint32_t t = 0xFFFFF;
        while ((RTC->ISR & RTC_ISR_INITF) && --t);
        printf("[RTC] Exited INIT (timeout=%lu)\r\n", (unsigned long)t);
    }

    /* 进入初始化模式 */
    printf("[RTC] Entering INIT mode...\r\n");
    RTC->ISR |= RTC_ISR_INIT;
    {
        volatile uint32_t timeout = 0xFFFFF;
        while ((RTC->ISR & RTC_ISR_INITF) == 0)
        {
            if (--timeout == 0) {
                printf("[RTC] ERROR: INITF timeout! ISR=0x%08lX\r\n", RTC->ISR);
                break;
            }
        }
        if (timeout > 0) {
            printf("[RTC] INITF set OK (timeout left=%lu)\r\n", (unsigned long)timeout);
        }
    }

    /* 设置预分频器 (必须一次 32 位写入，避免读改写问题) */
    RTC->PRER = ((uint32_t)RTC_ASYNCH_PREDIV << 16)
              | ((uint32_t)RTC_SYNCH_PREDIV);

    /* 设置 24 小时制 */
    RTC->CR &= ~RTC_CR_FMT;

    /* 退出初始化模式 */
    RTC->ISR &= ~((uint32_t)RTC_ISR_INIT);

    /* 重新使能写保护 */
    RTC->WPR = 0xFF;

    /* 保持 HAL 句柄一致（供 HAL 回调使用） */
    hrtc.Instance = RTC;
    hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
    hrtc.Init.AsynchPrediv = RTC_ASYNCH_PREDIV;
    hrtc.Init.SynchPrediv = RTC_SYNCH_PREDIV;
    hrtc.State = HAL_RTC_STATE_READY;

    /* 验证 PRER 写入 */
    printf("[RTC] Init Done. PRER=0x%08lX (expected 0x0063013F)\r\n", RTC->PRER);
}

void BSP_RTC_GetTime(uint8_t *hours, uint8_t *minutes, uint8_t *seconds)
{
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};

    /*
     * 等待 RSF（影子寄存器同步标志）置位。
     * 复位后 RSF=0，读一次 RTC_TR 触发 APB 同步，然后等待。
     */
    if ((RTC->ISR & RTC_ISR_RSF) == 0)
    {
        volatile uint32_t dummy = RTC->TR;
        (void)dummy;

        uint32_t timeout = 0xFFFF;
        while ((RTC->ISR & RTC_ISR_RSF) == 0)
        {
            if (--timeout == 0) break;
        }
    }

    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN); /* 必须读日期以解锁影子寄存器 */
    *hours   = sTime.Hours;
    *minutes = sTime.Minutes;
    *seconds = sTime.Seconds;
}

void BSP_RTC_SetTime(uint8_t hours, uint8_t minutes, uint8_t seconds)
{
    RTC_TimeTypeDef sTime = {0};
    sTime.Hours   = hours;
    sTime.Minutes = minutes;
    sTime.Seconds = seconds;

    HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
}
