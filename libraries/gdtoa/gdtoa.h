/****************************************************************

The author of this software is David M. Gay.

Copyright (C) 1998 by Lucent Technologies
All Rights Reserved

Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appear in all
copies and that both that the copyright notice and this
permission notice and warranty disclaimer appear in supporting
documentation, and that the name of Lucent or any of its entities
not be used in advertising or publicity pertaining to
distribution of the software without specific, written prior
permission.

LUCENT DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.
IN NO EVENT SHALL LUCENT OR ANY OF ITS ENTITIES BE LIABLE FOR ANY
SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER
IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
THIS SOFTWARE.

****************************************************************/

/* Please send bug reports to David M. Gay (dmg at acm dot org,
 * with " at " changed at "@" and " dot " changed to ".").	*/

#ifndef GDTOA_H_INCLUDED
#define GDTOA_H_INCLUDED

#if defined(_MSC_VER)
/* [RH] Generating arith.h strikes me as too cumbersome under Visual
 * Studio, so here's the equivalent, given the limited number of
 * architectures that MSC can target. (Itanium? Who cares about that?)
 */
#define IEEE_8087
#define Arith_Kind_ASL 1
#define Double_Align
#ifdef _M_X64
#define X64_bit_pointers
#endif
#elif defined(__APPLE__)
/* [BL] While generating the files may be easy, on OS X we have cross
 * compiling to deal with, which means we can't run the generation
 * program on the target.
 */
#if defined(__x86_64__) || defined(__arm64__)
#define IEEE_8087
#define Arith_Kind_ASL 1
#define Long int
#define Intcast (int)(long)
#define Double_Align
#define X64_bit_pointers
#else
#error Unsupported architecture
#endif
#else
#include "arith.h"
#endif
#include <stddef.h> /* for size_t */

#ifndef Long
#define Long int
#endif
#ifndef ULong
typedef unsigned Long ULong;
#endif
#ifndef UShort
typedef unsigned short UShort;
#endif

#ifndef ANSI
#ifdef KR_headers
#define ANSI(x) ()
#define Void /*nothing*/
#else
#define ANSI(x) x
#define Void void
#endif
#endif /* ANSI */

#ifndef CONST
#ifdef KR_headers
#define CONST /* blank */
#else
#define CONST const
#endif
#endif /* CONST */

 enum {	/* return values from strtodg */
	STRTOG_Zero	= 0,
	STRTOG_Normal	= 1,
	STRTOG_Denormal	= 2,
	STRTOG_Infinite	= 3,
	STRTOG_NaN	= 4,
	STRTOG_NaNbits	= 5,
	STRTOG_NoNumber	= 6,
	STRTOG_Retmask	= 7,

	/* The following may be or-ed into one of the above values. */

	STRTOG_Neg	= 0x08, /* does not affect STRTOG_Inexlo or STRTOG_Inexhi */
	STRTOG_Inexlo	= 0x10,	/* returned result rounded toward zero */
	STRTOG_Inexhi	= 0x20, /* returned result rounded away from zero */
	STRTOG_Inexact	= 0x30,
	STRTOG_Underflow= 0x40,
	STRTOG_Overflow	= 0x80
	};

 typedef struct
FPI {
	int nbits;
	int emin;
	int emax;
	int rounding;
	int sudden_underflow;
	int int_max;
	} FPI;

enum {	/* FPI.rounding values: same as FLT_ROUNDS */
	FPI_Round_zero = 0,
	FPI_Round_near = 1,
	FPI_Round_up = 2,
	FPI_Round_down = 3
	};

#ifdef __cplusplus
extern "C" {
#endif

extern char* dtoa  ANSI((double d, int mode, int ndigits, int *decpt,
			int *sign, char **rve));
extern char* gdtoa ANSI((FPI *fpi, int be, ULong *bits, int *kindp,
			int mode, int ndigits, int *decpt, char **rve));
extern void freedtoa ANSI((char*));
//extern float  strtof ANSI((CONST char *, char **));
//extern double strtod ANSI((CONST char *, char **));
extern int strtodg ANSI((CONST char*, char**, FPI*, Long*, ULong*));

extern char*	g_ddfmt   ANSI((char*, double*, int, size_t));
extern char*	g_ddfmt_p ANSI((char*, double*,	int, size_t, int));
extern char*	g_dfmt    ANSI((char*, double*, int, size_t));
extern char*	g_dfmt_p  ANSI((char*, double*,	int, size_t, int));
extern char*	g_ffmt    ANSI((char*, float*,  int, size_t));
extern char*	g_ffmt_p  ANSI((char*, float*,	int, size_t, int));
extern char*	g_Qfmt    ANSI((char*, void*,   int, size_t));
extern char*	g_Qfmt_p  ANSI((char*, void*,	int, size_t, int));
extern char*	g_xfmt    ANSI((char*, void*,   int, size_t));
extern char*	g_xfmt_p  ANSI((char*, void*,	int, size_t, int));
extern char*	g_xLfmt   ANSI((char*, void*,   int, size_t));
extern char*	g_xLfmt_p ANSI((char*, void*,	int, size_t, int));

extern int	strtoId  ANSI((CONST char*, char**, double*, double*));
extern int	strtoIdd ANSI((CONST char*, char**, double*, double*));
extern int	strtoIf  ANSI((CONST char*, char**, float*, float*));
extern int	strtoIQ  ANSI((CONST char*, char**, void*, void*));
extern int	strtoIx  ANSI((CONST char*, char**, void*, void*));
extern int	strtoIxL ANSI((CONST char*, char**, void*, void*));
extern int	strtord  ANSI((CONST char*, char**, int, double*));
extern int	strtordd ANSI((CONST char*, char**, int, double*));
extern int	strtorf  ANSI((CONST char*, char**, int, float*));
extern int	strtorQ  ANSI((CONST char*, char**, int, void*));
extern int	strtorx  ANSI((CONST char*, char**, int, void*));
extern int	strtorxL ANSI((CONST char*, char**, int, void*));
#if 1
extern int	strtodI  ANSI((CONST char*, char**, double*));
extern int	strtopd  ANSI((CONST char*, char**, double*));
extern int	strtopdd ANSI((CONST char*, char**, double*));
extern int	strtopf  ANSI((CONST char*, char**, float*));
extern int	strtopQ  ANSI((CONST char*, char**, void*));
extern int	strtopx  ANSI((CONST char*, char**, void*));
extern int	strtopxL ANSI((CONST char*, char**, void*));
#else
#define strtopd(s,se,x) strtord(s,se,1,x)
#define strtopdd(s,se,x) strtordd(s,se,1,x)
#define strtopf(s,se,x) strtorf(s,se,1,x)
#define strtopQ(s,se,x) strtorQ(s,se,1,x)
#define strtopx(s,se,x) strtorx(s,se,1,x)
#define strtopxL(s,se,x) strtorxL(s,se,1,x)
#endif

#ifdef __cplusplus
}
#endif
#endif /* GDTOA_H_INCLUDED */
