/*
 *	Name:		Main header include file
 *	Project:	MUS File Player Library
 *	Version:	1.75
 *	Author:		Vladimir Arnost (QA-Software)
 *	Last revision:	Mar-9-1996
 *	Compiler:	Borland C++ 3.1, Watcom C/C++ 10.0
 *
 */

/* From muslib175.zip/README.1ST:

1.1 - Disclaimer of Warranties
------------------------------

#ifdef LAWYER

THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
SUCH DAMAGE.

#else

Use this software at your own risk.

#endif


1.2 - Terms of Use
------------------

This library may be used in any freeware or shareware product free of
charge. The product may not be sold for profit (except for shareware) and
should be freely available to the public. It would be nice of you if you
credited me in your product and notified me if you use this library.

If you want to use this library in a commercial product, contact me
and we will make an agreement. It is a violation of the law to make money
of this product without prior signing an agreement and paying a license fee.
This licence will allow its holder to sell any products based on MUSLib,
royalty-free. There is no need to buy separate licences for different
products once the licence fee is paid.


1.3 - Contacting the Author
---------------------------

Internet (address valid probably until the end of year 1998):
  xarnos00@dcse.fee.vutbr.cz

FIDO:
  2:423/36.2

Snail-mail:

  Vladimir Arnost
  Ceska 921
  Chrudim 4
  537 01
  CZECH REPUBLIC

Voice-mail (Czech language only, not recommended; weekends only):

  +42-455-2154
*/

#ifndef __MUSLIB_H_
#define __MUSLIB_H_

#ifndef __DEFTYPES_H_
  #include "deftypes.h"
#endif

class FileReader;

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
	WORD	sec_channels;	// secondary channels (??)
	WORD    instrCnt;		// used instrument count
	WORD	dummy;
//	WORD	instruments[...];	// table of used instruments
};

/* OPL2 instrument */
struct OPL2instrument {
/*00*/	BYTE    trem_vibr_1;	/* OP 1: tremolo/vibrato/sustain/KSR/multi */
/*01*/	BYTE	att_dec_1;		/* OP 1: attack rate/decay rate */
/*02*/	BYTE	sust_rel_1;		/* OP 1: sustain level/release rate */
/*03*/	BYTE	wave_1;			/* OP 1: waveform select */
/*04*/	BYTE	scale_1;		/* OP 1: key scale level */
/*05*/	BYTE	level_1;		/* OP 1: output level */
/*06*/	BYTE	feedback;		/* feedback/AM-FM (both operators) */
/*07*/	BYTE    trem_vibr_2;	/* OP 2: tremolo/vibrato/sustain/KSR/multi */
/*08*/	BYTE	att_dec_2;		/* OP 2: attack rate/decay rate */
/*09*/	BYTE	sust_rel_2;		/* OP 2: sustain level/release rate */
/*0A*/	BYTE	wave_2;			/* OP 2: waveform select */
/*0B*/	BYTE	scale_2;		/* OP 2: key scale level */
/*0C*/	BYTE	level_2;		/* OP 2: output level */
/*0D*/	BYTE	unused;
/*0E*/	sshort	basenote;		/* base note offset */
};

/* OP2 instrument file entry */
struct OP2instrEntry {
/*00*/	WORD	flags;				// see FL_xxx below
/*02*/	BYTE	finetune;			// finetune value for 2-voice sounds
/*03*/	BYTE	note;				// note # for fixed instruments
/*04*/	struct OPL2instrument instr[2];	// instruments
};

#define FL_FIXED_PITCH	0x0001		// note has fixed pitch (see below)
#define FL_UNKNOWN		0x0002		// ??? (used in instrument #65 only)
#define FL_DOUBLE_VOICE	0x0004		// use two voices instead of one


#define OP2INSTRSIZE	sizeof(struct OP2instrEntry) // instrument size (36 bytes)
#define OP2INSTRCOUNT	(128 + 81-35+1)	// instrument count

/* From MLOPL_IO.CPP */
#define OPL2CHANNELS	9
#define OPL3CHANNELS	18
#define MAXOPL2CHIPS	8
#define MAXCHANNELS		(OPL2CHANNELS * MAXOPL2CHIPS)


/* Channel Flags: */
#define CH_SECONDARY	0x01
#define CH_SUSTAIN		0x02
#define CH_VIBRATO		0x04		/* set if modulation >= MOD_MIN */
#define CH_FREE			0x80

struct OPLdata {
	uint	channelInstr[CHANNELS];			// instrument #
	uchar	channelVolume[CHANNELS];		// volume
	uchar	channelLastVolume[CHANNELS];	// last volume
	schar	channelPan[CHANNELS];			// pan, 0=normal
	schar	channelPitch[CHANNELS];			// pitch wheel, 64=normal
	uchar	channelSustain[CHANNELS];		// sustain pedal value
	uchar	channelModulation[CHANNELS];	// modulation pot value
	ushort	channelPitchSens[CHANNELS];		// pitch sensitivity, 2=default
	ushort	channelRPN[CHANNELS];			// RPN number for data entry
	uchar	channelExpression[CHANNELS];	// expression
};

struct OPLio {
	virtual ~OPLio();

	void	OPLwriteChannel(uint regbase, uint channel, uchar data1, uchar data2);
	void	OPLwriteValue(uint regbase, uint channel, uchar value);
	void	OPLwriteFreq(uint channel, uint freq, uint octave, uint keyon);
	uint	OPLconvertVolume(uint data, uint volume);
	uint	OPLpanVolume(uint volume, int pan);
	void	OPLwriteVolume(uint channel, struct OPL2instrument *instr, uint volume);
	void	OPLwritePan(uint channel, struct OPL2instrument *instr, int pan);
	void	OPLwriteInstrument(uint channel, struct OPL2instrument *instr);
	void	OPLshutup(void);
	void	OPLwriteInitState(bool initopl3);

	virtual int		OPLinit(uint numchips, bool stereo=false, bool initopl3=false);
	virtual void	OPLdeinit(void);
	virtual void	OPLwriteReg(int which, uint reg, uchar data);
	virtual void	SetClockRate(double samples_per_tick);
	virtual void	WriteDelay(int ticks);

	class OPLEmul *chips[MAXOPL2CHIPS];
	uint OPLchannels;
	uint NumChips;
	bool IsOPL3;
};

struct DiskWriterIO : public OPLio
{
	DiskWriterIO(const char *filename);
	~DiskWriterIO();

	int OPLinit(uint numchips, bool notused, bool initopl3);
	void SetClockRate(double samples_per_tick);
	void WriteDelay(int ticks);

	FString Filename;
};

struct musicBlock {
	musicBlock();
	~musicBlock();

	BYTE *score;
	BYTE *scoredata;
	int playingcount;
	OPLdata driverdata;
	OPLio *io;

	struct OP2instrEntry *OPLinstruments;

	ulong MLtime;

	void OPLplayNote(uint channel, uchar note, int volume);
	void OPLreleaseNote(uint channel, uchar note);
	void OPLpitchWheel(uint channel, int pitch);
	void OPLchangeControl(uint channel, uchar controller, int value);
	void OPLprogramChange(uint channel, int value);
	void OPLresetControllers(uint channel, int vol);
	void OPLplayMusic(int vol);
	void OPLstopMusic();

	int OPLloadBank (FileReader &data);

protected:
	/* OPL channel (voice) data */
	struct channelEntry {
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

	void writeFrequency(uint slot, uint note, int pitch, uint keyOn);
	void writeModulation(uint slot, struct OPL2instrument *instr, int state);
	uint calcVolume(uint channelVolume, uint channelExpression, uint noteVolume);
	int occupyChannel(uint slot, uint channel,
						 int note, int volume, struct OP2instrEntry *instrument, uchar secondary);
	int releaseChannel(uint slot, uint killed);
	int releaseSustain(uint channel);
	int findFreeChannel(uint flag, uint channel, uchar note);
	struct OP2instrEntry *getInstrument(uint channel, uchar note);

	friend class Stat_opl;

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
	ctrlRPNHi,
	ctrlRPNLo,
	ctrlNRPNHi,
	ctrlNRPNLo,
	ctrlDataEntryHi,
	ctrlDataEntryLo,

	ctrlSoundsOff,
    ctrlNotesOff,
    ctrlMono,
    ctrlPoly,
};

#define ADLIB_CLOCK_MUL			24.0

#endif // __MUSLIB_H_
