/*
** configuration.cpp
** Handle zmusic's configuration.
**
**---------------------------------------------------------------------------
** Copyright 2019 Christoph Oelckers
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
#include <algorithm>
#include "timidity/timidity.h"
#include "timiditypp/timidity.h"
#include "oplsynth/oplio.h"
#include "../../libraries/dumb/include/dumb.h"

#include "zmusic.h"
#include "musinfo.h"
#include "midiconfig.h"

struct Dummy
{
	void ChangeSettingInt(const char*, int) {}
	void ChangeSettingNum(const char*, double) {}
	void ChangeSettingString(const char*, const char*) {}
};

#define devType() ((currSong)? (currSong)->GetDeviceType() : MDEV_DEFAULT)


MiscConfig miscConfig;
Callbacks musicCallbacks;

void ZMusic_SetCallbacks(const Callbacks* cb)
{
	dumb_decode_vorbis = cb->DumbVorbisDecode;
	musicCallbacks = *cb;
}

void ZMusic_SetGenMidi(const uint8_t* data)
{
	memcpy(oplConfig.OPLinstruments, data, 175 * 36);
	oplConfig.genmidiset = true;
}

void ZMusic_SetWgOpn(const void* data, unsigned len)
{
	opnConfig.default_bank.resize(len);
	memcpy(opnConfig.default_bank.data(), data, len);
}

void ZMusic_SetDmxGus(const void* data, unsigned len)
{
	gusConfig.dmxgus.resize(len);
	memcpy(gusConfig.dmxgus.data(), data, len);
}

template<class valtype>
void ChangeAndReturn(valtype &variable, valtype value, valtype *realv)
{
	variable = value;
	if (realv) *realv = value;
}

#define FLUID_CHORUS_MOD_SINE		0
#define FLUID_CHORUS_MOD_TRIANGLE	1
#define FLUID_CHORUS_DEFAULT_TYPE FLUID_CHORUS_MOD_SINE

//==========================================================================
//
// Timidity++ uses a static global set of configuration variables.
// THese can be changed live while the synth is playing but need synchronization.
//
// Currently the synth is not reentrant due to this and a handful
// of other global variables.
//
//==========================================================================

template<class T> void ChangeVarSync(T& var, T value)
{
	std::lock_guard<std::mutex> lock(TimidityPlus::ConfigMutex);
	var = value;
}

//==========================================================================
//
// Timidity++ reverb is a bit more complicated because it is two properties in one value.
//
//==========================================================================

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
static int local_timidity_reverb_level;
static int local_timidity_reverb;

static void TimidityPlus_SetReverb()
{
	int value = 0;
	int mode = local_timidity_reverb;
	int level = local_timidity_reverb_level;

	if (mode == 0 || level == 0) value = mode;
	else value = (mode - 1) * -128 - level;
	ChangeVarSync(TimidityPlus::timidity_reverb, value);
}


using namespace ZMusic;
//==========================================================================
//
// change an integer value
//
//==========================================================================

bool ChangeMusicSetting(ZMusic::EIntConfigKey key, MusInfo *currSong, int value, int *pRealValue)
{
	switch (key)
	{
		default:
			return false;
			
		case adl_chips_count: 
			ChangeAndReturn(adlConfig.adl_chips_count, value, pRealValue);
			return devType() == MDEV_ADL;

		case adl_emulator_id: 
			ChangeAndReturn(adlConfig.adl_emulator_id, value, pRealValue);
			return devType() == MDEV_ADL;

		case adl_run_at_pcm_rate: 
			ChangeAndReturn(adlConfig.adl_run_at_pcm_rate, value, pRealValue);
			return devType() == MDEV_ADL;

		case adl_fullpan: 
			ChangeAndReturn(adlConfig.adl_fullpan, value, pRealValue);
			return devType() == MDEV_ADL;

		case adl_bank: 
			ChangeAndReturn(adlConfig.adl_bank, value, pRealValue);
			return devType() == MDEV_ADL;

		case adl_use_custom_bank: 
			ChangeAndReturn(adlConfig.adl_use_custom_bank, value, pRealValue);
			return devType() == MDEV_ADL;

		case adl_volume_model: 
			ChangeAndReturn(adlConfig.adl_volume_model, value, pRealValue);
			return devType() == MDEV_ADL;

		case fluid_reverb: 
			if (currSong != NULL)
				currSong->ChangeSettingInt("fluidsynth.synth.reverb.active", value);

			ChangeAndReturn(fluidConfig.fluid_reverb, value, pRealValue);
			return false;

		case fluid_chorus: 
			if (currSong != NULL)
				currSong->ChangeSettingInt("fluidsynth.synth.chorus.active", value);

			ChangeAndReturn(fluidConfig.fluid_chorus, value, pRealValue);
			return false;

		case fluid_voices: 
			if (value < 16)
				value = 16;
			else if (value > 4096)
				value = 4096;
		
			if (currSong != NULL)
				currSong->ChangeSettingInt("fluidsynth.synth.polyphony", value);

			ChangeAndReturn(fluidConfig.fluid_voices, value, pRealValue);
			return false;
			
		case fluid_interp:
			// Values are: 0 = FLUID_INTERP_NONE
			//             1 = FLUID_INTERP_LINEAR
			//             4 = FLUID_INTERP_4THORDER (the FluidSynth default)
			//             7 = FLUID_INTERP_7THORDER
			// (And here I thought it was just a linear list.)
			// Round undefined values to the nearest valid one.
			if (value < 0)
				value = 0;
			else if (value == 2)
				value = 1;
			else if (value == 3 || value == 5)
				value = 4;
			else if (value == 6 || value > 7)
				value = 7;

			if (currSong != NULL)
				currSong->ChangeSettingInt("fluidsynth.synth.interpolation", value);

			ChangeAndReturn(fluidConfig.fluid_interp, value, pRealValue);
			return false;

		case fluid_samplerate:
			// This will only take effect for the next song. (Q: Is this even needed?)
			ChangeAndReturn(fluidConfig.fluid_samplerate, std::max<int>(value, 0), pRealValue);
			return false;

		// I don't know if this setting even matters for us, since we aren't letting
		// FluidSynth drives its own output.
		case fluid_threads:
			if (value < 1)
				value = 1;
			else if (value > 256)
				value = 256;

			ChangeAndReturn(fluidConfig.fluid_threads, value, pRealValue);
			return false;
			
		case fluid_chorus_voices:
			if (value < 0)
				value = 0;
			else if (value > 99)
				value = 99;

			if (currSong != NULL)
				currSong->ChangeSettingNum("fluidsynth.z.chorus", value);

			ChangeAndReturn(fluidConfig.fluid_chorus_voices, value, pRealValue);
			return false;
			
		case fluid_chorus_type:
			if (value != FLUID_CHORUS_MOD_SINE && value != FLUID_CHORUS_MOD_TRIANGLE)
				value = FLUID_CHORUS_DEFAULT_TYPE;
	
			if (currSong != NULL)
				currSong->ChangeSettingNum("fluidsynth.z.chorus", value); // Uses float to simplify the checking code in the renderer.

			ChangeAndReturn(fluidConfig.fluid_chorus_type, value, pRealValue);
			return false;
			
		case opl_numchips:
			if (value <= 0)
				value = 1;
			else if (value > MAXOPL2CHIPS)
				value = MAXOPL2CHIPS;

			if (currSong != NULL)
				currSong->ChangeSettingInt("opl.numchips", value);

			ChangeAndReturn(oplConfig.numchips, value, pRealValue);
			return false;

		case opl_core:
			if (value < 0) value = 0;
			else if (value > 3) value = 3;
			ChangeAndReturn(oplConfig.core, value, pRealValue);
			return devType() == MDEV_OPL;

		case opl_fullpan:
			ChangeAndReturn(oplConfig.fullpan, value, pRealValue);
			return false;

		case opn_chips_count:
			ChangeAndReturn(opnConfig.opn_chips_count, value, pRealValue);
			return devType() == MDEV_OPN;

		case opn_emulator_id:
			ChangeAndReturn(opnConfig.opn_emulator_id, value, pRealValue);
			return devType() == MDEV_OPN;

		case opn_run_at_pcm_rate:
			ChangeAndReturn(opnConfig.opn_run_at_pcm_rate, value, pRealValue);
			return devType() == MDEV_OPN;

		case opn_fullpan:
			ChangeAndReturn(opnConfig.opn_fullpan, value, pRealValue);
			return devType() == MDEV_OPN;

		case opn_use_custom_bank:
			ChangeAndReturn(opnConfig.opn_use_custom_bank, value, pRealValue);
			return devType() == MDEV_OPN;

		case gus_dmxgus:
			ChangeAndReturn(gusConfig.gus_dmxgus, value, pRealValue);
			return devType() == MDEV_GUS;

		case gus_midi_voices:
			ChangeAndReturn(gusConfig.midi_voices, value, pRealValue);
			return devType() == MDEV_GUS;
		
		case gus_memsize:
			ChangeAndReturn(gusConfig.gus_memsize, value, pRealValue);
			return devType() == MDEV_GUS && gusConfig.gus_dmxgus;
			
		case timidity_modulation_wheel:
			ChangeVarSync(TimidityPlus::timidity_modulation_wheel, value);
			if (pRealValue) *pRealValue = value;
			return false;

		case timidity_portamento:
			ChangeVarSync(TimidityPlus::timidity_portamento, value);
			if (pRealValue) *pRealValue = value;
			return false;

		case timidity_reverb:
			if (value < 0 || value > 4) value = 0;
			else TimidityPlus_SetReverb();
			local_timidity_reverb = value;
			if (pRealValue) *pRealValue = value;
			return false;

		case timidity_reverb_level:
			if (value < 0 || value > 127) value = 0;
			else TimidityPlus_SetReverb();
			local_timidity_reverb_level = value;
			if (pRealValue) *pRealValue = value;
			return false;

		case timidity_chorus:
			ChangeVarSync(TimidityPlus::timidity_chorus, value);
			if (pRealValue) *pRealValue = value;
			return false;

		case timidity_surround_chorus:
			ChangeVarSync(TimidityPlus::timidity_surround_chorus, value);
			if (pRealValue) *pRealValue = value;
			return devType() == MDEV_TIMIDITY;

		case timidity_channel_pressure:
			ChangeVarSync(TimidityPlus::timidity_channel_pressure, value);
			if (pRealValue) *pRealValue = value;
			return false;

		case timidity_lpf_def:
			ChangeVarSync(TimidityPlus::timidity_lpf_def, value);
			if (pRealValue) *pRealValue = value;
			return false;

		case timidity_temper_control:
			ChangeVarSync(TimidityPlus::timidity_temper_control, value);
			if (pRealValue) *pRealValue = value;
			return false;

		case timidity_modulation_envelope:
			ChangeVarSync(TimidityPlus::timidity_modulation_envelope, value);
			if (pRealValue) *pRealValue = value;
			return devType() == MDEV_TIMIDITY;

		case timidity_overlap_voice_allow:
			ChangeVarSync(TimidityPlus::timidity_overlap_voice_allow, value);
			if (pRealValue) *pRealValue = value;
			return false;

		case timidity_drum_effect:
			ChangeVarSync(TimidityPlus::timidity_drum_effect, value);
			if (pRealValue) *pRealValue = value;
			return false;

		case timidity_pan_delay:
			ChangeVarSync(TimidityPlus::timidity_pan_delay, value);
			if (pRealValue) *pRealValue = value;
			return false;

		case timidity_key_adjust:
			if (value < -24) value = -24;
			else if (value > 24) value = 24;
			ChangeVarSync(TimidityPlus::timidity_key_adjust, value);
			if (pRealValue) *pRealValue = value;
			return false;
			
		case wildmidi_reverb:
			if (currSong != NULL)
				currSong->ChangeSettingInt("wildmidi.reverb", value);
			wildMidiConfig.reverb = value;
			if (pRealValue) *pRealValue = value;
			return false;

		case wildmidi_enhanced_resampling:
			if (currSong != NULL)
				currSong->ChangeSettingInt("wildmidi.resampling", value);
			wildMidiConfig.enhanced_resampling = value;
			if (pRealValue) *pRealValue = value;
			return false;

		case snd_midiprecache:
			ChangeAndReturn(miscConfig.snd_midiprecache, value, pRealValue);
			return false;

		case snd_streambuffersize:
			if (value < 16)
			{
				value = 16;
			}
			else if (value > 1024)
			{
				value = 1024;
			}
			ChangeAndReturn(miscConfig.snd_streambuffersize, value, pRealValue);
			return false;

		case mod_samplerate:
			ChangeAndReturn(dumbConfig.mod_samplerate, value, pRealValue);
			return false;

		case mod_volramp:
			ChangeAndReturn(dumbConfig.mod_volramp, value, pRealValue);
			return false;

		case mod_interp:
			ChangeAndReturn(dumbConfig.mod_interp, value, pRealValue);
			return false;

		case mod_autochip:
			ChangeAndReturn(dumbConfig.mod_autochip, value, pRealValue);
			return false;

		case mod_autochip_size_force:
			ChangeAndReturn(dumbConfig.mod_autochip_size_force, value, pRealValue);
			return false;

		case mod_autochip_size_scan:
			ChangeAndReturn(dumbConfig.mod_autochip_size_scan, value, pRealValue);
			return false;

		case mod_autochip_scan_threshold:
			ChangeAndReturn(dumbConfig.mod_autochip_scan_threshold, value, pRealValue);
			return false;

		case snd_mididevice:
		{
			bool change = miscConfig.snd_mididevice != value;
			miscConfig.snd_mididevice = value;
			return change;
		}

		case snd_outputrate:
			miscConfig.snd_outputrate = value;
			return false;

	}
	return false;
}

bool ChangeMusicSetting(ZMusic::EFloatConfigKey key, MusInfo* currSong, float value, float *pRealValue)
{
	switch (key)
	{
		default:
			return false;
			
		case fluid_gain: 
			if (value < 0)
				value = 0;
			else if (value > 10)
				value = 10;
		
			if (currSong != NULL)
				currSong->ChangeSettingNum("fluidsynth.synth.gain", value);
		
			ChangeAndReturn(fluidConfig.fluid_gain, value, pRealValue);
			return false;

		case fluid_reverb_roomsize:
			if (value < 0)
				value = 0;
			else if (value > 1.2f)
				value = 1.2f;

			if (currSong != NULL)
				currSong->ChangeSettingNum("fluidsynth.z.reverb", value);

			ChangeAndReturn(fluidConfig.fluid_reverb_roomsize, value, pRealValue);
			return false;

		case fluid_reverb_damping:
			if (value < 0)
				value = 0;
			else if (value > 1)
				value = 1;

			if (currSong != NULL)
				currSong->ChangeSettingNum("fluidsynth.z.reverb", value);

			ChangeAndReturn(fluidConfig.fluid_reverb_damping, value, pRealValue);
			return false;

		case fluid_reverb_width:
			if (value < 0)
				value = 0;
			else if (value > 100)
				value = 100;

			if (currSong != NULL)
				currSong->ChangeSettingNum("fluidsynth.z.reverb", value);

			ChangeAndReturn(fluidConfig.fluid_reverb_width, value, pRealValue);
			return false;

		case fluid_reverb_level:
			if (value < 0)
				value = 0;
			else if (value > 1)
				value = 1;
		
			if (currSong != NULL)
				currSong->ChangeSettingNum("fluidsynth.z.reverb", value);

			ChangeAndReturn(fluidConfig.fluid_reverb_level, value, pRealValue);
			return false;

		case fluid_chorus_level:
			if (value < 0)
				value = 0;
			else if (value > 1)
				value = 1;

			if (currSong != NULL)
				currSong->ChangeSettingNum("fluidsynth.z.chorus", value);

			ChangeAndReturn(fluidConfig.fluid_chorus_level, value, pRealValue);
			return false;

		case fluid_chorus_speed:
			if (value < 0.29f)
				value = 0.29f;
			else if (value > 5)
				value = 5;

			if (currSong != NULL)
				currSong->ChangeSettingNum("fluidsynth.z.chorus", value);

			ChangeAndReturn(fluidConfig.fluid_chorus_speed, value, pRealValue);
			return false;

		// depth is in ms and actual maximum depends on the sample rate
		case fluid_chorus_depth:
			if (value < 0)
				value = 0;
			else if (value > 21)
				value = 21;

			if (currSong != NULL)
				currSong->ChangeSettingNum("fluidsynth.z.chorus", value);

			ChangeAndReturn(fluidConfig.fluid_chorus_depth, value, pRealValue);
			return false;

		case timidity_drum_power:
			if (value < 0) value = 0;
			else if (value > MAX_AMPLIFICATION / 100.f) value = MAX_AMPLIFICATION / 100.f;
			ChangeVarSync(TimidityPlus::timidity_drum_power, value);
			if (pRealValue) *pRealValue = value;
			return false;

		// For testing mainly.
		case timidity_tempo_adjust:
			if (value < 0.25) value = 0.25;
			else if (value > 10) value = 10;
			ChangeVarSync(TimidityPlus::timidity_tempo_adjust, value);
			if (pRealValue) *pRealValue = value;
			return false;

		case min_sustain_time:
			if (value < 0) value = 0;
			ChangeVarSync(TimidityPlus::min_sustain_time, value);
			if (pRealValue) *pRealValue = value;
			return false;

		case gme_stereodepth:
			if (currSong != nullptr)
				currSong->ChangeSettingNum("GME.stereodepth", value);
			ChangeAndReturn(miscConfig.gme_stereodepth, value, pRealValue);
			return false;

		case mod_dumb_mastervolume:
			if (value < 0) value = 0;
			ChangeAndReturn(dumbConfig.mod_dumb_mastervolume, value, pRealValue);
			return false;

		case snd_musicvolume:
			miscConfig.snd_musicvolume = value;
			return false;

		case relative_volume:
			miscConfig.relative_volume = value;
			return false;

		case snd_mastervolume:
			miscConfig.snd_mastervolume = value;
			return false;

	}
	return false;
}

bool ChangeMusicSetting(ZMusic::EStringConfigKey key, MusInfo* currSong, const char *value)
{
	switch (key)
	{
		default:
			return false;
			
		case adl_custom_bank: 
			adlConfig.adl_custom_bank = value;
			return devType() == MDEV_ADL;

		case fluid_lib: 
			fluidConfig.fluid_lib = value;
			return false; // only takes effect for next song.

		case fluid_patchset: 
			fluidConfig.fluid_patchset = value;
			return devType() == MDEV_FLUIDSYNTH;

		case opn_custom_bank: 
			opnConfig.opn_custom_bank = value;
			return devType() == MDEV_OPN && opnConfig.opn_use_custom_bank;

		case gus_config:
			gusConfig.gus_config = value;
			return devType() == MDEV_GUS;
			
		case gus_patchdir:
			gusConfig.gus_patchdir = value;
			return devType() == MDEV_GUS && gusConfig.gus_dmxgus;

		case timidity_config:
			timidityConfig.timidity_config = value;
			return devType() == MDEV_TIMIDITY;

		case wildmidi_config:
			wildMidiConfig.config = value;
			return devType() == MDEV_TIMIDITY;
			
	}
	return false;
}

