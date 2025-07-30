#ifndef __OPNA_H__
#define __OPNA_H__

#include <stdint.h>
#include "op.h"
#include "psg.h"

/*  YM2608 (OPNA) ------------------------------------------------------ */
typedef struct _OPNA
{
    int fmvolume;
    uint32_t    clock;
    uint32_t    rate;
    uint32_t    psgrate;
    uint32_t    status;
    Channel4  ch[6];
    Channel4* csmch;

    int32_t mixdelta;
    int     mpratio;
    uint8_t    interpolation;

    uint8_t timer_status;
    uint8_t regtc;
    uint8_t regta[2];

    int32_t timera, timera_count;
    int32_t timerb, timerb_count;
    int32_t timer_step;
    uint8_t prescale;
    uint8_t devmask;

    PSG     psg;

    Rhythm  rhythm[6];
    int8_t  rhythmtl;
    int     rhythmtvol;
    uint8_t rhythmkey;

    int32_t mixl, mixl1;
    int32_t mixr, mixr1;
    uint8_t fnum2[9];

    uint8_t reg22;
    uint32_t reg29;
    uint32_t statusnext;

    uint32_t lfocount;
    uint32_t lfodcount;
    uint32_t lfotab[8];

    uint32_t fnum[6];
    uint32_t fnum3[3];

    uint8_t aml;

    uint32_t currentratio;
    float rr;
    uint32_t ratetable[64];
} OPNA;

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------------------- */
uint8_t OPNAInit(OPNA *opna, uint32_t c, uint32_t r, uint8_t ipflag);
void OPNAReset(OPNA *opna);
void OPNASetVolumeRhythm(OPNA *opna, int index, int db);
uint8_t OPNASetRate(OPNA *opna, uint32_t r, uint8_t ipflag);
void OPNASetChannelMask(OPNA *opna, uint32_t mask);
void OPNASetReg(OPNA *opna, uint32_t addr, uint32_t data);
void OPNASetPan(OPNA *opna, uint32_t chan, uint32_t data);
uint8_t OPNATimerCount(OPNA *opna, int32_t us);
void OPNAMix(OPNA *opna, int16_t *buffer, uint32_t nframes);

/* --------------------------------------------------------------------------- */
static inline uint32_t OPNAReadStatus(OPNA *opna) { return opna->status & 0x03; }

static inline int32_t OPNAGetNextEvent(OPNA *opna)
{
    uint32_t ta = ((opna->timera_count + 0xffff) >> 16) - 1;
    uint32_t tb = ((opna->timerb_count + 0xfff) >> 12) - 1;
    return (ta < tb ? ta : tb) + 1;
}

#ifdef __cplusplus
}
#endif

#endif /* FM_OPNA_H */
