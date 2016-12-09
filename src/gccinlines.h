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

