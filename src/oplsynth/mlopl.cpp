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


static WORD freqtable[] = {					 /* note # */
	345, 365, 387, 410, 435, 460, 488, 517, 547, 580, 615, 651,  /*  0 */
	690, 731, 774, 820, 869, 921, 975, 517, 547, 580, 615, 651,  /* 12 */
	690, 731, 774, 820, 869, 921, 975, 517, 547, 580, 615, 651,  /* 24 */
	690, 731, 774, 820, 869, 921, 975, 517, 547, 580, 615, 651,  /* 36 */
	690, 731, 774, 820, 869, 921, 975, 517, 547, 580, 615, 651,  /* 48 */
	690, 731, 774, 820, 869, 921, 975, 517, 547, 580, 615, 651,  /* 60 */
	690, 731, 774, 820, 869, 921, 975, 517, 547, 580, 615, 651,  /* 72 */
	690, 731, 774, 820, 869, 921, 975, 517, 547, 580, 615, 651,  /* 84 */
	690, 731, 774, 820, 869, 921, 975, 517, 547, 580, 615, 651,  /* 96 */
	690, 731, 774, 820, 869, 921, 975, 517, 547, 580, 615, 651, /* 108 */
	690, 731, 774, 820, 869, 921, 975, 517					    /* 120 */
};

static BYTE octavetable[] = {					 /* note # */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,		     /*  0 */
	0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1,		     /* 12 */
	1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2,		     /* 24 */
	2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3,		     /* 36 */
	3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4,		     /* 48 */
	4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5,		     /* 60 */
	5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6,		     /* 72 */
	6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7,		     /* 84 */
	7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8,		     /* 96 */
	8, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9,		    /* 108 */
	9, 9, 9, 9, 9, 9, 9,10						/* 120 */
};

//#define HIGHEST_NOTE 102
#define HIGHEST_NOTE 127

static WORD pitchtable[] = {				    /* pitch wheel */
	29193U,29219U,29246U,29272U,29299U,29325U,29351U,29378U,  /* -128 */
	29405U,29431U,29458U,29484U,29511U,29538U,29564U,29591U,  /* -120 */
	29618U,29644U,29671U,29698U,29725U,29752U,29778U,29805U,  /* -112 */
	29832U,29859U,29886U,29913U,29940U,29967U,29994U,30021U,  /* -104 */
	30048U,30076U,30103U,30130U,30157U,30184U,30212U,30239U,  /*  -96 */
	30266U,30293U,30321U,30348U,30376U,30403U,30430U,30458U,  /*  -88 */
	30485U,30513U,30541U,30568U,30596U,30623U,30651U,30679U,  /*  -80 */
	30706U,30734U,30762U,30790U,30817U,30845U,30873U,30901U,  /*  -72 */
	30929U,30957U,30985U,31013U,31041U,31069U,31097U,31125U,  /*  -64 */
	31153U,31181U,31209U,31237U,31266U,31294U,31322U,31350U,  /*  -56 */
	31379U,31407U,31435U,31464U,31492U,31521U,31549U,31578U,  /*  -48 */
	31606U,31635U,31663U,31692U,31720U,31749U,31778U,31806U,  /*  -40 */
	31835U,31864U,31893U,31921U,31950U,31979U,32008U,32037U,  /*  -32 */
	32066U,32095U,32124U,32153U,32182U,32211U,32240U,32269U,  /*  -24 */
	32298U,32327U,32357U,32386U,32415U,32444U,32474U,32503U,  /*  -16 */
	32532U,32562U,32591U,32620U,32650U,32679U,32709U,32738U,  /*   -8 */
	32768U,32798U,32827U,32857U,32887U,32916U,32946U,32976U,  /*    0 */
	33005U,33035U,33065U,33095U,33125U,33155U,33185U,33215U,  /*    8 */
	33245U,33275U,33305U,33335U,33365U,33395U,33425U,33455U,  /*   16 */
	33486U,33516U,33546U,33576U,33607U,33637U,33667U,33698U,  /*   24 */
	33728U,33759U,33789U,33820U,33850U,33881U,33911U,33942U,  /*   32 */
	33973U,34003U,34034U,34065U,34095U,34126U,34157U,34188U,  /*   40 */
	34219U,34250U,34281U,34312U,34343U,34374U,34405U,34436U,  /*   48 */
	34467U,34498U,34529U,34560U,34591U,34623U,34654U,34685U,  /*   56 */
	34716U,34748U,34779U,34811U,34842U,34874U,34905U,34937U,  /*   64 */
	34968U,35000U,35031U,35063U,35095U,35126U,35158U,35190U,  /*   72 */
	35221U,35253U,35285U,35317U,35349U,35381U,35413U,35445U,  /*   80 */
	35477U,35509U,35541U,35573U,35605U,35637U,35669U,35702U,  /*   88 */
	35734U,35766U,35798U,35831U,35863U,35895U,35928U,35960U,  /*   96 */
	35993U,36025U,36058U,36090U,36123U,36155U,36188U,36221U,  /*  104 */
	36254U,36286U,36319U,36352U,36385U,36417U,36450U,36483U,  /*  112 */
	36516U,36549U,36582U,36615U,36648U,36681U,36715U,36748U	  /*  120 */
};


static void writeFrequency(uint slot, uint note, int pitch, uint keyOn)
{
	uint freq = freqtable[note];
	uint octave = octavetable[note];

	if (pitch)
	{
		if (pitch > 127) pitch = 127;
		else if (pitch < -128) pitch = -128;
		freq = ((ulong)freq * pitchtable[pitch + 128]) >> 15;
		if (freq >= 1024)
		{
			freq >>= 1;
			octave++;
		}
	}
	if (octave > 7)
		octave = 7;
	OPLwriteFreq(slot, freq, octave, keyOn);
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
		ch->finetune = instrument->finetune - 0x80;
	else
		ch->finetune = 0;
	ch->pitch = ch->finetune + data->channelPitch[channel];
	if (secondary)
		instr = &instrument->instr[1];
	else
		instr = &instrument->instr[0];
	ch->instr = instr;
	if ( (note += instr->basenote) < 0)
		while ((note += 12) < 0) {}
	else if (note > HIGHEST_NOTE)
		while ((note -= 12) > HIGHEST_NOTE) {}
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

static int findFreeChannel(struct musicBlock *mus, uint flag)
{
	static uint last = 1000000;
	uint i;
	uint oldest = 1000000;
	ulong oldesttime = MLtime;

	if (last >= OPLchannels)
	{
		last = 1000000;
	}

	/* find free channel */
	for (i = 0; i < OPLchannels; i++)
	{
		if (++last >= OPLchannels)	/* use cyclic `Next Fit' algorithm */
			last = 0;
		if (channels[last].flags & CH_FREE)
			return last;
	}

	if (flag & 1)
		return -1;			/* stop searching if bit 0 is set */

	/* find some 2nd-voice channel and determine the oldest */
	for (i = 0; i < OPLchannels; i++)
	{
		if (channels[i].flags & CH_SECONDARY)
		{
			releaseChannel(mus, i, 1);
			return i;
		}
		else if (channels[i].time < oldesttime)
		{
			oldesttime = channels[i].time;
			oldest = i;
		}
	}

	/* if possible, kill the oldest channel */
	if ( !(flag & 2) && oldest != 1000000)
	{
		releaseChannel(mus, oldest, 1);
		return oldest;
	}

	/* can't find any free channel */
	return -1;
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
		instrnumber = mus->driverdata.channelInstr[channel];

	if (OPLinstruments)
		return &OPLinstruments[instrnumber];
	else
		return NULL;
}


// code 1: play note
void OPLplayNote(struct musicBlock *mus, uint channel, uchar note, int volume)
{
	int i;
	struct OP2instrEntry *instr;

	if ( (instr = getInstrument(mus, channel, note)) == NULL )
		return;

	if ( (i = findFreeChannel(mus, (channel == PERCUSSION) ? 2 : 0)) != -1)
	{
		occupyChannel(mus, i, channel, note, volume, instr, 0);
		if (!OPLsinglevoice && instr->flags == FL_DOUBLE_VOICE)
		{
			if ( (i = findFreeChannel(mus, (channel == PERCUSSION) ? 3 : 1)) != -1)
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
		if (channels[i].channel == id && channels[i].note == note)
		{
			if (sustain < 0x40)
				releaseChannel(mus, i, 0);
			else
				channels[i].flags |= CH_SUSTAIN;
		}
}

// code 2: change pitch wheel (bender)
void OPLpitchWheel(struct musicBlock *mus, uint channel, int pitch)
{
	uint i;
	uint id = channel;
	struct OPLdata *data = &mus->driverdata;

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
		data->channelVolume[i] = 64;	/* default volume 127 (full volume) */ /* [RH] default to 64 */
		data->channelSustain[i] = data->channelLastVolume[i] = 0;
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
	return 0;
}
