#ifndef HCSR04_H
#define HCSR04_H

#include "stm32f1xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HCSR04_MEASUREMENT_INTERVAL_MS 100U
#define HCSR04_DEFAULT_TEMP_C 20.0f

/* The configured timer must count at 1 MHz with a 16-bit period (ARR=65535).
 * GPIO and timer initialization, including the capture IRQ, belong to the app.
 * One HCSR04 instance is supported by this module.
 */
typedef struct
{
  TIM_HandleTypeDef *timer;
  uint32_t channel;
  GPIO_TypeDef *trig_port;
  uint16_t trig_pin;
} HCSR04_Config;

/* hcsr04_state: active capture phase or the last completed outcome. */
typedef enum
{
  HCSR04_DIAG_IDLE = 0U,
  HCSR04_DIAG_WAIT_RISING = 1U,
  HCSR04_DIAG_WAIT_FALLING = 2U,
  HCSR04_DIAG_OK = 3U,
  HCSR04_DIAG_TIMEOUT_RISING = 4U,
  HCSR04_DIAG_TIMEOUT_FALLING = 5U,
  HCSR04_DIAG_INVALID = 6U
} HCSR04_DiagnosticState;

/* hcsr04_last_result remains available during the next measurement. */
typedef enum
{
  HCSR04_RESULT_NONE = 0U,
  HCSR04_RESULT_VALID = 1U,
  HCSR04_RESULT_TIMEOUT_RISING = 2U,
  HCSR04_RESULT_TIMEOUT_FALLING = 3U,
  HCSR04_RESULT_INVALID = 4U
} HCSR04_DiagnosticResult;

/* Values are exported so they can be inspected in a debugger watch window. */
extern volatile uint32_t echo_rising;
extern volatile uint32_t echo_falling;
extern volatile uint32_t echo_time_us;
extern volatile float distance_cm;
extern volatile uint8_t measurement_ready;
extern volatile uint8_t measurement_valid;
extern volatile uint8_t measurement_active;
extern volatile uint32_t measurement_count;
extern volatile uint32_t measurement_error_count;
extern volatile uint32_t measurement_timeout_count;
extern volatile uint32_t hcsr04_trigger_count;
extern volatile uint32_t hcsr04_echo_rise_count;
extern volatile uint32_t hcsr04_echo_fall_count;
extern volatile uint32_t hcsr04_invalid_count;
extern volatile uint32_t hcsr04_last_measurement_ms;
extern volatile uint8_t hcsr04_state;
extern volatile uint8_t hcsr04_last_result;
extern volatile float filtered_distance_cm;
extern volatile uint32_t hcsr04_filter_update_count;
extern volatile uint8_t hcsr04_filter_sample_count;
extern volatile uint8_t hcsr04_temperature_valid;
extern volatile float hcsr04_temperature_c;
extern volatile float hcsr04_temperature_used_c;
extern volatile float hcsr04_sound_speed_m_s;

/* Copies config and starts input capture. Call once after GPIO/timer setup. */
HAL_StatusTypeDef HCSR04_Init(const HCSR04_Config *config);
/* Accepts 0..50 C; other values leave the last valid temperature unchanged. */
void HCSR04_SetTemperature(float temperature_c);
/* Starts one measurement if idle; the trigger pulse lasts 10 us. */
void HCSR04_Trigger(void);
/* Call regularly from the main loop for timeout and median filtering. */
void HCSR04_Process(void);
void HCSR04_FilterReset(void);
/* Raw result in cm, or -1.0f for an invalid/currently pending reading. */
float HCSR04_GetDistance(void);
/* Remains set after completion until the next accepted trigger. */
uint8_t HCSR04_IsReady(void);
/* Forward HAL_TIM_IC_CaptureCallback here from the application. */
void HCSR04_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim);

void DWT_Delay_Init(void);
void delay_us(uint32_t us);

#ifdef __cplusplus
}
#endif

#endif /* HCSR04_H */
