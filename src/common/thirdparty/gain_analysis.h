/*
 *  ReplayGainAnalysis - analyzes input samples and give the recommended dB change
 *  Copyright (C) 2001-2009 David Robinson and Glen Sawyer
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  concept and filter values by David Robinson (David@Robinson.org)
 *    -- blame him if you think the idea is flawed
 *  coding by Glen Sawyer (mp3gain@hotmail.com) 735 W 255 N, Orem, UT 84057-4505 USA
 *    -- blame him if you think this runs too slowly, or the coding is otherwise flawed
 *
 *  For an explanation of the concepts and the basic algorithms involved, go to:
 *    http://www.replaygain.org/
 */

#ifndef GAIN_ANALYSIS_H
#define GAIN_ANALYSIS_H

#include <stddef.h>
#include <stdint.h>

#define GAIN_NOT_ENOUGH_SAMPLES  (-24601)
#define GAIN_ANALYSIS_ERROR           0
#define GAIN_ANALYSIS_OK              1

#define INIT_GAIN_ANALYSIS_ERROR      0
#define INIT_GAIN_ANALYSIS_OK         1

#define STEPS_per_dB      100.          // Table entries per dB
#define MAX_dB            120.          // Table entries for 0...MAX_dB (normal max. values are 70...80 dB)
#define MAX_SAMP_FREQ   96000.          // maximum allowed sample frequency [Hz]
#define RMS_WINDOW_TIME     0.050       // Time slice size [s]

#define YULE_ORDER         10
#define BUTTER_ORDER        2
#define MAX_ORDER               (BUTTER_ORDER > YULE_ORDER ? BUTTER_ORDER : YULE_ORDER)
#define MAX_SAMPLES_PER_WINDOW  (size_t) (MAX_SAMP_FREQ * RMS_WINDOW_TIME + 1)      // max. Samples per Time slice

typedef float Float_t;         // Type used for filtering

struct GainAnalyzer
{
	int InitGainAnalysis(int samplefreq);

	int AnalyzeSamples(const Float_t *left_samples, const Float_t *right_samples, size_t num_samples, int num_channels);

	int ResetSampleFrequency(int samplefreq);

	Float_t GetTitleGain(void);

	Float_t GetAlbumGain(void);
	
private:
	Float_t linprebuf[MAX_ORDER * 2];
	Float_t *linpre;                                          // left input samples, with pre-buffer
	Float_t lstepbuf[MAX_SAMPLES_PER_WINDOW + MAX_ORDER];
	Float_t *lstep;                                           // left "first step" (i.e. post first filter) samples
	Float_t loutbuf[MAX_SAMPLES_PER_WINDOW + MAX_ORDER];
	Float_t *lout;                                            // left "out" (i.e. post second filter) samples
	Float_t rinprebuf[MAX_ORDER * 2];
	Float_t *rinpre;                                          // right input samples ...
	Float_t rstepbuf[MAX_SAMPLES_PER_WINDOW + MAX_ORDER];
	Float_t *rstep;
	Float_t routbuf[MAX_SAMPLES_PER_WINDOW + MAX_ORDER];
	Float_t *rout;
	long sampleWindow;                                    // number of samples required to reach number of milliseconds required for RMS window
	long totsamp;
	double lsum;
	double rsum;
	int freqindex;
	unsigned int  A[(size_t) (STEPS_per_dB * MAX_dB)];
	unsigned int  B[(size_t) (STEPS_per_dB * MAX_dB)];

	void filterYule(const Float_t* input, Float_t* output, size_t nSamples, const Float_t* kernel);
	void filterButter(const Float_t* input, Float_t* output, size_t nSamples, const Float_t* kernel);
	Float_t analyzeResult(const  unsigned int* Array, size_t len);

};

#endif /* GAIN_ANALYSIS_H */
