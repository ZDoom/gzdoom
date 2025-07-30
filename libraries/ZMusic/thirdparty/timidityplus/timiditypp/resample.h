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

    resample.h
*/

#ifndef ___RESAMPLE_H_
#define ___RESAMPLE_H_

#include <stdint.h>
#include "sysdep.h"

namespace TimidityPlus
{


typedef int32_t resample_t;

enum {
	RESAMPLE_CSPLINE,
	RESAMPLE_LAGRANGE,
	RESAMPLE_GAUSS,
	RESAMPLE_NEWTON,
	RESAMPLE_LINEAR,
	RESAMPLE_NONE
};

extern void initialize_resampler_coeffs(void);
extern void free_gauss_table(void);

typedef struct resample_rec {
	splen_t loop_start;
	splen_t loop_end;
	splen_t data_length;
} resample_rec_t;

extern resample_t do_resamplation(sample_t *src, splen_t ofs, resample_rec_t *rec);

extern void pre_resample(Sample *sp);
class Player;

class Resampler	// This is only here to put the buffer on the stack without changing all the code.
{
	Player *player;
	resample_t resample_buffer[AUDIO_BUFFER_SIZE] = { 0 };
	int resample_buffer_offset = 0;

	resample_t *rs_plain_c(int v, int32_t *countptr);
	resample_t *rs_plain(int v, int32_t *countptr);
	resample_t *rs_loop_c(Voice *vp, int32_t count);
	resample_t *rs_loop(Voice *vp, int32_t count);
	resample_t *rs_bidir(Voice *vp, int32_t count);
	resample_t *rs_vib_plain(int v, int32_t *countptr);
	resample_t *rs_vib_loop(Voice *vp, int32_t count);
	resample_t *rs_vib_bidir(Voice *vp, int32_t count);
	resample_t *porta_resample_voice(int v, int32_t *countptr, int mode);
	resample_t *normal_resample_voice(int v, int32_t *countptr, int mode);
	resample_t *vib_resample_voice(int v, int32_t *countptr, int mode);
	int rs_update_porta(int v);
	int32_t update_vibrato(Voice *vp, int sign);

public:
	Resampler(Player *p)
	{
		player = p;
	}
	resample_t * resample_voice(int v, int32_t *countptr);
};

}

#endif /* ___RESAMPLE_H_ */
