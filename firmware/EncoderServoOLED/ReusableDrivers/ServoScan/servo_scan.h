#ifndef SERVO_SCAN_H
#define SERVO_SCAN_H

#include "servo.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    Servo_HandleTypeDef *servo;
    bool running;
    int16_t min_angle;
    int16_t max_angle;
    uint16_t step_deg;
    uint32_t interval_ms;
    int8_t direction;
    uint32_t last_tick;
} ServoScan_HandleTypeDef;

void ServoScan_Init(ServoScan_HandleTypeDef *scan, Servo_HandleTypeDef *servo,
                    int16_t min_angle, int16_t max_angle, uint16_t step_deg,
                    uint32_t interval_ms);
/* Pass the current tick from the application; the module never reads a clock. */
void ServoScan_Start(ServoScan_HandleTypeDef *scan, uint32_t now_ms);
void ServoScan_Stop(ServoScan_HandleTypeDef *scan);
void ServoScan_StopAndCenter(ServoScan_HandleTypeDef *scan, int16_t center_angle);
void ServoScan_Update(ServoScan_HandleTypeDef *scan, uint32_t now_ms);
bool ServoScan_IsRunning(const ServoScan_HandleTypeDef *scan);

#endif
