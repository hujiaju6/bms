/*---------------------------------------------------------------------------/
/  FatFs Functional Configurations
/---------------------------------------------------------------------------*/

#define FFCONF_DEF	80386	/* Revision ID for R0.16 */

/*---------------------------------------------------------------------------/
/ Function Configurations
/---------------------------------------------------------------------------*/

#define FF_FS_READONLY	0
/* 0:R/W, 1:R/O. This project needs write for logging */

#define FF_FS_MINIMIZE	0
/* 0:Full API. Keep all API functions for flexibility */

#define FF_USE_STRFUNC	1
/* 1:Enable f_gets, f_putc, f_puts, f_printf */

#define FF_USE_FIND		1
/* 1:Enable f_findfirst, f_findnext for directory traversal */

#define FF_USE_MKFS		1
/* 1:Enable f_mkfs for auto-formatting unformatted SD cards */

#define FF_USE_FASTSEEK	0
/* 0:Disable fast seek. Not needed for logging */

#define FF_USE_EXPAND	0
/* 0:Disable f_expand. Not needed */

#define FF_USE_CHMOD	0
/* 0:Disable f_chmod, f_utime */

#define FF_USE_LABEL	0
/* 0:Disable volume label functions */

#define FF_USE_FORWARD	0
/* 0:Disable f_forward for now */

/*---------------------------------------------------------------------------/
/ Locale and Namespace Configurations
/---------------------------------------------------------------------------*/

#define FF_CODE_PAGE	936
/* 936 = Simplified Chinese GBK. Supports Chinese filenames */

#define FF_USE_LFN		2
/* 2:LFN enabled, stack-local working buffer. Saves RAM vs LFN3 */

#define FF_MAX_LFN		64
/* Maximum LFN length. 64 is practical for Chinese filenames */

#define FF_LFN_UNICODE	0
/* 0:ANSI/OEM code page (GBK). 0 = non-Unicode LFN */

#define FF_LFN_BUF		255
#define FF_SFN_BUF		12
/* Used when LFN_UNICODE != 0, here as placeholder */

#define FF_STRF_ENCODE	3
/* 3:ANSI/OEM, matches FF_LFN_UNICODE=0 */

#define FF_FS_RPATH		0
/* 0:Disable relative path. Absolute paths only */

/*---------------------------------------------------------------------------/
/ Volume/Drive Configurations
/---------------------------------------------------------------------------*/

#define FF_VOLUMES		1
/* Only one volume: SD card */

#define FF_STR_VOLUME_ID	1
/* 1:Use string volume ID */

#define FF_VOLUME_STRS		"SD"
/* Volume ID string shown in mount */

#define FF_MULTI_PARTITION	0
/* 0:No partition table needed. Single partition on SD */

#define FF_MIN_SS		512
#define FF_MAX_SS		512
/* SD card fixed sector size = 512 bytes */

/*---------------------------------------------------------------------------/
/ System Configurations
/---------------------------------------------------------------------------*/

#define FF_FS_TINY		0
/* 0:Normal mode with separate sector buffer (better write performance) */

#define FF_FS_EXFAT		1
/* 1:exFAT enabled. Supports FAT32 + exFAT formatted SD cards */

#define FF_LBA64		0
/* 0:32-bit LBA (max 2TB). Required by exFAT, 0 is OK for SD≤32GB */

#define FF_FS_NORTC		0
/* 0:Use system RTC for file timestamps. STM32F4 has RTC */

#define FF_FS_RTC_ENABLED	0
/* R0.16 new macro, 0=no separate RTC enable needed */

#define FF_NORTC_MON	1
#define FF_NORTC_MDAY	1
#define FF_NORTC_YEAR	2025
/* Fallback date if RTC unavailable. Won't be used since RTC is active */

#define FF_FS_NOFSINFO	0
/* 0:Update FSINFO sector. Better for data integrity */

#define FF_FS_LOCK		8
/* Max 8 simultaneously open files/dirs for reentrant file lock */

/*---------------------------------------------------------------------------/
/ Reentrant (Thread-Safe) Configuration
/---------------------------------------------------------------------------*/

#define FF_FS_REENTRANT	1
/* 1:Enable reentrant (thread-safe) for FreeRTOS */

#define FF_FS_TIMEOUT	1000
/* Timeout for mutex acquisition: 1000ms. Tasks will wait if FS is busy */

#define FF_SYNC_t		void*
/* Opaque sync object handle, maps to CMSIS-RTOS v2 osMutexId_t */

/*---------------------------------------------------------------------------/
/ FAT Sub-Type (FAT12/FAT16/FAT32)
/ Note: FF_FS_EXFAT is already defined above (Section: System Configurations)
/---------------------------------------------------------------------------*/
