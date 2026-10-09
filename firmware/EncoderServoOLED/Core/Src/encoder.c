#include "encoder.h"
#include "main.h"
#include <limits.h>

/* Index: previous AB in bits 3:2, current AB in bits 1:0.
 * Positive sequence: 00 -> 01 -> 11 -> 10 -> 00. */
static const int8_t transition_delta[16] = {
     0,  1, -1,  0,
    -1,  0,  0,  1,
     1,  0,  0, -1,
     0, -1,  1,  0
};

static volatile uint8_t previous;
static volatile int8_t quarter_steps;
static volatile int32_t pending_steps;
static volatile int32_t count;
static volatile uint8_t ready;

static uint8_t Encoder_ReadAB(void)
{
    uint8_t a = (HAL_GPIO_ReadPin(ENC_A_GPIO_Port, ENC_A_Pin) == GPIO_PIN_SET) ? 1U : 0U;
    uint8_t b = (HAL_GPIO_ReadPin(ENC_B_GPIO_Port, ENC_B_Pin) == GPIO_PIN_SET) ? 1U : 0U;
    return (uint8_t)((a << 1) | b);
}

void Encoder_Init(void)
{
    ready = 0U;
    previous = Encoder_ReadAB();
    quarter_steps = 0;
    pending_steps = 0;
    count = 0;
    ready = 1U;
}

void Encoder_Update(void)
{
    if (ready == 0U) {
        return;
    }

    uint8_t current = Encoder_ReadAB();
    uint8_t transition = (uint8_t)((previous << 2) | current);
    previous = current;

    /* A two-bit jump loses phase information; discard the partial detent. */
    if ((transition == 3U) || (transition == 6U) ||
        (transition == 9U) || (transition == 12U)) {
        quarter_steps = 0;
        return;
    }

    int8_t delta = transition_delta[transition];
#if ENCODER_REVERSE
    delta = (int8_t)-delta;
#endif
    quarter_steps = (int8_t)(quarter_steps + delta);
    if ((quarter_steps == 4) || (quarter_steps == -4)) {
        int8_t step = (quarter_steps > 0) ? 1 : -1;
        quarter_steps = 0;
        if (((step > 0) && (count < INT32_MAX)) ||
            ((step < 0) && (count > INT32_MIN))) {
            count += step;
        }
        if (((step > 0) && (pending_steps < INT32_MAX)) ||
            ((step < 0) && (pending_steps > INT32_MIN))) {
            pending_steps += step;
        }
    }
}

int8_t Encoder_GetStep(void)
{
    int8_t step = 0;
    __disable_irq();
    if (pending_steps > 0) {
        --pending_steps;
        step = 1;
    } else if (pending_steps < 0) {
        ++pending_steps;
        step = -1;
    }
    __enable_irq();
    return step;
}

int32_t Encoder_GetCount(void)
{
    /* Aligned 32-bit reads are atomic on Cortex-M3. */
    return count;
}
