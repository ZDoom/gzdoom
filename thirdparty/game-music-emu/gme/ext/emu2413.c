/***********************************************************************************

  emu2413.c -- YM2413 emulator written by Mitsutaka Okazaki 2001

  2001 01-08 : Version 0.10 -- 1st version.
  2001 01-15 : Version 0.20 -- semi-public version.
  2001 01-16 : Version 0.30 -- 1st public version.
  2001 01-17 : Version 0.31 -- Fixed bassdrum problem.
             : Version 0.32 -- LPF implemented.
  2001 01-18 : Version 0.33 -- Fixed the drum problem, refine the mix-down method.
                            -- Fixed the LFO bug.
  2001 01-24 : Version 0.35 -- Fixed the drum problem,
                               support undocumented EG behavior.
  2001 02-02 : Version 0.38 -- Improved the performance.
                               Fixed the hi-hat and cymbal model.
                               Fixed the default percussive datas.
                               Noise reduction.
                               Fixed the feedback problem.
  2001 03-03 : Version 0.39 -- Fixed some drum bugs.
                               Improved the performance.
  2001 03-04 : Version 0.40 -- Improved the feedback.
                               Change the default table size.
                               Clock and Rate can be changed during play.
  2001 06-24 : Version 0.50 -- Improved the hi-hat and the cymbal tone.
                               Added VRC7 patch (OPLL_reset_patch is changed).
                               Fixed OPLL_reset() bug.
                               Added OPLL_setMask, OPLL_getMask and OPLL_toggleMask.
                               Added OPLL_writeIO.
  2001 09-28 : Version 0.51 -- Removed the noise table.
  2002 01-28 : Version 0.52 -- Added Stereo mode.
  2002 02-07 : Version 0.53 -- Fixed some drum bugs.
  2002 02-20 : Version 0.54 -- Added the best quality mode.
  2002 03-02 : Version 0.55 -- Removed OPLL_init & OPLL_close.
  2002 05-30 : Version 0.60 -- Fixed HH&CYM generator and all voice datas.
  2004 04-10 : Version 0.61 -- Added YMF281B tone (defined by Chabin).

  References:
    fmopl.c        -- 1999,2000 written by Tatsuyuki Satoh (MAME development).
    fmopl.c(fixed) -- (C) 2002 Jarek Burczynski.
    s_opl.c        -- 2001 written by Mamiya (NEZplug development).
    fmgen.cpp      -- 1999,2000 written by cisc.
    fmpac.ill      -- 2000 created by NARUTO.
    MSX-Datapack
    YMU757 data sheet
    YM2143 data sheet

**************************************************************************************/

/**
 *  Additions by Maxim:
 *  - per-channel panning
 *
 **/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "emu2413.h"
#include "panning.h" // Maxim

/*#ifdef EMU2413_COMPACTION
#define OPLL_TONE_NUM 1
static unsigned char default_inst[OPLL_TONE_NUM][(16 + 3) * 16] = {
  {
#include "2413tone.h"
   }
};
#else
#define OPLL_TONE_NUM 3
static unsigned char default_inst[OPLL_TONE_NUM][(16 + 3) * 16] = {
  {
#include "2413tone.h"
  },
  {
#include "vrc7tone.h"
   },
  {
#include "281btone.h"
  }
};
#endif*/

// Note: Dump size changed to 8 per instrument, since 9-15 were unused. -VB
#define OPLL_TONE_NUM 1
static unsigned char default_inst[OPLL_TONE_NUM][(16 + 3) * 8] = {
  {
#include "2413tone.h"
  }
};

/* Size of Sintable ( 8 -- 18 can be used. 9 recommended.) */
#define PG_BITS 9
#define PG_WIDTH (1<<PG_BITS)

/* Phase increment counter */
#define DP_BITS 18
#define DP_WIDTH (1<<DP_BITS)
#define DP_BASE_BITS (DP_BITS - PG_BITS)

/* Dynamic range (Accuracy of sin table) */
#define DB_BITS 8
#define DB_STEP (48.0/(1<<DB_BITS))
#define DB_MUTE (1<<DB_BITS)

/* Dynamic range of envelope */
#define EG_STEP 0.375
#define EG_BITS 7
#define EG_MUTE (1<<EG_BITS)

/* Dynamic range of total level */
#define TL_STEP 0.75
#define TL_BITS 6
#define TL_MUTE (1<<TL_BITS)

/* Dynamic range of sustine level */
#define SL_STEP 3.0
#define SL_BITS 4
#define SL_MUTE (1<<SL_BITS)

#define EG2DB(d) ((d)*(e_int32)(EG_STEP/DB_STEP))
#define TL2EG(d) ((d)*(e_int32)(TL_STEP/EG_STEP))
#define SL2EG(d) ((d)*(e_int32)(SL_STEP/EG_STEP))

#define DB_POS(x) (e_uint32)((x)/DB_STEP)
#define DB_NEG(x) (e_uint32)(DB_MUTE+DB_MUTE+(x)/DB_STEP)

/* Bits for liner value */
#define DB2LIN_AMP_BITS 8
#define SLOT_AMP_BITS (DB2LIN_AMP_BITS)

/* Bits for envelope phase incremental counter */
#define EG_DP_BITS 22
#define EG_DP_WIDTH (1<<EG_DP_BITS)

/* Bits for Pitch and Amp modulator */
#define PM_PG_BITS 8
#define PM_PG_WIDTH (1<<PM_PG_BITS)
#define PM_DP_BITS 16
#define PM_DP_WIDTH (1<<PM_DP_BITS)
#define AM_PG_BITS 8
#define AM_PG_WIDTH (1<<AM_PG_BITS)
#define AM_DP_BITS 16
#define AM_DP_WIDTH (1<<AM_DP_BITS)

/* PM table is calcurated by PM_AMP * pow(2,PM_DEPTH*sin(x)/1200) */
#define PM_AMP_BITS 8
#define PM_AMP (1<<PM_AMP_BITS)

/* PM speed(Hz) and depth(cent) */
#define PM_SPEED 6.4
#define PM_DEPTH 13.75

/* AM speed(Hz) and depth(dB) */
#define AM_SPEED 3.6413
#define AM_DEPTH 4.875

/* Cut the lower b bit(s) off. */
#define HIGHBITS(c,b) ((c)>>(b))

/* Leave the lower b bit(s). */
#define LOWBITS(c,b) ((c)&((1<<(b))-1))

/* Expand x which is s bits to d bits. */
#define EXPAND_BITS(x,s,d) ((x)<<((d)-(s)))

/* Expand x which is s bits to d bits and fill expanded bits '1' */
#define EXPAND_BITS_X(x,s,d) (((x)<<((d)-(s)))|((1<<((d)-(s)))-1))

/* Adjust envelope speed which depends on sampling rate. */
#define RATE_ADJUST(x) (rate==49716?(x):(e_uint32)((double)(x)*clk/72/rate + 0.5))      /* added 0.5 to round the value*/

#define MOD(o,x) (&(o)->slot[(x)<<1])
#define CAR(o,x) (&(o)->slot[((x)<<1)|1])

#define BIT(s,b) (((s)>>(b))&1)

/* Input clock */
static e_uint32 clk = 844451141;
/* Sampling rate */
static e_uint32 rate = 3354932;

/* WaveTable for each envelope amp */
static e_uint16 fullsintable[PG_WIDTH];
static e_uint16 halfsintable[PG_WIDTH];

static e_uint16 *waveform[2] = { fullsintable, halfsintable };

/* LFO Table */
static e_int32 pmtable[PM_PG_WIDTH];
static e_int32 amtable[AM_PG_WIDTH];

/* Phase delta for LFO */
static e_uint32 pm_dphase;
static e_uint32 am_dphase;

/* dB to Liner table */
static e_int16 DB2LIN_TABLE[(DB_MUTE + DB_MUTE) * 2];

/* Liner to Log curve conversion table (for Attack rate). */
static e_uint16 AR_ADJUST_TABLE[1 << EG_BITS];

/* Empty voice data */
static OPLL_PATCH null_patch = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

/* Basic voice Data */
static OPLL_PATCH default_patch[OPLL_TONE_NUM][(16 + 3) * 2];

/* Definition of envelope mode */
enum OPLL_EG_STATE
{ READY, ATTACK, DECAY, SUSHOLD, SUSTINE, RELEASE, SETTLE, FINISH };

/* Phase incr table for Attack */
static e_uint32 dphaseARTable[16][16];
/* Phase incr table for Decay and Release */
static e_uint32 dphaseDRTable[16][16];

/* KSL + TL Table */
static e_uint32 tllTable[16][8][1 << TL_BITS][4];
static e_int32 rksTable[2][8][2];

/* Phase incr table for PG */
static e_uint32 dphaseTable[512][8][16];

/***************************************************

                  Create tables

****************************************************/
INLINE static e_int32
Min (e_int32 i, e_int32 j)
{
  if (i < j)
    return i;
  else
    return j;
}

/* Table for AR to LogCurve. */
static void
makeAdjustTable (void)
{
  e_int32 i;

  AR_ADJUST_TABLE[0] = (1 << EG_BITS) - 1;
  for (i = 1; i < (1<<EG_BITS); i++)
    AR_ADJUST_TABLE[i] = (e_uint16) ((double) (1<<EG_BITS)-1 - ((1<<EG_BITS)-1)*log(i)/log(127));
}


/* Table for dB(0 -- (1<<DB_BITS)-1) to Liner(0 -- DB2LIN_AMP_WIDTH) */
static void
makeDB2LinTable (void)
{
  e_int32 i;

  for (i = 0; i < DB_MUTE + DB_MUTE; i++)
  {
    DB2LIN_TABLE[i] = (e_int16) ((double) ((1 << DB2LIN_AMP_BITS) - 1) * pow (10, -(double) i * DB_STEP / 20));
    if (i >= DB_MUTE) DB2LIN_TABLE[i] = 0;
    DB2LIN_TABLE[i + DB_MUTE + DB_MUTE] = (e_int16) (-DB2LIN_TABLE[i]);
  }
}

/* Liner(+0.0 - +1.0) to dB((1<<DB_BITS) - 1 -- 0) */
static e_int32
lin2db (double d)
{
  if (d == 0)
    return (DB_MUTE - 1);
  else
    return Min (-(e_int32) (20.0 * log10 (d) / DB_STEP), DB_MUTE-1);  /* 0 -- 127 */
}


/* Sin Table */
static void
makeSinTable (void)
{
  e_int32 i;

  for (i = 0; i < PG_WIDTH / 4; i++)
  {
    fullsintable[i] = (e_uint32) lin2db (sin (2.0 * PI * i / PG_WIDTH) );
  }

  for (i = 0; i < PG_WIDTH / 4; i++)
  {
    fullsintable[PG_WIDTH / 2 - 1 - i] = fullsintable[i];
  }

  for (i = 0; i < PG_WIDTH / 2; i++)
  {
    fullsintable[PG_WIDTH / 2 + i] = (e_uint32) (DB_MUTE + DB_MUTE + fullsintable[i]);
  }

  for (i = 0; i < PG_WIDTH / 2; i++)
    halfsintable[i] = fullsintable[i];
  for (i = PG_WIDTH / 2; i < PG_WIDTH; i++)
    halfsintable[i] = fullsintable[0];
}

static double saw(double phase)
{
  if(phase <= PI/2)
    return phase * 2 / PI ;
  else if(phase <= PI*3/2)
    return 2.0 - ( phase * 2 / PI );
  else
    return -4.0 + phase * 2 / PI;
}

/* Table for Pitch Modulator */
static void
makePmTable (void)
{
  e_int32 i;

  for (i = 0; i < PM_PG_WIDTH; i++)
    /* pmtable[i] = (e_int32) ((double) PM_AMP * pow (2, (double) PM_DEPTH * sin (2.0 * PI * i / PM_PG_WIDTH) / 1200)); */
    pmtable[i] = (e_int32) ((double) PM_AMP * pow (2, (double) PM_DEPTH * saw (2.0 * PI * i / PM_PG_WIDTH) / 1200));
}

/* Table for Amp Modulator */
static void
makeAmTable (void)
{
  e_int32 i;

  for (i = 0; i < AM_PG_WIDTH; i++)
    /* amtable[i] = (e_int32) ((double) AM_DEPTH / 2 / DB_STEP * (1.0 + sin (2.0 * PI * i / PM_PG_WIDTH))); */
    amtable[i] = (e_int32) ((double) AM_DEPTH / 2 / DB_STEP * (1.0 + saw (2.0 * PI * i / PM_PG_WIDTH)));
}

/* Phase increment counter table */
static void
makeDphaseTable (void)
{
  e_uint32 fnum, block, ML;
  e_uint32 mltable[16] =
    { 1, 1 * 2, 2 * 2, 3 * 2, 4 * 2, 5 * 2, 6 * 2, 7 * 2, 8 * 2, 9 * 2, 10 * 2, 10 * 2, 12 * 2, 12 * 2, 15 * 2, 15 * 2 };

  for (fnum = 0; fnum < 512; fnum++)
    for (block = 0; block < 8; block++)
      for (ML = 0; ML < 16; ML++)
        dphaseTable[fnum][block][ML] = RATE_ADJUST (((fnum * mltable[ML]) << block) >> (20 - DP_BITS));
}

static void
makeTllTable (void)
{
#define dB2(x) ((x)*2)

  static double kltable[16] = {
    dB2 (0.000), dB2 (9.000), dB2 (12.000), dB2 (13.875), dB2 (15.000), dB2 (16.125), dB2 (16.875), dB2 (17.625),
    dB2 (18.000), dB2 (18.750), dB2 (19.125), dB2 (19.500), dB2 (19.875), dB2 (20.250), dB2 (20.625), dB2 (21.000)
  };

  e_int32 tmp;
  e_int32 fnum, block, TL, KL;

  for (fnum = 0; fnum < 16; fnum++)
    for (block = 0; block < 8; block++)
      for (TL = 0; TL < 64; TL++)
        for (KL = 0; KL < 4; KL++)
        {
          if (KL == 0)
          {
            tllTable[fnum][block][TL][KL] = TL2EG (TL);
          }
          else
          {
            tmp = (e_int32) (kltable[fnum] - dB2 (3.000) * (7 - block));
            if (tmp <= 0)
              tllTable[fnum][block][TL][KL] = TL2EG (TL);
            else
              tllTable[fnum][block][TL][KL] = (e_uint32) ((tmp >> (3 - KL)) / EG_STEP) + TL2EG (TL);
          }
        }
}

#ifdef USE_SPEC_ENV_SPEED
static double attacktime[16][4] = {
  {0, 0, 0, 0},
  {1730.15, 1400.60, 1153.43, 988.66},
  {865.08, 700.30, 576.72, 494.33},
  {432.54, 350.15, 288.36, 247.16},
  {216.27, 175.07, 144.18, 123.58},
  {108.13, 87.54, 72.09, 61.79},
  {54.07, 43.77, 36.04, 30.90},
  {27.03, 21.88, 18.02, 15.45},
  {13.52, 10.94, 9.01, 7.72},
  {6.76, 5.47, 4.51, 3.86},
  {3.38, 2.74, 2.25, 1.93},
  {1.69, 1.37, 1.13, 0.97},
  {0.84, 0.70, 0.60, 0.54},
  {0.50, 0.42, 0.34, 0.30},
  {0.28, 0.22, 0.18, 0.14},
  {0.00, 0.00, 0.00, 0.00}
};

static double decaytime[16][4] = {
  {0, 0, 0, 0},
  {20926.60, 16807.20, 14006.00, 12028.60},
  {10463.30, 8403.58, 7002.98, 6014.32},
  {5231.64, 4201.79, 3501.49, 3007.16},
  {2615.82, 2100.89, 1750.75, 1503.58},
  {1307.91, 1050.45, 875.37, 751.79},
  {653.95, 525.22, 437.69, 375.90},
  {326.98, 262.61, 218.84, 187.95},
  {163.49, 131.31, 109.42, 93.97},
  {81.74, 65.65, 54.71, 46.99},
  {40.87, 32.83, 27.36, 23.49},
  {20.44, 16.41, 13.68, 11.75},
  {10.22, 8.21, 6.84, 5.87},
  {5.11, 4.10, 3.42, 2.94},
  {2.55, 2.05, 1.71, 1.47},
  {1.27, 1.27, 1.27, 1.27}
};
#endif

/* Rate Table for Attack */
static void
makeDphaseARTable (void)
{
  e_int32 AR, Rks, RM;
  e_uint32 RL;

#ifdef USE_SPEC_ENV_SPEED
  e_uint32 attacktable[16][4];

  for (RM = 0; RM < 16; RM++)
    for (RL = 0; RL < 4; RL++)
    {
      if (RM == 0)
        attacktable[RM][RL] = 0;
      else if (RM == 15)
        attacktable[RM][RL] = EG_DP_WIDTH;
      else
        attacktable[RM][RL] = (e_uint32) ((double) (1 << EG_DP_BITS) / (attacktime[RM][RL] * 3579545 / 72000));

    }
#endif

  for (AR = 0; AR < 16; AR++)
    for (Rks = 0; Rks < 16; Rks++)
    {
      RM = AR + (Rks >> 2);
      RL = Rks & 3;
      if (RM > 15)
        RM = 15;
      switch (AR)
      {
      case 0:
        dphaseARTable[AR][Rks] = 0;
        break;
      case 15:
        dphaseARTable[AR][Rks] = 0;/*EG_DP_WIDTH;*/
        break;
      default:
#ifdef USE_SPEC_ENV_SPEED
        dphaseARTable[AR][Rks] = RATE_ADJUST (attacktable[RM][RL]);
#else
        dphaseARTable[AR][Rks] = RATE_ADJUST ((3 * (RL + 4) << (RM + 1)));
#endif
        break;
      }
    }
}

/* Rate Table for Decay and Release */
static void
makeDphaseDRTable (void)
{
  e_int32 DR, Rks, RM;
  e_uint32 RL;

#ifdef USE_SPEC_ENV_SPEED
  e_uint32 decaytable[16][4];

  for (RM = 0; RM < 16; RM++)
    for (RL = 0; RL < 4; RL++)
      if (RM == 0)
        decaytable[RM][RL] = 0;
      else
        decaytable[RM][RL] = (e_uint32) ((double) (1 << EG_DP_BITS) / (decaytime[RM][RL] * 3579545 / 72000));
#endif

  for (DR = 0; DR < 16; DR++)
    for (Rks = 0; Rks < 16; Rks++)
    {
      RM = DR + (Rks >> 2);
      RL = Rks & 3;
      if (RM > 15)
        RM = 15;
      switch (DR)
      {
      case 0:
        dphaseDRTable[DR][Rks] = 0;
        break;
      default:
#ifdef USE_SPEC_ENV_SPEED
        dphaseDRTable[DR][Rks] = RATE_ADJUST (decaytable[RM][RL]);
#else
        dphaseDRTable[DR][Rks] = RATE_ADJUST ((RL + 4) << (RM - 1));
#endif
        break;
      }
    }
}

static void
makeRksTable (void)
{

  e_int32 fnum8, block, KR;

  for (fnum8 = 0; fnum8 < 2; fnum8++)
    for (block = 0; block < 8; block++)
      for (KR = 0; KR < 2; KR++)
      {
        if (KR != 0)
          rksTable[fnum8][block][KR] = (block << 1) + fnum8;
        else
          rksTable[fnum8][block][KR] = block >> 1;
      }
}

void
OPLL_dump2patch (const e_uint8 * dump, OPLL_PATCH * patch)
{
  patch[0].AM = (dump[0] >> 7) & 1;
  patch[1].AM = (dump[1] >> 7) & 1;
  patch[0].PM = (dump[0] >> 6) & 1;
  patch[1].PM = (dump[1] >> 6) & 1;
  patch[0].EG = (dump[0] >> 5) & 1;
  patch[1].EG = (dump[1] >> 5) & 1;
  patch[0].KR = (dump[0] >> 4) & 1;
  patch[1].KR = (dump[1] >> 4) & 1;
  patch[0].ML = (dump[0]) & 15;
  patch[1].ML = (dump[1]) & 15;
  patch[0].KL = (dump[2] >> 6) & 3;
  patch[1].KL = (dump[3] >> 6) & 3;
  patch[0].TL = (dump[2]) & 63;
  patch[0].FB = (dump[3]) & 7;
  patch[0].WF = (dump[3] >> 3) & 1;
  patch[1].WF = (dump[3] >> 4) & 1;
  patch[0].AR = (dump[4] >> 4) & 15;
  patch[1].AR = (dump[5] >> 4) & 15;
  patch[0].DR = (dump[4]) & 15;
  patch[1].DR = (dump[5]) & 15;
  patch[0].SL = (dump[6] >> 4) & 15;
  patch[1].SL = (dump[7] >> 4) & 15;
  patch[0].RR = (dump[6]) & 15;
  patch[1].RR = (dump[7]) & 15;
}

void
OPLL_getDefaultPatch (e_int32 type, e_int32 num, OPLL_PATCH * patch)
{
  OPLL_dump2patch (default_inst[type] + num * 8, patch);
}

static void
makeDefaultPatch ()
{
  e_int32 i, j;

  for (i = 0; i < OPLL_TONE_NUM; i++)
    for (j = 0; j < 19; j++)
      OPLL_getDefaultPatch (i, j, &default_patch[i][j * 2]);

}

void
OPLL_setPatch (OPLL * opll, const e_uint8 * dump)
{
  OPLL_PATCH patch[2];
  int i;

  for (i = 0; i < 19; i++)
  {
    OPLL_dump2patch (dump + i * 8, patch);
    memcpy (&opll->patch[i*2+0], &patch[0], sizeof (OPLL_PATCH));
    memcpy (&opll->patch[i*2+1], &patch[1], sizeof (OPLL_PATCH));
  }
}

void
OPLL_patch2dump (const OPLL_PATCH * patch, e_uint8 * dump)
{
  dump[0] = (e_uint8) ((patch[0].AM << 7) + (patch[0].PM << 6) + (patch[0].EG << 5) + (patch[0].KR << 4) + patch[0].ML);
  dump[1] = (e_uint8) ((patch[1].AM << 7) + (patch[1].PM << 6) + (patch[1].EG << 5) + (patch[1].KR << 4) + patch[1].ML);
  dump[2] = (e_uint8) ((patch[0].KL << 6) + patch[0].TL);
  dump[3] = (e_uint8) ((patch[1].KL << 6) + (patch[1].WF << 4) + (patch[0].WF << 3) + patch[0].FB);
  dump[4] = (e_uint8) ((patch[0].AR << 4) + patch[0].DR);
  dump[5] = (e_uint8) ((patch[1].AR << 4) + patch[1].DR);
  dump[6] = (e_uint8) ((patch[0].SL << 4) + patch[0].RR);
  dump[7] = (e_uint8) ((patch[1].SL << 4) + patch[1].RR);
  dump[8] = 0;
  dump[9] = 0;
  dump[10] = 0;
  dump[11] = 0;
  dump[12] = 0;
  dump[13] = 0;
  dump[14] = 0;
  dump[15] = 0;
}

/************************************************************

                      Calc Parameters

************************************************************/

INLINE static e_uint32
calc_eg_dphase (OPLL_SLOT * slot)
{

  switch (slot->eg_mode)
  {
  case ATTACK:
    return dphaseARTable[slot->patch->AR][slot->rks];

  case DECAY:
    return dphaseDRTable[slot->patch->DR][slot->rks];

  case SUSHOLD:
    return 0;

  case SUSTINE:
    return dphaseDRTable[slot->patch->RR][slot->rks];

  case RELEASE:
    if (slot->sustine)
      return dphaseDRTable[5][slot->rks];
    else if (slot->patch->EG)
      return dphaseDRTable[slot->patch->RR][slot->rks];
    else
      return dphaseDRTable[7][slot->rks];

  case SETTLE:
    return dphaseDRTable[15][0];

  case FINISH:
    return 0;

  default:
    return 0;
  }
}

/*************************************************************

                    OPLL internal interfaces

*************************************************************/
#define SLOT_BD1 12
#define SLOT_BD2 13
#define SLOT_HH 14
#define SLOT_SD 15
#define SLOT_TOM 16
#define SLOT_CYM 17

#define UPDATE_PG(S)  (S)->dphase = dphaseTable[(S)->fnum][(S)->block][(S)->patch->ML]
#define UPDATE_TLL(S)\
(((S)->type==0)?\
((S)->tll = tllTable[((S)->fnum)>>5][(S)->block][(S)->patch->TL][(S)->patch->KL]):\
((S)->tll = tllTable[((S)->fnum)>>5][(S)->block][(S)->volume][(S)->patch->KL]))
#define UPDATE_RKS(S) (S)->rks = rksTable[((S)->fnum)>>8][(S)->block][(S)->patch->KR]
#define UPDATE_WF(S)  (S)->sintbl = waveform[(S)->patch->WF]
#define UPDATE_EG(S)  (S)->eg_dphase = calc_eg_dphase(S)
#define UPDATE_ALL(S)\
  UPDATE_PG(S);\
  UPDATE_TLL(S);\
  UPDATE_RKS(S);\
  UPDATE_WF(S); \
  UPDATE_EG(S)                  /* EG should be updated last. */


/* Slot key on  */
INLINE static void
slotOn (OPLL_SLOT * slot)
{
  slot->eg_mode = ATTACK;
  slot->eg_phase = 0;
  slot->phase = 0;
  UPDATE_EG(slot);
}

/* Slot key on without reseting the phase */
INLINE static void
slotOn2 (OPLL_SLOT * slot)
{
  slot->eg_mode = ATTACK;
  slot->eg_phase = 0;
  UPDATE_EG(slot);
}

/* Slot key off */
INLINE static void
slotOff (OPLL_SLOT * slot)
{
  if (slot->eg_mode == ATTACK)
    slot->eg_phase = EXPAND_BITS (AR_ADJUST_TABLE[HIGHBITS (slot->eg_phase, EG_DP_BITS - EG_BITS)], EG_BITS, EG_DP_BITS);
  slot->eg_mode = RELEASE;
  UPDATE_EG(slot);
}

/* Channel key on */
INLINE static void
keyOn (OPLL * opll, e_int32 i)
{
  if (!opll->slot_on_flag[i * 2])
    slotOn (MOD(opll,i));
  if (!opll->slot_on_flag[i * 2 + 1])
    slotOn (CAR(opll,i));
  opll->key_status[i] = 1;
}

/* Channel key off */
INLINE static void
keyOff (OPLL * opll, e_int32 i)
{
  if (opll->slot_on_flag[i * 2 + 1])
    slotOff (CAR(opll,i));
  opll->key_status[i] = 0;
}

INLINE static void
keyOn_BD (OPLL * opll)
{
  keyOn (opll, 6);
}
INLINE static void
keyOn_SD (OPLL * opll)
{
  if (!opll->slot_on_flag[SLOT_SD])
    slotOn (CAR(opll,7));
}
INLINE static void
keyOn_TOM (OPLL * opll)
{
  if (!opll->slot_on_flag[SLOT_TOM])
    slotOn (MOD(opll,8));
}
INLINE static void
keyOn_HH (OPLL * opll)
{
  if (!opll->slot_on_flag[SLOT_HH])
    slotOn2 (MOD(opll,7));
}
INLINE static void
keyOn_CYM (OPLL * opll)
{
  if (!opll->slot_on_flag[SLOT_CYM])
    slotOn2 (CAR(opll,8));
}

/* Drum key off */
INLINE static void
keyOff_BD (OPLL * opll)
{
  keyOff (opll, 6);
}
INLINE static void
keyOff_SD (OPLL * opll)
{
  if (opll->slot_on_flag[SLOT_SD])
    slotOff (CAR(opll,7));
}
INLINE static void
keyOff_TOM (OPLL * opll)
{
  if (opll->slot_on_flag[SLOT_TOM])
    slotOff (MOD(opll,8));
}
INLINE static void
keyOff_HH (OPLL * opll)
{
  if (opll->slot_on_flag[SLOT_HH])
    slotOff (MOD(opll,7));
}
INLINE static void
keyOff_CYM (OPLL * opll)
{
  if (opll->slot_on_flag[SLOT_CYM])
    slotOff (CAR(opll,8));
}

/* Change a voice */
INLINE static void
setPatch (OPLL * opll, e_int32 i, e_int32 num)
{
  opll->patch_number[i] = num;
  MOD(opll,i)->patch = &opll->patch[num * 2 + 0];
  CAR(opll,i)->patch = &opll->patch[num * 2 + 1];
}

/* Change a rhythm voice */
INLINE static void
setSlotPatch (OPLL_SLOT * slot, OPLL_PATCH * patch)
{
  slot->patch = patch;
}

/* Set sustine parameter */
INLINE static void
setSustine (OPLL * opll, e_int32 c, e_int32 sustine)
{
  CAR(opll,c)->sustine = sustine;
  if (MOD(opll,c)->type)
    MOD(opll,c)->sustine = sustine;
}

/* Volume : 6bit ( Volume register << 2 ) */
INLINE static void
setVolume (OPLL * opll, e_int32 c, e_int32 volume)
{
  CAR(opll,c)->volume = volume;
}

INLINE static void
setSlotVolume (OPLL_SLOT * slot, e_int32 volume)
{
  slot->volume = volume;
}

/* Set F-Number ( fnum : 9bit ) */
INLINE static void
setFnumber (OPLL * opll, e_int32 c, e_int32 fnum)
{
  CAR(opll,c)->fnum = fnum;
  MOD(opll,c)->fnum = fnum;
}

/* Set Block data (block : 3bit ) */
INLINE static void
setBlock (OPLL * opll, e_int32 c, e_int32 block)
{
  CAR(opll,c)->block = block;
  MOD(opll,c)->block = block;
}

/* Change Rhythm Mode */
INLINE static void
update_rhythm_mode (OPLL * opll)
{
  if (opll->patch_number[6] & 0x10)
  {
    if (!(opll->slot_on_flag[SLOT_BD2] | (opll->reg[0x0e] & 0x20)))
    {
      opll->slot[SLOT_BD1].eg_mode = FINISH;
      opll->slot[SLOT_BD2].eg_mode = FINISH;
      setPatch (opll, 6, opll->reg[0x36] >> 4);
    }
  }
  else if (opll->reg[0x0e] & 0x20)
  {
    opll->patch_number[6] = 16;
    opll->slot[SLOT_BD1].eg_mode = FINISH;
    opll->slot[SLOT_BD2].eg_mode = FINISH;
    setSlotPatch (&opll->slot[SLOT_BD1], &opll->patch[16 * 2 + 0]);
    setSlotPatch (&opll->slot[SLOT_BD2], &opll->patch[16 * 2 + 1]);
  }

  if (opll->patch_number[7] & 0x10)
  {
    if (!((opll->slot_on_flag[SLOT_HH] && opll->slot_on_flag[SLOT_SD]) | (opll->reg[0x0e] & 0x20)))
    {
      opll->slot[SLOT_HH].type = 0;
      opll->slot[SLOT_HH].eg_mode = FINISH;
      opll->slot[SLOT_SD].eg_mode = FINISH;
      setPatch (opll, 7, opll->reg[0x37] >> 4);
    }
  }
  else if (opll->reg[0x0e] & 0x20)
  {
    opll->patch_number[7] = 17;
    opll->slot[SLOT_HH].type = 1;
    opll->slot[SLOT_HH].eg_mode = FINISH;
    opll->slot[SLOT_SD].eg_mode = FINISH;
    setSlotPatch (&opll->slot[SLOT_HH], &opll->patch[17 * 2 + 0]);
    setSlotPatch (&opll->slot[SLOT_SD], &opll->patch[17 * 2 + 1]);
  }

  if (opll->patch_number[8] & 0x10)
  {
    if (!((opll->slot_on_flag[SLOT_CYM] && opll->slot_on_flag[SLOT_TOM]) | (opll->reg[0x0e] & 0x20)))
    {
      opll->slot[SLOT_TOM].type = 0;
      opll->slot[SLOT_TOM].eg_mode = FINISH;
      opll->slot[SLOT_CYM].eg_mode = FINISH;
      setPatch (opll, 8, opll->reg[0x38] >> 4);
    }
  }
  else if (opll->reg[0x0e] & 0x20)
  {
    opll->patch_number[8] = 18;
    opll->slot[SLOT_TOM].type = 1;
    opll->slot[SLOT_TOM].eg_mode = FINISH;
    opll->slot[SLOT_CYM].eg_mode = FINISH;
    setSlotPatch (&opll->slot[SLOT_TOM], &opll->patch[18 * 2 + 0]);
    setSlotPatch (&opll->slot[SLOT_CYM], &opll->patch[18 * 2 + 1]);
  }
}

INLINE static void
update_key_status (OPLL * opll)
{
  int ch;

  for (ch = 0; ch < 9; ch++)
    opll->slot_on_flag[ch * 2] = opll->slot_on_flag[ch * 2 + 1] = (opll->reg[0x20 + ch]) & 0x10;

  if (opll->reg[0x0e] & 0x20)
  {
    opll->slot_on_flag[SLOT_BD1] |= (opll->reg[0x0e] & 0x10);
    opll->slot_on_flag[SLOT_BD2] |= (opll->reg[0x0e] & 0x10);
    opll->slot_on_flag[SLOT_SD] |= (opll->reg[0x0e] & 0x08);
    opll->slot_on_flag[SLOT_HH] |= (opll->reg[0x0e] & 0x01);
    opll->slot_on_flag[SLOT_TOM] |= (opll->reg[0x0e] & 0x04);
    opll->slot_on_flag[SLOT_CYM] |= (opll->reg[0x0e] & 0x02);
  }
}

void
OPLL_copyPatch (OPLL * opll, e_int32 num, OPLL_PATCH * patch)
{
  memcpy (&opll->patch[num], patch, sizeof (OPLL_PATCH));
}

/***********************************************************

                      Initializing

***********************************************************/

static void
OPLL_SLOT_reset (OPLL_SLOT * slot, int type)
{
  slot->type = type;
  slot->sintbl = waveform[0];
  slot->phase = 0;
  slot->dphase = 0;
  slot->output[0] = 0;
  slot->output[1] = 0;
  slot->feedback = 0;
  slot->eg_mode = FINISH;
  slot->eg_phase = EG_DP_WIDTH;
  slot->eg_dphase = 0;
  slot->rks = 0;
  slot->tll = 0;
  slot->sustine = 0;
  slot->fnum = 0;
  slot->block = 0;
  slot->volume = 0;
  slot->pgout = 0;
  slot->egout = 0;
  slot->patch = &null_patch;
}

static void
internal_refresh (void)
{
  makeDphaseTable ();
  makeDphaseARTable ();
  makeDphaseDRTable ();
  pm_dphase = (e_uint32) RATE_ADJUST (PM_SPEED * PM_DP_WIDTH / (clk / 72));
  am_dphase = (e_uint32) RATE_ADJUST (AM_SPEED * AM_DP_WIDTH / (clk / 72));
}

static void
maketables (e_uint32 c, e_uint32 r)
{
  if (c != clk)
  {
    clk = c;
    makePmTable ();
    makeAmTable ();
    makeDB2LinTable ();
    makeAdjustTable ();
    makeTllTable ();
    makeRksTable ();
    makeSinTable ();
    makeDefaultPatch ();
  }

  if (r != rate)
  {
    rate = r;
    internal_refresh ();
  }
}

OPLL *
OPLL_new (e_uint32 clk, e_uint32 rate)
{
  OPLL *opll;
  e_int32 i;

  maketables (clk, rate);

  opll = (OPLL *) calloc (1, sizeof (OPLL));
  if (opll == NULL)
    return NULL;

  opll->vrc7_mode = 0x00;

  for (i = 0; i < 19 * 2; i++)
    memcpy(&opll->patch[i],&null_patch,sizeof(OPLL_PATCH));
  for (i = 0; i < 14; i++)
    centre_panning( opll->pan[i] );

  opll->mask = 0;

  OPLL_reset (opll);
  OPLL_reset_patch (opll, 0);

  return opll;
}


void
OPLL_delete (OPLL * opll)
{
  free (opll);
}


/* Reset patch datas by system default. */
void
OPLL_reset_patch (OPLL * opll, e_int32 type)
{
  e_int32 i;

  for (i = 0; i < 19 * 2; i++)
    OPLL_copyPatch (opll, i, &default_patch[type % OPLL_TONE_NUM][i]);
}

/* Reset whole of OPLL except patch datas. */
void
OPLL_reset (OPLL * opll)
{
  e_int32 i;

  if (!opll)
    return;

  opll->adr = 0;
  opll->out = 0;

  opll->pm_phase = 0;
  opll->am_phase = 0;

  opll->noise_seed = 0xffff;
  //opll->mask = 0;

  for (i = 0; i <18; i++)
    OPLL_SLOT_reset(&opll->slot[i], i%2);

  for (i = 0; i < 9; i++)
  {
    opll->key_status[i] = 0;
    setPatch (opll, i, 0);
  }

  for (i = 0; i < 0x40; i++)
    OPLL_writeReg (opll, i, 0);

#ifndef EMU2413_COMPACTION
  opll->realstep = (e_uint32) ((1 << 31) / rate);
  opll->opllstep = (e_uint32) ((1 << 31) / (clk / 72));
  opll->oplltime = 0;
  /*for (i = 0; i < 14; i++)
  {
		//centre_panning( opll->pan[i] );
		opll->pan[i][0] = 1.0f;
		opll->pan[i][1] = 1.0f;
	}*/
  opll->sprev[0] = opll->sprev[1] = 0;
  opll->snext[0] = opll->snext[1] = 0;
#endif
}

/* Force Refresh (When external program changes some parameters). */
void
OPLL_forceRefresh (OPLL * opll)
{
  e_int32 i;

  if (opll == NULL)
    return;

  for (i = 0; i < 9; i++)
    setPatch(opll,i,opll->patch_number[i]);

  for (i = 0; i < 18; i++)
  {
    UPDATE_PG (&opll->slot[i]);
    UPDATE_RKS (&opll->slot[i]);
    UPDATE_TLL (&opll->slot[i]);
    UPDATE_WF (&opll->slot[i]);
    UPDATE_EG (&opll->slot[i]);
  }
}

void
OPLL_set_rate (OPLL * opll, e_uint32 r)
{
  if (opll->quality)
    rate = 49716;
  else
    rate = r;
  internal_refresh ();
  rate = r;
}

void
OPLL_set_quality (OPLL * opll, e_uint32 q)
{
  opll->quality = q;
  OPLL_set_rate (opll, rate);
}

/*********************************************************

                 Generate wave data

*********************************************************/
/* Convert Amp(0 to EG_HEIGHT) to Phase(0 to 2PI). */
#if ( SLOT_AMP_BITS - PG_BITS ) > 0
#define wave2_2pi(e)  ( (e) >> ( SLOT_AMP_BITS - PG_BITS ))
#else
#define wave2_2pi(e)  ( (e) << ( PG_BITS - SLOT_AMP_BITS ))
#endif

/* Convert Amp(0 to EG_HEIGHT) to Phase(0 to 4PI). */
#if ( SLOT_AMP_BITS - PG_BITS - 1 ) == 0
#define wave2_4pi(e)  (e)
#elif ( SLOT_AMP_BITS - PG_BITS - 1 ) > 0
#define wave2_4pi(e)  ( (e) >> ( SLOT_AMP_BITS - PG_BITS - 1 ))
#else
#define wave2_4pi(e)  ( (e) << ( 1 + PG_BITS - SLOT_AMP_BITS ))
#endif

/* Convert Amp(0 to EG_HEIGHT) to Phase(0 to 8PI). */
#if ( SLOT_AMP_BITS - PG_BITS - 2 ) == 0
#define wave2_8pi(e)  (e)
#elif ( SLOT_AMP_BITS - PG_BITS - 2 ) > 0
#define wave2_8pi(e)  ( (e) >> ( SLOT_AMP_BITS - PG_BITS - 2 ))
#else
#define wave2_8pi(e)  ( (e) << ( 2 + PG_BITS - SLOT_AMP_BITS ))
#endif

/* Update AM, PM unit */
static void
update_ampm (OPLL * opll)
{
  opll->pm_phase = (opll->pm_phase + pm_dphase) & (PM_DP_WIDTH - 1);
  opll->am_phase = (opll->am_phase + am_dphase) & (AM_DP_WIDTH - 1);
  opll->lfo_am = amtable[HIGHBITS (opll->am_phase, AM_DP_BITS - AM_PG_BITS)];
  opll->lfo_pm = pmtable[HIGHBITS (opll->pm_phase, PM_DP_BITS - PM_PG_BITS)];
}

/* PG */
INLINE static void
calc_phase (OPLL_SLOT * slot, e_int32 lfo)
{
  if (slot->patch->PM)
    slot->phase += (slot->dphase * lfo) >> PM_AMP_BITS;
  else
    slot->phase += slot->dphase;

  slot->phase &= (DP_WIDTH - 1);

  slot->pgout = HIGHBITS (slot->phase, DP_BASE_BITS);
}

/* Update Noise unit */
static void
update_noise (OPLL * opll)
{
   if(opll->noise_seed&1) opll->noise_seed ^= 0x8003020;
   opll->noise_seed >>= 1;
}

/* EG */
static void
calc_envelope (OPLL_SLOT * slot, e_int32 lfo)
{
#define S2E(x) (SL2EG((e_int32)(x/SL_STEP))<<(EG_DP_BITS-EG_BITS))

  static e_uint32 SL[16] = {
    S2E (0.0), S2E (3.0), S2E (6.0), S2E (9.0), S2E (12.0), S2E (15.0), S2E (18.0), S2E (21.0),
    S2E (24.0), S2E (27.0), S2E (30.0), S2E (33.0), S2E (36.0), S2E (39.0), S2E (42.0), S2E (48.0)
  };

  e_uint32 egout;

  switch (slot->eg_mode)
  {
  case ATTACK:
    egout = AR_ADJUST_TABLE[HIGHBITS (slot->eg_phase, EG_DP_BITS - EG_BITS)];
    slot->eg_phase += slot->eg_dphase;
    if((EG_DP_WIDTH & slot->eg_phase)||(slot->patch->AR==15))
    {
      egout = 0;
      slot->eg_phase = 0;
      slot->eg_mode = DECAY;
      UPDATE_EG (slot);
    }
    break;

  case DECAY:
    egout = HIGHBITS (slot->eg_phase, EG_DP_BITS - EG_BITS);
    slot->eg_phase += slot->eg_dphase;
    if (slot->eg_phase >= SL[slot->patch->SL])
    {
      if (slot->patch->EG)
      {
        slot->eg_phase = SL[slot->patch->SL];
        slot->eg_mode = SUSHOLD;
        UPDATE_EG (slot);
      }
      else
      {
        slot->eg_phase = SL[slot->patch->SL];
        slot->eg_mode = SUSTINE;
        UPDATE_EG (slot);
      }
    }
    break;

  case SUSHOLD:
    egout = HIGHBITS (slot->eg_phase, EG_DP_BITS - EG_BITS);
    if (slot->patch->EG == 0)
    {
      slot->eg_mode = SUSTINE;
      UPDATE_EG (slot);
    }
    break;

  case SUSTINE:
  case RELEASE:
    egout = HIGHBITS (slot->eg_phase, EG_DP_BITS - EG_BITS);
    slot->eg_phase += slot->eg_dphase;
    if (egout >= (1 << EG_BITS))
    {
      slot->eg_mode = FINISH;
      egout = (1 << EG_BITS) - 1;
    }
    break;

  case SETTLE:
    egout = HIGHBITS (slot->eg_phase, EG_DP_BITS - EG_BITS);
    slot->eg_phase += slot->eg_dphase;
    if (egout >= (1 << EG_BITS))
    {
      slot->eg_mode = ATTACK;
      egout = (1 << EG_BITS) - 1;
      UPDATE_EG(slot);
    }
    break;

  case FINISH:
    egout = (1 << EG_BITS) - 1;
    break;

  default:
    egout = (1 << EG_BITS) - 1;
    break;
  }

  if (slot->patch->AM)
    egout = EG2DB (egout + slot->tll) + lfo;
  else
    egout = EG2DB (egout + slot->tll);

  if (egout >= DB_MUTE)
    egout = DB_MUTE - 1;

  slot->egout = egout | 3;
}

/* CARRIOR */
INLINE static e_int32
calc_slot_car (OPLL_SLOT * slot, e_int32 fm)
{
  if (slot->egout >= (DB_MUTE - 1))
  {
    slot->output[0] = 0;
  }
  else
  {
    slot->output[0] = DB2LIN_TABLE[slot->sintbl[(slot->pgout+wave2_8pi(fm))&(PG_WIDTH-1)] + slot->egout];
  }

  slot->output[1] = (slot->output[1] + slot->output[0]) >> 1;
  return slot->output[1];
}

/* MODULATOR */
INLINE static e_int32
calc_slot_mod (OPLL_SLOT * slot)
{
  e_int32 fm;

  slot->output[1] = slot->output[0];

  if (slot->egout >= (DB_MUTE - 1))
  {
    slot->output[0] = 0;
  }
  else if (slot->patch->FB != 0)
  {
    fm = wave2_4pi (slot->feedback) >> (7 - slot->patch->FB);
    slot->output[0] = DB2LIN_TABLE[slot->sintbl[(slot->pgout+fm)&(PG_WIDTH-1)] + slot->egout];
  }
  else
  {
    slot->output[0] = DB2LIN_TABLE[slot->sintbl[slot->pgout] + slot->egout];
  }

  slot->feedback = (slot->output[1] + slot->output[0]) >> 1;

  return slot->feedback;

}

/* TOM */
INLINE static e_int32
calc_slot_tom (OPLL_SLOT * slot)
{
  if (slot->egout >= (DB_MUTE - 1))
    return 0;

  return DB2LIN_TABLE[slot->sintbl[slot->pgout] + slot->egout];

}

/* SNARE */
INLINE static e_int32
calc_slot_snare (OPLL_SLOT * slot, e_uint32 noise)
{
  if(slot->egout>=(DB_MUTE-1))
    return 0;

  if(BIT(slot->pgout,7))
    return DB2LIN_TABLE[(noise?DB_POS(0.0):DB_POS(15.0))+slot->egout];
  else
    return DB2LIN_TABLE[(noise?DB_NEG(0.0):DB_NEG(15.0))+slot->egout];
}

/*
  TOP-CYM
 */
INLINE static e_int32
calc_slot_cym (OPLL_SLOT * slot, e_uint32 pgout_hh)
{
  e_uint32 dbout;

  if (slot->egout >= (DB_MUTE - 1))
    return 0;
  else if(
      /* the same as fmopl.c */
      ((BIT(pgout_hh,PG_BITS-8)^BIT(pgout_hh,PG_BITS-1))|BIT(pgout_hh,PG_BITS-7)) ^
      /* different from fmopl.c */
     (BIT(slot->pgout,PG_BITS-7)&!BIT(slot->pgout,PG_BITS-5))
    )
    dbout = DB_NEG(3.0);
  else
    dbout = DB_POS(3.0);

  return DB2LIN_TABLE[dbout + slot->egout];
}

/*
  HI-HAT
*/
INLINE static e_int32
calc_slot_hat (OPLL_SLOT *slot, e_int32 pgout_cym, e_uint32 noise)
{
  e_uint32 dbout;

  if (slot->egout >= (DB_MUTE - 1))
    return 0;
  else if(
      /* the same as fmopl.c */
      ((BIT(slot->pgout,PG_BITS-8)^BIT(slot->pgout,PG_BITS-1))|BIT(slot->pgout,PG_BITS-7)) ^
      /* different from fmopl.c */
      (BIT(pgout_cym,PG_BITS-7)&!BIT(pgout_cym,PG_BITS-5))
    )
  {
    if(noise)
      dbout = DB_NEG(12.0);
    else
      dbout = DB_NEG(24.0);
  }
  else
  {
    if(noise)
      dbout = DB_POS(12.0);
    else
      dbout = DB_POS(24.0);
  }

  return DB2LIN_TABLE[dbout + slot->egout];
}

static e_int16
calc (OPLL * opll)
{
  e_int32 inst = 0, perc = 0, out = 0;
  e_int32 i;

  update_ampm (opll);
  update_noise (opll);

  for (i = 0; i < 18; i++)
  {
    calc_phase(&opll->slot[i],opll->lfo_pm);
    calc_envelope(&opll->slot[i],opll->lfo_am);
  }

  for (i = 0; i < 6; i++)
    if (!(opll->mask & OPLL_MASK_CH (i)) && (CAR(opll,i)->eg_mode != FINISH))
      inst += calc_slot_car (CAR(opll,i), calc_slot_mod(MOD(opll,i)));

 if (! opll->vrc7_mode)
 {
  /* CH6 */
  if (opll->patch_number[6] <= 15)
  {
    if (!(opll->mask & OPLL_MASK_CH (6)) && (CAR(opll,6)->eg_mode != FINISH))
      inst += calc_slot_car (CAR(opll,6), calc_slot_mod(MOD(opll,6)));
  }
  else
  {
    if (!(opll->mask & OPLL_MASK_BD) && (CAR(opll,6)->eg_mode != FINISH))
      perc += calc_slot_car (CAR(opll,6), calc_slot_mod(MOD(opll,6)));
  }

  /* CH7 */
  if (opll->patch_number[7] <= 15)
  {
    if (!(opll->mask & OPLL_MASK_CH (7)) && (CAR(opll,7)->eg_mode != FINISH))
      inst += calc_slot_car (CAR(opll,7), calc_slot_mod(MOD(opll,7)));
  }
  else
  {
    if (!(opll->mask & OPLL_MASK_HH) && (MOD(opll,7)->eg_mode != FINISH))
      perc += calc_slot_hat (MOD(opll,7), CAR(opll,8)->pgout, opll->noise_seed&1);
    if (!(opll->mask & OPLL_MASK_SD) && (CAR(opll,7)->eg_mode != FINISH))
      perc -= calc_slot_snare (CAR(opll,7), opll->noise_seed&1);
  }

  /* CH8 */
  if (opll->patch_number[8] <= 15)
  {
    if (!(opll->mask & OPLL_MASK_CH(8)) && (CAR(opll,8)->eg_mode != FINISH))
      inst += calc_slot_car (CAR(opll,8), calc_slot_mod (MOD(opll,8)));
  }
  else
  {
    if (!(opll->mask & OPLL_MASK_TOM) && (MOD(opll,8)->eg_mode != FINISH))
      perc += calc_slot_tom (MOD(opll,8));
    if (!(opll->mask & OPLL_MASK_CYM) && (CAR(opll,8)->eg_mode != FINISH))
      perc -= calc_slot_cym (CAR(opll,8), MOD(opll,7)->pgout);
  }
 } // end if (! opll->vrc7_mode)

  out = inst + (perc << 1);
  return (e_int16) out;
}

#ifdef EMU2413_COMPACTION
e_int16
OPLL_calc (OPLL * opll)
{
  return calc (opll);
}
#else
e_int16
OPLL_calc (OPLL * opll)
{
  if (!opll->quality)
    return calc (opll);

  while (opll->realstep > opll->oplltime)
  {
    opll->oplltime += opll->opllstep;
    opll->prev = opll->next;
    opll->next = calc (opll);
  }

  opll->oplltime -= opll->realstep;
  opll->out = (e_int16) (((double) opll->next * (opll->opllstep - opll->oplltime)
                          + (double) opll->prev * opll->oplltime) / opll->opllstep);

  return (e_int16) opll->out;
}
#endif

/*e_uint32
OPLL_setMask (OPLL * opll, e_uint32 mask)
{
  e_uint32 ret;

  if (opll)
  {
    ret = opll->mask;
    opll->mask = mask;
    return ret;
  }
  else
    return 0;
}*/

void OPLL_SetMuteMask(OPLL* opll, e_uint32 MuteMask)
{
	unsigned char CurChn;
	e_uint32 ChnMsk;

	for (CurChn = 0; CurChn < 14; CurChn ++)
	{
		if (CurChn < 9)
		{
			ChnMsk = OPLL_MASK_CH(CurChn);
		}
		else
		{
			switch(CurChn)
			{
			case 9:
				ChnMsk = OPLL_MASK_BD;
				break;
			case 10:
				ChnMsk = OPLL_MASK_SD;
				break;
			case 11:
				ChnMsk = OPLL_MASK_TOM;
				break;
			case 12:
				ChnMsk = OPLL_MASK_CYM;
				break;
			case 13:
				ChnMsk = OPLL_MASK_HH;
				break;
			default:
				ChnMsk = 0;
				break;
			}
		}
		if ((MuteMask >> CurChn) & 0x01)
			opll->mask |= ChnMsk;
		else
			opll->mask &= ~ChnMsk;
	}

	return;
}

/*e_uint32
OPLL_toggleMask (OPLL * opll, e_uint32 mask)
{
  e_uint32 ret;

  if (opll)
  {
    ret = opll->mask;
    opll->mask ^= mask;
    return ret;
  }
  else
    return 0;
}*/

void OPLL_SetChipMode(OPLL* opll, e_uint8 Mode)
{
	// Enable/Disable VRC7 Mode (with only 6 instead of 9 channels and no rhythm part)
	opll->vrc7_mode = Mode;

	return;
}

/****************************************************

                       I/O Ctrl

*****************************************************/

void
OPLL_writeReg (OPLL * opll, e_uint32 reg, e_uint32 data)
{
  e_int32 i, v, ch;

  data = data & 0xff;
  reg = reg & 0x3f;
  opll->reg[reg] = (e_uint8) data;

  switch (reg)
  {
  case 0x00:
    opll->patch[0].AM = (data >> 7) & 1;
    opll->patch[0].PM = (data >> 6) & 1;
    opll->patch[0].EG = (data >> 5) & 1;
    opll->patch[0].KR = (data >> 4) & 1;
    opll->patch[0].ML = (data) & 15;
    for (i = 0; i < 9; i++)
    {
      if (opll->patch_number[i] == 0)
      {
        UPDATE_PG (MOD(opll,i));
        UPDATE_RKS (MOD(opll,i));
        UPDATE_EG (MOD(opll,i));
      }
    }
    break;

  case 0x01:
    opll->patch[1].AM = (data >> 7) & 1;
    opll->patch[1].PM = (data >> 6) & 1;
    opll->patch[1].EG = (data >> 5) & 1;
    opll->patch[1].KR = (data >> 4) & 1;
    opll->patch[1].ML = (data) & 15;
    for (i = 0; i < 9; i++)
    {
      if (opll->patch_number[i] == 0)
      {
        UPDATE_PG (CAR(opll,i));
        UPDATE_RKS (CAR(opll,i));
        UPDATE_EG (CAR(opll,i));
      }
    }
    break;

  case 0x02:
    opll->patch[0].KL = (data >> 6) & 3;
    opll->patch[0].TL = (data) & 63;
    for (i = 0; i < 9; i++)
    {
      if (opll->patch_number[i] == 0)
      {
        UPDATE_TLL(MOD(opll,i));
      }
    }
    break;

  case 0x03:
    opll->patch[1].KL = (data >> 6) & 3;
    opll->patch[1].WF = (data >> 4) & 1;
    opll->patch[0].WF = (data >> 3) & 1;
    opll->patch[0].FB = (data) & 7;
    for (i = 0; i < 9; i++)
    {
      if (opll->patch_number[i] == 0)
      {
        UPDATE_WF(MOD(opll,i));
        UPDATE_WF(CAR(opll,i));
      }
    }
    break;

  case 0x04:
    opll->patch[0].AR = (data >> 4) & 15;
    opll->patch[0].DR = (data) & 15;
    for (i = 0; i < 9; i++)
    {
      if (opll->patch_number[i] == 0)
      {
        UPDATE_EG (MOD(opll,i));
      }
    }
    break;

  case 0x05:
    opll->patch[1].AR = (data >> 4) & 15;
    opll->patch[1].DR = (data) & 15;
    for (i = 0; i < 9; i++)
    {
      if (opll->patch_number[i] == 0)
      {
        UPDATE_EG(CAR(opll,i));
      }
    }
    break;

  case 0x06:
    opll->patch[0].SL = (data >> 4) & 15;
    opll->patch[0].RR = (data) & 15;
    for (i = 0; i < 9; i++)
    {
      if (opll->patch_number[i] == 0)
      {
        UPDATE_EG (MOD(opll,i));
      }
    }
    break;

  case 0x07:
    opll->patch[1].SL = (data >> 4) & 15;
    opll->patch[1].RR = (data) & 15;
    for (i = 0; i < 9; i++)
    {
      if (opll->patch_number[i] == 0)
      {
        UPDATE_EG (CAR(opll,i));
      }
    }
    break;

  case 0x0e:
    if (opll->vrc7_mode)
      break;
    update_rhythm_mode (opll);
    if (data & 0x20)
    {
      if (data & 0x10)
        keyOn_BD (opll);
      else
        keyOff_BD (opll);
      if (data & 0x8)
        keyOn_SD (opll);
      else
        keyOff_SD (opll);
      if (data & 0x4)
        keyOn_TOM (opll);
      else
        keyOff_TOM (opll);
      if (data & 0x2)
        keyOn_CYM (opll);
      else
        keyOff_CYM (opll);
      if (data & 0x1)
        keyOn_HH (opll);
      else
        keyOff_HH (opll);
    }
    update_key_status (opll);

    UPDATE_ALL (MOD(opll,6));
    UPDATE_ALL (CAR(opll,6));
    UPDATE_ALL (MOD(opll,7));
    UPDATE_ALL (CAR(opll,7));
    UPDATE_ALL (MOD(opll,8));
    UPDATE_ALL (CAR(opll,8));

    break;

  case 0x0f:
    break;

  case 0x10:
  case 0x11:
  case 0x12:
  case 0x13:
  case 0x14:
  case 0x15:
  case 0x16:
  case 0x17:
  case 0x18:
    ch = reg - 0x10;
    if (opll->vrc7_mode && ch >= 6)
      break;
    setFnumber (opll, ch, data + ((opll->reg[0x20 + ch] & 1) << 8));
    UPDATE_ALL (MOD(opll,ch));
    UPDATE_ALL (CAR(opll,ch));
    break;

  case 0x20:
  case 0x21:
  case 0x22:
  case 0x23:
  case 0x24:
  case 0x25:
  case 0x26:
  case 0x27:
  case 0x28:
    ch = reg - 0x20;
    if (opll->vrc7_mode && ch >= 6)
      break;
    setFnumber (opll, ch, ((data & 1) << 8) + opll->reg[0x10 + ch]);
    setBlock (opll, ch, (data >> 1) & 7);
    setSustine (opll, ch, (data >> 5) & 1);
	if (ch < 0x06 || ! (opll->reg[0x0E] & 0x20))
	{
		// Valley Bell Fix: prevent commands 0x26-0x28 from turning
		// the drums (BD, SD, CYM) off
		if (data & 0x10)
		  keyOn (opll, ch);
		else
		  keyOff (opll, ch);
	}
    UPDATE_ALL (MOD(opll,ch));
    UPDATE_ALL (CAR(opll,ch));
    update_key_status (opll);
    update_rhythm_mode (opll);
    break;

  case 0x30:
  case 0x31:
  case 0x32:
  case 0x33:
  case 0x34:
  case 0x35:
  case 0x36:
  case 0x37:
  case 0x38:
    if (opll->vrc7_mode && reg >= 0x36)
      break;
    i = (data >> 4) & 15;
    v = data & 15;
    if ((opll->reg[0x0e] & 0x20) && (reg >= 0x36))
    {
      switch (reg)
      {
      case 0x37:
        setSlotVolume (MOD(opll,7), i << 2);
        break;
      case 0x38:
        setSlotVolume (MOD(opll,8), i << 2);
        break;
      default:
        break;
      }
    }
    else
    {
      setPatch (opll, reg - 0x30, i);
    }
    setVolume (opll, reg - 0x30, v << 2);
    UPDATE_ALL (MOD(opll,reg - 0x30));
    UPDATE_ALL (CAR(opll,reg - 0x30));
    break;

  default:
    break;

  }
}

void
OPLL_writeIO (OPLL * opll, e_uint32 adr, e_uint32 val)
{
  if (adr & 1)
    OPLL_writeReg (opll, opll->adr, val);
  else
    opll->adr = val;
}

#ifndef EMU2413_COMPACTION
/* STEREO MODE (OPT) */
void
OPLL_set_pan (OPLL * opll, e_uint32 ch, e_int32 pan)
{
	e_uint32 fnl_ch;

	if (ch >= 14)
		return;

	if (ch < 9)
		fnl_ch = ch;
	else
		fnl_ch = 13 - (ch - 9);
	calc_panning( opll->pan[fnl_ch], pan ); // Maxim
}

static void
calc_stereo (OPLL * opll, e_int32 out[2], e_int32 ch)
{
	/* Maxim: added stereo control (multiply each side by a float in opll->pan[ch][side]) */
  e_int32 l=0,r=0;
/*  e_int32 b[4] = { 0, 0, 0, 0 };*/        /* Ignore, Right, Left, Center */
/*  e_int32 r[4] = { 0, 0, 0, 0 };*/        /* Ignore, Right, Left, Center */
  e_int32 i;
  e_int32 channel;

  if (ch < 0)
  {
    update_ampm (opll);
    update_noise (opll);

    for(i=0;i<18;i++)
    {
      calc_phase(&opll->slot[i],opll->lfo_pm);
      calc_envelope(&opll->slot[i],opll->lfo_am);
    }
  }

  for (i = 0; i < 6; i++)
  {
    if (ch >= 0 && i != ch) continue;
    if (!(opll->mask & OPLL_MASK_CH (i)) && (CAR(opll,i)->eg_mode != FINISH))
    {
      channel = calc_slot_car (CAR(opll,i), calc_slot_mod (MOD(opll,i)));
      if ( opll->pan[i][0] == 1.0f )
      {
        l += channel;
        r += channel;
      }
      else
      {
        l += (e_int32)( channel * opll->pan[i][0] );
        r += (e_int32)( channel * opll->pan[i][1] );
      }
    }
  }


  if (! opll->vrc7_mode)
  {
    if (ch < 0 || ch == 6)
    {
      if (opll->patch_number[6] <= 15)
      {
        if (!(opll->mask & OPLL_MASK_CH (6)) && (CAR(opll,6)->eg_mode != FINISH))
        {
          channel = calc_slot_car (CAR(opll,6), calc_slot_mod (MOD(opll,6)));
          if ( opll->pan[6][0] == 1.0f )
          {
            l += channel;
            r += channel;
          }
          else
          {
            l += (e_int32)( channel * opll->pan[6][0] );
            r += (e_int32)( channel * opll->pan[6][1] );
          }
        }
      }
      else
      {
        if (!(opll->mask & OPLL_MASK_BD) && (CAR(opll,6)->eg_mode != FINISH))
        {
          channel = calc_slot_car (CAR(opll,6), calc_slot_mod (MOD(opll,6))) << 1;
          if ( opll->pan[9][0] == 1.0f )
          {
            l += channel;
            r += channel;
          }
          else
          {
            l += (e_int32)( channel * opll->pan[9][0] );
            r += (e_int32)( channel * opll->pan[9][1] );
          }
        }
      }
    }

    if (ch < 0 || ch == 7)
    {
      if (opll->patch_number[7] <= 15)
      {
        if (!(opll->mask & OPLL_MASK_CH (7)) && (CAR (opll,7)->eg_mode != FINISH))
        {
          channel = calc_slot_car (CAR (opll,7), calc_slot_mod (MOD (opll,7)));
          if ( opll->pan[7][0] == 1.0f )
          {
            l += channel;
            r += channel;
          }
          else
          {
            l += (e_int32)( channel * opll->pan[7][0] );
            r += (e_int32)( channel * opll->pan[7][1] );
          }
        }
      }
      else
      {
        if (!(opll->mask & OPLL_MASK_HH) && (MOD (opll,7)->eg_mode != FINISH))
        {
          channel = calc_slot_hat (MOD (opll,7), CAR(opll,8)->pgout, opll->noise_seed&1) << 1;
          if ( opll->pan[10][0] == 1.0f )
          {
            l += channel;
            r += channel;
          }
          else
          {
            l += (e_int32)( channel * opll->pan[10][0] );
            r += (e_int32)( channel * opll->pan[10][1] );
          }
        }
        if (!(opll->mask & OPLL_MASK_SD) && (CAR (opll,7)->eg_mode != FINISH))
        {
          channel = -(calc_slot_snare (CAR (opll,7), opll->noise_seed&1) << 1); // this one is negated
          if ( opll->pan[11][0] == 1.0f )
          {
            l += channel;
            r += channel;
          }
          else
          {
            l += (e_int32)( channel * opll->pan[11][0] );
            r += (e_int32)( channel * opll->pan[11][1] );
          }
        }
      }
    }

    if (ch < 0 || ch == 8)
    {
      if (opll->patch_number[8] <= 15)
      {
        if (!(opll->mask & OPLL_MASK_CH (8)) && (CAR (opll,8)->eg_mode != FINISH))
        {
          channel = calc_slot_car (CAR (opll,8), calc_slot_mod (MOD (opll,8)));
          if ( opll->pan[8][0] == 1.0f )
          {
            l += channel;
            r += channel;
          }
          else
          {
            l += (e_int32)( channel * opll->pan[8][0] );
            r += (e_int32)( channel * opll->pan[8][1] );
          }
        }
      }
      else
      {
        if (!(opll->mask & OPLL_MASK_TOM) && (MOD (opll,8)->eg_mode != FINISH))
        {
          channel = calc_slot_tom (MOD (opll,8)) << 1;
          if ( opll->pan[12][0] == 1.0f )
          {
            l += channel;
            r += channel;
          }
          else
          {
            l += (e_int32)( channel * opll->pan[12][0] );
            r += (e_int32)( channel * opll->pan[12][1] );
          }
        }
        if (!(opll->mask & OPLL_MASK_CYM) && (CAR (opll,8)->eg_mode != FINISH))
        {
          channel = -(calc_slot_cym (CAR (opll,8), MOD(opll,7)->pgout) << 1); // negated
          if ( opll->pan[13][0] == 1.0f )
          {
            l += channel;
            r += channel;
          }
          else
          {
            l += (e_int32)( channel * opll->pan[13][0] );
            r += (e_int32)( channel * opll->pan[13][1] );
          }
        }
      }
    }
  } // end if (! opll->vrc7_mode)
/*
  out[1] = (b[1] + b[3] + ((r[1] + r[3]) << 1)) <<3;
  out[0] = (b[2] + b[3] + ((r[2] + r[3]) << 1)) <<3;
  */
  out[0] = l << 3;
  out[1] = r << 3;
}

/*void
OPLL_calc_stereo (OPLL * opll, e_int32 out[2], samples)
{
  if (!opll->quality)
  {
    calc_stereo (opll, out);
    return;
  }

  while (opll->realstep > opll->oplltime)
  {
    opll->oplltime += opll->opllstep;
    opll->sprev[0] = opll->snext[0];
    opll->sprev[1] = opll->snext[1];
    calc_stereo (opll, opll->snext);
  }

  opll->oplltime -= opll->realstep;
  out[0] = (e_int16) (((double) opll->snext[0] * (opll->opllstep - opll->oplltime)
                       + (double) opll->sprev[0] * opll->oplltime) / opll->opllstep);
  out[1] = (e_int16) (((double) opll->snext[1] * (opll->opllstep - opll->oplltime)
                       + (double) opll->sprev[1] * opll->oplltime) / opll->opllstep);
}*/
void
OPLL_advance (OPLL * opll)
{
  e_int32 buffers[2];
  calc_stereo (opll, buffers, -1);
}

void
OPLL_calc_stereo (OPLL * opll, e_int32 **out, e_int32 samples, e_int32 ch)
{
  e_int32 *bufMO = out[0];
  e_int32 *bufRO = out[1];
  e_int32 buffers[2];

  int i;

  for( i=0; i < samples ; i++ )
  {
    if (!opll->quality)
    {
      calc_stereo (opll, buffers, ch);
      bufMO[i] = buffers[0];
      bufRO[i] = buffers[1];
    }
    else
    {
      while (opll->realstep > opll->oplltime)
      {
        opll->oplltime += opll->opllstep;
        opll->sprev[0] = opll->snext[0];
        opll->sprev[1] = opll->snext[1];
        calc_stereo (opll, opll->snext, ch);
      }

      opll->oplltime -= opll->realstep;
      bufMO[i] = (e_int32) (((double) opll->snext[0] * (opll->opllstep - opll->oplltime)
                           + (double) opll->sprev[0] * opll->oplltime) / opll->opllstep);
      bufRO[i] = (e_int32) (((double) opll->snext[1] * (opll->opllstep - opll->oplltime)
                           + (double) opll->sprev[1] * opll->oplltime) / opll->opllstep);
    }
  }
}
#endif /* EMU2413_COMPACTION */
