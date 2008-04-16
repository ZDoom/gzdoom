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

#include "doomtype.h"
#include "zstring.h"

class FileReader;

namespace Timidity
{

/*
config.h
*/

/* Acoustic Grand Piano seems to be the usual default instrument. */
#define DEFAULT_PROGRAM				 0

/* 9 here is MIDI channel 10, which is the standard percussion channel.
   Some files (notably C:\WINDOWS\CANYON.MID) think that 16 is one too. 
   On the other hand, some files know that 16 is not a drum channel and
   try to play music on it. This is now a runtime option, so this isn't
   a critical choice anymore. */
#define DEFAULT_DRUMCHANNELS		(1<<9)
/*#define DEFAULT_DRUMCHANNELS		((1<<9) | (1<<15))*/

/* Default polyphony, and maximum polyphony. */
#define DEFAULT_VOICES				32
#define MAX_VOICES					256

#define MAXCHAN						16
#define MAXNOTE						128

/* 1000 here will give a control ratio of 22:1 with 22 kHz output.
   Higher CONTROLS_PER_SECOND values allow more accurate rendering
   of envelopes and tremolo. The cost is CPU time. */
#define CONTROLS_PER_SECOND			1000

/* How many bits to use for the fractional part of sample positions.
   This affects tonal accuracy. The entire position counter must fit
   in 32 bits, so with FRACTION_BITS equal to 12, the maximum size of
   a sample is 1048576 samples (2 megabytes in memory). The GUS gets
   by with just 9 bits and a little help from its friends...
   "The GUS does not SUCK!!!" -- a happy user :) */
#define FRACTION_BITS				12

/* For some reason the sample volume is always set to maximum in all
   patch files. Define this for a crude adjustment that may help
   equalize instrument volumes. */
#define ADJUST_SAMPLE_VOLUMES

/* The number of samples to use for ramping out a dying note. Affects
   click removal. */
#define MAX_DIE_TIME				20

/**************************************************************************/
/* Anything below this shouldn't need to be changed unless you're porting
   to a new machine with other than 32-bit, big-endian words. */
/**************************************************************************/

/* change FRACTION_BITS above, not these */
#define INTEGER_BITS				(32 - FRACTION_BITS)
#define INTEGER_MASK				(0xFFFFFFFF << FRACTION_BITS)
#define FRACTION_MASK				(~ INTEGER_MASK)
#define MAX_SAMPLE_SIZE				(1 << INTEGER_BITS)

/* This is enforced by some computations that must fit in an int */
#define MAX_CONTROL_RATIO			255

#define MAX_AMPLIFICATION			800

/* The TiMiditiy configuration file */
#define CONFIG_FILE	"timidity.cfg"

typedef float sample_t;
typedef float final_volume_t;
#define FINAL_VOLUME(v)				(v)

#define FSCALE(a,b)					((a) * (float)(1<<(b)))
#define FSCALENEG(a,b)				((a) * (1.0L / (float)(1<<(b))))

/* Vibrato and tremolo Choices of the Day */
#define SWEEP_TUNING				38
#define VIBRATO_AMPLITUDE_TUNING	1.0
#define VIBRATO_RATE_TUNING			38
#define TREMOLO_AMPLITUDE_TUNING	1.0
#define TREMOLO_RATE_TUNING			38

#define SWEEP_SHIFT					16
#define RATE_SHIFT					5

#define VIBRATO_SAMPLE_INCREMENTS	32

#ifndef PI
  #define PI 3.14159265358979323846
#endif

#if defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
// [RH] MinGW's pow() function is terribly slow compared to VC8's
// (I suppose because it's using an old version from MSVCRT.DLL).
// On an Opteron running x86-64 Linux, this also ended up being about
// 100 cycles faster than libm's pow(), which is why I'm using this
// for GCC in general and not just for MinGW.

extern __inline__ double pow_x87_inline(double x,double y)
{
	double result;

	if (y == 0)
	{
		return 1;
	}
	if (x == 0)
	{
		if (y > 0)
		{
			return 0;
		}
		else
		{
			union { double fp; long long ip; } infinity;
			infinity.ip = 0x7FF0000000000000ll;
			return infinity.fp;
		}
	}
	__asm__ (
		"fyl2x\n\t"
		"fld %%st(0)\n\t"
		"frndint\n\t"
		"fxch\n\t"
		"fsub %%st(1),%%st(0)\n\t"
		"f2xm1\n\t"
		"fld1\n\t"
		"faddp\n\t"
		"fxch\n\t"
		"fld1\n\t"
		"fscale\n\t"
		"fstp %%st(1)\n\t"
		"fmulp\n\t"
		: "=t" (result)
		: "0" (x), "u" (y)
		: "st(1)", "st(7)", "%3", "%4" );
	return result;
}
#define pow pow_x87_inline
#endif

/*
common.h
*/

#define OM_FILEORLUMP 0
#define OM_LUMP 1
#define OM_FILE 2

extern void add_to_pathlist(const char *s);
extern void clear_pathlist();
extern void *safe_malloc(size_t count);

FileReader *open_filereader(const char *name, int open, int *plumpnum);

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

void cmsg(int type, int verbosity_level, const char *fmt, ...);


/*
instrum.h
*/

struct Sample
{
	SDWORD
		loop_start, loop_end, data_length,
		sample_rate, low_vel, high_vel, low_freq, high_freq, root_freq;
	SDWORD
		envelope_rate[6], envelope_offset[6];
	float
		volume;
	sample_t *data;
	SDWORD 
		tremolo_sweep_increment, tremolo_phase_increment,
		vibrato_sweep_increment, vibrato_control_ratio;
	BYTE
		tremolo_depth, vibrato_depth,
		modes;
	WORD
		panning, scale_factor;
	SWORD
		scale_note;
	bool
		self_nonexclusive;
	BYTE
		key_group;
};

void convert_sample_data(Sample *sample, const void *data);
void free_instruments();

/* Patch definition: */
enum
{
	HEADER_SIZE					= 12,
	ID_SIZE						= 10,
	DESC_SIZE					= 60,
	RESERVED_SIZE				= 40,
	PATCH_HEADER_RESERVED_SIZE	= 36,
	LAYER_RESERVED_SIZE			= 40,
	PATCH_DATA_RESERVED_SIZE	= 36,
	INST_NAME_SIZE				= 16,
	ENVELOPES					= 6,
	MAX_LAYERS					= 4
};
#define GF1_HEADER_TEXT			"GF1PATCH110"

enum
{
	PATCH_16				= (1<<0),
	PATCH_UNSIGNED			= (1<<1),
	PATCH_LOOPEN			= (1<<2),
	PATCH_BIDIR				= (1<<3),
	PATCH_BACKWARD			= (1<<4),
	PATCH_SUSTAIN			= (1<<5),
	PATCH_NO_SRELEASE		= (1<<6),
	PATCH_FAST_REL			= (1<<7)
};

#ifdef _MSC_VER
#pragma pack(push, 1)
#define GCC_PACKED
#else
#define GCC_PACKED __attribute__((__packed__))
#endif

struct GF1PatchHeader
{
	char Header[HEADER_SIZE];
	char GravisID[ID_SIZE];		/* Id = "ID#000002" */
	char Description[DESC_SIZE];
	BYTE Instruments;
	BYTE Voices;
	BYTE Channels;
	WORD WaveForms;
	WORD MasterVolume;
	DWORD DataSize;
	BYTE Reserved[PATCH_HEADER_RESERVED_SIZE];
} GCC_PACKED;

struct GF1InstrumentData
{
	WORD Instrument;
	char InstrumentName[INST_NAME_SIZE];
	int  InstrumentSize;
	BYTE Layers;
	BYTE Reserved[RESERVED_SIZE];
} GCC_PACKED;

struct GF1LayerData
{
	BYTE LayerDuplicate;
	BYTE Layer;
	int  LayerSize;
	BYTE Samples;
	BYTE Reserved[LAYER_RESERVED_SIZE];
} GCC_PACKED;

struct GF1PatchData
{
	char  WaveName[7];
	BYTE  Fractions;
	int   WaveSize;
	int   StartLoop;
	int   EndLoop;
	WORD  SampleRate;
	int   LowFrequency;
	int   HighFrequency;
	int   RootFrequency;
	SWORD Tune;
	BYTE  Balance;
	BYTE  EnvelopeRate[6];
	BYTE  EnvelopeOffset[6];
	BYTE  TremoloSweep;
	BYTE  TremoloRate;
	BYTE  TremoloDepth;
	BYTE  VibratoSweep;
	BYTE  VibratoRate;
	BYTE  VibratoDepth;
	BYTE  Modes;
	SWORD ScaleFrequency;
	WORD  ScaleFactor;		/* From 0 to 2048 or 0 to 2 */
	BYTE  Reserved[PATCH_DATA_RESERVED_SIZE];
} GCC_PACKED;
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#undef GCC_PACKED

enum
{
	INST_GUS,
	INST_DLS
};

struct Instrument
{
	Instrument();
	~Instrument();

	int type;
	int samples;
	Sample *sample;
};

struct ToneBankElement
{
	ToneBankElement() :
		note(0), amp(0), pan(0), strip_loop(0), strip_envelope(0), strip_tail(0)
	{}

	FString name;
	int note, amp, pan, strip_loop, strip_envelope, strip_tail;
};

/* A hack to delay instrument loading until after reading the
entire MIDI file. */
#define MAGIC_LOAD_INSTRUMENT ((Instrument *)(-1))

enum
{
	MAXPROG				= 128,
	MAXBANK				= 128
};

struct ToneBank
{
	ToneBank();
	~ToneBank();

	ToneBankElement *tone;
	Instrument *instrument[MAXPROG];
};


#define SPECIAL_PROGRAM		-1

extern void pcmap(int *b, int *v, int *p, int *drums);

/*
mix.h
*/

extern void mix_voice(struct Renderer *song, float *buf, struct Voice *v, int c);
extern int recompute_envelope(struct Voice *v);
extern void apply_envelope_to_amp(struct Voice *v);

/*
playmidi.h
*/

/* Midi events */
enum
{
	ME_NOTEOFF				= 0x80,
	ME_NOTEON				= 0x90,
	ME_KEYPRESSURE			= 0xA0,
	ME_CONTROLCHANGE		= 0xB0,
	ME_PROGRAM				= 0xC0,
	ME_CHANNELPRESSURE		= 0xD0,
	ME_PITCHWHEEL			= 0xE0
};

/* Controllers */
enum
{
	CTRL_BANK_SELECT		= 0,
	CTRL_DATA_ENTRY			= 6,
	CTRL_VOLUME				= 7,
	CTRL_PAN				= 10,
	CTRL_EXPRESSION			= 11,
	CTRL_SUSTAIN			= 64,
	CTRL_HARMONICCONTENT	= 71,
	CTRL_RELEASETIME		= 72,
	CTRL_ATTACKTIME			= 73,
	CTRL_BRIGHTNESS			= 74,
	CTRL_REVERBERATION		= 91,
	CTRL_CHORUSDEPTH		= 93,
	CTRL_NRPN_LSB			= 98,
	CTRL_NRPN_MSB			= 99,
	CTRL_RPN_LSB			= 100,
	CTRL_RPN_MSB			= 101,
	CTRL_ALL_SOUNDS_OFF		= 120,
	CTRL_RESET_CONTROLLERS	= 121,
	CTRL_ALL_NOTES_OFF		= 123
};

/* RPNs */
enum
{
	RPN_PITCH_SENS			= 0x0000,
	RPN_FINE_TUNING			= 0x0001,
	RPN_COARSE_TUNING		= 0x0002,
	RPN_RESET				= 0x3fff
};

struct Channel
{
	int
		bank, program, sustain, pitchbend, 
		mono, /* one note only on this channel -- not implemented yet */
		pitchsens;
	WORD
		volume, expression;
	SWORD
		panning;
	WORD
		rpn, nrpn;
	bool
		nrpn_mode;
	float
		pitchfactor; /* precomputed pitch bend factor to save some fdiv's */
};

/* Causes the instrument's default panning to be used. */
#define NO_PANNING				-1
/* envelope points */
#define MAXPOINT				6

struct Voice
{
	BYTE
		status, channel, note, velocity;
	Sample *sample;
	float
		orig_frequency, frequency;
	int
		sample_offset, sample_increment,
		envelope_volume, envelope_target, envelope_increment,
		tremolo_sweep, tremolo_sweep_position,
		tremolo_phase, tremolo_phase_increment,
		vibrato_sweep, vibrato_sweep_position;

	final_volume_t left_mix, right_mix;

	float
		left_amp, right_amp, tremolo_volume;
	int
		vibrato_sample_increment[VIBRATO_SAMPLE_INCREMENTS];
	int
		vibrato_phase, vibrato_control_ratio, vibrato_control_counter,
		envelope_stage, control_counter, panning, panned;

};

/* Voice status options: */
enum
{
	VOICE_FREE,
	VOICE_ON,
	VOICE_SUSTAINED,
	VOICE_OFF,
	VOICE_DIE
};

/* Voice panned options: */
enum
{
	PANNED_MYSTERY,
	PANNED_LEFT,
	PANNED_RIGHT,
	PANNED_CENTER
};
/* Anything but PANNED_MYSTERY only uses the left volume */

/* Envelope stages: */
enum
{
	ATTACK,
	HOLD,
	DECAY,
	RELEASE,
	RELEASEB,
	RELEASEC
};

#define ISDRUMCHANNEL(c) ((drumchannels & (1<<(c))))

/*
resample.h
*/

extern sample_t *resample_voice(struct Renderer *song, Voice *v, int *countptr);
extern void pre_resample(struct Renderer *song, Sample *sp);

/* 
tables.h
*/

#define sine(x)			(sin((2*PI/1024.0) * (x)))
#define note_to_freq(x)	(float(8175.7989473096690661233836992789 * pow(2.0, (x) / 12.0)))
#define calc_vol(x)		(pow(2.0,((x)*6.0 - 6.0)))

/*
timidity.h
*/
struct DLS_Data;
int LoadConfig(const char *filename);
extern int LoadConfig();
extern void FreeAll();

extern ToneBank *tonebank[MAXBANK];
extern ToneBank *drumset[MAXBANK];

struct Renderer
{
	float rate;
	DLS_Data *patches;
	Instrument *default_instrument;
	int default_program;
	int resample_buffer_size;
	sample_t *resample_buffer;
	Channel channel[16];
	Voice voice[MAX_VOICES];
	int control_ratio, amp_with_poly;
	int drumchannels;
	int adjust_panning_immediately;
	int voices;
	int lost_notes, cut_notes;

	Renderer(float sample_rate);
	~Renderer();

	void HandleEvent(int status, int parm1, int parm2);
	void HandleLongMessage(const BYTE *data, int len);
	void HandleController(int chan, int ctrl, int val);
	void ComputeOutput(float *buffer, int num_samples);
	void MarkInstrument(int bank, int percussion, int instr);
	void Reset();

	int load_missing_instruments();
	int set_default_instrument(const char *name);
	int convert_tremolo_sweep(BYTE sweep);
	int convert_vibrato_sweep(BYTE sweep, int vib_control_ratio);
	int convert_tremolo_rate(BYTE rate);
	int convert_vibrato_rate(BYTE rate);

	void recompute_amp(Voice *v);
	void kill_key_group(int voice);
	float calculate_scaled_frequency(Sample *sample, int note);
	void start_note(int chan, int note, int vel, int voice);

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

	void select_sample(int voice, Instrument *instr, int vel);
	void recompute_freq(int voice);

	void kill_note(int voice);
	void finish_note(int voice);

	void DataEntryCoarseRPN(int chan, int rpn, int val);
	void DataEntryFineRPN(int chan, int rpn, int val);
	void DataEntryCoarseNRPN(int chan, int nrpn, int val);
	void DataEntryFineNRPN(int chan, int nrpn, int val);
};

}
#endif
