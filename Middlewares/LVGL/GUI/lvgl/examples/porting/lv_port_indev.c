#include "lv_port_indev.h"
#include "lvgl.h"
#include "gt5xx.h"
#include "string.h"
#include <stdio.h> /* 补上这一行，防止 printf 报错 */
/* 引用我们在 gt5xx.c 里面新增的全局变量 */
extern uint16_t lv_touch_x;
extern uint16_t lv_touch_y;
extern uint8_t lv_touch_state;

lv_indev_t * indev_touchpad;

/* LVGL 8.x 触摸状态读取回调 */
static void touchpad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
{
    /* 使用静态变量保存上一次的有效坐标 */
    static int16_t last_x = 0;
    static int16_t last_y = 0;

    /* 手动触发一次触摸芯片数据读取 */
    GTP_TouchProcess(); 

    if(lv_touch_state == 1) {
        data->state = LV_INDEV_STATE_PR;
        
        /* 核心转换：将 GT911 的 (800x480) 转换到 LVGL 竖屏 (480x800) */
        last_x = lv_touch_y;               /* GT911 的 Y 轴 (0~480) 对应 LVGL 的 X 轴 */
        last_y = 800 - lv_touch_x;         /* GT911 的 X 轴 (0~800) 对应 LVGL 的 Y 轴 (方向反转) */

        /* 恢复串口打印，方便观察坐标 */
        printf("Mapped Touch -> X:%d, Y:%d (Press)\r\n", last_x, last_y);
    } else {
        data->state = LV_INDEV_STATE_REL;
        /* 松开时，保持最后一次的有效坐标 */
    }

    /* 无论按下还是松开，必须将坐标实时赋给 LVGL */
    data->point.x = last_x;
    data->point.y = last_y;
}
void lv_port_indev_init(void)
{
    static lv_indev_drv_t indev_drv;

    /* 初始化底层触摸芯片 */
    GTP_Init_Panel();

    /* 注册 LVGL 8.x 输入设备 */
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read;
    indev_touchpad = lv_indev_drv_register(&indev_drv);
}