#ifndef __BMS_SIMULATOR_H
#define __BMS_SIMULATOR_H

#include "bms_data.h"

void BMS_Init(void);
void BMS_Update(void);              // 更新内部数据并刷新 UI
const BMS_Data_t* BMS_GetData(void);
void BMS_FaultInject(uint8_t fault_bit);   // 故障注入（置位指定标志）
void BMS_ClearFaults(void);                // 清除全部故障标志

#endif