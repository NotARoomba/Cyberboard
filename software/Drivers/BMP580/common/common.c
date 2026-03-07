/**
 * @file  common.c
 * @brief BMP5 platform abstraction – STM32 HAL I2C implementation
 */

#include <stdint.h>
#include <stdio.h>
#include "common.h"
#include "bmp5_defs.h"

/******************************************************************************/
/*                       Static helpers                                       */
/******************************************************************************/

/** The HAL handle is stored here by bmp5_interface_init() and retrieved inside
 *  the read / write / delay callbacks via intf_ptr.                          */
static I2C_HandleTypeDef *bmp5_hi2c;

/** 7-bit I2C address (unshifted) – set once by bmp5_interface_init(). */
static uint8_t bmp5_i2c_addr;

/******************************************************************************/
/*                       Read / Write / Delay                                 */
/******************************************************************************/

static BMP5_INTF_RET_TYPE bmp5_i2c_read(uint8_t reg_addr, uint8_t *reg_data,
                                         uint32_t length, void *intf_ptr)
{
    (void)intf_ptr;
    HAL_StatusTypeDef status;
    status = HAL_I2C_Mem_Read(bmp5_hi2c, (uint16_t)(bmp5_i2c_addr << 1),
                              reg_addr, I2C_MEMADD_SIZE_8BIT,
                              reg_data, (uint16_t)length, HAL_MAX_DELAY);
    return (status == HAL_OK) ? BMP5_INTF_RET_SUCCESS : BMP5_E_COM_FAIL;
}

static BMP5_INTF_RET_TYPE bmp5_i2c_write(uint8_t reg_addr,
                                          const uint8_t *reg_data,
                                          uint32_t length, void *intf_ptr)
{
    (void)intf_ptr;
    HAL_StatusTypeDef status;
    status = HAL_I2C_Mem_Write(bmp5_hi2c, (uint16_t)(bmp5_i2c_addr << 1),
                               reg_addr, I2C_MEMADD_SIZE_8BIT,
                               (uint8_t *)reg_data, (uint16_t)length,
                               HAL_MAX_DELAY);
    return (status == HAL_OK) ? BMP5_INTF_RET_SUCCESS : BMP5_E_COM_FAIL;
}

static void bmp5_delay_us(uint32_t period, void *intf_ptr)
{
    (void)intf_ptr;
    /* HAL_Delay works in ms – round up so we never wait less than requested */
    uint32_t ms = (period + 999U) / 1000U;
    if (ms == 0U)
        ms = 1U;
    HAL_Delay(ms);
}

/******************************************************************************/
/*                       Public API                                           */
/******************************************************************************/

int8_t bmp5_interface_init(struct bmp5_dev *bmp5_dev, I2C_HandleTypeDef *hi2c)
{
    if (bmp5_dev == NULL || hi2c == NULL)
        return BMP5_E_NULL_PTR;

    bmp5_hi2c    = hi2c;
    bmp5_i2c_addr = BMP5_I2C_ADDR_PRIM;        /* 0x47 – SDO high / secondary address */

    bmp5_dev->read      = bmp5_i2c_read;
    bmp5_dev->write     = bmp5_i2c_write;
    bmp5_dev->delay_us  = bmp5_delay_us;
    bmp5_dev->intf      = BMP5_I2C_INTF;
    bmp5_dev->intf_ptr  = &bmp5_i2c_addr;

    return BMP5_OK;
}

void bmp5_error_codes_print_result(const char api_name[], int8_t rslt)
{
    if (rslt != BMP5_OK)
    {
        printf("%s\t", api_name);
        switch (rslt)
        {
            case BMP5_E_NULL_PTR:        printf("Error [%d] : Null pointer\r\n", rslt);                   break;
            case BMP5_E_COM_FAIL:        printf("Error [%d] : Communication failure\r\n", rslt);          break;
            case BMP5_E_DEV_NOT_FOUND:   printf("Error [%d] : Device not found\r\n", rslt);               break;
            case BMP5_E_INVALID_CHIP_ID: printf("Error [%d] : Invalid chip id\r\n", rslt);                break;
            case BMP5_E_POWER_UP:        printf("Error [%d] : Power up error\r\n", rslt);                 break;
            case BMP5_E_POR_SOFTRESET:   printf("Error [%d] : Power-on reset/softreset failure\r\n", rslt); break;
            case BMP5_E_INVALID_POWERMODE: printf("Error [%d] : Invalid powermode\r\n", rslt);            break;
            default:                     printf("Error [%d] : Unknown error code\r\n", rslt);             break;
        }
    }
}
