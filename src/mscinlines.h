// "Build Engine & Tools" Copyright (c) 1993-1997 Ken Silverman
// Ken Silverman's official web site: "http://www.advsys.net/ken"
// See the included license file "BUILDLIC.TXT" for license info.
//
// This file is based on pragmas.h from Ken Silverman's original Build
// source code release but is meant for use with Visual C++ instead of
// Watcom C.
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

#pragma warning (disable: 4035)

__forceinline SDWORD Scale (SDWORD a, SDWORD b, SDWORD c)
{
	__asm mov eax,a
	__asm imul b
	__asm idiv c
}

#define MAKECONSTMulScale(s) \
	__forceinline SDWORD MulScale##s (SDWORD a, SDWORD b) \
	{ \
		__asm mov eax,a \
		__asm imul b \
		__asm shrd eax,edx,s \
	}
MAKECONSTMulScale(14)
MAKECONSTMulScale(16)
MAKECONSTMulScale(30)
#undef MAKECONSTMulScale

__forceinline SDWORD MulScale32 (SDWORD a, SDWORD b)
{
	__asm mov eax,a
	__asm imul b
	__asm mov eax,edx
}

__forceinline DWORD UMulScale16(DWORD a, DWORD b)
{
	__asm mov eax,a
	__asm mul b
	__asm shrd eax,edx,16
}

#define MAKECONSTDMulScale(s) \
	__forceinline SDWORD DMulScale##s (SDWORD a, SDWORD b, SDWORD c, SDWORD d) \
	{ \
		__asm mov eax,a \
		__asm imul b \
		__asm mov ebx,eax \
		__asm mov eax,c \
		__asm mov esi,edx \
		__asm imul d \
		__asm add eax,ebx \
		__asm adc edx,esi \
		__asm shrd eax,edx,s \
	}

MAKECONSTDMulScale(3)
MAKECONSTDMulScale(6)
MAKECONSTDMulScale(10)
MAKECONSTDMulScale(18)
#undef MAKCONSTDMulScale

__forceinline SDWORD DMulScale32 (SDWORD a, SDWORD b, SDWORD c, SDWORD d)
{
	__asm mov eax,a
	__asm imul b
	__asm mov ebx,eax
	__asm mov eax,c
	__asm mov esi,edx
	__asm imul d
	__asm add eax,ebx
	__asm adc edx,esi
	__asm mov eax,edx
}

#define MAKECONSTDivScale(s) \
	__forceinline SDWORD DivScale##s (SDWORD a, SDWORD b) \
	{ \
		__asm mov edx,a \
		__asm sar edx,32-s \
		__asm mov eax,a \
		__asm shl eax,s \
		__asm idiv b \
	}

MAKECONSTDivScale(6)
MAKECONSTDivScale(16)
MAKECONSTDivScale(21)
MAKECONSTDivScale(30)
#undef MAKECONSTDivScale

#pragma warning (default: 4035)
