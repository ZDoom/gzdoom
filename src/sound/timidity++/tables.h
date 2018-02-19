/*
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999-2004 Masanao Izumo <iz@onicos.co.jp>
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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

    tables.h
*/

#ifndef ___TABLES_H_
#define ___TABLES_H_

#include <math.h>
#include "sysdep.h"

namespace TimidityPlus
{

	inline double lookup_sine(double x)
	{
		return (sin((2 * M_PI / 1024.0) * (x)));
	}

	extern double lookup_triangular(int x);
	extern double lookup_log(int x);

#define SINE_CYCLE_LENGTH 1024
	extern int32_t freq_table[];
	extern int32_t freq_table_zapped[];
	extern int32_t freq_table_tuning[][128];
	extern int32_t freq_table_pytha[][128];
	extern int32_t freq_table_meantone[][128];
	extern int32_t freq_table_pureint[][128];
	extern double *vol_table;
	extern double def_vol_table[];
	extern double gs_vol_table[];
	extern double *xg_vol_table; /* == gs_vol_table */
	extern double *pan_table;
	extern double bend_fine[];
	extern double bend_coarse[];
	extern const double midi_time_table[], midi_time_table2[];
	extern const uint8_t reverb_macro_presets[];
	extern const uint8_t chorus_macro_presets[];
	extern const uint8_t delay_macro_presets[];
	extern const float delay_time_center_table[];
	extern const float pre_delay_time_table[];
	extern const float chorus_delay_time_table[];
	extern const float rate1_table[];
	extern double attack_vol_table[];
	extern double perceived_vol_table[];
	extern double gm2_vol_table[];
	extern float sc_eg_attack_table[];
	extern float sc_eg_decay_table[];
	extern float sc_eg_release_table[];
	extern double sc_vel_table[];
	extern double sc_vol_table[];
	extern double sc_pan_table[], gm2_pan_table[];
	extern double sc_drum_level_table[];
	extern double sb_vol_table[];
	extern double modenv_vol_table[];
	extern const float cb_to_amp_table[];
	extern const float reverb_time_table[];
	extern const float pan_delay_table[];
	extern const float chamberlin_filter_db_to_q_table[];
	extern const uint8_t multi_eq_block_table_xg[];
	extern const float eq_freq_table_xg[];
	extern const float lfo_freq_table_xg[];
	extern const float mod_delay_offset_table_xg[];
	extern const float reverb_time_table_xg[];
	extern const float delay_time_table_xg[];
	extern const int16_t cutoff_freq_table_gs[];
	extern const int16_t lpf_table_gs[];
	extern const int16_t eq_freq_table_gs[];
	extern const float lofi_sampling_freq_table_xg[];

	extern void init_tables(void);

	struct Sample;
	int32_t get_note_freq(Sample *sp, int note);

}

#endif /* ___TABLES_H_ */
