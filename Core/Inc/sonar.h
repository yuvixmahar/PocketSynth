#ifndef SONAR_H
#define SONAR_H

#include "stm32f4xx_hal.h"

void Sonar_Start(TIM_HandleTypeDef *trig_tim, TIM_HandleTypeDef *echo_tim);
int  Sonar_Read(float *cm);    /* 1 = a new distance was put in *cm, 0 = nothing new */

#endif /* SONAR_H */
