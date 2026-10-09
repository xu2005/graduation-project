#ifndef SERVO_H
#define SERVO_H

#include "main.h" /* CubeMX HAL types supplied by the target project. */
#include <stdint.h>

/* The caller must configure PWM for a 1 MHz timer counter (1 count = 1 us)
 * and a 20 ms / 50 Hz period. With a 72 MHz timer clock: PSC=71, ARR=19999. */
#define SERVO_MIN_ANGLE       0
#define SERVO_CENTER_ANGLE    90
#define SERVO_MAX_ANGLE       180
#define SERVO_MIN_PULSE_US    500U
#define SERVO_CENTER_PULSE_US 1500U
#define SERVO_MAX_PULSE_US    2500U

typedef struct {
    TIM_HandleTypeDef *htim;
    uint32_t channel;
    uint16_t min_pulse_us;
    uint16_t center_pulse_us;
    uint16_t max_pulse_us;
    int16_t min_angle;
    int16_t center_angle;
    int16_t max_angle;
    int16_t current_angle;
} Servo_HandleTypeDef;

HAL_StatusTypeDef Servo_Init(Servo_HandleTypeDef *servo, TIM_HandleTypeDef *htim,
                             uint32_t channel, uint16_t min_pulse_us,
                             uint16_t center_pulse_us, uint16_t max_pulse_us);
void Servo_SetAngle(Servo_HandleTypeDef *servo, int16_t angle);
int16_t Servo_GetAngle(const Servo_HandleTypeDef *servo);

#endif
