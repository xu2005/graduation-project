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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "encoder.h"
#include "key.h"
#include "servo.h"
#include "servo_scan.h"
#include "oled.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define AUTO_MIN_ANGLE        0
#define AUTO_MAX_ANGLE        180
#define AUTO_STEP_DEG         1
#define AUTO_INTERVAL_MS      30U
#define AUTO_OLED_INTERVAL_MS 90U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim2;

/* USER CODE BEGIN PV */
static OLED_HandleTypeDef oled;
static Servo_HandleTypeDef servo;
static ServoScan_HandleTypeDef scanner;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void Display_Mode(void)
{
  OLED_ClearLine(&oled, 2U);
  OLED_SetCursor(&oled, 2U, 0U);
  OLED_DrawString(&oled, ServoScan_IsRunning(&scanner) ? "Mode: AUTO" : "Mode: MANUAL");
}

static void Display_Angle(int16_t angle)
{
  char line[24];
  (void)snprintf(line, sizeof(line), "Angle: %3u deg", (unsigned int)angle);
  OLED_ClearLine(&oled, 4U);
  OLED_SetCursor(&oled, 4U, 0U);
  OLED_DrawString(&oled, line);
}

static void Display_Count(int32_t count)
{
  char line[24];
  (void)snprintf(line, sizeof(line), "Count: %3ld", (long)count);
  OLED_ClearLine(&oled, 6U);
  OLED_SetCursor(&oled, 6U, 0U);
  OLED_DrawString(&oled, line);
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
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */
  OLED_Config(&oled, OLED_SCL_GPIO_Port, OLED_SCL_Pin,
              OLED_SDA_GPIO_Port, OLED_SDA_Pin, 0x3CU);
  OLED_Init(&oled);
  Encoder_Init();
  Key_Init();
  if (Servo_Init(&servo, &htim2, TIM_CHANNEL_2,
                 SERVO_MIN_PULSE_US, SERVO_CENTER_PULSE_US,
                 SERVO_MAX_PULSE_US) != HAL_OK) {
    Error_Handler();
  }
  ServoScan_Init(&scanner, &servo, AUTO_MIN_ANGLE, AUTO_MAX_ANGLE,
                 AUTO_STEP_DEG, AUTO_INTERVAL_MS);

  Servo_SetAngle(&servo, SERVO_CENTER_ANGLE);
  int32_t encoder_count = 0;
  bool angle_pending = false;
  bool count_pending = false;
  uint32_t oled_last_tick = HAL_GetTick();
  OLED_SetCursor(&oled, 0U, 0U);
  OLED_DrawString(&oled, "Encoder Servo");
  Display_Mode();
  Display_Angle(Servo_GetAngle(&servo));
  Display_Count(encoder_count);
  OLED_Update(&oled);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    uint32_t now = HAL_GetTick();
    bool stopped_this_cycle = false;

    if (Key_WasPressed()) {
      if (ServoScan_IsRunning(&scanner)) {
        ServoScan_StopAndCenter(&scanner, SERVO_CENTER_ANGLE);
        stopped_this_cycle = true;
        angle_pending = true;
      } else {
        ServoScan_Start(&scanner, now);
      }
      Display_Mode();
    }

    int8_t step;
    while ((step = Encoder_GetStep()) != 0) {
      encoder_count = Encoder_GetCount();
      count_pending = true;
      if (!ServoScan_IsRunning(&scanner) && !stopped_this_cycle) {
        int16_t current_angle = Servo_GetAngle(&servo);
        int16_t next_angle = current_angle + (int16_t)step * 5;
        if (next_angle < (int16_t)SERVO_MIN_ANGLE) {
          next_angle = SERVO_MIN_ANGLE;
        } else if (next_angle > (int16_t)SERVO_MAX_ANGLE) {
          next_angle = SERVO_MAX_ANGLE;
        }
        if (next_angle != current_angle) {
          Servo_SetAngle(&servo, next_angle);
          angle_pending = true;
        }
      }
    }

    now = HAL_GetTick();
    int16_t angle_before_scan = Servo_GetAngle(&servo);
    ServoScan_Update(&scanner, now);
    if (Servo_GetAngle(&servo) != angle_before_scan) {
      angle_pending = true;
    }

    if (!ServoScan_IsRunning(&scanner) ||
        (uint32_t)(now - oled_last_tick) >= AUTO_OLED_INTERVAL_MS) {
      if (angle_pending) {
        Display_Angle(Servo_GetAngle(&servo));
        angle_pending = false;
      }
      if (count_pending) {
        Display_Count(encoder_count);
        count_pending = false;
      }
      oled_last_tick = now;
    }
    OLED_UpdateSlice(&oled);
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

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 71;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 19999;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 1500;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
  HAL_TIM_MspPostInit(&htim2);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8|GPIO_PIN_9, GPIO_PIN_SET);

  /*Configure GPIO pins : ENC_A_Pin ENC_B_Pin KEY_Pin */
  GPIO_InitStruct.Pin = ENC_A_Pin|ENC_B_Pin|KEY_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PB8 PB9 */
  GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
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
