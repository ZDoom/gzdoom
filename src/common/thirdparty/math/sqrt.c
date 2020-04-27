/*							_sqrt.c
 *
 *	Square root
 *
 *
 *
 * SYNOPSIS:
 *
 * double x, y, _sqrt();
 *
 * y = _sqrt( x );
 *
 *
 *
 * DESCRIPTION:
 *
 * Returns the square root of x.
 *
 * Range reduction involves isolating the power of two of the
 * argument and using a polynomial approximation to obtain
 * a rough value for the square root.  Then Heron's iteration
 * is used three times to converge to an accurate value.
 *
 *
 *
 * ACCURACY:
 *
 *
 *                      Relative error:
 * arithmetic   domain     # trials      peak         rms
 *    DEC       0, 10       60000       2.1e-17     7.9e-18
 *    IEEE      0,1.7e308   30000       1.7e-16     6.3e-17
 *
 *
 * ERROR MESSAGES:
 *
 *   message         condition      value returned
 * _sqrt domain        x < 0            0.0
 *
 */

/*
Cephes Math Library Release 2.8:  June, 2000
Copyright 1984, 1987, 1988, 2000 by Stephen L. Moshier

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
extern double frexp ( double, int * );
extern double ldexp ( double, int );
#else
double frexp(), ldexp();
#endif
extern double SQRT2;  /*  _sqrt2 = 1.41421356237309504880 */

double c_sqrt(x)
double x;
{
int e;
#ifndef UNK
short *q;
#endif
double z, w;

if( x <= 0.0 )
	{
	if( x < 0.0 )
		mtherr( "_sqrt", DOMAIN );
	return( 0.0 );
	}
w = x;
/* separate exponent and significand */
#ifdef UNK
z = frexp( x, &e );
#endif
#ifdef DEC
q = (short *)&x;
e = ((*q >> 7) & 0377) - 0200;
*q &= 0177;
*q |= 040000;
z = x;
#endif

/* Note, frexp and ldexp are used in order to
 * handle denormal numbers properly.
 */
#ifdef IBMPC
z = frexp( x, &e );
q = (short *)&x;
q += 3;
/*
e = ((*q >> 4) & 0x0fff) - 0x3fe;
*q &= 0x000f;
*q |= 0x3fe0;
z = x;
*/
#endif
#ifdef MIEEE
z = frexp( x, &e );
q = (short *)&x;
/*
e = ((*q >> 4) & 0x0fff) - 0x3fe;
*q &= 0x000f;
*q |= 0x3fe0;
z = x;
*/
#endif

/* approximate square root of number between 0.5 and 1
 * relative error of approximation = 7.47e-3
 */
x = 4.173075996388649989089E-1 + 5.9016206709064458299663E-1 * z;

/* adjust for odd powers of 2 */
if( (e & 1) != 0 )
	x *= SQRT2;

/* re-insert exponent */
#ifdef UNK
x = ldexp( x, (e >> 1) );
#endif
#ifdef DEC
*q += ((e >> 1) & 0377) << 7;
*q &= 077777;
#endif
#ifdef IBMPC
x = ldexp( x, (e >> 1) );
/*
*q += ((e >>1) & 0x7ff) << 4;
*q &= 077777;
*/
#endif
#ifdef MIEEE
x = ldexp( x, (e >> 1) );
/*
*q += ((e >>1) & 0x7ff) << 4;
*q &= 077777;
*/
#endif

/* Newton iterations: */
#ifdef UNK
x = 0.5*(x + w/x);
x = 0.5*(x + w/x);
x = 0.5*(x + w/x);
#endif

/* Note, assume the square root cannot be denormal,
 * so it is safe to use integer exponent operations here.
 */
#ifdef DEC
x += w/x;
*q -= 0200;
x += w/x;
*q -= 0200;
x += w/x;
*q -= 0200;
#endif
#ifdef IBMPC
x += w/x;
*q -= 0x10;
x += w/x;
*q -= 0x10;
x += w/x;
*q -= 0x10;
#endif
#ifdef MIEEE
x += w/x;
*q -= 0x10;
x += w/x;
*q -= 0x10;
x += w/x;
*q -= 0x10;
#endif

return(x);
}
