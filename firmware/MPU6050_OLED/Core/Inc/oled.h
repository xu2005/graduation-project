/**
  ******************************************************************************
  * @file    oled.h
  * @brief   SSD1306 OLED 驱动（软件模拟 I2C，PB8 = SCL / PB9 = SDA）
  ******************************************************************************
  * @note    引脚已由 CubeMX 在 MX_GPIO_Init() 中配置为开漏输出、初始高电平、
  *          无内部上下拉，本驱动不再重复初始化 GPIO，只做软件时序和显存写入。
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef PROJECT_OLED_H
#define PROJECT_OLED_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Exported constants --------------------------------------------------------*/
/* 控制器假设：SSD1306（常见的 0.96" 128x64 I2C OLED 模块） */
#define OLED_WIDTH             128U
#define OLED_HEIGHT            64U
#define OLED_PAGE_COUNT        (OLED_HEIGHT / 8U)   /* 页寻址模式下的页数：8 */

/* 7-bit 从机地址，模块背面默认焊 0x3C（若焊了 ADDR 电阻则为 0x3D）。
   软件 I2C 在线路上传的是 8 位字节，所以写地址 = (7bit << 1)，
   驱动内部一律由下面这一处左移得到，其他地方不再出现裸 0x78。 */
#define OLED_I2C_ADDR_7BIT     0x3CU
#define OLED_I2C_ADDR_WRITE    ((uint8_t)(OLED_I2C_ADDR_7BIT << 1))

/* 软件 I2C 引脚：PB8 = SCL，PB9 = SDA（CubeMX 已配置为开漏输出） */
#define OLED_SCL_GPIO_PORT     GPIOB
#define OLED_SCL_PIN           GPIO_PIN_8
#define OLED_SDA_GPIO_PORT     GPIOB
#define OLED_SDA_PIN           GPIO_PIN_9

/* Exported functions prototypes ---------------------------------------------*/

/**
  * @brief  初始化 SSD1306，并清空屏幕
  */
void OLED_Init(void);

/**
  * @brief  清空整个屏幕
  */
void OLED_Clear(void);

/**
  * @brief  显示一行中英混排文本
  * @param  x    起始列，单位像素（0 ~ 127）
  * @param  page 起始页，单位 8 像素；一行文本占 page 和 page+1 两页，
  *              所以只能取 0 / 2 / 4 / 6，共 4 行
  * @param  text UTF-8 字符串；汉字按 16x16、ASCII 按 8x16 绘制，
  *              字库未收录的汉字留一个汉字宽度的空白
  * @note   源文件是 UTF-8 编码，所以可以直接写 "姿态模块" 这样的字面量
  */
void OLED_ShowText(uint8_t x, uint8_t page, const char *text);

/**
  * @brief  显示十进制整数（8x16 ASCII）
  * @param  x    起始列，单位像素
  * @param  page 起始页，单位 8 像素（同样占两页）
  * @param  value 待显示的整数
  * @param  width 固定占用的字符数（1 ~ 11），右对齐、左侧补空格，
  *               定宽是为了让数值变化时字符位置不跳动
  */
void OLED_ShowTextNum(uint8_t x, uint8_t page, int32_t value, uint8_t width);

/**
  * @brief  显示一个字节的两位十六进制值（8x16 ASCII），例如 0x68 显示为 "68"
  */
void OLED_ShowTextHex(uint8_t x, uint8_t page, uint8_t value);

#ifdef __cplusplus
}
#endif

#endif /* PROJECT_OLED_H */