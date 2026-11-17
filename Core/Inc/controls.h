#ifndef CONTROLS_H
#define CONTROLS_H

#include "stm32f4xx_hal.h"

void Controls_Start(TIM_HandleTypeDef *enc_tim, TIM_HandleTypeDef *tick_tim, ADC_HandleTypeDef *adc);

#endif /* CONTROLS_H */
