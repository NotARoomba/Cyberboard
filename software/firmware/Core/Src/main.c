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
#include "ipcc.h"
#include "rf.h"
#include "rtc.h"
#include "spi.h"
#include "usb_device.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ICM42688.h"
#include "bmp5.h"
#include "kalman.h"
#include "common.h"
#include "usbd_cdc_if.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static void dbg_print(const char *msg)
{
  CDC_Transmit_FS((uint8_t *)msg, strlen(msg));
  HAL_Delay(10);
}
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* Set to 1 to enable BLE characteristic updates, 0 for USB serial logging only */
#define ENABLE_BLE       1
/* Set to 1 to enable USB CDC serial logging, 0 to disable */
#define ENABLE_USB_LOG   1
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
ICM42688_Handle icm;
ICM42688_Data imu_data;

struct bmp5_dev bmp5;
struct bmp5_osr_odr_press_config bmp5_osr_cfg;
struct bmp5_sensor_data bmp5_data;

Kalman_t kf_pitch;
Kalman_t kf_roll;

/* Gyro bias measured at startup (subtracted from every reading) */
float gyro_bias_x = 0.0f, gyro_bias_y = 0.0f, gyro_bias_z = 0.0f;

char serial_buf[256];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void PeriphCommonClock_Config(void);
/* USER CODE BEGIN PFP */
static int8_t BMP580_Setup(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* Forward-declare the BLE update function without pulling in full BLE headers.
   Opcode 0 = IMU_DATA, 1 = BARO_DATA  (matches Custom_STM_Char_Opcode_t). */
extern uint8_t Custom_STM_App_Update_Char(uint8_t CharOpcode, uint8_t *pPayload);
#define BLE_CHAR_IMU   0
#define BLE_CHAR_BARO  1

/* Set to 1 by Custom_APP_Init() once BLE stack + GATT services are ready */
volatile uint8_t ble_ready = 0;

static uint32_t last_sensor_poll = 0;
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  /* Early LED blink pattern for debugging startup hangs.
   * Each stage blinks N times before proceeding.
   * Count the blinks to see where it hangs. */
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();
  /* Config code for STM32_WPAN (HSE Tuning must be done before system clock configuration) */
  MX_APPE_Config();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* Configure the peripherals common clocks */
  PeriphCommonClock_Config();

  /* IPCC initialisation */
  MX_IPCC_Init();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_SPI1_Init();
  MX_USB_Device_Init();
  MX_RTC_Init();
  MX_RF_Init();
  /* USER CODE BEGIN 2 */
  /* Give USB time to enumerate before any CDC transmits */
  HAL_Delay(2000);
  dbg_print("=== Cyberboard startup ===\r\n");
  dbg_print("[OK] HAL, clocks, IPCC, peripherals initialized\r\n");

  dbg_print("[  ] I2C bus scan...\r\n");
  /* I2C bus scan – report every device that ACKs */
  {
    int len = snprintf(serial_buf, sizeof(serial_buf), "I2C scan:\r\n");
    CDC_Transmit_FS((uint8_t *)serial_buf, len);
    HAL_Delay(10);
    uint8_t found = 0;
    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
      if (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(addr << 1), 2, 10) == HAL_OK) {
        len = snprintf(serial_buf, sizeof(serial_buf), "  0x%02X ACK\r\n", addr);
        CDC_Transmit_FS((uint8_t *)serial_buf, len);
        HAL_Delay(10);
        found++;
      }
    }
    if (found == 0) {
      len = snprintf(serial_buf, sizeof(serial_buf), "  No devices found!\r\n");
      CDC_Transmit_FS((uint8_t *)serial_buf, len);
      HAL_Delay(10);
    }
    len = snprintf(serial_buf, sizeof(serial_buf), "I2C scan done (%d found)\r\n", found);
    CDC_Transmit_FS((uint8_t *)serial_buf, len);
    HAL_Delay(10);
  }

  dbg_print("[  ] ICM-42688-PC setup...\r\n");
  /* ICM-42688-PC IMU setup */
  icm.hspi    = &hspi1;
  icm.cs_port = ICM_CS_GPIO_Port;
  icm.cs_pin  = ICM_CS_Pin;

  {
    uint8_t whoami = ICM42688_WhoAmI(&icm);
    int len = snprintf(serial_buf, sizeof(serial_buf),
                       "ICM42688-PC WHO_AM_I: 0x%02X (expect 0x%02X)\r\n",
                       whoami, ICM42688PC_WHOAMI);
    CDC_Transmit_FS((uint8_t *)serial_buf, len);
    HAL_Delay(10);
  }

  int icm_err = ICM42688_Init(&icm);
  {
    int len = snprintf(serial_buf, sizeof(serial_buf),
                       "ICM42688-PC init rc: %d\r\n", icm_err);
    CDC_Transmit_FS((uint8_t *)serial_buf, len);
    HAL_Delay(10);
  }

  /* Startup calibration: average accel (for initial angle) and gyro (for bias).
   * Board must be stationary during this phase. */
  {
    const int cal_samples = 32;
    float sum_pitch = 0.0f, sum_roll = 0.0f;
    float sum_gx = 0.0f, sum_gy = 0.0f, sum_gz = 0.0f;
    int good = 0;
    dbg_print("[  ] Calibrating (hold still)...\r\n");
    for (int i = 0; i < cal_samples; i++) {
      if (ICM42688_ReadOnce(&icm, &imu_data) == 0) {
        sum_pitch += atan2f(-imu_data.acc_x, sqrtf(imu_data.acc_y * imu_data.acc_y + imu_data.acc_z * imu_data.acc_z));
        sum_roll  += atan2f( imu_data.acc_y, sqrtf(imu_data.acc_x * imu_data.acc_x + imu_data.acc_z * imu_data.acc_z));
        sum_gx += imu_data.gyr_x;
        sum_gy += imu_data.gyr_y;
        sum_gz += imu_data.gyr_z;
        good++;
      }
      HAL_Delay(2);
    }
    if (good > 0) {
      float deg = 180.0f / (float)M_PI;
      Kalman_Init(&kf_pitch, (sum_pitch / good) * deg);
      Kalman_Init(&kf_roll,  (sum_roll  / good) * deg);
      gyro_bias_x = sum_gx / good;
      gyro_bias_y = sum_gy / good;
      gyro_bias_z = sum_gz / good;
    } else {
      Kalman_Init(&kf_pitch, 0.0f);
      Kalman_Init(&kf_roll,  0.0f);
    }
  }
  dbg_print("[OK] Kalman filters initialized\r\n");

  dbg_print("[  ] BMP580 setup...\r\n");
  /* BMP580 barometric pressure sensor setup */
  int8_t bmp_err = BMP580_Setup();
  if (bmp_err != BMP5_OK) {
    while (1) {
      int len = snprintf(serial_buf, sizeof(serial_buf),
                         "BMP580 init error: %d\r\n", bmp_err);
      CDC_Transmit_FS((uint8_t *)serial_buf, len);
      HAL_Delay(10);
      for (int i = 0; i < (-bmp_err); i++) {
        HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
        HAL_Delay(100);
        HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
        HAL_Delay(100);
      }
      HAL_Delay(1500);
    }
  }
  /* USER CODE END 2 */

  /* Init code for STM32_WPAN */
  MX_APPE_Init();

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  dbg_print("[OK] Entering main loop\r\n");

  while (1) {
     /* --- Sensor polling (every 50 ms) --- */
    if (HAL_GetTick() - last_sensor_poll >= 50) {
      last_sensor_poll = HAL_GetTick();

      /* IMU */
      if (ICM42688_ReadOnce(&icm, &imu_data) == 0) {
        /* Kalman filter: fuse accel + gyro into stable pitch/roll angles.
         *
         * accel_pitch/roll: tilt angles derived from gravity direction.
         *   - Accurate on average, but noisy and wrong during acceleration.
         * gyro: rotation rate integrated over dt to track angle changes.
         *   - Smooth and responsive, but drifts over time.
         *
         * The Kalman filter blends both to get the best of each. */
        float dt = 0.05f;  /* 50 ms polling interval */

        /* atan2(axis, sqrt(other_two²)) gives tilt that stays valid at all angles.
         * Using just atan2(x, z) breaks down when z→0 (board near 90° tilt). */
        float accel_pitch = atan2f(-imu_data.acc_x, sqrtf(imu_data.acc_y * imu_data.acc_y + imu_data.acc_z * imu_data.acc_z)) * (180.0f / (float)M_PI);
        float accel_roll  = atan2f( imu_data.acc_y, sqrtf(imu_data.acc_x * imu_data.acc_x + imu_data.acc_z * imu_data.acc_z)) * (180.0f / (float)M_PI);

        /* Subtract startup bias from gyro before feeding into the filter */
        float pitch = Kalman_Update(&kf_pitch, imu_data.gyr_y - gyro_bias_y, accel_pitch, dt);
        float roll  = Kalman_Update(&kf_roll,  imu_data.gyr_x - gyro_bias_x, accel_roll,  dt);

#if ENABLE_USB_LOG
        {
          float gx_cor = imu_data.gyr_x - gyro_bias_x;
          float gy_cor = imu_data.gyr_y - gyro_bias_y;
          float gz_cor = imu_data.gyr_z - gyro_bias_z;

          int len = snprintf(serial_buf, sizeof(serial_buf),
                             "A:%.2f,%.2f,%.2f G:%.1f,%.1f,%.1f T:%.1f P:%.2f R:%.2f\r\n",
                             imu_data.acc_x, imu_data.acc_y, imu_data.acc_z,
                             gx_cor, gy_cor, gz_cor,
                             imu_data.temp_c, pitch, roll);
          CDC_Transmit_FS((uint8_t *)serial_buf, len);
        }
#endif

#if ENABLE_BLE
        if (ble_ready) {
          float imu_ble[7] = {
            imu_data.acc_x, imu_data.acc_y, imu_data.acc_z,
            imu_data.gyr_x, imu_data.gyr_y, imu_data.gyr_z,
            imu_data.temp_c
          };
          Custom_STM_App_Update_Char(BLE_CHAR_IMU, (uint8_t *)imu_ble);
        }
#endif
      }

      /* Barometer */
      {
        uint8_t int_status = 0;
        if (bmp5_get_interrupt_status(&int_status, &bmp5) == BMP5_OK) {
          if (int_status & BMP5_INT_ASSERTED_DRDY) {
            if (bmp5_get_sensor_data(&bmp5_data, &bmp5_osr_cfg, &bmp5) == BMP5_OK) {
#if ENABLE_USB_LOG
              int len = snprintf(serial_buf, sizeof(serial_buf),
                                 "P:%.2f Pa  T:%.2f C\r\n",
                                 bmp5_data.pressure, bmp5_data.temperature);
              CDC_Transmit_FS((uint8_t *)serial_buf, len);
#endif

#if ENABLE_BLE
              if (ble_ready) {
                float baro_ble[3] = {
                  (float)bmp5_data.pressure,
                  (float)bmp5_data.temperature,
                  0.0f
                };
                Custom_STM_App_Update_Char(BLE_CHAR_BARO, (uint8_t *)baro_ble);
              }
#endif
            }
          }
        }
      }

      HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
    }
    
    /* USER CODE END WHILE */
    MX_APPE_Process();

    /* USER CODE BEGIN 3 */
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

  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_MEDIUMHIGH);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSI1
                              |RCC_OSCILLATORTYPE_HSE|RCC_OSCILLATORTYPE_LSE
                              |RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV1;
  RCC_OscInitStruct.PLL.PLLN = 32;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the SYSCLKSource, HCLK, PCLK1 and PCLK2 clocks dividers
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK4|RCC_CLOCKTYPE_HCLK2
                              |RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.AHBCLK2Divider = RCC_SYSCLK_DIV2;
  RCC_ClkInitStruct.AHBCLK4Divider = RCC_SYSCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enable MSI Auto calibration
  */
  HAL_RCCEx_EnableMSIPLLMode();
}

/**
  * @brief Peripherals Common Clock Configuration
  * @retval None
  */
void PeriphCommonClock_Config(void)
{
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  /** Initializes the peripherals clock
  */
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_SMPS|RCC_PERIPHCLK_RFWAKEUP;
  PeriphClkInitStruct.RFWakeUpClockSelection = RCC_RFWKPCLKSOURCE_HSE_DIV1024;
  PeriphClkInitStruct.SmpsClockSelection = RCC_SMPSCLKSOURCE_HSI;
  PeriphClkInitStruct.SmpsDivSelection = RCC_SMPSCLKDIV_RANGE1;

  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN Smps */

  /* USER CODE END Smps */
}

/* USER CODE BEGIN 4 */

/**
 * @brief  Initialise BMP580 in normal mode with 50 Hz ODR, IIR filter,
 *         and DRDY interrupt enabled.
 */
static void bmp_log(const char *msg, int8_t rslt)
{
  int len = snprintf(serial_buf, sizeof(serial_buf), "BMP: %s -> %d\r\n", msg, rslt);
  CDC_Transmit_FS((uint8_t *)serial_buf, len);
  HAL_Delay(10);  /* give USB time to flush */
}

static int8_t BMP580_Setup(void)
{
  int8_t rslt;

  /* Platform-level init (wires up HAL I2C read/write/delay) */
  rslt = bmp5_interface_init(&bmp5, &hi2c1);
  bmp_log("interface_init", rslt);
  if (rslt != BMP5_OK) return rslt;

  /* Send soft-reset command manually. Do NOT call bmp5_soft_reset() because
   * it reads INT_STATUS (clearing the POR bit) which bmp5_init() also needs.
   * Instead, write the reset command, wait, and let bmp5_init() consume the
   * POR bit itself. */
  {
    uint8_t rst_cmd = 0xB6;  /* BMP5_SOFT_RESET_CMD */
    rslt = bmp5_set_regs(0x7E /* BMP5_REG_CMD */, &rst_cmd, 1, &bmp5);
    bmp_log("manual_reset_cmd", rslt);
    if (rslt != BMP5_OK) return rslt;
    HAL_Delay(5);  /* datasheet: 2 ms reset time, add margin */
  }

  /* Chip-level init with retry – gives the sensor time to come back */
  for (int attempt = 0; attempt < 3; attempt++) {
    rslt = bmp5_init(&bmp5);
    bmp_log("bmp5_init", rslt);
    if (rslt == BMP5_OK) break;
    HAL_Delay(10);
  }
  if (rslt != BMP5_OK) return rslt;

  /* --- Configure OSR / ODR / pressure enable --- */
  rslt = bmp5_set_power_mode(BMP5_POWERMODE_STANDBY, &bmp5);
  bmp_log("set_power_standby", rslt);
  if (rslt != BMP5_OK) return rslt;

  rslt = bmp5_get_osr_odr_press_config(&bmp5_osr_cfg, &bmp5);
  bmp_log("get_osr_odr_cfg", rslt);
  if (rslt != BMP5_OK) return rslt;

  bmp5_osr_cfg.odr      = BMP5_ODR_50_HZ;
  bmp5_osr_cfg.press_en = BMP5_ENABLE;
  bmp5_osr_cfg.osr_t    = BMP5_OVERSAMPLING_64X;
  bmp5_osr_cfg.osr_p    = BMP5_OVERSAMPLING_4X;

  rslt = bmp5_set_osr_odr_press_config(&bmp5_osr_cfg, &bmp5);
  bmp_log("set_osr_odr_cfg", rslt);
  if (rslt != BMP5_OK) return rslt;

  /* --- IIR filter --- */
  struct bmp5_iir_config iir_cfg;
  iir_cfg.set_iir_t     = BMP5_IIR_FILTER_COEFF_1;
  iir_cfg.set_iir_p     = BMP5_IIR_FILTER_COEFF_1;
  iir_cfg.shdw_set_iir_t = BMP5_ENABLE;
  iir_cfg.shdw_set_iir_p = BMP5_ENABLE;

  rslt = bmp5_set_iir_config(&iir_cfg, &bmp5);
  bmp_log("set_iir_config", rslt);
  if (rslt != BMP5_OK) return rslt;

  /* --- Enable DRDY interrupt --- */
  rslt = bmp5_configure_interrupt(BMP5_PULSED, BMP5_ACTIVE_HIGH,
                                  BMP5_INTR_PUSH_PULL, BMP5_INTR_ENABLE, &bmp5);
  bmp_log("configure_interrupt", rslt);
  if (rslt != BMP5_OK) return rslt;

  struct bmp5_int_source_select int_src = { 0 };
  int_src.drdy_en = BMP5_ENABLE;
  rslt = bmp5_int_source_select(&int_src, &bmp5);
  bmp_log("int_source_select", rslt);
  if (rslt != BMP5_OK) return rslt;

  /* --- Enter normal (continuous) mode --- */
  rslt = bmp5_set_power_mode(BMP5_POWERMODE_NORMAL, &bmp5);
  bmp_log("set_power_normal", rslt);
  return rslt;
}

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
  while (1) {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
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
  /* User can add his own implementation to report the file name and line
     number, ex: printf("Wrong parameters value: file %s on line %d\r\n", file,
     line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
