# STM32二维超声波扫描系统整合方案

> 审查日期：2026-09-26。下列源码路径均相对于 `C:\Users\xuhengyi\Desktop\item\stm32 project`。本报告依据现有 `.c`、`.h`、`.ioc`、CMake 和 Markdown 文件进行静态盘点，也查看了相关压缩包的目录；本轮未改动任何现有驱动、未烧录硬件。**【现状】**表示文件中可核实的实现或用户明确给定的最终约束（两者会写清）；**【分析】**表示从实现推得的限制；**【建议】**表示尚未实施的整合方案。源码存在和历史构建记录均不等同于本轮实机验收。

## 1. 项目最终目标

**【建议】**以 STM32F103C8T6 为主控，SG90 带动 HC-SR04 做单轴角度往复扫描；每个有效扫描点形成同一时刻、同一指向的 `(角度 °, 距离 cm)`，可在 LabVIEW 中转换成平面极坐标图或 `x=d·cosθ, y=d·sinθ` 点集。DHT11 每约 2 秒更新环境量，OLED 显示状态，PB12 切换页面，USART1 与 LabVIEW 交换控制和数据。全局由裸机主循环中的非阻塞扫描状态机调度，不引入 RTOS、DMA 或 C++。

**【现状】**按用户本轮最新要求，最终连线是 **PB0 TRIG、PA0/TIM2_CH1 ECHO、PA6/TIM3_CH1 舵机、PB8/PB9 软件 I2C OLED、PA9/PA10 USART1、PB12 按键**。先前说明中的 PB6/PB7 I2C1 OLED 已由这一最新要求替代。这是一套**最终目标配置**；目前所读独立工程并未整体采用此配置。

## 2. 当前已有工程与模块

| 工程或资产 | 读到的实现与证据 | 对本毕设的定位 |
|---|---|---|
| `HC_SR04_ECHO/` | **【现状】**`HC_SR04_ECHO.ioc:43-52,110-114` 与 `Core/Src/main.c:95-126,189-219` 为 PA0 TRIG、PA8/TIM1_CH1 ECHO；驱动在 `Drivers/HC_SR04/hcsr04.c/.h`，由顶层 `CMakeLists.txt:48-56` 编译。`Core/Src/stm32f1xx_it.c:204` 转交 TIM1 捕获中断。`PROJECT_STATE.md:64-67` 记录旧工程曾构建通过，同时说明无实机/滤波验收记录。 | 测距算法来源；**不是**已经完成的最终 TIM2 接线工程。 |
| `EncoderServoOLED/` | **【现状】**`EncoderServoOLED.ioc:46-79,138-144` 使用 PA1/TIM2_CH2 舵机、PB8/PB9 软件 I2C OLED、PB0/PB1 编码器、PB12 上拉按键；`Core/Src/main.c:129-214` 运行舵机、按键、OLED；顶层 `CMakeLists.txt:45-60` 纳入 `ReusableDrivers`。 | `Servo` 和 OLED 显存/刷新可复用；原自动扫描仅控制舵机，不能直接充当测距状态机。 |
| `OLED-DHT11/` | **【现状】**`OLED-DHT11.ioc:45-61` 用 PB14 DHT11、PB8/PB9 软件 I2C OLED；`Core/Src/main.c:98-115` 每次读数后 `HAL_Delay(2000)`。`cmake/stm32cubemx/CMakeLists.txt:15-31` 纳入驱动源码。 | DHT11 数据读取与校验可复用；调度方式与引脚硬编码需小改。 |
| `MPU6050_OLED/` | **【现状】**`MPU6050_OLED.ioc:48-70` 用 PB10/PB11 I2C2 访问 MPU6050，PB8/PB9 软件 I2C OLED，PB12 上拉按键；`Core/Src/main.c:81-99,294-310` 有主循环 20 ms 去抖和页面切换。`Core/Inc/oled_font_cn.h:30-42` 有完整 95 个可打印 ASCII 字形。 | PB12 轮询去抖和字形可作参考；MPU6050 及其整套显示驱动不进入最终功能。该子工程有独立 `AGENTS.md` 约定，本轮只读。 |
| `1/light/` | **【现状】**`light.ioc:44-63` 和 `Core/Src/main.c:70-94` 是 PA0–PA7 GPIO/延时练习。 | 与扫描系统无直接复用价值；其中 PA0、PA6 旧用途不能带入总工程。 |
| 压缩包与文档 | **【现状】**`驱动库/` 有 HC-SR04、DHT11、OLED、EncoderServoOLED ZIP；`舵机自动扫描oled.rar` 含同名 `ReusableDrivers`；`2.rar` 是 `2/2` 基础 CubeMX 工程；另有 `MPU6050_OLED.rar`。`计划实施过程报告/交付说明.md` 是学习计划文档。 | 以当前展开源码作为审查依据；压缩包作备份，不把副本算作新的已完成模块。 |

**【分析】**在所读展开工程的用户代码和 `.ioc` 中，未发现 PB0+PA0/TIM2_CH1 的测距实现、USART1 收发/协议实现、LabVIEW `.vi/.lvproj` 或统一 `FinalProject`。HAL 库自带 UART 代码不代表应用已实现串口；各独立工程 `Core/Inc/stm32f1xx_hal_conf.h:68` 仍注释 UART 模块开关。

## 3. 可直接复用的代码

| 代码 | 可复用部分与边界 |
|---|---|
| `HC_SR04_ECHO/Drivers/HC_SR04/hcsr04.c/.h` | **【现状】**`hcsr04.h:12-22` 通过句柄、通道、TRIG Port/Pin 配置硬件；`hcsr04.c:340-385` 捕获上升/下降沿、处理 16 位回绕及 116–23200 µs 有效范围；`174-198` 用 `/58` 换算厘米；`296-327` 实现约 40 ms 超时；`101-131` 实现首点直出、第二点均值、第三个有效样本起三点滚动中值。**【建议】**原核心算法保持，接入新 TIM2 配置并补少量接口/诊断。 |
| `EncoderServoOLED/ReusableDrivers/Servo/servo.c/.h` | **【现状】**`servo.h:16-30` 的 handle 可配置 `TIM_HandleTypeDef*`、通道和 500/1500/2500 µs 等脉宽；`servo.c:28-58` 做 0–180° 限幅与两段角度映射。**【建议】**不改映射核心，传入 `htim3/TIM_CHANNEL_1`，先校准真实机械安全角。 |
| `EncoderServoOLED/ReusableDrivers/OLED/oled.c/.h` | **【现状】**`oled.h:9-34` 有 1024 字节 framebuffer、脏页和每次最多 16 字节的 `OLED_UpdateSlice()`；`oled.c:242-300` 已实现按脏页/分片刷新。**【建议】**保留绘制、显存和分片刷新思路。 |
| `OLED-DHT11/Drivers/DHT11/dht11.c/.h` | **【现状】**`dht11.c:20-31,97-141` 有边沿等待超时、40 位读数、校验和、温湿度与错误状态。**【建议】**保留读取流程，改调度和引脚来源。 |
| 按键去抖逻辑 | **【现状】**`MPU6050_OLED/Core/Src/main.c:81-99` 已有基于 `HAL_GetTick()` 的 20 ms 主循环轮询去抖；`EncoderServoOLED/Core/Src/key.c:22-51` 有 1 ms SysTick 更新和按下事件。**【建议】**取前者的时间戳轮询思路，封装成小型 PB12 按键模块。 |

## 4. 需要修改的模块

| 模块 | 需要的最小改动及原因 |
|---|---|
| HC-SR04 接入 | **【建议】**新 `.ioc` 生成 PB0 GPIO TRIG、PA0/TIM2_CH1 输入捕获、`TIM2_IRQHandler` 和 NVIC；总工程传入 `&htim2/TIM_CHANNEL_1/GPIOB/GPIO_PIN_0`。移除独立 `main.c:119-126` 的每 100 ms 自主触发/`HAL_Delay(1)`，由 `ScanApp` 独占触发。增加“触发被接受/本次完成/结果快照”的最小接口或经严格状态与计数确认，见第 12 节。 |
| HC-SR04 诊断 | **【分析】**`hcsr04.c:211-260,269-277` 只在初始化和新触发前清 CCxOF，`340-385` 捕获回调未判别 overcapture。**【建议】**在总工程移植验收阶段补 CC1OF 检测、计数和本次结果作废，不重写测距/滤波算法。 |
| Servo 和 ServoScan | **【建议】**Servo 驱动本身仅换外部 TIM3 配置；`ReusableDrivers/ServoScan/servo_scan.c:49-74` 会到时自主推进角度，不能保证上一角测距结束，因此最终 `ScanApp` 只借鉴其边界反向算法，由测距完成事件决定下一步。 |
| OLED | **【现状】**`ReusableDrivers/OLED/oled.c:9-13` 在可配置 GPIO 上模拟 I2C；旧工程已经用 PB8/PB9。**【建议】**最终继续用 PB8/PB9 软件 I2C，把两脚在新 `.ioc` 配成开漏输出、初始高，直接复用底层时序、framebuffer、脏页和分片刷新。传入未左移的 7 位地址 `0x3C`，驱动内部左移。`oled.c:15-55` 只含演示所需字形，当前缺少如 `Angle/Distance/Humidity/USART` 的部分字母，应按页面文字补字形，或择用 `MPU6050_OLED` 的完整 ASCII 字形，避免把两套 OLED 驱动同时编译。 |
| DHT11 | **【现状】**`dht11.c:40-42` 在 `DHT11_Init()` 写死 PB14；`63-141` 的读操作阻塞。**【建议】**最终优先保留 PB14，并把 Port/Pin 由配置传入或至少集中定义；删掉示例主循环 2 秒整段等待，改为到期再调用一次读数。 |
| Button | **【建议】**PB12 上拉、按下为低，使用主循环 tick 去抖和一次短按事件；不要原样搬入 `key.c` 的 SysTick 逐毫秒计数实现，因为新的主循环调用周期不固定。 |
| 构建配置 | **【分析】**`OLED-DHT11` 把自定义驱动登记在可能再生成的 `cmake/stm32cubemx/CMakeLists.txt:15-31`；`EncoderServoOLED` 和 `HC_SR04_ECHO` 登记在顶层 CMake。**【建议】**总工程统一在顶层 CMake 注册用户源码/头目录，让 CubeMX 再生成安全。 |

## 5. 不建议继续使用的历史代码

**【建议】**以下仅作为历史对照，不直接复制进最终总工程：

1. `HC_SR04_ECHO/HC_SR04_ECHO.ioc:43-52,110-114`、`Core/Src/main.c:95-100`、`Drivers/HC_SR04/README.md:42-50` 的 PA0 TRIG、PA8/TIM1_CH1 示例；它们与用户确认的 PB0 TRIG、PA0/TIM2_CH1 目标不同。`hcsr04.c:368` 中写着 TIM1 的注释也只是旧说明。
2. `EncoderServoOLED/EncoderServoOLED.ioc:46-47,138-144` 的 PA1/TIM2_CH2 PWM 配置；TIM2 必须留给最终 ECHO。`Core/Src/main.c:196-198` 和 `ServoScan_Update()` 的按时间独立移角不能直接与回波采集并跑。
3. `EncoderServoOLED` 的 PB0/PB1 编码器接线和 `Core/Src/encoder.c`；PB0 已指定为最终 TRIG。`MPU6050_OLED` 的 I2C2/MPU6050 功能与目标无关。
4. `OLED-DHT11/Core/Src/main.c:107-115` 的 `HAL_Delay(2000)` 循环、`1/light/` 的延时练习。PB8/PB9 软件 I2C 接线本身与最新最终要求一致，应保留；只需在新 `.ioc` 核对开漏、初始高和上拉。

## 6. 最终硬件资源分配

### 6.1 GPIO

| 引脚 | 最终用途与模式 | 旧工程占用/检查 | 结论 |
|---|---|---|---|
| PA0 | HC-SR04 ECHO，TIM2_CH1 输入捕获 | HC 旧工程是 PA0 TRIG；`light` 是 GPIO 输出 | **【建议】**新板确认 ECHO 到 PA0 的 5 V→3.3 V 电平保护；不能沿用旧 PA8 保护的推断。 |
| PA6 | SG90，TIM3_CH1 PWM 复用推挽 | `light` 为 GPIO 输出；Servo 旧工程在 PA1 | **【建议】**按最终接线重配。 |
| PA9 / PA10 | USART1 TX / RX | 已读独立 `.ioc` 未启用 USART1 | **【建议】**接 USB 转串口模块并共地，电平为 3.3 V TTL；不是直接 RS-232 电平。 |
| PB0 | HC-SR04 TRIG，GPIO 推挽输出，初始低 | Encoder 旧工程为编码器 A | **【建议】**编码器功能不并入。 |
| PB8 / PB9 | OLED 软件 I2C SCL / SDA，GPIO 开漏输出、初始高 | `EncoderServoOLED` 与 `OLED-DHT11` 均为此接线 | **【建议】**直接复用可配置 OLED 驱动；检查 OLED 板上拉与 7 位地址 `0x3C`。 |
| PB6 / PB7 | 本版不分配给 OLED | 先前目标草案曾写 I2C1 | **【现状】**用户最新要求已改为 PB8/PB9 软件 I2C；新工程不启用 I2C1。 |
| PB12 | 用户按键，GPIO 输入上拉，低电平按下 | 两个旧工程均已有 PB12 输入上拉 | **【建议】**保留，主循环轮询。 |
| PB14 | DHT11 DATA，开漏释放/输入切换 | `OLED-DHT11.ioc:45-50` 与 `dht11.c:40-42` 一致 | **【建议】**优先保留 PB14；确认实际板线与 DATA 上拉。 |
| PA13 / PA14；PD0 / PD1 | SWD；8 MHz HSE | 各 72 MHz 示例工程保留 | **【建议】**继续保留，不给应用复用。 |

### 6.2 Timer

| 资源 | 最终参数/职责 | 冲突判定 |
|---|---|---|
| TIM2_CH1 | **【建议】**PA0 ECHO 输入捕获；1 MHz 计数、PSC=71（在 72 MHz TIM 时钟下）、ARR=65535；捕获中断。 | **【分析】**原舵机 TIM2_CH2 的 ARR=19999 与此共享同一个 TIM2 计数器，虽通道不同仍冲突，必须迁出。 |
| TIM3_CH1 | **【建议】**PA6 SG90 PWM；1 MHz 计数、PSC=71、ARR=19999，即 20 ms/50 Hz；不需 PWM 中断。 | 与 TIM2 分离。脉宽初值 500/1500/2500 µs 仍须实机校准限位。 |
| DWT CYCCNT | **【现状】**HC 的 10 µs TRIG 与 DHT11 的微秒读数都用 DWT，而非 TIM2/TIM3（`hcsr04.c:48-66`，`dht11.c:9-31,49-52`）。 | **【分析】**共享 DWT 本身不占 Timer，但两个 Init 都重置 CYCCNT；必须在开始扫描前完成初始化，不可在运行中重置。 |
| SysTick | **【建议】**保留 HAL 1 ms tick，供非阻塞时间戳比较。 | DHT11 短时关中断会影响 tick 精度，见第 14 节。 |

**【分析】**现有 72 MHz 工程 `.ioc` 将 APB1 分频为 2、TIM 时钟为 72 MHz（如 `EncoderServoOLED.ioc:118-123`）；总工程新生成后仍要核对时钟树，再填写 PSC，不仅凭旧数值。

### 6.3 UART

**【建议】**USART1 占 PA9/PA10，先试 115200、8N1、无流控；波特率是待实测参数。仅用普通 HAL UART 中断接收和单缓冲发送即可，暂不需要 DMA。串口口径、上位机串口号、换行符及错误应在协议联调阶段固定。现有源码没有可直接复用的 USART1 应用实现。OLED 软件 I2C 不占用 I2C1 外设或其中断。

### 6.4 Interrupt

| 中断/回调 | 最终职责与优先原则 |
|---|---|
| `TIM2_IRQHandler` → HAL → `HAL_TIM_IC_CaptureCallback` | **【建议】**只把 `htim2/CH1` 分派给 HC；回调中只做捕获状态推进，不绘屏、不发送串口。 |
| `USART1_IRQHandler` → HAL → `HAL_UART_RxCpltCallback`、`HAL_UART_ErrorCallback`、必要时 TX 完成回调 | **【建议】**每字节快收、重启接收；错误计数并恢复；主循环解析命令。 |
| SysTick | **【建议】**保留 HAL tick；PB12 轮询在主循环，因此不加按键 ISR。 |
| EXTI / TIM 更新中断 | **【现状】**现有 PB12 是输入上拉而非 EXTI；当前最终功能无需新增 EXTI 或 TIM3 更新中断。 |

## 7. 模块冲突检查

| 类型 | 实际发现或集成风险 | 处理方式 |
|---|---|---|
| TIM2 计数器/ARR | **【分析】**`EncoderServoOLED.ioc:138-144` 的 TIM2_CH2 PWM 为 ARR=19999，HC 驱动 `hcsr04.h:12-14` 要 ARR=65535；共享计数器无法各用各的周期。 | **【建议】**最终 Servo 用 TIM3_CH1，HC 用 TIM2_CH1；新建 `.ioc`，不复制旧 MSP。 |
| PA0/PB0 | **【分析】**HC 旧演示 PA0 TRIG，编码器旧演示 PB0 输入，均与最终连线相反/冲突。 | **【建议】**只采纳最终 PA0 ECHO、PB0 TRIG。 |
| OLED 引脚/总线 | **【现状】**最新目标与 `ReusableDrivers/OLED` 的 PB8/PB9 软件 I2C 示例一致，无引脚迁移冲突。 | **【建议】**新 `.ioc` 配 GPIO 开漏、初始高，传入原 GPIO/地址配置，实测总线时序与上拉。 |
| DWT | **【分析】**两个驱动初始化都重置 DWT 计数器；运行中重置会破坏正在等待的微秒计时。 | **【建议】**初始化在扫描启动前集中完成，避免运行时重新初始化。 |
| DHT11 与 TIM2 捕获 | **【分析】**`dht11.c:92-140` 在 40 位读数期间全局关中断；若此时 ECHO 发生，捕获 ISR 被推迟甚至出现 CC1OF。 | **【建议】**只在 `measurement_active=0`、当前角度结果已结算的安全空档启动 DHT；之后做并发实测。 |
| DHT11 与 UART/SysTick | **【分析】**约 18 ms 起始低脉冲 `HAL_Delay` 加随后数毫秒关中断，会拖延调度；UART 字节可能硬件 overrun，SysTick 也会延迟。仅加软件环形缓冲无法挽回硬件丢字节。 | **【建议】**低频读取、ACK/重发及 UART 错误计数；联调若 STOP/SET 丢失，优先缩短 DHT 临界区或改变采样窗口。 |
| OLED 刷新与主循环 | **【分析】**整页软件 I2C 刷新耗时，`OLED_Update()` 会阻塞主循环；现有 `OLED_UpdateSlice()` 每次最多 16 数据字节，且写入时未检查 ACK。 | **【建议】**直接保留分片刷新，测距待回波时持续调用 `HCSR04_Process()`；如实测需要断线诊断，再小幅增加 ACK/错误反馈。 |
| HAL 回调符号 | **【现状】**目前只在 `HC_SR04_ECHO/Core/Src/main.c:259-262` 找到用户自定义 `HAL_TIM_IC_CaptureCallback`；未找到应用自定义 UART、EXTI、TIM PeriodElapsed 同名回调。 | **【建议】**总工程每种 HAL 回调只定义一次，集中放在 `Core/Src/main.c` 的 USER CODE 段或单独 `App/callback_dispatch.c`；按实例和通道分发到驱动。不得把多个工程的同名函数并排复制。 |

统一分发示意（**【建议】**，不是现有实现）：

```c
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2 && htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1) {
        HCSR04_TIM_IC_CaptureCallback(htim);
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) { Protocol_OnRxByteIRQ(huart); }
}
```

**【建议】**`HAL_GPIO_EXTI_Callback`、`HAL_TIM_PeriodElapsedCallback` 暂不定义；将来确需其他模块时在唯一分发处增分支，避免链接重定义和回调被覆盖。

## 8. 最终软件目录结构

**【建议】**沿用现有 `ReusableDrivers` 名称，CubeMX 生成代码与业务代码分开，用户文件在顶层 CMake 登记：

```text
FinalProject/
├─ FinalProject.ioc
├─ CMakeLists.txt                 # 顶层登记自定义源码/头路径
├─ Core/Inc, Core/Src            # CubeMX 初始化、IRQ、main.c
├─ Drivers/STM32F1xx_HAL_Driver, Drivers/CMSIS
├─ ReusableDrivers/
│  ├─ HC_SR04/hcsr04.c, hcsr04.h
│  ├─ Servo/servo.c, servo.h
│  ├─ OLED/oled.c, oled.h         # SSD1306 显存+PB8/PB9 软件 I2C
│  ├─ DHT11/dht11.c, dht11.h
│  └─ Button/button.c, button.h
├─ App/
│  ├─ scan.c, scan.h             # 唯一角度/触发调度者
│  ├─ display.c, display.h       # 页面与快照
│  └─ protocol.c, protocol.h     # 串口行缓冲、命令、报文
└─ docs/                         # 接线、协议、实测记录
```

**【分析】**`ServoScan` 属于按固定节拍独立推进的业务逻辑，最终以 `App/scan.c` 接管；可以移植它的边界反向计算，不一定再编译原 `ServoScan`。`main.c` 只负责 HAL/CubeMX 初始化、各模块 Init 和循环 Update；业务代码放 `App`。每个驱动只保留一套版本。

## 9. 各驱动模块职责

| 驱动 | 仅负责什么 | 不负责什么 |
|---|---|---|
| HC-SR04 | 一次 TRIG、ECHO 捕获、超时、有效性、三点滤波、诊断 | 决定舵机角度、每点发几次、USART 报文 |
| Servo | 给定角度→限幅→PWM 比较值；保持 50 Hz | 决定何时走到下一角；判断机械已到位 |
| OLED | PB8/PB9 软件 I2C 发送、显存绘制与分片刷新 | 决定显示哪一页、取哪个测距结果 |
| DHT11 | PB14 单次读取、校验、状态返回 | 每个扫描点自动读取、阻塞主循环 2 秒 |
| Button | PB12 去抖、短按事件 | 直接修改扫描状态或绘屏 |

## 10. App层职责

**【建议】**`scan` 独占伺服控制与 HC 触发，保存当前角、方向、当前测量序号、样本数和状态；`display` 维护 3 个页面并读取最近一次完整系统快照；`protocol` 把串口输入行转换为 `START/STOP/SET` 事件，把扫描快照格式化成有限长度文本。`main` 顺序调用这些 `Update()`；驱动之间不直接互相调用。

**【建议】**创建一个小的 `ScanPoint` 快照（角度、有效性、距离、温湿度及其新鲜度、状态、序号）。只有 `ScanApp` 在完成本角度的所有样本后写入并发布它，OLED 和 UART 只读这份快照，避免显示角度已经变了、距离却仍是上角的值。

## 11. 扫描状态机设计

**【建议】**状态序列采用 `IDLE → MOVE → SETTLE → WAIT_PING_GAP → TRIGGER → WAIT_ECHO → COLLECT → PUBLISH → NEXT → MOVE`。`WAIT_PING_GAP` 保证两次超声触发的最小间隔；第三个有效样本后才 `PUBLISH`。使用 `HAL_GetTick()` 的无符号差值比较，所有等待状态立即返回主循环，无 `HAL_Delay()`。

```text
IDLE --START--> MOVE(锁定当前角并发 PWM)
MOVE --> SETTLE(机械稳定时间到)
SETTLE --> WAIT_PING_GAP(确认上一测量已结算；首次可直达 TRIGGER)
WAIT_PING_GAP --> TRIGGER(时间间隔满足)
TRIGGER --> WAIT_ECHO(本次触发已接受、记录所属角度/序号)
WAIT_ECHO --> COLLECT(完成或 40 ms 超时；先处理待滤波样本)
COLLECT --未集齐且未超次数--> WAIT_PING_GAP
COLLECT --集齐/超次数--> PUBLISH(有效距离或明确 ERR)
PUBLISH --> NEXT(计算下一角；端点反向)
NEXT --> MOVE
任意工作状态 --STOP--> 停止后续触发；当前测量完成或超时并结算后 IDLE
```

**【建议】**示例初值可用安全角范围 10–170°、步进 5–10°、舵机稳定 80–150 ms、相邻 TRIG 间隔 60–100 ms；这些都属于待测参数。40 ms 是**等待回波上限**，并非完整扫描点固定耗时。`Servo_GetAngle()` 表示命令目标角，并无到位反馈；等待时间要在负载下实测。

## 12. HC-SR04与Servo数据同步方案

**【现状】**`HCSR04_FilterReset()` 在 `hcsr04.c:157-172` 只清窗口与待处理滤波样本，不取消正在进行的测量。ISR 在 `174-198` 仍可稍后发布旧回波。`HCSR04_Trigger()` 在 `269-294` 遇忙时 `void` 静默返回；`measurement_ready` 在 `335-338` 是保持到下一次成功触发的电平，不是事件。成功/超时时 `filtered_distance_cm` 可能仍保留旧有效值，不能只读它就当本点结果。

**【建议】**每一角使用如下严格顺序：

1. 停止发起下一角；等旧测量 `measurement_active==0`，执行 `HCSR04_Process()` 消费待处理样本，然后才允许清本角滤波窗口。若收到 STOP/SET，也先结算当前回波再动舵机或重置窗口。
2. 固定 `sample_angle` 和本角 `sequence`；向 Servo 下发角度，等待机械稳定；**此时不测距**。
3. 在确认驱动空闲及触发间隔后调用一次 Trigger；用新增 `TryTrigger()` 返回值，或比较 `hcsr04_trigger_count` 前后值，确认这一次确实开始。不要把被拒绝的调用当作一次采样。
4. 保持舵机目标角不变，循环服务 `HCSR04_Process()`；先观察本次完成标志，再**再次**调用 `HCSR04_Process()`，确保 ISR 已发布的待滤波样本被主循环消费。短临界区一次性取得 `measurement_valid`、结果、更新计数和本次角度。若超时/无效，发布 `ERR` 或受限重试；不发送旧 `filtered_distance_cm`。
5. 在本角所需有效样本数达到要求后封存 `(sample_angle, distance, ...)`，再更新显示/串口，然后才能算下一角和移动舵机。

**【分析】**`hcsr04.c:134-155` 用 `volatile` 单槽样本和 PRIMASK 短临界区把 ISR 原始样本交给主循环，这是可复用的并发设计；但多个公开字段分开读取并不形成一致快照。**【建议】**总工程的小改优先是 `TryTrigger()` 与 `TakeResult()`/结果序号接口，在短临界区取一份一致结果；若暂不改接口，应用必须严格执行上述状态/计数顺序并实测竞态。`CC1OF` 发生时作废该次样本。

## 13. 超声波滤波方案选择

| 方案 | 扫描速度与角度分辨率 | 稳定性和边缘效果 | 实施复杂度/演示 |
|---|---|---|---|
| A：每角度采 3 个**有效**样本，第三个输出三点中值 | **【分析】**每点至少 3 次 TRIG，若间隔 60–100 ms，测量部分约 120–300 ms，再加机械稳定；低步进会明显慢。可先用 5–10° 步进控制一轮时长。 | 同角度滤波，有利于消除孤立异常，障碍边缘仍对应正确角度；适合答辩讲清“角度+距离”。 | **【建议】**保留现有滤波代码，每角清窗口，数有效样本；最多 5 次尝试仍不足 3 个则报无效，避免卡死。 |
| B：相邻角度各测 1 次，跨角滚动三点中值 | 一次 TRIG/角，较快，可用更细角度步进。 | **【分析】**中值含邻角数据；物体边缘和细目标会被平滑或偏移，数据虽稳定却不再严格是“当前角的 3 次测量”。 | 不必每角重置，但须解释空间滤波的物理含义；答辩较难说明边缘误差。 |

**【建议】**首个完整演示版本选 **A**，优先保证角度归属与可解释性。为快速联调可先用单次测量完成状态机，再开启每角三次；不要把 B 当作 A 的实现。A 中 `HCSR04_FilterReset()` 仅在上一角全部测量已完成并处理后、本角第一次 TRIG 前调用；首/次样本的直出/均值仅作内部过渡，不作为最终三点中值输出。调速区分“角步进”和“稳定等待/采样间隔”，不能仅靠 `ServoScan_Update()` 的移动节拍。

## 14. DHT11调度方案

**【现状】**DHT11 确认使用 **PB14**（`OLED-DHT11.ioc:45-50`、`dht11.c:40-42`）。`dht11.c:9-31` 用 DWT 微秒计时与 100 µs 等级边沿等待；`78-90` 拉低 18 ms；`92-140` 关总中断读 40 位、校验，最终给整数温/湿度或超时/校验错误。独立示例 `Core/Src/main.c:107-115` 每次后等待 2 秒，适合单模块演示，不适合总工程。

**【建议】**每 **2 秒**尝试更新一次环境量；一次完整有效读数保留 `last_dht_ok_tick`，读失败标记无效或过期，不把旧温湿度伪装成新读数。调度只选择测距已结算、下一个 TRIG 尚未开始的间隙；一轮扫描不必每点调用。`DHT11_Read()` 本身仍会约 18 ms 阻塞并短暂屏蔽中断，因此“主扫描状态机非阻塞”并不等于所有底层驱动零阻塞。先测它对 Servo PWM、TIM2 捕获、USART 收发和 SysTick 的影响；若串口 ORE/STOP 丢失，优先缩短/拆分 DHT 关中断段或在上位机协议中安排明确的读数窗口，不能以软件队列掩盖硬件 overrun。

**【建议】**温度声速补偿暂列后续选项。现有 HC 距离用固定 `/58`（`hcsr04.c:178`）；先在无补偿条件下完成测距、角度绑定和误差实测，再基于原始 `echo_time_us` 明确计算并标注“补偿距离”，避免悄悄改变已验证算法或在一份协议字段中混用两种距离定义。

## 15. OLED页面设计方案

**【建议】**页面 1：`Angle / Distance / Scan state`，无有效回波显示 `--` 或 `ERR`；页面 2：`Temperature / Humidity` 和读数新鲜度；页面 3：`USART / LabVIEW` 连接或最近命令状态、超时/串口错误计数。页面由 `App/display.c` 决定，驱动只负责绘制。页面数据取第 10 节的完整快照；角度、距离一起更新。

**【分析】**`ReusableDrivers/OLED/oled.h:16-18` 和 `oled.c:266-299` 有脏页+16 字节分片，但字体表 `oled.c:15-55` 只覆盖旧演示字符，不能直接完整显示上述英文词。**【建议】**只补必需 ASCII 字形，或取 `MPU6050_OLED` 的完整 ASCII 字库；选定一套后不重复编译。显示重绘可以 100–200 ms/次或新点到达时触发，页面切换时整页标脏，主循环每次最多送一个有界小片。当前软件 I2C 写入未检查 ACK，故不要把 OLED 断线检测写成已具备功能；如实测需要再小改。

## 16. PB12按键方案

**【现状】**`EncoderServoOLED.ioc:62-66` 和 `MPU6050_OLED.ioc:54-58` 均配置 PB12 上拉输入。前者 `Core/Src/key.c:4,22-51` 在 SysTick 中连续计 20 ms 后生成低电平按下事件；后者 `Core/Src/main.c:81-99` 在主循环用时间戳检测 20 ms 稳定按下。

**【建议】**用后者的**主循环轮询+软件去抖**，无需 EXTI。只在稳定的高→低边沿发一次短按事件，释放稳定后重新允许下一次；短按按顺序切换 3 页。按键不直接控制舵机，以免与 LabVIEW 的扫描控制相互抢占；后续若要赋予 Start/Stop，先定义本地与上位机命令的优先级。

## 17. USART通信架构

**【现状】**已读展开工程中无 USART1 初始化、`HAL_UART_Receive_IT`、RX 完成回调或 LabVIEW 协议代码；目前串口要从新总工程建立。

**【建议】**CubeMX 配 USART1 PA9/PA10，普通中断接收一个字节并立即重挂接。ISR 只做：把字节放入**固定长度的一行命令缓冲**、识别 `\n`、越界时丢弃整行并计数；主循环消费完整行、校验、产生事件。先限定 PC 每条命令等待 ACK 再发下一条，64 字节行缓冲和单条待处理行即可；无需 RTOS、DMA、复杂消息队列或初版环形缓冲。若压力测试出现连续命令积压，再加小型环形缓冲；环形缓冲也无法消除 DHT 关中断造成的 UART 硬件 overrun。`HAL_UART_ErrorCallback` 统计 ORE/FE 并恢复接收，TX 可用单个固定缓冲 `HAL_UART_Transmit_IT`，忙时按规则丢弃/延后，避免在 ISR 或待回波期间长时间阻塞。

**【建议】**`STOP` 立刻禁止新的 TRIG/移角，但让正在进行的 ECHO 在最多 40 ms 后完成或超时并结算；发送状态 ACK 后进入 IDLE。`SET` 在 RUN 中返回 BUSY 或在安全边界生效，初版推荐 **BUSY**，避免半途改变角度范围。

## 18. LabVIEW通信协议建议

**【建议】**以下是**候选版**，联调时再冻结，ASCII、以 `$` 开头、`,` 分隔、`\r\n` 结尾。数值单位固定为角度 `deg`、距离 `cm`、温度 `°C`、湿度 `%RH`，小数位统一；PC 发一条命令等待应答。不要把第三个 `SET` 字段同时解释为“速度”和“角度步进”。

| 方向 | 候选报文 | 含义 |
|---|---|---|
| PC→MCU | `$START\r\n`、`$STOP\r\n` | 启动或安全停止。 |
| PC→MCU | `$SET,10,170,5,100\r\n` | 起角 10°、止角 170°、步进 5°、舵机稳定等待 100 ms；两次 TRIG 的最小间隔仍独立约束。 |
| MCU→PC | `$ACK,START\r\n`、`$ACK,SET\r\n` 或 `$ERR,BUSY/RANGE/FORMAT\r\n` | 确认接受或拒绝，便于 LabVIEW 重发和提示。 |
| MCU→PC | `$SCAN,42,45,62.4,25,48,RUN\r\n` | 序号 42、角度 45°、距离 62.4 cm、温度 25、湿度 48、运行中；一行对应一个已封存扫描点。 |
| MCU→PC | `$SCAN,43,50,NA,25,48,TIMEOUT\r\n` | 测距失败显式 `NA`，不得复用旧距离。 |

**【建议】**SET 至少校验 `0≤start<end≤180`，还要受实机校准的安全角限制；`step>0` 且不大于区间跨度，`settle_ms` 设合理上下界，报文长度和整数溢出均要检查。端点前最后一步可夹到 `end`，不越界。LabVIEW 用 VISA 按行读入、解析字段；先以原始文本终端验证，再画二维点。温湿度失效用 `NA` 或独立有效标记，协议冻结时统一一种。初版可不加校验和；若线缆/干扰实测需要，再加简单 checksum 与版本号。

## 19. 系统主循环调度框架

**【建议】**伪代码展示调用关系，不表示已经实现：

```c
HAL_Init(); SystemClock_Config();
MX_GPIO_Init(); MX_TIM2_Init(); MX_TIM3_Init(); MX_USART1_UART_Init();
Button_Init(); DHT11_Init(...); Servo_Init(&servo, &htim3, TIM_CHANNEL_1, ...);
HCSR04_Init(&hc_config); OLED_Init(...); Protocol_Init(); Scan_Init();
for (;;) {
    uint32_t now = HAL_GetTick();
    HCSR04_Process();                 // 高频服务超时与 ISR 待滤波样本
    Protocol_Update(now);            // 主循环解析完整命令，不在 RX ISR 解析
    Button_Update(now);              // PB12 去抖，向 Display 递交页面事件
    Scan_Update(now);                // 独占移角/触发/结果封存
    Environment_UpdateIfSafe(now);   // 到期且无活跃 ECHO 才读 DHT11
    Display_Update(now);             // 限时/分片刷新
    Protocol_SendPending(now);       // 有新封存点才发；TX 忙则按规则处理
}
```

**【分析】**只在 `Scan_Update()` 前调用一次 `HCSR04_Process()` 仍可能遇到“ISR 恰在两者之间完成”的竞态；`Scan_Update()` 看到 ready 后须再执行一次 Process 或使用原子 `TakeResult()`，见第 12 节。所有周期判断使用 `now - last_tick` 的无符号差值。示例中的初始化顺序以实际 CubeMX 生成的依赖为准，所有 DWT 初始化必须在启动扫描前结束。

## 20. 模块整合顺序

**【建议】**按最核心的数据归属风险排先后，逐阶段留可回退的可编译版本；本轮**只规划，不实施**。

| 阶段 | 输入 | 做什么 |
|---|---|---|
| 1. 新基础工程 | 本报告第 6 节资源表、STM32F103C8T6 | CubeMX 建 `FinalProject`，72 MHz 时钟、最终引脚、TIM2 IC/TIM3 PWM/USART1/PB12/PB14/PB8/PB9/SWD；OLED 两脚配 GPIO 开漏、初始高，生成并构建空框架。 |
| 2. 舵机单项 | 阶段 1，`ReusableDrivers/Servo` | TIM3_CH1 接 SG90，校准安全角和 50 Hz/脉宽。 |
| 3. 超声单项 | 阶段 1，`HC_SR04_ECHO/Drivers/HC_SR04` | PB0+PA0/TIM2_CH1 接入、统一捕获回调、观察上升/下降沿及超时。 |
| 4. 固定角测距 | 阶段 2+3 | 一次移角、等待稳定、一次 TRIG、结果/超时与角度封存；验证 STOP 不串角。 |
| 5. OLED 接入 | 阶段 1+4，`ReusableDrivers/OLED` | 直接接入 PB8/PB9 软件 I2C 驱动，先显示封存角度/距离和 ERR，补缺少的英文字符，保留分片刷新。 |
| 6. 自动扫描 | 阶段 4+5，`ServoScan` 的端点逻辑 | 建 `ScanApp` 单一调度者，完成往返、每角 3 次有效采样、超次数错误出口。 |
| 7. DHT11 | 阶段 6，PB14 驱动 | 约 2 秒安全空档读取，缓存新鲜度，检查捕获/SysTick 影响。 |
| 8. PB12 页面 | 阶段 5+7，现有去抖范例 | 主循环 20 ms 去抖，三页切换与异常显示。 |
| 9. USART1 基础 | 阶段 6/7，新 UART 初始化 | 普通 IT RX/TX、行缓冲、错误恢复；先串口终端测试。 |
| 10. MCU 协议 | 阶段 9，已定 ScanPoint | 实现候选命令/ACK/ERR/SCAN，合法性与 STOP 安全边界。 |
| 11. LabVIEW | 阶段 10 及冻结协议 | VISA 收发、Start/Stop/Set、二维图和错误状态；先用串口回放假数据。 |
| 12. 系统联调 | 阶段 1–11 的可运行版本 | 连续运行、异常注入、电源/时序/角度与误差记录；最后再评估温度补偿。 |

## 21. 每阶段验收标准

| 阶段 | 如何验证／验收条件 | 失败先检查 |
|---|---|---|
| 1 | `.ioc` 引脚无红色冲突，CMake 清洁构建，TIM2/TIM3/USART1 时钟及 IRQ 生成正确；PB8/PB9 为 GPIO 开漏初始高。 | APB1 Timer 倍频、PA0/PA6 复用、PB8/PB9 模式与上拉。 |
| 2 | 示波器或逻辑分析仪测 PA6 周期约 20 ms、脉宽覆盖校准安全范围；舵机往返不卡机械止挡。 | TIM3 PSC/ARR、供电能力、共地、脉宽限位。 |
| 3 | PB0 有约 10 µs TRIG；PA0 上升/下降捕获与回波宽度合理；无回波约 40 ms 返回超时，后续可继续测。 | 旧 TIM1 代码残留、TIM2 IRQ/MSP、ECHO 电平转换和连线、CC1OF。 |
| 4 | 固定 2–3 个角，每个有效数据对应当前命令角；改角前旧测量已结束；超时显示无效而非旧距离。 | Trigger 是否被接受、Process 调用频率、ready 电平、FilterReset 时机。 |
| 5 | PB8/PB9 软件 I2C 能初始化 OLED，`Angle/Distance/ERR` 字体完整；分片刷新时 HC 不丢捕获。 | GPIO 开漏/初始高、外部上拉、7 位地址 `0x3C`、缺字形、刷新耗时。 |
| 6 | 连续往返至少数轮，端点只访问一次并反向；每角 3 个有效样本才输出，失败有最大次数出口，点序号/角度单调合理。 | 独立 ServoScan 是否仍自主移角、TRIG 间隔、样本窗口重置、超时路径。 |
| 7 | 每约 2 秒更新温湿度，CRC/超时可见；读 DHT 时无 HC 捕获丢失，记录最大主循环停顿。 | PB14 上拉、DWT 初始化、关中断窗口与采样安排。 |
| 8 | 连按/长按只按定义切页，不因扫描或 OLED 刷新误触发；扫描数据不因换页改变。 | PB12 上拉、低电平逻辑、20 ms 时间戳、事件消费。 |
| 9 | 串口终端发多条换行命令无越界；RX 错误后能恢复，测距时 TX 不阻塞扫描；统计 ORE。 | PA9/PA10 接线、波特率、IRQ/重挂接、DHT 关中断。 |
| 10 | 合法 START/STOP/SET 有 ACK；非法参数有 ERR；每扫描点恰一条、失败为 NA，STOP 后无新点。 | 行缓冲溢出、状态切换、发送忙、角度/距离快照。 |
| 11 | LabVIEW 发送命令与 MCU ACK 对得上，点按角度绘制；断线和无效点可见。 | VISA 终止符、串口号/波特率、字段顺序、数值单位。 |
| 12 | 长时间连续扫描无卡死、无跨角样本、无串口 ORE/可恢复异常；保留逻辑分析仪波形、误差表和运行记录。 | 舵机供电压降、回波过捕获、DHT 临界区、OLED I2C 超时和协议重发。 |

## 22. 当前风险与技术债

| 优先级 | 风险与事实依据 | 处理节点 |
|---|---|---|
| 高 | **【分析】**最终 PA0/TIM2_CH1+PB0 路由尚无现成已验收工程；旧 HC 工程在 TIM1/PA8/PA0。 | 阶段 1、3 最先验证。 |
| 高 | **【分析】**旧 TIM2 舵机与最终 TIM2 捕获周期冲突；PB0 旧编码器与 TRIG 冲突。 | 阶段 1、2 清除旧接线。 |
| 高 | **【分析】**旧测距结果可能跨角；`FilterReset()` 不取消在途 ECHO；ready 与滤波处理不同步。 | 阶段 4、6 的验收重点。 |
| 高 | **【分析】**DHT 关中断可能造成 HC CC1OF、UART ORE；HC 回调当前没有显式 CC1OF 作废。 | 阶段 3、7、9、12 实测和最小补丁。 |
| 中 | **【分析】**现成 OLED 的软件 I2C 和 PB8/PB9 可直接复用，但字形不全，整页刷新会占用主循环，写入不检查 ACK。 | 阶段 5。 |
| 中 | **【分析】**现有项目无 UART/协议/LabVIEW VI，PC 交互和可视化为未完成工作。 | 阶段 9–11。 |
| 中 | **【分析】**SG90 500–2500 µs 的全范围是驱动默认，不代表当前机械结构能安全到 0/180°；供电冲击可能复位 MCU。 | 阶段 2、12 校准与供电检查。 |
| 中 | **【分析】**旧文档 `PROJECT_STATE.md:64-67` 只证实曾构建，未提供本轮实机/长期运行数据；不能据文件断言可靠性。 | 每阶段保留测试记录。 |
| 低 | **【分析】**`OLED-DHT11` 自定义源码登记在 CubeMX 生成 CMake 文件中，重生代码可能造成丢失/重复。 | 阶段 1 采用顶层登记。 |

## 23. 毕设后续里程碑

| 里程碑 | 可展示的结果 | 完成定义 |
|---|---|---|
| M1 统一硬件基础 | 同一块 STM32 上 PA6 舵机与 PA0/PB0 测距 | 阶段 1–4 验收，角度归属正确。 |
| M2 本地自动扫描 | 端点往返、每角有效距离、OLED 状态 | 阶段 5–6 验收，无阻塞大延时。 |
| M3 环境与人机交互 | PB14 温湿度、PB12 三页 | 阶段 7–8 验收，测距不受影响。 |
| M4 电脑通信 | USART1 文本协议与 LabVIEW 二维图 | 阶段 9–11 验收，ACK/ERR/无效点闭环。 |
| M5 答辩可靠性证据 | 连续运行、误差/时序/异常记录，必要时温度补偿 | 阶段 12 验收并整理图表和可复现实验步骤。 |

## 24. 下一步最推荐执行的任务

**【建议】下一步最值得做的一件事情：新建 `FinalProject.ioc` 和可编译的空白总工程，并在 CubeMX 资源图中一次锁定本报告第 6 节的最终引脚与 TIM2/TIM3/USART1 及 PB8/PB9 GPIO 配置。**先把最终 PA0/TIM2_CH1、PB0 TRIG、PA6/TIM3_CH1 和 PB8/PB9 软件 I2C 的资源关系落实，记录时钟树、IRQ、PB14/PB12、SWD、OLED 上拉与 ECHO 电平保护。验收是配置无冲突、代码可构建；**此时不复制合并所有独立工程**。随后只接 Servo 与 HC 的单角测距，优先验证数据归属。

### 当前毕设实际完成到什么程度？

以下为**【分析】工程进度粗估**，不是百分比验收报告，也不按简单平均求总进度。用户说明多个独立功能已分别验证；本轮源码可确认实现，缺少能独立核对的最终接线实测、长期运行与上位机成果，所以硬件百分比含此不确定性。

| 维度 | 粗估 | 依据 |
|---|---:|---|
| 硬件模块验证完成度 | 约 **50%** | 已有独立 Servo、HC、PB8/PB9 OLED、DHT 与按键工程及用户描述的单项验证；最终 PA0/TIM2、PA6/TIM3 与电平/供电组合未见实测记录。 |
| MCU 驱动完成度 | 约 **70%** | 测距捕获/超时/滤波、舵机可配置驱动、PB8/PB9 软件 I2C OLED、DHT 读取、PB12 去抖均有源码；仍缺 HC 同步接口/CC1OF、页面字形和 USART。 |
| 系统整合完成度 | 约 **5%** | 有 `ReusableDrivers` 和少量跨模块演示，但未发现统一 FinalProject、Servo+HC 角度绑定扫描状态机或资源无冲突的全集成运行。 |
| LabVIEW 上位机完成度 | 约 **0%（按本目录可核证资产）** | 未找到 `.vi/.lvproj` 或 MCU 协议联调代码；若上位机另存他处，应另行盘点。 |
| 整体毕设完成度 | 约 **25%** | 基础模块资产较多，但论文核心的二维扫描数据同步、电脑可视化和长期可靠运行尚未形成系统证据；这个估值向系统交付与验证倾斜。 |

**明确结论：现在完成的是若干有价值的独立模块与可复用驱动，不是基本完成的整机。下一阶段的核心，是让这些模块在确定的资源分配下，以同角度测距、统一调度和可复现的长期运行记录构成一个完整系统。**
