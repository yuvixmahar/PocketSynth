#include "echo.h"
#include <stdint.h>
#include <string.h>

#define ECHO_LEN        16384       /* delay in samples: 16384 / 47991 Hz = 341 ms */
#define MAX_MIX         0.5f        /* echo volume at full pot */
#define MAX_FEEDBACK    0.6f        /* how much of each echo comes back again */

/* The delay line: a ring buffer holding the last 341 ms of sound.
   It lives in CCM RAM, a 64 KB block only the CPU can reach (DMA can't),
   so it doesn't take space from the main RAM that the DMA buffers need. */
static int16_t  line[ECHO_LEN] __attribute__((section(".ccmram")));
static uint32_t pos;                        /* where we read and write next */
static volatile float mix, feedback;        /* set by the pot (control tick) */

void Echo_Init(void)
{
    memset(line, 0, sizeof line);           /* startup code doesn't clear CCM RAM */
    pos = 0;
}

void Echo_SetAmount(float amount)
{
    mix      = amount * MAX_MIX;
    feedback = amount * MAX_FEEDBACK;
}

float Echo_Process(float in)
{
    /* The sample written 341 ms ago, scaled back to -1..1 */
    float delayed = line[pos] * (1.0f / 32768.0f);

    /* Store the new sound plus some of the old echo, so it repeats and fades */
    float store = in + delayed * feedback;
    if (store > 1.0f)  store = 1.0f;
    if (store < -1.0f) store = -1.0f;
    line[pos] = (int16_t)(store * 32767.0f);

    /* Move along the ring. ECHO_LEN is a power of 2, so '&' wraps it to 0. */
    pos = (pos + 1) & (ECHO_LEN - 1);

    return in + delayed * mix;
}
