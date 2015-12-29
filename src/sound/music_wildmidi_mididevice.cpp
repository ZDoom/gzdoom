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
#include "templates.h"
#include "doomdef.h"
#include "m_swap.h"
#include "w_wad.h"
#include "v_text.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static FString CurrentConfig;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

CVAR(String, wildmidi_config, "", CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Int, wildmidi_frequency, 0, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

// CODE --------------------------------------------------------------------

//==========================================================================
//
// WildMIDIDevice Constructor
//
//==========================================================================

WildMIDIDevice::WildMIDIDevice()
{
	Renderer = NULL;

	if (wildmidi_frequency >= 11025 && wildmidi_frequency < 65536)
	{ // Use our own sample rate instead of the global one
		SampleRate = wildmidi_frequency;
	}
	else
	{ // Else make sure we're not outside of WildMidi's range
		SampleRate = clamp(SampleRate, 11025, 65535);
	}

	if (CurrentConfig.CompareNoCase(wildmidi_config) != 0 || SampleRate != WildMidi_GetSampleRate())
	{
		if (CurrentConfig.IsNotEmpty())
		{
			WildMidi_Shutdown();
			CurrentConfig = "";
		}
		if (!WildMidi_Init(wildmidi_config, SampleRate, WM_MO_ENHANCED_RESAMPLING))
		{
			CurrentConfig = wildmidi_config;
		}
	}
	if (CurrentConfig.IsNotEmpty())
	{
		Renderer = new WildMidi_Renderer();
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

int WildMIDIDevice::Open(void (*callback)(unsigned int, void *, DWORD, DWORD), void *userdata)
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

void WildMIDIDevice::PrecacheInstruments(const WORD *instruments, int count)
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

void WildMIDIDevice::HandleLongEvent(const BYTE *data, int len)
{
	Renderer->LongEvent((const char *)data, len);
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
