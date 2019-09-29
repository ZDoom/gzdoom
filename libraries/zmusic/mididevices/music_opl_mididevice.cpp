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
**
*/

// HEADER FILES ------------------------------------------------------------

#include "mididevice.h"
#include "zmusic/mus2midi.h"
#include "oplsynth/opl.h"
#include "oplsynth/opl_mus_player.h"

// MACROS ------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

OPLConfig oplConfig;

// OPL implementation of a MIDI output device -------------------------------

class OPLMIDIDevice : public SoftSynthMIDIDevice, protected OPLmusicBlock
{
public:
	OPLMIDIDevice(int core);
	int OpenRenderer() override;
	void Close() override;
	int GetTechnology() const override;
	std::string GetStats() override;

protected:
	void CalcTickRate() override;
	int PlayTick() override;
	void HandleEvent(int status, int parm1, int parm2) override;
	void HandleLongEvent(const uint8_t *data, int len) override;
	void ComputeOutput(float *buffer, int len) override;
	bool ServiceStream(void *buff, int numbytes) override;
	int GetDeviceType() const override { return MDEV_OPL; }
};


// PUBLIC DATA DEFINITIONS -------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
//
// OPLMIDIDevice Contructor
//
//==========================================================================

OPLMIDIDevice::OPLMIDIDevice(int core)
	: SoftSynthMIDIDevice((int)OPL_SAMPLE_RATE), OPLmusicBlock(core, oplConfig.numchips)
{
	FullPan = oplConfig.fullpan;
	memcpy(OPLinstruments, oplConfig.OPLinstruments, sizeof(OPLinstruments));
	StreamBlockSize = 14;
}

//==========================================================================
//
// OPLMIDIDevice :: Open
//
// Returns 0 on success.
//
//==========================================================================

int OPLMIDIDevice::OpenRenderer()
{
	if (io == NULL || 0 == (NumChips = io->Init(currentCore, NumChips, FullPan, true)))
	{
		return 1;
	}
	isMono = !FullPan && !io->IsOPL3;
	stopAllVoices();
	resetAllControllers(100);
	return 0;
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
		//DEBUGOUT("Unhandled note aftertouch: Channel %d, note %d, value %d\n", channel, parm1, parm2);
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
			//DEBUGOUT("Unhandled controller: Channel %d, controller %d, value %d\n", channel, parm1, parm2);
			break;
		}
		break;

	case MIDI_PRGMCHANGE:
		programChange(channel, parm1);
		break;

	case MIDI_CHANPRESS:
		//DEBUGOUT("Unhandled channel aftertouch: Channel %d, value %d\n", channel, parm1, 0);
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

std::string OPLMIDIDevice::GetStats()
{
	std::string out;
	for (uint32_t i = 0; i < io->NumChannels; ++i)
	{
		char star = '*';
		if (voices[i].index == ~0u)
		{
			star = '/';
		}
		else if (voices[i].sustained)
		{
			star = '-';
		}
		else if (voices[i].current_instr_voice == &voices[i].current_instr->voices[1])
		{
			star = '\\';
		}
		else
		{
			star = '+';
		}
		out += star;
	}
	return out;
}


MIDIDevice* CreateOplMIDIDevice(const char *Args)
{
	if (!oplConfig.genmidiset) throw std::runtime_error("Cannot play OPL without GENMIDI data");
	int core = oplConfig.core;
	if (Args != NULL && *Args >= '0' && *Args < '4') core = *Args - '0';
	return new OPLMIDIDevice(core);
}
