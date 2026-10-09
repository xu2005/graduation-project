/**
  ******************************************************************************
  * @file    mpu6050.c
  * @brief   MPU6050 驱动（硬件 I2C2：PB10 = SCL / PB11 = SDA）
  ******************************************************************************
  * @note    hi2c2 由 CubeMX 生成的 MX_I2C2_Init() 初始化（100 kHz、7-bit、
  *          允许时钟拉伸、无 DMA、无中断），本文件直接复用，不再重复初始化。
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "mpu6050.h"
#include "i2c.h"

/* Private variables ---------------------------------------------------------*/
/* 陀螺仪零偏，单位与原始数据相同（131 LSB/(°/s)），未校准时保持 0 */
static int16_t MPU6050_GyroOffset[3] = {0, 0, 0};

/* Private functions prototypes ----------------------------------------------*/
static int16_t MPU6050_ToInt16(const uint8_t *bytes);

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  把 big-endian 的两个字节拼成一个有符号 16 位数
  */
static int16_t MPU6050_ToInt16(const uint8_t *bytes)
{
  return (int16_t)(((uint16_t)bytes[0] << 8) | (uint16_t)bytes[1]);
}

HAL_StatusTypeDef MPU6050_IsReady(void)
{
  /* Trials = 3 表示最多尝试 3 次，Timeout 是每次等待的超时时间（毫秒）。
     HAL 收到 NACK 后会很快进入下一次尝试，并不存在固定的毫秒级重试间隔；
     多次尝试只是为了不让偶发的总线干扰被误判成器件离线。 */
  return HAL_I2C_IsDeviceReady(&hi2c2, MPU6050_ADDR, 3U, MPU6050_I2C_TIMEOUT_MS);
}

HAL_StatusTypeDef MPU6050_ReadWhoAmI(uint8_t *id)
{
  if (id == NULL)
  {
    return HAL_ERROR;
  }

  return HAL_I2C_Mem_Read(&hi2c2,
                          MPU6050_ADDR,
                          MPU6050_REG_WHO_AM_I,
                          I2C_MEMADD_SIZE_8BIT,
                          id,
                          1U,
                          MPU6050_I2C_TIMEOUT_MS);
}

HAL_StatusTypeDef MPU6050_Init(void)
{
  uint8_t value;
  HAL_StatusTypeDef status;

  value = MPU6050_PWR_MGMT_1_WAKE;
  status = HAL_I2C_Mem_Write(&hi2c2, MPU6050_ADDR, MPU6050_REG_PWR_MGMT_1,
                             I2C_MEMADD_SIZE_8BIT, &value, 1U,
                             MPU6050_I2C_TIMEOUT_MS);
  if (status != HAL_OK)
  {
    return status;
  }

  /* 唤醒后先留出等待余量（依据与不确定性见 mpu6050.h 中的说明）再配置量程 */
  HAL_Delay(MPU6050_WAKEUP_DELAY_MS);

  value = MPU6050_GYRO_FS_250DPS;
  status = HAL_I2C_Mem_Write(&hi2c2, MPU6050_ADDR, MPU6050_REG_GYRO_CONFIG,
                             I2C_MEMADD_SIZE_8BIT, &value, 1U,
                             MPU6050_I2C_TIMEOUT_MS);
  if (status != HAL_OK)
  {
    return status;
  }

  value = MPU6050_ACCEL_FS_2G;
  status = HAL_I2C_Mem_Write(&hi2c2, MPU6050_ADDR, MPU6050_REG_ACCEL_CONFIG,
                             I2C_MEMADD_SIZE_8BIT, &value, 1U,
                             MPU6050_I2C_TIMEOUT_MS);

  return status;
}

HAL_StatusTypeDef MPU6050_ReadRawData(MPU6050_RawData_t *data)
{
  uint8_t buffer[MPU6050_RAW_DATA_LEN];
  HAL_StatusTypeDef status;

  if (data == NULL)
  {
    return HAL_ERROR;
  }

  /* 一次突发读 14 字节，避免分次读取拼出不同采样时刻的数据 */
  status = HAL_I2C_Mem_Read(&hi2c2, MPU6050_ADDR, MPU6050_REG_ACCEL_XOUT_H,
                            I2C_MEMADD_SIZE_8BIT, buffer, MPU6050_RAW_DATA_LEN,
                            MPU6050_I2C_TIMEOUT_MS);
  if (status != HAL_OK)
  {
    return status;
  }

  /* 只有读成功才动输出参数，失败时保留调用者原值 */
  data->accel_x = MPU6050_ToInt16(&buffer[0]);
  data->accel_y = MPU6050_ToInt16(&buffer[2]);
  data->accel_z = MPU6050_ToInt16(&buffer[4]);
  /* buffer[6..7] 是片上温度，本阶段不显示 */
  data->gyro_x = MPU6050_ToInt16(&buffer[8]);
  data->gyro_y = MPU6050_ToInt16(&buffer[10]);
  data->gyro_z = MPU6050_ToInt16(&buffer[12]);

  return HAL_OK;
}

HAL_StatusTypeDef MPU6050_CalibrateGyro(void)
{
  MPU6050_RawData_t data;
  int32_t sum_x = 0;
  int32_t sum_y = 0;
  int32_t sum_z = 0;

  /* 200 次累加，最坏情况 200 × 32768 = 6553600，int32 足够 */
  for (uint16_t i = 0U; i < MPU6050_CALIB_SAMPLES; i++)
  {
    if (MPU6050_ReadRawData(&data) != HAL_OK)
    {
      /* 采样不完整就丢弃结果：宁可零偏为 0，也不要用半截数据算出错误的偏置 */
      MPU6050_GyroOffset[0] = 0;
      MPU6050_GyroOffset[1] = 0;
      MPU6050_GyroOffset[2] = 0;
      return HAL_ERROR;
    }

    sum_x += data.gyro_x;
    sum_y += data.gyro_y;
    sum_z += data.gyro_z;

    HAL_Delay(MPU6050_CALIB_INTERVAL_MS);
  }

  MPU6050_GyroOffset[0] = (int16_t)(sum_x / (int32_t)MPU6050_CALIB_SAMPLES);
  MPU6050_GyroOffset[1] = (int16_t)(sum_y / (int32_t)MPU6050_CALIB_SAMPLES);
  MPU6050_GyroOffset[2] = (int16_t)(sum_z / (int32_t)MPU6050_CALIB_SAMPLES);

  return HAL_OK;
}

void MPU6050_GetGyroOffset(int16_t offset[3])
{
  if (offset == NULL)
  {
    return;
  }

  offset[0] = MPU6050_GyroOffset[0];
  offset[1] = MPU6050_GyroOffset[1];
  offset[2] = MPU6050_GyroOffset[2];
}