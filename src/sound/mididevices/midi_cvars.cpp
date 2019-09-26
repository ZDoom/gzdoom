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

// do this without including windows.h for this one single prototype
extern "C" unsigned __stdcall GetSystemDirectoryA(char* lpBuffer, unsigned uSize);

static void CheckRestart(int devtype)
{
	if (currSong != nullptr && currSong->GetDeviceType() == devtype)
	{
		MIDIDeviceChanged(-1, true);
	}
}

ADLConfig adlConfig;
FluidConfig fluidConfig;
	

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
	adlConfig.adl_use_custom_bank = self;
	CheckRestart(MDEV_ADL);
}

CUSTOM_CVAR(String, adl_custom_bank, "", CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	CheckRestart(MDEV_ADL);

	//Resolve the path here, so that the renderer does not have to do the work itself and only needs to process final names.
	auto info = sfmanager.FindSoundFont(self, SF_WOPL);
	if (info == nullptr) adlConfig.adl_custom_bank = nullptr;
	else adlConfig.adl_custom_bank = info->mFilename;
}

CUSTOM_CVAR(Int, adl_volume_model, ADLMIDI_VolumeModel_DMX, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	adlConfig.adl_volume_model = self;
	CheckRestart(MDEV_ADL);
}

#define FLUID_CHORUS_MOD_SINE		0
#define FLUID_CHORUS_MOD_TRIANGLE	1
#define FLUID_CHORUS_DEFAULT_TYPE FLUID_CHORUS_MOD_SINE


CUSTOM_CVAR(String, fluid_lib, "", CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	fluidConfig.fluid_lib = self;	// only takes effect for next song.
}

int BuildFluidPatchSetList(const char* patches, bool systemfallback)
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

	if (wpatches == NULL)
	{
		return 0;
	}
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
			fluidConfig.fluid_patchset.push_back(path.GetChars());
		}
		else
		{
			Printf("Could not find patch set %s.\n", tok);
		}
		tok = strtok(NULL, delim);
	}
	free(wpatches);
	if (fluidConfig.fluid_patchset.size() > 0) return 1;

	if (systemfallback)
	{
		// The following will only be used if no soundfont at all is provided, i.e. even the standard one coming with GZDoom is missing.
#ifdef __unix__
		// This is the standard location on Ubuntu.
		return BuildFluidPatchSetList("/usr/share/sounds/sf2/FluidR3_GS.sf2:/usr/share/sounds/sf2/FluidR3_GM.sf2", false);
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
				fluidConfig.fluid_patchset.push_back(sysdir);
				return 1;
			}
			// Try again with CT2MGM.SF2
			sysdir[filepart + 3] = '2';
			if (FileExists(sysdir))
			{
				fluidConfig.fluid_patchset.push_back(sysdir);
				return 1;
			}
		}

#endif

	}
	return 0;
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

