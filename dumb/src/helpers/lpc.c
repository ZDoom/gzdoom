/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggVorbis SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE OggVorbis SOURCE CODE IS (C) COPYRIGHT 1994-2009             *
 * by the Xiph.Org Foundation http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

  function: LPC low level routines
  last mod: $Id: lpc.c 16227 2009-07-08 06:58:46Z xiphmont $

 ********************************************************************/

/* Some of these routines (autocorrelator, LPC coefficient estimator)
   are derived from code written by Jutta Degener and Carsten Bormann;
   thus we include their copyright below.  The entirety of this file
   is freely redistributable on the condition that both of these
   copyright notices are preserved without modification.  */

/* Preserved Copyright: *********************************************/

/* Copyright 1992, 1993, 1994 by Jutta Degener and Carsten Bormann,
Technische Universita"t Berlin

Any use of this software is permitted provided that this notice is not
removed and that neither the authors nor the Technische Universita"t
Berlin are deemed to have made any representations as to the
suitability of this software for any purpose nor are held responsible
for any defects of this software. THERE IS ABSOLUTELY NO WARRANTY FOR
THIS SOFTWARE.

As a matter of courtesy, the authors request to be informed about uses
this software has found, about bugs in this software, and about any
improvements that may be of general interest.

Berlin, 28.11.1994
Jutta Degener
Carsten Bormann

*********************************************************************/

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "internal/stack_alloc.h"
#include "internal/lpc.h"

/* Autocorrelation LPC coeff generation algorithm invented by
   N. Levinson in 1947, modified by J. Durbin in 1959. */

/* Input : n elements of time doamin data
   Output: m lpc coefficients, excitation energy */

float vorbis_lpc_from_data(float *data,float *lpci,int n,int m){
  double *aut=alloca(sizeof(*aut)*(m+1));
  double *lpc=alloca(sizeof(*lpc)*(m));
  double error;
  double epsilon;
  int i,j;

  /* autocorrelation, p+1 lag coefficients */
  j=m+1;
  while(j--){
    double d=0; /* double needed for accumulator depth */
    for(i=j;i<n;i++)d+=(double)data[i]*data[(i-j)];
    aut[j]=d;
  }

  /* Generate lpc coefficients from autocorr values */

  /* set our noise floor to about -100dB */
  error=aut[0] * (1. + 1e-10);
  epsilon=1e-9*aut[0]+1e-10;

  for(i=0;i<m;i++){
    double r= -aut[i+1];

    if(error<epsilon){
      memset(lpc+i,0,(m-i)*sizeof(*lpc));
      goto done;
    }

    /* Sum up this iteration's reflection coefficient; note that in
       Vorbis we don't save it.  If anyone wants to recycle this code
       and needs reflection coefficients, save the results of 'r' from
       each iteration. */

    for(j=0;j<i;j++)r-=lpc[j]*aut[i-j];
    r/=error;

    /* Update LPC coefficients and total error */

    lpc[i]=r;
    for(j=0;j<i/2;j++){
      double tmp=lpc[j];

      lpc[j]+=r*lpc[i-1-j];
      lpc[i-1-j]+=r*tmp;
    }
    if(i&1)lpc[j]+=lpc[j]*r;

    error*=1.-r*r;

  }

 done:

  /* slightly damp the filter */
  {
    double g = .99;
    double damp = g;
    for(j=0;j<m;j++){
      lpc[j]*=damp;
      damp*=g;
    }
  }

  for(j=0;j<m;j++)lpci[j]=(float)lpc[j];

  /* we need the error value to know how big an impulse to hit the
     filter with later */

  return (float)error;
}

void vorbis_lpc_predict(float *coeff,float *prime,int m,
                     float *data,long n){

  /* in: coeff[0...m-1] LPC coefficients
         prime[0...m-1] initial values (allocated size of n+m-1)
    out: data[0...n-1] data samples */

  long i,j,o,p;
  float y;
  float *work=alloca(sizeof(*work)*(m+n));

  if(!prime)
    for(i=0;i<m;i++)
      work[i]=0.f;
  else
    for(i=0;i<m;i++)
      work[i]=prime[i];

  for(i=0;i<n;i++){
    y=0;
    o=i;
    p=m;
    for(j=0;j<m;j++)
      y-=work[o++]*coeff[--p];

    data[i]=work[o]=y;
  }
}

#include "dumb.h"
#include "internal/dumb.h"
#include "internal/it.h"

enum { lpc_max   = 256 }; /* Maximum number of input samples to train the function */
enum { lpc_order = 32  }; /* Order of the filter */
enum { lpc_extra = 64  }; /* How many samples of padding to predict or silence */


/* This extra sample padding is really only needed by the FIR resampler, but it helps the other resamplers as well. */

void dumb_it_add_lpc(struct DUMB_IT_SIGDATA *sigdata){
    float lpc[lpc_order * 2];
    float lpc_input[lpc_max * 2];
    float lpc_output[lpc_extra * 2];

    signed char * s8;
    signed short * s16;

    int n, o, offset, lpc_samples;

    for ( n = 0; n < sigdata->n_samples; n++ ) {
        IT_SAMPLE * sample = sigdata->sample + n;
        if ( ( sample->flags & ( IT_SAMPLE_EXISTS | IT_SAMPLE_LOOP) ) == IT_SAMPLE_EXISTS ) {
            /* If we have enough sample data to train the filter, use the filter to generate the padding */
            if ( sample->length >= lpc_order ) {
                lpc_samples = sample->length;
                if (lpc_samples > lpc_max) lpc_samples = lpc_max;
                offset = sample->length - lpc_samples;

                if ( sample->flags & IT_SAMPLE_STEREO )
                {
                    if ( sample->flags & IT_SAMPLE_16BIT )
                    {
                        s16 = ( signed short * ) sample->data;
                        s16 += offset * 2;
                        for ( o = 0; o < lpc_samples; o++ )
                        {
                            lpc_input[ o ] = s16[ o * 2 + 0 ];
                            lpc_input[ o + lpc_max ] = s16[ o * 2 + 1 ];
                        }
                    }
                    else
                    {
                        s8 = ( signed char * ) sample->data;
                        s8 += offset * 2;
                        for ( o = 0; o < lpc_samples; o++ )
                        {
                            lpc_input[ o ] = s8[ o * 2 + 0 ];
                            lpc_input[ o + lpc_max ] = s8[ o * 2 + 1 ];
                        }
                    }

                    vorbis_lpc_from_data( lpc_input, lpc, lpc_samples, lpc_order );
                    vorbis_lpc_from_data( lpc_input + lpc_max, lpc + lpc_order, lpc_samples, lpc_order );

                    vorbis_lpc_predict( lpc, lpc_input + lpc_samples - lpc_order, lpc_order, lpc_output, lpc_extra );
                    vorbis_lpc_predict( lpc + lpc_order, lpc_input + lpc_max + lpc_samples - lpc_order, lpc_order, lpc_output + lpc_extra, lpc_extra );

                    if ( sample->flags & IT_SAMPLE_16BIT )
                    {
                        s16 = ( signed short * ) realloc( sample->data, ( sample->length + lpc_extra ) * 2 * sizeof(short) );
                        sample->data = s16;

                        s16 += sample->length * 2;
                        sample->length += lpc_extra;

                        for ( o = 0; o < lpc_extra; o++ )
                        {
                            s16[ o * 2 + 0 ] = (signed short)lpc_output[ o ];
                            s16[ o * 2 + 1 ] = (signed short)lpc_output[ o + lpc_extra ];
                        }
                    }
                    else
                    {
                        s8 = ( signed char * ) realloc( sample->data, ( sample->length + lpc_extra ) * 2 );
                        sample->data = s8;

                        s8 += sample->length * 2;
                        sample->length += lpc_extra;

                        for ( o = 0; o < lpc_extra; o++ )
                        {
                            s8[ o * 2 + 0 ] = (signed char)lpc_output[ o ];
                            s8[ o * 2 + 1 ] = (signed char)lpc_output[ o + lpc_extra ];
                        }
                    }
                }
                else
                {
                    if ( sample->flags & IT_SAMPLE_16BIT )
                    {
                        s16 = ( signed short * ) sample->data;
                        s16 += offset;
                        for ( o = 0; o < lpc_samples; o++ )
                        {
                            lpc_input[ o ] = s16[ o ];
                        }
                    }
                    else
                    {
                        s8 = ( signed char * ) sample->data;
                        s8 += offset;
                        for ( o = 0; o < lpc_samples; o++ )
                        {
                            lpc_input[ o ] = s8[ o ];
                        }
                    }

                    vorbis_lpc_from_data( lpc_input, lpc, lpc_samples, lpc_order );

                    vorbis_lpc_predict( lpc, lpc_input + lpc_samples - lpc_order, lpc_order, lpc_output, lpc_extra );

                    if ( sample->flags & IT_SAMPLE_16BIT )
                    {
                        s16 = ( signed short * ) realloc( sample->data, ( sample->length + lpc_extra ) * sizeof(short) );
                        sample->data = s16;

                        s16 += sample->length;
                        sample->length += lpc_extra;

                        for ( o = 0; o < lpc_extra; o++ )
                        {
                            s16[ o ] = (signed short)lpc_output[ o ];
                        }
                    }
                    else
                    {
                        s8 = ( signed char * ) realloc( sample->data, sample->length + lpc_extra );
                        sample->data = s8;

                        s8 += sample->length;
                        sample->length += lpc_extra;

                        for ( o = 0; o < lpc_extra; o++ )
                        {
                            s8[ o ] = (signed char)lpc_output[ o ];
                        }
                    }
                }
            }
            else
            /* Otherwise, pad with silence. */
            {
                offset = sample->length;
                lpc_samples = lpc_extra;

                sample->length += lpc_samples;

                n = 1;
                if ( sample->flags & IT_SAMPLE_STEREO ) n *= 2;
                if ( sample->flags & IT_SAMPLE_16BIT ) n *= 2;

                offset *= n;
                lpc_samples *= n;

                sample->data = realloc( sample->data, offset + lpc_samples );
                memset( (char*)sample->data + offset, 0, lpc_samples );
            }
        }
    }
}
