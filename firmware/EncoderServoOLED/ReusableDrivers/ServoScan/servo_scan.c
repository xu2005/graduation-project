#include "servo_scan.h"
#include <stddef.h>

void ServoScan_Init(ServoScan_HandleTypeDef *scan, Servo_HandleTypeDef *servo,
                    int16_t min_angle, int16_t max_angle, uint16_t step_deg,
                    uint32_t interval_ms)
{
    if (scan == NULL) {
        return;
    }
    scan->servo = servo;
    scan->running = false;
    scan->min_angle = min_angle;
    scan->max_angle = max_angle;
    scan->step_deg = step_deg;
    scan->interval_ms = interval_ms;
    scan->direction = 1;
    scan->last_tick = 0U;
}

void ServoScan_Start(ServoScan_HandleTypeDef *scan, uint32_t now_ms)
{
    if ((scan == NULL) || (scan->servo == NULL) ||
        (scan->min_angle >= scan->max_angle) || (scan->step_deg == 0U) ||
        (scan->interval_ms == 0U)) {
        return;
    }
    scan->direction = (Servo_GetAngle(scan->servo) >= scan->max_angle) ? -1 : 1;
    scan->last_tick = now_ms;
    scan->running = true;
}

void ServoScan_Stop(ServoScan_HandleTypeDef *scan)
{
    if (scan != NULL) {
        scan->running = false;
    }
}

void ServoScan_StopAndCenter(ServoScan_HandleTypeDef *scan, int16_t center_angle)
{
    if (scan == NULL) {
        return;
    }
    scan->running = false;
    Servo_SetAngle(scan->servo, center_angle);
}

void ServoScan_Update(ServoScan_HandleTypeDef *scan, uint32_t now_ms)
{
    if ((scan == NULL) || !scan->running ||
        ((uint32_t)(now_ms - scan->last_tick) < scan->interval_ms)) {
        return;
    }

    scan->last_tick += scan->interval_ms;
    if ((uint32_t)(now_ms - scan->last_tick) >= scan->interval_ms) {
        scan->last_tick = now_ms;
    }

    int16_t current = Servo_GetAngle(scan->servo);
    int32_t next_angle = (int32_t)current +
                         (int32_t)scan->step_deg * scan->direction;
    if (next_angle >= scan->max_angle) {
        next_angle = scan->max_angle;
        scan->direction = -1;
    } else if (next_angle <= scan->min_angle) {
        next_angle = scan->min_angle;
        scan->direction = 1;
    }
    if (next_angle != current) {
        Servo_SetAngle(scan->servo, (int16_t)next_angle);
    }
}

bool ServoScan_IsRunning(const ServoScan_HandleTypeDef *scan)
{
    return (scan != NULL) && scan->running;
}
