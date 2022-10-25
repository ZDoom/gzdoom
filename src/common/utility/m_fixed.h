#ifndef __M_FIXED__
#define __M_FIXED__

#include <stdlib.h>
#include <stdint.h>
#include "basics.h"


__forceinline constexpr int32_t MulScale(int32_t a, int32_t b, int32_t shift) { return (int32_t)(((int64_t)a * b) >> shift); }
__forceinline constexpr double MulScaleF(double a, double b, int32_t shift) { return (a * b) * (1. / (uint32_t(1) << shift)); }
__forceinline constexpr int32_t DMulScale(int32_t a, int32_t b, int32_t c, int32_t d, int32_t shift) { return (int32_t)(((int64_t)a * b + (int64_t)c * d) >> shift); }
__forceinline constexpr int32_t DivScale(int32_t a, int32_t b, int shift) { return (int32_t)(((int64_t)a << shift) / b); }
__forceinline constexpr int64_t DivScaleL(int64_t a, int64_t b, int shift) { return ((a << shift) / b); }

#include "xs_Float.h"

template<int b = 16>
constexpr fixed_t FloatToFixed(double f)
{
	return int(f * (1 << b));
}

constexpr fixed_t FloatToFixed(double f, int b)
{
	return int(f * (1 << b));
}

template<int b = 16>
inline constexpr fixed_t IntToFixed(int32_t f)
{
	return f << b;
}

template<int b = 16>
inline constexpr double FixedToFloat(fixed_t f)
{
	return f * (1. / (1 << b));
}

inline constexpr double FixedToFloat(fixed_t f, int b)
{
	return f * (1. / (1 << b));
}

template<int b = 16>
inline constexpr int32_t FixedToInt(fixed_t f)
{
	return (f + (1 << (b-1))) >> b;
}

inline unsigned FloatToAngle(double f)
{
	return xs_CRoundToInt((f)* (0x40000000 / 90.));
}

#define FLOAT2FIXED(f)		FloatToFixed(f)
#define FIXED2FLOAT(f)		float(FixedToFloat(f))
#define FIXED2DBL(f)		FixedToFloat(f)


#endif
