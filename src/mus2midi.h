/*
** mus2midi.h
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
*/

#ifndef __MUS2MIDI_H__
#define __MUS2MIDI_H__

#ifdef _MSC_VER
#pragma once
#endif

#include <stdio.h>
#include "doomtype.h"

#define MIDI_SYSEX		((BYTE)0xF0)		 // SysEx begin
#define MIDI_SYSEXEND	((BYTE)0xF7)		 // SysEx end
#define MIDI_META		((BYTE)0xFF)		 // Meta event begin
#define MIDI_META_TEMPO ((BYTE)0x51)
#define MIDI_META_EOT	((BYTE)0x2F)		 // End-of-track
#define MIDI_META_SSPEC	((BYTE)0x7F)		 // System-specific event

#define MIDI_NOTEOFF	((BYTE)0x80)		 // + note + velocity
#define MIDI_NOTEON 	((BYTE)0x90)		 // + note + velocity
#define MIDI_POLYPRESS	((BYTE)0xA0)		 // + pressure (2 bytes)
#define MIDI_CTRLCHANGE ((BYTE)0xB0)		 // + ctrlr + value
#define MIDI_PRGMCHANGE ((BYTE)0xC0)		 // + new patch
#define MIDI_CHANPRESS	((BYTE)0xD0)		 // + pressure (1 byte)
#define MIDI_PITCHBEND	((BYTE)0xE0)		 // + pitch bend (2 bytes)

#define MUS_NOTEOFF 	((BYTE)0x00)
#define MUS_NOTEON		((BYTE)0x10)
#define MUS_PITCHBEND	((BYTE)0x20)
#define MUS_SYSEVENT	((BYTE)0x30)
#define MUS_CTRLCHANGE	((BYTE)0x40)
#define MUS_SCOREEND	((BYTE)0x60)

typedef struct
{
	DWORD Magic;
	WORD SongLen;
	WORD SongStart;
	WORD NumChans;
	WORD NumSecondaryChans;
	WORD NumInstruments;
	WORD Pad;
	// WORD UsedInstruments[NumInstruments];
} MUSHeader;

#endif //__MUS2MIDI_H__
