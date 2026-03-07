/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32wbxx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LED_Pin GPIO_PIN_13
#define LED_GPIO_Port GPIOC
#define BMP_SCL_Pin GPIO_PIN_8
#define BMP_SCL_GPIO_Port GPIOB
#define BMP_SDA_Pin GPIO_PIN_9
#define BMP_SDA_GPIO_Port GPIOB
#define BMP_INT_Pin GPIO_PIN_0
#define BMP_INT_GPIO_Port GPIOC
#define ICM_CS_Pin GPIO_PIN_1
#define ICM_CS_GPIO_Port GPIOD
#define ICM_SCK_Pin GPIO_PIN_3
#define ICM_SCK_GPIO_Port GPIOB
#define ICM_MISO_Pin GPIO_PIN_4
#define ICM_MISO_GPIO_Port GPIOB
#define ICM_MOSI_Pin GPIO_PIN_5
#define ICM_MOSI_GPIO_Port GPIOB
#define ICM_INT1_Pin GPIO_PIN_6
#define ICM_INT1_GPIO_Port GPIOB
#define ICM_INT2_Pin GPIO_PIN_7
#define ICM_INT2_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
