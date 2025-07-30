/* Extended Module Player
 * Copyright (C) 1996-2023 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "common.h"
#include "virtual.h"
#include "mixer.h"
#include "precomp_lut.h"

/* Mixers
 *
 * To increase performance eight mixers are defined, one for each
 * combination of the following parameters: interpolation, resolution
 * and number of channels.
 */
#define NEAREST_NEIGHBOR() do { \
    smp_in = ((int16)sptr[pos] << 8); \
} while (0)

#define NEAREST_NEIGHBOR_16BIT() do { \
    smp_in = sptr[pos]; \
} while (0)

#define LINEAR_INTERP() do { \
    smp_l1 = ((int16)sptr[pos] << 8); \
    smp_dt = ((int16)sptr[pos + 1] << 8) - smp_l1; \
    smp_in = smp_l1 + (((frac >> 1) * smp_dt) >> (SMIX_SHIFT - 1)); \
} while (0)

#define LINEAR_INTERP_16BIT() do { \
    smp_l1 = sptr[pos]; \
    smp_dt = sptr[pos + 1] - smp_l1; \
    smp_in = smp_l1 + (((frac >> 1) * smp_dt) >> (SMIX_SHIFT - 1)); \
} while (0)

/* The following lut settings are PRECOMPUTED. If you plan on changing these
 * settings, you MUST also regenerate the arrays.
 */
/* number of bits used to scale spline coefs */
#define SPLINE_QUANTBITS  14
#define SPLINE_SHIFT    (SPLINE_QUANTBITS)

/* log2(number) of precalculated splines (range is [4..14]) */
#define SPLINE_FRACBITS 10
#define SPLINE_LUTLEN (1L<<SPLINE_FRACBITS)

#define SPLINE_FRACSHIFT ((16 - SPLINE_FRACBITS) - 2)
#define SPLINE_FRACMASK  (((1L << (16 - SPLINE_FRACSHIFT)) - 1) & ~3)

#define SPLINE_INTERP() do { \
    int f = frac >> 6; \
    smp_in = (cubic_spline_lut0[f] * sptr[(int)pos - 1] + \
              cubic_spline_lut1[f] * sptr[pos    ] + \
              cubic_spline_lut3[f] * sptr[pos + 2] + \
              cubic_spline_lut2[f] * sptr[pos + 1]) >> (SPLINE_SHIFT - 8); \
} while (0)

#define SPLINE_INTERP_16BIT() do { \
    int f = frac >> 6; \
    smp_in = (cubic_spline_lut0[f] * sptr[(int)pos - 1] + \
              cubic_spline_lut1[f] * sptr[pos    ] + \
              cubic_spline_lut3[f] * sptr[pos + 2] + \
              cubic_spline_lut2[f] * sptr[pos + 1]) >> SPLINE_SHIFT; \
} while (0)

#define LOOP_AC for (; count > ramp; count--)

#define LOOP for (; count; count--)

#define UPDATE_POS() do { \
    frac += step; \
    pos += frac >> SMIX_SHIFT; \
    frac &= SMIX_MASK; \
} while (0)

#define MIX_MONO() do { \
    *(buffer++) += smp_in * vl; \
} while (0)

#define MIX_MONO_AC() do { \
    *(buffer++) += smp_in * (old_vl >> 8); old_vl += delta_l; \
} while (0)

/* IT's WAV output driver uses a clamp that seems to roughly match this:
 * compare the WAV output of OpenMPT env-flt-max.it and filter-reset.it */
#define MIX_FILTER_CLAMP(a) CLAMP((a), -65536, 65535)

#define MIX_MONO_FILTER() do { \
    sl = (a0 * smp_in + b0 * fl1 + b1 * fl2) >> FILTER_SHIFT; \
    MIX_FILTER_CLAMP(sl); \
    fl2 = fl1; fl1 = sl; \
    *(buffer++) += sl * vl; \
} while (0)

#define MIX_MONO_FILTER_AC() do { \
    int vl = old_vl >> 8; \
    MIX_MONO_FILTER(); \
    old_vl += delta_l; \
} while (0)

#define MIX_STEREO() do { \
    *(buffer++) += smp_in * vr; \
    *(buffer++) += smp_in * vl; \
} while (0)

#define MIX_STEREO_AC() do { \
    *(buffer++) += smp_in * (old_vr >> 8); old_vr += delta_r; \
    *(buffer++) += smp_in * (old_vl >> 8); old_vl += delta_l; \
} while (0)

#define MIX_STEREO_FILTER() do { \
    sr = (a0 * smp_in + b0 * fr1 + b1 * fr2) >> FILTER_SHIFT; \
    MIX_FILTER_CLAMP(sr); \
    fr2 = fr1; fr1 = sr; \
    sl = (a0 * smp_in + b0 * fl1 + b1 * fl2) >> FILTER_SHIFT; \
    MIX_FILTER_CLAMP(sl); \
    fl2 = fl1; fl1 = sl; \
    *(buffer++) += sr * vr; \
    *(buffer++) += sl * vl; \
} while (0)

#define MIX_STEREO_FILTER_AC() do { \
    int vr = old_vr >> 8; \
    int vl = old_vl >> 8; \
    MIX_STEREO_FILTER(); \
    old_vr += delta_r; \
    old_vl += delta_l; \
} while (0)

/* For "nearest" to be nearest neighbor (instead of floor), the position needs
 * to be rounded. This only needs to be done once at the start of mixing, and
 * is required for reverse samples to round the same as forward samples.
 */
#define NEAREST_ROUND() do { \
    frac += (1 << (SMIX_SHIFT - 1)); \
    pos += frac >> SMIX_SHIFT; \
    frac &= SMIX_MASK; \
} while (0)

#define VAR_NORM(x) \
    register int smp_in; \
    x *sptr = (x *)vi->sptr; \
    unsigned int pos = vi->pos; \
    int frac = (1 << SMIX_SHIFT) * (vi->pos - (int)vi->pos)

#define VAR_LINEAR_MONO(x) \
    VAR_NORM(x); \
    int old_vl = vi->old_vl; \
    int smp_l1, smp_dt

#define VAR_LINEAR_STEREO(x) \
    VAR_LINEAR_MONO(x); \
    int old_vr = vi->old_vr

#define VAR_SPLINE_MONO(x) \
    int old_vl = vi->old_vl; \
    VAR_NORM(x)

#define VAR_SPLINE_STEREO(x) \
    VAR_SPLINE_MONO(x); \
    int old_vr = vi->old_vr

#ifndef LIBXMP_CORE_DISABLE_IT

#define VAR_FILTER_MONO \
    int fl1 = vi->filter.l1, fl2 = vi->filter.l2; \
    int64 a0 = vi->filter.a0, b0 = vi->filter.b0, b1 = vi->filter.b1; \
    int sl

#define VAR_FILTER_STEREO \
    VAR_FILTER_MONO; \
    int fr1 = vi->filter.r1, fr2 = vi->filter.r2; \
    int sr

#define SAVE_FILTER_MONO() do { \
    vi->filter.l1 = fl1; \
    vi->filter.l2 = fl2; \
} while (0)

#define SAVE_FILTER_STEREO() do { \
    SAVE_FILTER_MONO(); \
    vi->filter.r1 = fr1; \
    vi->filter.r2 = fr2; \
} while (0)

#endif

#ifdef _MSC_VER
#pragma warning(disable:4457) /* shadowing */
#endif


/*
 * Nearest neighbor mixers
 */

/* Handler for 8 bit samples, nearest neighbor mono output
 */
MIXER(mono_8bit_nearest)
{
    VAR_NORM(int8);
    NEAREST_ROUND();

    LOOP { NEAREST_NEIGHBOR(); MIX_MONO(); UPDATE_POS(); }
}


/* Handler for 16 bit samples, nearest neighbor mono output
 */
MIXER(mono_16bit_nearest)
{
    VAR_NORM(int16);
    NEAREST_ROUND();

    LOOP { NEAREST_NEIGHBOR_16BIT(); MIX_MONO(); UPDATE_POS(); }
}

/* Handler for 8 bit samples, nearest neighbor stereo output
 */
MIXER(stereo_8bit_nearest)
{
    VAR_NORM(int8);
    NEAREST_ROUND();

    LOOP { NEAREST_NEIGHBOR(); MIX_STEREO(); UPDATE_POS(); }
}

/* Handler for 16 bit samples, nearest neighbor stereo output
 */
MIXER(stereo_16bit_nearest)
{
    VAR_NORM(int16);
    NEAREST_ROUND();

    LOOP { NEAREST_NEIGHBOR_16BIT(); MIX_STEREO(); UPDATE_POS(); }
}


/*
 * Linear mixers
 */

/* Handler for 8 bit samples, linear interpolated mono output
 */
MIXER(mono_8bit_linear)
{
    VAR_LINEAR_MONO(int8);

    LOOP_AC { LINEAR_INTERP(); MIX_MONO_AC(); UPDATE_POS(); }
    LOOP    { LINEAR_INTERP(); MIX_MONO(); UPDATE_POS(); }
}

/* Handler for 16 bit samples, linear interpolated mono output
 */
MIXER(mono_16bit_linear)
{
    VAR_LINEAR_MONO(int16);

    LOOP_AC { LINEAR_INTERP_16BIT(); MIX_MONO_AC(); UPDATE_POS(); }
    LOOP    { LINEAR_INTERP_16BIT(); MIX_MONO(); UPDATE_POS(); }
}

/* Handler for 8 bit samples, linear interpolated stereo output
 */
MIXER(stereo_8bit_linear)
{
   VAR_LINEAR_STEREO(int8);

    LOOP_AC { LINEAR_INTERP(); MIX_STEREO_AC(); UPDATE_POS(); }
    LOOP    { LINEAR_INTERP(); MIX_STEREO(); UPDATE_POS(); }
}

/* Handler for 16 bit samples, linear interpolated stereo output
 */
MIXER(stereo_16bit_linear)
{
    VAR_LINEAR_STEREO(int16);

    LOOP_AC { LINEAR_INTERP_16BIT(); MIX_STEREO_AC(); UPDATE_POS(); }
    LOOP    { LINEAR_INTERP_16BIT(); MIX_STEREO(); UPDATE_POS(); }
}


#ifndef LIBXMP_CORE_DISABLE_IT

/* Handler for 8 bit samples, filtered linear interpolated mono output
 */
MIXER(mono_8bit_linear_filter)
{
    VAR_LINEAR_MONO(int8);
    VAR_FILTER_MONO;

    LOOP_AC { LINEAR_INTERP(); MIX_MONO_FILTER_AC(); UPDATE_POS(); }
    LOOP    { LINEAR_INTERP(); MIX_MONO_FILTER(); UPDATE_POS(); }

    SAVE_FILTER_MONO();
}

/* Handler for 16 bit samples, filtered linear interpolated mono output
 */
MIXER(mono_16bit_linear_filter)
{
    VAR_LINEAR_MONO(int16);
    VAR_FILTER_MONO;

    LOOP_AC { LINEAR_INTERP_16BIT(); MIX_MONO_FILTER_AC(); UPDATE_POS(); }
    LOOP    { LINEAR_INTERP_16BIT(); MIX_MONO_FILTER(); UPDATE_POS(); }

    SAVE_FILTER_MONO();
}

/* Handler for 8 bit samples, filtered linear interpolated stereo output
 */
MIXER(stereo_8bit_linear_filter)
{
    VAR_LINEAR_STEREO(int8);
    VAR_FILTER_STEREO;

    LOOP_AC { LINEAR_INTERP(); MIX_STEREO_FILTER_AC(); UPDATE_POS(); }
    LOOP    { LINEAR_INTERP(); MIX_STEREO_FILTER(); UPDATE_POS(); }

    SAVE_FILTER_STEREO();
}

/* Handler for 16 bit samples, filtered linear interpolated stereo output
 */
MIXER(stereo_16bit_linear_filter)
{
    VAR_LINEAR_STEREO(int16);
    VAR_FILTER_STEREO;

    LOOP_AC { LINEAR_INTERP_16BIT(); MIX_STEREO_FILTER_AC(); UPDATE_POS(); }
    LOOP    { LINEAR_INTERP_16BIT(); MIX_STEREO_FILTER(); UPDATE_POS(); }

    SAVE_FILTER_STEREO();
}

#endif

/*
 * Spline mixers
 */

/* Handler for 8 bit samples, spline interpolated mono output
 */
MIXER(mono_8bit_spline)
{
    VAR_SPLINE_MONO(int8);

    LOOP_AC { SPLINE_INTERP(); MIX_MONO_AC(); UPDATE_POS(); }
    LOOP    { SPLINE_INTERP(); MIX_MONO(); UPDATE_POS(); }
}

/* Handler for 16 bit samples, spline interpolated mono output
 */
MIXER(mono_16bit_spline)
{
    VAR_SPLINE_MONO(int16);

    LOOP_AC { SPLINE_INTERP_16BIT(); MIX_MONO_AC(); UPDATE_POS(); }
    LOOP    { SPLINE_INTERP_16BIT(); MIX_MONO(); UPDATE_POS(); }
}

/* Handler for 8 bit samples, spline interpolated stereo output
 */
MIXER(stereo_8bit_spline)
{
    VAR_SPLINE_STEREO(int8);

    LOOP_AC { SPLINE_INTERP(); MIX_STEREO_AC(); UPDATE_POS(); }
    LOOP    { SPLINE_INTERP(); MIX_STEREO(); UPDATE_POS(); }
}

/* Handler for 16 bit samples, spline interpolated stereo output
 */
MIXER(stereo_16bit_spline)
{
    VAR_SPLINE_STEREO(int16);

    LOOP_AC { SPLINE_INTERP_16BIT(); MIX_STEREO_AC(); UPDATE_POS(); }
    LOOP    { SPLINE_INTERP_16BIT(); MIX_STEREO(); UPDATE_POS(); }
}

#ifndef LIBXMP_CORE_DISABLE_IT

/* Handler for 8 bit samples, filtered spline interpolated mono output
 */
MIXER(mono_8bit_spline_filter)
{
    VAR_SPLINE_MONO(int8);
    VAR_FILTER_MONO;

    LOOP_AC { SPLINE_INTERP(); MIX_MONO_FILTER_AC(); UPDATE_POS(); }
    LOOP    { SPLINE_INTERP(); MIX_MONO_FILTER(); UPDATE_POS(); }

    SAVE_FILTER_MONO();
}

/* Handler for 16 bit samples, filtered spline interpolated mono output
 */
MIXER(mono_16bit_spline_filter)
{
    VAR_SPLINE_MONO(int16);
    VAR_FILTER_MONO;

    LOOP_AC { SPLINE_INTERP_16BIT(); MIX_MONO_FILTER_AC(); UPDATE_POS(); }
    LOOP    { SPLINE_INTERP_16BIT(); MIX_MONO_FILTER(); UPDATE_POS(); }

    SAVE_FILTER_MONO();
}

/* Handler for 8 bit samples, filtered spline interpolated stereo output
 */
MIXER(stereo_8bit_spline_filter)
{
    VAR_SPLINE_STEREO(int8);
    VAR_FILTER_STEREO;

    LOOP_AC { SPLINE_INTERP(); MIX_STEREO_FILTER_AC(); UPDATE_POS(); }
    LOOP    { SPLINE_INTERP(); MIX_STEREO_FILTER(); UPDATE_POS(); }

    SAVE_FILTER_STEREO();
}

/* Handler for 16 bit samples, filtered spline interpolated stereo output
 */
MIXER(stereo_16bit_spline_filter)
{
    VAR_SPLINE_STEREO(int16);
    VAR_FILTER_STEREO;

    LOOP_AC { SPLINE_INTERP_16BIT(); MIX_STEREO_FILTER_AC(); UPDATE_POS(); }
    LOOP    { SPLINE_INTERP_16BIT(); MIX_STEREO_FILTER(); UPDATE_POS(); }

    SAVE_FILTER_STEREO();
}

#endif
