/*
**
** File: fmopl.c - software implementation of FM sound generator
**                                            types OPL and OPL2
**
** Copyright (C) 1999,2000 Tatsuyuki Satoh , MultiArcadeMachineEmulator development
** Copyright (C) 2002 Jarek Burczynski
**
** Version 0.60
**

[RH] The non-YM3816 and rhythm parts have been ripped out in the interest of trying
to make this a bit faster, since Doom music doesn't need them. And I also made it
render the sound a voice at a time instead of a sample at a time.

Revision History:

04-28-2002 Jarek Burczynski:
 - binary exact Envelope Generator (verified on real YM3812);
   compared to YM2151: the EG clock is equal to internal_clock,
   rates are 2 times slower and volume resolution is one bit less
 - modified interface functions (they no longer return pointer -
   that's internal to the emulator now):
    - new wrapper functions for OPLCreate: YM3526Init(), YM3812Init() and Y8950Init()
 - corrected 'off by one' error in feedback calculations (when feedback is off)
 - enabled waveform usage (credit goes to Vlad Romascanu and zazzal22)
 - speeded up noise generator calculations (Nicola Salmoria)

03-24-2002 Jarek Burczynski (thanks to Dox for the YM3812 chip)
 Complete rewrite (all verified on real YM3812):
 - corrected sin_tab and tl_tab data
 - corrected operator output calculations
 - corrected waveform_select_enable register;
   simply: ignore all writes to waveform_select register when
   waveform_select_enable == 0 and do not change the waveform previously selected.
 - corrected KSR handling
 - corrected Envelope Generator: attack shape, Sustain mode and
   Percussive/Non-percussive modes handling
 - Envelope Generator rates are two times slower now
 - LFO amplitude (tremolo) and phase modulation (vibrato)
 - rhythm sounds phase generation
 - white noise generator (big thanks to Olivier Galibert for mentioning Berlekamp-Massey algorithm)
 - corrected key on/off handling (the 'key' signal is ORed from three sources: FM, rhythm and CSM)
 - funky details (like ignoring output of operator 1 in BD rhythm sound when connect == 1)

12-28-2001 Acho A. Tang
 - reflected Delta-T EOS status on Y8950 status port.
 - fixed subscription range of attack/decay tables


	To do:
		add delay before key off in CSM mode (see CSMKeyControll)
		verify volume of the FM part on the Y8950
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
//#include "driver.h"		/* use M.A.M.E. */
#include "fmopl.h"

#ifndef PI
#define PI 3.14159265358979323846
#endif

#ifdef _MSC_VER
#pragma warning (disable: 4244)
#define INLINE			__forceinline
#endif

#ifdef __GNUC__
#define INLINE			__inline
#endif


#define FREQ_SH			16  /* 16.16 fixed point (frequency calculations) */
#define EG_SH			16  /* 16.16 fixed point (EG timing)              */
#define LFO_SH			24  /*  8.24 fixed point (LFO calculations)       */
#define TIMER_SH		16  /* 16.16 fixed point (timers calculations)    */

#define FREQ_MASK		((1<<FREQ_SH)-1)

/* envelope output entries */
#define ENV_BITS		10
#define ENV_LEN			(1<<ENV_BITS)
#define ENV_STEP		(128.0/ENV_LEN)

#define MAX_ATT_INDEX	((1<<(ENV_BITS-1))-1) /*511*/
#define MIN_ATT_INDEX	(0)

/* sinwave entries */
#define SIN_BITS		10
#define SIN_LEN			(1<<SIN_BITS)
#define SIN_MASK		(SIN_LEN-1)

#define TL_RES_LEN		(256)	/* 8 bits addressing (real chip) */



/* register number to channel number , slot offset */
#define SLOT1 0
#define SLOT2 1

/* Envelope Generator phases */

#define EG_ATT			4
#define EG_DEC			3
#define EG_SUS			2
#define EG_REL			1
#define EG_OFF			0

/* save output as raw 16-bit sample */

/*#define SAVE_SAMPLE*/

#ifdef SAVE_SAMPLE
static FILE *sample[1];
	#if 1	/*save to MONO file */
		#define SAVE_ALL_CHANNELS \
		{	signed int pom = lt; \
			fputc((unsigned short)pom&0xff,sample[0]); \
			fputc(((unsigned short)pom>>8)&0xff,sample[0]); \
		}
	#else	/*save to STEREO file */
		#define SAVE_ALL_CHANNELS \
		{	signed int pom = lt; \
			fputc((unsigned short)pom&0xff,sample[0]); \
			fputc(((unsigned short)pom>>8)&0xff,sample[0]); \
			pom = rt; \
			fputc((unsigned short)pom&0xff,sample[0]); \
			fputc(((unsigned short)pom>>8)&0xff,sample[0]); \
		}
	#endif
#endif

/* #define LOG_CYM_FILE */
#ifdef LOG_CYM_FILE
	FILE * cymfile = NULL;
#endif



#define OPL_TYPE_WAVESEL   0x01  /* waveform select		*/
#define OPL_TYPE_ADPCM     0x02  /* DELTA-T ADPCM unit	*/
#define OPL_TYPE_KEYBOARD  0x04  /* keyboard interface	*/
#define OPL_TYPE_IO        0x08  /* I/O port			*/

/* ---------- Generic interface section ---------- */
#define OPL_TYPE_YM3526 (0)
#define OPL_TYPE_YM3812 (OPL_TYPE_WAVESEL)
#define OPL_TYPE_Y8950  (OPL_TYPE_ADPCM|OPL_TYPE_KEYBOARD|OPL_TYPE_IO)



/* Saving is necessary for member of the 'R' mark for suspend/resume */

typedef struct{
	UINT32	ar;			/* attack rate: AR<<2			*/
	UINT32	dr;			/* decay rate:  DR<<2			*/
	UINT32	rr;			/* release rate:RR<<2			*/
	UINT8	KSR;		/* key scale rate				*/
	UINT8	ksl;		/* keyscale level				*/
	UINT8	ksr;		/* key scale rate: kcode>>KSR	*/
	UINT8	mul;		/* multiple: mul_tab[ML]		*/

	/* Phase Generator */
	UINT32	Cnt;		/* frequency counter			*/
	UINT32	Incr;		/* frequency counter step		*/
	UINT8   FB;			/* feedback shift value			*/
	INT32   *connect1;	/* slot1 output pointer			*/
	INT32   op1_out[2];	/* slot1 output for feedback	*/
	UINT8   CON;		/* connection (algorithm) type	*/

	/* Envelope Generator */
	UINT8	eg_type;	/* percussive/non-percussive mode */
	UINT8	state;		/* phase type					*/
	UINT32	TL;			/* total level: TL << 2			*/
	INT32	TLL;		/* adjusted now TL				*/
	INT32	volume;		/* envelope counter				*/
	UINT32	sl;			/* sustain level: sl_tab[SL]	*/

	UINT8	eg_sh_ar;	/* (attack state)				*/
	UINT8	eg_sel_ar;	/* (attack state)				*/
	UINT8	eg_sh_dr;	/* (decay state)				*/
	UINT8	eg_sel_dr;	/* (decay state)				*/
	UINT8	eg_sh_rr;	/* (release state)				*/
	UINT8	eg_sel_rr;	/* (release state)				*/

	UINT32	key;		/* 0 = KEY OFF, >0 = KEY ON		*/

	/* LFO */
	UINT32	AMmask;		/* LFO Amplitude Modulation enable mask */
	UINT8	vib;		/* LFO Phase Modulation enable flag (active high)*/

	/* waveform select */
	unsigned int wavetable;
} OPL_SLOT;

typedef struct{
	OPL_SLOT SLOT[2];
	/* phase generator state */
	UINT32  block_fnum;	/* block+fnum					*/
	UINT32  fc;			/* Freq. Increment base			*/
	UINT32  ksl_base;	/* KeyScaleLevel Base step		*/
	UINT8   kcode;		/* key code (for key scaling)	*/
} OPL_CH;

/* OPL state */
typedef struct fm_opl_f {
	/* FM channel slots */
	OPL_CH	P_CH[9];				/* OPL/OPL2 chips have 9 channels*/

	UINT32	eg_cnt;					/* global envelope generator counter	*/
	UINT32	eg_timer;				/* global envelope generator counter works at frequency = chipclock/72 */
	UINT32	eg_timer_add;			/* step of eg_timer						*/
	UINT32	eg_timer_overflow;		/* envelope generator timer overflows every 1 sample (on real chip) */

	UINT32	fn_tab[1024];			/* fnumber->increment counter	*/

	/* LFO */
	UINT8	lfo_am_depth;
	UINT8	lfo_pm_depth_range;
	UINT32	lfo_am_cnt;
	UINT32	lfo_am_inc;
	UINT32	lfo_pm_cnt;
	UINT32	lfo_pm_inc;

	UINT8	wavesel;				/* waveform select enable flag	*/

	int		T[2];					/* timer counters				*/
	UINT8	st[2];					/* timer enable					*/

	/* external event callback handlers */
	OPL_TIMERHANDLER  TimerHandler;	/* TIMER handler				*/
	int TimerParam;					/* TIMER parameter				*/
	OPL_IRQHANDLER    IRQHandler;	/* IRQ handler					*/
	int IRQParam;					/* IRQ parameter				*/
	OPL_UPDATEHANDLER UpdateHandler;/* stream update handler		*/
	int UpdateParam;				/* stream update parameter		*/

	UINT8 type;						/* chip type					*/
	UINT8 address;					/* address register				*/
	UINT8 status;					/* status flag					*/
	UINT8 statusmask;				/* status mask					*/
	UINT8 mode;						/* Reg.08 : CSM,notesel,etc.	*/

	int clock;						/* master clock  (Hz)			*/
	int rate;						/* sampling rate (Hz)			*/
	double freqbase;				/* frequency base				*/
	double TimerBase;				/* Timer base time (==sampling time)*/
} FM_OPL;



/* mapping of register number (offset) to slot number used by the emulator */
static const int slot_array[32]=
{
	 0, 2, 4, 1, 3, 5,-1,-1,
	 6, 8,10, 7, 9,11,-1,-1,
	12,14,16,13,15,17,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1
};

/* key scale level */
/* table is 3dB/octave , DV converts this into 6dB/octave */
/* 0.1875 is bit 0 weight of the envelope counter (volume) expressed in the 'decibel' scale */
#define DV (0.1875/2.0)
static const UINT32 ksl_tab[8*16]=
{
	/* OCT 0 */
	 UINT32(0.000/DV), UINT32(0.000/DV), UINT32(0.000/DV), UINT32(0.000/DV),
	 UINT32(0.000/DV), UINT32(0.000/DV), UINT32(0.000/DV), UINT32(0.000/DV),
	 UINT32(0.000/DV), UINT32(0.000/DV), UINT32(0.000/DV), UINT32(0.000/DV),
	 UINT32(0.000/DV), UINT32(0.000/DV), UINT32(0.000/DV), UINT32(0.000/DV),
	/* OCT 1 */
	 UINT32(0.000/DV), UINT32(0.000/DV), UINT32(0.000/DV), UINT32(0.000/DV),
	 UINT32(0.000/DV), UINT32(0.000/DV), UINT32(0.000/DV), UINT32(0.000/DV),
	 UINT32(0.000/DV), UINT32(0.750/DV), UINT32(1.125/DV), UINT32(1.500/DV),
	 UINT32(1.875/DV), UINT32(2.250/DV), UINT32(2.625/DV), UINT32(3.000/DV),
	/* OCT 2 */
	 UINT32(0.000/DV), UINT32(0.000/DV), UINT32(0.000/DV), UINT32(0.000/DV),
	 UINT32(0.000/DV), UINT32(1.125/DV), UINT32(1.875/DV), UINT32(2.625/DV),
	 UINT32(3.000/DV), UINT32(3.750/DV), UINT32(4.125/DV), UINT32(4.500/DV),
	 UINT32(4.875/DV), UINT32(5.250/DV), UINT32(5.625/DV), UINT32(6.000/DV),
	/* OCT 3 */
	 UINT32(0.000/DV), UINT32(0.000/DV), UINT32(0.000/DV), UINT32(1.875/DV),
	 UINT32(3.000/DV), UINT32(4.125/DV), UINT32(4.875/DV), UINT32(5.625/DV),
	 UINT32(6.000/DV), UINT32(6.750/DV), UINT32(7.125/DV), UINT32(7.500/DV),
	 UINT32(7.875/DV), UINT32(8.250/DV), UINT32(8.625/DV), UINT32(9.000/DV),
	/* OCT 4 */
	 UINT32(0.000/DV), UINT32(0.000/DV), UINT32(3.000/DV), UINT32(4.875/DV),
	 UINT32(6.000/DV), UINT32(7.125/DV), UINT32(7.875/DV), UINT32(8.625/DV),
	 UINT32(9.000/DV), UINT32(9.750/DV),UINT32(10.125/DV),UINT32(10.500/DV),
	UINT32(10.875/DV),UINT32(11.250/DV),UINT32(11.625/DV),UINT32(12.000/DV),
	/* OCT 5 */
	 UINT32(0.000/DV), UINT32(3.000/DV), UINT32(6.000/DV), UINT32(7.875/DV),
	 UINT32(9.000/DV),UINT32(10.125/DV),UINT32(10.875/DV),UINT32(11.625/DV),
	UINT32(12.000/DV),UINT32(12.750/DV),UINT32(13.125/DV),UINT32(13.500/DV),
	UINT32(13.875/DV),UINT32(14.250/DV),UINT32(14.625/DV),UINT32(15.000/DV),
	/* OCT 6 */
	 UINT32(0.000/DV), UINT32(6.000/DV), UINT32(9.000/DV),UINT32(10.875/DV),
	UINT32(12.000/DV),UINT32(13.125/DV),UINT32(13.875/DV),UINT32(14.625/DV),
	UINT32(15.000/DV),UINT32(15.750/DV),UINT32(16.125/DV),UINT32(16.500/DV),
	UINT32(16.875/DV),UINT32(17.250/DV),UINT32(17.625/DV),UINT32(18.000/DV),
	/* OCT 7 */
	 UINT32(0.000/DV), UINT32(9.000/DV),UINT32(12.000/DV),UINT32(13.875/DV),
	UINT32(15.000/DV),UINT32(16.125/DV),UINT32(16.875/DV),UINT32(17.625/DV),
	UINT32(18.000/DV),UINT32(18.750/DV),UINT32(19.125/DV),UINT32(19.500/DV),
	UINT32(19.875/DV),UINT32(20.250/DV),UINT32(20.625/DV),UINT32(21.000/DV)
};
#undef DV

/* sustain level table (3dB per step) */
/* 0 - 15: 0, 3, 6, 9,12,15,18,21,24,27,30,33,36,39,42,93 (dB)*/
#define SC(db) (UINT32) ( db * (2.0/ENV_STEP) )
static const UINT32 sl_tab[16]={
 SC( 0),SC( 1),SC( 2),SC(3 ),SC(4 ),SC(5 ),SC(6 ),SC( 7),
 SC( 8),SC( 9),SC(10),SC(11),SC(12),SC(13),SC(14),SC(31)
};
#undef SC


#define RATE_STEPS (8)
static const unsigned char eg_inc[15*RATE_STEPS]={

/*cycle:0 1  2 3  4 5  6 7*/

/* 0 */ 0,1, 0,1, 0,1, 0,1, /* rates 00..12 0 (increment by 0 or 1) */
/* 1 */ 0,1, 0,1, 1,1, 0,1, /* rates 00..12 1 */
/* 2 */ 0,1, 1,1, 0,1, 1,1, /* rates 00..12 2 */
/* 3 */ 0,1, 1,1, 1,1, 1,1, /* rates 00..12 3 */

/* 4 */ 1,1, 1,1, 1,1, 1,1, /* rate 13 0 (increment by 1) */
/* 5 */ 1,1, 1,2, 1,1, 1,2, /* rate 13 1 */
/* 6 */ 1,2, 1,2, 1,2, 1,2, /* rate 13 2 */
/* 7 */ 1,2, 2,2, 1,2, 2,2, /* rate 13 3 */

/* 8 */ 2,2, 2,2, 2,2, 2,2, /* rate 14 0 (increment by 2) */
/* 9 */ 2,2, 2,4, 2,2, 2,4, /* rate 14 1 */
/*10 */ 2,4, 2,4, 2,4, 2,4, /* rate 14 2 */
/*11 */ 2,4, 4,4, 2,4, 4,4, /* rate 14 3 */

/*12 */ 4,4, 4,4, 4,4, 4,4, /* rates 15 0, 15 1, 15 2, 15 3 (increment by 4) */
/*13 */ 8,8, 8,8, 8,8, 8,8, /* rates 15 2, 15 3 for attack */
/*14 */ 0,0, 0,0, 0,0, 0,0, /* infinity rates for attack and decay(s) */
};


#define O(a) (a*RATE_STEPS)

/*note that there is no O(13) in this table - it's directly in the code */
static const unsigned char eg_rate_select[16+64+16]={	/* Envelope Generator rates (16 + 64 rates + 16 RKS) */
/* 16 dummy (infinite time) rates */
O(14),O(14),O(14),O(14),O(14),O(14),O(14),O(14),
O(14),O(14),O(14),O(14),O(14),O(14),O(14),O(14),

/* rates 00-12 */
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),

/* rate 13 */
O( 4),O( 5),O( 6),O( 7),

/* rate 14 */
O( 8),O( 9),O(10),O(11),

/* rate 15 */
O(12),O(12),O(12),O(12),

/* 16 dummy rates (same as 15 3) */
O(12),O(12),O(12),O(12),O(12),O(12),O(12),O(12),
O(12),O(12),O(12),O(12),O(12),O(12),O(12),O(12),

};
#undef O

//rate  0,    1,    2,    3,   4,   5,   6,  7,  8,  9,  10, 11, 12, 13, 14, 15
//shift 12,   11,   10,   9,   8,   7,   6,  5,  4,  3,  2,  1,  0,  0,  0,  0
//mask  4095, 2047, 1023, 511, 255, 127, 63, 31, 15, 7,  3,  1,  0,  0,  0,  0

#define O(a) (a*1)
static const unsigned char eg_rate_shift[16+64+16]={	/* Envelope Generator counter shifts (16 + 64 rates + 16 RKS) */
/* 16 infinite time rates */
O(0),O(0),O(0),O(0),O(0),O(0),O(0),O(0),
O(0),O(0),O(0),O(0),O(0),O(0),O(0),O(0),

/* rates 00-12 */
O(12),O(12),O(12),O(12),
O(11),O(11),O(11),O(11),
O(10),O(10),O(10),O(10),
O( 9),O( 9),O( 9),O( 9),
O( 8),O( 8),O( 8),O( 8),
O( 7),O( 7),O( 7),O( 7),
O( 6),O( 6),O( 6),O( 6),
O( 5),O( 5),O( 5),O( 5),
O( 4),O( 4),O( 4),O( 4),
O( 3),O( 3),O( 3),O( 3),
O( 2),O( 2),O( 2),O( 2),
O( 1),O( 1),O( 1),O( 1),
O( 0),O( 0),O( 0),O( 0),

/* rate 13 */
O( 0),O( 0),O( 0),O( 0),

/* rate 14 */
O( 0),O( 0),O( 0),O( 0),

/* rate 15 */
O( 0),O( 0),O( 0),O( 0),

/* 16 dummy rates (same as 15 3) */
O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),
O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),

};
#undef O


/* multiple table */
#define ML 2
static const UINT8 mul_tab[16]= {
/* 1/2, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,10,12,12,15,15 */
   UINT8(0.50*ML), UINT8(1.00*ML), UINT8(2.00*ML), UINT8(3.00*ML), UINT8(4.00*ML), UINT8(5.00*ML), UINT8(6.00*ML), UINT8(7.00*ML),
   UINT8(8.00*ML), UINT8(9.00*ML),UINT8(10.00*ML),UINT8(10.00*ML),UINT8(12.00*ML),UINT8(12.00*ML),UINT8(15.00*ML),UINT8(15.00*ML)
};
#undef ML

/*	TL_TAB_LEN is calculated as:
*	12 - sinus amplitude bits     (Y axis)
*	2  - sinus sign bit           (Y axis)
*	TL_RES_LEN - sinus resolution (X axis)
*/
#define TL_TAB_LEN (12*2*TL_RES_LEN)
static signed int tl_tab[TL_TAB_LEN];

#define ENV_QUIET		(TL_TAB_LEN>>4)

/* sin waveform table in 'decibel' scale */
/* four waveforms on OPL2 type chips */
static unsigned int sin_tab[SIN_LEN * 4];


/* LFO Amplitude Modulation table (verified on real YM3812)
   27 output levels (triangle waveform); 1 level takes one of: 192, 256 or 448 samples

   Length: 210 elements.

	Each of the elements has to be repeated
	exactly 64 times (on 64 consecutive samples).
	The whole table takes: 64 * 210 = 13440 samples.

	When AM = 1 data is used directly
	When AM = 0 data is divided by 4 before being used (loosing precision is important)
*/

#define LFO_AM_TAB_ELEMENTS 210

static const UINT8 lfo_am_table[LFO_AM_TAB_ELEMENTS] = {
0,0,0,0,0,0,0,
1,1,1,1,
2,2,2,2,
3,3,3,3,
4,4,4,4,
5,5,5,5,
6,6,6,6,
7,7,7,7,
8,8,8,8,
9,9,9,9,
10,10,10,10,
11,11,11,11,
12,12,12,12,
13,13,13,13,
14,14,14,14,
15,15,15,15,
16,16,16,16,
17,17,17,17,
18,18,18,18,
19,19,19,19,
20,20,20,20,
21,21,21,21,
22,22,22,22,
23,23,23,23,
24,24,24,24,
25,25,25,25,
26,26,26,
25,25,25,25,
24,24,24,24,
23,23,23,23,
22,22,22,22,
21,21,21,21,
20,20,20,20,
19,19,19,19,
18,18,18,18,
17,17,17,17,
16,16,16,16,
15,15,15,15,
14,14,14,14,
13,13,13,13,
12,12,12,12,
11,11,11,11,
10,10,10,10,
9,9,9,9,
8,8,8,8,
7,7,7,7,
6,6,6,6,
5,5,5,5,
4,4,4,4,
3,3,3,3,
2,2,2,2,
1,1,1,1
};

/* LFO Phase Modulation table (verified on real YM3812) */
static const INT8 lfo_pm_table[8*8*2] = {

/* FNUM2/FNUM = 00 0xxxxxxx (0x0000) */
0, 0, 0, 0, 0, 0, 0, 0,	/*LFO PM depth = 0*/
0, 0, 0, 0, 0, 0, 0, 0,	/*LFO PM depth = 1*/

/* FNUM2/FNUM = 00 1xxxxxxx (0x0080) */
0, 0, 0, 0, 0, 0, 0, 0,	/*LFO PM depth = 0*/
1, 0, 0, 0,-1, 0, 0, 0,	/*LFO PM depth = 1*/

/* FNUM2/FNUM = 01 0xxxxxxx (0x0100) */
1, 0, 0, 0,-1, 0, 0, 0,	/*LFO PM depth = 0*/
2, 1, 0,-1,-2,-1, 0, 1,	/*LFO PM depth = 1*/

/* FNUM2/FNUM = 01 1xxxxxxx (0x0180) */
1, 0, 0, 0,-1, 0, 0, 0,	/*LFO PM depth = 0*/
3, 1, 0,-1,-3,-1, 0, 1,	/*LFO PM depth = 1*/

/* FNUM2/FNUM = 10 0xxxxxxx (0x0200) */
2, 1, 0,-1,-2,-1, 0, 1,	/*LFO PM depth = 0*/
4, 2, 0,-2,-4,-2, 0, 2,	/*LFO PM depth = 1*/

/* FNUM2/FNUM = 10 1xxxxxxx (0x0280) */
2, 1, 0,-1,-2,-1, 0, 1,	/*LFO PM depth = 0*/
5, 2, 0,-2,-5,-2, 0, 2,	/*LFO PM depth = 1*/

/* FNUM2/FNUM = 11 0xxxxxxx (0x0300) */
3, 1, 0,-1,-3,-1, 0, 1,	/*LFO PM depth = 0*/
6, 3, 0,-3,-6,-3, 0, 3,	/*LFO PM depth = 1*/

/* FNUM2/FNUM = 11 1xxxxxxx (0x0380) */
3, 1, 0,-1,-3,-1, 0, 1,	/*LFO PM depth = 0*/
7, 3, 0,-3,-7,-3, 0, 3	/*LFO PM depth = 1*/
};


/* lock level of common table */
static int num_lock = 0;

/* work table */
static void *cur_chip = NULL;	/* current chip point */

static signed int phase_modulation;		/* phase modulation input (SLOT 2) */
static signed int output[1];

static UINT32	LFO_AM;
static INT32	LFO_PM;

static bool CalcVoice (FM_OPL *OPL, int voice, float *buffer, int length);



/* status set and IRQ handling */
INLINE void OPL_STATUS_SET(FM_OPL *OPL,int flag)
{
	/* set status flag */
	OPL->status |= flag;
	if(!(OPL->status & 0x80))
	{
		if(OPL->status & OPL->statusmask)
		{	/* IRQ on */
			OPL->status |= 0x80;
			/* callback user interrupt handler (IRQ is OFF to ON) */
			if(OPL->IRQHandler) (OPL->IRQHandler)(OPL->IRQParam,1);
		}
	}
}

/* status reset and IRQ handling */
INLINE void OPL_STATUS_RESET(FM_OPL *OPL,int flag)
{
	/* reset status flag */
	OPL->status &=~flag;
	if((OPL->status & 0x80))
	{
		if (!(OPL->status & OPL->statusmask) )
		{
			OPL->status &= 0x7f;
			/* callback user interrupt handler (IRQ is ON to OFF) */
			if(OPL->IRQHandler) (OPL->IRQHandler)(OPL->IRQParam,0);
		}
	}
}

/* IRQ mask set */
INLINE void OPL_STATUSMASK_SET(FM_OPL *OPL,int flag)
{
	OPL->statusmask = flag;
	/* IRQ handling check */
	OPL_STATUS_SET(OPL,0);
	OPL_STATUS_RESET(OPL,0);
}


/* advance LFO to next sample */
INLINE void advance_lfo(FM_OPL *OPL)
{
	UINT8 tmp;

	/* LFO */
	OPL->lfo_am_cnt += OPL->lfo_am_inc;
	if (OPL->lfo_am_cnt >= (UINT32)(LFO_AM_TAB_ELEMENTS<<LFO_SH) )	/* lfo_am_table is 210 elements long */
		OPL->lfo_am_cnt -= (LFO_AM_TAB_ELEMENTS<<LFO_SH);

	tmp = lfo_am_table[ OPL->lfo_am_cnt >> LFO_SH ];

	if (OPL->lfo_am_depth)
		LFO_AM = tmp;
	else
		LFO_AM = tmp>>2;

	OPL->lfo_pm_cnt += OPL->lfo_pm_inc;
	LFO_PM = ((OPL->lfo_pm_cnt>>LFO_SH) & 7) | OPL->lfo_pm_depth_range;
}

/* advance to next sample */
INLINE void advance(FM_OPL *OPL)
{
	OPL_CH *CH;
	OPL_SLOT *op;
	int i;

	OPL->eg_timer += OPL->eg_timer_add;

	while (OPL->eg_timer >= OPL->eg_timer_overflow)
	{
		OPL->eg_timer -= OPL->eg_timer_overflow;

		OPL->eg_cnt++;

		for (i=0; i<9*2; i++)
		{
			CH  = &OPL->P_CH[i/2];
			op  = &CH->SLOT[i&1];

			/* Envelope Generator */
			switch(op->state)
			{
			case EG_ATT:		/* attack phase */
			{

				if ( !(OPL->eg_cnt & ((1<<op->eg_sh_ar)-1) ) )
				{
					op->volume += (~op->volume *
	                        		           (eg_inc[op->eg_sel_ar + ((OPL->eg_cnt>>op->eg_sh_ar)&7)])
        			                          ) >>3;

					if (op->volume <= MIN_ATT_INDEX)
					{
						op->volume = MIN_ATT_INDEX;
						op->state = EG_DEC;
					}

				}

			}
			break;

			case EG_DEC:	/* decay phase */
				if ( !(OPL->eg_cnt & ((1<<op->eg_sh_dr)-1) ) )
				{
					op->volume += eg_inc[op->eg_sel_dr + ((OPL->eg_cnt>>op->eg_sh_dr)&7)];

					if ( op->volume >= (INT32)op->sl )
						op->state = EG_SUS;

				}
			break;

			case EG_SUS:	/* sustain phase */

				/* this is important behaviour:
				one can change percusive/non-percussive modes on the fly and
				the chip will remain in sustain phase - verified on real YM3812 */

				if(op->eg_type)		/* non-percussive mode */
				{
									/* do nothing */
				}
				else				/* percussive mode */
				{
					/* during sustain phase chip adds Release Rate (in percussive mode) */
					if ( !(OPL->eg_cnt & ((1<<op->eg_sh_rr)-1) ) )
					{
						op->volume += eg_inc[op->eg_sel_rr + ((OPL->eg_cnt>>op->eg_sh_rr)&7)];

						if ( op->volume >= MAX_ATT_INDEX )
							op->volume = MAX_ATT_INDEX;
					}
					/* else do nothing in sustain phase */
				}
			break;

			case EG_REL:	/* release phase */
				if ( !(OPL->eg_cnt & ((1<<op->eg_sh_rr)-1) ) )
				{
					op->volume += eg_inc[op->eg_sel_rr + ((OPL->eg_cnt>>op->eg_sh_rr)&7)];

					if ( op->volume >= MAX_ATT_INDEX )
					{
						op->volume = MAX_ATT_INDEX;
						op->state = EG_OFF;
					}

				}
			break;

			default:
			break;
			}
		}
	}

	for (i=0; i<9*2; i++)
	{
		CH  = &OPL->P_CH[i/2];
		op  = &CH->SLOT[i&1];

		/* Phase Generator */
		if(op->vib)
		{
			UINT8 block;
			unsigned int block_fnum = CH->block_fnum;

			unsigned int fnum_lfo   = (block_fnum&0x0380) >> 7;

			signed int lfo_fn_table_index_offset = lfo_pm_table[LFO_PM + 16*fnum_lfo ];

			if (lfo_fn_table_index_offset)	/* LFO phase modulation active */
			{
				block_fnum += lfo_fn_table_index_offset;
				block = (block_fnum&0x1c00) >> 10;
				op->Cnt += (OPL->fn_tab[block_fnum&0x03ff] >> (7-block)) * op->mul;//ok
			}
			else	/* LFO phase modulation  = zero */
			{
				op->Cnt += op->Incr;
			}
		}
		else	/* LFO phase modulation disabled for this operator */
		{
			op->Cnt += op->Incr;
		}
	}
}


#define volume_calc(OP) ((OP)->TLL + ((UINT32)(OP)->volume) + (LFO_AM & (OP)->AMmask))

/* generic table initialize */
static int init_tables(void)
{
	signed int i,x;
	signed int n;
	double o,m;


	for (x=0; x<TL_RES_LEN; x++)
	{
		m = (1<<16) / pow(2.0, (x+1) * (ENV_STEP/4.0) / 8.0);
		m = floor(m);

		/* we never reach (1<<16) here due to the (x+1) */
		/* result fits within 16 bits at maximum */

		n = (int)m;		/* 16 bits here */
		n >>= 4;		/* 12 bits here */
		n = (n+1)>>1;	/* round to nearest */
						/* 11 bits here (rounded) */
		n <<= 1;		/* 12 bits here (as in real chip) */
		tl_tab[ x*2 + 0 ] = n;
		tl_tab[ x*2 + 1 ] = -tl_tab[ x*2 + 0 ];

		for (i=1; i<12; i++)
		{
			tl_tab[ x*2+0 + i*2*TL_RES_LEN ] =  tl_tab[ x*2+0 ]>>i;
			tl_tab[ x*2+1 + i*2*TL_RES_LEN ] = -tl_tab[ x*2+0 ]>>i;
		}
	#if 0
			logerror("tl %04i", x*2);
			for (i=0; i<12; i++)
				logerror(", [%02i] %5i", i*2, tl_tab[ x*2 /*+1*/ + i*2*TL_RES_LEN ] );
			logerror("\n");
	#endif
	}
	/*logerror("FMOPL.C: TL_TAB_LEN = %i elements (%i bytes)\n",TL_TAB_LEN, (int)sizeof(tl_tab));*/


	for (i=0; i<SIN_LEN; i++)
	{
		/* non-standard sinus */
		m = sin( ((i*2)+1) * PI / SIN_LEN ); /* checked against the real chip */

		/* we never reach zero here due to ((i*2)+1) */

		if (m>0.0)
			o = 8*log(1.0/m)/log(2.0);	/* convert to 'decibels' */
		else
			o = 8*log(-1.0/m)/log(2.0);	/* convert to 'decibels' */

		o = o / (ENV_STEP/4);

		n = (int)(2.0*o);
		if (n&1)						/* round to nearest */
			n = (n>>1)+1;
		else
			n = n>>1;

		sin_tab[ i ] = n*2 + (m>=0.0? 0: 1 );

		/*logerror("FMOPL.C: sin [%4i (hex=%03x)]= %4i (tl_tab value=%5i)\n", i, i, sin_tab[i], tl_tab[sin_tab[i]] );*/
	}

	for (i=0; i<SIN_LEN; i++)
	{
		/* waveform 1:  __      __     */
		/*             /  \____/  \____*/
		/* output only first half of the sinus waveform (positive one) */

		if (i & (1<<(SIN_BITS-1)) )
			sin_tab[1*SIN_LEN+i] = TL_TAB_LEN;
		else
			sin_tab[1*SIN_LEN+i] = sin_tab[i];

		/* waveform 2:  __  __  __  __ */
		/*             /  \/  \/  \/  \*/
		/* abs(sin) */

		sin_tab[2*SIN_LEN+i] = sin_tab[i & (SIN_MASK>>1) ];

		/* waveform 3:  _   _   _   _  */
		/*             / |_/ |_/ |_/ |_*/
		/* abs(output only first quarter of the sinus waveform) */

		if (i & (1<<(SIN_BITS-2)) )
			sin_tab[3*SIN_LEN+i] = TL_TAB_LEN;
		else
			sin_tab[3*SIN_LEN+i] = sin_tab[i & (SIN_MASK>>2)];

		/*logerror("FMOPL.C: sin1[%4i]= %4i (tl_tab value=%5i)\n", i, sin_tab[1*SIN_LEN+i], tl_tab[sin_tab[1*SIN_LEN+i]] );
		logerror("FMOPL.C: sin2[%4i]= %4i (tl_tab value=%5i)\n", i, sin_tab[2*SIN_LEN+i], tl_tab[sin_tab[2*SIN_LEN+i]] );
		logerror("FMOPL.C: sin3[%4i]= %4i (tl_tab value=%5i)\n", i, sin_tab[3*SIN_LEN+i], tl_tab[sin_tab[3*SIN_LEN+i]] );*/
	}
	/*logerror("FMOPL.C: ENV_QUIET= %08x (dec*8=%i)\n", ENV_QUIET, ENV_QUIET*8 );*/


#ifdef SAVE_SAMPLE
	sample[0]=fopen("sampsum.pcm","wb");
#endif

	return 1;
}

static void OPLCloseTable( void )
{
#ifdef SAVE_SAMPLE
	fclose(sample[0]);
#endif
}



static void OPL_initalize(FM_OPL *OPL)
{
	int i;

	/* frequency base */
#if 1
	OPL->freqbase  = (OPL->rate) ? ((double)OPL->clock / 72.0) / OPL->rate  : 0;
#else
	OPL->rate = (double)OPL->clock / 72.0;
	OPL->freqbase  = 1.0;
#endif

	/* Timer base time */
	OPL->TimerBase = 1.0 / ((double)OPL->clock / 72.0 );

	/* make fnumber -> increment counter table */
	for( i=0 ; i < 1024 ; i++ )
	{
		/* opn phase increment counter = 20bit */
		OPL->fn_tab[i] = (UINT32)( (double)i * 64 * OPL->freqbase * (1<<(FREQ_SH-10)) ); /* -10 because chip works with 10.10 fixed point, while we use 16.16 */
#if 0
		logerror("FMOPL.C: fn_tab[%4i] = %08x (dec=%8i)\n",
				 i, OPL->fn_tab[i]>>6, OPL->fn_tab[i]>>6 );
#endif
	}

#if 0
	for( i=0 ; i < 16 ; i++ )
	{
		logerror("FMOPL.C: sl_tab[%i] = %08x\n",
			i, sl_tab[i] );
	}
	for( i=0 ; i < 8 ; i++ )
	{
		int j;
		logerror("FMOPL.C: ksl_tab[oct=%2i] =",i);
		for (j=0; j<16; j++)
		{
			logerror("%08x ", ksl_tab[i*16+j] );
		}
		logerror("\n");
	}
#endif


	/* Amplitude modulation: 27 output levels (triangle waveform); 1 level takes one of: 192, 256 or 448 samples */
	/* One entry from LFO_AM_TABLE lasts for 64 samples */
	OPL->lfo_am_inc = UINT32((1.0 / 64.0 ) * (1<<LFO_SH) * OPL->freqbase);

	/* Vibrato: 8 output levels (triangle waveform); 1 level takes 1024 samples */
	OPL->lfo_pm_inc = UINT32((1.0 / 1024.0) * (1<<LFO_SH) * OPL->freqbase);

	/*logerror ("OPL->lfo_am_inc = %8x ; OPL->lfo_pm_inc = %8x\n", OPL->lfo_am_inc, OPL->lfo_pm_inc);*/

	OPL->eg_timer_add  = UINT32((1<<EG_SH)  * OPL->freqbase);
	OPL->eg_timer_overflow = UINT32(( 1 ) * (1<<EG_SH));
	/*logerror("OPLinit eg_timer_add=%8x eg_timer_overflow=%8x\n", OPL->eg_timer_add, OPL->eg_timer_overflow);*/

}

INLINE void FM_KEYON(OPL_SLOT *SLOT, UINT32 key_set)
{
	if( !SLOT->key )
	{
		/* restart Phase Generator */
		SLOT->Cnt = 0;
		/* phase -> Attack */
		SLOT->state = EG_ATT;
	}
	SLOT->key |= key_set;
}

INLINE void FM_KEYOFF(OPL_SLOT *SLOT, UINT32 key_clr)
{
	if( SLOT->key )
	{
		SLOT->key &= key_clr;

		if( !SLOT->key )
		{
			/* phase -> Release */
			if (SLOT->state>EG_REL)
				SLOT->state = EG_REL;
		}
	}
}

/* update phase increment counter of operator (also update the EG rates if necessary) */
void CALC_FCSLOT(OPL_CH *CH,OPL_SLOT *SLOT)
{
	int ksr;

	/* (frequency) phase increment counter */
	SLOT->Incr = CH->fc * SLOT->mul;
	ksr = CH->kcode >> SLOT->KSR;

	if( SLOT->ksr != ksr )
	{
		SLOT->ksr = ksr;

		/* calculate envelope generator rates */
		if ((SLOT->ar + SLOT->ksr) < 16+62)
		{
			SLOT->eg_sh_ar  = eg_rate_shift [SLOT->ar + SLOT->ksr ];
			SLOT->eg_sel_ar = eg_rate_select[SLOT->ar + SLOT->ksr ];
		}
		else
		{
			SLOT->eg_sh_ar  = 0;
			SLOT->eg_sel_ar = 13*RATE_STEPS;
		}
		SLOT->eg_sh_dr  = eg_rate_shift [SLOT->dr + SLOT->ksr ];
		SLOT->eg_sel_dr = eg_rate_select[SLOT->dr + SLOT->ksr ];
		SLOT->eg_sh_rr  = eg_rate_shift [SLOT->rr + SLOT->ksr ];
		SLOT->eg_sel_rr = eg_rate_select[SLOT->rr + SLOT->ksr ];
	}
}

/* set multi,am,vib,EG-TYP,KSR,mul */
void set_mul(FM_OPL *OPL,int slot,int v)
{
	OPL_CH   *CH   = &OPL->P_CH[slot/2];
	OPL_SLOT *SLOT = &CH->SLOT[slot&1];

	SLOT->mul     = mul_tab[v&0x0f];
	SLOT->KSR     = (v&0x10) ? 0 : 2;
	SLOT->eg_type = (v&0x20);
	SLOT->vib     = (v&0x40);
	SLOT->AMmask  = (v&0x80) ? ~0 : 0;
	CALC_FCSLOT(CH,SLOT);
}

/* set ksl & tl */
void set_ksl_tl(FM_OPL *OPL,int slot,int v)
{
	OPL_CH   *CH   = &OPL->P_CH[slot/2];
	OPL_SLOT *SLOT = &CH->SLOT[slot&1];
	int ksl = v>>6; /* 0 / 1.5 / 3.0 / 6.0 dB/OCT */

	SLOT->ksl = ksl ? 3-ksl : 31;
	SLOT->TL  = (v&0x3f)<<(ENV_BITS-1-7); /* 7 bits TL (bit 6 = always 0) */

	SLOT->TLL = SLOT->TL + (CH->ksl_base>>SLOT->ksl);
}

/* set attack rate & decay rate  */
INLINE void set_ar_dr(FM_OPL *OPL,int slot,int v)
{
	OPL_CH   *CH   = &OPL->P_CH[slot/2];
	OPL_SLOT *SLOT = &CH->SLOT[slot&1];

	SLOT->ar = (v>>4)  ? 16 + ((v>>4)  <<2) : 0;

	if ((SLOT->ar + SLOT->ksr) < 16+62)
	{
		SLOT->eg_sh_ar  = eg_rate_shift [SLOT->ar + SLOT->ksr ];
		SLOT->eg_sel_ar = eg_rate_select[SLOT->ar + SLOT->ksr ];
	}
	else
	{
		SLOT->eg_sh_ar  = 0;
		SLOT->eg_sel_ar = 13*RATE_STEPS;
	}

	SLOT->dr    = (v&0x0f)? 16 + ((v&0x0f)<<2) : 0;
	SLOT->eg_sh_dr  = eg_rate_shift [SLOT->dr + SLOT->ksr ];
	SLOT->eg_sel_dr = eg_rate_select[SLOT->dr + SLOT->ksr ];
}

/* set sustain level & release rate */
void set_sl_rr(FM_OPL *OPL,int slot,int v)
{
	OPL_CH   *CH   = &OPL->P_CH[slot/2];
	OPL_SLOT *SLOT = &CH->SLOT[slot&1];

	SLOT->sl  = sl_tab[ v>>4 ];

	SLOT->rr  = (v&0x0f)? 16 + ((v&0x0f)<<2) : 0;
	SLOT->eg_sh_rr  = eg_rate_shift [SLOT->rr + SLOT->ksr ];
	SLOT->eg_sel_rr = eg_rate_select[SLOT->rr + SLOT->ksr ];
}


/* write a value v to register r on OPL chip */
static void OPLWriteReg(FM_OPL *OPL, int r, int v)
{
	OPL_CH *CH;
	int slot;
	int block_fnum;


	/* adjust bus to 8 bits */
	r &= 0xff;
	v &= 0xff;

#ifdef LOG_CYM_FILE
	if ((cymfile) && (r!=0) )
	{
		fputc( (unsigned char)r, cymfile );
		fputc( (unsigned char)v, cymfile );
	}
#endif


	switch(r&0xe0)
	{
	case 0x00:	/* 00-1f:control */
		switch(r&0x1f)
		{
		case 0x01:	/* waveform select enable */
			if(OPL->type&OPL_TYPE_WAVESEL)
			{
				OPL->wavesel = v&0x20;
				/* do not change the waveform previously selected */
			}
			break;
		case 0x02:	/* Timer 1 */
			OPL->T[0] = (256-v)*4;
			break;
		case 0x03:	/* Timer 2 */
			OPL->T[1] = (256-v)*16;
			break;
		case 0x04:	/* IRQ clear / mask and Timer enable */
			if(v&0x80)
			{	/* IRQ flag clear */
				OPL_STATUS_RESET(OPL,0x7f);
			}
			else
			{	/* set IRQ mask ,timer enable*/
				UINT8 st1 = v&1;
				UINT8 st2 = (v>>1)&1;
				/* IRQRST,T1MSK,t2MSK,EOSMSK,BRMSK,x,ST2,ST1 */
				OPL_STATUS_RESET(OPL,v&0x78);
				OPL_STATUSMASK_SET(OPL,((~v)&0x78)|0x01);
				/* timer 2 */
				if(OPL->st[1] != st2)
				{
					double interval = st2 ? (double)OPL->T[1]*OPL->TimerBase : 0.0;
					OPL->st[1] = st2;
					if (OPL->TimerHandler) (OPL->TimerHandler)(OPL->TimerParam+1,interval);
				}
				/* timer 1 */
				if(OPL->st[0] != st1)
				{
					double interval = st1 ? (double)OPL->T[0]*OPL->TimerBase : 0.0;
					OPL->st[0] = st1;
					if (OPL->TimerHandler) (OPL->TimerHandler)(OPL->TimerParam+0,interval);
				}
			}
			break;
		}
		break;
	case 0x20:	/* am ON, vib ON, ksr, eg_type, mul */
		slot = slot_array[r&0x1f];
		if(slot < 0) return;
		set_mul(OPL,slot,v);
		break;
	case 0x40:
		slot = slot_array[r&0x1f];
		if(slot < 0) return;
		set_ksl_tl(OPL,slot,v);
		break;
	case 0x60:
		slot = slot_array[r&0x1f];
		if(slot < 0) return;
		set_ar_dr(OPL,slot,v);
		break;
	case 0x80:
		slot = slot_array[r&0x1f];
		if(slot < 0) return;
		set_sl_rr(OPL,slot,v);
		break;
	case 0xa0:
		if (r == 0xbd)			/* am depth, vibrato depth, r,bd,sd,tom,tc,hh */
		{
			OPL->lfo_am_depth = v & 0x80;
			OPL->lfo_pm_depth_range = (v&0x40) ? 8 : 0;
			return;
		}
		/* keyon,block,fnum */
		if( (r&0x0f) > 8) return;
		CH = &OPL->P_CH[r&0x0f];
		if(!(r&0x10))
		{	/* a0-a8 */
			block_fnum  = (CH->block_fnum&0x1f00) | v;
		}
		else
		{	/* b0-b8 */
			block_fnum = ((v&0x1f)<<8) | (CH->block_fnum&0xff);

			if(v&0x20)
			{
				FM_KEYON (&CH->SLOT[SLOT1], 1);
				FM_KEYON (&CH->SLOT[SLOT2], 1);
			}
			else
			{
				FM_KEYOFF(&CH->SLOT[SLOT1],~1);
				FM_KEYOFF(&CH->SLOT[SLOT2],~1);
			}
		}
		/* update */
		if(CH->block_fnum != (UINT32)block_fnum)
		{
			UINT8 block  = block_fnum >> 10;

			CH->block_fnum = block_fnum;

			CH->ksl_base = ksl_tab[block_fnum>>6];
			CH->fc       = OPL->fn_tab[block_fnum&0x03ff] >> (7-block);

			/* BLK 2,1,0 bits -> bits 3,2,1 of kcode */
			CH->kcode    = (CH->block_fnum&0x1c00)>>9;

			 /* the info below is actually opposite to what is stated in the Manuals (verifed on real YM3812) */
			/* if notesel == 0 -> lsb of kcode is bit 10 (MSB) of fnum  */
			/* if notesel == 1 -> lsb of kcode is bit 9 (MSB-1) of fnum */
			if (OPL->mode&0x40)
				CH->kcode |= (CH->block_fnum&0x100)>>8;	/* notesel == 1 */
			else
				CH->kcode |= (CH->block_fnum&0x200)>>9;	/* notesel == 0 */

			/* refresh Total Level in both SLOTs of this channel */
			CH->SLOT[SLOT1].TLL = CH->SLOT[SLOT1].TL + (CH->ksl_base>>CH->SLOT[SLOT1].ksl);
			CH->SLOT[SLOT2].TLL = CH->SLOT[SLOT2].TL + (CH->ksl_base>>CH->SLOT[SLOT2].ksl);

			/* refresh frequency counter in both SLOTs of this channel */
			CALC_FCSLOT(CH,&CH->SLOT[SLOT1]);
			CALC_FCSLOT(CH,&CH->SLOT[SLOT2]);
		}
		break;
	case 0xc0:
		/* FB,C */
		if( (r&0x0f) > 8) return;
		CH = &OPL->P_CH[r&0x0f];
		CH->SLOT[SLOT1].FB  = (v>>1)&7 ? ((v>>1)&7) + 7 : 0;
		CH->SLOT[SLOT1].CON = v&1;
		CH->SLOT[SLOT1].connect1 = CH->SLOT[SLOT1].CON ? &output[0] : &phase_modulation;
		break;
	case 0xe0: /* waveform select */
		/* simply ignore write to the waveform select register if selecting not enabled in test register */
		if(OPL->wavesel)
		{
			slot = slot_array[r&0x1f];
			if(slot < 0) return;
			CH = &OPL->P_CH[slot/2];

			CH->SLOT[slot&1].wavetable = (v&0x03)*SIN_LEN;
		}
		break;
	}
}

#ifdef LOG_CYM_FILE
static void cymfile_callback (int n)
{
	if (cymfile)
	{
		fputc( (unsigned char)0, cymfile );
	}
}
#endif

/* lock/unlock for common table */
static int OPL_LockTable(void)
{
	num_lock++;
	if(num_lock>1) return 0;

	/* first time */

	cur_chip = NULL;
	/* allocate total level table (128kb space) */
	if( !init_tables() )
	{
		num_lock--;
		return -1;
	}

#ifdef LOG_CYM_FILE
	cymfile = fopen("3812_.cym","wb");
	if (cymfile)
		timer_pulse ( TIME_IN_HZ(110), 0, cymfile_callback); /*110 Hz pulse timer*/
	else
		logerror("Could not create file 3812_.cym\n");
#endif

	return 0;
}

static void OPL_UnLockTable(void)
{
	if(num_lock) num_lock--;
	if(num_lock) return;

	/* last time */

	cur_chip = NULL;
	OPLCloseTable();

#ifdef LOG_CYM_FILE
	fclose (cymfile);
	cymfile = NULL;
#endif

}

static void OPLResetChip(FM_OPL *OPL)
{
	int c,s;
	int i;

	OPL->eg_timer = 0;
	OPL->eg_cnt   = 0;

	OPL->mode   = 0;	/* normal mode */
	OPL_STATUS_RESET(OPL,0x7f);

	/* reset with register write */
	OPLWriteReg(OPL,0x01,0); /* wavesel disable */
	OPLWriteReg(OPL,0x02,0); /* Timer1 */
	OPLWriteReg(OPL,0x03,0); /* Timer2 */
	OPLWriteReg(OPL,0x04,0); /* IRQ mask clear */
	for(i = 0xff ; i >= 0x20 ; i-- ) OPLWriteReg(OPL,i,0);

	/* reset operator parameters */
	for( c = 0 ; c < 9 ; c++ )
	{
		OPL_CH *CH = &OPL->P_CH[c];
		for(s = 0 ; s < 2 ; s++ )
		{
			/* wave table */
			CH->SLOT[s].wavetable = 0;
			CH->SLOT[s].state     = EG_OFF;
			CH->SLOT[s].volume    = MAX_ATT_INDEX;
		}
	}
}

/* Create one of virtual YM3812 */
/* 'clock' is chip clock in Hz  */
/* 'rate'  is sampling rate  */
static FM_OPL *OPLCreate(int type, int clock, int rate)
{
	char *ptr;
	FM_OPL *OPL;
	int state_size;

	if (OPL_LockTable() ==-1) return NULL;

	/* calculate OPL state size */
	state_size  = sizeof(FM_OPL);

	/* allocate memory block */
	ptr = (char *)malloc(state_size);

	if (ptr==NULL)
		return NULL;

	/* clear */
	memset(ptr,0,state_size);

	OPL  = (FM_OPL *)ptr;

	ptr += sizeof(FM_OPL);

	OPL->type  = type;
	OPL->clock = clock;
	OPL->rate  = rate;

	/* init global tables */
	OPL_initalize(OPL);

	/* reset chip */
	OPLResetChip(OPL);
	return OPL;
}

/* Destroy one of virtual YM3812 */
static void OPLDestroy(FM_OPL *OPL)
{
	OPL_UnLockTable();
	free(OPL);
}

/* Option handlers */

static void OPLSetTimerHandler(FM_OPL *OPL,OPL_TIMERHANDLER TimerHandler,int channelOffset)
{
	OPL->TimerHandler   = TimerHandler;
	OPL->TimerParam = channelOffset;
}
static void OPLSetIRQHandler(FM_OPL *OPL,OPL_IRQHANDLER IRQHandler,int param)
{
	OPL->IRQHandler     = IRQHandler;
	OPL->IRQParam = param;
}
static void OPLSetUpdateHandler(FM_OPL *OPL,OPL_UPDATEHANDLER UpdateHandler,int param)
{
	OPL->UpdateHandler = UpdateHandler;
	OPL->UpdateParam = param;
}

/* YM3812 I/O interface */
static int OPLWrite(FM_OPL *OPL,int a,int v)
{
	if( !(a&1) )
	{	/* address port */
		OPL->address = v & 0xff;
	}
	else
	{	/* data port */
		if(OPL->UpdateHandler) OPL->UpdateHandler(OPL->UpdateParam,0);
		OPLWriteReg(OPL,OPL->address,v);
	}
	return OPL->status>>7;
}

static unsigned char OPLRead(FM_OPL *OPL,int a)
{
	if( !(a&1) )
	{
		/* status port */
		return OPL->status & (OPL->statusmask|0x80);
	}

	return 0xff;
}

/* CSM Key Controll */
INLINE void CSMKeyControll(OPL_CH *CH)
{
	FM_KEYON (&CH->SLOT[SLOT1], 4);
	FM_KEYON (&CH->SLOT[SLOT2], 4);

	/* The key off should happen exactly one sample later - not implemented correctly yet */

	FM_KEYOFF(&CH->SLOT[SLOT1], ~4);
	FM_KEYOFF(&CH->SLOT[SLOT2], ~4);
}


static int OPLTimerOver(FM_OPL *OPL,int c)
{
	if( c )
	{	/* Timer B */
		OPL_STATUS_SET(OPL,0x20);
	}
	else
	{	/* Timer A */
		OPL_STATUS_SET(OPL,0x40);
		/* CSM mode key,TL controll */
		if( OPL->mode & 0x80 )
		{	/* CSM mode total level latch and auto key on */
			int ch;
			if(OPL->UpdateHandler) OPL->UpdateHandler(OPL->UpdateParam,0);
			for(ch=0; ch<9; ch++)
				CSMKeyControll( &OPL->P_CH[ch] );
		}
	}
	/* reload timer */
	if (OPL->TimerHandler) (OPL->TimerHandler)(OPL->TimerParam+c,(double)OPL->T[c]*OPL->TimerBase);
	return OPL->status>>7;
}


#define MAX_OPL_CHIPS 2


static FM_OPL *OPL_YM3812[MAX_OPL_CHIPS];	/* array of pointers to the YM3812's */
static int YM3812NumChips = 0;				/* number of chips */

int YM3812Init(int num, int clock, int rate)
{
	int i;

	if (YM3812NumChips)
		return -1;	/* duplicate init. */

	YM3812NumChips = num;

	for (i = 0;i < YM3812NumChips; i++)
	{
		/* emulator create */
		OPL_YM3812[i] = OPLCreate(OPL_TYPE_YM3812,clock,rate);
		if(OPL_YM3812[i] == NULL)
		{
			/* it's really bad - we run out of memeory */
			YM3812NumChips = 0;
			return -1;
		}
	}

	return 0;
}

void YM3812Shutdown(void)
{
	int i;

	for (i = 0;i < YM3812NumChips; i++)
	{
		/* emulator shutdown */
		OPLDestroy(OPL_YM3812[i]);
		OPL_YM3812[i] = NULL;
	}
	YM3812NumChips = 0;
}
void YM3812ResetChip(int which)
{
	OPLResetChip(OPL_YM3812[which]);
}

int YM3812Write(int which, int a, int v)
{
	return OPLWrite(OPL_YM3812[which], a, v);
}

unsigned char YM3812Read(int which, int a)
{
	/* YM3812 always returns bit2 and bit1 in HIGH state */
	return OPLRead(OPL_YM3812[which], a) | 0x06 ;
}
int YM3812TimerOver(int which, int c)
{
	return OPLTimerOver(OPL_YM3812[which], c);
}

void YM3812SetTimerHandler(int which, OPL_TIMERHANDLER TimerHandler, int channelOffset)
{
	OPLSetTimerHandler(OPL_YM3812[which], TimerHandler, channelOffset);
}
void YM3812SetIRQHandler(int which,OPL_IRQHANDLER IRQHandler,int param)
{
	OPLSetIRQHandler(OPL_YM3812[which], IRQHandler, param);
}
void YM3812SetUpdateHandler(int which,OPL_UPDATEHANDLER UpdateHandler,int param)
{
	OPLSetUpdateHandler(OPL_YM3812[which], UpdateHandler, param);
}


/*
** Generate samples for one of the YM3812's
**
** 'which' is the virtual YM3812 number
** '*buffer' is the output buffer pointer
** 'length' is the number of samples that should be generated
*/
void YM3812UpdateOne(int which, float *buffer, int length)
{
	FM_OPL		*OPL = OPL_YM3812[which];
	int i;

	if( (void *)OPL != cur_chip )
	{
		cur_chip = (void *)OPL;
	}
	UINT32 lfo_am_cnt_bak = OPL->lfo_am_cnt;
	UINT32 eg_timer_bak = OPL->eg_timer;
	UINT32 eg_cnt_bak = OPL->eg_cnt;

	UINT32 lfo_am_cnt_out = lfo_am_cnt_bak;
	UINT32 eg_timer_out = eg_timer_bak;
	UINT32 eg_cnt_out = eg_cnt_bak;

	for (i = 0; i <= 8; ++i)
	{
		OPL->lfo_am_cnt = lfo_am_cnt_bak;
		OPL->eg_timer = eg_timer_bak;
		OPL->eg_cnt = eg_cnt_bak;
		if (CalcVoice (OPL, i, buffer, length))
		{
			lfo_am_cnt_out = OPL->lfo_am_cnt;
			eg_timer_out = OPL->eg_timer;
			eg_cnt_out = OPL->eg_cnt;
		}
	}

	OPL->lfo_am_cnt = lfo_am_cnt_out;
	OPL->eg_timer = eg_timer_out;
	OPL->eg_cnt = eg_cnt_out;
}

// [RH] Render a whole voice at once. If nothing else, it lets us avoid
// wasting a lot of time on voices that aren't playing anything.

static bool CalcVoice (FM_OPL *OPL, int voice, float *buffer, int length)
{
	OPL_CH *const CH = &OPL->P_CH[voice];
	int i, j;

	if (CH->SLOT[0].state == EG_OFF && CH->SLOT[1].state == EG_OFF)
	{ // Voice is not playing, so don't do anything for it
		return false;
	}

	for (i = 0; i < length; ++i)
	{
		// advance_lfo
		UINT8 tmp;

		/* LFO */
		OPL->lfo_am_cnt += OPL->lfo_am_inc;
		if (OPL->lfo_am_cnt >= (UINT32)(LFO_AM_TAB_ELEMENTS<<LFO_SH) )	/* lfo_am_table is 210 elements long */
			OPL->lfo_am_cnt -= (LFO_AM_TAB_ELEMENTS<<LFO_SH);

		tmp = lfo_am_table[ OPL->lfo_am_cnt >> LFO_SH ];

		if (OPL->lfo_am_depth)
			LFO_AM = tmp;
		else
			LFO_AM = tmp>>2;

		OPL->lfo_pm_cnt += OPL->lfo_pm_inc;
		LFO_PM = ((OPL->lfo_pm_cnt>>LFO_SH) & 7) | OPL->lfo_pm_depth_range;

		// OPL_CALC_CH
		OPL_SLOT *SLOT;
		unsigned int env;
		signed int out;
		UINT32 p;

		phase_modulation = 0;
		output[0] = 0;

		/* SLOT 1 */
		SLOT = &CH->SLOT[SLOT1];
		env  = volume_calc(SLOT);
		out  = SLOT->op1_out[0] + SLOT->op1_out[1];
		SLOT->op1_out[0] = SLOT->op1_out[1];
		*SLOT->connect1 += SLOT->op1_out[0];
		SLOT->op1_out[1] = 0;
		if( env < ENV_QUIET )
		{
			if (!SLOT->FB)
				out = 0;
			p = (env<<4) + sin_tab[SLOT->wavetable + ((((SLOT->Cnt & ~FREQ_MASK) + (out<<SLOT->FB))>>FREQ_SH) & SIN_MASK)];
			SLOT->op1_out[1] = p >= TL_TAB_LEN ? 0 : tl_tab[p];
		}

		/* SLOT 2 */
		SLOT++;
		env = volume_calc(SLOT);
		if( env < ENV_QUIET )
		{
			p = (env<<4) + sin_tab[SLOT->wavetable + ((((signed int)((SLOT->Cnt & ~FREQ_MASK) + (phase_modulation<<16))) >> FREQ_SH ) & SIN_MASK) ];
			if (p < TL_TAB_LEN)
			{
				output[0] += tl_tab[p];
			}
			// [RH] Convert to floating point.
			buffer[i] += float(output[0]) / 10240;
		}

		// advance
		OPL->eg_timer += OPL->eg_timer_add;

		while (OPL->eg_timer >= OPL->eg_timer_overflow)
		{
			OPL->eg_timer -= OPL->eg_timer_overflow;

			OPL->eg_cnt++;

			for (j = 0; j < 2; ++j)
			{
				OPL_SLOT *op  = &CH->SLOT[j];

				/* Envelope Generator */
				switch(op->state)
				{
				case EG_ATT:		/* attack phase */
				{

					if ( !(OPL->eg_cnt & ((1<<op->eg_sh_ar)-1) ) )
					{
						op->volume += (~op->volume *
	                        					(eg_inc[op->eg_sel_ar + ((OPL->eg_cnt>>op->eg_sh_ar)&7)])
        										) >>3;

						if (op->volume <= MIN_ATT_INDEX)
						{
							op->volume = MIN_ATT_INDEX;
							op->state = EG_DEC;
						}

					}

				}
				break;

				case EG_DEC:	/* decay phase */
					if ( !(OPL->eg_cnt & ((1<<op->eg_sh_dr)-1) ) )
					{
						op->volume += eg_inc[op->eg_sel_dr + ((OPL->eg_cnt>>op->eg_sh_dr)&7)];

						if ( op->volume >= (INT32)op->sl )
							op->state = EG_SUS;

					}
				break;

				case EG_SUS:	/* sustain phase */

					/* this is important behaviour:
					one can change percusive/non-percussive modes on the fly and
					the chip will remain in sustain phase - verified on real YM3812 */

					if(op->eg_type)		/* non-percussive mode */
					{
										/* do nothing */
					}
					else				/* percussive mode */
					{
						/* during sustain phase chip adds Release Rate (in percussive mode) */
						if ( !(OPL->eg_cnt & ((1<<op->eg_sh_rr)-1) ) )
						{
							op->volume += eg_inc[op->eg_sel_rr + ((OPL->eg_cnt>>op->eg_sh_rr)&7)];

							if ( op->volume >= MAX_ATT_INDEX )
								op->volume = MAX_ATT_INDEX;
						}
						/* else do nothing in sustain phase */
					}
				break;

				case EG_REL:	/* release phase */
					if ( !(OPL->eg_cnt & ((1<<op->eg_sh_rr)-1) ) )
					{
						op->volume += eg_inc[op->eg_sel_rr + ((OPL->eg_cnt>>op->eg_sh_rr)&7)];

						if ( op->volume >= MAX_ATT_INDEX )
						{
							op->volume = MAX_ATT_INDEX;
							op->state = EG_OFF;
						}

					}
				break;

				default:
				break;
				}

				/* Phase Generator */
				if(op->vib)
				{
					UINT8 block;
					unsigned int block_fnum = CH->block_fnum;

					unsigned int fnum_lfo   = (block_fnum&0x0380) >> 7;

					signed int lfo_fn_table_index_offset = lfo_pm_table[LFO_PM + 16*fnum_lfo ];

					if (lfo_fn_table_index_offset)	/* LFO phase modulation active */
					{
						block_fnum += lfo_fn_table_index_offset;
						block = (block_fnum&0x1c00) >> 10;
						op->Cnt += (OPL->fn_tab[block_fnum&0x03ff] >> (7-block)) * op->mul;//ok
					}
					else	/* LFO phase modulation  = zero */
					{
						op->Cnt += op->Incr;
					}
				}
				else	/* LFO phase modulation disabled for this operator */
				{
					op->Cnt += op->Incr;
				}
			}
		}
	}
	return true;
}

FString YM3812GetVoiceString()
{
	FM_OPL *OPL = OPL_YM3812[0];
	char out[9*3];

	for (int i = 0; i <= 8; ++i)
	{
		int color;

		if (OPL != NULL && (OPL->P_CH[i].SLOT[0].state != EG_OFF || OPL->P_CH[i].SLOT[1].state != EG_OFF))
		{
			color = 'D';	// Green means in use
		}
		else
		{
			color = 'A';	// Brick means free
		}
		out[i*3+0] = '\x1c';
		out[i*3+1] = color;
		out[i*3+2] = '*';
	}
	return FString (out, 9*3);
}
