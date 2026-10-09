/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "oled.h"
#include "mpu6050.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum
{
  DISPLAY_PAGE_ACCEL = 0,
  DISPLAY_PAGE_GYRO
} DisplayPage_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define PAGE_KEY_DEBOUNCE_MS 20U
#define DATA_REFRESH_MS     100U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static uint8_t mpu6050_who_am_i = 0U;
static uint8_t mpu6050_ready = 0U; /* Initialization must succeed before sampling. */
static MPU6050_RawData_t mpu6050_raw;
static int16_t mpu6050_gyro_offset[3] = {0, 0, 0};
static DisplayPage_t display_page = DISPLAY_PAGE_ACCEL;
static uint8_t page_changed = 1U;
static uint8_t data_error = 0U;
static uint32_t last_refresh_tick = 0U;
static GPIO_PinState page_key_last = GPIO_PIN_SET;
static GPIO_PinState page_key_stable = GPIO_PIN_SET;
static uint32_t page_key_change_tick = 0U;
static const char *const AppScreenTitle[2] = {"加速度", "陀螺仪"};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static uint8_t AppInit(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* Axis labels used only by the startup calibration result screen. */
static const char *const AppAxisLabels[3] = {"X", "Y", "Z"};

/* Only a debounced press edge emits an event; release must debounce before rearming. */
static uint8_t AppPageKeyPressed(void)
{
  GPIO_PinState current = HAL_GPIO_ReadPin(PAGE_KEY_GPIO_Port, PAGE_KEY_Pin);
  uint32_t now = HAL_GetTick();

  if (current != page_key_last)
  {
    page_key_last = current;
    page_key_change_tick = now;
  }

  if ((current != page_key_stable) &&
      ((uint32_t)(now - page_key_change_tick) >= PAGE_KEY_DEBOUNCE_MS))
  {
    page_key_stable = current;
    return (current == GPIO_PIN_RESET) ? 1U : 0U;
  }
  return 0U;
}

static void AppShowPage(void)
{
  static const char *const labels[2][3] = {
    {"AX:", "AY:", "AZ:"},
    {"GX:", "GY:", "GZ:"}
  };

  OLED_Clear();
  OLED_ShowText(0U, 0U, AppScreenTitle[display_page]);
  for (uint8_t i = 0U; i < 3U; i++)
  {
    OLED_ShowText(0U, (uint8_t)(2U + (2U * i)), labels[display_page][i]);
  }
}

static void AppShowAxisLabels(void)
{
  for (uint8_t i = 0U; i < 3U; i++)
  {
    OLED_ShowText(0U, (uint8_t)(2U + (2U * i)), AppAxisLabels[i]);
  }
}

/**
  * @brief  Refresh the three values on the selected page.
  * @param  screen Acceleration raw values or gyroscope values minus offsets.
  */
static void AppShowValues(DisplayPage_t screen)
{
  int32_t value[3];

  /* A failed read must not leave stale values looking like current measurements. */
  if (data_error != 0U)
  {
    for (uint8_t i = 0U; i < 3U; i++)
    {
      OLED_ShowText(32U, (uint8_t)(2U + (2U * i)), "   ERR");
    }
    return;
  }

  if (screen == DISPLAY_PAGE_ACCEL)
  {
    value[0] = mpu6050_raw.accel_x;
    value[1] = mpu6050_raw.accel_y;
    value[2] = mpu6050_raw.accel_z;
  }
  else
  {
    /* 校准的含义就是把静止时测到的零偏减掉，这里直接把这一步写出来 */
    value[0] = (int32_t)mpu6050_raw.gyro_x - (int32_t)mpu6050_gyro_offset[0];
    value[1] = (int32_t)mpu6050_raw.gyro_y - (int32_t)mpu6050_gyro_offset[1];
    value[2] = (int32_t)mpu6050_raw.gyro_z - (int32_t)mpu6050_gyro_offset[2];
  }

  for (uint8_t i = 0U; i < 3U; i++)
  {
    OLED_ShowTextNum(32U, (uint8_t)(2U + (2U * i)), value[i], 6U);
  }
}

/**
  * @brief  Show a startup message and optionally hold it on screen.
  * @param  hold_ms Zero returns immediately; otherwise delay in milliseconds.
  *                 Used only during startup, before key polling begins.
  */
static void AppShowMessage(const char *line0, const char *line1, uint32_t hold_ms)
{
  OLED_Clear();
  OLED_ShowText(0U, 0U, line0);
  if (line1 != NULL)
  {
    OLED_ShowText(0U, 2U, line1);
  }

  if (hold_ms != 0U)
  {
    HAL_Delay(hold_ms);
  }
}

/**
  * @brief  Initialize the displays and sensor, then calibrate gyro offsets.
  * @retval 1 when ready to sample; 0 on initialization failure.
  * @note   Keep the failure message visible without blocking in a failure loop.
  *         器件不在线时系统仍然继续运行
  */
static uint8_t AppInit(void)
{
  OLED_Init();
  AppShowMessage("MPU6050", "INIT", 1200U);

  /* Check device acknowledgement and identity before configuring it.
     I2C 读写都带毫秒级超时，器件不在线时不会卡死 */
  if ((MPU6050_IsReady() != HAL_OK) || (MPU6050_ReadWhoAmI(&mpu6050_who_am_i) != HAL_OK))
  {
    AppShowMessage("NO RESPONSE", NULL, 0U);
    return 0U;
  }

  OLED_Clear();
  OLED_ShowText(0U, 0U, "ID:");
  OLED_ShowTextHex(48U, 0U, mpu6050_who_am_i);

  if (mpu6050_who_am_i != MPU6050_WHO_AM_I_ID)
  {
    /* 通信成功但身份不符：可能不是 MPU6050，或寄存器被改写 */
    OLED_ShowText(0U, 2U, "ID MISMATCH");
    return 0U;
  }
  OLED_ShowText(0U, 2U, "DEVICE OK");
  HAL_Delay(1200U);

  /* Wake the device before setting measurement ranges. */
  if (MPU6050_Init() != HAL_OK)
  {
    AppShowMessage("INIT FAILED", NULL, 0U);
    return 0U;
  }

  /* Allow time to put the sensor down before measuring stationary offsets.
     Keep the existing startup calibration sequence and timing. */
  AppShowMessage("CALIBRATING", "KEEP STILL", 500U);
  if (MPU6050_CalibrateGyro() != HAL_OK)
  {
    AppShowMessage("CALIB FAILED", NULL, 0U);
    return 0U;
  }

  MPU6050_GetGyroOffset(mpu6050_gyro_offset);

  /* Show offsets to allow a manual calibration sanity check. */
  OLED_Clear();
  OLED_ShowText(0U, 0U, "CALIB DONE");
  AppShowAxisLabels();
  for (uint8_t i = 0U; i < 3U; i++)
  {
    OLED_ShowTextNum(16U, (uint8_t)(2U + (2U * i)), mpu6050_gyro_offset[i], 6U);
  }
  HAL_Delay(1500U);

  /* The main loop draws the initial ACCEL page once after startup. */
  page_key_last = HAL_GPIO_ReadPin(PAGE_KEY_GPIO_Port, PAGE_KEY_Pin);
  page_key_stable = page_key_last;
  page_key_change_tick = HAL_GetTick();

  return 1U;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C2_Init();
  /* USER CODE BEGIN 2 */
  mpu6050_ready = AppInit();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    if (mpu6050_ready != 0U)
    {
      if (AppPageKeyPressed() != 0U)
      {
        display_page = (display_page == DISPLAY_PAGE_ACCEL)
                         ? DISPLAY_PAGE_GYRO : DISPLAY_PAGE_ACCEL;
        page_changed = 1U;
      }

      if ((page_changed != 0U) ||
          ((uint32_t)(HAL_GetTick() - last_refresh_tick) >= DATA_REFRESH_MS))
      {
        data_error = (MPU6050_ReadRawData(&mpu6050_raw) == HAL_OK) ? 0U : 1U;
        if (page_changed != 0U)
        {
          AppShowPage();
          page_changed = 0U;
        }
        AppShowValues(display_page);
        /* Keep polling the key between refreshes without a blocking delay. */
        last_refresh_tick = HAL_GetTick();
      }
    }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
