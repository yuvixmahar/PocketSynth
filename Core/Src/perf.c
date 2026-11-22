#include "perf.h"
#include "audio.h"      /* AUDIO_HALF_FRAMES, AUDIO_SAMPLE_RATE */

PerfStat perf_audio, perf_tick, perf_latency;
volatile PerfReport perf;

void Perf_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;     /* power up the debug/trace unit */
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;               /* count every CPU clock cycle */
}

/* Called from the interrupts, so keep it short. */
void Perf_Add(PerfStat *s, uint32_t cycles)
{
    s->last = cycles;
    if (cycles > s->max)
        s->max = cycles;
    s->total += cycles;
    s->runs++;
}

static float average(const PerfStat *s)
{
    return s->runs ? (float)s->total / (float)s->runs : 0.0f;
}

void Perf_Report(void)
{
    /* How many cycles each job is allowed before it falls behind */
    float block_cycles = SystemCoreClock * (AUDIO_HALF_FRAMES / AUDIO_SAMPLE_RATE);  /* 2.67 ms */
    float tick_cycles  = SystemCoreClock / 1000.0f;                                  /* 1 ms */
    float cycles_per_us = SystemCoreClock / 1000000.0f;

    perf.audio_avg_pct  = 100.0f * average(&perf_audio) / block_cycles;
    perf.audio_max_pct  = 100.0f * perf_audio.max       / block_cycles;
    perf.tick_avg_pct   = 100.0f * average(&perf_tick)  / tick_cycles;
    perf.tick_max_pct   = 100.0f * perf_tick.max        / tick_cycles;
    perf.latency_avg_us = average(&perf_latency) / cycles_per_us;
    perf.latency_max_us = perf_latency.max       / cycles_per_us;
}
