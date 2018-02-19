/*
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999-2002 Masanao Izumo <mo@goice.co.jp>
    Copyright (C) 1995 Tuukka Toivonen <tt@cgs.fi>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

   filter.c: written by Vincent Pagel ( pagel@loria.fr )

   implements fir antialiasing filter : should help when setting sample
   rates as low as 8Khz.

   April 95
      - first draft

   22/5/95
      - modify "filter" so that it simulate leading and trailing 0 in the buffer
   */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#include "timidity.h"
#include "common.h"
#include "instrum.h"
#include "filter.h"

namespace TimidityPlus
{

/*  bessel  function   */
static double ino(double x)
{
	double y, de, e, sde;
	int i;

	y = x / 2;
	e = 1.0;
	de = 1.0;
	i = 1;
	do {
		de = de * y / (double)i;
		sde = de * de;
		e += sde;
	} while (!((e * 1.0e-08 - sde > 0) || (i++ > 25)));
	return(e);
}

/* Kaiser Window (symetric) */
static void kaiser(double *w, int n, double beta)
{
	double xind, xi;
	int i;

	xind = (2 * n - 1) * (2 * n - 1);
	for (i = 0; i < n; i++)
	{
		xi = i + 0.5;
		w[i] = ino((double)(beta * sqrt((double)(1. - 4 * xi * xi / xind))))
			/ ino((double)beta);
	}
}

/*
	* fir coef in g, cuttoff frequency in fc
	*/
static void designfir(double *g, double fc)
{
	int i;
	double xi, omega, att, beta;
	double w[ORDER2];

	for (i = 0; i < ORDER2; i++)
	{
		xi = (double)i + 0.5;
		omega = M_PI * xi;
		g[i] = sin((double)omega * fc) / omega;
	}

	att = 40.; /* attenuation  in  db */
	beta = (double)exp(log((double)0.58417 * (att - 20.96)) * 0.4) + 0.07886
		* (att - 20.96);
	kaiser(w, ORDER2, beta);

	/* Matrix product */
	for (i = 0; i < ORDER2; i++)
		g[i] = g[i] * w[i];
}

/*
	* FIR filtering -> apply the filter given by coef[] to the data buffer
	* Note that we simulate leading and trailing 0 at the border of the
	* data buffer
	*/
static void filter(int16_t *result, int16_t *data, int32_t length, double coef[])
{
	int32_t sample, i, sample_window;
	int16_t peak = 0;
	double sum;

	/* Simulate leading 0 at the begining of the buffer */
	for (sample = 0; sample < ORDER2; sample++)
	{
		sum = 0.0;
		sample_window = sample - ORDER2;

		for (i = 0; i < ORDER; i++)
			sum += coef[i] *
			((sample_window < 0) ? 0.0 : data[sample_window++]);

		/* Saturation ??? */
		if (sum > 32767.) { sum = 32767.; peak++; }
		if (sum < -32768.) { sum = -32768; peak++; }
		result[sample] = (int16_t)sum;
	}

	/* The core of the buffer  */
	for (sample = ORDER2; sample < length - ORDER + ORDER2; sample++)
	{
		sum = 0.0;
		sample_window = sample - ORDER2;

		for (i = 0; i < ORDER; i++)
			sum += data[sample_window++] * coef[i];

		/* Saturation ??? */
		if (sum > 32767.) { sum = 32767.; peak++; }
		if (sum < -32768.) { sum = -32768; peak++; }
		result[sample] = (int16_t)sum;
	}

	/* Simulate 0 at the end of the buffer */
	for (sample = length - ORDER + ORDER2; sample < length; sample++)
	{
		sum = 0.0;
		sample_window = sample - ORDER2;

		for (i = 0; i < ORDER; i++)
			sum += coef[i] *
			((sample_window >= length) ? 0.0 : data[sample_window++]);

		/* Saturation ??? */
		if (sum > 32767.) { sum = 32767.; peak++; }
		if (sum < -32768.) { sum = -32768; peak++; }
		result[sample] = (int16_t)sum;
	}
}

/***********************************************************************/
/* Prevent aliasing by filtering any freq above the output_rate        */
/*                                                                     */
/* I don't worry about looping point -> they will remain soft if they  */
/* were already                                                        */
/***********************************************************************/
void antialiasing(int16_t *data, int32_t data_length,
	int32_t sample_rate, int32_t output_rate)
{
	int16_t *temp;
	int i;
	double fir_symetric[ORDER];
	double fir_coef[ORDER2];
	double freq_cut;  /* cutoff frequency [0..1.0] FREQ_CUT/SAMP_FREQ*/


	/* No oversampling  */
	if (output_rate >= sample_rate)
		return;

	freq_cut = (double)output_rate / (double)sample_rate;

	designfir(fir_coef, freq_cut);

	/* Make the filter symetric */
	for (i = 0; i < ORDER2; i++)
		fir_symetric[ORDER - 1 - i] = fir_symetric[i] = fir_coef[ORDER2 - 1 - i];

	/* We apply the filter we have designed on a copy of the patch */
	temp = (int16_t *)safe_malloc(2 * data_length);
	memcpy(temp, data, 2 * data_length);

	filter(data, temp, data_length, fir_symetric);

	free(temp);
}

}