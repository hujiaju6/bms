@echo off
chcp 65001 >nul
echo ==========================================
echo   STM32F407 固件烧录 (ST-Link)
echo ==========================================
echo.

if not exist "%~dp0build_final\my_f407rtos.elf" (
    echo [错误] 未找到 build_final\my_f407rtos.elf，请先编译！
    pause
    exit /b 1
)

echo [1/2] 连接 ST-Link 并烧录固件...
"G:\stm32cubeprogrammer\bin\STM32_Programmer_CLI.exe" -c port=SWD freq=4000 -d "%~dp0build_final\my_f407rtos.elf" -hardRst -s
if errorlevel 1 (
    echo.
    echo [错误] 烧录失败！请检查 ST-Link 连接。
    pause
    exit /b 1
)

echo.
echo [2/2] 烧录完成！固件已写入，芯片已复位运行。
echo.
pause
