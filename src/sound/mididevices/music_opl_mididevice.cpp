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
#include "w_wad.h"
#include "v_text.h"
#include "doomerrors.h"
#include "opl.h"

// MACROS ------------------------------------------------------------------

#if defined(_DEBUG) && defined(_WIN32) && defined(_MSC_VER)
void I_DebugPrint(const char *cp);
#define DEBUGOUT(m,c,s,t) \
	{ char foo[128]; mysnprintf(foo, countof(foo), m, c, s, t); I_DebugPrint(foo); }
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
	: SoftSynthMIDIDevice((int)OPL_SAMPLE_RATE)
{
	OPL_SetCore(args);
	FullPan = opl_fullpan;
	auto lump = Wads.CheckNumForName("GENMIDI", ns_global);
	if (lump < 0) I_Error("No GENMIDI lump found");
	auto data = Wads.OpenLumpReader(lump);

	uint8_t filehdr[8];
	data.Read(filehdr, 8);
	if (memcmp(filehdr, "#OPL_II#", 8)) I_Error("Corrupt GENMIDI lump");
	data.Read(OPLinstruments, sizeof(GenMidiInstrument) * GENMIDI_NUM_TOTAL);
}

//==========================================================================
//
// OPLMIDIDevice :: Open
//
// Returns 0 on success.
//
//==========================================================================

int OPLMIDIDevice::Open(MidiCallback callback, void *userdata)
{
	if (io == NULL || 0 == (NumChips = io->Init(opl_numchips, FullPan, true)))
	{
		return 1;
	}
	int ret = OpenStream(14, (FullPan || io->IsOPL3) ? 0 : SoundStream::Mono, callback, userdata);
	if (ret == 0)
	{
		stopAllVoices();
		resetAllControllers(100);
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
	io->Reset();
}

//==========================================================================
//
// OPLMIDIDevice :: GetTechnology
//
//==========================================================================

int OPLMIDIDevice::GetTechnology() const
{
	return MIDIDEV_FMSYNTH;
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

	// Swap voices 9 and 15, because their roles are reversed
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
		noteOff(channel, parm1);
		break;

	case MIDI_NOTEON:
		playingcount++;
		noteOn(channel, parm1, parm2);
		break;

	case MIDI_POLYPRESS:
		DEBUGOUT("Unhandled note aftertouch: Channel %d, note %d, value %d\n", channel, parm1, parm2);
		break;

	case MIDI_CTRLCHANGE:
		switch (parm1)
		{
		// some controllers here get passed on but are not handled by the player.
		//case 0:	changeBank(channel, parm2);							break;
		case 1:		changeModulation(channel, parm2);					break;
		case 6:		changeExtended(channel, ctrlDataEntryHi, parm2);	break;
		case 7:		changeVolume(channel, parm2, false);				break;
		case 10:	changePanning(channel, parm2);						break;
		case 11:	changeVolume(channel, parm2, true);					break;
		case 38:	changeExtended(channel, ctrlDataEntryLo, parm2);	break;
		case 64:	changeSustain(channel, parm2);						break;
		//case 67:	changeSoftPedal(channel, parm2);					break;
		//case 91:	changeReverb(channel, parm2);						break;
		//case 93:	changeChorus(channel, parm2);						break;
		case 98:	changeExtended(channel, ctrlNRPNLo, parm2);			break;
		case 99:	changeExtended(channel, ctrlNRPNHi, parm2);			break;
		case 100:	changeExtended(channel, ctrlRPNLo, parm2);			break;
		case 101:	changeExtended(channel, ctrlRPNHi, parm2);			break;
		case 120:	allNotesOff(channel, parm2);						break;
		case 121:	resetControllers(channel, 100);						break;
		case 123:	notesOff(channel, parm2);							break;
		//case 126:	changeMono(channel, parm2);							break;
		//case 127:	changePoly(channel, parm2);							break;
		default:
			DEBUGOUT("Unhandled controller: Channel %d, controller %d, value %d\n", channel, parm1, parm2);
			break;
		}
		break;

	case MIDI_PRGMCHANGE:
		programChange(channel, parm1);
		break;

	case MIDI_CHANPRESS:
		DEBUGOUT("Unhandled channel aftertouch: Channel %d, value %d\n", channel, parm1, 0);
		break;

	case MIDI_PITCHBEND:
		changePitch(channel, parm1, parm2);
		break;
	}
}

//==========================================================================
//
// OPLMIDIDevice :: HandleLongEvent
//
//==========================================================================

void OPLMIDIDevice::HandleLongEvent(const uint8_t *data, int len)
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
	for (uint32_t i = 0; i < io->NumChannels; ++i)
	{
		if (voices[i].index == ~0u)
		{
			star[1] = CR_BRICK + 'A';
		}
		else if (voices[i].sustained)
		{
			star[1] = CR_ORANGE + 'A';
		}
		else if (voices[i].current_instr_voice == &voices[i].current_instr->voices[1])
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
