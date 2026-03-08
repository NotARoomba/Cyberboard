/**
 * @file ICM42688.c
 * @brief ICM-42688-PC (Tokmas) driver — SPI, one-shot reads.
 *
 * Register map from C48586483 datasheet. This is NOT the TDK ICM-42688-P.
 */

#include "ICM42688.h"
#include <string.h>

/* ───────────────────── SPI helpers ───────────────────── */

static inline void cs_low(ICM42688_Handle *h)
{
  HAL_GPIO_WritePin(h->cs_port, h->cs_pin, GPIO_PIN_RESET);
}

static inline void cs_high(ICM42688_Handle *h)
{
  HAL_GPIO_WritePin(h->cs_port, h->cs_pin, GPIO_PIN_SET);
}

static int write_reg(ICM42688_Handle *h, uint8_t reg, uint8_t val)
{
  uint8_t tx[2] = { reg & 0x7Fu, val };   /* bit 7 = 0 → write */
  uint8_t rx[2];
  cs_low(h);
  HAL_StatusTypeDef st = HAL_SPI_TransmitReceive(h->hspi, tx, rx, 2, 100);
  cs_high(h);
  return (st == HAL_OK) ? 0 : -1;
}

static int read_regs(ICM42688_Handle *h, uint8_t reg, uint8_t count,
                     uint8_t *dest)
{
  uint8_t tx[15 + 1];
  uint8_t rx[15 + 1];

  if (count > 15) return -1;

  memset(tx, 0, count + 1u);
  tx[0] = reg | 0x80u;                    /* bit 7 = 1 → read */

  cs_low(h);
  HAL_StatusTypeDef st =
      HAL_SPI_TransmitReceive(h->hspi, tx, rx, count + 1u, 100);
  cs_high(h);

  if (st != HAL_OK) return -2;
  memcpy(dest, &rx[1], count);
  return 0;
}

static uint8_t read_reg(ICM42688_Handle *h, uint8_t reg)
{
  uint8_t val = 0;
  read_regs(h, reg, 1, &val);
  return val;
}

/* ───────────────────── Public API ───────────────────── */

uint8_t ICM42688_WhoAmI(ICM42688_Handle *h)
{
  return read_reg(h, ICM42688_WHO_AM_I);
}

int ICM42688_Init(ICM42688_Handle *h)
{
  cs_high(h);

  /* Datasheet: first SPI transaction may be incomplete after POR.
     Toggle CS and do a dummy read to synchronise the SPI interface. */
  cs_low(h);
  HAL_Delay(1);
  cs_high(h);
  HAL_Delay(1);
  (void)read_reg(h, 0x00);   /* dummy read */
  HAL_Delay(1);

  /* CTRL1: enable address auto-increment, keep big-endian (default),
     enable INT1 push-pull output */
  write_reg(h, ICM42688_CTRL1, CTRL1_ADDR_AI | CTRL1_BE | CTRL1_INT1_EN);
  HAL_Delay(1);

  /* CTRL2: Accel ±16g, ODR = 896.8 Hz (setting 0011) */
  write_reg(h, ICM42688_CTRL2, ACCEL_FS_16G | ACCEL_ODR_897);

  /* CTRL3: Gyro ±2048 dps, ODR = 896.8 Hz (setting 0011) */
  write_reg(h, ICM42688_CTRL3, GYRO_FS_2048DPS | GYRO_ODR_897);

  /* CTRL5: disable LPF for now (can enable later if needed) */
  write_reg(h, ICM42688_CTRL5, 0x00);

  /* CTRL7: enable accelerometer and gyroscope */
  write_reg(h, ICM42688_CTRL7, CTRL7_AEN | CTRL7_GEN);

  /* Gyro startup time: 150ms + 3/ODR ≈ 153 ms */
  HAL_Delay(200);

  /* Pre-compute scales:
   *   ±16g     → 16 / 32768 g/LSB
   *   ±2048 dps → 2048 / 32768 dps/LSB */
  h->accel_scale = 16.0f   / 32768.0f;
  h->gyro_scale  = 2048.0f / 32768.0f;

  return 0;
}

int ICM42688_ReadOnce(ICM42688_Handle *h, ICM42688_Data *data)
{
  /* Poll STATUS0 for data ready (aDA and gDA) */
  uint8_t status = 0;
  uint32_t t0 = HAL_GetTick();
  while ((status & (STATUS0_ADA | STATUS0_GDA)) != (STATUS0_ADA | STATUS0_GDA)) {
    if (read_regs(h, ICM42688_STATUS0, 1, &status) < 0)
      return -1;
    if ((HAL_GetTick() - t0) > 10)
      return -2;  /* timeout */
  }

  /* Burst-read 14 bytes starting at TEMP_L (0x33):
     temp(2) + accel(6) + gyro(6) */
  uint8_t buf[14];
  if (read_regs(h, ICM42688_TEMP_L, 14, buf) < 0)
    return -3;

  /* Combine bytes. CTRL1.BE=1 → big-endian: H byte first in register order.
     But register layout is L then H (0x33=TEMP_L, 0x34=TEMP_H), so with
     auto-increment we read L first, then H. This is little-endian byte order
     in the buffer regardless of the BE setting (BE controls the byte order
     within each register pair). With BE=1: buf[0]=low, buf[1]=high */
  int16_t raw_t  = (int16_t)((uint16_t)buf[1]  << 8 | buf[0]);
  int16_t raw_ax = (int16_t)((uint16_t)buf[3]  << 8 | buf[2]);
  int16_t raw_ay = (int16_t)((uint16_t)buf[5]  << 8 | buf[4]);
  int16_t raw_az = (int16_t)((uint16_t)buf[7]  << 8 | buf[6]);
  int16_t raw_gx = (int16_t)((uint16_t)buf[9]  << 8 | buf[8]);
  int16_t raw_gy = (int16_t)((uint16_t)buf[11] << 8 | buf[10]);
  int16_t raw_gz = (int16_t)((uint16_t)buf[13] << 8 | buf[12]);

  data->acc_x = raw_ax * h->accel_scale;
  data->acc_y = raw_ay * h->accel_scale;
  data->acc_z = raw_az * h->accel_scale;
  data->gyr_x = raw_gx * h->gyro_scale;
  data->gyr_y = raw_gy * h->gyro_scale;
  data->gyr_z = raw_gz * h->gyro_scale;

  /* Temperature: T = (TEMP_H * 256 + TEMP_L) / 256 °C */
  data->temp_c = (float)raw_t / ICM42688_TEMP_SCALE;

  return 0;
}
