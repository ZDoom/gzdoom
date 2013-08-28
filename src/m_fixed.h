// "Build Engine & Tools" Copyright (c) 1993-1997 Ken Silverman
// Ken Silverman's official web site: "http://www.advsys.net/ken"
// See the included license file "BUILDLIC.TXT" for license info.
//
// This file is based on pragmas.h from Ken Silverman's original Build
// source code release and contains routines that were originally
// inline assembly but are not now.

#ifndef __M_FIXED__
#define __M_FIXED__

#include <stdlib.h>
#include "doomtype.h"

#if defined(__GNUC__) && defined(__i386__) && !defined(__clang__)
#include "gccinlines.h"
#elif defined(_MSC_VER) && defined(_M_IX86)
#include "mscinlines.h"
#else
#include "basicinlines.h"
#endif

#include "xs_Float.h"

#define MAKESAFEDIVSCALE(x) \
	inline SDWORD SafeDivScale##x (SDWORD a, SDWORD b) \
	{ \
		if ((DWORD)abs(a) >> (31-x) >= (DWORD)abs (b)) \
			return (a^b)<0 ? FIXED_MIN : FIXED_MAX; \
		return DivScale##x (a, b); \
	}

MAKESAFEDIVSCALE(1)
MAKESAFEDIVSCALE(2)
MAKESAFEDIVSCALE(3)
MAKESAFEDIVSCALE(4)
MAKESAFEDIVSCALE(5)
MAKESAFEDIVSCALE(6)
MAKESAFEDIVSCALE(7)
MAKESAFEDIVSCALE(8)
MAKESAFEDIVSCALE(9)
MAKESAFEDIVSCALE(10)
MAKESAFEDIVSCALE(11)
MAKESAFEDIVSCALE(12)
MAKESAFEDIVSCALE(13)
MAKESAFEDIVSCALE(14)
MAKESAFEDIVSCALE(15)
MAKESAFEDIVSCALE(16)
MAKESAFEDIVSCALE(17)
MAKESAFEDIVSCALE(18)
MAKESAFEDIVSCALE(19)
MAKESAFEDIVSCALE(20)
MAKESAFEDIVSCALE(21)
MAKESAFEDIVSCALE(22)
MAKESAFEDIVSCALE(23)
MAKESAFEDIVSCALE(24)
MAKESAFEDIVSCALE(25)
MAKESAFEDIVSCALE(26)
MAKESAFEDIVSCALE(27)
MAKESAFEDIVSCALE(28)
MAKESAFEDIVSCALE(29)
MAKESAFEDIVSCALE(30)
#undef MAKESAFEDIVSCALE

inline SDWORD SafeDivScale31 (SDWORD a, SDWORD b)
{
	if ((DWORD)abs(a) >= (DWORD)abs (b))
		return (a^b)<0 ? FIXED_MIN : FIXED_MAX;
	return DivScale31 (a, b);
}

inline SDWORD SafeDivScale32 (SDWORD a, SDWORD b)
{
	if ((DWORD)abs(a) >= (DWORD)abs (b) >> 1)
		return (a^b)<0 ? FIXED_MIN : FIXED_MAX;
	return DivScale32 (a, b);
}

#define FixedMul MulScale16
#define FixedDiv SafeDivScale16

inline void qinterpolatedown16 (SDWORD *out, DWORD count, SDWORD val, SDWORD delta)
{
	if (count & 1)
	{
		out[0] = val >> 16;
		val += delta;
	}
	count >>= 1;
	while (count-- != 0)
	{
		int temp = val + delta;
		out[0] = val >> 16;
		val = temp + delta;
		out[1] = temp >> 16;
		out += 2;
	}
}

inline void qinterpolatedown16short (short *out, DWORD count, SDWORD val, SDWORD delta)
{
	if (count)
	{
		if ((size_t)out & 2)
		{ // align to dword boundary
			*out++ = (short)(val >> 16);
			count--;
			val += delta;
		}
		DWORD *o2 = (DWORD *)out;
		DWORD c2 = count>>1;
		while (c2-- != 0)
		{
			SDWORD temp = val + delta;
			*o2++ = (temp & 0xffff0000) | ((DWORD)val >> 16);
			val = temp + delta;
		}
		if (count & 1)
		{
			*(short *)o2 = (short)(val >> 16);
		}
	}
}

	//returns num/den, dmval = num%den
inline SDWORD DivMod (SDWORD num, SDWORD den, SDWORD *dmval)
{
	*dmval = num % den;
	return num / den;
}

	//returns num%den, dmval = num/den
inline SDWORD ModDiv (SDWORD num, SDWORD den, SDWORD *dmval)
{
	*dmval = num / den;
	return num % den;
}


#define FLOAT2FIXED(f)		xs_Fix<16>::ToFix(f)
#define FIXED2FLOAT(f)		((f) / float(65536))
#define FIXED2DBL(f)		((f) / double(65536))

#endif
