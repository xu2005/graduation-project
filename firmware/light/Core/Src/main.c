/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : PA0~PA7 sequential LED test
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
void Error_Handler(void);

/* Private variables ---------------------------------------------------------*/
static const uint16_t LED_Pins[8] =
{
    GPIO_PIN_0,
    GPIO_PIN_1,
    GPIO_PIN_2,
    GPIO_PIN_3,
    GPIO_PIN_4,
    GPIO_PIN_5,
    GPIO_PIN_6,
    GPIO_PIN_7
};

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
    uint8_t i;

    /* Reset of all peripherals, Initializes the Flash interface and SysTick */
    HAL_Init();

    /* Configure the system clock */
    SystemClock_Config();

    /* Initialize GPIO */
    MX_GPIO_Init();

    /*
     * LED 为低电平点亮：
     *
     * GPIO_PIN_RESET = LED 亮
     * GPIO_PIN_SET   = LED 灭
     *
     * 上电后先让 PA0~PA7 全部熄灭
     */
    HAL_GPIO_WritePin(
        GPIOA,
        GPIO_PIN_0 |
        GPIO_PIN_1 |
        GPIO_PIN_2 |
        GPIO_PIN_3 |
        GPIO_PIN_4 |
        GPIO_PIN_5 |
        GPIO_PIN_6 |
        GPIO_PIN_7,
        GPIO_PIN_SET
    );

    /* Infinite loop */
    while (1)
    {
        for (i = 0; i < 8; i++)
        {
            /*
             * 当前 LED 点亮
             */
            HAL_GPIO_WritePin(
                GPIOA,
                LED_Pins[i],
                GPIO_PIN_RESET
            );

            HAL_Delay(2000);

            /*
             * 当前 LED 熄灭
             */
            HAL_GPIO_WritePin(
                GPIOA,
                LED_Pins[i],
                GPIO_PIN_SET
            );

            HAL_Delay(500);
        }
    }
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /*
     * 使用内部 HSI 8 MHz 时钟
     * 不使用 PLL
     */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    /*
     * CPU、AHB、APB1、APB2 时钟配置
     */
    RCC_ClkInitStruct.ClockType =
        RCC_CLOCKTYPE_HCLK |
        RCC_CLOCKTYPE_SYSCLK |
        RCC_CLOCKTYPE_PCLK1 |
        RCC_CLOCKTYPE_PCLK2;

    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(
            &RCC_ClkInitStruct,
            FLASH_LATENCY_0) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
  * @brief GPIO Initialization Function
  * @retval None
  */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* Enable GPIOA clock */
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /*
     * PA0~PA7 初始输出高电平
     * 对当前 LED 接法来说就是全部熄灭
     */
    HAL_GPIO_WritePin(
        GPIOA,
        GPIO_PIN_0 |
        GPIO_PIN_1 |
        GPIO_PIN_2 |
        GPIO_PIN_3 |
        GPIO_PIN_4 |
        GPIO_PIN_5 |
        GPIO_PIN_6 |
        GPIO_PIN_7,
        GPIO_PIN_SET
    );

    /*
     * PA0~PA7 配置为：
     *
     * 推挽输出
     * 无上下拉
     * 低速
     */
    GPIO_InitStruct.Pin =
        GPIO_PIN_0 |
        GPIO_PIN_1 |
        GPIO_PIN_2 |
        GPIO_PIN_3 |
        GPIO_PIN_4 |
        GPIO_PIN_5 |
        GPIO_PIN_6 |
        GPIO_PIN_7;

    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

/**
  * @brief Error Handler
  * @retval None
  */
void Error_Handler(void)
{
    __disable_irq();

    while (1)
    {
    }
}

#ifdef USE_FULL_ASSERT

/**
  * @brief Reports the name of the source file and source line number
  *        where the assert_param error has occurred.
  */
void assert_failed(uint8_t *file, uint32_t line)
{
    /* Prevent unused parameter warnings */
    (void)file;
    (void)line;
}

#endif /* USE_FULL_ASSERT */