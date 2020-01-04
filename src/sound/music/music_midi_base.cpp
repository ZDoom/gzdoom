/*
** music_midi_base.cpp
**
**---------------------------------------------------------------------------
** Copyright 1998-2010 Randy Heit
** Copyright 2005-2020 Christoph Oelckers
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

#include "c_dispatch.h"

#include "v_text.h"
#include "menu/menu.h"
#include "zmusic/zmusic.h"
#include "s_music.h"

#define DEF_MIDIDEV -5

EXTERN_CVAR(Int, snd_mididevice)

void I_BuildMIDIMenuList(FOptionValues* opt)
{
	int amount;
	auto list = ZMusic_GetMidiDevices(&amount);

	for (int i = 0; i < amount; i++)
	{
		FOptionValues::Pair* pair = &opt->mValues[opt->mValues.Reserve(1)];
		pair->Text = list[i].Name;
		pair->Value = (float)list[i].ID;
	}
}

static void PrintMidiDevice (int id, const char *name, uint16_t tech, uint32_t support)
{
	if (id == snd_mididevice)
	{
		Printf (TEXTCOLOR_BOLD);
	}
	Printf ("% 2d. %s : ", id, name);
	switch (tech)
	{
	case MIDIDEV_MIDIPORT:		Printf ("MIDIPORT");		break;
	case MIDIDEV_SYNTH:			Printf ("SYNTH");			break;
	case MIDIDEV_SQSYNTH:		Printf ("SQSYNTH");			break;
	case MIDIDEV_FMSYNTH:		Printf ("FMSYNTH");			break;
	case MIDIDEV_MAPPER:		Printf ("MAPPER");			break;
	case MIDIDEV_WAVETABLE:		Printf ("WAVETABLE");		break;
	case MIDIDEV_SWSYNTH:		Printf ("SWSYNTH");			break;
	}
	Printf (TEXTCOLOR_NORMAL "\n");
}

CCMD (snd_listmididevices)
{
	int amount;
	auto list = ZMusic_GetMidiDevices(&amount);

	for (int i = 0; i < amount; i++)
	{
		PrintMidiDevice(list[i].ID, list[i].Name, list[i].Technology, 0);
	}
}


CUSTOM_CVAR (Int, snd_mididevice, DEF_MIDIDEV, CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL)
{
	int amount;
	auto list = ZMusic_GetMidiDevices(&amount);

	bool found = false;
	// The list is not necessarily contiguous so we need to check each entry.
	for (int i = 0; i < amount; i++)
	{
		if (self == list[i].ID)
		{
			found = true;
			break;
		}
	}
	if (!found)
	{
		// Don't do repeated message spam if there is no valid device.
		if (self != 0 && self != -1)
		{
			Printf("ID out of range. Using default device.\n");
		}
		if (self != DEF_MIDIDEV) self = DEF_MIDIDEV;
		return;
	}
	bool change = ChangeMusicSetting(zmusic_snd_mididevice, nullptr, self);
	if (change) S_MIDIDeviceChanged(self);
}
