/*
 ** midisource.cpp
 ** Implements base class for the different MIDI formats
 **
 **---------------------------------------------------------------------------
 ** Copyright 2008-2016 Randy Heit
 ** Copyright 2017-2018 Christoph Oelckers
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


#include "i_musicinterns.h"
#include "midisources.h"


//==========================================================================
//
// MIDISource :: SetTempo
//
// Sets the tempo from a track's initial meta events. Later tempo changes
// create MEVENT_TEMPO events instead.
//
//==========================================================================

void MIDISource::SetTempo(int new_tempo)
{
	InitialTempo = new_tempo;
	// This intentionally uses a callback to avoid any dependencies on the class that is playing the song.
	// This should probably be done differently, but right now that's not yet possible.
	if (TempoCallback(new_tempo))
	{
		Tempo = new_tempo;
	}
}


//==========================================================================
//
// MIDISource :: ClampLoopCount
//
// We use the XMIDI interpretation of loop count here, where 1 means it
// plays that section once (in other words, no loop) rather than the EMIDI
// interpretation where 1 means to loop it once.
//
// If LoopLimit is 1, we limit all loops, since this pass over the song is
// used to determine instruments for precaching.
//
// If LoopLimit is higher, we only limit infinite loops, since this song is
// being exported.
//
//==========================================================================

int MIDISource::ClampLoopCount(int loopcount)
{
	if (LoopLimit == 0)
	{
		return loopcount;
	}
	if (LoopLimit == 1)
	{
		return 1;
	}
	if (loopcount == 0)
	{
		return LoopLimit;
	}
	return loopcount;
}

//==========================================================================
//
// MIDISource :: VolumeControllerChange
//
// Some devices don't support master volume
// (e.g. the Audigy's software MIDI synth--but not its two hardware ones),
// so assume none of them do and scale channel volumes manually.
//
//==========================================================================

int MIDISource::VolumeControllerChange(int channel, int volume)
{
	ChannelVolumes[channel] = volume;
	// When exporting this MIDI file,
	// we should not adjust the volume level.
	return Exporting? volume : ((volume + 1) * Volume) >> 16;
}

//==========================================================================
//
// MIDISource :: Precache
//
// Generates a list of instruments this song uses and passes them to the
// MIDI device for precaching. The default implementation here pretends to
// play the song and watches for program change events on normal channels
// and note on events on channel 10.
//
//==========================================================================

TArray<uint16_t> MIDISource::PrecacheData()
{
	uint32_t Events[2][MAX_MIDI_EVENTS*3];
	uint8_t found_instruments[256] = { 0, };
	uint8_t found_banks[256] = { 0, };
	bool multiple_banks = false;
	
	LoopLimit = 1;
	DoRestart();
	found_banks[0] = true;		// Bank 0 is always used.
	found_banks[128] = true;
	
	// Simulate playback to pick out used instruments.
	while (!CheckDone())
	{
		uint32_t *event_end = MakeEvents(Events[0], &Events[0][MAX_MIDI_EVENTS*3], 1000000*600);
		for (uint32_t *event = Events[0]; event < event_end; )
		{
			if (MEVENT_EVENTTYPE(event[2]) == 0)
			{
				int command = (event[2] & 0x70);
				int channel = (event[2] & 0x0f);
				int data1 = (event[2] >> 8) & 0x7f;
				int data2 = (event[2] >> 16) & 0x7f;
				
				if (channel != 9 && command == (MIDI_PRGMCHANGE & 0x70))
				{
					found_instruments[data1] = true;
				}
				else if (channel == 9 && command == (MIDI_PRGMCHANGE & 0x70) && data1 != 0)
				{ // On a percussion channel, program change also serves as bank select.
					multiple_banks = true;
					found_banks[data1 | 128] = true;
				}
				else if (channel == 9 && command == (MIDI_NOTEON & 0x70) && data2 != 0)
				{
					found_instruments[data1 | 128] = true;
				}
				else if (command == (MIDI_CTRLCHANGE & 0x70) && data1 == 0 && data2 != 0)
				{
					multiple_banks = true;
					if (channel == 9)
					{
						found_banks[data2 | 128] = true;
					}
					else
					{
						found_banks[data2] = true;
					}
				}
			}
			// Advance to next event
			if (event[2] < 0x80000000)
			{ // short message
				event += 3;
			}
			else
			{ // long message
				event += 3 + ((MEVENT_EVENTPARM(event[2]) + 3) >> 2);
			}
		}
	}
	DoRestart();
	
	// Now pack everything into a contiguous region for the PrecacheInstruments call().
	TArray<uint16_t> packed;
	
	for (int i = 0; i < 256; ++i)
	{
		if (found_instruments[i])
		{
			uint16_t packnum = (i & 127) | ((i & 128) << 7);
			if (!multiple_banks)
			{
				packed.Push(packnum);
			}
			else
			{ // In order to avoid having to multiplex tracks in a type 1 file,
				// precache every used instrument in every used bank, even if not
				// all combinations are actually used.
				for (int j = 0; j < 128; ++j)
				{
					if (found_banks[j + (i & 128)])
					{
						packed.Push(packnum | (j << 7));
					}
				}
			}
		}
	}
	return packed;
}

//==========================================================================
//
// MIDISource :: CheckCaps
//
// Called immediately after the device is opened in case a source should
// want to alter its behavior depending on which device it got.
//
//==========================================================================

void MIDISource::CheckCaps(int tech)
{
}

//==========================================================================
//
// MIDISource :: SetMIDISubsong
//
// Selects which subsong to play. This is private.
//
//==========================================================================

bool MIDISource::SetMIDISubsong(int subsong)
{
	return subsong == 0;
}
