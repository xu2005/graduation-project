# 基于 LabVIEW 和 STM32 单片机的室内障碍物二维扫描检测与可视化系统

毕业设计项目仓库。

以 STM32F103C8T6 为主控：SG90 舵机带动 HC-SR04 超声波模块做单轴角度往复扫描，每个有效扫描点输出同一时刻的 `(角度°, 距离cm)`；DHT11 采集环境温湿度用于声速温度补偿；OLED 显示运行状态；USART1 与 LabVIEW 上位机通信，由上位机转换为极坐标 / 二维 XY 点图并保存 CSV。

> 现阶段仓库内容以**模块级验证工程**和毕设资料为主。最终整机工程的引脚分配与模块整合方案见 [docs/Graduation_Project_Integration_Plan.md](docs/Graduation_Project_Integration_Plan.md)（配 [审查报告](docs/Graduation_Project_Integration_Plan_Audit_v2.md)）。

## 目录结构

| 目录 | 说明 |
|---|---|
| `docs/` | 毕业论文、模块整合方案与审查报告、计划实施过程报告 |
| `firmware/` | 5 个 STM32CubeMX + CMake 模块验证工程（见下表） |
| `labview/` | LabVIEW 上位机资料：搭建文档、串口协议、Python 数据模拟器 |
| `drivers/` | 各模块驱动库源码（HC-SR04、DHT11、OLED、编码器舵机等） |
| `hardware/` | BOM、Gerber、原理图、立创 EDA 工程等硬件资料 |

## 固件工程（firmware/）

| 工程 | 验证内容 | 关键接线 |
|---|---|---|
| `light/` | GPIO 输出与延时练习（点灯） | PA0–PA7 |
| `OLED-DHT11/` | DHT11 温湿度读取 + OLED 显示 | PB14 DHT11，PB8/PB9 软件 I2C OLED |
| `HC_SR04_ECHO/` | HC-SR04 输入捕获测距、三点中值滤波、温度补偿 | PA0 TRIG，PA8/TIM1_CH1 ECHO |
| `EncoderServoOLED/` | 舵机 PWM、编码器、OLED 显示 | PA1/TIM2_CH2 舵机，PB0/PB1 编码器 |
| `MPU6050_OLED/` | MPU6050 姿态读取 + OLED 显示 | PB10/PB11 I2C2，PB8/PB9 软件 I2C OLED |

各工程均为独立验证工程，尚未整合为最终整机；目标引脚分配（PB0 TRIG、PA0/TIM2_CH1 ECHO、PA6/TIM3_CH1 舵机、PA9/PA10 USART1、PB12 按键、PB14 DHT11）以整合方案文档为准。`HC_SR04_ECHO/PROJECT_STATE.md` 记录了该工程的详细设计状态。

## 构建与调试

环境要求：STM32CubeMX、CMake、Ninja、GNU Arm Embedded Toolchain、VS Code（Cortex-Debug + pyOCD）。

```bash
cd firmware/<工程名>
cmake --preset Debug
cmake --build build/Debug --parallel
```

各工程 `.vscode/` 已配置 Cortex-Debug + pyOCD（SWD，target `stm32f103c8`），VS Code 中 F5 可直接下载调试。

## LabVIEW 上位机（labview/）

`labview/` 提供 LabVIEW 搭建文档（前面板、程序框图逐模块说明）、串口协议定义与 Python 串口模拟器，可先不接硬件跑通显示流程，详见 `labview/LabVIEW_Ultrasonic_Scanner_Demo/README.md`。

## 说明

- 仓库未包含各工程的 `build/` 编译输出与 `Drivers/CMSIS/Lib` 预编译库（本地构建即可生成）。
- 历史快照压缩包（`*.rar` 等）未纳入版本库。