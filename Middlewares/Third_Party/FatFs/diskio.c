/*-----------------------------------------------------------------------*/
/* Low level disk I/O module glue for FatFs / STM32F407 SDIO 4-bit       */
/* Uses HAL_SD driver in polling mode (all blocking calls).              */
/*-----------------------------------------------------------------------*/

#include "ff.h"			/* FatFs declarations */
#include "diskio.h"		/* Disk I/O declarations */
#include "bsp_sdio.h"	/* BSP: hsd1 handle, BSP_SD_Init() */
#include "bsp_rtc.h"	/* BSP: hrtc handle for get_fattime() */

/*-----------------------------------------------------------------------*/
/* Wait until card is ready for data transfer (state == TRANSFER)        */
/* Returns: 0=Timeout, 1=Ready                                           */
/*-----------------------------------------------------------------------*/
static int SD_WaitReady(uint32_t timeout_ms)
{
	uint32_t tickstart = HAL_GetTick();

	while ((uint32_t)(HAL_GetTick() - tickstart) < timeout_ms) {
		if (HAL_SD_GetCardState(&hsd1) == HAL_SD_CARD_TRANSFER) {
			return 1;
		}
		HAL_Delay(1);
	}
	return 0;
}

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
	BYTE pdrv		/* Physical drive number (0) */
)
{
	if (pdrv != 0) return STA_NOINIT;

	/* If card is initialized and in TRANSFER state, it's OK */
	if (hsd1.State == HAL_SD_STATE_READY) {
		HAL_SD_CardStateTypeDef st = HAL_SD_GetCardState(&hsd1);
		if (st == HAL_SD_CARD_TRANSFER || st == HAL_SD_CARD_STANDBY) {
			return 0;
		}
	}
	return STA_NOINIT;
}

/*-----------------------------------------------------------------------*/
/* Initialize Disk Drive                                                 */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
	BYTE pdrv		/* Physical drive number (0) */
)
{
	if (pdrv != 0) return STA_NOINIT;

	/* BSP_SD_Init() handles: GPIO init, SDK clock, card power-on, wide bus */
	if (BSP_SD_Init() != 0) {
		return STA_NOINIT;
	}

	/* Wait for card to reach TRANSFER state */
	if (!SD_WaitReady(1000)) {
		return STA_NOINIT;
	}

	return 0;	/* OK */
}

/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
	BYTE pdrv,		/* Physical drive number (0) */
	BYTE *buff,		/* Data buffer to store read data */
	LBA_t sector,	/* Start sector (LBA) */
	UINT count		/* Sector count (1..128) */
)
{
	HAL_StatusTypeDef status;

	if (pdrv != 0) return RES_PARERR;
	if (count == 0) return RES_PARERR;

	status = HAL_SD_ReadBlocks(&hsd1, buff, sector, count, 5000);
	if (status != HAL_OK) return RES_ERROR;

	/* Polling mode: wait until card is back to TRANSFER state */
	if (!SD_WaitReady(1000)) return RES_ERROR;

	return RES_OK;
}

/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if FF_FS_READONLY == 0

DRESULT disk_write (
	BYTE pdrv,			/* Physical drive number (0) */
	const BYTE *buff,	/* Data to be written */
	LBA_t sector,		/* Start sector (LBA) */
	UINT count			/* Sector count (1..128) */
)
{
	HAL_StatusTypeDef status;

	if (pdrv != 0) return RES_PARERR;
	if (count == 0) return RES_PARERR;

	status = HAL_SD_WriteBlocks(&hsd1, (uint8_t *)buff, sector, count, 5000);
	if (status != HAL_OK) return RES_ERROR;

	/* Polling mode: wait until card is back to TRANSFER state */
	if (!SD_WaitReady(2000)) return RES_ERROR;

	return RES_OK;
}

#endif /* !FF_FS_READONLY */

/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive number (0) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
	HAL_SD_CardInfoTypeDef cardInfo;
	DRESULT res = RES_OK;

	if (pdrv != 0) return RES_PARERR;

	switch (cmd) {

	case CTRL_SYNC:
		/* Flush pending write to physical media.
		 * Step 1: Wait for card to leave PROGRAM state and enter TRANSFER.
		 * Step 2: Brief delay to let internal NAND controller finish cache flush. */
		if (!SD_WaitReady(2000)) res = RES_ERROR;
		HAL_Delay(10);  /* 10ms grace for internal NAND cache flush */
		break;

	case GET_SECTOR_COUNT:
		HAL_SD_GetCardInfo(&hsd1, &cardInfo);
		*(LBA_t *)buff = cardInfo.LogBlockNbr;
		break;

	case GET_SECTOR_SIZE:
		HAL_SD_GetCardInfo(&hsd1, &cardInfo);
		*(WORD *)buff = cardInfo.LogBlockSize;
		break;

	case GET_BLOCK_SIZE:
		HAL_SD_GetCardInfo(&hsd1, &cardInfo);
		*(DWORD *)buff = cardInfo.LogBlockSize;  /* SD card: 1 sector = 1 erase block typically */
		break;

	case CTRL_TRIM:
		/* SD card TRIM: ignore or implement SD erase */
		/* Not implemented for now */
		res = RES_OK;
		break;

	default:
		res = RES_PARERR;
		break;
	}

	return res;
}

/*-----------------------------------------------------------------------*/
/* RTC function for FatFs timestamps (FF_FS_NORTC == 0)                  */
/* Returns packed: bit31:25=year(0..127), bit24:21=mon(1..12),          */
/*                 bit20:16=day(1..31), bit15:11=hour, bit10:5=min,     */
/*                 bit4:0=sec/2                                          */
/*-----------------------------------------------------------------------*/

DWORD get_fattime (void)
{
	RTC_TimeTypeDef sTime;
	RTC_DateTypeDef sDate;

	HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
	HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

	return (DWORD)(sDate.Year + 20) << 25     /* Year: 25 = 2025 */
	     | (DWORD)sDate.Month << 21
	     | (DWORD)sDate.Date << 16
	     | (DWORD)sTime.Hours << 11
	     | (DWORD)sTime.Minutes << 5
	     | (DWORD)sTime.Seconds >> 1;
}
