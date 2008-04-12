#include "i_musicinterns.h"
#include "c_dispatch.h"
#include "i_music.h"
#include "i_system.h"

#include "templates.h"
#include "v_text.h"
#include "m_menu.h"

static DWORD	nummididevices;
static bool		nummididevicesset;
#ifdef _WIN32
	   UINT		mididevice;

CUSTOM_CVAR (Int, snd_mididevice, -1, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	UINT oldmididev = mididevice;

	if (!nummididevicesset)
		return;

	if ((self >= (signed)nummididevices) || (self < -4))
	{
		Printf ("ID out of range. Using default device.\n");
		self = 0;
		return;
	}
	else if (self < 0)
	{
		mididevice = 0;
	}
	else
	{
		mididevice = self;
	}

	// If a song is playing, move it to the new device.
	if (oldmididev != mididevice && currSong != NULL && currSong->IsMIDI())
	{
		MusInfo *song = currSong;
		if (song->m_Status == MusInfo::STATE_Playing)
		{
			I_StopSong (song);
			I_PlaySong (song, song->m_Looping);
		}
	}
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

void I_BuildMIDIMenuList (struct value_t **outValues, float *numValues)
{
	if (*outValues == NULL)
	{
		int count = 3 + nummididevices;
		value_t *values;
		UINT id;
		int p;

		*outValues = values = new value_t[count];

		values[0].name = "OPL Synth Emulation";
		values[0].value = -3.0;
		values[1].name = "TiMidity++";
		values[1].value = -2.0;
		values[2].name = "FMOD";
		values[2].value = -1.0;
		for (id = 0, p = 3; id < nummididevices; ++id)
		{
			MIDIOUTCAPS caps;
			MMRESULT res;

			res = midiOutGetDevCaps (id, &caps, sizeof(caps));
			if (res == MMSYSERR_NOERROR)
			{
				size_t len = strlen (caps.szPname) + 1;
				char *name = new char[len];

				memcpy (name, caps.szPname, len);
				values[p].name = name;
				values[p].value = (float)id;
				++p;
			}
		}
		assert(p == count);
		*numValues = float(count);
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

	PrintMidiDevice (-3, "Emulated OPL FM Synth", MOD_FMSYNTH, 0);
	PrintMidiDevice (-2, "TiMidity++", 0, MOD_SWSYNTH);
	PrintMidiDevice (-1, "FMOD", 0, MOD_SWSYNTH);
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
	if (self < -3)
		self = -3;
	else if (self > -1)
		self = -1;
}

void I_BuildMIDIMenuList (struct value_t **outValues, float *numValues)
{
	if (*outValues == NULL)
	{
		value_t *values;

		*outValues = values = new value_t[3];

		values[0].name = "OPL Synth Emulation";
		values[0].value = -3.0;
		values[1].name = "TiMidity++";
		values[1].value = -2.0;
		values[2].name = "FMOD";
		values[2].value = -1.0;
		*numValues = 3.f;
	}
}

CCMD (snd_listmididevices)
{
	Printf("%s-3. Emulated OPL FM Synth\n", -3 == snd_mididevice ? TEXTCOLOR_BOLD : "");
	Printf("%s-2. TiMidity++\n", -2 == snd_mididevice ? TEXTCOLOR_BOLD : "");
	Printf("%s-1. FMOD\n", -1 == snd_mididevice ? TEXTCOLOR_BOLD : "");
}
#endif
