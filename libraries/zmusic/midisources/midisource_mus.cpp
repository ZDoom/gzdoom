/*
** music_mus_midiout.cpp
** Code to let ZDoom play MUS music through the MIDI streaming API.
**
**---------------------------------------------------------------------------
** Copyright 1998-2008 Randy Heit
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
*/

// HEADER FILES ------------------------------------------------------------

#include <algorithm>
#include "midisource.h"
#include "zmusic/m_swap.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

int MUSHeaderSearch(const uint8_t *head, int len);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static const uint8_t CtrlTranslate[15] =
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

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
//
// MUSSong2 Constructor
//
// Performs some validity checks on the MUS file, buffers it, and creates
// the playback thread control events.
//
//==========================================================================

MUSSong2::MUSSong2 (const uint8_t *data, size_t len)
{
	int start;

	// To tolerate sloppy wads (diescum.wad, I'm looking at you), we search
	// the first 32 bytes of the file for a signature. My guess is that DMX
	// does no validation whatsoever and just assumes it was passed a valid
	// MUS file, since where the header is offset affects how it plays.
	start = MUSHeaderSearch(data, 32);
	if (start < 0)
	{
		return;
	}
	data += start;
	len -= start;

	// Read the remainder of the song.
	if (len < sizeof(MUSHeader))
	{ // It's too short.
		return;
	}
	MusData.resize(len);
	memcpy(MusData.data(), data, len);
	auto MusHeader = (MUSHeader*)MusData.data();

	// Do some validation of the MUS file.
	if (LittleShort(MusHeader->NumChans) > 15)
	{
		return;
	}

	MusBuffer = MusData.data() + LittleShort(MusHeader->SongStart);
	MaxMusP = std::min<int>(LittleShort(MusHeader->SongLen), int(len) - LittleShort(MusHeader->SongStart));
	Division = 140;
	Tempo = InitialTempo = 1000000;
}

//==========================================================================
//
// MUSSong2 :: DoInitialSetup
//
// Sets up initial velocities and channel volumes.
//
//==========================================================================

void MUSSong2::DoInitialSetup()
{
	for (int i = 0; i < 16; ++i)
	{
		LastVelocity[i] = 100;
		ChannelVolumes[i] = 127;
	}
}

//==========================================================================
//
// MUSSong2 :: DoRestart
//
// Rewinds the song.
//
//==========================================================================

void MUSSong2::DoRestart()
{
	MusP = 0;
}

//==========================================================================
//
// MUSSong2 :: CheckDone
//
//==========================================================================

bool MUSSong2::CheckDone()
{
	return MusP >= MaxMusP;
}

//==========================================================================
//
// MUSSong2 :: Precache
//
// MUS songs contain information in their header for exactly this purpose.
//
//==========================================================================

std::vector<uint16_t> MUSSong2::PrecacheData()
{
	auto MusHeader = (MUSHeader*)MusData.data();
	std::vector<uint16_t> work;
	const uint8_t *used = MusData.data() + sizeof(MUSHeader) / sizeof(uint8_t);
	int i, k;

	int numinstr = LittleShort(MusHeader->NumInstruments);
	work.reserve(LittleShort(MusHeader->NumInstruments));
	for (i = k = 0; i < numinstr; ++i)
	{
		uint8_t instr = used[k++];
		uint16_t val;
		if (instr < 128)
		{
			val = instr;
		}
		else if (instr >= 135 && instr <= 188)
		{ // Percussions are 100-based, not 128-based, eh?
			val = instr - 100 + (1 << 14);
		}
		else
		{
			// skip it.
			val = used[k++];
			k += val;
			continue;
		}

		int numbanks = used[k++];
		if (numbanks > 0)
		{
			for (int b = 0; b < numbanks; b++)
			{
				work.push_back(val | (used[k++] << 7));
			}
		}
		else
		{
			work.push_back(val);
		}
	}
	return work;
}

//==========================================================================
//
// MUSSong2 :: MakeEvents
//
// Translates MUS events into MIDI events and puts them into a MIDI stream
// buffer. Returns the new position in the buffer.
//
//==========================================================================

uint32_t *MUSSong2::MakeEvents(uint32_t *events, uint32_t *max_event_p, uint32_t max_time)
{
	uint32_t tot_time = 0;
	uint32_t time = 0;
	auto MusHeader = (MUSHeader*)MusData.data();

	max_time = max_time * Division / Tempo;

	while (events < max_event_p && tot_time <= max_time)
	{
		uint8_t mid1, mid2;
		uint8_t channel;
		uint8_t t = 0, status;
		uint8_t event = MusBuffer[MusP++];
		
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
			status |= MIDI_NOTEON;
			mid1 = t;
			mid2 = 0;
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
				if (mid1 == 7)
				{ // Clamp volume to 127, since DMX apparently allows 8-bit volumes.
				  // Fix courtesy of Gez, courtesy of Ben Ryves.
					mid2 = VolumeControllerChange(channel, std::min<int>(mid2, 0x7F));
				}
			}
			break;
			
		case MUS_SCOREEND:
		default:
			MusP = MaxMusP;
			goto end;
		}

		events[0] = time;			// dwDeltaTime
		events[1] = 0;				// dwStreamID
		events[2] = status | (mid1 << 8) | (mid2 << 16);
		events += 3;

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
		events[0] = time;			// dwDeltaTime
		events[1] = 0;				// dwStreamID
		events[2] = MEVENT_NOP << 24;	// dwEvent
		events += 3;
	}
	return events;
}

//==========================================================================
//
// MUSHeaderSearch
//
// Searches for the MUS header within the given memory block, returning
// the offset it was found at, or -1 if not present.
//
//==========================================================================

int MUSHeaderSearch(const uint8_t *head, int len)
{
	len -= 4;
	for (int i = 0; i <= len; ++i)
	{
		if (head[i+0] == 'M' && head[i+1] == 'U' && head[i+2] == 'S' && head[i+3] == 0x1A)
		{
			return i;
		}
	}
	return -1;
}
