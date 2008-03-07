/*
** music_mididevice.cpp
** Provides a WinMM implementation of a generic MIDI output device.
**
**---------------------------------------------------------------------------
** Copyright 2008 Randy Heit
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

#ifdef _WIN32

// HEADER FILES ------------------------------------------------------------

#include "i_musicinterns.h"
#include "templates.h"
#include "doomdef.h"
#include "m_swap.h"

// MACROS ------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
//
// WinMIDIDevice Contructor
//
//==========================================================================

WinMIDIDevice::WinMIDIDevice(int dev_id)
{
	DeviceID = MAX<DWORD>(dev_id, 0);
	MidiOut = 0;
}

//==========================================================================
//
// WinMIDIDevice Destructor
//
//==========================================================================

WinMIDIDevice::~WinMIDIDevice()
{
	Close();
}

//==========================================================================
//
// WinMIDIDevice :: Open
//
//==========================================================================

int WinMIDIDevice::Open(void (*callback)(UINT, void *, DWORD, DWORD), void *userdata)
{
	MMRESULT err;

	Callback = callback;
	CallbackData = userdata;
	if (MidiOut == NULL)
	{
		err = midiStreamOpen(&MidiOut, &DeviceID, 1, (DWORD_PTR)CallbackFunc, (DWORD_PTR)this, CALLBACK_FUNCTION);

		if (err == MMSYSERR_NOERROR)
		{
			// Set master volume to full, if the device allows it on this interface.
			VolumeWorks = (MMSYSERR_NOERROR == midiOutGetVolume((HMIDIOUT)MidiOut, &SavedVolume));
			if (VolumeWorks)
			{
				VolumeWorks &= (MMSYSERR_NOERROR == midiOutSetVolume((HMIDIOUT)MidiOut, 0xffffffff));
			}
		}
	}
	return 0;
}

//==========================================================================
//
// WinMIDIDevice :: Close
//
//==========================================================================

void WinMIDIDevice::Close()
{
	if (MidiOut != NULL)
	{
		midiStreamClose(MidiOut);
		MidiOut = NULL;
	}
}

//==========================================================================
//
// WinMIDIDevice :: IsOpen
//
//==========================================================================

bool WinMIDIDevice::IsOpen() const
{
	return MidiOut != NULL;
}

//==========================================================================
//
// WinMIDIDevice :: GetTechnology
//
//==========================================================================

int WinMIDIDevice::GetTechnology() const
{
	MIDIOUTCAPS caps;

	if (MMSYSERR_NOERROR == midiOutGetDevCaps(DeviceID, &caps, sizeof(caps)))
	{
		return caps.wTechnology;
	}
	return -1;
}

//==========================================================================
//
// WinMIDIDevice :: SetTempo
//
//==========================================================================

int WinMIDIDevice::SetTempo(int tempo)
{
	MIDIPROPTEMPO data = { sizeof(MIDIPROPTEMPO), tempo };
	return midiStreamProperty(MidiOut, (LPBYTE)&data, MIDIPROP_SET | MIDIPROP_TEMPO);
}

//==========================================================================
//
// WinMIDIDevice :: SetTimeDiv
//
//==========================================================================

int WinMIDIDevice::SetTimeDiv(int timediv)
{
	MIDIPROPTIMEDIV data = { sizeof(MIDIPROPTIMEDIV), timediv };
	return midiStreamProperty(MidiOut, (LPBYTE)&data, MIDIPROP_SET | MIDIPROP_TIMEDIV);
}

//==========================================================================
//
// WinMIDIDevice :: Resume
//
//==========================================================================

int WinMIDIDevice::Resume()
{
	return midiStreamRestart(MidiOut);
}

//==========================================================================
//
// WinMIDIDevice :: Stop
//
//==========================================================================

void WinMIDIDevice::Stop()
{
	midiStreamStop(MidiOut);
	midiOutReset((HMIDIOUT)MidiOut);
	if (VolumeWorks)
	{
		midiOutSetVolume((HMIDIOUT)MidiOut, SavedVolume);
	}
}

//==========================================================================
//
// WinMIDIDevice :: StreamOut
//
//==========================================================================

int WinMIDIDevice::StreamOut(MIDIHDR *header)
{
	return midiStreamOut(MidiOut, header, sizeof(MIDIHDR));
}

//==========================================================================
//
// WinMIDIDevice :: PrepareHeader
//
//==========================================================================

int WinMIDIDevice::PrepareHeader(MIDIHDR *header)
{
	return midiOutPrepareHeader((HMIDIOUT)MidiOut, header, sizeof(MIDIHDR));
}

//==========================================================================
//
// WinMIDIDevice :: UnprepareHeader
//
//==========================================================================

int WinMIDIDevice::UnprepareHeader(MIDIHDR *header)
{
	return midiOutUnprepareHeader((HMIDIOUT)MidiOut, header, sizeof(MIDIHDR));
}

//==========================================================================
//
// WinMIDIDevice :: CallbackFunc									static
//
//==========================================================================

void CALLBACK WinMIDIDevice::CallbackFunc(HMIDIOUT hOut, UINT uMsg, DWORD_PTR dwInstance, DWORD dwParam1, DWORD dwParam2)
{
	WinMIDIDevice *self = (WinMIDIDevice *)dwInstance;
	if (self->Callback != NULL)
	{
		self->Callback(uMsg, self->CallbackData, dwParam1, dwParam2);
	}
}

#endif
