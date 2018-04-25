/*
** music_wildmidi_mididevice.cpp
** Provides access to WildMidi as a generic MIDI device.
**
**---------------------------------------------------------------------------
** Copyright 2015 Randy Heit
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
#include "i_system.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static FString CurrentConfig;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

CUSTOM_CVAR(String, wildmidi_config, "", CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (currSong != nullptr && currSong->GetDeviceType() == MDEV_WILDMIDI)
	{
		MIDIDeviceChanged(-1, true);
	}
}

CVAR(Int, wildmidi_frequency, 0, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CUSTOM_CVAR(Bool, wildmidi_reverb, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	if (currSong != NULL)
		currSong->WildMidiSetOption(WM_MO_REVERB, *self? WM_MO_REVERB:0);
}

CUSTOM_CVAR(Bool, wildmidi_enhanced_resampling, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	if (currSong != NULL)
		currSong->WildMidiSetOption(WM_MO_ENHANCED_RESAMPLING, *self? WM_MO_ENHANCED_RESAMPLING:0);
}

// CODE --------------------------------------------------------------------

//==========================================================================
//
// WildMIDIDevice Constructor
//
//==========================================================================

WildMIDIDevice::WildMIDIDevice(const char *args, int samplerate)
	:SoftSynthMIDIDevice(samplerate <= 0? wildmidi_frequency : samplerate, 11025, 65535)
{
	Renderer = NULL;

	if (args == NULL || *args == 0) args = wildmidi_config;

	if (CurrentConfig.CompareNoCase(args) != 0 || SampleRate != WildMidi_GetSampleRate())
	{
		if (CurrentConfig.IsNotEmpty())
		{
			WildMidi_Shutdown();
			CurrentConfig = "";
		}
		if (!WildMidi_Init(args, SampleRate, 0))
		{
			CurrentConfig = args;
		}
	}
	if (CurrentConfig.IsNotEmpty())
	{
		Renderer = new WildMidi_Renderer();
		int flags = 0;
		if (wildmidi_enhanced_resampling) flags |= WM_MO_ENHANCED_RESAMPLING;
		if (wildmidi_reverb) flags |= WM_MO_REVERB;
		Renderer->SetOption(WM_MO_ENHANCED_RESAMPLING | WM_MO_REVERB, flags);
	}
	else
	{
		I_Error("Failed to load any MIDI patches");
	}
}

//==========================================================================
//
// WildMIDIDevice Destructor
//
//==========================================================================

WildMIDIDevice::~WildMIDIDevice()
{
	Close();
	if (Renderer != NULL)
	{
		delete Renderer;
	}
	// Do not shut down the device so that it can be reused for the next song being played.
}

//==========================================================================
//
// WildMIDIDevice :: Open
//
// Returns 0 on success.
//
//==========================================================================

int WildMIDIDevice::Open(MidiCallback callback, void *userdata)
{
	if (Renderer == NULL)
	{
		return 1;
	}
	int ret = OpenStream(2, 0, callback, userdata);
	if (ret == 0)
	{
//		Renderer->Reset();
	}
	return ret;
}

//==========================================================================
//
// WildMIDIDevice :: PrecacheInstruments
//
// Each entry is packed as follows:
//   Bits 0- 6: Instrument number
//   Bits 7-13: Bank number
//   Bit    14: Select drum set if 1, tone bank if 0
//
//==========================================================================

void WildMIDIDevice::PrecacheInstruments(const uint16_t *instruments, int count)
{
	for (int i = 0; i < count; ++i)
	{
		Renderer->LoadInstrument((instruments[i] >> 7) & 127, instruments[i] >> 14, instruments[i] & 127);
	}
}


//==========================================================================
//
// WildMIDIDevice :: HandleEvent
//
//==========================================================================

void WildMIDIDevice::HandleEvent(int status, int parm1, int parm2)
{
	Renderer->ShortEvent(status, parm1, parm2);
}

//==========================================================================
//
// WildMIDIDevice :: HandleLongEvent
//
//==========================================================================

void WildMIDIDevice::HandleLongEvent(const uint8_t *data, int len)
{
	Renderer->LongEvent(data, len);
}

//==========================================================================
//
// WildMIDIDevice :: ComputeOutput
//
//==========================================================================

void WildMIDIDevice::ComputeOutput(float *buffer, int len)
{
	Renderer->ComputeOutput(buffer, len);
}

//==========================================================================
//
// WildMIDIDevice :: GetStats
//
//==========================================================================

FString WildMIDIDevice::GetStats()
{
	FString out;
	out.Format("%3d voices", Renderer->GetVoiceCount());
	return out;
}

//==========================================================================
//
// WildMIDIDevice :: GetStats
//
//==========================================================================

void WildMIDIDevice::WildMidiSetOption(int opt, int set)
{
	Renderer->SetOption(opt, set);
}
