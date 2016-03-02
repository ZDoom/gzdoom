// "Build Engine & Tools" Copyright (c) 1993-1997 Ken Silverman
// Ken Silverman's official web site: "http://www.advsys.net/ken"
// See the included license file "BUILDLIC.TXT" for license info.
//
// This file is based on pragmas.h from Ken Silverman's original Build
// source code release but is meant for use with GCC instead of Watcom C.
//
// Some of the inline assembly has been turned into C code, because
// modern compilers are smart enough to produce code at least as good as
// Ken's inline assembly.
//

// I can come up with several different operand constraints for the
// following that work just fine when used all by themselves, but
// when I concatenate them together so that the compiler can choose
// between them, I get the following (but only if enough parameters
// are passed as the result of some operations instead of as
// variables/constants--e.g. DMulScale16 (a*2, b*2, c*2, d*2) instead of
// DMulScale16 (a, b, c, d)):
//
// `asm' operand requires impossible reload
//
// Why?

#include <stdlib.h>

#ifndef alloca
// MinGW does not seem to come with alloca defined.
#define alloca __builtin_alloca
#endif

static inline SDWORD Scale (SDWORD a, SDWORD b, SDWORD c)
{
	SDWORD result, dummy;

	asm volatile
		("imull %3\n\t"
		 "idivl %4"
		 : "=a,a,a,a,a,a" (result),
		   "=&d,&d,&d,&d,d,d" (dummy)
		 :   "a,a,a,a,a,a" (a),
		     "m,r,m,r,d,d" (b),
		     "r,r,m,m,r,m" (c)
		 : "cc"
			);

	return result;
}

static inline SDWORD MulScale (SDWORD a, SDWORD b, SDWORD c)
{
	SDWORD result, dummy;

	asm volatile
		("imull %3\n\t"
		 "shrdl %b4,%1,%0"
		 : "=a,a,a,a" (result),
		   "=d,d,d,d" (dummy)
		 :  "a,a,a,a" (a),
		    "m,r,m,r" (b),
		    "c,c,I,I" (c)
		 : "cc"
			);
	return result;
}

#define MAKECONSTMulScale(s) \
static inline SDWORD MulScale##s (SDWORD a, SDWORD b) { return ((SQWORD)a * b) >> s; }

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

static inline DWORD UMulScale16(DWORD a, DWORD b) { return ((QWORD)a * b) >> 16; }

#define MAKECONSTDMulScale(s) \
	static inline SDWORD DMulScale##s (SDWORD a, SDWORD b, SDWORD c, SDWORD d) \
	{ \
		return (((SQWORD)a * b) + ((SQWORD)c * d)) >> s; \
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
#undef MAKECONSTDMulScale

#define MAKECONSTTMulScale(s) \
	static inline SDWORD TMulScale##s (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD ee) \
	{ \
		return (((SQWORD)a * b) + ((SQWORD)c * d) + ((SQWORD)e * ee)) >> s; \
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

static inline SDWORD BoundMulScale (SDWORD a, SDWORD b, SDWORD c)
{
	union {
		long long big;
		struct
		{
			int l, h;
		};
	} u;
	u.big = ((long long)a * b) >> c;
	if ((u.h ^ u.l) < 0 || (unsigned int)(u.h+1) > 1) return (u.h >> 31) ^ 0x7fffffff;
	return u.l;
}

static inline SDWORD DivScale (SDWORD a, SDWORD b, SDWORD c)
{
	SDWORD result, dummy;
	SDWORD lo = a << c;
	SDWORD hi = a >> (-c);

	asm volatile
		("idivl %4"
		:"=a" (result),
		 "=d" (dummy)
		: "a" (lo),
		  "d" (hi),
		  "r" (b)
		: "cc");
	return result;
}

static inline SDWORD DivScale1 (SDWORD a, SDWORD b)
{
	SDWORD result, dummy;

	asm volatile
		("addl %%eax,%%eax\n\t"
		 "sbbl %%edx,%%edx\n\t"
		 "idivl %3"
		:"=a,a" (result),
		 "=&d,d" (dummy)
		: "a,a" (a),
		  "r,m" (b)
		: "cc");
	return result;
}

#define MAKECONSTDivScale(s) \
	static inline SDWORD DivScale##s (SDWORD a, SDWORD b) \
	{ \
		SDWORD result, dummy; \
		asm volatile \
			("idivl %4" \
			:"=a,a" (result), \
			 "=d,d" (dummy) \
			: "a,a" (a<<s), \
			  "d,d" (a>>(32-s)), \
			  "r,m" (b) \
			: "cc"); \
		return result; \
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

static inline SDWORD DivScale32 (SDWORD a, SDWORD b)
{
	SDWORD result = 0, dummy;

	asm volatile
		("idivl %3"
		:"+a,a" (result),
		 "=d,d" (dummy)
		: "d,d" (a),
		  "r,m" (b)
		: "cc");
	return result;
}

static inline void clearbuf (void *buff, int count, SDWORD clear)
{
	int dummy1, dummy2;
	asm volatile
		("rep stosl"
		:"=D" (dummy1),
		 "=c" (dummy2)
		: "D" (buff),
		  "c" (count),
		  "a" (clear)
		);
}

static inline void clearbufshort (void *buff, unsigned int count, WORD clear)
{
	asm volatile
		("shr $1,%%ecx\n\t"
		 "rep stosl\n\t"
		 "adc %%ecx,%%ecx\n\t"
		 "rep stosw"
		:"=D" (buff), "=c" (count)
		:"D" (buff), "c" (count), "a" (clear|(clear<<16))
		:"cc");
}

static inline SDWORD ksgn (SDWORD a)
{
	SDWORD result, dummy;

	asm volatile
		("add %0,%0\n\t"
		 "sbb %1,%1\n\t"
		 "cmp %0,%1\n\t"
		 "adc $0,%1"
		:"=r" (dummy), "=r" (result)
		:"0" (a)
		:"cc");
	return result;
}
