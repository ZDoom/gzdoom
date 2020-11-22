#ifndef __M_FIXED__
#define __M_FIXED__

#include <stdlib.h>
#include <stdint.h>
#include "basics.h"


__forceinline constexpr int32_t MulScale(int32_t a, int32_t b, int32_t shift) { return (int32_t)(((int64_t)a * b) >> shift); }
__forceinline constexpr int32_t DMulScale(int32_t a, int32_t b, int32_t c, int32_t d, int32_t shift) { return (int32_t)(((int64_t)a * b + (int64_t)c * d) >> shift); }
__forceinline constexpr int32_t DivScale(int32_t a, int32_t b, int shift) { return (int32_t)(((int64_t)a << shift) / b); }

#include "xs_Float.h"

inline fixed_t FloatToFixed(double f)
{
	return xs_Fix<16>::ToFix(f);
}

inline constexpr fixed_t IntToFixed(int32_t f)
{
	return f << FRACBITS;
}

inline constexpr double FixedToFloat(fixed_t f)
{
	return f * (1/65536.);
}

inline constexpr int32_t FixedToInt(fixed_t f)
{
	return (f + FRACUNIT/2) >> FRACBITS;
}

inline unsigned FloatToAngle(double f)
{
	return xs_CRoundToInt((f)* (0x40000000 / 90.));
}

inline constexpr double AngleToFloat(unsigned f)
{
	return f * (90. / 0x40000000);
}

inline constexpr double AngleToFloat(int f)
{
	return f * (90. / 0x40000000);
}

#define FLOAT2FIXED(f)		FloatToFixed(f)
#define FIXED2FLOAT(f)		float(FixedToFloat(f))
#define FIXED2DBL(f)		FixedToFloat(f)

#define ANGLE2DBL(f)		AngleToFloat(f)

#endif
