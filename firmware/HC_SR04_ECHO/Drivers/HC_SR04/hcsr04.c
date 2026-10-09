#include "hcsr04.h"

#define HCSR04_ECHO_TIMEOUT_MS  40U
#define HCSR04_MIN_ECHO_US      116U
#define HCSR04_MAX_ECHO_US      23200U
#define HCSR04_SOUND_SPEED_AT_ZERO_M_S 331.3f
#define HCSR04_SOUND_SPEED_SLOPE_M_S_PER_C 0.606f
/* Round-trip microseconds and m/s to one-way centimeters. */
#define HCSR04_ECHO_US_TO_CM_DIVISOR 20000.0f

typedef enum
{
  HCSR04_STATE_IDLE = 0U,
  HCSR04_STATE_WAIT_RISING,
  HCSR04_STATE_WAIT_FALLING
} HCSR04_State_t;

static volatile HCSR04_State_t measurement_state = HCSR04_STATE_IDLE;
static uint32_t trigger_tick;
static HCSR04_Config hardware;
static uint32_t capture_flag;
static uint32_t active_channel;
static uint8_t initialized;

volatile uint32_t echo_rising = 0U;
volatile uint32_t echo_falling = 0U;
volatile uint32_t echo_time_us = 0U;
volatile float distance_cm = -1.0f;
volatile uint8_t measurement_ready = 0U;
volatile uint8_t measurement_valid = 0U;
volatile uint8_t measurement_active = 0U;
volatile uint32_t measurement_count = 0U;
volatile uint32_t measurement_error_count = 0U;
volatile uint32_t measurement_timeout_count = 0U;
volatile uint32_t hcsr04_trigger_count = 0U;
volatile uint32_t hcsr04_echo_rise_count = 0U;
volatile uint32_t hcsr04_echo_fall_count = 0U;
volatile uint32_t hcsr04_invalid_count = 0U;
volatile uint32_t hcsr04_last_measurement_ms = 0U;
volatile uint8_t hcsr04_state = HCSR04_DIAG_IDLE;
volatile uint8_t hcsr04_last_result = HCSR04_RESULT_NONE;
volatile float filtered_distance_cm = -1.0f;
volatile uint32_t hcsr04_filter_update_count = 0U;
volatile uint8_t hcsr04_filter_sample_count = 0U;
volatile uint8_t hcsr04_temperature_valid = 0U;
volatile float hcsr04_temperature_c = HCSR04_DEFAULT_TEMP_C;
volatile float hcsr04_temperature_used_c = HCSR04_DEFAULT_TEMP_C;
volatile float hcsr04_sound_speed_m_s =
    HCSR04_SOUND_SPEED_AT_ZERO_M_S +
    HCSR04_SOUND_SPEED_SLOPE_M_S_PER_C * HCSR04_DEFAULT_TEMP_C;

static float filter_samples[3];
static uint8_t filter_next_index;
static uint8_t filter_sample_count;
static volatile float filter_pending_sample;
static volatile uint8_t filter_sample_pending;

static float HCSR04_CalculateSoundSpeed(float temperature_c)
{
  return HCSR04_SOUND_SPEED_AT_ZERO_M_S +
         HCSR04_SOUND_SPEED_SLOPE_M_S_PER_C * temperature_c;
}

void HCSR04_SetTemperature(float temperature_c)
{
  /* This comparison also rejects NaN and infinities. */
  if ((temperature_c >= 0.0f) && (temperature_c <= 50.0f))
  {
    hcsr04_temperature_c = temperature_c;
    hcsr04_temperature_valid = 1U;
  }
}

void DWT_Delay_Init(void)
{
  /* Cortex-M3 DWT cycle counter uses SystemCoreClock. */
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  __DSB();
  __ISB();
}

void delay_us(uint32_t us)
{
  uint32_t start = DWT->CYCCNT;
  uint32_t cycles = (SystemCoreClock / 1000000U) * us;

  while ((uint32_t)(DWT->CYCCNT - start) < cycles)
  {
    __NOP();
  }
}

static void HCSR04_SetRisingPolarity(void)
{
  __HAL_TIM_SET_CAPTUREPOLARITY(hardware.timer, hardware.channel,
                                TIM_INPUTCHANNELPOLARITY_RISING);
}

static float HCSR04_FilterMedian3(float a, float b, float c)
{
  float temporary;

  if (a > b)
  {
    temporary = a;
    a = b;
    b = temporary;
  }
  if (b > c)
  {
    temporary = b;
    b = c;
    c = temporary;
  }
  if (a > b)
  {
    temporary = a;
    a = b;
    b = temporary;
  }

  return b;
}

static void HCSR04_FilterAddSample(float sample)
{
  filter_samples[filter_next_index] = sample;
  filter_next_index++;
  if (filter_next_index >= 3U)
  {
    filter_next_index = 0U;
  }

  if (filter_sample_count < 3U)
  {
    filter_sample_count++;
  }
  hcsr04_filter_sample_count = filter_sample_count;

  if (filter_sample_count == 1U)
  {
    filtered_distance_cm = sample;
  }
  else if (filter_sample_count == 2U)
  {
    filtered_distance_cm = (filter_samples[0] + filter_samples[1]) * 0.5f;
  }
  else
  {
    filtered_distance_cm = HCSR04_FilterMedian3(filter_samples[0],
                                                filter_samples[1],
                                                filter_samples[2]);
  }

  hcsr04_filter_update_count++;
}

static void HCSR04_FilterProcessPending(void)
{
  uint32_t primask;
  float sample;
  uint8_t has_sample = 0U;

  /* Claim the ISR-published raw sample once, then do float math in main context. */
  primask = __get_PRIMASK();
  __disable_irq();
  if (filter_sample_pending != 0U)
  {
    sample = filter_pending_sample;
    filter_sample_pending = 0U;
    has_sample = 1U;
  }
  __set_PRIMASK(primask);

  if (has_sample != 0U)
  {
    HCSR04_FilterAddSample(sample);
  }
}

void HCSR04_FilterReset(void)
{
  uint32_t primask = __get_PRIMASK();

  __disable_irq();
  filter_samples[0] = 0.0f;
  filter_samples[1] = 0.0f;
  filter_samples[2] = 0.0f;
  filter_next_index = 0U;
  filter_sample_count = 0U;
  filter_pending_sample = 0.0f;
  filter_sample_pending = 0U;
  hcsr04_filter_sample_count = 0U;
  filtered_distance_cm = -1.0f;
  __set_PRIMASK(primask);
}

static void HCSR04_Complete(uint8_t valid)
{
  if (valid != 0U)
  {
    hcsr04_temperature_used_c = hcsr04_temperature_c;
    hcsr04_sound_speed_m_s =
        HCSR04_CalculateSoundSpeed(hcsr04_temperature_used_c);
    distance_cm = (float)echo_time_us * hcsr04_sound_speed_m_s /
                  HCSR04_ECHO_US_TO_CM_DIVISOR;
    measurement_valid = 1U;
    measurement_count++;
    hcsr04_last_measurement_ms = HAL_GetTick();
    hcsr04_last_result = HCSR04_RESULT_VALID;
    hcsr04_state = HCSR04_DIAG_OK;
    filter_pending_sample = distance_cm;
    filter_sample_pending = 1U;
  }
  else
  {
    distance_cm = -1.0f;
    measurement_valid = 0U;
    measurement_error_count++;
  }

  HCSR04_SetRisingPolarity();
  measurement_state = HCSR04_STATE_IDLE;
  measurement_active = 0U;
  measurement_ready = 1U;
}

HAL_StatusTypeDef HCSR04_Init(const HCSR04_Config *config)
{
  HAL_StatusTypeDef status;

  if ((config == NULL) || (config->timer == NULL) ||
      (config->timer->Instance == NULL) || (config->trig_port == NULL) ||
      (config->trig_pin == 0U) || (initialized != 0U))
  {
    return HAL_ERROR;
  }

  switch (config->channel)
  {
    case TIM_CHANNEL_1:
      capture_flag = TIM_FLAG_CC1 | TIM_FLAG_CC1OF;
      active_channel = HAL_TIM_ACTIVE_CHANNEL_1;
      break;
    case TIM_CHANNEL_2:
      capture_flag = TIM_FLAG_CC2 | TIM_FLAG_CC2OF;
      active_channel = HAL_TIM_ACTIVE_CHANNEL_2;
      break;
    case TIM_CHANNEL_3:
      capture_flag = TIM_FLAG_CC3 | TIM_FLAG_CC3OF;
      active_channel = HAL_TIM_ACTIVE_CHANNEL_3;
      break;
    case TIM_CHANNEL_4:
      capture_flag = TIM_FLAG_CC4 | TIM_FLAG_CC4OF;
      active_channel = HAL_TIM_ACTIVE_CHANNEL_4;
      break;
    default:
      return HAL_ERROR;
  }

  hardware = *config;
  DWT_Delay_Init();
  HAL_GPIO_WritePin(hardware.trig_port, hardware.trig_pin, GPIO_PIN_RESET);

  echo_rising = 0U;
  echo_falling = 0U;
  echo_time_us = 0U;
  distance_cm = -1.0f;
  measurement_ready = 0U;
  measurement_valid = 0U;
  measurement_active = 0U;
  measurement_count = 0U;
  measurement_error_count = 0U;
  measurement_timeout_count = 0U;
  hcsr04_trigger_count = 0U;
  hcsr04_echo_rise_count = 0U;
  hcsr04_echo_fall_count = 0U;
  hcsr04_invalid_count = 0U;
  hcsr04_last_measurement_ms = 0U;
  hcsr04_state = HCSR04_DIAG_IDLE;
  hcsr04_last_result = HCSR04_RESULT_NONE;
  hcsr04_filter_update_count = 0U;
  hcsr04_temperature_valid = 0U;
  hcsr04_temperature_c = HCSR04_DEFAULT_TEMP_C;
  hcsr04_temperature_used_c = HCSR04_DEFAULT_TEMP_C;
  hcsr04_sound_speed_m_s =
      HCSR04_CalculateSoundSpeed(HCSR04_DEFAULT_TEMP_C);
  measurement_state = HCSR04_STATE_IDLE;
  HCSR04_FilterReset();

  HCSR04_SetRisingPolarity();
  __HAL_TIM_CLEAR_FLAG(hardware.timer, capture_flag);
  status = HAL_TIM_IC_Start_IT(hardware.timer, hardware.channel);
  if (status != HAL_OK)
  {
    return status;
  }
  initialized = 1U;
  return HAL_OK;
}

void HCSR04_Trigger(void)
{
  if ((initialized == 0U) || (measurement_state != HCSR04_STATE_IDLE))
  {
    return;
  }

  HCSR04_SetRisingPolarity();
  __HAL_TIM_CLEAR_FLAG(hardware.timer, capture_flag);

  echo_rising = 0U;
  echo_falling = 0U;
  echo_time_us = 0U;
  distance_cm = -1.0f;
  measurement_ready = 0U;
  measurement_valid = 0U;
  measurement_active = 1U;
  measurement_state = HCSR04_STATE_WAIT_RISING;
  hcsr04_state = HCSR04_DIAG_WAIT_RISING;

  HAL_GPIO_WritePin(hardware.trig_port, hardware.trig_pin, GPIO_PIN_SET);
  delay_us(10U);
  HAL_GPIO_WritePin(hardware.trig_port, hardware.trig_pin, GPIO_PIN_RESET);
  trigger_tick = HAL_GetTick();
  hcsr04_trigger_count++;
}

void HCSR04_Process(void)
{
  uint32_t primask;

  HCSR04_FilterProcessPending();

  if ((measurement_state == HCSR04_STATE_IDLE) ||
      ((uint32_t)(HAL_GetTick() - trigger_tick) < HCSR04_ECHO_TIMEOUT_MS))
  {
    return;
  }

  /* Avoid racing the capture callback when the timeout expires. */
  primask = __get_PRIMASK();
  __disable_irq();
  if ((measurement_state != HCSR04_STATE_IDLE) &&
      ((uint32_t)(HAL_GetTick() - trigger_tick) >= HCSR04_ECHO_TIMEOUT_MS))
  {
    if (measurement_state == HCSR04_STATE_WAIT_RISING)
    {
      hcsr04_state = HCSR04_DIAG_TIMEOUT_RISING;
      hcsr04_last_result = HCSR04_RESULT_TIMEOUT_RISING;
    }
    else
    {
      hcsr04_state = HCSR04_DIAG_TIMEOUT_FALLING;
      hcsr04_last_result = HCSR04_RESULT_TIMEOUT_FALLING;
    }
    measurement_timeout_count++;
    HCSR04_Complete(0U);
  }
  __set_PRIMASK(primask);
}

float HCSR04_GetDistance(void)
{
  return distance_cm;
}

uint8_t HCSR04_IsReady(void)
{
  return measurement_ready;
}

void HCSR04_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
  uint32_t capture;

  if ((initialized == 0U) || (htim != hardware.timer) ||
      (htim->Channel != active_channel))
  {
    return;
  }

  if (measurement_state == HCSR04_STATE_WAIT_RISING)
  {
    capture = HAL_TIM_ReadCapturedValue(htim, hardware.channel);
    echo_rising = capture;
    hcsr04_echo_rise_count++;
    __HAL_TIM_SET_CAPTUREPOLARITY(htim, hardware.channel,
                                  TIM_INPUTCHANNELPOLARITY_FALLING);
    measurement_state = HCSR04_STATE_WAIT_FALLING;
    hcsr04_state = HCSR04_DIAG_WAIT_FALLING;
  }
  else if (measurement_state == HCSR04_STATE_WAIT_FALLING)
  {
    uint16_t pulse_width;

    capture = HAL_TIM_ReadCapturedValue(htim, hardware.channel);
    echo_falling = capture;
    hcsr04_echo_fall_count++;

    /* Unsigned 16-bit subtraction handles one TIM1 counter wrap. */
    pulse_width = (uint16_t)((uint16_t)capture - (uint16_t)echo_rising);
    echo_time_us = pulse_width;

    if ((pulse_width >= HCSR04_MIN_ECHO_US) &&
        (pulse_width <= HCSR04_MAX_ECHO_US))
    {
      HCSR04_Complete(1U);
    }
    else
    {
      hcsr04_invalid_count++;
      hcsr04_state = HCSR04_DIAG_INVALID;
      hcsr04_last_result = HCSR04_RESULT_INVALID;
      HCSR04_Complete(0U);
    }
  }
}
