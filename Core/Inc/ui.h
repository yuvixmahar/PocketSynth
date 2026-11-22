#ifndef UI_H
#define UI_H

#include "stm32f4xx_hal.h"

void UI_Start(I2C_HandleTypeDef *lcd_i2c, TIM_HandleTypeDef *led_tim);
void UI_Update(void);       /* call over and over from the main loop */

#endif /* UI_H */
