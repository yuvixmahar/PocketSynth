#ifndef PERF_H
#define PERF_H

#include "stm32f4xx_hal.h"

/* Cycle counts for one piece of code, measured with the DWT cycle counter. */
typedef struct {
    uint32_t last;      /* cycles taken by the latest run */
    uint32_t max;       /* slowest run since startup */
    uint32_t runs;      /* how many runs were measured */
    uint64_t total;     /* all runs added up, for the average */
} PerfStat;

/* The same numbers in human units. Watch "perf" in the debugger. */
typedef struct {
    float audio_avg_pct, audio_max_pct;     /* audio render, % of its 2.67 ms */
    float tick_avg_pct,  tick_max_pct;      /* control tick, % of its 1 ms */
    float latency_avg_us, latency_max_us;   /* NoteOn -> render picks it up */
} PerfReport;

extern PerfStat perf_audio, perf_tick, perf_latency;
extern volatile PerfReport perf;

void Perf_Init(void);                       /* start the cycle counter */
void Perf_Add(PerfStat *s, uint32_t cycles);
void Perf_Report(void);                     /* turn the counts into perf (main loop) */

/* Current cycle count. Subtracting two readings gives the cycles in between,
   even if the 32-bit counter wrapped around (every 25 s at 168 MHz). */
static inline uint32_t Perf_Now(void)
{
    return DWT->CYCCNT;
}

#endif /* PERF_H */
