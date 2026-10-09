# STM32F103C8T6 + HC-SR04 独立测距开发提示词

你现在正在修改一个由 STM32CubeMX 生成的 STM32F103C8T6 HAL 工程。

## 一、当前硬件与 CubeMX 配置

### MCU
- STM32F103C8T6
- HAL 库
- 系统时钟 SYSCLK = 72 MHz
- HSE = 8 MHz
- PLL ×9
- HCLK = 72 MHz
- APB1 = 36 MHz
- APB2 = 72 MHz

### 调试器
- 使用 DAP Mini，通过 SWD 烧录和调试
- PA13 = SWDIO
- PA14 = SWCLK
- 本阶段不使用 DAP Mini 的 UART 功能
- 不使用 USART

### HC-SR04 接线
- TRIG → PA0
- ECHO → PA8 / TIM1_CH1
- VCC → 5V
- GND → GND
- ECHO 硬件侧已经考虑 5V 到 3.3V 电平保护，不需要在软件中处理

### CubeMX 已完成如下配置

#### PA0
- GPIO Output
- Push-Pull
- No Pull
- 初始输出 Low

#### PA8
- TIM1_CH1
- Input Capture Direct Mode

#### TIM1
- 时钟 = 72 MHz
- Prescaler = 71
- Counter Period / ARR = 65535
- Counter Mode = Up
- Clock Division = No Division
- Channel 1 Input Capture
- 初始捕获极性 = Rising Edge
- IC Selection = Direct
- IC Prescaler = No Division
- Input Filter = 0
- TIM1 Capture Compare Interrupt 已开启

因为：

```text
72 MHz / (71 + 1) = 1 MHz
```

所以 TIM1 每个计数对应 1 μs。

## 二、本阶段目标

请只实现一个可靠、简单、容易调试的 HC-SR04 测距模块。

暂时不要加入：
- OLED
- SG90
- LabVIEW
- USART
- DMA
- FreeRTOS
- printf 重定向
- 其他传感器

我要先确认 HC-SR04 和 TIM1 输入捕获能够正常工作。

## 三、程序工作逻辑

1. PA0 给 HC-SR04 TRIG 发送至少 10 μs 的高电平脉冲。
2. PA8/TIM1_CH1 检测 ECHO。
3. 第一次捕获 ECHO 上升沿。
4. 记录上升沿计数值。
5. 将 TIM1_CH1 捕获极性切换为下降沿。
6. 第二次捕获下降沿。
7. 计算 ECHO 高电平持续时间，单位 μs。
8. 要正确考虑 16 位 TIM1 计数器发生溢出的情况。
9. 计算距离：

```c
distance_cm = echo_time_us / 58.0f;
```

10. 完成一次测量以后，把输入捕获极性恢复成 Rising Edge，准备下一次测量。
11. 主循环每约 100 ms 发起一次测量。

## 四、必须保留的调试变量

请创建适合在 VS Code / GDB / Debug Watch 中直接观察的全局 `volatile` 变量，例如：

```c
volatile uint32_t echo_rising;
volatile uint32_t echo_falling;
volatile uint32_t echo_time_us;
volatile float distance_cm;
volatile uint8_t measurement_ready;
volatile uint32_t measurement_count;
```

如果你认为变量名称需要调整可以调整，但必须让我能够在 Debug Watch 中直接看到：

- 上升沿捕获值
- 下降沿捕获值
- ECHO 脉宽
- 当前距离 cm
- 是否完成一次测量
- 已经成功测量多少次

不要依赖串口才能观察结果。

## 五、10 μs 延时

`HAL_Delay()` 只能做到毫秒级，不能用于产生 10 μs TRIG。

请实现可靠的微秒级延时。

优先考虑使用：
- DWT Cycle Counter

或者其他不会破坏 TIM1 输入捕获功能的方案。

不要为了 10 μs 延时重新占用 TIM1，因为 TIM1 已经用于 HC-SR04 ECHO 输入捕获。

如果 STM32F103 的 DWT 可以使用，请实现：

```c
DWT_Delay_Init();
delay_us(uint32_t us);
```

并说明其依据。

## 六、代码组织

希望采用模块化结构，优先新增：

```text
Core/Inc/hcsr04.h
Core/Src/hcsr04.c
```

模块至少提供类似接口：

```c
void HCSR04_Init(void);
void HCSR04_Trigger(void);
float HCSR04_GetDistance(void);
uint8_t HCSR04_IsReady(void);
```

具体接口可以根据实际工程适当调整。

TIM1 输入捕获中断使用 HAL：

```c
HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
```

必须判断：

```c
htim->Instance == TIM1
```

并确认是 TIM_CHANNEL_1 对应的活动通道，避免以后加入其他定时器后产生冲突。

## 七、CubeMX 工程保护要求

这是 CubeMX 自动生成的工程。

请遵守以下规则：

1. 不要随意重写 `main.c`。
2. 不要删除 CubeMX 自动生成的初始化代码。
3. 对 `main.c` 的修改尽量放在：

```c
/* USER CODE BEGIN ... */
/* USER CODE END ... */
```

区域。

4. 不要修改 CubeMX 已经生成的时钟配置，除非发现明确错误。
5. 不要修改 TIM1 的 PSC、ARR 和 GPIO 引脚分配。
6. 不要修改 PA13/PA14 SWD。
7. 不要加入 USART。
8. 不要引入新的第三方库。

## 八、可靠性要求

HC-SR04 有可能：
- 没有收到回波
- 测量超时
- 出现异常脉宽
- 超出有效测量范围

因此不能让程序永久等待 ECHO。

请加入基本的超时机制。

如果一次测量失败：
- 不要死循环
- 不要阻塞整个 MCU
- 标记本次数据无效
- 下一周期允许重新测量

可以把无效距离设置为：

```c
-1.0f
```

或使用单独的状态变量。

## 九、你的工作顺序

在修改任何代码之前：

1. 先检查当前 CubeMX 生成工程的文件结构。
2. 阅读 `main.c`、`tim.c`、`gpio.c`、`stm32f1xx_it.c`、对应头文件。
3. 确认实际生成出来的 TIM1 句柄名称和 PA0 GPIO 宏。
4. 不要猜测已有变量和宏的名称。

然后：

5. 创建 HC-SR04 驱动模块。
6. 对 `main.c` 做最少量修改。
7. 编译工程。
8. 如果存在编译错误，修正到零 error。
9. 检查 warning，不能为了消除 warning 随意破坏 CubeMX 代码。

## 十、完成后向我报告

修改完成后不要只说“完成”。

请明确告诉我：

1. 新建了哪些文件。
2. 修改了哪些文件。
3. `main.c` 加了什么。
4. TIM1 输入捕获回调在哪里。
5. 10 μs 延时如何实现。
6. 如何处理计数器溢出。
7. 如何处理 HC-SR04 超时。
8. 我烧录以后应该在 Debug Watch 中观察哪些变量。
9. 正常情况下这些变量应该出现什么变化。

最后给出第一次上电测试步骤。

**不要继续加入 OLED、舵机或者 LabVIEW。等 HC-SR04 独立测试通过以后再进入下一阶段。**
