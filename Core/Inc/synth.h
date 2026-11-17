#ifndef SYNTH_H
#define SYNTH_H

#include <stdint.h>

#define SYNTH_NUM_VOICES	4

typedef enum {
	WAVE_SINE = 0,
	WAVE_SQUARE,
	WAVE_SAW,
	WAVE_TRIANGLE,
	WAVE_COUNT			/* = 4, the number of waveforms */
} Waveform;

void Synth_Init(float sample_rate);
void Synth_NoteOn(int voice);
void Synth_NoteOff(int voice);
void Synth_SetFrequency(int voice, float freq_hz);
void Synth_SetWaveform(Waveform w);
void Synth_SetAttack(float seconds);
void Synth_SetRelease(float seconds);
void Synth_Render(int16_t *dst, int frames);

#endif /* SYNTH_H */
