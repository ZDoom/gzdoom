/*							cosh.c
 *
 *	Hyperbolic cosine
 *
 *
 *
 * SYNOPSIS:
 *
 * double x, y, cosh();
 *
 * y = cosh( x );
 *
 *
 *
 * DESCRIPTION:
 *
 * Returns hyperbolic cosine of argument in the range MINLOG to
 * MAXLOG.
 *
 * cosh(x)  =  ( exp(x) + exp(-x) )/2.
 *
 *
 *
 * ACCURACY:
 *
 *                      Relative error:
 * arithmetic   domain     # trials      peak         rms
 *    DEC       +- 88       50000       4.0e-17     7.7e-18
 *    IEEE     +-MAXLOG     30000       2.6e-16     5.7e-17
 *
 *
 * ERROR MESSAGES:
 *
 *   message         condition      value returned
 * cosh overflow    |x| > MAXLOG       MAXNUM
 *
 *
 */

/*							cosh.c */

/*
Cephes Math Library Release 2.8:  June, 2000
Copyright 1985, 1995, 2000 by Stephen L. Moshier
*/

#include "mconf.h"
#ifdef ANSIPROT
extern double c_exp ( double );
extern int isnan ( double );
extern int isfinite ( double );
#else
double c_exp();
int isnan(), isfinite();
#endif
extern double MAXLOG, INFINITY, LOGE2;

double c_cosh(x)
double x;
{
double y;

#ifdef NANS
if( isnan(x) )
	return(x);
#endif
if( x < 0 )
	x = -x;
if( x > (MAXLOG + LOGE2) )
	{
	mtherr( "cosh", OVERFLOW );
	return( INFINITY );
	}	
if( x >= (MAXLOG - LOGE2) )
	{
	y = c_exp(0.5 * x);
	y = (0.5 * y) * y;
	return(y);
	}
y = c_exp(x);
y = 0.5 * (y + 1.0 / y);
return( y );
}
