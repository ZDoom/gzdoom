/* Extended Module Player
 * Copyright (C) 1996-2024 Claudio Matsuoka and Hipolito Carraro Jr
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
#include "mixer.h"
#include "precomp_lut.h"

#if defined(__cplusplus) && (__cplusplus >= 201402L)
#define REGISTER
#else
#define REGISTER register
#endif

/* Mixers
 *
 * To increase performance eight mixers are defined, one for each
 * combination of the following parameters: interpolation, resolution
 * and number of channels.
 */
#define NEAREST_8BIT(smp_in, off) do { \
    (smp_in) = ((int16)sptr[pos + (off)] << 8); \
} while (0)

#define NEAREST_16BIT(smp_in, off) do { \
    (smp_in) = sptr[pos + (off)]; \
} while (0)

#define LINEAR_8BIT(smp_in, off) do { \
    smp_l1 = ((int16)sptr[pos + (off)] << 8); \
    smp_dt = ((int16)sptr[pos + (off) + chn] << 8) - smp_l1; \
    (smp_in) = smp_l1 + (((frac >> 1) * smp_dt) >> (SMIX_SHIFT - 1)); \
} while (0)

#define LINEAR_16BIT(smp_in, off) do { \
    smp_l1 = sptr[pos + (off)]; \
    smp_dt = sptr[pos + (off) + chn] - smp_l1; \
    (smp_in) = smp_l1 + (((frac >> 1) * smp_dt) >> (SMIX_SHIFT - 1)); \
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

#define SPLINE_8BIT(smp_in, off) do { \
    int f = frac >> 6; \
    (smp_in) = (cubic_spline_lut0[f] * sptr[pos + (off) - chn] + \
                cubic_spline_lut1[f] * sptr[pos + (off)   ] + \
                cubic_spline_lut3[f] * sptr[pos + (off) + (chn << 1)] + \
                cubic_spline_lut2[f] * sptr[pos + (off) + chn]) >> (SPLINE_SHIFT - 8); \
} while (0)

#define SPLINE_16BIT(smp_in, off) do { \
    int f = frac >> 6; \
    (smp_in) = (cubic_spline_lut0[f] * sptr[pos + (off) - chn] + \
                cubic_spline_lut1[f] * sptr[pos + (off)   ] + \
                cubic_spline_lut3[f] * sptr[pos + (off) + (chn << 1)] + \
                cubic_spline_lut2[f] * sptr[pos + (off) + chn]) >> SPLINE_SHIFT; \
} while (0)

#define LOOP_AC for (; count > ramp; count--)

#define LOOP for (; count; count--)

#define UPDATE_POS() do { \
    frac += step; \
    pos += (frac >> SMIX_SHIFT) * (chn); \
    frac &= SMIX_MASK; \
} while (0)

/* Sample pre-amplification is required to fix filter rounding errors
 * at high sample rates. The non-filtered mixers do not need this. */
#define PREAMP_BITS 15

/* IT's WAV output driver uses a clamp that seems to roughly match this:
 * compare the WAV output of OpenMPT env-flt-max.it and filter-reset.it */
#define FILTER_MIN (-65536 * (1 << PREAMP_BITS))
#define FILTER_MAX (65535 * (1 << PREAMP_BITS))
#define MIX_FILTER_CLAMP(a) \
 ((a) < FILTER_MIN ? FILTER_MIN : (a) > FILTER_MAX ? FILTER_MAX : (a))

#define FILTER_LEFT(smp_in_l) do { \
    sl64 = (a0 * ((smp_in_l) << PREAMP_BITS) + b0 * fl1 + b1 * fl2) >> FILTER_SHIFT; \
    sl = MIX_FILTER_CLAMP(sl64); \
    fl2 = fl1; fl1 = sl; \
    (smp_in_l) = sl >> PREAMP_BITS; \
} while (0)

#define FILTER_RIGHT(smp_in_r) do { \
    sr64 = (a0 * ((smp_in_r) << PREAMP_BITS) + b0 * fr1 + b1 * fr2) >> FILTER_SHIFT; \
    sr = MIX_FILTER_CLAMP(sr64); \
    fr2 = fr1; fr1 = sr; \
    (smp_in_r) = sr >> PREAMP_BITS; \
} while (0)

#define FILTER_MONO(smp_in_l) do { \
    FILTER_LEFT((smp_in_l)); \
} while(0)

#define FILTER_STEREO(smp_in_l, smp_in_r) do { \
    FILTER_LEFT((smp_in_l)); \
    FILTER_RIGHT((smp_in_r)); \
} while (0)

#define MIX_OUT(out_sample, out_level) do { \
    *(buffer++) += (out_sample) * (out_level); \
} while (0)

#define MIX_MONO(smp_in) do { \
    MIX_OUT((smp_in), vl); \
} while (0)

#define MIX_MONO_AC(smp_in) do { \
    MIX_OUT((smp_in), old_vl >> 8); \
    old_vl += delta_l; \
} while (0)

#define MIX_STEREO(smp_in_l, smp_in_r) do { \
    MIX_OUT((smp_in_l), vl); \
    MIX_OUT((smp_in_r), vr); \
} while (0)

#define MIX_STEREO_AC(smp_in_l, smp_in_r) do { \
    MIX_OUT((smp_in_l), old_vl >> 8); \
    MIX_OUT((smp_in_r), old_vr >> 8); \
    old_vl += delta_l; \
    old_vr += delta_r; \
} while (0)

#define AVERAGE(smp_in_l, smp_in_r) (((smp_in_l) + (smp_in_r)) >> 1)

#define MIX_MONO_AVG(smp_in_l, smp_in_r) do { \
    MIX_MONO(AVERAGE((smp_in_l), (smp_in_r))); \
} while (0)

#define MIX_MONO_AVG_AC(smp_in_l, smp_in_r) do { \
    MIX_MONO_AC(AVERAGE((smp_in_l), (smp_in_r))); \
} while (0)

/* For "nearest" to be nearest neighbor (instead of floor), the position needs
 * to be rounded. This only needs to be done once at the start of mixing, and
 * is required for reverse samples to round the same as forward samples.
 */
#define NEAREST_ROUND() do { \
    frac += (1 << (SMIX_SHIFT - 1)); \
    pos += (frac >> SMIX_SHIFT) * (chn); \
    frac &= SMIX_MASK; \
} while (0)

#define VAR_NORM(x) \
    REGISTER int smpl; \
    x *sptr = (x *)vi->sptr; \
    int pos = ((int)vi->pos) * chn; \
    int frac = (1 << SMIX_SHIFT) * (vi->pos - (int)vi->pos)

#define VAR_MONO(x) \
    const int chn = 1; \
    VAR_NORM(x)

#define VAR_STEREO(x) \
    const int chn = 2; \
    REGISTER int smpr; \
    VAR_NORM(x)

#define VAR_LINEAR() \
    int smp_l1, smp_dt

#define VAR_LINEAR_MONO(x) \
    VAR_MONO(x); \
    VAR_LINEAR()

#define VAR_LINEAR_STEREO(x) \
    VAR_STEREO(x); \
    VAR_LINEAR()

#define VAR_SPLINE_MONO(x) \
    VAR_MONO(x)

#define VAR_SPLINE_STEREO(x) \
    VAR_STEREO(x)

#define VAR_MONOOUT \
    int old_vl = vi->old_vl

#define VAR_STEREOOUT \
    VAR_MONOOUT; \
    int old_vr = vi->old_vr

#ifndef LIBXMP_CORE_DISABLE_IT

#define VAR_FILTER_MONO \
    int fl1 = vi->filter.l1, fl2 = vi->filter.l2; \
    int64 a0 = vi->filter.a0, b0 = vi->filter.b0, b1 = vi->filter.b1; \
    int64 sl64; \
    int sl

#define VAR_FILTER_STEREO \
    VAR_FILTER_MONO; \
    int fr1 = vi->filter.r1, fr2 = vi->filter.r2; \
    int64 sr64; \
    int sr

/* Note: copying the left to the right here is for just in case these
 * values don't get cleared between playing stereo/mono samples. */
#define SAVE_FILTER_MONO() do { \
    vi->filter.l1 = fl1; \
    vi->filter.l2 = fl2; \
    vi->filter.r1 = fl1; \
    vi->filter.r2 = fl2; \
} while (0)

#define SAVE_FILTER_STEREO() do { \
    SAVE_FILTER_MONO(); \
    vi->filter.r1 = fr1; \
    vi->filter.r2 = fr2; \
} while (0)

#endif


/*
 * Nearest neighbor mixers
 */

/* Handler for 8-bit mono samples, nearest neighbor mono output
 */
MIXER(monoout_mono_8bit_nearest)
{
    VAR_MONO(int8);
    NEAREST_ROUND();

    LOOP { NEAREST_8BIT(smpl, 0); MIX_MONO(smpl); UPDATE_POS(); }
}

/* Handler for 16-bit mono samples, nearest neighbor mono output
 */
MIXER(monoout_mono_16bit_nearest)
{
    VAR_MONO(int16);
    NEAREST_ROUND();

    LOOP { NEAREST_16BIT(smpl, 0); MIX_MONO(smpl); UPDATE_POS(); }
}

/* Handler for 8-bit stereo samples, nearest neighbor mono output
 */
MIXER(monoout_stereo_8bit_nearest)
{
    VAR_STEREO(int8);
    NEAREST_ROUND();

    LOOP { NEAREST_8BIT(smpl, 0); NEAREST_8BIT(smpr, 1);
           MIX_MONO_AVG(smpl, smpr); UPDATE_POS(); }
}

/* Handler for 16-bit stereo samples, nearest neighbor mono output
 */
MIXER(monoout_stereo_16bit_nearest)
{
    VAR_STEREO(int16);
    NEAREST_ROUND();

    LOOP { NEAREST_16BIT(smpl, 0); NEAREST_16BIT(smpr, 1);
           MIX_MONO_AVG(smpl, smpr); UPDATE_POS(); }
}

/* Handler for 8-bit mono samples, nearest neighbor stereo output
 */
MIXER(stereoout_mono_8bit_nearest)
{
    VAR_MONO(int8);
    NEAREST_ROUND();

    LOOP { NEAREST_8BIT(smpl, 0); MIX_STEREO(smpl, smpl); UPDATE_POS(); }
}

/* Handler for 16-bit mono samples, nearest neighbor stereo output
 */
MIXER(stereoout_mono_16bit_nearest)
{
    VAR_MONO(int16);
    NEAREST_ROUND();

    LOOP { NEAREST_16BIT(smpl, 0); MIX_STEREO(smpl, smpl); UPDATE_POS(); }
}

/* Handler for 8-bit stereo samples, nearest neighbor stereo output
 */
MIXER(stereoout_stereo_8bit_nearest)
{
    VAR_STEREO(int8);
    NEAREST_ROUND();

    LOOP { NEAREST_8BIT(smpl, 0); NEAREST_8BIT(smpr, 1);
           MIX_STEREO(smpl, smpr); UPDATE_POS(); }
}

/* Handler for 16-bit stereo samples, nearest neighbor stereo output
 */
MIXER(stereoout_stereo_16bit_nearest)
{
    VAR_STEREO(int16);
    NEAREST_ROUND();

    LOOP { NEAREST_16BIT(smpl, 0); NEAREST_16BIT(smpr, 1);
           MIX_STEREO(smpl, smpr); UPDATE_POS(); }
}


/*
 * Linear mixers
 */

/* Handler for 8-bit mono samples, linear interpolated mono output
 */
MIXER(monoout_mono_8bit_linear)
{
    VAR_LINEAR_MONO(int8);
    VAR_MONOOUT;

    LOOP_AC { LINEAR_8BIT(smpl, 0); MIX_MONO_AC(smpl); UPDATE_POS(); }
    LOOP    { LINEAR_8BIT(smpl, 0); MIX_MONO(smpl); UPDATE_POS(); }
}

/* Handler for 16-bit mono samples, linear interpolated mono output
 */
MIXER(monoout_mono_16bit_linear)
{
    VAR_LINEAR_MONO(int16);
    VAR_MONOOUT;

    LOOP_AC { LINEAR_16BIT(smpl, 0); MIX_MONO_AC(smpl); UPDATE_POS(); }
    LOOP    { LINEAR_16BIT(smpl, 0); MIX_MONO(smpl); UPDATE_POS(); }
}

/* Handler for 8-bit stereo samples, linear interpolated mono output
 */
MIXER(monoout_stereo_8bit_linear)
{
    VAR_LINEAR_STEREO(int8);
    VAR_MONOOUT;

    LOOP_AC {   LINEAR_8BIT(smpl, 0); LINEAR_8BIT(smpr, 1);
                MIX_MONO_AVG_AC(smpl, smpr); UPDATE_POS(); }
    LOOP    {   LINEAR_8BIT(smpl, 0); LINEAR_8BIT(smpr, 1);
                MIX_MONO_AVG(smpl, smpr); UPDATE_POS(); }
}

/* Handler for 16-bit stereo samples, linear interpolated mono output
 */
MIXER(monoout_stereo_16bit_linear)
{
    VAR_LINEAR_STEREO(int16);
    VAR_MONOOUT;

    LOOP_AC {   LINEAR_16BIT(smpl, 0); LINEAR_16BIT(smpr, 1);
                MIX_MONO_AVG_AC(smpl, smpr); UPDATE_POS(); }
    LOOP    {   LINEAR_16BIT(smpl, 0); LINEAR_16BIT(smpr, 1);
                MIX_MONO_AVG(smpl, smpr); UPDATE_POS(); }
}

/* Handler for 8-bit mono samples, linear interpolated stereo output
 */
MIXER(stereoout_mono_8bit_linear)
{
    VAR_LINEAR_MONO(int8);
    VAR_STEREOOUT;

    LOOP_AC { LINEAR_8BIT(smpl, 0); MIX_STEREO_AC(smpl, smpl); UPDATE_POS(); }
    LOOP    { LINEAR_8BIT(smpl, 0); MIX_STEREO(smpl, smpl); UPDATE_POS(); }
}

/* Handler for 16-bit mono samples, linear interpolated stereo output
 */
MIXER(stereoout_mono_16bit_linear)
{
    VAR_LINEAR_MONO(int16);
    VAR_STEREOOUT;

    LOOP_AC { LINEAR_16BIT(smpl, 0); MIX_STEREO_AC(smpl, smpl); UPDATE_POS(); }
    LOOP    { LINEAR_16BIT(smpl, 0); MIX_STEREO(smpl, smpl); UPDATE_POS(); }
}

/* Handler for 8-bit stereo samples, linear interpolated stereo output
 */
MIXER(stereoout_stereo_8bit_linear)
{
    VAR_LINEAR_STEREO(int8);
    VAR_STEREOOUT;

    LOOP_AC {   LINEAR_8BIT(smpl, 0); LINEAR_8BIT(smpr, 1);
                MIX_STEREO_AC(smpl, smpr); UPDATE_POS(); }
    LOOP    {   LINEAR_8BIT(smpl, 0); LINEAR_8BIT(smpr, 1);
                MIX_STEREO(smpl, smpr); UPDATE_POS(); }
}

/* Handler for 16-bit stereo samples, linear interpolated stereo output
 */
MIXER(stereoout_stereo_16bit_linear)
{
    VAR_LINEAR_STEREO(int16);
    VAR_STEREOOUT;

    LOOP_AC {   LINEAR_16BIT(smpl, 0); LINEAR_16BIT(smpr, 1);
                MIX_STEREO_AC(smpl, smpr); UPDATE_POS(); }
    LOOP    {   LINEAR_16BIT(smpl, 0); LINEAR_16BIT(smpr, 1);
                MIX_STEREO(smpl, smpr); UPDATE_POS(); }
}

#ifndef LIBXMP_CORE_DISABLE_IT

/* Handler for 8-bit mono samples, filtered linear interpolated mono output
 */
MIXER(monoout_mono_8bit_linear_filter)
{
    VAR_LINEAR_MONO(int8);
    VAR_FILTER_MONO;
    VAR_MONOOUT;

    LOOP_AC {   LINEAR_8BIT(smpl, 0); FILTER_MONO(smpl);
                MIX_MONO_AC(smpl); UPDATE_POS(); }
    LOOP    {   LINEAR_8BIT(smpl, 0); FILTER_MONO(smpl);
                MIX_MONO(smpl); UPDATE_POS(); }

    SAVE_FILTER_MONO();
}

/* Handler for 16-bit mono samples, filtered linear interpolated mono output
 */
MIXER(monoout_mono_16bit_linear_filter)
{
    VAR_LINEAR_MONO(int16);
    VAR_FILTER_MONO;
    VAR_MONOOUT;

    LOOP_AC {   LINEAR_16BIT(smpl, 0); FILTER_MONO(smpl);
                MIX_MONO_AC(smpl); UPDATE_POS(); }
    LOOP    {   LINEAR_16BIT(smpl, 0); FILTER_MONO(smpl);
                MIX_MONO(smpl); UPDATE_POS(); }

    SAVE_FILTER_MONO();
}

/* Handler for 8-bit stereo samples, filtered linear interpolated mono output
 */
MIXER(monoout_stereo_8bit_linear_filter)
{
    VAR_LINEAR_STEREO(int8);
    VAR_FILTER_STEREO;
    VAR_MONOOUT;

    LOOP_AC {   LINEAR_8BIT(smpl, 0); LINEAR_8BIT(smpr, 1); FILTER_STEREO(smpl, smpr);
                MIX_MONO_AVG_AC(smpl, smpr); UPDATE_POS(); }
    LOOP    {   LINEAR_8BIT(smpl, 0); LINEAR_8BIT(smpr, 1); FILTER_STEREO(smpl, smpr);
                MIX_MONO_AVG(smpl, smpr); UPDATE_POS(); }

    SAVE_FILTER_STEREO();
}

/* Handler for 16-bit stereo samples, filtered linear interpolated mono output
 */
MIXER(monoout_stereo_16bit_linear_filter)
{
    VAR_LINEAR_STEREO(int16);
    VAR_FILTER_STEREO;
    VAR_MONOOUT;

    LOOP_AC {   LINEAR_16BIT(smpl, 0); LINEAR_16BIT(smpr, 1); FILTER_STEREO(smpl, smpr);
                MIX_MONO_AVG_AC(smpl, smpr); UPDATE_POS(); }
    LOOP    {   LINEAR_16BIT(smpl, 0); LINEAR_16BIT(smpr, 1); FILTER_STEREO(smpl, smpr);
                MIX_MONO_AVG(smpl, smpr); UPDATE_POS(); }

    SAVE_FILTER_STEREO();
}

/* Handler for 8-bit mono samples, filtered linear interpolated stereo output
 */
MIXER(stereoout_mono_8bit_linear_filter)
{
    VAR_LINEAR_MONO(int8);
    VAR_FILTER_MONO;
    VAR_STEREOOUT;

    LOOP_AC {   LINEAR_8BIT(smpl, 0); FILTER_MONO(smpl);
                MIX_STEREO_AC(smpl, smpl); UPDATE_POS(); }
    LOOP    {   LINEAR_8BIT(smpl, 0); FILTER_MONO(smpl);
                MIX_STEREO(smpl, smpl); UPDATE_POS(); }

    SAVE_FILTER_MONO();
}

/* Handler for 16-bit mono samples, filtered linear interpolated stereo output
 */
MIXER(stereoout_mono_16bit_linear_filter)
{
    VAR_LINEAR_MONO(int16);
    VAR_FILTER_MONO;
    VAR_STEREOOUT;

    LOOP_AC {   LINEAR_16BIT(smpl, 0); FILTER_MONO(smpl);
                MIX_STEREO_AC(smpl, smpl); UPDATE_POS(); }
    LOOP    {   LINEAR_16BIT(smpl, 0); FILTER_MONO(smpl);
                MIX_STEREO(smpl, smpl); UPDATE_POS(); }

    SAVE_FILTER_MONO();
}

/* Handler for 8-bit stereo samples, filtered linear interpolated stereo output
 */
MIXER(stereoout_stereo_8bit_linear_filter)
{
    VAR_LINEAR_STEREO(int8);
    VAR_FILTER_STEREO;
    VAR_STEREOOUT;

    LOOP_AC {   LINEAR_8BIT(smpl, 0); LINEAR_8BIT(smpr, 1); FILTER_STEREO(smpl, smpr);
                MIX_STEREO_AC(smpl, smpr); UPDATE_POS(); }
    LOOP    {   LINEAR_8BIT(smpl, 0); LINEAR_8BIT(smpr, 1); FILTER_STEREO(smpl, smpr);
                MIX_STEREO(smpl, smpr); UPDATE_POS(); }

    SAVE_FILTER_STEREO();
}

/* Handler for 16-bit stereo samples, filtered linear interpolated stereo output
 */
MIXER(stereoout_stereo_16bit_linear_filter)
{
    VAR_LINEAR_STEREO(int16);
    VAR_FILTER_STEREO;
    VAR_STEREOOUT;

    LOOP_AC {   LINEAR_16BIT(smpl, 0); LINEAR_16BIT(smpr, 1); FILTER_STEREO(smpl, smpr);
                MIX_STEREO_AC(smpl, smpr); UPDATE_POS(); }
    LOOP    {   LINEAR_16BIT(smpl, 0); LINEAR_16BIT(smpr, 1); FILTER_STEREO(smpl, smpr);
                MIX_STEREO(smpl, smpr); UPDATE_POS(); }

    SAVE_FILTER_STEREO();
}

#endif

/*
 * Spline mixers
 */

/* Handler for 8-bit mono samples, spline interpolated mono output
 */
MIXER(monoout_mono_8bit_spline)
{
    VAR_SPLINE_MONO(int8);
    VAR_MONOOUT;

    LOOP_AC { SPLINE_8BIT(smpl, 0); MIX_MONO_AC(smpl); UPDATE_POS(); }
    LOOP    { SPLINE_8BIT(smpl, 0); MIX_MONO(smpl); UPDATE_POS(); }
}

/* Handler for 16-bit mono samples, spline interpolated mono output
 */
MIXER(monoout_mono_16bit_spline)
{
    VAR_SPLINE_MONO(int16);
    VAR_MONOOUT;

    LOOP_AC { SPLINE_16BIT(smpl, 0); MIX_MONO_AC(smpl); UPDATE_POS(); }
    LOOP    { SPLINE_16BIT(smpl, 0); MIX_MONO(smpl); UPDATE_POS(); }
}

/* Handler for 8-bit stereo samples, spline interpolated mono output
 */
MIXER(monoout_stereo_8bit_spline)
{
    VAR_SPLINE_STEREO(int8);
    VAR_MONOOUT;

    LOOP_AC {   SPLINE_8BIT(smpl, 0); SPLINE_8BIT(smpr, 1);
                MIX_MONO_AVG_AC(smpl, smpr); UPDATE_POS(); }
    LOOP    {   SPLINE_8BIT(smpl, 0); SPLINE_8BIT(smpr, 1);
                MIX_MONO_AVG(smpl, smpr); UPDATE_POS(); }
}

/* Handler for 16-bit stereo samples, spline interpolated mono output
 */
MIXER(monoout_stereo_16bit_spline)
{
    VAR_SPLINE_STEREO(int16);
    VAR_MONOOUT;

    LOOP_AC {   SPLINE_16BIT(smpl, 0); SPLINE_16BIT(smpr, 1);
                MIX_MONO_AVG_AC(smpl, smpr); UPDATE_POS(); }
    LOOP    {   SPLINE_16BIT(smpl, 0); SPLINE_16BIT(smpr, 1);
                MIX_MONO_AVG(smpl, smpr); UPDATE_POS(); }
}

/* Handler for 8-bit mono samples, spline interpolated stereo output
 */
MIXER(stereoout_mono_8bit_spline)
{
    VAR_SPLINE_MONO(int8);
    VAR_STEREOOUT;

    LOOP_AC { SPLINE_8BIT(smpl, 0); MIX_STEREO_AC(smpl, smpl); UPDATE_POS(); }
    LOOP    { SPLINE_8BIT(smpl, 0); MIX_STEREO(smpl, smpl); UPDATE_POS(); }
}

/* Handler for 16-bit mono samples, spline interpolated stereo output
 */
MIXER(stereoout_mono_16bit_spline)
{
    VAR_SPLINE_MONO(int16);
    VAR_STEREOOUT;

    LOOP_AC { SPLINE_16BIT(smpl, 0); MIX_STEREO_AC(smpl, smpl); UPDATE_POS(); }
    LOOP    { SPLINE_16BIT(smpl, 0); MIX_STEREO(smpl, smpl); UPDATE_POS(); }
}

/* Handler for 8-bit stereo samples, spline interpolated stereo output
 */
MIXER(stereoout_stereo_8bit_spline)
{
    VAR_SPLINE_STEREO(int8);
    VAR_STEREOOUT;

    LOOP_AC {   SPLINE_8BIT(smpl, 0); SPLINE_8BIT(smpr, 1);
                MIX_STEREO_AC(smpl, smpr); UPDATE_POS(); }
    LOOP    {   SPLINE_8BIT(smpl, 0); SPLINE_8BIT(smpr, 1);
                MIX_STEREO(smpl, smpr); UPDATE_POS(); }
}

/* Handler for 16-bit stereo samples, spline interpolated stereo output
 */
MIXER(stereoout_stereo_16bit_spline)
{
    VAR_SPLINE_STEREO(int16);
    VAR_STEREOOUT;

    LOOP_AC {   SPLINE_16BIT(smpl, 0); SPLINE_16BIT(smpr, 1);
                MIX_STEREO_AC(smpl, smpr); UPDATE_POS(); }
    LOOP    {   SPLINE_16BIT(smpl, 0); SPLINE_16BIT(smpr, 1);
                MIX_STEREO(smpl, smpr); UPDATE_POS(); }
}

#ifndef LIBXMP_CORE_DISABLE_IT

/* Handler for 8-bit mono samples, filtered spline interpolated mono output
 */
MIXER(monoout_mono_8bit_spline_filter)
{
    VAR_SPLINE_MONO(int8);
    VAR_FILTER_MONO;
    VAR_MONOOUT;

    LOOP_AC {   SPLINE_8BIT(smpl, 0); FILTER_MONO(smpl);
                MIX_MONO_AC(smpl); UPDATE_POS(); }
    LOOP    {   SPLINE_8BIT(smpl, 0); FILTER_MONO(smpl);
                MIX_MONO(smpl); UPDATE_POS(); }

    SAVE_FILTER_MONO();
}

/* Handler for 16-bit mono samples, filtered spline interpolated mono output
 */
MIXER(monoout_mono_16bit_spline_filter)
{
    VAR_SPLINE_MONO(int16);
    VAR_FILTER_MONO;
    VAR_MONOOUT;

    LOOP_AC {   SPLINE_16BIT(smpl, 0); FILTER_MONO(smpl);
                MIX_MONO_AC(smpl); UPDATE_POS(); }
    LOOP    {   SPLINE_16BIT(smpl, 0); FILTER_MONO(smpl);
                MIX_MONO(smpl); UPDATE_POS(); }

    SAVE_FILTER_MONO();
}

/* Handler for 8-bit stereo samples, filtered spline interpolated mono output
 */
MIXER(monoout_stereo_8bit_spline_filter)
{
    VAR_SPLINE_STEREO(int8);
    VAR_FILTER_STEREO;
    VAR_MONOOUT;

    LOOP_AC {   SPLINE_8BIT(smpl, 0); SPLINE_8BIT(smpr, 1); FILTER_STEREO(smpl, smpr);
                MIX_MONO_AVG_AC(smpl, smpr); UPDATE_POS(); }
    LOOP    {   SPLINE_8BIT(smpl, 0); SPLINE_8BIT(smpr, 1); FILTER_STEREO(smpl, smpr);
                MIX_MONO_AVG(smpl, smpr); UPDATE_POS(); }

    SAVE_FILTER_STEREO();
}

/* Handler for 16-bit stereo samples, filtered spline interpolated mono output
 */
MIXER(monoout_stereo_16bit_spline_filter)
{
    VAR_SPLINE_STEREO(int16);
    VAR_FILTER_STEREO;
    VAR_MONOOUT;

    LOOP_AC {   SPLINE_16BIT(smpl, 0); SPLINE_16BIT(smpr, 1); FILTER_STEREO(smpl, smpr);
                MIX_MONO_AVG_AC(smpl, smpr); UPDATE_POS(); }
    LOOP    {   SPLINE_16BIT(smpl, 0); SPLINE_16BIT(smpr, 1); FILTER_STEREO(smpl, smpr);
                MIX_MONO_AVG(smpl, smpr); UPDATE_POS(); }

    SAVE_FILTER_STEREO();
}

/* Handler for 8-bit mono samples, filtered spline interpolated stereo output
 */
MIXER(stereoout_mono_8bit_spline_filter)
{
    VAR_SPLINE_MONO(int8);
    VAR_FILTER_MONO;
    VAR_STEREOOUT;

    LOOP_AC {   SPLINE_8BIT(smpl, 0); FILTER_MONO(smpl);
                MIX_STEREO_AC(smpl, smpl); UPDATE_POS(); }
    LOOP    {   SPLINE_8BIT(smpl, 0); FILTER_MONO(smpl);
                MIX_STEREO(smpl, smpl); UPDATE_POS(); }

    SAVE_FILTER_MONO();
}

/* Handler for 16-bit mono samples, filtered spline interpolated stereo output
 */
MIXER(stereoout_mono_16bit_spline_filter)
{
    VAR_SPLINE_MONO(int16);
    VAR_FILTER_MONO;
    VAR_STEREOOUT;

    LOOP_AC {   SPLINE_16BIT(smpl, 0); FILTER_MONO(smpl);
                MIX_STEREO_AC(smpl, smpl); UPDATE_POS(); }
    LOOP    {   SPLINE_16BIT(smpl, 0); FILTER_MONO(smpl);
                MIX_STEREO(smpl, smpl); UPDATE_POS(); }

    SAVE_FILTER_MONO();
}

/* Handler for 8-bit stereo samples, filtered spline interpolated stereo output
 */
MIXER(stereoout_stereo_8bit_spline_filter)
{
    VAR_SPLINE_STEREO(int8);
    VAR_FILTER_STEREO;
    VAR_STEREOOUT;

    LOOP_AC {   SPLINE_8BIT(smpl, 0); SPLINE_8BIT(smpr, 1); FILTER_STEREO(smpl, smpr);
                MIX_STEREO_AC(smpl, smpr); UPDATE_POS(); }
    LOOP    {   SPLINE_8BIT(smpl, 0); SPLINE_8BIT(smpr, 1); FILTER_STEREO(smpl, smpr);
                MIX_STEREO(smpl, smpr); UPDATE_POS(); }

    SAVE_FILTER_STEREO();
}

/* Handler for 16-bit stereo samples, filtered spline interpolated stereo output
 */
MIXER(stereoout_stereo_16bit_spline_filter)
{
    VAR_SPLINE_STEREO(int16);
    VAR_FILTER_STEREO;
    VAR_STEREOOUT;

    LOOP_AC {   SPLINE_16BIT(smpl, 0); SPLINE_16BIT(smpr, 1); FILTER_STEREO(smpl, smpr);
                MIX_STEREO_AC(smpl, smpr); UPDATE_POS(); }
    LOOP    {   SPLINE_16BIT(smpl, 0); SPLINE_16BIT(smpr, 1); FILTER_STEREO(smpl, smpr);
                MIX_STEREO(smpl, smpr); UPDATE_POS(); }

    SAVE_FILTER_STEREO();
}

#endif
