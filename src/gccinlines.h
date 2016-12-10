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

MAKECONSTMulScale(14)
MAKECONSTMulScale(16)
MAKECONSTMulScale(30)
MAKECONSTMulScale(32)
#undef MAKECONSTMulScale

static inline DWORD UMulScale16(DWORD a, DWORD b) { return ((QWORD)a * b) >> 16; }

#define MAKECONSTDMulScale(s) \
	static inline SDWORD DMulScale##s (SDWORD a, SDWORD b, SDWORD c, SDWORD d) \
	{ \
		return (((SQWORD)a * b) + ((SQWORD)c * d)) >> s; \
	}

MAKECONSTDMulScale(3)
MAKECONSTDMulScale(6)
MAKECONSTDMulScale(10)
MAKECONSTDMulScale(18)
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

MAKECONSTDivScale(6)
MAKECONSTDivScale(16)
MAKECONSTDivScale(21)
MAKECONSTDivScale(30)
#undef MAKECONSTDivScale

