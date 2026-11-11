#include "audio.h"
#include <math.h>

#define WAVETABLE_SIZE   1024
#define BUFFER_SAMPLES   (AUDIO_HALF_FRAMES * 2 * 2)   /* 2 halves x 2 channels (L, R) */
#define AMPLITUDE        8192.0f                       /* 1/4 of full scale (32767) */

static int16_t  audio_buf[BUFFER_SAMPLES];   /* what the DMA sends: L, R, L, R, ... */
static float    sine_table[WAVETABLE_SIZE];  /* one cycle of a sine, from -1 to +1 */

static uint32_t          phase;       /* where we are in the wave: 0 .. 2^32-1   */
static volatile uint32_t phase_inc;   /* how far to move per sample (sets pitch)  */

static void fill(int16_t *dst)
{
    for (int i = 0; i < AUDIO_HALF_FRAMES; i++)
    {
        float s = sine_table[phase >> 22];   /* top 10 bits -> index 0..1023 */
        phase += phase_inc;                  /* step forward; wraps on its own */

        int16_t out = (int16_t)(s * AMPLITUDE);
        dst[2 * i]     = out;                /* left  */
        dst[2 * i + 1] = out;                /* right */
    }
}

/* DMA finished sending the first half: refill it while the second half plays. */
void HAL_I2S_TxHalfCpltCallback(I2S_HandleTypeDef *hi2s)
{
    if (hi2s->Instance == SPI3)
        fill(&audio_buf[0]);
}

/* DMA finished the second half (and wrapped to the start): refill the second half. */
void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef *hi2s)
{
    if (hi2s->Instance == SPI3)
        fill(&audio_buf[BUFFER_SAMPLES / 2]);
}

void Audio_SetFrequency(float hz)
{
    /* phase_inc = hz / sample_rate * 2^32 */
    phase_inc = (uint32_t)(hz / AUDIO_SAMPLE_RATE * 4294967296.0f);
}

HAL_StatusTypeDef Audio_Start(I2S_HandleTypeDef *hi2s)
{
    /* Build the sine table once. sinf() is slow, but here it runs only 1024 times. */
    for (int i = 0; i < WAVETABLE_SIZE; i++)
        sine_table[i] = sinf(2.0f * (float)M_PI * i / WAVETABLE_SIZE);

    Audio_SetFrequency(440.0f);              /* A4 */

    /* Fill both halves before starting, so the very first samples are real sound. */
    fill(&audio_buf[0]);
    fill(&audio_buf[BUFFER_SAMPLES / 2]);

    /* Start the circular DMA. Size = number of 16-bit values in the buffer. */
    return HAL_I2S_Transmit_DMA(hi2s, (uint16_t *)audio_buf, BUFFER_SAMPLES);
}
