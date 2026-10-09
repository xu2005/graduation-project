#include "dht11.h"

#include <stddef.h>

#define DHT11_START_LOW_MS         18U
#define DHT11_RESPONSE_TIMEOUT_US 100U
#define DHT11_BIT_SAMPLE_US        40U

static void DHT11_DelayUs(uint32_t us)
{
    const uint32_t cycles_per_us = SystemCoreClock / 1000000U;
    const uint32_t cycles = cycles_per_us * us;
    const uint32_t start = DWT->CYCCNT;

    while ((uint32_t)(DWT->CYCCNT - start) < cycles) {
        __NOP();
    }
}

static uint8_t DHT11_WaitForLevel(DHT11_HandleTypeDef *hdht, GPIO_PinState level)
{
    const uint32_t timeout_cycles =
        (SystemCoreClock / 1000000U) * DHT11_RESPONSE_TIMEOUT_US;
    const uint32_t start = DWT->CYCCNT;

    while (HAL_GPIO_ReadPin(hdht->port, hdht->pin) != level) {
        if ((uint32_t)(DWT->CYCCNT - start) >= timeout_cycles) {
            return 0U;
        }
    }
    return 1U;
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
    uint32_t primask;
    hdht->status = DHT11_STATUS_TIMEOUT;

    /* Send the DHT11 start pulse, then release DATA and switch to input. */
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = hdht->pin;
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(hdht->port, &gpio);
    HAL_GPIO_WritePin(hdht->port, hdht->pin, GPIO_PIN_RESET);
    HAL_Delay(DHT11_START_LOW_MS);
    HAL_GPIO_WritePin(hdht->port, hdht->pin, GPIO_PIN_SET);

    gpio.Mode = GPIO_MODE_INPUT;
    HAL_GPIO_Init(hdht->port, &gpio);

    /* Keep interrupt latency out of the 40-bit pulse measurement. */
    primask = __get_PRIMASK();
    __disable_irq();
    DHT11_DelayUs(30U);

    /* Sensor response: about 80 us low followed by about 80 us high. */
    if ((DHT11_WaitForLevel(hdht, GPIO_PIN_RESET) == 0U) ||
        (DHT11_WaitForLevel(hdht, GPIO_PIN_SET) == 0U)) {
        hdht->status = DHT11_STATUS_TIMEOUT;
        goto restore_pin;
    }

    for (uint8_t bit = 0U; bit < 40U; ++bit) {
        /* Each data bit starts low; its rising edge starts the high pulse. */
        if ((DHT11_WaitForLevel(hdht, GPIO_PIN_RESET) == 0U) ||
            (DHT11_WaitForLevel(hdht, GPIO_PIN_SET) == 0U)) {
            hdht->status = DHT11_STATUS_TIMEOUT;
            goto restore_pin;
        }

        DHT11_DelayUs(DHT11_BIT_SAMPLE_US);
        data[bit / 8U] <<= 1U;
        if (HAL_GPIO_ReadPin(hdht->port, hdht->pin) == GPIO_PIN_SET) {
            data[bit / 8U] |= 1U;
        }
    }

    /* Wait for the final high pulse to end before taking control of DATA. */
    if (DHT11_WaitForLevel(hdht, GPIO_PIN_RESET) == 0U) {
        hdht->status = DHT11_STATUS_TIMEOUT;
        goto restore_pin;
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
    if (primask == 0U) {
        __enable_irq();
    }
    return result;
}
