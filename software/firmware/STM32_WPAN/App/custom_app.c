/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    App/custom_app.c
  * @author  MCD Application Team
  * @brief   Custom Example Application (Server)
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
#include "app_common.h"
#include "dbg_trace.h"
#include "ble.h"
#include "custom_app.h"
#include "custom_stm.h"
#include "stm32_seq.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
typedef struct
{
  /* SENSOR_SERVICE */
  uint8_t               Imu_data_Notification_Status;
  uint8_t               Baro_data_Notification_Status;
  /* USER CODE BEGIN CUSTOM_APP_Context_t */

  /* USER CODE END CUSTOM_APP_Context_t */

  uint16_t              ConnectionHandle;
} Custom_App_Context_t;

/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private defines ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macros -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/**
 * START of Section BLE_APP_CONTEXT
 */

static Custom_App_Context_t Custom_App_Context;

/**
 * END of Section BLE_APP_CONTEXT
 */

uint8_t UpdateCharData[512];
uint8_t NotifyCharData[512];
uint16_t Connection_Handle;
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* SENSOR_SERVICE */
static void Custom_Imu_data_Update_Char(void);
static void Custom_Imu_data_Send_Notification(void);
static void Custom_Baro_data_Update_Char(void);
static void Custom_Baro_data_Send_Notification(void);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Functions Definition ------------------------------------------------------*/
void Custom_STM_App_Notification(Custom_STM_App_Notification_evt_t *pNotification)
{
  /* USER CODE BEGIN CUSTOM_STM_App_Notification_1 */

  /* USER CODE END CUSTOM_STM_App_Notification_1 */
  switch (pNotification->Custom_Evt_Opcode)
  {
    /* USER CODE BEGIN CUSTOM_STM_App_Notification_Custom_Evt_Opcode */

    /* USER CODE END CUSTOM_STM_App_Notification_Custom_Evt_Opcode */

    /* SENSOR_SERVICE */
    case CUSTOM_STM_IMU_DATA_READ_EVT:
      /* USER CODE BEGIN CUSTOM_STM_IMU_DATA_READ_EVT */

      /* USER CODE END CUSTOM_STM_IMU_DATA_READ_EVT */
      break;

    case CUSTOM_STM_IMU_DATA_NOTIFY_ENABLED_EVT:
      /* USER CODE BEGIN CUSTOM_STM_IMU_DATA_NOTIFY_ENABLED_EVT */

      /* USER CODE END CUSTOM_STM_IMU_DATA_NOTIFY_ENABLED_EVT */
      break;

    case CUSTOM_STM_IMU_DATA_NOTIFY_DISABLED_EVT:
      /* USER CODE BEGIN CUSTOM_STM_IMU_DATA_NOTIFY_DISABLED_EVT */

      /* USER CODE END CUSTOM_STM_IMU_DATA_NOTIFY_DISABLED_EVT */
      break;

    case CUSTOM_STM_BARO_DATA_READ_EVT:
      /* USER CODE BEGIN CUSTOM_STM_BARO_DATA_READ_EVT */

      /* USER CODE END CUSTOM_STM_BARO_DATA_READ_EVT */
      break;

    case CUSTOM_STM_BARO_DATA_NOTIFY_ENABLED_EVT:
      /* USER CODE BEGIN CUSTOM_STM_BARO_DATA_NOTIFY_ENABLED_EVT */

      /* USER CODE END CUSTOM_STM_BARO_DATA_NOTIFY_ENABLED_EVT */
      break;

    case CUSTOM_STM_BARO_DATA_NOTIFY_DISABLED_EVT:
      /* USER CODE BEGIN CUSTOM_STM_BARO_DATA_NOTIFY_DISABLED_EVT */

      /* USER CODE END CUSTOM_STM_BARO_DATA_NOTIFY_DISABLED_EVT */
      break;

    case CUSTOM_STM_NOTIFICATION_COMPLETE_EVT:
      /* USER CODE BEGIN CUSTOM_STM_NOTIFICATION_COMPLETE_EVT */

      /* USER CODE END CUSTOM_STM_NOTIFICATION_COMPLETE_EVT */
      break;

    default:
      /* USER CODE BEGIN CUSTOM_STM_App_Notification_default */

      /* USER CODE END CUSTOM_STM_App_Notification_default */
      break;
  }
  /* USER CODE BEGIN CUSTOM_STM_App_Notification_2 */

  /* USER CODE END CUSTOM_STM_App_Notification_2 */
  return;
}

void Custom_APP_Notification(Custom_App_ConnHandle_Not_evt_t *pNotification)
{
  /* USER CODE BEGIN CUSTOM_APP_Notification_1 */

  /* USER CODE END CUSTOM_APP_Notification_1 */

  switch (pNotification->Custom_Evt_Opcode)
  {
    /* USER CODE BEGIN CUSTOM_APP_Notification_Custom_Evt_Opcode */

    /* USER CODE END P2PS_CUSTOM_Notification_Custom_Evt_Opcode */
    case CUSTOM_CONN_HANDLE_EVT :
      /* USER CODE BEGIN CUSTOM_CONN_HANDLE_EVT */

      /* USER CODE END CUSTOM_CONN_HANDLE_EVT */
      break;

    case CUSTOM_DISCON_HANDLE_EVT :
      /* USER CODE BEGIN CUSTOM_DISCON_HANDLE_EVT */

      /* USER CODE END CUSTOM_DISCON_HANDLE_EVT */
      break;

    default:
      /* USER CODE BEGIN CUSTOM_APP_Notification_default */

      /* USER CODE END CUSTOM_APP_Notification_default */
      break;
  }

  /* USER CODE BEGIN CUSTOM_APP_Notification_2 */

  /* USER CODE END CUSTOM_APP_Notification_2 */

  return;
}

void Custom_APP_Init(void)
{
  /* USER CODE BEGIN CUSTOM_APP_Init */
  extern volatile uint8_t ble_ready;
  ble_ready = 1;
  /* USER CODE END CUSTOM_APP_Init */
  return;
}

/* USER CODE BEGIN FD */

/* USER CODE END FD */

/*************************************************************
 *
 * LOCAL FUNCTIONS
 *
 *************************************************************/

/* SENSOR_SERVICE */
__USED void Custom_Imu_data_Update_Char(void) /* Property Read */
{
  uint8_t updateflag = 0;

  /* USER CODE BEGIN Imu_data_UC_1*/

  /* USER CODE END Imu_data_UC_1*/

  if (updateflag != 0)
  {
    Custom_STM_App_Update_Char(CUSTOM_STM_IMU_DATA, (uint8_t *)UpdateCharData);
  }

  /* USER CODE BEGIN Imu_data_UC_Last*/

  /* USER CODE END Imu_data_UC_Last*/
  return;
}

void Custom_Imu_data_Send_Notification(void) /* Property Notification */
{
  uint8_t updateflag = 0;

  /* USER CODE BEGIN Imu_data_NS_1*/

  /* USER CODE END Imu_data_NS_1*/

  if (updateflag != 0)
  {
    Custom_STM_App_Update_Char(CUSTOM_STM_IMU_DATA, (uint8_t *)NotifyCharData);
  }

  /* USER CODE BEGIN Imu_data_NS_Last*/

  /* USER CODE END Imu_data_NS_Last*/

  return;
}

__USED void Custom_Baro_data_Update_Char(void) /* Property Read */
{
  uint8_t updateflag = 0;

  /* USER CODE BEGIN Baro_data_UC_1*/

  /* USER CODE END Baro_data_UC_1*/

  if (updateflag != 0)
  {
    Custom_STM_App_Update_Char(CUSTOM_STM_BARO_DATA, (uint8_t *)UpdateCharData);
  }

  /* USER CODE BEGIN Baro_data_UC_Last*/

  /* USER CODE END Baro_data_UC_Last*/
  return;
}

void Custom_Baro_data_Send_Notification(void) /* Property Notification */
{
  uint8_t updateflag = 0;

  /* USER CODE BEGIN Baro_data_NS_1*/

  /* USER CODE END Baro_data_NS_1*/

  if (updateflag != 0)
  {
    Custom_STM_App_Update_Char(CUSTOM_STM_BARO_DATA, (uint8_t *)NotifyCharData);
  }

  /* USER CODE BEGIN Baro_data_NS_Last*/

  /* USER CODE END Baro_data_NS_Last*/

  return;
}

/* USER CODE BEGIN FD_LOCAL_FUNCTIONS*/

/* USER CODE END FD_LOCAL_FUNCTIONS*/
