/*
*	Name:		OPL2/OPL3 Music driver
*	Project:	MUS File Player Library
*	Version:	1.67
*	Author:		Vladimir Arnost (QA-Software)
*	Last revision:	May-2-1996
*	Compiler:	Borland C++ 3.1, Watcom C/C++ 10.0
*
*/

/*
* Revision History:
*
*	Aug-8-1994	V1.00	V.Arnost
*		Written from scratch
*	Aug-9-1994	V1.10	V.Arnost
*		Some minor changes to improve sound quality. Tried to add
*		stereo sound capabilities, but failed to -- my SB Pro refuses
*		to switch to stereo mode.
*	Aug-13-1994	V1.20	V.Arnost
*		Stereo sound fixed. Now works also with Sound Blaster Pro II
*		(chip OPL3 -- gives 18 "stereo" (ahem) channels).
*		Changed code to handle properly notes without volume.
*		(Uses previous volume on given channel.)
*		Added cyclic channel usage to avoid annoying clicking noise.
*	Aug-28-1994	V1.40	V.Arnost
*		Added Adlib and SB Pro II detection.
*	Apr-16-1995	V1.60	V.Arnost
*		Moved into separate source file MUSLIB.C
*	Jul-12-1995	V1.62	V.Arnost
*		Module created and source code copied from MUSLIB.C
*	Aug-08-1995	V1.63	V.Arnost
*		Modified to follow changes in MLOPL_IO.C
*	Aug-13-1995	V1.64	V.Arnost
*		Added OPLsendMIDI() function
*	Sep-8-1995	V1.65	V.Arnost
*		Added sustain pedal support.
*		Improved pause/unpause functions. Now all notes are made
*		silent (even released notes, which haven't sounded off yet).
*	Mar-1-1996	V1.66	V.Arnost
*		Cleaned up the source
*	May-2-1996	V1.67	V.Arnost
*		Added modulation wheel (vibrato) support
*/

#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#endif
#include "muslib.h"
#include "files.h"

#include "c_cvars.h"

#define MOD_MIN		40		/* vibrato threshold */


//#define HIGHEST_NOTE 102
#define HIGHEST_NOTE 127

musicBlock::musicBlock ()
{
	memset (this, 0, sizeof(*this));
}

musicBlock::~musicBlock ()
{
	if (OPLinstruments != NULL) free(OPLinstruments);
}

void musicBlock::writeFrequency(uint slot, uint note, int pitch, uint keyOn)
{
	io->OPLwriteFreq (slot, note, pitch, keyOn);
}

void musicBlock::writeModulation(uint slot, struct OPL2instrument *instr, int state)
{
	if (state)
		state = 0x40;	/* enable Frequency Vibrato */
	io->OPLwriteChannel(0x20, slot,
		(instr->feedback & 1) ? (instr->trem_vibr_1 | state) : instr->trem_vibr_1,
		instr->trem_vibr_2 | state);
}

uint musicBlock::calcVolume(uint channelVolume, uint channelExpression, uint noteVolume)
{
	noteVolume = ((ulong)channelVolume * channelExpression * noteVolume) / (127*127);
	if (noteVolume > 127)
		return 127;
	else
		return noteVolume;
}

int musicBlock::occupyChannel(uint slot, uint channel,
						 int note, int volume, struct OP2instrEntry *instrument, uchar secondary)
{
	struct OPL2instrument *instr;
	struct channelEntry *ch = &channels[slot];

	ch->channel = channel;
	ch->note = note;
	ch->flags = secondary ? CH_SECONDARY : 0;
	if (driverdata.channelModulation[channel] >= MOD_MIN)
		ch->flags |= CH_VIBRATO;
	ch->time = MLtime;
	if (volume == -1)
		volume = driverdata.channelLastVolume[channel];
	else
		driverdata.channelLastVolume[channel] = volume;
	ch->realvolume = calcVolume(driverdata.channelVolume[channel],
		driverdata.channelExpression[channel], ch->volume = volume);
	if (instrument->flags & FL_FIXED_PITCH)
		note = instrument->note;
	else if (channel == PERCUSSION)
		note = 60;			// C-5
	if (secondary && (instrument->flags & FL_DOUBLE_VOICE))
		ch->finetune = (instrument->finetune - 0x80) >> 1;
	else
		ch->finetune = 0;
	ch->pitch = ch->finetune + driverdata.channelPitch[channel];
	if (secondary)
		instr = &instrument->instr[1];
	else
		instr = &instrument->instr[0];
	ch->instr = instr;
	if (channel != PERCUSSION && !(instrument->flags & FL_FIXED_PITCH))
	{
		if ( (note += instr->basenote) < 0)
			while ((note += 12) < 0) {}
		else if (note > HIGHEST_NOTE)
			while ((note -= 12) > HIGHEST_NOTE) {}
	}
	ch->realnote = note;

	io->OPLwriteInstrument(slot, instr);
	if (ch->flags & CH_VIBRATO)
		writeModulation(slot, instr, 1);
	io->OPLwritePan(slot, instr, driverdata.channelPan[channel]);
	io->OPLwriteVolume(slot, instr, ch->realvolume);
	writeFrequency(slot, note, ch->pitch, 1);
	return slot;
}

int musicBlock::releaseChannel(uint slot, uint killed)
{
	struct channelEntry *ch = &channels[slot];
	writeFrequency(slot, ch->realnote, ch->pitch, 0);
	ch->channel |= CH_FREE;
	ch->time = MLtime;
	ch->flags = CH_FREE;
	if (killed)
	{
		io->OPLwriteChannel(0x80, slot, 0x0F, 0x0F);  // release rate - fastest
		io->OPLwriteChannel(0x40, slot, 0x3F, 0x3F);  // no volume
	}
	return slot;
}

int musicBlock::releaseSustain(uint channel)
{
	uint i;
	uint id = channel;

	for(i = 0; i < io->OPLchannels; i++)
	{
		if (channels[i].channel == id && channels[i].flags & CH_SUSTAIN)
			releaseChannel(i, 0);
	}
	return 0;
}

int musicBlock::findFreeChannel(uint flag, uint channel, uchar note)
{
	uint i;

	ulong bestfit = 0;
	uint bestvoice = 0;

	for (i = 0; i < io->OPLchannels; ++i)
	{
		ulong magic;

		magic = ((channels[i].flags & CH_FREE) << 24) |
				((channels[i].note == note &&
					channels[i].channel == channel) << 30) |
				((channels[i].flags & CH_SUSTAIN) << 28) |
				((MLtime - channels[i].time) & 0x1fffffff);
		if (magic > bestfit)
		{
			bestfit = magic;
			bestvoice = i;
		}
	}
	if ((flag & 1) && !(bestfit & 0x80000000))
	{ // No free channels good enough
		return -1;
	}
	releaseChannel (bestvoice, 1);
	return bestvoice;
}

struct OP2instrEntry *musicBlock::getInstrument(uint channel, uchar note)
{
	uint instrnumber;

	if (channel == PERCUSSION)
	{
		if (note < 35 || note > 81)
			return NULL;		/* wrong percussion number */
		instrnumber = note + (128-35);
	}
	else
	{
		instrnumber = driverdata.channelInstr[channel];
	}

	if (OPLinstruments)
		return &OPLinstruments[instrnumber];
	else
		return NULL;
}


// code 1: play note
CVAR (Bool, opl_singlevoice, 0, 0)

void musicBlock::OPLplayNote(uint channel, uchar note, int volume)
{
	int i;
	struct OP2instrEntry *instr;

	if (volume == 0)
	{
		OPLreleaseNote (channel, note);
		return;
	}

	if ( (instr = getInstrument(channel, note)) == NULL )
		return;

	if ( (i = findFreeChannel((channel == PERCUSSION) ? 2 : 0, channel, note)) != -1)
	{
		occupyChannel(i, channel, note, volume, instr, 0);
		if ((instr->flags & FL_DOUBLE_VOICE) && !opl_singlevoice)
		{
			if ( (i = findFreeChannel((channel == PERCUSSION) ? 3 : 1, channel, note)) != -1)
				occupyChannel(i, channel, note, volume, instr, 1);
		}
	}
}

// code 0: release note
void musicBlock::OPLreleaseNote(uint channel, uchar note)
{
	uint i;
	uint id = channel;
	uint sustain = driverdata.channelSustain[channel];

	for(i = 0; i < io->OPLchannels; i++)
	{
		if (channels[i].channel == id && channels[i].note == note)
		{
			if (sustain < 0x40)
				releaseChannel(i, 0);
			else
				channels[i].flags |= CH_SUSTAIN;
		}
	}
}

// code 2: change pitch wheel (bender)
void musicBlock::OPLpitchWheel(uint channel, int pitch)
{
	uint i;
	uint id = channel;

	// Convert pitch from 14-bit to 7-bit, then scale it, since the player
	// code only understands sensitivities of 2 semitones.
	pitch = (pitch - 8192) * driverdata.channelPitchSens[channel] / (200 * 128) + 64;
	driverdata.channelPitch[channel] = pitch;
	for(i = 0; i < io->OPLchannels; i++)
	{
		struct channelEntry *ch = &channels[i];
		if (ch->channel == id)
		{
			ch->time = MLtime;
			ch->pitch = ch->finetune + pitch;
			writeFrequency(i, ch->realnote, ch->pitch, 1);
		}
	}
}

// code 4: change control
void musicBlock::OPLchangeControl(uint channel, uchar controller, int value)
{
	uint i;
	uint id = channel;

	switch (controller)
	{
	case ctrlPatch:			/* change instrument */
		OPLprogramChange(channel, value);
		break;

	case ctrlModulation:
		driverdata.channelModulation[channel] = value;
		for(i = 0; i < io->OPLchannels; i++)
		{
			struct channelEntry *ch = &channels[i];
			if (ch->channel == id)
			{
				uchar flags = ch->flags;
				ch->time = MLtime;
				if (value >= MOD_MIN)
				{
					ch->flags |= CH_VIBRATO;
					if (ch->flags != flags)
						writeModulation(i, ch->instr, 1);
				} else {
					ch->flags &= ~CH_VIBRATO;
					if (ch->flags != flags)
						writeModulation(i, ch->instr, 0);
				}
			}
		}
		break;

	case ctrlVolume:		/* change volume */
		driverdata.channelVolume[channel] = value;
		/* fall-through */
	case ctrlExpression:	/* change expression */
		if (controller == ctrlExpression)
		{
			driverdata.channelExpression[channel] = value;
		}
		for(i = 0; i < io->OPLchannels; i++)
		{
			struct channelEntry *ch = &channels[i];
			if (ch->channel == id)
			{
				ch->time = MLtime;
				ch->realvolume = calcVolume(driverdata.channelVolume[channel],
					driverdata.channelExpression[channel], ch->volume);
				io->OPLwriteVolume(i, ch->instr, ch->realvolume);
			}
		}
		break;

	case ctrlPan:			/* change pan (balance) */
		driverdata.channelPan[channel] = value -= 64;
		for(i = 0; i < io->OPLchannels; i++)
		{
			struct channelEntry *ch = &channels[i];
			if (ch->channel == id)
			{
				ch->time = MLtime;
				io->OPLwritePan(i, ch->instr, value);
			}
		}
		break;

	case ctrlSustainPedal:		/* change sustain pedal (hold) */
		driverdata.channelSustain[channel] = value;
		if (value < 0x40)
			releaseSustain(channel);
		break;

	case ctrlNotesOff:			/* turn off all notes that are not sustained */
		for (i = 0; i < io->OPLchannels; ++i)
		{
			if (channels[i].channel == id)
			{
				if (driverdata.channelSustain[id] < 0x40)
					releaseChannel(i, 0);
				else
					channels[i].flags |= CH_SUSTAIN;
			}
		}
		break;

	case ctrlSoundsOff:			/* release all notes for this channel */
		for (i = 0; i < io->OPLchannels; ++i)
		{
			if (channels[i].channel == id)
			{
				releaseChannel(i, 0);
			}
		}
		break;

	case ctrlRPNHi:
		driverdata.channelRPN[id] = (driverdata.channelRPN[id] & 0x007F) | (value << 7);
		break;

	case ctrlRPNLo:
		driverdata.channelRPN[id] = (driverdata.channelRPN[id] & 0x3F80) | value;
		break;

	case ctrlNRPNLo:
	case ctrlNRPNHi:
		driverdata.channelRPN[id] = 0x3FFF;
		break;

	case ctrlDataEntryHi:
		if (driverdata.channelRPN[id] == 0)
		{
			driverdata.channelPitchSens[id] = value * 100 + (driverdata.channelPitchSens[id] % 100);
		}
		break;

	case ctrlDataEntryLo:
		if (driverdata.channelRPN[id] == 0)
		{
			driverdata.channelPitchSens[id] = value + (driverdata.channelPitchSens[id] / 100) * 100;
		}
		break;
	}
}

void musicBlock::OPLresetControllers(uint chan, int vol)
{
	driverdata.channelVolume[chan] = vol;
	driverdata.channelExpression[chan] = 127;
	driverdata.channelSustain[chan] = 0;
	driverdata.channelLastVolume[chan] = 64;
	driverdata.channelPitch[chan] = 64;
	driverdata.channelRPN[chan] = 0x3fff;
	driverdata.channelPitchSens[chan] = 200;
}

void musicBlock::OPLprogramChange(uint channel, int value)
{
	driverdata.channelInstr[channel] = value;
}

void musicBlock::OPLplayMusic(int vol)
{
	uint i;

	for (i = 0; i < CHANNELS; i++)
	{
		OPLresetControllers(i, vol);
	}
}

void musicBlock::OPLstopMusic()
{
	uint i;
	for(i = 0; i < io->OPLchannels; i++)
		if (!(channels[i].flags & CH_FREE))
			releaseChannel(i, 1);
}

int musicBlock::OPLloadBank (FileReader &data)
{
	static const uchar masterhdr[8] = { '#','O','P','L','_','I','I','#' };
	struct OP2instrEntry *instruments;

	uchar filehdr[8];

	data.Read (filehdr, 8);
	if (memcmp(filehdr, masterhdr, 8))
		return -2;			/* bad instrument file */
	if ( (instruments = (struct OP2instrEntry *)calloc(OP2INSTRCOUNT, OP2INSTRSIZE)) == NULL)
		return -3;			/* not enough memory */
	data.Read (instruments, OP2INSTRSIZE * OP2INSTRCOUNT);
	if (OPLinstruments != NULL)
	{
		free(OPLinstruments);
	}
	OPLinstruments = instruments;
#if 0
	for (int i = 0; i < 175; ++i)
	{
		Printf ("%3d.%-33s%3d %3d %3d %d\n", i,
			(BYTE *)data+6308+i*32,
			OPLinstruments[i].instr[0].basenote,
			OPLinstruments[i].instr[1].basenote,
			OPLinstruments[i].note,
			OPLinstruments[i].flags);
	}
#endif
	return 0;
}
