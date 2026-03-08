/**
 * @file ICM42688.h
 * @brief ICM-42688-PC (Tokmas) IMU driver (SPI, one-shot polling)
 *
 * Register addresses and bit fields from C48586483 datasheet.
 * NOTE: This is NOT the TDK ICM-42688-P — register map is completely different.
 */

#ifndef ICM42688_H
#define ICM42688_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32wbxx_hal.h"
#include <stdint.h>

/* ─────────────── Register Addresses ─────────────── */
/* Flat register map — no bank selection */

#define ICM42688_WHO_AM_I              0x00
#define ICM42688_REVISION_ID           0x01
#define ICM42688_CTRL1                 0x02
#define ICM42688_CTRL2                 0x03
#define ICM42688_CTRL3                 0x04
#define ICM42688_CTRL5                 0x06
#define ICM42688_CTRL7                 0x08
#define ICM42688_CTRL8                 0x09
#define ICM42688_CTRL9                 0x0A

/* Calibration registers (for CTRL9 commands) */
#define ICM42688_CAL1_L                0x0B
#define ICM42688_CAL1_H                0x0C
#define ICM42688_CAL2_L                0x0D
#define ICM42688_CAL2_H                0x0E
#define ICM42688_CAL3_L                0x0F
#define ICM42688_CAL3_H                0x10
#define ICM42688_CAL4_L                0x11
#define ICM42688_CAL4_H                0x12

/* FIFO registers */
#define ICM42688_FIFO_WTM_TH           0x13
#define ICM42688_FIFO_CTRL             0x14
#define ICM42688_FIFO_SMPL_CNT         0x15
#define ICM42688_FIFO_STATUS           0x16
#define ICM42688_FIFO_DATA             0x17

/* Status registers */
#define ICM42688_STATUSINT             0x2D
#define ICM42688_STATUS0               0x2E
#define ICM42688_STATUS1               0x2F

/* Timestamp */
#define ICM42688_TIMESTAMP_LOW         0x30
#define ICM42688_TIMESTAMP_MID         0x31
#define ICM42688_TIMESTAMP_HIGH        0x32

/* Sensor data output (little-endian pairs: L then H) */
#define ICM42688_TEMP_L                0x33
#define ICM42688_TEMP_H                0x34
#define ICM42688_AX_L                  0x35
#define ICM42688_AX_H                  0x36
#define ICM42688_AY_L                  0x37
#define ICM42688_AY_H                  0x38
#define ICM42688_AZ_L                  0x39
#define ICM42688_AZ_H                  0x3A
#define ICM42688_GX_L                  0x3B
#define ICM42688_GX_H                  0x3C
#define ICM42688_GY_L                  0x3D
#define ICM42688_GY_H                  0x3E
#define ICM42688_GZ_L                  0x3F
#define ICM42688_GZ_H                  0x40

/* Misc */
#define ICM42688_COD_STATUS            0x46
#define ICM42688_TAP_STATUS            0x59
#define ICM42688_RESET                 0x60

/* ─────────────── Bit Definitions ─────────────── */

/* CTRL1 (0x02) — default 0x20 (BE=1) */
#define CTRL1_SIM                      (1u << 7)  /* 3-wire SPI */
#define CTRL1_ADDR_AI                  (1u << 6)  /* Address auto-increment */
#define CTRL1_BE                       (1u << 5)  /* Big-endian (default=1) */
#define CTRL1_INT2_EN                  (1u << 4)  /* INT2 push-pull enable */
#define CTRL1_INT1_EN                  (1u << 3)  /* INT1 push-pull enable */
#define CTRL1_FIFO_INT_SEL             (1u << 2)  /* FIFO interrupt to INT1 */
#define CTRL1_SENSOR_DISABLE           (1u << 0)  /* Disable oscillator */

/* CTRL2 (0x03) — Accelerometer settings */
#define CTRL2_AST                      (1u << 7)  /* Accel self-test */
/* aFS [6:4] — Accelerometer full-scale */
#define ACCEL_FS_2G                    (0u << 4)
#define ACCEL_FS_4G                    (1u << 4)
#define ACCEL_FS_8G                    (2u << 4)
#define ACCEL_FS_16G                   (3u << 4)
/* aODR [3:0] — Accelerometer ODR (6DOF mode values) */
#define ACCEL_ODR_7174                 0x00u  /* 7174.4 Hz (6DOF only) */
#define ACCEL_ODR_3587                 0x01u  /* 3587.2 Hz (6DOF only) */
#define ACCEL_ODR_1794                 0x02u  /* 1793.6 Hz (6DOF only) */
#define ACCEL_ODR_897                  0x03u  /* 896.8 Hz */
#define ACCEL_ODR_448                  0x04u  /* 448.4 Hz */
#define ACCEL_ODR_224                  0x05u  /* 224.2 Hz */
#define ACCEL_ODR_112                  0x06u  /* 112.1 Hz */
#define ACCEL_ODR_56                   0x07u  /* 56.05 Hz */
#define ACCEL_ODR_28                   0x08u  /* 28.025 Hz */

/* CTRL3 (0x04) — Gyroscope settings */
#define CTRL3_GST                      (1u << 7)  /* Gyro self-test */
/* gFS [6:4] — Gyroscope full-scale */
#define GYRO_FS_16DPS                  (0u << 4)
#define GYRO_FS_32DPS                  (1u << 4)
#define GYRO_FS_64DPS                  (2u << 4)
#define GYRO_FS_128DPS                 (3u << 4)
#define GYRO_FS_256DPS                 (4u << 4)
#define GYRO_FS_512DPS                 (5u << 4)
#define GYRO_FS_1024DPS                (6u << 4)
#define GYRO_FS_2048DPS                (7u << 4)
/* gODR [3:0] — Gyroscope ODR */
#define GYRO_ODR_7174                  0x00u  /* 7174.4 Hz */
#define GYRO_ODR_3587                  0x01u  /* 3587.2 Hz */
#define GYRO_ODR_1794                  0x02u  /* 1793.6 Hz */
#define GYRO_ODR_897                   0x03u  /* 896.8 Hz */
#define GYRO_ODR_448                   0x04u  /* 448.4 Hz */
#define GYRO_ODR_224                   0x05u  /* 224.2 Hz */
#define GYRO_ODR_112                   0x06u  /* 112.1 Hz */
#define GYRO_ODR_56                    0x07u  /* 56.05 Hz */
#define GYRO_ODR_28                    0x08u  /* 28.025 Hz */

/* CTRL5 (0x06) — LPF settings */
#define CTRL5_GLPF_EN                  (1u << 4)  /* Gyro LPF enable */
#define CTRL5_GLPF_MODE_MASK           (3u << 5)  /* Gyro LPF mode [6:5] */
#define CTRL5_ALPF_EN                  (1u << 0)  /* Accel LPF enable */
#define CTRL5_ALPF_MODE_MASK           (3u << 1)  /* Accel LPF mode [2:1] */

/* CTRL7 (0x08) — Enable sensors */
#define CTRL7_SYNC_SAMPLE              (1u << 7)  /* SyncSample mode */
#define CTRL7_DRDY_DIS                 (1u << 5)  /* Disable DRDY on INT2 */
#define CTRL7_GSN                      (1u << 4)  /* Gyro snooze mode */
#define CTRL7_GEN                      (1u << 1)  /* Gyro enable */
#define CTRL7_AEN                      (1u << 0)  /* Accel enable */

/* CTRL8 (0x09) — Motion detection */
#define CTRL8_CTRL9_HS_TYPE            (1u << 7)  /* CTRL9 handshake via STATUSINT */
#define CTRL8_ACTIVITY_INT_SEL         (1u << 6)  /* Activity detect to INT1 */

/* CTRL9 (0x0A) — Host commands */
#define CTRL9_CMD_ACK                  0x00u
#define CTRL9_CMD_RST_FIFO             0x04u
#define CTRL9_CMD_REQ_FIFO             0x05u
#define CTRL9_CMD_ACCEL_OFFSET         0x09u
#define CTRL9_CMD_GYRO_OFFSET          0x0Au
#define CTRL9_CMD_CONFIGURE_TAP        0x0Cu
#define CTRL9_CMD_CONFIGURE_MOTION     0x0Eu
#define CTRL9_CMD_COPY_USID            0x10u
#define CTRL9_CMD_SET_RPU              0x11u
#define CTRL9_CMD_AHB_CLOCK_GATING     0x12u
#define CTRL9_CMD_ON_DEMAND_CAL        0xA2u
#define CTRL9_CMD_APPLY_GYRO_GAINS     0xAAu

/* STATUSINT (0x2D) */
#define STATUSINT_CTRL9_DONE           (1u << 7)  /* CTRL9 command done */
#define STATUSINT_LOCKED               (1u << 1)  /* Sensor data locked */
#define STATUSINT_AVAIL                (1u << 0)  /* Sensor data available */

/* STATUS0 (0x2E) */
#define STATUS0_GDA                    (1u << 1)  /* Gyro data available */
#define STATUS0_ADA                    (1u << 0)  /* Accel data available */

/* RESET (0x60) */
#define RESET_CMD                      0xB0u  /* Write to trigger soft reset */

/* WHO_AM_I expected value */
#define ICM42688PC_WHOAMI              0x05u

/* Temperature: T_degC = (TEMP_H * 256 + TEMP_L) / 256 */
#define ICM42688_TEMP_SCALE            256.0f

/* ─────────────── Data Types ─────────────── */

typedef struct {
  float acc_x, acc_y, acc_z;   /* [g]   */
  float gyr_x, gyr_y, gyr_z;  /* [dps] */
  float temp_c;                /* [°C]  */
} ICM42688_Data;

typedef struct {
  SPI_HandleTypeDef *hspi;
  GPIO_TypeDef      *cs_port;
  uint16_t           cs_pin;
  float              accel_scale;  /* g/LSB   */
  float              gyro_scale;   /* dps/LSB */
} ICM42688_Handle;

/* ─────────────── Public API ─────────────── */

/**
 * @brief  Initialise ICM-42688-PC.
 *         Configures ±16g / ±2048dps / 896.8 Hz ODR, enables auto-increment.
 * @param  h  Handle (caller fills hspi, cs_port, cs_pin before calling)
 * @retval 0 on success, <0 on error
 */
int ICM42688_Init(ICM42688_Handle *h);

/**
 * @brief  Read WHO_AM_I register.
 * @retval Register value (0x05 for ICM-42688-PC), or 0 on SPI error.
 */
uint8_t ICM42688_WhoAmI(ICM42688_Handle *h);

/**
 * @brief  One-shot read of accel, gyro, and temperature.
 *         Polls data-ready, burst-reads 14 bytes, converts to engineering units.
 * @retval 0 on success, <0 on error
 */
int ICM42688_ReadOnce(ICM42688_Handle *h, ICM42688_Data *data);

#ifdef __cplusplus
}
#endif

#endif /* ICM42688_H */
