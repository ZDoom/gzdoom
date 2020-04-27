#ifndef __CMATH_H
#define __CMATH_H

#include "xs_Float.h"

#define USE_CUSTOM_MATH	// we want repreducably reliable results, even at the cost of performance
#define USE_FAST_MATH	// use faster table-based sin and cos variants with limited precision (sufficient for Doom gameplay)

extern"C"
{
double c_asin(double);
double c_acos(double);
double c_atan(double);
double c_atan2(double, double);
double c_sin(double);
double c_cos(double);
double c_tan(double);
double c_cot(double);
double c_sqrt(double);
double c_sinh(double);
double c_cosh(double);
double c_tanh(double);
double c_exp(double);
double c_log(double);
double c_log10(double);
double c_pow(double, double);
}


// This uses a sine table with linear interpolation
// For in-game calculations this is precise enough
// and this code is more than 10x faster than the
// Cephes sin and cos function.

struct FFastTrig
{
	static const int TBLPERIOD = 8192;
	static const int BITSHIFT = 19;
	static const int REMAINDER = (1 << BITSHIFT) - 1;
	float sinetable[2049];

	double sinq1(unsigned);
	
public:
	FFastTrig();
	double sin(unsigned);
	double cos(unsigned);
};

extern FFastTrig fasttrig;

// This must use xs_Float to guarantee proper integer wraparound.
#define DEG2BAM(f)		((unsigned)xs_CRoundToInt((f) * (0x40000000/90.)))
#define RAD2BAM(f)		((unsigned)xs_CRoundToInt((f) * (0x80000000/3.14159265358979323846)))


inline double fastcosdeg(double v)
{
	return fasttrig.cos(DEG2BAM(v));
}

inline double fastsindeg(double v)
{
	return fasttrig.sin(DEG2BAM(v));
}

inline double fastcos(double v)
{
	return fasttrig.cos(RAD2BAM(v));
}

inline double fastsin(double v)
{
	return fasttrig.sin(RAD2BAM(v));
}

// these are supposed to be local to this file.
#undef DEG2BAM
#undef RAD2BAM

inline double sindeg(double v)
{
#ifdef USE_CUSTOM_MATH
	return c_sin(v * (3.14159265358979323846 / 180.));
#else
	return sin(v * (3.14159265358979323846 / 180.));
#endif
}

inline double cosdeg(double v)
{
#ifdef USE_CUSTOM_MATH
	return c_cos(v * (3.14159265358979323846 / 180.));
#else
	return cos(v * (3.14159265358979323846 / 180.));
#endif
}


#ifndef USE_CUSTOM_MATH
#define g_asin  asin
#define g_acos  acos
#define g_atan  atan
#define g_atan2 atan2
#define g_sin	sin
#define g_cos	cos
#define g_sindeg	sindeg
#define g_cosdeg	cosdeg
#define g_tan	tan
#define g_cot	cot
#define g_sqrt  sqrt
#define g_sinh  sinh
#define g_cosh  cosh
#define g_tanh  tanh
#define g_exp	exp
#define g_log	log
#define g_log10 log10
#define g_pow	pow
#else
#define g_asin  c_asin
#define g_acos  c_acos
#define g_atan  c_atan
#define g_atan2 c_atan2
#ifndef USE_FAST_MATH
#define g_sindeg	sindeg
#define g_cosdeg	cosdeg
#define g_sin	c_sin
#define g_cos	c_cos
#else
#define g_sindeg	fastsindeg
#define g_cosdeg	fastcosdeg
#define g_sin	fastsin
#define g_cos	fastcos
#endif
#define g_tan	c_tan
#define g_cot	c_cot
#define g_sqrt  c_sqrt
#define g_sinh  c_sinh
#define g_cosh  c_cosh
#define g_tanh  c_tanh
#define g_exp	c_exp
#define g_log	c_log
#define g_log10 c_log10
#define g_pow	c_pow
#endif



#endif