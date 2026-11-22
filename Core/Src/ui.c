#include "ui.h"
#include "lcd.h"
#include "leds.h"
#include "controls.h"
#include "synth.h"
#include <stdio.h>

#define LCD_PERIOD_MS   100     /* redraw the screen 10 times a second */
#define LED_PERIOD_MS   20      /* update the LEDs 50 times a second */
#define METER_FULL      0.6f    /* output level that lights all 8 LEDs */
#define METER_FALL      0.85f   /* the meter keeps 85% of its level each update */

static const char *const wave_names[WAVE_COUNT] = { "SINE", "SQUARE", "SAW", "TRI" };
static const char *const note_names[12] =
    { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

static void draw_lcd(void)
{
    ControlStatus s;
    char line[32];                          /* room to spare: the LCD shows the first 16 */

    Controls_GetStatus(&s);

    /* Top line, e.g. "KEYS  SQUARE O+1" */
    snprintf(line, sizeof line, "%-5s %-6s O%+d",
             s.theremin ? "THERM" : "KEYS", wave_names[s.waveform], s.octave);
    LCD_PrintLine(0, line);

    /* Bottom line */
    if (!s.theremin)
    {
        /* e.g. "A  10 R 300 E40%": attack and release in ms, echo in % */
        snprintf(line, sizeof line, "A%4d R%4d E%2d%%",
                 (int)(s.attack_s * 1000.0f + 0.5f), (int)(s.release_s * 1000.0f + 0.5f),
                 (int)(s.echo * 99.0f + 0.5f));      /* + 0.5 rounds instead of chopping */
    }
    else if (s.hand)
    {
        int n = (int)(s.note + 0.5f);       /* nearest whole note */
        snprintf(line, sizeof line, "Hand %2dcm  %s%d",
                 (int)s.hand_cm, note_names[n % 12], n / 12 - 1);
    }
    else
    {
        snprintf(line, sizeof line, "Hand: none");
    }
    LCD_PrintLine(1, line);
}

static void draw_leds(void)
{
    static float level;                     /* what the meter shows, 0..1 */
    ControlStatus s;

    Controls_GetStatus(&s);
    float peak = Synth_TakePeak();          /* always take it, so it never goes stale */

    if (!s.theremin)
    {
        /* Level meter: jump up to each new peak, then fall back slowly. */
        level *= METER_FALL;
        if (peak > level)
            level = peak;

        int lit = (int)(level / METER_FULL * LED_COUNT + 0.5f);
        for (int i = 0; i < LED_COUNT; i++)
        {
            if (i >= lit)       Leds_Set(i, 0, 0, 0);
            else if (i < 5)     Leds_Set(i, 0, 255, 0);       /* green  */
            else if (i < 7)     Leds_Set(i, 255, 160, 0);     /* yellow */
            else                Leds_Set(i, 255, 0, 0);       /* red    */
        }
    }
    else
    {
        /* Theremin: the bar grows and turns from blue to red as the hand gets closer. */
        int lit = s.hand ? 1 + (int)(s.closeness * (LED_COUNT - 1) + 0.5f) : 0;
        uint8_t red = (uint8_t)(s.closeness * 255.0f);

        for (int i = 0; i < LED_COUNT; i++)
        {
            if (i < lit)
                Leds_Set(i, red, 0, 255 - red);
            else
                Leds_Set(i, 0, 0, 0);
        }
    }
    Leds_Show();
}

void UI_Start(I2C_HandleTypeDef *lcd_i2c, TIM_HandleTypeDef *led_tim)
{
    LCD_Init(lcd_i2c);
    Leds_Start(led_tim);
}

/* Each part runs on its own schedule, using the 1 ms HAL tick as a clock. */
void UI_Update(void)
{
    static uint32_t last_led, last_lcd;
    uint32_t now = HAL_GetTick();

    if (now - last_led >= LED_PERIOD_MS)
    {
        last_led = now;
        draw_leds();
    }
    if (now - last_lcd >= LCD_PERIOD_MS)
    {
        last_lcd = now;
        draw_lcd();
    }
}
