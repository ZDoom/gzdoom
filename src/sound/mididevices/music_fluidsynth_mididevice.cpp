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

#include "i_musicinterns.h"

// FluidSynth implementation of a MIDI device -------------------------------

#ifndef DYN_FLUIDSYNTH
#include <fluidsynth.h>
#else
#include "i_module.h"
extern FModule FluidSynthModule;

struct fluid_settings_t;
struct fluid_synth_t;
#endif

class FluidSynthMIDIDevice : public SoftSynthMIDIDevice
{
public:
	FluidSynthMIDIDevice(int samplerate, const FluidConfig *config, int (*printfunc_)(const char *, ...));
	~FluidSynthMIDIDevice();
	
	int Open(MidiCallback, void *userdata);
	FString GetStats();
	void ChangeSettingInt(const char *setting, int value);
	void ChangeSettingNum(const char *setting, double value);
	void ChangeSettingString(const char *setting, const char *value);
	int GetDeviceType() const override { return MDEV_FLUIDSYNTH; }
	
protected:
	void HandleEvent(int status, int parm1, int parm2);
	void HandleLongEvent(const uint8_t *data, int len);
	void ComputeOutput(float *buffer, int len);
	int LoadPatchSets(const FluidConfig *config);
	
	fluid_settings_t *FluidSettings;
	fluid_synth_t *FluidSynth;
	int (*printfunc)(const char*, ...);

	// These are getting set together, so if we want to keep the renderer and the config data separate we need to store local copies to apply changes.
	float fluid_reverb_roomsize, fluid_reverb_damping, fluid_reverb_width, fluid_reverb_level;
	float fluid_chorus_level,fluid_chorus_speed, fluid_chorus_depth;
	int fluid_chorus_voices, fluid_chorus_type;

	
#ifdef DYN_FLUIDSYNTH
	enum { FLUID_FAILED = -1, FLUID_OK = 0 };
	static TReqProc<FluidSynthModule, fluid_settings_t *(*)()> new_fluid_settings;
	static TReqProc<FluidSynthModule, fluid_synth_t *(*)(fluid_settings_t *)> new_fluid_synth;
	static TReqProc<FluidSynthModule, int (*)(fluid_synth_t *)> delete_fluid_synth;
	static TReqProc<FluidSynthModule, void (*)(fluid_settings_t *)> delete_fluid_settings;
	static TReqProc<FluidSynthModule, int (*)(fluid_settings_t *, const char *, double)> fluid_settings_setnum;
	static TReqProc<FluidSynthModule, int (*)(fluid_settings_t *, const char *, const char *)> fluid_settings_setstr;
	static TReqProc<FluidSynthModule, int (*)(fluid_settings_t *, const char *, int)> fluid_settings_setint;
	static TReqProc<FluidSynthModule, int (*)(fluid_settings_t *, const char *, int *)> fluid_settings_getint;
	static TReqProc<FluidSynthModule, void (*)(fluid_synth_t *, int)> fluid_synth_set_reverb_on;
	static TReqProc<FluidSynthModule, void (*)(fluid_synth_t *, int)> fluid_synth_set_chorus_on;
	static TReqProc<FluidSynthModule, int (*)(fluid_synth_t *, int, int)> fluid_synth_set_interp_method;
	static TReqProc<FluidSynthModule, int (*)(fluid_synth_t *, int)> fluid_synth_set_polyphony;
	static TReqProc<FluidSynthModule, int (*)(fluid_synth_t *)> fluid_synth_get_polyphony;
	static TReqProc<FluidSynthModule, int (*)(fluid_synth_t *)> fluid_synth_get_active_voice_count;
	static TReqProc<FluidSynthModule, double (*)(fluid_synth_t *)> fluid_synth_get_cpu_load;
	static TReqProc<FluidSynthModule, int (*)(fluid_synth_t *)> fluid_synth_system_reset;
	static TReqProc<FluidSynthModule, int (*)(fluid_synth_t *, int, int, int)> fluid_synth_noteon;
	static TReqProc<FluidSynthModule, int (*)(fluid_synth_t *, int, int)> fluid_synth_noteoff;
	static TReqProc<FluidSynthModule, int (*)(fluid_synth_t *, int, int, int)> fluid_synth_cc;
	static TReqProc<FluidSynthModule, int (*)(fluid_synth_t *, int, int)> fluid_synth_program_change;
	static TReqProc<FluidSynthModule, int (*)(fluid_synth_t *, int, int)> fluid_synth_channel_pressure;
	static TReqProc<FluidSynthModule, int (*)(fluid_synth_t *, int, int)> fluid_synth_pitch_bend;
	static TReqProc<FluidSynthModule, int (*)(fluid_synth_t *, int, void *, int, int, void *, int, int)> fluid_synth_write_float;
	static TReqProc<FluidSynthModule, int (*)(fluid_synth_t *, const char *, int)> fluid_synth_sfload;
	static TReqProc<FluidSynthModule, void (*)(fluid_synth_t *, double, double, double, double)> fluid_synth_set_reverb;
	static TReqProc<FluidSynthModule, void (*)(fluid_synth_t *, int, double, double, double, int)> fluid_synth_set_chorus;
	static TReqProc<FluidSynthModule, int (*)(fluid_synth_t *, const char *, int, char *, int *, int *, int)> fluid_synth_sysex;
	
	bool LoadFluidSynth(const char *fluid_lib);
	void UnloadFluidSynth();
#endif
};

// MACROS ------------------------------------------------------------------

#ifdef DYN_FLUIDSYNTH

#ifdef _WIN32

#ifndef _M_X64
#define FLUIDSYNTHLIB1	"fluidsynth.dll"
#define FLUIDSYNTHLIB2	"libfluidsynth.dll"
#else
#define FLUIDSYNTHLIB1	"fluidsynth64.dll"
#define FLUIDSYNTHLIB2	"libfluidsynth64.dll"
#endif
#else
#include <dlfcn.h>

#ifdef __APPLE__
#define FLUIDSYNTHLIB1	"libfluidsynth.1.dylib"
#else // !__APPLE__
#define FLUIDSYNTHLIB1	"libfluidsynth.so.1"
#endif // __APPLE__
#endif

#endif


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

FluidSynthMIDIDevice::FluidSynthMIDIDevice(int samplerate, const FluidConfig *config, int (*printfunc_)(const char*, ...) = nullptr)
	: SoftSynthMIDIDevice(samplerate <= 0? config->fluid_samplerate : samplerate, 22050, 96000)
{
	// These are needed for changing the settings. If something posts a transient config in here we got no way to retrieve the values afterward.
	fluid_reverb_roomsize = config->fluid_reverb_roomsize;
	fluid_reverb_damping = config->fluid_reverb_damping;
	fluid_reverb_width = config->fluid_reverb_width;
	fluid_reverb_level = config->fluid_reverb_level;
	fluid_chorus_voices = config->fluid_chorus_voices;
	fluid_chorus_level = config->fluid_chorus_level;
	fluid_chorus_speed = config->fluid_chorus_speed;
	fluid_chorus_depth = config->fluid_chorus_depth;
	fluid_chorus_type = config->fluid_chorus_type;

	printfunc = printfunc_;
	FluidSynth = NULL;
	FluidSettings = NULL;
#ifdef DYN_FLUIDSYNTH
	if (!LoadFluidSynth(config->fluid_lib.c_str()))
	{
		throw std::runtime_error("Failed to load FluidSynth.\n");
	}
#endif
	FluidSettings = new_fluid_settings();
	if (FluidSettings == NULL)
	{
		throw std::runtime_error("Failed to create FluidSettings.\n");
	}
	fluid_settings_setnum(FluidSettings, "synth.sample-rate", SampleRate);
	fluid_settings_setnum(FluidSettings, "synth.gain", config->fluid_gain);
	fluid_settings_setint(FluidSettings, "synth.reverb.active", config->fluid_reverb);
	fluid_settings_setint(FluidSettings, "synth.chorus.active", config->fluid_chorus);
	fluid_settings_setint(FluidSettings, "synth.polyphony", config->fluid_voices);
	fluid_settings_setint(FluidSettings, "synth.cpu-cores", config->fluid_threads);
	FluidSynth = new_fluid_synth(FluidSettings);
	if (FluidSynth == NULL)
	{
		delete_fluid_settings(FluidSettings);
		throw std::runtime_error("Failed to create FluidSynth.\n");
	}
	fluid_synth_set_interp_method(FluidSynth, -1, config->fluid_interp);
	fluid_synth_set_reverb(FluidSynth, fluid_reverb_roomsize, fluid_reverb_damping,
		fluid_reverb_width, fluid_reverb_level);
	fluid_synth_set_chorus(FluidSynth, fluid_chorus_voices, fluid_chorus_level,
		fluid_chorus_speed, fluid_chorus_depth, fluid_chorus_type);

	// try loading a patch set that got specified with $mididevice.
	int res = 0;

	if (LoadPatchSets(config))
	{
		return;
	}

	delete_fluid_settings(FluidSettings);
	delete_fluid_synth(FluidSynth);
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
#ifdef DYN_FLUIDSYNTH
	UnloadFluidSynth();
#endif
}

//==========================================================================
//
// FluidSynthMIDIDevice :: Open
//
// Returns 0 on success.
//
//==========================================================================

int FluidSynthMIDIDevice::Open(MidiCallback callback, void *userdata)
{
	if (FluidSynth == NULL)
	{
		return 2;
	}
	int ret = OpenStream(4, 0, callback, userdata);
	if (ret == 0)
	{
		fluid_synth_system_reset(FluidSynth);
	}
	return ret;
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
	if (len > 1 && (data[0] == 0xF0 || data[0] == 0xF7))
	{
		fluid_synth_sysex(FluidSynth, (const char *)data + 1, len - 1, NULL, NULL, NULL, 0);
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

int FluidSynthMIDIDevice::LoadPatchSets(const FluidConfig *config)
{
	int count = 0;
	for (auto& file : config->fluid_patchset)
	{
		if (FLUID_FAILED != fluid_synth_sfload(FluidSynth, file.c_str(), count == 0))
		{
			//DPrintf(DMSG_NOTIFY, "Loaded patch set %s.\n", tok);
			count++;
		}
		else
		{
			if (printfunc) printfunc("Failed to load patch set %s.\n", file.c_str());
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
			if (printfunc) printfunc("Setting interpolation method %d failed.\n", value);
		}
	}
	else if (strcmp(setting, "synth.polyphony") == 0)
	{
		if (FLUID_OK != fluid_synth_set_polyphony(FluidSynth, value))
		{
			if (printfunc) printfunc("Setting polyphony to %d failed.\n", value);
		}
	}
	else if (0 == fluid_settings_setint(FluidSettings, setting, value))
	{
		if (printfunc) printfunc("Failed to set %s to %d.\n", setting, value);
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

	bool reverbchanged = false, choruschanged = false;
	if (strcmp(setting, "z.reverb-roomsize") == 0)
	{
		fluid_reverb_roomsize = (float)value;
		reverbchanged = true;
	}
	else if (strcmp(setting, "z.reverb-damping") == 0)
	{
		fluid_reverb_damping = (float)value;
		reverbchanged = true;
	}
	else if (strcmp(setting, "z.reverb-width") == 0)
	{
		fluid_reverb_width = (float)value;
		reverbchanged = true;
	}
	else if (strcmp(setting, "z.reverb-level") == 0)
	{
		fluid_reverb_level = (float)value;
		reverbchanged = true;
	}
	else if (strcmp(setting, "z.chorus-voices") == 0)
	{
		fluid_chorus_voices = (int)value;
		choruschanged = true;
	}
	else if (strcmp(setting, "z.chorus-level") == 0)
	{
		fluid_chorus_level = (float)value;
		choruschanged = true;
	}
	else if (strcmp(setting, "z.chorus-speed") == 0)
	{
		fluid_chorus_speed = (float)value;
		choruschanged = true;
	}
	else if (strcmp(setting, "z.chorus-depth") == 0)
	{
		fluid_chorus_depth = (float)value;
		choruschanged = true;
	}
	else if (strcmp(setting, "z.chorus-type") == 0)
	{
		fluid_chorus_type = (int)value;
		choruschanged = true;
	}
	else if (0 == fluid_settings_setnum(FluidSettings, setting, value))
	{
		if (printfunc) printfunc("Failed to set %s to %g.\n", setting, value);
	}
	if (reverbchanged) fluid_synth_set_reverb(FluidSynth, fluid_reverb_roomsize, fluid_reverb_damping, fluid_reverb_width, fluid_reverb_level);
	if (choruschanged) fluid_synth_set_chorus(FluidSynth, (int)fluid_chorus_voices, fluid_chorus_level, fluid_chorus_speed, fluid_chorus_depth, (int)fluid_chorus_type);

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

	if (0 == fluid_settings_setstr(FluidSettings, setting, value))
	{
		if (printfunc) printfunc("Failed to set %s to %s.\n", setting, value);
	}
}

//==========================================================================
//
// FluidSynthMIDIDevice :: GetStats
//
//==========================================================================

FString FluidSynthMIDIDevice::GetStats()
{
	if (FluidSynth == NULL || FluidSettings == NULL)
	{
		return "FluidSynth is invalid";
	}
	FString out;

	std::lock_guard<std::mutex> lock(CritSec);
	int polyphony = fluid_synth_get_polyphony(FluidSynth);
	int voices = fluid_synth_get_active_voice_count(FluidSynth);
	double load = fluid_synth_get_cpu_load(FluidSynth);
	int chorus, reverb, maxpoly;
	fluid_settings_getint(FluidSettings, "synth.chorus.active", &chorus);
	fluid_settings_getint(FluidSettings, "synth.reverb.active", &reverb);
	fluid_settings_getint(FluidSettings, "synth.polyphony", &maxpoly);

	out.Format("Voices: %3d/%3d(%3d) %6.2f%% CPU   Reverb: %3s Chorus: %3s",
		voices, polyphony, maxpoly, load, reverb ? "yes" : "no", chorus ? "yes" : "no");
	return out;
}

#ifdef DYN_FLUIDSYNTH

//==========================================================================
//
// FluidSynthMIDIDevice :: LoadFluidSynth
//
// Returns true if the FluidSynth library was successfully loaded.
//
//==========================================================================

FModuleMaybe<DYN_FLUIDSYNTH> FluidSynthModule{"FluidSynth"};

#define DYN_FLUID_SYM(x) decltype(FluidSynthMIDIDevice::x) FluidSynthMIDIDevice::x{#x}
DYN_FLUID_SYM(new_fluid_settings);
DYN_FLUID_SYM(new_fluid_synth);
DYN_FLUID_SYM(delete_fluid_synth);
DYN_FLUID_SYM(delete_fluid_settings);
DYN_FLUID_SYM(fluid_settings_setnum);
DYN_FLUID_SYM(fluid_settings_setstr);
DYN_FLUID_SYM(fluid_settings_setint);
DYN_FLUID_SYM(fluid_settings_getint);
DYN_FLUID_SYM(fluid_synth_set_reverb_on);
DYN_FLUID_SYM(fluid_synth_set_chorus_on);
DYN_FLUID_SYM(fluid_synth_set_interp_method);
DYN_FLUID_SYM(fluid_synth_set_polyphony);
DYN_FLUID_SYM(fluid_synth_get_polyphony);
DYN_FLUID_SYM(fluid_synth_get_active_voice_count);
DYN_FLUID_SYM(fluid_synth_get_cpu_load);
DYN_FLUID_SYM(fluid_synth_system_reset);
DYN_FLUID_SYM(fluid_synth_noteon);
DYN_FLUID_SYM(fluid_synth_noteoff);
DYN_FLUID_SYM(fluid_synth_cc);
DYN_FLUID_SYM(fluid_synth_program_change);
DYN_FLUID_SYM(fluid_synth_channel_pressure);
DYN_FLUID_SYM(fluid_synth_pitch_bend);
DYN_FLUID_SYM(fluid_synth_write_float);
DYN_FLUID_SYM(fluid_synth_sfload);
DYN_FLUID_SYM(fluid_synth_set_reverb);
DYN_FLUID_SYM(fluid_synth_set_chorus);
DYN_FLUID_SYM(fluid_synth_sysex);

bool FluidSynthMIDIDevice::LoadFluidSynth(const char *fluid_lib)
{
	if (fluid_lib && strlen(fluid_lib) > 0)
	{
		if(!FluidSynthModule.Load({fluid_lib}))
		{
			const char* libname = fluid_lib;
			if (printfunc) printfunc("Could not load %s\n", libname);
		}
		else
			return true;
	}

#ifdef FLUIDSYNTHLIB2
	if(!FluidSynthModule.Load({FLUIDSYNTHLIB1, FLUIDSYNTHLIB2}))
	{
		if (printfunc) printfunc("Could not load " FLUIDSYNTHLIB1 " or " FLUIDSYNTHLIB2 "\n");
		return false;
	}
#else
	if(!FluidSynthModule.Load({fluid_lib, FLUIDSYNTHLIB1}))
	{
		if (printfunc) printfunc("Could not load " FLUIDSYNTHLIB1 ": %s\n", dlerror());
		return false;
	}
#endif
	return true;
}

//==========================================================================
//
// FluidSynthMIDIDevice :: UnloadFluidSynth
//
//==========================================================================

void FluidSynthMIDIDevice::UnloadFluidSynth()
{
	FluidSynthModule.Unload();
}

#endif

//==========================================================================
//
//
//
//==========================================================================

MIDIDevice *CreateFluidSynthMIDIDevice(int samplerate, const FluidConfig *config, int (*printfunc)(const char*, ...))
{
	return new FluidSynthMIDIDevice(samplerate, config, printfunc);
}
