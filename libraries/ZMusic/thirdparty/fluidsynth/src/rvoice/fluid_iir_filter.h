/* FluidSynth - A Software Synthesizer
 *
 * Copyright (C) 2003  Peter Hanappe and others.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 */

#ifndef _FLUID_IIR_FILTER_H
#define _FLUID_IIR_FILTER_H

#include "fluidsynth_priv.h"

// Uncomment to get debug logging for filter parameters
// #define DBG_FILTER
#ifdef DBG_FILTER
#define LOG_FILTER(...) FLUID_LOG(FLUID_DBG, __VA_ARGS__)
#else
#define LOG_FILTER(...) 
#endif

#define Q_MIN ((fluid_real_t)0.001)

#ifdef __cplusplus
extern "C" {
#endif

// Calculate the filter coefficients with single precision
typedef float IIR_COEFF_T;

typedef struct _sincos_t
{
    IIR_COEFF_T sin;
    IIR_COEFF_T cos;
} fluid_iir_sincos_t;

/* We can't do information hiding here, as fluid_voice_t includes the struct
   without a pointer. */
struct _fluid_iir_filter_t
{
    enum fluid_iir_filter_type type; /* specifies the type of this filter */
    enum fluid_iir_filter_flags flags; /* additional flags to customize this filter */

    /* filter coefficients */
    /* The coefficients are normalized to a0. */
    /* b0 and b2 are identical => b02 */
    IIR_COEFF_T b02;             /* b0 / a0 */
    IIR_COEFF_T b1;              /* b1 / a0 */
    IIR_COEFF_T a1;              /* a0 / a0 */
    IIR_COEFF_T a2;              /* a1 / a0 */

    fluid_real_t hist1, hist2;      /* Sample history for the IIR filter */
    int filter_startup;             /* Flag: If set, the filter parameters will be set directly. Else it changes smoothly. */

    fluid_real_t fres;              /* The desired resonance frequency, in absolute cents, this filter is currently set to */
    fluid_real_t last_fres;         /* The filter's current (smoothed out) resonance frequency in Hz, which will converge towards its target fres once fres_incr_count has become zero */
    fluid_real_t fres_incr;         /* The linear increment of fres each sample */
    int fres_incr_count;            /* The number of samples left for the smoothed last_fres adjustment to complete */

    fluid_real_t last_q;            /* The filter's current (smoothed) Q-factor (or "bandwidth", or "resonance-friendlyness") on a linear scale. Just like fres, this will converge towards its target Q once q_incr_count has become zero. */
    fluid_real_t q_incr;            /* The linear increment of q each sample */
    int q_incr_count;               /* The number of samples left for the smoothed Q adjustment to complete */
#ifdef DBG_FILTER
    fluid_real_t target_fres;       /* The filter's target fres, that last_fres should converge towards - for debugging only */
    fluid_real_t target_q;          /* The filter's target Q - for debugging only */
#endif

    // the final gain amplifier to be applied by the last filter in the chain, zero for all other filters
    fluid_real_t amp;                /* current linear amplitude */
    fluid_real_t amp_incr;           /* amplitude increment value for the next FLUID_BUFSIZE samples */

    fluid_iir_sincos_t *sincos_table; /* pointer to the precalculated sin and cos values, owned by the synth */
};

enum
{
    CENTS_STEP = 1 /* cents */,
    FRES_MIN = 1500 /* cents */,
    FRES_MAX = 13500 /* cents */,
    SINCOS_TAB_SIZE = ((FRES_MAX /* upper fc in cents */ - FRES_MIN /* lower fc in cents */) / CENTS_STEP)
                      +
                      1 /* add one because asking for FRES_MAX cents must yield a valid coefficient */,
};


typedef struct _fluid_iir_filter_t fluid_iir_filter_t;

DECLARE_FLUID_RVOICE_FUNCTION(fluid_iir_filter_init);
DECLARE_FLUID_RVOICE_FUNCTION(fluid_iir_filter_set_fres);
DECLARE_FLUID_RVOICE_FUNCTION(fluid_iir_filter_set_q);

void fluid_iir_filter_init_table(fluid_iir_sincos_t *sincos_table, fluid_real_t sample_rate);

void fluid_iir_filter_reset(fluid_iir_filter_t *iir_filter);

void fluid_iir_filter_calc(fluid_iir_filter_t *iir_filter,
                           fluid_real_t output_rate, fluid_real_t fres_mod);

void fluid_iir_filter_apply(fluid_iir_filter_t *iir_filter,
                            fluid_iir_filter_t *custom_filter,
                            fluid_real_t *dsp_buf,
                            unsigned int count);

#ifdef __cplusplus
}
#endif
#endif

