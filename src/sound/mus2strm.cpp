/*
** mus2strm.cpp
** Converts MUS files into MIDI streams
**
**---------------------------------------------------------------------------
** Copyright 1998-2001 Randy Heit
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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#include <assert.h>

#include "doomtype.h"
#include "mus2strm.h"
#include "m_alloc.h"
#include "m_swap.h"

#define last(e)         ((unsigned char)(e & 0x80))
#define channel(e)      ((unsigned char)(e & 0x0F))

#define MUSMAGIC		"MUS\032"

#define NOTMUSFILE		1		/* Not a MUS file */
#define MUSFILECOR		2		/* MUS file corrupted */
#define TOOMCHAN		3		/* Too many channels */
#define MEMALLOC		4		/* Memory allocation error */

extern OUTSTREAMSTATE ots;

struct Track
{
	unsigned long	 current;
	char			 vel;
	long			 DeltaTime;
	unsigned char	 LastEvent;
	char			*data;		/* Primary data */
};

// Description of the input MUS file
//
typedef struct
{
	DWORD			cbFile;				// Total bytes in file
	LPBYTE			pFile;				// -> entire file in memory
	DWORD			cbLeft;				// Bytes left unread
	LPBYTE			pFilePointer;		// -> next byte to read

	DWORD			tkNextEventDue;
	BOOL			endEncountered;
} INFILESTATE;


static INFILESTATE ifs;
static const byte MUS2MIDcontrol[15] = {
	0,			/* Program change - not a MIDI control change */
	0x00,		/* Bank select */
	0x01,		/* Modulation pot */
	0x07,		/* Volume */
	0x0A,		/* Pan pot */
	0x0B,		/* Expression pot */
	0x5B,		/* Reverb depth */
	0x5D,		/* Chorus depth */
	0x40,		/* Sustain pedal */
	0x43,		/* Soft pedal */
	0x78,		/* All sounds off */
	0x7B,		/* All notes off */
	0x7E,		/* Mono */
	0x7F,		/* Poly */
	0x79		/* Reset all controllers */
};
static signed char MUS2MIDchannel[16];
static char MUSvelocity[16];

static DWORD		ReadTime (void);
static char			FirstChannelAvailable (signed char MUS2MIDchannel[]);
static BOOL			ReadMUSheader (MUSHeader *MUSh);
static BOOL			GetTrackEvent (MUSHeader *MUSh, MEVENT *me);
static int			DoConversion (byte *in, DWORD insize, int division);


static BOOL ReadMUSheader (MUSHeader *mush)
{
	if (ifs.cbLeft < sizeof(*mush))
		return FALSE;

	memcpy (mush, ifs.pFilePointer, sizeof(*mush));

	if (strncmp ((char *)&mush->Magic, MUSMAGIC, 4))
		return FALSE;

	mush->SongLen = SHORT(mush->SongLen);
	mush->SongStart = SHORT(mush->SongStart);
	mush->NumChans = SHORT(mush->NumChans);
	mush->NumSecondaryChans = SHORT(mush->NumSecondaryChans);
	mush->NumInstruments = SHORT(mush->NumInstruments);

	if (ifs.cbLeft < mush->SongStart)
		return FALSE;

	ifs.cbLeft -= mush->SongStart;
	ifs.pFilePointer += mush->SongStart;

	return TRUE;
}

static DWORD ReadTime (void)
{
	DWORD time = 0;
	byte byt = 0;

	do {
		if (ifs.cbLeft > 0) {
			byt = *(ifs.pFilePointer++);
			ifs.cbLeft--;
			time = (time << 7) + (byt & 0x7F);
		}
	} while ((ifs.cbLeft > 0) && (byt & 0x80));

	return time;
}

static char FirstChannelAvailable (signed char MUS2MIDchannel[])
{
	int i;
	signed char max = -1;

	for (i = 0; i < 15; i++)
		if (MUS2MIDchannel[i] > max)
			max = MUS2MIDchannel[i];

	return max == 8 ? 10 : max+1;
}

static BOOL GetTrackEvent (MUSHeader *MUSh, MEVENT *me)
{
	BYTE event, data = 0;
	int MUSchannel, MIDIchannel;

	me->pEvent = NULL;
	
	if (!ifs.cbLeft)
		return FALSE;
	event = *(ifs.pFilePointer++);
	ifs.cbLeft--;

	MUSchannel = channel(event);
	if (MUS2MIDchannel[MUSchannel] == -1)
		MIDIchannel = MUS2MIDchannel[MUSchannel] =
			(MUSchannel == 15 ? 9 : FirstChannelAvailable (MUS2MIDchannel));
	else
		MIDIchannel = MUS2MIDchannel[MUSchannel];

	if ((event & 0x70) <= MUS_CTRLCHANGE)
	{
		if (ifs.cbLeft < 1)
			return FALSE;
		data = *(ifs.pFilePointer++);
		ifs.cbLeft--;
	}

	switch (event & 0x70)
	{
	case MUS_NOTEOFF:
		me->cbEvent = 3;
		me->abEvent[0] = MIDI_NOTEOFF | MIDIchannel;
		me->abEvent[1] = data;
		me->abEvent[2] = 64;
		break;

	case MUS_NOTEON:
		me->cbEvent = 3;
		me->abEvent[0] = MIDI_NOTEON | MIDIchannel;
		me->abEvent[1] = data & 0x7f;
		if (data & 0x80) {
			if (ifs.cbLeft < 1)
				return FALSE;
			else {
				MUSvelocity[MUSchannel] = *(ifs.pFilePointer++);
				ifs.cbLeft--;
			}
		}
		me->abEvent[2] = MUSvelocity[MUSchannel];
		break;

	case MUS_PITCHBEND:
		me->cbEvent = 3;
		me->abEvent[0] = MIDI_PITCHBEND | MIDIchannel;
		me->abEvent[1] = (data & 1) << 6;
		me->abEvent[2] = data >> 1;
		break ;

	case MUS_SYSEVENT:
		me->cbEvent = 3;
		me->abEvent[0] = MIDI_CTRLCHANGE | MIDIchannel;
		me->abEvent[1] = MUS2MIDcontrol[data];
		me->abEvent[2] = data == 12 ? MUSh->NumChans+1 : 0;
		break;

	case MUS_CTRLCHANGE:
		if (ifs.cbLeft < 1)
			return FALSE;
		ifs.cbLeft--;
		if (data) {
			me->cbEvent = 3;
			me->abEvent[0] = MIDI_CTRLCHANGE | MIDIchannel;
			me->abEvent[1] = MUS2MIDcontrol[data];
			me->abEvent[2] = *(ifs.pFilePointer++);
		} else {
			me->cbEvent = 2;
			me->abEvent[0] = MIDI_PRGMCHANGE | MIDIchannel;
			me->abEvent[1] = *(ifs.pFilePointer++);
		}
		break ;

	case MUS_SCOREEND:
		ifs.endEncountered = TRUE;
		break;

	default:
		return FALSE;
	}

	me->tkEvent = ifs.tkNextEventDue;
	if (last(event))
		ifs.tkNextEventDue += ReadTime ();

	return TRUE;
}

static int DoConversion (byte *in, DWORD insize, int division)
{	
	// The start of the first stream buffer contains an event to set
	// the tempo to 500,000 microseconds per quarter note.
	static BYTE tempoSet[3] = { 0x07, 0xa1, 0x20 };
	static MEVENT tempoEvent = {
		0,
		{ MIDI_META, MIDI_META_TEMPO, 0, 0 },
		3,
		tempoSet
	};

	MUSHeader MUSh;
	int i;

	memset (UsedPatches, 0, sizeof(UsedPatches));

	ifs.cbFile = insize;
	ifs.pFile = in;

	ifs.cbLeft = ifs.cbFile;
	ifs.pFilePointer = ifs.pFile;

	if (!ReadMUSheader (&MUSh))
		return NOTMUSFILE;

	if (MUSh.NumChans > 15)			/* <=> MUSchannels+drums > 16 */
		return TOOMCHAN;

	for (i = 0; i < 16; i++) {
		MUS2MIDchannel[i] = -1;
		MUSvelocity[i] = 64;
	}
	ifs.tkNextEventDue = 0;
	ifs.endEncountered = FALSE;
	ots.tkTrack = 0;
	ots.pFirst = NULL;
	ots.pLast = NULL;

	if (!AddEventToStream (&tempoEvent))
		return MEMALLOC;

	while (!ifs.endEncountered) {
		MEVENT me;

		if (!GetTrackEvent (&MUSh, &me))
			return MUSFILECOR;

		if (!ifs.endEncountered && !AddEventToStream (&me))
			return MEMALLOC;
	}

	midTimeDiv = division ? division : 70;

	return 0;
}

void mus2strmCleanup (void)
{
	PSTREAMBUF pCurr = ots.pFirst;
	while (pCurr) {
		PSTREAMBUF pNext = pCurr->pNext;
		free (pCurr);
		pCurr = pNext;
	}
}

PSTREAMBUF mus2strmConvert (BYTE *inFile, DWORD inSize)
{
	int error = DoConversion (inFile, inSize, 0);

	if (error) {
		Printf ("MUS error: ");
		switch (error) {
			case NOTMUSFILE:
				Printf ("Not a MUS file.\n"); break;
			case MUSFILECOR:
				Printf ("MUS file is corrupt.\n"); break;
			case TOOMCHAN:
				Printf ("MUS file has more than 16 channels.\n"); break;
			case MEMALLOC:
				Printf ("Not enough memory.\n"); break;
		}
		mus2strmCleanup ();
		return NULL;
	}

	return ots.pFirst;
}
