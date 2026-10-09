#ifndef OLED_H
#define OLED_H

#include "main.h" /* CubeMX HAL types supplied by the target project. */
#include <stdbool.h>
#include <stdint.h>

/* SSD1306-compatible 128x64 display. GPIOs must already be open-drain outputs. */
typedef struct {
    GPIO_TypeDef *scl_port;
    uint16_t scl_pin;
    GPIO_TypeDef *sda_port;
    uint16_t sda_pin;
    uint8_t address; /* Unshifted 7-bit I2C address, normally 0x3C. */

    uint8_t framebuffer[8][128];
    bool dirty[8];
    uint8_t next_column[8];
    uint8_t cursor_page;
    uint8_t cursor_column;
    bool initialized;
} OLED_HandleTypeDef;

void OLED_Config(OLED_HandleTypeDef *oled, GPIO_TypeDef *scl_port, uint16_t scl_pin,
                 GPIO_TypeDef *sda_port, uint16_t sda_pin, uint8_t address);
void OLED_Init(OLED_HandleTypeDef *oled);
void OLED_Clear(OLED_HandleTypeDef *oled);
void OLED_ClearLine(OLED_HandleTypeDef *oled, uint8_t page);
void OLED_SetCursor(OLED_HandleTypeDef *oled, uint8_t page, uint8_t column);
void OLED_DrawChar(OLED_HandleTypeDef *oled, char character);
void OLED_DrawString(OLED_HandleTypeDef *oled, const char *text);
void OLED_Update(OLED_HandleTypeDef *oled);
/* Flush at most 16 display bytes so the caller can service other tasks. */
void OLED_UpdateSlice(OLED_HandleTypeDef *oled);

#endif
