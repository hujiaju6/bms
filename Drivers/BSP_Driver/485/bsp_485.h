#ifndef __485_H
#define __485_H

#include "stm32f4xx_hal.h"
#include <stdio.h>

/* ========== 引脚定义（与您原来完全相同） ========== */
#define _485_USART                          USART2
#define _485_USART_BAUDRATE                 115200
#define _485_IRQn  USART2_IRQn
// TX: PA2
#define _485_USART_TX_GPIO_PORT             GPIOA
#define _485_USART_TX_PIN                   GPIO_PIN_2
#define _485_USART_TX_AF                    GPIO_AF7_USART2

// RX: PA3
#define _485_USART_RX_GPIO_PORT             GPIOA
#define _485_USART_RX_PIN                   GPIO_PIN_3
#define _485_USART_RX_AF                    GPIO_AF7_USART2

// RE 控制脚: PC0
#define _485_RE_GPIO_PORT                   GPIOC
#define _485_RE_PIN                         GPIO_PIN_0

// 中断号
#define _485_IRQn                           USART2_IRQn

/* ========== 时钟使能（HAL 标准宏） ========== */
#define _485_USART_CLK_ENABLE()             __HAL_RCC_USART2_CLK_ENABLE()
#define _485_USART_TX_GPIO_CLK_ENABLE()     __HAL_RCC_GPIOA_CLK_ENABLE()
#define _485_USART_RX_GPIO_CLK_ENABLE()     __HAL_RCC_GPIOA_CLK_ENABLE()
#define _485_RE_GPIO_CLK_ENABLE()           __HAL_RCC_GPIOC_CLK_ENABLE()

/* ========== 收发控制宏（使用 HAL 函数） ========== */
// 注意：这里使用了简单的循环延时，确保状态切换稳定
#define _485_delay(n)   for(volatile uint32_t _i=0; _i<(n); _i++)

#define _485_RX_EN()    do{ _485_delay(1000); HAL_GPIO_WritePin(_485_RE_GPIO_PORT, _485_RE_PIN, GPIO_PIN_RESET); _485_delay(1000); }while(0)
#define _485_TX_EN()    do{ _485_delay(1000); HAL_GPIO_WritePin(_485_RE_GPIO_PORT, _485_RE_PIN, GPIO_PIN_SET);   _485_delay(1000); }while(0)

/* ========== 调试宏（保留您原来的定义） ========== */
#define _485_DEBUG_ON          1
#define _485_DEBUG_ARRAY_ON    1
#define _485_DEBUG_FUNC_ON     1

#define _485_INFO(fmt,arg...)           printf("<<-_485-INFO->> "fmt"\n",##arg)
#define _485_ERROR(fmt,arg...)          printf("<<-_485-ERROR->> "fmt"\n",##arg)
#define _485_DEBUG(fmt,arg...)          do{ if(_485_DEBUG_ON) printf("<<-_485-DEBUG->> [%d]"fmt"\n",__LINE__, ##arg); }while(0)
#define _485_DEBUG_ARRAY(array, num)    do{ if(_485_DEBUG_ARRAY_ON){ printf("<<-_485-DEBUG-ARRAY->>\n"); for(int i=0;i<(num);i++){ printf("%02x   ", ((uint8_t*)(array))[i]); if((i+1)%10==0) printf("\n"); } printf("\n"); } }while(0)
#define _485_DEBUG_FUNC()               do{ if(_485_DEBUG_FUNC_ON) printf("<<-_485-FUNC->> Func:%s@Line:%d\n",__func__,__LINE__); }while(0)

/* ========== 函数声明 ========== */
void _485_Config(void);
void _485_SendByte(uint8_t ch);
void _485_SendStr_length(uint8_t *str, uint32_t strlen);
void _485_SendString(uint8_t *str);
void USART2_IRQHandler(void);          // 中断服务函数（必须与启动向量表一致）
char *get_rebuff(uint16_t *len);
void clean_rebuff(void);

#endif