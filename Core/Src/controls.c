#include "controls.h"
#include "synth.h"
#include "main.h"		/* BTN1_Pin, BTN1_GPIO_Port, ... */
#include <math.h>

#define NUM_BUTTONS				4
#define DEBOUNCE_MS			5	/* a change must last 5 ticks (5 ms) to count */
#define ENC_COUNTS_PER_DETENT	4	/* TIM3 counts per encoder click */

typedef struct {
	GPIO_TypeDef *port;
	uint16_t	 pin;
	uint8_t		 midi_note;			/* 60 = middle C */
} NoteButton;

/* A C major chord: press all four together for a chord. */
static const NoteButton buttons[NUM_BUTTONS] = {
	{ BTN1_GPIO_Port, BTN1_Pin, 60 },   /* C4 */
	{ BTN2_GPIO_Port, BTN2_Pin, 64 },   /* E4 */
	{ BTN3_GPIO_Port, BTN3_Pin, 67 },   /* G4 */
	{ BTN4_GPIO_Port, BTN4_Pin, 72 },   /* C5 */
};

static TIM_HandleTypeDef *enc_htim;
static uint8_t stable[NUM_BUTTONS];		/* debounced state: 1 = pressed */
static uint8_t counter[NUM_BUTTONS];	/* how many ms the raw reading has disagreed */

/* MIDI note number -> frequency. Each semitone is a factor of 2^(1/12). */
static float midi_to_hz(int note)
{
    return 440.0f * powf(2.0f, (note - 69) / 12.0f);
}

static void scan_buttons(void)
{
    for (int i = 0; i < NUM_BUTTONS; i++)
    {
        /* Pull-up wiring: the pin reads 0 while the button is pressed. */
        uint8_t raw = (HAL_GPIO_ReadPin(buttons[i].port, buttons[i].pin) == GPIO_PIN_RESET);

        if (raw == stable[i])
        {
            counter[i] = 0;                       /* nothing is changing */
        }
        else if (++counter[i] >= DEBOUNCE_MS)
        {
            stable[i]  = raw;                     /* changed, and stayed changed for 5 ms */
            counter[i] = 0;

            if (raw)
                Synth_NoteOn(i, midi_to_hz(buttons[i].midi_note));
            else
                Synth_NoteOff(i);
        }
    }
}

static void read_encoder(void)
{
    uint16_t count = __HAL_TIM_GET_COUNTER(enc_htim);
    uint16_t clicks = count / ENC_COUNTS_PER_DETENT;
    Synth_SetWaveform((Waveform)(clicks % WAVE_COUNT));
}

/* HAL calls this when any timer with an update interrupt overflows. */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6)              /* our 1 kHz control tick */
    {
        scan_buttons();
        read_encoder();
    }
}

void Controls_Start(TIM_HandleTypeDef *enc_tim, TIM_HandleTypeDef *tick_tim)
{
    enc_htim = enc_tim;

    /* Start the count in the middle so turning left never goes below 0. */
    __HAL_TIM_SET_COUNTER(enc_htim, 32768);
    HAL_TIM_Encoder_Start(enc_htim, TIM_CHANNEL_ALL);

    HAL_TIM_Base_Start_IT(tick_tim);         /* start the 1 kHz interrupt */
}

