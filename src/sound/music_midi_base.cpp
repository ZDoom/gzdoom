#ifdef _WIN32
#include "i_musicinterns.h"
#include "c_dispatch.h"
#include "i_music.h"

#include "templates.h"
#include "v_text.h"
#include "m_menu.h"

static DWORD	nummididevices;
static bool		nummididevicesset;
	   UINT		mididevice;

CVAR (Bool, snd_midiprecache, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG);

CUSTOM_CVAR (Int, snd_mididevice, -1, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	UINT oldmididev = mididevice;

	if (!nummididevicesset)
		return;

	if ((self >= (signed)nummididevices) || (self < -2))
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
	// I don't know if this is an NT 4.0 bug or an FMOD bug, but if waveout
	// is used for sound, and a MIDI is also played, then when I quit, the OS
	// tells me a free block was modified after being freed. This is
	// apparently a synchronization issue between two threads, because if I
	// put this Sleep here after stopping the music but before shutting down
	// the entire sound system, the error does not happen. I don't think it's
	// a driver problem, because it happens with both a Vortex 2 and an Audigy.
	// Though if their drivers are both based off some common Microsoft sample
	// code, I suppose it could be a driver issue.
	Sleep (50);
}

void I_BuildMIDIMenuList (struct value_s **outValues, float *numValues)
{
	if (*outValues == NULL)
	{
		int count = 1 + nummididevices + (nummididevices > 0);
		value_t *values;

		*outValues = values = new value_t[count];

		values[0].name = "TiMidity++";
		values[0].value = -2.0;
		if (nummididevices > 0)
		{
			UINT id;
			int p;

			values[1].name = "MIDI Mapper";
			values[1].value = -1.0;
			for (id = 0, p = 2; id < nummididevices; ++id)
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
			*numValues = (float)p;
		}
		else
		{
			*numValues = 1.f;
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
#ifdef MOD_WAVETABLE
	case MOD_WAVETABLE:		Printf ("WAVETABLE");		break;
	case MOD_SWSYNTH:		Printf ("SWSYNTH");			break;
#endif
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

	PrintMidiDevice (-2, "TiMidity++", 0, 0);
	if (nummididevices != 0)
	{
		PrintMidiDevice (-1, "MIDI Mapper", MOD_MAPPER, 0);
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
#endif
