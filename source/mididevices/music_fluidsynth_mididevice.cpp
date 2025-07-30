/*
** music_fluidsynth_mididevice.cpp
** Provides access to FluidSynth as a generic MIDI device.
**
**---------------------------------------------------------------------------
** Copyright 2010 Randy Heit
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

// HEADER FILES ------------------------------------------------------------

#include <mutex>
#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>
#include "zmusic/zmusic_internal.h"
#include "mididevice.h"
#include "zmusic/mus2midi.h"
#include "loader/i_module.h"

// FluidSynth implementation of a MIDI device -------------------------------

FluidConfig fluidConfig;

#include "../thirdparty/fluidsynth/include/fluidsynth.h"

class FluidSynthMIDIDevice : public SoftSynthMIDIDevice
{
public:
	FluidSynthMIDIDevice(int samplerate, std::vector<std::string> &config);
	~FluidSynthMIDIDevice();
	
	int OpenRenderer() override;
	std::string GetStats() override;
	void ChangeSettingInt(const char *setting, int value) override;
	void ChangeSettingNum(const char *setting, double value) override;
	void ChangeSettingString(const char *setting, const char *value) override;
	int GetDeviceType() const override { return MDEV_FLUIDSYNTH; }
	
protected:
	void HandleEvent(int status, int parm1, int parm2) override;
	void HandleLongEvent(const uint8_t *data, int len) override;
	void ComputeOutput(float *buffer, int len) override;
	int LoadPatchSets(const std::vector<std::string>& config);
	
	fluid_settings_t *FluidSettings;
	fluid_synth_t *FluidSynth;

	// Possible results returned by fluid_settings_...() functions
	// Initial values are for FluidSynth 2.x
	int FluidSettingsResultOk     = FLUID_OK;
	int FluidSettingsResultFailed = FLUID_FAILED;

};

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

const char *BaseFileSearch(const char *file, const char *ext, bool lookfirstinprogdir = false);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
//
// FluidSynthMIDIDevice Constructor
//
//==========================================================================

FluidSynthMIDIDevice::FluidSynthMIDIDevice(int samplerate, std::vector<std::string> &config)
	: SoftSynthMIDIDevice(samplerate <= 0? fluidConfig.fluid_samplerate : samplerate, 22050, 96000)
{
	StreamBlockSize = 4;

	FluidSynth = NULL;
	FluidSettings = NULL;

	FluidSettings = new_fluid_settings();
	if (FluidSettings == NULL)
	{
		throw std::runtime_error("Failed to create FluidSettings.\n");
	}
	fluid_settings_setint(FluidSettings, "synth.dynamic-sample-loading", 1);
	fluid_settings_setnum(FluidSettings, "synth.sample-rate", SampleRate);
	fluid_settings_setnum(FluidSettings, "synth.gain", fluidConfig.fluid_gain);
	fluid_settings_setint(FluidSettings, "synth.reverb.active", fluidConfig.fluid_reverb);
	fluid_settings_setint(FluidSettings, "synth.chorus.active", fluidConfig.fluid_chorus);
	fluid_settings_setint(FluidSettings, "synth.polyphony", fluidConfig.fluid_voices);
	fluid_settings_setint(FluidSettings, "synth.cpu-cores", fluidConfig.fluid_threads);
	FluidSynth = new_fluid_synth(FluidSettings);
	if (FluidSynth == NULL)
	{
		delete_fluid_settings(FluidSettings);
		throw std::runtime_error("Failed to create FluidSynth.\n");
	}
	fluid_synth_set_interp_method(FluidSynth, -1, fluidConfig.fluid_interp);
	fluid_synth_set_reverb(FluidSynth, fluidConfig.fluid_reverb_roomsize, fluidConfig.fluid_reverb_damping,
		fluidConfig.fluid_reverb_width, fluidConfig.fluid_reverb_level);
	fluid_synth_set_chorus(FluidSynth, fluidConfig.fluid_chorus_voices, fluidConfig.fluid_chorus_level,
		fluidConfig.fluid_chorus_speed, fluidConfig.fluid_chorus_depth, fluidConfig.fluid_chorus_type);

	// try loading a patch set that got specified with $mididevice.

	if (LoadPatchSets(config))
	{
		return;
	}

	delete_fluid_synth(FluidSynth);
	delete_fluid_settings(FluidSettings);
	FluidSynth = nullptr;
	FluidSettings = nullptr;
	throw std::runtime_error("Failed to load any MIDI patches.\n");

}


//==========================================================================
//
// FluidSynthMIDIDevice Destructor
//
//==========================================================================

FluidSynthMIDIDevice::~FluidSynthMIDIDevice()
{
	Close();
	if (FluidSynth != NULL)
	{
		delete_fluid_synth(FluidSynth);
	}
	if (FluidSettings != NULL)
	{
		delete_fluid_settings(FluidSettings);
	}
}

//==========================================================================
//
// FluidSynthMIDIDevice :: Open
//
// Returns 0 on success.
//
//==========================================================================

int FluidSynthMIDIDevice::OpenRenderer()
{
	// Send MIDI system reset command (big red 'panic' button), turns off notes, resets controllers and restores initial basic channel configuration.
	//fluid_synth_system_reset(FluidSynth);
	return 0;
}

//==========================================================================
//
// FluidSynthMIDIDevice :: HandleEvent
//
// Translates a MIDI event into FluidSynth calls.
//
//==========================================================================

void FluidSynthMIDIDevice::HandleEvent(int status, int parm1, int parm2)
{
	int command = status & 0xF0;
	int channel = status & 0x0F;
	
	switch (command)
	{
	case MIDI_NOTEOFF:
		fluid_synth_noteoff(FluidSynth, channel, parm1);
		break;

	case MIDI_NOTEON:
		fluid_synth_noteon(FluidSynth, channel, parm1, parm2);
		break;

	case MIDI_POLYPRESS:
		break;

	case MIDI_CTRLCHANGE:
		fluid_synth_cc(FluidSynth, channel, parm1, parm2);
		break;

	case MIDI_PRGMCHANGE:
		fluid_synth_program_change(FluidSynth, channel, parm1);
		break;

	case MIDI_CHANPRESS:
		fluid_synth_channel_pressure(FluidSynth, channel, parm1);
		break;

	case MIDI_PITCHBEND:
		fluid_synth_pitch_bend(FluidSynth, channel, (parm1 & 0x7f) | ((parm2 & 0x7f) << 7));
		break;
	}
}

//==========================================================================
//
// FluidSynthMIDIDevice :: HandleLongEvent
//
// Handle SysEx messages.
//
//==========================================================================

void FluidSynthMIDIDevice::HandleLongEvent(const uint8_t *data, int len)
{
	constexpr int excludedByteCount = 2;						// 0xF0 (first byte) and 0xF7 (last byte) are not given to FluidSynth.
	if (len > excludedByteCount && data[0] == 0xF0 && data[len - 1] == 0xF7)
	{
		fluid_synth_sysex(FluidSynth, (const char *)data + 1, len - excludedByteCount, NULL, NULL, NULL, 0);
	}
}

//==========================================================================
//
// FluidSynthMIDIDevice :: ComputeOutput
//
//==========================================================================

void FluidSynthMIDIDevice::ComputeOutput(float *buffer, int len)
{
	fluid_synth_write_float(FluidSynth, len,
		buffer, 0, 2,
		buffer, 1, 2);
}

//==========================================================================
//
// FluidSynthMIDIDevice :: LoadPatchSets
//
//==========================================================================

int FluidSynthMIDIDevice::LoadPatchSets(const std::vector<std::string> &config)
{
	int count = 0;
	for (auto& file : config)
	{
		if (FLUID_FAILED != fluid_synth_sfload(FluidSynth, file.c_str(), count == 0))
		{
			ZMusic_Printf(ZMUSIC_MSG_DEBUG, "Loaded patch set %s.\n", file.c_str());
			count++;
		}
		else
		{
			ZMusic_Printf(ZMUSIC_MSG_ERROR, "Failed to load patch set %s.\n", file.c_str());
		}
	}
	return count;
}

//==========================================================================
//
// FluidSynthMIDIDevice :: ChangeSettingInt
//
// Changes an integer setting.
//
//==========================================================================

void FluidSynthMIDIDevice::ChangeSettingInt(const char *setting, int value)
{
	if (FluidSynth == nullptr || FluidSettings == nullptr || strncmp(setting, "fluidsynth.", 11))
	{
		return;
	}
	setting += 11;

	if (strcmp(setting, "synth.interpolation") == 0)
	{
		if (FLUID_OK != fluid_synth_set_interp_method(FluidSynth, -1, value))
		{
			ZMusic_Printf(ZMUSIC_MSG_ERROR, "Setting interpolation method %d failed.\n", value);
		}
	}
	else if (strcmp(setting, "synth.polyphony") == 0)
	{
		if (FLUID_OK != fluid_synth_set_polyphony(FluidSynth, value))
		{
			ZMusic_Printf(ZMUSIC_MSG_ERROR, "Setting polyphony to %d failed.\n", value);
		}
	}
	else if (FluidSettingsResultFailed == fluid_settings_setint(FluidSettings, setting, value))
	{
		ZMusic_Printf(ZMUSIC_MSG_ERROR, "Failed to set %s to %d.\n", setting, value);
	}
	// fluid_settings_setint succeeded; update these settings in the running synth, too
	else if (strcmp(setting, "synth.reverb.active") == 0)
	{
		fluid_synth_set_reverb_on(FluidSynth, value);
	}
	else if (strcmp(setting, "synth.chorus.active") == 0)
	{
		fluid_synth_set_chorus_on(FluidSynth, value);
	}
}

//==========================================================================
//
// FluidSynthMIDIDevice :: ChangeSettingNum
//
// Changes a numeric setting.
//
//==========================================================================

void FluidSynthMIDIDevice::ChangeSettingNum(const char *setting, double value)
{
	if (FluidSynth == nullptr || FluidSettings == nullptr || strncmp(setting, "fluidsynth.", 11))
	{
		return;
	}
	setting += 11;

	if (strcmp(setting, "z.reverb") == 0)
	{
		fluid_synth_set_reverb(FluidSynth, fluidConfig.fluid_reverb_roomsize, fluidConfig.fluid_reverb_damping, fluidConfig.fluid_reverb_width, fluidConfig.fluid_reverb_level);
	}
	else if (strcmp(setting, "z.chorus") == 0)
	{
		fluid_synth_set_chorus(FluidSynth, fluidConfig.fluid_chorus_voices, fluidConfig.fluid_chorus_level, fluidConfig.fluid_chorus_speed, fluidConfig.fluid_chorus_depth, fluidConfig.fluid_chorus_type);
	}
	else if (FluidSettingsResultFailed == fluid_settings_setnum(FluidSettings, setting, value))
	{
		ZMusic_Printf(ZMUSIC_MSG_ERROR, "Failed to set %s to %g.\n", setting, value);
	}

}

//==========================================================================
//
// FluidSynthMIDIDevice :: ChangeSettingString
//
// Changes a string setting.
//
//==========================================================================

void FluidSynthMIDIDevice::ChangeSettingString(const char *setting, const char *value)
{
	if (FluidSynth == nullptr || FluidSettings == nullptr || strncmp(setting, "fluidsynth.", 11))
	{
		return;
	}
	setting += 11;

	if (FluidSettingsResultFailed == fluid_settings_setstr(FluidSettings, setting, value))
	{
		ZMusic_Printf(ZMUSIC_MSG_ERROR, "Failed to set %s to %s.\n", setting, value);
	}
}

//==========================================================================
//
// FluidSynthMIDIDevice :: GetStats
//
//==========================================================================

std::string FluidSynthMIDIDevice::GetStats()
{
	if (FluidSynth == NULL || FluidSettings == NULL)
	{
		return "FluidSynth is invalid";
	}

	int polyphony = fluid_synth_get_polyphony(FluidSynth);
	int voices = fluid_synth_get_active_voice_count(FluidSynth);
	double load = fluid_synth_get_cpu_load(FluidSynth);
	int chorus, reverb, maxpoly;
	fluid_settings_getint(FluidSettings, "synth.chorus.active", &chorus);
	fluid_settings_getint(FluidSettings, "synth.reverb.active", &reverb);
	fluid_settings_getint(FluidSettings, "synth.polyphony", &maxpoly);

	char out[100];
	snprintf(out, 100,"Voices: %3d/%3d(%3d) %6.2f%% CPU   Reverb: %3s Chorus: %3s",
		voices, polyphony, maxpoly, load, reverb ? "yes" : "no", chorus ? "yes" : "no");
	return out;
}

//
// sndfile
//
//==========================================================================


#ifdef _WIN32
// do this without including windows.h for this one single prototype
extern "C" unsigned __stdcall GetSystemDirectoryA(char* lpBuffer, unsigned uSize);
#endif // _WIN32

void Fluid_SetupConfig(const char* patches, std::vector<std::string> &patch_paths, bool systemfallback)
{
	if (*patches == 0) patches = fluidConfig.fluid_patchset.c_str();

	//Resolve the paths here, the renderer will only get a final list of file names.

	if (musicCallbacks.PathForSoundfont)
	{
		auto info = musicCallbacks.PathForSoundfont(patches, SF_SF2);
		if (info) patches = info;
	}

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
			std::string path;
#ifdef _WIN32
			// If the path does not contain any path separators, automatically
			// prepend $PROGDIR to the path.
			if (strcspn(tok, ":/\\") == strlen(tok))
			{
				path = FModule_GetProgDir() + "/" + tok;
			}
			else
#endif
			{
				path = tok;
			}
			if (musicCallbacks.NicePath)
				path = musicCallbacks.NicePath(path.c_str());

			if (MusicIO::fileExists(path.c_str()))
			{
				patch_paths.push_back(path);
			}
			else
			{
				ZMusic_Printf(ZMUSIC_MSG_ERROR, "Could not find patch set %s.\n", tok);
			}
			tok = strtok(NULL, delim);
		}
		free(wpatches);
		if (patch_paths.size() > 0) return;
	}

	if (systemfallback)
	{
		// The following will only be used if no soundfont at all is provided, i.e. even the standard one coming with GZDoom is missing.
#ifdef __unix__
		// This is the standard location on Ubuntu.
		Fluid_SetupConfig("/usr/share/sounds/sf2/FluidR3_GS.sf2:/usr/share/sounds/sf2/FluidR3_GM.sf2", patch_paths, false);
#endif
#ifdef _WIN32
		// On Windows, look for the 4 megabyte patch set installed by Creative's drivers as a default.
		char sysdir[260 + sizeof("\\CT4MGM.SF2")];
		uint32_t filepart;
		if (0 != (filepart = GetSystemDirectoryA(sysdir, 260)))
		{
			strcat(sysdir, "\\CT4MGM.SF2");
			if (MusicIO::fileExists(sysdir))
			{
				patch_paths.push_back(sysdir);
				return;
			}
			// Try again with CT2MGM.SF2
			sysdir[filepart + 3] = '2';
			if (MusicIO::fileExists(sysdir))
			{
				patch_paths.push_back(sysdir);
				return;
			}
		}

#endif

	}
}

//==========================================================================
//
//
//
//==========================================================================

MIDIDevice *CreateFluidSynthMIDIDevice(int samplerate, const char *Args)
{
	std::vector<std::string> fluid_patchset;

	Fluid_SetupConfig(Args, fluid_patchset, true);
	return new FluidSynthMIDIDevice(samplerate, fluid_patchset);
}
