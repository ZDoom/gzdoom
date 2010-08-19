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

#ifdef HAVE_FLUIDSYNTH

// HEADER FILES ------------------------------------------------------------

#include "i_musicinterns.h"
#include "templates.h"
#include "doomdef.h"
#include "m_swap.h"
#include "w_wad.h"
#include "v_text.h"

// MACROS ------------------------------------------------------------------

#ifdef DYN_FLUIDSYNTH

#ifdef _WIN32
#ifndef _M_X64
#define FLUIDSYNTHLIB	"fluidsynth.dll"
#else
#define FLUIDSYNTHLIB	"fluidsynth64.dll"
#endif
#else
#include <dlfcn.h>

#define FLUIDSYNTHLIB	"libfluidsynth.so.1"
#endif

#define FLUID_REVERB_DEFAULT_ROOMSIZE 0.2f
#define FLUID_REVERB_DEFAULT_DAMP 0.0f
#define FLUID_REVERB_DEFAULT_WIDTH 0.5f
#define FLUID_REVERB_DEFAULT_LEVEL 0.9f

#define FLUID_CHORUS_MOD_SINE		0
#define FLUID_CHORUS_MOD_TRIANGLE	1

#define FLUID_CHORUS_DEFAULT_N 3
#define FLUID_CHORUS_DEFAULT_LEVEL 2.0f
#define FLUID_CHORUS_DEFAULT_SPEED 0.3f
#define FLUID_CHORUS_DEFAULT_DEPTH 8.0f
#define FLUID_CHORUS_DEFAULT_TYPE FLUID_CHORUS_MOD_SINE

#endif

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

CVAR(String, fluid_patchset, "", CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

CUSTOM_CVAR(Float, fluid_gain, 0.5, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 0)
		self = 0;
	else if (self > 10)
		self = 10;
	else if (currSong != NULL)
		currSong->FluidSettingNum("synth.gain", self);
}

CUSTOM_CVAR(Bool, fluid_reverb, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (currSong != NULL)
		currSong->FluidSettingStr("synth.reverb.active", self ? "yes" : "no");
}

CUSTOM_CVAR(Bool, fluid_chorus, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (currSong != NULL)
		currSong->FluidSettingStr("synth.chorus.active", self ? "yes" : "no");
}

CUSTOM_CVAR(Int, fluid_voices, 128, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 16)
		self = 16;
	else if (self > 4096)
		self = 4096;
	else if (currSong != NULL)
		currSong->FluidSettingInt("synth.polyphony", self);
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
	else if (currSong != NULL)
		currSong->FluidSettingInt("synth.interpolation", self);
}

CVAR(Int, fluid_samplerate, 0, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

// I don't know if this setting even matters for us, since we aren't letting
// FluidSynth drives its own output.
CUSTOM_CVAR(Int, fluid_threads, 1, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 1)
		self = 1;
	else if (self > 256)
		self = 256;
}

CUSTOM_CVAR(Float, fluid_reverb_roomsize, FLUID_REVERB_DEFAULT_ROOMSIZE, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 0)
		self = 0;
	else if (self > 1.2f)
		self = 1.2f;
	else if (currSong != NULL)
		currSong->FluidSettingInt("z.reverb-changed", 0);
}

CUSTOM_CVAR(Float, fluid_reverb_damping, FLUID_REVERB_DEFAULT_DAMP, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 0)
		self = 0;
	else if (self > 1)
		self = 1;
	else if (currSong != NULL)
		currSong->FluidSettingInt("z.reverb-changed", 0);
}

CUSTOM_CVAR(Float, fluid_reverb_width, FLUID_REVERB_DEFAULT_WIDTH, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 0)
		self = 0;
	else if (self > 100)
		self = 100;
	else if (currSong != NULL)
		currSong->FluidSettingInt("z.reverb-changed", 0);
}

CUSTOM_CVAR(Float, fluid_reverb_level, FLUID_REVERB_DEFAULT_LEVEL, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 0)
		self = 0;
	else if (self > 1)
		self = 1;
	else if (currSong != NULL)
		currSong->FluidSettingInt("z.reverb-changed", 0);
}

CUSTOM_CVAR(Int, fluid_chorus_voices, FLUID_CHORUS_DEFAULT_N, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 0)
		self = 0;
	else if (self > 99)
		self = 99;
	else if (currSong != NULL)
		currSong->FluidSettingInt("z.chorus-changed", 0);
}

CUSTOM_CVAR(Float, fluid_chorus_level, FLUID_CHORUS_DEFAULT_LEVEL, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 0)
		self = 0;
	else if (self > 1)
		self = 1;
	else if (currSong != NULL)
		currSong->FluidSettingInt("z.chorus-changed", 0);
}

CUSTOM_CVAR(Float, fluid_chorus_speed, FLUID_CHORUS_DEFAULT_SPEED, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 0.29f)
		self = 0.29f;
	else if (self > 5)
		self = 5;
	else if (currSong != NULL)
		currSong->FluidSettingInt("z.chorus-changed", 0);
}

// depth is in ms and actual maximum depends on the sample rate
CUSTOM_CVAR(Float, fluid_chorus_depth, FLUID_CHORUS_DEFAULT_DEPTH, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 0)
		self = 0;
	else if (self > 21)
		self = 21;
	else if (currSong != NULL)
		currSong->FluidSettingInt("z.chorus-changed", 0);
}

CUSTOM_CVAR(Int, fluid_chorus_type, FLUID_CHORUS_DEFAULT_TYPE, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self != FLUID_CHORUS_MOD_SINE && self != FLUID_CHORUS_MOD_TRIANGLE)
		self = FLUID_CHORUS_DEFAULT_TYPE;
	else if (currSong != NULL)
		currSong->FluidSettingInt("z.chorus-changed", 0);
}

// CODE --------------------------------------------------------------------

//==========================================================================
//
// FluidSynthMIDIDevice Constructor
//
//==========================================================================

FluidSynthMIDIDevice::FluidSynthMIDIDevice()
{
	Stream = NULL;
	Tempo = 0;
	Division = 0;
	Events = NULL;
	Started = false;
	FluidSynth = NULL;
	FluidSettings = NULL;
#ifdef DYN_FLUIDSYNTH
	if (!LoadFluidSynth())
	{
		return;
	}
#endif
	FluidSettings = new_fluid_settings();
	if (FluidSettings == NULL)
	{
		printf("Failed to create FluidSettings.\n");
		return;
	}
	SampleRate = fluid_samplerate;
	if (SampleRate < 22050 || SampleRate > 96000)
	{ // Match sample rate to SFX rate
		SampleRate = clamp((int)GSnd->GetOutputRate(), 22050, 96000);
	}
	fluid_settings_setnum(FluidSettings, "synth.sample-rate", SampleRate);
	fluid_settings_setnum(FluidSettings, "synth.gain", fluid_gain);
	fluid_settings_setstr(FluidSettings, "synth.reverb.active", fluid_reverb ? "yes" : "no");
	fluid_settings_setstr(FluidSettings, "synth.chorus.active", fluid_chorus ? "yes" : "no");
	fluid_settings_setint(FluidSettings, "synth.polyphony", fluid_voices);
	fluid_settings_setint(FluidSettings, "synth.cpu-cores", fluid_threads);
	FluidSynth = new_fluid_synth(FluidSettings);
	if (FluidSynth == NULL)
	{
		Printf("Failed to create FluidSynth.\n");
		return;
	}
	fluid_synth_set_interp_method(FluidSynth, -1, fluid_interp);
	fluid_synth_set_reverb(FluidSynth, fluid_reverb_roomsize, fluid_reverb_damping,
		fluid_reverb_width, fluid_reverb_level);
	fluid_synth_set_chorus(FluidSynth, fluid_chorus_voices, fluid_chorus_level,
		fluid_chorus_speed, fluid_chorus_depth, fluid_chorus_type);
	if (0 == LoadPatchSets(fluid_patchset))
	{
#ifdef unix
		// This is the standard location on Ubuntu.
		if (0 == LoadPatchSets("/usr/share/sounds/sf2/FluidR3_GS.sf2:/usr/share/sounds/sf2/FluidR3_GM.sf2"))
		{
#endif
#ifdef _WIN32
		// On Windows, look for the 4 megabyte patch set installed by Creative's drivers as a default.
		char sysdir[MAX_PATH+sizeof("\\CT4MGM.SF2")];
		UINT filepart;
		if (0 != (filepart = GetSystemDirectoryA(sysdir, MAX_PATH)))
		{
			strcat(sysdir, "\\CT4MGM.SF2");
			if (0 == LoadPatchSets(sysdir))
			{
				// Try again with CT2MGM.SF2
				sysdir[filepart + 3] = '2';
				if (0 == LoadPatchSets(sysdir))
				{
#endif
					Printf("Failed to load any MIDI patches.\n");
					delete_fluid_synth(FluidSynth);
					FluidSynth = NULL;
#ifdef _WIN32
				}
			}
		}
#endif
#ifdef unix
		}
#endif
	}
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

int FluidSynthMIDIDevice::Open(void (*callback)(unsigned int, void *, DWORD, DWORD), void *userdata)
{
	if (FluidSynth == NULL)
	{
		return 2;
	}
	Stream = GSnd->CreateStream(FillStream, (SampleRate / 4) * 4,
		SoundStream::Float, SampleRate, this);
	if (Stream == NULL)
	{
		return 2;
	}

	fluid_synth_system_reset(FluidSynth);
	Callback = callback;
	CallbackData = userdata;
	Tempo = 500000;
	Division = 100;
	CalcTickRate();
	return 0;
}

//==========================================================================
//
// FluidSynthMIDIDevice :: Close
//
//==========================================================================

void FluidSynthMIDIDevice::Close()
{
	if (Stream != NULL)
	{
		delete Stream;
		Stream = NULL;
	}
	Started = false;
}

//==========================================================================
//
// FluidSynthMIDIDevice :: IsOpen
//
//==========================================================================

bool FluidSynthMIDIDevice::IsOpen() const
{
	return Stream != NULL;
}

//==========================================================================
//
// FluidSynthMIDIDevice :: GetTechnology
//
//==========================================================================

int FluidSynthMIDIDevice::GetTechnology() const
{
	return MOD_SWSYNTH;
}

//==========================================================================
//
// FluidSynthMIDIDevice :: SetTempo
//
//==========================================================================

int FluidSynthMIDIDevice::SetTempo(int tempo)
{
	Tempo = tempo;
	CalcTickRate();
	return 0;
}

//==========================================================================
//
// FluidSynthMIDIDevice :: SetTimeDiv
//
//==========================================================================

int FluidSynthMIDIDevice::SetTimeDiv(int timediv)
{
	Division = timediv;
	CalcTickRate();
	return 0;
}

//==========================================================================
//
// FluidSynthMIDIDevice :: CalcTickRate
//
// Tempo is the number of microseconds per quarter note.
// Division is the number of ticks per quarter note.
//
//==========================================================================

void FluidSynthMIDIDevice::CalcTickRate()
{
	SamplesPerTick = SampleRate / (1000000.0 / Tempo) / Division;
}

//==========================================================================
//
// FluidSynthMIDIDevice :: Resume
//
//==========================================================================

int FluidSynthMIDIDevice::Resume()
{
	if (!Started)
	{
		if (Stream->Play(true, 1))
		{
			Started = true;
			return 0;
		}
		return 1;
	}
	return 0;
}

//==========================================================================
//
// FluidSynthMIDIDevice :: Stop
//
//==========================================================================

void FluidSynthMIDIDevice::Stop()
{
	if (Started)
	{
		Stream->Stop();
		Started = false;
	}
}

//==========================================================================
//
// FluidSynthMIDIDevice :: StreamOutSync
//
// This version is called from the main game thread and needs to
// synchronize with the player thread.
//
//==========================================================================

int FluidSynthMIDIDevice::StreamOutSync(MIDIHDR *header)
{
	CritSec.Enter();
	StreamOut(header);
	CritSec.Leave();
	return 0;
}

//==========================================================================
//
// FluidSynthMIDIDevice :: StreamOut
//
// This version is called from the player thread so does not need to
// arbitrate for access to the Events pointer.
//
//==========================================================================

int FluidSynthMIDIDevice::StreamOut(MIDIHDR *header)
{
	header->lpNext = NULL;
	if (Events == NULL)
	{
		Events = header;
		NextTickIn = SamplesPerTick * *(DWORD *)header->lpData;
		Position = 0;
	}
	else
	{
		MIDIHDR **p;

		for (p = &Events; *p != NULL; p = &(*p)->lpNext)
		{ }
		*p = header;
	}
	return 0;
}

//==========================================================================
//
// FluidSynthMIDIDevice :: PrepareHeader
//
//==========================================================================

int FluidSynthMIDIDevice::PrepareHeader(MIDIHDR *header)
{
	return 0;
}

//==========================================================================
//
// FluidSynthMIDIDevice :: UnprepareHeader
//
//==========================================================================

int FluidSynthMIDIDevice::UnprepareHeader(MIDIHDR *header)
{
	return 0;
}

//==========================================================================
//
// FluidSynthMIDIDevice :: FakeVolume
//
// Since the FluidSynth output is rendered as a normal stream, its volume is
// controlled through the GSnd interface, not here.
//
//==========================================================================

bool FluidSynthMIDIDevice::FakeVolume()
{
	return false;
}

//==========================================================================
//
// FluidSynthMIDIDevice :: NeedThreadedCallabck
//
// FluidSynth can service the callback directly rather than using a separate
// thread.
//
//==========================================================================

bool FluidSynthMIDIDevice::NeedThreadedCallback()
{
	return false;
}

//==========================================================================
//
// FluidSynthMIDIDevice :: Pause
//
//==========================================================================

bool FluidSynthMIDIDevice::Pause(bool paused)
{
	if (Stream != NULL)
	{
		return Stream->SetPaused(paused);
	}
	return true;
}

//==========================================================================
//
// FluidSynthMIDIDevice :: PrecacheInstruments
//
// Each entry is packed as follows:
//   Bits 0- 6: Instrument number
//   Bits 7-13: Bank number
//   Bit    14: Select drum set if 1, tone bank if 0
//
//==========================================================================

void FluidSynthMIDIDevice::PrecacheInstruments(const WORD *instruments, int count)
{
#if 0
	for (int i = 0; i < count; ++i)
	{
		Renderer->MarkInstrument((instruments[i] >> 7) & 127, instruments[i] >> 14, instruments[i] & 127);
	}
	Renderer->load_missing_instruments();
#endif
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
// FluidSynthMIDIDevice :: PlayTick
//
// event[0] = delta time
// event[1] = unused
// event[2] = event
//
//==========================================================================

int FluidSynthMIDIDevice::PlayTick()
{
	DWORD delay = 0;

	while (delay == 0 && Events != NULL)
	{
		DWORD *event = (DWORD *)(Events->lpData + Position);
		if (MEVT_EVENTTYPE(event[2]) == MEVT_TEMPO)
		{
			SetTempo(MEVT_EVENTPARM(event[2]));
		}
		else if (MEVT_EVENTTYPE(event[2]) == MEVT_LONGMSG)
		{
#if 0
			Renderer->HandleLongMessage((BYTE *)&event[3], MEVT_EVENTPARM(event[2]));
#endif
		}
		else if (MEVT_EVENTTYPE(event[2]) == 0)
		{ // Short MIDI event
			int status = event[2] & 0xff;
			int parm1 = (event[2] >> 8) & 0x7f;
			int parm2 = (event[2] >> 16) & 0x7f;
			HandleEvent(status, parm1, parm2);
		}

		// Advance to next event.
		if (event[2] < 0x80000000)
		{ // Short message
			Position += 12;
		}
		else
		{ // Long message
			Position += 12 + ((MEVT_EVENTPARM(event[2]) + 3) & ~3);
		}

		// Did we use up this buffer?
		if (Position >= Events->dwBytesRecorded)
		{
			Events = Events->lpNext;
			Position = 0;

			if (Callback != NULL)
			{
				Callback(MOM_DONE, CallbackData, 0, 0);
			}
		}

		if (Events == NULL)
		{ // No more events. Just return something to keep the song playing
		  // while we wait for more to be submitted.
			return int(Division);
		}

		delay = *(DWORD *)(Events->lpData + Position);
	}
	return delay;
}

//==========================================================================
//
// FluidSynthtMIDIDevice :: ServiceStream
//
//==========================================================================

bool FluidSynthMIDIDevice::ServiceStream (void *buff, int numbytes)
{
	float *samples = (float *)buff;
	float *samples1;
	int numsamples = numbytes / sizeof(float) / 2;
	bool prev_ended = false;
	bool res = true;

	samples1 = samples;
	memset(buff, 0, numbytes);

	CritSec.Enter();
	while (Events != NULL && numsamples > 0)
	{
		double ticky = NextTickIn;
		int tick_in = int(NextTickIn);
		int samplesleft = MIN(numsamples, tick_in);

		if (samplesleft > 0)
		{
			fluid_synth_write_float(FluidSynth, samplesleft,
				samples1, 0, 2,
				samples1, 1, 2);
			assert(NextTickIn == ticky);
			NextTickIn -= samplesleft;
			assert(NextTickIn >= 0);
			numsamples -= samplesleft;
			samples1 += samplesleft * 2;
		}
		
		if (NextTickIn < 1)
		{
			int next = PlayTick();
			assert(next >= 0);
			if (next == 0)
			{ // end of song
				if (numsamples > 0)
				{
					fluid_synth_write_float(FluidSynth, numsamples,
						samples1, 0, 2,
						samples1, 1, 2);
				}
				res = false;
				break;
			}
			else
			{
				NextTickIn += SamplesPerTick * next;
				assert(NextTickIn >= 0);
			}
		}
	}
	if (Events == NULL)
	{
		res = false;
	}
	CritSec.Leave();
	return res;
}

//==========================================================================
//
// FluidSynthMIDIDevice :: LoadPatchSets
//
// Loads a delimiter-separated list of patch sets. This delimiter matches
// that of the PATH environment variable. On Windows, it is ';'. On other
// systems, it is ':'. Returns the number of patch sets loaded.
//
//==========================================================================

int FluidSynthMIDIDevice::LoadPatchSets(const char *patches)
{
	int count;
	char *wpatches = strdup(patches);
	char *tok;
#ifdef _WIN32
	const char *const delim = ";";
#else
	const char *const delim = ":";
#endif

	if (wpatches == NULL)
	{
		return 0;
	}
	tok = strtok(wpatches, delim);
	count = 0;
	while (tok != NULL)
	{
		if (FLUID_FAILED != fluid_synth_sfload(FluidSynth, tok, count == 0))
		{
			DPrintf("Loaded patch set %s.\n", tok);
			count++;
		}
		else
		{
			DPrintf("Failed to load patch set %s.\n", tok);
		}
		tok = strtok(NULL, delim);
	}
	free(wpatches);
	return count;
}

//==========================================================================
//
// FluidSynthMIDIDevice :: FluidSettingInt
//
// Changes an integer setting.
//
//==========================================================================

void FluidSynthMIDIDevice::FluidSettingInt(const char *setting, int value)
{
	if (FluidSynth == NULL || FluidSettings == NULL)
	{
		return;
	}

	if (strcmp(setting, "synth.interpolation") == 0)
	{
		if (FLUID_OK != fluid_synth_set_interp_method(FluidSynth, -1, value))
		{
			Printf("Setting interpolation method %d failed.\n", value);
		}
	}
	else if (strcmp(setting, "synth.polyphony") == 0)
	{
		if (FLUID_OK != fluid_synth_set_polyphony(FluidSynth, value))
		{
			Printf("Setting polyphony to %d failed.\n", value);
		}
	}
	else if (strcmp(setting, "z.reverb-changed") == 0)
	{
		fluid_synth_set_reverb(FluidSynth, fluid_reverb_roomsize, fluid_reverb_damping,
			fluid_reverb_width, fluid_reverb_level);
	}
	else if (strcmp(setting, "z.chorus-changed") == 0)
	{
		fluid_synth_set_chorus(FluidSynth, fluid_chorus_voices, fluid_chorus_level,
			fluid_chorus_speed, fluid_chorus_depth, fluid_chorus_type);
	}
	else if (FLUID_OK != fluid_settings_setint(FluidSettings, setting, value))
	{
		Printf("Faild to set %s to %d.\n", setting, value);
	}
}

//==========================================================================
//
// FluidSynthMIDIDevice :: FluidSettingNum
//
// Changes a numeric setting.
//
//==========================================================================

void FluidSynthMIDIDevice::FluidSettingNum(const char *setting, double value)
{
	if (FluidSettings != NULL)
	{
		if (!fluid_settings_setnum(FluidSettings, setting, value))
		{
			Printf("Failed to set %s to %g.\n", setting, value);
		}
	}
}

//==========================================================================
//
// FluidSynthMIDIDevice :: FluidSettingStr
//
// Changes a string setting.
//
//==========================================================================

void FluidSynthMIDIDevice::FluidSettingStr(const char *setting, const char *value)
{
	if (FluidSettings != NULL)
	{
		if (!fluid_settings_setstr(FluidSettings, setting, value))
		{
			Printf("Failed to set %s to %s.\n", setting, value);
		}
	}
}

//==========================================================================
//
// FluidSynthMIDIDevice :: FillStream									static
//
//==========================================================================

bool FluidSynthMIDIDevice::FillStream(SoundStream *stream, void *buff, int len, void *userdata)
{
	FluidSynthMIDIDevice *device = (FluidSynthMIDIDevice *)userdata;
	return device->ServiceStream(buff, len);
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

	CritSec.Enter();
	int polyphony = fluid_synth_get_polyphony(FluidSynth);
	int voices = fluid_synth_get_active_voice_count(FluidSynth);
	double load = fluid_synth_get_cpu_load(FluidSynth);
	char *chorus, *reverb;
	int maxpoly;
	fluid_settings_getstr(FluidSettings, "synth.chorus.active", &chorus);
	fluid_settings_getstr(FluidSettings, "synth.reverb.active", &reverb);
	fluid_settings_getint(FluidSettings, "synth.polyphony", &maxpoly);
	CritSec.Leave();

	out.Format("Voices: "TEXTCOLOR_YELLOW"%3d"TEXTCOLOR_NORMAL"/"TEXTCOLOR_ORANGE"%3d"TEXTCOLOR_NORMAL"("TEXTCOLOR_RED"%3d"TEXTCOLOR_NORMAL")"
			   TEXTCOLOR_YELLOW"%6.2f"TEXTCOLOR_NORMAL"%% CPU   "
			   "Reverb: "TEXTCOLOR_YELLOW"%3s"TEXTCOLOR_NORMAL
			   " Chorus: "TEXTCOLOR_YELLOW"%3s",
		voices, polyphony, maxpoly, load, reverb, chorus);
	return out;
}

#ifdef DYN_FLUIDSYNTH

struct LibFunc
{
	void **FuncPointer;
	const char *FuncName;
};

//==========================================================================
//
// FluidSynthMIDIDevice :: LoadFluidSynth
//
// Returns true if the FluidSynth library was successfully loaded.
//
//==========================================================================

bool FluidSynthMIDIDevice::LoadFluidSynth()
{
	LibFunc imports[] =
	{
		{ (void **)&new_fluid_settings,					"new_fluid_settings" },
		{ (void **)&new_fluid_synth,					"new_fluid_synth" },
		{ (void **)&delete_fluid_synth,					"delete_fluid_synth" },
		{ (void **)&delete_fluid_settings,				"delete_fluid_settings" },
		{ (void **)&fluid_settings_setnum,				"fluid_settings_setnum" },
		{ (void **)&fluid_settings_setstr,				"fluid_settings_setstr" },
		{ (void **)&fluid_settings_setint,				"fluid_settings_setint" },
		{ (void **)&fluid_settings_getstr,				"fluid_settings_getstr" },
		{ (void **)&fluid_settings_getint,				"fluid_settings_getint" },
		{ (void **)&fluid_synth_set_interp_method,		"fluid_synth_set_interp_method" },
		{ (void **)&fluid_synth_set_polyphony,			"fluid_synth_set_polyphony" },
		{ (void **)&fluid_synth_get_polyphony,			"fluid_synth_get_polyphony" },
		{ (void **)&fluid_synth_get_active_voice_count,	"fluid_synth_get_active_voice_count" },
		{ (void **)&fluid_synth_get_cpu_load,			"fluid_synth_get_cpu_load" },
		{ (void **)&fluid_synth_system_reset,			"fluid_synth_system_reset" },
		{ (void **)&fluid_synth_noteon,					"fluid_synth_noteon" },
		{ (void **)&fluid_synth_noteoff,				"fluid_synth_noteoff" },
		{ (void **)&fluid_synth_cc,						"fluid_synth_cc" },
		{ (void **)&fluid_synth_program_change,			"fluid_synth_program_change" },
		{ (void **)&fluid_synth_channel_pressure,		"fluid_synth_channel_pressure" },
		{ (void **)&fluid_synth_pitch_bend,				"fluid_synth_pitch_bend" },
		{ (void **)&fluid_synth_write_float,			"fluid_synth_write_float" },
		{ (void **)&fluid_synth_sfload,					"fluid_synth_sfload" },
		{ (void **)&fluid_synth_set_reverb,				"fluid_synth_set_reverb" },
		{ (void **)&fluid_synth_set_chorus,				"fluid_synth_set_chorus" },
	};
	int fail = 0;

#ifdef _WIN32
	FluidSynthDLL = LoadLibrary(FLUIDSYNTHLIB);
	if (FluidSynthDLL == NULL)
	{
		Printf(TEXTCOLOR_RED"Could not load " FLUIDSYNTHLIB "\n");
		return false;
	}
#else
	FluidSynthSO = dlopen(FLUIDSYNTHLIB, RTLD_LAZY);
	if (FluidSynthSO == NULL)
	{
		Printf(TEXTCOLOR_RED"Could not load " FLUIDSYNTHLIB ": %s\n", dlerror());
		return false;
	}
#endif

	for (int i = 0; i < countof(imports); ++i)
	{
#ifdef _WIN32
		FARPROC proc = GetProcAddress(FluidSynthDLL, imports[i].FuncName);
#else
		void *proc = dlsym(FluidSynthSO, imports[i].FuncName);
#endif
		if (proc == NULL)
		{
			Printf(TEXTCOLOR_RED"Failed to find %s in %s\n", imports[i].FuncName, FLUIDSYNTHLIB);
			fail++;
		}
		*imports[i].FuncPointer = proc;
	}
	if (fail == 0)
	{
		return true;
	}
	else
	{
#ifdef _WIN32
		FreeLibrary(FluidSynthDLL);
		FluidSynthDLL = NULL;
#else
		dlclose(FluidSynthSO);
		FluidSynthSO = NULL;
#endif
		return false;
	}

}

//==========================================================================
//
// FluidSynthMIDIDevice :: UnloadFluidSynth
//
//==========================================================================

void FluidSynthMIDIDevice::UnloadFluidSynth()
{
#ifdef _WIN32
	if (FluidSynthDLL != NULL)
	{
		FreeLibrary(FluidSynthDLL);
		FluidSynthDLL = NULL;
	}
#else
	if (FluidSynthSO != NULL)
	{
		dlclose(FluidSynthSO);
		FluidSynthSO = NULL;
	}
#endif
}

#endif

#endif

