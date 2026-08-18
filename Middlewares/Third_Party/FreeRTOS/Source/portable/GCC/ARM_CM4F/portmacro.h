/*
 * FreeRTOS Kernel V10.3.1
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef PORTMACRO_H
#define PORTMACRO_H

#ifdef __cplusplus
extern "C" {
#endif

/*-----------------------------------------------------------
 * Port specific definitions.
 *
 * The settings in this file configure FreeRTOS correctly for the
 * given hardware and compiler.
 *
 * These settings should not be altered.
 *-----------------------------------------------------------
 */

/* Type definitions. */
#define portCHAR          char
#define portFLOAT         float
#define portDOUBLE        double
#define portLONG          long
#define portSHORT         short
#define portSTACK_TYPE    uint32_t
#define portBASE_TYPE     long

typedef portSTACK_TYPE StackType_t;
typedef long BaseType_t;
typedef unsigned long UBaseType_t;

#if ( configUSE_16_BIT_TICKS == 1 )
    typedef uint16_t TickType_t;
    #define portMAX_DELAY ( TickType_t ) 0xffff
#else
    typedef uint32_t TickType_t;
    #define portMAX_DELAY ( TickType_t ) 0xffffffffUL

    /* 32-bit tick type on a 32-bit architecture, so reads of the tick count do
    not need to be guarded with a critical section. */
    #define portTICK_TYPE_IS_ATOMIC 1
#endif

/*-----------------------------------------------------------*/

/* Architecture specifics. */
#define portSTACK_GROWTH      ( -1 )
#define portTICK_PERIOD_MS    ( ( TickType_t ) 1000 / configTICK_RATE_HZ )
#define portBYTE_ALIGNMENT    8
#define portDONT_DISCARD      __attribute__(( used ))
/*-----------------------------------------------------------*/

/* Scheduler utilities. */
#define portYIELD()                                 \
{                                                   \
    /* Set a PendSV to request a context switch. */ \
    portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT; \
    __asm volatile ( "dsb" ::: "memory" );          \
    __asm volatile ( "isb" );                       \
}

#define portNVIC_INT_CTRL_REG     ( * ( ( volatile uint32_t * ) 0xe000ed04 ) )
#define portNVIC_PENDSVSET_BIT    ( 1UL << 28UL )
#define portEND_SWITCHING_ISR( xSwitchRequired ) if( xSwitchRequired != pdFALSE ) portYIELD()
#define portYIELD_FROM_ISR( x ) portEND_SWITCHING_ISR( x )
/*-----------------------------------------------------------*/

/* Critical section management. */
extern void vPortEnterCritical( void );
extern void vPortExitCritical( void );

#define portFORCE_INLINE inline

static portFORCE_INLINE void vPortRaiseBASEPRI( void )
{
    uint32_t ulNewBASEPRI = configMAX_SYSCALL_INTERRUPT_PRIORITY;
    __asm volatile (
        "   msr basepri, %0  \n"
        "   dsb              \n"
        "   isb              \n"
        ::"r"( ulNewBASEPRI ) : "memory"
    );
}

static portFORCE_INLINE uint32_t ulPortRaiseBASEPRI( void )
{
    uint32_t ulOriginalBASEPRI, ulNewBASEPRI;
    __asm volatile (
        "   mrs %0, basepri                                               \n"
        "   mov %1, %2                                                    \n"
        "   msr basepri, %1                                               \n"
        "   dsb                                                           \n"
        "   isb                                                           \n"
        :"=r"( ulOriginalBASEPRI ), "=r"( ulNewBASEPRI ) : "i"( configMAX_SYSCALL_INTERRUPT_PRIORITY ) : "memory"
    );
    return ulOriginalBASEPRI;
}

static portFORCE_INLINE void vPortSetBASEPRI( uint32_t ulNewMaskValue )
{
    __asm volatile (
        "   msr basepri, %0  \n"
        "   dsb              \n"
        "   isb              \n"
        ::"r"( ulNewMaskValue ) : "memory"
    );
}

#define portSET_INTERRUPT_MASK_FROM_ISR()         ulPortRaiseBASEPRI()
#define portCLEAR_INTERRUPT_MASK_FROM_ISR( x )    vPortSetBASEPRI( x )
#define portDISABLE_INTERRUPTS()                  vPortRaiseBASEPRI()
#define portENABLE_INTERRUPTS()                   vPortSetBASEPRI( 0 )
#define portENTER_CRITICAL()                      vPortEnterCritical()
#define portEXIT_CRITICAL()                       vPortExitCritical()

/*-----------------------------------------------------------*/

/* Tickless idle/low power functionality. */
#ifndef portSUPPRESS_TICKS_AND_SLEEP
    extern void vPortSuppressTicksAndSleep( TickType_t xExpectedIdleTime );
    #define portSUPPRESS_TICKS_AND_SLEEP( xExpectedIdleTime ) vPortSuppressTicksAndSleep( xExpectedIdleTime )
#endif
/*-----------------------------------------------------------*/

/* Task function macros as described on the FreeRTOS.org WEB site. */
#define portTASK_FUNCTION_PROTO( vFunction, pvParameters ) void vFunction( void *pvParameters )
#define portTASK_FUNCTION( vFunction, pvParameters ) void vFunction( void *pvParameters )
/*-----------------------------------------------------------*/

/* Required to enable hardware floating point support. */
#define portINITIAL_XPSR              ( 0x01000000 )
#define portINITIAL_EXC_RETURN        ( 0xfffffffd )
#define portSTART_ADDRESS_MASK        ( 0xfffffffeUL )

/* For backward compatibility, ensure configKERNEL_INTERRUPT_PRIORITY is defined. */
#ifndef configKERNEL_INTERRUPT_PRIORITY
    #define configKERNEL_INTERRUPT_PRIORITY 255
#endif

#define portMEMORY_BARRIER() __asm volatile ( "" ::: "memory" )

#ifdef __cplusplus
}
#endif

#endif /* PORTMACRO_H */
