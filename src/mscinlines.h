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

#pragma warning (disable: 4035)

#include <string.h>
#include <stddef.h>

__forceinline SDWORD Scale (SDWORD a, SDWORD b, SDWORD c)
{
	__asm mov eax,a
	__asm mov ecx,c
	__asm imul b
	__asm idiv ecx
}

__forceinline SDWORD MulScale (SDWORD a, SDWORD b, SDWORD c)
{
	__asm mov eax,a
	__asm mov ecx,c
	__asm imul b
	__asm shrd eax,edx,cl
}

#define MAKECONSTMulScale(s) \
	__forceinline SDWORD MulScale##s (SDWORD a, SDWORD b) \
	{ \
		__asm mov eax,a \
		__asm imul b \
		__asm shrd eax,edx,s \
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
#undef MAKECONSTMulScale

__forceinline SDWORD MulScale32 (SDWORD a, SDWORD b)
{
	__asm mov eax,a
	__asm imul b
	__asm mov eax,edx
}

__forceinline SDWORD DMulScale (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD s)
{
	__asm mov eax,a
	__asm imul b
	__asm mov ebx,eax
	__asm mov eax,c
	__asm mov esi,edx
	__asm mov ecx,s
	__asm imul d
	__asm add eax,ebx
	__asm adc edx,esi
	__asm shrd eax,edx,cl
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

#define MAKECONSTTMulScale(s) \
	__forceinline SDWORD TMulScale##s (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f) \
	{ \
		__asm mov eax,a \
		__asm imul b \
		__asm mov ebx,eax \
		__asm mov eax,d \
		__asm mov ecx,edx \
		__asm imul c \
		__asm add ebx,eax \
		__asm mov eax,e \
		__asm adc ecx,edx \
		__asm imul f \
		__asm add eax,ebx \
		__asm adc edx,ecx \
		__asm shrd eax,edx,s \
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
#undef MAKECONSTTMulScale

__forceinline SDWORD TMulScale32 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD f)
{
	__asm mov eax,a
	__asm imul b
	__asm mov ebx,eax
	__asm mov eax,c
	__asm mov ecx,edx
	__asm imul d
	__asm add ebx,eax
	__asm mov eax,e
	__asm adc ecx,edx
	__asm imul f
	__asm add eax,ebx
	__asm adc edx,ecx
	__asm mov eax,edx
}

__forceinline SDWORD BoundMulScale (SDWORD a, SDWORD b, SDWORD c)
{
	__asm mov eax,a
	__asm imul b
	__asm mov ebx,edx
	__asm mov ecx,c
	__asm shrd eax,edx,cl
	__asm sar edx,cl
	__asm xor edx,eax
	__asm js checkit
	__asm xor edx,eax
	__asm jz skipboundit
	__asm cmp edx,0xffffffff
	__asm je skipboundit
checkit:
	__asm mov eax,ebx
	__asm sar eax,31
	__asm xor eax,0x7fffffff
skipboundit:
	;
}

__forceinline SDWORD DivScale (SDWORD a, SDWORD b, SDWORD c)
{
	__asm mov eax,a
	__asm mov ecx,c
	__asm shl eax,cl
	__asm mov edx,a
	__asm neg cl
	__asm sar edx,cl
	__asm idiv b
}

__forceinline SDWORD DivScale1 (SDWORD a, SDWORD b)
{
	__asm mov eax,a
	__asm add eax,eax
	__asm sbb edx,edx
	__asm idiv b
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
#undef MAKECONSTDivScale

__forceinline SDWORD DivScale32 (SDWORD a, SDWORD b)
{
	__asm mov edx,a
	__asm xor eax,eax
	__asm idiv b
}

__forceinline void clearbuf (void *buff, int count, SDWORD clear)
{
	SDWORD *b2 = (SDWORD *)buff;
	while (count--)
		*b2++ = clear;
}

__forceinline void clearbufshort (void *buff, unsigned int count, WORD clear)
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

__forceinline void qinterpolatedown16 (SDWORD *out, DWORD count, SDWORD val, SDWORD delta)
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

__forceinline void qinterpolatedown16short (short *out, DWORD count, SDWORD val, SDWORD delta)
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
__forceinline SDWORD DivMod (DWORD num, SDWORD den, SDWORD *dmval)
{
	__asm mov eax,num
	__asm xor edx,edx
	__asm div den
	__asm mov ecx,dmval
	__asm mov [ecx],edx
}

	//returns num%den, dmval = num/den
__forceinline SDWORD ModDiv (DWORD num, SDWORD den, SDWORD *dmval)
{
	__asm mov eax,num
	__asm xor edx,edx
	__asm div den
	__asm mov ecx,dmval
	__asm mov [ecx],eax
	__asm mov eax,edx
}

__forceinline SDWORD ksgn (SDWORD a)
{
	__asm mov edx,a
	__asm add edx,edx
	__asm sbb eax,eax
	__asm cmp eax,edx
	__asm adc eax,0
}

__forceinline int toint (float v)
{
	QWORD res;
	__asm fld v;
	__asm fistp res;
	return (int)res;
}

__forceinline int quickertoint (float v)
{
	DWORD res;
	__asm fld v;
	__asm fistp res;
	return (int)res;
}

#pragma warning (default: 4035)
