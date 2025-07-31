/*
** midisource_mids.cpp
** Code to let ZMusic play MIDS MIDI music through the MIDI streaming API.
**
**---------------------------------------------------------------------------
** Copyright 2020 Cacodemon345
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

#include <algorithm>
#include "midisource.h"
#include "zmusic/m_swap.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
//
//  MIDSSong constructor
//
//  Reads the buffers from the file and validates the MIDS header
//
//==========================================================================

MIDSSong::MIDSSong(const uint8_t* data, size_t len)
{
    if (len <= 52)
        return;

    if ((len % 4) != 0)
        return;

    // Validate the header first.
    if (data[12] != 'f' || data[13] != 'm' || data[14] != 't' || data[15] != ' ')
    {        
        return;
    }
    int headerSize = LittleLong(GetInt(&data[16]));
    if (headerSize != 12) return;
    Division = LittleLong(GetInt(&data[20]));
    FormatFlags = LittleLong(GetInt(&data[28]));
    // Validate the data chunk.
    if (data[32] != 'd' || data[33] != 'a' || data[34] != 't' || data[35] != 'a')
    {
        return;
    }
    int NumBlocks = LittleLong(GetInt(&data[40]));
    const uint32_t* midiData = (const uint32_t*)&data[44];
    uint32_t tkStart = 0;
    uint32_t cbBuffer = 0;
    while (NumBlocks-- > 0)
    {
        tkStart = LittleLong(*midiData);
        cbBuffer = LittleLong(*(midiData + 1));
        midiData += 2;
        if ((cbBuffer % (FormatFlags ? 8 : 12)) != 0) return;
        MidsBuffer.insert(MidsBuffer.end(), midiData, midiData + (cbBuffer / 4));
        midiData += cbBuffer / 4;
    }
    MidsP = 0;
    MaxMidsP = MidsBuffer.size() - 1;
    for (auto& curMidiData : MidsBuffer)
    {
        curMidiData = LittleLong(curMidiData);
    }
}

//==========================================================================
//
// MIDSSong :: DoInitialSetup
//
// Sets the starting channel volumes.
//
//==========================================================================

void MIDSSong::DoInitialSetup()
{
	for (int i = 0; i < 16; ++i)
	{
		ChannelVolumes[i] = 100;
	}
}

//==========================================================================
//
// MIDSSong :: CheckDone
//
//==========================================================================

bool MIDSSong::CheckDone()
{
    return MidsP >= MaxMidsP;
}

//==========================================================================
//
// MIDSSong :: DoRestart()
//
// Rewinds the song
//
//==========================================================================

void MIDSSong::DoRestart()
{
    MidsP = 0;
    ProcessInitialTempoEvents();
}

//==========================================================================
//
// MIDSSong :: ProcessInitialTempoEvents()
//
// Process initial tempo events at the start of the song.
//
//==========================================================================

void MIDSSong::ProcessInitialTempoEvents()
{
    if (MEVENT_EVENTTYPE(MidsBuffer[FormatFlags ? 1 : 2]) == MEVENT_TEMPO)
    {
        SetTempo(MEVENT_EVENTPARM(MidsBuffer[FormatFlags ? 1 : 2]));
    }
}

//==========================================================================
//
// MUSSong2 :: MakeEvents
//
// Puts MIDS events into a MIDI stream
// buffer. Returns the new position in the buffer.
//
//==========================================================================

uint32_t* MIDSSong::MakeEvents(uint32_t* events, uint32_t* max_event_p, uint32_t max_time)
{
    uint32_t time = 0;
    uint32_t tot_time = 0;

    max_time = max_time * Division / Tempo;
    while (events < max_event_p && tot_time <= max_time)
    {
        events[0] = time = MidsBuffer[MidsP++];
        events[1] = FormatFlags ? 0 : MidsBuffer[MidsP++];
        events[2] = MidsBuffer[MidsP++];        
        events += 3;
        tot_time += time;
        if (MidsP >= MaxMidsP) break;               
    }
    return events;
}
