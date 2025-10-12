/* SPDX-License-Identifier: MIT */
/****************************************************************************

  emu2149.c -- YM2149/AY-3-8910 emulator by Mitsutaka Okazaki 2001

  2001 04-28 : Version 1.00beta -- 1st Beta Release.
  2001 08-14 : Version 1.10
  2001 10-03 : Version 1.11     -- Added PSG_set_quality().
  2002 03-02 : Version 1.12     -- Removed PSG_init & PSG_close.
  2002 10-13 : Version 1.14     -- Fixed the envelope unit.
  2003 09-19 : Version 1.15     -- Added PSG_setMask and PSG_toggleMask
  2004 01-11 : Version 1.16     -- Fixed an envelope problem where the envelope
                                   frequency register is written before key-on.

  References:
    psg.vhd        -- 2000 written by Kazuhiro Tsujikawa.
    s_fme7.c       -- 1999,2000 written by Mamiya (NEZplug).
    ay8910.c       -- 1998-2001 Author unknown (MAME).
    MSX-Datapack   -- 1991 ASCII Corp.
    AY-3-8910 data sheet

*****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "emu2149.h"

static UINT32 voltbl[2][32] = {
  {0x00, 0x01, 0x01, 0x02, 0x02, 0x03, 0x03, 0x04, 0x05, 0x06, 0x07, 0x09,
   0x0B, 0x0D, 0x0F, 0x12,
   0x16, 0x1A, 0x1F, 0x25, 0x2D, 0x35, 0x3F, 0x4C, 0x5A, 0x6A, 0x7F, 0x97,
   0xB4, 0xD6, 0xEB, 0xFF},
  {0x00, 0x00, 0x01, 0x01, 0x02, 0x02, 0x03, 0x03, 0x05, 0x05, 0x07, 0x07,
   0x0B, 0x0B, 0x0F, 0x0F,
   0x16, 0x16, 0x1F, 0x1F, 0x2D, 0x2D, 0x3F, 0x3F, 0x5A, 0x5A, 0x7F, 0x7F,
   0xB4, 0xB4, 0xFF, 0xFF}
};

#define GETA_BITS 24

static void
internal_refresh (PSG * psg)
{
  if (psg->quality)
  {
    psg->base_incr = 1 << GETA_BITS;
    psg->realstep = (UINT32) ((1 << 31) / psg->rate);
    psg->psgstep = (UINT32) ((1 << 31) / (psg->clk / 8));
    psg->psgtime = 0;
  }
  else
  {
    psg->base_incr =
      (UINT32) ((double) psg->clk * (1 << GETA_BITS) / (8.0 * psg->rate));
  }
}

EMU2149_API void
PSG_set_clock(PSG * psg, UINT32 c)
{
  psg->clk = c;
  internal_refresh(psg);
}

EMU2149_API void
PSG_set_rate (PSG * psg, UINT32 r)
{
  psg->rate = r ? r : 44100;
  internal_refresh (psg);
}

EMU2149_API void
PSG_set_quality (PSG * psg, UINT32 q)
{
  psg->quality = q;
  internal_refresh (psg);
}

EMU2149_API void
PSG_init (PSG * psg, UINT32 c, UINT32 r)
{
  memset(psg, 0x00, sizeof(PSG));

  PSG_setVolumeMode (psg, EMU2149_VOL_DEFAULT);
  psg->clk = c;
  psg->rate = r ? r : 44100;
  PSG_set_quality (psg, 0);
  psg->stereo_mask[0] = 0x03;
  psg->stereo_mask[1] = 0x03;
  psg->stereo_mask[2] = 0x03;
}

EMU2149_API void
PSG_setFlags (PSG * psg, UINT8 flags)
{
	if (flags & EMU2149_ZX_STEREO)
	{
		/* ABC Stereo */
		psg->stereo_mask[0] = 0x01;
		psg->stereo_mask[1] = 0x03;
		psg->stereo_mask[2] = 0x02;
	}
	else
	{
		psg->stereo_mask[0] = 0x03;
		psg->stereo_mask[1] = 0x03;
		psg->stereo_mask[2] = 0x03;
	}

	return;
}

EMU2149_API void
PSG_setVolumeMode (PSG * psg, int type)
{
  switch (type)
  {
  case 1:
    psg->voltbl = voltbl[EMU2149_VOL_YM2149];
    break;
  case 2:
    psg->voltbl = voltbl[EMU2149_VOL_AY_3_8910];
    break;
  default:
    psg->voltbl = voltbl[EMU2149_VOL_DEFAULT];
    break;
  }
}

EMU2149_API UINT32
PSG_setMask (PSG *psg, UINT32 mask)
{
  UINT32 ret = 0;
  if(psg)
  {
    ret = psg->mask;
    psg->mask = mask;
  }
  return ret;
}

EMU2149_API void
PSG_setStereoMask (PSG *psg, UINT32 mask)
{
  if(psg)
  {
    psg->stereo_mask[0] = (mask >>0) &3;
    psg->stereo_mask[1] = (mask >>2) &3;
    psg->stereo_mask[2] = (mask >>4) &3;
  }
}

EMU2149_API UINT32
PSG_toggleMask (PSG *psg, UINT32 mask)
{
  UINT32 ret = 0;
  if(psg)
  {
    ret = psg->mask;
    psg->mask ^= mask;
  }
  return ret;
}

EMU2149_API void
PSG_reset (PSG * psg)
{
  int i;

  psg->base_count = 0;

  for (i = 0; i < 3; i++)
  {
    psg->cout[i] = 0;
    psg->count[i] = 0x1000;
    psg->freq[i] = 0;
    psg->edge[i] = 0;
    psg->volume[i] = 0;
  }

  psg->mask = 0;

  for (i = 0; i < 16; i++)
    psg->reg[i] = 0;
  psg->adr = 0;

  psg->noise_seed = 0xffff;
  psg->noise_count = 0x40;
  psg->noise_freq = 0;

  psg->env_volume = 0;
  psg->env_ptr = 0;
  psg->env_freq = 0;
  psg->env_count = 0;
  psg->env_pause = 1;

  psg->out = 0;
}

EMU2149_API void
PSG_delete (PSG * psg)
{
  free (psg);
}

EMU2149_API UINT8
PSG_readIO (PSG * psg)
{
  return (UINT8) (psg->reg[psg->adr]);
}

EMU2149_API UINT8
PSG_readReg (PSG * psg, UINT32 reg)
{
  return (UINT8) (psg->reg[reg & 0x1f]);

}

EMU2149_API void
PSG_writeIO (PSG * psg, UINT32 adr, UINT32 val)
{
  if (adr & 1)
    PSG_writeReg (psg, psg->adr, val);
  else
    psg->adr = val & 0x1f;
}

/*INLINE*/ static INT16
calc (PSG * psg)
{

  int i, noise;
  UINT32 incr;
  INT32 mix = 0;

  psg->base_count += psg->base_incr;
  incr = (psg->base_count >> GETA_BITS);
  psg->base_count &= (1 << GETA_BITS) - 1;

  /* Envelope */
  psg->env_count += incr;
  while (psg->env_count>=0x10000 && psg->env_freq!=0)
  {
    if (!psg->env_pause)
    {
      if(psg->env_face)
        psg->env_ptr = (psg->env_ptr + 1) & 0x3f ;
      else
        psg->env_ptr = (psg->env_ptr + 0x3f) & 0x3f;
    }

    if (psg->env_ptr & 0x20) /* if carry or borrow */
    {
      if (psg->env_continue)
      {
        if (psg->env_alternate^psg->env_hold) psg->env_face ^= 1;
        if (psg->env_hold) psg->env_pause = 1;
        psg->env_ptr = psg->env_face?0:0x1f;
      }
      else
      {
        psg->env_pause = 1;
        psg->env_ptr = 0;
      }
    }

    psg->env_count -= psg->env_freq;
  }

  /* Noise */
  psg->noise_count += incr;
  if (psg->noise_count & 0x40)
  {
    if (psg->noise_seed & 1)
      psg->noise_seed ^= 0x24000;
    psg->noise_seed >>= 1;
    psg->noise_count -= psg->noise_freq;
  }
  noise = psg->noise_seed & 1;

  /* Tone */
  for (i = 0; i < 3; i++)
  {
    psg->count[i] += incr;
    if (psg->count[i] & 0x1000)
    {
      if (psg->freq[i] > 1)
      {
        psg->edge[i] = !psg->edge[i];
        psg->count[i] -= psg->freq[i];
      }
      else
      {
        psg->edge[i] = 1;
      }
    }

    psg->cout[i] = 0; /* BS maintaining cout for stereo mix */

    if (psg->mask&PSG_MASK_CH(i))
      continue;

    if ((psg->tmask[i] || psg->edge[i]) && (psg->nmask[i] || noise))
    {
      if (!(psg->volume[i] & 32))
        psg->cout[i] = psg->voltbl[psg->volume[i] & 31];
      else
        psg->cout[i] = psg->voltbl[psg->env_ptr];

      mix += psg->cout[i];
    }
  }

  return (INT16) mix;
}

EMU2149_API INT16
PSG_calc (PSG * psg)
{
  if (!psg->quality)
    return (INT16) (calc (psg) << 4);

  /* Simple rate converter */
  while (psg->realstep > psg->psgtime)
  {
    psg->psgtime += psg->psgstep;
    psg->out += calc (psg);
    psg->out >>= 1;
  }

  psg->psgtime = psg->psgtime - psg->realstep;

  return (INT16) (psg->out << 4);
}

/*INLINE*/ static void
calc_stereo (PSG * psg, INT32 out[2])
{
  int i, noise;
  UINT32 incr;
  INT32 l = 0, r = 0;

  psg->base_count += psg->base_incr;
  incr = (psg->base_count >> GETA_BITS);
  psg->base_count &= (1 << GETA_BITS) - 1;

  /* Envelope */
  psg->env_count += incr;
  while (psg->env_count>=0x10000 && psg->env_freq!=0)
  {
    if (!psg->env_pause)
    {
      if(psg->env_face)
        psg->env_ptr = (psg->env_ptr + 1) & 0x3f ;
      else
        psg->env_ptr = (psg->env_ptr + 0x3f) & 0x3f;
    }

    if (psg->env_ptr & 0x20) /* if carry or borrow */
    {
      if (psg->env_continue)
      {
        if (psg->env_alternate^psg->env_hold) psg->env_face ^= 1;
        if (psg->env_hold) psg->env_pause = 1;
        psg->env_ptr = psg->env_face?0:0x1f;
      }
      else
      {
        psg->env_pause = 1;
        psg->env_ptr = 0;
      }
    }

    psg->env_count -= psg->env_freq;
  }

  /* Noise */
  psg->noise_count += incr;
  if (psg->noise_count & 0x40)
  {
    if (psg->noise_seed & 1)
      psg->noise_seed ^= 0x24000;
    psg->noise_seed >>= 1;
    psg->noise_count -= psg->noise_freq;
  }
  noise = psg->noise_seed & 1;

  /* Tone */
  for (i = 0; i < 3; i++)
  {
    psg->count[i] += incr;
    if (psg->count[i] & 0x1000)
    {
      if (psg->freq[i] > 1)
      {
        psg->edge[i] = !psg->edge[i];
        psg->count[i] -= psg->freq[i];
      }
      else
      {
        psg->edge[i] = 1;
      }
    }

    psg->cout[i] = 0; /* BS maintaining cout for stereo mix */

    if (psg->mask&PSG_MASK_CH(i))
      continue;

    if ((psg->tmask[i] || psg->edge[i]) && (psg->nmask[i] || noise))
    {
      if (!(psg->volume[i] & 32))
        psg->cout[i] = psg->voltbl[psg->volume[i] & 31];
      else
        psg->cout[i] = psg->voltbl[psg->env_ptr];

      if (psg->stereo_mask[i] & 0x01)
        l += psg->cout[i];
      if (psg->stereo_mask[i] & 0x02)
        r += psg->cout[i];
    }
  }

  out[0] = l << 5;
  out[1] = r << 5;

  return;
}

EMU2149_API void
PSG_calc_stereo (PSG * psg, INT32 **out, INT32 samples)
{
  INT32 *bufMO = out[0];
  INT32 *bufRO = out[1];
  INT32 buffers[2];

  int i;

  for (i = 0; i < samples; i ++)
  {
    if (!psg->quality)
    {
      calc_stereo (psg, buffers);
      bufMO[i] = buffers[0];
      bufRO[i] = buffers[1];
    }
    else
    {
      while (psg->realstep > psg->psgtime)
      {
        psg->psgtime += psg->psgstep;
        psg->sprev[0] = psg->snext[0];
        psg->sprev[1] = psg->snext[1];
        calc_stereo (psg, psg->snext);
      }

      psg->psgtime -= psg->realstep;
      bufMO[i] = (INT32) (((double) psg->snext[0] * (psg->psgstep - psg->psgtime)
                         + (double) psg->sprev[0] * psg->psgtime) / psg->psgstep);
      bufRO[i] = (INT32) (((double) psg->snext[1] * (psg->psgstep - psg->psgtime)
                         + (double) psg->sprev[1] * psg->psgtime) / psg->psgstep);
    }
  }
}

EMU2149_API void
PSG_writeReg (PSG * psg, UINT32 reg, UINT32 val)
{
  int c;

  if (reg > 15) return;

  psg->reg[reg] = (UINT8) (val & 0xff);
  switch (reg)
  {
  case 0:
  case 2:
  case 4:
  case 1:
  case 3:
  case 5:
    c = reg >> 1;
    psg->freq[c] = ((psg->reg[c * 2 + 1] & 15) << 8) + psg->reg[c * 2];
    break;

  case 6:
    psg->noise_freq = (val == 0) ? 1 : ((val & 31) << 1);
    break;

  case 7:
    psg->tmask[0] = (val & 1);
    psg->tmask[1] = (val & 2);
    psg->tmask[2] = (val & 4);
    psg->nmask[0] = (val & 8);
    psg->nmask[1] = (val & 16);
    psg->nmask[2] = (val & 32);
    break;

  case 8:
  case 9:
  case 10:
    psg->volume[reg - 8] = val << 1;

    break;

  case 11:
  case 12:
    psg->env_freq = (psg->reg[12] << 8) + psg->reg[11];
    break;

  case 13:
    psg->env_continue = (val >> 3) & 1;
    psg->env_attack = (val >> 2) & 1;
    psg->env_alternate = (val >> 1) & 1;
    psg->env_hold = val & 1;
    psg->env_face = psg->env_attack;
    psg->env_pause = 0;
    psg->env_count = 0x10000 - psg->env_freq;
    psg->env_ptr = psg->env_face?0:0x1f;
    break;

  case 14:
  case 15:
  default:
    break;
  }

  return;
}
