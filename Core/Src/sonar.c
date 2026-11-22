#include "sonar.h"

/* Sound travels 343 m/s and the echo goes there and back:
   2 x 1 cm / 34300 cm/s = 58 us per cm of distance. */
#define US_PER_CM   58.0f

static TIM_HandleTypeDef *echo_htim;

void Sonar_Start(TIM_HandleTypeDef *trig_tim, TIM_HandleTypeDef *echo_tim)
{
    echo_htim = echo_tim;

    /* TIM2 in PWM-input mode, counting in microseconds:
       the ECHO rising edge resets the counter (channel 1),
       the falling edge copies the count into CCR2 (channel 2).
       So CCR2 = how long ECHO was high = the round trip time. */
    HAL_TIM_IC_Start(echo_htim, TIM_CHANNEL_1);
    HAL_TIM_IC_Start(echo_htim, TIM_CHANNEL_2);

    /* TIM9 sends a 10 us pulse on TRIG every 60 ms, all by itself. */
    HAL_TIM_PWM_Start(trig_tim, TIM_CHANNEL_1);
}

int Sonar_Read(float *cm)
{
    /* The CC2 flag means "a falling edge was captured since you last read CCR2". */
    if (!__HAL_TIM_GET_FLAG(echo_htim, TIM_FLAG_CC2))
        return 0;

    /* Reading CCR2 also clears the flag, ready for the next pulse. */
    uint32_t us = HAL_TIM_ReadCapturedValue(echo_htim, TIM_CHANNEL_2);
    *cm = us / US_PER_CM;
    return 1;
}
