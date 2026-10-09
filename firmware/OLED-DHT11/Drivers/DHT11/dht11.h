#ifndef DHT11_H
#define DHT11_H

#include "main.h"
#include <stdint.h>

typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
    uint8_t temperature;
    uint8_t humidity;
    uint8_t status;
} DHT11_HandleTypeDef;

#define DHT11_STATUS_READY           0U
#define DHT11_STATUS_OK              1U
#define DHT11_STATUS_TIMEOUT         2U
#define DHT11_STATUS_CHECKSUM_ERROR  3U
#define DHT11_STATUS_INVALID_HANDLE  4U

void DHT11_Init(DHT11_HandleTypeDef *hdht);
/* Returns 1 after a valid frame, or 0 after a timeout/checksum error. */
uint8_t DHT11_Read(DHT11_HandleTypeDef *hdht);

#endif /* DHT11_H */
