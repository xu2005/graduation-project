# PROJECT_STATE

> 状态依据：本工程当前 `.ioc`、源码和 VS Code/CMake 配置；最近复核日期：2026-09-25。若本文与工程文件冲突，以工程文件为准。

## 1. 项目概述

- MCU：STM32F103C8T6，HAL，CMake 工程。
- 目的：HC-SR04 单模块周期测距，带输入捕获诊断和三点距离滤波。
- 当前阶段：测距及滤波代码已实现；当前源码可确认实现路径，实际硬件运行/滤波验收未能由工程文件确认（待确认）。

## 2. 硬件配置

| 功能 | 器件/信号 | STM32 引脚 | 工程配置状态 |
|---|---|---|---|
| 测距触发 | HC-SR04 TRIG | PA0 | GPIO 推挽输出，初始低电平 |
| 回波捕获 | HC-SR04 ECHO | PA8 / TIM1_CH1 | 输入捕获输入，无上下拉 |
| 调试 | SWD | PA13/SWDIO、PA14/SWCLK | `.ioc` 配为 Serial Wire |
| 串口、显示、舵机 | USART、OLED、SG90 | — | 当前应用源码未配置；物理连接情况待确认 |

## 3. 时钟配置

- `.ioc`/源码选择 HSE，`HSE_VALUE=8 MHz`；PLL 源 HSE、倍频 ×9。
- SYSCLK/HCLK=72 MHz；APB1=36 MHz（分频 2），APB2=72 MHz。
- TIM1 位于 APB2，计数时钟 72 MHz；PSC=71、ARR=65535，计数频率 1 MHz，即 1 tick=1 μs，16 位周期 65.536 ms。

## 4. CubeMX / 外设配置

- CubeMX 版本 6.12.0，固件包 STM32Cube FW_F1 V1.8.7；MCU 的 HSE 引脚为 PD0/PD1。
- GPIO/TIM 初始化由 `Core/Src/main.c` 中的 `MX_GPIO_Init()`、`MX_TIM1_Init()` 完成；本工程没有独立 `gpio.c` 或 `tim.c`。
- PA8 配为 TIM1_CH1 输入捕获 Direct TI，初始上升沿，捕获分频 DIV1、数字滤波 0；TIM1_CC 中断已启用，由 `stm32f1xx_it.c` 转交 HAL。
- PA13/PA14 保留 SWD。`.ioc` 中 PA0、PA8 标记为锁定引脚。

## 5. HC-SR04 驱动状态

- 驱动位于 `Drivers/HC_SR04/hcsr04.h` 和 `Drivers/HC_SR04/hcsr04.c`，模块说明见 `Drivers/HC_SR04/README.md`。`HCSR04_Init(const HCSR04_Config *)` 复制应用传入的定时器句柄、捕获通道、TRIG 端口和引脚，调用 `HAL_TIM_IC_Start_IT()` 启动捕获并返回 HAL 状态。当前仅支持单个实例；应用须先初始化 GPIO 和定时器，并配置 1 MHz、16 位计数周期。
- `HCSR04_Trigger()` 拉高配置的 TRIG 引脚，使用 DWT `CYCCNT` 忙等 10 μs 后拉低；当前应用在 `main.c` 每 100 ms 触发一次。
- `HAL_TIM_IC_CaptureCallback()` 位于 `main.c` 的 USER CODE 区，转发给 `HCSR04_TIM_IC_CaptureCallback()`。驱动只处理配置的定时器句柄及活动通道：捕获上升沿后切换为下降沿，捕获下降沿后恢复上升沿。
- 脉宽使用 16 位无符号减法计算，可处理一次计数器回绕；有效范围 116–23200 μs，距离=`echo_time_us / 58.0f` cm。
- `HCSR04_Process()` 每轮主循环运行，按触发后 40 ms 超时；超时区分等待上升沿/下降沿，并恢复空闲状态和上升沿捕获。主循环另有 `HAL_Delay(1U)`。

## 6. 滤波算法

- 三个有效距离样本的滚动中值；原始值 `distance_cm`，滤波值 `filtered_distance_cm`。
- 启动时第 1 点直通、第 2 点取两点平均；第 3 点起对最近 3 点取中值。
- 只有有效测量进入滤波。无效/超时不更新滤波输出；`measurement_valid` 单独表示测量有效性。
- 捕获完成时发布一次待处理样本；由主循环 `HCSR04_Process()` 消费并计算，不在中断中排序。`HCSR04_FilterReset()` 清空窗口并将输出设为 -1；累计计数 `hcsr04_filter_update_count` 不由该函数清零。

## 7. Debug / 诊断变量

当前在 `hcsr04.h` 导出的 Watch 变量包括：

```text
hcsr04_trigger_count, hcsr04_echo_rise_count, hcsr04_echo_fall_count
echo_rising, echo_falling, echo_time_us, distance_cm, filtered_distance_cm
measurement_ready, measurement_valid, measurement_active
measurement_count, measurement_error_count, measurement_timeout_count
hcsr04_invalid_count, hcsr04_last_measurement_ms
hcsr04_state, hcsr04_last_result
hcsr04_filter_update_count, hcsr04_filter_sample_count
```

`hcsr04_state`：0 空闲、1 等上升沿、2 等下降沿、3 成功、4 上升沿超时、5 下降沿超时、6 无效。`hcsr04_last_result`：0 无结果、1 有效、2/3 对应两阶段超时、4 无效。捕获值、脉宽和两个距离变量分别用于观察边沿、测量值及滤波值。

## 8. 当前已验证功能

- 源码中存在周期触发、TIM1 输入捕获、脉宽/距离计算、超时恢复、诊断计数和三点滤波实现。2026-09-25 驱动移动到 `Drivers/HC_SR04/` 后，`cmake --build build --parallel 4` 构建通过；硬件参数由应用初始化配置传入。
- 本文不把源码实现等同于硬件验收；传感器实测、连续运行稳定性和滤波效果均待确认。工程内未找到硬件测试记录文件。

## 9. 当前已知问题 / 风险

- 低风险：驱动会清除 TIM1 CC1OF 标志，但没有统计或显式处理 overcapture。
- 低风险：最近一次构建输出有链接器警告 `LOAD segment with RWX permissions`；根因/是否需要调整链接布局待确认。
- 未发现由当前项目文件确认的阻断性问题；硬件行为仍需实测验收。

## 10. 当前明确不使用的功能

当前应用未配置或调用 USART 通信、OLED、SG90、LabVIEW 接口、FreeRTOS、DMA 或 Kalman 滤波。HAL 库文件存在不代表应用使用这些功能。

## 11. 后续计划

工程文件未记录明确的近期计划，待确认；不要将 OLED、SG90、扫描状态机或 LabVIEW 当作已实现功能。

## 12. 关键设计约束

- TIM1_CH1/PA8 当前用于 ECHO；PA0 当前用于 TRIG；PA13/PA14 必须保留 SWD。
- 不要未经确认改动引脚、SYSCLK、TIM1 PSC/ARR 或 CubeMX 配置。
- `HCSR04_Process()` 需要持续获得执行机会；避免长时间阻塞主循环，新功能优先非阻塞实现。
- 不要大规模重写 CubeMX 工程。更改架构或外设后同步更新本文。

## 13. 开发工具链

- 工程使用 STM32CubeMX、VS Code、CMake、GNU Arm Toolchain；`.vscode/launch.json` 配置 Cortex-Debug + pyOCD、SWD、target `stm32f103c8`。
- 调试 ELF 配置路径：`build/HC_SR04_ECHO.elf`；`tasks.json` 的预启动任务调用 CMake 构建。
- 配置文件只能证明项目设置了这些工具；扩展安装状态、DAP Mini 在线状态和 F5 实际连接结果待确认。

## 14. AI Agent 接手规则

任何 AI Agent 在修改本工程前：

1. 先阅读 `PROJECT_STATE.md`，再读取实际相关源码。
2. 若本文与源码或 `.ioc` 冲突，以当前源码和 CubeMX 配置为准。
3. 不依赖旧聊天历史作为唯一事实源。
4. 修改关键架构后同步更新本文；不记录临时调试数据。
5. 未经确认，不修改已锁定引脚和时钟配置。

## 15. 2026-09-29 温度补偿 V1

- 本独立测距工程接入从 `OLED-DHT11` 复用的 DHT11 驱动，沿用 PB14 DATA；`main.c` 每约 2 秒在 ECHO 非活动时尝试读取一次，成功后通过 `HCSR04_SetTemperature()` 更新缓存。没有改变本工程原有 PA0 TRIG、PA8/TIM1_CH1 ECHO 接线，也没有实现最终总工程的 TIM2/舵机/串口/LabVIEW 功能。
- HC-SR04 将有效 ECHO 脉宽按 `echo_us * (331.3 + 0.606 * T) / 20000` 转成厘米，然后进入原三点距离滤波。接受温度范围 0–50 °C；开机未取得有效温度时用 20 °C，读取失败或越界时保留最后有效温度。`hcsr04_temperature_valid`、`hcsr04_temperature_c`、`hcsr04_temperature_used_c`、`hcsr04_sound_speed_m_s` 可用于观察。
- 2026-09-29 在原工程执行 GNU Arm/CMake 构建通过。DHT11 读数、温度补偿后的实际测距和与未来 UART 的并发行为尚未做硬件验证；原有链接器 RWX 警告仍存在。
