#ifndef CONTROLS_H
#define CONTROLS_H

#include "stm32f4xx_hal.h"

/* A snapshot of the controls, for the screen and LEDs. */
typedef struct {
    int   theremin;     /* 1 = theremin mode, 0 = keyboard */
    int   octave;       /* -1, 0 or +1 */
    int   waveform;     /* 0..3, same order as the Waveform enum */
    float attack_s;     /* attack time in seconds */
    float release_s;    /* release time in seconds */
    float echo;         /* echo amount, 0..1 */
    int   hand;         /* 1 = a hand is in range */
    float hand_cm;      /* latest distance */
    float note;         /* theremin MIDI note, with the octave shift */
    float closeness;    /* theremin: 0 = far, 1 = near */
} ControlStatus;

void Controls_Start(TIM_HandleTypeDef *enc_tim, TIM_HandleTypeDef *tick_tim, ADC_HandleTypeDef *adc);
void Controls_GetStatus(ControlStatus *s);

#endif /* CONTROLS_H */
