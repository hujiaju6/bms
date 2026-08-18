/*---------------------------------------------------------------------------/
/ FatFs Reentrant (Thread-Safe) Glue for FreeRTOS + CMSIS-RTOS v2
/
/ Called automatically by FatFs when FF_FS_REENTRANT == 1.
/ Uses the old FatFs API (int vol) -- sync objects managed internally.
/---------------------------------------------------------------------------*/

#include "ff.h"
#include "cmsis_os.h"   /* CMSIS-RTOS v2 API */

/* Supports up to FF_VOLUMES volumes */
static osMutexId_t fatfs_mutex[FF_VOLUMES + 1] = {NULL};

/*===========================================================================*/
/* ff_mutex_create() - Called once per volume at mount time                   */
/* Returns: 1=OK, 0=Failed                                                    */
/*===========================================================================*/
int ff_mutex_create (
    int vol             /* [IN] Volume number (0-based) */
)
{
    if ((unsigned int)vol > FF_VOLUMES) {
        return 0;
    }
    osMutexAttr_t attr = {
        .name      = "FatFsMTX",
        .attr_bits = osMutexRecursive,   /* Allow recursive lock */
        .cb_mem    = NULL,
        .cb_size   = 0U
    };
    osMutexId_t mtx = osMutexNew(&attr);
    if (mtx == NULL) {
        return 0;   /* Failed */
    }
    fatfs_mutex[vol] = mtx;
    return 1;       /* OK */
}

/*===========================================================================*/
/* ff_mutex_delete() - Called when volume is unmounted                        */
/*===========================================================================*/
void ff_mutex_delete (
    int vol             /* [IN] Volume number (0-based) */
)
{
    if ((unsigned int)vol > FF_VOLUMES) {
        return;
    }
    if (fatfs_mutex[vol] != NULL) {
        osMutexDelete(fatfs_mutex[vol]);
        fatfs_mutex[vol] = NULL;
    }
}

/*===========================================================================*/
/* ff_mutex_take() - Lock mutex before accessing file system                  */
/* Returns: 1=OK, 0=Timeout/Failed                                            */
/*===========================================================================*/
int ff_mutex_take (
    int vol             /* [IN] Volume number (0-based) */
)
{
    if ((unsigned int)vol > FF_VOLUMES || fatfs_mutex[vol] == NULL) {
        return 0;
    }
    /* osMutexAcquire returns osOK on success */
    if (osMutexAcquire(fatfs_mutex[vol], FF_FS_TIMEOUT) == osOK) {
        return 1;
    }
    return 0;   /* Timeout or error */
}

/*===========================================================================*/
/* ff_mutex_give() - Unlock mutex after file system access                    */
/*===========================================================================*/
void ff_mutex_give (
    int vol             /* [IN] Volume number (0-based) */
)
{
    if ((unsigned int)vol > FF_VOLUMES || fatfs_mutex[vol] == NULL) {
        return;
    }
    osMutexRelease(fatfs_mutex[vol]);
}
