#ifndef __BSP_CAN_H
#define __BSP_CAN_H

#include "main.h"
#include "stm32f4xx_hal.h"
#include <stdio.h>

/* 全局 CAN 数据包结构体，方便各处读写使用 */
typedef struct {
    uint8_t data[8];
} CAN_DataPacket_t;

/* CAN 句柄 (供中断服务/任务使用) */
extern CAN_HandleTypeDef Can_Handle;       /* CAN2 句柄, 用于 485 通信及测试 */
extern volatile uint32_t flag;
extern volatile uint32_t can_irq_count;

void CAN_Config(void);
HAL_StatusTypeDef CAN_SendMessage(void);
uint32_t CAN_GetAndClearFlag(void);
uint8_t CAN_PopRxPacket(CAN_DataPacket_t *packet);

/* 新接口: 灵活发送/滤波/回调/错误查询 */
HAL_StatusTypeDef CANx_SendPacket(CAN_HandleTypeDef *hcan, uint32_t StdId, uint8_t *pData, uint8_t DLC);
void CANx_ConfigFilter(CAN_HandleTypeDef *hcan, uint8_t BankNum, uint32_t StdId);
void CAN_PrintReceivedPacket(CAN_HandleTypeDef *hcan, const char *tag);
uint32_t CAN_GetErrorCounter(CAN_HandleTypeDef *hcan);

#endif /* __BSP_CAN_H */