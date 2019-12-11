/*
** music_midi_base.cpp
**
**---------------------------------------------------------------------------
** Copyright 1998-2010 Randy Heit
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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#endif

#include "c_dispatch.h"

#include "v_text.h"
#include "menu/menu.h"
#include "zmusic/zmusic.h"
#include "s_music.h"

#define DEF_MIDIDEV -5

EXTERN_CVAR(Int, snd_mididevice)
static uint32_t	nummididevices;

#define NUM_DEF_DEVICES 7

static void AddDefaultMidiDevices(FOptionValues *opt)
{
	FOptionValues::Pair *pair = &opt->mValues[opt->mValues.Reserve(NUM_DEF_DEVICES)];
	pair[0].Text = "FluidSynth";
	pair[0].Value = -5.0;
	pair[1].Text = "TiMidity++";
	pair[1].Value = -2.0;
	pair[2].Text = "WildMidi";
	pair[2].Value = -6.0;
	pair[3].Text = "GUS";
	pair[3].Value = -4.0;
	pair[4].Text = "OPL Synth Emulation";
	pair[4].Value = -3.0;
	pair[5].Text = "libADL";
	pair[5].Value = -7.0;
	pair[6].Text = "libOPN";
	pair[6].Value = -8.0;

}

#ifdef _WIN32

void I_InitMusicWin32 ()
{
	nummididevices = midiOutGetNumDevs ();
}

void I_BuildMIDIMenuList (FOptionValues *opt)
{
	AddDefaultMidiDevices(opt);

	for (uint32_t id = 0; id < nummididevices; ++id)
	{
		MIDIOUTCAPS caps;
		MMRESULT res;

		res = midiOutGetDevCaps (id, &caps, sizeof(caps));
		assert(res == MMSYSERR_NOERROR);
		if (res == MMSYSERR_NOERROR)
		{
			FOptionValues::Pair *pair = &opt->mValues[opt->mValues.Reserve(1)];
			pair->Text = caps.szPname;
			pair->Value = (float)id;
		}
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
	if (support & MIDICAPS_CACHE)
	{
		Printf (" CACHE");
	}
	if (support & MIDICAPS_LRVOLUME)
	{
		Printf (" LRVOLUME");
	}
	if (support & MIDICAPS_STREAM)
	{
		Printf (" STREAM");
	}
	if (support & MIDICAPS_VOLUME)
	{
		Printf (" VOLUME");
	}
	Printf (TEXTCOLOR_NORMAL "\n");
}

CCMD (snd_listmididevices)
{
	UINT id;
	MIDIOUTCAPS caps;
	MMRESULT res;

	PrintMidiDevice(-8, "libOPN", MIDIDEV_FMSYNTH, 0);
	PrintMidiDevice(-7, "libADL", MIDIDEV_FMSYNTH, 0);
	PrintMidiDevice (-6, "WildMidi", MIDIDEV_SWSYNTH, 0);
	PrintMidiDevice (-5, "FluidSynth", MIDIDEV_SWSYNTH, 0);
	PrintMidiDevice (-4, "Gravis Ultrasound Emulation", MIDIDEV_SWSYNTH, 0);
	PrintMidiDevice (-3, "Emulated OPL FM Synth", MIDIDEV_FMSYNTH, 0);
	PrintMidiDevice (-2, "TiMidity++", MIDIDEV_SWSYNTH, 0);
	if (nummididevices != 0)
	{
		for (id = 0; id < nummididevices; ++id)
		{
			FString text;
			res = midiOutGetDevCaps (id, &caps, sizeof(caps));
			if (res == MMSYSERR_NODRIVER)
				text = "<Driver not installed>";
			else if (res == MMSYSERR_NOMEM)
				text = "<No memory for description>";
			else if (res == MMSYSERR_NOERROR)
				text = caps.szPname;
			else
				continue;

			PrintMidiDevice (id, text, caps.wTechnology, caps.dwSupport);
		}
	}
}

#else

void I_BuildMIDIMenuList (FOptionValues *opt)
{
	AddDefaultMidiDevices(opt);
}

CCMD (snd_listmididevices)
{
	Printf("%s-8. libOPN\n", -8 == snd_mididevice ? TEXTCOLOR_BOLD : "");
	Printf("%s-7. libADL\n", -7 == snd_mididevice ? TEXTCOLOR_BOLD : "");
	Printf("%s-6. WildMidi\n", -6 == snd_mididevice ? TEXTCOLOR_BOLD : "");
	Printf("%s-5. FluidSynth\n", -5 == snd_mididevice ? TEXTCOLOR_BOLD : "");
	Printf("%s-4. Gravis Ultrasound Emulation\n", -4 == snd_mididevice ? TEXTCOLOR_BOLD : "");
	Printf("%s-3. Emulated OPL FM Synth\n", -3 == snd_mididevice ? TEXTCOLOR_BOLD : "");
	Printf("%s-2. TiMidity++\n", -2 == snd_mididevice ? TEXTCOLOR_BOLD : "");
}
#endif


CUSTOM_CVAR (Int, snd_mididevice, DEF_MIDIDEV, CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL)
{
	if ((self >= (signed)nummididevices) || (self < -8))
	{
		// Don't do repeated message spam if there is no valid device.
		if (self != 0)
		{
			Printf("ID out of range. Using default device.\n");
		}
		self = DEF_MIDIDEV;
		return;
	}
	else if (self == -1)
	{
		self = DEF_MIDIDEV;
		return;
	}
	bool change = ChangeMusicSetting(ZMusic::snd_mididevice, nullptr, self);
	if (change) S_MIDIDeviceChanged(self);
}
