/*
** music_config.cpp
** This forwards all CVAR changes to the music system.
**
**---------------------------------------------------------------------------
** Copyright 1999-2016 Randy Heit
** Copyright 2005-2019 Christoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include <stdint.h>
#include <mutex>
#include <string>
#include "c_cvars.h"
#include "s_music.h"
#include "version.h"
#include <zmusic.h>

//==========================================================================
//
// ADL Midi device
//
//==========================================================================

#define FORWARD_CVAR(key) \
	decltype(*self) newval; \
	auto ret = ChangeMusicSetting(zmusic_##key, mus_playing.handle, *self, &newval); \
	self = (decltype(*self))newval; \
	if (ret) S_MIDIDeviceChanged(-1);

#define FORWARD_BOOL_CVAR(key) \
	int newval; \
	auto ret = ChangeMusicSetting(zmusic_##key, mus_playing.handle,*self, &newval); \
	self = !!newval; \
	if (ret) S_MIDIDeviceChanged(-1);

#define FORWARD_STRING_CVAR(key) \
	auto ret = ChangeMusicSetting(zmusic_##key, mus_playing.handle,*self); \
	if (ret) S_MIDIDeviceChanged(-1); 

#ifndef ZMUSIC_LITE
CUSTOM_CVAR(Int, adl_chips_count, 6, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_CVAR(adl_chips_count);
}

CUSTOM_CVAR(Int, adl_emulator_id, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_CVAR(adl_emulator_id);
}

CUSTOM_CVAR(Bool, adl_run_at_pcm_rate, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_BOOL_CVAR(adl_run_at_pcm_rate);
}

CUSTOM_CVAR(Bool, adl_fullpan, 1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_BOOL_CVAR(adl_fullpan);
}

CUSTOM_CVAR(Int, adl_bank, 14, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_CVAR(adl_bank);
}

CUSTOM_CVAR(Bool, adl_use_custom_bank, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_BOOL_CVAR(adl_use_custom_bank);
}

CUSTOM_CVAR(String, adl_custom_bank, "", CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_STRING_CVAR(adl_custom_bank);
}

CUSTOM_CVAR(Int, adl_volume_model, 0 /*ADLMIDI_VolumeModel_AUTO*/, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_CVAR(adl_volume_model);
}
#endif
//==========================================================================
//
// Fluidsynth MIDI device
//
//==========================================================================

CUSTOM_CVAR(String, fluid_lib, "", CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_STRING_CVAR(fluid_lib);
}

CUSTOM_CVAR(String, fluid_patchset, GAMENAMELOWERCASE, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_STRING_CVAR(fluid_patchset);
}

CUSTOM_CVAR(Float, fluid_gain, 0.5, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_CVAR(fluid_gain);
}

CUSTOM_CVAR(Bool, fluid_reverb, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_BOOL_CVAR(fluid_reverb);
}

CUSTOM_CVAR(Bool, fluid_chorus, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_BOOL_CVAR(fluid_chorus);
}

CUSTOM_CVAR(Int, fluid_voices, 128, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_CVAR(fluid_voices);
}

CUSTOM_CVAR(Int, fluid_interp, 1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_CVAR(fluid_interp);
}

CUSTOM_CVAR(Int, fluid_samplerate, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_CVAR(fluid_samplerate);
}

CUSTOM_CVAR(Int, fluid_threads, 1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_CVAR(fluid_threads);
}

CUSTOM_CVAR(Float, fluid_reverb_roomsize, 0.61f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_CVAR(fluid_reverb_roomsize);
}

CUSTOM_CVAR(Float, fluid_reverb_damping, 0.23f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_CVAR(fluid_reverb_damping);
}

CUSTOM_CVAR(Float, fluid_reverb_width, 0.76f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_CVAR(fluid_reverb_width);
}

CUSTOM_CVAR(Float, fluid_reverb_level, 0.57f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_CVAR(fluid_reverb_level);
}

CUSTOM_CVAR(Int, fluid_chorus_voices, 3, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_CVAR(fluid_chorus_voices);
}

CUSTOM_CVAR(Float, fluid_chorus_level, 1.2f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_CVAR(fluid_chorus_level);
}

CUSTOM_CVAR(Float, fluid_chorus_speed, 0.3f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_CVAR(fluid_chorus_speed);
}

// depth is in ms and actual maximum depends on the sample rate
CUSTOM_CVAR(Float, fluid_chorus_depth, 8, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_CVAR(fluid_chorus_depth);
}

CUSTOM_CVAR(Int, fluid_chorus_type, 0/*FLUID_CHORUS_DEFAULT_TYPE*/, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_CVAR(fluid_chorus_type);
}


//==========================================================================
//
// OPL MIDI device
//
//==========================================================================

CUSTOM_CVAR(Int, opl_numchips, 2, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_CVAR(opl_numchips);
}

CUSTOM_CVAR(Int, opl_core, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_CVAR(opl_core);
}

CUSTOM_CVAR(Bool, opl_fullpan, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_BOOL_CVAR(opl_fullpan);
}

#ifndef ZMUSIC_LITE
//==========================================================================
//
// OPN MIDI device
//
//==========================================================================


CUSTOM_CVAR(Int, opn_chips_count, 8, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_CVAR(opn_chips_count);
}

CUSTOM_CVAR(Int, opn_emulator_id, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_CVAR(opn_emulator_id);
}

CUSTOM_CVAR(Bool, opn_run_at_pcm_rate, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_BOOL_CVAR(opn_run_at_pcm_rate);
}

CUSTOM_CVAR(Bool, opn_fullpan, 1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_BOOL_CVAR(opn_fullpan);
}

CUSTOM_CVAR(Bool, opn_use_custom_bank, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_BOOL_CVAR(opn_use_custom_bank);
}

CUSTOM_CVAR(String, opn_custom_bank, "", CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_STRING_CVAR(opn_custom_bank);
}

//==========================================================================
//
// GUS MIDI device
//
//==========================================================================


CUSTOM_CVAR(String, midi_config, GAMENAMELOWERCASE, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_STRING_CVAR(gus_config);
}

CUSTOM_CVAR(Bool, midi_dmxgus, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)	// This was 'true' but since it requires special setup that's not such a good idea.
{
	FORWARD_BOOL_CVAR(gus_dmxgus);
}

CUSTOM_CVAR(String, gus_patchdir, "", CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_STRING_CVAR(gus_patchdir);
}

CUSTOM_CVAR(Int, midi_voices, 32, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_CVAR(gus_midi_voices);
}

CUSTOM_CVAR(Int, gus_memsize, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_CVAR(gus_memsize);
}

//==========================================================================
//
// Timidity++ device
//
//==========================================================================

CUSTOM_CVAR(Bool, timidity_modulation_wheel, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_BOOL_CVAR(timidity_modulation_wheel);
}

CUSTOM_CVAR(Bool, timidity_portamento, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_BOOL_CVAR(timidity_portamento);
}

CUSTOM_CVAR(Int, timidity_reverb, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_CVAR(timidity_reverb);
}

CUSTOM_CVAR(Int, timidity_reverb_level, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_CVAR(timidity_reverb_level);
}

CUSTOM_CVAR(Int, timidity_chorus, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_CVAR(timidity_chorus);
}

CUSTOM_CVAR(Bool, timidity_surround_chorus, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_BOOL_CVAR(timidity_surround_chorus);
}

CUSTOM_CVAR(Bool, timidity_channel_pressure, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_BOOL_CVAR(timidity_channel_pressure);
}

CUSTOM_CVAR(Int, timidity_lpf_def, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_CVAR(timidity_lpf_def);
}

CUSTOM_CVAR(Bool, timidity_temper_control, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_BOOL_CVAR(timidity_temper_control);
}

CUSTOM_CVAR(Bool, timidity_modulation_envelope, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_BOOL_CVAR(timidity_modulation_envelope);
}

CUSTOM_CVAR(Bool, timidity_overlap_voice_allow, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_BOOL_CVAR(timidity_overlap_voice_allow);
}

CUSTOM_CVAR(Bool, timidity_drum_effect, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_BOOL_CVAR(timidity_drum_effect);
}

CUSTOM_CVAR(Bool, timidity_pan_delay, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_BOOL_CVAR(timidity_pan_delay);
}

CUSTOM_CVAR(Float, timidity_drum_power, 1.0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL) /* coef. of drum amplitude */
{
	FORWARD_CVAR(timidity_drum_power);
}

CUSTOM_CVAR(Int, timidity_key_adjust, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_CVAR(timidity_key_adjust);
}

CUSTOM_CVAR(Float, timidity_tempo_adjust, 1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_CVAR(timidity_tempo_adjust);
}

CUSTOM_CVAR(Float, timidity_min_sustain_time, 5000, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_CVAR(timidity_min_sustain_time);
}
#endif

CUSTOM_CVAR(String, timidity_config, GAMENAMELOWERCASE, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_STRING_CVAR(timidity_config);
}
#ifndef ZMUSIC_LITE
//==========================================================================
//
// WildMidi
//
//==========================================================================

CUSTOM_CVAR(String, wildmidi_config, "", CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_STRING_CVAR(wildmidi_config);
}

CUSTOM_CVAR(Bool, wildmidi_reverb, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_BOOL_CVAR(wildmidi_reverb);
}

CUSTOM_CVAR(Bool, wildmidi_enhanced_resampling, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_BOOL_CVAR(wildmidi_enhanced_resampling);
}
#endif

//==========================================================================
//
// This one is for Win32 MMAPI.
//
//==========================================================================

CUSTOM_CVAR(Bool, snd_midiprecache, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_BOOL_CVAR(snd_midiprecache);
}

//==========================================================================
//
// GME
//
//==========================================================================

CUSTOM_CVAR(Float, gme_stereodepth, 0.f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_CVAR(gme_stereodepth);
}

//==========================================================================
//
// sndfile
//
//==========================================================================

CUSTOM_CVAR(Int, snd_streambuffersize, 64, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_CVAR(snd_streambuffersize);
}

//==========================================================================
//
// Dumb
//
//==========================================================================

CUSTOM_CVAR(Int,  mod_samplerate,				0,	   CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	if (self != 0 && self != 11025 && self != 22050 && self != 44100 && self != 48000)
	{
		self = 0;
		return;
	}
	FORWARD_CVAR(mod_samplerate);
}

CUSTOM_CVAR(Int,  mod_volramp,					2,	   CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_CVAR(mod_volramp);
}

CUSTOM_CVAR(Int,  mod_interp,					2/*DUMB_LQ_CUBIC*/,	CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_CVAR(mod_interp);
}

CUSTOM_CVAR(Bool, mod_autochip,				false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_BOOL_CVAR(mod_autochip);
}

CUSTOM_CVAR(Int,  mod_autochip_size_force,		100,   CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_CVAR(mod_autochip_size_force);
}

CUSTOM_CVAR(Int,  mod_autochip_size_scan,		500,   CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_CVAR(mod_autochip_size_scan);
}

CUSTOM_CVAR(Int,  mod_autochip_scan_threshold, 12,	   CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_CVAR(mod_autochip_scan_threshold);
}

CUSTOM_CVAR(Float, mod_dumb_mastervolume, 1.f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_CVAR(mod_dumb_mastervolume);
}

