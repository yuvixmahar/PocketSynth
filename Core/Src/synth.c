#include "synth.h"
#include "echo.h"
#include <math.h>

#define TABLE_SIZE		1024
#define TABLE_SHIFT		22			/* 32 bits - 10 bits of table index */
#define VOICE_GAIN		0.25f		/* 4 voices x 0.25 = 1.0 at most: never clips */

/* The pots change attack and release while playing. */
#define ATTACK_S		0.010f 		/* 10 ms to reach full volume */
#define DECAY_S			0.150f		/* 150 ms to drop to sustain */
#define SUSTAIN_LVL		0.7f		/* level while the key is held */
#define RELEASE_S		0.300f		/* 300 ms to fade out */

typedef enum { ENV_IDLE, ENV_ATTACK, ENV_DECAY, ENV_SUSTAIN, ENV_RELEASE } EnvStage;

typedef struct {
	/* Written by the control tick (TIM6), read by the audio interrupt */
	volatile uint32_t phase_inc;	/* pitch */
	volatile uint8_t gate;			/* 1 = key is held down */

	/* Used only inside the audio interrupt */
	uint32_t phase;
	EnvStage stage;
	float    env;					/* Current volume, 0..1 */
} Voice;

static float	tables[WAVE_COUNT][TABLE_SIZE];
static Voice	voices[SYNTH_NUM_VOICES];
static volatile	Waveform waveform = WAVE_SINE;

static float fs;
static float decay_step;
static volatile float attack_step, release_step;	/* set by the pots */

void Synth_Init(float sample_rate)
{
	fs = sample_rate;

	/* One cycle of each shape. x goes from 0 to 1 across the cycle. */
	for (int i = 0; i < TABLE_SIZE; i++)
	{
		float x = (float)i / TABLE_SIZE;
		tables[WAVE_SINE][i] 	 = sinf(2.0f * (float)M_PI * x);
		tables[WAVE_SQUARE][i]	 = (x < 0.5f) ? 1.0f : -1.0f;
		tables[WAVE_SAW][i]		 = 2.0f * x - 1.0f;
		tables[WAVE_TRIANGLE][i] = (x < 0.5f) ? (4.0f * x - 1.0f) : (3.0f - 4.0f * x);
	}

	/* How much the volume changes per sample in each stage */
	decay_step = (1.0f - SUSTAIN_LVL) / (DECAY_S * fs);
	Synth_SetAttack(ATTACK_S);
	Synth_SetRelease(RELEASE_S);

	Echo_Init();
}

void Synth_NoteOn(int voice)
{
	voices[voice].gate = 1;
}

void Synth_NoteOff(int voice)
{
	voices[voice].gate = 0;
}

/* The control tick calls this every 1 ms, so bend and vibrato move the pitch live. */
void Synth_SetFrequency(int voice, float freq_hz)
{
	voices[voice].phase_inc = (uint32_t)(freq_hz / fs * 4294967296.0f);
}

void Synth_SetWaveform(Waveform w)
{
	waveform = w;
}

void Synth_SetAttack(float seconds)
{
	attack_step = 1.0f / (seconds * fs);
}

void Synth_SetRelease(float seconds)
{
	release_step = 1.0f / (seconds * fs);
}

/* Move one voice's envelope forward by one sample and return its volume. */
static float envelope_next(Voice *v)
{
	/* React to the key: pressed starts the attack, released starts the release. */
	if (v->gate)
	{
		if (v->stage == ENV_IDLE || v->stage == ENV_RELEASE)
			v->stage = ENV_ATTACK;
	}
	else if (v->stage != ENV_IDLE)
	{
		v->stage = ENV_RELEASE;
	}

	switch (v->stage)
	{
	case ENV_ATTACK:
		v->env += attack_step;
		if (v->env >= 1.0f) { v->env = 1.0f; v->stage = ENV_DECAY; }
		break;

	case ENV_DECAY:
		v->env -= decay_step;
		if(v->env <= SUSTAIN_LVL) { v->env = SUSTAIN_LVL; v->stage = ENV_SUSTAIN; }
		break;

	case ENV_RELEASE:
		v->env -= release_step;
		if (v->env <= 0.0f) { v->env = 0.0f; v->stage = ENV_IDLE; }
		break;

	default:
		break;
	}

	return v->env;
}

/* Fill 'frames' stereo frames (L, R, L, R, ...). Called from the DMA interrupt. */
void Synth_Render(int16_t *dst, int frames)
{
    const float *table = tables[waveform];   /* read once per block */

    for (int i = 0; i < frames; i++)
    {
        float mix = 0.0f;

        for (int n = 0; n < SYNTH_NUM_VOICES; n++)
        {
            Voice *v = &voices[n];
            float env = envelope_next(v);
            if (v->stage == ENV_IDLE)
                continue;                     /* silent voice: skip the work */

            mix += table[v->phase >> TABLE_SHIFT] * env;
            v->phase += v->phase_inc;
        }
        /* Echo runs even with no keys held, so the repeats can fade out. */
        float out = Echo_Process(mix * VOICE_GAIN);
        if (out > 1.0f)  out = 1.0f;          /* dry + echo can pass 1.0: clip it */
        if (out < -1.0f) out = -1.0f;

        int16_t s = (int16_t)(out * 32767.0f);
        dst[2 * i]     = s;                   /* left  */
        dst[2 * i + 1] = s;                   /* right */
    }
}
