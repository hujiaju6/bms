#ifndef __MY_UI_H
#define __MY_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

/* ── 故障注入系统：全局对象引用 ── */
extern lv_obj_t * fault_flags_label;
extern lv_obj_t * system_alarm_label;
extern lv_obj_t * card_overview_v;
extern lv_obj_t * card_overview_i;
extern lv_obj_t * card_overview_meter;

/* ── 故障联动刷新接口（由 freertos.c / UI_Task 调用）── */
void UI_RefreshFaultPage(uint8_t fault_flags);
void UI_RefreshAlarmBar(uint8_t fault_flags);
void UI_SetCardFlash(uint8_t enable);

// 原始数据声明
extern lv_obj_t * can_status_label;
extern lv_obj_t * can_data_label;
extern lv_obj_t * bms_soc_label;
extern lv_obj_t * bms_voltage_label;
extern lv_obj_t * bms_current_label;
extern lv_obj_t * rs485_data_label;
extern lv_obj_t * time_label;

/* Storage 页面全局标签 */
extern lv_obj_t * l_sd_storage;
extern lv_obj_t * l_csv_storage;

extern lv_obj_t             * bms_chart;
extern lv_chart_series_t    * bms_chart_ser_v;
extern lv_chart_series_t    * bms_chart_ser_i;
extern lv_obj_t             * bms_meter;
extern lv_meter_indicator_t * bms_meter_indic;

/**
  * @brief  UI触发CAN测试发送
  */
void UI_RequestCanTestSend(void);

/**
  * @brief  工业级科技感仪表盘 UI
  * @param  无
  * @retval 无
  */
void create_pretty_dashboard_ui(void);
void create_touch_demo_ui(void);

/**
  * @brief  刷新 Storage 页面 (由 Storage_Task 调用)
  * @param  status:    0=无卡, 1=已挂载(显示容量), 2=新CSV记录
  * @param  csv_line:  status==2 时传入最新 CSV 行文本
  * @param  total_mb:  SD 卡总容量 (MB), status==1 时有效
  * @param  free_mb:   SD 卡剩余空间 (MB), status==1 时有效
  * @retval 无
  */
void UI_RefreshStoragePage(uint8_t status, const char *csv_line,
                           uint32_t total_mb, uint32_t free_mb);
#ifdef __cplusplus
}
#endif

#endif /* __MY_UI_H */