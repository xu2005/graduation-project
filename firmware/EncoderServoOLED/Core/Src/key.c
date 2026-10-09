#include "key.h"
#include "main.h"

#define KEY_DEBOUNCE_MS 20U

static volatile uint8_t stable_level;
static volatile uint8_t candidate_level;
static volatile uint8_t candidate_ms;
static volatile uint8_t pressed_event;
static volatile uint8_t ready;

void Key_Init(void)
{
    ready = 0U;
    stable_level = (HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin) == GPIO_PIN_SET) ? 1U : 0U;
    candidate_level = stable_level;
    candidate_ms = 0U;
    pressed_event = 0U;
    ready = 1U;
}

void Key_Update(void)
{
    if (ready == 0U) {
        return;
    }

    uint8_t level = (HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin) == GPIO_PIN_SET) ? 1U : 0U;
    if (level != candidate_level) {
        candidate_level = level;
        candidate_ms = 0U;
    } else if (candidate_ms < KEY_DEBOUNCE_MS) {
        ++candidate_ms;
    }

    if ((candidate_ms == KEY_DEBOUNCE_MS) && (stable_level != candidate_level)) {
        stable_level = candidate_level;
        if (stable_level == 0U) {
            pressed_event = 1U;
        }
    }
}

bool Key_WasPressed(void)
{
    bool pressed;
    __disable_irq();
    pressed = (pressed_event != 0U);
    pressed_event = 0U;
    __enable_irq();
    return pressed;
}
