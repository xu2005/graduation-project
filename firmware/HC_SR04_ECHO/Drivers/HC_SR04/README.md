# HC-SR04 Ultrasonic Driver

## Platform

STM32F103C8T6 with the STM32 HAL library.

## Files

- `hcsr04.h`: driver configuration, diagnostic values, and public API.
- `hcsr04.c`: input capture, timeout handling, measurement state machine,
  distance conversion, and three-sample median filter.

The application remains responsible for initializing the GPIO and timer and
for forwarding the HAL input-capture callback to the driver.

## Hardware requirements

- Timer counter frequency: **1 MHz** (one counter tick equals 1 us).
- Timer period: **16 bit**, with ARR set to 65535.
- A configured input-capture channel and its interrupt.
- A configured GPIO output connected to the module TRIG pin.

## API

```c
HAL_StatusTypeDef HCSR04_Init(const HCSR04_Config *config);
void HCSR04_Trigger(void);
void HCSR04_Process(void);
float HCSR04_GetDistance(void);
void HCSR04_SetTemperature(float temperature_c);
```

`HCSR04_Init()` copies the hardware configuration and starts input capture.
Call it once after initializing the timer and GPIO. The current driver supports
one sensor instance.

`HCSR04_Trigger()` starts a measurement when the driver is idle. The application
chooses the trigger interval. Call `HCSR04_Process()` regularly from the main
loop to handle timeouts and update the median filter. `HCSR04_GetDistance()`
returns the latest raw distance in centimeters, or `-1.0f` if no valid reading
is available.

The application may call `HCSR04_SetTemperature()` after a successful DHT11
read. The driver keeps the last accepted value in the 0–50 C range; before the
first accepted value it uses 20 C. Debugger watch variables
`hcsr04_temperature_valid`, `hcsr04_temperature_c`,
`hcsr04_temperature_used_c`, and `hcsr04_sound_speed_m_s` show the cache and
the values used for the most recent valid echo. Distance is calculated from
the 1 MHz echo width as `echo_us * (331.3 + 0.606 * temperature_c) / 20000`
cm before the existing three-sample distance filter. A DHT11 read failure
leaves the cached temperature unchanged.

Example configuration for TIM1 channel 1 and PA0:

```c
const HCSR04_Config config = {
  .timer = &htim1,
  .channel = TIM_CHANNEL_1,
  .trig_port = GPIOA,
  .trig_pin = GPIO_PIN_0
};

if (HCSR04_Init(&config) != HAL_OK)
{
  Error_Handler();
}
```

Forward the capture callback from the application:

```c
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
  HCSR04_TIM_IC_CaptureCallback(htim);
}
```
