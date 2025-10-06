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

#include "fluid_iir_filter.h"
#include "fluid_sys.h"
#include "fluid_conv.h"


DECLARE_FLUID_RVOICE_FUNCTION(fluid_iir_filter_init)
{
    fluid_iir_filter_t *iir_filter = obj;
    enum fluid_iir_filter_type type = param[0].i;
    enum fluid_iir_filter_flags flags = param[1].i;

    iir_filter->type = type;
    iir_filter->flags = flags;

    if(type != FLUID_IIR_DISABLED)
    {
        fluid_iir_filter_reset(iir_filter);
    }
}

void
fluid_iir_filter_reset(fluid_iir_filter_t *iir_filter)
{
    iir_filter->hist1 = 0;
    iir_filter->hist2 = 0;
    iir_filter->last_fres = -1.;
    iir_filter->last_q = 0;
    iir_filter->filter_startup = 1;
    iir_filter->amp = 0;
    iir_filter->amp_incr = 0;
}

DECLARE_FLUID_RVOICE_FUNCTION(fluid_iir_filter_set_fres)
{
    fluid_iir_filter_t *iir_filter = obj;
    fluid_real_t fres = param[0].real;

    iir_filter->fres = fres;
    
    LOG_FILTER("fluid_iir_filter_set_fres: fres= %f [acents]",fres);
}

static fluid_real_t fluid_iir_filter_q_from_dB(fluid_real_t q_dB)
{
    /* The generator contains 'centibels' (1/10 dB) => divide by 10 to
     * obtain dB */
    q_dB /= 10.0f;

    /* Range: SF2.01 section 8.1.3 # 8 (convert from cB to dB => /10) */
    fluid_clip(q_dB, 0.0f, 96.0f);

    /* Short version: Modify the Q definition in a way, that a Q of 0
     * dB leads to no resonance hump in the freq. response.
     *
     * Long version: From SF2.01, page 39, item 9 (initialFilterQ):
     * "The gain at the cutoff frequency may be less than zero when
     * zero is specified".  Assume q_dB=0 / q_lin=1: If we would leave
     * q as it is, then this results in a 3 dB hump slightly below
     * fc. At fc, the gain is exactly the DC gain (0 dB).  What is
     * (probably) meant here is that the filter does not show a
     * resonance hump for q_dB=0. In this case, the corresponding
     * q_lin is 1/sqrt(2)=0.707.  The filter should have 3 dB of
     * attenuation at fc now.  In this case Q_dB is the height of the
     * resonance peak not over the DC gain, but over the frequency
     * response of a non-resonant filter.  This idea is implemented as
     * follows: */
    q_dB -= 3.01f;

    /* The 'sound font' Q is defined in dB. The filter needs a linear
       q. Convert. */
    return FLUID_POW(10.0f, q_dB / 20.0f);
}

DECLARE_FLUID_RVOICE_FUNCTION(fluid_iir_filter_set_q)
{
    fluid_iir_filter_t *iir_filter = obj;
    fluid_real_t q = param[0].real;
    int flags = iir_filter->flags;
    
    LOG_FILTER("fluid_iir_filter_set_q: Q= %f [dB]",q);

    if(flags & FLUID_IIR_Q_ZERO_OFF && q <= 0.0)
    {
        q = 0;
    }
    else if(flags & FLUID_IIR_Q_LINEAR)
    {
        /* q is linear (only for user-defined filter) */
    }
    else
    {
        q = fluid_iir_filter_q_from_dB(q);
    }

    LOG_FILTER("fluid_iir_filter_set_q: Q= %f [linear]",q);
    
    if(iir_filter->filter_startup)
    {
        iir_filter->last_q = q;
        iir_filter->q_incr_count = 0;
    }
    else
    {
        static const fluid_real_t q_incr_count = FLUID_BUFSIZE;
        // Q must be at least Q_MIN, otherwise fluid_iir_filter_apply would never be entered
        if(q >= Q_MIN && iir_filter->last_q < Q_MIN)
        {
            iir_filter->last_q = Q_MIN;
        }
        iir_filter->q_incr = (q - iir_filter->last_q) / (q_incr_count);
        iir_filter->q_incr_count = q_incr_count;
        LOG_FILTER("%f - %f / %f = %f", q , iir_filter->last_q, q_incr_count, iir_filter->q_incr);
    }
#ifdef DBG_FILTER
    iir_filter->target_q = q;
#endif
}
