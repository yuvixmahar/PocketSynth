#include "leds.h"

/* WS2812: each LED takes 24 bits (green, red, blue, 8 bits each, top bit first).
   Every bit is one 1.25 us PWM period (210 ticks of TIM1 at 168 MHz).
   A short high pulse means 0, a long one means 1. */
#define BITS_PER_LED    24
#define PULSE_0         67      /* 0.40 us high */
#define PULSE_1         134     /* 0.80 us high */
#define TAIL            4       /* extra 0-width periods at the end so the line stays low */
#define PULSE_COUNT     (LED_COUNT * BITS_PER_LED + TAIL)
#define BRIGHTNESS      40      /* out of 255: bright enough, and keeps the USB current low */

static TIM_HandleTypeDef *led_htim;
static uint16_t pulses[PULSE_COUNT];    /* one pulse width per bit; the DMA feeds these to CCR1 */
static volatile int busy;               /* 1 while the DMA is sending a frame */

void Leds_Start(TIM_HandleTypeDef *htim)
{
    led_htim = htim;
    for (int i = 0; i < LED_COUNT; i++)
        Leds_Set(i, 0, 0, 0);
    Leds_Show();                        /* start with every LED off */
}

void Leds_Set(int i, uint8_t r, uint8_t g, uint8_t b)
{
    if (i < 0 || i >= LED_COUNT)
        return;

    /* Dim, then pack in the order the LEDs expect: green, red, blue. */
    uint32_t grb = ((uint32_t)(g * BRIGHTNESS / 255) << 16)
                 | ((uint32_t)(r * BRIGHTNESS / 255) << 8)
                 |  (uint32_t)(b * BRIGHTNESS / 255);

    /* One pulse width per bit, top bit first. */
    uint16_t *p = &pulses[i * BITS_PER_LED];
    for (int bit = 23; bit >= 0; bit--)
        *p++ = ((grb >> bit) & 1) ? PULSE_1 : PULSE_0;
}

void Leds_Show(void)
{
    if (busy)
        return;                         /* the last frame is still going out: skip this one */

    /* The DMA copies the next pulse width into TIM1's CCR1 every period, by itself. */
    busy = 1;
    if (HAL_TIM_PWM_Start_DMA(led_htim, TIM_CHANNEL_1, (uint32_t *)pulses, PULSE_COUNT) != HAL_OK)
        busy = 0;
}

/* HAL calls this from the DMA interrupt once the last pulse width is copied. */
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
    if (htim == led_htim)
    {
        HAL_TIM_PWM_Stop_DMA(htim, TIM_CHANNEL_1);
        busy = 0;
    }
}
