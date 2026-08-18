/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           freertos.c
  * @brief          7 个 FreeRTOS 用户任务 + 消息队列 + 互斥锁的完整实现
  *
  * 【RTOS 核心概念速查表】
  *
  * ┌──────────────────────────────────────────────────────────────────┐
  * │  概念          │  含义                                            │
  * ├──────────────────────────────────────────────────────────────────┤
  * │  Task (任务)    │ 独立的执行线程，每个任务有自己的栈空间和优先级    │
  * │  Queue (队列)   │ 任务间传递数据的 FIFO 管道，线程安全              │
  * │  Mutex (互斥锁) │ 保护共享资源，同一时刻只有一个任务持有锁          │
  * │  Priority       │ 数字越大优先级越高，高优先级可抢占低优先级        │
  * │  vTaskDelay()   │ 任务进入 Blocked(阻塞) 态，让出 CPU 给其他任务    │
  * │  Stack          │ 每个任务独立的栈，存储局部变量和函数调用链        │
  * │  ISR            │ 中断服务函数，运行在特权模式，不是 FreeRTOS 任务  │
  * └──────────────────────────────────────────────────────────────────┘
  *
  * 【本系统任务图】
  *
  *  BMS_Task ──(100ms/BMS数据)──→ UI_Message_Queue ──→ UI_Task ──→ LVGL 刷新
  *  CAN_Task ──(接收/状态)───────→ UI_Message_Queue ──→ UI_Task
  *  按钮点击 ──→ Can_Cmd_Queue ──→ CAN_Task
  *  RS485_Task ──(RX/TX数据)─────→ UI_Message_Queue ──→ UI_Task
  *  Sys_Task ──(1s/RTC时间)──────→ UI_Message_Queue ──→ UI_Task
  *  Storage_Task ──(1min)───────→ FatFs → SD卡写入BMS日志CSV
  *  USART2 ISR ──→ Ring Buffer ──→ RS485_Task (通过 get_rebuff 轮询)
  *  CAN2_RX0 ISR ──→ flag ──→ CAN_Task (通过 CAN_PopRxPacket 轮询)
  *
  * 【优先级设计理念】
  *  osPriorityHigh   : CAN_Task — 通信实时性最重要，不能被阻塞太久
  *  osPriorityNormal : UI_Task, BMS_Task — 界面和数据处理同等重要
  *  osPriorityBelowNormal : RS485_Task — 485 通信优先级低于 CAN
  *  osPriorityLow    : Sys_Task, Storage_Task — 后台监控，最低优先级
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"
#include "bms_simulator.h"
#include "ui_messages.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "led.h"
#include "ff.h"         /* FatFs R0.16 — f_mount, f_open, f_write, f_close etc. */
#include "bsp_sdio.h"   /* BSP_SD_Init(), hsd1 */
#include "lvgl.h"
#include <math.h>
#include "stdio.h"
#include "string.h"
#include "lv_port_disp.h"   
#include "lv_port_indev.h"  
#include "MY_UI.h"          /* 工业级科技感仪表盘UI */
#include "bsp_can.h"
#include "bsp_485.h"
#include "bsp_rtc.h"

#define scr_act_width() lv_obj_get_width(lv_scr_act())
#define scr_act_height() lv_obj_get_height(lv_scr_act())
/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/*
 * 【RTOS 概念：消息队列 (Message Queue)】
 * 消息队列是任务间通信的标准方式，本质是一个线程安全的 FIFO 缓冲区。
 * - osMessageQueueNew(容量, 每条消息大小, NULL)
 * - osMessageQueuePut() → 发送（放入队列尾部）
 * - osMessageQueueGet() → 接收（从队列头部取出）
 * - 超时 = 0     → 非阻塞，队列空/满则立即返回 err
 * - 超时 = osWaitForever → 阻塞等待，直到有数据
 *
 * 本系统用 3 条队列实现生产者-消费者模式：
 */

/* UI_Message_Queue: 所有数据汇总入口，容量 16，每条 UI_Message_t */
osMessageQueueId_t UI_Message_QueueHandle;

/* Can_Cmd_Queue: CAN 发送命令，容量 8，每条 CAN_Command_t */
osMessageQueueId_t Can_Cmd_QueueHandle;

/* Modbus_Data_Queue: 预留 485 Modbus 轮询数据，容量 16 */
osMessageQueueId_t Modbus_Data_QueueHandle;

osThreadId_t BMS_TaskHandle;

/*
 * 【RTOS 概念：互斥锁 (Mutex)】
 * 互斥锁用于保护共享资源（如全局变量、文件系统、外设寄存器）。
 * 规则：使用前必须 osMutexAcquire() 获取锁，用完必须 osMutexRelease() 释放锁。
 * 获取不到锁时任务进入 Blocked 态等待。
 * LVGL_MutexHandle: 保护 LVGL 对象访问，防止 UI_Task 的 lv_timer_handler()
 *   与 Storage_Task 的 UI_RefreshStoragePage() 产生竞态条件导致屏幕卡死。
 * FS_MutexHandle: 保护 FatFs 文件系统操作，防止多任务同时读写 SD 卡。
 */
osMutexId_t LVGL_MutexHandle;
osMutexId_t FS_MutexHandle;

/* FatFs work area (small, allocated in .bss / ~550 bytes) */
FATFS fs;

/*
 * 【RTOS 概念：任务句柄 (Task Handle)】
 * 句柄是指向任务控制块(TCB)的指针，可用于：
 * - uxTaskGetStackHighWaterMark(h)  → 查询栈剩余
 * - vTaskSuspend(h) / vTaskResume(h) → 挂起/恢复任务
 * - vTaskDelete(h)                  → 删除任务
 * - eTaskGetState(h)                → 查询任务状态
 */
osThreadId_t UI_TaskHandle;
osThreadId_t CAN_TaskHandle;
osThreadId_t RS485_TaskHandle;
osThreadId_t Storage_TaskHandle;
osThreadId_t Sys_TaskHandle;

extern lv_obj_t * can_status_label;
extern lv_obj_t * can_data_label;
extern lv_obj_t * rs485_data_label;
extern lv_obj_t * time_label;

/* BMS 仪表盘/曲线图 全局指针 */
extern lv_obj_t             * bms_chart;
extern lv_chart_series_t    * bms_chart_ser_v;
extern lv_chart_series_t    * bms_chart_ser_i;
extern lv_obj_t             * bms_meter;
extern lv_meter_indicator_t * bms_meter_indic;

/* ====== 数据持久化缓存 — 切换页面后恢复显示 ====== */
RS485_Data_t g_last_rs485_rx;
RS485_Data_t g_last_rs485_tx;
CAN_DataPacket_t g_last_can_rx;
uint8_t g_has_last_can_rx = 0;

/*
 * 【RTOS 概念：任务属性 (Task Attributes)】
 *
 * 每个任务创建时需要定义：
 *   .name       → 任务名，调试用（串口打印时显示）
 *   .stack_size → 栈大小（CMSIS_V2 单位是 字节）
 *   .priority   → 优先级（CMSIS_V2 枚举：Low < BelowNormal < Normal < High < Realtime）
 *
 * 【栈大小如何确定？】
 *   - 太小 → 运行时栈溢出 (StackOverflow)，触发 vApplicationStackOverflowHook
 *   - 太大 → 浪费 RAM（F407 总共只有 192KB SRAM）
 *   - 估算方法：每个局部变量 + 函数调用深度 × 每层开销 + ISR 嵌套开销
 *   - 调试方法：用 uxTaskGetStackHighWaterMark() 查看剩余栈空间，
 *     如果剩余 < 10%，说明栈太小需要加大
 */

/* BMS 任务：栈 1KB，优先级 Normal */
const osThreadAttr_t BMS_Task_attributes = {
  .name = "BMS_Task",
  .stack_size = 256 * 4,                       /* 256×4 = 1024 字节 */
  .priority = (osPriority_t) osPriorityNormal, /* 与 UI_Task 同级 */
};

/* UI 任务：栈 4KB，优先级 Normal（LVGL 渲染需要较大栈） */
const osThreadAttr_t UI_Task_attributes = {
  .name = "UI_Task",
  .stack_size = 1024 * 4,                      /* 1024×4 = 4096 字节 */
  .priority = (osPriority_t) osPriorityNormal,
};

/* CAN 任务：栈 2KB，优先级 High（通信实时性最高） */
const osThreadAttr_t CAN_Task_attributes = {
  .name = "CAN_Task",
  .stack_size = 512 * 4,                       /* 512×4 = 2048 字节 */
  .priority = (osPriority_t) osPriorityHigh,   /* ★ 最高优先级，抢占 Normal/Low */
};

/* RS485 任务：栈 2KB，优先级 BelowNormal */
const osThreadAttr_t RS485_Task_attributes = {
  .name = "RS485_Task",
  .stack_size = 512 * 4,                       /* 512×4 = 2048 字节 */
  .priority = (osPriority_t) osPriorityBelowNormal,
};

/* 存储任务：栈 4KB，优先级 Low（预留，目前空循环） */
const osThreadAttr_t Storage_Task_attributes = {
  .name = "Storage_Task",
  .stack_size = 1024 * 4,                      /* 1024×4 = 4096 字节 */
  .priority = (osPriority_t) osPriorityLow,
};

/* 系统监控任务：栈 1KB，优先级 Low */
const osThreadAttr_t Sys_Task_attributes = {
  .name = "Sys_Task",
  .stack_size = 256 * 4,                       /* 256×4 = 1024 字节 */
  .priority = (osPriority_t) osPriorityLow,
};
/* USER CODE END Variables */

/* Private function prototypes -----------------------------------------------*/
void UI_Task_Entry(void *argument);
void CAN_Task_Entry(void *argument);
void RS485_Task_Entry(void *argument);
void Storage_Task_Entry(void *argument);
void Sys_Task_Entry(void *argument);
void BMS_Task_Entry(void *argument);
void UI_RequestCanTestSend(void);
void MX_FREERTOS_Init(void); 

/**
  * @brief  FreeRTOS 初始化
  * @note   此函数在主函数 main() 中 HAL 库初始化完成后被调用
  *
  * 【RTOS 启动流程】
  *  1. main() → HAL_Init() → SystemClock_Config() → 各外设初始化
  *  2. main() → MX_FREERTOS_Init() → 创建队列、互斥锁、任务
  *  3. main() → osKernelStart()  → ★ 调度器启动，从此 main() 不再执行
  *                                   各任务开始抢占 CPU
  */
void MX_FREERTOS_Init(void) {
  /*
   * ===== 创建互斥量 =====
   * 互斥量初始为 "\346\234\252\351\224\201\345\256\232" 状态，谁先 acquire 谁获得
   */
  LVGL_MutexHandle = osMutexNew(NULL);
  FS_MutexHandle = osMutexNew(NULL);
  if (LVGL_MutexHandle == NULL) printf("[FREERTOS] LVGL_MutexHandle NULL!\r\n");
  if (FS_MutexHandle == NULL) printf("[FREERTOS] FS_MutexHandle NULL!\r\n");

  /*
   * ===== 创建消息队列 =====
   * 参数：(队列容量, 每条消息的 sizeof, 属性)
   * 队列为空时 Get 会阻塞（或立即返回）；队列满时 Put 会阻塞（或立即返回）
   */
  UI_Message_QueueHandle = osMessageQueueNew(16, sizeof(UI_Message_t), NULL);
  Can_Cmd_QueueHandle = osMessageQueueNew(8, sizeof(CAN_Command_t), NULL);
  Modbus_Data_QueueHandle = osMessageQueueNew(16, sizeof(uint32_t), NULL);
  if (UI_Message_QueueHandle == NULL) printf("[FREERTOS] UI_Message_Queue NULL!\r\n");
  if (Can_Cmd_QueueHandle == NULL) printf("[FREERTOS] Can_Cmd_Queue NULL!\r\n");
  if (Modbus_Data_QueueHandle == NULL) printf("[FREERTOS] Modbus_Queue NULL!\r\n");

  /*
   * ===== 创建所有任务 =====
   *
   * 【任务创建后的状态】
   * osThreadNew() 创建任务 → 任务进入 Ready(就绪) 态 → 等待调度器启动
   * osKernelStart() 后 → 最高优先级的 Ready 任务获得 CPU → Running(运行) 态
   *
   * 【CMSIS_V2 vs FreeRTOS 原生 API】
   * osThreadNew() 内部封装了 xTaskCreate()
   * CMSIS 额外提供 .stack_size 的字节单位（原生用 word）
   * 注意 CMSIS 参数以 4 字节为单位，实际栈大小需 ×4
   */
  UI_TaskHandle = osThreadNew(UI_Task_Entry, NULL, &UI_Task_attributes);
  printf("[FREERTOS] UI_Task %s\r\n", UI_TaskHandle ? "OK" : "FAIL");

    CAN_TaskHandle = osThreadNew(CAN_Task_Entry, NULL, &CAN_Task_attributes);
    printf("[FREERTOS] CAN_Task %s\r\n", CAN_TaskHandle ? "OK" : "FAIL");

    RS485_TaskHandle = osThreadNew(RS485_Task_Entry, NULL, &RS485_Task_attributes);
    printf("[FREERTOS] RS485_Task %s\r\n", RS485_TaskHandle ? "OK" : "FAIL");

  Storage_TaskHandle = osThreadNew(Storage_Task_Entry, NULL, &Storage_Task_attributes);
  printf("[FREERTOS] Storage_Task %s\r\n", Storage_TaskHandle ? "OK" : "FAIL");

  Sys_TaskHandle = osThreadNew(Sys_Task_Entry, NULL, &Sys_Task_attributes);
  printf("[FREERTOS] Sys_Task %s\r\n", Sys_TaskHandle ? "OK" : "FAIL");

  BMS_TaskHandle = osThreadNew(BMS_Task_Entry, NULL, &BMS_Task_attributes);
  printf("[FREERTOS] BMS_Task %s\r\n", BMS_TaskHandle ? "OK" : "FAIL");
}

/* Private application code --------------------------------------------------*/

/* =========================================================================
 * 任务 1/6：UI_Task — GUI 刷新与消息处理中心
 * =========================================================================
 *
 * 【角色】所有数据的消费者，所有 UI 更新的执行者
 * 【优先级】osPriorityNormal
 * 【周期】每 5ms 唤醒一次
 * 【栈】4096 字节（LVGL 渲染需要较深调用栈）
 *
 * 【RTOS 学习点 1：消息消费者模式】
 *   本任务的核心模式是 "\346\211\271\351\207\217\346\266\210\350\264\271\346\266\210\346\201\257"：
 *     while(osMessageQueueGet(...) == osOK) { // 非阻塞: timeout=0, 有多少收多少 }
 *   这样做的好处是：如果 5ms 内 BMS_Task、CAN_Task、Sys_Task 都发了消息，
 *   可以在一个周期内全部处理完，不会堆积。
 *
 * 【RTOS 学习点 2：vTaskDelay vs vTaskDelayUntil】
 *   vTaskDelay(5ms)     → "\344\273\216\347\216\260\345\234\250\350\265\267" 延迟 5ms。缺点是如果处理耗时 2ms，
 *                          实际周期 = 2+5 = 7ms，周期不稳定。
 *   vTaskDelayUntil()   → "\347\273\235\345\257\271\345\200\274" 延迟。无论处理耗时多少，都在固定节拍唤醒。
 *                          本项目用 vTaskDelay 是因为 LVGL 处理时间波动不大。
 *
 * 【RTOS 学习点 3：lv_timer_handler 的单线程约束】
 *   LVGL 不是线程安全的，所有 lv_xxx API 必须在同一个任务（或同一线程）中调用。
 *   因此所有 UI 更新都通过消息队列汇聚到本任务，而不是让 BMS_Task 直接改 UI。
 *
 * 【RTOS 学习点 4：任务状态机】
 *   Running(运行态):    正在执行本任务的代码
 *   Ready(就绪态):      等待 CPU（被高优先级任务抢占后）
 *   Blocked(阻塞态):    vTaskDelay 或 osMessageQueueGet(timeout>0) 期间
 *   Suspended(挂起态):  被 vTaskSuspend() 主动挂起后（本项目未使用）
 *
 *   本任务的典型周期：
 *   Running → vTaskDelay(5) → Blocked(等待5ms) → Ready(调度器唤醒) → Running → ...
 */
void UI_Task_Entry(void *argument)
{
    /*
     * ★ 一次性初始化 UI 界面（4 个页面全部预创建，后续只做 显示/隐藏 切换）
     * 放在任务入口而非 MX_FREERTOS_Init 中，确保 LVGL 已就绪
     */
    create_pretty_dashboard_ui();

    UI_Message_t msg;
    uint32_t loop_cnt = 0;
    uint32_t print_cnt = 0;

    /* ===== 主事件循环 (Event Loop) ===== */
    while (1)
    {
        loop_cnt++;

        /*
         * 【心跳日志】每 100 次循环（100×5ms = 500ms）打印一次
         * 如果串口停更，说明 UI_Task 可能卡死或栈溢出
         */
        if (loop_cnt % 100 == 0) {
            printf("[UI] Alive, loop=%lu\r\n", loop_cnt);
        }

        /*
         * ★ 整个 LVGL 操作区加锁，防止 Storage_Task 跨任务调用 lv_xxx API 时数据竞争
         */
        if (osMutexAcquire(LVGL_MutexHandle, pdMS_TO_TICKS(50)) == osOK)
        {
        /*
         * 【批量消费消息】
         * osMessageQueueGet(h, &msg, NULL, 0):
         *   第 3 个参数 NULL = 不使用消息优先级
         *   第 4 个参数 0   = 非阻塞模式，队列为空时立即返回 osErrorResource
         *
         * 为什么用非阻塞而不是 osWaitForever？
         *   → 因为 lv_timer_handler() 必须每 5ms 执行一次，
         *     如果阻塞等消息，LVGL 定时器会不准，动画会卡顿。
         */
        while (osMessageQueueGet(UI_Message_QueueHandle, &msg, NULL, 0) == osOK)
        {
            switch (msg.type)
            {
                /* ── 消息类型 1：BMS 数据更新 ────────────────────── */
                case UI_MSG_BMS_UPDATE:
                    /*
                     * 更新 SOC 百分比文字（仪表盘中央）
                     * snprintf 将数字格式化到 buffer，lv_label_set_text 刷新显示
                     */
                    if (bms_soc_label != NULL) {
                        char buf[8];
                        snprintf(buf, sizeof(buf), "%d%%", msg.payload.bms.soc);
                        lv_label_set_text(bms_soc_label, buf);
                        lv_obj_set_style_text_color(bms_soc_label, lv_color_hex(0x10B981), LV_PART_MAIN);
                    }

                    /* 更新仪表盘指针（物理角度随 SOC 变化） */
                    if (bms_meter != NULL && bms_meter_indic != NULL) {
                        lv_meter_set_indicator_value(bms_meter, bms_meter_indic, msg.payload.bms.soc);
                    }

                    /* 更新总电压卡片（如 "52.35 V"） */
                    if (bms_voltage_label != NULL) {
                        char buf[16];
                        snprintf(buf, sizeof(buf), "%.2f V", msg.payload.bms.voltage);
                        lv_label_set_text(bms_voltage_label, buf);
                    }

                    /* 更新总电流卡片（使用 fabsf 取绝对值显示，如 "14.8 A"） */
                    if (bms_current_label != NULL) {
                        char buf[16];
                        snprintf(buf, sizeof(buf), "%.1f A", fabsf(msg.payload.bms.current));
                        lv_label_set_text(bms_current_label, buf);
                    }

                    /*
                     * ★ 更新曲线图 — LVGL Shift 模式
                     * lv_chart_set_next_value() 在 60 点窗口末尾添加新数据，
                     * 窗口满后最左边的点被挤出（FIFO 滑动效果）。
                     * 电压单位转换为 mV（×1000），电流单位转换为 mA。
                     */
                    if (bms_chart != NULL && bms_chart_ser_v != NULL) {
                        lv_chart_set_next_value(bms_chart, bms_chart_ser_v,
                            (lv_coord_t)(msg.payload.bms.voltage * 1000));
                    }
                    if (bms_chart != NULL && bms_chart_ser_i != NULL) {
                        lv_chart_set_next_value(bms_chart, bms_chart_ser_i,
                            (lv_coord_t)(msg.payload.bms.current * 1000));
                    }

                    /* ★ 故障标志位联动 UI 更新 */
                    {
                        uint8_t flags = msg.payload.bms.fault_flags;
                        UI_RefreshFaultPage(flags);       /* Fault 页面 [ ]→[X] */
                        UI_RefreshAlarmBar(flags);        /* 顶部报警文字 */
                        UI_SetCardFlash(flags != 0);      /* 卡片红闪 */
                    }
                    break;

                /* ── 消息类型 2：CAN 接收数据 ────────────────────── */
                case UI_MSG_CAN_RX:
                    /*
                     * 缓存到全局变量，切换页面再切回来时可恢复显示
                     * g_has_last_can_rx 标志表示缓存中是否有有效数据
                     */
                    g_last_can_rx = msg.payload.can;
                    g_has_last_can_rx = 1;

                    if (can_data_label != NULL) {
                        /*
                         * static 变量 can_rx_cnt 在函数多次调用间保持值不变
                         * 作为接收帧计数器，每次收到一帧 CAN 数据后 +1
                         */
                        static uint32_t can_rx_cnt = 0;
                        char buf[128];
                        snprintf(buf, sizeof(buf),
                            "CAN \346\216\245\346\224\266\346\225\260\346\215\256:\n"
                           "%02X %02X %02X %02X %02X %02X %02X %02X #%lu",
                            msg.payload.can.data[0], msg.payload.can.data[1],
                            msg.payload.can.data[2], msg.payload.can.data[3],
                            msg.payload.can.data[4], msg.payload.can.data[5],
                            msg.payload.can.data[6], msg.payload.can.data[7],
                            (unsigned long)++can_rx_cnt);
                        lv_label_set_text(can_data_label, buf);
                        lv_obj_set_style_text_color(can_data_label, lv_color_hex(0x10B981), LV_PART_MAIN);
                    }
                    break;

                /* ── 消息类型 3：CAN 发送状态 ────────────────────── */
                case UI_MSG_CAN_STATUS:
                    /*
                     * msg.payload.status:
                     *   0 → HAL_OK     → 绿色 TX: SUCCESS
                     *   1 → HAL_ERROR  → 红色 TX: FAILED
                     */
                    if (can_status_label != NULL) {
                        if (msg.payload.status == 0) {
                            lv_label_set_text(can_status_label, "\345\217\221\351\200\201:\346\210\220\345\212\237");
                            lv_obj_set_style_text_color(can_status_label, lv_color_hex(0x10B981), LV_PART_MAIN);
                        } else {
                            lv_label_set_text(can_status_label, "\345\217\221\351\200\201:\345\244\261\350\264\245");
                            lv_obj_set_style_text_color(can_status_label, lv_color_hex(0xEF4444), LV_PART_MAIN);
                        }
                    }
                    break;

                /* ── 消息类型 4：RS485 接收数据 ────────────────────── */
                case UI_MSG_RS485_RX:
                    /*
                     * 缓存到全局变量（和 CAN 同样的缓存策略）
                     * 然后格式化十六进制显示，每 16 字节换行
                     */
                    g_last_rs485_rx = msg.payload.rs485;
                    if (rs485_data_label != NULL) {
                        static uint32_t rs485_rx_cnt = 0;
                        char buf[512];
                        int offset = 0;
                        offset += snprintf(buf + offset, sizeof(buf) - offset,
                            "RS485 \346\216\245\346\224\266[%u\345\255\227\350\212\202]HEX:\n", msg.payload.rs485.len);
                        /*
                         * 逐字节格式化为 %02X，每 16 字节加一个换行
                         * snprintf 返回实际写入的字符数，offset 累加确保不越界
                         */
                        for (uint16_t i = 0; i < msg.payload.rs485.len && offset < (int)sizeof(buf) - 8; i++) {
                            offset += snprintf(buf + offset, sizeof(buf) - offset,
                                "%02X ", msg.payload.rs485.data[i]);
                            if ((i + 1) % 16 == 0 && i + 1 < msg.payload.rs485.len) {
                                offset += snprintf(buf + offset, sizeof(buf) - offset, "\n");
                            }
                        }
                        /*
                         * 追加递增计数器 #N，确保每次都生成不同的字符串
                         * 这样 LVGL 才会真正触发重绘（如果字符串没变，LVGL 会跳过）
                         */
                        offset += snprintf(buf + offset, sizeof(buf) - offset,
                            "\n#%lu", (unsigned long)++rs485_rx_cnt);
                        lv_label_set_text(rs485_data_label, buf);
                        lv_obj_set_style_text_color(rs485_data_label, lv_color_hex(0x0284C7), LV_PART_MAIN);
                    }
                    break;

                /* ── 消息类型 5：RS485 自测发送数据回显 ────────────── */
                case UI_MSG_RS485_TX:
                    /*
                     * 只在启动自测时触发一次，显示上电自检发送的数据
                     * 颜色用黄色(0xF59E0B) 区别于蓝色接收数据
                     */
                    g_last_rs485_tx = msg.payload.rs485;
                    if (rs485_data_label != NULL) {
                        char buf[512];
                        int offset = 0;
                        offset += snprintf(buf + offset, sizeof(buf) - offset,
                            "RS485 \345\217\221\351\200\201[%u\345\255\227\350\212\202]:\n", msg.payload.rs485.len);
                        for (uint16_t i = 0; i < msg.payload.rs485.len && offset < (int)sizeof(buf) - 8; i++) {
                            offset += snprintf(buf + offset, sizeof(buf) - offset,
                                "%02X ", msg.payload.rs485.data[i]);
                        }
                        lv_label_set_text(rs485_data_label, buf);
                        lv_obj_set_style_text_color(rs485_data_label, lv_color_hex(0xF59E0B), LV_PART_MAIN);
                    }
                    break;

                /* ── 消息类型 6：RTC 时间更新 ────────────────────── */
                case UI_MSG_RTC_TIME:
                    /*
                     * 串口打印调试信息，然后更新屏幕顶部时间标签
                     * 格式 HH:MM:SS，替代初始的 "14:35"
                     */
                    printf("[UI] RTC msg rx: %02d:%02d:%02d\r\n",
                        msg.payload.rtc.hours,
                        msg.payload.rtc.minutes,
                        msg.payload.rtc.seconds);
                    if (time_label != NULL) {
                        char buf[16];
                        snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
                            msg.payload.rtc.hours,
                            msg.payload.rtc.minutes,
                            msg.payload.rtc.seconds);
                        lv_label_set_text(time_label, buf);
                    } else {
                        printf("[UI] ERROR: time_label is NULL!\r\n");
                    }
                    break;

                default:
                    break;
            }
            /*
             * 注意：lv_label_set_text() 内部已自动设置 dirty flag，
             * lv_obj_invalidate() 会自动触发，无需手动调用
             */
        }

        /*
         * 【内存监控】大约每 133 次循环（133×5ms≈665ms）打印一次
         * uxTaskGetStackHighWaterMark(NULL)  ← NULL 表示查当前任务自身
         * xPortGetFreeHeapSize()              ← 全局堆剩余
         *
         * 【什么是"\351\253\230\346\260\264\344\275\215\347\272\277"？】
         * FreeRTOS 创建任务时会用 0xA5 填充整个栈空间。
         * uxTaskGetStackHighWaterMark() 返回从栈顶到最近一个未被覆盖的 0xA5 之间的距离。
         * 也就是说：栈曾经被使用过但未溢出的"\346\234\200\345\244\247\346\267\261\345\272\246"。
         * 如果高水位线很小（比如 < 50 字节），说明栈曾经很接近溢出。
         */
        if (++print_cnt >= 133)
        {
            print_cnt = 0;
            UBaseType_t lvgl_stack_words = uxTaskGetStackHighWaterMark(NULL);
            size_t free_heap = xPortGetFreeHeapSize();
            printf("[MEM] LVGL\344\273\273\345\212\241\346\240\210\345\211\251\344\275\231: %lu \345\255\227\350\212\202, \345\211\251\344\275\231\345\240\206: %zu \345\255\227\350\212\202\r\n",
                   (unsigned long)(lvgl_stack_words * 4), free_heap);
        }

            /*
             * ★ 核心：驱动 LVGL 定时器引擎
             * 每 5ms 调用一次，LVGL 内部完成：
             *   1. 处理动画进度
             *   2. 处理用户输入事件
             *   3. 刷新脏(dirty)区域到帧缓冲
             */
            lv_timer_handler();
            osMutexRelease(LVGL_MutexHandle);
        }

        /*
         * 【RTOS 关键：vTaskDelay(5ms) ≠ 死等 5ms】
         * vTaskDelay 让当前任务进入 Blocked 态，调度器立即切换到下一个 Ready 任务。
         * 如果只用 while(1){} 死循环不加 delay，同优先级/更低优先级任务会永远得不到 CPU。
         *
         * pdMS_TO_TICKS(5) 把 5ms 转换为系统 tick 数（默认 tick = 1ms）
         * 5 ticks = 5ms，RTOS 在 5 个 tick 后把本任务从 Blocked 移回 Ready。
         */
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

/* =========================================================================
 * 任务 2/6：CAN_Task — CAN 总线收发管理
 * =========================================================================
 *
 * 【角色】CAN 发送命令消费者 + CAN 接收数据轮询者 + UI 消息生产者
 * 【优先级】osPriorityHigh（最高优先级，保证通信实时性）
 * 【周期】最多阻塞 50ms（等命令），无命令时每 20ms 检查接收
 * 【栈】2048 字节
 *
 * 【RTOS 学习点 5：中断 → 任务的桥接模式】
 *   中断服务函数 (ISR) 必须极短、不能 printf、不能长时间阻塞。
 *   本项目使用 "\346\240\207\345\277\227\344\275\215" 桥接 ISR 和任务：
 *     CAN2_RX0_IRQHandler (ISR端)   → 只设置 flag=1 + 读取寄存器
 *     CAN_Task (任务端)             → 轮询 CAN_PopRxPacket() 检查 flag
 *   这是一种轻量级的 ISR→Task 通信方式，适合低频事件。
 *   高频事件可以用 FreeRTOS 的 Task Notification 或 Queue-From-ISR。
 *
 * 【RTOS 学习点 6：阻塞等待 vs 轮询】
 *   osMessageQueueGet(h, &cmd, NULL, pdMS_TO_TICKS(50)):
 *     超时 = 50ms → 等待命令，50ms 内没收到则返回 osErrorTimeout
 *     超时 = 0    → 非阻塞，立即返回（如 UI_Task 中那样）
 *     超时 = osWaitForever → 无限等待，任务永久进入 Blocked 态
 *
 *   这里用 50ms 超时而非永久等待，因为还需要处理 CAN_RX 轮询。
 *   如果永久阻塞等命令，CAN_RX 将永远得不到处理。
 */
void CAN_Task_Entry(void *argument)
{
    CAN_Command_t cmd;
    UI_Message_t msg;
    uint32_t loop_cnt = 0;

    while (1)
    {
        loop_cnt++;

        /*
         * ===== 第一步：处理 CAN 发送命令 =====
         * 阻塞等待 50ms，收到命令后调用 HAL_CAN_AddTxMessage 发送
         *
         * 【数据流向】
         *   触摸屏按钮 → LVGL 回调 → UI_RequestCanTestSend()
         *   → osMessageQueuePut(Can_Cmd_Queue) → 本任务接收
         *   → CAN_SendMessage() → HAL_CAN_AddTxMessage() → CAN2 TX 引脚
         *   → Loopback 模式 → CAN2 RX 引脚 → FIFO0 → CAN2_RX0 中断
         */
        if (osMessageQueueGet(Can_Cmd_QueueHandle, &cmd, NULL, pdMS_TO_TICKS(50)) == osOK)
        {
            if (cmd.type == CAN_CMD_SEND_TEST)
            {
                printf("[CAN] Sending test frame...\r\n");

                /*
                 * CAN_SendMessage() 发送扩展帧 ID=0x1314，DLC=8
                 * 数据: 0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07
                 * 返回值：HAL_OK 表示成功放入发送邮箱
                 *         HAL_ERROR 表示邮箱满（3 个邮箱都被占用）
                 */
                HAL_StatusTypeDef status = CAN_SendMessage();

                if (status == HAL_OK) {
                    printf("[CAN] TX OK, IRQ count=%lu\r\n", (unsigned long)can_irq_count);
                } else {
                    printf("[CAN] TX FAILED (err=0x%02X), IRQ count=%lu\r\n",
                           (unsigned int)status, (unsigned long)can_irq_count);
                }

                /*
                 * 发送结果通过 UI_Message_Queue 通知 UI_Task
                 * 非阻塞 put（timeout=0），队列满了就丢弃
                 */
                msg.type = UI_MSG_CAN_STATUS;
                msg.payload.status = (status == HAL_OK) ? 0 : 1;
                osMessageQueuePut(UI_Message_QueueHandle, &msg, 0, 0);
            }
        }

        /*
         * ===== 第二步：轮询 CAN 接收数据 =====
         *
         * 【完整数据流】
         *   CAN2 收到数据 → CAN2_RX0_IRQHandler (ISR)
         *     → HAL_CAN_IRQHandler()
         *       → HAL_CAN_RxFifo0MsgPendingCallback()
         *         → 读取 RxHeader + RxData[8], 设置 flag=1
         *
         *   本任务：
         *     → CAN_PopRxPacket() 检查 flag
         *       → 如果 flag=1：拷贝数据到 msg.payload.can，清零 flag
         *       → 如果 flag=0：返回 0，跳过
         *
         *   然后：
         *     → osMessageQueuePut(UI_Message_Queue, &msg, 0, 0)
         *     → UI_Task 收到 UI_MSG_CAN_RX → 更新屏幕
         */
        if (CAN_PopRxPacket(&msg.payload.can))
        {
            msg.type = UI_MSG_CAN_RX;
            osMessageQueuePut(UI_Message_QueueHandle, &msg, 0, 0);
        }

        /*
         * 无论是否处理了命令或数据，都延迟 20ms 再进入下一轮
         * 20ms 周期 = 50Hz 的 CAN 接收轮询频率，足够响应 500Kbps 的 CAN 总线
         */
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

/* =========================================================================
 * 任务 3/6：RS485_Task — RS-485 透明传输
 * =========================================================================
 *
 * 【角色】485 总线数据接收 + 上电自检 + UI 消息生产者
 * 【优先级】osPriorityBelowNormal（低于 CAN，高于 Sys/Storage）
 * 【周期】有数据立即处理，无数据时 30ms 轮询一次
 * 【栈】2048 字节
 *
 * 【RTOS 学习点 7：ISR + 环形缓冲区的设计】
 *   中断中以最小开销写入环形缓冲区，任务中以较低频率轮询取出。
 *
 *   本项目的简化版环形缓冲区（在 bsp_485.c 中）：
 *     uart_buff[1024]  ← 固定大小缓冲区
 *     uart_p           ← 写入指针（由 ISR 递增）
 *
 *   ISR 端（USART2_IRQHandler）：
 *     收到一个字节 → uart_buff[uart_p++] = data;  (极其快，只有 2 条指令)
 *
 *   任务端（RS485_Task）：
 *     get_rebuff(&len) → 获取当前写入位置和数据指针
 *     clean_rebuff()   → 清空缓冲区（重置指针 + 清零数据）
 *
 *   ★ 注意：这不是真正的环形缓冲区，而是线性缓冲区。
 *     clean_rebuff() 清空后重新从 0 开始写。
 *     真正的环形缓冲区会用取模运算 %1024 循环写入。
 *
 * 【RTOS 学习点 8：临界区保护 — HAL_NVIC_DisableIRQ/EnableIRQ】
 *   任务和 ISR 共享 uart_buff 缓冲区，存在竞态条件：
 *     任务正在 memcpy 读取时，ISR 如果同时修改 uart_p，数据可能损坏。
 *
 *   解决方案：拷贝数据前临时关闭 USART2 中断
 *     HAL_NVIC_DisableIRQ(USART2_IRQn);  // 进入临界区
 *     memcpy(...);                        // 安全拷贝
 *     clean_rebuff();                     // 安全清空
 *     HAL_NVIC_EnableIRQ(USART2_IRQn);   // 退出临界区
 *
 *   为什么不用 FreeRTOS 的 taskENTER_CRITICAL()？
 *   → taskENTER_CRITICAL() 会关闭所有中断，影响 CAN、RTC、SysTick 等。
 *   → 这里只需要关闭一个 IRQ，Disable/Enable 单个中断即可，影响最小。
 */
void RS485_Task_Entry(void *argument)
{
    printf("[RS485] Task started, baud 115200, waiting data...\r\n");

    /* 初始状态：485 芯片处于接收模式（RE=0, DE=0） */
    _485_RX_EN();

    /*
     * ====== 上电自收发测试 ======
     *
     * 【测试目的】验证 USART2 + 485 芯片的硬件链路是否正常
     * 【前置条件】需要用跳线帽连接 PA2(TX) 和 PA3(RX)
     *
     * 【测试流程】
     * 1. clean_rebuff()           → 清空接收缓冲区
     * 2. _485_TX_EN()             → 切换 485 芯片到发送模式
     * 3. 依次发送 AA, 55, 12      → 3 个探测字节
     * 4. _485_delay(0xFFFF)       → 忙等发送完成
     * 5. _485_RX_EN()             → 切回接收模式
     * 6. _485_TX 数据推送 UI      → 在屏幕上显示发送数据
     * 7. vTaskDelay(100ms)        → 等待 ISR 接收完成 + 中断处理
     * 8. get_rebuff(&len)         → 读取接收到的数据
     * 9. 比对 TX 和 RX → PASS / FAIL
     */
    {
        clean_rebuff();

        uint8_t tx_bytes[] = {0xAA, 0x55, 0x12};
        _485_TX_EN();
        _485_SendByte(tx_bytes[0]);
        _485_SendByte(tx_bytes[1]);
        _485_SendByte(tx_bytes[2]);
        _485_delay(0xFFFF);   /* 等待发送完成 */
        _485_RX_EN();

        /* 把发送数据推送到 UI（显示为黄色发送数据） */
        {
            UI_Message_t tx_msg;
            tx_msg.type = UI_MSG_RS485_TX;
            tx_msg.payload.rs485.len = 3;
            memcpy(tx_msg.payload.rs485.data, tx_bytes, 3);
            osMessageQueuePut(UI_Message_QueueHandle, &tx_msg, 0, 0);
        }

        /*
         * ★ 关键：这里 vTaskDelay 让出 CPU，给 USART2_IRQHandler 时间处理
         * 如果不 delay，get_rebuff 可能读不到任何数据（ISR 还没触发）
         */
        vTaskDelay(pdMS_TO_TICKS(100));

        uint16_t test_len = 0;
        uint8_t *test_data = (uint8_t *)get_rebuff(&test_len);

        if (test_len == 3 && test_data != NULL)
        {
            printf("[RS485] SELF-TEST PASS! TX: AA 55 12, RX: %02X %02X %02X\r\n",
                   test_data[0], test_data[1], test_data[2]);
        }
        else
        {
            printf("[RS485] SELF-TEST FAIL! TX=3 bytes, RX=%u bytes\r\n", test_len);
        }
        clean_rebuff();
    }
    /* ====== 自测结束 ====== */

    /* ====== 主循环：持续轮询 485 接收数据 ====== */
    while (1)
    {
        uint16_t len = 0;

        /*
         * get_rebuff(&len) 返回环形缓冲区的读取指针
         * len 为当前已存入的字节数，data 为缓冲区首地址
         */
        uint8_t *pData = get_rebuff(&len);

        if (len > 0 && pData != NULL)
        {
            /* 收到数据，封装消息 */
            UI_Message_t msg;
            msg.type = UI_MSG_RS485_RX;
            /* 防止数据超过 RS485_DATA_MAX_LEN(256) 导致消息体溢出 */
            msg.payload.rs485.len = (len > RS485_DATA_MAX_LEN) ? RS485_DATA_MAX_LEN : len;

            /*
             * 【临界区保护】
             * 关闭 USART2 中断，防止 ISR 在 memcpy 期间修改缓冲区或 uart_p
             * 临界区要尽量短：只做 memcpy + clean_rebuff，然后立即开中断
             */
            HAL_NVIC_DisableIRQ(USART2_IRQn);
            memcpy(msg.payload.rs485.data, pData, msg.payload.rs485.len);
            clean_rebuff();       /* 取完数据后清空，为下一帧腾空间 */
            HAL_NVIC_EnableIRQ(USART2_IRQn);

            /* printf 调试：走 USART1（PA9 TX），不与 485 总线冲突 */
            printf("[RS485] RX %u bytes: ", msg.payload.rs485.len);
            for (uint16_t i = 0; i < msg.payload.rs485.len; i++) {
                printf("%02X ", msg.payload.rs485.data[i]);
            }
            printf("\r\n");

            /*
             * 非阻塞推送 UI 队列
             * 如果 Queue 满了（UI_Task 处理不过来），消息被丢弃
             * 这是设计中的权衡：宁可丢一帧数据，也不能让本任务卡住
             */
            if (UI_Message_QueueHandle != NULL) {
                osMessageQueuePut(UI_Message_QueueHandle, &msg, 0, 0);
            }
        }
        else
        {
            /*
             * 无数据 → 进入 Blocked 态 30ms
             * 这 30ms 内 CPU 可以被其他任务使用（Sys_Task, BMS_Task 等）
             *
             * 为什么是 30ms？
             *   → 485 波特率 115200 ≈ 11520 字节/秒
             *   → 一帧 Modbus RTU 最大 256 字节 ≈ 22ms
             *   → 30ms 轮询一次不会错过完整帧
             */
            vTaskDelay(pdMS_TO_TICKS(30));
        }
    }
}

/* =========================================================================
 * 任务 4/6：Storage_Task — 数据存储（预留）
 * =========================================================================
 *
 * 【角色】预留的 SDIO+FATFS 数据存储任务
 * 【优先级】osPriorityLow（最低优先级之一，不抢占实时任务）
 * 【状态】空循环，等待后续实现日志记录功能
 * 【栈】4096 字节（FATFS 文件操作需要较大栈空间）
 *
 * 【功能】每分钟将 BMS 数据追加写入 SD 卡 CSV 日志文件
 * 【文件】0:/BMS_LOG.CSV（追加模式，首行写表头）
 * 【优先级】osPriorityLow — 最低优先级不阻塞 UI/通信
 *
 * 【CSV 列】时间戳 | Vcell1-12 | PackV | 电流 | SOC | Temp1-4 | 状态
 */
void Storage_Task_Entry(void *argument)
{
    FRESULT fr;
    FIL     logFile;
    UINT    bw;
    char    buf[256];
    static uint32_t log_seq  = 0;
    uint8_t header_written   = 0;
    uint8_t s_mounted        = 0;   /* Track mount state */
    uint32_t next_csv_tick   = 0;   /* Next CSV write timestamp (tick/ms) */

    /* 等待系统上电稳定 */
    vTaskDelay(1000);
    printf("[Storage] Task started \342\200\224 polling for SD card...\r\n");

    /* 初始 UI 显示: 无卡 */
    if (osMutexAcquire(LVGL_MutexHandle, pdMS_TO_TICKS(100)) == osOK) {
        UI_RefreshStoragePage(0, NULL, 0, 0);
        osMutexRelease(LVGL_MutexHandle);
    }

    while(1)
    {
        /* ====== 每 2 秒检测一次 SD 卡状态 ====== */
        vTaskDelay(2000);

        if (s_mounted && fs.fs_type == 0) {
            /* 文件系统意外丢失 (SD 卡被拔出) */
            printf("[Storage] SD card removed!\r\n");
            s_mounted = 0;
            header_written = 0;
            if (osMutexAcquire(LVGL_MutexHandle, pdMS_TO_TICKS(100)) == osOK) {
                UI_RefreshStoragePage(0, NULL, 0, 0);
                osMutexRelease(LVGL_MutexHandle);
            }
        }

        if (!s_mounted) {
            /* 未挂载: 先快速探测卡是否存在 */
            if (!BSP_SD_IsCardPresent()) {
                if (osMutexAcquire(LVGL_MutexHandle, pdMS_TO_TICKS(100)) == osOK) {
                    UI_RefreshStoragePage(0, NULL, 0, 0);
                    osMutexRelease(LVGL_MutexHandle);
                }
                continue;  /* No card, keep polling */
            }

            /* 卡已插入, f_mount 会通过 disk_initialize -> BSP_SD_Init 自动初始化 */
            printf("[Storage] Card detected, mounting...\r\n");

            fr = f_mount(&fs, "0:", 1);
            if (fr == FR_NO_FILESYSTEM) {
                /* 卡已插入但无文件系统, 自动格式化为 FAT32 */
                printf("[Storage] No filesystem, auto-formatting as FAT32...\r\n");
                f_mount(NULL, "0:", 1);
                {
                    MKFS_PARM opt = {0};
                    opt.fmt = FM_FAT32;
                    opt.n_fat = 1;
                    opt.au_size = 1024;
                    BYTE *mkfs_work = pvPortMalloc(1024);
                    if (mkfs_work != NULL) {
                        fr = f_mkfs("0:", &opt, mkfs_work, 1024);
                        vPortFree(mkfs_work);
                    } else {
                        fr = FR_NOT_ENOUGH_CORE;
                    }
                }
                if (fr == FR_OK) {
                    printf("[Storage] Format OK, remounting...\r\n");
                    fr = f_mount(&fs, "0:", 1);
                    if (fr != FR_OK) {
                        printf("[Storage] ERROR: remount after format failed (%d)\r\n", (int)fr);
                        BSP_SD_DeInit();
                        if (osMutexAcquire(LVGL_MutexHandle, pdMS_TO_TICKS(100)) == osOK) {
                            UI_RefreshStoragePage(0, NULL, 0, 0);
                            osMutexRelease(LVGL_MutexHandle);
                        }
                        continue;
                    }
                } else {
                    printf("[Storage] ERROR: f_mkfs failed (%d)\r\n", (int)fr);
                    BSP_SD_DeInit();
                    if (osMutexAcquire(LVGL_MutexHandle, pdMS_TO_TICKS(100)) == osOK) {
                        UI_RefreshStoragePage(0, NULL, 0, 0);
                        osMutexRelease(LVGL_MutexHandle);
                    }
                    continue;
                }
            } else if (fr == FR_DISK_ERR) {
                printf("[Storage] ERROR: disk init/read failed (%d), retrying...\r\n", (int)fr);
                BSP_SD_DeInit();
                if (osMutexAcquire(LVGL_MutexHandle, pdMS_TO_TICKS(100)) == osOK) {
                    UI_RefreshStoragePage(0, NULL, 0, 0);
                    osMutexRelease(LVGL_MutexHandle);
                }
                continue;
            } else if (fr != FR_OK) {
                printf("[Storage] ERROR: f_mount failed (%d)\r\n", (int)fr);
                BSP_SD_DeInit();
                if (osMutexAcquire(LVGL_MutexHandle, pdMS_TO_TICKS(100)) == osOK) {
                    UI_RefreshStoragePage(0, NULL, 0, 0);
                    osMutexRelease(LVGL_MutexHandle);
                }
                continue;
            }
            printf("[Storage] SD card mounted OK.\r\n");
            s_mounted = 1;
            header_written = 0;
            next_csv_tick = xTaskGetTickCount() + 2000; /* First CSV in 2s */

            /* 获取卡片信息更新 UI */
            HAL_SD_CardInfoTypeDef info;
            HAL_SD_GetCardInfo(&hsd1, &info);
            uint32_t total_mb = (uint32_t)(((unsigned long long)info.LogBlockNbr * info.LogBlockSize) / 1024 / 1024);

            /* 获取剩余空间 */
            DWORD free_clusters;
            FATFS *pfs;
            uint32_t free_mb = 0;
            fr = f_getfree("0:", &free_clusters, &pfs);
            if (fr == FR_OK) {
                free_mb = (uint32_t)(((unsigned long long)free_clusters * pfs->csize * 512) / 1024 / 1024);
            }
            if (osMutexAcquire(LVGL_MutexHandle, pdMS_TO_TICKS(100)) == osOK) {
                UI_RefreshStoragePage(1, NULL, total_mb, free_mb);
                osMutexRelease(LVGL_MutexHandle);
            }

            /* 读取已有 CSV 末行，立即显示最近一次记录 */
            fr = f_open(&logFile, "0:/BMS_LOG.CSV", FA_READ);
            if (fr == FR_OK) {
                FSIZE_t fsz = f_size(&logFile);
                if (fsz > 0) {
                    header_written = 1;

                    /* 读取文件尾部查找最后一条数据行 */
                    FSIZE_t seek_pos = (fsz > (FSIZE_t)sizeof(buf) - 1)
                                     ? (fsz - (FSIZE_t)sizeof(buf) + 1) : 0;
                    UINT br;
                    memset(buf, 0, sizeof(buf));
                    f_lseek(&logFile, seek_pos);
                    f_read(&logFile, buf, sizeof(buf) - 1, &br);

                    /* 去除末尾换行符 */
                    char *end = buf + br;
                    while (end > buf && (*(end - 1) == '\n' || *(end - 1) == '\r'))
                        *(--end) = '\0';

                    /* 定位最后一个 '\n' 之后的行起始 */
                    char *line = buf;
                    for (char *s = buf; s < end; s++) {
                        if (*s == '\n') line = s + 1;
                    }

                    /* 如果不是表头行，则作为数据行显示 */
                    if (strncmp(line, "Timestamp", 9) != 0 && *line != '\0') {
                        if (osMutexAcquire(LVGL_MutexHandle, pdMS_TO_TICKS(100)) == osOK) {
                            UI_RefreshStoragePage(2, line, total_mb, free_mb);
                            osMutexRelease(LVGL_MutexHandle);
                        }
                    }
                }
                f_close(&logFile);
            }
            continue;
        }

        /* ====== 已挂载: 检查是否到了写 CSV 的时间 ====== */
        if (xTaskGetTickCount() < next_csv_tick) continue;
        next_csv_tick = xTaskGetTickCount() + 5000;

        /* ====== 打开文件 — 追加模式 ====== */
        fr = f_open(&logFile, "0:/BMS_LOG.CSV", FA_OPEN_APPEND | FA_WRITE);
        if (fr == FR_NO_FILE) {
            fr = f_open(&logFile, "0:/BMS_LOG.CSV", FA_CREATE_ALWAYS | FA_WRITE);
            header_written = 0;
        }
        if (fr != FR_OK) {
            printf("[Storage] f_open error (%d)\r\n", (int)fr);
            if (osMutexAcquire(LVGL_MutexHandle, pdMS_TO_TICKS(100)) == osOK) {
                UI_RefreshStoragePage(0, "f_open failed", 0, 0);
                osMutexRelease(LVGL_MutexHandle);
            }
            s_mounted = 0;
            continue;
        }

        /* 首次写入时写 CSV 表头 */
        if (!header_written) {
            f_puts("Timestamp,Date,Time,"
                   "Vcell1,Vcell2,Vcell3,Vcell4,"
                   "Vcell5,Vcell6,Vcell7,Vcell8,"
                   "Vcell9,Vcell10,Vcell11,Vcell12,"
                   "PackV,Current,SOC,"
                   "Temp1,Temp2,Temp3,Temp4,"
                   "Status,Seq\r\n",
                   &logFile);
            header_written = 1;
        }

        /* ====== 读取 RTC ====== */
        RTC_TimeTypeDef t;
        RTC_DateTypeDef d;
        HAL_RTC_GetTime(&hrtc, &t, RTC_FORMAT_BIN);
        HAL_RTC_GetDate(&hrtc, &d, RTC_FORMAT_BIN);

        /* ====== 读取 BMS 全局数据 ====== */
        const BMS_Data_t *bd = BMS_GetData();

        /* ====== 构造 CSV 数据行 (适配 BMS_Data_t 实际字段) ====== */
        /* Vcell1-12: 暂无单体电芯数组, 用 max/min 填充前两项, 其余填 0 */
        int pack_v_mv  = (int)(bd->voltage * 1000.0f);
        int curr_ma    = (int)(bd->current * 1000.0f);
        int max_cell_v_mv = (int)(bd->max_cell_v * 1000.0f);
        int min_cell_v_mv = (int)(bd->min_cell_v * 1000.0f);
        int max_temp_c = (int)bd->max_temp;
        int min_temp_c = (int)bd->min_temp;

        int len = snprintf(buf, sizeof(buf),
                 "%lu,%02d-%02d-%02d %02d:%02d:%02d,"
                 "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                 "%d,%d,%d,"
                 "%d,%d,%d,%d,"
                 "0x%02X,%lu\r\n",
                 (unsigned long)(xTaskGetTickCount() / 1000),
                 2000 + d.Year, d.Month, d.Date,
                 t.Hours, t.Minutes, t.Seconds,
                 max_cell_v_mv, min_cell_v_mv, 0,0,0,0,0,0,0,0,0,0, /* Vcell1-12 */
                 pack_v_mv, curr_ma, (int)bd->soc,
                 max_temp_c, min_temp_c, 0, 0,                        /* Temp1-4 */
                 bd->fault_flags,
                 log_seq++
                 );
        (void)len;

        /* ====== 写入 + f_sync 确保掉电不丢数据 ====== */
        f_puts(buf, &logFile);
        f_sync(&logFile);
        f_close(&logFile);

        /* 更新 UI 中的 CSV 记录预览 */
        if (osMutexAcquire(LVGL_MutexHandle, pdMS_TO_TICKS(100)) == osOK) {
            UI_RefreshStoragePage(2, buf, 0, 0);
            osMutexRelease(LVGL_MutexHandle);
        }

        printf("[Storage] CSV record #%lu written OK.\r\n", (unsigned long)(log_seq - 1));
    }
}

/* =========================================================================
 * 任务 5/6：Sys_Task — 系统监控与维护
 * =========================================================================
 *
 * 【角色】LED 心跳 + RTC 时间读取 + 内存监控
 * 【优先级】osPriorityLow（后台任务，不影响业务）
 * 【周期】每 250ms × 2 = 500ms 心跳周期，每 1s 推送时间，每 2s 打印内存
 * 【栈】1024 字节
 *
 * 【RTOS 学习点 10：为什么 Sys_Task 是 Low 优先级却不会饿死？】
 *   因为 FreeRTOS 是抢占式调度 + 时间片轮转：
 *   - 高优先级任务 CAN_Task 大部分时间在 Blocked（等命令 / vTaskDelay）
 *   - UI_Task 和 BMS_Task 每次运行完都会 vTaskDelay 让出 CPU
 *   - 当所有高优先级任务都 Blocked 时，Low 优先级的 Sys_Task 就能运行
 *   - Sys_Task 自己也会 vTaskDelay 让出 CPU
 *
 *   所以只要高优先级任务不是 100% 占满 CPU（死循环），
 *   低优先级任务总能得到执行机会。
 *
 * 【RTOS 学习点 11：xPortGetFreeHeapSize 和 uxTaskGetStackHighWaterMark】
 *   这两个函数是开发阶段最重要的调试工具：
 *   - xPortGetFreeHeapSize()          → 全局堆剩余（创建任务/队列/互斥锁都会消耗堆）
 *   - uxTaskGetStackHighWaterMark(h)  → 某任务栈的 "\351\253\230\346\260\264\344\275\215\347\272\277" 剩余
 *
 *   典型内存泄漏检测：启动后观察 FreeHeap，如果持续下降不回升，说明有内存泄漏。
 */
void Sys_Task_Entry(void *argument)
{
    uint32_t cnt = 0;
    uint32_t rtc_tick = 0;

    while(1)
    {
        /*
         * ===== LED 心跳 =====
         * 每 250ms 翻转一次 → 周期 500ms，频率 2Hz
         * 这是嵌入式系统最常见的"\346\210\221\350\277\230\346\264\273\347\235\200"信号
         */
        LED_Ctrl(On);   
        vTaskDelay(250);   /* Blocked 250ms，亮 */
        LED_Ctrl(Off);  
        vTaskDelay(250);   /* Blocked 250ms，灭 */

        /*
         * ===== RTC 时间更新（每 1 秒一次） =====
         * 250ms × 2 = 500ms 一个循环，每 2 个循环 = 1 秒
         * rtc_tick 从 0 开始，到 2 归零
         */
        rtc_tick++;
        if (rtc_tick >= 2) {
            rtc_tick = 0;

            uint8_t h, m, s;
            /*
             * BSP_RTC_GetTime() 内部：
             *   1. 等 RSF（影子寄存器同步标志）
             *   2. HAL_RTC_GetTime() 读 TR 寄存器 → 时/分/秒
             *   3. HAL_RTC_GetDate() 读 DR 寄存器 → 解锁影子寄存器
             */
            BSP_RTC_GetTime(&h, &m, &s);
            printf("[SYS] RTC read: %02d:%02d:%02d\r\n", h, m, s);

            /*
             * 构造 RTC 时间消息 → 推送到 UI_Message_Queue
             * UI_Task 收到 UI_MSG_RTC_TIME 后更新屏幕顶部时间
             */
            UI_Message_t time_msg;
            time_msg.type = UI_MSG_RTC_TIME;
            time_msg.payload.rtc.hours   = h;
            time_msg.payload.rtc.minutes = m;
            time_msg.payload.rtc.seconds = s;

            if (UI_Message_QueueHandle != NULL) {
                osMessageQueuePut(UI_Message_QueueHandle, &time_msg, 0, 0);
            }
        }

        /*
         * ===== 内存监控（每 2 秒打印一次） =====
         * cnt 在 4 次循环（4×500ms = 2s）后归零
         */
        cnt++;
        if (cnt >= 4) {
            cnt = 0;

            /* 打印全局堆剩余 — 这是最重要的内存指标 */
            size_t freeHeap = xPortGetFreeHeapSize();
            printf("[MEM] Free Heap: %zu bytes\r\n", freeHeap);

            /*
             * 打印各任务栈高水位线（单位：字 = 4 字节）
             * 高水位线 = 从栈顶向下扫描，到最后一个未被覆盖的 0xA5 的深度
             * 如果输出接近 0，说明该任务栈即将溢出，需要增大 stack_size
             */
            printf("[MEM] UI_Task stack free: %lu words\r",
                   uxTaskGetStackHighWaterMark(UI_TaskHandle));
            printf("[MEM] CAN_Task stack free: %lu words\r",
                   uxTaskGetStackHighWaterMark(CAN_TaskHandle));
            printf("[MEM] RS485_Task stack free: %lu words\r",
                   uxTaskGetStackHighWaterMark(RS485_TaskHandle));
            printf("[MEM] Storage_Task stack free: %lu words\r",
                   uxTaskGetStackHighWaterMark(Storage_TaskHandle));
            printf("[MEM] Sys_Task stack free: %lu words\r",
                   uxTaskGetStackHighWaterMark(Sys_TaskHandle));

            if (BMS_TaskHandle != NULL) {
                printf("[MEM] BMS_Task stack free: %lu words\r",
                       uxTaskGetStackHighWaterMark(BMS_TaskHandle));
            }
        }
    }
}

/* =========================================================================
 * 辅助函数：UI_RequestCanTestSend — 按钮到 CAN 的桥接函数
 * =========================================================================
 *
 * 【调用链】
 *   触摸屏 SEND CAN TEST 按钮按下
 *   → MY_UI.c: can_send_btn_event_cb (LVGL 事件回调)
 *     → UI_RequestCanTestSend()         ← 本函数
 *       → osMessageQueuePut(Can_Cmd_QueueHandle, ...)
 *         → CAN_Task 在 50ms 内收到 CAN_CMD_SEND_TEST
 *           → CAN_SendMessage() → 硬件发送
 *
 * 这个函数本身在 UI_Task 上下文中执行（因为 LVGL 回调由 UI_Task 触发）
 */
void UI_RequestCanTestSend(void)
{
    if (Can_Cmd_QueueHandle != NULL)
    {
        /*
         * 构造一个 CAN_CMD_SEND_TEST 命令，非阻塞放入队列
         * 如果队列满了（说明 CAN_Task 来不及处理），该命令被丢弃
         */
        CAN_Command_t cmd = { .type = CAN_CMD_SEND_TEST };
        osMessageQueuePut(Can_Cmd_QueueHandle, &cmd, 0, 0);
    }
}

/* =========================================================================
 * FreeRTOS 钩子函数：vApplicationStackOverflowHook
 * =========================================================================
 *
 * 【RTOS 学习点 12：栈溢出保护机制】
 *   FreeRTOS 提供两种栈溢出检测（在 FreeRTOSConfig.h 中配置）：
 *   方法 1 (configCHECK_FOR_STACK_OVERFLOW=1):
 *     任务被换出时检查栈顶的 0xA5 是否被覆盖
 *     被覆盖 → 调用本函数
 *
 *   方法 2 (configCHECK_FOR_STACK_OVERFLOW=2):
 *     方法 1 + 任务创建时检查栈完整性
 *
 *   本函数是最后一道防线：检测到溢出后：
 *     1. 打印出问题的任务名
 *     2. 关闭全局中断（__disable_irq）
 *     3. 死循环（防止系统在损坏状态下继续运行）
 *
 *   ★ 生产环境中应让看门狗复位，而非死循环。
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    printf("\r\n[\344\270\245\351\207\215\351\224\231\350\257\257] \344\273\273\345\212\241\346\240\210\346\272\242\345\207\272!!!: %s !\r\n", pcTaskName);

    /*
     * 关闭所有中断，防止其他任务或 ISR 继续执行
     * 使用 BASEPRI 而不是 PRIMASK 可以保留 FreeRTOS 内核 tick 中断
     */
    __disable_irq();

    /* 死循环，等待看门狗复位（如果启用的话） */
    while (1);
}

/* =========================================================================
 * 任务 6/6：BMS_Task — 电池数据模拟与周期性更新
 * =========================================================================
 *
 * 【角色】BMS 数据生产者，周期更新电池参数
 * 【优先级】osPriorityNormal（与 UI_Task 同级）
 * 【周期】每 100ms
 * 【栈】1024 字节
 *
 * 【RTOS 学习点 13：优先级相同时的时间片轮转】
 *   BMS_Task 和 UI_Task 都是 osPriorityNormal。
 *   FreeRTOS 默认开启同优先级时间片轮转 (configUSE_TIME_SLICING=1)。
 *   当一个 tick 到来时，如果多个同优先级任务 Ready：
 *     → 当前运行的任务被移到同一优先级链表末尾
 *     → 链表头的下一个任务获得 CPU
 *     → 轮流执行
 *
 *   所以：BMS_Task 被 vTaskDelay(100) 阻塞时，UI_Task 独占 CPU。
 *   当 100ms 后 BMS_Task 醒来，如果恰好 UI_Task 在运行，
 *   调度器会在下一个 tick(1ms) 让 BMS_Task 也分到时间片。
 *
 * 【RTOS 学习点 14：时间片轮转的粒度】
 *   tick = 1ms（HAL 默认），意味着最多 1ms 后就会切换同优先级任务。
 *   如果 UI_Task 在一次 lv_timer_handler 中耗时 3ms，
 *   中间 BMS_Task 的 100ms 到期了，也要等 3ms 结束后才能切换。
 *   但 CAN_Task (High) 不受此限制，可以立即抢占。
 */
void BMS_Task_Entry(void *argument)
{
    printf("[BMS] Task started!\r\n");

    while(1)
    {
        /*
         * BMS_Update() 内部用正弦函数模拟电池数据波动：
         *   SOC    : 80%-90% 之间波动 (tick*0.01 慢速)
         *   电压   : 50V-54V 之间波动 (tick*0.015 + 0.5 相位偏移)
         *   电流   : -20A 到 -10A 之间 (tick*0.02 稍快)
         *
         * 实际项目中应替换为从 BMS 采集板读取真实数据
         */
        BMS_Update();

        /*
         * BMS_GetData() 返回指向静态 g_bms_data 的指针
         * 将数据拷贝到消息体并推送到 UI 队列
         */
        const BMS_Data_t *data = BMS_GetData();
        if (data != NULL && UI_Message_QueueHandle != NULL)
        {
            UI_Message_t msg;
            msg.type = UI_MSG_BMS_UPDATE;
            msg.payload.bms = *data;  /* 值拷贝，不依赖指针生命周期 */

            /*
             * 非阻塞推送，队列满了丢弃
             * 100ms 一帧，队列容量 16，足够缓冲 1.6 秒的数据
             */
            osMessageQueuePut(UI_Message_QueueHandle, &msg, 0, 0);
        }

        /*
         * vTaskDelay(100ms) — 将任务置于 Blocked 态
         *
         * ★ 这 100ms 内 BMS_Task 完全不消耗 CPU！
         * CPU 可以被 UI_Task 用于渲染、被 CAN_Task 用于通信。
         * 这就是 RTOS 的核心优势：CPU 永远在干有用的事。
         */
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
