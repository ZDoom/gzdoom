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

#include <malloc.h>
#include <string.h>
#include <io.h>
#include "muslib.h"

#include "stats.h"
#include "v_font.h"
#include "c_cvars.h"

/* Internal variables */
static uint	OPLsinglevoice = 0;
static struct OP2instrEntry *OPLinstruments = NULL;


/* OPL channel (voice) data */
static struct channelEntry {
	uchar	channel;		/* MUS channel number */
	uchar	note;			/* note number */
	uchar	flags;			/* see CH_xxx below */
	uchar	realnote;		/* adjusted note number */
	schar	finetune;		/* frequency fine-tune */
	sint	pitch;			/* pitch-wheel value */
	uint	volume;			/* note volume */
	uint	realvolume;		/* adjusted note volume */
	struct OPL2instrument *instr;	/* current instrument */
	ulong	time;			/* note start time */
} channels[MAXCHANNELS];

ulong MLtime;

/* Flags: */
#define CH_SECONDARY	0x01
#define CH_SUSTAIN	0x02
#define CH_VIBRATO	0x04		/* set if modulation >= MOD_MIN */
#define CH_FREE		0x80

#define MOD_MIN		40		/* vibrato threshold */

#define MAKE_ID(ch, mus) ((uchar)(ch)|((uchar)(mus)<<8))


//#define HIGHEST_NOTE 102
#define HIGHEST_NOTE 127


static void writeFrequency(uint slot, uint note, int pitch, uint keyOn)
{
	OPLwriteFreq (slot, note, pitch, keyOn);
}

static void writeModulation(uint slot, struct OPL2instrument *instr, int state)
{
	if (state)
		state = 0x40;	/* enable Frequency Vibrato */
	OPLwriteChannel(0x20, slot,
		(instr->feedback & 1) ? (instr->trem_vibr_1 | state) : instr->trem_vibr_1,
		instr->trem_vibr_2 | state);
}

static uint calcVolume(uint channelVolume, uint MUSvolume, uint noteVolume)
{
	noteVolume = ((ulong)channelVolume * MUSvolume * noteVolume) / (256*127);
	if (noteVolume > 127)
		return 127;
	else
		return noteVolume;
}

static int occupyChannel(struct musicBlock *mus, uint slot, uint channel,
						 int note, int volume, struct OP2instrEntry *instrument, uchar secondary)
{
	struct OPL2instrument *instr;
	struct OPLdata *data = &mus->driverdata;
	struct channelEntry *ch = &channels[slot];

	ch->channel = channel;
	ch->note = note;
	ch->flags = secondary ? CH_SECONDARY : 0;
	if (data->channelModulation[channel] >= MOD_MIN)
		ch->flags |= CH_VIBRATO;
	ch->time = MLtime;
	if (volume == -1)
		volume = data->channelLastVolume[channel];
	else
		data->channelLastVolume[channel] = volume;
	ch->realvolume = calcVolume(data->channelVolume[channel], 256, ch->volume = volume);
	if (instrument->flags & FL_FIXED_PITCH)
		note = instrument->note;
	else if (channel == PERCUSSION)
		note = 60;			// C-5
	if (secondary && (instrument->flags & FL_DOUBLE_VOICE))
		ch->finetune = (instrument->finetune - 0x80) >> 1;
	else
		ch->finetune = 0;
	ch->pitch = ch->finetune + data->channelPitch[channel];
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

	OPLwriteInstrument(slot, instr);
	if (ch->flags & CH_VIBRATO)
		writeModulation(slot, instr, 1);
	OPLwritePan(slot, instr, data->channelPan[channel]);
	OPLwriteVolume(slot, instr, ch->realvolume);
	writeFrequency(slot, note, ch->pitch, 1);
	return slot;
}

static int releaseChannel(struct musicBlock *mus, uint slot, uint killed)
{
	struct channelEntry *ch = &channels[slot];
	writeFrequency(slot, ch->realnote, ch->pitch, 0);
	ch->channel |= CH_FREE;
	ch->time = MLtime;
	ch->flags = CH_FREE;
	if (killed)
	{
		OPLwriteChannel(0x80, slot, 0x0F, 0x0F);  // release rate - fastest
		OPLwriteChannel(0x40, slot, 0x3F, 0x3F);  // no volume
	}
	return slot;
}

static int releaseSustain(struct musicBlock *mus, uint channel)
{
	uint i;
	uint id = channel;

	for(i = 0; i < OPLchannels; i++)
	{
		if (channels[i].channel == id && channels[i].flags & CH_SUSTAIN)
			releaseChannel(mus, i, 0);
	}
	return 0;
}

static int findFreeChannel(struct musicBlock *mus, uint flag, uint channel, uchar note)
{
	static uint last = 1000000;
	uint i;
	uint oldest = 1000000;
	ulong oldesttime = MLtime;

	ulong bestfit = 0;
	uint bestvoice = 0;

	for (i = 0; i < OPLchannels; ++i)
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
	releaseChannel (mus, bestvoice, 1);
	return bestvoice;
}

static struct OP2instrEntry *getInstrument(struct musicBlock *mus, uint channel, uchar note)
{
	uint instrnumber;

	if (channel == PERCUSSION)
	{
		if (note < 35 || note > 81)
			return NULL;		/* wrong percussion number */
		instrnumber = note + (128-35);
	} else
	{
		instrnumber = mus->driverdata.channelInstr[channel];
	}

	if (OPLinstruments)
		return &OPLinstruments[instrnumber];
	else
		return NULL;
}


// code 1: play note
CVAR (Bool, opl_singlevoice, 0, 0)

void OPLplayNote(struct musicBlock *mus, uint channel, uchar note, int volume)
{
	int i;
	struct OP2instrEntry *instr;

	if (volume == 0)
	{
		OPLreleaseNote (mus, channel, note);
		return;
	}

	if ( (instr = getInstrument(mus, channel, note)) == NULL )
		return;

	if ( (i = findFreeChannel(mus, (channel == PERCUSSION) ? 2 : 0, channel, note)) != -1)
	{
		occupyChannel(mus, i, channel, note, volume, instr, 0);
		if (!OPLsinglevoice && (instr->flags & FL_DOUBLE_VOICE) && !opl_singlevoice)
		{
			if ( (i = findFreeChannel(mus, (channel == PERCUSSION) ? 3 : 1, channel, note)) != -1)
				occupyChannel(mus, i, channel, note, volume, instr, 1);
		}
	}
}

// code 0: release note
void OPLreleaseNote(struct musicBlock *mus, uint channel, uchar note)
{
	uint i;
	uint id = channel;
	struct OPLdata *data = &mus->driverdata;
	uint sustain = data->channelSustain[channel];

	for(i = 0; i < OPLchannels; i++)
	{
		if (channels[i].channel == id && channels[i].note == note)
		{
			if (sustain < 0x40)
				releaseChannel(mus, i, 0);
			else
				channels[i].flags |= CH_SUSTAIN;
		}
	}
}

// code 2: change pitch wheel (bender)
void OPLpitchWheel(struct musicBlock *mus, uint channel, int pitch)
{
	uint i;
	uint id = channel;
	struct OPLdata *data = &mus->driverdata;

	//pitch -= 0x80;
	pitch >>= 1;
	data->channelPitch[channel] = pitch;
	for(i = 0; i < OPLchannels; i++)
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
void OPLchangeControl(struct musicBlock *mus, uint channel, uchar controller, int value)
{
	uint i;
	uint id = channel;
	struct OPLdata *data = &mus->driverdata;

	switch (controller)
	{
	case ctrlPatch:			/* change instrument */
		data->channelInstr[channel] = value;
		break;
	case ctrlModulation:
		data->channelModulation[channel] = value;
		for(i = 0; i < OPLchannels; i++)
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
		data->channelVolume[channel] = value;
		for(i = 0; i < OPLchannels; i++)
		{
			struct channelEntry *ch = &channels[i];
			if (ch->channel == id)
			{
				ch->time = MLtime;
				ch->realvolume = calcVolume(value, 256, ch->volume);
				OPLwriteVolume(i, ch->instr, ch->realvolume);
			}
		}
		break;
	case ctrlPan:			/* change pan (balance) */
		data->channelPan[channel] = value -= 64;
		for(i = 0; i < OPLchannels; i++)
		{
			struct channelEntry *ch = &channels[i];
			if (ch->channel == id)
			{
				ch->time = MLtime;
				OPLwritePan(i, ch->instr, value);
			}
		}
		break;
	case ctrlSustainPedal:		/* change sustain pedal (hold) */
		data->channelSustain[channel] = value;
		if (value < 0x40)
			releaseSustain(mus, channel);
		break;
	}
}


void OPLplayMusic(struct musicBlock *mus)
{
	uint i;
	struct OPLdata *data = &mus->driverdata;

	for (i = 0; i < CHANNELS; i++)
	{
		data->channelVolume[i] = 127;	/* default volume 127 (full volume) */
		data->channelSustain[i] = 0;
		data->channelLastVolume[i] = 64;
		data->channelPitch[i] = 64;
	}
}

void OPLstopMusic(struct musicBlock *mus)
{
	uint i;
	for(i = 0; i < OPLchannels; i++)
		if (!(channels[i].flags & CH_FREE))
			releaseChannel(mus, i, 1);
}

void OPLchangeVolume(struct musicBlock *mus, uint volume)
{
	uchar *channelVolume = mus->driverdata.channelVolume;
	uint i;
	for(i = 0; i < OPLchannels; i++)
	{
		struct channelEntry *ch = &channels[i];
		ch->realvolume = calcVolume(channelVolume[ch->channel & 0xF], volume, ch->volume);
		OPLwriteVolume(i, ch->instr, ch->realvolume);
	}
}

int OPLinitDriver(void)
{
	memset(channels, 0xFF, sizeof channels);
	OPLinstruments = NULL;
	return 0;
}

int OPLdeinitDriver(void)
{
	free(OPLinstruments);
	OPLinstruments = NULL;
	return 0;
}

int OPLdriverParam(uint message, uint param1, void *param2)
{
	switch (message)
	{
	case DP_SINGLE_VOICE:
		OPLsinglevoice = param1;
		break;
	}
	return 0;
}

int OPLloadBank (const void *data)
{
	static uchar masterhdr[8] = { '#','O','P','L','_','I','I','#' };
	struct OP2instrEntry *instruments;

	if (memcmp(data, masterhdr, 8))
		return -2;			/* bad instrument file */
	if ( (instruments = (struct OP2instrEntry *)calloc(OP2INSTRCOUNT, OP2INSTRSIZE)) == NULL)
		return -3;			/* not enough memory */
	memcpy (instruments, (BYTE *)data + 8, OP2INSTRSIZE * OP2INSTRCOUNT);
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

ADD_STAT (opl, out)
{
	uint i;

	for (i = 0; i < OPLchannels; ++i)
	{
		int color;

		if (channels[i].flags & CH_FREE)
		{
			color = CR_BRICK;
		}
		else if (channels[i].flags & CH_SUSTAIN)
		{
			color = CR_ORANGE;
		}
		else if (channels[i].flags & CH_SECONDARY)
		{
			color = CR_BLUE;
		}
		else
		{
			color = CR_GREEN;
		}
		out[i*3+0] = '\x1c';
		out[i*3+1] = 'A'+color;
		out[i*3+2] = '*';
	}
	out[i*3] = 0;
}
