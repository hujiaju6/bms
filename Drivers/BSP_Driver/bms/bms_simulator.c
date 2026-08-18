#include "bms_simulator.h"
#include <math.h>
#include <stdio.h>

static BMS_Data_t g_bms_data;
static uint32_t tick = 0;

void BMS_Init(void)
{
    printf("[BMS] Init called\r\n");
    g_bms_data.soc = 85;
    g_bms_data.voltage = 52.35f;
    g_bms_data.current = -14.8f;
    g_bms_data.soh = 98.5f;
    g_bms_data.cycle = 142;
    g_bms_data.max_cell_v = 3.420f;
    g_bms_data.min_cell_v = 3.315f;
    g_bms_data.max_temp = 32.4f;
    g_bms_data.min_temp = 28.1f;
    g_bms_data.fault_flags = 0;
    tick = 0;
    printf("[BMS] Init done, SOC=%d, V=%.2f\r\n", g_bms_data.soc, g_bms_data.voltage);
}

void BMS_Update(void)
{
    static uint32_t update_cnt = 0;
    update_cnt++;
    if (update_cnt % 10 == 0) {
        printf("[BMS] Update #%lu: soc=%u, volt=%.2f, cur=%.2f\r\n",
               update_cnt, g_bms_data.soc, g_bms_data.voltage, g_bms_data.current);
    }

    // ---- 模拟数据更新 ----
    tick++;
    float t = tick * 0.01f;
    g_bms_data.soc = (uint8_t)(85 + 5 * sinf(t * 0.1f));
    if (g_bms_data.soc > 100) g_bms_data.soc = 100;
    if (g_bms_data.soc < 80)  g_bms_data.soc = 80;
    g_bms_data.voltage = 52.0f + 2.0f * sinf(t * 0.15f + 0.5);
    g_bms_data.current = -15.0f + 5.0f * sinf(t * 0.2f);

    (void)0; // update internal data only, UI updates occur in UI task
}

const BMS_Data_t* BMS_GetData(void)
{
    return &g_bms_data;
}

void BMS_FaultInject(uint8_t fault_bit)
{
    g_bms_data.fault_flags |= fault_bit;
    printf("[BMS] Fault injected: 0x%02X, current flags=0x%02X\r\n",
           fault_bit, g_bms_data.fault_flags);
}

void BMS_ClearFaults(void)
{
    g_bms_data.fault_flags = 0;
    printf("[BMS] All faults cleared\r\n");
}