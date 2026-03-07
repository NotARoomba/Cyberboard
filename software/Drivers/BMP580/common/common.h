/**
 * @file  common.h
 * @brief BMP5 platform (STM32 HAL I2C) abstraction header
 */

#ifndef _BMP5_COMMON_H
#define _BMP5_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include "bmp5.h"
#include "stm32wbxx_hal.h"

/*!
 *  @brief Initialise the bmp5_dev structure for I2C communication via STM32 HAL.
 *
 *  @param[in,out] bmp5_dev : Structure instance of bmp5_dev.
 *  @param[in]     hi2c     : Pointer to an initialised HAL I2C handle.
 *
 *  @return 0 on success, < 0 on failure.
 */
int8_t bmp5_interface_init(struct bmp5_dev *bmp5_dev, I2C_HandleTypeDef *hi2c);

/*!
 *  @brief Prints the execution status of the APIs (via printf / SWO).
 */
void bmp5_error_codes_print_result(const char api_name[], int8_t rslt);

#ifdef __cplusplus
}
#endif

#endif /* _BMP5_COMMON_H */
