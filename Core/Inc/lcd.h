#ifndef LCD_H
#define LCD_H

#include "stm32f4xx_hal.h"

int  LCD_Init(I2C_HandleTypeDef *hi2c);          /* 1 = LCD found, 0 = not connected */
void LCD_PrintLine(int row, const char *text);   /* row 0 or 1, up to 16 characters */

#endif /* LCD_H */
