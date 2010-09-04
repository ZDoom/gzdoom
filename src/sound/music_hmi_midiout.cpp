/*
** music_midi_midiout.cpp
** Code to let ZDoom play HMI MIDI music through the MIDI streaming API.
**
**---------------------------------------------------------------------------
** Copyright 2010 Randy Heit
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

// MACROS ------------------------------------------------------------------

#define SONG_MAGIC		"HMI-MIDISONG"
#define TRACK_MAGIC		"HMI-MIDITRACK"

// Used by SendCommand to check for unexpected end-of-track conditions.
#define CHECK_FINISHED \
	if (track->TrackP >= track->MaxTrackP) \
	{ \
		track->Finished = true; \
		return events; \
	}

// In song header
#define TRACK_COUNT_OFFSET			0xE4
#define TRACK_DIR_PTR_OFFSET		0xE8

// In track header
#define TRACK_DATA_PTR_OFFSET		0x57
#define TRACK_DESIGNATION_OFFSET	0x99

#define NUM_DESIGNATIONS			8

// MIDI device types for designation
#define HMI_DEV_GM					0xA000	// Generic General MIDI (not a real device)
#define HMI_DEV_MPU401				0xA001	// MPU-401, Roland Sound Canvas, Ensoniq SoundScape, Rolad RAP-10
#define HMI_DEV_OPL2				0xA002	// SoundBlaster (Pro), ESS AudioDrive
#define HMI_DEV_MT32				0xA004	// MT-32
#define HMI_DEV_SBAWE32				0xA008	// SoundBlaster AWE32
#define HMI_DEV_OPL3				0xA009	// SoundBlaster 16, Microsoft Sound System, Pro Audio Spectrum 16
#define HMI_DEV_GUS					0xA00A	// Gravis UltraSound, Gravis UltraSound Max/Ace


// Data accessors, since this data is highly likely to be unaligned.
#if defined(_M_IX86) || defined(_M_X64) || defined(__i386__) 
inline int GetShort(const BYTE *foo)
{
	return *(const short *)foo;
}
inline int GetInt(const BYTE *foo)
{
	return *(const int *)foo;
}
#else
inline int GetShort(const BYTE *foo)
{
	return short(foo[0] | (foo[1] << 8));
}
inline int GetInt(const BYTE *foo)
{
	return int(foo[0] | (foo[1] << 8) | (foo[2] << 16) | (foo[3] << 24));
}
#endif

// TYPES -------------------------------------------------------------------

struct HMISong::TrackInfo
{
	const BYTE *TrackBegin;
	size_t TrackP;
	size_t MaxTrackP;
	DWORD Delay;
	DWORD PlayedTime;
	WORD Designation[NUM_DESIGNATIONS];
	bool Enabled;
	bool Finished;
	BYTE RunningStatus;
    
	DWORD ReadVarLen ();
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern char MIDI_EventLengths[7];
extern char MIDI_CommonLengths[15];

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
//
// HMISong Constructor
//
// Buffers the file and does some validation of the HMI header.
//
//==========================================================================

HMISong::HMISong (FILE *file, BYTE *musiccache, int len, EMIDIDevice type)
: MIDIStreamer(type), MusHeader(0), Tracks(0)
{
	int p;
	int i;

#ifdef _WIN32
	if (ExitEvent == NULL)
	{
		return;
	}
#endif
	if (len < 0x100)
	{ // Way too small to be HMI.
		return;
	}
	MusHeader = new BYTE[len];
	SongLen = len;
	if (file != NULL)
	{
		if (fread(MusHeader, 1, len, file) != (size_t)len)
			return;
	}
	else
	{
		memcpy(MusHeader, musiccache, len);
	}

	// Do some validation of the MIDI file
	if (memcmp(MusHeader, SONG_MAGIC, 12) != 0)
		return;

	NumTracks = GetShort(MusHeader + TRACK_COUNT_OFFSET);
	if (NumTracks <= 0)
	{
		return;
	}

	// The division is the number of pulses per quarter note (PPQN).
	Division = 60;

	Tracks = new TrackInfo[NumTracks + 1];
	int track_dir = GetInt(MusHeader + TRACK_DIR_PTR_OFFSET);

	// Gather information about each track
	for (i = 0, p = 0; i < NumTracks; ++i)
	{
		int start = GetInt(MusHeader + track_dir + i*4);
		int tracklen, datastart;

		if (start > len - TRACK_DESIGNATION_OFFSET - 4)
		{ // Track is incomplete.
			continue;
		}

		// BTW, HMI does not actually check the track header.
		if (memcmp(MusHeader + start, TRACK_MAGIC, 13) != 0)
		{
			continue;
		}

		// The track ends where the next one begins. If this is the
		// last track, then it ends at the end of the file.
		if (i == NumTracks - 1)
		{
			tracklen = len - start;
		}
		else
		{
			tracklen = GetInt(MusHeader + track_dir + i*4 + 4) - start;
		}
		// Clamp incomplete tracks to the end of the file.
		tracklen = MIN(tracklen, len - start);
		if (tracklen <= 0)
		{
			continue;
		}

		// Offset to actual MIDI events.
		datastart = GetInt(MusHeader + start + TRACK_DATA_PTR_OFFSET);
		tracklen -= datastart;
		if (tracklen <= 0)
		{
			continue;
		}

		// Store track information
		Tracks[p].TrackBegin = MusHeader + start + datastart;
		Tracks[p].TrackP = 0;
		Tracks[p].MaxTrackP = tracklen;

		// Retrieve track designations. We can't check them yet, since we have not yet
		// connected to the MIDI device.
		for (int ii = 0; ii < NUM_DESIGNATIONS; ++ii)
		{
			Tracks[p].Designation[ii] = GetShort(MusHeader + start + TRACK_DESIGNATION_OFFSET + ii*2);
		}

		p++;
	}

	// In case there were fewer actual chunks in the file than the
	// header specified, update NumTracks with the current value of p.
	NumTracks = p;

	if (NumTracks == 0)
	{ // No tracks, so nothing to play
		return;
	}
}

//==========================================================================
//
// HMISong Destructor
//
//==========================================================================

HMISong::~HMISong ()
{
	if (Tracks != NULL)
	{
		delete[] Tracks;
	}
	if (MusHeader != NULL)
	{
		delete[] MusHeader;
	}
}

//==========================================================================
//
// HMISong :: CheckCaps
//
// Check track designations and disable tracks that have not been
// designated for the device we will be playing on.
//
//==========================================================================

void HMISong::CheckCaps(int tech)
{
	// What's the equivalent HMI device for our technology?
	if (tech == MOD_FMSYNTH)
	{
		tech = HMI_DEV_OPL3;
	}
	else if (tech == MOD_MIDIPORT)
	{
		tech = HMI_DEV_MPU401;
	}
	else
	{ // Good enough? Or should we just say we're GM.
		tech = HMI_DEV_SBAWE32;
	}

	for (int i = 0; i < NumTracks; ++i)
	{
		Tracks[i].Enabled = false;
		// Track designations are stored in a 0-terminated array.
		for (int j = 0; j < NUM_DESIGNATIONS && Tracks[i].Designation[j] != 0; ++j)
		{
			if (Tracks[i].Designation[j] == tech)
			{
				Tracks[i].Enabled = true;
			}
			// If a track is designated for device 0xA000, it will be played by a MIDI
			// driver for device types 0xA000, 0xA001, and 0xA008. Why this does not
			// include the GUS, I do not know.
			else if (Tracks[i].Designation[j] == HMI_DEV_GM)
			{
				Tracks[i].Enabled = (tech == HMI_DEV_MPU401 || tech == HMI_DEV_SBAWE32);
			}
			// If a track is designated for device 0xA002, it will be played by a MIDI
			// driver for device types 0xA002 or 0xA009.
			else if (Tracks[i].Designation[j] == HMI_DEV_OPL2)
			{
				Tracks[i].Enabled = (tech == HMI_DEV_OPL3);
			}
			// Any other designation must match the specific MIDI driver device number.
			// (Which we handled first above.)

			if (Tracks[i].Enabled)
			{ // This track's been enabled, so we can stop checking other designations.
				break;
			}
		}
	}
}


//==========================================================================
//
// HMISong :: DoInitialSetup
//
// Sets the starting channel volumes.
//
//==========================================================================

void HMISong :: DoInitialSetup()
{
	for (int i = 0; i < 16; ++i)
	{
		ChannelVolumes[i] = 100;
	}
}

//==========================================================================
//
// HMISong :: DoRestart
//
// Rewinds every track.
//
//==========================================================================

void HMISong :: DoRestart()
{
	int i;

	// Set initial state.
	FakeTrack = &Tracks[NumTracks];
	NoteOffs.Clear();
	for (i = 0; i <= NumTracks; ++i)
	{
		Tracks[i].TrackP = 0;
		Tracks[i].Finished = false;
		Tracks[i].RunningStatus = 0;
		Tracks[i].PlayedTime = 0;
	}
	ProcessInitialMetaEvents ();
	for (i = 0; i < NumTracks; ++i)
	{
		Tracks[i].Delay = Tracks[i].ReadVarLen();
	}
	Tracks[i].Delay = 0;	// for the FakeTrack
	Tracks[i].Enabled = true;
	TrackDue = Tracks;
	TrackDue = FindNextDue();
}

//==========================================================================
//
// HMISong :: CheckDone
//
//==========================================================================

bool HMISong::CheckDone()
{
	return TrackDue == NULL;
}

//==========================================================================
//
// HMISong :: MakeEvents
//
// Copies MIDI events from the file and puts them into a MIDI stream
// buffer. Returns the new position in the buffer.
//
//==========================================================================

DWORD *HMISong::MakeEvents(DWORD *events, DWORD *max_event_p, DWORD max_time)
{
	DWORD *start_events;
	DWORD tot_time = 0;
	DWORD time = 0;
	DWORD delay;

	start_events = events;
	while (TrackDue && events < max_event_p && tot_time <= max_time)
	{
		// It's possible that this tick may be nothing but meta-events and
		// not generate any real events. Repeat this until we actually
		// get some output so we don't send an empty buffer to the MIDI
		// device.
		do
		{
			delay = TrackDue->Delay;
			time += delay;
			// Advance time for all tracks by the amount needed for the one up next.
			tot_time += delay * Tempo / Division;
			AdvanceTracks(delay);
			// Play all events for this tick.
			do
			{
				DWORD *new_events = SendCommand(events, TrackDue, time);
				TrackDue = FindNextDue();
				if (new_events != events)
				{
					time = 0;
				}
				events = new_events;
			}
			while (TrackDue && TrackDue->Delay == 0 && events < max_event_p);
		}
		while (start_events == events && TrackDue);
		time = 0;
	}
	return events;
}

//==========================================================================
//
// HMISong :: AdvanceTracks
//
// Advaces time for all tracks by the specified amount.
//
//==========================================================================

void HMISong::AdvanceTracks(DWORD time)
{
	for (int i = 0; i <= NumTracks; ++i)
	{
		if (Tracks[i].Enabled && !Tracks[i].Finished)
		{
			Tracks[i].Delay -= time;
			Tracks[i].PlayedTime += time;
		}
	}
	NoteOffs.AdvanceTime(time);
}

//==========================================================================
//
// HMISong :: SendCommand
//
// Places a single MIDIEVENT in the event buffer.
//
//==========================================================================

DWORD *HMISong::SendCommand (DWORD *events, TrackInfo *track, DWORD delay)
{
	DWORD len;
	BYTE event, data1 = 0, data2 = 0;

	// If the next event comes from the fake track, pop an entry off the note-off queue.
	if (track == FakeTrack)
	{
		AutoNoteOff off;
		NoteOffs.Pop(off);
		events[0] = delay;
		events[1] = 0;
		events[2] = MIDI_NOTEON | off.Channel | (off.Key << 8);
		return events + 3;
	}

	CHECK_FINISHED
	event = track->TrackBegin[track->TrackP++];
	CHECK_FINISHED

	if (event != MIDI_SYSEX && event != MIDI_META && event != MIDI_SYSEXEND && event != 0xFe)
	{
		// Normal short message
		if ((event & 0xF0) == 0xF0)
		{
			if (MIDI_CommonLengths[event & 15] > 0)
			{
				data1 = track->TrackBegin[track->TrackP++];
				if (MIDI_CommonLengths[event & 15] > 1)
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

		if (MIDI_EventLengths[(event&0x70)>>4] == 2)
		{
			data2 = track->TrackBegin[track->TrackP++];
		}

		// Monitor channel volume controller changes.
		if ((event & 0x70) == (MIDI_CTRLCHANGE & 0x70) && data1 == 7)
		{
			data2 = VolumeControllerChange(event & 15, data2);
		}

		events[0] = delay;
		events[1] = 0;
		if (event != MIDI_META)
		{
			events[2] = event | (data1<<8) | (data2<<16);
		}
		else
		{
			events[2] = MEVT_NOP;
		}
		events += 3;

		if ((event & 0x70) == (MIDI_NOTEON & 0x70))
		{ // HMI note on events include the time until an implied note off event.
			NoteOffs.AddNoteOff(track->ReadVarLen(), event & 0x0F, data1);
		}
	}
	else
	{
		// Skip SysEx events just because I don't want to bother with them.
		// The old MIDI player ignored them too, so this won't break
		// anything that played before.
		if (event == MIDI_SYSEX || event == MIDI_SYSEXEND)
		{
			len = track->ReadVarLen ();
			track->TrackP += len;
		}
		else if (event == MIDI_META)
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
				case MIDI_META_EOT:
					track->Finished = true;
					break;

				case MIDI_META_TEMPO:
					Tempo =
						(track->TrackBegin[track->TrackP+0]<<16) |
						(track->TrackBegin[track->TrackP+1]<<8)  |
						(track->TrackBegin[track->TrackP+2]);
					events[0] = delay;
					events[1] = 0;
					events[2] = (MEVT_TEMPO << 24) | Tempo;
					events += 3;
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
		else if (event == 0xFE)
		{ // Skip unknown HMI events.
			event = track->TrackBegin[track->TrackP++];
			CHECK_FINISHED
			if (event == 0x13 || event == 0x15)
			{
				track->TrackP += 6;
			}
			else if (event == 0x12 || event == 0x14)
			{
				track->TrackP += 2;
			}
			else if (event == 0x10)
			{
				track->TrackP += 2;
				CHECK_FINISHED
				track->TrackP += track->TrackBegin[track->TrackP] + 5;
				CHECK_FINISHED
			}
			else
			{ // No idea.
				track->Finished = true;
			}
		}
	}
	if (!track->Finished)
	{
		track->Delay = track->ReadVarLen();
	}
	return events;
}

//==========================================================================
//
// HMISong :: ProcessInitialMetaEvents
//
// Handle all the meta events at the start of each track.
//
//==========================================================================

void HMISong::ProcessInitialMetaEvents ()
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
				case MIDI_META_EOT:
					track->Finished = true;
					break;

				case MIDI_META_TEMPO:
					SetTempo(
						(track->TrackBegin[track->TrackP+0]<<16) |
						(track->TrackBegin[track->TrackP+1]<<8)  |
						(track->TrackBegin[track->TrackP+2])
					);
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

//==========================================================================
//
// HMISong :: TrackInfo :: ReadVarLen
//
// Reads a variable-length SMF number.
//
//==========================================================================

DWORD HMISong::TrackInfo::ReadVarLen ()
{
	DWORD time = 0, t = 0x80;

	while ((t & 0x80) && TrackP < MaxTrackP)
	{
		t = TrackBegin[TrackP++];
		time = (time << 7) | (t & 127);
	}
	return time;
}

//==========================================================================
//
// HMISong :: NoteOffQueue :: AddNoteOff
//
//==========================================================================

void HMISong::NoteOffQueue::AddNoteOff(DWORD delay, BYTE channel, BYTE key)
{
	unsigned int i = Reserve(1);
	while (i > 0 && (*this)[Parent(i)].Delay > delay)
	{
		(*this)[i] = (*this)[Parent(i)];
		i = Parent(i);
	}
	(*this)[i].Delay = delay;
	(*this)[i].Channel = channel;
	(*this)[i].Key = key;
}

//==========================================================================
//
// HMISong :: NoteOffQueue :: Pop
//
//==========================================================================

bool HMISong::NoteOffQueue::Pop(AutoNoteOff &item)
{
	item = (*this)[0];
	if (TArray<AutoNoteOff>::Pop((*this)[0]))
	{
		Heapify();
		return true;
	}
	return false;
}

//==========================================================================
//
// HMISong :: NoteOffQueue :: AdvanceTime
//
//==========================================================================

void HMISong::NoteOffQueue::AdvanceTime(DWORD time)
{
	// Because the time is decreasing by the same amount for every entry,
	// the heap property is maintained.
	for (unsigned int i = 0; i < Size(); ++i)
	{
		assert((*this)[i].Delay >= time);
		(*this)[i].Delay -= time;
	}
}

//==========================================================================
//
// HMISong :: NoteOffQueue :: Heapify
//
//==========================================================================

void HMISong::NoteOffQueue::Heapify()
{
	unsigned int i = 0;
	for (;;)
	{
		unsigned int l = Left(i);
		unsigned int r = Right(i);
		unsigned int smallest = i;
		if (l < Size() && (*this)[l].Delay < (*this)[i].Delay)
		{
			smallest = l;
		}
		if (r < Size() && (*this)[r].Delay < (*this)[smallest].Delay)
		{
			smallest = r;
		}
		if (smallest == i)
		{
			break;
		}
		swapvalues((*this)[i], (*this)[smallest]);
		i = smallest;
	}
}

//==========================================================================
//
// HMISong :: FindNextDue
//
// Scans every track for the next event to play. Returns NULL if all events
// have been consumed.
//
//==========================================================================

HMISong::TrackInfo *HMISong::FindNextDue ()
{
	TrackInfo *track;
	DWORD best;
	int i;

	if (TrackDue != FakeTrack && !TrackDue->Finished && TrackDue->Delay == 0)
	{
		return TrackDue;
	}

	// Check regular tracks.
	track = NULL;
	best = 0xFFFFFFFF;
	for (i = 0; i < NumTracks; ++i)
	{
		if (Tracks[i].Enabled && !Tracks[i].Finished && Tracks[i].Delay < best)
		{
			best = Tracks[i].Delay;
			track = &Tracks[i];
		}
	}
	// Check automatic note-offs.
	if (NoteOffs.Size() != 0 && NoteOffs[0].Delay <= best)
	{
		FakeTrack->Delay = NoteOffs[0].Delay;
		return FakeTrack;
	}
	return track;
}


//==========================================================================
//
// HMISong :: SetTempo
//
// Sets the tempo from a track's initial meta events.
//
//==========================================================================

void HMISong::SetTempo(int new_tempo)
{
	if (0 == MIDI->SetTempo(new_tempo))
	{
		Tempo = new_tempo;
	}
}

//==========================================================================
//
// HMISong :: GetOPLDumper
//
//==========================================================================

MusInfo *HMISong::GetOPLDumper(const char *filename)
{
	return new HMISong(this, filename, MIDI_OPL);
}

//==========================================================================
//
// HMISong :: GetWaveDumper
//
//==========================================================================

MusInfo *HMISong::GetWaveDumper(const char *filename, int rate)
{
	return new HMISong(this, filename, MIDI_GUS);
}

//==========================================================================
//
// HMISong File Dumping Constructor
//
//==========================================================================

HMISong::HMISong(const HMISong *original, const char *filename, EMIDIDevice type)
: MIDIStreamer(filename, type)
{
	SongLen = original->SongLen;
	MusHeader = new BYTE[original->SongLen];
	memcpy(MusHeader, original->MusHeader, original->SongLen);
	NumTracks = original->NumTracks;
	Division = original->Division;
	Tempo = InitialTempo = original->InitialTempo;
	Tracks = new TrackInfo[NumTracks];
	for (int i = 0; i < NumTracks; ++i)
	{
		TrackInfo *newtrack = &Tracks[i];
		const TrackInfo *oldtrack = &original->Tracks[i];

		newtrack->TrackBegin = MusHeader + (oldtrack->TrackBegin - original->MusHeader);
		newtrack->TrackP = 0;
		newtrack->MaxTrackP = oldtrack->MaxTrackP;
	}
}
