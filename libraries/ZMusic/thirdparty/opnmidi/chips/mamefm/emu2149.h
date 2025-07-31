/* SPDX-License-Identifier: MIT */
/* emu2149.h */
#ifndef EMU2149_H_
#define EMU2149_H_
#include "emutypes.h"

#define EMU2149_API

#define EMU2149_VOL_DEFAULT 1
#define EMU2149_VOL_YM2149 0
#define EMU2149_VOL_AY_3_8910 1

#define EMU2149_ZX_STEREO			0x80

#define PSG_MASK_CH(x) (1<<(x))

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct __PSG
  {

    /* Volume Table */
    UINT32 *voltbl;

    UINT8 reg[0x20];
    INT32 out;
    INT32 cout[3];

    UINT32 clk, rate, base_incr, quality;

    UINT32 count[3];
    UINT32 volume[3];
    UINT32 freq[3];
    UINT32 edge[3];
    UINT32 tmask[3];
    UINT32 nmask[3];
    UINT32 mask;
    UINT32 stereo_mask[3];

    UINT32 base_count;

    UINT32 env_volume;
    UINT32 env_ptr;
    UINT32 env_face;

    UINT32 env_continue;
    UINT32 env_attack;
    UINT32 env_alternate;
    UINT32 env_hold;
    UINT32 env_pause;
    UINT32 env_reset;

    UINT32 env_freq;
    UINT32 env_count;

    UINT32 noise_seed;
    UINT32 noise_count;
    UINT32 noise_freq;

    /* rate converter */
    UINT32 realstep;
    UINT32 psgtime;
    UINT32 psgstep;
    INT32 prev, next;
    INT32 sprev[2], snext[2];

    /* I/O Ctrl */
    UINT32 adr;

  }
  PSG;

  EMU2149_API void PSG_set_quality (PSG * psg, UINT32 q);
  EMU2149_API void PSG_set_clock(PSG * psg, UINT32 c);
  EMU2149_API void PSG_set_rate (PSG * psg, UINT32 r);
  EMU2149_API void PSG_init (PSG * psg, UINT32 clk, UINT32 rate);
  EMU2149_API void PSG_reset (PSG *);
  EMU2149_API void PSG_delete (PSG *);
  EMU2149_API void PSG_writeReg (PSG *, UINT32 reg, UINT32 val);
  EMU2149_API void PSG_writeIO (PSG * psg, UINT32 adr, UINT32 val);
  EMU2149_API UINT8 PSG_readReg (PSG * psg, UINT32 reg);
  EMU2149_API UINT8 PSG_readIO (PSG * psg);
  EMU2149_API INT16 PSG_calc (PSG *);
  EMU2149_API void PSG_calc_stereo (PSG * psg, INT32 **out, INT32 samples);
  EMU2149_API void PSG_setFlags (PSG * psg, UINT8 flags);
  EMU2149_API void PSG_setVolumeMode (PSG * psg, int type);
  EMU2149_API UINT32 PSG_setMask (PSG *, UINT32 mask);
  EMU2149_API UINT32 PSG_toggleMask (PSG *, UINT32 mask);
  EMU2149_API void PSG_setStereoMask (PSG *psg, UINT32 mask);

#ifdef __cplusplus
}
#endif

#endif
