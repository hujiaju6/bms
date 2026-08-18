cmake_minimum_required(VERSION 3.20)

# ============================================================
# 交叉编译强制设置 (必须在 project() 之前)
# 防止忘记指定 toolchain file 时 CMake 误识别为原生编译
# 检测到非交叉编译配置时自动纠正，确保始终针对 ARM 裸机构建
# 注意: 检查 CACHE 变量, 因为 toolchain 设置的 SYSTEM_NAME 在 project() 前仅在缓存中
# ============================================================
if("$CACHE{CMAKE_SYSTEM_NAME}" STREQUAL "Generic")
    message(STATUS "Cross-compilation: Generic (ARM Cortex-M4)")
else()
    if(DEFINED CACHE{CMAKE_SYSTEM_NAME} AND NOT "$CACHE{CMAKE_SYSTEM_NAME}" STREQUAL "")
        message(WARNING "CMAKE_SYSTEM_NAME='$CACHE{CMAKE_SYSTEM_NAME}' is not bare-metal. Forcing Generic (ARM Cortex-M4).")
    endif()
    set(CMAKE_SYSTEM_NAME Generic CACHE STRING "" FORCE)
    set(CMAKE_SYSTEM_PROCESSOR arm CACHE STRING "" FORCE)
    set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY CACHE STRING "" FORCE)
    # 清除可能残留的原生链接库标志 (Windows SDK 库等)
    set(CMAKE_C_STANDARD_LIBRARIES "" CACHE STRING "" FORCE)
    set(CMAKE_CXX_STANDARD_LIBRARIES "" CACHE STRING "" FORCE)
    set(CMAKE_EXE_LINKER_FLAGS_INIT "" CACHE STRING "" FORCE)
    set(CMAKE_SHARED_LINKER_FLAGS_INIT "" CACHE STRING "" FORCE)
    set(CMAKE_MODULE_LINKER_FLAGS_INIT "" CACHE STRING "" FORCE)
endif()

project(my_f407rtos LANGUAGES C ASM)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# 架构参数 (Architecture Flags)
# STM32F407ZGTx: Cortex-M4, FPUv2 (fpv4-sp-d16), hard-float ABI
set(CPU_FLAGS
    -mcpu=cortex-m4
    -mthumb
    -mfloat-abi=hard
    -mfpu=fpv4-sp-d16
)

# 编译宏定义 (Compile Definitions)
set(PROJECT_DEFINES
    USE_HAL_DRIVER
    STM32F407xx
)

# 头文件路径 (Include Directories)
set(PROJECT_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Inc/Legacy
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32F4xx/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/include
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/LVGL/GUI
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/LVGL/GUI/lvgl
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/LVGL/GUI/lvgl/src
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/LVGL/GUI/lvgl/examples/porting
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/BSP_Driver
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/BSP_Driver/lcd
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/BSP_Driver/touch
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/BSP_Driver/flash
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/BSP_Driver/font
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/BSP_Driver/485
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/BSP_Driver/can
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/BSP_Driver/bms
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/BSP_Driver/rtc
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/BSP_Driver/dma
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/BSP_Driver/usart
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/BSP_Driver/sdio
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/FatFs
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/SYSTEM
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/SYSTEM/delay
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/SYSTEM/sys
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/SYSTEM/usart
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/LVGL/GUI_APP
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/LVGL/GUI_APP/GUI_FONT
    ${CMAKE_CURRENT_SOURCE_DIR}/MDK-ARM
)

# 启动文件 (GCC语法) / Startup
set(STARTUP_SOURCES
    Drivers/CMSIS/Device/ST/STM32F4xx/Source/Templates/gcc/startup_stm32f407xx.s
)

# 应用层 / 用户代码
set(APP_SOURCES
    Core/Src/main.c
    Core/Src/gpio.c
    Core/Src/freertos.c
    Core/Src/stm32f4xx_it.c
    Core/Src/stm32f4xx_hal_msp.c
    Core/Src/stm32f4xx_hal_timebase_tim.c
    MDK-ARM/MY_UI.c
    MDK-ARM/bsp_dma.c
)

# CMSIS 系统文件
set(CMSIS_SOURCES
    Core/Src/system_stm32f4xx.c
)

# STM32F4xx HAL 驱动库
set(HAL_SOURCES
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_fsmc.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_sdram.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_sram.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc_ex.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ex.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ramfunc.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma_ex.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr_ex.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cortex.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_exti.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim_ex.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_usart.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_i2c.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_i2c_ex.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_usart.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_uart.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_spi.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_can.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rtc.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rtc_ex.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_sd.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_sdmmc.c
)

# FreeRTOS 源文件
# GCC port (arm-none-eabi-gcc) replaces Keil RVDS port
set(FREERTOS_SOURCES
    Middlewares/Third_Party/FreeRTOS/Source/croutine.c
    Middlewares/Third_Party/FreeRTOS/Source/event_groups.c
    Middlewares/Third_Party/FreeRTOS/Source/list.c
    Middlewares/Third_Party/FreeRTOS/Source/queue.c
    Middlewares/Third_Party/FreeRTOS/Source/stream_buffer.c
    Middlewares/Third_Party/FreeRTOS/Source/tasks.c
    Middlewares/Third_Party/FreeRTOS/Source/timers.c
    Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F/port.c
    Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/heap_4.c
    Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2/cmsis_os2.c
)

# BSP Drivers
set(BSP_SOURCES
    Drivers/BSP_Driver/AT.c
    Drivers/BSP_Driver/bsp_debug_usart.c
    Drivers/BSP_Driver/led.c
    Drivers/BSP_Driver/bms/bms_simulator.c
    Drivers/BSP_Driver/485/bsp_485.c
    Drivers/BSP_Driver/can/bsp_can.c
    Drivers/BSP_Driver/lcd/bsp_nt35510_lcd.c
    Drivers/BSP_Driver/touch/bsp_i2c_touch.c
    Drivers/BSP_Driver/touch/gt5xx.c
    Drivers/BSP_Driver/flash/bsp_spi_flash.c
    Drivers/BSP_Driver/rtc/bsp_rtc.c
    Drivers/BSP_Driver/font/fonts.c
    Drivers/BSP_Driver/sdio/bsp_sdio.c
)

# FatFs R0.16 — File System
set(FATFS_SOURCES
    Middlewares/Third_Party/FatFs/ff.c
    Middlewares/Third_Party/FatFs/ffunicode.c
    Middlewares/Third_Party/FatFs/diskio.c
    Middlewares/Third_Party/FatFs/ffsystem.c
)

# LVGL 核心库
set(LVGL_CORE_SOURCES
    Middlewares/LVGL/GUI/lvgl/src/core/lv_disp.c
    Middlewares/LVGL/GUI/lvgl/src/core/lv_event.c
    Middlewares/LVGL/GUI/lvgl/src/core/lv_group.c
    Middlewares/LVGL/GUI/lvgl/src/core/lv_indev.c
    Middlewares/LVGL/GUI/lvgl/src/core/lv_indev_scroll.c
    Middlewares/LVGL/GUI/lvgl/src/core/lv_obj.c
    Middlewares/LVGL/GUI/lvgl/src/core/lv_obj_class.c
    Middlewares/LVGL/GUI/lvgl/src/core/lv_obj_draw.c
    Middlewares/LVGL/GUI/lvgl/src/core/lv_obj_pos.c
    Middlewares/LVGL/GUI/lvgl/src/core/lv_obj_scroll.c
    Middlewares/LVGL/GUI/lvgl/src/core/lv_obj_style.c
    Middlewares/LVGL/GUI/lvgl/src/core/lv_obj_style_gen.c
    Middlewares/LVGL/GUI/lvgl/src/core/lv_obj_tree.c
    Middlewares/LVGL/GUI/lvgl/src/core/lv_refr.c
    Middlewares/LVGL/GUI/lvgl/src/core/lv_theme.c
)

set(LVGL_DRAW_SOURCES
    Middlewares/LVGL/GUI/lvgl/src/draw/lv_draw.c
    Middlewares/LVGL/GUI/lvgl/src/draw/lv_draw_arc.c
    Middlewares/LVGL/GUI/lvgl/src/draw/lv_draw_img.c
    Middlewares/LVGL/GUI/lvgl/src/draw/lv_draw_label.c
    Middlewares/LVGL/GUI/lvgl/src/draw/lv_draw_line.c
    Middlewares/LVGL/GUI/lvgl/src/draw/lv_draw_mask.c
    Middlewares/LVGL/GUI/lvgl/src/draw/lv_draw_rect.c
    Middlewares/LVGL/GUI/lvgl/src/draw/lv_draw_triangle.c
    Middlewares/LVGL/GUI/lvgl/src/draw/lv_img_buf.c
    Middlewares/LVGL/GUI/lvgl/src/draw/lv_img_cache.c
    Middlewares/LVGL/GUI/lvgl/src/draw/lv_img_decoder.c
)

set(LVGL_EXTRA_SOURCES
    Middlewares/LVGL/GUI/lvgl/src/extra/lv_extra.c
    Middlewares/LVGL/GUI/lvgl/src/extra/layouts/flex/lv_flex.c
    Middlewares/LVGL/GUI/lvgl/src/extra/layouts/grid/lv_grid.c
    Middlewares/LVGL/GUI/lvgl/src/extra/themes/basic/lv_theme_basic.c
    Middlewares/LVGL/GUI/lvgl/src/extra/themes/default/lv_theme_default.c
    Middlewares/LVGL/GUI/lvgl/src/extra/themes/mono/lv_theme_mono.c
)

# LVGL Extra Widgets (calendar, keyboard, list, menu, msgbox, spinbox, tabview, tileview, win, etc.)
set(LVGL_EXTRA_WIDGET_SOURCES
    Middlewares/LVGL/GUI/lvgl/src/extra/widgets/animimg/lv_animimg.c
    Middlewares/LVGL/GUI/lvgl/src/extra/widgets/calendar/lv_calendar.c
    Middlewares/LVGL/GUI/lvgl/src/extra/widgets/calendar/lv_calendar_header_arrow.c
    Middlewares/LVGL/GUI/lvgl/src/extra/widgets/calendar/lv_calendar_header_dropdown.c
    Middlewares/LVGL/GUI/lvgl/src/extra/widgets/chart/lv_chart.c
    Middlewares/LVGL/GUI/lvgl/src/extra/widgets/colorwheel/lv_colorwheel.c
    Middlewares/LVGL/GUI/lvgl/src/extra/widgets/imgbtn/lv_imgbtn.c
    Middlewares/LVGL/GUI/lvgl/src/extra/widgets/keyboard/lv_keyboard.c
    Middlewares/LVGL/GUI/lvgl/src/extra/widgets/led/lv_led.c
    Middlewares/LVGL/GUI/lvgl/src/extra/widgets/list/lv_list.c
    Middlewares/LVGL/GUI/lvgl/src/extra/widgets/menu/lv_menu.c
    Middlewares/LVGL/GUI/lvgl/src/extra/widgets/meter/lv_meter.c
    Middlewares/LVGL/GUI/lvgl/src/extra/widgets/msgbox/lv_msgbox.c
    Middlewares/LVGL/GUI/lvgl/src/extra/widgets/span/lv_span.c
    Middlewares/LVGL/GUI/lvgl/src/extra/widgets/spinbox/lv_spinbox.c
    Middlewares/LVGL/GUI/lvgl/src/extra/widgets/spinner/lv_spinner.c
    Middlewares/LVGL/GUI/lvgl/src/extra/widgets/tabview/lv_tabview.c
    Middlewares/LVGL/GUI/lvgl/src/extra/widgets/tileview/lv_tileview.c
    Middlewares/LVGL/GUI/lvgl/src/extra/widgets/win/lv_win.c
)

# LVGL Draw SW (software rendering backend)
set(LVGL_DRAW_SW_SOURCES
    Middlewares/LVGL/GUI/lvgl/src/draw/sw/lv_draw_sw.c
    Middlewares/LVGL/GUI/lvgl/src/draw/sw/lv_draw_sw_arc.c
    Middlewares/LVGL/GUI/lvgl/src/draw/sw/lv_draw_sw_blend.c
    Middlewares/LVGL/GUI/lvgl/src/draw/sw/lv_draw_sw_dither.c
    Middlewares/LVGL/GUI/lvgl/src/draw/sw/lv_draw_sw_gradient.c
    Middlewares/LVGL/GUI/lvgl/src/draw/sw/lv_draw_sw_img.c
    Middlewares/LVGL/GUI/lvgl/src/draw/sw/lv_draw_sw_letter.c
    Middlewares/LVGL/GUI/lvgl/src/draw/sw/lv_draw_sw_line.c
    Middlewares/LVGL/GUI/lvgl/src/draw/sw/lv_draw_sw_polygon.c
    Middlewares/LVGL/GUI/lvgl/src/draw/sw/lv_draw_sw_rect.c
)

set(LVGL_FONT_SOURCES
    Middlewares/LVGL/GUI/lvgl/src/font/lv_font.c
    Middlewares/LVGL/GUI/lvgl/src/font/lv_font_dejavu_16_persian_hebrew.c
    Middlewares/LVGL/GUI/lvgl/src/font/lv_font_fmt_txt.c
    Middlewares/LVGL/GUI/lvgl/src/font/lv_font_loader.c
    Middlewares/LVGL/GUI/lvgl/src/font/lv_font_montserrat_10.c
    Middlewares/LVGL/GUI/lvgl/src/font/lv_font_montserrat_12.c
    Middlewares/LVGL/GUI/lvgl/src/font/lv_font_montserrat_12_subpx.c
    Middlewares/LVGL/GUI/lvgl/src/font/lv_font_montserrat_14.c
    Middlewares/LVGL/GUI/lvgl/src/font/lv_font_montserrat_16.c
    Middlewares/LVGL/GUI/lvgl/src/font/lv_font_montserrat_18.c
    Middlewares/LVGL/GUI/lvgl/src/font/lv_font_montserrat_20.c
    Middlewares/LVGL/GUI/lvgl/src/font/lv_font_montserrat_22.c
    Middlewares/LVGL/GUI/lvgl/src/font/lv_font_montserrat_24.c
    Middlewares/LVGL/GUI/lvgl/src/font/lv_font_montserrat_26.c
    Middlewares/LVGL/GUI/lvgl/src/font/lv_font_montserrat_28.c
    Middlewares/LVGL/GUI/lvgl/src/font/lv_font_montserrat_30.c
    Middlewares/LVGL/GUI/lvgl/src/font/lv_font_montserrat_32.c
    Middlewares/LVGL/GUI/lvgl/src/font/lv_font_montserrat_34.c
    Middlewares/LVGL/GUI/lvgl/src/font/lv_font_montserrat_36.c
    Middlewares/LVGL/GUI/lvgl/src/font/lv_font_montserrat_38.c
    Middlewares/LVGL/GUI/lvgl/src/font/lv_font_montserrat_40.c
    Middlewares/LVGL/GUI/lvgl/src/font/lv_font_montserrat_42.c
    Middlewares/LVGL/GUI/lvgl/src/font/lv_font_montserrat_44.c
    Middlewares/LVGL/GUI/lvgl/src/font/lv_font_montserrat_46.c
    Middlewares/LVGL/GUI/lvgl/src/font/lv_font_montserrat_48.c
    Middlewares/LVGL/GUI/lvgl/src/font/lv_font_montserrat_8.c
    Middlewares/LVGL/GUI/lvgl/src/font/lv_font_simsun_14_cjk.c
    Middlewares/LVGL/GUI/lvgl/src/font/lv_font_unscii_16.c
    Middlewares/LVGL/GUI/lvgl/src/font/lv_font_unscii_8.c
)

set(LVGL_SDL_SOURCES
    Middlewares/LVGL/GUI/lvgl/src/hal/lv_hal_disp.c
    Middlewares/LVGL/GUI/lvgl/src/hal/lv_hal_indev.c
    Middlewares/LVGL/GUI/lvgl/src/hal/lv_hal_tick.c
)

set(LVGL_HAL_SOURCES
    # HAL stubs (非SDL，实际在STM32裸机中使用)
)

set(LVGL_MISC_SOURCES
    Middlewares/LVGL/GUI/lvgl/src/misc/lv_anim.c
    Middlewares/LVGL/GUI/lvgl/src/misc/lv_anim_timeline.c
    Middlewares/LVGL/GUI/lvgl/src/misc/lv_area.c
    Middlewares/LVGL/GUI/lvgl/src/misc/lv_async.c
    Middlewares/LVGL/GUI/lvgl/src/misc/lv_bidi.c
    Middlewares/LVGL/GUI/lvgl/src/misc/lv_color.c
    Middlewares/LVGL/GUI/lvgl/src/misc/lv_fs.c
    Middlewares/LVGL/GUI/lvgl/src/misc/lv_gc.c
    Middlewares/LVGL/GUI/lvgl/src/misc/lv_ll.c
    Middlewares/LVGL/GUI/lvgl/src/misc/lv_log.c
    Middlewares/LVGL/GUI/lvgl/src/misc/lv_lru.c
    Middlewares/LVGL/GUI/lvgl/src/misc/lv_math.c
    Middlewares/LVGL/GUI/lvgl/src/misc/lv_mem.c
    Middlewares/LVGL/GUI/lvgl/src/misc/lv_printf.c
    Middlewares/LVGL/GUI/lvgl/src/misc/lv_style.c
    Middlewares/LVGL/GUI/lvgl/src/misc/lv_style_gen.c
    Middlewares/LVGL/GUI/lvgl/src/misc/lv_templ.c
    Middlewares/LVGL/GUI/lvgl/src/misc/lv_timer.c
    Middlewares/LVGL/GUI/lvgl/src/misc/lv_tlsf.c
    Middlewares/LVGL/GUI/lvgl/src/misc/lv_txt.c
    Middlewares/LVGL/GUI/lvgl/src/misc/lv_txt_ap.c
    Middlewares/LVGL/GUI/lvgl/src/misc/lv_utils.c
)

set(LVGL_WIDGET_SOURCES
    Middlewares/LVGL/GUI/lvgl/src/widgets/lv_arc.c
    Middlewares/LVGL/GUI/lvgl/src/widgets/lv_bar.c
    Middlewares/LVGL/GUI/lvgl/src/widgets/lv_btn.c
    Middlewares/LVGL/GUI/lvgl/src/widgets/lv_btnmatrix.c
    Middlewares/LVGL/GUI/lvgl/src/widgets/lv_canvas.c
    Middlewares/LVGL/GUI/lvgl/src/widgets/lv_checkbox.c
    Middlewares/LVGL/GUI/lvgl/src/widgets/lv_dropdown.c
    Middlewares/LVGL/GUI/lvgl/src/widgets/lv_img.c
    Middlewares/LVGL/GUI/lvgl/src/widgets/lv_label.c
    Middlewares/LVGL/GUI/lvgl/src/widgets/lv_line.c
    Middlewares/LVGL/GUI/lvgl/src/widgets/lv_objx_templ.c
    Middlewares/LVGL/GUI/lvgl/src/widgets/lv_roller.c
    Middlewares/LVGL/GUI/lvgl/src/widgets/lv_slider.c
    Middlewares/LVGL/GUI/lvgl/src/widgets/lv_switch.c
    Middlewares/LVGL/GUI/lvgl/src/widgets/lv_table.c
    Middlewares/LVGL/GUI/lvgl/src/widgets/lv_textarea.c
)

set(LVGL_PORTING_SOURCES
    Middlewares/LVGL/GUI/lvgl/examples/porting/lv_port_disp.c
    Middlewares/LVGL/GUI/lvgl/examples/porting/lv_port_fs.c
    Middlewares/LVGL/GUI/lvgl/examples/porting/lv_port_indev.c
)

# 系统驱动 / SYSTEM
set(SYSTEM_SOURCES
    Drivers/SYSTEM/delay/delay.c
    Drivers/SYSTEM/sys/sys.c
)

# LVGL GUI 应用层
set(GUI_APP_SOURCES
    Middlewares/LVGL/GUI_APP/lv_mainstart.c
    Middlewares/LVGL/GUI_APP/GUI_FONT/myFont14.c
)

# 汇总所有源文件
set(ALL_SOURCES
    ${STARTUP_SOURCES}
    ${APP_SOURCES}
    ${CMSIS_SOURCES}
    ${HAL_SOURCES}
    ${FREERTOS_SOURCES}
    ${BSP_SOURCES}
    ${LVGL_CORE_SOURCES}
    ${LVGL_DRAW_SOURCES}
    ${LVGL_EXTRA_SOURCES}
    ${LVGL_FONT_SOURCES}
    ${LVGL_SDL_SOURCES}
    ${LVGL_HAL_SOURCES}
    ${LVGL_MISC_SOURCES}
    ${LVGL_WIDGET_SOURCES}
    ${LVGL_PORTING_SOURCES}
    ${SYSTEM_SOURCES}
    ${LVGL_EXTRA_WIDGET_SOURCES}
    ${LVGL_DRAW_SW_SOURCES}
    ${GUI_APP_SOURCES}
    ${FATFS_SOURCES}
)

# 可执行目标
add_executable(${PROJECT_NAME}.elf ${ALL_SOURCES})

set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/my_f407rtos.ld")
set_property(TARGET ${PROJECT_NAME}.elf PROPERTY LINK_DEPENDS ${LINKER_SCRIPT})

target_include_directories(${PROJECT_NAME}.elf PRIVATE
    ${PROJECT_INCLUDES}
)

target_compile_definitions(${PROJECT_NAME}.elf PRIVATE
    ${PROJECT_DEFINES}
)

# GCC 编译选项
# Keil ARMCC optimization L4 → GCC -Os (optimize for size)
# MicroLIB → newlib-nano
target_compile_options(${PROJECT_NAME}.elf PRIVATE
    ${CPU_FLAGS}
    $<$<COMPILE_LANGUAGE:C>:-Os>
    $<$<COMPILE_LANGUAGE:C>:-ffunction-sections>
    $<$<COMPILE_LANGUAGE:C>:-fdata-sections>
    $<$<COMPILE_LANGUAGE:C>:-Wall>
    $<$<COMPILE_LANGUAGE:C>:-Wextra>
    $<$<COMPILE_LANGUAGE:ASM>:-x>
    $<$<COMPILE_LANGUAGE:ASM>:assembler-with-cpp>
)

# 链接选项
target_link_options(${PROJECT_NAME}.elf PRIVATE
    ${CPU_FLAGS}
    -T${LINKER_SCRIPT}
    -Wl,-Map=${PROJECT_NAME}.map
    -Wl,--gc-sections
    -Wl,--print-memory-usage
    --specs=nano.specs
    --specs=nosys.specs
    -u _printf_float
)

# Post-Build: Generate .bin, .hex, size report using external script to avoid path encoding issues
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND powershell -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/post_build.ps1" -buildDir "${CMAKE_CURRENT_BINARY_DIR}" -elfName "${PROJECT_NAME}.elf"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Post-build: generating .bin/.hex/size report"
    VERBATIM
)

target_link_libraries(${PROJECT_NAME}.elf m)
