/*
** music_config.cpp
** All game side configuration settings for the various parts of the
** music system
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
#include "adlmidi.h"
#include "cmdlib.h"
#include "doomerrors.h"
#include "timidity/timidity.h"
#include "timidity/playmidi.h"
#include "timidity/instrum.h"
#include "timiditypp/controls.h"
#include "timiditypp/timidity.h"
#include "timiditypp/instrum.h"
#include "v_text.h"

// do this without including windows.h for this one single prototype
#ifdef _WIN32
extern "C" unsigned __stdcall GetSystemDirectoryA(char* lpBuffer, unsigned uSize);
#endif

static void CheckRestart(int devtype)
{
	if (currSong != nullptr && currSong->GetDeviceType() == devtype)
	{
		MIDIDeviceChanged(-1, true);
	}
}

ADLConfig adlConfig;
FluidConfig fluidConfig;
OPLMidiConfig oplMidiConfig;
OpnConfig opnConfig;
GUSConfig gusConfig;
TimidityConfig timidityConfig;

//==========================================================================
//
// ADL Midi device
//
//==========================================================================

CUSTOM_CVAR(Int, adl_chips_count, 6, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	adlConfig.adl_chips_count = self;
	CheckRestart(MDEV_ADL);
}

CUSTOM_CVAR(Int, adl_emulator_id, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	adlConfig.adl_emulator_id = self;
	CheckRestart(MDEV_ADL);
}

CUSTOM_CVAR(Bool, adl_run_at_pcm_rate, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	adlConfig.adl_run_at_pcm_rate = self;
	CheckRestart(MDEV_ADL);
}

CUSTOM_CVAR(Bool, adl_fullpan, 1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	adlConfig.adl_fullpan = self;
	CheckRestart(MDEV_ADL);
}

CUSTOM_CVAR(Int, adl_bank, 14, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	adlConfig.adl_bank = self;
	CheckRestart(MDEV_ADL);
}

CUSTOM_CVAR(Bool, adl_use_custom_bank, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	CheckRestart(MDEV_ADL);
}

CUSTOM_CVAR(String, adl_custom_bank, "", CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (adl_use_custom_bank) CheckRestart(MDEV_ADL);
}

void ADL_SetupConfig(ADLConfig *config, const char *Args)
{
	//Resolve the path here, so that the renderer does not have to do the work itself and only needs to process final names.
	const char *bank = Args && *Args? Args : adl_use_custom_bank? *adl_custom_bank : nullptr;
	config->adl_bank = adl_bank;
	if (bank && *bank)
	{
		auto info = sfmanager.FindSoundFont(bank, SF_WOPL);
		if (info == nullptr)
		{
			if (*bank >= '0' && *bank <= '9')
			{
				config->adl_bank = (int)strtoll(bank, nullptr, 10);
			}
			config->adl_custom_bank = nullptr;
		}
		else
		{
			config->adl_custom_bank = info->mFilename;
		}
	}
}


CUSTOM_CVAR(Int, adl_volume_model, ADLMIDI_VolumeModel_DMX, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	adlConfig.adl_volume_model = self;
	CheckRestart(MDEV_ADL);
}

//==========================================================================
//
// Fluidsynth MIDI device
//
//==========================================================================


#define FLUID_CHORUS_MOD_SINE		0
#define FLUID_CHORUS_MOD_TRIANGLE	1
#define FLUID_CHORUS_DEFAULT_TYPE FLUID_CHORUS_MOD_SINE


CUSTOM_CVAR(String, fluid_lib, "", CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	fluidConfig.fluid_lib = self;	// only takes effect for next song.
}

void Fluid_SetupConfig(FluidConfig *config, const char* patches, bool systemfallback)
{
	fluidConfig.fluid_patchset.clear();
	//Resolve the paths here, the renderer will only get a final list of file names.
	auto info = sfmanager.FindSoundFont(patches, SF_SF2);
	if (info != nullptr) patches = info->mFilename.GetChars();
	
	int count;
	char* wpatches = strdup(patches);
	char* tok;
#ifdef _WIN32
	const char* const delim = ";";
#else
	const char* const delim = ":";
#endif
	
	if (wpatches != NULL)
	{
		tok = strtok(wpatches, delim);
		count = 0;
		while (tok != NULL)
		{
			FString path;
#ifdef _WIN32
			// If the path does not contain any path separators, automatically
			// prepend $PROGDIR to the path.
			if (strcspn(tok, ":/\\") == strlen(tok))
			{
				path << "$PROGDIR/" << tok;
				path = NicePath(path);
			}
			else
#endif
			{
				path = NicePath(tok);
			}
			if (FileExists(path))
			{
				config->fluid_patchset.push_back(path.GetChars());
			}
			else
			{
				Printf("Could not find patch set %s.\n", tok);
			}
			tok = strtok(NULL, delim);
		}
		free(wpatches);
		if (config->fluid_patchset.size() > 0) return;
	}

	if (systemfallback)
	{
		// The following will only be used if no soundfont at all is provided, i.e. even the standard one coming with GZDoom is missing.
#ifdef __unix__
		// This is the standard location on Ubuntu.
		Fluid_SetupConfig(config, "/usr/share/sounds/sf2/FluidR3_GS.sf2:/usr/share/sounds/sf2/FluidR3_GM.sf2", false);
#endif
#ifdef _WIN32
		// On Windows, look for the 4 megabyte patch set installed by Creative's drivers as a default.
		char sysdir[PATH_MAX + sizeof("\\CT4MGM.SF2")];
		uint32_t filepart;
		if (0 != (filepart = GetSystemDirectoryA(sysdir, PATH_MAX)))
		{
			strcat(sysdir, "\\CT4MGM.SF2");
			if (FileExists(sysdir))
			{
				config->fluid_patchset.push_back(sysdir);
				return;
			}
			// Try again with CT2MGM.SF2
			sysdir[filepart + 3] = '2';
			if (FileExists(sysdir))
			{
				config->fluid_patchset.push_back(sysdir);
				return;
			}
		}

#endif

	}
}

CUSTOM_CVAR(String, fluid_patchset, "gzdoom", CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	CheckRestart(MDEV_FLUIDSYNTH);
}

CUSTOM_CVAR(Float, fluid_gain, 0.5, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 0)
		self = 0;
	else if (self > 10)
		self = 10;
	else 
	{
		if (currSong != NULL)
			currSong->ChangeSettingNum("fluidsynth.synth.gain", self);
		fluidConfig.fluid_gain = self;
	}
}

CUSTOM_CVAR(Bool, fluid_reverb, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (currSong != NULL)
		currSong->ChangeSettingInt("fluidsynth.synth.reverb.active", self);
	fluidConfig.fluid_reverb = self;
}

CUSTOM_CVAR(Bool, fluid_chorus, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (currSong != NULL)
		currSong->ChangeSettingInt("fluidsynth.synth.chorus.active", self);
	fluidConfig.fluid_chorus = self;
}

CUSTOM_CVAR(Int, fluid_voices, 128, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 16)
		self = 16;
	else if (self > 4096)
		self = 4096;
	else 
	{
		if (currSong != NULL)
			currSong->ChangeSettingInt("fluidsynth.synth.polyphony", self);
		fluidConfig.fluid_voices = self;
	}
}

CUSTOM_CVAR(Int, fluid_interp, 1, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	// Values are: 0 = FLUID_INTERP_NONE
	//             1 = FLUID_INTERP_LINEAR
	//             4 = FLUID_INTERP_4THORDER (the FluidSynth default)
	//             7 = FLUID_INTERP_7THORDER
	// (And here I thought it was just a linear list.)
	// Round undefined values to the nearest valid one.
	if (self < 0)
		self = 0;
	else if (self == 2)
		self = 1;
	else if (self == 3 || self == 5)
		self = 4;
	else if (self == 6 || self > 7)
		self = 7;
	else 
	{
		if (currSong != NULL)
			currSong->ChangeSettingInt("fluidsynth.synth.interpolation", self);
		fluidConfig.fluid_interp = self;
	}
}

CUSTOM_CVAR(Int, fluid_samplerate, 0, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	fluidConfig.fluid_samplerate = std::max<int>(*self, 0);
}

// I don't know if this setting even matters for us, since we aren't letting
// FluidSynth drives its own output.
CUSTOM_CVAR(Int, fluid_threads, 1, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 1)
		self = 1;
	else if (self > 256)
		self = 256;
	else
		fluidConfig.fluid_threads = self;
}

CUSTOM_CVAR(Float, fluid_reverb_roomsize, 0.61f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 0)
		self = 0;
	else if (self > 1.2f)
		self = 1.2f;
	else 
	{
		if (currSong != NULL)
			currSong->ChangeSettingNum("fluidsynth.z.reverb-roomsize", self);
		fluidConfig.fluid_reverb_roomsize = self;
	}
}

CUSTOM_CVAR(Float, fluid_reverb_damping, 0.23f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 0)
		self = 0;
	else if (self > 1)
		self = 1;
	else 
	{
		if (currSong != NULL)
			currSong->ChangeSettingNum("fluidsynth.z.reverb-damping", self);
		fluidConfig.fluid_reverb_damping = self;
	}
}

CUSTOM_CVAR(Float, fluid_reverb_width, 0.76f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 0)
		self = 0;
	else if (self > 100)
		self = 100;
	else 
	{
		if (currSong != NULL)
			currSong->ChangeSettingNum("fluidsynth.z.reverb-width", self);
		fluidConfig.fluid_reverb_width = self;
	}
}

CUSTOM_CVAR(Float, fluid_reverb_level, 0.57f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 0)
		self = 0;
	else if (self > 1)
		self = 1;
	else 
	{
		if (currSong != NULL)
			currSong->ChangeSettingNum("fluidsynth.z.reverb-level", self);
		fluidConfig.fluid_reverb_level = self;
	}
}

CUSTOM_CVAR(Int, fluid_chorus_voices, 3, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 0)
		self = 0;
	else if (self > 99)
		self = 99;
	else 
	{
		if (currSong != NULL)
			currSong->ChangeSettingNum("fluidsynth.z.chorus-voices", self);
		fluidConfig.fluid_chorus_voices = self;
	}
}

CUSTOM_CVAR(Float, fluid_chorus_level, 1.2f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 0)
		self = 0;
	else if (self > 1)
		self = 1;
	else 
	{
		if (currSong != NULL)
			currSong->ChangeSettingNum("fluidsynth.z.chorus-level", self);
		fluidConfig.fluid_chorus_level = self;
	}
}

CUSTOM_CVAR(Float, fluid_chorus_speed, 0.3f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 0.29f)
		self = 0.29f;
	else if (self > 5)
		self = 5;
	else 
	{
		if (currSong != NULL)
			currSong->ChangeSettingNum("fluidsynth.z.chorus-speed", self);
		fluidConfig.fluid_chorus_speed = self;
	}
}

// depth is in ms and actual maximum depends on the sample rate
CUSTOM_CVAR(Float, fluid_chorus_depth, 8, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 0)
		self = 0;
	else if (self > 21)
		self = 21;
	else 
	{
		if (currSong != NULL)
			currSong->ChangeSettingNum("fluidsynth.z.chorus-depth", self);
		fluidConfig.fluid_chorus_depth = self;
	}
}

CUSTOM_CVAR(Int, fluid_chorus_type, FLUID_CHORUS_DEFAULT_TYPE, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self != FLUID_CHORUS_MOD_SINE && self != FLUID_CHORUS_MOD_TRIANGLE)
		self = FLUID_CHORUS_DEFAULT_TYPE;
	else 
	{
		if (currSong != NULL)
			currSong->ChangeSettingNum("fluidsynth.z.chorus-type", self); // Uses float to simplify the checking code in the renderer.
		fluidConfig.fluid_chorus_type = self;
	}
}


//==========================================================================
//
// OPL MIDI device
//
//==========================================================================

CUSTOM_CVAR(Int, opl_numchips, 2, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
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
		oplMidiConfig.numchips = self;
	}
}

CUSTOM_CVAR(Int, opl_core, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	CheckRestart(MDEV_OPL);
}

CUSTOM_CVAR(Bool, opl_fullpan, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	oplMidiConfig.fullpan = self;
}

void OPL_SetupConfig(OPLMidiConfig *config, const char *args)
{
	// The OPL renderer should not care about where this comes from.
	// Note: No I_Error here - this needs to be consistent with the rest of the music code.
	auto lump = Wads.CheckNumForName("GENMIDI", ns_global);
	if (lump < 0) throw std::runtime_error("No GENMIDI lump found");
	auto data = Wads.OpenLumpReader(lump);

	uint8_t filehdr[8];
	data.Read(filehdr, 8);
	if (memcmp(filehdr, "#OPL_II#", 8)) throw std::runtime_error("Corrupt GENMIDI lump");
	data.Read(oplMidiConfig.OPLinstruments, sizeof(GenMidiInstrument) * GENMIDI_NUM_TOTAL);
	
	config->core = opl_core;
	if (args != NULL && *args >= '0' && *args < '4') config->core = *args - '0';
}


//==========================================================================
//
// OPN MIDI device
//
//==========================================================================


CUSTOM_CVAR(Int, opn_chips_count, 8, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	opnConfig.opn_chips_count = self;
	CheckRestart(MDEV_OPN);
}

CUSTOM_CVAR(Int, opn_emulator_id, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	opnConfig.opn_emulator_id = self;
	CheckRestart(MDEV_OPN);

}

CUSTOM_CVAR(Bool, opn_run_at_pcm_rate, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	opnConfig.opn_run_at_pcm_rate = self;
	CheckRestart(MDEV_OPN);

}

CUSTOM_CVAR(Bool, opn_fullpan, 1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	opnConfig.opn_fullpan = self;
	CheckRestart(MDEV_OPN);

}

CUSTOM_CVAR(Bool, opn_use_custom_bank, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	CheckRestart(MDEV_OPN);

}

CUSTOM_CVAR(String, opn_custom_bank, "", CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
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


CUSTOM_CVAR(String, midi_config, "gzdoom", CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	CheckRestart(MDEV_GUS);
}

CUSTOM_CVAR(Bool, midi_dmxgus, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)	// This was 'true' but since it requires special setup that's not such a good idea.
{
	CheckRestart(MDEV_GUS);
}

CUSTOM_CVAR(String, gus_patchdir, "", CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	gusConfig.gus_patchdir = self;
	CheckRestart(MDEV_GUS);
}

CUSTOM_CVAR(Int, midi_voices, 32, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	gusConfig.midi_voices = self;
	CheckRestart(MDEV_GUS);
}

CUSTOM_CVAR(Int, gus_memsize, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	gusConfig.gus_memsize = self;
	CheckRestart(MDEV_GUS);
}

//==========================================================================
//
// Error printing override to redirect to the internal console instead of stdout.
//
//==========================================================================

static void gus_printfunc(int type, int verbosity_level, const char* fmt, ...)
{
	if (verbosity_level >= Timidity::VERB_DEBUG) return;	// Don't waste time on diagnostics.
	
	va_list args;
	va_start(args, fmt);
	FString msg;
	msg.VFormat(fmt, args);
	va_end(args);
	
	switch (type)
	{
		case Timidity::CMSG_ERROR:
			Printf(TEXTCOLOR_RED "%s\n", msg.GetChars());
			break;
			
		case Timidity::CMSG_WARNING:
			Printf(TEXTCOLOR_YELLOW "%s\n", msg.GetChars());
			break;
			
		case Timidity::CMSG_INFO:
			DPrintf(DMSG_SPAMMY, "%s\n", msg.GetChars());
			break;
	}
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
	config->errorfunc = gus_printfunc;
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

template<class T> void ChangeVarSync(T& var, T value)
{
	std::lock_guard<std::mutex> lock(TimidityPlus::ConfigMutex);
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
	ChangeVarSync(TimidityPlus::timidity_surround_chorus, *self);
	CheckRestart(MDEV_TIMIDITY);
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
	CheckRestart(MDEV_TIMIDITY);
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
	else if (self > MAX_AMPLIFICATION / 100.f) self = MAX_AMPLIFICATION / 100.f;
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

CUSTOM_CVAR(Float, min_sustain_time, 5000, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 0) self = 0;
	ChangeVarSync(TimidityPlus::min_sustain_time, *self);
}

// Config file to use
CUSTOM_CVAR(String, timidity_config, "gzdoom", CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	CheckRestart(MDEV_TIMIDITY);
}

CVAR(Int, timidity_frequency, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)


bool Timidity_SetupConfig(TimidityConfig* config, const char* args)
{
	config->errorfunc = gus_printfunc;
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
