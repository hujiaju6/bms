#include "bsp_485.h"

static UART_HandleTypeDef Uart2_Handle;

// 接收缓存
#define UART_BUFF_SIZE  1024
volatile uint16_t uart_p = 0;
uint8_t uart_buff[UART_BUFF_SIZE];

/* 初始化函数 */
void _485_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* 使能时钟 */
    _485_USART_CLK_ENABLE();
    _485_USART_TX_GPIO_CLK_ENABLE();
    _485_USART_RX_GPIO_CLK_ENABLE();
    _485_RE_GPIO_CLK_ENABLE();

    /* 配置 TX (PA2) */
    GPIO_InitStruct.Pin       = _485_USART_TX_PIN;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_PULLUP;
    GPIO_InitStruct.Speed     = GPIO_SPEED_HIGH;
    GPIO_InitStruct.Alternate = _485_USART_TX_AF;
    HAL_GPIO_Init(_485_USART_TX_GPIO_PORT, &GPIO_InitStruct);

    /* 配置 RX (PA3) */
    GPIO_InitStruct.Pin       = _485_USART_RX_PIN;
    GPIO_InitStruct.Alternate = _485_USART_RX_AF;
    HAL_GPIO_Init(_485_USART_RX_GPIO_PORT, &GPIO_InitStruct);

    /* 配置 RE 控制脚 (PC0) */
    GPIO_InitStruct.Pin       = _485_RE_PIN;
    GPIO_InitStruct.Mode      = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull      = GPIO_PULLUP;
    GPIO_InitStruct.Speed     = GPIO_SPEED_HIGH;
    HAL_GPIO_Init(_485_RE_GPIO_PORT, &GPIO_InitStruct);

    /* 配置 UART2 */
    Uart2_Handle.Instance        = _485_USART;
    Uart2_Handle.Init.BaudRate   = _485_USART_BAUDRATE;
    Uart2_Handle.Init.WordLength = UART_WORDLENGTH_8B;
    Uart2_Handle.Init.StopBits   = UART_STOPBITS_1;
    Uart2_Handle.Init.Parity     = UART_PARITY_NONE;
    Uart2_Handle.Init.Mode       = UART_MODE_TX_RX;
    Uart2_Handle.Init.HwFlowCtl  = UART_HWCONTROL_NONE;
    Uart2_Handle.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&Uart2_Handle);

/* 中断配置（FreeRTOS MAX_SYSCALL_PRIO=5, 必须 >= 5） */
HAL_NVIC_SetPriority(_485_IRQn, 5, 0);
HAL_NVIC_EnableIRQ(_485_IRQn);

    /* 使能接收中断 */
    __HAL_UART_ENABLE_IT(&Uart2_Handle, UART_IT_RXNE);

    /* 默认进入接收模式 */
    HAL_GPIO_WritePin(_485_RE_GPIO_PORT, _485_RE_PIN, GPIO_PIN_RESET);
}

/* 发送单字节 */
void _485_SendByte(uint8_t ch)
{
    HAL_UART_Transmit(&Uart2_Handle, &ch, 1, 0xFFFF);
}

/* 发送指定长度 */
void _485_SendStr_length(uint8_t *str, uint32_t strlen)
{
    _485_TX_EN();
    for(uint32_t i=0; i<strlen; i++) {
        _485_SendByte(str[i]);
    }
    _485_delay(0xFFF);   // 等待发送完成
    _485_RX_EN();
}

/* 发送字符串 */
void _485_SendString(uint8_t *str)
{
    _485_TX_EN();
    uint32_t i=0;
    while(str[i] != '\0') {
        _485_SendByte(str[i++]);
    }
    _485_delay(0xFFF);
    _485_RX_EN();
}

/* 中断服务函数（直接读取 DR，不依赖 HAL 回调） */
void USART2_IRQHandler(void)
{
    /* DEBUG: 翻转LED，只要中断触发就能看到闪烁 */
    HAL_GPIO_TogglePin(GPIOF, GPIO_PIN_6);

    /* 接收中断 */
    if(__HAL_UART_GET_FLAG(&Uart2_Handle, UART_FLAG_RXNE) != RESET) {
        uint8_t data = (uint8_t)(Uart2_Handle.Instance->DR & 0xFF);
        if(uart_p < UART_BUFF_SIZE) {
            uart_buff[uart_p++] = data;
        } else {
            // 缓存满，清空（可自行处理）
            clean_rebuff();
        }
    }
    /* 其他中断（如溢出、空闲等）可调用 HAL 处理，但为了简单，此处不处理 */
    // 如需处理错误，可启用 HAL_UART_IRQHandler，但会与上面的重复，建议只选一种方式。
    // 这里只处理接收，其他中断忽略。
}

/* 获取缓存数据和长度 */
char *get_rebuff(uint16_t *len)
{
    *len = uart_p;
    return (char *)uart_buff;
}

/* 清空缓存 */
void clean_rebuff(void)
{
    uint16_t i = UART_BUFF_SIZE;
    uart_p = 0;
    while(i--) uart_buff[i] = 0;
}