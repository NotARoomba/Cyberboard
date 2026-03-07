/**
 * @file ICM42688.c
 * @brief ICM-42688-P IMU driver – pure C, STM32 HAL SPI, one-shot reads
 */

#include "ICM42688.h"
#include <string.h>

/* ───────────────────── Private helpers ───────────────────── */

/** CS low (select) */
static inline void cs_low(ICM42688_Handle *h) {
  HAL_GPIO_WritePin(h->cs_port, h->cs_pin, GPIO_PIN_RESET);
}

/** CS high (deselect) */
static inline void cs_high(ICM42688_Handle *h) {
  HAL_GPIO_WritePin(h->cs_port, h->cs_pin, GPIO_PIN_SET);
}

/**
 * @brief Write a single register
 * @retval 0 on success, <0 on error
 */
static int write_reg(ICM42688_Handle *h, uint8_t reg, uint8_t val) {
  uint8_t tx[2] = {reg & 0x7F, val}; /* bit 7 = 0 → write */

  cs_low(h);
  HAL_StatusTypeDef st = HAL_SPI_Transmit(h->hspi, tx, 2, 100);
  cs_high(h);

  if (st != HAL_OK)
    return -1;

  /* read-back verify */
  HAL_Delay(10);
  uint8_t rd = 0;
  uint8_t tx_rd[2] = {reg | 0x80, 0x00};
  uint8_t rx_rd[2] = {0};

  cs_low(h);
  st = HAL_SPI_TransmitReceive(h->hspi, tx_rd, rx_rd, 2, 100);
  cs_high(h);

  if (st != HAL_OK)
    return -2;
  rd = rx_rd[1];

  return (rd == val) ? 0 : -3;
}

/**
 * @brief Read one or more registers into dest[]
 * @retval 0 on success, <0 on error
 */
static int read_regs(ICM42688_Handle *h, uint8_t reg, uint8_t count,
                     uint8_t *dest) {
  uint8_t tx_buf[15 + 1]; /* max burst we ever need */
  uint8_t rx_buf[15 + 1];

  if (count > 15)
    return -1;

  memset(tx_buf, 0, count + 1);
  tx_buf[0] = reg | 0x80; /* bit 7 = 1 → read */

  cs_low(h);
  HAL_StatusTypeDef st =
      HAL_SPI_TransmitReceive(h->hspi, tx_buf, rx_buf, count + 1, 100);
  cs_high(h);

  if (st != HAL_OK)
    return -2;

  memcpy(dest, &rx_buf[1], count);
  return 0;
}

/**
 * @brief Switch register bank (0-4).  Skips write if already selected.
 */
static int set_bank(ICM42688_Handle *h, uint8_t bank) {
  if (h->current_bank == bank)
    return 0;
  h->current_bank = bank;
  return write_reg(h, ICM42688_REG_BANK_SEL, bank);
}

/* ───────────────────── Public API ───────────────────── */

uint8_t ICM42688_WhoAmI(ICM42688_Handle *h) {
  set_bank(h, 0);
  uint8_t val = 0;
  if (read_regs(h, ICM42688_UB0_WHO_AM_I, 1, &val) < 0)
    return 0;
  return val;
}

int ICM42688_SetAccelFS(ICM42688_Handle *h, ICM42688_AccelFS fs) {
  set_bank(h, 0);
  uint8_t reg;
  if (read_regs(h, ICM42688_UB0_ACCEL_CONFIG0, 1, &reg) < 0)
    return -1;
  reg = ((uint8_t)fs << 5) | (reg & 0x1F);
  if (write_reg(h, ICM42688_UB0_ACCEL_CONFIG0, reg) < 0)
    return -2;

  /* pre-compute scale: full-scale / 32768 */
  h->accel_scale = (float)(1 << (4 - (uint8_t)fs)) / 32768.0f;
  h->accel_fs = fs;
  return 0;
}

int ICM42688_SetGyroFS(ICM42688_Handle *h, ICM42688_GyroFS fs) {
  set_bank(h, 0);
  uint8_t reg;
  if (read_regs(h, ICM42688_UB0_GYRO_CONFIG0, 1, &reg) < 0)
    return -1;
  reg = ((uint8_t)fs << 5) | (reg & 0x1F);
  if (write_reg(h, ICM42688_UB0_GYRO_CONFIG0, reg) < 0)
    return -2;

  h->gyro_scale = (2000.0f / (float)(1 << (uint8_t)fs)) / 32768.0f;
  h->gyro_fs = fs;
  return 0;
}

int ICM42688_SetAccelODR(ICM42688_Handle *h, ICM42688_ODR odr) {
  set_bank(h, 0);
  uint8_t reg;
  if (read_regs(h, ICM42688_UB0_ACCEL_CONFIG0, 1, &reg) < 0)
    return -1;
  reg = (uint8_t)odr | (reg & 0xF0);
  if (write_reg(h, ICM42688_UB0_ACCEL_CONFIG0, reg) < 0)
    return -2;
  return 0;
}

int ICM42688_SetGyroODR(ICM42688_Handle *h, ICM42688_ODR odr) {
  set_bank(h, 0);
  uint8_t reg;
  if (read_regs(h, ICM42688_UB0_GYRO_CONFIG0, 1, &reg) < 0)
    return -1;
  reg = (uint8_t)odr | (reg & 0xF0);
  if (write_reg(h, ICM42688_UB0_GYRO_CONFIG0, reg) < 0)
    return -2;
  return 0;
}

int ICM42688_Init(ICM42688_Handle *h) {
  /* make sure CS starts de-asserted */
  cs_high(h);
  h->current_bank = 0;

  /* ── Slow SPI to ≤1 MHz for register configuration ── */
  uint32_t saved_prescaler = h->hspi->Init.BaudRatePrescaler;
  h->hspi->Init.BaudRatePrescaler =
      SPI_BAUDRATEPRESCALER_128; /* 64MHz/128 = 500kHz */
  HAL_SPI_Init(h->hspi);

  /* ── Toggle CS to latch SPI mode (chip defaults to I2C if CS is high) ── */
  cs_low(h);
  HAL_Delay(1);
  cs_high(h);
  HAL_Delay(1);

  /* ── Dummy read to fully activate SPI interface ── */
  {
    uint8_t tx[2] = {0x75 | 0x80, 0x00}; /* read WHO_AM_I register */
    uint8_t rx[2] = {0};
    cs_low(h);
    HAL_SPI_TransmitReceive(h->hspi, tx, rx, 2, 100);
    cs_high(h);
    HAL_Delay(1);
  }

  /* software reset */
  /* Don't verify reset write — the bit self-clears */
  {
    uint8_t tx[2] = {ICM42688_UB0_DEVICE_CONFIG & 0x7F, 0x01};
    cs_low(h);
    HAL_SPI_Transmit(h->hspi, tx, 2, 100);
    cs_high(h);
  }
  HAL_Delay(50); /* wait for reset to complete */

  /* After reset, re-latch SPI mode */
  cs_low(h);
  HAL_Delay(1);
  cs_high(h);
  HAL_Delay(1);

  /* After reset, bank is 0 */
  h->current_bank = 0;

  /* verify WHO_AM_I (retry a few times) */
  uint8_t wai = 0;
  for (int i = 0; i < 5; i++) {
    wai = ICM42688_WhoAmI(h);
    if (wai == ICM42688_WHO_AM_I_VAL)
      break;
    HAL_Delay(10);
  }
  if (wai != ICM42688_WHO_AM_I_VAL) {
    return -1;
  }

  /* turn on accel + gyro in Low Noise mode */
  if (write_reg(h, ICM42688_UB0_PWR_MGMT0, 0x0F) < 0) {
    return -2;
  }
  HAL_Delay(1); /* wait for sensors to stabilise */

  /* default full-scale: ±16 g, 2000 dps */
  if (ICM42688_SetAccelFS(h, ICM42688_ACCEL_FS_16G) < 0)
    return -3;
  if (ICM42688_SetGyroFS(h, ICM42688_GYRO_FS_2000DPS) < 0)
    return -4;

  /* default ODR: 1 kHz */
  if (ICM42688_SetAccelODR(h, ICM42688_ODR_1K) < 0)
    return -5;
  if (ICM42688_SetGyroODR(h, ICM42688_ODR_1K) < 0)
    return -6;

  /* disable inner filters (AAF + Notch) for lowest latency */
  set_bank(h, 1);
  write_reg(h, ICM42688_UB1_GYRO_CONFIG_STATIC2,
            0x03); /* NF disable | AAF disable */
  set_bank(h, 2);
  write_reg(h, ICM42688_UB2_ACCEL_CONFIG_STATIC2, 0x01); /* AAF disable */
  set_bank(h, 0);

  /* ── Restore fast SPI for data reads ── */
  h->hspi->Init.BaudRatePrescaler = saved_prescaler;
  HAL_SPI_Init(h->hspi);

  return 0;
}

int ICM42688_ReadOnce(ICM42688_Handle *h, ICM42688_Data *data) {
  set_bank(h, 0);

  /* ── Wait for data-ready (INT_STATUS bit 3) ── */
  uint8_t status = 0;
  uint32_t start = HAL_GetTick();
  while (!(status & ICM42688_INT_STATUS_DATA_RDY)) {
    if (read_regs(h, ICM42688_UB0_INT_STATUS, 1, &status) < 0)
      return -1;
    if ((HAL_GetTick() - start) > 10)
      return -2; /* 10 ms timeout */
  }

  /* ── Burst-read 14 bytes: temp(2) + accel(6) + gyro(6) ── */
  uint8_t buf[14];
  if (read_regs(h, ICM42688_UB0_TEMP_DATA1, 14, buf) < 0)
    return -3;

  /* combine bytes → signed 16-bit (big-endian on wire) */
  int16_t raw_t = (int16_t)((uint16_t)buf[0] << 8 | buf[1]);
  int16_t raw_ax = (int16_t)((uint16_t)buf[2] << 8 | buf[3]);
  int16_t raw_ay = (int16_t)((uint16_t)buf[4] << 8 | buf[5]);
  int16_t raw_az = (int16_t)((uint16_t)buf[6] << 8 | buf[7]);
  int16_t raw_gx = (int16_t)((uint16_t)buf[8] << 8 | buf[9]);
  int16_t raw_gy = (int16_t)((uint16_t)buf[10] << 8 | buf[11]);
  int16_t raw_gz = (int16_t)((uint16_t)buf[12] << 8 | buf[13]);

  /* scale to engineering units */
  data->acc_x = raw_ax * h->accel_scale;
  data->acc_y = raw_ay * h->accel_scale;
  data->acc_z = raw_az * h->accel_scale;
  data->gyr_x = raw_gx * h->gyro_scale;
  data->gyr_y = raw_gy * h->gyro_scale;
  data->gyr_z = raw_gz * h->gyro_scale;
  data->temp_c = (float)raw_t / 132.48f + 25.0f;

  return 0;
}
