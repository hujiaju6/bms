# CMake 构建说明 / CMake Build Guide

## 状态 / Status

- 中文：目标 `my_f407rtos`，状态为 **已生成、未验证**
  - arm-none-eabi-gcc 工具链未安装，FreeRTOS GCC 移植文件已生成但未经编译验证
- English: Target `my_f407rtos`; status: **generated, unverified**
  - arm-none-eabi-gcc toolchain is not installed; FreeRTOS GCC port files have been generated but not compiled

## 环境检查 / Environment Check

中文：在项目根目录执行以下命令，确认必需工具可用：

English: Run this command from the project root to confirm required tools are available:

```text
python check_build_env.py
```

必需工具 / Required tools:
- CMake >= 3.20
- Ninja
- arm-none-eabi-gcc (GNU Arm Embedded Toolchain)

## 编译 / Build

中文：在项目根目录依次执行：

English: Run these commands from the project root:

```text
cmake -S . -B build -G Ninja "-DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi-gcc.cmake"
cmake --build build
```

## 编译成品 / Build Outputs

| 文件 / File | 简介 / Description |
| --- | --- |
| `build/my_f407rtos.elf` | 可调试的固件及符号 / Debuggable firmware and symbols |
| `build/my_f407rtos.bin` | 原始烧录镜像 / Raw programming image |
| `build/my_f407rtos.hex` | Intel HEX 烧录镜像 / Intel HEX programming image |
| `build/my_f407rtos.map` | 链接映射与内存布局 / Link map and memory layout |

## 转换说明 / Migration Notes

中文：
- 基于 Keil MDK-ARM V5.25, ARMCC V5.06 工程转换
- MCU: STM32F407ZGTx (Cortex-M4, FPUv2, 1MB Flash, 128KB RAM)
- 启动文件：从 ARMCC `startup_stm32f407xx.s` 切换为 GCC 版本 (`Drivers/CMSIS/Device/ST/STM32F4xx/Source/Templates/gcc/`)
- FreeRTOS 移植：从 RVDS/ARM_CM4F 切换为 GCC/ARM_CM4F（移植文件已生成于 `Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F/`）
- MicroLIB 替代：使用 newlib-nano (`--specs=nano.specs`) + nosys
- 优化等级：Keil L4 → GCC -Os (尺寸优化)
- 浮点打印：启用 `-u _printf_float` 以支持浮点数格式化
- 链接脚本：从 scatter file 转换为 GNU ld script (`my_f407rtos.ld`)
- Keil 工程文件保留不变，可继续在 MDK 中编译

English:
- Converted from Keil MDK-ARM V5.25, ARMCC V5.06 project
- MCU: STM32F407ZGTx (Cortex-M4, FPUv2, 1MB Flash, 128KB RAM)
- Startup: switched from ARMCC `startup_stm32f407xx.s` to GCC version (`Drivers/CMSIS/Device/ST/STM32F4xx/Source/Templates/gcc/`)
- FreeRTOS port: switched from RVDS/ARM_CM4F to GCC/ARM_CM4F (port files generated in `Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F/`)
- MicroLIB replacement: newlib-nano (`--specs=nano.specs`) + nosys
- Optimization: Keil L4 → GCC -Os (optimize for size)
- Float printf: enabled `-u _printf_float` for float formatting support
- Linker script: converted from scatter file to GNU ld script (`my_f407rtos.ld`)
- Keil project files are preserved; the project can still be built in MDK

## 验证命令（待执行）/ Verification Commands (to be executed)

中文：安装 arm-none-eabi-gcc 工具链后，执行以下验证：

```text
# 1. 环境检查
python check_build_env.py

# 2. 配置并编译
cmake -S . -B build -G Ninja "-DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi-gcc.cmake"
cmake --build build

# 3. 查看固件信息
arm-none-eabi-size --format=berkeley build/my_f407rtos.elf

# 4. 与 Keil 构建对比验证（需要 Python）
python C:\Users\胡嘉驹\.codex\skills\convert-keil-to-cmake\scripts\validate_conversion.py^
  MDK-ARM\my_f407rtos.uvprojx^
  build\compile_commands.json^
  --elf build\my_f407rtos.elf^
  --source-substitution MDK-ARM\startup_stm32f407xx.s=Drivers/CMSIS/Device/ST/STM32F4xx/Source/Templates/gcc/startup_stm32f407xx.s^
  --source-substitution Middlewares/Third_Party/FreeRTOS/Source/portable/RVDS/ARM_CM4F/port.c=Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F/port.c
```

English: After installing the arm-none-eabi-gcc toolchain, run the following verification:

```text
# 1. Environment check
python check_build_env.py

# 2. Configure and build
cmake -S . -B build -G Ninja "-DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi-gcc.cmake"
cmake --build build

# 3. Inspect firmware size
arm-none-eabi-size --format=berkeley build/my_f407rtos.elf

# 4. Compare with Keil build (requires Python)
python C:\Users\胡嘉驹\.codex\skills\convert-keil-to-cmake\scripts\validate_conversion.py^
  MDK-ARM\my_f407rtos.uvprojx^
  build\compile_commands.json^
  --elf build\my_f407rtos.elf^
  --source-substitution MDK-ARM\startup_stm32f407xx.s=Drivers/CMSIS/Device/ST/STM32F4xx/Source/Templates/gcc/startup_stm32f407xx.s^
  --source-substitution Middlewares/Third_Party/FreeRTOS/Source/portable/RVDS/ARM_CM4F/port.c=Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F/port.c
```

## Keil 原始构建统计 / Keil Original Build Statistics

| 项目 / Item | 大小 / Size |
| --- | --- |
| Code | 176,644 bytes |
| RO-data | 71,884 bytes |
| RW-data | 792 bytes |
| ZI-data | 126,336 bytes |
| Flash 总计 / Flash total | 249,320 bytes |
| RAM 总计 / RAM total | 127,128 bytes |
