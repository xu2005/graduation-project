# ReusableDrivers

这三个模块直接由当前工程编译；目录中不包含 HAL、CMSIS、构建产物、Encoder 或 Key。把需要的模块复制到新的 CubeMX 工程，加入 `.c` 源文件和对应头文件目录即可。模块头文件通过新工程的 `main.h` 取得 HAL 类型；应用的 GPIO、Timer 和 Channel 只在调用处指定。

## OLED：软件 I2C，SSD1306 兼容 128×64

在 CubeMX 中选任意两个 GPIO，配置为 **Output Open Drain、Initial High、No Pull、Low Speed**。模块不初始化或切换 GPIO 模式。`OLED_Config` 接收 SCL/SDA 的 Port、Pin 和未左移的 7 位地址；本工程地址为 `0x3C`，发送写地址时驱动内部左移一次。软件 I2C 保留原有 Start/Stop、MSB first、释放 SDA 的第九个时钟和短 NOP 延时，不读取 ACK。

```c
#include "oled.h"

static OLED_HandleTypeDef display; /* 约 1 KiB framebuffer，放在静态存储区 */

/* 先调用 CubeMX 生成的 MX_GPIO_Init()。 */
OLED_Config(&display, OLED_SCL_GPIO_Port, OLED_SCL_Pin,
            OLED_SDA_GPIO_Port, OLED_SDA_Pin, 0x3C);
OLED_Init(&display);
OLED_SetCursor(&display, 0, 0); /* page 0, pixel column 0 */
OLED_DrawString(&display, "Encoder Servo");
OLED_Update(&display);
```

每个 OLED handle 自带 framebuffer、脏页标记和刷新游标，可分别保存多个设备的显示状态。`OLED_Init` 会清空并实际发送整个屏幕；`OLED_Update` 一次发送所有变化页；`OLED_UpdateSlice` 每次最多发送 16 个显示数据字节，适合有定时任务的主循环。改变短字符串前可先 `OLED_ClearLine(&display, page)`，避免旧字符残留。内置 5×7 字体只覆盖当前示例画面用到的 ASCII 字符，其他字符会显示为空格。软件延时默认按当前 72 MHz 工程使用；换系统频率后请在目标硬件上确认总线时序。

## Servo：位置舵机 PWM

在 CubeMX 中选择支持 PWM 的 Timer Channel，并把 **计数频率设为 1 MHz**（1 count = 1 µs）、周期设为 **20 ms / 50 Hz**。例如 STM32F103 的 Timer 时钟为 72 MHz 时，PSC=71、ARR=19999。驱动不修改 Timer 初始化，也不指定引脚。脉宽可由调用方校准；当前 SG90 使用 500/1500/2500 µs 对应 0/90/180°，角度映射及限幅与原工程一致。

```c
#include "servo.h"

static Servo_HandleTypeDef servo;

/* 先调用 CubeMX 生成的 MX_TIM2_Init()。 */
if (Servo_Init(&servo, &htim2, TIM_CHANNEL_2, 500, 1500, 2500) != HAL_OK) {
    Error_Handler();
}
Servo_SetAngle(&servo, 90);
```

在新工程中只需把 `&htim2` 和 `TIM_CHANNEL_2` 换为已配置的 Timer handle 与 Channel。无需修改 `servo.c`。`Servo_Init` 启动 PWM，`Servo_GetAngle` 返回当前目标角度。

## ServoScan：非阻塞往复扫描

模块只依赖 `Servo_HandleTypeDef`。调用方提供当前毫秒 tick；模块不读取按键、GPIO 或系统时钟，也不使用阻塞延时。一次 `ServoScan_Update` 最多推进一个步进，并在上下限反向。

```c
#include "servo_scan.h"

static ServoScan_HandleTypeDef scan;
ServoScan_Init(&scan, &servo, 0, 180, 1, 30);

/* 在业务层处理按键、串口或其他启动命令。 */
ServoScan_Start(&scan, HAL_GetTick());

/* 主循环中反复调用。 */
ServoScan_Update(&scan, HAL_GetTick());

/* 停止命令：立即回到 90°。 */
ServoScan_StopAndCenter(&scan, 90);
```

`ServoScan_IsRunning` 可由业务层决定 OLED 上显示 `AUTO` 还是 `MANUAL`。手动控制时由业务层调用 `Servo_SetAngle`；扫描运行时不要再从手动输入调用它。扫描上下限应处于 Servo 的角度范围内，步进和间隔必须大于零。

## 接入 CMake

```cmake
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ReusableDrivers/OLED/oled.c
    ReusableDrivers/Servo/servo.c
    ReusableDrivers/ServoScan/servo_scan.c
)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ReusableDrivers/OLED
    ReusableDrivers/Servo
    ReusableDrivers/ServoScan
)
```
