// "Build Engine & Tools" Copyright (c) 1993-1997 Ken Silverman
// Ken Silverman's official web site: "http://www.advsys.net/ken"
// See the included license file "BUILDLIC.TXT" for license info.
//
// This file is based on pragmas.h from Ken Silverman's original Build
// source code release but is meant for use with any compiler and does not
// rely on any inline assembly.
//

#if _MSC_VER
#pragma once
#endif

#if defined(__GNUC__) && !defined(__forceinline)
#define __forceinline __inline__ __attribute__((always_inline))
#endif

static __forceinline SDWORD Scale (SDWORD a, SDWORD b, SDWORD c)
{
	return (SDWORD)(((SQWORD)a*b)/c);
}

static __forceinline SDWORD MulScale (SDWORD a, SDWORD b, SDWORD c)
{
	return (SDWORD)(((SQWORD)a*b)>>c);
}

static __forceinline SDWORD MulScale1 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 1); }
static __forceinline SDWORD MulScale2 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 2); }
static __forceinline SDWORD MulScale3 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 3); }
static __forceinline SDWORD MulScale4 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 4); }
static __forceinline SDWORD MulScale5 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 5); }
static __forceinline SDWORD MulScale6 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 6); }
static __forceinline SDWORD MulScale7 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 7); }
static __forceinline SDWORD MulScale8 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 8); }
static __forceinline SDWORD MulScale9 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 9); }
static __forceinline SDWORD MulScale10 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 10); }
static __forceinline SDWORD MulScale11 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 11); }
static __forceinline SDWORD MulScale12 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 12); }
static __forceinline SDWORD MulScale13 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 13); }
static __forceinline SDWORD MulScale14 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 14); }
static __forceinline SDWORD MulScale15 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 15); }
static __forceinline SDWORD MulScale16 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 16); }
static __forceinline SDWORD MulScale17 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 17); }
static __forceinline SDWORD MulScale18 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 18); }
static __forceinline SDWORD MulScale19 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 19); }
static __forceinline SDWORD MulScale20 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 20); }
static __forceinline SDWORD MulScale21 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 21); }
static __forceinline SDWORD MulScale22 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 22); }
static __forceinline SDWORD MulScale23 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 23); }
static __forceinline SDWORD MulScale24 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 24); }
static __forceinline SDWORD MulScale25 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 25); }
static __forceinline SDWORD MulScale26 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 26); }
static __forceinline SDWORD MulScale27 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 27); }
static __forceinline SDWORD MulScale28 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 28); }
static __forceinline SDWORD MulScale29 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 29); }
static __forceinline SDWORD MulScale30 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 30); }
static __forceinline SDWORD MulScale31 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 31); }
static __forceinline SDWORD MulScale32 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 32); }

static __forceinline DWORD UMulScale16 (DWORD a, DWORD b) { return (DWORD)(((QWORD)a * b) >> 16); }

static __forceinline SDWORD DMulScale (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD s)
{
	return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> s);
}

static __forceinline SDWORD DMulScale1 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 1); }
static __forceinline SDWORD DMulScale2 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 2); }
static __forceinline SDWORD DMulScale3 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 3); }
static __forceinline SDWORD DMulScale4 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 4); }
static __forceinline SDWORD DMulScale5 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 5); }
static __forceinline SDWORD DMulScale6 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 6); }
static __forceinline SDWORD DMulScale7 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 7); }
static __forceinline SDWORD DMulScale8 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 8); }
static __forceinline SDWORD DMulScale9 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 9); }
static __forceinline SDWORD DMulScale10 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 10); }
static __forceinline SDWORD DMulScale11 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 11); }
static __forceinline SDWORD DMulScale12 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 12); }
static __forceinline SDWORD DMulScale13 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 13); }
static __forceinline SDWORD DMulScale14 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 14); }
static __forceinline SDWORD DMulScale15 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 15); }
static __forceinline SDWORD DMulScale16 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 16); }
static __forceinline SDWORD DMulScale17 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 17); }
static __forceinline SDWORD DMulScale18 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 18); }
static __forceinline SDWORD DMulScale19 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 19); }
static __forceinline SDWORD DMulScale20 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 20); }
static __forceinline SDWORD DMulScale21 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 21); }
static __forceinline SDWORD DMulScale22 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 22); }
static __forceinline SDWORD DMulScale23 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 23); }
static __forceinline SDWORD DMulScale24 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 24); }
static __forceinline SDWORD DMulScale25 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 25); }
static __forceinline SDWORD DMulScale26 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 26); }
static __forceinline SDWORD DMulScale27 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 27); }
static __forceinline SDWORD DMulScale28 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 28); }
static __forceinline SDWORD DMulScale29 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 29); }
static __forceinline SDWORD DMulScale30 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 30); }
static __forceinline SDWORD DMulScale31 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 31); }
static __forceinline SDWORD DMulScale32 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 32); }

static __forceinline SDWORD TMulScale1 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 1); }
static __forceinline SDWORD TMulScale2 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 2); }
static __forceinline SDWORD TMulScale3 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 3); }
static __forceinline SDWORD TMulScale4 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 4); }
static __forceinline SDWORD TMulScale5 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 5); }
static __forceinline SDWORD TMulScale6 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 6); }
static __forceinline SDWORD TMulScale7 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 7); }
static __forceinline SDWORD TMulScale8 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 8); }
static __forceinline SDWORD TMulScale9 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 9); }
static __forceinline SDWORD TMulScale10 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 10); }
static __forceinline SDWORD TMulScale11 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 11); }
static __forceinline SDWORD TMulScale12 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 12); }
static __forceinline SDWORD TMulScale13 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 13); }
static __forceinline SDWORD TMulScale14 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 14); }
static __forceinline SDWORD TMulScale15 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 15); }
static __forceinline SDWORD TMulScale16 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 16); }
static __forceinline SDWORD TMulScale17 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 17); }
static __forceinline SDWORD TMulScale18 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 18); }
static __forceinline SDWORD TMulScale19 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 19); }
static __forceinline SDWORD TMulScale20 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 20); }
static __forceinline SDWORD TMulScale21 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 21); }
static __forceinline SDWORD TMulScale22 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 22); }
static __forceinline SDWORD TMulScale23 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 23); }
static __forceinline SDWORD TMulScale24 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 24); }
static __forceinline SDWORD TMulScale25 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 25); }
static __forceinline SDWORD TMulScale26 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 26); }
static __forceinline SDWORD TMulScale27 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 27); }
static __forceinline SDWORD TMulScale28 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 28); }
static __forceinline SDWORD TMulScale29 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 29); }
static __forceinline SDWORD TMulScale30 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 30); }
static __forceinline SDWORD TMulScale31 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 31); }
static __forceinline SDWORD TMulScale32 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 32); }

static __forceinline SDWORD BoundMulScale (SDWORD a, SDWORD b, SDWORD c)
{
	SQWORD x = ((SQWORD)a * b) >> c;
	return x >  0x7FFFFFFFll ? 0x7FFFFFFF :
		   x < -0x80000000ll ? 0x80000000 :
		   (SDWORD)x;
}

static inline SDWORD DivScale (SDWORD a, SDWORD b, SDWORD c)
{
	return (SDWORD)(((SQWORD)a << c) / b);
}

static inline SDWORD DivScale1 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 1) / b); }
static inline SDWORD DivScale2 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 2) / b); }
static inline SDWORD DivScale3 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 3) / b); }
static inline SDWORD DivScale4 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 4) / b); }
static inline SDWORD DivScale5 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 5) / b); }
static inline SDWORD DivScale6 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 6) / b); }
static inline SDWORD DivScale7 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 7) / b); }
static inline SDWORD DivScale8 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 8) / b); }
static inline SDWORD DivScale9 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 9) / b); }
static inline SDWORD DivScale10 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 10) / b); }
static inline SDWORD DivScale11 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 11) / b); }
static inline SDWORD DivScale12 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 12) / b); }
static inline SDWORD DivScale13 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 13) / b); }
static inline SDWORD DivScale14 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 14) / b); }
static inline SDWORD DivScale15 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 15) / b); }
static inline SDWORD DivScale16 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 16) / b); }
static inline SDWORD DivScale17 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 17) / b); }
static inline SDWORD DivScale18 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 18) / b); }
static inline SDWORD DivScale19 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 19) / b); }
static inline SDWORD DivScale20 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 20) / b); }
static inline SDWORD DivScale21 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 21) / b); }
static inline SDWORD DivScale22 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 22) / b); }
static inline SDWORD DivScale23 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 23) / b); }
static inline SDWORD DivScale24 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 24) / b); }
static inline SDWORD DivScale25 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 25) / b); }
static inline SDWORD DivScale26 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 26) / b); }
static inline SDWORD DivScale27 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 27) / b); }
static inline SDWORD DivScale28 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 28) / b); }
static inline SDWORD DivScale29 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 29) / b); }
static inline SDWORD DivScale30 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 30) / b); }
static inline SDWORD DivScale31 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 31) / b); }
static inline SDWORD DivScale32 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 32) / b); }

static __forceinline void clearbuf (void *buff, unsigned int count, SDWORD clear)
{
	SDWORD *b2 = (SDWORD *)buff;
	for (unsigned int i = 0; i != count; ++i)
	{
		b2[i] = clear;
	}
}

static __forceinline void clearbufshort (void *buff, unsigned int count, WORD clear)
{
	SWORD *b2 = (SWORD *)buff;
	for (unsigned int i = 0; i != count; ++i)
	{
		b2[i] = clear;
	}
}

static __forceinline SDWORD ksgn (SDWORD a)
{
	if (a < 0) return -1;
	else if (a > 0) return 1;
	else return 0;
}
