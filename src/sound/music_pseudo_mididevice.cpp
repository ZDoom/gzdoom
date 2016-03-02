/*
** music_pseudo_mididevice.cpp
** Common base class for pseudo MIDI devices.
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
#include "files.h"

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
// PseudoMIDIDevice Constructor
//
//==========================================================================

PseudoMIDIDevice::PseudoMIDIDevice()
{
	Stream = NULL;
	Started = false;
	bLooping = true;
}

//==========================================================================
//
// PseudoMIDIDevice Destructor
//
//==========================================================================

PseudoMIDIDevice::~PseudoMIDIDevice()
{
	Close();
}

//==========================================================================
//
// PseudoMIDIDevice :: Close
//
//==========================================================================

void PseudoMIDIDevice::Close()
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
// PseudoMIDIDevice :: IsOpen
//
//==========================================================================

bool PseudoMIDIDevice::IsOpen() const
{
	return Stream != NULL;
}

//==========================================================================
//
// PseudoMIDIDevice :: GetTechnology
//
//==========================================================================

int PseudoMIDIDevice::GetTechnology() const
{
	return MOD_MIDIPORT;
}

//==========================================================================
//
// PseudoMIDIDevice :: Resume
//
//==========================================================================

int PseudoMIDIDevice::Resume()
{
	if (!Started)
	{
		if (Stream && Stream->Play(bLooping, 1))
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
// PseudoMIDIDevice :: Stop
//
//==========================================================================

void PseudoMIDIDevice::Stop()
{
	if (Started)
	{
		if (Stream)
			Stream->Stop();
		Started = false;
	}
}

//==========================================================================
//
// PseudoMIDIDevice :: Pause
//
//==========================================================================

bool PseudoMIDIDevice::Pause(bool paused)
{
	if (Stream != NULL)
	{
		return Stream->SetPaused(paused);
	}
	return true;
}

//==========================================================================
//
// PseudoMIDIDevice :: StreamOutSync
//
//==========================================================================

int PseudoMIDIDevice::StreamOutSync(MIDIHDR *header)
{
	assert(0);
	return 0;
}

//==========================================================================
//
// PseudoMIDIDevice :: StreamOut
//
//==========================================================================

int PseudoMIDIDevice::StreamOut(MIDIHDR *header)
{
	assert(0);
	return 0;
}

//==========================================================================
//
// PseudoMIDIDevice :: SetTempo
//
//==========================================================================

int PseudoMIDIDevice::SetTempo(int tempo)
{
	return 0;
}

//==========================================================================
//
// PseudoMIDIDevice :: SetTimeDiv
//
//==========================================================================

int PseudoMIDIDevice::SetTimeDiv(int timediv)
{
	return 0;
}

//==========================================================================
//
// PseudoMIDIDevice :: GetStats
//
//==========================================================================

FString PseudoMIDIDevice::GetStats()
{
	if (Stream != NULL)
	{
		return Stream->GetStats();
	}
	return "Pseudo MIDI device not open";
}


//==========================================================================
//
// SndSysMIDIDevice :: Open
//
//==========================================================================

int SndSysMIDIDevice::Open(void (*callback)(unsigned int, void *, DWORD, DWORD), void *userdata)
{
	return 0;
}

//==========================================================================
//
// SndSysMIDIDevice :: Preprocess
//
// Create a standard MIDI file and stream it.
//
//==========================================================================

bool SndSysMIDIDevice::Preprocess(MIDIStreamer *song, bool looping)
{
    MemoryArrayReader *reader = new MemoryArrayReader(NULL, 0);
    song->CreateSMF(reader->GetArray(), looping ? 0 : 1);
    reader->UpdateLength();

    bLooping = looping;
    Stream = GSnd->OpenStream(reader, looping ? SoundStream::Loop : 0);
    return false;
}
