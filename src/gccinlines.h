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

inline SDWORD Scale (SDWORD a, SDWORD b, SDWORD c)
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
		 : "%cc"
			);

	return result;
}

inline SDWORD MulScale (SDWORD a, SDWORD b, SDWORD c)
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
		 : "%cc"
			);
	return result;
}

#define MAKECONSTMulScale(s) \
inline SDWORD MulScale##s (SDWORD a, SDWORD b) { return MulScale (a, b, s); }

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

inline SDWORD MulScale32 (SDWORD a, SDWORD b)
{
	SDWORD result, dummy;

	asm volatile
		("imull %3"
		 :"=d,d" (result), "=a,a" (dummy)
		 : "a,a" (a), "r,m" (b)
		 : "%cc");
	return result;
}


inline SDWORD DMulScale (SDWORD a, SDWORD b, SDWORD c, SDWORD d, int s)
{
	SDWORD f, g, h, i;
	asm volatile
		("imull %3\n\t"
		:"=a,a" (f), "=d,d" (g)
		: "%0,%0,%0" (a),  "r,m" (b)
		:"%cc");
	asm volatile
		("imull %3\n\t"
		:"=a,a" (h), "=d,d" (i)
		: "%0,%0,%0" (c),  "r,m" (d)
		:"%cc");
	asm volatile
		("addl %4,%2\n\t"
		 "adcl %5,%3\n\t"
		:"=a,a" (h), "=d,d" (i)
		: "0,0" (h), "1,1" (i), "m,r" (f), "m,r" (g)
		:"%cc");
	asm volatile
		("shrd %2,%1,%0\n\t"
		:"=a,a" (h), "=d,d" (i)
		:"I,c" (s)
		:"%cc");
	return h;
}

#define MAKECONSTDMulScale(s) \
	inline SDWORD DMulScale##s (SDWORD a, SDWORD b, SDWORD c, SDWORD d) \
	{ \
		SDWORD f, g, h, i; \
		asm volatile \
			("imull %3\n\t" \
			:"=a" (f), "=d" (g) \
			: "%0" (a),  "r" (b) \
			:"%cc"); \
		asm volatile \
			("imull %3\n\t" \
			:"=a,a" (h), "=d,d" (i) \
			: "%0,%0" (c),  "r,m" (d) \
			:"%cc"); \
		asm volatile \
			("addl %4,%2\n\t" \
			"adcl %5,%3\n\t" \
			"shrd $" #s ",%3,%2\n\t" \
			:"=a,a" (h), "=d,d" (i) \
			: "0,0" (h), "1,1" (i), "m,r" (f), "m,r" (g) \
			:"%cc"); \
		return h; \
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
#undef MAKECONSTDMulScale

inline SDWORD DMulScale32 (SDWORD a, SDWORD b, SDWORD c, SDWORD d)
{
	SDWORD f, g, h, i;
	asm volatile
		("imull %3\n\t"
		:"=a" (f), "=d" (g)
		: "%0" (a), "r" (b)
		:"%cc");
	asm volatile
		("imull %3\n\t"
		:"=a,a" (h), "=d,d" (i)
		: "%0,%0" (c),"r,m" (d)
		:"%cc");
	asm volatile
		("addl %4,%2\n\t"
		 "adcl %5,%3\n\t"
		:"=a,a" (h), "=d,d" (i)
		: "0,0" (h), "1,1" (i), "r,m" (f), "r,m" (g)
		:"%cc");
	return i;
}

#define MAKECONSTTMulScale(s) \
	inline SDWORD TMulScale##s (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD ee) \
	{ \
		SDWORD f, g, h, i; \
		asm volatile \
			("imull %3\n\t" \
			:"=a,a" (f), "=d,d" (g) \
			: "%0,%0" (a),  "r,m" (b) \
			:"%cc"); \
		asm volatile \
			("imull %3\n\t" \
			:"=a,a" (h), "=d,d" (i) \
			: "%0,%0" (c),  "r,m" (d) \
			:"%cc"); \
		asm volatile \
			("addl %2,%4\n\t" \
			"adcl %3,%5\n\t" \
			:"=r,m" (f), "=r,m" (g) \
			: "a,a" (h), "d,d" (i), "0,0" (f), "1,1" (g) \
			:"%cc"); \
		asm volatile \
			("imull %3\n\t" \
			:"=a,a" (h), "=d,d" (i) \
			: "%0,%0" (e),  "r,m" (ee) \
			:"%cc"); \
		asm volatile \
			("addl %4,%2\n\t" \
			"adcl %5,%3\n\t" \
			"shrd $" #s ",%3,%2\n\t" \
			:"=a,a" (h), "=d,d" (i) \
			: "0,0" (h), "1,1" (i), "r,m" (f), "r,m" (g) \
			:"%cc"); \
		return h; \
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

inline SDWORD TMulScale32 (SDWORD a, SDWORD b, SDWORD c, SDWORD d, SDWORD e, SDWORD ee)
{
	SDWORD f, g, h, i, j, k;
	asm volatile
		("imull %3\n\t"
		:"=a,a" (f), "=d,d" (g)
		: "%0,%0" (a),  "r,m" (b)
		:"%cc");
	asm volatile
		("imull %3\n\t"
		:"=a,a" (h), "=d,d" (i)
		: "%0,%0" (c),  "r,m" (d)
		:"%cc");
	asm volatile
		("addl %2,%4\n\t"
		 "adcl %3,%5\n\t"
		:"=m,r" (f), "=m,r" (g)
		: "a,a" (h), "d,d" (i), "0,0" (f), "1,1" (g)
		:"%cc");
	asm volatile
		("imull %3\n\t"
		:"=a,a" (j), "=d,d" (k)
		: "%0,%0" (e),  "r,m" (ee)
		:"%cc");
	asm volatile
		("addl %4,%2\n\t"
		"adcl %5,%3\n\t"
		:"=a,a" (j), "=d,d" (k)
		: "0,0" (j), "1,1" (k), "m,r" (f), "m,r" (g)
		:"%cc");
	return k;
}

inline SDWORD BoundMulScale (SDWORD a, SDWORD b, SDWORD c)
{
	SDWORD result, dummy1, dummy2;

	asm volatile
		("imull %4\n\t"
		 "movl %%edx,%2\n\t"
		 "shrdl %b5,%%edx,%%eax\n\t"
		 "sarl %b5,%%edx\n\t"
		 "xorl %%eax,%%edx\n\t"
		 "js 0f\n\t"
		 "xorl %%eax,%%edx\n\t"
		 "jz 1f\n\t"
		 "cmpl $0xffffffff,%%edx\n\t"
		 "je 1f\n\t"
		 "0:\n\t"
		 "movl %2,%%eax\n\t"
		 "sarl $31,%%eax\n\t"
		 "xorl $0x7fffffff,%%eax\n\t"
		 "1:"
		: "=a,a,a,a" (result),	// 0
		  "=d,d,d,d" (dummy1),	// 1
		  "=r,r,r,r" (dummy2)	// 2
		:  "a,a,a,a" (a),		// 3
		   "r,m,r,m" (b),		// 4
		   "c,c,I,I" (c)		// 5
		: "%cc");
	return result;
}

inline SDWORD DivScale (SDWORD a, SDWORD b, SDWORD c)
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
		: "%cc");
	return result;
}

inline SDWORD DivScale1 (SDWORD a, SDWORD b)
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
		: "%cc");
	return result;
}

#define MAKECONSTDivScale(s) \
	inline SDWORD DivScale##s (SDWORD a, SDWORD b) \
	{ \
		SDWORD result, dummy; \
		asm volatile \
			("idivl %4" \
			:"=a,a" (result), \
			 "=d,d" (dummy) \
			: "a,a" (a<<s), \
			  "d,d" (a>>(32-s)), \
			  "r,m" (b) \
			: "%cc"); \
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

inline SDWORD DivScale32 (SDWORD a, SDWORD b)
{
	SDWORD result, dummy;

	asm volatile
		("xor %%eax,%%eax\n\t"
		 "idivl %3"
		:"=&a,a" (result),
		 "=d,d" (dummy)
		: "d,d" (a),
		  "r,m" (b)
		: "%cc");
	return result;
}

inline void clearbuf (void *buff, int count, SDWORD clear)
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

inline void clearbufshort (void *buff, unsigned int count, WORD clear)
{
	SWORD *b2 = (SWORD *)buff;
	if ((size_t)b2 & 3)
	{
		*b2++ = clear;
		count--;
	}
	unsigned int c2 = count>>1;
	if (c2 > 0)
	{
		asm volatile ("rep stosl"
			:"=D" (b2), "=c" (c2)
			:"D" (b2), "c" (c2), "a" (clear|(clear<<16)));
	}
	if (count & 1)
		*b2 = clear;
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
		if ((odd & 1) == 0)
			return;
	}
	*out = val >> 16;
}

inline void qinterpolatedown16short (short *out, DWORD count, SDWORD val, SDWORD delta)
{
	if (count)
	{
		if ((size_t)out & 2)
		{ // align to dword boundary
			*out++ = (short)(val >> 16);
			if (--count == 0)
				return;
			val += delta;
		}
		SDWORD temp1, temp2;
		asm volatile
			("subl $2,%6\n\t"
			 "jc 1f\n\t"
			 "0:\n\t"
			 "movl %7,%0\n\t"
			 "addl %8,%7\n\t"
			 "shrl $16,%0\n\t"
			 "movl %7,%1\n\t"
			 "andl $0xffff0000,%1\n\t"
			 "addl %8,%7\n\t"
			 "addl %0,%1\n\t"
			 "movl %1,(%5)\n\t"
			 "addl $4,%5\n\t"
			 "subl $2,%6\n\t"
			 "jnc 0b\n\t"
			 "testl $1,%6\n\t"
			 "jz 2f\n\t"
			 "1:\n\t"
			 "sarl $16,%7\n\t"
			 "movw %w7,(%5)\n\t"
			 "2:"
			:"=&r" (temp1), "=&r" (temp2), "=&r" (out), "=&r" (val), "=&g" (count)
			:"2" (out), "4" (count), "3" (val), "r" (delta)
			:"%cc");
	}
}

	//returns num/den, dmval = num%den
inline SDWORD DivMod (DWORD num, SDWORD den, SDWORD *dmval)
{
	SDWORD remainder, result;

	asm volatile
		("xorl %%edx,%%edx\n\t"
		 "divl %3\n"
		:"=&d,&d" (remainder), "=a,a" (result)
		:"a,a" (num), "r,m" (den)
		:"%cc");
	*dmval = remainder;
	return result;
}

	//returns num%den, dmval = num/den
inline SDWORD ModDiv (DWORD num, SDWORD den, SDWORD *dmval)
{
	SDWORD remainder, result;

	asm volatile
		("xorl %%edx,%%edx\n\t"
		 "divl %3\n"
		:"=&d,&d" (remainder), "=a,a" (result)
		:"a,a" (num), "r,m" (den)
		:"%cc");
	*dmval = result;
	return remainder;
}

inline SDWORD ksgn (SDWORD a)
{
	SDWORD result, dummy;

	asm volatile
		("add %0,%0\n\t"
		 "sbb %1,%1\n\t"
		 "cmp %0,%1\n\t"
		 "adc $0,%1"
		:"=r" (dummy), "=r" (result)
		:"0" (a)
		:"%cc");
	return result;
}

inline int toint (float v)
{
	volatile QWORD result;

	asm volatile
		("fistpq %0"
		:"=m" (result)
		:"t" (v)
		:"%st");
	return result;
}

inline int quickertoint (float v)
{
	volatile int result;

	asm volatile
		("fistpl %0"
		:"=m" (result)
		:"t" (v)
		:"%st");
	return result;
}
