/*
** music_opl_mididevice.cpp
** Provides an emulated OPL implementation of a MIDI output device.
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
** Uses Vladimir Arnost's MUS player library.
*/

// HEADER FILES ------------------------------------------------------------

#include "i_musicinterns.h"
#include "templates.h"
#include "doomdef.h"
#include "m_swap.h"
#include "w_wad.h"

// MACROS ------------------------------------------------------------------

#if defined(_DEBUG) && defined(_WIN32)
#define DEBUGOUT(m,c,s,t) \
	{ char foo[128]; sprintf(foo, m, c, s, t); OutputDebugString(foo); }
#else
#define DEBUGOUT(m,c,s,t)
#endif

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern OPLmusicBlock *BlockForStats;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
//
// OPLMIDIDevice Contructor
//
//==========================================================================

OPLMIDIDevice::OPLMIDIDevice()
{
	Stream = NULL;
	Tempo = 0;
	Division = 0;
	Events = NULL;
	Started = false;

	FWadLump data = Wads.OpenLumpName("GENMIDI");
	OPLloadBank(data);
}

//==========================================================================
//
// OPLMIDIDevice Destructor
//
//==========================================================================

OPLMIDIDevice::~OPLMIDIDevice()
{
	Close();
}

//==========================================================================
//
// OPLMIDIDevice :: Open
//
// Returns 0 on success.
//
//==========================================================================

int OPLMIDIDevice::Open(void (*callback)(unsigned int, void *, DWORD, DWORD), void *userdata)
{
	if (io == NULL || io->OPLinit(TwoChips + 1, uint(OPL_SAMPLE_RATE)))
	{
		return 1;
	}

	Stream = GSnd->CreateStream(FillStream, int(OPL_SAMPLE_RATE / 14) * 4,
		SoundStream::Mono | SoundStream::Float, int(OPL_SAMPLE_RATE), this);
	if (Stream == NULL)
	{
		return 2;
	}

	Callback = callback;
	CallbackData = userdata;
	Tempo = 500000;
	Division = 100;
	CalcTickRate();

	OPLstopMusic();
	OPLplayMusic(100);
	DEBUGOUT("========= New song started ==========\n", 0, 0, 0);

	return 0;
}

//==========================================================================
//
// OPLMIDIDevice :: Close
//
//==========================================================================

void OPLMIDIDevice::Close()
{
	if (Stream != NULL)
	{
		delete Stream;
		Stream = NULL;
	}
	io->OPLdeinit();
	Started = false;
}

//==========================================================================
//
// OPLMIDIDevice :: IsOpen
//
//==========================================================================

bool OPLMIDIDevice::IsOpen() const
{
	return Stream != NULL;
}

//==========================================================================
//
// OPLMIDIDevice :: GetTechnology
//
//==========================================================================

int OPLMIDIDevice::GetTechnology() const
{
	return MOD_FMSYNTH;
}

//==========================================================================
//
// OPLMIDIDevice :: SetTempo
//
//==========================================================================

int OPLMIDIDevice::SetTempo(int tempo)
{
	Tempo = tempo;
	CalcTickRate();
	DEBUGOUT("Tempo changed to %.0f, %.2f samples/tick\n", Tempo, SamplesPerTick, 0);
	return 0;
}

//==========================================================================
//
// OPLMIDIDevice :: SetTimeDiv
//
//==========================================================================

int OPLMIDIDevice::SetTimeDiv(int timediv)
{
	Division = timediv;
	CalcTickRate();
	DEBUGOUT("Division changed to %.0f, %.2f samples/tick\n", Division, SamplesPerTick, 0);
	return 0;
}

//==========================================================================
//
// OPLMIDIDevice :: CalcTickRate
//
// Tempo is the number of microseconds per quarter note.
// Division is the number of ticks per quarter note.
//
//==========================================================================

void OPLMIDIDevice::CalcTickRate()
{
	SamplesPerTick = OPL_SAMPLE_RATE / (1000000.0 / Tempo) / Division;
}

//==========================================================================
//
// OPLMIDIDevice :: Resume
//
//==========================================================================

int OPLMIDIDevice::Resume()
{
	if (!Started)
	{
		if (Stream->Play(true, 1, false))
		{
			Started = true;
			BlockForStats = this;
			return 0;
		}
		return 1;
	}
	return 0;
}

//==========================================================================
//
// OPLMIDIDevice :: Stop
//
//==========================================================================

void OPLMIDIDevice::Stop()
{
	if (Started)
	{
		Stream->Stop();
		Started = false;
		BlockForStats = NULL;
	}
}

//==========================================================================
//
// OPLMIDIDevice :: StreamOutSync
//
// This version is called from the main game thread and needs to
// synchronize with the player thread.
//
//==========================================================================

int OPLMIDIDevice::StreamOutSync(MIDIHDR *header)
{
	Serialize();
	StreamOut(header);
	Unserialize();
	return 0;
}

//==========================================================================
//
// OPLMIDIDevice :: StreamOut
//
// This version is called from the player thread so does not need to
// arbitrate for access to the Events pointer.
//
//==========================================================================

int OPLMIDIDevice::StreamOut(MIDIHDR *header)
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
// OPLMIDIDevice :: PrepareHeader
//
//==========================================================================

int OPLMIDIDevice::PrepareHeader(MIDIHDR *header)
{
	return 0;
}

//==========================================================================
//
// OPLMIDIDevice :: UnprepareHeader
//
//==========================================================================

int OPLMIDIDevice::UnprepareHeader(MIDIHDR *header)
{
	return 0;
}

//==========================================================================
//
// OPLMIDIDevice :: FakeVolume
//
// Since the OPL output is rendered as a normal stream, its volume is
// controlled through the GSnd interface, not here.
//
//==========================================================================

bool OPLMIDIDevice::FakeVolume()
{
	return false;
}

//==========================================================================
//
// OPLMIDIDevice :: NeedThreadedCallabck
//
// OPL can service the callback directly rather than using a separate
// thread.
//
//==========================================================================

bool OPLMIDIDevice::NeedThreadedCallback()
{
	return false;
}

//==========================================================================
//
// OPLMIDIDevice :: Pause
//
//==========================================================================

bool OPLMIDIDevice::Pause(bool paused)
{
	if (Stream != NULL)
	{
		return Stream->SetPaused(paused);
	}
	return true;
}

//==========================================================================
//
// OPLMIDIDevice :: PlayTick
//
// event[0] = delta time
// event[1] = unused
// event[2] = event
//
//==========================================================================

int OPLMIDIDevice::PlayTick()
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
		{ // Should I handle master volume changes?
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
// OPLMIDIDevice :: HandleEvent
//
// Processes a normal MIDI event.
//
//==========================================================================

void OPLMIDIDevice::HandleEvent(int status, int parm1, int parm2)
{
	int command = status & 0xF0;
	int channel = status & 0x0F;

	// Swap channels 9 and 15, because their roles are reversed
	// in MUS and MIDI formats.
	if (channel == 9)
	{
		channel = 15;
	}
	else if (channel == 15)
	{
		channel = 9;
	}

	switch (command)
	{
	case MIDI_NOTEOFF:
		playingcount--;
		OPLreleaseNote(channel, parm1);
		break;

	case MIDI_NOTEON:
		playingcount++;
		OPLplayNote(channel, parm1, parm2);
		break;

	case MIDI_POLYPRESS:
		DEBUGOUT("Unhandled note aftertouch: Channel %d, note %d, value %d\n", channel, parm1, parm2);
		break;

	case MIDI_CTRLCHANGE:
		switch (parm1)
		{
		case 0:		OPLchangeControl(channel, ctrlBank, parm2);			break;
		case 1:		OPLchangeControl(channel, ctrlModulation, parm2);	break;
		case 7:		OPLchangeControl(channel, ctrlVolume, parm2);		break;
		case 10:	OPLchangeControl(channel, ctrlPan, parm2);			break;
		case 11:	OPLchangeControl(channel, ctrlExpression, parm2);	break;
		case 64:	OPLchangeControl(channel, ctrlSustainPedal, parm2);	break;
		case 67:	OPLchangeControl(channel, ctrlSoftPedal, parm2);	break;
		case 91:	OPLchangeControl(channel, ctrlReverb, parm2);		break;
		case 93:	OPLchangeControl(channel, ctrlChorus, parm2);		break;
		case 120:	OPLchangeControl(channel, ctrlSoundsOff, parm2);	break;
		case 121:	OPLchangeControl(channel, ctrlResetCtrls, parm2);	break;
		case 123:	OPLchangeControl(channel, ctrlNotesOff, parm2);		break;
		case 126:	OPLchangeControl(channel, ctrlMono, parm2);			break;
		case 127:	OPLchangeControl(channel, ctrlPoly, parm2);			break;
		default:
			DEBUGOUT("Unhandled controller: Channel %d, controller %d, value %d\n", channel, parm1, parm2);
			break;
		}
		break;

	case MIDI_PRGMCHANGE:
		OPLprogramChange(channel, parm1);
		break;

	case MIDI_CHANPRESS:
		DEBUGOUT("Unhandled channel aftertouch: Channel %d, value %d\n", channel, parm1, 0);
		break;

	case MIDI_PITCHBEND:
		OPLpitchWheel(channel, parm1 | (parm2 << 7));
		break;
	}
}

//==========================================================================
//
// OPLMIDIDevice :: FillStream										static
//
//==========================================================================

bool OPLMIDIDevice::FillStream(SoundStream *stream, void *buff, int len, void *userdata)
{
	OPLMIDIDevice *device = (OPLMIDIDevice *)userdata;
	return device->ServiceStream (buff, len);
}
