/*
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999-2009 Masanao Izumo <iz@onicos.co.jp>
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

    playmidi.c -- random stuff in need of rearrangement
*/

#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <math.h>
#include <mutex>
#include "timidity.h"
#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "mix.h"
#include "recache.h"
#include "reverb.h"
#include "freq.h"
#include "quantity.h"
#include "c_cvars.h"
#include "tables.h"
#include "effect.h"
#include "i_musicinterns.h"


namespace TimidityPlus
{
	std::mutex CvarCritSec;
	bool timidity_modulation_wheel = true;
	bool timidity_portamento = false;
	int timidity_reverb = 0;
	int  timidity_chorus = 0;
	bool timidity_surround_chorus = false;	// requires restart!
	bool timidity_channel_pressure = false;
	int timidity_lpf_def = true;
	bool timidity_temper_control = true;
	bool timidity_modulation_envelope = true;
	bool timidity_overlap_voice_allow = true;
	bool timidity_drum_effect = false;
	bool timidity_pan_delay = false;
	float timidity_drum_power = 1.f;
	int timidity_key_adjust = 0;
	float timidity_tempo_adjust = 1.f;

	// The following options have no generic use and are only meaningful for some SYSEX events not normally found in common MIDIs.
	// For now they are kept as unchanging global variables
	static bool opt_eq_control = false;
	static bool op_nrpn_vibrato = true;
	static bool opt_tva_attack = false;
	static bool opt_tva_decay = false;
	static bool opt_tva_release = false;
	static bool opt_insertion_effect = false;
	static bool opt_delay_control = false;
}

template<class T> void ChangeVarSync(T&var, T value)
{
	std::lock_guard<std::mutex> lock(TimidityPlus::CvarCritSec);
	var = value;
}

CUSTOM_CVAR(Bool, timidity_modulation_wheel, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	ChangeVarSync(TimidityPlus::timidity_modulation_wheel, *self);
}

CUSTOM_CVAR(Bool, timidity_portamento, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	ChangeVarSync(TimidityPlus::timidity_portamento, *self);
}
/*
* reverb=0     no reverb                 0
* reverb=1     old reverb                1
* reverb=1,n   set reverb level to n   (-1 to -127)
* reverb=2     "global" old reverb       2
* reverb=2,n   set reverb level to n   (-1 to -127) - 128
* reverb=3     new reverb                3
* reverb=3,n   set reverb level to n   (-1 to -127) - 256
* reverb=4     "global" new reverb       4
* reverb=4,n   set reverb level to n   (-1 to -127) - 384
*/
EXTERN_CVAR(Int, timidity_reverb_level)
EXTERN_CVAR(Int, timidity_reverb)

static void SetReverb()
{
	int value = 0;
	int mode = timidity_reverb;
	int level = timidity_reverb_level;

	if (mode == 0 || level == 0) value = mode;
	else value = (mode - 1) * -128 - level;
	ChangeVarSync(TimidityPlus::timidity_reverb, value);
}

CUSTOM_CVAR(Int, timidity_reverb, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 0 || self > 4) self = 0;
	else SetReverb();
}

CUSTOM_CVAR(Int, timidity_reverb_level, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 0 || self > 127) self = 0;
	else SetReverb();
}

CUSTOM_CVAR(Int, timidity_chorus, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	ChangeVarSync(TimidityPlus::timidity_chorus, *self);
}

CUSTOM_CVAR(Bool, timidity_surround_chorus, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (currSong != nullptr && currSong->GetDeviceType() == MDEV_TIMIDITY)
	{
		MIDIDeviceChanged(-1, true);
	}
	ChangeVarSync(TimidityPlus::timidity_surround_chorus, *self);
}

CUSTOM_CVAR(Bool, timidity_channel_pressure, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	ChangeVarSync(TimidityPlus::timidity_channel_pressure, *self);
}

CUSTOM_CVAR(Int, timidity_lpf_def, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	ChangeVarSync(TimidityPlus::timidity_lpf_def, *self);
}

CUSTOM_CVAR(Bool, timidity_temper_control, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	ChangeVarSync(TimidityPlus::timidity_temper_control, *self);
}

CUSTOM_CVAR(Bool, timidity_modulation_envelope, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	ChangeVarSync(TimidityPlus::timidity_modulation_envelope, *self);
	if (currSong != nullptr && currSong->GetDeviceType() == MDEV_TIMIDITY)
	{
		MIDIDeviceChanged(-1, true);
	}
}

CUSTOM_CVAR(Bool, timidity_overlap_voice_allow, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	ChangeVarSync(TimidityPlus::timidity_overlap_voice_allow, *self);
}

CUSTOM_CVAR(Bool, timidity_drum_effect, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	ChangeVarSync(TimidityPlus::timidity_drum_effect, *self);
}

CUSTOM_CVAR(Bool, timidity_pan_delay, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	ChangeVarSync(TimidityPlus::timidity_pan_delay, *self);
}

CUSTOM_CVAR(Float, timidity_drum_power, 1.0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG) /* coef. of drum amplitude */
{
	if (self < 0) self = 0;
	else if (self > MAX_AMPLIFICATION/100.f) self = MAX_AMPLIFICATION/100.f;
	ChangeVarSync(TimidityPlus::timidity_drum_power, *self);
}
CUSTOM_CVAR(Int, timidity_key_adjust, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < -24) self = -24;
	else if (self > 24) self = 24;
	ChangeVarSync(TimidityPlus::timidity_key_adjust, *self);
}
// For testing mainly.
CUSTOM_CVAR(Float, timidity_tempo_adjust, 1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 0.25) self = 0.25;
	else if (self > 10) self = 10;
	ChangeVarSync(TimidityPlus::timidity_tempo_adjust, *self);
}


namespace TimidityPlus
{

// These two variables need to remain global or things will get messy because they get accessed from non-class code.
int32_t control_ratio = 22;
int32_t playback_rate = 22050;

#define PLAY_INTERLEAVE_SEC			1.0
#define PORTAMENTO_TIME_TUNING		(1.0 / 5000.0)
#define PORTAMENTO_CONTROL_RATIO	256	/* controls per sec */
#define DEFAULT_CHORUS_DELAY1		0.02
#define DEFAULT_CHORUS_DELAY2		0.003
#define CHORUS_OPPOSITE_THRESHOLD	32
#define EOT_PRESEARCH_LEN			32
#define SPEED_CHANGE_RATE			1.0594630943592953  /* 2^(1/12) */
#define DEFAULT_AMPLIFICATION 		70
#define VIBRATO_DEPTH_MAX 384	/* 600 cent */

void set_playback_rate(int freq)
{
	const int CONTROLS_PER_SECOND = 1000;
	const int MAX_CONTROL_RATIO = 255;

	playback_rate = freq;
	control_ratio = playback_rate / CONTROLS_PER_SECOND;
	if (control_ratio < 1)
		control_ratio = 1;
	else if (control_ratio > MAX_CONTROL_RATIO)
		control_ratio = MAX_CONTROL_RATIO;
}


Player::Player(Instruments *instr)
{
	last_reverb_setting = timidity_reverb;
	memset(this, 0, sizeof(*this));

	// init one-time global stuff - this should go to the device class once it exists.
	instruments = instr;
	initialize_resampler_coeffs();
	init_tables();

	new_midi_file_info();
	init_mblock(&playmidi_pool);

	reverb = new Reverb;
	reverb->init_effect_status(play_system_mode);
	effect = new Effect(reverb);


	mixer = new Mixer(this);
	recache = new Recache(this);

	for (int i = 0; i < MAX_CHANNELS; i++)
		init_channel_layer(i);

	instruments->init_userdrum();
	instruments->init_userinst();

	master_volume_ratio = 0xFFFF;
	vol_table = def_vol_table;

	play_system_mode = DEFAULT_SYSTEM_MODE;
	midi_streaming = 0;
	stream_max_compute = 500; /* compute time limit (in msec) when streaming */
	current_keysig = 0;
	current_temper_keysig = 0;
	temper_adj = 0;
	current_play_tempo = 500000;
	opt_realtime_playing = 0;
	check_eot_flag;
	playmidi_seek_flag = 0;
	opt_pure_intonation = 0;
	current_freq_table = 0;
	current_temper_freq_table = 0;
	master_tuning = 0;

	make_rvid_flag = 0; /* For reverb optimization */

	voices = DEFAULT_VOICES;
	amplification = DEFAULT_AMPLIFICATION;


	static const int drums[] = { 10, -1 };

	CLEAR_CHANNELMASK(default_drumchannels);
	for (int i = 0; drums[i] > 0; i++)
	{
		SET_CHANNELMASK(default_drumchannels, drums[i] - 1);
	}
	for (int i = 16; i < MAX_CHANNELS; i++)
	{
		if (IS_SET_CHANNELMASK(default_drumchannels, i & 0xF))
			SET_CHANNELMASK(default_drumchannels, i);
	}
	COPY_CHANNELMASK(drumchannels, default_drumchannels);
	COPY_CHANNELMASK(drumchannel_mask, default_drumchannel_mask);

}

Player::~Player()
{
	reuse_mblock(&playmidi_pool);
	if (reverb_buffer != nullptr) free(reverb_buffer);
	for (int i = 0; i < MAX_CHANNELS; i++) free_drum_effect(i);
	delete mixer;
	delete recache;
	delete effect;
	delete reverb;
}


bool Player::IS_SYSEX_EVENT_TYPE(MidiEvent *event)
{
	return ((event)->type == ME_NONE || (event)->type >= ME_RANDOM_PAN || (event)->b == SYSEX_TAG);
}


void Player::init_freq_table_user(void)
{
	int p, i, j, k, l;
	double f;

	for (p = 0; p < 4; p++)
		for (i = 0; i < 12; i++)
			for (j = -1; j < 11; j++) {
				f = 440 * pow(2.0, (i - 9) / 12.0 + j - 5);
				for (k = 0; k < 12; k++) {
					l = i + j * 12 + k;
					if (l < 0 || l >= 128)
						continue;
					freq_table_user[p][i][l] = f * 1000 + 0.5;
					freq_table_user[p][i + 12][l] = f * 1000 + 0.5;
					freq_table_user[p][i + 24][l] = f * 1000 + 0.5;
					freq_table_user[p][i + 36][l] = f * 1000 + 0.5;
				}
			}
}


/*! convert Hz to internal vibrato control ratio. */
double Player::cnv_Hz_to_vib_ratio(double freq)
{
	return ((double)(playback_rate) / (freq * 2.0f * VIBRATO_SAMPLE_INCREMENTS));
}

void Player::adjust_amplification(void)
{
	static const double compensation_ratio = 1.0;
    /* compensate master volume */
    master_volume = (double)(amplification) / 100.0 *
	((double)master_volume_ratio * (compensation_ratio/0xFFFF));
}

int Player::new_vidq(int ch, int note)
{
    int i;

    if(timidity_overlap_voice_allow)
    {
	i = ch * 128 + note;
	return vidq_head[i]++;
    }
    return 0;
}

int Player::last_vidq(int ch, int note)
{
    int i;

    if(timidity_overlap_voice_allow)
    {
	i = ch * 128 + note;
	if(vidq_head[i] == vidq_tail[i])
	{
	    return -1;
	}
	return vidq_tail[i]++;
    }
    return 0;
}

void Player::reset_voices(void)
{
    int i;
    for(i = 0; i < max_voices; i++)
    {
	voice[i].status = VOICE_FREE;
	voice[i].temper_instant = 0;
	voice[i].chorus_link = i;
    }
    upper_voices = 0;
    memset(vidq_head, 0, sizeof(vidq_head));
    memset(vidq_tail, 0, sizeof(vidq_tail));
}

void Player::kill_note(int i)
{
    voice[i].status = VOICE_DIE;
}

void Player::kill_all_voices(void)
{
    int i, uv = upper_voices;

    for(i = 0; i < uv; i++)
	if(voice[i].status & ~(VOICE_FREE | VOICE_DIE))
	    kill_note(i);
    memset(vidq_head, 0, sizeof(vidq_head));
    memset(vidq_tail, 0, sizeof(vidq_tail));
}

void Player::reset_drum_controllers(struct DrumParts *d[], int note)
{
	int i, j;

	if (note == -1)
	{
		for (i = 0; i < 128; i++)
			if (d[i] != NULL)
			{
				d[i]->drum_panning = NO_PANNING;
				for (j = 0; j < 6; j++) { d[i]->drum_envelope_rate[j] = -1; }
				d[i]->pan_random = 0;
				d[i]->drum_level = 1.0f;
				d[i]->coarse = 0;
				d[i]->fine = 0;
				d[i]->delay_level = -1;
				d[i]->chorus_level = -1;
				d[i]->reverb_level = -1;
				d[i]->play_note = -1;
				d[i]->drum_cutoff_freq = 0;
				d[i]->drum_resonance = 0;
				init_rx_drum(d[i]);
			}
	}
	else
	{
		d[note]->drum_panning = NO_PANNING;
		for (j = 0; j < 6; j++) { d[note]->drum_envelope_rate[j] = -1; }
		d[note]->pan_random = 0;
		d[note]->drum_level = 1.0f;
		d[note]->coarse = 0;
		d[note]->fine = 0;
		d[note]->delay_level = -1;
		d[note]->chorus_level = -1;
		d[note]->reverb_level = -1;
		d[note]->play_note = -1;
		d[note]->drum_cutoff_freq = 0;
		d[note]->drum_resonance = 0;
		init_rx_drum(d[note]);
	}
}

void Player::reset_nrpn_controllers(int c)
{
	int i;

	/* NRPN */
	reset_drum_controllers(channel[c].drums, -1);
	channel[c].vibrato_ratio = 1.0;
	channel[c].vibrato_depth = 0;
	channel[c].vibrato_delay = 0;
	channel[c].param_cutoff_freq = 0;
	channel[c].param_resonance = 0;
	channel[c].cutoff_freq_coef = 1.0;
	channel[c].resonance_dB = 0;

	/* System Exclusive */
	channel[c].dry_level = 127;
	channel[c].eq_gs = 1;
	channel[c].insertion_effect = 0;
	channel[c].velocity_sense_depth = 0x40;
	channel[c].velocity_sense_offset = 0x40;
	channel[c].pitch_offset_fine = 0;
	if (play_system_mode == GS_SYSTEM_MODE) { channel[c].assign_mode = 1; }
	else {
		if (ISDRUMCHANNEL(c)) { channel[c].assign_mode = 1; }
		else { channel[c].assign_mode = 2; }
	}
	for (i = 0; i < 12; i++)
		channel[c].scale_tuning[i] = 0;
	channel[c].prev_scale_tuning = 0;
	channel[c].temper_type = 0;

	init_channel_layer(c);
	init_part_eq_xg(&(channel[c].eq_xg));

	/* channel pressure & polyphonic key pressure control */
	init_midi_controller(&(channel[c].mod));
	init_midi_controller(&(channel[c].bend));
	init_midi_controller(&(channel[c].caf));
	init_midi_controller(&(channel[c].paf));
	init_midi_controller(&(channel[c].cc1));
	init_midi_controller(&(channel[c].cc2));
	channel[c].bend.pitch = 2;

	init_rx(c);
	channel[c].note_limit_high = 127;
	channel[c].note_limit_low = 0;
	channel[c].vel_limit_high = 127;
	channel[c].vel_limit_low = 0;

	free_drum_effect(c);

	channel[c].legato = 0;
	channel[c].damper_mode = 0;
	channel[c].loop_timeout = 0;

	channel[c].sysex_gs_msb_addr = channel[c].sysex_gs_msb_val =
		channel[c].sysex_xg_msb_addr = channel[c].sysex_xg_msb_val =
		channel[c].sysex_msb_addr = channel[c].sysex_msb_val = 0;
}

/* Process the Reset All Controllers event */
void Player::reset_controllers(int c)
{
	int j;
	/* Some standard says, although the SCC docs say 0. */

	if (play_system_mode == XG_SYSTEM_MODE)
		channel[c].volume = 100;
	else
		channel[c].volume = 90;

	channel[c].expression = 127; /* SCC-1 does this. */
	channel[c].sustain = 0;
	channel[c].sostenuto = 0;
	channel[c].pitchbend = 0x2000;
	channel[c].pitchfactor = 0; /* to be computed */
	channel[c].mod.val = 0;
	channel[c].bend.val = 0;
	channel[c].caf.val = 0;
	channel[c].paf.val = 0;
	channel[c].cc1.val = 0;
	channel[c].cc2.val = 0;
	channel[c].portamento_time_lsb = 0;
	channel[c].portamento_time_msb = 0;
	channel[c].porta_control_ratio = 0;
	channel[c].portamento = 0;
	channel[c].last_note_fine = -1;
	for (j = 0; j < 6; j++) { channel[c].envelope_rate[j] = -1; }
	update_portamento_controls(c);
	set_reverb_level(c, -1);
	if (timidity_chorus == 1)
		channel[c].chorus_level = 0;
	else
		channel[c].chorus_level = -timidity_chorus;
	channel[c].mono = 0;
	channel[c].delay_level = 0;
}

int Player::get_default_mapID(int ch)
{
	if (play_system_mode == XG_SYSTEM_MODE)
		return ISDRUMCHANNEL(ch) ? XG_DRUM_MAP : XG_NORMAL_MAP;
	return INST_NO_MAP;
}

void Player::reset_midi(int playing)
{
	int i;
	
	for (i = 0; i < MAX_CHANNELS; i++) {
		reset_controllers(i);
		reset_nrpn_controllers(i);
		channel[i].tone_map0_number = 0;
		channel[i].mod.lfo1_pitch_depth = 50;
		/* The rest of these are unaffected
		 * by the Reset All Controllers event
		 */
		channel[i].program = instruments->defaultProgram(i);
		channel[i].panning = NO_PANNING;
		channel[i].pan_random = 0;
		/* tone bank or drum set */
		if (ISDRUMCHANNEL(i)) {
			channel[i].bank = 0;
			channel[i].altassign = instruments->drumSet(0)->alt;
		} else {
			if (special_tonebank >= 0)
				channel[i].bank = special_tonebank;
			else
				channel[i].bank = default_tonebank;
		}
		channel[i].bank_lsb = channel[i].bank_msb = 0;
		if (play_system_mode == XG_SYSTEM_MODE && i % 16 == 9)
			channel[i].bank_msb = 127;	/* Use MSB=127 for XG */
		update_rpn_map(i, RPN_ADDR_FFFF, 0);
		channel[i].special_sample = 0;
		channel[i].key_shift = 0;
		channel[i].mapID = get_default_mapID(i);
		channel[i].lasttime = 0;
	}
	if (playing) {
		kill_all_voices();
		if (temper_type_mute) {
			if (temper_type_mute & 1)
				FILL_CHANNELMASK(channel_mute);
			else
				CLEAR_CHANNELMASK(channel_mute);
		}
	} else
		reset_voices();
	master_volume_ratio = 0xffff;
	adjust_amplification();
	master_tuning = 0;
	if (current_file_info) {
		COPY_CHANNELMASK(drumchannels, current_file_info->drumchannels);
		COPY_CHANNELMASK(drumchannel_mask,
				current_file_info->drumchannel_mask);
	} else {
		COPY_CHANNELMASK(drumchannels, default_drumchannels);
		COPY_CHANNELMASK(drumchannel_mask, default_drumchannel_mask);
	}
}

void Player::recompute_freq(int v)
{
	int i;
	int ch = voice[v].channel;
	int note = voice[v].note;
	int32_t tuning = 0;
	int8_t st = channel[ch].scale_tuning[note % 12];
	int8_t tt = channel[ch].temper_type;
	uint8_t tp = channel[ch].rpnmap[RPN_ADDR_0003];
	int32_t f;
	int pb = channel[ch].pitchbend;
	int32_t tmp;
	double pf, root_freq;
	int32_t a;
	Voice *vp = &(voice[v]);

	if (! voice[v].sample->sample_rate)
		return;
	if (! timidity_modulation_wheel)
		channel[ch].mod.val = 0;
	if (! timidity_portamento)
		voice[v].porta_control_ratio = 0;
	voice[v].vibrato_control_ratio = voice[v].orig_vibrato_control_ratio;
	if (voice[v].vibrato_control_ratio || channel[ch].mod.val > 0) {
		/* This instrument has vibrato. Invalidate any precomputed
		 * sample_increments.
		 */

		/* MIDI controllers LFO pitch depth */
		if (timidity_channel_pressure || timidity_modulation_wheel) {
			vp->vibrato_depth = vp->sample->vibrato_depth + channel[ch].vibrato_depth;
			vp->vibrato_depth += get_midi_controller_pitch_depth(&(channel[ch].mod))
				+ get_midi_controller_pitch_depth(&(channel[ch].bend))
				+ get_midi_controller_pitch_depth(&(channel[ch].caf))
				+ get_midi_controller_pitch_depth(&(channel[ch].paf))
				+ get_midi_controller_pitch_depth(&(channel[ch].cc1))
				+ get_midi_controller_pitch_depth(&(channel[ch].cc2));
			if (vp->vibrato_depth > VIBRATO_DEPTH_MAX) {vp->vibrato_depth = VIBRATO_DEPTH_MAX;}
			else if (vp->vibrato_depth < 1) {vp->vibrato_depth = 1;}
			if (vp->sample->vibrato_depth < 0) {	/* in opposite phase */
				vp->vibrato_depth = -vp->vibrato_depth;
			}
		}
		
		/* fill parameters for modulation wheel */
		if (channel[ch].mod.val > 0) {
			if(vp->vibrato_control_ratio == 0) {
				vp->vibrato_control_ratio = 
					vp->orig_vibrato_control_ratio = (int)(cnv_Hz_to_vib_ratio(5.0) * channel[ch].vibrato_ratio);
			}
			vp->vibrato_delay = 0;
		}

		for (i = 0; i < VIBRATO_SAMPLE_INCREMENTS; i++)
			vp->vibrato_sample_increment[i] = 0;
		vp->cache = NULL;
	}
	/* At least for GM2, it's recommended not to apply master_tuning for drum channels */
	tuning = ISDRUMCHANNEL(ch) ? 0 : master_tuning;
	/* fine: [0..128] => [-256..256]
	 * 1 coarse = 256 fine (= 1 note)
	 * 1 fine = 2^5 tuning
	 */
	tuning += (channel[ch].rpnmap[RPN_ADDR_0001] - 0x40
			+ (channel[ch].rpnmap[RPN_ADDR_0002] - 0x40) * 64) << 7;
	/* for NRPN Coarse Pitch of Drum (GS) & Fine Pitch of Drum (XG) */
	if (ISDRUMCHANNEL(ch) && channel[ch].drums[note] != NULL
			&& (channel[ch].drums[note]->fine
			|| channel[ch].drums[note]->coarse)) {
		tuning += (channel[ch].drums[note]->fine
				+ channel[ch].drums[note]->coarse * 64) << 7;
	}
	/* MIDI controllers pitch control */
	if (timidity_channel_pressure) {
		tuning += get_midi_controller_pitch(&(channel[ch].mod))
			+ get_midi_controller_pitch(&(channel[ch].bend))
			+ get_midi_controller_pitch(&(channel[ch].caf))
			+ get_midi_controller_pitch(&(channel[ch].paf))
			+ get_midi_controller_pitch(&(channel[ch].cc1))
			+ get_midi_controller_pitch(&(channel[ch].cc2));
	}
	if (timidity_modulation_envelope) {
		if (voice[v].sample->tremolo_to_pitch) {
			tuning += lookup_triangular(voice[v].tremolo_phase >> RATE_SHIFT)
					* (voice[v].sample->tremolo_to_pitch << 13) / 100.0 + 0.5;
			channel[ch].pitchfactor = 0;
		}
		if (voice[v].sample->modenv_to_pitch) {
			tuning += voice[v].last_modenv_volume
					* (voice[v].sample->modenv_to_pitch << 13) / 100.0 + 0.5;
			channel[ch].pitchfactor = 0;
		}
	}
	/* GS/XG - Scale Tuning */
	if (! ISDRUMCHANNEL(ch)) {
		tuning += ((st << 13) + 50) / 100;
		if (st != channel[ch].prev_scale_tuning) {
			channel[ch].pitchfactor = 0;
			channel[ch].prev_scale_tuning = st;
		}
	}
	if (! opt_pure_intonation
			&& timidity_temper_control && voice[v].temper_instant) {
		switch (tt) {
		case 0:
			f = freq_table_tuning[tp][note];
			break;
		case 1:
			if (current_temper_keysig < 8)
				f = freq_table_pytha[current_temper_freq_table][note];
			else
				f = freq_table_pytha[current_temper_freq_table + 12][note];
			break;
		case 2:
			if (current_temper_keysig < 8)
				f = freq_table_meantone[current_temper_freq_table
						+ ((temper_adj) ? 36 : 0)][note];
			else
				f = freq_table_meantone[current_temper_freq_table
						+ ((temper_adj) ? 24 : 12)][note];
			break;
		case 3:
			if (current_temper_keysig < 8)
				f = freq_table_pureint[current_temper_freq_table
						+ ((temper_adj) ? 36 : 0)][note];
			else
				f = freq_table_pureint[current_temper_freq_table
						+ ((temper_adj) ? 24 : 12)][note];
			break;
		default:	/* user-defined temperament */
			if ((tt -= 0x40) >= 0 && tt < 4) {
				if (current_temper_keysig < 8)
					f = freq_table_user[tt][current_temper_freq_table
							+ ((temper_adj) ? 36 : 0)][note];
				else
					f = freq_table_user[tt][current_temper_freq_table
							+ ((temper_adj) ? 24 : 12)][note];
			} else
				f = freq_table[note];
			break;
		}
		voice[v].orig_frequency = f;
	}
	if (! voice[v].porta_control_ratio) {
		if (tuning == 0 && pb == 0x2000)
			voice[v].frequency = voice[v].orig_frequency;
		else {
			pb -= 0x2000;
			if (! channel[ch].pitchfactor) {
				/* Damn.  Somebody bent the pitch. */
				tmp = pb * channel[ch].rpnmap[RPN_ADDR_0000] + tuning;
				if (tmp >= 0)
					channel[ch].pitchfactor = bend_fine[tmp >> 5 & 0xff]
							* bend_coarse[tmp >> 13 & 0x7f];
				else
					channel[ch].pitchfactor = 1.0 /
							(bend_fine[-tmp >> 5 & 0xff]
							* bend_coarse[-tmp >> 13 & 0x7f]);
			}
			voice[v].frequency =
					voice[v].orig_frequency * channel[ch].pitchfactor;
			if (voice[v].frequency != voice[v].orig_frequency)
				voice[v].cache = NULL;
		}
	} else {	/* Portamento */
		pb -= 0x2000;
		tmp = pb * channel[ch].rpnmap[RPN_ADDR_0000]
				+ (voice[v].porta_pb << 5) + tuning;
		if (tmp >= 0)
			pf = bend_fine[tmp >> 5 & 0xff]
					* bend_coarse[tmp >> 13 & 0x7f];
		else
			pf = 1.0 / (bend_fine[-tmp >> 5 & 0xff]
					* bend_coarse[-tmp >> 13 & 0x7f]);
		voice[v].frequency = voice[v].orig_frequency * pf;
		voice[v].cache = NULL;
	}
	root_freq = voice[v].sample->root_freq;
	a = TIM_FSCALE(((double) voice[v].sample->sample_rate
			* ((double)voice[v].frequency + channel[ch].pitch_offset_fine))
			/ (root_freq * playback_rate), FRACTION_BITS) + 0.5;
	/* need to preserve the loop direction */
	voice[v].sample_increment = (voice[v].sample_increment >= 0) ? a : -a;
}

int32_t Player::calc_velocity(int32_t ch,int32_t vel)
{
	int32_t velocity;
	velocity = channel[ch].velocity_sense_depth * vel / 64 + (channel[ch].velocity_sense_offset - 64) * 2;
	if(velocity > 127) {velocity = 127;}
	return velocity;
}

void Player::recompute_voice_tremolo(int v)
{
	Voice *vp = &(voice[v]);
	int ch = vp->channel;
	int32_t depth = vp->sample->tremolo_depth;
	depth += get_midi_controller_amp_depth(&(channel[ch].mod))
		+ get_midi_controller_amp_depth(&(channel[ch].bend))
		+ get_midi_controller_amp_depth(&(channel[ch].caf))
		+ get_midi_controller_amp_depth(&(channel[ch].paf))
		+ get_midi_controller_amp_depth(&(channel[ch].cc1))
		+ get_midi_controller_amp_depth(&(channel[ch].cc2));
	if(depth > 256) {depth = 256;}
	vp->tremolo_depth = depth;
}

void Player::recompute_amp(int v)
{
	double tempamp;
	int ch = voice[v].channel;

	/* master_volume and sample->volume are percentages, used to scale
	 *  amplitude directly, NOT perceived volume
	 *
	 * all other MIDI volumes are linear in perceived volume, 0-127
	 * use a lookup table for the non-linear scalings
	 */
	if (play_system_mode == GM2_SYSTEM_MODE) {
		tempamp = master_volume *
			voice[v].sample->volume *
			gm2_vol_table[calc_velocity(ch, voice[v].velocity)] *	/* velocity: not in GM2 standard */
			gm2_vol_table[channel[ch].volume] *
			gm2_vol_table[channel[ch].expression]; /* 21 bits */
	}
	else if (play_system_mode == GS_SYSTEM_MODE) {	/* use measured curve */
		tempamp = master_volume *
			voice[v].sample->volume *
			sc_vel_table[calc_velocity(ch, voice[v].velocity)] *
			sc_vol_table[channel[ch].volume] *
			sc_vol_table[channel[ch].expression]; /* 21 bits */
	}
	else {	/* use generic exponential curve */
		tempamp = master_volume *
			voice[v].sample->volume *
			perceived_vol_table[calc_velocity(ch, voice[v].velocity)] *
			perceived_vol_table[channel[ch].volume] *
			perceived_vol_table[channel[ch].expression]; /* 21 bits */
	}

	/* every digital effect increases amplitude,
	 * so that it must be reduced in advance.
	 */
	if (
		(timidity_reverb || timidity_chorus || opt_delay_control
			|| (opt_eq_control && (reverb->eq_status_gs.low_gain != 0x40
				|| reverb->eq_status_gs.high_gain != 0x40))
			|| opt_insertion_effect))
		tempamp *= 1.35f * 0.55f;
	else
		tempamp *= 1.35f;

	/* Reduce amplitude for chorus partners.
	 * 2x voices -> 2x power -> sqrt(2)x amplitude.
	 * 1 / sqrt(2) = ~0.7071, which is very close to the old
	 * CHORUS_VELOCITY_TUNING1 value of 0.7.
	 *
	 * The previous amp scaling for the various digital effects should
	 * really be redone to split them into separate scalings for each
	 * effect, rather than a single scaling if any one of them is used
	 * (which is NOT correct).  As it is now, if partner chorus is the
	 * only effect in use, then it is reduced in volume twice and winds
	 * up too quiet.  Compare the output of "-EFreverb=0 -EFchorus=0",
	 * "-EFreverb=0 -EFchorus=2", "-EFreverb=4 -EFchorus=2", and
	 * "-EFreverb=4 -EFchorus=0" to see how the digital effect volumes
	 * are not scaled properly.  Idealy, all the resulting output should
	 * have the same volume, regardless of effects used.  This will
	 * require empirically determining the amount to scale for each
	 * individual effect.
	 */
	if (voice[v].chorus_link != v)
		tempamp *= 0.7071067811865f;

	/* NRPN - drum instrument tva level */
	if (ISDRUMCHANNEL(ch)) {
		if (channel[ch].drums[voice[v].note] != NULL) {
			tempamp *= channel[ch].drums[voice[v].note]->drum_level;
		}
		tempamp *= (double)timidity_drum_power;	/* global drum power */
	}

	/* MIDI controllers amplitude control */
	if (timidity_channel_pressure) {
		tempamp *= get_midi_controller_amp(&(channel[ch].mod))
			* get_midi_controller_amp(&(channel[ch].bend))
			* get_midi_controller_amp(&(channel[ch].caf))
			* get_midi_controller_amp(&(channel[ch].paf))
			* get_midi_controller_amp(&(channel[ch].cc1))
			* get_midi_controller_amp(&(channel[ch].cc2));
		recompute_voice_tremolo(v);
	}

	if (voice[v].fc.type != 0) {
		tempamp *= voice[v].fc.gain;	/* filter gain */
	}

	/* applying panning to amplitude */
	if (true)
	{
		if (voice[v].panning == 64)
		{
			voice[v].panned = PANNED_CENTER;
			voice[v].left_amp = voice[v].right_amp = TIM_FSCALENEG(tempamp * pan_table[64], 27);
		}
		else if (voice[v].panning < 2)
		{
			voice[v].panned = PANNED_LEFT;
			voice[v].left_amp = TIM_FSCALENEG(tempamp, 20);
			voice[v].right_amp = 0;
		}
		else if (voice[v].panning == 127)
		{
			if (voice[v].panned == PANNED_MYSTERY) {
				voice[v].old_left_mix = voice[v].old_right_mix;
				voice[v].old_right_mix = 0;
			}
			voice[v].panned = PANNED_RIGHT;
			voice[v].left_amp = TIM_FSCALENEG(tempamp, 20);
			voice[v].right_amp = 0;
		}
		else
		{
			if (voice[v].panned == PANNED_RIGHT) {
				voice[v].old_right_mix = voice[v].old_left_mix;
				voice[v].old_left_mix = 0;
			}
			voice[v].panned = PANNED_MYSTERY;
			voice[v].left_amp = TIM_FSCALENEG(tempamp * pan_table[128 - voice[v].panning], 27);
			voice[v].right_amp = TIM_FSCALENEG(tempamp * pan_table[voice[v].panning], 27);
		}
	}
	else
	{
		voice[v].panned = PANNED_CENTER;
		voice[v].left_amp = TIM_FSCALENEG(tempamp, 21);
	}
}

#define RESONANCE_COEFF 0.2393

void Player::recompute_channel_filter(int ch, int note)
{
	double coef = 1.0f, reso = 0;

	if(channel[ch].special_sample > 0) {return;}

	/* Soft Pedal */
	if(channel[ch].soft_pedal != 0) {
		if(note > 49) {	/* tre corde */
			coef *= 1.0 - 0.20 * ((double)channel[ch].soft_pedal) / 127.0f;
		} else {	/* una corda (due corde) */
			coef *= 1.0 - 0.25 * ((double)channel[ch].soft_pedal) / 127.0f;
		}
	}

	if(!ISDRUMCHANNEL(ch)) {
		/* NRPN Filter Cutoff */
		coef *= pow(1.26, (double)(channel[ch].param_cutoff_freq) / 8.0f);
		/* NRPN Resonance */
		reso = (double)channel[ch].param_resonance * RESONANCE_COEFF;
	}

	channel[ch].cutoff_freq_coef = coef;
	channel[ch].resonance_dB = reso;
}

void Player::init_voice_filter(int i)
{
  memset(&(voice[i].fc), 0, sizeof(FilterCoefficients));
  if(timidity_lpf_def && voice[i].sample->cutoff_freq) {
	  voice[i].fc.orig_freq = voice[i].sample->cutoff_freq;
	  voice[i].fc.orig_reso_dB = (double)voice[i].sample->resonance / 10.0f - 3.01f;
	  if (voice[i].fc.orig_reso_dB < 0.0f) {voice[i].fc.orig_reso_dB = 0.0f;}
	  if (timidity_lpf_def == 2) {
		  voice[i].fc.gain = 1.0;
		  voice[i].fc.type = 2;
	  } else if(timidity_lpf_def == 1) {
		  voice[i].fc.gain = pow(10.0f, -voice[i].fc.orig_reso_dB / 2.0f / 20.0f);
		  voice[i].fc.type = 1;
	  }
	  voice[i].fc.start_flag = 0;
  } else {
	  voice[i].fc.type = 0;
  }
}

#define CHAMBERLIN_RESONANCE_MAX 24.0

void Player::recompute_voice_filter(int v)
{
	int ch = voice[v].channel, note = voice[v].note;
	double coef = 1.0, reso = 0, cent = 0, depth_cent = 0, freq;
	FilterCoefficients *fc = &(voice[v].fc);
	Sample *sp = (Sample *) &voice[v].sample;

	if(fc->type == 0) {return;}
	coef = channel[ch].cutoff_freq_coef;

	if(ISDRUMCHANNEL(ch) && channel[ch].drums[note] != NULL) {
		/* NRPN Drum Instrument Filter Cutoff */
		coef *= pow(1.26, (double)(channel[ch].drums[note]->drum_cutoff_freq) / 8.0f);
		/* NRPN Drum Instrument Filter Resonance */
		reso += (double)channel[ch].drums[note]->drum_resonance * RESONANCE_COEFF;
	}

	/* MIDI controllers filter cutoff control and LFO filter depth */
	if(timidity_channel_pressure) {
		cent += get_midi_controller_filter_cutoff(&(channel[ch].mod))
			+ get_midi_controller_filter_cutoff(&(channel[ch].bend))
			+ get_midi_controller_filter_cutoff(&(channel[ch].caf))
			+ get_midi_controller_filter_cutoff(&(channel[ch].paf))
			+ get_midi_controller_filter_cutoff(&(channel[ch].cc1))
			+ get_midi_controller_filter_cutoff(&(channel[ch].cc2));
		depth_cent += get_midi_controller_filter_depth(&(channel[ch].mod))
			+ get_midi_controller_filter_depth(&(channel[ch].bend))
			+ get_midi_controller_filter_depth(&(channel[ch].caf))
			+ get_midi_controller_filter_depth(&(channel[ch].paf))
			+ get_midi_controller_filter_depth(&(channel[ch].cc1))
			+ get_midi_controller_filter_depth(&(channel[ch].cc2));
	}

	if(sp->vel_to_fc) {	/* velocity to filter cutoff frequency */
		if(voice[v].velocity > sp->vel_to_fc_threshold)
			cent += sp->vel_to_fc * (double)(127 - voice[v].velocity) / 127.0f;
		else
			coef += sp->vel_to_fc * (double)(127 - sp->vel_to_fc_threshold) / 127.0f;
	}
	if(sp->vel_to_resonance) {	/* velocity to filter resonance */
		reso += (double)voice[v].velocity * sp->vel_to_resonance / 127.0f / 10.0f;
	}
	if(sp->key_to_fc) {	/* filter cutoff key-follow */
		cent += sp->key_to_fc * (double)(voice[v].note - sp->key_to_fc_bpo);
	}

	if(timidity_modulation_envelope) {
		if(voice[v].sample->tremolo_to_fc + (int16_t)depth_cent) {
			cent += ((double)voice[v].sample->tremolo_to_fc + depth_cent) * lookup_triangular(voice[v].tremolo_phase >> RATE_SHIFT);
		}
		if(voice[v].sample->modenv_to_fc) {
			cent += (double)voice[v].sample->modenv_to_fc * voice[v].last_modenv_volume;
		}
	}

	if(cent != 0) {coef *= pow(2.0, cent / 1200.0f);}

	freq = (double)fc->orig_freq * coef;

	if (freq > playback_rate / 2) {freq = playback_rate / 2;}
	else if(freq < 5) {freq = 5;}
	fc->freq = (int32_t)freq;

	fc->reso_dB = fc->orig_reso_dB + channel[ch].resonance_dB + reso;
	if(fc->reso_dB < 0.0f) {fc->reso_dB = 0.0f;}
	else if(fc->reso_dB > 96.0f) {fc->reso_dB = 96.0f;}

	if(fc->type == 1) {	/* Chamberlin filter */
		if(fc->freq > playback_rate / 6) {
			if (fc->start_flag == 0) {fc->type = 0;}	/* turn off. */ 
			else {fc->freq = playback_rate / 6;}
		}
		if(fc->reso_dB > CHAMBERLIN_RESONANCE_MAX) {fc->reso_dB = CHAMBERLIN_RESONANCE_MAX;}
	} else if(fc->type == 2) {	/* Moog VCF */
		if(fc->reso_dB > fc->orig_reso_dB / 2) {
			fc->gain = pow(10.0f, (fc->reso_dB - fc->orig_reso_dB / 2) / 20.0f);
		}
	}
	fc->start_flag = 1;	/* filter is started. */
}

float Player::calc_drum_tva_level(int ch, int note, int level)
{
	int def_level, nbank, nprog;
	const ToneBank *bank;

	if(channel[ch].special_sample > 0) {return 1.0;}

	nbank = channel[ch].bank;
	nprog = note;
	instruments->instrument_map(channel[ch].mapID, &nbank, &nprog);

	if(ISDRUMCHANNEL(ch)) {
		bank = instruments->drumSet(nbank);
		if(bank == NULL) {bank = instruments->drumSet(0);}
	} else {
		return 1.0;
	}

	def_level = bank->tone[nprog].tva_level;

	if(def_level == -1 || def_level == 0) {def_level = 127;}
	else if(def_level > 127) {def_level = 127;}

	return (sc_drum_level_table[level] / sc_drum_level_table[def_level]);
}

int32_t Player::calc_random_delay(int ch, int note)
{
	int nbank, nprog;
	const ToneBank *bank;

	if(channel[ch].special_sample > 0) {return 0;}

	nbank = channel[ch].bank;

	if(ISDRUMCHANNEL(ch)) {
		nprog = note;
		instruments->instrument_map(channel[ch].mapID, &nbank, &nprog);
		bank = instruments->drumSet(nbank);
		if (bank == NULL) {bank = instruments->drumSet(0);}
	} else {
		nprog = channel[ch].program;
		if(nprog == SPECIAL_PROGRAM) {return 0;}
		instruments->instrument_map(channel[ch].mapID, &nbank, &nprog);
		bank = instruments->toneBank(nbank);
		if(bank == NULL) {bank = instruments->toneBank(0);}
	}

	if (bank->tone[nprog].rnddelay == 0) {return 0;}
	else {return (int32_t)((double)bank->tone[nprog].rnddelay * playback_rate / 1000.0
		* (reverb->get_pink_noise_light(&reverb->global_pink_noise_light) + 1.0f) * 0.5);}
}

void Player::recompute_bank_parameter(int ch, int note)
{
	int nbank, nprog;
	const ToneBank *bank;
	struct DrumParts *drum;

	if(channel[ch].special_sample > 0) {return;}

	nbank = channel[ch].bank;

	if(ISDRUMCHANNEL(ch)) {
		nprog = note;
		instruments->instrument_map(channel[ch].mapID, &nbank, &nprog);
		bank = instruments->drumSet(nbank);
		if (bank == NULL) {bank = instruments->drumSet(0);}
		if (channel[ch].drums[note] == NULL)
				play_midi_setup_drums(ch, note);
		drum = channel[ch].drums[note];
		if (drum->reverb_level == -1 && bank->tone[nprog].reverb_send != -1) {
			drum->reverb_level = bank->tone[nprog].reverb_send;
		}
		if (drum->chorus_level == -1 && bank->tone[nprog].chorus_send != -1) {
			drum->chorus_level = bank->tone[nprog].chorus_send;
		}
		if (drum->delay_level == -1 && bank->tone[nprog].delay_send != -1) {
			drum->delay_level = bank->tone[nprog].delay_send;
		}
	} else {
		nprog = channel[ch].program;
		if (nprog == SPECIAL_PROGRAM) {return;}
		instruments->instrument_map(channel[ch].mapID, &nbank, &nprog);
		bank = instruments->toneBank(nbank);
		if (bank == NULL) {bank = instruments->toneBank(0);}
		channel[ch].legato = bank->tone[nprog].legato;
		channel[ch].damper_mode = bank->tone[nprog].damper_mode;
		channel[ch].loop_timeout = bank->tone[nprog].loop_timeout;
	}
}


/* this reduces voices while maintaining sound quality */
int Player::reduce_voice(void)
{
    int32_t lv, v;
    int i, j, lowest=-0x7FFFFFFF;

    i = upper_voices;
    lv = 0x7FFFFFFF;
    
    /* Look for the decaying note with the smallest volume */
    /* Protect drum decays.  Truncating them early sounds bad, especially on
       snares and cymbals */
    for(j = 0; j < i; j++)
    {
	if(voice[j].status & VOICE_FREE ||
	   (voice[j].sample->note_to_use && ISDRUMCHANNEL(voice[j].channel)))
	    continue;
	
	if(voice[j].status & ~(VOICE_ON | VOICE_DIE | VOICE_SUSTAINED))
	{
	    /* find lowest volume */
	    v = voice[j].left_mix;
	    if(voice[j].panned == PANNED_MYSTERY && voice[j].right_mix > v)
	    	v = voice[j].right_mix;
	    if(v < lv)
	    {
		lv = v;
		lowest = j;
	    }
	}
    }
    if(lowest != -0x7FFFFFFF)
    {
	/* This can still cause a click, but if we had a free voice to
	   spare for ramping down this note, we wouldn't need to kill it
	   in the first place... Still, this needs to be fixed. Perhaps
	   we could use a reserve of voices to play dying notes only. */

	cut_notes++;
	free_voice(lowest);
	return lowest;
    }

    /* try to remove VOICE_DIE before VOICE_ON */
    lv = 0x7FFFFFFF;
    lowest = -1;
    for(j = 0; j < i; j++)
    {
      if(voice[j].status & VOICE_FREE)
	    continue;
      if(voice[j].status & ~(VOICE_ON | VOICE_SUSTAINED))
      {
	/* continue protecting drum decays */
	if (voice[j].status & ~(VOICE_DIE) &&
	    (voice[j].sample->note_to_use && ISDRUMCHANNEL(voice[j].channel)))
		continue;
	/* find lowest volume */
	v = voice[j].left_mix;
	if(voice[j].panned == PANNED_MYSTERY && voice[j].right_mix > v)
	    v = voice[j].right_mix;
	if(v < lv)
	{
	    lv = v;
	    lowest = j;
	}
      }
    }
    if(lowest != -1)
    {
	cut_notes++;
	free_voice(lowest);
	return lowest;
    }

    /* try to remove VOICE_SUSTAINED before VOICE_ON */
    lv = 0x7FFFFFFF;
    lowest = -0x7FFFFFFF;
    for(j = 0; j < i; j++)
    {
      if(voice[j].status & VOICE_FREE)
	    continue;
      if(voice[j].status & VOICE_SUSTAINED)
      {
	/* find lowest volume */
	v = voice[j].left_mix;
	if(voice[j].panned == PANNED_MYSTERY && voice[j].right_mix > v)
	    v = voice[j].right_mix;
	if(v < lv)
	{
	    lv = v;
	    lowest = j;
	}
      }
    }
    if(lowest != -0x7FFFFFFF)
    {
	cut_notes++;
	free_voice(lowest);
	return lowest;
    }

    /* try to remove chorus before VOICE_ON */
    lv = 0x7FFFFFFF;
    lowest = -0x7FFFFFFF;
    for(j = 0; j < i; j++)
    {
      if(voice[j].status & VOICE_FREE)
	    continue;
      if(voice[j].chorus_link < j)
      {
	/* find lowest volume */
	v = voice[j].left_mix;
	if(voice[j].panned == PANNED_MYSTERY && voice[j].right_mix > v)
	    v = voice[j].right_mix;
	if(v < lv)
	{
	    lv = v;
	    lowest = j;
	}
      }
    }
    if(lowest != -0x7FFFFFFF)
    {
	cut_notes++;

	/* fix pan */
	j = voice[lowest].chorus_link;
    	voice[j].panning = channel[voice[lowest].channel].panning;
    	recompute_amp(j);
    	mixer->apply_envelope_to_amp(j);

	free_voice(lowest);
	return lowest;
    }

    lost_notes++;

    /* remove non-drum VOICE_ON */
    lv = 0x7FFFFFFF;
    lowest = -0x7FFFFFFF;
    for(j = 0; j < i; j++)
    {
        if(voice[j].status & VOICE_FREE ||
	   (voice[j].sample->note_to_use && ISDRUMCHANNEL(voice[j].channel)))
	   	continue;

	/* find lowest volume */
	v = voice[j].left_mix;
	if(voice[j].panned == PANNED_MYSTERY && voice[j].right_mix > v)
	    v = voice[j].right_mix;
	if(v < lv)
	{
	    lv = v;
	    lowest = j;
	}
    }
    if(lowest != -0x7FFFFFFF)
    {
	free_voice(lowest);
	return lowest;
    }

    /* remove all other types of notes */
    lv = 0x7FFFFFFF;
    lowest = 0;
    for(j = 0; j < i; j++)
    {
	if(voice[j].status & VOICE_FREE)
	    continue;
	/* find lowest volume */
	v = voice[j].left_mix;
	if(voice[j].panned == PANNED_MYSTERY && voice[j].right_mix > v)
	    v = voice[j].right_mix;
	if(v < lv)
	{
	    lv = v;
	    lowest = j;
	}
    }

    free_voice(lowest);
    return lowest;
}

void Player::free_voice(int v1)
{
    int v2;

#ifdef ENABLE_PAN_DELAY
	if (voice[v1].pan_delay_buf != NULL) {
		free(voice[v1].pan_delay_buf);
		voice[v1].pan_delay_buf = NULL;
	}
#endif /* ENABLE_PAN_DELAY */

    v2 = voice[v1].chorus_link;
    if(v1 != v2)
    {
	/* Unlink chorus link */
	voice[v1].chorus_link = v1;
	voice[v2].chorus_link = v2;
    }
    voice[v1].status = VOICE_FREE;
    voice[v1].temper_instant = 0;
}

int Player::find_free_voice(void)
{
    int i, nv = voices, lowest;
    int32_t lv, v;

    for(i = 0; i < nv; i++)
	if(voice[i].status == VOICE_FREE)
	{
	    if(upper_voices <= i)
		upper_voices = i + 1;
	    return i;
	}

    upper_voices = voices;

    /* Look for the decaying note with the lowest volume */
    lv = 0x7FFFFFFF;
    lowest = -1;
    for(i = 0; i < nv; i++)
    {
	if(voice[i].status & ~(VOICE_ON | VOICE_DIE) &&
	   !(voice[i].sample && voice[i].sample->note_to_use && ISDRUMCHANNEL(voice[i].channel)))
	{
	    v = voice[i].left_mix;
	    if((voice[i].panned==PANNED_MYSTERY) && (voice[i].right_mix>v))
		v = voice[i].right_mix;
	    if(v<lv)
	    {
		lv = v;
		lowest = i;
	    }
	}
    }
    return lowest;
}

int Player::find_samples(MidiEvent *e, int *vlist)
{
	int i, j, ch, bank, prog, note, nv;
	const SpecialPatch *s;
	Instrument *ip;
	
	ch = e->channel;
	if (channel[ch].special_sample > 0) {
		if ((s = instruments->specialPatch(channel[ch].special_sample)) == NULL) {
			return 0;
		}
		note = e->a + channel[ch].key_shift + note_key_offset;
		note = (note < 0) ? 0 : ((note > 127) ? 127 : note);
		return select_play_sample(s->sample, s->samples, &note, vlist, e);
	}
	bank = channel[ch].bank;
	if (ISDRUMCHANNEL(ch)) {
		note = e->a & 0x7f;
		instruments->instrument_map(channel[ch].mapID, &bank, &note);
		if (! (ip = play_midi_load_instrument(1, bank, note)))
			return 0;	/* No instrument? Then we can't play. */

		/* "keynum" of SF2, and patch option "note=" */
		if (ip->sample->note_to_use)
			note = ip->sample->note_to_use;
	} else {
		if ((prog = channel[ch].program) == SPECIAL_PROGRAM)
			ip = instruments->defaultInstrument();
		else {
			instruments->instrument_map(channel[ch].mapID, &bank, &prog);
			if (! (ip = play_midi_load_instrument(0, bank, prog)))
				return 0;	/* No instrument? Then we can't play. */
		}
		note = ((ip->sample->note_to_use) ? ip->sample->note_to_use : e->a)
				+ channel[ch].key_shift + note_key_offset;
		note = (note < 0) ? 0 : ((note > 127) ? 127 : note);
	}
	nv = select_play_sample(ip->sample, ip->samples, &note, vlist, e);
	/* Replace the sample if the sample is cached. */
	if (ip->sample->note_to_use)
		note = MIDI_EVENT_NOTE(e);
	for (i = 0; i < nv; i++) {
		j = vlist[i];
		if (! opt_realtime_playing && allocate_cache_size > 0
				&& ! channel[ch].portamento) {
			voice[j].cache = recache->resamp_cache_fetch(voice[j].sample, note);
			if (voice[j].cache)	/* cache hit */
				voice[j].sample = voice[j].cache->resampled;
		} else
			voice[j].cache = NULL;
	}
	return nv;
}

int Player::select_play_sample(Sample *splist, int nsp, int *note, int *vlist, MidiEvent *e)
{
	int ch = e->channel, kn = e->a & 0x7f, vel = e->b;
	int32_t f, fs, ft, fst, fc, fr, cdiff, diff, sample_link;
	int8_t tt = channel[ch].temper_type;
	uint8_t tp = channel[ch].rpnmap[RPN_ADDR_0003];
	Sample *sp, *spc, *spr;
	int16_t sf, sn;
	double ratio;
	int i, j, k, nv, nvc;
	
	if (ISDRUMCHANNEL(ch))
		f = fs = freq_table[*note];
	else {
		if (opt_pure_intonation) {
			if (current_keysig < 8)
				f = freq_table_pureint[current_freq_table][*note];
			else
				f = freq_table_pureint[current_freq_table + 12][*note];
		} else if (timidity_temper_control)
			switch (tt) {
			case 0:
				f = freq_table_tuning[tp][*note];
				break;
			case 1:
				if (current_temper_keysig < 8)
					f = freq_table_pytha[
							current_temper_freq_table][*note];
				else
					f = freq_table_pytha[
							current_temper_freq_table + 12][*note];
				break;
			case 2:
				if (current_temper_keysig < 8)
					f = freq_table_meantone[current_temper_freq_table
							+ ((temper_adj) ? 36 : 0)][*note];
				else
					f = freq_table_meantone[current_temper_freq_table
							+ ((temper_adj) ? 24 : 12)][*note];
				break;
			case 3:
				if (current_temper_keysig < 8)
					f = freq_table_pureint[current_temper_freq_table
							+ ((temper_adj) ? 36 : 0)][*note];
				else
					f = freq_table_pureint[current_temper_freq_table
							+ ((temper_adj) ? 24 : 12)][*note];
				break;
			default:	/* user-defined temperament */
				if ((tt -= 0x40) >= 0 && tt < 4) {
					if (current_temper_keysig < 8)
						f = freq_table_user[tt][current_temper_freq_table
								+ ((temper_adj) ? 36 : 0)][*note];
					else
						f = freq_table_user[tt][current_temper_freq_table
								+ ((temper_adj) ? 24 : 12)][*note];
				} else
					f = freq_table[*note];
				break;
			}
		else
			f = freq_table[*note];
		if (! opt_pure_intonation && timidity_temper_control
				&& tt == 0 && f != freq_table[*note]) {
			*note = log(f / 440000.0) / log(2) * 12 + 69.5;
			*note = (*note < 0) ? 0 : ((*note > 127) ? 127 : *note);
			fs = freq_table[*note];
		} else
			fs = freq_table[*note];
	}
	nv = 0;
	for (i = 0, sp = splist; i < nsp; i++, sp++) {
		/* GUS/SF2 - Scale Tuning */
		if ((sf = sp->scale_factor) != 1024) {
			sn = sp->scale_freq;
			ratio = pow(2.0, (*note - sn) * (sf - 1024) / 12288.0);
			ft = f * ratio + 0.5, fst = fs * ratio + 0.5;
		} else
			ft = f, fst = fs;
		if (ISDRUMCHANNEL(ch) && channel[ch].drums[kn] != NULL)
			if ((ratio = get_play_note_ratio(ch, kn)) != 1.0)
				ft = ft * ratio + 0.5, fst = fst * ratio + 0.5;
		if (sp->low_freq <= fst && sp->high_freq >= fst
				&& sp->low_vel <= vel && sp->high_vel >= vel
				&& ! (sp->inst_type == INST_SF2
				&& sp->sample_type == SF_SAMPLETYPE_RIGHT)) {
			j = vlist[nv] = find_voice(e);
			voice[j].orig_frequency = ft;
			voice[j].sample = sp;
			voice[j].status = VOICE_ON;
			nv++;
		}
	}
	if (nv == 0) {	/* we must select at least one sample. */
		fr = fc = 0;
		spc = spr = NULL;
		cdiff = 0x7fffffff;
		for (i = 0, sp = splist; i < nsp; i++, sp++) {
			/* GUS/SF2 - Scale Tuning */
			if ((sf = sp->scale_factor) != 1024) {
				sn = sp->scale_freq;
				ratio = pow(2.0, (*note - sn) * (sf - 1024) / 12288.0);
				ft = f * ratio + 0.5, fst = fs * ratio + 0.5;
			} else
				ft = f, fst = fs;
			if (ISDRUMCHANNEL(ch) && channel[ch].drums[kn] != NULL)
				if ((ratio = get_play_note_ratio(ch, kn)) != 1.0)
					ft = ft * ratio + 0.5, fst = fst * ratio + 0.5;
			diff = abs(sp->root_freq - fst);
			if (diff < cdiff) {
				if (sp->inst_type == INST_SF2
						&& sp->sample_type == SF_SAMPLETYPE_RIGHT) {
					fr = ft;	/* reserve */
					spr = sp;	/* reserve */
				} else {
					fc = ft;
					spc = sp;
					cdiff = diff;
				}
			}
		}
		/* If spc is not NULL, a makeshift sample is found. */
		/* Otherwise, it's a lonely right sample, but better than nothing. */
		j = vlist[nv] = find_voice(e);
		voice[j].orig_frequency = (spc) ? fc : fr;
		voice[j].sample = (spc) ? spc : spr;
		voice[j].status = VOICE_ON;
		nv++;
	}
	nvc = nv;
	for (i = 0; i < nvc; i++) {
		spc = voice[vlist[i]].sample;
		/* If it's left sample, there must be right sample. */
		if (spc->inst_type == INST_SF2
				&& spc->sample_type == SF_SAMPLETYPE_LEFT) {
			sample_link = spc->sf_sample_link;
			for (j = 0, sp = splist; j < nsp; j++, sp++)
				if (sp->inst_type == INST_SF2
						&& sp->sample_type == SF_SAMPLETYPE_RIGHT
						&& sp->sf_sample_index == sample_link) {
					/* right sample is found. */
					/* GUS/SF2 - Scale Tuning */
					if ((sf = sp->scale_factor) != 1024) {
						sn = sp->scale_freq;
						ratio = pow(2.0, (*note - sn) * (sf - 1024) / 12288.0);
						ft = f * ratio + 0.5;
					} else
						ft = f;
					if (ISDRUMCHANNEL(ch) && channel[ch].drums[kn] != NULL)
						if ((ratio = get_play_note_ratio(ch, kn)) != 1.0)
							ft = ft * ratio + 0.5;
					k = vlist[nv] = find_voice(e);
					voice[k].orig_frequency = ft;
					voice[k].sample = sp;
					voice[k].status = VOICE_ON;
					nv++;
					break;
				}
		}
	}
	return nv;
}

double Player::get_play_note_ratio(int ch, int note)
{
	int play_note = channel[ch].drums[note]->play_note;
	int bank = channel[ch].bank;
	const ToneBank *dbank;
	int def_play_note;
	
	if (play_note == -1)
		return 1.0;
	instruments->instrument_map(channel[ch].mapID, &bank, &note);
	dbank = (instruments->drumSet(bank)) ? instruments->drumSet(bank) : instruments->drumSet(0);
	if ((def_play_note = dbank->tone[note].play_note) == -1)
		return 1.0;
	if (play_note >= def_play_note)
		return bend_coarse[(play_note - def_play_note) & 0x7f];
	else
		return 1 / bend_coarse[(def_play_note - play_note) & 0x7f];
}

/* Only one instance of a note can be playing on a single channel. */
int Player::find_voice(MidiEvent *e)
{
	int ch = e->channel;
	int note = MIDI_EVENT_NOTE(e);
	int status_check, mono_check;
	AlternateAssign *altassign;
	int i, lowest = -1;
	
	status_check = (timidity_overlap_voice_allow)
			? (VOICE_OFF | VOICE_SUSTAINED) : 0xff;
	mono_check = channel[ch].mono;
	altassign = instruments->find_altassign(channel[ch].altassign, note);
	for (i = 0; i < upper_voices; i++)
		if (voice[i].status == VOICE_FREE) {
			lowest = i;	/* lower volume */
			break;
		}
	for (i = 0; i < upper_voices; i++)
		if (voice[i].status != VOICE_FREE && voice[i].channel == ch) {
			if (voice[i].note == note && (voice[i].status & status_check))
				kill_note(i);
			else if (mono_check)
				kill_note(i);
			else if (altassign && instruments->find_altassign(altassign, voice[i].note))
				kill_note(i);
			else if (voice[i].note == note && (channel[ch].assign_mode == 0
					|| (channel[ch].assign_mode == 1 &&
					    voice[i].proximate_flag == 0)))
				kill_note(i);
		}
	for (i = 0; i < upper_voices; i++)
		if (voice[i].channel == ch && voice[i].note == note)
			voice[i].proximate_flag = 0;
	if (lowest != -1)	/* Found a free voice. */
		return lowest;
	if (upper_voices < voices)
		return upper_voices++;
	return reduce_voice();
}

int Player::get_panning(int ch, int note,int v)
{
    int pan;

	if(channel[ch].panning != NO_PANNING) {pan = (int)channel[ch].panning - 64;}
	else {pan = 0;}
	if(ISDRUMCHANNEL(ch) &&
	 channel[ch].drums[note] != NULL &&
	 channel[ch].drums[note]->drum_panning != NO_PANNING) {
		pan += channel[ch].drums[note]->drum_panning;
	} else {
		pan += voice[v].sample->panning;
	}

	if (pan > 127) pan = 127;
	else if (pan < 0) pan = 0;

	return pan;
}

/*! initialize vibrato parameters for a voice. */
void Player::init_voice_vibrato(int v)
{
	Voice *vp = &(voice[v]);
	int ch = vp->channel, j, nrpn_vib_flag;
	double ratio;

	/* if NRPN vibrato is set, it's believed that there must be vibrato. */
	nrpn_vib_flag = op_nrpn_vibrato
		&& (channel[ch].vibrato_ratio != 1.0 || channel[ch].vibrato_depth != 0);
	
	/* vibrato sweep */
	vp->vibrato_sweep = vp->sample->vibrato_sweep_increment;
	vp->vibrato_sweep_position = 0;

	/* vibrato rate */
	if (nrpn_vib_flag) {
		if(vp->sample->vibrato_control_ratio == 0) {
			ratio = cnv_Hz_to_vib_ratio(5.0) * channel[ch].vibrato_ratio;
		} else {
			ratio = (double)vp->sample->vibrato_control_ratio * channel[ch].vibrato_ratio;
		}
		if (ratio < 0) {ratio = 0;}
		vp->vibrato_control_ratio = (int)ratio;
	} else {
		vp->vibrato_control_ratio = vp->sample->vibrato_control_ratio;
	}
	
	/* vibrato depth */
	if (nrpn_vib_flag) {
		vp->vibrato_depth = vp->sample->vibrato_depth + channel[ch].vibrato_depth;
		if (vp->vibrato_depth > VIBRATO_DEPTH_MAX) {vp->vibrato_depth = VIBRATO_DEPTH_MAX;}
		else if (vp->vibrato_depth < 1) {vp->vibrato_depth = 1;}
		if (vp->sample->vibrato_depth < 0) {	/* in opposite phase */
			vp->vibrato_depth = -vp->vibrato_depth;
		}
	} else {
		vp->vibrato_depth = vp->sample->vibrato_depth;
	}
	
	/* vibrato delay */
	vp->vibrato_delay = vp->sample->vibrato_delay + channel[ch].vibrato_delay;
	
	/* internal parameters */
	vp->orig_vibrato_control_ratio = vp->vibrato_control_ratio;
	vp->vibrato_control_counter = vp->vibrato_phase = 0;
	for (j = 0; j < VIBRATO_SAMPLE_INCREMENTS; j++) {
		vp->vibrato_sample_increment[j] = 0;
	}
}

/*! initialize panning-delay for a voice. */
void Player::init_voice_pan_delay(int v)
{
#ifdef ENABLE_PAN_DELAY
	Voice *vp = &(voice[v]);
	int ch = vp->channel;
	double pan_delay_diff; 

	if (vp->pan_delay_buf != NULL) {
		free(vp->pan_delay_buf);
		vp->pan_delay_buf = NULL;
	}
	vp->pan_delay_rpt = 0;
	if (timidity_pan_delay && channel[ch].insertion_effect == 0 && !timidity_surround_chorus) {
		if (vp->panning == 64) {vp->delay += pan_delay_table[64] * playback_rate / 1000;}
		else {
			if(pan_delay_table[vp->panning] > pan_delay_table[127 - vp->panning]) {
				pan_delay_diff = pan_delay_table[vp->panning] - pan_delay_table[127 - vp->panning];
				vp->delay += (pan_delay_table[vp->panning] - pan_delay_diff) * playback_rate / 1000;
			} else {
				pan_delay_diff = pan_delay_table[127 - vp->panning] - pan_delay_table[vp->panning];
				vp->delay += (pan_delay_table[127 - vp->panning] - pan_delay_diff) * playback_rate / 1000;
			}
			vp->pan_delay_rpt = pan_delay_diff * playback_rate / 1000;
		}
		if(vp->pan_delay_rpt < 1) {vp->pan_delay_rpt = 0;}
		vp->pan_delay_wpt = 0;
		vp->pan_delay_spt = vp->pan_delay_wpt - vp->pan_delay_rpt;
		if (vp->pan_delay_spt < 0) {vp->pan_delay_spt += PAN_DELAY_BUF_MAX;}
		vp->pan_delay_buf = (int32_t *)safe_malloc(sizeof(int32_t) * PAN_DELAY_BUF_MAX);
		memset(vp->pan_delay_buf, 0, sizeof(int32_t) * PAN_DELAY_BUF_MAX);
	}
#endif	/* ENABLE_PAN_DELAY */
}

/*! initialize portamento or legato for a voice. */
void Player::init_voice_portamento(int v)
{
	Voice *vp = &(voice[v]);
	int ch = vp->channel;

	vp->porta_control_counter = 0;
	if (channel[ch].legato && channel[ch].legato_flag) {
		update_legato_controls(ch);
	}
	else if (channel[ch].portamento && !channel[ch].porta_control_ratio) {
		update_portamento_controls(ch);
	}
	vp->porta_control_ratio = 0;
	if (channel[ch].porta_control_ratio)
	{
		if (channel[ch].last_note_fine == -1) {
			/* first on */
			channel[ch].last_note_fine = vp->note * 256;
			channel[ch].porta_control_ratio = 0;
		}
		else {
			vp->porta_control_ratio = channel[ch].porta_control_ratio;
			vp->porta_dpb = channel[ch].porta_dpb;
			vp->porta_pb = channel[ch].last_note_fine -
				vp->note * 256;
			if (vp->porta_pb == 0) { vp->porta_control_ratio = 0; }
		}
	}
}

/*! initialize tremolo for a voice. */
void Player::init_voice_tremolo(int v)
{
	Voice *vp = &(voice[v]);

  vp->tremolo_delay = vp->sample->tremolo_delay;
  vp->tremolo_phase = 0;
  vp->tremolo_phase_increment = vp->sample->tremolo_phase_increment;
  vp->tremolo_sweep = vp->sample->tremolo_sweep_increment;
  vp->tremolo_sweep_position = 0;
  vp->tremolo_depth = vp->sample->tremolo_depth;
}

void Player::start_note(MidiEvent *e, int i, int vid, int cnt)
{
	int j, ch, note;

	ch = e->channel;

	note = MIDI_EVENT_NOTE(e);
	voice[i].status = VOICE_ON;
	voice[i].channel = ch;
	voice[i].note = note;
	voice[i].velocity = e->b;
	voice[i].chorus_link = i;	/* No link */
	voice[i].proximate_flag = 1;

	j = channel[ch].special_sample;
	if (j == 0 || instruments->specialPatch(j) == NULL)
		voice[i].sample_offset = 0;
	else
	{
		voice[i].sample_offset = instruments->specialPatch(j)->sample_offset << FRACTION_BITS;
		if (voice[i].sample->modes & MODES_LOOPING)
		{
			if (voice[i].sample_offset > voice[i].sample->loop_end)
				voice[i].sample_offset = voice[i].sample->loop_start;
		}
		else if (voice[i].sample_offset > voice[i].sample->data_length)
		{
			free_voice(i);
			return;
		}
	}
	voice[i].sample_increment = 0; /* make sure it isn't negative */
	voice[i].vid = vid;
	voice[i].delay = voice[i].sample->envelope_delay;
	voice[i].modenv_delay = voice[i].sample->modenv_delay;
	voice[i].delay_counter = 0;

	init_voice_tremolo(i);	/* tremolo */
	init_voice_filter(i);		/* resonant lowpass filter */
	init_voice_vibrato(i);	/* vibrato */
	voice[i].panning = get_panning(ch, note, i);	/* pan */
	init_voice_pan_delay(i);	/* panning-delay */
	init_voice_portamento(i);	/* portamento or legato */

	if (cnt == 0)
		channel[ch].last_note_fine = voice[i].note * 256;

	/* initialize modulation envelope */
	if (voice[i].sample->modes & MODES_ENVELOPE)
	{
		voice[i].modenv_stage = EG_GUS_ATTACK;
		voice[i].modenv_volume = 0;
		mixer->recompute_modulation_envelope(i);
		mixer->apply_modulation_envelope(i);
	}
	else
	{
		voice[i].modenv_increment = 0;
		mixer->apply_modulation_envelope(i);
	}
	recompute_freq(i);
	recompute_voice_filter(i);

	recompute_amp(i);
	/* initialize volume envelope */
	if (voice[i].sample->modes & MODES_ENVELOPE)
	{
		/* Ramp up from 0 */
		voice[i].envelope_stage = EG_GUS_ATTACK;
		voice[i].envelope_volume = 0;
		voice[i].control_counter = 0;
		mixer->recompute_envelope(i);
		mixer->apply_envelope_to_amp(i);
	}
	else
	{
		voice[i].envelope_increment = 0;
		mixer->apply_envelope_to_amp(i);
	}

	voice[i].timeout = -1;
}

void Player::finish_note(int i)
{
    if (voice[i].sample->modes & MODES_ENVELOPE)
    {
		/* We need to get the envelope out of Sustain stage. */
		/* Note that voice[i].envelope_stage < EG_GUS_RELEASE1 */
		voice[i].status = VOICE_OFF;
		voice[i].envelope_stage = EG_GUS_RELEASE1;
		mixer->recompute_envelope(i);
		voice[i].modenv_stage = EG_GUS_RELEASE1;
		mixer->recompute_modulation_envelope(i);
		mixer->apply_modulation_envelope(i);
		mixer->apply_envelope_to_amp(i);
	}
    else
    {
		/* Set status to OFF so resample_voice() will let this voice out
		of its loop, if any. In any case, this voice dies when it
			hits the end of its data (ofs>=data_length). */
		if(voice[i].status != VOICE_OFF)
		{
		voice[i].status = VOICE_OFF;
		}
    }
}

void Player::set_envelope_time(int ch, int val, int stage)
{
	val = val & 0x7F;
#if 0
	switch(stage) {
	case EG_ATTACK:	/* Attack */
		//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Attack Time (CH:%d VALUE:%d)", ch, val);
		break;
	case EG_DECAY: /* Decay */
		//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Decay Time (CH:%d VALUE:%d)", ch, val);
		break;
	case EG_RELEASE:	/* Release */
		//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Release Time (CH:%d VALUE:%d)", ch, val);
		break;
	default:
		//ctl_cmsg(CMSG_INFO,VERB_NOISY,"? Time (CH:%d VALUE:%d)", ch, val);
	}
#endif
	channel[ch].envelope_rate[stage] = val;
}



/* Yet another chorus implementation
 *	by Eric A. Welsh <ewelsh@gpc.wustl.edu>.
 */
void Player::new_chorus_voice_alternate(int v1, int level)
{
    int v2, ch, panlevel;
    uint8_t pan;
    double delay;
    double freq, frac;
    int note_adjusted;

    if((v2 = find_free_voice()) == -1)
	return;
    ch = voice[v1].channel;
    voice[v2] = voice[v1];

    /* NRPN Chorus Send Level of Drum */
    if(ISDRUMCHANNEL(ch) && channel[ch].drums[voice[v1].note] != NULL) {
	level *= (double)channel[ch].drums[voice[v1].note]->chorus_level / 127.0;
    }

    /* for our purposes, hard left will be equal to 1 instead of 0 */
    pan = voice[v1].panning;
    if (!pan) pan = 1;

    /* Choose lower voice index for base voice (v1) */
    if(v1 > v2)
    {
    	v1 ^= v2;
    	v2 ^= v1;
    	v1 ^= v2;
    }

    /* Make doubled link v1 and v2 */
    voice[v1].chorus_link = v2;
    voice[v2].chorus_link = v1;

    /* detune notes for chorus effect */
    level >>= 2;		/* scale to a "better" value */
    if (level)
    {
        if(channel[ch].pitchbend + level < 0x2000)
            voice[v2].orig_frequency *= bend_fine[level];
        else
	    voice[v2].orig_frequency /= bend_fine[level];
        voice[v2].cache = NULL;
    }

    delay = 0.003;

    /* Try to keep the delayed voice from cancelling out the other voice */
    /* Pitch detection is used to find the real pitches for drums and MODs */
    note_adjusted = voice[v1].note + voice[v1].sample->transpose_detected;
    if (note_adjusted > 127) note_adjusted = 127;
    else if (note_adjusted < 0) note_adjusted = 0;
    freq = pitch_freq_table[note_adjusted];
    delay *= freq;
    frac = delay - floor(delay);

    /* force the delay away from 0.5 period */
    if (frac < 0.5 && frac > 0.40)
    {
    	delay = (floor(delay) + 0.40) / freq;
   	    delay += (0.5 - frac) * (1.0 - labs(64 - pan) / 63.0) / freq;
    }
    else if (frac >= 0.5 && frac < 0.60)
    {
    	delay = (floor(delay) + 0.60) / freq;
   	    delay += (0.5 - frac) * (1.0 - labs(64 - pan) / 63.0) / freq;
    }
    else
	delay = 0.003;

    /* set panning & delay for pseudo-surround effect */
    {
        panlevel = 63;
        if (pan - panlevel < 1) panlevel = pan - 1;
        if (pan + panlevel > 127) panlevel = 127 - pan;
        voice[v1].panning -= panlevel;
        voice[v2].panning += panlevel;

        /* choose which voice is delayed based on panning */
        if (voice[v1].panned == PANNED_CENTER) {
            /* randomly choose which voice is delayed */
            if (int_rand(2))
                voice[v1].delay += (int)(playback_rate * delay);
            else
                voice[v2].delay += (int)(playback_rate * delay);
        }
        else if (pan - 64 < 0) {
            voice[v2].delay += (int)(playback_rate * delay);
        }
        else {
            voice[v1].delay += (int)(playback_rate * delay);
        }
    }

    /* check for similar drums playing simultaneously with center pans */
    if (ISDRUMCHANNEL(ch) && voice[v1].panned == PANNED_CENTER)
    {
    	int i, j;
    
    	/* force Rimshot (37), Snare1 (38), Snare2 (40), and XG #34 to have
    	 * the same delay, otherwise there will be bad voice cancellation.
    	 */
    	if (voice[v1].note == 37 ||
    	    voice[v1].note == 38 ||
    	    voice[v1].note == 40 ||
    	    (voice[v1].note == 34 && play_system_mode == XG_SYSTEM_MODE))
    	{
    	    for (i = 0; i < upper_voices; i++)
    	    {
    	    	if (voice[i].status & (VOICE_DIE | VOICE_FREE))
    	    	    continue;

    	    	if (!ISDRUMCHANNEL(voice[i].channel))
    	    	    continue;

	    	if (i == v1 || i == v2)
	    	    continue;

	    	if (voice[i].note == 37 ||
	    	    voice[i].note == 38 ||
	    	    voice[i].note == 40 ||
	    	    (voice[i].note == 34 &&
	    	     play_system_mode == XG_SYSTEM_MODE))
	    	{
	    	    j = voice[i].chorus_link;

	    	    if (voice[i].panned == PANNED_LEFT &&
	    	        voice[j].panned == PANNED_RIGHT)
	    	    {
	    	    	voice[v1].delay = voice[i].delay;
	    	    	voice[v2].delay = voice[j].delay;

	    	    	break;
	    	    }
	    	}
    	    }
    	}

    	/* force Kick1 (35), Kick2 (36), and XG Kick #33 to have the same
    	 * delay, otherwise there will be bad voice cancellation.
    	 */
    	if (voice[v1].note == 35 ||
    	    voice[v1].note == 36 ||
    	    (voice[v1].note == 33 && play_system_mode == XG_SYSTEM_MODE))
    	{
    	    for (i = 0; i < upper_voices; i++)
    	    {
    	    	if (voice[i].status & (VOICE_DIE | VOICE_FREE))
    	    	    continue;

    	    	if (!ISDRUMCHANNEL(voice[i].channel))
    	    	    continue;

	    	if (i == v1 || i == v2)
	    	    continue;

	    	if (voice[i].note == 35 ||
	    	    voice[i].note == 36 ||
	    	    (voice[i].note == 33 &&
	    	     play_system_mode == XG_SYSTEM_MODE))
	    	{
	    	    j = voice[i].chorus_link;

	    	    if (voice[i].panned == PANNED_LEFT &&
	    	        voice[j].panned == PANNED_RIGHT)
	    	    {
	    	    	voice[v1].delay = voice[i].delay;
	    	    	voice[v2].delay = voice[j].delay;

	    	    	break;
	    	    }
	    	}
    	    }
    	}
    }

    init_voice_pan_delay(v1);
    init_voice_pan_delay(v2);

    recompute_amp(v1);
	mixer->apply_envelope_to_amp(v1);
    recompute_amp(v2);
	mixer->apply_envelope_to_amp(v2);
    if (level) recompute_freq(v2);
}

void Player::note_on(MidiEvent *e)
{
    int i, nv, v, ch, note;
    int vlist[32];
    int vid;
	int32_t random_delay = 0;

	ch = e->channel;
	note = MIDI_EVENT_NOTE(e);
	
	if(ISDRUMCHANNEL(ch) &&
	   channel[ch].drums[note] != NULL &&
	   !get_rx_drum(channel[ch].drums[note], RX_NOTE_ON)) {	/* Rx. Note On */
		return;
	}
	if(channel[ch].note_limit_low > note ||
		channel[ch].note_limit_high < note ||
		channel[ch].vel_limit_low > e->b ||
		channel[ch].vel_limit_high < e->b) {
		return;
	}
    if((nv = find_samples(e, vlist)) == 0)
	return;

    vid = new_vidq(e->channel, note);

	recompute_bank_parameter(ch, note);
	recompute_channel_filter(ch, note);
	random_delay = calc_random_delay(ch, note);

    for(i = 0; i < nv; i++)
    {
	v = vlist[i];
	if(ISDRUMCHANNEL(ch) &&
	   channel[ch].drums[note] != NULL &&
	   channel[ch].drums[note]->pan_random)
	    channel[ch].drums[note]->drum_panning = int_rand(128);
	else if(channel[ch].pan_random)
	{
	    channel[ch].panning = int_rand(128);
	}
	start_note(e, v, vid, nv - i - 1);
	voice[v].delay += random_delay;
	voice[v].modenv_delay += random_delay;
	voice[v].old_left_mix = voice[v].old_right_mix =
	voice[v].left_mix_inc = voice[v].left_mix_offset =
	voice[v].right_mix_inc = voice[v].right_mix_offset = 0;
	if(timidity_surround_chorus)
	    new_chorus_voice_alternate(v, 0);
    }

    channel[ch].legato_flag = 1;
}

/*! sostenuto is now implemented as an instant sustain */
void Player::update_sostenuto_controls(int ch)
{
  int uv = upper_voices, i;

  if(ISDRUMCHANNEL(ch) || channel[ch].sostenuto == 0) {return;}

  for(i = 0; i < uv; i++)
  {
	if ((voice[i].status & (VOICE_ON | VOICE_OFF))
			&& voice[i].channel == ch)
	 {
		  voice[i].status = VOICE_SUSTAINED;
		  voice[i].envelope_stage = EG_GUS_RELEASE1;
		  mixer->recompute_envelope(i);
	 }
  }
}

/*! redamper effect for piano instruments */
void Player::update_redamper_controls(int ch)
{
  int uv = upper_voices, i;

  if(ISDRUMCHANNEL(ch) || channel[ch].damper_mode == 0) {return;}

  for(i = 0; i < uv; i++)
  {
	if ((voice[i].status & (VOICE_ON | VOICE_OFF))
			&& voice[i].channel == ch)
	  {
		  voice[i].status = VOICE_SUSTAINED;
		  voice[i].envelope_stage = EG_GUS_RELEASE1;
		  mixer->recompute_envelope(i);
	  }
  }
}

void Player::note_off(MidiEvent *e)
{
  int uv = upper_voices, i;
  int ch, note, vid, sustain;

  ch = e->channel;
  note = MIDI_EVENT_NOTE(e);

  if(ISDRUMCHANNEL(ch))
  {
      int nbank, nprog;

      nbank = channel[ch].bank;
      nprog = note;
      instruments->instrument_map(channel[ch].mapID, &nbank, &nprog);
      
      if (channel[ch].drums[nprog] != NULL &&
          get_rx_drum(channel[ch].drums[nprog], RX_NOTE_OFF))
      {
          auto bank = instruments->drumSet(nbank);
          if(bank == NULL) bank = instruments->drumSet(0);
          
          /* uh oh, this drum doesn't have an instrument loaded yet */
          if (bank->tone[nprog].instrument == NULL)
              return;

          /* this drum is not loaded for some reason (error occured?) */
          if (IS_MAGIC_INSTRUMENT(bank->tone[nprog].instrument))
              return;

          /* only disallow Note Off if the drum sample is not looped */
          if (!(bank->tone[nprog].instrument->sample->modes & MODES_LOOPING))
              return;	/* Note Off is not allowed. */
      }
  }

  if ((vid = last_vidq(ch, note)) == -1)
      return;
  sustain = channel[ch].sustain;
  for (i = 0; i < uv; i++)
  {
      if(voice[i].status == VOICE_ON &&
	 voice[i].channel == ch &&
	 voice[i].note == note &&
	 voice[i].vid == vid)
      {
	  if(sustain)
	  {
	      voice[i].status = VOICE_SUSTAINED;
	  }
	  else
	      finish_note(i);
      }
  }

  channel[ch].legato_flag = 0;
}

/* Process the All Notes Off event */
void Player::all_notes_off(int c)
{
  int i, uv = upper_voices;
  ctl_cmsg(CMSG_INFO, VERB_DEBUG, "All notes off on channel %d", c);
  for(i = 0; i < uv; i++)
    if (voice[i].status==VOICE_ON &&
	voice[i].channel==c)
      {
	if (channel[c].sustain)
	  {
	    voice[i].status=VOICE_SUSTAINED;
	  }
	else
	  finish_note(i);
      }
  for(i = 0; i < 128; i++)
      vidq_head[c * 128 + i] = vidq_tail[c * 128 + i] = 0;
}

/* Process the All Sounds Off event */
void Player::all_sounds_off(int c)
{
  int i, uv = upper_voices;
  for(i = 0; i < uv; i++)
    if (voice[i].channel==c &&
	(voice[i].status & ~(VOICE_FREE | VOICE_DIE)))
      {
	kill_note(i);
      }
  for(i = 0; i < 128; i++)
      vidq_head[c * 128 + i] = vidq_tail[c * 128 + i] = 0;
}

/*! adjust polyphonic key pressure (PAf, PAT) */
void Player::adjust_pressure(MidiEvent *e)
{
    int i, uv = upper_voices;
    int note, ch;

    if(timidity_channel_pressure)
    {
	ch = e->channel;
    note = MIDI_EVENT_NOTE(e);
	channel[ch].paf.val = e->b;
	if(channel[ch].paf.pitch != 0) {channel[ch].pitchfactor = 0;}

    for(i = 0; i < uv; i++)
    if(voice[i].status == VOICE_ON &&
       voice[i].channel == ch &&
       voice[i].note == note)
    {
		recompute_amp(i);
		mixer->apply_envelope_to_amp(i);
		recompute_freq(i);
		recompute_voice_filter(i);
    }
	}
}

/*! adjust channel pressure (channel aftertouch, CAf, CAT) */
void Player::adjust_channel_pressure(MidiEvent *e)
{
    if(timidity_channel_pressure)
    {
	int i, uv = upper_voices;
	int ch;

	ch = e->channel;
	channel[ch].caf.val = e->a;
	if(channel[ch].caf.pitch != 0) {channel[ch].pitchfactor = 0;}
	  
	for(i = 0; i < uv; i++)
	{
	    if(voice[i].status == VOICE_ON && voice[i].channel == ch)
	    {
		recompute_amp(i);
		mixer->apply_envelope_to_amp(i);
		recompute_freq(i);
		recompute_voice_filter(i);
		}
	}
    }
}

void Player::adjust_panning(int c)
{
    int i, uv = upper_voices, pan = channel[c].panning;
    for(i = 0; i < uv; i++)
    {
	if ((voice[i].channel==c) &&
	    (voice[i].status & (VOICE_ON | VOICE_SUSTAINED)))
	{
            /* adjust pan to include drum/sample pan offsets */
            pan = get_panning(c, voice[i].note, i);

	    /* Hack to handle -EFchorus=2 in a "reasonable" way */
	    if(timidity_surround_chorus && voice[i].chorus_link != i)
	    {
		int v1, v2;

		if(i >= voice[i].chorus_link)
		    /* `i' is not base chorus voice.
		     *  This sub voice is already updated.
		     */
		    continue;

		v1 = i;				/* base voice */
		v2 = voice[i].chorus_link;	/* sub voice (detuned) */

		if(timidity_surround_chorus) /* Surround chorus mode by Eric. */
		{
		    int panlevel;

		    if (!pan) pan = 1;	/* make hard left be 1 instead of 0 */
		    panlevel = 63;
		    if (pan - panlevel < 1) panlevel = pan - 1;
		    if (pan + panlevel > 127) panlevel = 127 - pan;
		    voice[v1].panning = pan - panlevel;
		    voice[v2].panning = pan + panlevel;
		}
		else
		{
		    voice[v1].panning = pan;
		    if(pan > 60 && pan < 68) /* PANNED_CENTER */
			voice[v2].panning =
			    64 + int_rand(40) - 20; /* 64 +- rand(20) */
		    else if(pan < CHORUS_OPPOSITE_THRESHOLD)
			voice[v2].panning = 127;
		    else if(pan > 127 - CHORUS_OPPOSITE_THRESHOLD)
			voice[v2].panning = 0;
		    else
			voice[v2].panning = (pan < 64 ? 0 : 127);
		}
		recompute_amp(v2);
		mixer->apply_envelope_to_amp(v2);
		/* v1 == i, so v1 will be updated next */
	    }
	    else
		voice[i].panning = pan;

		recompute_amp(i);
		mixer->apply_envelope_to_amp(i);
	}
    }
}

void Player::play_midi_setup_drums(int ch, int note)
{
    channel[ch].drums[note] = (struct DrumParts *)
	new_segment(&playmidi_pool, sizeof(struct DrumParts));
    reset_drum_controllers(channel[ch].drums, note);
}

void Player::adjust_drum_panning(int ch, int note)
{
    int i, uv = upper_voices;

    for(i = 0; i < uv; i++) {
		if(voice[i].channel == ch &&
		   voice[i].note == note &&
		   (voice[i].status & (VOICE_ON | VOICE_SUSTAINED)))
		{
			voice[i].panning = get_panning(ch, note, i);
			recompute_amp(i);
			mixer->apply_envelope_to_amp(i);
		}
	}
}

void Player::drop_sustain(int c)
{
  int i, uv = upper_voices;
  for(i = 0; i < uv; i++)
    if (voice[i].status == VOICE_SUSTAINED && voice[i].channel == c)
      finish_note(i);
}

void Player::adjust_all_pitch(void)
{
	int ch, i, uv = upper_voices;
	
	for (ch = 0; ch < MAX_CHANNELS; ch++)
		channel[ch].pitchfactor = 0;
	for (i = 0; i < uv; i++)
		if (voice[i].status != VOICE_FREE)
			recompute_freq(i);
}

void Player::adjust_pitch(int c)
{
  int i, uv = upper_voices;
  for(i = 0; i < uv; i++)
    if (voice[i].status != VOICE_FREE && voice[i].channel == c)
	recompute_freq(i);
}

void Player::adjust_volume(int c)
{
  int i, uv = upper_voices;
  for(i = 0; i < uv; i++)
    if (voice[i].channel == c &&
	(voice[i].status & (VOICE_ON | VOICE_SUSTAINED)))
      {
	recompute_amp(i);
	mixer->apply_envelope_to_amp(i);
      }
}

void Player::set_reverb_level(int ch, int level)
{
	if (level == -1) {
		channel[ch].reverb_level = channel[ch].reverb_id =
				(timidity_reverb < 0)
				? -timidity_reverb & 0x7f : DEFAULT_REVERB_SEND_LEVEL;
		make_rvid_flag = 1;
		return;
	}
	channel[ch].reverb_level = level;
	make_rvid_flag = 0;	/* to update reverb_id */
}

int Player::get_reverb_level(int ch)
{
	if (channel[ch].reverb_level == -1)
		return (timidity_reverb < 0)
			? -timidity_reverb & 0x7f : DEFAULT_REVERB_SEND_LEVEL;
	return channel[ch].reverb_level;
}

int Player::get_chorus_level(int ch)
{
#ifdef DISALLOW_DRUM_BENDS
    if(ISDRUMCHANNEL(ch))
	return 0; /* Not supported drum channel chorus */
#endif
    if(timidity_chorus == 1)
	return channel[ch].chorus_level;
    return -timidity_chorus;
}


void Player::free_drum_effect(int ch)
{
	int i;
	if (channel[ch].drum_effect != NULL) {
		for (i = 0; i < channel[ch].drum_effect_num; i++) {
			if (channel[ch].drum_effect[i].buf != NULL) {
				free(channel[ch].drum_effect[i].buf);
				channel[ch].drum_effect[i].buf = NULL;
			}
		}
		free(channel[ch].drum_effect);
		channel[ch].drum_effect = NULL;
	}
	channel[ch].drum_effect_num = 0;
	channel[ch].drum_effect_flag = 0;
}

void Player::make_drum_effect(int ch)
{
	int i, note, num = 0;
	int8_t note_table[128];
	struct DrumParts *drum;
	struct DrumPartEffect *de;

	if (channel[ch].drum_effect_flag == 0) {
		free_drum_effect(ch);
		memset(note_table, 0, sizeof(int8_t) * 128);

		for(i = 0; i < 128; i++) {
			if ((drum = channel[ch].drums[i]) != NULL)
			{
				if (drum->reverb_level != -1
				|| drum->chorus_level != -1 || drum->delay_level != -1) {
					note_table[num++] = i;
				}
			}
		}

		channel[ch].drum_effect = (struct DrumPartEffect *)safe_malloc(sizeof(struct DrumPartEffect) * num);

		for(i = 0; i < num; i++) {
			de = &(channel[ch].drum_effect[i]);
			de->note = note = note_table[i];
			drum = channel[ch].drums[note];
			de->reverb_send = (int32_t)drum->reverb_level * (int32_t)get_reverb_level(ch) / 127;
			de->chorus_send = (int32_t)drum->chorus_level * (int32_t)channel[ch].chorus_level / 127;
			de->delay_send = (int32_t)drum->delay_level * (int32_t)channel[ch].delay_level / 127;
			de->buf = (int32_t *)safe_malloc(AUDIO_BUFFER_SIZE * 8);
			memset(de->buf, 0, AUDIO_BUFFER_SIZE * 8);
		}

		channel[ch].drum_effect_num = num;
		channel[ch].drum_effect_flag = 1;
	}
}

void Player::adjust_master_volume(void)
{
  int i, uv = upper_voices;
  adjust_amplification();
  for(i = 0; i < uv; i++)
      if(voice[i].status & (VOICE_ON | VOICE_SUSTAINED))
      {
	  recompute_amp(i);
	  mixer->apply_envelope_to_amp(i);
      }
}

int Player::midi_drumpart_change(int ch, int isdrum)
{
	if (IS_SET_CHANNELMASK(drumchannel_mask, ch))
		return 0;
	if (isdrum) {
		SET_CHANNELMASK(drumchannels, ch);
		SET_CHANNELMASK(current_file_info->drumchannels, ch);
	} else {
		UNSET_CHANNELMASK(drumchannels, ch);
		UNSET_CHANNELMASK(current_file_info->drumchannels, ch);
	}
	return 1;
}

void Player::midi_program_change(int ch, int prog)
{
	int dr = ISDRUMCHANNEL(ch);
	int newbank, b, p, map;
	
	switch (play_system_mode) {
	case GS_SYSTEM_MODE:	/* GS */
		if ((map = channel[ch].bank_lsb) == 0) {
			map = channel[ch].tone_map0_number;
		}
		switch (map) {
		case 0:		/* No change */
			break;
		case 1:
			channel[ch].mapID = (dr) ? SC_55_DRUM_MAP : SC_55_TONE_MAP;
			break;
		case 2:
			channel[ch].mapID = (dr) ? SC_88_DRUM_MAP : SC_88_TONE_MAP;
			break;
		case 3:
			channel[ch].mapID = (dr) ? SC_88PRO_DRUM_MAP : SC_88PRO_TONE_MAP;
			break;
		case 4:
			channel[ch].mapID = (dr) ? SC_8850_DRUM_MAP : SC_8850_TONE_MAP;
			break;
		default:
			break;
		}
		newbank = channel[ch].bank_msb;
		break;
	case XG_SYSTEM_MODE:	/* XG */
		switch (channel[ch].bank_msb) {
		case 0:		/* Normal */
#if 0
			if (ch == 9 && channel[ch].bank_lsb == 127
					&& channel[ch].mapID == XG_DRUM_MAP)
				/* FIXME: Why this part is drum?  Is this correct? */
				break;
#endif
/* Eric's explanation for the FIXME (March 2004):
 *
 * I don't have the original email from my archived inbox, but I found a
 * reply I made in my archived sent-mail from 1999.  A September 5th message
 * to Masanao Izumo is discussing a problem with a "reapxg.mid", a file which
 * I still have, and how it issues an MSB=0 with a program change on ch 9, 
 * thus turning it into a melodic channel.  The strange thing is, this doesn't
 * happen on XG hardware, nor on the XG softsynth.  It continues to play as a
 * normal drum.  The author of the midi file obviously intended it to be
 * drumset 16 too.  The original fix was to detect LSB == -1, then break so
 * as to not set it to a melodic channel.  I'm guessing that this somehow got
 * mutated into checking for 127 instead, and the current FIXME is related to
 * the original hack from Sept 1999.  The Sept 5th email discusses patches
 * being applied to version 2.5.1 to get XG drums to work properly, and a
 * Sept 7th email to someone else discusses the fixes being part of the
 * latest 2.6.0-beta3.  A September 23rd email to Masanao Izumo specifically
 * mentions the LSB == -1 hack (and reapxg.mid not playing "correctly"
 * anymore), as well as new changes in 2.6.0 that broke a lot of other XG
 * files (XG drum support was extremely buggy in 1999 and we were still trying
 * to figure out how to initialize things to reproduce hardware behavior).  An
 * October 5th email says that 2.5.1 was correct, 2.6.0 had very broken XG
 * drum changes, and 2.6.1 still has problems.  Further discussions ensued
 * over what was "correct": to follow the XG spec, or to reproduce
 * "features" / bugs in the hardware.  I can't find the rest of the
 * discussions, but I think it ended with us agreeing to just follow the spec
 * and not try to reproduce the hardware strangeness.  I don't know how the
 * current FIXME wound up the way it is now.  I'm still going to guess it is
 * related to the old reapxg.mid hack.
 *
 * Now that reset_midi() initializes channel[ch].bank_lsb to 0 instead of -1,
 * checking for LSB == -1 won't do anything anymore, so changing the above
 * FIXME to the original == -1 won't do any good.  It is best to just #if 0
 * it out and leave it here as a reminder that there is at least one XG
 * hardware / softsynth "bug" that is not reproduced by timidity at the
 * moment.
 *
 * If the current FIXME actually reproduces some other XG hadware bug that
 * I don't know about, then it may have a valid purpose.  I just don't know
 * what that purpose is at the moment.  Perhaps someone else does?  I still
 * have src going back to 2.10.4, and the FIXME comment was already there by
 * then.  I don't see any entries in the Changelog that could explain it
 * either.  If someone has src from 2.5.1 through 2.10.3 and wants to
 * investigate this further, go for it :)
 */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = XG_NORMAL_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 64:	/* SFX voice */
			midi_drumpart_change(ch, 0);
			channel[ch].mapID = XG_SFX64_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 126:	/* SFX kit */
			midi_drumpart_change(ch, 1);
			channel[ch].mapID = XG_SFX126_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		case 127:	/* Drum kit */
			midi_drumpart_change(ch, 1);
			channel[ch].mapID = XG_DRUM_MAP;
			dr = ISDRUMCHANNEL(ch);
			break;
		default:
			break;
		}
		newbank = channel[ch].bank_lsb;
		break;
	case GM2_SYSTEM_MODE:	/* GM2 */
		if ((channel[ch].bank_msb & 0xfe) == 0x78) {	/* 0x78/0x79 */
			midi_drumpart_change(ch, channel[ch].bank_msb == 0x78);
			dr = ISDRUMCHANNEL(ch);
		}
		channel[ch].mapID = (dr) ? GM2_DRUM_MAP : GM2_TONE_MAP;
		newbank = channel[ch].bank_lsb;
		break;
	default:
		newbank = channel[ch].bank_msb;
		break;
	}
	if (dr) {
		channel[ch].bank = prog;	/* newbank is ignored */
		channel[ch].program = prog;
		if (instruments->drumSet(prog) == NULL || instruments->drumSet(prog)->alt == NULL)
			channel[ch].altassign = instruments->drumSet(0)->alt;
		else
			channel[ch].altassign = instruments->drumSet(prog)->alt;
	} else {
		channel[ch].bank = (special_tonebank >= 0)
				? special_tonebank : newbank;
		channel[ch].program = (instruments->defaultProgram(ch) == SPECIAL_PROGRAM)
				? SPECIAL_PROGRAM : prog;
		channel[ch].altassign = NULL;
		if (opt_realtime_playing) 
		{
			b = channel[ch].bank, p = prog;
			instruments->instrument_map(channel[ch].mapID, &b, &p);
			play_midi_load_instrument(0, b, p);
		}
	}
}


/*! add a new layer. */
void Player::add_channel_layer(int to_ch, int from_ch)
{
	if (to_ch >= MAX_CHANNELS || from_ch >= MAX_CHANNELS)
		return;
	/* add a channel layer */
	UNSET_CHANNELMASK(channel[to_ch].channel_layer, to_ch);
	SET_CHANNELMASK(channel[to_ch].channel_layer, from_ch);
	//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Channel Layer (CH:%d -> CH:%d)", from_ch, to_ch);
}

/*! remove all layers for this channel. */
void Player::remove_channel_layer(int ch)
{
	int i, offset;

	if (ch >= MAX_CHANNELS)
		return;
	/* remove channel layers */
	offset = ch & ~0xf;
	for (i = offset; i < offset + REDUCE_CHANNELS; i++)
		UNSET_CHANNELMASK(channel[i].channel_layer, ch);
	SET_CHANNELMASK(channel[ch].channel_layer, ch);
}

/*! process system exclusive sent from parse_sysex_event_multi(). */
void Player::process_sysex_event(int ev, int ch, int val, int b)
{
	int temp, msb, note;

	if (ch >= MAX_CHANNELS)
		return;
	if (ev == ME_SYSEX_MSB) {
		channel[ch].sysex_msb_addr = b;
		channel[ch].sysex_msb_val = val;
	} else if(ev == ME_SYSEX_GS_MSB) {
		channel[ch].sysex_gs_msb_addr = b;
		channel[ch].sysex_gs_msb_val = val;
	} else if(ev == ME_SYSEX_XG_MSB) {
		channel[ch].sysex_xg_msb_addr = b;
		channel[ch].sysex_xg_msb_val = val;
	} else if(ev == ME_SYSEX_LSB) {	/* Universal system exclusive message */
		msb = channel[ch].sysex_msb_addr;
		note = channel[ch].sysex_msb_val;
		channel[ch].sysex_msb_addr = channel[ch].sysex_msb_val = 0;
		switch(b)
		{
		case 0x00:	/* CAf Pitch Control */
			if(val > 0x58) {val = 0x58;}
			else if(val < 0x28) {val = 0x28;}
			channel[ch].caf.pitch = val - 64;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"CAf Pitch Control (CH:%d %d semitones)", ch, channel[ch].caf.pitch);
			break;
		case 0x01:	/* CAf Filter Cutoff Control */
			channel[ch].caf.cutoff = (val - 64) * 150;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"CAf Filter Cutoff Control (CH:%d %d cents)", ch, channel[ch].caf.cutoff);
			break;
		case 0x02:	/* CAf Amplitude Control */
			channel[ch].caf.amp = (float)val / 64.0f - 1.0f;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"CAf Amplitude Control (CH:%d %.2f)", ch, channel[ch].caf.amp);
			break;
		case 0x03:	/* CAf LFO1 Rate Control */
			channel[ch].caf.lfo1_rate = (float)(val - 64) / 6.4f;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"CAf LFO1 Rate Control (CH:%d %.1f Hz)", ch, channel[ch].caf.lfo1_rate);
			break;
		case 0x04:	/* CAf LFO1 Pitch Depth */
			channel[ch].caf.lfo1_pitch_depth = conv_lfo_pitch_depth(val);
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"CAf LFO1 Pitch Depth (CH:%d %d cents)", ch, channel[ch].caf.lfo1_pitch_depth); 
			break;
		case 0x05:	/* CAf LFO1 Filter Depth */
			channel[ch].caf.lfo1_tvf_depth = conv_lfo_filter_depth(val);
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"CAf LFO1 Filter Depth (CH:%d %d cents)", ch, channel[ch].caf.lfo1_tvf_depth); 
			break;
		case 0x06:	/* CAf LFO1 Amplitude Depth */
			channel[ch].caf.lfo1_tva_depth = (float)val / 127.0f;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"CAf LFO1 Amplitude Depth (CH:%d %.2f)", ch, channel[ch].caf.lfo1_tva_depth); 
			break;
		case 0x07:	/* CAf LFO2 Rate Control */
			channel[ch].caf.lfo2_rate = (float)(val - 64) / 6.4f;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"CAf LFO2 Rate Control (CH:%d %.1f Hz)", ch, channel[ch].caf.lfo2_rate);
			break;
		case 0x08:	/* CAf LFO2 Pitch Depth */
			channel[ch].caf.lfo2_pitch_depth = conv_lfo_pitch_depth(val);
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"CAf LFO2 Pitch Depth (CH:%d %d cents)", ch, channel[ch].caf.lfo2_pitch_depth); 
			break;
		case 0x09:	/* CAf LFO2 Filter Depth */
			channel[ch].caf.lfo2_tvf_depth = conv_lfo_filter_depth(val);
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"CAf LFO2 Filter Depth (CH:%d %d cents)", ch, channel[ch].caf.lfo2_tvf_depth); 
			break;
		case 0x0A:	/* CAf LFO2 Amplitude Depth */
			channel[ch].caf.lfo2_tva_depth = (float)val / 127.0f;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"CAf LFO2 Amplitude Depth (CH:%d %.2f)", ch, channel[ch].caf.lfo2_tva_depth); 
			break;
		case 0x0B:	/* PAf Pitch Control */
			if(val > 0x58) {val = 0x58;}
			else if(val < 0x28) {val = 0x28;}
			channel[ch].paf.pitch = val - 64;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"PAf Pitch Control (CH:%d %d semitones)", ch, channel[ch].paf.pitch);
			break;
		case 0x0C:	/* PAf Filter Cutoff Control */
			channel[ch].paf.cutoff = (val - 64) * 150;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"PAf Filter Cutoff Control (CH:%d %d cents)", ch, channel[ch].paf.cutoff);
			break;
		case 0x0D:	/* PAf Amplitude Control */
			channel[ch].paf.amp = (float)val / 64.0f - 1.0f;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"PAf Amplitude Control (CH:%d %.2f)", ch, channel[ch].paf.amp);
			break;
		case 0x0E:	/* PAf LFO1 Rate Control */
			channel[ch].paf.lfo1_rate = (float)(val - 64) / 6.4f;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"PAf LFO1 Rate Control (CH:%d %.1f Hz)", ch, channel[ch].paf.lfo1_rate);
			break;
		case 0x0F:	/* PAf LFO1 Pitch Depth */
			channel[ch].paf.lfo1_pitch_depth = conv_lfo_pitch_depth(val);
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"PAf LFO1 Pitch Depth (CH:%d %d cents)", ch, channel[ch].paf.lfo1_pitch_depth); 
			break;
		case 0x10:	/* PAf LFO1 Filter Depth */
			channel[ch].paf.lfo1_tvf_depth = conv_lfo_filter_depth(val);
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"PAf LFO1 Filter Depth (CH:%d %d cents)", ch, channel[ch].paf.lfo1_tvf_depth); 
			break;
		case 0x11:	/* PAf LFO1 Amplitude Depth */
			channel[ch].paf.lfo1_tva_depth = (float)val / 127.0f;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"PAf LFO1 Amplitude Depth (CH:%d %.2f)", ch, channel[ch].paf.lfo1_tva_depth); 
			break;
		case 0x12:	/* PAf LFO2 Rate Control */
			channel[ch].paf.lfo2_rate = (float)(val - 64) / 6.4f;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"PAf LFO2 Rate Control (CH:%d %.1f Hz)", ch, channel[ch].paf.lfo2_rate);
			break;
		case 0x13:	/* PAf LFO2 Pitch Depth */
			channel[ch].paf.lfo2_pitch_depth = conv_lfo_pitch_depth(val);
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"PAf LFO2 Pitch Depth (CH:%d %d cents)", ch, channel[ch].paf.lfo2_pitch_depth); 
			break;
		case 0x14:	/* PAf LFO2 Filter Depth */
			channel[ch].paf.lfo2_tvf_depth = conv_lfo_filter_depth(val);
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"PAf LFO2 Filter Depth (CH:%d %d cents)", ch, channel[ch].paf.lfo2_tvf_depth); 
			break;
		case 0x15:	/* PAf LFO2 Amplitude Depth */
			channel[ch].paf.lfo2_tva_depth = (float)val / 127.0f;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"PAf LFO2 Amplitude Depth (CH:%d %.2f)", ch, channel[ch].paf.lfo2_tva_depth); 
			break;
		case 0x16:	/* MOD Pitch Control */
			if(val > 0x58) {val = 0x58;}
			else if(val < 0x28) {val = 0x28;}
			channel[ch].mod.pitch = val - 64;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"MOD Pitch Control (CH:%d %d semitones)", ch, channel[ch].mod.pitch);
			break;
		case 0x17:	/* MOD Filter Cutoff Control */
			channel[ch].mod.cutoff = (val - 64) * 150;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"MOD Filter Cutoff Control (CH:%d %d cents)", ch, channel[ch].mod.cutoff);
			break;
		case 0x18:	/* MOD Amplitude Control */
			channel[ch].mod.amp = (float)val / 64.0f - 1.0f;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"MOD Amplitude Control (CH:%d %.2f)", ch, channel[ch].mod.amp);
			break;
		case 0x19:	/* MOD LFO1 Rate Control */
			channel[ch].mod.lfo1_rate = (float)(val - 64) / 6.4f;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"MOD LFO1 Rate Control (CH:%d %.1f Hz)", ch, channel[ch].mod.lfo1_rate);
			break;
		case 0x1A:	/* MOD LFO1 Pitch Depth */
			channel[ch].mod.lfo1_pitch_depth = conv_lfo_pitch_depth(val);
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"MOD LFO1 Pitch Depth (CH:%d %d cents)", ch, channel[ch].mod.lfo1_pitch_depth); 
			break;
		case 0x1B:	/* MOD LFO1 Filter Depth */
			channel[ch].mod.lfo1_tvf_depth = conv_lfo_filter_depth(val);
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"MOD LFO1 Filter Depth (CH:%d %d cents)", ch, channel[ch].mod.lfo1_tvf_depth); 
			break;
		case 0x1C:	/* MOD LFO1 Amplitude Depth */
			channel[ch].mod.lfo1_tva_depth = (float)val / 127.0f;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"MOD LFO1 Amplitude Depth (CH:%d %.2f)", ch, channel[ch].mod.lfo1_tva_depth); 
			break;
		case 0x1D:	/* MOD LFO2 Rate Control */
			channel[ch].mod.lfo2_rate = (float)(val - 64) / 6.4f;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"MOD LFO2 Rate Control (CH:%d %.1f Hz)", ch, channel[ch].mod.lfo2_rate);
			break;
		case 0x1E:	/* MOD LFO2 Pitch Depth */
			channel[ch].mod.lfo2_pitch_depth = conv_lfo_pitch_depth(val);
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"MOD LFO2 Pitch Depth (CH:%d %d cents)", ch, channel[ch].mod.lfo2_pitch_depth); 
			break;
		case 0x1F:	/* MOD LFO2 Filter Depth */
			channel[ch].mod.lfo2_tvf_depth = conv_lfo_filter_depth(val);
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"MOD LFO2 Filter Depth (CH:%d %d cents)", ch, channel[ch].mod.lfo2_tvf_depth); 
			break;
		case 0x20:	/* MOD LFO2 Amplitude Depth */
			channel[ch].mod.lfo2_tva_depth = (float)val / 127.0f;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"MOD LFO2 Amplitude Depth (CH:%d %.2f)", ch, channel[ch].mod.lfo2_tva_depth); 
			break;
		case 0x21:	/* BEND Pitch Control */
			if(val > 0x58) {val = 0x58;}
			else if(val < 0x28) {val = 0x28;}
			channel[ch].bend.pitch = val - 64;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"BEND Pitch Control (CH:%d %d semitones)", ch, channel[ch].bend.pitch);
			break;
		case 0x22:	/* BEND Filter Cutoff Control */
			channel[ch].bend.cutoff = (val - 64) * 150;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"BEND Filter Cutoff Control (CH:%d %d cents)", ch, channel[ch].bend.cutoff);
			break;
		case 0x23:	/* BEND Amplitude Control */
			channel[ch].bend.amp = (float)val / 64.0f - 1.0f;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"BEND Amplitude Control (CH:%d %.2f)", ch, channel[ch].bend.amp);
			break;
		case 0x24:	/* BEND LFO1 Rate Control */
			channel[ch].bend.lfo1_rate = (float)(val - 64) / 6.4f;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"BEND LFO1 Rate Control (CH:%d %.1f Hz)", ch, channel[ch].bend.lfo1_rate);
			break;
		case 0x25:	/* BEND LFO1 Pitch Depth */
			channel[ch].bend.lfo1_pitch_depth = conv_lfo_pitch_depth(val);
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"BEND LFO1 Pitch Depth (CH:%d %d cents)", ch, channel[ch].bend.lfo1_pitch_depth); 
			break;
		case 0x26:	/* BEND LFO1 Filter Depth */
			channel[ch].bend.lfo1_tvf_depth = conv_lfo_filter_depth(val);
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"BEND LFO1 Filter Depth (CH:%d %d cents)", ch, channel[ch].bend.lfo1_tvf_depth); 
			break;
		case 0x27:	/* BEND LFO1 Amplitude Depth */
			channel[ch].bend.lfo1_tva_depth = (float)val / 127.0f;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"BEND LFO1 Amplitude Depth (CH:%d %.2f)", ch, channel[ch].bend.lfo1_tva_depth); 
			break;
		case 0x28:	/* BEND LFO2 Rate Control */
			channel[ch].bend.lfo2_rate = (float)(val - 64) / 6.4f;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"BEND LFO2 Rate Control (CH:%d %.1f Hz)", ch, channel[ch].bend.lfo2_rate);
			break;
		case 0x29:	/* BEND LFO2 Pitch Depth */
			channel[ch].bend.lfo2_pitch_depth = conv_lfo_pitch_depth(val);
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"BEND LFO2 Pitch Depth (CH:%d %d cents)", ch, channel[ch].bend.lfo2_pitch_depth); 
			break;
		case 0x2A:	/* BEND LFO2 Filter Depth */
			channel[ch].bend.lfo2_tvf_depth = conv_lfo_filter_depth(val);
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"BEND LFO2 Filter Depth (CH:%d %d cents)", ch, channel[ch].bend.lfo2_tvf_depth); 
			break;
		case 0x2B:	/* BEND LFO2 Amplitude Depth */
			channel[ch].bend.lfo2_tva_depth = (float)val / 127.0f;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"BEND LFO2 Amplitude Depth (CH:%d %.2f)", ch, channel[ch].bend.lfo2_tva_depth); 
			break;
		case 0x2C:	/* CC1 Pitch Control */
			if(val > 0x58) {val = 0x58;}
			else if(val < 0x28) {val = 0x28;}
			channel[ch].cc1.pitch = val - 64;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"CC1 Pitch Control (CH:%d %d semitones)", ch, channel[ch].cc1.pitch);
			break;
		case 0x2D:	/* CC1 Filter Cutoff Control */
			channel[ch].cc1.cutoff = (val - 64) * 150;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"CC1 Filter Cutoff Control (CH:%d %d cents)", ch, channel[ch].cc1.cutoff);
			break;
		case 0x2E:	/* CC1 Amplitude Control */
			channel[ch].cc1.amp = (float)val / 64.0f - 1.0f;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"CC1 Amplitude Control (CH:%d %.2f)", ch, channel[ch].cc1.amp);
			break;
		case 0x2F:	/* CC1 LFO1 Rate Control */
			channel[ch].cc1.lfo1_rate = (float)(val - 64) / 6.4f;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"CC1 LFO1 Rate Control (CH:%d %.1f Hz)", ch, channel[ch].cc1.lfo1_rate);
			break;
		case 0x30:	/* CC1 LFO1 Pitch Depth */
			channel[ch].cc1.lfo1_pitch_depth = conv_lfo_pitch_depth(val);
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"CC1 LFO1 Pitch Depth (CH:%d %d cents)", ch, channel[ch].cc1.lfo1_pitch_depth); 
			break;
		case 0x31:	/* CC1 LFO1 Filter Depth */
			channel[ch].cc1.lfo1_tvf_depth = conv_lfo_filter_depth(val);
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"CC1 LFO1 Filter Depth (CH:%d %d cents)", ch, channel[ch].cc1.lfo1_tvf_depth); 
			break;
		case 0x32:	/* CC1 LFO1 Amplitude Depth */
			channel[ch].cc1.lfo1_tva_depth = (float)val / 127.0f;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"CC1 LFO1 Amplitude Depth (CH:%d %.2f)", ch, channel[ch].cc1.lfo1_tva_depth); 
			break;
		case 0x33:	/* CC1 LFO2 Rate Control */
			channel[ch].cc1.lfo2_rate = (float)(val - 64) / 6.4f;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"CC1 LFO2 Rate Control (CH:%d %.1f Hz)", ch, channel[ch].cc1.lfo2_rate);
			break;
		case 0x34:	/* CC1 LFO2 Pitch Depth */
			channel[ch].cc1.lfo2_pitch_depth = conv_lfo_pitch_depth(val);
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"CC1 LFO2 Pitch Depth (CH:%d %d cents)", ch, channel[ch].cc1.lfo2_pitch_depth); 
			break;
		case 0x35:	/* CC1 LFO2 Filter Depth */
			channel[ch].cc1.lfo2_tvf_depth = conv_lfo_filter_depth(val);
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"CC1 LFO2 Filter Depth (CH:%d %d cents)", ch, channel[ch].cc1.lfo2_tvf_depth); 
			break;
		case 0x36:	/* CC1 LFO2 Amplitude Depth */
			channel[ch].cc1.lfo2_tva_depth = (float)val / 127.0f;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"CC1 LFO2 Amplitude Depth (CH:%d %.2f)", ch, channel[ch].cc1.lfo2_tva_depth); 
			break;
		case 0x37:	/* CC2 Pitch Control */
			if(val > 0x58) {val = 0x58;}
			else if(val < 0x28) {val = 0x28;}
			channel[ch].cc2.pitch = val - 64;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"CC2 Pitch Control (CH:%d %d semitones)", ch, channel[ch].cc2.pitch);
			break;
		case 0x38:	/* CC2 Filter Cutoff Control */
			channel[ch].cc2.cutoff = (val - 64) * 150;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"CC2 Filter Cutoff Control (CH:%d %d cents)", ch, channel[ch].cc2.cutoff);
			break;
		case 0x39:	/* CC2 Amplitude Control */
			channel[ch].cc2.amp = (float)val / 64.0f - 1.0f;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"CC2 Amplitude Control (CH:%d %.2f)", ch, channel[ch].cc2.amp);
			break;
		case 0x3A:	/* CC2 LFO1 Rate Control */
			channel[ch].cc2.lfo1_rate = (float)(val - 64) / 6.4f;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"CC2 LFO1 Rate Control (CH:%d %.1f Hz)", ch, channel[ch].cc2.lfo1_rate);
			break;
		case 0x3B:	/* CC2 LFO1 Pitch Depth */
			channel[ch].cc2.lfo1_pitch_depth = conv_lfo_pitch_depth(val);
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"CC2 LFO1 Pitch Depth (CH:%d %d cents)", ch, channel[ch].cc2.lfo1_pitch_depth); 
			break;
		case 0x3C:	/* CC2 LFO1 Filter Depth */
			channel[ch].cc2.lfo1_tvf_depth = conv_lfo_filter_depth(val);
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"CC2 LFO1 Filter Depth (CH:%d %d cents)", ch, channel[ch].cc2.lfo1_tvf_depth); 
			break;
		case 0x3D:	/* CC2 LFO1 Amplitude Depth */
			channel[ch].cc2.lfo1_tva_depth = (float)val / 127.0f;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"CC2 LFO1 Amplitude Depth (CH:%d %.2f)", ch, channel[ch].cc2.lfo1_tva_depth); 
			break;
		case 0x3E:	/* CC2 LFO2 Rate Control */
			channel[ch].cc2.lfo2_rate = (float)(val - 64) / 6.4f;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"CC2 LFO2 Rate Control (CH:%d %.1f Hz)", ch, channel[ch].cc2.lfo2_rate);
			break;
		case 0x3F:	/* CC2 LFO2 Pitch Depth */
			channel[ch].cc2.lfo2_pitch_depth = conv_lfo_pitch_depth(val);
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"CC2 LFO2 Pitch Depth (CH:%d %d cents)", ch, channel[ch].cc2.lfo2_pitch_depth); 
			break;
		case 0x40:	/* CC2 LFO2 Filter Depth */
			channel[ch].cc2.lfo2_tvf_depth = conv_lfo_filter_depth(val);
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"CC2 LFO2 Filter Depth (CH:%d %d cents)", ch, channel[ch].cc2.lfo2_tvf_depth); 
			break;
		case 0x41:	/* CC2 LFO2 Amplitude Depth */
			channel[ch].cc2.lfo2_tva_depth = (float)val / 127.0f;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"CC2 LFO2 Amplitude Depth (CH:%d %.2f)", ch, channel[ch].cc2.lfo2_tva_depth); 
			break;
		case 0x42:	/* Note Limit Low */
			channel[ch].note_limit_low = val;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Note Limit Low (CH:%d VAL:%d)", ch, val); 
			break;
		case 0x43:	/* Note Limit High */
			channel[ch].note_limit_high = val;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Note Limit High (CH:%d VAL:%d)", ch, val); 
			break;
		case 0x44:	/* Velocity Limit Low */
			channel[ch].vel_limit_low = val;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Velocity Limit Low (CH:%d VAL:%d)", ch, val); 
			break;
		case 0x45:	/* Velocity Limit High */
			channel[ch].vel_limit_high = val;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Velocity Limit High (CH:%d VAL:%d)", ch, val); 
			break;
		case 0x46:	/* Rx. Note Off */
			if (channel[ch].drums[note] == NULL)
				play_midi_setup_drums(ch, note);
			set_rx_drum(channel[ch].drums[note], RX_NOTE_OFF, val);
			ctl_cmsg(CMSG_INFO, VERB_NOISY,
				"Drum Instrument Rx. Note Off (CH:%d NOTE:%d VAL:%d)",
				ch, note, val);
			break;
		case 0x47:	/* Rx. Note On */
			if (channel[ch].drums[note] == NULL)
				play_midi_setup_drums(ch, note);
			set_rx_drum(channel[ch].drums[note], RX_NOTE_ON, val);
			ctl_cmsg(CMSG_INFO, VERB_NOISY,
				"Drum Instrument Rx. Note On (CH:%d NOTE:%d VAL:%d)",
				ch, note, val);
			break;
		case 0x48:	/* Rx. Pitch Bend */
			set_rx(ch, RX_PITCH_BEND, val);
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Rx. Pitch Bend (CH:%d VAL:%d)", ch, val); 
			break;
		case 0x49:	/* Rx. Channel Pressure */
			set_rx(ch, RX_CH_PRESSURE, val);
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Rx. Channel Pressure (CH:%d VAL:%d)", ch, val); 
			break;
		case 0x4A:	/* Rx. Program Change */
			set_rx(ch, RX_PROGRAM_CHANGE, val);
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Rx. Program Change (CH:%d VAL:%d)", ch, val); 
			break;
		case 0x4B:	/* Rx. Control Change */
			set_rx(ch, RX_CONTROL_CHANGE, val);
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Rx. Control Change (CH:%d VAL:%d)", ch, val); 
			break;
		case 0x4C:	/* Rx. Poly Pressure */
			set_rx(ch, RX_POLY_PRESSURE, val);
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Rx. Poly Pressure (CH:%d VAL:%d)", ch, val); 
			break;
		case 0x4D:	/* Rx. Note Message */
			set_rx(ch, RX_NOTE_MESSAGE, val);
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Rx. Note Message (CH:%d VAL:%d)", ch, val); 
			break;
		case 0x4E:	/* Rx. RPN */
			set_rx(ch, RX_RPN, val);
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Rx. RPN (CH:%d VAL:%d)", ch, val); 
			break;
		case 0x4F:	/* Rx. NRPN */
			set_rx(ch, RX_NRPN, val);
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Rx. NRPN (CH:%d VAL:%d)", ch, val); 
			break;
		case 0x50:	/* Rx. Modulation */
			set_rx(ch, RX_MODULATION, val);
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Rx. Modulation (CH:%d VAL:%d)", ch, val); 
			break;
		case 0x51:	/* Rx. Volume */
			set_rx(ch, RX_VOLUME, val);
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Rx. Volume (CH:%d VAL:%d)", ch, val); 
			break;
		case 0x52:	/* Rx. Panpot */
			set_rx(ch, RX_PANPOT, val);
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Rx. Panpot (CH:%d VAL:%d)", ch, val); 
			break;
		case 0x53:	/* Rx. Expression */
			set_rx(ch, RX_EXPRESSION, val);
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Rx. Expression (CH:%d VAL:%d)", ch, val); 
			break;
		case 0x54:	/* Rx. Hold1 */
			set_rx(ch, RX_HOLD1, val);
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Rx. Hold1 (CH:%d VAL:%d)", ch, val); 
			break;
		case 0x55:	/* Rx. Portamento */
			set_rx(ch, RX_PORTAMENTO, val);
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Rx. Portamento (CH:%d VAL:%d)", ch, val); 
			break;
		case 0x56:	/* Rx. Sostenuto */
			set_rx(ch, RX_SOSTENUTO, val);
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Rx. Sostenuto (CH:%d VAL:%d)", ch, val); 
			break;
		case 0x57:	/* Rx. Soft */
			set_rx(ch, RX_SOFT, val);
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Rx. Soft (CH:%d VAL:%d)", ch, val); 
			break;
		case 0x58:	/* Rx. Bank Select */
			set_rx(ch, RX_BANK_SELECT, val);
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Rx. Bank Select (CH:%d VAL:%d)", ch, val); 
			break;
		case 0x59:	/* Rx. Bank Select LSB */
			set_rx(ch, RX_BANK_SELECT_LSB, val);
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Rx. Bank Select LSB (CH:%d VAL:%d)", ch, val); 
			break;
		case 0x60:	/* Reverb Type (GM2) */
			if (val > 8) {val = 8;}
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Reverb Type (%d)", val);
			reverb->set_reverb_macro_gm2(val);
			reverb->recompute_reverb_status_gs();
			reverb->init_reverb();
			break;
		case 0x61:	/* Chorus Type (GM2) */
			if (val > 5) {val = 5;}
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Chorus Type (%d)", val);
			reverb->set_chorus_macro_gs(val);
			reverb->recompute_chorus_status_gs();
			reverb->init_ch_chorus();
			break;
		default:
			break;
		}
		return;
	} else if(ev == ME_SYSEX_GS_LSB) {	/* GS system exclusive message */
		msb = channel[ch].sysex_gs_msb_addr;
		note = channel[ch].sysex_gs_msb_val;
		channel[ch].sysex_gs_msb_addr = channel[ch].sysex_gs_msb_val = 0;
		switch(b)
		{
		case 0x00:	/* EQ ON/OFF */
			if(!opt_eq_control) {break;}
			channel[ch].eq_gs = val;
			break;
		case 0x01:	/* EQ LOW FREQ */
			if(!opt_eq_control) {break;}
			reverb->eq_status_gs.low_freq = val;
			reverb->recompute_eq_status_gs();
			break;
		case 0x02:	/* EQ LOW GAIN */
			if(!opt_eq_control) {break;}
			reverb->eq_status_gs.low_gain = val;
			reverb->recompute_eq_status_gs();
			break;
		case 0x03:	/* EQ HIGH FREQ */
			if(!opt_eq_control) {break;}
			reverb->eq_status_gs.high_freq = val;
			reverb->recompute_eq_status_gs();
			break;
		case 0x04:	/* EQ HIGH GAIN */
			if(!opt_eq_control) {break;}
			reverb->eq_status_gs.high_gain = val;
			reverb->recompute_eq_status_gs();
			break;
		case 0x05:	/* Reverb Macro */
			if (val > 7) {val = 7;}
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Reverb Macro (%d)",val);
			reverb->set_reverb_macro_gs(val);
			reverb->recompute_reverb_status_gs();
			reverb->init_reverb();
			break;
		case 0x06:	/* Reverb Character */
			if (val > 7) {val = 7;}
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Reverb Character (%d)",val);
			if (reverb->reverb_status_gs.character != val) {
				reverb->reverb_status_gs.character = val;
				reverb->recompute_reverb_status_gs();
				reverb->init_reverb();
			}
			break;
		case 0x07:	/* Reverb Pre-LPF */
			if (val > 7) {val = 7;}
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Reverb Pre-LPF (%d)",val);
			if(reverb->reverb_status_gs.pre_lpf != val) {
				reverb->reverb_status_gs.pre_lpf = val;
				reverb->recompute_reverb_status_gs();
			}
			break;
		case 0x08:	/* Reverb Level */
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Reverb Level (%d)",val);
			if(reverb->reverb_status_gs.level != val) {
				reverb->reverb_status_gs.level = val;
				reverb->recompute_reverb_status_gs();
				reverb->init_reverb();
			}
			break;
		case 0x09:	/* Reverb Time */
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Reverb Time (%d)",val);
			if(reverb->reverb_status_gs.time != val) {
				reverb->reverb_status_gs.time = val;
				reverb->recompute_reverb_status_gs();
				reverb->init_reverb();
			}
			break;
		case 0x0A:	/* Reverb Delay Feedback */
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Reverb Delay Feedback (%d)",val);
			if(reverb->reverb_status_gs.delay_feedback != val) {
				reverb->reverb_status_gs.delay_feedback = val;
				reverb->recompute_reverb_status_gs();
				reverb->init_reverb();
			}
			break;
		case 0x0C:	/* Reverb Predelay Time */
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Reverb Predelay Time (%d)",val);
			if(reverb->reverb_status_gs.pre_delay_time != val) {
				reverb->reverb_status_gs.pre_delay_time = val;
				reverb->recompute_reverb_status_gs();
				reverb->init_reverb();
			}
			break;
		case 0x0D:	/* Chorus Macro */
			if (val > 7) {val = 7;}
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Chorus Macro (%d)",val);
			reverb->set_chorus_macro_gs(val);
			reverb->recompute_chorus_status_gs();
			reverb->init_ch_chorus();
			break;
		case 0x0E:	/* Chorus Pre-LPF */
			if (val > 7) {val = 7;}
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Chorus Pre-LPF (%d)",val);
			if (reverb->chorus_status_gs.pre_lpf != val) {
				reverb->chorus_status_gs.pre_lpf = val;
				reverb->recompute_chorus_status_gs();
			}
			break;
		case 0x0F:	/* Chorus Level */
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Chorus Level (%d)",val);
			if (reverb->chorus_status_gs.level != val) {
				reverb->chorus_status_gs.level = val;
				reverb->recompute_chorus_status_gs();
				reverb->init_ch_chorus();
			}
			break;
		case 0x10:	/* Chorus Feedback */
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Chorus Feedback (%d)",val);
			if (reverb->chorus_status_gs.feedback != val) {
				reverb->chorus_status_gs.feedback = val;
				reverb->recompute_chorus_status_gs();
				reverb->init_ch_chorus();
			}
			break;
		case 0x11:	/* Chorus Delay */
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Chorus Delay (%d)",val);
			if (reverb->chorus_status_gs.delay != val) {
				reverb->chorus_status_gs.delay = val;
				reverb->recompute_chorus_status_gs();
				reverb->init_ch_chorus();
			}
			break;
		case 0x12:	/* Chorus Rate */
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Chorus Rate (%d)",val);
			if (reverb->chorus_status_gs.rate != val) {
				reverb->chorus_status_gs.rate = val;
				reverb->recompute_chorus_status_gs();
				reverb->init_ch_chorus();
			}
			break;
		case 0x13:	/* Chorus Depth */
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Chorus Depth (%d)",val);
			if (reverb->chorus_status_gs.depth != val) {
				reverb->chorus_status_gs.depth = val;
				reverb->recompute_chorus_status_gs();
				reverb->init_ch_chorus();
			}
			break;
		case 0x14:	/* Chorus Send Level to Reverb */
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Chorus Send Level to Reverb (%d)",val);
			if (reverb->chorus_status_gs.send_reverb != val) {
				reverb->chorus_status_gs.send_reverb = val;
				reverb->recompute_chorus_status_gs();
				reverb->init_ch_chorus();
			}
			break;
		case 0x15:	/* Chorus Send Level to Delay */
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Chorus Send Level to Delay (%d)",val);
			if (reverb->chorus_status_gs.send_delay != val) {
				reverb->chorus_status_gs.send_delay = val;
				reverb->recompute_chorus_status_gs();
				reverb->init_ch_chorus();
			}
			break;
		case 0x16:	/* Delay Macro */
			if (val > 7) {val = 7;}
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Delay Macro (%d)",val);
			reverb->set_delay_macro_gs(val);
			reverb->recompute_delay_status_gs();
			reverb->init_ch_delay();
			break;
		case 0x17:	/* Delay Pre-LPF */
			if (val > 7) {val = 7;}
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Delay Pre-LPF (%d)",val);
			val &= 0x7;
			if (reverb->delay_status_gs.pre_lpf != val) {
				reverb->delay_status_gs.pre_lpf = val;
				reverb->recompute_delay_status_gs();
			}
			break;
		case 0x18:	/* Delay Time Center */
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Delay Time Center (%d)",val);
			if (reverb->delay_status_gs.time_c != val) {
				reverb->delay_status_gs.time_c = val;
				reverb->recompute_delay_status_gs();
				reverb->init_ch_delay();
			}
			break;
		case 0x19:	/* Delay Time Ratio Left */
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Delay Time Ratio Left (%d)",val);
			if (val == 0) {val = 1;}
			if (reverb->delay_status_gs.time_l != val) {
				reverb->delay_status_gs.time_l = val;
				reverb->recompute_delay_status_gs();
				reverb->init_ch_delay();
			}
			break;
		case 0x1A:	/* Delay Time Ratio Right */
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Delay Time Ratio Right (%d)",val);
			if (val == 0) {val = 1;}
			if (reverb->delay_status_gs.time_r != val) {
				reverb->delay_status_gs.time_r = val;
				reverb->recompute_delay_status_gs();
				reverb->init_ch_delay();
			}
			break;
		case 0x1B:	/* Delay Level Center */
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Delay Level Center (%d)",val);
			if (reverb->delay_status_gs.level_center != val) {
				reverb->delay_status_gs.level_center = val;
				reverb->recompute_delay_status_gs();
				reverb->init_ch_delay();
			}
			break;
		case 0x1C:	/* Delay Level Left */
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Delay Level Left (%d)",val);
			if (reverb->delay_status_gs.level_left != val) {
				reverb->delay_status_gs.level_left = val;
				reverb->recompute_delay_status_gs();
				reverb->init_ch_delay();
			}
			break;
		case 0x1D:	/* Delay Level Right */
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Delay Level Right (%d)",val);
			if (reverb->delay_status_gs.level_right != val) {
				reverb->delay_status_gs.level_right = val;
				reverb->recompute_delay_status_gs();
				reverb->init_ch_delay();
			}
			break;
		case 0x1E:	/* Delay Level */
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Delay Level (%d)",val);
			if (reverb->delay_status_gs.level != val) {
				reverb->delay_status_gs.level = val;
				reverb->recompute_delay_status_gs();
				reverb->init_ch_delay();
			}
			break;
		case 0x1F:	/* Delay Feedback */
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Delay Feedback (%d)",val);
			if (reverb->delay_status_gs.feedback != val) {
				reverb->delay_status_gs.feedback = val;
				reverb->recompute_delay_status_gs();
				reverb->init_ch_delay();
			}
			break;
		case 0x20:	/* Delay Send Level to Reverb */
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Delay Send Level to Reverb (%d)",val);
			if (reverb->delay_status_gs.send_reverb != val) {
				reverb->delay_status_gs.send_reverb = val;
				reverb->recompute_delay_status_gs();
				reverb->init_ch_delay();
			}
			break;
		case 0x21:	/* Velocity Sense Depth */
			channel[ch].velocity_sense_depth = val;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Velocity Sense Depth (CH:%d VAL:%d)",ch,val);
			break;
		case 0x22:	/* Velocity Sense Offset */
			channel[ch].velocity_sense_offset = val;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Velocity Sense Offset (CH:%d VAL:%d)",ch,val);
			break;
		case 0x23:	/* Insertion Effect ON/OFF */
			if(!opt_insertion_effect) {break;}
			if(channel[ch].insertion_effect != val) {
				//if(val) {//ctl_cmsg(CMSG_INFO,VERB_NOISY,"EFX ON (CH:%d)",ch);}
				//else {//ctl_cmsg(CMSG_INFO,VERB_NOISY,"EFX OFF (CH:%d)",ch);}
			}
			channel[ch].insertion_effect = val;
			break;
		case 0x24:	/* Assign Mode */
			channel[ch].assign_mode = val;
			if(val == 0) {
				//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Assign Mode: Single (CH:%d)",ch);
			} else if(val == 1) {
				//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Assign Mode: Limited-Multi (CH:%d)",ch);
			} else if(val == 2) {
				//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Assign Mode: Full-Multi (CH:%d)",ch);
			}
			break;
		case 0x25:	/* TONE MAP-0 NUMBER */
			channel[ch].tone_map0_number = val;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Tone Map-0 Number (CH:%d VAL:%d)",ch,val);
			break;
		case 0x26:	/* Pitch Offset Fine */
			channel[ch].pitch_offset_fine = (double)((((int32_t)val << 4) | (int32_t)val) - 0x80) / 10.0;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Pitch Offset Fine (CH:%d %3fHz)",ch,channel[ch].pitch_offset_fine);
			break;
		case 0x27:	/* Insertion Effect Parameter */
			if(!opt_insertion_effect) {break;}
			temp = reverb->insertion_effect_gs.type;
			reverb->insertion_effect_gs.type_msb = val;
			reverb->insertion_effect_gs.type = ((int32_t)reverb->insertion_effect_gs.type_msb << 8) | (int32_t)reverb->insertion_effect_gs.type_lsb;
			if(temp == reverb->insertion_effect_gs.type) {
				reverb->recompute_insertion_effect_gs();
			} else {
				reverb->realloc_insertion_effect_gs();
			}
			break;
		case 0x28:	/* Insertion Effect Parameter */
			if(!opt_insertion_effect) {break;}
			temp = reverb->insertion_effect_gs.type;
			reverb->insertion_effect_gs.type_lsb = val;
			reverb->insertion_effect_gs.type = ((int32_t)reverb->insertion_effect_gs.type_msb << 8) | (int32_t)reverb->insertion_effect_gs.type_lsb;
			if(temp == reverb->insertion_effect_gs.type) {
				reverb->recompute_insertion_effect_gs();
			} else {
				reverb->realloc_insertion_effect_gs();
			}
			break;
		case 0x29:
			reverb->insertion_effect_gs.parameter[0] = val;
			reverb->recompute_insertion_effect_gs();
			break;
		case 0x2A:
			reverb->insertion_effect_gs.parameter[1] = val;
			reverb->recompute_insertion_effect_gs();
			break;
		case 0x2B:
			reverb->insertion_effect_gs.parameter[2] = val;
			reverb->recompute_insertion_effect_gs();
			break;
		case 0x2C:
			reverb->insertion_effect_gs.parameter[3] = val;
			reverb->recompute_insertion_effect_gs();
			break;
		case 0x2D:
			reverb->insertion_effect_gs.parameter[4] = val;
			reverb->recompute_insertion_effect_gs();
			break;
		case 0x2E:
			reverb->insertion_effect_gs.parameter[5] = val;
			reverb->recompute_insertion_effect_gs();
			break;
		case 0x2F:
			reverb->insertion_effect_gs.parameter[6] = val;
			reverb->recompute_insertion_effect_gs();
			break;
		case 0x30:
			reverb->insertion_effect_gs.parameter[7] = val;
			reverb->recompute_insertion_effect_gs();
			break;
		case 0x31:
			reverb->insertion_effect_gs.parameter[8] = val;
			reverb->recompute_insertion_effect_gs();
			break;
		case 0x32:
			reverb->insertion_effect_gs.parameter[9] = val;
			reverb->recompute_insertion_effect_gs();
			break;
		case 0x33:
			reverb->insertion_effect_gs.parameter[10] = val;
			reverb->recompute_insertion_effect_gs();
			break;
		case 0x34:
			reverb->insertion_effect_gs.parameter[11] = val;
			reverb->recompute_insertion_effect_gs();
			break;
		case 0x35:
			reverb->insertion_effect_gs.parameter[12] = val;
			reverb->recompute_insertion_effect_gs();
			break;
		case 0x36:
			reverb->insertion_effect_gs.parameter[13] = val;
			reverb->recompute_insertion_effect_gs();
			break;
		case 0x37:
			reverb->insertion_effect_gs.parameter[14] = val;
			reverb->recompute_insertion_effect_gs();
			break;
		case 0x38:
			reverb->insertion_effect_gs.parameter[15] = val;
			reverb->recompute_insertion_effect_gs();
			break;
		case 0x39:
			reverb->insertion_effect_gs.parameter[16] = val;
			reverb->recompute_insertion_effect_gs();
			break;
		case 0x3A:
			reverb->insertion_effect_gs.parameter[17] = val;
			reverb->recompute_insertion_effect_gs();
			break;
		case 0x3B:
			reverb->insertion_effect_gs.parameter[18] = val;
			reverb->recompute_insertion_effect_gs();
			break;
		case 0x3C:
			reverb->insertion_effect_gs.parameter[19] = val;
			reverb->recompute_insertion_effect_gs();
			break;
		case 0x3D:
			reverb->insertion_effect_gs.send_reverb = val;
			reverb->recompute_insertion_effect_gs();
			break;
		case 0x3E:
			reverb->insertion_effect_gs.send_chorus = val;
			reverb->recompute_insertion_effect_gs();
			break;
		case 0x3F:
			reverb->insertion_effect_gs.send_delay = val;
			reverb->recompute_insertion_effect_gs();
			break;
		case 0x40:
			reverb->insertion_effect_gs.control_source1 = val;
			reverb->recompute_insertion_effect_gs();
			break;
		case 0x41:
			reverb->insertion_effect_gs.control_depth1 = val;
			reverb->recompute_insertion_effect_gs();
			break;
		case 0x42:
			reverb->insertion_effect_gs.control_source2 = val;
			reverb->recompute_insertion_effect_gs();
			break;
		case 0x43:
			reverb->insertion_effect_gs.control_depth2 = val;
			reverb->recompute_insertion_effect_gs();
			break;
		case 0x44:
			reverb->insertion_effect_gs.send_eq_switch = val;
			reverb->recompute_insertion_effect_gs();
			break;
		case 0x45:	/* Rx. Channel */
			reset_controllers(ch);
			all_notes_off(ch);
			if (val == 0x80)
				remove_channel_layer(ch);
			else
				add_channel_layer(ch, val);
			break;
		case 0x46:	/* Channel Msg Rx Port */
			reset_controllers(ch);
			all_notes_off(ch);
			channel[ch].port_select = val;
			break;
		case 0x47:	/* Play Note Number */
			if (channel[ch].drums[note] == NULL)
				play_midi_setup_drums(ch, note);
			channel[ch].drums[note]->play_note = val;
			ctl_cmsg(CMSG_INFO, VERB_NOISY,
				"Drum Instrument Play Note (CH:%d NOTE:%d VAL:%d)",
				ch, note, channel[ch].drums[note]->play_note);
			channel[ch].pitchfactor = 0;
			break;
		default:
			break;
		}
		return;
	} else if(ev == ME_SYSEX_XG_LSB) {	/* XG system exclusive message */
		msb = channel[ch].sysex_xg_msb_addr;
		note = channel[ch].sysex_xg_msb_val;
		if (note == 3 && msb == 0) {	/* Effect 2 */
		note = 0;	/* force insertion effect num 0 ?? */
		if (note >= XG_INSERTION_EFFECT_NUM || note < 0) {return;}
		switch(b)
		{
		case 0x00:	/* Insertion Effect Type MSB */
			if (reverb->insertion_effect_xg[note].type_msb != val) {
				//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Insertion Effect Type MSB (%d %02X)", note, val);
				reverb->insertion_effect_xg[note].type_msb = val;
				reverb->realloc_effect_xg(&reverb->insertion_effect_xg[note]);
			}
			break;
		case 0x01:	/* Insertion Effect Type LSB */
			if (reverb->insertion_effect_xg[note].type_lsb != val) {
				//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Insertion Effect Type LSB (%d %02X)", note, val);
				reverb->insertion_effect_xg[note].type_lsb = val;
				reverb->realloc_effect_xg(&reverb->insertion_effect_xg[note]);
			}
			break;
		case 0x02:	/* Insertion Effect Parameter 1 - 10 */
		case 0x03:
		case 0x04:
		case 0x05:
		case 0x06:
		case 0x07:
		case 0x08:
		case 0x09:
		case 0x0A:
		case 0x0B:
			if (reverb->insertion_effect_xg[note].use_msb) {break;}
			temp = b - 0x02;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Insertion Effect Parameter %d (%d %d)", temp + 1, note, val);
			if (reverb->insertion_effect_xg[note].param_lsb[temp] != val) {
				reverb->insertion_effect_xg[note].param_lsb[temp] = val;
				reverb->recompute_effect_xg(&reverb->insertion_effect_xg[note]);
			}
			break;
		case 0x0C:	/* Insertion Effect Part */
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Insertion Effect Part (%d %d)", note, val);
			if (reverb->insertion_effect_xg[note].part != val) {
				reverb->insertion_effect_xg[note].part = val;
				reverb->recompute_effect_xg(&reverb->insertion_effect_xg[note]);
			}
			break;
		case 0x0D:	/* MW Insertion Control Depth */
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"MW Insertion Control Depth (%d %d)", note, val);
			if (reverb->insertion_effect_xg[note].mw_depth != val) {
				reverb->insertion_effect_xg[note].mw_depth = val;
				reverb->recompute_effect_xg(&reverb->insertion_effect_xg[note]);
			}
			break;
		case 0x0E:	/* BEND Insertion Control Depth */
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"BEND Insertion Control Depth (%d %d)", note, val);
			if (reverb->insertion_effect_xg[note].bend_depth != val) {
				reverb->insertion_effect_xg[note].bend_depth = val;
				reverb->recompute_effect_xg(&reverb->insertion_effect_xg[note]);
			}
			break;
		case 0x0F:	/* CAT Insertion Control Depth */
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"CAT Insertion Control Depth (%d %d)", note, val);
			if (reverb->insertion_effect_xg[note].cat_depth != val) {
				reverb->insertion_effect_xg[note].cat_depth = val;
				reverb->recompute_effect_xg(&reverb->insertion_effect_xg[note]);
			}
			break;
		case 0x10:	/* AC1 Insertion Control Depth */
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"AC1 Insertion Control Depth (%d %d)", note, val);
			if (reverb->insertion_effect_xg[note].ac1_depth != val) {
				reverb->insertion_effect_xg[note].ac1_depth = val;
				reverb->recompute_effect_xg(&reverb->insertion_effect_xg[note]);
			}
			break;
		case 0x11:	/* AC2 Insertion Control Depth */
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"AC2 Insertion Control Depth (%d %d)", note, val);
			if (reverb->insertion_effect_xg[note].ac2_depth != val) {
				reverb->insertion_effect_xg[note].ac2_depth = val;
				reverb->recompute_effect_xg(&reverb->insertion_effect_xg[note]);
			}
			break;
		case 0x12:	/* CBC1 Insertion Control Depth */
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"CBC1 Insertion Control Depth (%d %d)", note, val);
			if (reverb->insertion_effect_xg[note].cbc1_depth != val) {
				reverb->insertion_effect_xg[note].cbc1_depth = val;
				reverb->recompute_effect_xg(&reverb->insertion_effect_xg[note]);
			}
			break;
		case 0x13:	/* CBC2 Insertion Control Depth */
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"CBC2 Insertion Control Depth (%d %d)", note, val);
			if (reverb->insertion_effect_xg[note].cbc2_depth != val) {
				reverb->insertion_effect_xg[note].cbc2_depth = val;
				reverb->recompute_effect_xg(&reverb->insertion_effect_xg[note]);
			}
			break;
		case 0x20:	/* Insertion Effect Parameter 11 - 16 */
		case 0x21:
		case 0x22:
		case 0x23:
		case 0x24:
		case 0x25:
			temp = b - 0x20 + 10;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Insertion Effect Parameter %d (%d %d)", temp + 1, note, val);
			if (reverb->insertion_effect_xg[note].param_lsb[temp] != val) {
				reverb->insertion_effect_xg[note].param_lsb[temp] = val;
				reverb->recompute_effect_xg(&reverb->insertion_effect_xg[note]);
			}
			break;
		case 0x30:	/* Insertion Effect Parameter 1 - 10 MSB */
		case 0x32:
		case 0x34:
		case 0x36:
		case 0x38:
		case 0x3A:
		case 0x3C:
		case 0x3E:
		case 0x40:
		case 0x42:
			if (!reverb->insertion_effect_xg[note].use_msb) {break;}
			temp = (b - 0x30) / 2;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Insertion Effect Parameter %d MSB (%d %d)", temp + 1, note, val);
			if (reverb->insertion_effect_xg[note].param_msb[temp] != val) {
				reverb->insertion_effect_xg[note].param_msb[temp] = val;
				reverb->recompute_effect_xg(&reverb->insertion_effect_xg[note]);
			}
			break;
		case 0x31:	/* Insertion Effect Parameter 1 - 10 LSB */
		case 0x33:
		case 0x35:
		case 0x37:
		case 0x39:
		case 0x3B:
		case 0x3D:
		case 0x3F:
		case 0x41:
		case 0x43:
			if (!reverb->insertion_effect_xg[note].use_msb) {break;}
			temp = (b - 0x31) / 2;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Insertion Effect Parameter %d LSB (%d %d)", temp + 1, note, val);
			if (reverb->insertion_effect_xg[note].param_lsb[temp] != val) {
				reverb->insertion_effect_xg[note].param_lsb[temp] = val;
				reverb->recompute_effect_xg(&reverb->insertion_effect_xg[note]);
			}
			break;
		default:
			break;
		}
		} else if (note == 2 && msb == 1) {	/* Effect 1 */
		note = 0;	/* force variation effect num 0 ?? */
		switch(b)
		{
		case 0x00:	/* Reverb Type MSB */
			if (reverb->reverb_status_xg.type_msb != val) {
				//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Reverb Type MSB (%02X)", val);
				reverb->reverb_status_xg.type_msb = val;
				reverb->realloc_effect_xg(&reverb->reverb_status_xg);
			}
			break;
		case 0x01:	/* Reverb Type LSB */
			if (reverb->reverb_status_xg.type_lsb != val) {
				//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Reverb Type LSB (%02X)", val);
				reverb->reverb_status_xg.type_lsb = val;
				reverb->realloc_effect_xg(&reverb->reverb_status_xg);
			}
			break;
		case 0x02:	/* Reverb Parameter 1 - 10 */
		case 0x03:
		case 0x04:
		case 0x05:
		case 0x06:
		case 0x07:
		case 0x08:
		case 0x09:
		case 0x0A:
		case 0x0B:
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Reverb Parameter %d (%d)", b - 0x02 + 1, val);
			if (reverb->reverb_status_xg.param_lsb[b - 0x02] != val) {
				reverb->reverb_status_xg.param_lsb[b - 0x02] = val;
				reverb->recompute_effect_xg(&reverb->reverb_status_xg);
			}
			break;
		case 0x0C:	/* Reverb Return */
#if 0	/* XG specific reverb is not currently implemented */
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Reverb Return (%d)", val);
			if (reverb->reverb_status_xg.ret != val) {
				reverb->reverb_status_xg.ret = val;
				reverb->recompute_effect_xg(&reverb->reverb_status_xg);
			}
#else	/* use GS reverb instead */
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Reverb Return (%d)", val);
			if (reverb->reverb_status_gs.level != val) {
				reverb->reverb_status_gs.level = val;
				reverb->recompute_reverb_status_gs();
				reverb->init_reverb();
			}
#endif
			break;
		case 0x0D:	/* Reverb Pan */
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Reverb Pan (%d)", val);
			if (reverb->reverb_status_xg.pan != val) {
				reverb->reverb_status_xg.pan = val;
				reverb->recompute_effect_xg(&reverb->reverb_status_xg);
			}
			break;
		case 0x10:	/* Reverb Parameter 11 - 16 */
		case 0x11:
		case 0x12:
		case 0x13:
		case 0x14:
		case 0x15:
			temp = b - 0x10 + 10;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Reverb Parameter %d (%d)", temp + 1, val);
			if (reverb->reverb_status_xg.param_lsb[temp] != val) {
				reverb->reverb_status_xg.param_lsb[temp] = val;
				reverb->recompute_effect_xg(&reverb->reverb_status_xg);
			}
			break;
		case 0x20:	/* Chorus Type MSB */
			if (reverb->chorus_status_xg.type_msb != val) {
				//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Chorus Type MSB (%02X)", val);
				reverb->chorus_status_xg.type_msb = val;
				reverb->realloc_effect_xg(&reverb->chorus_status_xg);
			}
			break;
		case 0x21:	/* Chorus Type LSB */
			if (reverb->chorus_status_xg.type_lsb != val) {
				//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Chorus Type LSB (%02X)", val);
				reverb->chorus_status_xg.type_lsb = val;
				reverb->realloc_effect_xg(&reverb->chorus_status_xg);
			}
			break;
		case 0x22:	/* Chorus Parameter 1 - 10 */
		case 0x23:
		case 0x24:
		case 0x25:
		case 0x26:
		case 0x27:
		case 0x28:
		case 0x29:
		case 0x2A:
		case 0x2B:
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Chorus Parameter %d (%d)", b - 0x22 + 1, val);
			if (reverb->chorus_status_xg.param_lsb[b - 0x22] != val) {
				reverb->chorus_status_xg.param_lsb[b - 0x22] = val;
				reverb->recompute_effect_xg(&reverb->chorus_status_xg);
			}
			break;
		case 0x2C:	/* Chorus Return */
#if 0	/* XG specific chorus is not currently implemented */
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Chorus Return (%d)", val);
			if (reverb->chorus_status_xg.ret != val) {
				reverb->chorus_status_xg.ret = val;
				reverb->recompute_effect_xg(&reverb->chorus_status_xg);
			}
#else	/* use GS chorus instead */
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Chorus Return (%d)", val);
			if (reverb->chorus_status_gs.level != val) {
				reverb->chorus_status_gs.level = val;
				reverb->recompute_chorus_status_gs();
				reverb->init_ch_chorus();
			}
#endif
			break;
		case 0x2D:	/* Chorus Pan */
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Chorus Pan (%d)", val);
			if (reverb->chorus_status_xg.pan != val) {
				reverb->chorus_status_xg.pan = val;
				reverb->recompute_effect_xg(&reverb->chorus_status_xg);
			}
			break;
		case 0x2E:	/* Send Chorus To Reverb */
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Send Chorus To Reverb (%d)", val);
			if (reverb->chorus_status_xg.send_reverb != val) {
				reverb->chorus_status_xg.send_reverb = val;
				reverb->recompute_effect_xg(&reverb->chorus_status_xg);
			}
			break;
		case 0x30:	/* Chorus Parameter 11 - 16 */
		case 0x31:
		case 0x32:
		case 0x33:
		case 0x34:
		case 0x35:
			temp = b - 0x30 + 10;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Chorus Parameter %d (%d)", temp + 1, val);
			if (reverb->chorus_status_xg.param_lsb[temp] != val) {
				reverb->chorus_status_xg.param_lsb[temp] = val;
				reverb->recompute_effect_xg(&reverb->chorus_status_xg);
			}
			break;
		case 0x40:	/* Variation Type MSB */
			if (note >= XG_VARIATION_EFFECT_NUM || note < 0) {break;}
			if (reverb->variation_effect_xg[note].type_msb != val) {
				//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Variation Type MSB (%02X)", val);
				reverb->variation_effect_xg[note].type_msb = val;
				reverb->realloc_effect_xg(&reverb->variation_effect_xg[note]);
			}
			break;
		case 0x41:	/* Variation Type LSB */
			if (note >= XG_VARIATION_EFFECT_NUM || note < 0) {break;}
			if (reverb->variation_effect_xg[note].type_lsb != val) {
				//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Variation Type LSB (%02X)", val);
				reverb->variation_effect_xg[note].type_lsb = val;
				reverb->realloc_effect_xg(&reverb->variation_effect_xg[note]);
			}
			break;
		case 0x42:	/* Variation Parameter 1 - 10 MSB */
		case 0x44:
		case 0x46:
		case 0x48:
		case 0x4A:
		case 0x4C:
		case 0x4E:
		case 0x50:
		case 0x52:
		case 0x54:
			if (note >= XG_VARIATION_EFFECT_NUM || note < 0) {break;}
			temp = (b - 0x42) / 2;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Variation Parameter %d MSB (%d)", temp, val);
			if (reverb->variation_effect_xg[note].param_msb[temp] != val) {
				reverb->variation_effect_xg[note].param_msb[temp] = val;
				reverb->recompute_effect_xg(&reverb->variation_effect_xg[note]);
			}
			break;
		case 0x43:	/* Variation Parameter 1 - 10 LSB */
		case 0x45:
		case 0x47:
		case 0x49:
		case 0x4B:
		case 0x4D:
		case 0x4F:
		case 0x51:
		case 0x53:
		case 0x55:
			if (note >= XG_VARIATION_EFFECT_NUM || note < 0) {break;}
			temp = (b - 0x43) / 2;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Variation Parameter %d LSB (%d)", temp, val);
			if (reverb->variation_effect_xg[note].param_lsb[temp] != val) {
				reverb->variation_effect_xg[note].param_lsb[temp] = val;
				reverb->recompute_effect_xg(&reverb->variation_effect_xg[note]);
			}
			break;
		case 0x56:	/* Variation Return */
			if (note >= XG_VARIATION_EFFECT_NUM || note < 0) {break;}
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Variation Return (%d)", val);
			if (reverb->variation_effect_xg[note].ret != val) {
				reverb->variation_effect_xg[note].ret = val;
				reverb->recompute_effect_xg(&reverb->variation_effect_xg[note]);
			}
			break;
		case 0x57:	/* Variation Pan */
			if (note >= XG_VARIATION_EFFECT_NUM || note < 0) {break;}
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Variation Pan (%d)", val);
			if (reverb->variation_effect_xg[note].pan != val) {
				reverb->variation_effect_xg[note].pan = val;
				reverb->recompute_effect_xg(&reverb->variation_effect_xg[note]);
			}
			break;
		case 0x58:	/* Send Variation To Reverb */
			if (note >= XG_VARIATION_EFFECT_NUM || note < 0) {break;}
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Send Variation To Reverb (%d)", val);
			if (reverb->variation_effect_xg[note].send_reverb != val) {
				reverb->variation_effect_xg[note].send_reverb = val;
				reverb->recompute_effect_xg(&reverb->variation_effect_xg[note]);
			}
			break;
		case 0x59:	/* Send Variation To Chorus */
			if (note >= XG_VARIATION_EFFECT_NUM || note < 0) {break;}
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Send Variation To Chorus (%d)", val);
			if (reverb->variation_effect_xg[note].send_chorus != val) {
				reverb->variation_effect_xg[note].send_chorus = val;
				reverb->recompute_effect_xg(&reverb->variation_effect_xg[note]);
			}
			break;
		case 0x5A:	/* Variation Connection */
			if (note >= XG_VARIATION_EFFECT_NUM || note < 0) {break;}
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Variation Connection (%d)", val);
			if (reverb->variation_effect_xg[note].connection != val) {
				reverb->variation_effect_xg[note].connection = val;
				reverb->recompute_effect_xg(&reverb->variation_effect_xg[note]);
			}
			break;
		case 0x5B:	/* Variation Part */
			if (note >= XG_VARIATION_EFFECT_NUM || note < 0) {break;}
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Variation Part (%d)", val);
			if (reverb->variation_effect_xg[note].part != val) {
				reverb->variation_effect_xg[note].part = val;
				reverb->recompute_effect_xg(&reverb->variation_effect_xg[note]);
			}
			break;
		case 0x5C:	/* MW Variation Control Depth */
			if (note >= XG_VARIATION_EFFECT_NUM || note < 0) {break;}
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"MW Variation Control Depth (%d)", val);
			if (reverb->variation_effect_xg[note].mw_depth != val) {
				reverb->variation_effect_xg[note].mw_depth = val;
				reverb->recompute_effect_xg(&reverb->variation_effect_xg[note]);
			}
			break;
		case 0x5D:	/* BEND Variation Control Depth */
			if (note >= XG_VARIATION_EFFECT_NUM || note < 0) {break;}
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"BEND Variation Control Depth (%d)", val);
			if (reverb->variation_effect_xg[note].bend_depth != val) {
				reverb->variation_effect_xg[note].bend_depth = val;
				reverb->recompute_effect_xg(&reverb->variation_effect_xg[note]);
			}
			break;
		case 0x5E:	/* CAT Variation Control Depth */
			if (note >= XG_VARIATION_EFFECT_NUM || note < 0) {break;}
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"CAT Variation Control Depth (%d)", val);
			if (reverb->variation_effect_xg[note].cat_depth != val) {
				reverb->variation_effect_xg[note].cat_depth = val;
				reverb->recompute_effect_xg(&reverb->variation_effect_xg[note]);
			}
			break;
		case 0x5F:	/* AC1 Variation Control Depth */
			if (note >= XG_VARIATION_EFFECT_NUM || note < 0) {break;}
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"AC1 Variation Control Depth (%d)", val);
			if (reverb->variation_effect_xg[note].ac1_depth != val) {
				reverb->variation_effect_xg[note].ac1_depth = val;
				reverb->recompute_effect_xg(&reverb->variation_effect_xg[note]);
			}
			break;
		case 0x60:	/* AC2 Variation Control Depth */
			if (note >= XG_VARIATION_EFFECT_NUM || note < 0) {break;}
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"AC2 Variation Control Depth (%d)", val);
			if (reverb->variation_effect_xg[note].ac2_depth != val) {
				reverb->variation_effect_xg[note].ac2_depth = val;
				reverb->recompute_effect_xg(&reverb->variation_effect_xg[note]);
			}
			break;
		case 0x61:	/* CBC1 Variation Control Depth */
			if (note >= XG_VARIATION_EFFECT_NUM || note < 0) {break;}
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"CBC1 Variation Control Depth (%d)", val);
			if (reverb->variation_effect_xg[note].cbc1_depth != val) {
				reverb->variation_effect_xg[note].cbc1_depth = val;
				reverb->recompute_effect_xg(&reverb->variation_effect_xg[note]);
			}
			break;
		case 0x62:	/* CBC2 Variation Control Depth */
			if (note >= XG_VARIATION_EFFECT_NUM || note < 0) {break;}
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"CBC2 Variation Control Depth (%d)", val);
			if (reverb->variation_effect_xg[note].cbc2_depth != val) {
				reverb->variation_effect_xg[note].cbc2_depth = val;
				reverb->recompute_effect_xg(&reverb->variation_effect_xg[note]);
			}
			break;
		case 0x70:	/* Variation Parameter 11 - 16 */
		case 0x71:
		case 0x72:
		case 0x73:
		case 0x74:
		case 0x75:
			temp = b - 0x70 + 10;
			if (note >= XG_VARIATION_EFFECT_NUM || note < 0) {break;}
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Variation Parameter %d (%d)", temp + 1, val);
			if (reverb->variation_effect_xg[note].param_lsb[temp] != val) {
				reverb->variation_effect_xg[note].param_lsb[temp] = val;
				reverb->recompute_effect_xg(&reverb->variation_effect_xg[note]);
			}
			break;
		default:
			break;
		}
		} else if (note == 2 && msb == 40) {	/* Multi EQ */
		switch(b)
		{
		case 0x00:	/* EQ type */
			if(opt_eq_control) {
				//ctl_cmsg(CMSG_INFO,VERB_NOISY,"EQ type (%d)", val);
				reverb->multi_eq_xg.type = val;
				reverb->set_multi_eq_type_xg(val);
				reverb->recompute_multi_eq_xg();
			}
			break;
		case 0x01:	/* EQ gain1 */
			if(opt_eq_control) {
				if(val > 0x4C) {val = 0x4C;}
				else if(val < 0x34) {val = 0x34;}
				//ctl_cmsg(CMSG_INFO,VERB_NOISY,"EQ gain1 (%d dB)", val - 0x40);
				reverb->multi_eq_xg.gain1 = val;
				reverb->recompute_multi_eq_xg();
			}
			break;
		case 0x02:	/* EQ frequency1 */
			if(opt_eq_control) {
				if(val > 60) {val = 60;}
				//ctl_cmsg(CMSG_INFO,VERB_NOISY,"EQ frequency1 (%d Hz)", (int32_t)eq_freq_table_xg[val]);
				reverb->multi_eq_xg.freq1 = val;
				reverb->recompute_multi_eq_xg();
			}
			break;
		case 0x03:	/* EQ Q1 */
			if(opt_eq_control) {
				//ctl_cmsg(CMSG_INFO,VERB_NOISY,"EQ Q1 (%f)", (double)val / 10.0);
				reverb->multi_eq_xg.q1 = val;
				reverb->recompute_multi_eq_xg();
			}
			break;
		case 0x04:	/* EQ shape1 */
			if(opt_eq_control) {
				//ctl_cmsg(CMSG_INFO,VERB_NOISY,"EQ shape1 (%d)", val);
				reverb->multi_eq_xg.shape1 = val;
				reverb->recompute_multi_eq_xg();
			}
			break;
		case 0x05:	/* EQ gain2 */
			if(opt_eq_control) {
				if(val > 0x4C) {val = 0x4C;}
				else if(val < 0x34) {val = 0x34;}
				//ctl_cmsg(CMSG_INFO,VERB_NOISY,"EQ gain2 (%d dB)", val - 0x40);
				reverb->multi_eq_xg.gain2 = val;
				reverb->recompute_multi_eq_xg();
			}
			break;
		case 0x06:	/* EQ frequency2 */
			if(opt_eq_control) {
				if(val > 60) {val = 60;}
				//ctl_cmsg(CMSG_INFO,VERB_NOISY,"EQ frequency2 (%d Hz)", (int32_t)eq_freq_table_xg[val]);
				reverb->multi_eq_xg.freq2 = val;
				reverb->recompute_multi_eq_xg();
			}
			break;
		case 0x07:	/* EQ Q2 */
			if(opt_eq_control) {
				//ctl_cmsg(CMSG_INFO,VERB_NOISY,"EQ Q2 (%f)", (double)val / 10.0);
				reverb->multi_eq_xg.q2 = val;
				reverb->recompute_multi_eq_xg();
			}
			break;
		case 0x09:	/* EQ gain3 */
			if(opt_eq_control) {
				if(val > 0x4C) {val = 0x4C;}
				else if(val < 0x34) {val = 0x34;}
				//ctl_cmsg(CMSG_INFO,VERB_NOISY,"EQ gain3 (%d dB)", val - 0x40);
				reverb->multi_eq_xg.gain3 = val;
				reverb->recompute_multi_eq_xg();
			}
			break;
		case 0x0A:	/* EQ frequency3 */
			if(opt_eq_control) {
				if(val > 60) {val = 60;}
				//ctl_cmsg(CMSG_INFO,VERB_NOISY,"EQ frequency3 (%d Hz)", (int32_t)eq_freq_table_xg[val]);
				reverb->multi_eq_xg.freq3 = val;
				reverb->recompute_multi_eq_xg();
			}
			break;
		case 0x0B:	/* EQ Q3 */
			if(opt_eq_control) {
				//ctl_cmsg(CMSG_INFO,VERB_NOISY,"EQ Q3 (%f)", (double)val / 10.0);
				reverb->multi_eq_xg.q3 = val;
				reverb->recompute_multi_eq_xg();
			}
			break;
		case 0x0D:	/* EQ gain4 */
			if(opt_eq_control) {
				if(val > 0x4C) {val = 0x4C;}
				else if(val < 0x34) {val = 0x34;}
				//ctl_cmsg(CMSG_INFO,VERB_NOISY,"EQ gain4 (%d dB)", val - 0x40);
				reverb->multi_eq_xg.gain4 = val;
				reverb->recompute_multi_eq_xg();
			}
			break;
		case 0x0E:	/* EQ frequency4 */
			if(opt_eq_control) {
				if(val > 60) {val = 60;}
				//ctl_cmsg(CMSG_INFO,VERB_NOISY,"EQ frequency4 (%d Hz)", (int32_t)eq_freq_table_xg[val]);
				reverb->multi_eq_xg.freq4 = val;
				reverb->recompute_multi_eq_xg();
			}
			break;
		case 0x0F:	/* EQ Q4 */
			if(opt_eq_control) {
				//ctl_cmsg(CMSG_INFO,VERB_NOISY,"EQ Q4 (%f)", (double)val / 10.0);
				reverb->multi_eq_xg.q4 = val;
				reverb->recompute_multi_eq_xg();
			}
			break;
		case 0x11:	/* EQ gain5 */
			if(opt_eq_control) {
				if(val > 0x4C) {val = 0x4C;}
				else if(val < 0x34) {val = 0x34;}
				//ctl_cmsg(CMSG_INFO,VERB_NOISY,"EQ gain5 (%d dB)", val - 0x40);
				reverb->multi_eq_xg.gain5 = val;
				reverb->recompute_multi_eq_xg();
			}
			break;
		case 0x12:	/* EQ frequency5 */
			if(opt_eq_control) {
				if(val > 60) {val = 60;}
				//ctl_cmsg(CMSG_INFO,VERB_NOISY,"EQ frequency5 (%d Hz)", (int32_t)eq_freq_table_xg[val]);
				reverb->multi_eq_xg.freq5 = val;
				reverb->recompute_multi_eq_xg();
			}
			break;
		case 0x13:	/* EQ Q5 */
			if(opt_eq_control) {
				//ctl_cmsg(CMSG_INFO,VERB_NOISY,"EQ Q5 (%f)", (double)val / 10.0);
				reverb->multi_eq_xg.q5 = val;
				reverb->recompute_multi_eq_xg();
			}
			break;
		case 0x14:	/* EQ shape5 */
			if(opt_eq_control) {
				//ctl_cmsg(CMSG_INFO,VERB_NOISY,"EQ shape5 (%d)", val);
				reverb->multi_eq_xg.shape5 = val;
				reverb->recompute_multi_eq_xg();
			}
			break;
		}
		} else if (note == 8 && msb == 0) {	/* Multi Part */
		switch(b)
		{
		case 0x99:	/* Rcv CHANNEL, remapped from 0x04 */
			reset_controllers(ch);
			all_notes_off(ch);
			if (val == 0x7f)
				remove_channel_layer(ch);
			else {
				if((ch < REDUCE_CHANNELS) != (val < REDUCE_CHANNELS)) {
					channel[ch].port_select = ch < REDUCE_CHANNELS ? 1 : 0;
				}
				if((ch % REDUCE_CHANNELS) != (val % REDUCE_CHANNELS)) {
					add_channel_layer(ch, val);
				}
			}
			break;
		case 0x06:	/* Same Note Number Key On Assign */
			if(val == 0) {
				channel[ch].assign_mode = 0;
				//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Same Note Number Key On Assign: Single (CH:%d)",ch);
			} else if(val == 1) {
				channel[ch].assign_mode = 2;
				//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Same Note Number Key On Assign: Multi (CH:%d)",ch);
			} else if(val == 2) {
				//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Same Note Number Key On Assign: Inst is not supported. (CH:%d)",ch);
			}
			break;
		case 0x11:	/* Dry Level */
			channel[ch].dry_level = val;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Dry Level (CH:%d VAL:%d)", ch, val);
			break;
		}
		} else if ((note & 0xF0) == 0x30) {	/* Drum Setup */
		note = msb;
		switch(b)
		{
		case 0x0E:	/* EG Decay1 */
			if (channel[ch].drums[note] == NULL)
				play_midi_setup_drums(ch, note);
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Drum Instrument EG Decay1 (CH:%d NOTE:%d VAL:%d)", ch, note, val);
			channel[ch].drums[note]->drum_envelope_rate[EG_DECAY1] = val;
			break;
		case 0x0F:	/* EG Decay2 */
			if (channel[ch].drums[note] == NULL)
				play_midi_setup_drums(ch, note);
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Drum Instrument EG Decay2 (CH:%d NOTE:%d VAL:%d)", ch, note, val);
			channel[ch].drums[note]->drum_envelope_rate[EG_DECAY2] = val;
			break;
		default:
			break;
		}
		}
		return;
	}
}

/*! convert GS NRPN to vibrato rate ratio. */
/* from 0 to 3.0. */
double Player::gs_cnv_vib_rate(int rate)
{
	double ratio;

	if(rate == 0) {
		ratio = 1.6 / 100.0;
	} else if(rate == 64) {
		ratio = 1.0;
	} else if(rate <= 100) {
		ratio = (double)rate * 1.6 / 100.0;
	} else {
		ratio = (double)(rate - 101) * 1.33 / 26.0 + 1.67;
	}
	return (1.0 / ratio);
}

/*! convert GS NRPN to vibrato depth. */
/* from -9.6 cents to +9.45 cents. */
int32_t Player::gs_cnv_vib_depth(int depth)
{
	double cent;
	cent = (double)(depth - 64) * 0.15;

	return (int32_t)(cent * 256.0 / 400.0);
}

/*! convert GS NRPN to vibrato delay. */
/* from 0 ms to 5074 ms. */
int32_t Player::gs_cnv_vib_delay(int delay)
{
	double ms;
	ms = 0.2092 * exp(0.0795 * (double)delay);
	if(delay == 0) {ms = 0;}
	return (int32_t)((double)playback_rate * ms * 0.001);
}

int Player::last_rpn_addr(int ch)
{
	int lsb, msb, addr, i;
	const struct rpn_tag_map_t *addrmap;
	struct rpn_tag_map_t {
		int addr, mask, tag;
	};
	static const struct rpn_tag_map_t nrpn_addr_map[] = {
		{0x0108, 0xffff, NRPN_ADDR_0108},
		{0x0109, 0xffff, NRPN_ADDR_0109},
		{0x010a, 0xffff, NRPN_ADDR_010A},
		{0x0120, 0xffff, NRPN_ADDR_0120},
		{0x0121, 0xffff, NRPN_ADDR_0121},
		{0x0130, 0xffff, NRPN_ADDR_0130},
		{0x0131, 0xffff, NRPN_ADDR_0131},
		{0x0134, 0xffff, NRPN_ADDR_0134},
		{0x0135, 0xffff, NRPN_ADDR_0135},
		{0x0163, 0xffff, NRPN_ADDR_0163},
		{0x0164, 0xffff, NRPN_ADDR_0164},
		{0x0166, 0xffff, NRPN_ADDR_0166},
		{0x1400, 0xff00, NRPN_ADDR_1400},
		{0x1500, 0xff00, NRPN_ADDR_1500},
		{0x1600, 0xff00, NRPN_ADDR_1600},
		{0x1700, 0xff00, NRPN_ADDR_1700},
		{0x1800, 0xff00, NRPN_ADDR_1800},
		{0x1900, 0xff00, NRPN_ADDR_1900},
		{0x1a00, 0xff00, NRPN_ADDR_1A00},
		{0x1c00, 0xff00, NRPN_ADDR_1C00},
		{0x1d00, 0xff00, NRPN_ADDR_1D00},
		{0x1e00, 0xff00, NRPN_ADDR_1E00},
		{0x1f00, 0xff00, NRPN_ADDR_1F00},
		{0x3000, 0xff00, NRPN_ADDR_3000},
		{0x3100, 0xff00, NRPN_ADDR_3100},
		{0x3400, 0xff00, NRPN_ADDR_3400},
		{0x3500, 0xff00, NRPN_ADDR_3500},
		{-1, -1, 0}
	};
	static const struct rpn_tag_map_t rpn_addr_map[] = {
		{0x0000, 0xffff, RPN_ADDR_0000},
		{0x0001, 0xffff, RPN_ADDR_0001},
		{0x0002, 0xffff, RPN_ADDR_0002},
		{0x0003, 0xffff, RPN_ADDR_0003},
		{0x0004, 0xffff, RPN_ADDR_0004},
		{0x0005, 0xffff, RPN_ADDR_0005},
		{0x7f7f, 0xffff, RPN_ADDR_7F7F},
		{0xffff, 0xffff, RPN_ADDR_FFFF},
		{-1, -1}
	};
	
	if (channel[ch].nrpn == -1)
		return -1;
	lsb = channel[ch].lastlrpn;
	msb = channel[ch].lastmrpn;
	if (lsb == 0xff || msb == 0xff)
		return -1;
	addr = (msb << 8 | lsb);
	if (channel[ch].nrpn)
		addrmap = nrpn_addr_map;
	else
		addrmap = rpn_addr_map;
	for (i = 0; addrmap[i].addr != -1; i++)
		if (addrmap[i].addr == (addr & addrmap[i].mask))
			return addrmap[i].tag;
	return -1;
}

void Player::update_rpn_map(int ch, int addr, int update_now)
{
	int val, drumflag, i, note;
	
	val = channel[ch].rpnmap[addr];
	drumflag = 0;
	switch (addr) {
	case NRPN_ADDR_0108:	/* Vibrato Rate */
		if (op_nrpn_vibrato) {
			//ctl_cmsg(CMSG_INFO, VERB_NOISY,	"Vibrato Rate (CH:%d VAL:%d)", ch, val - 64);
			channel[ch].vibrato_ratio = gs_cnv_vib_rate(val);
		}
		if (update_now)
			adjust_pitch(ch);
		break;
	case NRPN_ADDR_0109:	/* Vibrato Depth */
		if (op_nrpn_vibrato) {
			//ctl_cmsg(CMSG_INFO, VERB_NOISY,	"Vibrato Depth (CH:%d VAL:%d)", ch, val - 64);
			channel[ch].vibrato_depth = gs_cnv_vib_depth(val);
		}
		if (update_now)
			adjust_pitch(ch);
		break;
	case NRPN_ADDR_010A:	/* Vibrato Delay */
		if (op_nrpn_vibrato) {
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Vibrato Delay (CH:%d VAL:%d)", ch, val);
			channel[ch].vibrato_delay = gs_cnv_vib_delay(val);
		}
		if (update_now)
			adjust_pitch(ch);
		break;
	case NRPN_ADDR_0120:	/* Filter Cutoff Frequency */
		if (timidity_lpf_def) {
			//ctl_cmsg(CMSG_INFO, VERB_NOISY,	"Filter Cutoff (CH:%d VAL:%d)", ch, val - 64);
			channel[ch].param_cutoff_freq = val - 64;
		}
		break;
	case NRPN_ADDR_0121:	/* Filter Resonance */
		if (timidity_lpf_def) {
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Filter Resonance (CH:%d VAL:%d)", ch, val - 64);
			channel[ch].param_resonance = val - 64;
		}
		break;
	case NRPN_ADDR_0130:	/* EQ BASS */
		if (opt_eq_control) {
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"EQ BASS (CH:%d %.2f dB)", ch, 0.19 * (double)(val - 0x40));
			channel[ch].eq_xg.bass = val;
			recompute_part_eq_xg(&(channel[ch].eq_xg));
		}
		break;
	case NRPN_ADDR_0131:	/* EQ TREBLE */
		if (opt_eq_control) {
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"EQ TREBLE (CH:%d %.2f dB)", ch, 0.19 * (double)(val - 0x40));
			channel[ch].eq_xg.treble = val;
			recompute_part_eq_xg(&(channel[ch].eq_xg));
		}
		break;
	case NRPN_ADDR_0134:	/* EQ BASS frequency */
		if (opt_eq_control) {
			if(val < 4) {val = 4;}
			else if(val > 40) {val = 40;}
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"EQ BASS frequency (CH:%d %d Hz)", ch, (int32_t)eq_freq_table_xg[val]);
			channel[ch].eq_xg.bass_freq = val;
			recompute_part_eq_xg(&(channel[ch].eq_xg));
		}
		break;
	case NRPN_ADDR_0135:	/* EQ TREBLE frequency */
		if (opt_eq_control) {
			if(val < 28) {val = 28;}
			else if(val > 58) {val = 58;}
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"EQ TREBLE frequency (CH:%d %d Hz)", ch, (int32_t)eq_freq_table_xg[val]);
			channel[ch].eq_xg.treble_freq = val;
			recompute_part_eq_xg(&(channel[ch].eq_xg));
		}
		break;
	case NRPN_ADDR_0163:	/* Attack Time */
		if (opt_tva_attack) {set_envelope_time(ch, val, EG_ATTACK);}
		break;
	case NRPN_ADDR_0164:	/* EG Decay Time */
		if (opt_tva_decay) {set_envelope_time(ch, val, EG_DECAY);}
		break;
	case NRPN_ADDR_0166:	/* EG Release Time */
		if (opt_tva_release) {set_envelope_time(ch, val, EG_RELEASE);}
		break;
	case NRPN_ADDR_1400:	/* Drum Filter Cutoff (XG) */
		drumflag = 1;
		note = channel[ch].lastlrpn;
		if (channel[ch].drums[note] == NULL)
			play_midi_setup_drums(ch, note);
		//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Drum Instrument Filter Cutoff (CH:%d NOTE:%d VAL:%d)", ch, note, val);
		channel[ch].drums[note]->drum_cutoff_freq = val - 64;
		break;
	case NRPN_ADDR_1500:	/* Drum Filter Resonance (XG) */
		drumflag = 1;
		note = channel[ch].lastlrpn;
		if (channel[ch].drums[note] == NULL)
			play_midi_setup_drums(ch, note);
		//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Drum Instrument Filter Resonance (CH:%d NOTE:%d VAL:%d)", ch, note, val);
		channel[ch].drums[note]->drum_resonance = val - 64;
		break;
	case NRPN_ADDR_1600:	/* Drum EG Attack Time (XG) */
		drumflag = 1;
		if (opt_tva_attack) {
			val = val & 0x7f;
			note = channel[ch].lastlrpn;
			if (channel[ch].drums[note] == NULL)
				play_midi_setup_drums(ch, note);
			val	-= 64;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Drum Instrument Attack Time (CH:%d NOTE:%d VAL:%d)", ch, note, val);
			channel[ch].drums[note]->drum_envelope_rate[EG_ATTACK] = val;
		}
		break;
	case NRPN_ADDR_1700:	/* Drum EG Decay Time (XG) */
		drumflag = 1;
		if (opt_tva_decay) {
			val = val & 0x7f;
			note = channel[ch].lastlrpn;
			if (channel[ch].drums[note] == NULL)
				play_midi_setup_drums(ch, note);
			val	-= 64;
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Drum Instrument Decay Time (CH:%d NOTE:%d VAL:%d)", ch, note, val);
			channel[ch].drums[note]->drum_envelope_rate[EG_DECAY1] =
				channel[ch].drums[note]->drum_envelope_rate[EG_DECAY2] = val;
		}
		break;
	case NRPN_ADDR_1800:	/* Coarse Pitch of Drum (GS) */
		drumflag = 1;
		note = channel[ch].lastlrpn;
		if (channel[ch].drums[note] == NULL)
			play_midi_setup_drums(ch, note);
		channel[ch].drums[note]->coarse = val - 64;
		//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Drum Instrument Pitch Coarse (CH:%d NOTE:%d VAL:%d)", ch, note, channel[ch].drums[note]->coarse);
		channel[ch].pitchfactor = 0;
		break;
	case NRPN_ADDR_1900:	/* Fine Pitch of Drum (XG) */
		drumflag = 1;
		note = channel[ch].lastlrpn;
		if (channel[ch].drums[note] == NULL)
			play_midi_setup_drums(ch, note);
		channel[ch].drums[note]->fine = val - 64;
		ctl_cmsg(CMSG_INFO, VERB_NOISY,	"Drum Instrument Pitch Fine (CH:%d NOTE:%d VAL:%d)", ch, note, channel[ch].drums[note]->fine);
		channel[ch].pitchfactor = 0;
		break;
	case NRPN_ADDR_1A00:	/* Level of Drum */	 
		drumflag = 1;
		note = channel[ch].lastlrpn;
		if (channel[ch].drums[note] == NULL)
			play_midi_setup_drums(ch, note);
		//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Drum Instrument TVA Level (CH:%d NOTE:%d VAL:%d)", ch, note, val);
		channel[ch].drums[note]->drum_level =
				calc_drum_tva_level(ch, note, val);
		break;
	case NRPN_ADDR_1C00:	/* Panpot of Drum */
		drumflag = 1;
		note = channel[ch].lastlrpn;
		if (channel[ch].drums[note] == NULL)
			play_midi_setup_drums(ch, note);
		if(val == 0) {
			val = int_rand(128);
			channel[ch].drums[note]->pan_random = 1;
		} else
			channel[ch].drums[note]->pan_random = 0;
		channel[ch].drums[note]->drum_panning = val;
		if (update_now && adjust_panning_immediately && ! channel[ch].pan_random)
			adjust_drum_panning(ch, note);
		break;
	case NRPN_ADDR_1D00:	/* Reverb Send Level of Drum */
		drumflag = 1;
		note = channel[ch].lastlrpn;
		if (channel[ch].drums[note] == NULL)
			play_midi_setup_drums(ch, note);
		ctl_cmsg(CMSG_INFO, VERB_NOISY,	"Reverb Send Level of Drum (CH:%d NOTE:%d VALUE:%d)", ch, note, val);
		if (channel[ch].drums[note]->reverb_level != val) {
			channel[ch].drum_effect_flag = 0;
		}
		channel[ch].drums[note]->reverb_level = val;
		break;
	case NRPN_ADDR_1E00:	/* Chorus Send Level of Drum */
		drumflag = 1;
		note = channel[ch].lastlrpn;
		if (channel[ch].drums[note] == NULL)
			play_midi_setup_drums(ch, note);
		ctl_cmsg(CMSG_INFO, VERB_NOISY,	"Chorus Send Level of Drum (CH:%d NOTE:%d VALUE:%d)", ch, note, val);
		if (channel[ch].drums[note]->chorus_level != val) {
			channel[ch].drum_effect_flag = 0;
		}
		channel[ch].drums[note]->chorus_level = val;
		
		break;
	case NRPN_ADDR_1F00:	/* Variation Send Level of Drum */
		drumflag = 1;
		note = channel[ch].lastlrpn;
		if (channel[ch].drums[note] == NULL)
			play_midi_setup_drums(ch, note);
		//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Delay Send Level of Drum (CH:%d NOTE:%d VALUE:%d)", ch, note, val);
		if (channel[ch].drums[note]->delay_level != val) {
			channel[ch].drum_effect_flag = 0;
		}
		channel[ch].drums[note]->delay_level = val;
		break;
	case NRPN_ADDR_3000:	/* Drum EQ BASS */
		drumflag = 1;
		note = channel[ch].lastlrpn;
		if (channel[ch].drums[note] == NULL)
			play_midi_setup_drums(ch, note);
		break;
	case NRPN_ADDR_3100:	/* Drum EQ TREBLE */
		drumflag = 1;
		note = channel[ch].lastlrpn;
		if (channel[ch].drums[note] == NULL)
			play_midi_setup_drums(ch, note);
		break;
	case NRPN_ADDR_3400:	/* Drum EQ BASS frequency */
		drumflag = 1;
		note = channel[ch].lastlrpn;
		if (channel[ch].drums[note] == NULL)
			play_midi_setup_drums(ch, note);
		break;
	case NRPN_ADDR_3500:	/* Drum EQ TREBLE frequency */
		drumflag = 1;
		note = channel[ch].lastlrpn;
		if (channel[ch].drums[note] == NULL)
			play_midi_setup_drums(ch, note);
		break;
	case RPN_ADDR_0000:		/* Pitch bend sensitivity */
		ctl_cmsg(CMSG_INFO, VERB_DEBUG,	"Pitch Bend Sensitivity (CH:%d VALUE:%d)", ch, val);
		/* for mod2mid.c, arpeggio */
		if (channel[ch].rpnmap[RPN_ADDR_0000] > 24)
			channel[ch].rpnmap[RPN_ADDR_0000] = 24;
		channel[ch].pitchfactor = 0;
		break;
	case RPN_ADDR_0001:		/* Master Fine Tuning */
		ctl_cmsg(CMSG_INFO, VERB_DEBUG, "Master Fine Tuning (CH:%d VALUE:%d)", ch, val);
		channel[ch].pitchfactor = 0;
		break;
	case RPN_ADDR_0002:		/* Master Coarse Tuning */
		ctl_cmsg(CMSG_INFO, VERB_DEBUG, "Master Coarse Tuning (CH:%d VALUE:%d)", ch, val);
		channel[ch].pitchfactor = 0;
		break;
	case RPN_ADDR_0003:		/* Tuning Program Select */
		ctl_cmsg(CMSG_INFO, VERB_DEBUG, "Tuning Program Select (CH:%d VALUE:%d)", ch, val);
		for (i = 0; i < upper_voices; i++)
			if (voice[i].status != VOICE_FREE) {
				voice[i].temper_instant = 1;
				recompute_freq(i);
			}
		break;
	case RPN_ADDR_0004:		/* Tuning Bank Select */
		ctl_cmsg(CMSG_INFO, VERB_DEBUG, "Tuning Bank Select (CH:%d VALUE:%d)", ch, val);
		for (i = 0; i < upper_voices; i++)
			if (voice[i].status != VOICE_FREE) {
				voice[i].temper_instant = 1;
				recompute_freq(i);
			}
		break;
	case RPN_ADDR_0005:		/* GM2: Modulation Depth Range */
		channel[ch].mod.lfo1_pitch_depth = (((int32_t)channel[ch].rpnmap[RPN_ADDR_0005] << 7) | channel[ch].rpnmap_lsb[RPN_ADDR_0005]) * 100 / 128;
		//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Modulation Depth Range (CH:%d VALUE:%d)", ch, channel[ch].rpnmap[RPN_ADDR_0005]);
		break;
	case RPN_ADDR_7F7F:		/* RPN reset */
		channel[ch].rpn_7f7f_flag = 1;
		break;
	case RPN_ADDR_FFFF:		/* RPN initialize */
		/* All reset to defaults */
		channel[ch].rpn_7f7f_flag = 0;
		memset(channel[ch].rpnmap, 0, sizeof(channel[ch].rpnmap));
		channel[ch].lastlrpn = channel[ch].lastmrpn = 0;
		channel[ch].nrpn = 0;
		channel[ch].rpnmap[RPN_ADDR_0000] = 2;
		channel[ch].rpnmap[RPN_ADDR_0001] = 0x40;
		channel[ch].rpnmap[RPN_ADDR_0002] = 0x40;
		channel[ch].rpnmap_lsb[RPN_ADDR_0005] = 0x40;
		channel[ch].rpnmap[RPN_ADDR_0005] = 0;	/* +- 50 cents */
		channel[ch].pitchfactor = 0;
		break;
	}
	drumflag = 0;
	if (drumflag && midi_drumpart_change(ch, 1)) {
		midi_program_change(ch, channel[ch].program);
	}
}

void Player::voice_increment(int n)
{
    int i;
    for(i = 0; i < n; i++)
    {
	if(voices == max_voices)
	    break;
	voice[voices].status = VOICE_FREE;
	voice[voices].temper_instant = 0;
	voice[voices].chorus_link = voices;
	voices++;
    }
}

void Player::voice_decrement(int n)
{
    int i, j, lowest;
    int32_t lv, v;

    /* decrease voice */
    for(i = 0; i < n && voices > 0; i++)
    {
	voices--;
	if(voice[voices].status == VOICE_FREE)
	    continue;	/* found */

	for(j = 0; j < voices; j++)
	    if(voice[j].status == VOICE_FREE)
		break;
	if(j != voices)
	{
	    voice[j] = voice[voices];
	    continue;	/* found */
	}

	/* Look for the decaying note with the lowest volume */
	lv = 0x7FFFFFFF;
	lowest = -1;
	for(j = 0; j <= voices; j++)
	{
	    if(voice[j].status & ~(VOICE_ON | VOICE_DIE))
	    {
		v = voice[j].left_mix;
		if((voice[j].panned==PANNED_MYSTERY) &&
		   (voice[j].right_mix > v))
		    v = voice[j].right_mix;
		if(v < lv)
		{
		    lv = v;
		    lowest = j;
		}
	    }
	}

	if(lowest != -1)
	{
	    cut_notes++;
	    free_voice(lowest);
	    voice[lowest] = voice[voices];
	}
	else
	    lost_notes++;
    }
    if(upper_voices > voices)
	upper_voices = voices;
}

/* EAW -- do not throw away good notes, stop decrementing */
void Player::voice_decrement_conservative(int n)
{
    int i, j, lowest, finalnv;
    int32_t lv, v;

    /* decrease voice */
    finalnv = voices - n;
    for(i = 1; i <= n && voices > 0; i++)
    {
	if(voice[voices-1].status == VOICE_FREE) {
	    voices--;
	    continue;	/* found */
	}

	for(j = 0; j < finalnv; j++)
	    if(voice[j].status == VOICE_FREE)
		break;
	if(j != finalnv)
	{
	    voice[j] = voice[voices-1];
	    voices--;
	    continue;	/* found */
	}

	/* Look for the decaying note with the lowest volume */
	lv = 0x7FFFFFFF;
	lowest = -1;
	for(j = 0; j < voices; j++)
	{
	    if(voice[j].status & ~(VOICE_ON | VOICE_DIE) &&
	       !(voice[j].sample->note_to_use &&
	         ISDRUMCHANNEL(voice[j].channel)))
	    {
		v = voice[j].left_mix;
		if((voice[j].panned==PANNED_MYSTERY) &&
		   (voice[j].right_mix > v))
		    v = voice[j].right_mix;
		if(v < lv)
		{
		    lv = v;
		    lowest = j;
		}
	    }
	}

	if(lowest != -1)
	{
	    voices--;
	    cut_notes++;
	    free_voice(lowest);
	    voice[lowest] = voice[voices];
	}
	else break;
    }
    if(upper_voices > voices)
	upper_voices = voices;
}

void Player::mix_signal(int32_t *dest, int32_t *src, int32_t count)
{
	int32_t i;
	for (i = 0; i < count; i++) {
		dest[i] += src[i];
	}
}

int Player::is_insertion_effect_xg(int ch)
{
	int i;
	for (i = 0; i < XG_INSERTION_EFFECT_NUM; i++) {
		if (reverb->insertion_effect_xg[i].part == ch) {
			return 1;
		}
	}
	for (i = 0; i < XG_VARIATION_EFFECT_NUM; i++) {
		if (reverb->variation_effect_xg[i].connection == XG_CONN_INSERTION
			&& reverb->variation_effect_xg[i].part == ch) {
			return 1;
		}
	}
	return 0;
}

/* do_compute_data_midi() with DSP Effect */
void Player::do_compute_data(int32_t count)
{
	int i, j, uv, stereo, n, ch, note;
	int32_t *vpblist[MAX_CHANNELS];
	int channel_effect, channel_reverb, channel_chorus, channel_delay, channel_eq;
	int32_t cnt = count * 2, rev_max_delay_out;
	struct DrumPartEffect *de;
	
	stereo = true;
	n = count * ((stereo) ? 8 : 4); /* in bytes */

	memset(buffer_pointer, 0, n);
	memset(insertion_effect_buffer, 0, n);

	if (timidity_reverb == 3) {
		rev_max_delay_out = 0x7fffffff;	/* disable */
	} else {
		rev_max_delay_out = REVERB_MAX_DELAY_OUT;
	}

	/* are effects valid? / don't supported in mono */
	channel_reverb = (stereo && (timidity_reverb == 1
			|| timidity_reverb == 3
			|| (timidity_reverb < 0 && timidity_reverb & 0x80)));
	channel_chorus = (stereo && timidity_chorus && !timidity_surround_chorus);
	channel_delay = 0;

	/* is EQ valid? */
	channel_eq = 0;

	channel_effect = (stereo && (channel_reverb || channel_chorus
			|| channel_delay || channel_eq || opt_insertion_effect));

	uv = upper_voices;
	for(i = 0; i < uv; i++) {
		if(voice[i].status != VOICE_FREE) {
			channel[voice[i].channel].lasttime = current_sample + count;
		}
	}

	/* appropriate buffers for channels */
	if(channel_effect) {
		int buf_index = 0;
		
		if(reverb_buffer == NULL) {	/* allocating buffer for channel effect */
			reverb_buffer = (char *)safe_malloc(MAX_CHANNELS * AUDIO_BUFFER_SIZE * 8);
		}

		for(i = 0; i < MAX_CHANNELS; i++) {
			if(opt_insertion_effect && channel[i].insertion_effect) {
				vpblist[i] = insertion_effect_buffer;
			} else if(channel[i].eq_gs || (get_reverb_level(i) != DEFAULT_REVERB_SEND_LEVEL
					&& current_sample - channel[i].lasttime < rev_max_delay_out)
					|| channel[i].chorus_level > 0 || channel[i].delay_level > 0
					|| channel[i].eq_xg.valid
					|| channel[i].dry_level != 127
					|| (timidity_drum_effect && ISDRUMCHANNEL(i))
					|| is_insertion_effect_xg(i)) {
				vpblist[i] = (int32_t*)(reverb_buffer + buf_index);
				buf_index += n;
			} else {
				vpblist[i] = buffer_pointer;
			}
			/* clear buffers of drum-part effect */
			if (timidity_drum_effect && ISDRUMCHANNEL(i)) {
				for (j = 0; j < channel[i].drum_effect_num; j++) {
					if (channel[i].drum_effect[j].buf != NULL) {
						memset(channel[i].drum_effect[j].buf, 0, n);
					}
				}
			}
		}

		if(buf_index) {memset(reverb_buffer, 0, buf_index);}
	}

	for (i = 0; i < uv; i++) {
		if (voice[i].status != VOICE_FREE) {
			int32_t *vpb = NULL;
			int8_t flag;
			
			if (channel_effect) {
				flag = 0;
				ch = voice[i].channel;
				if (timidity_drum_effect && ISDRUMCHANNEL(ch)) {
					make_drum_effect(ch);
					note = voice[i].note;
					for (j = 0; j < channel[ch].drum_effect_num; j++) {
						if (channel[ch].drum_effect[j].note == note) {
							vpb = channel[ch].drum_effect[j].buf;
							flag = 1;
						}
					}
					if (flag == 0) {vpb = vpblist[ch];}
				} else {
					vpb = vpblist[ch];
				}
			} else {
				vpb = buffer_pointer;
			}

			if(!IS_SET_CHANNELMASK(channel_mute, voice[i].channel)) {
				mixer->mix_voice(vpb, i, count);
			} else {
				free_voice(i);
			}

			if(voice[i].timeout == 1 && voice[i].timeout < current_sample) {
				free_voice(i);
			}
		}
	}

	while(uv > 0 && voice[uv - 1].status == VOICE_FREE)	{uv--;}
	upper_voices = uv;

	if(play_system_mode == XG_SYSTEM_MODE && channel_effect) {	/* XG */
		if (opt_insertion_effect) { 	/* insertion effect */
			for (i = 0; i < XG_INSERTION_EFFECT_NUM; i++) {
				if (reverb->insertion_effect_xg[i].part <= MAX_CHANNELS) {
					reverb->do_insertion_effect_xg(vpblist[reverb->insertion_effect_xg[i].part], cnt, &reverb->insertion_effect_xg[i]);
				}
			}
			for (i = 0; i < XG_VARIATION_EFFECT_NUM; i++) {
				if (reverb->variation_effect_xg[i].part <= MAX_CHANNELS) {
					reverb->do_insertion_effect_xg(vpblist[reverb->variation_effect_xg[i].part], cnt, &reverb->variation_effect_xg[i]);
				}
			}
		}
		for(i = 0; i < MAX_CHANNELS; i++) {	/* system effects */
			int32_t *p;
			p = vpblist[i];
			if(p != buffer_pointer) {
				if (timidity_drum_effect && ISDRUMCHANNEL(i)) {
					for (j = 0; j < channel[i].drum_effect_num; j++) {
						de = &(channel[i].drum_effect[j]);
						if (de->reverb_send > 0) {
							reverb->set_ch_reverb(de->buf, cnt, de->reverb_send);
						}
						if (de->chorus_send > 0) {
							reverb->set_ch_chorus(de->buf, cnt, de->chorus_send);
						}
						if (de->delay_send > 0) {
							reverb->set_ch_delay(de->buf, cnt, de->delay_send);
						}
						mix_signal(p, de->buf, cnt);
					}
				} else {
					if(channel_eq && channel[i].eq_xg.valid) {
						reverb->do_ch_eq_xg(p, cnt, &(channel[i].eq_xg));
					}
					if(channel_chorus && channel[i].chorus_level > 0) {
						reverb->set_ch_chorus(p, cnt, channel[i].chorus_level);
					}
					if(channel_delay && channel[i].delay_level > 0) {
						reverb->set_ch_delay(p, cnt, channel[i].delay_level);
					}
					if(channel_reverb && channel[i].reverb_level > 0
						&& current_sample - channel[i].lasttime < rev_max_delay_out) {
						reverb->set_ch_reverb(p, cnt, channel[i].reverb_level);
					}
				}
				if(channel[i].dry_level == 127) {
					reverb->set_dry_signal(p, cnt);
				} else {
					reverb->set_dry_signal_xg(p, cnt, channel[i].dry_level);
				}
			}
		}
		
		if(channel_reverb) {
			reverb->set_ch_reverb(buffer_pointer, cnt, DEFAULT_REVERB_SEND_LEVEL);
		}
		reverb->set_dry_signal(buffer_pointer, cnt);

		/* mixing signal and applying system effects */ 
		reverb->mix_dry_signal(buffer_pointer, cnt);
		if(channel_delay) { reverb->do_variation_effect1_xg(buffer_pointer, cnt);}
		if(channel_chorus) { reverb->do_ch_chorus_xg(buffer_pointer, cnt);}
		if(channel_reverb) { reverb->do_ch_reverb(buffer_pointer, cnt);}
		if(reverb->multi_eq_xg.valid) { reverb->do_multi_eq_xg(buffer_pointer, cnt);}
	} else if(channel_effect) {	/* GM & GS */
		if(opt_insertion_effect) { 	/* insertion effect */
			/* applying insertion effect */
			reverb->do_insertion_effect_gs(insertion_effect_buffer, cnt);
			/* sending insertion effect voice to channel effect */
			reverb->set_ch_chorus(insertion_effect_buffer, cnt, reverb->insertion_effect_gs.send_chorus);
			reverb->set_ch_delay(insertion_effect_buffer, cnt, reverb->insertion_effect_gs.send_delay);
			reverb->set_ch_reverb(insertion_effect_buffer, cnt, reverb->insertion_effect_gs.send_reverb);
			if(reverb->insertion_effect_gs.send_eq_switch && channel_eq) {
				reverb->set_ch_eq_gs(insertion_effect_buffer, cnt);
			} else {
				reverb->set_dry_signal(insertion_effect_buffer, cnt);
			}
		}

		for(i = 0; i < MAX_CHANNELS; i++) {	/* system effects */
			int32_t *p;	
			p = vpblist[i];
			if(p != buffer_pointer && p != insertion_effect_buffer) {
				if (timidity_drum_effect && ISDRUMCHANNEL(i)) {
					for (j = 0; j < channel[i].drum_effect_num; j++) {
						de = &(channel[i].drum_effect[j]);
						if (de->reverb_send > 0) {
							reverb->set_ch_reverb(de->buf, cnt, de->reverb_send);
						}
						if (de->chorus_send > 0) {
							reverb->set_ch_chorus(de->buf, cnt, de->chorus_send);
						}
						if (de->delay_send > 0) {
							reverb->set_ch_delay(de->buf, cnt, de->delay_send);
						}
						mix_signal(p, de->buf, cnt);
					}
				} else {
					if(channel_chorus && channel[i].chorus_level > 0) {
						reverb->set_ch_chorus(p, cnt, channel[i].chorus_level);
					}
					if(channel_delay && channel[i].delay_level > 0) {
						reverb->set_ch_delay(p, cnt, channel[i].delay_level);
					}
					if(channel_reverb && channel[i].reverb_level > 0
						&& current_sample - channel[i].lasttime < rev_max_delay_out) {
						reverb->set_ch_reverb(p, cnt, channel[i].reverb_level);
					}
				}
				if(channel_eq && channel[i].eq_gs) {
					reverb->set_ch_eq_gs(p, cnt);
				} else {
					reverb->set_dry_signal(p, cnt);
				}
			}
		}
		
		if(channel_reverb) {
			reverb->set_ch_reverb(buffer_pointer, cnt, DEFAULT_REVERB_SEND_LEVEL);
		}
		reverb->set_dry_signal(buffer_pointer, cnt);

		/* mixing signal and applying system effects */ 
		reverb->mix_dry_signal(buffer_pointer, cnt);
		if(channel_eq) { reverb->do_ch_eq_gs(buffer_pointer, cnt);}
		if(channel_chorus) { reverb->do_ch_chorus(buffer_pointer, cnt);}
		if(channel_delay) { reverb->do_ch_delay(buffer_pointer, cnt);}
		if(channel_reverb) { reverb->do_ch_reverb(buffer_pointer, cnt);}
	}

	current_sample += count;
}

int Player::compute_data(float *buffer, int32_t count)
{
	if (count == 0) return RC_OK;

	std::lock_guard<std::mutex> lock(CvarCritSec);

	if (last_reverb_setting != timidity_reverb)
	{
		// If the reverb mode has changed some buffers need to be reallocated before doing any sound generation.
		reverb->free_effect_buffers();
		reverb->init_reverb();
		last_reverb_setting = timidity_reverb;
	}

	buffer_pointer = common_buffer;
	computed_samples += count;

	while (count > 0)
	{
		int process = std::min(count, AUDIO_BUFFER_SIZE);
		do_compute_data(process);
		count -= process;

		effect->do_effect(common_buffer, process);
		// pass to caller
		for (int i = 0; i < process*2; i++)
		{
			*buffer++ = (common_buffer[i])*(5.f / 0x80000000u);
		}
	}
	return RC_OK;
}

void Player::update_modulation_wheel(int ch)
{
    int i, uv = upper_voices;
	channel[ch].pitchfactor = 0;
    for(i = 0; i < uv; i++)
	if(voice[i].status != VOICE_FREE && voice[i].channel == ch)
	{
	    /* Set/Reset mod-wheel */
		voice[i].vibrato_control_counter = voice[i].vibrato_phase = 0;
	    recompute_amp(i);
		mixer->apply_envelope_to_amp(i);
	    recompute_freq(i);
		recompute_voice_filter(i);
	}
}

void Player::drop_portamento(int ch)
{
    int i, uv = upper_voices;

    channel[ch].porta_control_ratio = 0;
    for(i = 0; i < uv; i++)
	if(voice[i].status != VOICE_FREE &&
	   voice[i].channel == ch &&
	   voice[i].porta_control_ratio)
	{
	    voice[i].porta_control_ratio = 0;
	    recompute_freq(i);
	}
    channel[ch].last_note_fine = -1;
}

void Player::update_portamento_controls(int ch)
{
    if(!channel[ch].portamento ||
       (channel[ch].portamento_time_msb | channel[ch].portamento_time_lsb)
       == 0)
	drop_portamento(ch);
    else
    {
	double mt, dc;
	int d;

	mt = midi_time_table[channel[ch].portamento_time_msb & 0x7F] *
	    midi_time_table2[channel[ch].portamento_time_lsb & 0x7F] *
		PORTAMENTO_TIME_TUNING;
	dc = playback_rate * mt;
	d = (int)(1.0 / (mt * PORTAMENTO_CONTROL_RATIO));
	d++;
	channel[ch].porta_control_ratio = (int)(d * dc + 0.5);
	channel[ch].porta_dpb = d;
    }
}

void Player::update_portamento_time(int ch)
{
    int i, uv = upper_voices;
    int dpb;
    int32_t ratio;

    update_portamento_controls(ch);
    dpb = channel[ch].porta_dpb;
    ratio = channel[ch].porta_control_ratio;

    for(i = 0; i < uv; i++)
    {
	if(voice[i].status != VOICE_FREE &&
	   voice[i].channel == ch &&
	   voice[i].porta_control_ratio)
	{
	    voice[i].porta_control_ratio = ratio;
	    voice[i].porta_dpb = dpb;
	    recompute_freq(i);
	}
    }
}

void Player::update_legato_controls(int ch)
{
	double mt, dc;
	int d;

	mt = 0.06250 * PORTAMENTO_TIME_TUNING * 0.3;
	dc = playback_rate * mt;
	d = (int)(1.0 / (mt * PORTAMENTO_CONTROL_RATIO));
	d++;
	channel[ch].porta_control_ratio = (int)(d * dc + 0.5);
	channel[ch].porta_dpb = d;
}

int Player::play_event(MidiEvent *ev)
{
	int32_t i, j;
	int k, l, ch, orig_ch, port_ch, offset, layered;

	current_event = ev;

#ifndef SUPPRESS_CHANNEL_LAYER
	orig_ch = ev->channel;
	layered = !IS_SYSEX_EVENT_TYPE(ev);
	for (k = 0; k < MAX_CHANNELS; k += 16) {
		port_ch = (orig_ch + k) % MAX_CHANNELS;
		offset = port_ch & ~0xf;
		for (l = offset; l < offset + 16; l++) {
			if (!layered && (k || l != offset))
				continue;
			if (layered) {
				if (!IS_SET_CHANNELMASK(channel[l].channel_layer, port_ch)
					|| channel[l].port_select != (orig_ch >> 4))
					continue;
				ev->channel = l;
			}
#endif
			ch = ev->channel;

			switch (ev->type)
			{
				/* MIDI Events */
			case ME_NOTEOFF:
				note_off(ev);
				break;

			case ME_NOTEON:
				note_on(ev);
				break;

			case ME_KEYPRESSURE:
				adjust_pressure(ev);
				break;

			case ME_PROGRAM:
				midi_program_change(ch, ev->a);
				break;

			case ME_CHANNEL_PRESSURE:
				adjust_channel_pressure(ev);
				break;

			case ME_PITCHWHEEL:
				channel[ch].pitchbend = ev->a + ev->b * 128;
				channel[ch].pitchfactor = 0;
				/* Adjust pitch for notes already playing */
				adjust_pitch(ch);
				break;

				/* Controls */
			case ME_TONE_BANK_MSB:
				channel[ch].bank_msb = ev->a;
				break;

			case ME_TONE_BANK_LSB:
				channel[ch].bank_lsb = ev->a;
				break;

			case ME_MODULATION_WHEEL:
				channel[ch].mod.val = ev->a;
				update_modulation_wheel(ch);
				break;

			case ME_MAINVOLUME:
				channel[ch].volume = ev->a;
				adjust_volume(ch);
				break;

			case ME_PAN:
				channel[ch].panning = ev->a;
				channel[ch].pan_random = 0;
				if (adjust_panning_immediately && !channel[ch].pan_random)
					adjust_panning(ch);
				break;

			case ME_EXPRESSION:
				channel[ch].expression = ev->a;
				adjust_volume(ch);
				break;

			case ME_SUSTAIN:
				if (channel[ch].sustain == 0 && ev->a >= 64) {
					update_redamper_controls(ch);
				}
				channel[ch].sustain = ev->a;
				if (channel[ch].damper_mode == 0) {	/* half-damper is not allowed. */
					if (channel[ch].sustain >= 64) { channel[ch].sustain = 127; }
					else { channel[ch].sustain = 0; }
				}
				if (channel[ch].sustain == 0 && channel[ch].sostenuto == 0)
					drop_sustain(ch);
				break;

			case ME_SOSTENUTO:
				channel[ch].sostenuto = (ev->a >= 64);
				if (channel[ch].sustain == 0 && channel[ch].sostenuto == 0)
					drop_sustain(ch);
				else { update_sostenuto_controls(ch); }
				break;

			case ME_LEGATO_FOOTSWITCH:
				channel[ch].legato = (ev->a >= 64);
				break;

			case ME_HOLD2:
				break;

			case ME_BREATH:
				break;

			case ME_FOOT:
				break;

			case ME_BALANCE:
				break;

			case ME_PORTAMENTO_TIME_MSB:
				channel[ch].portamento_time_msb = ev->a;
				update_portamento_time(ch);
				break;

			case ME_PORTAMENTO_TIME_LSB:
				channel[ch].portamento_time_lsb = ev->a;
				update_portamento_time(ch);
				break;

			case ME_PORTAMENTO:
				channel[ch].portamento = (ev->a >= 64);
				if (!channel[ch].portamento)
					drop_portamento(ch);
				break;

			case ME_SOFT_PEDAL:
				if (timidity_lpf_def) {
					channel[ch].soft_pedal = ev->a;
					//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Soft Pedal (CH:%d VAL:%d)", ch, channel[ch].soft_pedal);
				}
				break;

			case ME_HARMONIC_CONTENT:
				if (timidity_lpf_def) {
					channel[ch].param_resonance = ev->a - 64;
					//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Harmonic Content (CH:%d VAL:%d)", ch, channel[ch].param_resonance);
				}
				break;

			case ME_BRIGHTNESS:
				if (timidity_lpf_def) {
					channel[ch].param_cutoff_freq = ev->a - 64;
					//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Brightness (CH:%d VAL:%d)", ch, channel[ch].param_cutoff_freq);
				}
				break;

			case ME_DATA_ENTRY_MSB:
				if (channel[ch].rpn_7f7f_flag) /* disable */
					break;
				if ((i = last_rpn_addr(ch)) >= 0)
				{
					channel[ch].rpnmap[i] = ev->a;
					update_rpn_map(ch, i, 1);
				}
				break;

			case ME_DATA_ENTRY_LSB:
				if (channel[ch].rpn_7f7f_flag) /* disable */
					break;
				if ((i = last_rpn_addr(ch)) >= 0)
				{
					channel[ch].rpnmap_lsb[i] = ev->a;
				}
				break;

			case ME_REVERB_EFFECT:
				if (timidity_reverb) {
					if (ISDRUMCHANNEL(ch) && get_reverb_level(ch) != ev->a) { channel[ch].drum_effect_flag = 0; }
					set_reverb_level(ch, ev->a);
				}
				break;

			case ME_CHORUS_EFFECT:
				if (timidity_chorus)
				{
					if (timidity_chorus == 1) {
						if (ISDRUMCHANNEL(ch) && channel[ch].chorus_level != ev->a) { channel[ch].drum_effect_flag = 0; }
						channel[ch].chorus_level = ev->a;
					}
					else {
						channel[ch].chorus_level = -timidity_chorus;
					}
					if (ev->a) {
						//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Chorus Send (CH:%d LEVEL:%d)", ch, ev->a);
					}
				}
				break;

			case ME_TREMOLO_EFFECT:
				//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Tremolo Send (CH:%d LEVEL:%d)", ch, ev->a);
				break;

			case ME_CELESTE_EFFECT:
				if (opt_delay_control) {
					if (ISDRUMCHANNEL(ch) && channel[ch].delay_level != ev->a) { channel[ch].drum_effect_flag = 0; }
					channel[ch].delay_level = ev->a;
					if (play_system_mode == XG_SYSTEM_MODE) {
						//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Variation Send (CH:%d LEVEL:%d)", ch, ev->a);
					}
					else {
						//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Delay Send (CH:%d LEVEL:%d)", ch, ev->a);
					}
				}
				break;

			case ME_ATTACK_TIME:
				if (!opt_tva_attack) { break; }
				set_envelope_time(ch, ev->a, EG_ATTACK);
				break;

			case ME_RELEASE_TIME:
				if (!opt_tva_release) { break; }
				set_envelope_time(ch, ev->a, EG_RELEASE);
				break;

			case ME_PHASER_EFFECT:
				//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Phaser Send (CH:%d LEVEL:%d)", ch, ev->a);
				break;

			case ME_RPN_INC:
				if (channel[ch].rpn_7f7f_flag) /* disable */
					break;
				if ((i = last_rpn_addr(ch)) >= 0)
				{
					if (channel[ch].rpnmap[i] < 127)
						channel[ch].rpnmap[i]++;
					update_rpn_map(ch, i, 1);
				}
				break;

			case ME_RPN_DEC:
				if (channel[ch].rpn_7f7f_flag) /* disable */
					break;
				if ((i = last_rpn_addr(ch)) >= 0)
				{
					if (channel[ch].rpnmap[i] > 0)
						channel[ch].rpnmap[i]--;
					update_rpn_map(ch, i, 1);
				}
				break;

			case ME_NRPN_LSB:
				channel[ch].lastlrpn = ev->a;
				channel[ch].nrpn = 1;
				break;

			case ME_NRPN_MSB:
				channel[ch].lastmrpn = ev->a;
				channel[ch].nrpn = 1;
				break;

			case ME_RPN_LSB:
				channel[ch].lastlrpn = ev->a;
				channel[ch].nrpn = 0;
				break;

			case ME_RPN_MSB:
				channel[ch].lastmrpn = ev->a;
				channel[ch].nrpn = 0;
				break;

			case ME_ALL_SOUNDS_OFF:
				all_sounds_off(ch);
				break;

			case ME_RESET_CONTROLLERS:
				reset_controllers(ch);
				break;

			case ME_ALL_NOTES_OFF:
				all_notes_off(ch);
				break;

			case ME_MONO:
				channel[ch].mono = 1;
				all_notes_off(ch);
				break;

			case ME_POLY:
				channel[ch].mono = 0;
				all_notes_off(ch);
				break;

				/* TiMidity Extensionals */
			case ME_RANDOM_PAN:
				channel[ch].panning = int_rand(128);
				channel[ch].pan_random = 1;
				if (adjust_panning_immediately && !channel[ch].pan_random)
					adjust_panning(ch);
				break;

			case ME_SET_PATCH:
				i = channel[ch].special_sample = current_event->a;
				instruments->setSpecialPatchOffset(i, 0);
				break;

			case ME_TEMPO:
				current_play_tempo = ch + ev->b * 256 + ev->a * 65536;
				break;

			case ME_CHORUS_TEXT:
			case ME_LYRIC:
			case ME_MARKER:
			case ME_INSERT_TEXT:
			case ME_TEXT:
			case ME_KARAOKE_LYRIC:
			case ME_GSLCD:
				break;

			case ME_MASTER_VOLUME:
				master_volume_ratio = (int32_t)ev->a + 256 * (int32_t)ev->b;
				adjust_master_volume();
				break;

			case ME_RESET:
				change_system_mode(ev->a);
				reset_midi(1);
				break;

			case ME_PATCH_OFFS:
				i = channel[ch].special_sample;
				instruments->setSpecialPatchOffset(i, current_event->a | (256 * current_event->b));
				break;

			case ME_WRD:
				break;

			case ME_SHERRY:
				break;

			case ME_DRUMPART:
				if (midi_drumpart_change(ch, current_event->a))
				{
					/* Update bank information */
					midi_program_change(ch, channel[ch].program);
				}
				break;

			case ME_KEYSHIFT:
				i = (int)current_event->a - 0x40;
				if (i != channel[ch].key_shift)
				{
					all_sounds_off(ch);
					channel[ch].key_shift = (int8_t)i;
				}
				break;

			case ME_KEYSIG:
				if (opt_init_keysig != 8)
					break;
				current_keysig = current_event->a + current_event->b * 16;
				if (opt_force_keysig != 8) {
					i = current_keysig - ((current_keysig < 8) ? 0 : 16), j = 0;
					while (i != opt_force_keysig && i != opt_force_keysig + 12)
						i += (i > 0) ? -5 : 7, j++;
					while (abs(j - note_key_offset) > 7)
						j += (j > note_key_offset) ? -12 : 12;
					if (abs(j - timidity_key_adjust) >= 12)
						j += (j > timidity_key_adjust) ? -12 : 12;
					note_key_offset = j;
					kill_all_voices();
				}
				i = current_keysig + ((current_keysig < 8) ? 7 : -9), j = 0;
				while (i != 7)
					i += (i < 7) ? 5 : -7, j++;
				j += note_key_offset, j -= floor(j / 12.0) * 12;
				current_freq_table = j;
				break;

			case ME_MASTER_TUNING:
				set_master_tuning((ev->b << 8) | ev->a);
				adjust_all_pitch();
				break;

			case ME_SCALE_TUNING:
				recache->resamp_cache_refer_alloff(ch, computed_samples);
				channel[ch].scale_tuning[current_event->a] = current_event->b;
				adjust_pitch(ch);
				break;

			case ME_BULK_TUNING_DUMP:
				set_single_note_tuning(ch, current_event->a, current_event->b, 0);
				break;

			case ME_SINGLE_NOTE_TUNING:
				set_single_note_tuning(ch, current_event->a, current_event->b, 1);
				break;

			case ME_TEMPER_KEYSIG:
				current_temper_keysig = (current_event->a + 8) % 32 - 8;
				temper_adj = ((current_event->a + 8) & 0x20) ? 1 : 0;
				i = current_temper_keysig + ((current_temper_keysig < 8) ? 7 : -9);
				j = 0;
				while (i != 7)
					i += (i < 7) ? 5 : -7, j++;
				j += note_key_offset, j -= floor(j / 12.0) * 12;
				current_temper_freq_table = j;
				if (current_event->b)
					for (i = 0; i < upper_voices; i++)
						if (voice[i].status != VOICE_FREE) {
							voice[i].temper_instant = 1;
							recompute_freq(i);
						}
				break;

			case ME_TEMPER_TYPE:
				channel[ch].temper_type = current_event->a;
				if (temper_type_mute) {
					if (temper_type_mute & (1 << (current_event->a
						- ((current_event->a >= 0x40) ? 0x3c : 0)))) {
						SET_CHANNELMASK(channel_mute, ch);
					}
					else {
						UNSET_CHANNELMASK(channel_mute, ch);
					}
				}
				if (current_event->b)
					for (i = 0; i < upper_voices; i++)
						if (voice[i].status != VOICE_FREE) {
							voice[i].temper_instant = 1;
							recompute_freq(i);
						}
				break;

			case ME_MASTER_TEMPER_TYPE:
				for (i = 0; i < MAX_CHANNELS; i++) {
					channel[i].temper_type = current_event->a;
				}
				if (temper_type_mute) {
					if (temper_type_mute & (1 << (current_event->a
						- ((current_event->a >= 0x40) ? 0x3c : 0)))) {
						FILL_CHANNELMASK(channel_mute);
					}
					else {
						CLEAR_CHANNELMASK(channel_mute);
					}
				}
				if (current_event->b)
					for (i = 0; i < upper_voices; i++)
						if (voice[i].status != VOICE_FREE) {
							voice[i].temper_instant = 1;
							recompute_freq(i);
						}
				break;

			case ME_USER_TEMPER_ENTRY:
				set_user_temper_entry(ch, current_event->a, current_event->b);
				break;

			case ME_SYSEX_LSB:
				process_sysex_event(ME_SYSEX_LSB, ch, current_event->a, current_event->b);
				break;

			case ME_SYSEX_MSB:
				process_sysex_event(ME_SYSEX_MSB, ch, current_event->a, current_event->b);
				break;

			case ME_SYSEX_GS_LSB:
				process_sysex_event(ME_SYSEX_GS_LSB, ch, current_event->a, current_event->b);
				break;

			case ME_SYSEX_GS_MSB:
				process_sysex_event(ME_SYSEX_GS_MSB, ch, current_event->a, current_event->b);
				break;

			case ME_SYSEX_XG_LSB:
				process_sysex_event(ME_SYSEX_XG_LSB, ch, current_event->a, current_event->b);
				break;

			case ME_SYSEX_XG_MSB:
				process_sysex_event(ME_SYSEX_XG_MSB, ch, current_event->a, current_event->b);
				break;

			case ME_NOTE_STEP:
				break;

			case ME_EOT:
				break;
			}
#ifndef SUPPRESS_CHANNEL_LAYER
		}
	}
	ev->channel = orig_ch;
#endif

	return RC_OK;
}

void Player::set_master_tuning(int tune)
{
	if (tune & 0x4000)	/* 1/8192 semitones + 0x2000 | 0x4000 */
		tune = (tune & 0x3FFF) - 0x2000;
	else if (tune & 0x8000)	/* 1 semitones | 0x8000 */
		tune = ((tune & 0x7F) - 0x40) << 13;
	else	/* millisemitones + 0x400 */
		tune = (((tune - 0x400) << 13) + 500) / 1000;
	master_tuning = tune;
}

void Player::set_single_note_tuning(int part, int a, int b, int rt)
{
	static int tp;	/* tuning program number */
	static int kn;	/* MIDI key number */
	static int st;	/* the nearest equal-tempered semitone */
	double f, fst;	/* fraction of semitone */
	int i;
	
	switch (part) {
	case 0:
		tp = a;
		break;
	case 1:
		kn = a, st = b;
		break;
	case 2:
		if (st == 0x7f && a == 0x7f && b == 0x7f)	/* no change */
			break;
		f = 440 * pow(2.0, (st - 69) / 12.0);
		fst = pow(2.0, (a << 7 | b) / 196608.0);
		freq_table_tuning[tp][kn] = f * fst * 1000 + 0.5;
		if (rt)
			for (i = 0; i < upper_voices; i++)
				if (voice[i].status != VOICE_FREE) {
					voice[i].temper_instant = 1;
					recompute_freq(i);
				}
		break;
	}
}

void Player::set_user_temper_entry(int part, int a, int b)
{
	static int tp;		/* temperament program number */
	static int ll;		/* number of formula */
	static int fh, fl;	/* applying pitch bit mask (forward) */
	static int bh, bl;	/* applying pitch bit mask (backward) */
	static int aa, bb;	/* fraction (aa/bb) */
	static int cc, dd;	/* power (cc/dd)^(ee/ff) */
	static int ee, ff;
	static int ifmax, ibmax, count;
	static double rf[11], rb[11];
	int i, j, k, l, n, m;
	double ratio[12], f, sc;
	
	switch (part) {
	case 0:
		for (i = 0; i < 11; i++)
			rf[i] = rb[i] = 1;
		ifmax = ibmax = 0;
		count = 0;
		tp = a, ll = b;
		break;
	case 1:
		fh = a, fl = b;
		break;
	case 2:
		bh = a, bl = b;
		break;
	case 3:
		aa = a, bb = b;
		break;
	case 4:
		cc = a, dd = b;
		break;
	case 5:
		ee = a, ff = b;
		for (i = 0; i < 11; i++) {
			if (((fh & 0xf) << 7 | fl) & 1 << i) {
				rf[i] *= (double) aa / bb
						* pow((double) cc / dd, (double) ee / ff);
				if (ifmax < i + 1)
					ifmax = i + 1;
			}
			if (((bh & 0xf) << 7 | bl) & 1 << i) {
				rb[i] *= (double) aa / bb
						* pow((double) cc / dd, (double) ee / ff);
				if (ibmax < i + 1)
					ibmax = i + 1;
			}
		}
		if (++count < ll)
			break;
		ratio[0] = 1;
		for (i = n = m = 0; i < ifmax; i++, m = n) {
			n += (n > 4) ? -5 : 7;
			ratio[n] = ratio[m] * rf[i];
			if (ratio[n] > 2)
				ratio[n] /= 2;
		}
		for (i = n = m = 0; i < ibmax; i++, m = n) {
			n += (n > 6) ? -7 : 5;
			ratio[n] = ratio[m] / rb[i];
			if (ratio[n] < 1)
				ratio[n] *= 2;
		}
		sc = 27 / ratio[9] / 16;	/* syntonic comma */
		for (i = 0; i < 12; i++)
			for (j = -1; j < 11; j++) {
				f = 440 * pow(2.0, (i - 9) / 12.0 + j - 5);
				for (k = 0; k < 12; k++) {
					l = i + j * 12 + k;
					if (l < 0 || l >= 128)
						continue;
					if (! (fh & 0x40)) {	/* major */
						freq_table_user[tp][i][l] =
								f * ratio[k] * 1000 + 0.5;
						freq_table_user[tp][i + 36][l] =
								f * ratio[k] * sc * 1000 + 0.5;
					}
					if (! (bh & 0x40)) {	/* minor */
						freq_table_user[tp][i + 12][l] =
								f * ratio[k] * sc * 1000 + 0.5;
						freq_table_user[tp][i + 24][l] =
								f * ratio[k] * 1000 + 0.5;
					}
				}
			}
		break;
	}
}




struct midi_file_info *Player::new_midi_file_info()
{
	struct midi_file_info *p = &midifileinfo;

	/* Initialize default members */
	memset(p, 0, sizeof(struct midi_file_info));
	p->hdrsiz = -1;
	p->format = -1;
	p->tracks = -1;
	p->divisions = -1;
	p->time_sig_n = p->time_sig_d = -1;
	p->samples = -1;
	p->max_channel = -1;
	COPY_CHANNELMASK(p->drumchannels, default_drumchannels);
	COPY_CHANNELMASK(p->drumchannel_mask, default_drumchannel_mask);
	return p;
}



/*
 * For MIDI stream player.
 */
void Player::playmidi_stream_init(void)
{
    int i;
    static int first = 1;

    note_key_offset = timidity_key_adjust;
    midi_time_ratio = timidity_tempo_adjust;
    CLEAR_CHANNELMASK(channel_mute);
	if (temper_type_mute & 1)
		FILL_CHANNELMASK(channel_mute);
	if (first)
	{
		first = 0;
		init_mblock(&playmidi_pool);
		midi_streaming = 1;
	}
    else
        reuse_mblock(&playmidi_pool);

    /* Fill in current_file_info */
	current_file_info = &midifileinfo;
    current_file_info->readflag = 1;
    current_file_info->hdrsiz = 0;
    current_file_info->format = 0;
    current_file_info->tracks = 0;
    current_file_info->divisions = 192; /* ?? */
    current_file_info->time_sig_n = 4; /* 4/ */
    current_file_info->time_sig_d = 4; /* /4 */
    current_file_info->time_sig_c = 24; /* clock */
    current_file_info->time_sig_b = 8;  /* q.n. */
    current_file_info->samples = 0;
    current_file_info->max_channel = MAX_CHANNELS;
    current_file_info->compressed = 0;

    current_play_tempo = 500000;
    check_eot_flag = 0;

    /* Setup default drums */
	COPY_CHANNELMASK(current_file_info->drumchannels, default_drumchannels);
	COPY_CHANNELMASK(current_file_info->drumchannel_mask, default_drumchannel_mask);
    for(i = 0; i < MAX_CHANNELS; i++)
	memset(channel[i].drums, 0, sizeof(channel[i].drums));
    change_system_mode(DEFAULT_SYSTEM_MODE);
    reset_midi(0);

    playmidi_tmr_reset();
}

void Player::playmidi_tmr_reset(void)
{
    int i;

    current_sample = 0;
    buffer_pointer = common_buffer;
    for(i = 0; i < MAX_CHANNELS; i++)
	channel[i].lasttime = 0;
}


/*! initialize Part EQ (XG) */
void Player::init_part_eq_xg(struct part_eq_xg *p)
{
	p->bass = 0x40;
	p->treble = 0x40;
	p->bass_freq = 0x0C;
	p->treble_freq = 0x36;
	p->valid = 0;
}

/*! recompute Part EQ (XG) */
void Player::recompute_part_eq_xg(struct part_eq_xg *p)
{
	int8_t vbass, vtreble;

	if(p->bass_freq >= 4 && p->bass_freq <= 40 && p->bass != 0x40) {
		vbass = 1;
		p->basss.q = 0.7;
		p->basss.freq = eq_freq_table_xg[p->bass_freq];
		if(p->bass == 0) {p->basss.gain = -12.0;}
		else {p->basss.gain = 0.19 * (double)(p->bass - 0x40);}
		reverb->calc_filter_shelving_low(&(p->basss));
	} else {vbass = 0;}
	if(p->treble_freq >= 28 && p->treble_freq <= 58 && p->treble != 0x40) {
		vtreble = 1;
		p->trebles.q = 0.7;
		p->trebles.freq = eq_freq_table_xg[p->treble_freq];
		if(p->treble == 0) {p->trebles.gain = -12.0;}
		else {p->trebles.gain = 0.19 * (double)(p->treble - 0x40);}
		reverb->calc_filter_shelving_high(&(p->trebles));
	} else {vtreble = 0;}
	p->valid = vbass || vtreble;
}

void Player::init_midi_controller(midi_controller *p)
{
	p->val = 0;
	p->pitch = 0;
	p->cutoff = 0;
	p->amp = 0.0;
	p->lfo1_rate = p->lfo2_rate = p->lfo1_tva_depth = p->lfo2_tva_depth = 0;
	p->lfo1_pitch_depth = p->lfo2_pitch_depth = p->lfo1_tvf_depth = p->lfo2_tvf_depth = 0;
	p->variation_control_depth = p->insertion_control_depth = 0;
}

float Player::get_midi_controller_amp(midi_controller *p)
{
	return (1.0 + (float)p->val * (1.0f / 127.0f) * p->amp);
}

float Player::get_midi_controller_filter_cutoff(midi_controller *p)
{
	return ((float)p->val * (1.0f / 127.0f) * (float)p->cutoff);
}

float Player::get_midi_controller_filter_depth(midi_controller *p)
{
	return ((float)p->val * (1.0f / 127.0f) * (float)p->lfo1_tvf_depth);
}

int32_t Player::get_midi_controller_pitch(midi_controller *p)
{
	return ((int32_t)(p->val * p->pitch) << 6);
}

int16_t Player::get_midi_controller_pitch_depth(midi_controller *p)
{
	return (int16_t)((float)p->val * (float)p->lfo1_pitch_depth * (1.0f / 127.0f * 256.0 / 400.0));
}

int16_t Player::get_midi_controller_amp_depth(midi_controller *p)
{
	return (int16_t)((float)p->val * (float)p->lfo1_tva_depth * (1.0f / 127.0f * 256.0));
}

void Player::init_rx(int ch)
{
	channel[ch].rx = 0xFFFFFFFF;	/* all on */
}

void Player::set_rx(int ch, int32_t rx, int flag)
{
	if(ch > MAX_CHANNELS) {return;}
	if(flag) {channel[ch].rx |= rx;}
	else {channel[ch].rx &= ~rx;}
}

void Player::init_rx_drum(struct DrumParts *p)
{
	p->rx = 0xFFFFFFFF;	/* all on */
}

void Player::set_rx_drum(struct DrumParts *p, int32_t rx, int flag)
{
	if(flag) {p->rx |= rx;}
	else {p->rx &= ~rx;}
}

int32_t Player::get_rx_drum(struct DrumParts *p, int32_t rx)
{
	return (p->rx & rx);
}

Instrument *Player::play_midi_load_instrument(int dr, int bk, int prog)
{
	bool load_success;
	// The inner workings of this function which alters the instrument data has been put into the Instruments class.
	auto instr = instruments->play_midi_load_instrument(dr, bk, prog, &load_success);
	//if (load_success) send_output(NULL, 0);	/* Update software buffer */
	return instr;
}

void Player::change_system_mode(int mode)
{
	pan_table = sc_pan_table;
	switch (mode)
	{
	case GM_SYSTEM_MODE:
		if (play_system_mode == DEFAULT_SYSTEM_MODE)
		{
			play_system_mode = GM_SYSTEM_MODE;
			vol_table = def_vol_table;
		}
		break;
	case GM2_SYSTEM_MODE:
		play_system_mode = GM2_SYSTEM_MODE;
		vol_table = def_vol_table;
		pan_table = gm2_pan_table;
		break;
	case GS_SYSTEM_MODE:
		play_system_mode = GS_SYSTEM_MODE;
		vol_table = gs_vol_table;
		break;
	case XG_SYSTEM_MODE:
		if (play_system_mode != XG_SYSTEM_MODE) { reverb->init_all_effect_xg(); }
		play_system_mode = XG_SYSTEM_MODE;
		vol_table = xg_vol_table;
		break;
	default:
		play_system_mode = DEFAULT_SYSTEM_MODE;
		vol_table = def_vol_table;
		break;
	}
}


/*! initialize channel layers. */
void Player::init_channel_layer(int ch)
{
	if (ch >= MAX_CHANNELS)
		return;
	CLEAR_CHANNELMASK(channel[ch].channel_layer);
	SET_CHANNELMASK(channel[ch].channel_layer, ch);
	channel[ch].port_select = ch >> 4;
}


static const struct ctl_chg_types {
	unsigned char mtype;
	int ttype;
} ctl_chg_list[] = {
	{ 0, ME_TONE_BANK_MSB },
{ 1, ME_MODULATION_WHEEL },
{ 2, ME_BREATH },
{ 4, ME_FOOT },
{ 5, ME_PORTAMENTO_TIME_MSB },
{ 6, ME_DATA_ENTRY_MSB },
{ 7, ME_MAINVOLUME },
{ 8, ME_BALANCE },
{ 10, ME_PAN },
{ 11, ME_EXPRESSION },
{ 32, ME_TONE_BANK_LSB },
{ 37, ME_PORTAMENTO_TIME_LSB },
{ 38, ME_DATA_ENTRY_LSB },
{ 64, ME_SUSTAIN },
{ 65, ME_PORTAMENTO },
{ 66, ME_SOSTENUTO },
{ 67, ME_SOFT_PEDAL },
{ 68, ME_LEGATO_FOOTSWITCH },
{ 69, ME_HOLD2 },
{ 71, ME_HARMONIC_CONTENT },
{ 72, ME_RELEASE_TIME },
{ 73, ME_ATTACK_TIME },
{ 74, ME_BRIGHTNESS },
{ 84, ME_PORTAMENTO_CONTROL },
{ 91, ME_REVERB_EFFECT },
{ 92, ME_TREMOLO_EFFECT },
{ 93, ME_CHORUS_EFFECT },
{ 94, ME_CELESTE_EFFECT },
{ 95, ME_PHASER_EFFECT },
{ 96, ME_RPN_INC },
{ 97, ME_RPN_DEC },
{ 98, ME_NRPN_LSB },
{ 99, ME_NRPN_MSB },
{ 100, ME_RPN_LSB },
{ 101, ME_RPN_MSB },
{ 120, ME_ALL_SOUNDS_OFF },
{ 121, ME_RESET_CONTROLLERS },
{ 123, ME_ALL_NOTES_OFF },
{ 126, ME_MONO },
{ 127, ME_POLY },
};

int Player::convert_midi_control_change(int chn, int type, int val, MidiEvent *ev_ret)
{
	int etype = -1;
	for (auto &t : ctl_chg_list)
	{
		if (t.mtype == type)
		{
			if (val > 127) val = 127;
			ev_ret->type = t.ttype;
			ev_ret->channel = chn;
			ev_ret->a = val;
			ev_ret->b = 0;
			return 1;
		}
	}
	return 0;
}


int Player::send_event(int status, int parm1, int parm2)
{
	MidiEvent ev;

	ev.type = ME_NONE;
	ev.channel = status & 0x0000000f;
	//ev.channel = ev.channel + port * 16;
	ev.a = (uint8_t)parm1;
	ev.b = (uint8_t)parm2;
	switch ((int)(status & 0x000000f0)) 
	{
	case 0x80:
		ev.type = ME_NOTEOFF;
		break;
	case 0x90:
		ev.type = (ev.b) ? ME_NOTEON : ME_NOTEOFF;
		break;
	case 0xa0:
		ev.type = ME_KEYPRESSURE;
		break;
	case 0xb0:
		if (!convert_midi_control_change(ev.channel, ev.a, ev.b, &ev))
			ev.type = ME_NONE;
		break;
	case 0xc0:
		ev.type = ME_PROGRAM;
		break;
	case 0xd0:
		ev.type = ME_CHANNEL_PRESSURE;
		break;
	case 0xe0:
		ev.type = ME_PITCHWHEEL;
		break;
	/*
	case 0xf0:
		if ((status & 0x000000ff) == 0xf2) 
		{
			ev.type = ME_PROGRAM;
		}
		break;
	*/
	default:
		break;
	}
	if (ev.type != ME_NONE) 
	{
		play_event(&ev);
	}
	return 0;
}

void Player::send_long_event(const uint8_t *sysexbuffer, int exlen) 
{
	int i, ne;
	MidiEvent ev;
	MidiEvent evm[260];
	SysexConvert sc;

	if ((sysexbuffer[0] != 0xf0) && (sysexbuffer[0] != 0xf7)) return;
	
	if (sc.parse_sysex_event(sysexbuffer + 1, exlen - 1, &ev, instruments)) 
	{
		if (ev.type == ME_RESET)
		{
			kill_all_voices();
			for (int i = 0; i < MAX_CHANNELS; i++)
				init_channel_layer(i);

			/* initialize effect status */
			reverb->init_effect_status(play_system_mode);
			effect->init_effect();
			instruments->init_userdrum();
			instruments->init_userinst();
			playmidi_stream_init();
		}
		play_event(&ev);
		return;
	}
	if ((ne = sc.parse_sysex_event_multi(sysexbuffer + 1, exlen - 1, evm, instruments)))
	{
		for (i = 0; i < ne; i++) 
		{
			play_event(&evm[i]);
		}
	}
}



}
