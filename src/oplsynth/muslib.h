/*
 *	Name:		Main header include file
 *	Project:	MUS File Player Library
 *	Version:	1.75
 *	Author:		Vladimir Arnost (QA-Software)
 *	Last revision:	Mar-9-1996
 *	Compiler:	Borland C++ 3.1, Watcom C/C++ 10.0
 *
 */

#ifndef __MUSLIB_H_
#define __MUSLIB_H_

#ifndef __DEFTYPES_H_
  #include "deftypes.h"
#endif

/* Global Definitions */

#define MLVERSION	0x0175
#define MLVERSIONSTR	"1.75"
extern char MLversion[];
extern char MLcopyright[];

#define CHANNELS	16		// total channels 0..CHANNELS-1
#define PERCUSSION	15		// percussion channel

/* MUS file header structure */
struct MUSheader {
	char	ID[4];			// identifier "MUS" 0x1A
	WORD	scoreLen;		// score length
	WORD	scoreStart;		// score start
	WORD	channels;		// primary channels
	WORD	sec_channels;		// secondary channels (??)
	WORD    instrCnt;		// used instrument count
	WORD	dummy;
//	WORD	instruments[...];	// table of used instruments
};

/* OPL2 instrument */
struct OPL2instrument {
/*00*/	BYTE    trem_vibr_1;	/* OP 1: tremolo/vibrato/sustain/KSR/multi */
/*01*/	BYTE	att_dec_1;	/* OP 1: attack rate/decay rate */
/*02*/	BYTE	sust_rel_1;	/* OP 1: sustain level/release rate */
/*03*/	BYTE	wave_1;		/* OP 1: waveform select */
/*04*/	BYTE	scale_1;	/* OP 1: key scale level */
/*05*/	BYTE	level_1;	/* OP 1: output level */
/*06*/	BYTE	feedback;	/* feedback/AM-FM (both operators) */
/*07*/	BYTE    trem_vibr_2;	/* OP 2: tremolo/vibrato/sustain/KSR/multi */
/*08*/	BYTE	att_dec_2;	/* OP 2: attack rate/decay rate */
/*09*/	BYTE	sust_rel_2;	/* OP 2: sustain level/release rate */
/*0A*/	BYTE	wave_2;		/* OP 2: waveform select */
/*0B*/	BYTE	scale_2;	/* OP 2: key scale level */
/*0C*/	BYTE	level_2;	/* OP 2: output level */
/*0D*/	BYTE	unused;
/*0E*/	sshort	basenote;	/* base note offset */
};

/* OP2 instrument file entry */
struct OP2instrEntry {
/*00*/	WORD	flags;			// see FL_xxx below
/*02*/	BYTE	finetune;		// finetune value for 2-voice sounds
/*03*/	BYTE	note;			// note # for fixed instruments
/*04*/	struct OPL2instrument instr[2];	// instruments
};

#define FL_FIXED_PITCH	0x0001		// note has fixed pitch (see below)
#define FL_UNKNOWN	0x0002		// ??? (used in instrument #65 only)
#define FL_DOUBLE_VOICE	0x0004		// use two voices instead of one


#define OP2INSTRSIZE	sizeof(struct OP2instrEntry) // instrument size (36 bytes)
#define OP2INSTRCOUNT	(128 + 81-35+1)	// instrument count

struct OPLdata {
	uint	channelInstr[CHANNELS];		// instrument #
	uchar	channelVolume[CHANNELS];	// volume
	uchar	channelLastVolume[CHANNELS];	// last volume
	schar	channelPan[CHANNELS];		// pan, 0=normal
	schar	channelPitch[CHANNELS];		// pitch wheel, 0=normal
	uchar	channelSustain[CHANNELS];	// sustain pedal value
	uchar	channelModulation[CHANNELS];	// modulation pot value
};

struct musicBlock {
	BYTE *score;
	BYTE *scoredata;
	int playingcount;
	OPLdata driverdata;
};

enum MUSctrl {
    ctrlPatch = 0,
    ctrlBank,
    ctrlModulation,
    ctrlVolume,
    ctrlPan,
    ctrlExpression,
    ctrlReverb,
    ctrlChorus,
    ctrlSustainPedal,
    ctrlSoftPedal,
    _ctrlCount_,
    ctrlSoundsOff = _ctrlCount_,
    ctrlNotesOff,
    ctrlMono,
    ctrlPoly,
    ctrlResetCtrls
};

/* driverParam message codes */
#define DP_SINGLE_VOICE		0x0002	/* OPLx: disabling double-voice mode */
/* DP_SINGLE_VOICE: param1 codes */
#define DPP_SINGLE_VOICE_OFF	0	/* default: off */
#define DPP_SINGLE_VOICE_ON		1

/* From MLKERNEL.CPP */
int playTick (musicBlock *mus);

/* From MLOPL.CPP */
void	OPLplayNote(struct musicBlock *mus, uint channel, uchar note, int volume);
void	OPLreleaseNote(struct musicBlock *mus, uint channel, uchar note);
void	OPLpitchWheel(struct musicBlock *mus, uint channel, int pitch);
void	OPLchangeControl(struct musicBlock *mus, uint channel, uchar controller, int value);
void OPLplayMusic(struct musicBlock *mus);
void OPLstopMusic(struct musicBlock *mus);

int	OPLdriverParam(uint message, uint param1, void *param2);
int	OPLloadBank(void *data);

extern ulong MLtime;

/* From MLOPL_IO.CPP */
#define OPL2CHANNELS	9
#define OPL3CHANNELS	18
#define MAXCHANNELS	18

extern uint OPLchannels;

void	OPLwriteChannel(uint regbase, uint channel, uchar data1, uchar data2);
void	OPLwriteValue(uint regbase, uint channel, uchar value);
void	OPLwriteFreq(uint channel, uint freq, uint octave, uint keyon);
uint	OPLconvertVolume(uint data, uint volume);
uint	OPLpanVolume(uint volume, int pan);
void	OPLwriteVolume(uint channel, struct OPL2instrument *instr, uint volume);
void	OPLwritePan(uint channel, struct OPL2instrument *instr, int pan);
void	OPLwriteInstrument(uint channel, struct OPL2instrument *instr);
void	OPLshutup(void);
int		OPLinit(uint numchips, uint rate);
void	OPLdeinit(void);

#endif // __MUSLIB_H_
