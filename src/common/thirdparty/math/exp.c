/*							exp.c
 *
 *	Exponential function
 *
 *
 *
 * SYNOPSIS:
 *
 * double x, y, exp();
 *
 * y = exp( x );
 *
 *
 *
 * DESCRIPTION:
 *
 * Returns e (2.71828...) raised to the x power.
 *
 * Range reduction is accomplished by separating the argument
 * into an integer k and fraction f such that
 *
 *     x    k  f
 *    e  = 2  e.
 *
 * A Pade' form  1 + 2x P(x**2)/( Q(x**2) - P(x**2) )
 * of degree 2/3 is used to approximate exp(f) in the basic
 * interval [-0.5, 0.5].
 *
 *
 * ACCURACY:
 *
 *                      Relative error:
 * arithmetic   domain     # trials      peak         rms
 *    DEC       0, MAXLOG   38000       3.0e-17     6.2e-18
 *    IEEE      +- 708      40000       2.0e-16     5.6e-17
 *
 *
 * Error amplification in the exponential function can be
 * a serious matter.  The error propagation involves
 * exp( X(1+delta) ) = exp(X) ( 1 + X*delta + ... ),
 * which shows that a 1 lsb error in representing X produces
 * a relative error of X times 1 lsb in the function.
 * While the routine gives an accurate result for arguments
 * that are exactly represented by a double precision
 * computer number, the result contains amplified roundoff
 * error for large arguments not exactly represented.
 *
 *
 * ERROR MESSAGES:
 *
 *   message         condition      value returned
 * exp underflow    x < MINLOG         0.0
 * exp overflow     x > MAXLOG         MAXNUM
 *
 */

/*
Cephes Math Library Release 2.2:  January, 1991
Copyright 1984, 1991 by Stephen L. Moshier

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

Direct inquiries to 30 Frost Street, Cambridge, MA 02140
*/


/*	Exponential function	*/

#include "mconf.h"
static char fname[] = {"exp"};

#ifdef UNK

static double P[] = {
 1.26177193074810590878E-4,
 3.02994407707441961300E-2,
 9.99999999999999999910E-1,
};
static double Q[] = {
 3.00198505138664455042E-6,
 2.52448340349684104192E-3,
 2.27265548208155028766E-1,
 2.00000000000000000009E0,
};
static double C1 = 6.93145751953125E-1;
static double C2 = 1.42860682030941723212E-6;
#endif

#ifdef DEC
static short P[] = {
0035004,0047156,0127442,0057502,
0036770,0033210,0063121,0061764,
0040200,0000000,0000000,0000000,
};
static short Q[] = {
0033511,0072665,0160662,0176377,
0036045,0070715,0124105,0132777,
0037550,0134114,0142077,0001637,
0040400,0000000,0000000,0000000,
};
static short sc1[] = {0040061,0071000,0000000,0000000};
#define C1 (*(double *)sc1)
static short sc2[] = {0033277,0137216,0075715,0057117};
#define C2 (*(double *)sc2)
#endif

#ifdef IBMPC
static short P[] = {
0x4be8,0xd5e4,0x89cd,0x3f20,
0x2c7e,0x0cca,0x06d1,0x3f9f,
0x0000,0x0000,0x0000,0x3ff0,
};
static short Q[] = {
0x5fa0,0xbc36,0x2eb6,0x3ec9,
0xb6c0,0xb508,0xae39,0x3f64,
0xe074,0x9887,0x1709,0x3fcd,
0x0000,0x0000,0x0000,0x4000,
};
static short sc1[] = {0x0000,0x0000,0x2e40,0x3fe6};
#define C1 (*(double *)sc1)
static short sc2[] = {0xabca,0xcf79,0xf7d1,0x3eb7};
#define C2 (*(double *)sc2)
#endif

#ifdef MIEEE
static short P[] = {
0x3f20,0x89cd,0xd5e4,0x4be8,
0x3f9f,0x06d1,0x0cca,0x2c7e,
0x3ff0,0x0000,0x0000,0x0000,
};
static short Q[] = {
0x3ec9,0x2eb6,0xbc36,0x5fa0,
0x3f64,0xae39,0xb508,0xb6c0,
0x3fcd,0x1709,0x9887,0xe074,
0x4000,0x0000,0x0000,0x0000,
};
static short sc1[] = {0x3fe6,0x2e40,0x0000,0x0000};
#define C1 (*(double *)sc1)
static short sc2[] = {0x3eb7,0xf7d1,0xcf79,0xabca};
#define C2 (*(double *)sc2)
#endif

extern double LOGE2, LOG2E, MAXLOG, MINLOG, MAXNUM;

double c_exp(double x)
{
double px, xx;
int n;
double polevl(double, void *, int), floor(double), ldexp(double, int);

if( x > MAXLOG)
	{
	mtherr( fname, OVERFLOW );
	return( MAXNUM );
	}

if( x < MINLOG )
	{
	mtherr( fname, UNDERFLOW );
	return(0.0);
	}

/* Express e**x = e**g 2**n
 *   = e**g e**( n loge(2) )
 *   = e**( g + n loge(2) )
 */
px = floor( LOG2E * x + 0.5 ); /* floor() truncates toward -infinity. */
n = (int)px;
x -= px * C1;
x -= px * C2;

/* rational approximation for exponential
 * of the fractional part:
 * e**x = 1 + 2x P(x**2)/( Q(x**2) - P(x**2) )
 */
xx = x * x;
px = x * polevl( xx, P, 2 );
x =  px/( polevl( xx, Q, 3 ) - px );
x = 1.0 + ldexp( x, 1 );

/* multiply by power of 2 */
x = ldexp( x, n );
return(x);
}
