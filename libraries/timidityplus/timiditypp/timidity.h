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

/*
 * Historical issues: This file once was a huge header file, but now is
 * devided into some smaller ones.  Please do not add things to this
 * header, but consider put them on other files.
 */


#ifndef TIMIDITY_H_INCLUDED
#define TIMIDITY_H_INCLUDED 1

#include "controls.h"
#include "mblock.h"
#include <mutex>
#include <stdint.h>

#ifdef _MSC_VER
#pragma warning(disable:4244)	// double->float truncation occurs so often in here that it's pointless to fix it all.
#endif



/* 
   Table of contents:
   (1) Flags and definitions to customize timidity
   (3) inportant definitions not to customize
   (2) #includes -- include other headers
 */

/*****************************************************************************\
 section 1: some customize issues
\*****************************************************************************/


/* How many bits to use for the fractional part of sample positions.
   This affects tonal accuracy. The entire position counter must fit
   in 32 bits, so with FRACTION_BITS equal to 12, the maximum size of
   a sample is 1048576 samples (2 megabytes in memory). The GUS gets
   by with just 9 bits and a little help from its friends...
   "The GUS does not SUCK!!!" -- a happy user :) */
#define FRACTION_BITS 12
   /* change FRACTION_BITS above, not this */
#define FRACTION_MASK (~(0xFFFFFFFF << FRACTION_BITS))


/* The number of samples to use for ramping out a dying note. Affects
   click removal. */
#define MAX_DIE_TIME 20


/* Define the pre-resampling cache size.
 * This value is default. You can change the cache saze with
 * command line option.
 */
#define DEFAULT_CACHE_DATA_SIZE (2*1024*1024)


/*****************************************************************************\
 section 2: some important definitions
\*****************************************************************************/
/*
  Anything below this shouldn't need to be changed unless you're porting
   to a new machine with other than 32-bit, big-endian words.
 */


/* Audio buffer size has to be a power of two to allow DMA buffer
   fragments under the VoxWare (Linux & FreeBSD) audio driver */
#define AUDIO_BUFFER_SIZE (1<<12)

/* These affect general volume */
#define GUARD_BITS 3
#define AMP_BITS (15-GUARD_BITS)

#define MAX_AMPLIFICATION 800
#define MAX_CHANNELS 32

/* Vibrato and tremolo Choices of the Day */
#define SWEEP_TUNING 38
#define VIBRATO_AMPLITUDE_TUNING 1.0L
#define VIBRATO_RATE_TUNING 38
#define TREMOLO_AMPLITUDE_TUNING 1.0L
#define TREMOLO_RATE_TUNING 38

#define SWEEP_SHIFT 16
#define RATE_SHIFT 5

#define VIBRATO_SAMPLE_INCREMENTS 32

#define MODULATION_WHEEL_RATE (1.0/6.0)
/* #define MODULATION_WHEEL_RATE (midi_time_ratio/8.0) */
/* #define MODULATION_WHEEL_RATE (current_play_tempo/500000.0/32.0) */

#define VIBRATO_DEPTH_TUNING (1.0/4.0)

/* malloc's limit */
#define MAX_SAFE_MALLOC_SIZE (1<<23) /* 8M */

#define DEFAULT_SOUNDFONT_ORDER 0


/*****************************************************************************\
 section 3: include other headers
\*****************************************************************************/

namespace TimidityPlus
{

extern std::mutex ConfigMutex;
extern int timidity_modulation_wheel;
extern int timidity_portamento;
extern int timidity_reverb;
extern int timidity_chorus;
extern int timidity_surround_chorus;
extern int timidity_channel_pressure;
extern int timidity_lpf_de;
extern int timidity_temper_control;
extern int timidity_modulation_envelope;
extern int timidity_overlap_voice_allow;
extern int timidity_drum_effect;
extern int timidity_pan_delay;
extern float timidity_drum_power;
extern int timidity_key_adjust;
extern float timidity_tempo_adjust;
extern float min_sustain_time;
extern int timidity_lpf_def;

extern int32_t playback_rate;
extern int32_t control_ratio;	// derived from playback_rate

enum play_system_modes
{
	DEFAULT_SYSTEM_MODE,
	GM_SYSTEM_MODE,
	GM2_SYSTEM_MODE,
	GS_SYSTEM_MODE,
	XG_SYSTEM_MODE
};


const int DEFAULT_VOICES = 256;

// These were configurable in Timidity++ but it doesn't look like this is really needed.
// In case it becomes necessary, they can be turned into CVARs.
const int default_tonebank = 0;
const int special_tonebank = -1;
const int effect_lr_mode = -1;
const int effect_lr_delay_msec = 25;
const int adjust_panning_immediately = 1;
const int antialiasing_allowed = 0;
const int fast_decay = 0;
const int cutoff_allowed = 0;
const int opt_force_keysig = 8;
const int max_voices = DEFAULT_VOICES;
const int temper_type_mute = 0;
const int opt_preserve_silence = 0;
const int opt_init_keysig = 8;

}
#endif /* TIMIDITY_H_INCLUDED */
