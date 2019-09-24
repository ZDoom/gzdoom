#pragma once

namespace Timidity
{
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


/* Causes the instrument's default panning to be used. */
#define NO_PANNING				-1


/* Voice status options: */
enum
{
	VOICE_RUNNING			= (1<<0),
	VOICE_SUSTAINING		= (1<<1),
	VOICE_RELEASING			= (1<<2),
	VOICE_STOPPING			= (1<<3),

	VOICE_LPE				= (1<<4),
	NOTE_SUSTAIN			= (1<<5),
};

/* Envelope stages: */
enum
{
	GF1_ATTACK,
	GF1_HOLD,
	GF1_DECAY,
	GF1_RELEASE,
	GF1_RELEASEB,
	GF1_RELEASEC
};

enum
{
	SF2_DELAY,
	SF2_ATTACK,
	SF2_HOLD,
	SF2_DECAY,
	SF2_SUSTAIN,
	SF2_RELEASE,
	SF2_FINISHED
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

const double log_of_2 = 0.69314718055994529;

#define sine(x)			(sin((2*PI/1024.0) * (x)))

#define note_to_freq(x)	(float(8175.7989473096690661233836992789 * pow(2.0, (x) / 12.0)))
#define freq_to_note(x) (log((x) / 8175.7989473096690661233836992789) * (12.0 / log_of_2))

#define calc_gf1_amp(x)	(pow(2.0,((x)*16.0 - 16.0)))			// Actual GUS equation
}
