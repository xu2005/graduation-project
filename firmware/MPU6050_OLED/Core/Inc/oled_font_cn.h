/**
  ******************************************************************************
  * @file    oled_font_cn.h
  * @brief   中文 16x16 与 ASCII 8x16 点阵字库声明
  ******************************************************************************
  * @note    汉字按 Unicode 码点查表，ASCII 按 (字符 - 0x20) 查表。
  *          字形数据在 oled_font_cn.c 中，由脚本从系统字体栅格化生成。
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef PROJECT_OLED_FONT_CN_H
#define PROJECT_OLED_FONT_CN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Exported constants --------------------------------------------------------*/
/* 汉字：16x16，每个字形 32 字节 = 上半页 16 列 + 下半页 16 列 */
#define OLED_FONT_CN_COUNT      6U
#define OLED_FONT_CN_BYTES      32U
#define OLED_FONT_CN_WIDTH      16U
#define OLED_FONT_CN_HEIGHT     16U

/* ASCII：8x16 半角，每个字形 16 字节 = 上半页 8 列 + 下半页 8 列 */
#define OLED_FONT_ASCII_FIRST   0x20U
#define OLED_FONT_ASCII_LAST    0x7EU
#define OLED_FONT_ASCII_COUNT   95U
#define OLED_FONT_ASCII_BYTES   16U
#define OLED_FONT_ASCII_WIDTH   8U
#define OLED_FONT_ASCII_HEIGHT  16U

/* Fixed UTF-8 title subset: 加 速 度 陀 螺 仪. Add future glyphs explicitly. */

/* Exported variables --------------------------------------------------------*/
extern const uint16_t OLED_FontCN16_Code[OLED_FONT_CN_COUNT];
extern const uint8_t OLED_FontCN16[OLED_FONT_CN_COUNT][OLED_FONT_CN_BYTES];
extern const uint8_t OLED_FontASCII8x16[OLED_FONT_ASCII_COUNT][OLED_FONT_ASCII_BYTES];

#ifdef __cplusplus
}
#endif

#endif /* PROJECT_OLED_FONT_CN_H */