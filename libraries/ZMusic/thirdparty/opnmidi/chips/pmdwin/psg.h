#ifndef __PSG_H__
#define __PSG_H__

#include <stdint.h>
#define PSG_SAMPLETYPE      int32_t     /* any of int16_t, int32_t or float will work here. */

/* Constants for the shift amounts used in the counters.
*/
enum {
    toneshift = 24,
    envshift = 22
};

typedef struct _PSG {
    uint8_t reg[16];

    const uint32_t *envelop;
    uint32_t rng;
    uint32_t olevel[3];
    uint32_t scount[3], speriod[3];
    uint32_t ecount, eperiod;
    uint32_t ncount, nperiod;
    uint32_t tperiodbase;
    uint32_t eperiodbase;
    int volume;
    int mask;
} PSG;

#ifdef __cplusplus
extern "C" {
#endif

/* Mostly self-explanatory.
// Actual descriptions of each function can be found in psg.c
// Also, PSGGetReg() is basically useless.
// (More info on that can *also* be found in psg.c). */
void PSGInit(PSG *psg);
void PSGReset(PSG *psg);
void PSGSetClock(PSG *psg, uint32_t clock, uint32_t rate);
void PSGSetChannelMask(PSG *psg, int c);
void PSGSetReg(PSG *psg, uint8_t regnum, uint8_t data);
void PSGMix(PSG *psg, int32_t *dest, uint32_t nsamples);

static inline uint32_t PSGGetReg(PSG *psg, uint8_t regnum) {
    return psg->reg[regnum & 0x0f];
}

#ifdef __cplusplus
}
#endif

#endif /* PSG_H */
