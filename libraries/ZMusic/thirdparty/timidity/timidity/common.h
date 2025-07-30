#pragma once

namespace Timidity
{
/*
config.h
*/

/* Acoustic Grand Piano seems to be the usual default instrument. */
#define DEFAULT_PROGRAM				 0

/* 9 here is MIDI channel 10, which is the standard percussion channel.
   Some files (notably C:\WINDOWS\CANYON.MID) think that 16 is one too. 
   On the other hand, some files know that 16 is not a drum channel and
   try to play music on it. This is now a runtime option, so this isn't
   a critical choice anymore. */
#define DEFAULT_DRUMCHANNELS		(1<<9)
/*#define DEFAULT_DRUMCHANNELS		((1<<9) | (1<<15))*/

#define MAXCHAN						16
#define MAXNOTE						128

/* 1000 here will give a control ratio of 22:1 with 22 kHz output.
   Higher CONTROLS_PER_SECOND values allow more accurate rendering
   of envelopes and tremolo. The cost is CPU time. */
#define CONTROLS_PER_SECOND			1000

/* A scalar applied to the final mix to try and approximate the
   volume level of FMOD's built-in MIDI player. */
#define FINAL_MIX_SCALE				0.5

/* How many bits to use for the fractional part of sample positions.
   This affects tonal accuracy. The entire position counter must fit
   in 32 bits, so with FRACTION_BITS equal to 12, the maximum size of
   a sample is 1048576 samples (2 megabytes in memory). The GUS gets
   by with just 9 bits and a little help from its friends...
   "The GUS does not SUCK!!!" -- a happy user :) */
#define FRACTION_BITS				12

/* For some reason the sample volume is always set to maximum in all
   patch files. Define this for a crude adjustment that may help
   equalize instrument volumes. */
//#define ADJUST_SAMPLE_VOLUMES

/* The number of samples to use for ramping out a dying note. Affects
   click removal. */
#define MAX_DIE_TIME				20

/**************************************************************************/
/* Anything below this shouldn't need to be changed unless you're porting
   to a new machine with other than 32-bit, big-endian words. */
/**************************************************************************/

/* change FRACTION_BITS above, not these */
#define INTEGER_BITS				(32 - FRACTION_BITS)
#define INTEGER_MASK				(0xFFFFFFFF << FRACTION_BITS)
#define FRACTION_MASK				(~ INTEGER_MASK)
#define MAX_SAMPLE_SIZE				(1 << INTEGER_BITS)

/* This is enforced by some computations that must fit in an int */
#define MAX_CONTROL_RATIO			255

#define MAX_AMPLIFICATION			800

#define FINAL_VOLUME(v)				(v)

#define FSCALE(a,b)					((a) * (float)(1<<(b)))
#define FSCALENEG(a,b)				((a) * (1.0L / (float)(1<<(b))))

/* Vibrato and tremolo Choices of the Day */
#define SWEEP_TUNING				38
#define VIBRATO_AMPLITUDE_TUNING	1.0
#define VIBRATO_RATE_TUNING			38
#define TREMOLO_AMPLITUDE_TUNING	1.0
#define TREMOLO_RATE_TUNING			38

#define SWEEP_SHIFT					16
#define RATE_SHIFT					5

#ifndef PI
  #define PI 3.14159265358979323846
#endif

#if defined(__GNUC__) && !defined(__clang__) && (defined(__i386__) || defined(__x86_64__))
// [RH] MinGW's pow() function is terribly slow compared to VC8's
// (I suppose because it's using an old version from MSVCRT.DLL).
// On an Opteron running x86-64 Linux, this also ended up being about
// 100 cycles faster than libm's pow(), which is why I'm using this
// for GCC in general and not just for MinGW.
// [CE] Clang doesn't yet support some inline ASM operations so I disabled it for that instance

extern __inline__ double pow_x87_inline(double x,double y)
{
	double result;

	if (y == 0)
	{
		return 1;
	}
	if (x == 0)
	{
		if (y > 0)
		{
			return 0;
		}
		else
		{
			union { double fp; long long ip; } infinity;
			infinity.ip = 0x7FF0000000000000ll;
			return infinity.fp;
		}
	}
	__asm__ (
		"fyl2x\n\t"
		"fld %%st(0)\n\t"
		"frndint\n\t"
		"fxch\n\t"
		"fsub %%st(1),%%st(0)\n\t"
		"f2xm1\n\t"
		"fld1\n\t"
		"faddp\n\t"
		"fxch\n\t"
		"fld1\n\t"
		"fscale\n\t"
		"fstp %%st(1)\n\t"
		"fmulp\n\t"
		: "=t" (result)
		: "0" (x), "u" (y)
		: "st(1)", "st(7)" );
	return result;
}
#define pow pow_x87_inline
#endif

/*
common.h
*/

extern void *safe_malloc(size_t count);

#ifndef MAKE_ID
#ifndef __BIG_ENDIAN__
#define MAKE_ID(a,b,c,d)	((uint32_t)((a)|((b)<<8)|((c)<<16)|((d)<<24)))
#else
#define MAKE_ID(a,b,c,d)	((uint32_t)((d)|((c)<<8)|((b)<<16)|((a)<<24)))
#endif
#endif

}
