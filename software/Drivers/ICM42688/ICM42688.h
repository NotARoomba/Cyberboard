/**
 * @file ICM42688.h
 * @brief ICM-42688-P IMU driver for STM32 HAL (SPI, one-shot mode)
 */

#ifndef ICM42688_H
#define ICM42688_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32wbxx_hal.h"
#include <stdint.h>

/* ───────────────────────── Register Map ───────────────────────── */

/* Accessible from all user banks */
#define ICM42688_REG_BANK_SEL 0x76

/* User Bank 0 */
#define ICM42688_UB0_DEVICE_CONFIG 0x11
#define ICM42688_UB0_INT_CONFIG 0x14
#define ICM42688_UB0_FIFO_CONFIG 0x16
#define ICM42688_UB0_TEMP_DATA1 0x1D
#define ICM42688_UB0_TEMP_DATA0 0x1E
#define ICM42688_UB0_ACCEL_DATA_X1 0x1F
#define ICM42688_UB0_ACCEL_DATA_X0 0x20
#define ICM42688_UB0_ACCEL_DATA_Y1 0x21
#define ICM42688_UB0_ACCEL_DATA_Y0 0x22
#define ICM42688_UB0_ACCEL_DATA_Z1 0x23
#define ICM42688_UB0_ACCEL_DATA_Z0 0x24
#define ICM42688_UB0_GYRO_DATA_X1 0x25
#define ICM42688_UB0_GYRO_DATA_X0 0x26
#define ICM42688_UB0_GYRO_DATA_Y1 0x27
#define ICM42688_UB0_GYRO_DATA_Y0 0x28
#define ICM42688_UB0_GYRO_DATA_Z1 0x29
#define ICM42688_UB0_GYRO_DATA_Z0 0x2A
#define ICM42688_UB0_INT_STATUS 0x2D
#define ICM42688_UB0_PWR_MGMT0 0x4E
#define ICM42688_UB0_GYRO_CONFIG0 0x4F
#define ICM42688_UB0_ACCEL_CONFIG0 0x50
#define ICM42688_UB0_INT_CONFIG1 0x64
#define ICM42688_UB0_INT_SOURCE0 0x65
#define ICM42688_UB0_WHO_AM_I 0x75

/* User Bank 1 */
#define ICM42688_UB1_GYRO_CONFIG_STATIC2 0x0B

/* User Bank 2 */
#define ICM42688_UB2_ACCEL_CONFIG_STATIC2 0x03

/* Expected WHO_AM_I value */
#define ICM42688_WHO_AM_I_VAL 0x47

/* INT_STATUS bits */
#define ICM42688_INT_STATUS_DATA_RDY 0x08

/* ───────────────────────── Enumerations ───────────────────────── */

typedef enum {
  ICM42688_ACCEL_FS_16G = 0x00,
  ICM42688_ACCEL_FS_8G = 0x01,
  ICM42688_ACCEL_FS_4G = 0x02,
  ICM42688_ACCEL_FS_2G = 0x03
} ICM42688_AccelFS;

typedef enum {
  ICM42688_GYRO_FS_2000DPS = 0x00,
  ICM42688_GYRO_FS_1000DPS = 0x01,
  ICM42688_GYRO_FS_500DPS = 0x02,
  ICM42688_GYRO_FS_250DPS = 0x03,
  ICM42688_GYRO_FS_125DPS = 0x04,
  ICM42688_GYRO_FS_62_5DPS = 0x05,
  ICM42688_GYRO_FS_31_25DPS = 0x06,
  ICM42688_GYRO_FS_15_625DPS = 0x07
} ICM42688_GyroFS;

typedef enum {
  ICM42688_ODR_32K = 0x01,
  ICM42688_ODR_16K = 0x02,
  ICM42688_ODR_8K = 0x03,
  ICM42688_ODR_4K = 0x04,
  ICM42688_ODR_2K = 0x05,
  ICM42688_ODR_1K = 0x06,
  ICM42688_ODR_200 = 0x07,
  ICM42688_ODR_100 = 0x08,
  ICM42688_ODR_50 = 0x09,
  ICM42688_ODR_25 = 0x0A,
  ICM42688_ODR_12_5 = 0x0B,
  ICM42688_ODR_6_25 = 0x0C,   /* LP mode only (accel only) */
  ICM42688_ODR_3_125 = 0x0D,  /* LP mode only (accel only) */
  ICM42688_ODR_1_5625 = 0x0E, /* LP mode only (accel only) */
  ICM42688_ODR_500 = 0x0F
} ICM42688_ODR;

/* ───────────────────────── Data Structures ───────────────────────── */

/**
 * @brief One-shot measurement result (converted to engineering units)
 */
typedef struct {
  float acc_x;  /**< Acceleration X [g] */
  float acc_y;  /**< Acceleration Y [g] */
  float acc_z;  /**< Acceleration Z [g] */
  float gyr_x;  /**< Gyroscope X [dps] */
  float gyr_y;  /**< Gyroscope Y [dps] */
  float gyr_z;  /**< Gyroscope Z [dps] */
  float temp_c; /**< Die temperature [°C] */
} ICM42688_Data;

/**
 * @brief Driver handle – keeps HW references and current config
 */
typedef struct {
  SPI_HandleTypeDef *hspi;
  GPIO_TypeDef *cs_port;
  uint16_t cs_pin;
  uint8_t current_bank;
  float accel_scale; /* g per LSB */
  float gyro_scale;  /* dps per LSB */
  ICM42688_AccelFS accel_fs;
  ICM42688_GyroFS gyro_fs;
} ICM42688_Handle;

/* ───────────────────────── Public API ───────────────────────── */

/**
 * @brief  Initialise the ICM-42688-P.
 *         Resets the device, verifies WHO_AM_I, enables accel+gyro in
 *         Low-Noise mode, sets default FS (±16 g / 2000 dps) and 1 kHz ODR.
 *
 * @param  h   Pointer to a handle (caller must fill hspi, cs_port, cs_pin)
 * @retval  0  Success
 * @retval <0  Error code
 */
int ICM42688_Init(ICM42688_Handle *h);

/**
 * @brief  Read WHO_AM_I register.
 * @retval Register value (0x47 expected), or 0 on SPI error.
 */
uint8_t ICM42688_WhoAmI(ICM42688_Handle *h);

/**
 * @brief  Configure accelerometer full-scale range.
 */
int ICM42688_SetAccelFS(ICM42688_Handle *h, ICM42688_AccelFS fs);

/**
 * @brief  Configure gyroscope full-scale range.
 */
int ICM42688_SetGyroFS(ICM42688_Handle *h, ICM42688_GyroFS fs);

/**
 * @brief  Configure accelerometer output data rate.
 */
int ICM42688_SetAccelODR(ICM42688_Handle *h, ICM42688_ODR odr);

/**
 * @brief  Configure gyroscope output data rate.
 */
int ICM42688_SetGyroODR(ICM42688_Handle *h, ICM42688_ODR odr);

/**
 * @brief  Perform a single (one-shot) read of accel, gyro, and temperature.
 *         Waits for data-ready, reads 14 bytes starting at TEMP_DATA1,
 *         converts to engineering units, and fills *data.
 *
 * @param  h     Driver handle
 * @param  data  Output struct
 * @retval  0    Success
 * @retval <0    Error
 */
int ICM42688_ReadOnce(ICM42688_Handle *h, ICM42688_Data *data);

#ifdef __cplusplus
}
#endif

#endif /* ICM42688_H */
