/*
** music_opnmidi_mididevice.cpp
** Provides access to libOPNMIDI as a generic MIDI device.
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

// HEADER FILES ------------------------------------------------------------

#include "i_musicinterns.h"
#include "templates.h"
#include "doomdef.h"
#include "m_swap.h"
#include "w_wad.h"
#include "v_text.h"
#include "i_system.h"
#include "opnmidi/opnmidi.h"
#include <errno.h>

enum
{
	ME_NOTEOFF = 0x80,
	ME_NOTEON = 0x90,
	ME_KEYPRESSURE = 0xA0,
	ME_CONTROLCHANGE = 0xB0,
	ME_PROGRAM = 0xC0,
	ME_CHANNELPRESSURE = 0xD0,
	ME_PITCHWHEEL = 0xE0
};

CUSTOM_CVAR(Int, opn_chips_count, 8, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (currSong != nullptr && currSong->GetDeviceType() == MDEV_OPN)
	{
		MIDIDeviceChanged(-1, true);
	}
}

//==========================================================================
//
// OPNMIDIDevice Constructor
//
//==========================================================================

OPNMIDIDevice::OPNMIDIDevice(const char *args)
	:SoftSynthMIDIDevice(44100)
{
	Renderer = opn2_init(44100);	// todo: make it configurable
	if (Renderer != nullptr)
	{
		int lump = Wads.CheckNumForFullName("xg.wopn");
		if (lump < 0)
		{
			I_Error("No OPN bank found");
		}
		FMemLump data = Wads.ReadLump(lump);
		opn2_openBankData(Renderer, data.GetMem(), (long)data.GetSize());
		opn2_setNumChips(Renderer, opn_chips_count);
	}
}

//==========================================================================
//
// OPNMIDIDevice Destructor
//
//==========================================================================

OPNMIDIDevice::~OPNMIDIDevice()
{
	Close();
	if (Renderer != nullptr)
	{
		opn2_close(Renderer);
	}
}

//==========================================================================
//
// OPNMIDIDevice :: Open
//
// Returns 0 on success.
//
//==========================================================================

int OPNMIDIDevice::Open(MidiCallback callback, void *userdata)
{
	int ret = OpenStream(2, 0, callback, userdata);
	if (ret == 0)
	{
		opn2_rt_resetState(Renderer);
	}
	return ret;
}

//==========================================================================
//
// OPNMIDIDevice :: HandleEvent
//
//==========================================================================

void OPNMIDIDevice::HandleEvent(int status, int parm1, int parm2)
{
	int command = status & 0xF0;
	int chan	= status & 0x0F;

	switch (command)
	{
	case ME_NOTEON:
		opn2_rt_noteOn(Renderer, chan, parm1, parm2);
		break;

	case ME_NOTEOFF:
		opn2_rt_noteOff(Renderer, chan, parm1);
		break;

	case ME_KEYPRESSURE:
		opn2_rt_noteAfterTouch(Renderer, chan, parm1, parm2);
		break;

	case ME_CONTROLCHANGE:
		opn2_rt_controllerChange(Renderer, chan, parm1, parm2);
		break;

	case ME_PROGRAM:
		opn2_rt_patchChange(Renderer, chan, parm1);
		break;

	case ME_CHANNELPRESSURE:
		opn2_rt_channelAfterTouch(Renderer, chan, parm1);
		break;

	case ME_PITCHWHEEL:
		opn2_rt_pitchBendML(Renderer, chan, parm2, parm1);
		break;
	}
}

//==========================================================================
//
// OPNMIDIDevice :: HandleLongEvent
//
//==========================================================================

void OPNMIDIDevice::HandleLongEvent(const uint8_t *data, int len)
{
}

//==========================================================================
//
// OPNMIDIDevice :: ComputeOutput
//
//==========================================================================

void OPNMIDIDevice::ComputeOutput(float *buffer, int len)
{
	if ((int)shortbuffer.Size() < len*2) shortbuffer.Resize(len*2);
	auto result = opn2_generate(Renderer, len*2, &shortbuffer[0]);
	for(int i=0; i<result; i++)
	{
		buffer[i] = shortbuffer[i] * (1.f/32768.f);
	}
}

