#ifdef _WIN32
#include "i_musicinterns.h"
#include "templates.h"
#include "doomdef.h"
#include "m_swap.h"

EXTERN_CVAR (Float, snd_midivolume)

struct MIDISong2::TrackInfo
{
	const BYTE *TrackBegin;
	size_t TrackP;
	size_t MaxTrackP;
	DWORD Delay;
	bool Finished;
	BYTE RunningStatus;
	SBYTE LoopCount;
	bool Designated;
	bool EProgramChange;
	bool EVolume;
	WORD Designation;

	size_t LoopBegin;
	DWORD LoopDelay;
	bool LoopFinished;
    
	DWORD ReadVarLen ();
};

extern DWORD midivolume;
extern UINT mididevice;

static BYTE EventLengths[7] = { 2, 2, 2, 2, 1, 1, 2 };
static BYTE CommonLengths[15] = { 0, 1, 2, 1, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0 };

MIDISong2::MIDISong2 (FILE *file, char * musiccache, int len)
: MidiOut (0), PlayerThread (0),
  PauseEvent (0), ExitEvent (0), VolumeChangeEvent (0),
  MusHeader (0)
{
	int p;
	int i;

	MusHeader = new BYTE[len];
	if (file != NULL)
	{
		if (fread (MusHeader, 1, len, file) != (size_t)len)
			return;
	}
	else
	{
		memcpy(MusHeader, musiccache, len);
	}

	// Do some validation of the MIDI file
	if (MusHeader[4] != 0 || MusHeader[5] != 0 || MusHeader[6] != 0 || MusHeader[7] != 6)
		return;

	if (MusHeader[8] != 0 || MusHeader[9] > 2)
		return;

	Format = MusHeader[9];

	if (Format == 0)
	{
		NumTracks = 1;
	}
	else
	{
		NumTracks = MusHeader[10] * 256 + MusHeader[11];
	}

	// The timers only have millisecond accuracy, not microsecond.
	Division = (MusHeader[12] * 256 + MusHeader[13]) * 1000;

	Tracks = new TrackInfo[NumTracks];

	// Gather information about each track
	for (i = 0, p = 14; i < NumTracks && p < len + 8; ++i)
	{
		DWORD chunkLen =
			(MusHeader[p+4]<<24) |
			(MusHeader[p+5]<<16) |
			(MusHeader[p+6]<<8)  |
			(MusHeader[p+7]);

		if (chunkLen + p + 8 > (DWORD)len)
		{ // Track too long, so truncate it
			chunkLen = len - p - 8;
		}

		if (MusHeader[p+0] == 'M' &&
			MusHeader[p+1] == 'T' &&
			MusHeader[p+2] == 'r' &&
			MusHeader[p+3] == 'k')
		{
			Tracks[i].TrackBegin = MusHeader + p + 8;
			Tracks[i].TrackP = 0;
			Tracks[i].MaxTrackP = chunkLen;
		}

		p += chunkLen + 8;
	}

	// In case there were fewer actual chunks in the file than the
	// header specified, update NumTracks with the current value of i
	NumTracks = i;

	if (NumTracks == 0)
	{ // No tracks, so nothing to play
		return;
	}

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
}

MIDISong2::~MIDISong2 ()
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
	if (Tracks != NULL)
	{
		delete[] Tracks;
	}
	if (MusHeader != NULL)
	{
		delete[] MusHeader;
	}
}

bool MIDISong2::IsMIDI () const
{
	return true;
}

bool MIDISong2::IsValid () const
{
	return PauseEvent != 0;
}

void MIDISong2::Play (bool looping)
{
	MIDIOUTCAPS caps;
	DWORD tid;

	m_Status = STATE_Stopped;
	m_Looping = looping;

	// Find out if this an FM synth or not for EMIDI
	DesignationMask = 0xFF0F;
	if (MMSYSERR_NOERROR == midiOutGetDevCaps (mididevice<0? MIDI_MAPPER:mididevice, &caps, sizeof(caps)))
	{
		if (caps.wTechnology == MOD_FMSYNTH)
		{
			DesignationMask = 0x00F0;
		}
		else if (caps.wTechnology == MOD_MIDIPORT)
		{
			DesignationMask = 0x0001;
		}
	}

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

void MIDISong2::Pause ()
{
	if (m_Status == STATE_Playing)
	{
		SetEvent (PauseEvent);
		m_Status = STATE_Paused;
	}
}

void MIDISong2::Resume ()
{
	if (m_Status == STATE_Paused)
	{
		SetEvent (PauseEvent);
		m_Status = STATE_Playing;
	}
}

void MIDISong2::Stop ()
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

bool MIDISong2::IsPlaying ()
{
	return m_Status != STATE_Stopped;
}

void MIDISong2::SetVolume (float volume)
{
	SetEvent (VolumeChangeEvent);
}

DWORD WINAPI MIDISong2::PlayerProc (LPVOID lpParameter)
{
	MIDISong2 *song = (MIDISong2 *)lpParameter;
	HANDLE events[2] = { song->ExitEvent, song->PauseEvent };
	bool waited = false;
	int i;
	DWORD wait;

	SetThreadPriority (GetCurrentThread (), THREAD_PRIORITY_TIME_CRITICAL);

	for (i = 0; i < 16; ++i)
	{
		// The ASS uses a default volume of 90, but all the other
		// sources I can find say it's 100. Ideally, any song that
		// cares about its volume is going to initialize it to
		// whatever it wants and override this default.
		song->ChannelVolumes[i] = 100;
	}

	song->OutputVolume (midivolume & 0xffff);
	song->Tempo = 500000;

	do
	{
		for (i = 0; i < song->NumTracks; ++i)
		{
			song->Tracks[i].TrackP = 0;
			song->Tracks[i].Finished = false;
			song->Tracks[i].RunningStatus = 0;
			song->Tracks[i].Designated = false;
			song->Tracks[i].Designation = 0;
			song->Tracks[i].LoopCount = -1;
			song->Tracks[i].EProgramChange = false;
			song->Tracks[i].EVolume = false;
		}

		song->ProcessInitialMetaEvents ();

		for (i = 0; i < song->NumTracks; ++i)
		{
			song->Tracks[i].Delay = song->Tracks[i].ReadVarLen ();
		}

		song->TrackDue = song->Tracks;
		song->TrackDue = song->FindNextDue ();

		while (0 != (wait = song->SendCommands ()))
		{
			waited = true;

			// Wait for the exit or pause event or the next note
			switch (WaitForMultipleObjects (2, events, FALSE, wait * song->Tempo / song->Division))
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

			for (i = 0; i < song->NumTracks; ++i)
			{
				if (!song->Tracks[i].Finished)
				{
					song->Tracks[i].Delay -= wait;
				}
			}
			song->TrackDue = song->FindNextDue ();

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

void MIDISong2::OutputVolume (DWORD volume)
{
	for (int i = 0; i < 16; ++i)
	{
		BYTE courseVol = (BYTE)(((ChannelVolumes[i]+1) * volume) >> 16);
		midiOutShortMsg (MidiOut, i | MIDI_CTRLCHANGE | (7<<8) | (courseVol<<16));
	}
}

DWORD MIDISong2::SendCommands ()
{
	while (TrackDue && TrackDue->Delay == 0)
	{
		SendCommand (TrackDue);
		TrackDue = FindNextDue ();
	}
	return TrackDue ? TrackDue->Delay : 0;
}

#define CHECK_FINISHED \
	if (track->TrackP >= track->MaxTrackP) \
	{ \
		track->Finished = true; \
		return; \
	}

void MIDISong2::SendCommand (TrackInfo *track)
{
	DWORD len;
	BYTE event, data1 = 0, data2 = 0;
	int i;

	CHECK_FINISHED
	event = track->TrackBegin[track->TrackP++];
	CHECK_FINISHED

	if (event != 0xF0 && event != 0xFF && event != 0xF7)
	{
		// Normal short message
		if ((event & 0xF0) == 0xF0)
		{
			if (CommonLengths[event & 15] > 0)
			{
				data1 = track->TrackBegin[track->TrackP++];
				if (CommonLengths[event & 15] > 1)
				{
					data2 = track->TrackBegin[track->TrackP++];
				}
			}
		}
		else if ((event & 0x80) == 0)
		{
			data1 = event;
			event = track->RunningStatus;
		}
		else
		{
			track->RunningStatus = event;
			data1 = track->TrackBegin[track->TrackP++];
		}

		CHECK_FINISHED

		if (EventLengths[(event&0x70)>>4] == 2)
		{
			data2 = track->TrackBegin[track->TrackP++];
		}

		switch (event & 0x70)
		{
		case 0x40:
			if (track->EProgramChange)
			{
				event = 0xFF;
			}
			break;

		case 0x30:
			switch (data1)
			{
			case 7:
				if (track->EVolume)
				{
					event = 0xFF;
				}
				else
				{
					// Some devices don't support master volume
					// (e.g. the Audigy's software MIDI synth--but not its two hardware ones),
					// so assume none of them do and scale channel volumes manually.
					ChannelVolumes[event & 15] = data2;
					data2 = (BYTE)(((data2 + 1) * (midivolume & 0xffff)) >> 16);
				}
				break;

			case 39:
				// Skip fine volume adjustment because I am lazy.
				// (And it doesn't seem to be used much anyway.)
				event = 0xFF;
				break;

			case 110:	// EMIDI Track Designation
				// Instruments 4, 5, 6, and 7 are all FM syth.
				// The rest are all wavetable.
				if (data2 == 127)
				{
					track->Designation = ~0;
				}
				else
				{
					if (data2 <= 9)
					{
						track->Designation |= 1 << data2;
					}
				}
				track->Designated = true;
				event = 0xFF;
				break;

			case 111:	// EMIDI Track Exclusion
				if (track->Designated)
				{
					track->Designation &= ~(1 << data2);
				}
				event = 0xFF;
				break;

			case 112:	// EMIDI Program Change
				track->EProgramChange = true;
				event = 0xC0 | (event & 0x0F);
				data1 = data2;
				data2 = 0;
				break;

			case 113:	// EMIDI Volume
				track->EVolume = true;
				data1 = 7;
				ChannelVolumes[event & 15] = data2;
				data2 = (BYTE)(((data2 + 1) * (midivolume & 0xffff)) >> 16);
				break;

			case 116:	// EMIDI Loop Begin
				track->LoopBegin = track->TrackP;
				track->LoopDelay = 0;
				track->LoopCount = data2;
				track->LoopFinished = track->Finished;
				event = 0xFF;
				break;

			case 117:	// EMIDI Loop End
				if (track->LoopCount >= 0 && data2 == 127)
				{
					if (track->LoopCount == 0 && !m_Looping)
					{
						track->Finished = true;
					}
					else
					{
						if (track->LoopCount > 0 && --track->LoopCount == 0)
						{
							track->LoopCount = -1;
						}
						track->TrackP = track->LoopBegin;
						track->Delay = track->LoopDelay;
						track->Finished = track->LoopFinished;
					}
				}
				event = 0xFF;
				break;

			case 118:	// EMIDI Global Loop Begin
				for (i = 0; i < NumTracks; ++i)
				{
					Tracks[i].LoopBegin = Tracks[i].TrackP;
					Tracks[i].LoopDelay = Tracks[i].Delay;
					Tracks[i].LoopCount = data2;
					Tracks[i].LoopFinished = Tracks[i].Finished;
				}
				event = 0xFF;
				break;

			case 119:	// EMIDI Global Loop End
				if (data2 == 127)
				{
					for (i = 0; i < NumTracks; ++i)
					{
						if (Tracks[i].LoopCount >= 0)
						{
							if (Tracks[i].LoopCount == 0 && !m_Looping)
							{
								Tracks[i].Finished = true;
							}
							else
							{
								if (Tracks[i].LoopCount > 0 && --Tracks[i].LoopCount == 0)
								{
									Tracks[i].LoopCount = -1;
								}
								Tracks[i].TrackP = Tracks[i].LoopBegin;
								Tracks[i].Delay = Tracks[i].LoopDelay;
								Tracks[i].Finished = Tracks[i].LoopFinished;
							}
						}
					}
				}
				event = 0xFF;
				break;
			}
		}
		if (event != 0xFF && (!track->Designated || (track->Designation & DesignationMask)))
		{
			if (MMSYSERR_NOERROR != midiOutShortMsg (MidiOut, event | (data1<<8) | (data2<<16)))
			{
				track->Finished = true;
				return;
			}
		}
	}
	else
	{
		// Skip SysEx events just because I don't want to bother with
		// preparing headers and sending them out. The old MIDI player
		// ignores them too, so this won't break anything that played
		// before.
		if (event == 0xF0 || event == 0xF7)
		{
			len = track->ReadVarLen ();
			track->TrackP += len;
		}
		else if (event == 0xFF)
		{
			// It's a meta-event
			event = track->TrackBegin[track->TrackP++];
			CHECK_FINISHED
			len = track->ReadVarLen ();
			CHECK_FINISHED

			if (track->TrackP + len <= track->MaxTrackP)
			{
				switch (event)
				{
				case 0x2F:
					track->Finished = true;
					break;

				case 0x51:
					Tempo =
						(track->TrackBegin[track->TrackP+0]<<16) |
						(track->TrackBegin[track->TrackP+1]<<8)  |
						(track->TrackBegin[track->TrackP+2]);
					break;
				}
				track->TrackP += len;
				if (track->TrackP == track->MaxTrackP)
				{
					track->Finished = true;
				}
			}
			else
			{
				track->Finished = true;
			}
		}
	}
	if (!track->Finished)
	{
		track->Delay = track->ReadVarLen ();
	}
}

#undef CHECK_FINISHED

void MIDISong2::ProcessInitialMetaEvents ()
{
	TrackInfo *track;
	int i;
	BYTE event;
	DWORD len;

	for (i = 0; i < NumTracks; ++i)
	{
		track = &Tracks[i];
		while (!track->Finished &&
				track->TrackP < track->MaxTrackP - 4 &&
				track->TrackBegin[track->TrackP] == 0 &&
				track->TrackBegin[track->TrackP+1] == 0xFF)
		{
			event = track->TrackBegin[track->TrackP+2];
			track->TrackP += 3;
			len = track->ReadVarLen ();
			if (track->TrackP + len <= track->MaxTrackP)
			{
				switch (event)
				{
				case 0x2F:
					track->Finished = true;
					break;

				case 0x51:
					Tempo =
						(track->TrackBegin[track->TrackP+0]<<16) |
						(track->TrackBegin[track->TrackP+1]<<8)  |
						(track->TrackBegin[track->TrackP+2]);
					break;
				}
			}
			track->TrackP += len;
		}
		if (track->TrackP >= track->MaxTrackP - 4)
		{
			track->Finished = true;
		}
	}
}

DWORD MIDISong2::TrackInfo::ReadVarLen ()
{
	DWORD time = 0, t = 0x80;

	while ((t & 0x80) && TrackP < MaxTrackP)
	{
		t = TrackBegin[TrackP++];
		time = (time << 7) | (t & 127);
	}
	return time;
}

MIDISong2::TrackInfo *MIDISong2::FindNextDue ()
{
	TrackInfo *track;
	DWORD best;
	int i;

	if (!TrackDue->Finished && TrackDue->Delay == 0)
	{
		return TrackDue;
	}

	switch (Format)
	{
	case 0:
		return Tracks[0].Finished ? NULL : Tracks;
		
	case 1:
		track = NULL;
		best = 0xFFFFFFFF;
		for (i = 0; i < NumTracks; ++i)
		{
			if (!Tracks[i].Finished)
			{
				if (Tracks[i].Delay < best)
				{
					best = Tracks[i].Delay;
					track = &Tracks[i];
				}
			}
		}
		return track;

	case 2:
		track = TrackDue;
		if (track->Finished)
		{
			track++;
		}
		return track < &Tracks[NumTracks] ? track : NULL;
	}
	return NULL;
}
#endif
