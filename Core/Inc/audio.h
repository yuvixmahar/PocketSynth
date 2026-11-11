#ifndef AUDIO_H
#define AUDIO_H

#include "stm32f4xx_hal.h"

/* Real I2S rate: 86 MHz / (256 * 7) = 47991 Hz, not exactly 48000.
   Using the real value keeps the notes exactly in tune. */
#define AUDIO_SAMPLE_RATE   47991.0f

/* Stereo frames in each half of the DMA buffer: 128 / 47991 = 2.67 ms */
#define AUDIO_HALF_FRAMES   128

HAL_StatusTypeDef Audio_Start(I2S_HandleTypeDef *hi2s);
void Audio_SetFrequency(float hz);

#endif /* AUDIO_H */
