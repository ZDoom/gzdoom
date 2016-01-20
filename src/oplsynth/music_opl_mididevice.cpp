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
#include "v_text.h"
#include "opl.h"

// MACROS ------------------------------------------------------------------

#if defined(_DEBUG) && defined(_WIN32) && defined(_MSC_VER)
#define DEBUGOUT(m,c,s,t) \
	{ char foo[128]; mysnprintf(foo, countof(foo), m, c, s, t); OutputDebugString(foo); }
#else
#define DEBUGOUT(m,c,s,t)
#endif

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------
void OPL_SetCore(const char *args);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

EXTERN_CVAR(Int, opl_numchips)

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

CVAR(Bool, opl_fullpan, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG);

// CODE --------------------------------------------------------------------

//==========================================================================
//
// OPLMIDIDevice Contructor
//
//==========================================================================

OPLMIDIDevice::OPLMIDIDevice(const char *args)
{
	OPL_SetCore(args);
	FullPan = opl_fullpan;
	FWadLump data = Wads.OpenLumpName("GENMIDI");
	OPLloadBank(data);
	SampleRate = (int)OPL_SAMPLE_RATE;
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
	if (io == NULL || 0 == (NumChips = io->OPLinit(opl_numchips, FullPan, true)))
	{
		return 1;
	}
	int ret = OpenStream(14, (FullPan || io->IsOPL3) ? 0 : SoundStream::Mono, callback, userdata);
	if (ret == 0)
	{
		OPLstopMusic();
		OPLplayMusic(100);
		DEBUGOUT("========= New song started ==========\n", 0, 0, 0);
	}
	return ret;
}

//==========================================================================
//
// OPLMIDIDevice :: Close
//
//==========================================================================

void OPLMIDIDevice::Close()
{
	SoftSynthMIDIDevice::Close();
	io->OPLdeinit();
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
// OPLMIDIDevice :: CalcTickRate
//
// Tempo is the number of microseconds per quarter note.
// Division is the number of ticks per quarter note.
//
//==========================================================================

void OPLMIDIDevice::CalcTickRate()
{
	SoftSynthMIDIDevice::CalcTickRate();
	io->SetClockRate(OPLmusicBlock::SamplesPerTick = SoftSynthMIDIDevice::SamplesPerTick);
}

//==========================================================================
//
// OPLMIDIDevice :: PlayTick
//
// We derive from two base classes that both define PlayTick(), so we need
// to be unambiguous about which one to use.
//
//==========================================================================

int OPLMIDIDevice::PlayTick()
{
	return SoftSynthMIDIDevice::PlayTick();
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
		case 6:		OPLchangeControl(channel, ctrlDataEntryHi, parm2);	break;
		case 7:		OPLchangeControl(channel, ctrlVolume, parm2);		break;
		case 10:	OPLchangeControl(channel, ctrlPan, parm2);			break;
		case 11:	OPLchangeControl(channel, ctrlExpression, parm2);	break;
		case 38:	OPLchangeControl(channel, ctrlDataEntryLo, parm2);	break;
		case 64:	OPLchangeControl(channel, ctrlSustainPedal, parm2);	break;
		case 67:	OPLchangeControl(channel, ctrlSoftPedal, parm2);	break;
		case 91:	OPLchangeControl(channel, ctrlReverb, parm2);		break;
		case 93:	OPLchangeControl(channel, ctrlChorus, parm2);		break;
		case 98:	OPLchangeControl(channel, ctrlNRPNLo, parm2);		break;
		case 99:	OPLchangeControl(channel, ctrlNRPNHi, parm2);		break;
		case 100:	OPLchangeControl(channel, ctrlRPNLo, parm2);		break;
		case 101:	OPLchangeControl(channel, ctrlRPNHi, parm2);		break;
		case 120:	OPLchangeControl(channel, ctrlSoundsOff, parm2);	break;
		case 121:	OPLresetControllers(channel, 100);					break;
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
// OPLMIDIDevice :: HandleLongEvent
//
//==========================================================================

void OPLMIDIDevice::HandleLongEvent(const BYTE *data, int len)
{
}

//==========================================================================
//
// OPLMIDIDevice :: ComputeOutput
//
// We override ServiceStream, so this function is never actually called.
//
//==========================================================================

void OPLMIDIDevice::ComputeOutput(float *buffer, int len)
{
}

//==========================================================================
//
// OPLMIDIDevice :: ServiceStream
//
//==========================================================================

bool OPLMIDIDevice::ServiceStream(void *buff, int numbytes)
{
	return OPLmusicBlock::ServiceStream(buff, numbytes);
}

//==========================================================================
//
// OPLMIDIDevice :: GetStats
//
//==========================================================================

FString OPLMIDIDevice::GetStats()
{
	FString out;
	char star[3] = { TEXTCOLOR_ESCAPE, 'A', '*' };
	for (uint i = 0; i < io->OPLchannels; ++i)
	{
		if (channels[i].flags & CH_FREE)
		{
			star[1] = CR_BRICK + 'A';
		}
		else if (channels[i].flags & CH_SUSTAIN)
		{
			star[1] = CR_ORANGE + 'A';
		}
		else if (channels[i].flags & CH_SECONDARY)
		{
			star[1] = CR_BLUE + 'A';
		}
		else
		{
			star[1] = CR_GREEN + 'A';
		}
		out.AppendCStrPart (star, 3);
	}
	return out;
}
