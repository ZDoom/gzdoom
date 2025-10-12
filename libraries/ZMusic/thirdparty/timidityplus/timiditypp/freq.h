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

#include <vector>

namespace TimidityPlus
{

extern const float pitch_freq_table[129];
extern const float pitch_freq_ub_table[129];
extern const float pitch_freq_lb_table[129];
extern const int chord_table[4][3][3];

extern int assign_pitch_to_freq(float freq);

enum
{
	CHORD_MAJOR = 0,
	CHORD_MINOR = 3,
	CHORD_DIM = 6,
	CHORD_FIFTH = 9,
	LOWEST_PITCH = 0,
	HIGHEST_PITCH = 127
};

struct Sample;

class Freq
{
	std::vector<float> floatData;
	std::vector<float> magData;
	std::vector<float> pruneMagData;
	std::vector<int> ipa;
	std::vector<float> wa;
	std::vector<int> fft1BinToPitch;
	uint32_t oldfftsize = 0;
	float pitchmags[129] = { 0 };
	double pitchbins[129] = { 0 };
	double new_pitchbins[129] = { 0 };

	int assign_chord(double *pitchbins, int *chord, int min_guesspitch, int max_guesspitch, int root_pitch);
	int freq_initialize_fft_arrays(Sample *sp);

public:

	float freq_fourier(Sample *sp, int *chord);

};

}