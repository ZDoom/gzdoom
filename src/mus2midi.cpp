// MUS files are essentially format 0 MIDI files with some
// space-saving modifications. Conversion is quite straight-forward.
// If you were to hook a main() into this that calls ProduceMIDI,
// you could create a self-contained MUS->MIDI converter. However, if
// you want to do that, you would be better off using qmus2mid, since
// it creates multitrack files that usually maintain running status
// better than single track files and are thus smaller.

#include <string.h>

#include "m_swap.h"
#include "mus2midi.h"

static const BYTE StaticMIDIhead[] =
{ 'M','T','h','d', 0, 0, 0, 6,
0, 0, // format 0: only one track
0, 1, // yes, there is really only one track
0, 70, // 70 divisions
'M','T','r','k', 0, 0, 0, 0,
// The first event sets the tempo to 500,000 microsec/quarter note
0, 255, 81, 3, 0x07, 0xa1, 0x20
};

static const BYTE MUSMagic[4] = { 'M','U','S',0x1a };

static const BYTE CtrlTranslate[15] =
{
	0,	// program change
	0,	// bank select
	1,	// modulation pot
	7,	// valume
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

static size_t WriteVarLen (FILE *file, int time)
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
		fputc (buffer & 0xff, file);
		if (buffer & 0x80)
			buffer >>= 8;
		else
			break;
	}
	return ofs;
}

bool ProduceMIDI (const BYTE *musBuf, FILE *outFile)
{
	BYTE midStatus, midArgs, mid1, mid2;
	size_t mus_p, maxmus_p;
	BYTE event;
	int deltaTime;
	const MUSHeader *musHead = (const MUSHeader *)musBuf;
	BYTE status;
	BYTE lastVel[16];
	SBYTE chanMap[16];
	int chanCount;
	int dupCount = 0;
	long trackLen;
	
	// Do some validation of the MUS file
	if (*(DWORD *)MUSMagic != *(DWORD *)musHead->Magic)
		return false;
	
	if (SHORT(musHead->NumChans) > 15)
		return false;
	
	// Prep for conversion
	fwrite (StaticMIDIhead, 1, sizeof(StaticMIDIhead), outFile);

	musBuf += SHORT(musHead->SongStart);
	maxmus_p = SHORT(musHead->SongLen);
	mus_p = 0;
	
	memset (lastVel, 64, 16);
	memset (chanMap, -1, 15);
	chanMap[15] = 9;
	chanCount = 0;
	event = 0;
	deltaTime = 0;
	status = 0;
	
	while (mus_p < maxmus_p && (event & 0x70) != MUS_SCOREEND)
	{
		int channel;
		BYTE t;
		
		event = musBuf[mus_p++];
		
		if ((event & 0x70) != MUS_SCOREEND)
		{
			t = musBuf[mus_p++];
		}
		channel = event & 15;
		
		if (chanMap[channel] < 0)
		{
			chanMap[channel] = chanCount++;
			if (chanCount == 9)
				++chanCount;
		}
		
		midStatus = channel = chanMap[channel];
		midArgs = 0;		// Most events have two args (0 means 2, 1 means 1)
		
		switch (event & 0x70)
		{
		case MUS_NOTEOFF:
			midStatus |= MIDI_NOTEOFF;
			mid1 = t;
			mid2 = 64;
			break;
			
		case MUS_NOTEON:
			midStatus |= MIDI_NOTEON;
			mid1 = t & 127;
			if (t & 128)
			{
				lastVel[channel] = musBuf[mus_p++];;
			}
			mid2 = lastVel[channel];
			break;
			
		case MUS_PITCHBEND:
			midStatus |= MIDI_PITCHBEND;
			mid1 = (t & 1) << 6;
			mid2 = (t >> 1) & 127;
			break;
			
		case MUS_SYSEVENT:
			midStatus |= MIDI_CTRLCHANGE;
			mid1 = CtrlTranslate[t];
			mid2 = t == 12 ? SHORT(musHead->NumChans) : 0;
			break;
			
		case MUS_CTRLCHANGE:
			if (t == 0)
			{ // program change
				midArgs = 1;
				midStatus |= MIDI_PRGMCHANGE;
				mid1 = musBuf[mus_p++];
			}
			else
			{
				midStatus |= MIDI_CTRLCHANGE;
				mid1 = CtrlTranslate[t];
				mid2 = musBuf[mus_p++];
			}
			break;
			
		case MUS_SCOREEND:
			midStatus = 0xff;
			mid1 = 0x2f;
			mid2 = 0x00;
			break;
			
		default:
			return false;
		}

		WriteVarLen (outFile, deltaTime);

		if (midStatus == status)
		{
			++dupCount;
			fputc (mid1, outFile);
			if (!midArgs)
				fputc (mid2, outFile);
		}
		else
		{
			status = midStatus;
			fputc (status, outFile);
			fputc (mid1, outFile);
			if (!midArgs)
				fputc (mid2, outFile);
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
	trackLen = ftell (outFile) - 22;
	fseek (outFile, 18, SEEK_SET);
	fputc ((trackLen >> 24) & 255, outFile);
	fputc ((trackLen >> 16) & 255, outFile);
	fputc ((trackLen >> 8) & 255, outFile);
	fputc (trackLen & 255, outFile);
	
	return true;
}
