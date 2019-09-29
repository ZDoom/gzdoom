/*
** music_config.cpp
** This forwards all CVAR changes to the music system which
** was designed for any kind of configuration system.
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
#include "i_musicinterns.h"
#include "i_soundfont.h"
#include "cmdlib.h"
#include "doomerrors.h"
#include "v_text.h"
#include "c_console.h"
#include "zmusic/zmusic.h"

#include "zmusic/midiconfig.h"
#include "../libraries/timidity/timidity/timidity.h"
#include "../libraries/timidityplus/timiditypp/timidity.h"
#include "../libraries/oplsynth/oplsynth/oplio.h"
#include "../libraries/dumb/include/dumb.h"

#ifdef _WIN32
// do this without including windows.h for this one single prototype
extern "C" unsigned __stdcall GetSystemDirectoryA(char* lpBuffer, unsigned uSize);
#endif // _WIN32

static void CheckRestart(int devtype)
{
	if (currSong != nullptr && currSong->GetDeviceType() == devtype)
	{
		MIDIDeviceChanged(-1, true);
	}
}

//==========================================================================
//
// ADL Midi device
//
//==========================================================================

#define FORWARD_CVAR(key) \
	decltype(*self) newval; \
	auto ret = ChangeMusicSetting(ZMusic::key, *self, &newval); \
	self = (decltype(*self))newval; \
	if (ret) MIDIDeviceChanged(-1, true); 

#define FORWARD_BOOL_CVAR(key) \
	int newval; \
	auto ret = ChangeMusicSetting(ZMusic::key, *self, &newval); \
	self = !!newval; \
	if (ret) MIDIDeviceChanged(-1, true); 

#define FORWARD_STRING_CVAR(key) \
	auto ret = ChangeMusicSetting(ZMusic::key, *self); \
	if (ret) MIDIDeviceChanged(-1, true); 


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

CUSTOM_CVAR(Int, adl_volume_model, 3/*ADLMIDI_VolumeModel_DMX*/, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_CVAR(adl_bank);
}

//==========================================================================
//
// Fluidsynth MIDI device
//
//==========================================================================

CUSTOM_CVAR(String, fluid_lib, "", CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	FORWARD_STRING_CVAR(fluid_lib);
}

CUSTOM_CVAR(String, fluid_patchset, "gzdoom", CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
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
	if (*self <= 0)
	{
		self = 1;
	}
	else if (*self > MAXOPL2CHIPS)
	{
		self = MAXOPL2CHIPS;
	}
	else 
	{
		if (currSong != NULL)
			currSong->ChangeSettingInt("opl.numchips", self);
		oplConfig.numchips = self;
	}
}

CUSTOM_CVAR(Int, opl_core, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	CheckRestart(MDEV_OPL);
}

CUSTOM_CVAR(Bool, opl_fullpan, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	oplConfig.fullpan = self;
}

void OPL_SetupConfig(OPLConfig *config, const char *args, bool midi)
{
	// This needs to be done only once.
	if (!config->genmidiset && midi)
	{
		// The OPL renderer should not care about where this comes from.
		// Note: No I_Error here - this needs to be consistent with the rest of the music code.
		auto lump = Wads.CheckNumForName("GENMIDI", ns_global);
		if (lump < 0) throw std::runtime_error("No GENMIDI lump found");
		auto data = Wads.OpenLumpReader(lump);

		uint8_t filehdr[8];
		data.Read(filehdr, 8);
		if (memcmp(filehdr, "#OPL_II#", 8)) throw std::runtime_error("Corrupt GENMIDI lump");
		data.Read(oplConfig.OPLinstruments, 175 * 36);
		config->genmidiset = true;
	}
	
	config->core = opl_core;
	if (args != NULL && *args >= '0' && *args < '4') config->core = *args - '0';
}


//==========================================================================
//
// OPN MIDI device
//
//==========================================================================


CUSTOM_CVAR(Int, opn_chips_count, 8, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	opnConfig.opn_chips_count = self;
	CheckRestart(MDEV_OPN);
}

CUSTOM_CVAR(Int, opn_emulator_id, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	opnConfig.opn_emulator_id = self;
	CheckRestart(MDEV_OPN);

}

CUSTOM_CVAR(Bool, opn_run_at_pcm_rate, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	opnConfig.opn_run_at_pcm_rate = self;
	CheckRestart(MDEV_OPN);

}

CUSTOM_CVAR(Bool, opn_fullpan, 1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	opnConfig.opn_fullpan = self;
	CheckRestart(MDEV_OPN);

}

CUSTOM_CVAR(Bool, opn_use_custom_bank, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	CheckRestart(MDEV_OPN);

}

CUSTOM_CVAR(String, opn_custom_bank, "", CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	if (opn_use_custom_bank)
	{
		CheckRestart(MDEV_OPN);
	}
}

void OPN_SetupConfig(OpnConfig *config, const char *Args)
{
	//Resolve the path here, so that the renderer does not have to do the work itself and only needs to process final names.
	const char *bank = Args && *Args? Args : opn_use_custom_bank? *opn_custom_bank : nullptr;
	if (bank && *bank)
	{
		auto info = sfmanager.FindSoundFont(bank, SF_WOPN);
		if (info == nullptr)
		{
			config->opn_custom_bank = "";
		}
		else
		{
			config->opn_custom_bank = info->mFilename;
		}
	}
	
	int lump = Wads.CheckNumForFullName("xg.wopn");
	if (lump < 0)
	{
		config->default_bank.resize(0);
		return;
	}
	FMemLump data = Wads.ReadLump(lump);
	config->default_bank.resize(data.GetSize());
	memcpy(config->default_bank.data(), data.GetMem(), data.GetSize());
}


//==========================================================================
//
// GUS MIDI device
//
//==========================================================================


CUSTOM_CVAR(String, midi_config, "gzdoom", CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	CheckRestart(MDEV_GUS);
}

CUSTOM_CVAR(Bool, midi_dmxgus, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)	// This was 'true' but since it requires special setup that's not such a good idea.
{
	CheckRestart(MDEV_GUS);
}

CUSTOM_CVAR(String, gus_patchdir, "", CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	gusConfig.gus_patchdir = self;
	CheckRestart(MDEV_GUS);
}

CUSTOM_CVAR(Int, midi_voices, 32, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	gusConfig.midi_voices = self;
	CheckRestart(MDEV_GUS);
}

CUSTOM_CVAR(Int, gus_memsize, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	gusConfig.gus_memsize = self;
	CheckRestart(MDEV_GUS);
}


// make sure we can use the above function for the Timidity++ device as well.
static_assert(Timidity::CMSG_ERROR == TimidityPlus::CMSG_ERROR, "Timidity constant mismatch");
static_assert(Timidity::CMSG_WARNING == TimidityPlus::CMSG_WARNING, "Timidity constant mismatch");
static_assert(Timidity::CMSG_INFO == TimidityPlus::CMSG_INFO, "Timidity constant mismatch");
static_assert(Timidity::VERB_DEBUG == TimidityPlus::VERB_DEBUG, "Timidity constant mismatch");

//==========================================================================
//
// Sets up the date to load the instruments for the GUS device.
// The actual instrument loader is part of the device.
//
//==========================================================================

bool GUS_SetupConfig(GUSConfig *config, const char *args)
{
	if ((midi_dmxgus && *args == 0) || !stricmp(args, "DMXGUS"))
	{
		if (stricmp(config->loadedConfig.c_str(), "DMXGUS") == 0) return false; // aleady loaded
		int lump = Wads.CheckNumForName("DMXGUS");
		if (lump == -1) lump = Wads.CheckNumForName("DMXGUSC");
		if (lump >= 0)
		{
			auto data = Wads.OpenLumpReader(lump);
			if (data.GetLength() > 0)
			{
				config->dmxgus.resize(data.GetLength());
				data.Read(config->dmxgus.data(), data.GetLength());
				return true;
			}
		}
	}
	if (*args == 0) args = midi_config;
	if (stricmp(config->loadedConfig.c_str(), args) == 0) return false; // aleady loaded
	
	auto reader = sfmanager.OpenSoundFont(args, SF_GUS | SF_SF2);
	if (reader == nullptr)
	{
		char error[80];
		snprintf(error, 80, "GUS: %s: Unable to load sound font\n",args);
		throw std::runtime_error(error);
	}
	config->reader = reader;
	config->readerName = args;
	return true;
}


//==========================================================================
//
// CVar interface to configurable parameters
//
// Timidity++ uses a static global set of configuration variables
// THese can be changed while the synth is playing but need synchronization.
//
// Currently the synth is not fully reentrant due to this and a handful
// of other global variables.
//
//==========================================================================

/*
template<class T> void ChangeVarSync(T& var, T value)
{
	std::lock_guard<std::mutex> lock(TimidityPlus::ConfigMutex);
	var = value;
}
*/

CUSTOM_CVAR(Bool, timidity_modulation_wheel, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	////ChangeVarSync(TimidityPlus::timidity_modulation_wheel, *self);
}

CUSTOM_CVAR(Bool, timidity_portamento, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	////ChangeVarSync(TimidityPlus::timidity_portamento, *self);
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
	//ChangeVarSync(TimidityPlus::timidity_reverb, value);
}

CUSTOM_CVAR(Int, timidity_reverb, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	if (self < 0 || self > 4) self = 0;
	else SetReverb();
}

CUSTOM_CVAR(Int, timidity_reverb_level, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	if (self < 0 || self > 127) self = 0;
	else SetReverb();
}

CUSTOM_CVAR(Int, timidity_chorus, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	//ChangeVarSync(TimidityPlus::timidity_chorus, *self);
}

CUSTOM_CVAR(Bool, timidity_surround_chorus, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	//ChangeVarSync(TimidityPlus::timidity_surround_chorus, *self);
	CheckRestart(MDEV_TIMIDITY);
}

CUSTOM_CVAR(Bool, timidity_channel_pressure, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	//ChangeVarSync(TimidityPlus::timidity_channel_pressure, *self);
}

CUSTOM_CVAR(Int, timidity_lpf_def, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	//ChangeVarSync(TimidityPlus::timidity_lpf_def, *self);
}

CUSTOM_CVAR(Bool, timidity_temper_control, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	//ChangeVarSync(TimidityPlus::timidity_temper_control, *self);
}

CUSTOM_CVAR(Bool, timidity_modulation_envelope, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	//ChangeVarSync(TimidityPlus::timidity_modulation_envelope, *self);
	CheckRestart(MDEV_TIMIDITY);
}

CUSTOM_CVAR(Bool, timidity_overlap_voice_allow, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	//ChangeVarSync(TimidityPlus::timidity_overlap_voice_allow, *self);
}

CUSTOM_CVAR(Bool, timidity_drum_effect, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	//ChangeVarSync(TimidityPlus::timidity_drum_effect, *self);
}

CUSTOM_CVAR(Bool, timidity_pan_delay, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	//ChangeVarSync(TimidityPlus::timidity_pan_delay, *self);
}

CUSTOM_CVAR(Float, timidity_drum_power, 1.0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL) /* coef. of drum amplitude */
{
	if (self < 0) self = 0;
	else if (self > MAX_AMPLIFICATION / 100.f) self = MAX_AMPLIFICATION / 100.f;
	//ChangeVarSync(TimidityPlus::timidity_drum_power, *self);
}
CUSTOM_CVAR(Int, timidity_key_adjust, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	if (self < -24) self = -24;
	else if (self > 24) self = 24;
	//ChangeVarSync(TimidityPlus::timidity_key_adjust, *self);
}
// For testing mainly.
CUSTOM_CVAR(Float, timidity_tempo_adjust, 1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	if (self < 0.25) self = 0.25;
	else if (self > 10) self = 10;
	//ChangeVarSync(TimidityPlus::timidity_tempo_adjust, *self);
}

CUSTOM_CVAR(Float, min_sustain_time, 5000, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	if (self < 0) self = 0;
	//ChangeVarSync(TimidityPlus::min_sustain_time, *self);
}

// Config file to use
CUSTOM_CVAR(String, timidity_config, "gzdoom", CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	CheckRestart(MDEV_TIMIDITY);
}

bool Timidity_SetupConfig(TimidityConfig* config, const char* args)
{
	if (*args == 0) args = timidity_config;
	if (stricmp(config->loadedConfig.c_str(), args) == 0) return false; // aleady loaded

	auto reader = sfmanager.OpenSoundFont(args, SF_GUS | SF_SF2);
	if (reader == nullptr)
	{
		char error[80];
		snprintf(error, 80, "Timidity++: %s: Unable to load sound font\n", args);
		throw std::runtime_error(error);
	}
	config->reader = reader;
	config->readerName = args;
	return true;
}

//==========================================================================
//
// WildMidi
//
//==========================================================================

CUSTOM_CVAR(String, wildmidi_config, "", CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	CheckRestart(MDEV_WILDMIDI);
}

CUSTOM_CVAR(Bool, wildmidi_reverb, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL | CVAR_NOINITCALL)
{
	if (currSong != NULL)
		currSong->ChangeSettingInt("wildmidi.reverb", *self);
	wildMidiConfig.reverb = self;
}

CUSTOM_CVAR(Bool, wildmidi_enhanced_resampling, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL | CVAR_NOINITCALL)
{
	if (currSong != NULL)
		currSong->ChangeSettingInt("wildmidi.resampling", *self);
	wildMidiConfig.enhanced_resampling = self;
}


bool WildMidi_SetupConfig(WildMidiConfig* config, const char* args)
{
	if (*args == 0) args = wildmidi_config;
	if (stricmp(config->loadedConfig.c_str(), args) == 0) return false; // aleady loaded

	auto reader = sfmanager.OpenSoundFont(args, SF_GUS);
	if (reader == nullptr)
	{
		char error[80];
		snprintf(error, 80, "WildMidi: %s: Unable to load sound font\n", args);
		throw std::runtime_error(error);
	}
	config->reader = reader;
	config->readerName = args;
	config->reverb = wildmidi_reverb;
	config->enhanced_resampling = wildmidi_enhanced_resampling;
	return true;
}

//==========================================================================
//
// This one is for Win32 MMAPI.
//
//==========================================================================

CVAR(Bool, snd_midiprecache, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL);

//==========================================================================
//
// GME
//
//==========================================================================

CUSTOM_CVAR(Float, gme_stereodepth, 0.f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	if (currSong != nullptr)
		currSong->ChangeSettingNum("GME.stereodepth", *self);
}

//==========================================================================
//
// Dumb
//
//==========================================================================

CVAR(Int,  mod_samplerate,				0,	   CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL);
CVAR(Int,  mod_volramp,					2,	   CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL);
CVAR(Int,  mod_interp,					DUMB_LQ_CUBIC,	CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL);
CVAR(Bool, mod_autochip,				false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL);
CVAR(Int,  mod_autochip_size_force,		100,   CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL);
CVAR(Int,  mod_autochip_size_scan,		500,   CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL);
CVAR(Int,  mod_autochip_scan_threshold, 12,	   CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL);
CUSTOM_CVAR(Float, mod_dumb_mastervolume, 1.f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	if (self < 0.5f) self = 0.5f;
	else if (self > 16.f) self = 16.f;
}


void Dumb_SetupConfig(DumbConfig* config)
{
	config->mod_samplerate					= mod_samplerate;			
    config->mod_volramp                     = mod_volramp;            
    config->mod_interp                      = mod_interp;               
    config->mod_autochip                    = mod_autochip;
    config->mod_autochip_size_force         = mod_autochip_size_force;
    config->mod_autochip_size_scan          = mod_autochip_size_scan;
    config->mod_autochip_scan_threshold     = mod_autochip_scan_threshold;
    config->mod_dumb_mastervolume           = mod_dumb_mastervolume;
}

//==========================================================================
//
// sndfile
//
//==========================================================================

CUSTOM_CVAR(Int, snd_streambuffersize, 64, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_VIRTUAL)
{
	if (self < 16)
	{
		self = 16;
	}
	else if (self > 1024)
	{
		self = 1024;
	}
}

