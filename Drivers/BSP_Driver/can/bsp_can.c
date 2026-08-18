/**
  * @file    bsp_can.c
  * @brief   CAN2 底层驱动完整实现，寄存器级配置 RCC、GPIO、中断
  */
#include "bsp_can.h"
#include <stdio.h>
#include <string.h>

/* 全局变量 */
CAN_HandleTypeDef Can_Handle;
volatile uint32_t flag = 0;
volatile uint32_t can_irq_count = 0;

CAN_TxHeaderTypeDef TxHeader;
CAN_RxHeaderTypeDef RxHeader;
uint8_t TxData[8];
uint8_t RxData[8];

uint8_t CAN_PopRxPacket(CAN_DataPacket_t *packet)
{
    if (packet == NULL) {
        return 0;
    }

    if (CAN_GetAndClearFlag()) {
        memcpy(packet->data, RxData, sizeof(packet->data));
        return 1;
    }

    return 0;
}

/* ==================== CAN 初始化（寄存器级） ==================== */
void CAN_Config(void)
{
    printf("\r\n--- CAN2 Config Start ---\r\n");

    /* 1. 同时使能 CAN1 和 CAN2 时钟（CAN2 依赖 CAN1 的时钟和总线逻辑） */
    RCC->APB1ENR |= RCC_APB1ENR_CAN1EN | RCC_APB1ENR_CAN2EN;
    printf("  CAN1 & CAN2 clocks enabled\r\n");

    /* 2. 同时复位 CAN1 和 CAN2 */
    RCC->APB1RSTR |= RCC_APB1RSTR_CAN1RST | RCC_APB1RSTR_CAN2RST;
    for (volatile int i = 0; i < 100; i++);
    RCC->APB1RSTR &= ~(RCC_APB1RSTR_CAN1RST | RCC_APB1RSTR_CAN2RST);
    printf("  CAN1 & CAN2 reset done\r\n");

    /* 3. GPIOB 时钟 */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;

    /* 4. 配置 PB12 (RX) 和 PB13 (TX) 为 CAN2 AF9 */
    GPIOB->MODER &= ~(GPIO_MODER_MODER12 | GPIO_MODER_MODER13);
    GPIOB->MODER |= (GPIO_MODER_MODER12_1 | GPIO_MODER_MODER13_1);
    GPIOB->AFR[1] &= ~(0xF << ((12 - 8) * 4) | 0xF << ((13 - 8) * 4));
    GPIOB->AFR[1] |= (9 << ((12 - 8) * 4)) | (9 << ((13 - 8) * 4));
    GPIOB->PUPDR |= (GPIO_PUPDR_PUPDR12_0 | GPIO_PUPDR_PUPDR13_0);
    GPIOB->OSPEEDR |= (GPIO_OSPEEDER_OSPEEDR12 | GPIO_OSPEEDER_OSPEEDR13);
    printf("  PB12/PB13 configured as CAN2 AF9\r\n");

    /* 5. NVIC 配置 (CAN2 中断, 优先级 >= 5 兼容 FreeRTOS) */
    HAL_NVIC_SetPriority(CAN2_RX0_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(CAN2_RX0_IRQn);
    printf("  NVIC configured for CAN2_RX0\r\n");

    /* 6. 初始化 CAN1（作为 CAN2 的从设备支持，不占用额外 GPIO） */
    CAN_HandleTypeDef Can1_Handle;
    Can1_Handle.Instance = CAN1;
    Can1_Handle.Init.Prescaler = 6;
    Can1_Handle.Init.Mode = CAN_MODE_NORMAL;
    Can1_Handle.Init.SyncJumpWidth = CAN_SJW_1TQ;
    Can1_Handle.Init.TimeSeg1 = CAN_BS1_5TQ;
    Can1_Handle.Init.TimeSeg2 = CAN_BS2_3TQ;
    Can1_Handle.Init.TimeTriggeredMode = DISABLE;
    Can1_Handle.Init.AutoBusOff = ENABLE;
    Can1_Handle.Init.AutoWakeUp = ENABLE;
    Can1_Handle.Init.AutoRetransmission = ENABLE;
    Can1_Handle.Init.ReceiveFifoLocked = DISABLE;
    Can1_Handle.Init.TransmitFifoPriority = DISABLE;
    HAL_CAN_Init(&Can1_Handle);
    HAL_CAN_Start(&Can1_Handle);
    printf("  CAN1 initialized (as slave support for CAN2)\r\n");

    /* 7. CAN2 初始化 */
    Can_Handle.Instance = CAN2;
    Can_Handle.Init.Prescaler = 16;
    Can_Handle.Init.Mode = CAN_MODE_LOOPBACK;   /* 回环模式: 发送数据自动回到自身接收FIFO，无需外部节点ACK */
    Can_Handle.Init.SyncJumpWidth = CAN_SJW_1TQ;
    Can_Handle.Init.TimeSeg1 = CAN_BS1_5TQ;
    Can_Handle.Init.TimeSeg2 = CAN_BS2_3TQ;
    Can_Handle.Init.TimeTriggeredMode = DISABLE;
    Can_Handle.Init.AutoBusOff = ENABLE;
    Can_Handle.Init.AutoWakeUp = ENABLE;
    Can_Handle.Init.AutoRetransmission = ENABLE;
    Can_Handle.Init.ReceiveFifoLocked = DISABLE;
    Can_Handle.Init.TransmitFifoPriority = DISABLE;
    HAL_CAN_Init(&Can_Handle);

    /* 8. 滤波器 (Bank 14，全通) */
    CAN_FilterTypeDef sFilterConfig = {0};
    sFilterConfig.FilterBank = 14;
    sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
    sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
    sFilterConfig.FilterIdHigh = 0x0000;
    sFilterConfig.FilterIdLow = 0x0000;
    sFilterConfig.FilterMaskIdHigh = 0x0000;
    sFilterConfig.FilterMaskIdLow = 0x0000;
    sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
    sFilterConfig.FilterActivation = ENABLE;
    sFilterConfig.SlaveStartFilterBank = 14;
    HAL_CAN_ConfigFilter(&Can_Handle, &sFilterConfig);

    /* 9. 启动 CAN2 */
    HAL_CAN_Start(&Can_Handle);

    /* 10. 激活接收中断 */
    HAL_CAN_ActivateNotification(&Can_Handle, CAN_IT_RX_FIFO0_MSG_PENDING);

    printf("CAN2: MCR=0x%08X, BTR=0x%08X, RF0R=0x%08X\n", CAN2->MCR, CAN2->BTR, CAN2->RF0R);
    printf("--- CAN2 Config Done ---\r\n\r\n");
}

/* ==================== 发送函数（回环模式，用于调试） ==================== */
HAL_StatusTypeDef CAN_SendMessage(void)
{
    uint32_t TxMailbox;
    TxHeader.ExtId = 0x1314;
    TxHeader.IDE = CAN_ID_EXT;
    TxHeader.RTR = CAN_RTR_DATA;
    TxHeader.DLC = 8;
    TxHeader.TransmitGlobalTime = DISABLE;
    TxHeader.StdId = 0x00;
    for (uint8_t i = 0; i < 8; i++) TxData[i] = i;

    // 直接放入发送邮箱，不阻塞，不等待
    return HAL_CAN_AddTxMessage(&Can_Handle, &TxHeader, TxData, &TxMailbox);
}

/* ==================== 读取标志 ==================== */
uint32_t CAN_GetAndClearFlag(void)
{
    uint32_t tmp = flag;
    flag = 0;
    return tmp;
}

/* ==================== 回调函数 ==================== */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &RxHeader, RxData) == HAL_OK) {
        if (RxHeader.ExtId == 0x1314 && RxHeader.IDE == CAN_ID_EXT && RxHeader.DLC == 8) {
            flag = 1;
            /* 不在 ISR 中 printf; 由 CAN_Task 通过 CAN_PopRxPacket 轮询后打印 */
        }
    }
}

/* ==================== 中断服务函数 ==================== */
void CAN2_RX0_IRQHandler(void)
{
    can_irq_count++;
    /* printf 在中断上下文中极慢，使用标志+任务打印代替 */
    HAL_CAN_IRQHandler(&Can_Handle);
}