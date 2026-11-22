#ifndef LEDS_H
#define LEDS_H

#include "stm32f4xx_hal.h"

#define LED_COUNT   8

void Leds_Start(TIM_HandleTypeDef *htim);
void Leds_Set(int i, uint8_t r, uint8_t g, uint8_t b);   /* 0..255 each, shown on the next Leds_Show */
void Leds_Show(void);                                    /* send all 8 colours to the LEDs */

#endif /* LEDS_H */
