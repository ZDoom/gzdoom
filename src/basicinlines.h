// "Build Engine & Tools" Copyright (c) 1993-1997 Ken Silverman
// Ken Silverman's official web site: "http://www.advsys.net/ken"
// See the included license file "BUILDLIC.TXT" for license info.
//
// This file is based on pragmas.h from Ken Silverman's original Build
// source code release but is meant for use with any compiler and does not
// rely on any inline assembly.
//
// Some of the inline assembly has been turned into C code, because VC++
// is smart enough to produce code at least as good as Ken's inlines.
// The more used functions are still inline assembly, because they do
// things that can't really be done in C. (I consider this a bad thing,
// because VC++ has considerably poorer support for inline assembly than
// Watcom, so it's better to rely on its C optimizer to produce fast code.)
//

#include <string.h>
#include <stddef.h>

#define toint(f)			((int)(f))
#define quickertoint(f)		((int)(f))

inline SDWORD Scale (SDWORD a, SDWORD b, SDWORD c)
{
	return (fixed_t)(((__int64) a * (__int64) b) / c);
}

inline SDWORD MulScale (SDWORD a, SDWORD b, SDWORD c)
{
	return (fixed_t)(((__int64) a * (__int64) b) >> c);
}

#define MAKECONSTMulScale(s) \
	inline SDWORD MulScale##s (SDWORD a, SDWORD b) \
	{ \
		return (fixed_t)(((__int64) a * (__int64) b) >> s); \
	}
MAKECONSTMulScale(1)
MAKECONSTMulScale(2)
MAKECONSTMulScale(3)
MAKECONSTMulScale(4)
MAKECONSTMulScale(5)
MAKECONSTMulScale(6)
MAKECONSTMulScale(7)
MAKECONSTMulScale(8)
MAKECONSTMulScale(9)
MAKECONSTMulScale(10)
MAKECONSTMulScale(11)
MAKECONSTMulScale(12)
MAKECONSTMulScale(13)
MAKECONSTMulScale(14)
MAKECONSTMulScale(15)
MAKECONSTMulScale(16)
MAKECONSTMulScale(17)
MAKECONSTMulScale(18)
MAKECONSTMulScale(19)
MAKECONSTMulScale(20)
MAKECONSTMulScale(21)
MAKECONSTMulScale(22)
MAKECONSTMulScale(23)
MAKECONSTMulScale(24)
MAKECONSTMulScale(25)
MAKECONSTMulScale(26)
MAKECONSTMulScale(27)
MAKECONSTMulScale(28)
MAKECONSTMulScale(29)
MAKECONSTMulScale(30)
MAKECONSTMulScale(31)
MAKECONSTMulScale(32)
#undef MAKECONSTMulScale

inline SDWORD DMulScale (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD s)
{
	return (fixed_t)((((__int64) a * (__int64) b) + ((__int64) c * (__int64) d)) >> s);
}

#define MAKECONSTDMulScale(s) \
	inline SDWORD DMulScale##s (SDWORD a, SDWORD b, SDWORD c, SDWORD d) \
	{ \
		return (fixed_t)((((__int64) a * (__int64) b) + ((__int64) c * (__int64) d)) >> s); \
	}

MAKECONSTDMulScale(1)
MAKECONSTDMulScale(2)
MAKECONSTDMulScale(3)
MAKECONSTDMulScale(4)
MAKECONSTDMulScale(5)
MAKECONSTDMulScale(6)
MAKECONSTDMulScale(7)
MAKECONSTDMulScale(8)
MAKECONSTDMulScale(9)
MAKECONSTDMulScale(10)
MAKECONSTDMulScale(11)
MAKECONSTDMulScale(12)
MAKECONSTDMulScale(13)
MAKECONSTDMulScale(14)
MAKECONSTDMulScale(15)
MAKECONSTDMulScale(16)
MAKECONSTDMulScale(17)
MAKECONSTDMulScale(18)
MAKECONSTDMulScale(19)
MAKECONSTDMulScale(20)
MAKECONSTDMulScale(21)
MAKECONSTDMulScale(22)
MAKECONSTDMulScale(23)
MAKECONSTDMulScale(24)
MAKECONSTDMulScale(25)
MAKECONSTDMulScale(26)
MAKECONSTDMulScale(27)
MAKECONSTDMulScale(28)
MAKECONSTDMulScale(29)
MAKECONSTDMulScale(30)
MAKECONSTDMulScale(31)
MAKECONSTDMulScale(32)
#undef MAKCONSTDMulScale

#define MAKECONSTTMulScale(s) \
	inline SDWORD TMulScale##s (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) \
	{ \
		return (fixed_t)((((__int64) a * (__int64) b) + ((__int64) c * (__int64) d) \
				+ ((__int64) e* (__int64) f)) >> s); \
	}

MAKECONSTTMulScale(1)
MAKECONSTTMulScale(2)
MAKECONSTTMulScale(3)
MAKECONSTTMulScale(4)
MAKECONSTTMulScale(5)
MAKECONSTTMulScale(6)
MAKECONSTTMulScale(7)
MAKECONSTTMulScale(8)
MAKECONSTTMulScale(9)
MAKECONSTTMulScale(10)
MAKECONSTTMulScale(11)
MAKECONSTTMulScale(12)
MAKECONSTTMulScale(13)
MAKECONSTTMulScale(14)
MAKECONSTTMulScale(15)
MAKECONSTTMulScale(16)
MAKECONSTTMulScale(17)
MAKECONSTTMulScale(18)
MAKECONSTTMulScale(19)
MAKECONSTTMulScale(20)
MAKECONSTTMulScale(21)
MAKECONSTTMulScale(22)
MAKECONSTTMulScale(23)
MAKECONSTTMulScale(24)
MAKECONSTTMulScale(25)
MAKECONSTTMulScale(26)
MAKECONSTTMulScale(27)
MAKECONSTTMulScale(28)
MAKECONSTTMulScale(29)
MAKECONSTTMulScale(30)
MAKECONSTTMulScale(31)
MAKECONSTTMulScale(32)
#undef MAKECONSTTMulScale

inline SDWORD BoundMulScale (SDWORD a, SDWORD b, SDWORD c)
{
	return 0xBadBeef;
}

inline SDWORD DivScale (SDWORD a, SDWORD b, SDWORD c)
{
	return (fixed_t)((((__int64) a) << c) / b);
}

#define MAKECONSTDivScale(s) \
	inline SDWORD DivScale##s (SDWORD a, SDWORD b) \
	{ \
			return (fixed_t)((((__int64) a) << s) / b); \
	}

MAKECONSTDivScale(1)
MAKECONSTDivScale(2)
MAKECONSTDivScale(3)
MAKECONSTDivScale(4)
MAKECONSTDivScale(5)
MAKECONSTDivScale(6)
MAKECONSTDivScale(7)
MAKECONSTDivScale(8)
MAKECONSTDivScale(9)
MAKECONSTDivScale(10)
MAKECONSTDivScale(11)
MAKECONSTDivScale(12)
MAKECONSTDivScale(13)
MAKECONSTDivScale(14)
MAKECONSTDivScale(15)
MAKECONSTDivScale(16)
MAKECONSTDivScale(17)
MAKECONSTDivScale(18)
MAKECONSTDivScale(19)
MAKECONSTDivScale(20)
MAKECONSTDivScale(21)
MAKECONSTDivScale(22)
MAKECONSTDivScale(23)
MAKECONSTDivScale(24)
MAKECONSTDivScale(25)
MAKECONSTDivScale(26)
MAKECONSTDivScale(27)
MAKECONSTDivScale(28)
MAKECONSTDivScale(29)
MAKECONSTDivScale(30)
MAKECONSTDivScale(31)
MAKECONSTDivScale(32)
#undef MAKECONSTDivScale

inline void clearbuf (void *buff, int count, SDWORD clear)
{
	SDWORD *b2 = (SDWORD *)buff;
	while (count--)
		*b2++ = clear;
}

inline void clearbufshort (void *buff, unsigned int count, WORD clear)
{
	if (!count)
		return;
	SWORD *b2 = (SWORD *)buff;
	if ((size_t)b2 & 2)
	{
		*b2++ = clear;
		if (--count == 0)
			return;
	}
	do
	{
		*b2++ = clear;
	} while (--count);
}

inline void qinterpolatedown16 (SDWORD *out, DWORD count, SDWORD val, SDWORD delta)
{
	SDWORD odd = count;
	if ((count >>= 1) != 0)
	{
		do
		{
			SDWORD temp = val + delta;
			out[0] = val >> 16;
			val = temp + delta;
			out[1] = temp >> 16;
			out += 2;
		} while (--count);
		if (!(odd & 1))
			return;
	}
	*out = val >> 16;
}

inline void qinterpolatedown16short (short *out, DWORD count, SDWORD val, SDWORD delta)
{
	if ((size_t)out & 2)
	{ // align to dword boundary
		*out++ = (short)(val >> 16);
		count--;
		val += delta;
	}
	DWORD c2 = count>>1;
	if (c2)
	{
		DWORD *o2 = (DWORD *)out;
		do
		{
			SDWORD temp = val + delta;
			*o2++ = (temp & 0xffff0000) | ((DWORD)val >> 16);
			val = temp + delta;
		} while (--c2);
		out = (short *)o2;
	}
	if (count & 1)
	{
		*out = (short)(val >> 16);
	}
}


	//returns num/den, dmval = num%den
inline SDWORD DivMod (DWORD num, SDWORD den, SDWORD *dmval)
{
	*dmval = num%den;
	return num/den;
}

	//returns num%den, dmval = num/den
inline SDWORD ModDiv (DWORD num, SDWORD den, SDWORD *dmval)
{
	*dmval = num/den;
	return num%den;
}

inline SDWORD ksgn (SDWORD a)
{
	if (a < 0) return -1;
	else if (a > 0) return 1;
	else return 0;
}
