/*
*	Name:		Low-level OPL2/OPL3 I/O interface
*	Project:	MUS File Player Library
*	Version:	1.64
*	Author:		Vladimir Arnost (QA-Software)
*	Last revision:	Mar-1-1996
*	Compiler:	Borland C++ 3.1, Watcom C/C++ 10.0
*
*/

/*
* Revision History:
*
*	Aug-8-1994	V1.00	V.Arnost
*		Written from scratch
*	Aug-9-1994	V1.10	V.Arnost
*		Added stereo capabilities
*	Aug-13-1994	V1.20	V.Arnost
*		Stereo capabilities made functional
*	Aug-24-1994	V1.30	V.Arnost
*		Added Adlib and SB Pro II detection
*	Oct-30-1994	V1.40	V.Arnost
*		Added BLASTER variable parsing
*	Apr-14-1995	V1.50	V.Arnost
*              Some declarations moved from adlib.h to deftypes.h
*	Jul-22-1995	V1.60	V.Arnost
*		Ported to Watcom C
*		Simplified WriteChannel() and WriteValue()
*	Jul-24-1995	V1.61	V.Arnost
*		DetectBlaster() moved to MLMISC.C
*	Aug-8-1995	V1.62	V.Arnost
*		Module renamed to MLOPL_IO.C and functions renamed to OPLxxx
*		Mixer-related functions moved to module MLSBMIX.C
*	Sep-8-1995	V1.63	V.Arnost
*		OPLwriteReg() routine sped up on OPL3 cards
*	Mar-1-1996	V1.64	V.Arnost
*		Cleaned up the source
*/

#include <dos.h>
#include <conio.h>
#include "muslib.h"
#include "fmopl.h"

uint OPLchannels = OPL2CHANNELS;

void OPLwriteReg(int which, uint reg, uchar data)
{
	YM3812Write (which, 0, reg);
	YM3812Write (which, 1, data);
}

/*
* Write to an operator pair. To be used for register bases of 0x20, 0x40,
* 0x60, 0x80 and 0xE0.
*/
inline void OPLwriteChannel(uint regbase, uint channel, uchar data1, uchar data2)
{
	static const uint op_num[] = {
		0x000, 0x001, 0x002, 0x008, 0x009, 0x00A, 0x010, 0x011, 0x012,
		0x100, 0x101, 0x102, 0x108, 0x109, 0x10A, 0x110, 0x111, 0x112};

	uint reg = regbase+op_num[channel];
	uint which = reg>>8;
	OPLwriteReg (which, reg, data1);
	OPLwriteReg (which, reg+3, data2);
}

/*
* Write to channel a single value. To be used for register bases of
* 0xA0, 0xB0 and 0xC0.
*/
inline void OPLwriteValue(uint regbase, uint channel, uchar value)
{
	static const uint reg_num[] = {
		0x000, 0x001, 0x002, 0x003, 0x004, 0x005, 0x006, 0x007, 0x008,
		0x100, 0x101, 0x102, 0x103, 0x104, 0x105, 0x106, 0x107, 0x108};

	uint reg = regbase+reg_num[channel];
	uint which = reg>>8;
	OPLwriteReg (which, reg, value);
}

/*
* Write frequency/octave/keyon data to a channel
*/
void OPLwriteFreq(uint channel, uint freq, uint octave, uint keyon)
{
	OPLwriteValue(0xA0, channel, (BYTE)freq);
	OPLwriteValue(0xB0, channel, (freq >> 8) | (octave << 2) | (keyon << 5));
}

/*
* Adjust volume value (register 0x40)
*/
inline uint OPLconvertVolume(uint data, uint volume)
{
	static uchar volumetable[128] = {
		0,   1,   3,   5,   6,   8,  10,  11,
		13,  14,  16,  17,  19,  20,  22,  23,
		25,  26,  27,  29,  30,  32,  33,  34,
		36,  37,  39,  41,  43,  45,  47,  49,
		50,  52,  54,  55,  57,  59,  60,  61,
		63,  64,  66,  67,  68,  69,  71,  72,
		73,  74,  75,  76,  77,  79,  80,  81,
		82,  83,  84,  84,  85,  86,  87,  88,
		89,  90,  91,  92,  92,  93,  94,  95,
		96,  96,  97,  98,  99,  99, 100, 101,
		101, 102, 103, 103, 104, 105, 105, 106,
		107, 107, 108, 109, 109, 110, 110, 111,
		112, 112, 113, 113, 114, 114, 115, 115,
		116, 117, 117, 118, 118, 119, 119, 120,
		120, 121, 121, 122, 122, 123, 123, 123,
		124, 124, 125, 125, 126, 126, 127, 127};

	return 0x3F - (((0x3F - data) *
		(uint)volumetable[volume <= 127 ? volume : 127]) >> 7);
}

uint OPLpanVolume(uint volume, int pan)
{
	if (pan >= 0)
		return volume;
	else
		return (volume * (pan + 64)) / 64;
}

/*
* Write volume data to a channel
*/
void OPLwriteVolume(uint channel, struct OPL2instrument *instr, uint volume)
{
	if (instr != 0)
	{
		OPLwriteChannel(0x40, channel, ((instr->feedback & 1) ?
			OPLconvertVolume(instr->level_1, volume) : instr->level_1) | instr->scale_1,
			OPLconvertVolume(instr->level_2, volume) | instr->scale_2);
	}
}

/*
* Write pan (balance) data to a channel
*/
void OPLwritePan(uint channel, struct OPL2instrument *instr, int pan)
{
	if (instr != 0)
	{
		uchar bits;
		if (pan < -36) bits = 0x10;		// left
		else if (pan > 36) bits = 0x20;	// right
		else bits = 0x30;			// both

		OPLwriteValue(0xC0, channel, instr->feedback | bits);
	}
}

/*
* Write an instrument to a channel
*
* Instrument layout:
*
*   Operator1  Operator2  Descr.
*    data[0]    data[7]   reg. 0x20 - tremolo/vibrato/sustain/KSR/multi
*    data[1]    data[8]   reg. 0x60 - attack rate/decay rate
*    data[2]    data[9]   reg. 0x80 - sustain level/release rate
*    data[3]    data[10]  reg. 0xE0 - waveform select
*    data[4]    data[11]  reg. 0x40 - key scale level
*    data[5]    data[12]  reg. 0x40 - output level (bottom 6 bits only)
*          data[6]        reg. 0xC0 - feedback/AM-FM (both operators)
*/
void OPLwriteInstrument(uint channel, struct OPL2instrument *instr)
{
	OPLwriteChannel(0x40, channel, 0x3F, 0x3F);		// no volume
	OPLwriteChannel(0x20, channel, instr->trem_vibr_1, instr->trem_vibr_2);
	OPLwriteChannel(0x60, channel, instr->att_dec_1,   instr->att_dec_2);
	OPLwriteChannel(0x80, channel, instr->sust_rel_1,  instr->sust_rel_2);
	OPLwriteChannel(0xE0, channel, instr->wave_1,      instr->wave_2);
	OPLwriteValue  (0xC0, channel, instr->feedback | 0x30);
}

/*
* Stop all sounds
*/
void OPLshutup(void)
{
	uint i;

	for(i = 0; i < OPLchannels; i++)
	{
		OPLwriteChannel(0x40, i, 0x3F, 0x3F);	// turn off volume
		OPLwriteChannel(0x60, i, 0xFF, 0xFF);	// the fastest attack, decay
		OPLwriteChannel(0x80, i, 0x0F, 0x0F);	// ... and release
		OPLwriteValue(0xB0, i, 0);		// KEY-OFF
	}
}

/*
* Initialize hardware upon startup
*/
int OPLinit(uint numchips, uint rate)
{
	if (!YM3812Init (numchips, 3579545, rate))
	{
		uint i;

		OPLchannels = OPL2CHANNELS*numchips;
		for (i = 0; i < numchips; ++i)
		{
			OPLwriteReg (i, 0x01, 0x20);	// enable Waveform Select
			OPLwriteReg (i, 0x0B, 0x40);	// turn off CSW mode
			OPLwriteReg (i, 0xBD, 0x00);	// set vibrato/tremolo depth to low, set melodic mode
		}
		OPLshutup();
		return 0;
	}
	else
	{
		return -1;
	}
}

/*
* Deinitialize hardware before shutdown
*/
void OPLdeinit(void)
{
	YM3812Shutdown ();
}
