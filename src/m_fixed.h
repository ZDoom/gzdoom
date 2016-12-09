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

__forceinline void fillshort(void *buff, unsigned int count, WORD clear)
{
	SWORD *b2 = (SWORD *)buff;
	for (unsigned int i = 0; i != count; ++i)
	{
		b2[i] = clear;
	}
}

#include "xs_Float.h"

inline SDWORD FixedDiv (SDWORD a, SDWORD b) 
{ 
	if ((DWORD)abs(a) >> (31-16) >= (DWORD)abs (b)) 
		return (a^b)<0 ? FIXED_MIN : FIXED_MAX; 
	return DivScale16 (a, b); 
}

#define FixedMul MulScale16

inline fixed_t FloatToFixed(double f)
{
	return xs_Fix<16>::ToFix(f);
}

inline double FixedToFloat(fixed_t f)
{
	return f / 65536.;
}

inline unsigned FloatToAngle(double f)
{
	return xs_CRoundToInt((f)* (0x40000000 / 90.));
}

inline double AngleToFloat(unsigned f)
{
	return f * (90. / 0x40000000);
}

inline double AngleToFloat(int f)
{
	return f * (90. / 0x40000000);
}

#define FLOAT2FIXED(f)		FloatToFixed(f)
#define FIXED2FLOAT(f)		float(FixedToFloat(f))
#define FIXED2DBL(f)		FixedToFloat(f)

#define ANGLE2DBL(f)		AngleToFloat(f)

#endif
