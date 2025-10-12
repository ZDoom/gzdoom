/*
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999-2002 Masanao Izumo <mo@goice.co.jp>
    Copyright (C) 1995 Tuukka Toivonen <tt@cgs.fi>

    In case you haven't heard, this program is free software;
    you can redistribute it and/or modify it under the terms of the
    GNU General Public License as published by the Free Software
    Foundation; either version 2 of the License, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    mix.h

*/

#ifndef ___MIX_H_
#define ___MIX_H_

#include "resample.h"

namespace TimidityPlus
{


typedef int32_t mix_t;
class Player;

class Mixer
{
	Player *player;
	int32_t filter_buffer[AUDIO_BUFFER_SIZE];

	int do_voice_filter(int, resample_t*, mix_t*, int32_t);
	void recalc_voice_resonance(int);
	void recalc_voice_fc(int);
	void ramp_out(mix_t *, int32_t *, int, int32_t);
	void mix_mono_signal(mix_t *, int32_t *, int, int);
	void mix_mystery_signal(mix_t *, int32_t *, int, int);
	void mix_mystery(mix_t *, int32_t *, int, int);
	void mix_center_signal(mix_t *, int32_t *, int, int);
	void mix_center(mix_t *, int32_t *, int, int);
	void mix_single_signal(mix_t *, int32_t *, int, int);
	void mix_single(mix_t *, int32_t *, int, int);
	int update_signal(int);
	int update_envelope(int);
	int update_modulation_envelope(int);
	void voice_ran_out(int);
	int next_stage(int);
	int modenv_next_stage(int);
	void update_tremolo(int);
	void compute_mix_smoothing(Voice *);
	int get_eg_stage(int v, int stage);

public:
	Mixer(Player *p)
	{
		player = p;
	}
	void mix_voice(int32_t *, int, int32_t);
	int recompute_envelope(int);
	int apply_envelope_to_amp(int);
	int recompute_modulation_envelope(int);
	int apply_modulation_envelope(int);
};

}
#endif /* ___MIX_H_ */
