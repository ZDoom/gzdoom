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

#ifndef _MSC_VER
#define __forceinline inline
#endif

__forceinline SDWORD Scale (SDWORD a, SDWORD b, SDWORD c)
{
	return (SDWORD)(((SQWORD)a*b)/c);
}

__forceinline SDWORD MulScale (SDWORD a, SDWORD b, SDWORD c)
{
	return (SDWORD)(((SQWORD)a*b)>>c);
}

__forceinline SDWORD MulScale1 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 1); }
__forceinline SDWORD MulScale2 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 2); }
__forceinline SDWORD MulScale3 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 3); }
__forceinline SDWORD MulScale4 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 4); }
__forceinline SDWORD MulScale5 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 5); }
__forceinline SDWORD MulScale6 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 6); }
__forceinline SDWORD MulScale7 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 7); }
__forceinline SDWORD MulScale8 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 8); }
__forceinline SDWORD MulScale9 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 9); }
__forceinline SDWORD MulScale10 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 10); }
__forceinline SDWORD MulScale11 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 11); }
__forceinline SDWORD MulScale12 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 12); }
__forceinline SDWORD MulScale13 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 13); }
__forceinline SDWORD MulScale14 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 14); }
__forceinline SDWORD MulScale15 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 15); }
__forceinline SDWORD MulScale16 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 16); }
__forceinline SDWORD MulScale17 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 17); }
__forceinline SDWORD MulScale18 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 18); }
__forceinline SDWORD MulScale19 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 19); }
__forceinline SDWORD MulScale20 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 20); }
__forceinline SDWORD MulScale21 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 21); }
__forceinline SDWORD MulScale22 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 22); }
__forceinline SDWORD MulScale23 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 23); }
__forceinline SDWORD MulScale24 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 24); }
__forceinline SDWORD MulScale25 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 25); }
__forceinline SDWORD MulScale26 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 26); }
__forceinline SDWORD MulScale27 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 27); }
__forceinline SDWORD MulScale28 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 28); }
__forceinline SDWORD MulScale29 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 29); }
__forceinline SDWORD MulScale30 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 30); }
__forceinline SDWORD MulScale31 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 31); }
__forceinline SDWORD MulScale32 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a * b) >> 32); }

__forceinline SDWORD DMulScale (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD s)
{
	return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> s);
}

__forceinline SDWORD DMulScale1 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 1); }
__forceinline SDWORD DMulScale2 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 2); }
__forceinline SDWORD DMulScale3 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 3); }
__forceinline SDWORD DMulScale4 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 4); }
__forceinline SDWORD DMulScale5 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 5); }
__forceinline SDWORD DMulScale6 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 6); }
__forceinline SDWORD DMulScale7 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 7); }
__forceinline SDWORD DMulScale8 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 8); }
__forceinline SDWORD DMulScale9 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 9); }
__forceinline SDWORD DMulScale10 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 10); }
__forceinline SDWORD DMulScale11 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 11); }
__forceinline SDWORD DMulScale12 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 12); }
__forceinline SDWORD DMulScale13 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 13); }
__forceinline SDWORD DMulScale14 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 14); }
__forceinline SDWORD DMulScale15 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 15); }
__forceinline SDWORD DMulScale16 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 16); }
__forceinline SDWORD DMulScale17 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 17); }
__forceinline SDWORD DMulScale18 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 18); }
__forceinline SDWORD DMulScale19 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 19); }
__forceinline SDWORD DMulScale20 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 20); }
__forceinline SDWORD DMulScale21 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 21); }
__forceinline SDWORD DMulScale22 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 22); }
__forceinline SDWORD DMulScale23 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 23); }
__forceinline SDWORD DMulScale24 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 24); }
__forceinline SDWORD DMulScale25 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 25); }
__forceinline SDWORD DMulScale26 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 26); }
__forceinline SDWORD DMulScale27 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 27); }
__forceinline SDWORD DMulScale28 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 28); }
__forceinline SDWORD DMulScale29 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 29); }
__forceinline SDWORD DMulScale30 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 30); }
__forceinline SDWORD DMulScale31 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 31); }
__forceinline SDWORD DMulScale32 (SDWORD a, SDWORD b, SDWORD c, SDWORD d) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d) >> 32); }

__forceinline SDWORD TMulScale1 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 1); }
__forceinline SDWORD TMulScale2 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 2); }
__forceinline SDWORD TMulScale3 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 3); }
__forceinline SDWORD TMulScale4 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 4); }
__forceinline SDWORD TMulScale5 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 5); }
__forceinline SDWORD TMulScale6 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 6); }
__forceinline SDWORD TMulScale7 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 7); }
__forceinline SDWORD TMulScale8 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 8); }
__forceinline SDWORD TMulScale9 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 9); }
__forceinline SDWORD TMulScale10 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 10); }
__forceinline SDWORD TMulScale11 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 11); }
__forceinline SDWORD TMulScale12 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 12); }
__forceinline SDWORD TMulScale13 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 13); }
__forceinline SDWORD TMulScale14 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 14); }
__forceinline SDWORD TMulScale15 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 15); }
__forceinline SDWORD TMulScale16 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 16); }
__forceinline SDWORD TMulScale17 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 17); }
__forceinline SDWORD TMulScale18 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 18); }
__forceinline SDWORD TMulScale19 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 19); }
__forceinline SDWORD TMulScale20 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 20); }
__forceinline SDWORD TMulScale21 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 21); }
__forceinline SDWORD TMulScale22 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 22); }
__forceinline SDWORD TMulScale23 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 23); }
__forceinline SDWORD TMulScale24 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 24); }
__forceinline SDWORD TMulScale25 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 25); }
__forceinline SDWORD TMulScale26 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 26); }
__forceinline SDWORD TMulScale27 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 27); }
__forceinline SDWORD TMulScale28 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 28); }
__forceinline SDWORD TMulScale29 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 29); }
__forceinline SDWORD TMulScale30 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 30); }
__forceinline SDWORD TMulScale31 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 31); }
__forceinline SDWORD TMulScale32 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) { return (SDWORD)(((SQWORD)a*b + (SQWORD)c*d + (SQWORD)e*f) >> 32); }

__forceinline SDWORD BoundMulScale (SDWORD a, SDWORD b, SDWORD c)
{
	SQWORD x = ((SQWORD)a * b) >> c;
	return x >  0x7FFFFFFFll ? 0x7FFFFFFF :
		   x < -0x80000000ll ? 0x80000000 :
		   (SDWORD)x;
}

inline SDWORD DivScale (SDWORD a, SDWORD b, SDWORD c)
{
	return (SDWORD)(((SQWORD)a << c) / b);
}

inline SDWORD DivScale1 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 1) / b); }
inline SDWORD DivScale2 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 2) / b); }
inline SDWORD DivScale3 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 3) / b); }
inline SDWORD DivScale4 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 4) / b); }
inline SDWORD DivScale5 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 5) / b); }
inline SDWORD DivScale6 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 6) / b); }
inline SDWORD DivScale7 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 7) / b); }
inline SDWORD DivScale8 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 8) / b); }
inline SDWORD DivScale9 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 9) / b); }
inline SDWORD DivScale10 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 10) / b); }
inline SDWORD DivScale11 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 11) / b); }
inline SDWORD DivScale12 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 12) / b); }
inline SDWORD DivScale13 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 13) / b); }
inline SDWORD DivScale14 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 14) / b); }
inline SDWORD DivScale15 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 15) / b); }
inline SDWORD DivScale16 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 16) / b); }
inline SDWORD DivScale17 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 17) / b); }
inline SDWORD DivScale18 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 18) / b); }
inline SDWORD DivScale19 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 19) / b); }
inline SDWORD DivScale20 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 20) / b); }
inline SDWORD DivScale21 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 21) / b); }
inline SDWORD DivScale22 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 22) / b); }
inline SDWORD DivScale23 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 23) / b); }
inline SDWORD DivScale24 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 24) / b); }
inline SDWORD DivScale25 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 25) / b); }
inline SDWORD DivScale26 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 26) / b); }
inline SDWORD DivScale27 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 27) / b); }
inline SDWORD DivScale28 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 28) / b); }
inline SDWORD DivScale29 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 29) / b); }
inline SDWORD DivScale30 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 30) / b); }
inline SDWORD DivScale31 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 31) / b); }
inline SDWORD DivScale32 (SDWORD a, SDWORD b) { return (SDWORD)(((SQWORD)a << 32) / b); }

__forceinline void clearbuf (void *buff, unsigned int count, SDWORD clear)
{
	SDWORD *b2 = (SDWORD *)buff;
	for (unsigned int i = 0; i != count; ++i)
	{
		b2[i] = clear;
	}
}

__forceinline void clearbufshort (void *buff, unsigned int count, WORD clear)
{
	SWORD *b2 = (SWORD *)buff;
	for (unsigned int i = 0; i != count; ++i)
	{
		b2[i] = clear;
	}
}

__forceinline SDWORD ksgn (SDWORD a)
{
	if (a < 0) return -1;
	else if (a > 0) return 1;
	else return 0;
}

__forceinline int toint (float v)
{
	return int(v);
}

__forceinline int quickertoint (float v)
{
	return int(v);
}
