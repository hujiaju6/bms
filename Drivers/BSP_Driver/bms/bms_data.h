#ifndef __BMS_DATA_H
#define __BMS_DATA_H

#include <stdint.h>

typedef struct {
    uint8_t soc;          // 0~100%
    float voltage;        // 总电压 (V)
    float current;        // 总电流 (A)，正为放电
    float soh;            // 健康度 (%)
    uint16_t cycle;       // 循环次数
    float max_cell_v;     // 最高单体电压
    float min_cell_v;     // 最低单体电压
    float max_temp;       // 最高温度 (°C)
    float min_temp;       // 最低温度 (°C)
    uint8_t fault_flags;  // 故障标志位（见下方定义）
} BMS_Data_t;

// 故障标志位（可按位组合）
#define BMS_FAULT_OV       0x01   // 过压
#define BMS_FAULT_UV       0x02   // 欠压
#define BMS_FAULT_OT       0x04   // 过温
#define BMS_FAULT_SC       0x08   // 短路

#endif