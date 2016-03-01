/*
** mus2midi.cpp
** Simple converter from MUS to MIDI format
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
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
** MUS files are essentially format 0 MIDI files with some
** space-saving modifications. Conversion is quite straight-forward.
** If you were to hook a main() into this that calls ProduceMIDI,
** you could create a self-contained MUS->MIDI converter.
*/


#include <string.h>

#include "m_swap.h"
#include "mus2midi.h"
#include "doomdef.h"

static const BYTE StaticMIDIhead[] =
{ 'M','T','h','d', 0, 0, 0, 6,
0, 0, // format 0: only one track
0, 1, // yes, there is really only one track
0, 70, // 70 divisions
'M','T','r','k', 0, 0, 0, 0,
// The first event sets the tempo to 500,000 microsec/quarter note
0, 255, 81, 3, 0x07, 0xa1, 0x20
};

extern int MUSHeaderSearch(const BYTE *head, int len);

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

static size_t ReadVarLen (const BYTE *buf, int *time_out)
{
	int time = 0;
	size_t ofs = 0;
	BYTE t;
	
	do
	{
		t = buf[ofs++];
		time = (time << 7) | (t & 127);
	} while (t & 128);
	*time_out = time;
	return ofs;
}

static size_t WriteVarLen (TArray<BYTE> &file, int time)
{
	long buffer;
	size_t ofs;
	
	buffer = time & 0x7f;
	while ((time >>= 7) > 0)
	{
		buffer = (buffer << 8) | 0x80 | (time & 0x7f);
	}
	for (ofs = 0;;)
	{
		file.Push(BYTE(buffer & 0xff));
		if (buffer & 0x80)
			buffer >>= 8;
		else
			break;
	}
	return ofs;
}

bool ProduceMIDI (const BYTE *musBuf, int len, TArray<BYTE> &outFile)
{
	BYTE midStatus, midArgs, mid1, mid2;
	size_t mus_p, maxmus_p;
	BYTE event;
	int deltaTime;
	const MUSHeader *musHead;
	BYTE status;
	BYTE chanUsed[16];
	BYTE lastVel[16];
	long trackLen;
	bool no_op;

	// Find the header
	int offset = MUSHeaderSearch(musBuf, len);

	if (offset < 0 || offset + (int)sizeof(MUSHeader) >= len)
		return false;

	musBuf += offset;
	len -= offset;
	musHead = (const MUSHeader *)musBuf;

	// Do some validation of the MUS file
	if (LittleShort(musHead->NumChans) > 15)
		return false;
	
	// Prep for conversion
	outFile.Clear();
	outFile.Reserve(sizeof(StaticMIDIhead));
	memcpy(&outFile[0], StaticMIDIhead, sizeof(StaticMIDIhead));

	musBuf += LittleShort(musHead->SongStart);
	mus_p = 0;
	maxmus_p = LittleShort(musHead->SongLen);
	if ((size_t)len - LittleShort(musHead->SongStart) < maxmus_p)
	{
		maxmus_p = len - LittleShort(musHead->SongStart);
	}
	
	memset (lastVel, 100, 16);
	memset (chanUsed, 0, 16);
	event = 0;
	deltaTime = 0;
	status = 0;
	
	while (mus_p < maxmus_p && (event & 0x70) != MUS_SCOREEND)
	{
		int channel;
		BYTE t = 0;
		
		event = musBuf[mus_p++];
		
		if ((event & 0x70) != MUS_SCOREEND)
		{
			t = musBuf[mus_p++];
		}
		channel = event & 15;
		if (channel == 15)
		{
			channel = 9;
		}
		else if (channel >= 9)
		{
			channel++;
		}
		
		if (!chanUsed[channel])
		{
			// This is the first time this channel has been used,
			// so sets its volume to 127.
			chanUsed[channel] = 1;
			outFile.Push(0);
			outFile.Push(0xB0 | channel);
			outFile.Push(7);
			outFile.Push(127);
		}
		
		midStatus = channel;
		midArgs = 0;		// Most events have two args (0 means 2, 1 means 1)
		no_op = false;

		switch (event & 0x70)
		{
		case MUS_NOTEOFF:
			midStatus |= MIDI_NOTEON;
			mid1 = t & 127;
			mid2 = 0;
			break;
			
		case MUS_NOTEON:
			midStatus |= MIDI_NOTEON;
			mid1 = t & 127;
			if (t & 128)
			{
				lastVel[channel] = musBuf[mus_p++] & 127;
			}
			mid2 = lastVel[channel];
			break;
			
		case MUS_PITCHBEND:
			midStatus |= MIDI_PITCHBEND;
			mid1 = (t & 1) << 6;
			mid2 = (t >> 1) & 127;
			break;
			
		case MUS_SYSEVENT:
			if (t < 10 || t > 14)
			{
				no_op = true;
			}
			else
			{
				midStatus |= MIDI_CTRLCHANGE;
				mid1 = CtrlTranslate[t];
				mid2 = t == 12 /* Mono */ ? LittleShort(musHead->NumChans) : 0;
			}
			break;
			
		case MUS_CTRLCHANGE:
			if (t == 0)
			{ // program change
				midArgs = 1;
				midStatus |= MIDI_PRGMCHANGE;
				mid1 = musBuf[mus_p++] & 127;
				mid2 = 0;	// Assign mid2 just to make GCC happy
			}
			else if (t > 0 && t < 10)
			{
				midStatus |= MIDI_CTRLCHANGE;
				mid1 = CtrlTranslate[t];
				mid2 = musBuf[mus_p++];
			}
			else
			{
				no_op = true;
			}
			break;
			
		case MUS_SCOREEND:
			midStatus = MIDI_META;
			mid1 = MIDI_META_EOT;
			mid2 = 0;
			break;
			
		default:
			return false;
		}

		if (no_op)
		{
			// A system-specific event with no data is a no-op.
			midStatus = MIDI_META;
			mid1 = MIDI_META_SSPEC;
			mid2 = 0;
		}

		WriteVarLen (outFile, deltaTime);

		if (midStatus != status)
		{
			status = midStatus;
			outFile.Push(status);
		}
		outFile.Push(mid1);
		if (midArgs == 0)
		{
			outFile.Push(mid2);
		}
		if (event & 128)
		{
			mus_p += ReadVarLen (&musBuf[mus_p], &deltaTime);
		}
		else
		{
			deltaTime = 0;
		}
	}
	
	// fill in track length
	trackLen = outFile.Size() - 22;
	outFile[18] = BYTE((trackLen >> 24) & 255);
	outFile[19] = BYTE((trackLen >> 16) & 255);
	outFile[20] = BYTE((trackLen >> 8) & 255);
	outFile[21] = BYTE(trackLen & 255);
	return true;
}

bool ProduceMIDI(const BYTE *musBuf, int len, FILE *outFile)
{
	TArray<BYTE> work;
	if (ProduceMIDI(musBuf, len, work))
	{
		return fwrite(&work[0], 1, work.Size(), outFile) == work.Size();
	}
	return false;
}
