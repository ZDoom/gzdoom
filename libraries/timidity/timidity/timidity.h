/*
	TiMidity -- Experimental MIDI to WAVE converter
	Copyright (C) 1995 Tuukka Toivonen <toivonen@clinet.fi>

	This library is free software; you can redistribute it and/or
	modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation; either
	version 2.1 of the License, or (at your option) any later version.

	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public
	License along with this library; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef TIMIDITY_H
#define TIMIDITY_H

#include <stdint.h>
#include <string>
#include "../../music_common/fileio.h"

namespace Timidity
{
	using timidity_file = MusicIO::FileInterface;


/*
controls.h
*/

enum
{
	CMSG_INFO,
	CMSG_WARNING,
	CMSG_ERROR
};

enum
{
	VERB_NORMAL,
	VERB_VERBOSE,
	VERB_NOISY,
	VERB_DEBUG
};

extern void (*printMessage)(int type, int verbosity_level, const char *fmt, ...);


/*
timidity.h
*/
struct DLS_Data;
struct Instrument;
struct Sample;
typedef float sample_t;
typedef float final_volume_t;
class Instruments;

enum
{
	VIBRATO_SAMPLE_INCREMENTS = 32
};

struct MinEnvelope
{
	uint8_t stage;
	uint8_t bUpdating;
};

struct GF1Envelope : public MinEnvelope
{
	int volume, target, increment;
	int rate[6], offset[6];

	void Init(struct Renderer* song, struct Voice* v);
	bool Update(struct Voice* v);
	bool Recompute(struct Voice* v);
	void ApplyToAmp(struct Voice* v);
	void Release(struct Voice* v);
};

struct SF2Envelope : public MinEnvelope
{
	float volume;
	float DelayTime;	// timecents
	float AttackTime;	// timecents
	float HoldTime;		// timecents
	float DecayTime;	// timecents
	float SustainLevel;	// -0.1%
	float ReleaseTime;	// timecents
	float SampleRate;
	int HoldStart;
	float RateMul;
	float RateMul_cB;

	void Init(struct Renderer* song, Voice* v);
	bool Update(struct Voice* v);
	void ApplyToAmp(struct Voice* v);
	void Release(struct Voice* v);
};

struct Envelope
{
	union
	{
		MinEnvelope env;
		GF1Envelope gf1;
		SF2Envelope sf2;
	};

	uint8_t Type;

	void Init(struct Renderer* song, struct Voice* v);
	bool Update(struct Voice* v);
	void ApplyToAmp(struct Voice* v);
	void Release(struct Voice* v);
};

struct Channel
{
	int
		bank, program, sustain, pitchbend,
		mono, /* one note only on this channel */
		pitchsens;
	uint8_t
		volume, expression;
	int8_t
		panning;
	uint16_t
		rpn, nrpn;
	bool
		nrpn_mode;
	float
		pitchfactor; /* precomputed pitch bend factor to save some fdiv's */
};

struct Voice
{
	uint8_t
		status, channel, note, velocity;
	Sample* sample;
	float
		orig_frequency, frequency;
	int
		sample_offset, sample_increment,
		tremolo_sweep, tremolo_sweep_position,
		tremolo_phase, tremolo_phase_increment,
		vibrato_sweep, vibrato_sweep_position;

	Envelope eg1, eg2;

	final_volume_t left_mix, right_mix;

	float
		attenuation, left_offset, right_offset;
	float
		tremolo_volume;
	int
		vibrato_sample_increment[VIBRATO_SAMPLE_INCREMENTS];
	int
		vibrato_phase, vibrato_control_ratio, vibrato_control_counter,
		control_counter;

	int
		sample_count;
};

struct Renderer
{
//private:
	float rate;
	DLS_Data *patches;
	Instruments* instruments;
	Instrument *default_instrument;
	int default_program;
	int resample_buffer_size;
	sample_t *resample_buffer;
	Channel channel[16];
	Voice *voice;
	int control_ratio, amp_with_poly;
	int drumchannels;
	int adjust_panning_immediately;
	int voices;
	int lost_notes, cut_notes;
public:
	Renderer(float sample_rate, int voices, Instruments *instr);
	~Renderer();

	void HandleEvent(int status, int parm1, int parm2);
	void HandleLongMessage(const uint8_t *data, int len);
	void HandleController(int chan, int ctrl, int val);
	void ComputeOutput(float *buffer, int num_samples);
	void MarkInstrument(int bank, int percussion, int instr);
	void Reset();

	int load_missing_instruments();
//private:
	int set_default_instrument(const char *name);
	int convert_tremolo_sweep(uint8_t sweep);
	int convert_vibrato_sweep(uint8_t sweep, int vib_control_ratio);
	int convert_tremolo_rate(uint8_t rate);
	int convert_vibrato_rate(uint8_t rate);

	void recompute_freq(int voice);
	void recompute_amp(Voice *v);
	void recompute_pan(Channel *chan);

	void kill_key_group(int voice);
	float calculate_scaled_frequency(Sample *sample, int note);
	void start_note(int chan, int note, int vel);
	bool start_region(int chan, int note, int vel, Sample *sp, float freq);

	void note_on(int chan, int note, int vel);
	void note_off(int chan, int note, int vel);
	void all_notes_off(int chan);
	void all_sounds_off(int chan);
	void adjust_pressure(int chan, int note, int amount);
	void adjust_panning(int chan);
	void drop_sustain(int chan);
	void adjust_pitchbend(int chan);
	void adjust_volume(int chan);

	void reset_voices();
	void reset_controllers(int chan);
	void reset_midi();

	int allocate_voice();

	void kill_note(int voice);
	void finish_note(int voice);

	void DataEntryCoarseRPN(int chan, int rpn, int val);
	void DataEntryFineRPN(int chan, int rpn, int val);
	void DataEntryCoarseNRPN(int chan, int nrpn, int val);
	void DataEntryFineNRPN(int chan, int nrpn, int val);

	int fill_bank(int dr, int b);
	Instrument* load_instrument(const char* name, int percussion,
		int panning, int note_to_use,
		int strip_loop, int strip_envelope,
		int strip_tail);

	Instrument* load_instrument_font(const char* font, int drum, int bank, int instrument);
	Instrument* load_instrument_font_order(int order, int drum, int bank, int instrument);

	static void compute_pan(double panning, int type, float &left_offset, float &right_offset);
};

}
#endif
