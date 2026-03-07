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
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ICM42688.h"
#include "bmp5.h"
#include "common.h"
#include "usbd_cdc_if.h"
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

SPI_HandleTypeDef hspi1;

/* USER CODE BEGIN PV */
ICM42688_Handle icm;
ICM42688_Data imu_data;

struct bmp5_dev bmp5;
struct bmp5_osr_odr_press_config bmp5_osr_cfg;
struct bmp5_sensor_data bmp5_data;

char serial_buf[256];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void PeriphCommonClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_SPI1_Init(void);
/* USER CODE BEGIN PFP */
static int8_t BMP580_Setup(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick.
   */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* Configure the peripherals common clocks */
  PeriphCommonClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_SPI1_Init();
  MX_USB_Device_Init();
  /* USER CODE BEGIN 2 */
  /* Give USB time to enumerate before any CDC transmits */
  HAL_Delay(2000);

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

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1) {
    if (ICM42688_ReadOnce(&icm, &imu_data) == 0) {
      CDC_Transmit_FS((uint8_t *)"ICM42688 data ready\r\n", 22);
    }

    /* BMP580 – read pressure & temperature (normal mode, DRDY polling) */
    {
      uint8_t int_status = 0;
      if (bmp5_get_interrupt_status(&int_status, &bmp5) == BMP5_OK) {
        if (int_status & BMP5_INT_ASSERTED_DRDY) {
          if (bmp5_get_sensor_data(&bmp5_data, &bmp5_osr_cfg, &bmp5) == BMP5_OK) {
            int len = snprintf(serial_buf, sizeof(serial_buf),
                               "P:%.2f Pa  T:%.2f C\r\n",
                               bmp5_data.pressure, bmp5_data.temperature);
            CDC_Transmit_FS((uint8_t *)serial_buf, len);
          }
        }
      }
    }

    HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
    HAL_Delay(100);
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
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
  RCC_OscInitStruct.OscillatorType =
      RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_HSE | RCC_OSCILLATORTYPE_LSE |
      RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV1;
  RCC_OscInitStruct.PLL.PLLN = 32;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    Error_Handler();
  }

  /** Configure the SYSCLKSource, HCLK, PCLK1 and PCLK2 clocks dividers
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK4 | RCC_CLOCKTYPE_HCLK2 |
                                RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.AHBCLK2Divider = RCC_SYSCLK_DIV2;
  RCC_ClkInitStruct.AHBCLK4Divider = RCC_SYSCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK) {
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
void PeriphCommonClock_Config(void) {
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  /** Initializes the peripherals clock
   */
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_SMPS;
  PeriphClkInitStruct.SmpsClockSelection = RCC_SMPSCLKSOURCE_HSI;
  PeriphClkInitStruct.SmpsDivSelection = RCC_SMPSCLKDIV_RANGE1;

  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN Smps */

  /* USER CODE END Smps */
}

/**
 * @brief I2C1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_I2C1_Init(void) {

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x10B17DB5;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
    Error_Handler();
  }

  /** Configure Analogue filter
   */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK) {
    Error_Handler();
  }

  /** Configure Digital filter
   */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */
}

/**
 * @brief SPI1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_SPI1_Init(void) {

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_HIGH;
  hspi1.Init.CLKPhase = SPI_PHASE_2EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 7;
  hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  if (HAL_SPI_Init(&hspi1) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */
}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(ICM_CS_GPIO_Port, ICM_CS_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : LED_Pin */
  GPIO_InitStruct.Pin = LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : BMP_INT_Pin */
  GPIO_InitStruct.Pin = BMP_INT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(BMP_INT_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : ICM_CS_Pin */
  GPIO_InitStruct.Pin = ICM_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(ICM_CS_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : ICM_INT1_Pin ICM_INT2_Pin */
  GPIO_InitStruct.Pin = ICM_INT1_Pin | ICM_INT2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
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
void Error_Handler(void) {
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
void assert_failed(uint8_t *file, uint32_t line) {
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line
     number, ex: printf("Wrong parameters value: file %s on line %d\r\n", file,
     line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
