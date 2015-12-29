#include "i_musicinterns.h"
#include "c_dispatch.h"
#include "i_music.h"
#include "i_system.h"

#include "templates.h"
#include "v_text.h"
#include "menu/menu.h"

static DWORD	nummididevices;
static bool		nummididevicesset;

#ifdef HAVE_FLUIDSYNTH
#define NUM_DEF_DEVICES 6
#else
#define NUM_DEF_DEVICES 5
#endif

static void AddDefaultMidiDevices(FOptionValues *opt)
{
	int p;
	FOptionValues::Pair *pair = &opt->mValues[opt->mValues.Reserve(NUM_DEF_DEVICES)];
#ifdef HAVE_FLUIDSYNTH
	pair[0].Text = "FluidSynth";
	pair[0].Value = -5.0;
	p = 1;
#else
	p = 0;
#endif
	pair[p].Text = "GUS";
	pair[p].Value = -4.0;
	pair[p+1].Text = "OPL Synth Emulation";
	pair[p+1].Value = -3.0;
	pair[p+2].Text = "TiMidity++";
	pair[p+2].Value = -2.0;
	pair[p+3].Text = "WildMidi";
	pair[p+3].Value = -6.0;
	pair[p+4].Text = "Sound System";
	pair[p+4].Value = -1.0;

}

static void MIDIDeviceChanged(int newdev)
{
	static int oldmididev = INT_MIN;

	// If a song is playing, move it to the new device.
	if (oldmididev != newdev)
	{
		if (currSong != NULL && currSong->IsMIDI())
		{
			MusInfo *song = currSong;
			if (song->m_Status == MusInfo::STATE_Playing)
			{
				song->Stop();
				song->Start(song->m_Looping);
			}
		}
		else
		{
			S_MIDIDeviceChanged();
		}
	}
	oldmididev = newdev;
}

#ifdef _WIN32
UINT mididevice;

CUSTOM_CVAR (Int, snd_mididevice, -1, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (!nummididevicesset)
		return;

	if ((self >= (signed)nummididevices) || (self < -6))
	{
		Printf ("ID out of range. Using default device.\n");
		self = 0;
		return;
	}
	mididevice = MAX<UINT>(0, self);
	MIDIDeviceChanged(self);
}

void I_InitMusicWin32 ()
{
	nummididevices = midiOutGetNumDevs ();
	nummididevicesset = true;
	snd_mididevice.Callback ();
}

void I_ShutdownMusicWin32 ()
{
	// Ancient bug a saw on NT 4.0 and an old version of FMOD 3: If waveout
	// is used for sound and a MIDI is also played, then when I quit, the OS
	// tells me a free block was modified after being freed. This is
	// apparently a synchronization issue between two threads, because if I
	// put this Sleep here after stopping the music but before shutting down
	// the entire sound system, the error does not happen. Observed with a
	// Vortex 2 (may Aureal rest in peace) and an Audigy (damn you, Creative!).
	// I no longer have a system with NT4 drivers, so I don't know if this
	// workaround is still needed or not.
	if (OSPlatform == os_WinNT4)
	{
		Sleep(50);
	}
}

void I_BuildMIDIMenuList (FOptionValues *opt)
{
	AddDefaultMidiDevices(opt);

	for (DWORD id = 0; id < nummididevices; ++id)
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

static void PrintMidiDevice (int id, const char *name, WORD tech, DWORD support)
{
	if (id == snd_mididevice)
	{
		Printf (TEXTCOLOR_BOLD);
	}
	Printf ("% 2d. %s : ", id, name);
	switch (tech)
	{
	case MOD_MIDIPORT:		Printf ("MIDIPORT");		break;
	case MOD_SYNTH:			Printf ("SYNTH");			break;
	case MOD_SQSYNTH:		Printf ("SQSYNTH");			break;
	case MOD_FMSYNTH:		Printf ("FMSYNTH");			break;
	case MOD_MAPPER:		Printf ("MAPPER");			break;
	case MOD_WAVETABLE:		Printf ("WAVETABLE");		break;
	case MOD_SWSYNTH:		Printf ("SWSYNTH");			break;
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

	PrintMidiDevice (-6, "WildMidi", MOD_SWSYNTH, 0);
#ifdef HAVE_FLUIDSYNTH
	PrintMidiDevice (-5, "FluidSynth", MOD_SWSYNTH, 0);
#endif
	PrintMidiDevice (-4, "Gravis Ultrasound Emulation", MOD_SWSYNTH, 0);
	PrintMidiDevice (-3, "Emulated OPL FM Synth", MOD_FMSYNTH, 0);
	PrintMidiDevice (-2, "TiMidity++", MOD_SWSYNTH, 0);
	PrintMidiDevice (-1, "Sound System", 0, 0);
	if (nummididevices != 0)
	{
		for (id = 0; id < nummididevices; ++id)
		{
			res = midiOutGetDevCaps (id, &caps, sizeof(caps));
			if (res == MMSYSERR_NODRIVER)
				strcpy (caps.szPname, "<Driver not installed>");
			else if (res == MMSYSERR_NOMEM)
				strcpy (caps.szPname, "<No memory for description>");
			else if (res != MMSYSERR_NOERROR)
				continue;

			PrintMidiDevice (id, caps.szPname, caps.wTechnology, caps.dwSupport);
		}
	}
}

#else

// Everything but Windows uses this code.

CUSTOM_CVAR(Int, snd_mididevice, -1, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < -6)
		self = -6;
	else if (self > -1)
		self = -1;
	else
		MIDIDeviceChanged(self);
}

void I_BuildMIDIMenuList (FOptionValues *opt)
{
	AddDefaultMidiDevices(opt);
}

CCMD (snd_listmididevices)
{
	Printf("%s-6. WildMidi\n", -6 == snd_mididevice ? TEXTCOLOR_BOLD : "");
#ifdef HAVE_FLUIDSYNTH
	Printf("%s-5. FluidSynth\n", -5 == snd_mididevice ? TEXTCOLOR_BOLD : "");
#endif
	Printf("%s-4. Gravis Ultrasound Emulation\n", -4 == snd_mididevice ? TEXTCOLOR_BOLD : "");
	Printf("%s-3. Emulated OPL FM Synth\n", -3 == snd_mididevice ? TEXTCOLOR_BOLD : "");
	Printf("%s-2. TiMidity++\n", -2 == snd_mididevice ? TEXTCOLOR_BOLD : "");
	Printf("%s-1. Sound System\n", -1 == snd_mididevice ? TEXTCOLOR_BOLD : "");
}
#endif
