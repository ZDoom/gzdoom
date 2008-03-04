#ifdef _WIN32
#include "i_musicinterns.h"
#include "templates.h"
#include "doomdef.h"
#include "m_swap.h"

extern DWORD midivolume;
extern UINT mididevice;
extern HANDLE MusicEvent;

#define MAX_TIME		(10)

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

MUSSong2::MUSSong2 (FILE *file, char *musiccache, int len)
: MidiOut(0), MusHeader(0), MusBuffer(0)
{
	MusHeader = (MUSHeader *)new BYTE[len];

	if (file != NULL)
	{
		if (fread(MusHeader, 1, len, file) != (size_t)len)
		{
			return;
		}
	}
	else
	{
		memcpy(MusHeader, musiccache, len);
	}

	// Do some validation of the MUS file
	if (MusHeader->Magic != MAKE_ID('M','U','S','\x1a'))
	{
		return;
	}
	
	if (LittleShort(MusHeader->NumChans) > 15)
	{
		return;
	}

	FullVolEvent.dwDeltaTime = 0;
	FullVolEvent.dwStreamID = 0;
	FullVolEvent.dwEvent = MEVT_LONGMSG | 8;
	FullVolEvent.SysEx[0] = 0xf0;
	FullVolEvent.SysEx[1] = 0x7f;
	FullVolEvent.SysEx[2] = 0x7f;
	FullVolEvent.SysEx[3] = 0x04;
	FullVolEvent.SysEx[4] = 0x01;
	FullVolEvent.SysEx[5] = 0x7f;
	FullVolEvent.SysEx[6] = 0x7f;
	FullVolEvent.SysEx[7] = 0xf7;

	MusBuffer = (BYTE *)MusHeader + LittleShort(MusHeader->SongStart);
	MaxMusP = MIN<int> (LittleShort(MusHeader->SongLen), len - LittleShort(MusHeader->SongStart));
	MusP = 0;
}

MUSSong2::~MUSSong2 ()
{
	Stop ();
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
	UINT dev_id;

	m_Status = STATE_Stopped;
	m_Looping = looping;
	EndQueued = false;
	VolumeChanged = false;
	Restarting = false;

	dev_id = MAX(mididevice, 0u);
	if (MMSYSERR_NOERROR != midiStreamOpen(&MidiOut, &dev_id, 1, (DWORD_PTR)Callback, (DWORD_PTR)this, CALLBACK_FUNCTION))
	{
		Printf(PRINT_BOLD, "Could not open MIDI out device\n");
		return;
	}

	// Set time division and tempo.
	MIDIPROPTIMEDIV timediv = { sizeof(MIDIPROPTIMEDIV), 140 };
	MIDIPROPTEMPO tempo = { sizeof(MIDIPROPTEMPO), 1000000 };

	if (MMSYSERR_NOERROR != midiStreamProperty(MidiOut, (LPBYTE)&timediv, MIDIPROP_SET | MIDIPROP_TIMEDIV) ||
		MMSYSERR_NOERROR != midiStreamProperty(MidiOut, (LPBYTE)&tempo, MIDIPROP_SET | MIDIPROP_TEMPO))
	{
		Printf(PRINT_BOLD, "Setting MIDI stream speed failed\n");
		midiStreamClose(MidiOut);
		MidiOut = NULL;
		return;
	}

	// Try two different methods for setting the stream to full volume.
	// Unfortunately, this isn't as reliable as it once was, which is a pity.
	// The real volume selection is done by setting the volume controller for
	// each channel. Because every General MIDI-compliant device must support
	// this controller, it is the most reliable means of setting the volume.

	VolumeWorks = (MMSYSERR_NOERROR == midiOutGetVolume((HMIDIOUT)MidiOut, &SavedVolume));
	if (VolumeWorks)
	{
		VolumeWorks &= (MMSYSERR_NOERROR == midiOutSetVolume((HMIDIOUT)MidiOut, 0xffffffff));
	}
	if (!VolumeWorks)
	{ // Send the standard SysEx message for full master volume
		memset(&Buffer[0], 0, sizeof(Buffer[0]));
		Buffer[0].lpData = (LPSTR)&FullVolEvent;
		Buffer[0].dwBufferLength = sizeof(FullVolEvent);
		Buffer[0].dwBytesRecorded = sizeof(FullVolEvent);

		if (MMSYSERR_NOERROR == midiOutPrepareHeader((HMIDIOUT)MidiOut, &Buffer[0], sizeof(Buffer[0])))
		{
			midiStreamOut(MidiOut, &Buffer[0], sizeof(Buffer[0]));
		}
		BufferNum = 1;
	}
	else
	{
		BufferNum = 0;
	}

	snd_midivolume.Callback();	// set volume to current music's properties
	for (int i = 0; i < 16; ++i)
	{
		LastVelocity[i] = 64;
		ChannelVolumes[i] = 127;
	}

	// Fill the initial buffers for the song.
	do
	{
		int res = FillBuffer(BufferNum, MAX_EVENTS, MAX_TIME);
		if (res == SONG_MORE)
		{
			if (MMSYSERR_NOERROR != midiStreamOut(MidiOut, &Buffer[BufferNum], sizeof(Buffer[0])))
			{
				Stop();
				return;
			}
			BufferNum ^= 1;
		}
		else if (res == SONG_DONE)
		{
			if (looping)
			{
				MusP = 0;
				if (SONG_MORE == FillBuffer(BufferNum, MAX_EVENTS, MAX_TIME))
				{
					if (MMSYSERR_NOERROR != midiStreamOut(MidiOut, &Buffer[BufferNum], sizeof(MIDIHDR)))
					{
						Stop();
						return;
					}
					BufferNum ^= 1;
				}
				else
				{
					Stop();
					return;
				}
			}
			else
			{
				EndQueued = true;
			}
		}
		else
		{
			Stop();
			return;
		}
	}
	while (BufferNum != 0);

	if (MMSYSERR_NOERROR != midiStreamRestart(MidiOut))
	{
		Stop();
	}
	else
	{
		m_Status = STATE_Playing;
	}
}

void MUSSong2::Pause ()
{
	if (m_Status == STATE_Playing)
	{
		m_Status = STATE_Paused;
		OutputVolume(0);
	}
}

void MUSSong2::Resume ()
{
	if (m_Status == STATE_Paused)
	{
		OutputVolume(midivolume & 0xffff);
		m_Status = STATE_Playing;
	}
}

void MUSSong2::Stop ()
{
	EndQueued = 2;
	if (MidiOut)
	{
		midiStreamStop(MidiOut);
		midiOutReset((HMIDIOUT)MidiOut);
		if (VolumeWorks)
		{
			midiOutSetVolume((HMIDIOUT)MidiOut, SavedVolume);
		}
		midiOutUnprepareHeader((HMIDIOUT)MidiOut, &Buffer[0], sizeof(MIDIHDR));
		midiOutUnprepareHeader((HMIDIOUT)MidiOut, &Buffer[1], sizeof(MIDIHDR));
		midiStreamClose(MidiOut);
		MidiOut = NULL;
	}
	m_Status = STATE_Stopped;
}

bool MUSSong2::IsPlaying ()
{
	return m_Status != STATE_Stopped;
}

void MUSSong2::SetVolume (float volume)
{
	OutputVolume(midivolume & 0xffff);
}

void MUSSong2::OutputVolume (DWORD volume)
{
	NewVolume = volume;
	VolumeChanged = true;
}

void CALLBACK MUSSong2::Callback(HMIDIOUT hOut, UINT uMsg, DWORD_PTR dwInstance, DWORD dwParam1, DWORD dwParam2)
{
	MUSSong2 *self = (MUSSong2 *)dwInstance;

	if (self->EndQueued > 1)
	{
		return;
	}
	if (uMsg == MOM_DONE)
	{
		SetEvent(MusicEvent);
	}
}

void MUSSong2::ServiceEvent()
{
	if (EndQueued == 1)
	{
		Stop();
		return;
	}
	if (MMSYSERR_NOERROR != midiOutUnprepareHeader((HMIDIOUT)MidiOut, &Buffer[BufferNum], sizeof(MIDIHDR)))
	{
		Printf ("Failed unpreparing MIDI header.\n");
		Stop();
		return;
	}
fill:
	switch (FillBuffer(BufferNum, MAX_EVENTS, MAX_TIME))
	{
	case SONG_MORE:
		if (MMSYSERR_NOERROR != midiStreamOut(MidiOut, &Buffer[BufferNum], sizeof(MIDIHDR)))
		{
			Printf ("Failed streaming MIDI buffer.\n");
			Stop();
		}
		else
		{
			BufferNum ^= 1;
		}
		break;

	case SONG_DONE:
		if (m_Looping)
		{
			MusP = 0;
			Restarting = true;
			goto fill;
		}
		EndQueued = 1;
		break;

	default:
		Stop();
		break;
	}
}

// Returns SONG_MORE if the buffer was prepared with data.
// Returns SONG_DONE if the song's end was reached. The buffer will never have data in this case.
// Returns SONG_ERROR if there was a problem preparing the buffer.
int MUSSong2::FillBuffer(int buffer_num, int max_events, DWORD max_time)
{
	if (MusP >= MaxMusP)
	{
		return SONG_DONE;
	}

	int i = 0;
	SHORTMIDIEVENT *events = Events[buffer_num];
	DWORD tot_time = 0;
	DWORD time = 0;

	// If the volume has changed, stick those events at the start of this buffer.
	if (VolumeChanged)
	{
		VolumeChanged = false;
		for (; i < 16; ++i)
		{
			BYTE courseVol = (BYTE)(((ChannelVolumes[i]+1) * NewVolume) >> 16);
			events[i].dwDeltaTime = 0;
			events[i].dwStreamID = 0;
			events[i].dwEvent = MEVT_SHORTMSG | MIDI_CTRLCHANGE | i | (7<<8) | (courseVol<<16);
		}
	}

	// If the song is starting over, stop all notes in case any were left hanging.
	if (Restarting)
	{
		Restarting = false;
		for (int j = 0; j < 16; ++i, ++j)
		{
			events[i].dwDeltaTime = 0;
			events[i].dwStreamID = 0;
			events[i].dwEvent = MEVT_SHORTMSG | MIDI_NOTEOFF | i | (60 << 8) | (64<<16);
		}
	}

	// Play nothing while paused.
	if (m_Status == STATE_Paused)
	{
		time = max_time;
		goto end;
	}

	// The final event is for a NOP to hold the delay from the last event.
	max_events--;

	for (; i < max_events && tot_time <= max_time; ++i)
	{
		BYTE mid1, mid2;
		BYTE channel;
		BYTE t = 0, status;
		BYTE event = MusBuffer[MusP++];
		
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
				LastVelocity[channel] = MusBuffer[MusP++];
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
			MusP = MaxMusP;
			goto end;
		}

		events[i].dwDeltaTime = time;
		events[i].dwStreamID = 0;
		events[i].dwEvent = MEVT_SHORTMSG | status | (mid1 << 8) | (mid2 << 16);

		time = 0;
		if (event & 128)
		{
			do
			{
				t = MusBuffer[MusP++];
				time = (time << 7) | (t & 127);
			}
			while (t & 128);
		}
		tot_time += time;
	}
end:
	if (time != 0)
	{
		events[i].dwDeltaTime = time;
		events[i].dwStreamID = 0;
		events[i].dwEvent = MEVT_NOP;
		i++;
	}
	memset(&Buffer[buffer_num], 0, sizeof(MIDIHDR));
	Buffer[buffer_num].lpData = (LPSTR)events;
	Buffer[buffer_num].dwBufferLength = sizeof(events[0]) * i;
	Buffer[buffer_num].dwBytesRecorded = sizeof(events[0]) * i;
	if (MMSYSERR_NOERROR != midiOutPrepareHeader((HMIDIOUT)MidiOut, &Buffer[buffer_num], sizeof(MIDIHDR)))
	{
		Printf ("Preparing MIDI header failed.\n");
		return SONG_ERROR;
	}
	return SONG_MORE;
}
#endif
