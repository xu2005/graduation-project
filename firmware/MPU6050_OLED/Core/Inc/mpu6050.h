/**
  ******************************************************************************
  * @file    mpu6050.h
  * @brief   MPU6050 驱动（使用 CubeMX 已初始化好的硬件 I2C2）
  ******************************************************************************
  * @note    本阶段实现设备在线检测、WHO_AM_I 读取、唤醒与量程配置、
  *          六轴原始数据读取；姿态解算、FIFO、DMP、中断等留到后续阶段。
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef PROJECT_MPU6050_H
#define PROJECT_MPU6050_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Exported types ------------------------------------------------------------*/
/* 六轴原始数据。MPU6050 的数据寄存器是 big-endian（高字节在前），
   拼成有符号 16 位整数后的物理量纲由量程决定，本阶段不做换算。 */
typedef struct
{
  int16_t accel_x;
  int16_t accel_y;
  int16_t accel_z;
  int16_t gyro_x;
  int16_t gyro_y;
  int16_t gyro_z;
} MPU6050_RawData_t;

/* Exported constants --------------------------------------------------------*/
/* MPU6050 的 7-bit 从机地址：AD0 引脚接地时为 0x68，接高电平则为 0x69。
   STM32 HAL 的 I2C 接口要求传入左移一位后的 8 位地址，
   因此这里只定义 7-bit 地址，统一由宏左移得到，
   全工程不再出现 0x68 / 0xD0 / 0xD1 混用的情况。 */
#define MPU6050_ADDR_7BIT      0x68U
#define MPU6050_ADDR           ((uint16_t)(MPU6050_ADDR_7BIT << 1))

/* 寄存器 */
#define MPU6050_REG_GYRO_CONFIG    0x1BU   /* 陀螺仪量程 */
#define MPU6050_REG_ACCEL_CONFIG   0x1CU   /* 加速度计量程 */
#define MPU6050_REG_ACCEL_XOUT_H   0x3BU   /* 六轴数据起始寄存器 */
#define MPU6050_REG_PWR_MGMT_1     0x6BU   /* 电源管理 1 */
#define MPU6050_REG_WHO_AM_I       0x75U

/* 从 0x3B 起连续读 14 字节：加速度 6 字节 + 温度 2 字节 + 陀螺仪 6 字节。
   一次突发读完才能保证六个轴来自同一采样时刻 */
#define MPU6050_RAW_DATA_LEN       14U

/* PWR_MGMT_1 写入值：清 SLEEP 位唤醒器件，CLKSEL = 1 选择陀螺仪 PLL 作时钟源，
   比复位默认的内部 8 MHz 振荡器稳定 */
#define MPU6050_PWR_MGMT_1_WAKE    0x01U

/* 量程（FS_SEL 位）。当前都用复位默认值，显式写出是为了让配置可见、便于以后改：
   陀螺仪 ±250 °/s -> 131 LSB/(°/s)；加速度计 ±2 g -> 16384 LSB/g */
#define MPU6050_GYRO_FS_250DPS     0x00U
#define MPU6050_ACCEL_FS_2G        0x00U

/* 退出睡眠后器件不保证能立刻接受寄存器写入，这里留一个保守的等待余量。
   具体需要多久应以器件启动规格为准，当前取值未经实测标定 */
#define MPU6050_WAKEUP_DELAY_MS    50U

/* 零偏校准的采样次数与间隔：200 次 × 2 ms 约 0.5 秒，
   实际还要加上每次 I2C 读取本身的耗时 */
#define MPU6050_CALIB_SAMPLES      200U
#define MPU6050_CALIB_INTERVAL_MS  2U

/* WHO_AM_I 的正常返回值，与 MPU6050_ADDR_7BIT 恰好相同，但两者含义无关，
   分别定义是为了避免以后改地址时误改这里的判断条件 */
#define MPU6050_WHO_AM_I_ID    0x68U

/* I2C 操作超时（毫秒），保证设备不在线时函数会返回而不是死等 */
#define MPU6050_I2C_TIMEOUT_MS 100U

/* Exported functions prototypes ---------------------------------------------*/

/**
  * @brief  检查 MPU6050 是否应答（用于区分“器件不在”和“寄存器读失败”）
  * @retval HAL_OK 器件在线，其余值表示无应答或总线错误
  */
HAL_StatusTypeDef MPU6050_IsReady(void);

/**
  * @brief  读取 WHO_AM_I 寄存器（0x75）
  * @param  id 输出参数，读取成功时存放 WHO_AM_I 的值，正常应为 0x68
  * @retval HAL_OK 读取成功，其余值表示失败
  */
HAL_StatusTypeDef MPU6050_ReadWhoAmI(uint8_t *id);

/**
  * @brief  唤醒 MPU6050 并配置六轴量程
  * @retval HAL_OK 配置成功，其余值表示写入失败
  * @note   必须在 MPU6050_IsReady() / MPU6050_ReadWhoAmI() 通过之后调用
  */
HAL_StatusTypeDef MPU6050_Init(void);

/**
  * @brief  读取六轴原始数据（0x3B 起连续 14 字节，一次突发读完）
  * @param  data 输出参数，不能为 NULL
  * @retval HAL_OK 读取成功，其余值表示失败（此时 data 内容不会被修改）
  */
HAL_StatusTypeDef MPU6050_ReadRawData(MPU6050_RawData_t *data);

/**
  * @brief  在静止状态下求陀螺仪三个轴的零偏，结果保存在驱动内部
  * @retval HAL_OK 校准成功；采样过程中任一次读取失败则返回该失败状态
  * @note   调用期间模块必须保持静止，否则平均值里会混进真实的角速度。
  *         不提供"读一次就算零偏"的用法，因为单次采样噪声太大。
  */
HAL_StatusTypeDef MPU6050_CalibrateGyro(void);

/**
  * @brief  取回最近一次校准得到的陀螺仪零偏
  * @param  offset 输出参数，依次为 x / y / z 的零偏，不能为 NULL
  * @note   未校准时三个值都为 0
  */
void MPU6050_GetGyroOffset(int16_t offset[3]);

#ifdef __cplusplus
}
#endif

#endif /* PROJECT_MPU6050_H */