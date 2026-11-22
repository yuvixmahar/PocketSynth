#include "controls.h"
#include "synth.h"
#include "echo.h"
#include "sonar.h"
#include "main.h"		/* BTN1_Pin, JOY_SW_Pin, ... */
#include <math.h>

#define NUM_BUTTONS				4
#define DEBOUNCE_MS				5	/* a change must last 5 ticks (5 ms) to count */
#define ENC_COUNTS_PER_DETENT	4	/* TIM3 counts per encoder click */

/* ADC1 scan order. Must match the Rank order set in CubeMX. */
enum { ADC_JOY_X, ADC_JOY_Y, ADC_ATTACK, ADC_RELEASE, ADC_ECHO, ADC_COUNT };

#define TICK_HZ			1000.0f
#define SMOOTH			0.05f		/* pot filter: move 5% toward each new reading */
#define JOY_HALF		2048.0f		/* ADC counts from centre to the end of travel */
#define JOY_DEADZONE	150.0f		/* ignore small wobble around the centre */
#define BEND_RANGE		2.0f		/* semitones at full joystick X */
#define VIB_DEPTH		0.5f		/* semitones of wobble at full joystick Y */
#define VIB_RATE_HZ		5.5f		/* wobbles per second */

/* Theremin: hand distance -> pitch */
#define HAND_NEAR_CM	5.0f		/* closest useful distance = highest note */
#define HAND_FAR_CM		50.0f		/* further than this = no hand = silence */
#define THEREMIN_LOW	48.0f		/* MIDI note at HAND_FAR_CM (C3) */
#define THEREMIN_RANGE	24.0f		/* semitones from far to near (2 octaves) */
#define GLIDE			0.02f		/* pitch moves 2% toward the target each ms */
#define SONAR_TIMEOUT_MS 200		/* no reading for this long = sensor missing */


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

typedef struct {
	uint8_t stable;		/* debounced state: 1 = pressed */
	uint8_t count;		/* how many ms the raw reading has disagreed */
} Debouncer;

static TIM_HandleTypeDef *enc_htim;
static Debouncer note_db[NUM_BUTTONS];
static Debouncer joy_db;

static Debouncer mode_db;

static int   theremin_mode;							/* 0 = keyboard, 1 = theremin */
static int   hand;									/* 1 = a hand is in range */
static float target_note  = THEREMIN_LOW;			/* from the latest distance */
static float theremin_note = THEREMIN_LOW;			/* glides toward target_note */
static float hand_cm;								/* latest distance, for the screen */

static float base_hz[NUM_BUTTONS];		/* each button's note before bend/octave */
static int   octave;					/* -1, 0 or +1 */

static volatile uint16_t adc_buf[ADC_COUNT];	/* DMA keeps this up to date */
static float smooth[ADC_COUNT];					/* filtered copy, 0..4095 */
static float centre_x, centre_y;				/* joystick resting position */

/* Kept only so the screen can show them */
static int   waveform;
static float attack_s, release_s, echo_amt;

/* MIDI note number -> frequency. Each semitone is a factor of 2^(1/12). */
static float midi_to_hz(float note)
{
    return 440.0f * powf(2.0f, (note - 69.0f) / 12.0f);
}

/* Feed in one raw reading per ms. Returns 1 when the debounced state changes. */
static int debounce(Debouncer *d, uint8_t raw)
{
    if (raw == d->stable)
    {
        d->count = 0;                         /* nothing is changing */
        return 0;
    }
    if (++d->count < DEBOUNCE_MS)
        return 0;                             /* changed, but not for long enough yet */

    d->stable = raw;                          /* changed, and stayed changed for 5 ms */
    d->count  = 0;
    return 1;
}

static void scan_buttons(void)
{
    for (int i = 0; i < NUM_BUTTONS; i++)
    {
        /* Pull-up wiring: the pin reads 0 while the button is pressed. */
        uint8_t raw = (HAL_GPIO_ReadPin(buttons[i].port, buttons[i].pin) == GPIO_PIN_RESET);

        /* In theremin mode the note buttons are still debounced, but not played. */
        if (debounce(&note_db[i], raw) && !theremin_mode)
        {
            if (note_db[i].stable)
                Synth_NoteOn(i);
            else
                Synth_NoteOff(i);
        }
    }

    /* Joystick press: step the octave 0 -> +1 -> -1 -> 0 ... */
    uint8_t raw = (HAL_GPIO_ReadPin(JOY_SW_GPIO_Port, JOY_SW_Pin) == GPIO_PIN_RESET);
    if (debounce(&joy_db, raw) && joy_db.stable)
        octave = (octave == 1) ? -1 : octave + 1;

    /* Blue user button (PA0): switch keyboard <-> theremin.
       This one reads 1 while pressed (the board has a pull-down on it). */
    raw = (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_SET);
    if (debounce(&mode_db, raw) && mode_db.stable)
    {
        theremin_mode = !theremin_mode;
        for (int i = 0; i < NUM_BUTTONS; i++)
            Synth_NoteOff(i);                 /* start each mode in silence */
    }
}

static void update_theremin(void)
{
    static uint16_t quiet_ms;                 /* ms since the last reading */
    float cm;

    if (Sonar_Read(&cm))                      /* a new reading, every 60 ms */
    {
        quiet_ms = 0;
        hand = (cm <= HAND_FAR_CM);
        hand_cm = cm;
        if (hand)
        {
            if (cm < HAND_NEAR_CM)
                cm = HAND_NEAR_CM;            /* closer than 5 cm: stay on the top note */

            float closeness = (HAND_FAR_CM - cm) / (HAND_FAR_CM - HAND_NEAR_CM);  /* 0..1 */
            target_note = THEREMIN_LOW + closeness * THEREMIN_RANGE;
        }
    }
    else if (quiet_ms < SONAR_TIMEOUT_MS)
    {
        quiet_ms++;
    }
    else
    {
        hand = 0;                             /* no readings: sensor missing or unplugged */
    }

    /* Glide: readings only come every 60 ms, so slide toward each one
       a little every ms instead of jumping. */
    theremin_note += GLIDE * (target_note - theremin_note);

    if (theremin_mode)
    {
        if (hand)
            Synth_NoteOn(0);                  /* the theremin plays on voice 0 */
        else
            Synth_NoteOff(0);
    }
}

static void read_encoder(void)
{
    uint16_t count = __HAL_TIM_GET_COUNTER(enc_htim);
    uint16_t clicks = count / ENC_COUNTS_PER_DETENT;
    waveform = clicks % WAVE_COUNT;
    Synth_SetWaveform((Waveform)waveform);
}

/* Joystick axis -> -1..+1, with 0 anywhere inside the dead zone. */
static float joy_axis(float reading, float centre)
{
    float d = reading - centre;
    if (fabsf(d) < JOY_DEADZONE)
        return 0.0f;

    d -= (d > 0.0f) ? JOY_DEADZONE : -JOY_DEADZONE;   /* start from 0 at the zone's edge */
    float v = d / (JOY_HALF - JOY_DEADZONE);
    if (v > 1.0f)  v = 1.0f;
    if (v < -1.0f) v = -1.0f;
    return v;
}

static void read_analog(void)
{
    /* Low-pass filter: smooths ADC noise so pitch and pots don't jitter. */
    for (int i = 0; i < ADC_COUNT; i++)
        smooth[i] += SMOOTH * (adc_buf[i] - smooth[i]);

    /* Pots: 0..1, then mapped so the low end has finer control. */
    float attack  = smooth[ADC_ATTACK]  / 4095.0f;
    float release = smooth[ADC_RELEASE] / 4095.0f;
    float echo    = smooth[ADC_ECHO]    / 4095.0f;

    attack_s  = 0.002f * powf(1000.0f, attack);         /* 2 ms .. 2 s  */
    release_s = 0.010f * powf(300.0f, release);         /* 10 ms .. 3 s */
    echo_amt  = echo;

    Synth_SetAttack(attack_s);
    Synth_SetRelease(release_s);
    Echo_SetAmount(echo_amt);
}

static void update_pitch(void)
{
    /* Vibrato LFO: a slow sine wave, stepped once per ms. */
    static float lfo_phase;                   /* 0..1 = one wobble */
    lfo_phase += VIB_RATE_HZ / TICK_HZ;
    if (lfo_phase >= 1.0f)
        lfo_phase -= 1.0f;

    float bend  = joy_axis(smooth[ADC_JOY_X], centre_x) * BEND_RANGE;
    float depth = fabsf(joy_axis(smooth[ADC_JOY_Y], centre_y)) * VIB_DEPTH;
    float vib   = depth * sinf(2.0f * (float)M_PI * lfo_phase);

    /* Everything in semitones, then one powf turns it into a frequency ratio. */
    float semis = 12.0f * octave + bend + vib;

    if (theremin_mode)
    {
        /* theremin_note is a MIDI number with a fraction, so just add the semitones. */
        Synth_SetFrequency(0, midi_to_hz(theremin_note + semis));
        return;
    }

    float ratio = powf(2.0f, semis / 12.0f);

    for (int i = 0; i < NUM_BUTTONS; i++)
        Synth_SetFrequency(i, base_hz[i] * ratio);
}

/* HAL calls this when any timer with an update interrupt overflows. */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6)              /* our 1 kHz control tick */
    {
        scan_buttons();
        read_encoder();
        read_analog();
        update_theremin();
        update_pitch();
    }
}

void Controls_Start(TIM_HandleTypeDef *enc_tim, TIM_HandleTypeDef *tick_tim,
                    ADC_HandleTypeDef *adc)
{
    enc_htim = enc_tim;

    for (int i = 0; i < NUM_BUTTONS; i++)
        base_hz[i] = midi_to_hz(buttons[i].midi_note);

    /* ADC1 scans the 5 inputs over and over; DMA copies each result into adc_buf. */
    HAL_ADC_Start_DMA(adc, (uint32_t *)adc_buf, ADC_COUNT);

    /* We only look at adc_buf once per ms, so turn off the DMA's
       "transfer done" interrupts (they'd fire ~17,000 times a second for nothing). */
    __HAL_DMA_DISABLE_IT(adc->DMA_Handle, DMA_IT_TC | DMA_IT_HT);

    /* Let a few scans land, then take the joystick's resting spot as its centre. */
    HAL_Delay(10);
    for (int i = 0; i < ADC_COUNT; i++)
        smooth[i] = adc_buf[i];
    centre_x = smooth[ADC_JOY_X];
    centre_y = smooth[ADC_JOY_Y];

    /* Start the count in the middle so turning left never goes below 0. */
    __HAL_TIM_SET_COUNTER(enc_htim, 32768);
    HAL_TIM_Encoder_Start(enc_htim, TIM_CHANNEL_ALL);

    HAL_TIM_Base_Start_IT(tick_tim);         /* start the 1 kHz interrupt */
}


/* Copy the current state for the screen and LEDs. Called from the main loop,
   so the values can be up to 1 ms old, which is fine for a display. */
void Controls_GetStatus(ControlStatus *s)
{
    s->theremin  = theremin_mode;
    s->octave    = octave;
    s->waveform  = waveform;
    s->attack_s  = attack_s;
    s->release_s = release_s;
    s->echo      = echo_amt;
    s->hand      = hand;
    s->hand_cm   = hand_cm;
    s->note      = theremin_note + 12.0f * octave;
    s->closeness = (theremin_note - THEREMIN_LOW) / THEREMIN_RANGE;
}
