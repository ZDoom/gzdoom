/*
** music_hmi_midiout.cpp
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

// MACROS ------------------------------------------------------------------

#define HMP_NEW_DATE	"013195"
#define HMI_SONG_MAGIC	"HMI-MIDISONG061595"
#define TRACK_MAGIC		"HMI-MIDITRACK"

// Used by SendCommand to check for unexpected end-of-track conditions.
#define CHECK_FINISHED \
	if (track->TrackP >= track->MaxTrackP) \
	{ \
		track->Finished = true; \
		return events; \
	}

// In song header
#define HMI_DIVISION_OFFSET			0xD4
#define HMI_TRACK_COUNT_OFFSET		0xE4
#define HMI_TRACK_DIR_PTR_OFFSET	0xE8

#define HMP_DIVISION_OFFSET			0x38
#define HMP_TRACK_COUNT_OFFSET		0x30
#define HMP_DESIGNATIONS_OFFSET		0x94
#define HMP_TRACK_OFFSET_0			0x308	// original HMP
#define HMP_TRACK_OFFSET_1			0x388	// newer HMP

// In track header
#define HMITRACK_DATA_PTR_OFFSET	0x57
#define HMITRACK_DESIGNATION_OFFSET	0x99

#define HMPTRACK_LEN_OFFSET			4
#define HMPTRACK_DESIGNATION_OFFSET	8
#define HMPTRACK_MIDI_DATA_OFFSET	12

#define NUM_HMP_DESIGNATIONS		5
#define NUM_HMI_DESIGNATIONS		8

// MIDI device types for designation
#define HMI_DEV_GM					0xA000	// Generic General MIDI (not a real device)
#define HMI_DEV_MPU401				0xA001	// MPU-401, Roland Sound Canvas, Ensoniq SoundScape, Rolad RAP-10
#define HMI_DEV_OPL2				0xA002	// SoundBlaster (Pro), ESS AudioDrive
#define HMI_DEV_MT32				0xA004	// MT-32
#define HMI_DEV_SBAWE32				0xA008	// SoundBlaster AWE32
#define HMI_DEV_OPL3				0xA009	// SoundBlaster 16, Microsoft Sound System, Pro Audio Spectrum 16
#define HMI_DEV_GUS					0xA00A	// Gravis UltraSound, Gravis UltraSound Max/Ace

// TYPES -------------------------------------------------------------------

struct HMISong::TrackInfo
{
	const uint8_t *TrackBegin;
	size_t TrackP;
	size_t MaxTrackP;
	uint32_t Delay;
	uint32_t PlayedTime;
	uint16_t Designation[NUM_HMI_DESIGNATIONS];
	bool Enabled;
	bool Finished;
	uint8_t RunningStatus;
    
	uint32_t ReadVarLenHMI();
	uint32_t ReadVarLenHMP();
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

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

HMISong::HMISong (FileReader &reader)
{
    int len = (int)reader.GetLength();
	if (len < 0x100)
	{ // Way too small to be HMI.
		return;
	}
	MusHeader = reader.Read();
	NumTracks = 0;
	if (MusHeader.Size() == 0)
		return;

	// Do some validation of the MIDI file
	if (memcmp(&MusHeader[0], HMI_SONG_MAGIC, sizeof(HMI_SONG_MAGIC)) == 0)
	{
		SetupForHMI(len);
	}
	else if (memcmp(&MusHeader[0], "HMIMIDIP", 8) == 0)
	{
		SetupForHMP(len);
	}
}

//==========================================================================
//
// HMISong Destructor
//
//==========================================================================

HMISong::~HMISong()
{
	if (Tracks != nullptr)
	{
		delete[] Tracks;
	}
}

//==========================================================================
//
// HMISong :: SetupForHMI
//
//==========================================================================

void HMISong::SetupForHMI(int len)
{
	int i, p;

	auto MusPtr = &MusHeader[0];

	ReadVarLen = ReadVarLenHMI;
	NumTracks = GetShort(MusPtr + HMI_TRACK_COUNT_OFFSET);

	if (NumTracks <= 0)
	{
		return;
	}

	// The division is the number of pulses per quarter note (PPQN).
	// HMI files have two values here, a full value and a quarter value. Some games, 
	// notably Quarantines, have identical values for some reason, so it's safer to
	// use the quarter value and multiply it by four than to trust the full value.
	Division = GetShort(MusPtr + HMI_DIVISION_OFFSET) << 2;
	Tempo = InitialTempo = 4000000;

	Tracks = new TrackInfo[NumTracks + 1];
	int track_dir = GetInt(MusPtr + HMI_TRACK_DIR_PTR_OFFSET);

	// Gather information about each track
	for (i = 0, p = 0; i < NumTracks; ++i)
	{
		int start = GetInt(MusPtr + track_dir + i*4);
		int tracklen, datastart;

		if (start > len - HMITRACK_DESIGNATION_OFFSET - 4)
		{ // Track is incomplete.
			continue;
		}

		// BTW, HMI does not actually check the track header.
		if (memcmp(MusPtr + start, TRACK_MAGIC, 13) != 0)
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
			tracklen = GetInt(MusPtr + track_dir + i*4 + 4) - start;
		}
		// Clamp incomplete tracks to the end of the file.
		tracklen = MIN(tracklen, len - start);
		if (tracklen <= 0)
		{
			continue;
		}

		// Offset to actual MIDI events.
		datastart = GetInt(MusPtr + start + HMITRACK_DATA_PTR_OFFSET);
		tracklen -= datastart;
		if (tracklen <= 0)
		{
			continue;
		}

		// Store track information
		Tracks[p].TrackBegin = MusPtr + start + datastart;
		Tracks[p].TrackP = 0;
		Tracks[p].MaxTrackP = tracklen;

		// Retrieve track designations. We can't check them yet, since we have not yet
		// connected to the MIDI device.
		for (int ii = 0; ii < NUM_HMI_DESIGNATIONS; ++ii)
		{
			Tracks[p].Designation[ii] = GetShort(MusPtr + start + HMITRACK_DESIGNATION_OFFSET + ii*2);
		}

		p++;
	}

	// In case there were fewer actual chunks in the file than the
	// header specified, update NumTracks with the current value of p.
	NumTracks = p;
}

//==========================================================================
//
// HMISong :: SetupForHMP
//
//==========================================================================

void HMISong::SetupForHMP(int len)
{
	int track_data;
	int i, p;

	auto MusPtr = &MusHeader[0];

	ReadVarLen = ReadVarLenHMP;
	if (MusPtr[8] == 0)
	{
		track_data = HMP_TRACK_OFFSET_0;
	}
	else if (memcmp(MusPtr + 8, HMP_NEW_DATE, sizeof(HMP_NEW_DATE)) == 0)
	{
		track_data = HMP_TRACK_OFFSET_1;
	}
	else
	{ // unknown HMIMIDIP version
		return;
	}

	NumTracks = GetInt(MusPtr + HMP_TRACK_COUNT_OFFSET);

	if (NumTracks <= 0)
	{
		return;
	}

	// The division is the number of pulses per quarter note (PPQN).
	Division = GetInt(MusPtr + HMP_DIVISION_OFFSET);
	Tempo = InitialTempo = 1000000;

	Tracks = new TrackInfo[NumTracks + 1];

	// Gather information about each track
	for (i = 0, p = 0; i < NumTracks; ++i)
	{
		int start = track_data;
		int tracklen;

		if (start > len - HMPTRACK_MIDI_DATA_OFFSET)
		{ // Track is incomplete.
			break;
		}

		tracklen = GetInt(MusPtr + start + HMPTRACK_LEN_OFFSET);
		track_data += tracklen;

		// Clamp incomplete tracks to the end of the file.
		tracklen = MIN(tracklen, len - start);
		if (tracklen <= 0)
		{
			continue;
		}

		// Subtract track header size.
		tracklen -= HMPTRACK_MIDI_DATA_OFFSET;
		if (tracklen <= 0)
		{
			continue;
		}

		// Store track information
		Tracks[p].TrackBegin = MusPtr + start + HMPTRACK_MIDI_DATA_OFFSET;
		Tracks[p].TrackP = 0;
		Tracks[p].MaxTrackP = tracklen;

		// Retrieve track designations. We can't check them yet, since we have not yet
		// connected to the MIDI device.
#if 0
		// This is completely a guess based on knowledge of how designations work with
		// HMI files. Some songs contain nothing but zeroes for this data, so I'd rather
		// not go around using it without confirmation.

		Printf("Track %d: %d %08x %d:  \034I", i, GetInt(MusPtr + start),
			GetInt(MusPtr + start + 4), GetInt(MusPtr + start + 8));

		int designations = HMP_DESIGNATIONS_OFFSET +
			GetInt(MusPtr + start + HMPTRACK_DESIGNATION_OFFSET) * 4 * NUM_HMP_DESIGNATIONS;
		for (int ii = 0; ii < NUM_HMP_DESIGNATIONS; ++ii)
		{
			Printf(" %04x", GetInt(MusPtr + designations + ii*4));
		}
		Printf("\n");
#endif
		Tracks[p].Designation[0] = HMI_DEV_GM;
		Tracks[p].Designation[1] = HMI_DEV_GUS;
		Tracks[p].Designation[2] = HMI_DEV_OPL2;
		Tracks[p].Designation[3] = 0;

		p++;
	}

	// In case there were fewer actual chunks in the file than the
	// header specified, update NumTracks with the current value of p.
	NumTracks = p;
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
	if (tech == MIDIDEV_FMSYNTH)
	{
		tech = HMI_DEV_OPL3;
	}
	else if (tech == MIDIDEV_MIDIPORT)
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
		for (unsigned int j = 0; j < countof(Tracks[i].Designation) && Tracks[i].Designation[j] != 0; ++j)
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
		Tracks[i].Delay = ReadVarLen(&Tracks[i]);
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
	return TrackDue == nullptr;
}

//==========================================================================
//
// HMISong :: MakeEvents
//
// Copies MIDI events from the file and puts them into a MIDI stream
// buffer. Returns the new position in the buffer.
//
//==========================================================================

uint32_t *HMISong::MakeEvents(uint32_t *events, uint32_t *max_event_p, uint32_t max_time)
{
	uint32_t *start_events;
	uint32_t tot_time = 0;
	uint32_t time = 0;
	uint32_t delay;

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
				bool sysex_noroom = false;
				uint32_t *new_events = SendCommand(events, TrackDue, time, max_event_p - events, sysex_noroom);
				if (sysex_noroom)
				{
					return events;
				}
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
// Advances time for all tracks by the specified amount.
//
//==========================================================================

void HMISong::AdvanceTracks(uint32_t time)
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

uint32_t *HMISong::SendCommand (uint32_t *events, TrackInfo *track, uint32_t delay, ptrdiff_t room, bool &sysex_noroom)
{
	uint32_t len;
	uint8_t event, data1 = 0, data2 = 0;

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

	sysex_noroom = false;
	size_t start_p = track->TrackP;

	CHECK_FINISHED
	event = track->TrackBegin[track->TrackP++];
	CHECK_FINISHED

	// The actual event type will be filled in below. If it's not a NOP,
	// the events pointer will be advanced once the actual event is written.
	// Otherwise, we do it at the end of the function.
	events[0] = delay;
	events[1] = 0;
	events[2] = MEVENT_NOP << 24;

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

		if (event != MIDI_META)
		{
			events[2] = event | (data1<<8) | (data2<<16);
		}

		if (ReadVarLen == ReadVarLenHMI && (event & 0x70) == (MIDI_NOTEON & 0x70))
		{ // HMI note on events include the time until an implied note off event.
			NoteOffs.AddNoteOff(track->ReadVarLenHMI(), event & 0x0F, data1);
		}
	}
	else
	{
		// SysEx events could potentially not have enough room in the buffer...
		if (event == MIDI_SYSEX || event == MIDI_SYSEXEND)
		{
			len = ReadVarLen(track);
			if (len >= (MAX_MIDI_EVENTS-1)*3*4 || skipSysex)
			{ // This message will never fit. Throw it away.
				track->TrackP += len;
			}
			else if (len + 12 >= (size_t)room * 4)
			{ // Not enough room left in this buffer. Backup and wait for the next one.
				track->TrackP = start_p;
				sysex_noroom = true;
				return events;
			}
			else
			{
				uint8_t *msg = (uint8_t *)&events[3];
				if (event == MIDI_SYSEX)
				{ // Need to add the SysEx marker to the message.
					events[2] = (MEVENT_LONGMSG << 24) | (len + 1);
					*msg++ = MIDI_SYSEX;
				}
				else
				{
					events[2] = (MEVENT_LONGMSG << 24) | len;
				}
				memcpy(msg, &track->TrackBegin[track->TrackP], len);
				msg += len;
				// Must pad with 0
				while ((size_t)msg & 3)
				{
					*msg++ = 0;
				}
				track->TrackP += len;
			}
		}
		else if (event == MIDI_META)
		{
			// It's a meta-event
			event = track->TrackBegin[track->TrackP++];
			CHECK_FINISHED
			len = ReadVarLen(track);
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
					events[2] = (MEVENT_TEMPO << 24) | Tempo;
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
		track->Delay = ReadVarLen(track);
	}
	// Advance events pointer unless this is a non-delaying NOP.
	if (events[0] != 0 || MEVENT_EVENTTYPE(events[2]) != MEVENT_NOP)
	{
		if (MEVENT_EVENTTYPE(events[2]) == MEVENT_LONGMSG)
		{
			events += 3 + ((MEVENT_EVENTPARM(events[2]) + 3) >> 2);
		}
		else
		{
			events += 3;
		}
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
	uint8_t event;
	uint32_t len;

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
			len = ReadVarLen(track);
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
// HMISong :: ReadVarLenHMI											static
//
//==========================================================================

uint32_t HMISong::ReadVarLenHMI(TrackInfo *track)
{
	return track->ReadVarLenHMI();
}

//==========================================================================
//
// HMISong :: ReadVarLenHMP											static
//
//==========================================================================

uint32_t HMISong::ReadVarLenHMP(TrackInfo *track)
{
	return track->ReadVarLenHMP();
}

//==========================================================================
//
// HMISong :: TrackInfo :: ReadVarLenHMI
//
// Reads a variable-length SMF number.
//
//==========================================================================

uint32_t HMISong::TrackInfo::ReadVarLenHMI()
{
	uint32_t time = 0, t = 0x80;

	while ((t & 0x80) && TrackP < MaxTrackP)
	{
		t = TrackBegin[TrackP++];
		time = (time << 7) | (t & 127);
	}
	return time;
}

//==========================================================================
//
// HMISong :: TrackInfo :: ReadVarLenHMP
//
// Reads a variable-length HMP number. This is similar to the standard SMF
// variable length number, except it's stored little-endian, and the high
// bit set means the number is done.
//
//==========================================================================

uint32_t HMISong::TrackInfo::ReadVarLenHMP()
{
	uint32_t time = 0;
	uint8_t t = 0;
	int off = 0;

	while (!(t & 0x80) && TrackP < MaxTrackP)
	{
		t = TrackBegin[TrackP++];
		time |= (t & 127) << off;
		off += 7;
	}
	return time;
}

//==========================================================================
//
// NoteOffQueue :: AddNoteOff
//
//==========================================================================

void NoteOffQueue::AddNoteOff(uint32_t delay, uint8_t channel, uint8_t key)
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
// NoteOffQueue :: Pop
//
//==========================================================================

bool NoteOffQueue::Pop(AutoNoteOff &item)
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
// NoteOffQueue :: AdvanceTime
//
//==========================================================================

void NoteOffQueue::AdvanceTime(uint32_t time)
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
// NoteOffQueue :: Heapify
//
//==========================================================================

void NoteOffQueue::Heapify()
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
// Scans every track for the next event to play. Returns nullptr if all events
// have been consumed.
//
//==========================================================================

HMISong::TrackInfo *HMISong::FindNextDue ()
{
	TrackInfo *track;
	uint32_t best;
	int i;

	// Give precedence to whichever track last had events taken from it.
	if (TrackDue != FakeTrack && !TrackDue->Finished && TrackDue->Delay == 0)
	{
		return TrackDue;
	}
	if (TrackDue == FakeTrack && NoteOffs.Size() != 0 && NoteOffs[0].Delay == 0)
	{
		FakeTrack->Delay = 0;
		return FakeTrack;
	}

	// Check regular tracks.
	track = nullptr;
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

