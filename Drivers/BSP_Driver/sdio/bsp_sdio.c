/*-----------------------------------------------------------------------*/
/* bsp_sdio.c - SDIO BSP Driver for STM32F407 + SD Card 4-bit           */
/*                                                                       */
/* Pins:  PC8=SDIO_D0, PC9=D1, PC10=D2, PC11=D3, PC12=CK, PD2=CMD      */
/* Clock: PLL48CLK = 48 MHz (PLLQ=7, VCO=336MHz)                        */
/*        SDIO_CK = 48/(CLKDIV+2)                                       */
/*          Init:  CLKDIV=118 → ~400 kHz                                 */
/*          Trans: CLKDIV=0   → ~24 MHz (4-bit safe)                    */
/*-----------------------------------------------------------------------*/

#include "bsp_sdio.h"
#include "stm32f4xx_ll_sdmmc.h"
#include "gpio.h"
#include <stdio.h>

/*-----------------------------------------------------------------------*/
/* Global handle                                                         */
/*-----------------------------------------------------------------------*/
SD_HandleTypeDef hsd1;

/* DMA handles for SDIO TX/RX */
static DMA_HandleTypeDef hdma_sdio_tx;
static DMA_HandleTypeDef hdma_sdio_rx;

/* Initialized flag */
static uint8_t sd_initialized = 0;

/*-----------------------------------------------------------------------*/
/* HAL_SD_MspInit() — Called by HAL_SD_Init()                            */
/* GPIOs, clocks, DMA, NVIC for SDIO                                     */
/*-----------------------------------------------------------------------*/
void HAL_SD_MspInit(SD_HandleTypeDef *hsd)
{
	GPIO_InitTypeDef gpio = {0};

	(void)hsd; /* prevent unused warning */

	/* ---- 1. Enable peripheral clocks ---- */
	__HAL_RCC_SDIO_CLK_ENABLE();
	__HAL_RCC_DMA2_CLK_ENABLE();
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOD_CLK_ENABLE();

	/* ---- 2. GPIO: PC8-PC12, PD2 → AF12 (SDIO) ---- */
	gpio.Mode      = GPIO_MODE_AF_PP;
	gpio.Pull      = GPIO_PULLUP;
	gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
	gpio.Alternate = GPIO_AF12_SDIO;

	/* PC8=D0, PC9=D1, PC10=D2, PC11=D3, PC12=CK */
	gpio.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12;
	HAL_GPIO_Init(GPIOC, &gpio);

	/* PD2=CMD */
	gpio.Pin = GPIO_PIN_2;
	HAL_GPIO_Init(GPIOD, &gpio);

	/* ---- 3. DMA2 Stream3 Channel4 = SDIO TX ---- */
	hdma_sdio_tx.Instance                 = DMA2_Stream3;
	hdma_sdio_tx.Init.Channel             = DMA_CHANNEL_4;
	hdma_sdio_tx.Init.Direction           = DMA_MEMORY_TO_PERIPH;
	hdma_sdio_tx.Init.PeriphInc           = DMA_PINC_DISABLE;
	hdma_sdio_tx.Init.MemInc              = DMA_MINC_ENABLE;
	hdma_sdio_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
	hdma_sdio_tx.Init.MemDataAlignment    = DMA_MDATAALIGN_WORD;
	hdma_sdio_tx.Init.Mode                = DMA_PFCTRL;
	hdma_sdio_tx.Init.Priority            = DMA_PRIORITY_HIGH;
	hdma_sdio_tx.Init.FIFOMode            = DMA_FIFOMODE_ENABLE;
	hdma_sdio_tx.Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_FULL;
	hdma_sdio_tx.Init.MemBurst            = DMA_MBURST_INC4;
	hdma_sdio_tx.Init.PeriphBurst         = DMA_PBURST_INC4;
	HAL_DMA_Init(&hdma_sdio_tx);
	__HAL_LINKDMA(hsd, hdmatx, hdma_sdio_tx);

	/* ---- 4. DMA2 Stream6 Channel4 = SDIO RX ---- */
	hdma_sdio_rx.Instance                 = DMA2_Stream6;
	hdma_sdio_rx.Init.Channel             = DMA_CHANNEL_4;
	hdma_sdio_rx.Init.Direction           = DMA_PERIPH_TO_MEMORY;
	hdma_sdio_rx.Init.PeriphInc           = DMA_PINC_DISABLE;
	hdma_sdio_rx.Init.MemInc              = DMA_MINC_ENABLE;
	hdma_sdio_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
	hdma_sdio_rx.Init.MemDataAlignment    = DMA_MDATAALIGN_WORD;
	hdma_sdio_rx.Init.Mode                = DMA_PFCTRL;
	hdma_sdio_rx.Init.Priority            = DMA_PRIORITY_HIGH;
	hdma_sdio_rx.Init.FIFOMode            = DMA_FIFOMODE_ENABLE;
	hdma_sdio_rx.Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_FULL;
	hdma_sdio_rx.Init.MemBurst            = DMA_MBURST_INC4;
	hdma_sdio_rx.Init.PeriphBurst         = DMA_PBURST_INC4;
	HAL_DMA_Init(&hdma_sdio_rx);
	__HAL_LINKDMA(hsd, hdmarx, hdma_sdio_rx);

	/* ---- 5. NVIC — enable SDIO global interrupt ---- */
	HAL_NVIC_SetPriority(SDIO_IRQn, 6, 0);
	HAL_NVIC_EnableIRQ(SDIO_IRQn);

	/* ---- 6. NVIC — DMA stream interrupts ---- */
	HAL_NVIC_SetPriority(DMA2_Stream3_IRQn, 6, 0);
	HAL_NVIC_EnableIRQ(DMA2_Stream3_IRQn);

	HAL_NVIC_SetPriority(DMA2_Stream6_IRQn, 6, 0);
	HAL_NVIC_EnableIRQ(DMA2_Stream6_IRQn);

	printf("[SDIO] MspInit done.\r\n");
}

/*-----------------------------------------------------------------------*/
/* HAL_SD_MspDeInit() — Called by HAL_SD_DeInit()                        */
/*-----------------------------------------------------------------------*/
void HAL_SD_MspDeInit(SD_HandleTypeDef *hsd)
{
	(void)hsd;

	HAL_NVIC_DisableIRQ(SDIO_IRQn);
	HAL_NVIC_DisableIRQ(DMA2_Stream3_IRQn);
	HAL_NVIC_DisableIRQ(DMA2_Stream6_IRQn);

	HAL_DMA_DeInit(&hdma_sdio_tx);
	HAL_DMA_DeInit(&hdma_sdio_rx);

	__HAL_RCC_SDIO_CLK_DISABLE();

	printf("[SDIO] MspDeInit done.\r\n");
}

/*-----------------------------------------------------------------------*/
/* BSP_SD_Init() — Initialize SDIO → SD card (4-bit)                     */
/* Returns: 0=OK, -1=error                                               */
/*-----------------------------------------------------------------------*/
int BSP_SD_Init(void)
{
	if (sd_initialized) {
		return 0;   /* Already up */
	}

	/* ---- Step 0: SDIO clock source (STM32F407: always PLL48CK from PLLQ) ---- */
	/* On STM32F407, SDIO 48MHz is fixed from main PLL Q output.
	   No clock source selection needed (unlike F42x/F43x with DCKCFGR.SDIOSEL).
	   Clock enable is handled in HAL_SD_MspInit() called by HAL_SD_Init(). */

	/* ---- Step 1: Configure handle ---- */
	hsd1.Instance                 = SDIO;
	hsd1.Init.ClockEdge           = SDIO_CLOCK_EDGE_RISING;
	hsd1.Init.ClockBypass         = SDIO_CLOCK_BYPASS_DISABLE;
	hsd1.Init.ClockPowerSave      = SDIO_CLOCK_POWER_SAVE_DISABLE;
	hsd1.Init.BusWide             = SDIO_BUS_WIDE_1B;        /* Start 1-bit, HAL switches to 4-bit */
	hsd1.Init.HardwareFlowControl = SDIO_HARDWARE_FLOW_CONTROL_DISABLE;
	hsd1.Init.ClockDiv            = SDIO_INIT_CLK_DIV;        /* ~400 kHz for init phase */

	/* ---- Step 2: Call HAL_SD_Init() — detects card, sets 4-bit ---- */
	if (HAL_SD_Init(&hsd1) != HAL_OK) {
		printf("[SDIO] ERROR: HAL_SD_Init failed!\r\n");
		return -1;
	}

	/* ---- Step 3: Explicitly configure 4-bit wide bus ---- */
	if (HAL_SD_ConfigWideBusOperation(&hsd1, SDIO_BUS_WIDE_4B) != HAL_OK) {
		printf("[SDIO] WARNING: 4-bit wide bus config failed, falling back to 1-bit.\r\n");
		/* Continue anyway — 1-bit still works, just slower */
	}

	/* ---- Step 4: Print card info ---- */
	HAL_SD_CardInfoTypeDef info;
	HAL_SD_GetCardInfo(&hsd1, &info);
	printf("[SDIO] Card: Type=%lu, Capacity=%llu MB, BlockSize=%lu\r\n",
	       info.CardType,
	       ((unsigned long long)info.LogBlockNbr * info.LogBlockSize) / 1024 / 1024,
	       (unsigned long)info.LogBlockSize);

	sd_initialized = 1;
	printf("[SDIO] Init OK.\r\n");
	return 0;
}

/*-----------------------------------------------------------------------*/
/* BSP_SD_IsCardPresent() — Fast non-blocking card detect                */
/*                                                                       */
/* Uses raw SDIO register access to send CMD8 (SEND_IF_COND) and check   */
/* for a response within ~200ms timeout. Does NOT call HAL_SD_Init()     */
/* (which would block forever if no card is inserted).                   */
/*                                                                       */
/* Safe to call from FreeRTOS tasks — returns immediately with timeout.  */
/*                                                                       */
/* Returns: 1 = card detected (CMD8 response received)                   */
/*          0 = no card / timeout / error                                */
/*-----------------------------------------------------------------------*/
int BSP_SD_IsCardPresent(void)
{
	uint32_t timeout;
	uint32_t sta;
	int ret = 0;

	/* Already initialized → card is definitely present */
	if (sd_initialized) return 1;

	/* ---- 1. Enable SDIO and GPIO clocks ---- */
	__HAL_RCC_SDIO_CLK_ENABLE();
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOD_CLK_ENABLE();

	/* ---- 2. Configure GPIO: PC8-PC12, PD2 → AF12 (SDIO) ---- */
	{
		GPIO_InitTypeDef gpio_sd = {0};
		gpio_sd.Mode      = GPIO_MODE_AF_PP;
		gpio_sd.Pull      = GPIO_PULLUP;
		gpio_sd.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
		gpio_sd.Alternate = GPIO_AF12_SDIO;

		/* PC8=D0, PC9=D1, PC10=D2, PC11=D3, PC12=CK */
		gpio_sd.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12;
		HAL_GPIO_Init(GPIOC, &gpio_sd);

		/* PD2=CMD */
		gpio_sd.Pin = GPIO_PIN_2;
		HAL_GPIO_Init(GPIOD, &gpio_sd);
	}

	/* ---- 3. Configure SDIO for 400kHz init speed ---- */
	SDIO->CLKCR  = SDIO_CLKCR_CLKEN | (SDIO_INIT_CLK_DIV & SDIO_CLKCR_CLKDIV);
	SDIO->POWER  = SDIO_POWER_PWRCTRL | 0x03;  /* PWRCTRL = 11 = power-on */
	SDIO->CLKCR |= SDIO_CLKCR_CLKEN;           /* re-enable after power */

	/* Small delay for power stabilization (~2ms @ 48MHz ≈ 100k cycles) */
	for (volatile uint32_t d = 0; d < 100000; d++) { __NOP(); }

	/* ---- 4. Send CMD0 (GO_IDLE_STATE) — no response expected ---- */
	SDIO->ARG = 0;
	SDIO->CMD = SDIO_CMD_CPSMEN | 0;           /* CMD0, no response */
	timeout = 0xFFFF;
	while (!(SDIO->STA & SDIO_STA_CMDSENT) && timeout--) { }
	SDIO->ICR = SDIO_STA_CMDSENT;

	/* ---- 5. Send CMD8 (SEND_IF_COND) — expect short response ---- */
	/* Arg: bits [11:8]=1 (VHS=2.7-3.6V), bits [7:0]=0xAA (check pattern) */
	SDIO->ARG = (1U << 8) | 0xAA;
	SDIO->CMD = SDIO_CMD_CPSMEN | SDIO_RESPONSE_SHORT | 8;

	/* ---- 6. Wait for response or timeout (~200ms) ---- */
	timeout = 0xFFFFF;
	while (timeout--) {
		sta = SDIO->STA;
		if (sta & (SDIO_STA_CMDREND | SDIO_STA_CTIMEOUT | SDIO_STA_CCRCFAIL)) {
			break;
		}
	}

	/* ---- 7. Check result ---- */
	if (sta & SDIO_STA_CMDREND) {
		ret = 1;  /* Card responded to CMD8 */
	}

	/* ---- 8. Cleanup: clear all status flags, power off, disable clock ---- */
	SDIO->ICR   = 0xFFFFFFFF;
	SDIO->POWER = 0x00;          /* power-off */
	SDIO->CLKCR = 0x00;          /* disable clock */
	__HAL_RCC_SDIO_CLK_DISABLE();

	return ret;
}

/*-----------------------------------------------------------------------*/
/* BSP_SD_DeInit()                                                       */
/*-----------------------------------------------------------------------*/
int BSP_SD_DeInit(void)
{
	if (!sd_initialized) return 0;

	HAL_SD_DeInit(&hsd1);
	sd_initialized = 0;
	return 0;
}

/*-----------------------------------------------------------------------*/
/* SDIO interrupts — DMA TC handlers for the HAL                         */
/* Call from stm32f4xx_it.c: void SDIO_IRQHandler(void)                  */
/*                           void DMA2_Stream3_IRQHandler(void)          */
/*                           void DMA2_Stream6_IRQHandler(void)          */
/*-----------------------------------------------------------------------*/
void BSP_SDIO_IRQHandler(void)
{
	HAL_SD_IRQHandler(&hsd1);
}

void BSP_SDIO_DMA_Tx_IRQHandler(void)
{
	HAL_DMA_IRQHandler(&hdma_sdio_tx);
}

void BSP_SDIO_DMA_Rx_IRQHandler(void)
{
	HAL_DMA_IRQHandler(&hdma_sdio_rx);
}
