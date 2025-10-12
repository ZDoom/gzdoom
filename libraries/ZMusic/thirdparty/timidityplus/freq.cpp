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
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "timidity.h"
#include "common.h"
#include "instrum.h"
#include "freq.h"
#include "fft4g.h"


namespace TimidityPlus
{


/* middle C = pitch 60 = 261.6 Hz
	freq     = 13.75 * exp((pitch - 9) / 12 * log(2))
	pitch    = 9 - log(13.75 / freq) * 12 / log(2)
			= -36.37631656 + 17.31234049 * log(freq)
*/
const float pitch_freq_table[129] = {
	8.17579892f, 8.66195722f, 9.17702400f, 9.72271824f, 10.3008612f, 10.9133822f,
	11.5623257f, 12.2498574f, 12.9782718f, 13.7500000f, 14.5676175f, 15.4338532f,

	16.3515978f, 17.3239144f, 18.3540480f, 19.4454365f, 20.6017223f, 21.8267645f,
	23.1246514f, 24.4997147f, 25.9565436f, 27.5000000f, 29.1352351f, 30.8677063f,

	32.7031957f, 34.6478289f, 36.7080960f, 38.8908730f, 41.2034446f, 43.6535289f,
	46.2493028f, 48.9994295f, 51.9130872f, 55.0000000f, 58.2704702f, 61.7354127f,

	65.4063913f, 69.2956577f, 73.4161920f, 77.7817459f, 82.4068892f, 87.3070579f,
	92.4986057f, 97.9988590f, 103.826174f, 110.000000f, 116.540940f, 123.470825f,

	130.812783f, 138.591315f, 146.832384f, 155.563492f, 164.813778f, 174.614116f,
	184.997211f, 195.997718f, 207.652349f, 220.000000f, 233.081881f, 246.941651f,

	261.625565f, 277.182631f, 293.664768f, 311.126984f, 329.627557f, 349.228231f,
	369.994423f, 391.995436f, 415.304698f, 440.000000f, 466.163762f, 493.883301f,

	523.251131f, 554.365262f, 587.329536f, 622.253967f, 659.255114f, 698.456463f,
	739.988845f, 783.990872f, 830.609395f, 880.000000f, 932.327523f, 987.766603f,

	1046.50226f, 1108.73052f, 1174.65907f, 1244.50793f, 1318.51023f, 1396.91293f,
	1479.97769f, 1567.98174f, 1661.21879f, 1760.00000f, 1864.65505f, 1975.53321f,

	2093.00452f, 2217.46105f, 2349.31814f, 2489.01587f, 2637.02046f, 2793.82585f,
	2959.95538f, 3135.96349f, 3322.43758f, 3520.00000f, 3729.31009f, 3951.06641f,

	4186.00904f, 4434.92210f, 4698.63629f, 4978.03174f, 5274.04091f, 5587.65170f,
	5919.91076f, 6271.92698f, 6644.87516f, 7040.00000f, 7458.62018f, 7902.13282f,

	8372.01809f, 8869.84419f, 9397.27257f, 9956.06348f, 10548.0818f, 11175.3034f,
	11839.8215f, 12543.8540f, 13289.7503f
};



/* center_pitch + 0.49999 */
const float pitch_freq_ub_table[129] = {
	8.41536325f, 8.91576679f, 9.44592587f, 10.0076099f, 10.6026933f, 11.2331623f,
	11.9011208f, 12.6087983f, 13.3585565f, 14.1528976f, 14.9944727f, 15.8860904f,
	16.8307265f, 17.8315336f, 18.8918517f, 20.0152197f, 21.2053866f, 22.4663245f,
	23.8022417f, 25.2175966f, 26.7171129f, 28.3057952f, 29.9889453f, 31.7721808f,
	33.6614530f, 35.6630672f, 37.7837035f, 40.0304394f, 42.4107732f, 44.9326490f,
	47.6044834f, 50.4351932f, 53.4342259f, 56.6115903f, 59.9778907f, 63.5443616f,
	67.3229060f, 71.3261343f, 75.5674070f, 80.0608788f, 84.8215464f, 89.8652980f,
	95.2089667f, 100.870386f, 106.868452f, 113.223181f, 119.955781f, 127.088723f,
	134.645812f, 142.652269f, 151.134814f, 160.121758f, 169.643093f, 179.730596f,
	190.417933f, 201.740773f, 213.736904f, 226.446361f, 239.911563f, 254.177446f,
	269.291624f, 285.304537f, 302.269628f, 320.243515f, 339.286186f, 359.461192f,
	380.835867f, 403.481546f, 427.473807f, 452.892723f, 479.823125f, 508.354893f,
	538.583248f, 570.609074f, 604.539256f, 640.487030f, 678.572371f, 718.922384f,
	761.671734f, 806.963092f, 854.947614f, 905.785445f, 959.646250f, 1016.70979f,
	1077.16650f, 1141.21815f, 1209.07851f, 1280.97406f, 1357.14474f, 1437.84477f,
	1523.34347f, 1613.92618f, 1709.89523f, 1811.57089f, 1919.29250f, 2033.41957f,
	2154.33299f, 2282.43630f, 2418.15702f, 2561.94812f, 2714.28948f, 2875.68954f,
	3046.68693f, 3227.85237f, 3419.79046f, 3623.14178f, 3838.58500f, 4066.83914f,
	4308.66598f, 4564.87260f, 4836.31405f, 5123.89624f, 5428.57897f, 5751.37907f,
	6093.37387f, 6455.70474f, 6839.58092f, 7246.28356f, 7677.17000f, 8133.67829f,
	8617.33197f, 9129.74519f, 9672.62809f, 10247.7925f, 10857.1579f, 11502.7581f,
	12186.7477f, 12911.4095f, 13679.1618f
};



/* center_pitch - 0.49999 */
const float pitch_freq_lb_table[129] = {
	7.94305438f, 8.41537297f, 8.91577709f, 9.44593678f, 10.0076214f, 10.6027055f,
	11.2331752f, 11.9011346f, 12.6088129f, 13.3585719f, 14.1529139f, 14.9944900f,
	15.8861088f, 16.8307459f, 17.8315542f, 18.8918736f, 20.0152428f, 21.2054111f,
	22.4663505f, 23.8022692f, 25.2176258f, 26.7171438f, 28.3058279f, 29.9889800f,
	31.7722175f, 33.6614919f, 35.6631084f, 37.7837471f, 40.0304857f, 42.4108222f,
	44.9327009f, 47.6045384f, 50.4352515f, 53.4342876f, 56.6116557f, 59.9779599f,
	63.5444350f, 67.3229838f, 71.3262167f, 75.5674943f, 80.0609713f, 84.8216444f,
	89.8654018f, 95.2090767f, 100.870503f, 106.868575f, 113.223311f, 119.955920f,
	127.088870f, 134.645968f, 142.652433f, 151.134989f, 160.121943f, 169.643289f,
	179.730804f, 190.418153f, 201.741006f, 213.737151f, 226.446623f, 239.911840f,
	254.177740f, 269.291935f, 285.304867f, 302.269977f, 320.243885f, 339.286578f,
	359.461607f, 380.836307f, 403.482012f, 427.474301f, 452.893246f, 479.823680f,
	508.355480f, 538.583870f, 570.609734f, 604.539954f, 640.487770f, 678.573155f,
	718.923215f, 761.672614f, 806.964024f, 854.948602f, 905.786491f, 959.647359f,
	1016.71096f, 1077.16774f, 1141.21947f, 1209.07991f, 1280.97554f, 1357.14631f,
	1437.84643f, 1523.34523f, 1613.92805f, 1709.89720f, 1811.57298f, 1919.29472f,
	2033.42192f, 2154.33548f, 2282.43893f, 2418.15982f, 2561.95108f, 2714.29262f,
	2875.69286f, 3046.69045f, 3227.85610f, 3419.79441f, 3623.14597f, 3838.58944f,
	4066.84384f, 4308.67096f, 4564.87787f, 4836.31963f, 5123.90216f, 5428.58524f,
	5751.38572f, 6093.38091f, 6455.71219f, 6839.58882f, 7246.29193f, 7677.17887f,
	8133.68768f, 8617.34192f, 9129.75574f, 9672.63927f, 10247.8043f, 10857.1705f,
	11502.7714f, 12186.7618f, 12911.4244f
};



/* (M)ajor,		rotate back 1,	rotate back 2
	(m)inor,		rotate back 1,	rotate back 2
	(d)iminished minor,	rotate back 1,	rotate back 2
	(f)ifth,		rotate back 1,	rotate back 2
*/
const int chord_table[4][3][3] = {
	{ { 0, 4, 7, }, { -5, 0, 4, }, { -8, -5, 0, }, },
	{ { 0, 3, 7, }, { -5, 0, 3, }, { -9, -5, 0, }, },
	{ { 0, 3, 6, }, { -6, 0, 3, }, { -9, -6, 0, }, },
	{ { 0, 5, 7, }, { -5, 0, 5, }, { -7, -5, 0, }, },
};

/* write the chord type to *chord, returns the root note of the chord */
int Freq::assign_chord(double *pitchbins, int *chord,
	int min_guesspitch, int max_guesspitch, int root_pitch)
{

	int type, subtype;
	int pitches[19] = { 0 };
	int prune_pitches[10] = { 0 };
	int i, j, k, n, n2;
	double val, cutoff, max;
	int root_flag;

	*chord = -1;

	if (root_pitch - 9 > min_guesspitch)
		min_guesspitch = root_pitch - 9;

	if (min_guesspitch <= LOWEST_PITCH)
		min_guesspitch = LOWEST_PITCH + 1;

	if (root_pitch + 9 < max_guesspitch)
		max_guesspitch = root_pitch + 9;

	if (max_guesspitch >= HIGHEST_PITCH)
		max_guesspitch = HIGHEST_PITCH - 1;

	/* keep only local maxima */
	for (i = min_guesspitch, n = 0; i <= max_guesspitch; i++)
	{
		val = pitchbins[i];
		if (val)
		{
			if (pitchbins[i - 1] < val && pitchbins[i + 1] < val)
				pitches[n++] = i;
		}
	}

	if (n < 3)
		return -1;

	/* find largest peak */
	max = -1;
	for (i = 0; i < n; i++)
	{
		val = pitchbins[pitches[i]];
		if (val > max)
			max = val;
	}

	/* discard any peaks below cutoff */
	cutoff = 0.2 * max;
	for (i = 0, n2 = 0, root_flag = 0; i < n; i++)
	{
		val = pitchbins[pitches[i]];
		if (val >= cutoff)
		{
			prune_pitches[n2++] = pitches[i];
			if (pitches[i] == root_pitch)
				root_flag = 1;
		}
	}

	if (!root_flag || n2 < 3)
		return -1;

	/* search for a chord, must contain root pitch */
	for (i = 0; i < n2; i++)
	{
		for (subtype = 0; subtype < 3; subtype++)
		{
			if (i + subtype >= n2)
				continue;

			for (type = 0; type < 4; type++)
			{
				for (j = 0, n = 0, root_flag = 0; j < 3; j++)
				{
					k = i + j;

					if (k >= n2)
						continue;

					if (prune_pitches[k] == root_pitch)
						root_flag = 1;

					if (prune_pitches[k] - prune_pitches[i + subtype] ==
						chord_table[type][subtype][j])
						n++;
				}
				if (root_flag && n == 3)
				{
					*chord = 3 * type + subtype;
					return prune_pitches[i + subtype];
				}
			}
		}
	}

	return -1;
}



/* initialize FFT arrays for the frequency analysis */
int Freq::freq_initialize_fft_arrays(Sample *sp)
{

	uint32_t i;
	uint32_t length, newlength;
	unsigned int rate;
	sample_t *origdata;

	rate = sp->sample_rate;
	length = sp->data_length >> FRACTION_BITS;
	origdata = sp->data;

	/* copy the sample to a new float array */
	floatData.resize(length);
	for (i = 0; i < length; i++)
		floatData[i] = origdata[i];

	/* length must be a power of 2 */
	/* set it to smallest power of 2 >= 1.4*rate */
	/* at least 1.4*rate is required for decent resolution of low notes */
	newlength = pow(2, ceil(log(1.4*rate) / log(2)));
	if (length < newlength)
	{
		floatData.resize(newlength);
		memset(&floatData[0] + length, 0, (newlength - length) * sizeof(float));
	}
	length = newlength;

	/* allocate FFT arrays */
	/* calculate sin/cos and fft1_bin_to_pitch tables */
	if (length != oldfftsize)
	{
		float f0;

		magData.resize(length);
		pruneMagData.resize(length);
		ipa.resize(int(2 + sqrt(length)) * sizeof(int));
		ipa[0] = 0;
		wa.resize(length >> 1);
		fft1BinToPitch.resize(length >> 1);

		for (i = 1, f0 = (float)rate / length; i < (length >> 1); i++) {
			fft1BinToPitch[i] = assign_pitch_to_freq(i * f0);
		}
	}
	oldfftsize = length;

	/* zero out arrays that need it */
	memset(pitchmags, 0, 129 * sizeof(float));
	memset(pitchbins, 0, 129 * sizeof(double));
	memset(new_pitchbins, 0, 129 * sizeof(double));
	memset(&pruneMagData[0], 0, length * sizeof(float));

	return(length);
}



/* return the frequency of the sample */
/* max of 1.4 - 2.0 seconds of audio is analyzed, depending on sample rate */
/* samples < 1.4 seconds are padded to max length for higher fft accuracy */
float Freq::freq_fourier(Sample *sp, int *chord)
{

	int32_t length, length0;
	int32_t maxoffset, minoffset, minoffset1, minoffset2;
	int32_t minbin, maxbin;
	int32_t bin;
	int32_t i;
	int32_t j, n, total;
	unsigned int rate;
	int pitch, bestpitch, minpitch, maxpitch, maxpitch2;
	sample_t *origdata;
	float f0, mag, maxmag;
	int16_t amp, oldamp, maxamp;
	int32_t maxpos = 0;
	double sum, weightsum, maxsum;
	double f0_inv;
	float freq, newfreq, bestfreq, freq_inc;
	float minfreq, maxfreq, minfreq2, maxfreq2;
	float min_guessfreq, max_guessfreq;

	rate = sp->sample_rate;
	length = length0 = sp->data_length >> FRACTION_BITS;
	origdata = sp->data;

	length = freq_initialize_fft_arrays(sp);

	/* base frequency of the FFT */
	f0 = (float)rate / length;
	f0_inv = 1.0 / f0;

	/* get maximum amplitude */
	maxamp = -1;
	for (int32_t i = 0; i < length0; i++)
	{
		amp = abs(origdata[i]);
		if (amp >= maxamp)
		{
			maxamp = amp;
			maxpos = i;
		}
	}

	/* go out 2 zero crossings in both directions, starting at maxpos */
	/* find the peaks after the 2nd crossing */
	minoffset1 = 0;
	for (n = 0, oldamp = origdata[maxpos], i = maxpos - 1; i >= 0 && n < 2; i--)
	{
		amp = origdata[i];
		if ((oldamp && amp == 0) || (oldamp > 0 && amp < 0) ||
			(oldamp < 0 && amp > 0))
			n++;
		oldamp = amp;
	}
	minoffset1 = i;
	maxamp = labs(origdata[i]);
	while (i >= 0)
	{
		amp = origdata[i];
		if ((oldamp && amp == 0) || (oldamp > 0 && amp < 0) ||
			(oldamp < 0 && amp > 0))
		{
			break;
		}
		oldamp = amp;

		amp = labs(amp);
		if (amp > maxamp)
		{
			maxamp = amp;
			minoffset1 = i;
		}
		i--;
	}

	minoffset2 = 0;
	for (n = 0, oldamp = origdata[maxpos], i = maxpos + 1; i < length0 && n < 2; i++)
	{
		amp = origdata[i];
		if ((oldamp && amp == 0) || (oldamp > 0 && amp < 0) ||
			(oldamp < 0 && amp > 0))
			n++;
		oldamp = amp;
	}
	minoffset2 = i;
	maxamp = labs(origdata[i]);
	while (i < length0)
	{
		amp = origdata[i];
		if ((oldamp && amp == 0) || (oldamp > 0 && amp < 0) ||
			(oldamp < 0 && amp > 0))
		{
			break;
		}
		oldamp = amp;

		amp = labs(amp);
		if (amp > maxamp)
		{
			maxamp = amp;
			minoffset2 = i;
		}
		i++;
	}

	/* upper bound on the detected frequency */
	/* distance between the two peaks is at most 2 periods */
	minoffset = (minoffset2 - minoffset1);
	if (minoffset < 4)
		minoffset = 4;
	max_guessfreq = (float)rate / (minoffset * 0.5);
	if (max_guessfreq >= (rate >> 1)) max_guessfreq = (rate >> 1) - 1;

	/* lower bound on the detected frequency */
	maxoffset = rate / pitch_freq_lb_table[LOWEST_PITCH] + 0.5;
	if (maxoffset > (length >> 1))
		maxoffset = (length >> 1);
	min_guessfreq = (float)rate / maxoffset;

	/* perform the in place FFT */
	rdft(length, 1, &floatData[0], &ipa[0], &wa[0]);

	/* calc the magnitudes */
	for (i = 2; i < length; i++)
	{
		mag = floatData[i++];
		mag *= mag;
		mag += floatData[i] * floatData[i];
		magData[i >> 1] = sqrt(mag);
	}

	/* find max mag */
	maxmag = 0;
	for (i = 1; i < (length >> 1); i++)
	{
		mag = magData[i];

		pitch = fft1BinToPitch[i];
		if (pitch && mag > maxmag)
			maxmag = mag;
	}

	/* Apply non-linear scaling to the magnitudes
		* I don't know why this improves the pitch detection, but it does
		* The best choice of power seems to be between 1.64 - 1.68
		*/
	for (i = 1; i < (length >> 1); i++)
		magData[i] = maxmag * pow(magData[i] / maxmag, 1.66);

	/* bin the pitches */
	for (i = 1; i < (length >> 1); i++)
	{
		mag = magData[i];

		pitch = fft1BinToPitch[i];
		pitchbins[pitch] += mag;

		if (mag > pitchmags[pitch])
			pitchmags[pitch] = mag;
	}

	/* zero out lowest pitch, since it contains all lower frequencies too */
	pitchbins[LOWEST_PITCH] = 0;

	/* find the largest peak */
	for (i = LOWEST_PITCH + 1, maxsum = -42; i <= HIGHEST_PITCH; i++)
	{
		sum = pitchbins[i];
		if (sum > maxsum)
		{
			maxsum = sum;
		}
	}

	minpitch = assign_pitch_to_freq(min_guessfreq);
	if (minpitch > HIGHEST_PITCH) minpitch = HIGHEST_PITCH;

	/* zero out any peak below minpitch */
	for (int i = LOWEST_PITCH + 1; i < minpitch; i++)
		pitchbins[i] = 0;

	/* remove all pitches below threshold */
	for (int i = minpitch; i <= HIGHEST_PITCH; i++)
	{
		if (pitchbins[i] / maxsum < 0.01 && pitchmags[i] / maxmag < 0.01)
			pitchbins[i] = 0;
	}

	/* keep local maxima */
	for (int i = LOWEST_PITCH + 1; i < HIGHEST_PITCH; i++)
	{
		double temp;

		temp = pitchbins[i];

		/* also keep significant bands to either side */
		if (temp && pitchbins[i - 1] < temp && pitchbins[i + 1] < temp)
		{
			new_pitchbins[i] = temp;

			temp *= 0.5;
			if (pitchbins[i - 1] >= temp)
				new_pitchbins[i - 1] = pitchbins[i - 1];
			if (pitchbins[i + 1] >= temp)
				new_pitchbins[i + 1] = pitchbins[i - 1];
		}
	}
	memcpy(pitchbins, new_pitchbins, 129 * sizeof(double));

	/* find lowest and highest pitches */
	minpitch = LOWEST_PITCH;
	while (minpitch < HIGHEST_PITCH && !pitchbins[minpitch])
		minpitch++;
	maxpitch = HIGHEST_PITCH;
	while (maxpitch > LOWEST_PITCH && !pitchbins[maxpitch])
		maxpitch--;

	/* uh oh, no pitches left...
		* best guess is middle C
		* return 260 Hz, since exactly 260 Hz is never returned except on error
		* this should only occur on blank/silent samples
		*/
	if (maxpitch < minpitch)
	{
		return 260.0;
	}

	/* pitch assignment bounds based on zero crossings and pitches kept */
	if (pitch_freq_lb_table[minpitch] > min_guessfreq)
		min_guessfreq = pitch_freq_lb_table[minpitch];
	if (pitch_freq_ub_table[maxpitch] < max_guessfreq)
		max_guessfreq = pitch_freq_ub_table[maxpitch];

	minfreq = pitch_freq_lb_table[minpitch];
	if (minfreq >= (rate >> 1)) minfreq = (rate >> 1) - 1;

	maxfreq = pitch_freq_ub_table[maxpitch];
	if (maxfreq >= (rate >> 1)) maxfreq = (rate >> 1) - 1;

	minbin = minfreq / f0;
	if (!minbin)
		minbin = 1;
	maxbin = ceil(maxfreq / f0);
	if (maxbin >= (length >> 1))
		maxbin = (length >> 1) - 1;

	/* filter out all "noise" from magnitude array */
	for (int i = minbin, n = 0; i <= maxbin; i++)
	{
		pitch = fft1BinToPitch[i];
		if (pitchbins[pitch])
		{
			pruneMagData[i] = magData[i];
			n++;
		}
	}

	/* whoa!, there aren't any strong peaks at all !!! bomb early
		* best guess is middle C
		* return 260 Hz, since exactly 260 Hz is never returned except on error
		* this should only occur on blank/silent samples
		*/
	if (!n)
	{
		return 260.0;
	}

	memset(new_pitchbins, 0, 129 * sizeof(double));

	maxsum = -1;
	minpitch = assign_pitch_to_freq(min_guessfreq);
	maxpitch = assign_pitch_to_freq(max_guessfreq);
	maxpitch2 = assign_pitch_to_freq(max_guessfreq) + 9;
	if (maxpitch2 > HIGHEST_PITCH) maxpitch2 = HIGHEST_PITCH;

	/* initial guess is first local maximum */
	bestfreq = pitch_freq_table[minpitch];
	if (minpitch < HIGHEST_PITCH &&
		pitchbins[minpitch + 1] > pitchbins[minpitch])
		bestfreq = pitch_freq_table[minpitch + 1];

	/* find best fundamental */
	for (int i = minpitch; i <= maxpitch2; i++)
	{
		if (!pitchbins[i])
			continue;

		minfreq2 = pitch_freq_lb_table[i];
		maxfreq2 = pitch_freq_ub_table[i];
		freq_inc = (maxfreq2 - minfreq2) * 0.1;
		if (minfreq2 >= (rate >> 1)) minfreq2 = (rate >> 1) - 1;
		if (maxfreq2 >= (rate >> 1)) maxfreq2 = (rate >> 1) - 1;

		/* look for harmonics */
		for (freq = minfreq2; freq <= maxfreq2; freq += freq_inc)
		{
			n = total = 0;
			sum = weightsum = 0;

			for (j = 1; j <= 32 && (newfreq = j * freq) <= maxfreq; j++)
			{
				pitch = assign_pitch_to_freq(newfreq);

				if (pitchbins[pitch])
				{
					sum += pitchbins[pitch];
					n++;
					total = j;
				}
			}

			/* only pitches with good harmonics are assignment candidates */
			if (n > 1)
			{
				double ratio;

				ratio = (double)n / total;
				if (ratio >= 0.333333)
				{
					weightsum = ratio * sum;
					pitch = assign_pitch_to_freq(freq);

					/* use only these pitches for chord detection */
					if (pitch <= HIGHEST_PITCH && pitchbins[pitch])
						new_pitchbins[pitch] = weightsum;

					if (pitch > maxpitch)
						continue;

					if (n < 2 || weightsum > maxsum)
					{
						maxsum = weightsum;
						bestfreq = freq;
					}
				}
			}
		}
	}

	bestpitch = assign_pitch_to_freq(bestfreq);

	/* assign chords */
	if ((pitch = assign_chord(new_pitchbins, chord,
		bestpitch - 9, maxpitch2, bestpitch)) >= 0)
		bestpitch = pitch;

	bestfreq = pitch_freq_table[bestpitch];

	/* tune based on the fundamental and harmonics up to +5 octaves */
	sum = weightsum = 0;
	for (i = 1; i <= 32 && (freq = i * bestfreq) <= maxfreq; i++)
	{
		double tune;

		minfreq2 = pitch_freq_lb_table[bestpitch];
		maxfreq2 = pitch_freq_ub_table[bestpitch];

		minbin = minfreq2 * f0_inv;
		if (!minbin) minbin = 1;
		maxbin = ceil(maxfreq2 * f0_inv);
		if (maxbin >= (length >> 1))
			maxbin = (length >> 1) - 1;

		for (bin = minbin; bin <= maxbin; bin++)
		{
			tune = -36.37631656 + 17.31234049 * log(bin*f0) - bestpitch;
			sum += magData[bin];
			weightsum += magData[bin] * tune;
		}
	}

	bestfreq = 13.75 * exp(((bestpitch + weightsum / sum) - 9) /
		12 * log(2));

	/* Since we are using exactly 260 Hz as an error code, fudge the freq
		* on the extremely unlikely chance that the detected pitch is exactly
		* 260 Hz.
		*/
	if (bestfreq == 260.0f)
		bestfreq += 1E-5f;

	return bestfreq;
}



int assign_pitch_to_freq(float freq)
{
	/* round to nearest integer using: ceil(fraction - 0.5) */
	/* -0.5 is already added into the first constant below */
	int pitch = ceil(-36.87631656f + 17.31234049f * log(freq));

	/* min and max pitches */
	if (pitch < LOWEST_PITCH) pitch = LOWEST_PITCH;
	else if (pitch > HIGHEST_PITCH) pitch = HIGHEST_PITCH;

	return pitch;
}

}