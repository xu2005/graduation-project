# MPU6050_OLED —— 工程约定

## 硬件（CubeMX 锁定，任何情况下不要改动）

- MCU: STM32F103C8T6。SYSCLK/HCLK 72 MHz，PCLK1 36 MHz，PCLK2 72 MHz（HSE 8 MHz × PLL9）
- PB8 = OLED SCL / PB9 = OLED SDA：GPIO 开漏输出、无上下拉、初始高，软件模拟 I2C
- PB10 = I2C2_SCL / PB11 = I2C2_SDA：MPU6050 走硬件 I2C2，100 kHz、7-bit、允许时钟拉伸、无 DMA、无中断
- PA13/PA14 = SWDIO/SWCLK，必须保留
- 不要改 .ioc、不要改时钟、不要改引脚、不要改 I2C2 参数。确实需要改时先停下来说明原因，等用户确认。

## 代码规则

- 只用 HAL API（stm32f1xx_hal.h / HAL_GPIO_* / HAL_I2C_* / HAL_Delay）。
  禁止标准外设库（GPIO_Init、I2C_GenerateSTART、RCC_APB1PeriphClockCmd…）和 LL 库。
- 不用 Keil、不用 FreeRTOS、不建 Task/Queue/多层抽象。这是裸机教学项目，驱动保持单层。
- 头文件保护宏禁止 _XXX_H_ 这类保留标识符，用 PROJECT_XXX_H。
- 整数用 uint8_t/uint16_t/uint32_t/int8_t/int16_t/int32_t；字符串可用 char。
- 命名如 MPU6050_Init / OLED_ShowString，不要极端缩写。注释解释"为什么"，不要机械翻译代码。

## CubeMX 生成文件的保护规则

- main.c / gpio.c / i2c.c / stm32f1xx_hal_msp.c / stm32f1xx_it.c 由 CubeMX 生成。
- 只允许在 /* USER CODE BEGIN ... */ 与 /* USER CODE END ... */ 之间插代码。
- 不要删除 CubeMX 标记，不要重写 SystemClock_Config()，不要重新初始化 PB8/PB9/PB10/PB11。
- 必须保证用户随时能重新运行 CubeMX Generate Code 而不破坏用户代码。
- USER CODE 区域之外的任何改动，一律按严重问题处理。

## 构建

- CMake + Ninja + arm-none-eabi-gcc 14.3.1，C11，-Wall -Wextra -Wpedantic，Debug = -O0 -g3。
- 用户源文件登记在顶层 CMakeLists.txt 的 "# Add user sources here" 段；
  不要动 cmake/stm32cubemx/CMakeLists.txt（它会被 CubeMX 重新生成）。
- 构建命令：

  ```powershell
  cmake --preset Debug
  cmake --build --preset Debug
  ```

  产物在 build/Debug/：MPU6050_OLED.elf / .bin / .hex / .map。

- 已知且可接受，不要报告为问题：
  1. CMake 开发者警告 project() should be called prior to enable_language()
  2. 链接器警告 LOAD segment with RWX permissions
  3. 软件 I2C 写 SSD1306 时忽略 ACK 返回值（写操作没有可回读的数据）

## 审查要求

- 默认只读：不修改文件，输出结构化问题清单（严重/中等/建议，含 文件:行 与理由）。
- 静态分析无法确定的问题（需要示波器或实机验证），单独列到"需实机验证"，不要猜。