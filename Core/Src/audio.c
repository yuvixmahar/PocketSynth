#include "audio.h"
#include "synth.h"
#include "perf.h"

#define BUFFER_SAMPLES   (AUDIO_HALF_FRAMES * 2 * 2)   /* 2 halves x 2 channels (L, R) */

static int16_t audio_buf[BUFFER_SAMPLES];   /* what the DMA sends: L, R, L, R, ... */

/* DMA finished sending the first half: refill it while the second half plays. */
void HAL_I2S_TxHalfCpltCallback(I2S_HandleTypeDef *hi2s)
{
    if (hi2s->Instance == SPI3)
    {
        uint32_t start = Perf_Now();
        Synth_Render(&audio_buf[0], AUDIO_HALF_FRAMES);
        Perf_Add(&perf_audio, Perf_Now() - start);
    }
}

/* DMA finished the second half (and wrapped to the start): refill the second half. */
void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef *hi2s)
{
    if (hi2s->Instance == SPI3)
    {
        uint32_t start = Perf_Now();
        Synth_Render(&audio_buf[BUFFER_SAMPLES / 2], AUDIO_HALF_FRAMES);
        Perf_Add(&perf_audio, Perf_Now() - start);
    }
}

HAL_StatusTypeDef Audio_Start(I2S_HandleTypeDef *hi2s)
{
    Synth_Init(AUDIO_SAMPLE_RATE);

    /* The buffer starts as zeros (silence), so the DMA can start right away. */
    return HAL_I2S_Transmit_DMA(hi2s, (uint16_t *)audio_buf, BUFFER_SAMPLES);
}
