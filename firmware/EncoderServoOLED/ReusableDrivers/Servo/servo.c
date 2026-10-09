#include "servo.h"

HAL_StatusTypeDef Servo_Init(Servo_HandleTypeDef *servo, TIM_HandleTypeDef *htim,
                             uint32_t channel, uint16_t min_pulse_us,
                             uint16_t center_pulse_us, uint16_t max_pulse_us)
{
    if ((servo == NULL) || (htim == NULL) || (min_pulse_us >= center_pulse_us) ||
        (center_pulse_us >= max_pulse_us)) {
        return HAL_ERROR;
    }
    servo->htim = htim;
    servo->channel = channel;
    servo->min_pulse_us = min_pulse_us;
    servo->center_pulse_us = center_pulse_us;
    servo->max_pulse_us = max_pulse_us;
    servo->min_angle = SERVO_MIN_ANGLE;
    servo->center_angle = SERVO_CENTER_ANGLE;
    servo->max_angle = SERVO_MAX_ANGLE;
    servo->current_angle = SERVO_CENTER_ANGLE;

    HAL_StatusTypeDef status = HAL_TIM_PWM_Start(htim, channel);
    if (status == HAL_OK) {
        Servo_SetAngle(servo, servo->center_angle);
    }
    return status;
}

void Servo_SetAngle(Servo_HandleTypeDef *servo, int16_t angle)
{
    if ((servo == NULL) || (servo->htim == NULL)) {
        return;
    }
    if (angle < servo->min_angle) {
        angle = servo->min_angle;
    } else if (angle > servo->max_angle) {
        angle = servo->max_angle;
    }

    /* The configured timer runs at 1 MHz: one compare count is one us. */
    uint32_t pulse_us;
    if (angle <= servo->center_angle) {
        pulse_us = servo->min_pulse_us +
            ((uint32_t)(angle - servo->min_angle) *
             (servo->center_pulse_us - servo->min_pulse_us)) /
            (uint32_t)(servo->center_angle - servo->min_angle);
    } else {
        pulse_us = servo->center_pulse_us +
            ((uint32_t)(angle - servo->center_angle) *
             (servo->max_pulse_us - servo->center_pulse_us)) /
            (uint32_t)(servo->max_angle - servo->center_angle);
    }
    if (pulse_us < servo->min_pulse_us) {
        pulse_us = servo->min_pulse_us;
    } else if (pulse_us > servo->max_pulse_us) {
        pulse_us = servo->max_pulse_us;
    }
    __HAL_TIM_SET_COMPARE(servo->htim, servo->channel, pulse_us);
    servo->current_angle = angle;
}

int16_t Servo_GetAngle(const Servo_HandleTypeDef *servo)
{
    return (servo != NULL) ? servo->current_angle : 0;
}
