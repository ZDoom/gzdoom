#ifdef _WIN32
#include "i_musicinterns.h"
#include "templates.h"
#include "doomdef.h"
#include "m_swap.h"

extern DWORD midivolume;
extern UINT mididevice;

EXTERN_CVAR (Float, snd_midivolume)

static const BYTE CtrlTranslate[15] =
{
	0,	// program change
	0,	// bank select
	1,	// modulation pot
	7,	// volume
	10, // pan pot
	11, // expression pot
	91, // reverb depth
	93, // chorus depth
	64, // sustain pedal
	67, // soft pedal
	120, // all sounds off
	123, // all notes off
	126, // mono
	127, // poly
	121, // reset all controllers
};

MUSSong2::MUSSong2 (FILE *file, char * musiccache, int len)
: MidiOut (0), PlayerThread (0),
  PauseEvent (0), ExitEvent (0), VolumeChangeEvent (0),
  MusBuffer (0), MusHeader (0)
{
	MusHeader = (MUSHeader *)new BYTE[len];

	if (file != NULL)
	{
		if (fread (MusHeader, 1, len, file) != (size_t)len)
			return;
	}
	else
	{
		memcpy(MusHeader, musiccache, len);
	}

	// Do some validation of the MUS file
	if (MusHeader->Magic != MAKE_ID('M','U','S','\x1a'))
		return;
	
	if (LittleShort(MusHeader->NumChans) > 15)
		return;

	ExitEvent = CreateEvent (NULL, FALSE, FALSE, NULL);
	if (ExitEvent == NULL)
	{
		Printf (PRINT_BOLD, "Could not create exit event for MIDI playback\n");
		return;
	}
	VolumeChangeEvent = CreateEvent (NULL, FALSE, FALSE, NULL);
	if (VolumeChangeEvent == NULL)
	{
		Printf (PRINT_BOLD, "Could not create volume event for MIDI playback\n");
		return;
	}
	PauseEvent = CreateEvent (NULL, FALSE, FALSE, NULL);
	if (PauseEvent == NULL)
	{
		Printf (PRINT_BOLD, "Could not create pause event for MIDI playback\n");
	}

	MusBuffer = (BYTE *)MusHeader + LittleShort(MusHeader->SongStart);
	MaxMusP = MIN<int> (LittleShort(MusHeader->SongLen), len - LittleShort(MusHeader->SongStart));
	MusP = 0;
}

MUSSong2::~MUSSong2 ()
{
	Stop ();

	if (PauseEvent != NULL)
	{
		CloseHandle (PauseEvent);
	}
	if (ExitEvent != NULL)
	{
		CloseHandle (ExitEvent);
	}
	if (VolumeChangeEvent != NULL)
	{
		CloseHandle (VolumeChangeEvent);
	}
	if (MusHeader != 0)
	{
		delete[] ((BYTE *)MusHeader);
	}
}

bool MUSSong2::IsMIDI () const
{
	return true;
}

bool MUSSong2::IsValid () const
{
	return MusBuffer != 0;
}

void MUSSong2::Play (bool looping)
{
	DWORD tid;

	m_Status = STATE_Stopped;
	m_Looping = looping;

	if (MMSYSERR_NOERROR != midiOutOpen (&MidiOut, mididevice<0? MIDI_MAPPER:mididevice, 0, 0, CALLBACK_NULL))
	{
		Printf (PRINT_BOLD, "Could not open MIDI out device\n");
		return;
	}

	// Try two different methods for setting the stream to full volume.
	// Unfortunately, this isn't as reliable as it once was, which is a pity.
	// The real volume selection is done by setting the volume controller for
	// each channel. Because every General MIDI-compliant device must support
	// this controller, it is the most reliable means of setting the volume.

	VolumeWorks = (MMSYSERR_NOERROR == midiOutGetVolume (MidiOut, &SavedVolume));
	if (VolumeWorks)
	{
		VolumeWorks &= (MMSYSERR_NOERROR == midiOutSetVolume (MidiOut, 0xffffffff));
	}
	else
	{
		// Send the standard SysEx message for full master volume
		BYTE volmess[] = { 0xf0, 0x7f, 0x7f, 0x04, 0x01, 0x7f, 0x7f, 0xf7 };
		MIDIHDR hdr = { (LPSTR)volmess, sizeof(volmess), };

		if (MMSYSERR_NOERROR == midiOutPrepareHeader (MidiOut, &hdr, sizeof(hdr)))
		{
			midiOutLongMsg (MidiOut, &hdr, sizeof(hdr));
			while (MIDIERR_STILLPLAYING == midiOutUnprepareHeader (MidiOut, &hdr, sizeof(hdr)))
			{
				Sleep (10);
			}
		}
	}

	snd_midivolume.Callback();	// set volume to current music's properties
	PlayerThread = CreateThread (NULL, 0, PlayerProc, this, 0, &tid);
	if (PlayerThread == NULL)
	{
		if (VolumeWorks)
		{
			midiOutSetVolume (MidiOut, SavedVolume);
		}
		midiOutClose (MidiOut);
		MidiOut = NULL;
	}

	m_Status = STATE_Playing;
}

void MUSSong2::Pause ()
{
	if (m_Status == STATE_Playing)
	{
		SetEvent (PauseEvent);
		m_Status = STATE_Paused;
	}
}

void MUSSong2::Resume ()
{
	if (m_Status == STATE_Paused)
	{
		SetEvent (PauseEvent);
		m_Status = STATE_Playing;
	}
}

void MUSSong2::Stop ()
{
	if (PlayerThread)
	{
		SetEvent (ExitEvent);
		WaitForSingleObject (PlayerThread, INFINITE);
		CloseHandle (PlayerThread);
		PlayerThread = NULL;
	}
	if (MidiOut)
	{
		midiOutReset (MidiOut);
		if (VolumeWorks)
		{
			midiOutSetVolume (MidiOut, SavedVolume);
		}
		midiOutClose (MidiOut);
		MidiOut = NULL;
	}
}

bool MUSSong2::IsPlaying ()
{
	return m_Status != STATE_Stopped;
}

void MUSSong2::SetVolume (float volume)
{
	SetEvent (VolumeChangeEvent);
}

DWORD WINAPI MUSSong2::PlayerProc (LPVOID lpParameter)
{
	MUSSong2 *song = (MUSSong2 *)lpParameter;
	HANDLE events[2] = { song->ExitEvent, song->PauseEvent };
	bool waited;
	int i;

	SetThreadPriority (GetCurrentThread (), THREAD_PRIORITY_TIME_CRITICAL);

	for (i = 0; i < 16; ++i)
	{
		song->LastVelocity[i] = 64;
		song->ChannelVolumes[i] = 127;
	}

	song->OutputVolume (midivolume & 0xffff);

	do
	{
		waited = false;
		song->MusP = 0;

		while (song->SendCommand () == SEND_WAIT)
		{
			DWORD time = 0;
			BYTE t;
			do
			{
				t = song->MusBuffer[song->MusP++];
				time = (time << 7) | (t & 127);
			}
			while (t & 128);

			waited = true;

			// Wait for the exit or pause event or the next note
			switch (WaitForMultipleObjects (2, events, FALSE, time * 1000 / 140))
			{
			case WAIT_OBJECT_0:
				song->m_Status = STATE_Stopped;
				return 0;

			case WAIT_OBJECT_0+1:
				// Go paused
				song->OutputVolume (0);
				// Wait for the exit or pause event
				if (WAIT_OBJECT_0 == WaitForMultipleObjects (2, events, FALSE, INFINITE))
				{
					song->m_Status = STATE_Stopped;
					return 0;
				}
				song->OutputVolume (midivolume & 0xffff);
			}

			// Check if the volume needs changing
			if (WAIT_OBJECT_0 == WaitForSingleObject (song->VolumeChangeEvent, 0))
			{
				song->OutputVolume (midivolume & 0xffff);
			}
		}
	}
	while (waited && song->m_Looping);

	song->m_Status = STATE_Stopped;
	return 0;
}

void MUSSong2::OutputVolume (DWORD volume)
{
	for (int i = 0; i < 16; ++i)
	{
		BYTE courseVol = (BYTE)(((ChannelVolumes[i]+1) * volume) >> 16);
		midiOutShortMsg (MidiOut, i | MIDI_CTRLCHANGE | (7<<8) | (courseVol<<16));
	}
}

int MUSSong2::SendCommand ()
{
	BYTE event = 0;

	if (MusP >= MaxMusP)
		return SEND_DONE;

	while (MusP < MaxMusP && (event & 0x70) != MUS_SCOREEND)
	{
		BYTE mid1, mid2;
		BYTE channel;
		BYTE t = 0, status;
		
		event = MusBuffer[MusP++];
		
		if ((event & 0x70) != MUS_SCOREEND)
		{
			t = MusBuffer[MusP++];
		}
		channel = event & 15;

		// Map MUS channels to MIDI channels
		if (channel == 15)
		{
			channel = 9;
		}
		else if (channel >= 9)
		{
			channel = channel + 1;
		}

		status = channel;

		switch (event & 0x70)
		{
		case MUS_NOTEOFF:
			status |= MIDI_NOTEOFF;
			mid1 = t;
			mid2 = 64;
			break;
			
		case MUS_NOTEON:
			status |= MIDI_NOTEON;
			mid1 = t & 127;
			if (t & 128)
			{
				LastVelocity[channel] = MusBuffer[MusP++];;
			}
			mid2 = LastVelocity[channel];
			break;
			
		case MUS_PITCHBEND:
			status |= MIDI_PITCHBEND;
			mid1 = (t & 1) << 6;
			mid2 = (t >> 1) & 127;
			break;
			
		case MUS_SYSEVENT:
			status |= MIDI_CTRLCHANGE;
			mid1 = CtrlTranslate[t];
			mid2 = t == 12 ? LittleShort(MusHeader->NumChans) : 0;
			break;
			
		case MUS_CTRLCHANGE:
			if (t == 0)
			{ // program change
				status |= MIDI_PRGMCHANGE;
				mid1 = MusBuffer[MusP++];
				mid2 = 0;
			}
			else
			{
				status |= MIDI_CTRLCHANGE;
				mid1 = CtrlTranslate[t];
				mid2 = MusBuffer[MusP++];

				// Some devices don't support master volume
				// (e.g. the Audigy's software MIDI synth--but not its two hardware ones),
				// so assume none of them do and scale channel volumes manually.
				if (mid1 == 7)
				{
					ChannelVolumes[channel] = mid2;
					mid2 = (BYTE)(((mid2 + 1) * (midivolume & 0xffff)) >> 16);
				}
			}
			break;
			
		case MUS_SCOREEND:
		default:
			return SEND_DONE;
			break;
		}

		if (MMSYSERR_NOERROR != midiOutShortMsg (MidiOut, status | (mid1 << 8) | (mid2 << 16)))
		{
			return SEND_DONE;
		}
		if (event & 128)
		{
			return SEND_WAIT;
		}
	}

	return SEND_DONE;
}
#endif
