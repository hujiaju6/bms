#include "MY_UI.h"
#include "ui_messages.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bms_simulator.h"

/* 简体中文字体（自定义生成，含界面全部汉字 + LVGL 图标） */
LV_FONT_DECLARE(lv_font_simsun_14_cjk);

/* LV_SYMBOL_CHART 在 LVGL 8.2.0 中不存在，手动定义 */
#ifndef LV_SYMBOL_CHART
#define LV_SYMBOL_CHART "\xEF\x88\x80" /* FontAwesome fa-chart-line */
#endif

/* =========================================================================
 * 工业级配色方案 (Industrial Dark/Light High-Contrast Theme)
 * ========================================================================= */
#define COLOR_BG            0xF8FAFC // 页面浅灰背景
#define COLOR_CARD          0xFFFFFF // 卡片白色
#define COLOR_BORDER        0xE2E8F0 // 边框浅灰
#define COLOR_PRIMARY       0x0284C7 // 主色调(蓝)
#define COLOR_GREEN         0x10B981 // 正常/安全(绿)
#define COLOR_YELLOW        0xF59E0B // 警告(黄)
#define COLOR_RED           0xEF4444 // 故障/错误(红)
#define COLOR_TEXT_DARK     0x0F172A // 深色文字
#define COLOR_TEXT_MUTED    0x64748B // 浅色文字

/* 全局 UI 控制结构体 */
typedef struct {
    lv_obj_t * top_bar;
    lv_obj_t * label_time;
    lv_obj_t * label_title;
    lv_obj_t * content_area;
    lv_obj_t * nav_btns[4];
    uint8_t current_page;
} ui_os_t;

static ui_os_t g_ui;

/* 4 个页面容器（预创建，切换时只做显示/隐藏） */
static lv_obj_t * page_containers[4];

/* 全局 CAN 状态显示标签（TX: SUCCESS/FAILED），由 freertos.c 中任务使用 */
lv_obj_t * can_status_label = NULL;
/* 全局 CAN 数据接收显示标签，由 freertos.c 中任务使用 */
lv_obj_t * can_data_label = NULL;

/* 全局 BMS 数据显示标签，由 bms_simulator.c 任务使用 */
lv_obj_t * bms_soc_label = NULL;
lv_obj_t * bms_voltage_label = NULL;
lv_obj_t * bms_current_label = NULL;

/* 全局 RS485 数据接收显示标签 */
lv_obj_t * rs485_data_label = NULL;

/* 全局 BMS 仪表盘/曲线图控件，由 freertos.c 中的 BMS 处理更新 */
lv_obj_t             * bms_chart       = NULL;
lv_chart_series_t    * bms_chart_ser_v = NULL;
lv_chart_series_t    * bms_chart_ser_i = NULL;
lv_obj_t             * bms_meter       = NULL;
lv_meter_indicator_t * bms_meter_indic = NULL;

/* 顶部时间标签，由 freertos.c 中 Sys_Task 每秒更新 */
lv_obj_t * time_label = NULL;

/* Storage 页面全局标签 — 由 Storage_Task 动态更新 */
lv_obj_t * l_sd_storage  = NULL;
lv_obj_t * l_csv_storage = NULL;

/* 故障注入系统：Fault 页面动态标签 */
lv_obj_t * fault_flags_label = NULL;
/* 故障注入系统：顶部状态栏报警文字 */
lv_obj_t * system_alarm_label = NULL;
/* 故障注入系统：BMS Overview 数据卡片引用（用于红闪边框） */
lv_obj_t * card_overview_v = NULL;
lv_obj_t * card_overview_i = NULL;
lv_obj_t * card_overview_meter = NULL;
/* 故障注入系统：红闪定时器 */
static lv_timer_t * flash_timer = NULL;
static uint8_t flash_state = 0;

/* 函数声明 */
static void load_page_bms_main(lv_obj_t * parent);
static void load_page_bms_fault(lv_obj_t * parent);
static void load_page_modbus(lv_obj_t * parent);
static void load_page_storage(lv_obj_t * parent);
static void switch_page(uint8_t page_idx);

/* CAN 发送按钮点击事件回调 */
static void can_send_btn_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED) {
        printf("[BTN] CAN send button pressed, request CAN task to send test frame\r\n");
        UI_RequestCanTestSend();
    }
}

/* 通用卡片生成函数 */
static lv_obj_t * create_industrial_card(lv_obj_t * parent, int32_t w, int32_t h)
{
    lv_obj_t * card = lv_obj_create(parent);
    lv_obj_set_size(card, w, h);
    lv_obj_set_style_bg_color(card, lv_color_hex(COLOR_CARD), LV_PART_MAIN);
    lv_obj_set_style_border_color(card, lv_color_hex(COLOR_BORDER), LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(card, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, 12, LV_PART_MAIN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    return card;
}

/* =========================================================================
 * 故障注入按钮回调（Fault 页面 → 弹窗选择故障类型）
 * ========================================================================= */
static void fault_type_btn_cb(lv_event_t * e)
{
    uint8_t fault_bit = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    BMS_FaultInject(fault_bit);
    /* 关闭弹窗 */
    lv_obj_t * btn = lv_event_get_target(e);
    lv_obj_t * popup = lv_obj_get_parent(btn);
    lv_obj_del(popup);
}

static void fault_inject_btn_cb(lv_event_t * e)
{
    (void)e;
    static const char *fault_names[] = {"\350\277\207\345\216\213 (OV)", "\346\254\240\345\216\213 (UV)", "\350\277\207\346\270\251 (OT)", "\347\237\255\350\267\257 (SC)"};
    static const uint8_t  fault_bits[]  = {BMS_FAULT_OV, BMS_FAULT_UV, BMS_FAULT_OT, BMS_FAULT_SC};
    static const lv_color_t fault_colors[] = {
        LV_COLOR_MAKE(0xEF, 0x44, 0x44),
        LV_COLOR_MAKE(0xF5, 0x9E, 0x0B),
        LV_COLOR_MAKE(0xEF, 0x44, 0x44),
        LV_COLOR_MAKE(0xEF, 0x44, 0x44),
    };

    lv_obj_t * popup = lv_obj_create(lv_layer_top());
    lv_obj_set_size(popup, 260, 200);
    lv_obj_center(popup);
    lv_obj_set_style_bg_color(popup, lv_color_hex(COLOR_CARD), LV_PART_MAIN);
    lv_obj_set_style_border_width(popup, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(popup, lv_color_hex(COLOR_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_radius(popup, 8, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(popup, 20, LV_PART_MAIN);
    lv_obj_set_layout(popup, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(popup, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(popup, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(popup, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_row(popup, 8, LV_PART_MAIN);

    lv_obj_t * title = lv_label_create(popup);
    lv_label_set_text(title, "\351\200\211\346\213\251\346\225\205\351\232\234\347\261\273\345\236\213:");
    lv_obj_set_style_text_font(title, &lv_font_simsun_14_cjk, LV_PART_MAIN);

    for (int i = 0; i < 4; i++) {
        lv_obj_t * btn = lv_btn_create(popup);
        lv_obj_set_size(btn, 220, 30);
        lv_obj_set_style_bg_color(btn, fault_colors[i], LV_PART_MAIN);
        lv_obj_set_style_radius(btn, 4, LV_PART_MAIN);
        lv_obj_t * lbl = lv_label_create(btn);
        lv_label_set_text(lbl, fault_names[i]);
        lv_obj_set_style_text_font(lbl, &lv_font_simsun_14_cjk, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_center(lbl);
        lv_obj_add_event_cb(btn, fault_type_btn_cb, LV_EVENT_PRESSED, (void *)(uintptr_t)fault_bits[i]);
    }

    lv_obj_t * btn_cancel = lv_btn_create(popup);
    lv_obj_set_size(btn_cancel, 220, 30);
    lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(COLOR_TEXT_MUTED), LV_PART_MAIN);
    lv_obj_set_style_radius(btn_cancel, 4, LV_PART_MAIN);
    lv_obj_t * lbl_c = lv_label_create(btn_cancel);
    lv_label_set_text(lbl_c, "\345\217\226\346\266\210");
    lv_obj_set_style_text_font(lbl_c, &lv_font_simsun_14_cjk, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_c, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_center(lbl_c);
    lv_obj_add_event_cb(btn_cancel, fault_type_btn_cb, LV_EVENT_PRESSED, (void *)(uintptr_t)0);
}

static void fault_clear_btn_cb(lv_event_t * e)
{
    (void)e;
    BMS_ClearFaults();
}

/* =========================================================================
 * 1. BMS 核心概览页面 (Overview) - 含 CAN 底层回环测试
 * ========================================================================= */
static void load_page_bms_main(lv_obj_t * parent)
{
    lv_obj_set_layout(parent, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(parent, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_top(parent, 8, LV_PART_MAIN);

    /* ══════════════════════════════════════════════════════════════════════
     * 第 1 行：SOC 环形仪表盘 (448 x 170)
     * ══════════════════════════════════════════════════════════════════════ */
    lv_obj_t * card_meter = create_industrial_card(parent, 448, 170);
    card_overview_meter = card_meter;
    lv_obj_set_layout(card_meter, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card_meter, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(card_meter, LV_FLEX_ALIGN_SPACE_AROUND, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* 仪表盘容器 (无背景无边框) */
    lv_obj_t * meter_cont = lv_obj_create(card_meter);
    lv_obj_set_size(meter_cont, 140, 140);
    lv_obj_set_style_bg_opa(meter_cont, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(meter_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(meter_cont, 0, LV_PART_MAIN);
    lv_obj_clear_flag(meter_cont, LV_OBJ_FLAG_SCROLLABLE);

    /* lv_meter 仪表 */
    bms_meter = lv_meter_create(meter_cont);
    lv_obj_remove_style(bms_meter, NULL, LV_PART_MAIN);
    lv_obj_remove_style(bms_meter, NULL, LV_PART_INDICATOR);
    lv_obj_set_size(bms_meter, 140, 140);
    lv_obj_center(bms_meter);

    lv_meter_scale_t * meter_scale = lv_meter_add_scale(bms_meter);
    lv_meter_set_scale_ticks(bms_meter, meter_scale, 11, 2, 8, lv_color_hex(COLOR_TEXT_MUTED));
    lv_meter_set_scale_major_ticks(bms_meter, meter_scale, 5, 3, 12, lv_color_hex(0x1E293B), 10);
    lv_meter_set_scale_range(bms_meter, meter_scale, 0, 100, 270, 135);

    /* 灰底背景弧 */
    lv_meter_indicator_t * indic_bg = lv_meter_add_arc(bms_meter, meter_scale, 8, lv_color_hex(COLOR_BORDER), -5);
    lv_meter_set_indicator_start_value(bms_meter, indic_bg, 0);
    lv_meter_set_indicator_end_value(bms_meter, indic_bg, 100);

    /* 彩色弧：绿 0-70 / 黄 70-90 / 红 90-100 */
    lv_meter_indicator_t * indic_g = lv_meter_add_arc(bms_meter, meter_scale, 6, lv_color_hex(COLOR_GREEN), -2);
    lv_meter_set_indicator_start_value(bms_meter, indic_g, 0);
    lv_meter_set_indicator_end_value(bms_meter, indic_g, 70);

    lv_meter_indicator_t * indic_y = lv_meter_add_arc(bms_meter, meter_scale, 6, lv_color_hex(COLOR_YELLOW), -2);
    lv_meter_set_indicator_start_value(bms_meter, indic_y, 70);
    lv_meter_set_indicator_end_value(bms_meter, indic_y, 90);

    lv_meter_indicator_t * indic_r = lv_meter_add_arc(bms_meter, meter_scale, 6, lv_color_hex(COLOR_RED), -2);
    lv_meter_set_indicator_start_value(bms_meter, indic_r, 90);
    lv_meter_set_indicator_end_value(bms_meter, indic_r, 100);

    /* 指针 */
    bms_meter_indic = lv_meter_add_needle_line(bms_meter, meter_scale, 4, lv_color_hex(0x1E293B), -18);
    lv_meter_set_indicator_value(bms_meter, bms_meter_indic, 85);

    /* SOC 百分比文字 (覆盖在仪表盘中央) */
    bms_soc_label = lv_label_create(meter_cont);
    lv_label_set_text(bms_soc_label, "85%");
    lv_obj_set_style_text_font(bms_soc_label, &lv_font_montserrat_22, LV_PART_MAIN);
    lv_obj_set_style_text_color(bms_soc_label, lv_color_hex(0x1E293B), LV_PART_MAIN);
    lv_obj_center(bms_soc_label);

    /* 右侧信息栏 */
    lv_obj_t * info_cont = lv_obj_create(card_meter);
    lv_obj_set_size(info_cont, 240, 140);
    lv_obj_set_style_bg_opa(info_cont, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(info_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(info_cont, 0, LV_PART_MAIN);
    lv_obj_clear_flag(info_cont, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * lbl_info = lv_label_create(info_cont);
    lv_label_set_recolor(lbl_info, true);
    lv_label_set_text(lbl_info,
        "#000000 BMS \347\212\266\346\200\201:\346\255\243\345\270\270#\n\n"
        "#64748B \345\201\245\345\272\267\345\272\246:98.5%  |  \345\276\252\347\216\257:142\346\254\241#\n"
        "#64748B \347\224\265\350\212\257:16S1P  \351\224\202\347\246\273\345\255\220#");
    lv_obj_set_style_text_font(lbl_info, &lv_font_simsun_14_cjk, LV_PART_MAIN);
    lv_obj_align(lbl_info, LV_ALIGN_CENTER, 0, 0);

    /* ══════════════════════════════════════════════════════════════════════
     * 第 2 行：实时电压/电流趋势曲线图 (448 x 220)
     * ══════════════════════════════════════════════════════════════════════ */
    lv_obj_t * card_chart = create_industrial_card(parent, 448, 220);

    lv_obj_t * chart_title = lv_label_create(card_chart);
    lv_label_set_text(chart_title, LV_SYMBOL_CHART"  \347\224\265\345\216\213/\347\224\265\346\265\201\345\256\236\346\227\266\350\266\213\345\212\277");
    lv_obj_set_style_text_font(chart_title, &lv_font_simsun_14_cjk, LV_PART_MAIN);
    lv_obj_set_style_text_color(chart_title, lv_color_hex(COLOR_TEXT_MUTED), LV_PART_MAIN);
    lv_obj_align(chart_title, LV_ALIGN_TOP_LEFT, 5, 2);

    /* 图例 */
    lv_obj_t * legend = lv_obj_create(card_chart);
    lv_obj_set_size(legend, 220, 22);
    lv_obj_set_style_bg_opa(legend, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(legend, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(legend, 0, LV_PART_MAIN);
    lv_obj_clear_flag(legend, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(legend, LV_ALIGN_TOP_RIGHT, -2, 0);

    lv_obj_t * leg_v = lv_label_create(legend);
    lv_label_set_recolor(leg_v, true);
    lv_label_set_text(leg_v, "#3B82F6 "LV_SYMBOL_MINUS" V#   #10B981 "LV_SYMBOL_MINUS" A#");
    lv_obj_set_style_text_font(leg_v, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_center(leg_v);

    /* lv_chart 双轴曲线 */
    bms_chart = lv_chart_create(card_chart);
    lv_obj_set_size(bms_chart, 426, 190);
    lv_obj_align(bms_chart, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_chart_set_type(bms_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(bms_chart, 60);
    lv_chart_set_div_line_count(bms_chart, 5, 6);
    lv_chart_set_update_mode(bms_chart, LV_CHART_UPDATE_MODE_SHIFT);

    lv_obj_set_style_line_width(bms_chart, 2, LV_PART_ITEMS);
    lv_obj_set_style_size(bms_chart, 0, LV_PART_INDICATOR); /* 隐藏数据点 */

    /* Y 轴范围: 电压 30000-60000mV, 电流 -25000-25000mA */
    lv_chart_set_range(bms_chart, LV_CHART_AXIS_PRIMARY_Y, 30000, 60000);
    lv_chart_set_range(bms_chart, LV_CHART_AXIS_SECONDARY_Y, -25000, 25000);

    /* Y 轴单位标签 */
    lv_obj_t * y_label_v = lv_label_create(card_chart);
    lv_label_set_text(y_label_v, "\347\224\265\345\216\213\n(mV)");
    lv_obj_set_style_text_color(y_label_v, lv_color_hex(COLOR_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_text_font(y_label_v, &lv_font_simsun_14_cjk, LV_PART_MAIN);
    lv_obj_align(y_label_v, LV_ALIGN_LEFT_MID, 2, 15);

    lv_obj_t * y_label_i = lv_label_create(card_chart);
    lv_label_set_text(y_label_i, "\347\224\265\346\265\201\n(mA)");
    lv_obj_set_style_text_color(y_label_i, lv_color_hex(COLOR_GREEN), LV_PART_MAIN);
    lv_obj_set_style_text_font(y_label_i, &lv_font_simsun_14_cjk, LV_PART_MAIN);
    lv_obj_align(y_label_i, LV_ALIGN_RIGHT_MID, -2, 15);

    bms_chart_ser_v = lv_chart_add_series(bms_chart, lv_color_hex(COLOR_PRIMARY), LV_CHART_AXIS_PRIMARY_Y);
    bms_chart_ser_i = lv_chart_add_series(bms_chart, lv_color_hex(COLOR_GREEN), LV_CHART_AXIS_SECONDARY_Y);

    /* ══════════════════════════════════════════════════════════════════════
     * 第 3 行：数据卡片行 —— 总电压 + 总电流 (448 x 115)
     * ══════════════════════════════════════════════════════════════════════ */
    lv_obj_t * row_data = lv_obj_create(parent);
    lv_obj_set_size(row_data, 448, 115);
    lv_obj_set_style_bg_opa(row_data, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(row_data, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(row_data, 0, LV_PART_MAIN);
    lv_obj_set_layout(row_data, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row_data, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_data, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(row_data, LV_OBJ_FLAG_SCROLLABLE);

    /* 电压卡片 */
    lv_obj_t * card_v = create_industrial_card(row_data, 218, 115);
    card_overview_v = card_v;
    lv_obj_t * l_vt = lv_label_create(card_v);
    lv_label_set_text(l_vt, "\346\200\273\347\224\265\345\216\213");
    lv_obj_set_style_text_font(l_vt, &lv_font_simsun_14_cjk, LV_PART_MAIN);
    lv_obj_set_style_text_color(l_vt, lv_color_hex(COLOR_TEXT_MUTED), LV_PART_MAIN);
    lv_obj_align(l_vt, LV_ALIGN_TOP_LEFT, 0, 8);

    bms_voltage_label = lv_label_create(card_v);
    lv_label_set_text(bms_voltage_label, "52.35 V");
    lv_obj_set_style_text_font(bms_voltage_label, &lv_font_montserrat_26, LV_PART_MAIN);
    lv_obj_set_style_text_color(bms_voltage_label, lv_color_hex(COLOR_GREEN), LV_PART_MAIN);
    lv_obj_align(bms_voltage_label, LV_ALIGN_BOTTOM_LEFT, 0, -8);

    /* 电流卡片 */
    lv_obj_t * card_i = create_industrial_card(row_data, 218, 115);
    card_overview_i = card_i;
    lv_obj_t * l_it = lv_label_create(card_i);
    lv_label_set_text(l_it, "\346\200\273\347\224\265\346\265\201");
    lv_obj_set_style_text_font(l_it, &lv_font_simsun_14_cjk, LV_PART_MAIN);
    lv_obj_set_style_text_color(l_it, lv_color_hex(COLOR_TEXT_MUTED), LV_PART_MAIN);
    lv_obj_align(l_it, LV_ALIGN_TOP_LEFT, 0, 8);

    bms_current_label = lv_label_create(card_i);
    lv_label_set_text(bms_current_label, "+14.8 A");
    lv_obj_set_style_text_font(bms_current_label, &lv_font_montserrat_26, LV_PART_MAIN);
    lv_obj_set_style_text_color(bms_current_label, lv_color_hex(COLOR_PRIMARY), LV_PART_MAIN);
    lv_obj_align(bms_current_label, LV_ALIGN_BOTTOM_LEFT, 0, -8);

    /* ══════════════════════════════════════════════════════════════════════
     * 第 4 行：CAN 测试收发卡片 (448 x 160)
     * ══════════════════════════════════════════════════════════════════════ */
    lv_obj_t * card_can = create_industrial_card(parent, 448, 160);

    /* SEND CAN 按钮 */
    lv_obj_t * btn_can = lv_btn_create(card_can);
    lv_obj_set_size(btn_can, 200, 38);
    lv_obj_align(btn_can, LV_ALIGN_TOP_MID, 0, 6);
    lv_obj_add_event_cb(btn_can, can_send_btn_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_set_style_bg_color(btn_can, lv_color_hex(COLOR_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_radius(btn_can, 6, LV_PART_MAIN);

    lv_obj_t * btn_lbl = lv_label_create(btn_can);
    lv_label_set_text(btn_lbl, LV_SYMBOL_UPLOAD"  \345\217\221\351\200\201CAN\346\265\213\350\257\225");
    lv_obj_set_style_text_font(btn_lbl, &lv_font_simsun_14_cjk, LV_PART_MAIN);
    lv_obj_center(btn_lbl);

    /* CAN 发送状态标签（TX: SUCCESS / TX: FAILED） */
    can_status_label = lv_label_create(card_can);
    lv_label_set_text(can_status_label, "\345\217\221\351\200\201:\347\251\272\351\227\262");
    lv_obj_set_style_text_font(can_status_label, &lv_font_simsun_14_cjk, LV_PART_MAIN);
    lv_obj_set_style_text_color(can_status_label, lv_color_hex(0xF59E0B), LV_PART_MAIN);
    lv_obj_align(can_status_label, LV_ALIGN_TOP_MID, 0, 50);

    /* CAN 接收数据展示标签 */
    can_data_label = lv_label_create(card_can);
    lv_obj_set_style_text_font(can_data_label, &lv_font_simsun_14_cjk, LV_PART_MAIN);
    lv_obj_set_width(can_data_label, lv_pct(95));

    /* 从缓存恢复上次 CAN 数据 */
    extern CAN_DataPacket_t g_last_can_rx;
    extern uint8_t g_has_last_can_rx;
    if (g_has_last_can_rx) {
        char buf[128];
        snprintf(buf, sizeof(buf),
            "%02X %02X %02X %02X %02X %02X %02X %02X",
            g_last_can_rx.data[0], g_last_can_rx.data[1],
            g_last_can_rx.data[2], g_last_can_rx.data[3],
            g_last_can_rx.data[4], g_last_can_rx.data[5],
            g_last_can_rx.data[6], g_last_can_rx.data[7]);
        lv_label_set_text(can_data_label, buf);
        lv_obj_set_style_text_color(can_data_label, lv_color_hex(0x10B981), LV_PART_MAIN);
    } else {
        lv_label_set_text(can_data_label, "--");
        lv_obj_set_style_text_color(can_data_label, lv_color_hex(COLOR_GREEN), LV_PART_MAIN);
    }
    lv_obj_align(can_data_label, LV_ALIGN_TOP_MID, 0, 75);
}

/* =========================================================================
 * 2. 单节电压/故障报警页面 (Cell & Fault)
 * ========================================================================= */
static void load_page_bms_fault(lv_obj_t * parent)
{
    lv_obj_set_layout(parent, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(parent, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_top(parent, 10, LV_PART_MAIN);

    lv_obj_t * card_cell = create_industrial_card(parent, 448, 145);
    lv_obj_t * l_cell = lv_label_create(card_cell);
    lv_label_set_text(l_cell, "\345\215\225\344\275\223\347\224\265\345\216\213\344\270\216\346\270\251\345\272\246\346\236\201\345\200\274\n\n"
                              "\346\234\200\351\253\230\345\215\225\344\275\223\347\224\265\345\216\213:3.420V (\347\254\25412\350\212\202)\n"
                              "\346\234\200\344\275\216\345\215\225\344\275\223\347\224\265\345\216\213:3.315V (\347\254\25405\350\212\202)\n"
                              "\346\234\200\351\253\230\346\270\251\345\272\246:32.4\342\204\203  |  \346\234\200\344\275\216\346\270\251\345\272\246:28.1\342\204\203");
    lv_obj_set_style_text_font(l_cell, &lv_font_simsun_14_cjk, LV_PART_MAIN);
    lv_obj_set_style_text_color(l_cell, lv_color_hex(COLOR_TEXT_DARK), LV_PART_MAIN);
    lv_obj_center(l_cell);

    lv_obj_t * card_fault = create_industrial_card(parent, 448, 175);
    lv_obj_t * l_ftitle = lv_label_create(card_fault);
    lv_label_set_text(l_ftitle, "\346\225\205\351\232\234\346\212\245\350\255\246\347\212\266\346\200\201\346\240\207\345\277\227");
    lv_obj_set_style_text_font(l_ftitle, &lv_font_simsun_14_cjk, LV_PART_MAIN);
    lv_obj_set_style_text_color(l_ftitle, lv_color_hex(COLOR_TEXT_MUTED), LV_PART_MAIN);
    lv_obj_align(l_ftitle, LV_ALIGN_TOP_LEFT, 5, 0);

    /* 使用全局指针：由 UI_RefreshFaultPage() 动态更新 [X]/[ ] 和颜色 */
    fault_flags_label = lv_label_create(card_fault);
    lv_label_set_text(fault_flags_label,
        "[ ] \350\277\207\345\216\213 (OV)\n"
        "[ ] \346\254\240\345\216\213 (UV)\n"
        "[ ] \350\277\207\346\270\251 (OT)\n"
        "[ ] \347\237\255\350\267\257 (SC)");
    lv_obj_set_style_text_font(fault_flags_label, &lv_font_simsun_14_cjk, LV_PART_MAIN);
    lv_obj_set_style_text_color(fault_flags_label, lv_color_hex(COLOR_GREEN), LV_PART_MAIN);
    lv_obj_align(fault_flags_label, LV_ALIGN_BOTTOM_LEFT, 5, -5);

    /* 故障注入按钮 */
    lv_obj_t * btn_inject = lv_btn_create(parent);
    lv_obj_set_size(btn_inject, 200, 38);
    lv_obj_set_style_bg_color(btn_inject, lv_color_hex(COLOR_YELLOW), LV_PART_MAIN);
    lv_obj_set_style_radius(btn_inject, 6, LV_PART_MAIN);
    lv_obj_add_event_cb(btn_inject, fault_inject_btn_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_t * btn_inj_lbl = lv_label_create(btn_inject);
    lv_label_set_text(btn_inj_lbl, LV_SYMBOL_WARNING "  \346\225\205\351\232\234\346\263\250\345\205\245");
    lv_obj_set_style_text_font(btn_inj_lbl, &lv_font_simsun_14_cjk, LV_PART_MAIN);
    lv_obj_set_style_text_color(btn_inj_lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_center(btn_inj_lbl);

    /* 清除故障按钮 */
    lv_obj_t * btn_clear = lv_btn_create(parent);
    lv_obj_set_size(btn_clear, 200, 38);
    lv_obj_set_style_bg_color(btn_clear, lv_color_hex(COLOR_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_radius(btn_clear, 6, LV_PART_MAIN);
    lv_obj_add_event_cb(btn_clear, fault_clear_btn_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_t * btn_clr_lbl = lv_label_create(btn_clear);
    lv_label_set_text(btn_clr_lbl, LV_SYMBOL_REFRESH "  \346\270\205\351\231\244\346\225\205\351\232\234");
    lv_obj_set_style_text_font(btn_clr_lbl, &lv_font_simsun_14_cjk, LV_PART_MAIN);
    lv_obj_set_style_text_color(btn_clr_lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_center(btn_clr_lbl);
}

/* =========================================================================
 * 3. Modbus 485 轮询数据页面 (Modbus)
 * ========================================================================= */
static void load_page_modbus(lv_obj_t * parent)
{
    lv_obj_set_layout(parent, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(parent, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_top(parent, 10, LV_PART_MAIN);

    lv_obj_t * card_hw = create_industrial_card(parent, 448, 70);
    lv_obj_t * l_hw = lv_label_create(card_hw);
    lv_label_set_text(l_hw, "RS485 \351\200\217\344\274\240\346\250\241\345\274\217\n"
                            "\346\263\242\347\211\271\347\216\207:115200bps | \346\226\271\345\220\221\346\216\247\345\210\266:PC0\n"
                            "\346\255\243\345\234\250\344\273\216RS-485\346\200\273\347\272\277\346\216\245\346\224\266\345\216\237\345\247\213\346\225\260\346\215\256...");
    lv_obj_set_style_text_font(l_hw, &lv_font_simsun_14_cjk, LV_PART_MAIN);
    lv_obj_set_style_text_color(l_hw, lv_color_hex(COLOR_TEXT_DARK), LV_PART_MAIN);
    lv_obj_center(l_hw);

    /* RS485 实时接收数据显示卡片 */
    lv_obj_t * card_rs485 = create_industrial_card(parent, 448, 260);
    lv_obj_t * l_title = lv_label_create(card_rs485);
    lv_label_set_text(l_title, "RS485 \346\216\245\346\224\266\346\225\260\346\215\256 (HEX, \346\234\200\346\226\260\345\270\247):");
    lv_obj_set_style_text_font(l_title, &lv_font_simsun_14_cjk, LV_PART_MAIN);
    lv_obj_set_style_text_color(l_title, lv_color_hex(COLOR_TEXT_MUTED), LV_PART_MAIN);
    lv_obj_align(l_title, LV_ALIGN_TOP_LEFT, 5, 0);

    rs485_data_label = lv_label_create(card_rs485);

    /* 从缓存恢复上次 RS485 接收数据（不显示过时的 TX 自测数据） */
    extern RS485_Data_t g_last_rs485_rx;
    if (g_last_rs485_rx.len > 0) {
        char buf[512];
        int offset = 0;
        offset += snprintf(buf + offset, sizeof(buf) - offset,
            "RS485 \346\216\245\346\224\266[%u\345\255\227\350\212\202]HEX:\n", g_last_rs485_rx.len);
        for (uint16_t i = 0; i < g_last_rs485_rx.len && offset < (int)sizeof(buf) - 8; i++) {
            offset += snprintf(buf + offset, sizeof(buf) - offset,
                "%02X ", g_last_rs485_rx.data[i]);
            if ((i + 1) % 16 == 0 && i + 1 < g_last_rs485_rx.len) {
                offset += snprintf(buf + offset, sizeof(buf) - offset, "\n");
            }
        }
        lv_label_set_text(rs485_data_label, buf);
        lv_obj_set_style_text_color(rs485_data_label, lv_color_hex(0x0284C7), LV_PART_MAIN);
    } else {
        lv_label_set_text(rs485_data_label, "\346\255\243\345\234\250\347\255\211\345\276\205RS-485\346\225\260\346\215\256...");
        lv_obj_set_style_text_color(rs485_data_label, lv_color_hex(COLOR_PRIMARY), LV_PART_MAIN);
    }
    lv_obj_set_style_text_font(rs485_data_label, &lv_font_simsun_14_cjk, LV_PART_MAIN);
    lv_obj_set_style_text_line_space(rs485_data_label, 4, LV_PART_MAIN);
    lv_obj_align(rs485_data_label, LV_ALIGN_TOP_LEFT, 5, 25);
}

/* =========================================================================
 * 4. SDIO 存储与日志页面 (Storage)
 * ========================================================================= */
static void load_page_storage(lv_obj_t * parent)
{
    lv_obj_set_layout(parent, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(parent, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_top(parent, 10, LV_PART_MAIN);

    lv_obj_t * card_sd = create_industrial_card(parent, 448, 145);
    l_sd_storage = lv_label_create(card_sd);
    lv_label_set_text(l_sd_storage, "SDIO+FATFS \345\255\230\345\202\250\347\212\266\346\200\201\n\n"
                            "\345\215\241\347\211\207:\346\234\252\346\217\222\345\205\245\n"
                            "\346\255\243\345\234\250\347\255\211\345\276\205SD\345\215\241...");
    lv_obj_set_style_text_font(l_sd_storage, &lv_font_simsun_14_cjk, LV_PART_MAIN);
    lv_obj_set_style_text_color(l_sd_storage, lv_color_hex(COLOR_YELLOW), LV_PART_MAIN);
    lv_obj_center(l_sd_storage);

    lv_obj_t * card_csv = create_industrial_card(parent, 448, 490);
    lv_obj_set_scroll_dir(card_csv, LV_DIR_VER);

    l_csv_storage = lv_label_create(card_csv);
    lv_label_set_long_mode(l_csv_storage, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(l_csv_storage, 448 - 2 * 12);
    lv_label_set_text(l_csv_storage, "\346\234\200\346\226\260CSV\350\256\260\345\275\225(\345\267\262\345\220\214\346\255\245):\n\n"
                             "(\346\232\202\346\227\240\350\256\260\345\275\225)");
    lv_obj_set_style_text_font(l_csv_storage, &lv_font_simsun_14_cjk, LV_PART_MAIN);
    lv_obj_set_style_text_color(l_csv_storage, lv_color_hex(COLOR_PRIMARY), LV_PART_MAIN);
    lv_obj_align(l_csv_storage, LV_ALIGN_TOP_LEFT, 0, 0);
}

/* =========================================================================
 * 故障联动 UI 更新函数 — 由 freertos.c / UI_Task 调用
 * ========================================================================= */

/* 卡片红闪定时器回调 */
static void flash_card_borders(lv_timer_t * timer)
{
    (void)timer;
    flash_state = !flash_state;
    lv_color_t border_c = flash_state ? lv_color_hex(COLOR_RED)
                                       : lv_color_hex(COLOR_BORDER);
    if (card_overview_v)    lv_obj_set_style_border_color(card_overview_v,    border_c, LV_PART_MAIN);
    if (card_overview_i)    lv_obj_set_style_border_color(card_overview_i,    border_c, LV_PART_MAIN);
    if (card_overview_meter) lv_obj_set_style_border_color(card_overview_meter, border_c, LV_PART_MAIN);
}

/* 根据 fault_flags 刷新 Fault 页面的 [X]/[ ] 和颜色 */
void UI_RefreshFaultPage(uint8_t fault_flags)
{
    if (fault_flags_label == NULL) return;

    char buf[256];
    snprintf(buf, sizeof(buf),
        "[%c] \350\277\207\345\216\213 (OV)\n"
        "[%c] \346\254\240\345\216\213 (UV)\n"
        "[%c] \350\277\207\346\270\251 (OT)\n"
        "[%c] \347\237\255\350\267\257 (SC)",
        (fault_flags & BMS_FAULT_OV) ? 'X' : ' ',
        (fault_flags & BMS_FAULT_UV) ? 'X' : ' ',
        (fault_flags & BMS_FAULT_OT) ? 'X' : ' ',
        (fault_flags & BMS_FAULT_SC) ? 'X' : ' ');

    lv_label_set_text(fault_flags_label, buf);
    lv_color_t color = (fault_flags != 0) ? lv_color_hex(COLOR_RED) : lv_color_hex(COLOR_GREEN);
    lv_obj_set_style_text_color(fault_flags_label, color, LV_PART_MAIN);
}

/* 根据 fault_flags 刷新顶部状态栏报警文字 */
void UI_RefreshAlarmBar(uint8_t fault_flags)
{
    if (system_alarm_label == NULL) return;

    if (fault_flags != 0) {
        char alarm_text[64] = "" LV_SYMBOL_WARNING " \346\225\205\351\232\234:";
        strcpy(alarm_text, LV_SYMBOL_WARNING " \346\225\205\351\232\234:");
        if (fault_flags & BMS_FAULT_OV) strcat(alarm_text, "\350\277\207\345\216\213");
        if (fault_flags & BMS_FAULT_UV) strcat(alarm_text, "\346\254\240\345\216\213");
        if (fault_flags & BMS_FAULT_OT) strcat(alarm_text, "\350\277\207\346\270\251");
        if (fault_flags & BMS_FAULT_SC) strcat(alarm_text, "\347\237\255\350\267\257");
        lv_label_set_text(system_alarm_label, alarm_text);
        lv_obj_set_style_text_color(system_alarm_label, lv_color_hex(COLOR_RED), LV_PART_MAIN);
    } else {
        lv_label_set_text(system_alarm_label, LV_SYMBOL_WIFI " \346\255\243\345\270\270");
        lv_obj_set_style_text_color(system_alarm_label, lv_color_hex(COLOR_GREEN), LV_PART_MAIN);
    }
}

/* 启停 Overview 卡片红闪 */
void UI_SetCardFlash(uint8_t enable)
{
    if (enable && flash_timer == NULL) {
        flash_timer = lv_timer_create(flash_card_borders, 500, NULL);
    } else if (!enable && flash_timer != NULL) {
        lv_timer_del(flash_timer);
        flash_timer = NULL;
        flash_state = 0;
        /* 恢复正常边框 */
        if (card_overview_v)    lv_obj_set_style_border_color(card_overview_v,    lv_color_hex(COLOR_BORDER), LV_PART_MAIN);
        if (card_overview_i)    lv_obj_set_style_border_color(card_overview_i,    lv_color_hex(COLOR_BORDER), LV_PART_MAIN);
        if (card_overview_meter) lv_obj_set_style_border_color(card_overview_meter, lv_color_hex(COLOR_BORDER), LV_PART_MAIN);
    }
}

/* =========================================================================
 * Storage 页面刷新 — 由 Storage_Task 调用
 *   status: 0=无卡, 1=已挂载(显示容量), 2=新CSV记录
 * ========================================================================= */
void UI_RefreshStoragePage(uint8_t status, const char *csv_line,
                           uint32_t total_mb, uint32_t free_mb)
{
    static uint32_t cached_total_mb = 0;
    static uint32_t cached_free_mb  = 0;
    static char sd_status[256];

    /* Cache storage info when mounted */
    if (status == 1) {
        cached_total_mb = total_mb;
        cached_free_mb  = free_mb;
    }

    if (l_sd_storage) {
        if (status == 0) {
            snprintf(sd_status, sizeof(sd_status),
                "SDIO+FATFS \345\255\230\345\202\250\347\212\266\346\200\201\n\n"
                "\345\215\241\347\211\207:\346\234\252\346\217\222\345\205\245\n"
                "\351\251\261\345\212\250:4\344\275\215\346\250\241\345\274\217+DMA (\345\276\205\346\234\272)\n"
                "\346\255\243\345\234\250\347\255\211\345\276\205SD\345\215\241...");
            lv_obj_set_style_text_color(l_sd_storage, lv_color_hex(COLOR_YELLOW), LV_PART_MAIN);
        } else {
            /* status == 1 (mounted) or status == 2 (csv written) */
            snprintf(sd_status, sizeof(sd_status),
                "SDIO+FATFS \345\255\230\345\202\250\347\212\266\346\200\201\n\n"
                "\345\215\241\347\211\207\351\251\261\345\212\250:4\344\275\215\346\250\241\345\274\217+DMA\n"
                "\346\226\207\344\273\266\347\263\273\347\273\237:FatFS (\346\214\202\350\275\275\346\210\220\345\212\237)\n"
                "\345\211\251\344\275\231:%lu MB / %lu MB",
                (unsigned long)cached_free_mb, (unsigned long)cached_total_mb);
            lv_obj_set_style_text_color(l_sd_storage, lv_color_hex(COLOR_GREEN), LV_PART_MAIN);
        }
        lv_label_set_text_static(l_sd_storage, sd_status);
    }

    if (l_csv_storage && status == 2 && csv_line) {
        /* Parse CSV fields and format into 3 human-readable lines per record */
        char parsed[3][64];  /* 3 lines, max 64 chars each */
        memset(parsed, 0, sizeof(parsed));

        /* Make a mutable copy for strtok */
        char csv_copy[256];
        strncpy(csv_copy, csv_line, sizeof(csv_copy) - 1);
        csv_copy[sizeof(csv_copy) - 1] = '\0';

        /* Tokenize: extract fields by comma */
        const char *fields[23];
        int nf = 0;
        char *token = strtok(csv_copy, ",");
        while (token && nf < 23) {
            fields[nf++] = token;
            token = strtok(NULL, ",");
        }

        if (nf >= 19) {  /* Need at least up to min_temp */
            /* Line 1: DateTime + Seq# */
            /* fields[1]="YYYY-MM-DD HH:MM:SS", fields[22]=log_seq */
            const char *dt  = (nf > 1)  ? fields[1]  : "----";
            const char *seq = (nf > 22) ? fields[22] : "0";
            snprintf(parsed[0], sizeof(parsed[0]),
                     "%s  #%s", dt, seq);

            /* Line 2: PackV + Current + SOC */
            /* fields[14]=pack_v_mV, fields[15]=curr_mA, fields[16]=soc */
            float pack_v = (nf > 14) ? (float)atoi(fields[14]) / 1000.0f : 0.0f;
            float curr_a = (nf > 15) ? (float)abs(atoi(fields[15])) / 1000.0f : 0.0f;
            int   soc    = (nf > 16) ? atoi(fields[16]) : 0;
            snprintf(parsed[1], sizeof(parsed[1]),
                     "\347\224\265\345\216\213:%.2fV  |  \347\224\265\346\265\201:%.1fA  |  SOC:%d%%",
                     pack_v, curr_a, soc);

            /* Line 3: Cell Vmax/Vmin + Temp + Fault */
            /* fields[2]=max_cell_mV, fields[3]=min_cell_mV,
               fields[17]=max_temp, fields[18]=min_temp, fields[21]=fault */
            int cell_max = (nf > 2)  ? atoi(fields[2])  : 0;
            int cell_min = (nf > 3)  ? atoi(fields[3])  : 0;
            int temp_max = (nf > 17) ? atoi(fields[17]) : 0;
            int temp_min = (nf > 18) ? atoi(fields[18]) : 0;
            const char *fault = (nf > 21) ? fields[21] : "0x00";
            snprintf(parsed[2], sizeof(parsed[2]),
                     "\345\215\225\344\275\223:%d/%dmV  |  \346\270\251\345\272\246:%d/%d\342\204\203  |  \346\225\205\351\232\234:%s",
                     cell_max, cell_min, temp_max, temp_min, fault);
        } else {
            /* Fallback: show raw line truncated if parsing fails */
            snprintf(parsed[0], sizeof(parsed[0]), "%.60s", csv_line);
            parsed[1][0] = '\0';
            parsed[2][0] = '\0';
        }

        /* ---- Shift history: keep last 2 records (each = 3 lines) ---- */
        static char csv_hist[1024] = "";
        char new_block[256];
        snprintf(new_block, sizeof(new_block),
                 "%s\n%s\n%s",
                 parsed[0],
                 parsed[1][0] ? parsed[1] : "",
                 parsed[2][0] ? parsed[2] : "");

        /* Find the 2nd record's start in history (keep last 3 lines) */
        char tmp[1024];
        strncpy(tmp, csv_hist, sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = '\0';

        int nl_count = 0;
        for (char *p = tmp; *p; p++) {
            if (*p == '\n') nl_count++;
        }
        char *cut = tmp;
        if (nl_count >= 3) {
            int skip = nl_count - 3;
            while (skip > 0 && *cut) {
                if (*cut == '\n') skip--;
                cut++;
            }
            if (*cut == '\n') cut++;
        }

        snprintf(csv_hist, sizeof(csv_hist),
                 "\346\234\200\346\226\260CSV\350\256\260\345\275\225(\345\267\262\345\220\214\346\255\245):\n\n"
                 "%s\n%s",
                 cut, new_block);

        lv_label_set_text_static(l_csv_storage, csv_hist);
    }
}

/* =========================================================================
 * 页面切换逻辑，切换前将标签指针置为 NULL
 * ========================================================================= */
static void nav_btn_cb(lv_event_t * e)
{
    uint32_t page_idx = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
    if (page_idx == g_ui.current_page) return;
    switch_page(page_idx);
}

static void switch_page(uint8_t page_idx)
{
    g_ui.current_page = page_idx;

    /* 离开 BMS 页面时重置 CAN 接收标志，确保切回时显示 "Waiting" */
    if (page_idx != 0) {
        extern uint8_t g_has_last_can_rx;
        g_has_last_can_rx = 0;
    }

    // 更新底部导航按钮样式
    for (int i = 0; i < 4; i++) {
        if (i == page_idx) {
            lv_obj_set_style_bg_color(g_ui.nav_btns[i], lv_color_hex(COLOR_PRIMARY), LV_PART_MAIN);
            lv_obj_set_style_text_color(lv_obj_get_child(g_ui.nav_btns[i], 0), lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        } else {
            lv_obj_set_style_bg_color(g_ui.nav_btns[i], lv_color_hex(COLOR_CARD), LV_PART_MAIN);
            lv_obj_set_style_text_color(lv_obj_get_child(g_ui.nav_btns[i], 0), lv_color_hex(COLOR_TEXT_MUTED), LV_PART_MAIN);
        }
    }

    /* 只做显示/隐藏，不删除对象 —— 全局标签指针生命周期稳定 */
    for (int i = 0; i < 4; i++) {
        if (i == page_idx) {
            lv_obj_clear_flag(page_containers[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(page_containers[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    // 更新标题
    switch (page_idx) {
        case 0: lv_label_set_text(g_ui.label_title, "BMS \346\246\202\350\247\210"); break;
        case 1: lv_label_set_text(g_ui.label_title, "\345\215\225\344\275\223\344\270\216\346\225\205\351\232\234"); break;
        case 2: lv_label_set_text(g_ui.label_title, "RS485 \346\225\260\346\215\256"); break;
        case 3: lv_label_set_text(g_ui.label_title, "SD\345\215\241\345\255\230\345\202\250"); break;
    }
}

/* =========================================================================
 * UI 初始化入口
 * ========================================================================= */
void create_pretty_dashboard_ui(void)
{
    int i;
    lv_obj_t * bottom_bar;
    const char * nav_titles[4] = {"\346\246\202\350\247\210", "\346\225\205\351\232\234", "485", "\345\255\230\345\202\250"};
    const char * nav_icons[4]  = {LV_SYMBOL_HOME, LV_SYMBOL_WARNING, LV_SYMBOL_REFRESH, LV_SYMBOL_SD_CARD};

    lv_obj_t * scr = lv_scr_act();
    lv_obj_clean(scr);

    lv_obj_set_style_bg_color(scr, lv_color_hex(COLOR_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* 1. 顶部状态栏 (高度 40px) */
    g_ui.top_bar = lv_obj_create(scr);
    lv_obj_set_size(g_ui.top_bar, 480, 40);
    lv_obj_align(g_ui.top_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(g_ui.top_bar, lv_color_hex(COLOR_CARD), LV_PART_MAIN);
    lv_obj_set_style_border_color(g_ui.top_bar, lv_color_hex(COLOR_BORDER), LV_PART_MAIN);
    lv_obj_set_style_border_width(g_ui.top_bar, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(g_ui.top_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(g_ui.top_bar, 15, LV_PART_MAIN);
    lv_obj_clear_flag(g_ui.top_bar, LV_OBJ_FLAG_SCROLLABLE);

    g_ui.label_time = lv_label_create(g_ui.top_bar);
    lv_label_set_text(g_ui.label_time, "14:35");
    lv_obj_set_style_text_font(g_ui.label_time, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(g_ui.label_time, lv_color_hex(COLOR_TEXT_DARK), LV_PART_MAIN);
    lv_obj_align(g_ui.label_time, LV_ALIGN_LEFT_MID, 0, 0);

    g_ui.label_title = lv_label_create(g_ui.top_bar);
    lv_label_set_text(g_ui.label_title, "BMS \346\246\202\350\247\210");
    lv_obj_set_style_text_font(g_ui.label_title, &lv_font_simsun_14_cjk, LV_PART_MAIN);
    lv_obj_set_style_text_color(g_ui.label_title, lv_color_hex(COLOR_PRIMARY), LV_PART_MAIN);
    lv_obj_center(g_ui.label_title);

    /* 使用全局指针：由 UI_RefreshAlarmBar() 动态更新报警文字 */
    system_alarm_label = lv_label_create(g_ui.top_bar);
    lv_label_set_text(system_alarm_label, LV_SYMBOL_WIFI " \346\255\243\345\270\270");
    lv_obj_set_style_text_font(system_alarm_label, &lv_font_simsun_14_cjk, LV_PART_MAIN);
    lv_obj_set_style_text_color(system_alarm_label, lv_color_hex(COLOR_GREEN), LV_PART_MAIN);
    lv_obj_align(system_alarm_label, LV_ALIGN_RIGHT_MID, 0, 0);

    /* 2. 动态内容区域 (高度 670px) */
    g_ui.content_area = lv_obj_create(scr);
    lv_obj_set_size(g_ui.content_area, 480, 670);
    lv_obj_align(g_ui.content_area, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_set_style_bg_opa(g_ui.content_area, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_ui.content_area, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(g_ui.content_area, 0, LV_PART_MAIN);
    lv_obj_clear_flag(g_ui.content_area, LV_OBJ_FLAG_SCROLLABLE);

    /* 预创建 4 个页面容器 (都在 content_area 内，默认全部隐藏，切换时只做显示/隐藏) */
    for (i = 0; i < 4; i++) {
        page_containers[i] = lv_obj_create(g_ui.content_area);
        lv_obj_set_size(page_containers[i], 480, 670);
        lv_obj_set_style_bg_opa(page_containers[i], LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(page_containers[i], 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(page_containers[i], 0, LV_PART_MAIN);
        lv_obj_add_flag(page_containers[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(page_containers[i], LV_OBJ_FLAG_SCROLLABLE);
    }

    /* 加载各页面到各自的容器（只执行一次，全局标签指针生命周期稳定） */
    load_page_bms_main(page_containers[0]);
    load_page_bms_fault(page_containers[1]);
    load_page_modbus(page_containers[2]);
    load_page_storage(page_containers[3]);

    /* 3. 底部导航栏 (高度 60px) */
    bottom_bar = lv_obj_create(scr);
    lv_obj_set_size(bottom_bar, 480, 60);
    lv_obj_align(bottom_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(bottom_bar, lv_color_hex(COLOR_CARD), LV_PART_MAIN);
    lv_obj_set_style_border_color(bottom_bar, lv_color_hex(COLOR_BORDER), LV_PART_MAIN);
    lv_obj_set_style_border_width(bottom_bar, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(bottom_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(bottom_bar, 5, LV_PART_MAIN);
    lv_obj_clear_flag(bottom_bar, LV_OBJ_FLAG_SCROLLABLE);

    for (i = 0; i < 4; i++) {
        lv_obj_t * btn_lbl;
        g_ui.nav_btns[i] = lv_btn_create(bottom_bar);
        lv_obj_set_size(g_ui.nav_btns[i], 110, 48);
        lv_obj_align(g_ui.nav_btns[i], LV_ALIGN_LEFT_MID, i * 118 + 4, 0);
        lv_obj_set_style_radius(g_ui.nav_btns[i], 6, LV_PART_MAIN);

        btn_lbl = lv_label_create(g_ui.nav_btns[i]);
        lv_label_set_text_fmt(btn_lbl, "%s %s", nav_icons[i], nav_titles[i]);
        lv_obj_set_style_text_font(btn_lbl, &lv_font_simsun_14_cjk, LV_PART_MAIN);
        lv_obj_center(btn_lbl);

        lv_obj_add_event_cb(g_ui.nav_btns[i], nav_btn_cb, LV_EVENT_PRESSED, (void *)(uintptr_t)i);
    }

    /* 4. 默认显示 BMS Overview 页面 */
    switch_page(0);
    /* 注意: 不再注册全屏触摸事件，避免误触发 CAN 发送 */

    /* 暴露时间标签给 freertos.c 中 Sys_Task 更新时间 */
    time_label = g_ui.label_time;
}