#include "oled.h"
#include <stdbool.h>
#include <stddef.h>

#define OLED_WIDTH 128U
#define OLED_PAGES 8U
#define OLED_GLYPH_WIDTH 6U

#define OLED_SCL_H(oled) HAL_GPIO_WritePin((oled)->scl_port, (oled)->scl_pin, GPIO_PIN_SET)
#define OLED_SCL_L(oled) HAL_GPIO_WritePin((oled)->scl_port, (oled)->scl_pin, GPIO_PIN_RESET)
#define OLED_SDA_H(oled) HAL_GPIO_WritePin((oled)->sda_port, (oled)->sda_pin, GPIO_PIN_SET)
#define OLED_SDA_L(oled) HAL_GPIO_WritePin((oled)->sda_port, (oled)->sda_pin, GPIO_PIN_RESET)
#define OLED_ADDRESS_WRITE(oled) ((uint8_t)((oled)->address << 1U))

/* Only the printable ASCII glyphs needed by this application's display. */
static const struct {
    char character;
    uint8_t columns[5];
} font[] = {
    {' ', {0x00, 0x00, 0x00, 0x00, 0x00}},
    {'-', {0x08, 0x08, 0x08, 0x08, 0x08}},
    {':', {0x00, 0x36, 0x36, 0x00, 0x00}},
    {'0', {0x3E, 0x51, 0x49, 0x45, 0x3E}},
    {'1', {0x00, 0x42, 0x7F, 0x40, 0x00}},
    {'2', {0x42, 0x61, 0x51, 0x49, 0x46}},
    {'3', {0x21, 0x41, 0x45, 0x4B, 0x31}},
    {'4', {0x18, 0x14, 0x12, 0x7F, 0x10}},
    {'5', {0x27, 0x45, 0x45, 0x45, 0x39}},
    {'6', {0x3C, 0x4A, 0x49, 0x49, 0x30}},
    {'7', {0x01, 0x71, 0x09, 0x05, 0x03}},
    {'8', {0x36, 0x49, 0x49, 0x49, 0x36}},
    {'9', {0x06, 0x49, 0x49, 0x29, 0x1E}},
    {'A', {0x7E, 0x11, 0x11, 0x11, 0x7E}},
    {'C', {0x3E, 0x41, 0x41, 0x41, 0x22}},
    {'D', {0x7F, 0x41, 0x41, 0x22, 0x1C}},
    {'E', {0x7F, 0x49, 0x49, 0x49, 0x41}},
    {'L', {0x7F, 0x40, 0x40, 0x40, 0x40}},
    {'M', {0x7F, 0x02, 0x0C, 0x02, 0x7F}},
    {'N', {0x7F, 0x04, 0x08, 0x10, 0x7F}},
    {'O', {0x3E, 0x41, 0x41, 0x41, 0x3E}},
    {'S', {0x46, 0x49, 0x49, 0x49, 0x31}},
    {'T', {0x01, 0x01, 0x7F, 0x01, 0x01}},
    {'U', {0x3F, 0x40, 0x40, 0x40, 0x3F}},
    {'H', {0x7F, 0x08, 0x08, 0x08, 0x7F}},
    {'%', {0x63, 0x13, 0x08, 0x64, 0x63}},
    {'a', {0x20, 0x54, 0x54, 0x54, 0x78}},
    {'c', {0x38, 0x44, 0x44, 0x44, 0x20}},
    {'d', {0x38, 0x44, 0x44, 0x48, 0x7F}},
    {'e', {0x38, 0x54, 0x54, 0x54, 0x18}},
    {'g', {0x0C, 0x52, 0x52, 0x52, 0x3E}},
    {'i', {0x00, 0x44, 0x7D, 0x40, 0x00}},
    {'l', {0x00, 0x41, 0x7F, 0x40, 0x00}},
    {'m', {0x7C, 0x04, 0x18, 0x04, 0x78}},
    {'n', {0x7C, 0x08, 0x04, 0x04, 0x78}},
    {'o', {0x38, 0x44, 0x44, 0x44, 0x38}},
    {'p', {0x7C, 0x14, 0x14, 0x14, 0x08}},
    {'r', {0x7C, 0x08, 0x04, 0x04, 0x08}},
    {'t', {0x04, 0x3F, 0x44, 0x40, 0x20}},
    {'u', {0x3C, 0x40, 0x40, 0x20, 0x7C}},
    {'v', {0x1C, 0x20, 0x40, 0x20, 0x1C}},
    {'y', {0x0C, 0x50, 0x50, 0x50, 0x3C}}
};

static void OLED_I2C_Delay(void)
{
    /* Conservative half-cycle for an open-drain GPIO bus at 72 MHz. */
    for (volatile uint32_t i = 0U; i < 80U; ++i) {
        __NOP();
    }
}

static void OLED_I2C_Start(OLED_HandleTypeDef *oled)
{
    OLED_SDA_H(oled);
    OLED_SCL_H(oled);
    OLED_I2C_Delay();
    OLED_SDA_L(oled);
    OLED_I2C_Delay();
    OLED_SCL_L(oled);
}

static void OLED_I2C_Stop(OLED_HandleTypeDef *oled)
{
    OLED_SDA_L(oled);
    OLED_I2C_Delay();
    OLED_SCL_H(oled);
    OLED_I2C_Delay();
    OLED_SDA_H(oled);
    OLED_I2C_Delay();
}

static void OLED_I2C_WriteByte(OLED_HandleTypeDef *oled, uint8_t value)
{
    for (uint8_t bit = 0U; bit < 8U; ++bit) {
        if ((value & 0x80U) != 0U) {
            OLED_SDA_H(oled);
        } else {
            OLED_SDA_L(oled);
        }
        OLED_I2C_Delay();
        OLED_SCL_H(oled);
        OLED_I2C_Delay();
        OLED_SCL_L(oled);
        value <<= 1;
    }
    /* Ninth clock: release SDA so the OLED may acknowledge. */
    OLED_SDA_H(oled);
    OLED_I2C_Delay();
    OLED_SCL_H(oled);
    OLED_I2C_Delay();
    OLED_SCL_L(oled);
}

static void OLED_WriteCommands(OLED_HandleTypeDef *oled, const uint8_t *commands,
                               uint8_t length)
{
    OLED_I2C_Start(oled);
    OLED_I2C_WriteByte(oled, OLED_ADDRESS_WRITE(oled));
    OLED_I2C_WriteByte(oled, 0x00U);
    for (uint8_t i = 0U; i < length; ++i) {
        OLED_I2C_WriteByte(oled, commands[i]);
    }
    OLED_I2C_Stop(oled);
}

static const uint8_t *OLED_Glyph(char character)
{
    for (uint32_t i = 0U; i < sizeof(font) / sizeof(font[0]); ++i) {
        if (font[i].character == character) {
            return font[i].columns;
        }
    }
    return font[0].columns;
}

void OLED_Config(OLED_HandleTypeDef *oled, GPIO_TypeDef *scl_port, uint16_t scl_pin,
                 GPIO_TypeDef *sda_port, uint16_t sda_pin, uint8_t address)
{
    if (oled == NULL) {
        return;
    }
    oled->scl_port = scl_port;
    oled->scl_pin = scl_pin;
    oled->sda_port = sda_port;
    oled->sda_pin = sda_pin;
    oled->address = address;
    oled->initialized = false;
}

void OLED_Init(OLED_HandleTypeDef *oled)
{
    static const uint8_t init_commands[] = {
        0xAE,       /* Display off */
        0xD5, 0x80, /* Display clock */
        0xA8, 0x3F, /* 64 multiplex lines */
        0xD3, 0x00, /* Display offset */
        0x40,       /* Start line */
        0x8D, 0x14, /* Internal charge pump */
        0x20, 0x02, /* Page addressing mode */
        0xA1,       /* Segment remap */
        0xC8,       /* Reverse COM scan direction */
        0xDA, 0x12, /* COM pin layout for 128x64 */
        0x81, 0x7F, /* Contrast */
        0xD9, 0xF1, /* Pre-charge */
        0xDB, 0x40, /* VCOMH */
        0xA4,       /* Use display RAM */
        0xA6,       /* Normal display */
        0x2E,       /* Disable scrolling */
        0xAF        /* Display on */
    };

    if ((oled == NULL) || (oled->scl_port == NULL) || (oled->sda_port == NULL) ||
        (oled->address > 0x7FU)) {
        return;
    }
    OLED_SCL_H(oled);
    OLED_SDA_H(oled);
    HAL_Delay(20U); /* Power-up settling only; bit timing uses OLED_I2C_Delay. */
    OLED_WriteCommands(oled, init_commands, (uint8_t)sizeof(init_commands));
    oled->initialized = true;
    OLED_Clear(oled);
}

void OLED_Clear(OLED_HandleTypeDef *oled)
{
    if (oled == NULL) {
        return;
    }
    for (uint8_t page = 0U; page < OLED_PAGES; ++page) {
        for (uint8_t column = 0U; column < OLED_WIDTH; ++column) {
            oled->framebuffer[page][column] = 0U;
        }
        oled->dirty[page] = true;
        oled->next_column[page] = 0U;
    }
    oled->cursor_page = 0U;
    oled->cursor_column = 0U;
    OLED_Update(oled);
}

void OLED_ClearLine(OLED_HandleTypeDef *oled, uint8_t page)
{
    if ((oled == NULL) || (page >= OLED_PAGES)) {
        return;
    }
    for (uint8_t column = 0U; column < OLED_WIDTH; ++column) {
        oled->framebuffer[page][column] = 0U;
    }
    oled->dirty[page] = true;
    oled->next_column[page] = 0U;
}

void OLED_SetCursor(OLED_HandleTypeDef *oled, uint8_t page, uint8_t column)
{
    if ((oled != NULL) && (page < OLED_PAGES) && (column < OLED_WIDTH)) {
        oled->cursor_page = page;
        oled->cursor_column = column;
    }
}

void OLED_DrawChar(OLED_HandleTypeDef *oled, char character)
{
    if ((oled == NULL) || (oled->cursor_page >= OLED_PAGES) ||
        (oled->cursor_column > OLED_WIDTH - OLED_GLYPH_WIDTH)) {
        return;
    }
    const uint8_t *glyph = OLED_Glyph(character);
    for (uint8_t i = 0U; i < OLED_GLYPH_WIDTH; ++i) {
        uint8_t value = (i < 5U) ? glyph[i] : 0U;
        if (oled->framebuffer[oled->cursor_page][oled->cursor_column + i] != value) {
            oled->framebuffer[oled->cursor_page][oled->cursor_column + i] = value;
            oled->dirty[oled->cursor_page] = true;
            oled->next_column[oled->cursor_page] = 0U;
        }
    }
    oled->cursor_column = (uint8_t)(oled->cursor_column + OLED_GLYPH_WIDTH);
}

void OLED_DrawString(OLED_HandleTypeDef *oled, const char *text)
{
    if ((oled == NULL) || (text == NULL)) {
        return;
    }
    while (*text != '\0') {
        OLED_DrawChar(oled, *text++);
    }
}

void OLED_Update(OLED_HandleTypeDef *oled)
{
    if ((oled == NULL) || !oled->initialized) {
        return;
    }
    for (uint8_t page = 0U; page < OLED_PAGES; ++page) {
        if (!oled->dirty[page]) {
            continue;
        }
        uint8_t position[] = {(uint8_t)(0xB0U | page), 0x00U, 0x10U};
        OLED_WriteCommands(oled, position, (uint8_t)sizeof(position));

        OLED_I2C_Start(oled);
        OLED_I2C_WriteByte(oled, OLED_ADDRESS_WRITE(oled));
        OLED_I2C_WriteByte(oled, 0x40U);
        for (uint8_t column = 0U; column < OLED_WIDTH; ++column) {
            OLED_I2C_WriteByte(oled, oled->framebuffer[page][column]);
        }
        OLED_I2C_Stop(oled);
        oled->dirty[page] = false;
        oled->next_column[page] = 0U;
    }
}

void OLED_UpdateSlice(OLED_HandleTypeDef *oled)
{
    if ((oled == NULL) || !oled->initialized) {
        return;
    }
    for (uint8_t page = 0U; page < OLED_PAGES; ++page) {
        if (!oled->dirty[page]) {
            continue;
        }

        uint8_t column = oled->next_column[page];
        uint8_t position[] = {
            (uint8_t)(0xB0U | page),
            (uint8_t)(column & 0x0FU),
            (uint8_t)(0x10U | (column >> 4))
        };
        OLED_WriteCommands(oled, position, (uint8_t)sizeof(position));

        OLED_I2C_Start(oled);
        OLED_I2C_WriteByte(oled, OLED_ADDRESS_WRITE(oled));
        OLED_I2C_WriteByte(oled, 0x40U);
        for (uint8_t sent = 0U; (sent < 16U) && (column < OLED_WIDTH);
             ++sent, ++column) {
            OLED_I2C_WriteByte(oled, oled->framebuffer[page][column]);
        }
        OLED_I2C_Stop(oled);

        oled->next_column[page] = column;
        if (column == OLED_WIDTH) {
            oled->dirty[page] = false;
            oled->next_column[page] = 0U;
        }
        return;
    }
}
