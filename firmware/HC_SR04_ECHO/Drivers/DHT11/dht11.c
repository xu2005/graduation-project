#include "dht11.h"

#include <stddef.h>

#define DHT11_START_LOW_US         18000U
#define DHT11_RESPONSE_TIMEOUT_US 100U
#define DHT11_MAX_POLL_GAP_US      12U
#define DHT11_BIT_LOW_MIN_US       30U
#define DHT11_BIT_LOW_MAX_US       70U
#define DHT11_BIT_ZERO_MIN_US      15U
#define DHT11_BIT_ZERO_MAX_US      40U
#define DHT11_BIT_ONE_MIN_US       55U
#define DHT11_BIT_ONE_MAX_US       90U

#ifndef DHT11_CYCLES_NOW
#define DHT11_CYCLES_NOW() (DWT->CYCCNT)
#endif

static void DHT11_DelayUs(uint32_t us, uint32_t cycles_per_us)
{
    const uint32_t cycles = cycles_per_us * us;
    const uint32_t start = DHT11_CYCLES_NOW();

    while ((uint32_t)(DHT11_CYCLES_NOW() - start) < cycles) {
        __NOP();
    }
}

static uint8_t DHT11_WaitForLevel(DHT11_HandleTypeDef *hdht,
                                  GPIO_PinState level, uint32_t cycles_per_us,
                                  uint32_t *last_poll, uint32_t *edge_cycle)
{
    const uint32_t timeout_cycles = cycles_per_us * DHT11_RESPONSE_TIMEOUT_US;
    const uint32_t max_gap_cycles = cycles_per_us * DHT11_MAX_POLL_GAP_US;
    const uint32_t start = DHT11_CYCLES_NOW();

    for (;;) {
        GPIO_PinState pin = HAL_GPIO_ReadPin(hdht->port, hdht->pin);
        uint32_t now = DHT11_CYCLES_NOW();

        /* An ISR may hide an edge; discard this frame instead of guessing a bit. */
        if (((uint32_t)(now - *last_poll) > max_gap_cycles) ||
            ((uint32_t)(now - start) >= timeout_cycles)) {
            return 0U;
        }
        *last_poll = now;
        if (pin == level) {
            *edge_cycle = now;
            return 1U;
        }
    }
}

void DHT11_Init(DHT11_HandleTypeDef *hdht)
{
    if (hdht == NULL) {
        return;
    }

    /* This board wires the DHT11 DATA signal to PB14. */
    hdht->port = GPIOB;
    hdht->pin = GPIO_PIN_14;
    hdht->temperature = 0U;
    hdht->humidity = 0U;
    hdht->status = DHT11_STATUS_READY;

    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* Cortex-M3 DWT cycle counter provides microsecond timing without a timer. */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = hdht->pin;
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(hdht->port, &gpio);
    HAL_GPIO_WritePin(hdht->port, hdht->pin, GPIO_PIN_SET);
}

uint8_t DHT11_Read(DHT11_HandleTypeDef *hdht)
{
    if (hdht == NULL) {
        return 0U;
    }
    if ((hdht->port == NULL) || (hdht->pin == 0U)) {
        hdht->status = DHT11_STATUS_INVALID_HANDLE;
        return 0U;
    }

    uint8_t data[5] = {0U};
    uint8_t result = 0U;
    uint32_t edge_cycle;
    uint32_t low_start;
    uint32_t high_start;
    uint32_t high_end;
    uint32_t last_poll;
    const uint32_t cycles_per_us = SystemCoreClock / 1000000U;
    hdht->status = DHT11_STATUS_TIMEOUT;

    if ((cycles_per_us == 0U) ||
        ((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) == 0U)) {
        return 0U;
    }

    /* Send the DHT11 start pulse, then release DATA and switch to input. */
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = hdht->pin;
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(hdht->port, &gpio);
    HAL_GPIO_WritePin(hdht->port, hdht->pin, GPIO_PIN_RESET);
    /* DWT also completes when the caller entered with interrupts disabled. */
    DHT11_DelayUs(DHT11_START_LOW_US, cycles_per_us);
    HAL_GPIO_WritePin(hdht->port, hdht->pin, GPIO_PIN_SET);

    gpio.Mode = GPIO_MODE_INPUT;
    HAL_GPIO_Init(hdht->port, &gpio);

    last_poll = DHT11_CYCLES_NOW();

    /* Sensor response: about 80 us low followed by about 80 us high. */
    if ((DHT11_WaitForLevel(hdht, GPIO_PIN_RESET, cycles_per_us,
                            &last_poll, &edge_cycle) == 0U) ||
        (DHT11_WaitForLevel(hdht, GPIO_PIN_SET, cycles_per_us,
                            &last_poll, &edge_cycle) == 0U)) {
        goto restore_pin;
    }

    for (uint8_t bit = 0U; bit < 40U; ++bit) {
        /* Measure the low and high pulse with DWT while IRQs stay enabled. */
        if ((DHT11_WaitForLevel(hdht, GPIO_PIN_RESET, cycles_per_us,
                                &last_poll, &low_start) == 0U) ||
            (DHT11_WaitForLevel(hdht, GPIO_PIN_SET, cycles_per_us,
                                &last_poll, &high_start) == 0U)) {
            goto restore_pin;
        }
        if (((uint32_t)(high_start - low_start) <
             cycles_per_us * DHT11_BIT_LOW_MIN_US) ||
            ((uint32_t)(high_start - low_start) >
             cycles_per_us * DHT11_BIT_LOW_MAX_US) ||
            (DHT11_WaitForLevel(hdht, GPIO_PIN_RESET, cycles_per_us,
                                &last_poll, &high_end) == 0U)) {
            goto restore_pin;
        }

        uint32_t high_cycles = (uint32_t)(high_end - high_start);
        data[bit / 8U] <<= 1U;
        if ((high_cycles >= cycles_per_us * DHT11_BIT_ONE_MIN_US) &&
            (high_cycles <= cycles_per_us * DHT11_BIT_ONE_MAX_US)) {
            data[bit / 8U] |= 1U;
        } else if ((high_cycles < cycles_per_us * DHT11_BIT_ZERO_MIN_US) ||
                   (high_cycles > cycles_per_us * DHT11_BIT_ZERO_MAX_US)) {
            goto restore_pin;
        }
    }

    if ((uint8_t)(data[0] + data[1] + data[2] + data[3]) == data[4]) {
        hdht->temperature = data[2];
        hdht->humidity = data[0];
        hdht->status = DHT11_STATUS_OK;
        result = 1U;
    } else {
        hdht->status = DHT11_STATUS_CHECKSUM_ERROR;
    }

restore_pin:
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    HAL_GPIO_Init(hdht->port, &gpio);
    HAL_GPIO_WritePin(hdht->port, hdht->pin, GPIO_PIN_SET);
    return result;
}
