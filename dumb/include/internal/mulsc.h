#ifndef INTERNAL_MULSC_H
#define INTERNAL_MULSC_H

#if !defined(_MSC_VER) || !defined(_M_IX86) || _MSC_VER >= 1800
//#define MULSC(a, b) ((int)((LONG_LONG)(a) * (b) >> 16))
//#define MULSC(a, b) ((a) * ((b) >> 2) >> 14)
#define MULSCV(a, b) ((int)((LONG_LONG)(a) * (b) >> 32))
#define MULSCA(a, b) ((int)((LONG_LONG)((a) << 4) * (b) >> 32))
#define MULSC(a, b) ((int)((LONG_LONG)((a) << 4) * ((b) << 12) >> 32))
#define MULSC16(a, b) ((int)((LONG_LONG)((a) << 12) * ((b) << 12) >> 32))
#else
/* VC++ calls __allmull and __allshr for the above math. I don't know why.
 * [Need to check if this still applies to recent versions of the compiler.] */
static __forceinline unsigned long long MULLL(int a, int b)
{
	__asm mov eax,a
	__asm imul b
}
static __forceinline int MULSCV (int a, int b)
{
#ifndef _DEBUG
	union { unsigned long long q; struct { int l, h; }; } val;
	val.q = MULLL(a,b);
	return val.h;
#else
	__asm mov eax,a
	__asm imul b
	__asm mov eax,edx
#endif
}
#define MULSCA(a, b)  MULSCV((a) << 4, b)
#define MULSC(a, b)   MULSCV((a) << 4, (b) << 12)
#define MULSC16(a, b) MULSCV((a) << 12, (b) << 12)
#endif

#endif /* INTERNAL_MULSC_H */