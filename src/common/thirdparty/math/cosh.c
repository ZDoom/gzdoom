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

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. Neither the name of the <ORGANIZATION> nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
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

double c_cosh(double x)
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
