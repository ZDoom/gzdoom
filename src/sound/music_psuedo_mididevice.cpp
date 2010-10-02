/*
** music_psuedo_mididevice.cpp
** Common base class for psuedo MIDI devices.
**
**---------------------------------------------------------------------------
** Copyright 2008-2010 Randy Heit
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

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
//
// PsuedoMIDIDevice Constructor
//
//==========================================================================

PsuedoMIDIDevice::PsuedoMIDIDevice()
{
	Stream = NULL;
	Started = false;
	bLooping = true;
}

//==========================================================================
//
// PsuedoMIDIDevice Destructor
//
//==========================================================================

PsuedoMIDIDevice::~PsuedoMIDIDevice()
{
	Close();
}

//==========================================================================
//
// PsuedoMIDIDevice :: Close
//
//==========================================================================

void PsuedoMIDIDevice::Close()
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
// PsuedoMIDIDevice :: IsOpen
//
//==========================================================================

bool PsuedoMIDIDevice::IsOpen() const
{
	return Stream != NULL;
}

//==========================================================================
//
// PsuedoMIDIDevice :: GetTechnology
//
//==========================================================================

int PsuedoMIDIDevice::GetTechnology() const
{
	return MOD_MIDIPORT;
}

//==========================================================================
//
// PsuedoMIDIDevice :: Resume
//
//==========================================================================

int PsuedoMIDIDevice::Resume()
{
	if (!Started)
	{
		if (Stream->Play(bLooping, 1))
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
// PsuedoMIDIDevice :: Stop
//
//==========================================================================

void PsuedoMIDIDevice::Stop()
{
	if (Started)
	{
		Stream->Stop();
		Started = false;
	}
}

//==========================================================================
//
// PsuedoMIDIDevice :: Pause
//
//==========================================================================

bool PsuedoMIDIDevice::Pause(bool paused)
{
	if (Stream != NULL)
	{
		return Stream->SetPaused(paused);
	}
	return true;
}

//==========================================================================
//
// PsuedoMIDIDevice :: StreamOutSync
//
//==========================================================================

int PsuedoMIDIDevice::StreamOutSync(MIDIHDR *header)
{
	assert(0);
	return 0;
}

//==========================================================================
//
// PsuedoMIDIDevice :: StreamOut
//
//==========================================================================

int PsuedoMIDIDevice::StreamOut(MIDIHDR *header)
{
	assert(0);
	return 0;
}

//==========================================================================
//
// PsuedoMIDIDevice :: SetTempo
//
//==========================================================================

int PsuedoMIDIDevice::SetTempo(int tempo)
{
	return 0;
}

//==========================================================================
//
// PsuedoMIDIDevice :: SetTimeDiv
//
//==========================================================================

int PsuedoMIDIDevice::SetTimeDiv(int timediv)
{
	return 0;
}

//==========================================================================
//
// PsuedoMIDIDevice :: GetStats
//
//==========================================================================

FString PsuedoMIDIDevice::GetStats()
{
	if (Stream != NULL)
	{
		return Stream->GetStats();
	}
	return "Psuedo MIDI device not open";
}


//==========================================================================
//
// FMODMIDIDevice :: Open
//
//==========================================================================

int FMODMIDIDevice::Open(void (*callback)(unsigned int, void *, DWORD, DWORD), void *userdata)
{
	return 0;
}

//==========================================================================
//
// FMODMIDIDevice :: Preprocess
//
// Create a standard MIDI file and stream it.
//
//==========================================================================

bool FMODMIDIDevice::Preprocess(MIDIStreamer *song, bool looping)
{
	TArray<BYTE> midi;

	song->CreateSMF(midi);
	Stream = GSnd->OpenStream((char *)&midi[0], looping ? SoundStream::Loop : 0, -1, midi.Size());
	return false;
}
