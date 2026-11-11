#ifndef CS43L22_H
#define CS43L22_H

#include "stm32f4xx_hal.h"

/* 8-bit I2C address (7-bit 0x4A shifted left). AD0 is tied low on the board.
*/
#define CS43L22_I2C_ADDR 0x94

HAL_StatusTypeDef CS43L22_Init(I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef CS43L22_PowerOn(void);
HAL_StatusTypeDef CS43L22_SetVolume(int8_t db);


#endif /* CS43L22_H */
