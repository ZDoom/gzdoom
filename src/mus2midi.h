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
	char Magic[4];
	WORD SongLen;
	WORD SongStart;
	WORD NumChans;
	WORD NumSecondaryChans;
	WORD NumInstruments;
	WORD Pad;
} MUSHeader;

bool ProduceMIDI (const BYTE *musBuf, FILE *outFile);

#endif //__MUS2MIDI_H__
