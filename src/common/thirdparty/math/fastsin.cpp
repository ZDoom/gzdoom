/*
** fastsin.cpp
** a table/linear interpolation-based sine function that is both 
** precise and fast enough for most purposes.
**
**---------------------------------------------------------------------------
** Copyright 2015 Christoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include <math.h>
#include "cmath.h"


#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

FFastTrig fasttrig;

FFastTrig::FFastTrig()
{
	const double pimul = M_PI * 2 / TBLPERIOD;

	for (int i = 0; i < 2049; i++)
	{
		sinetable[i] = (float)c_sin(i*pimul);
	}
}

__forceinline double FFastTrig::sinq1(unsigned bangle)
{
	unsigned int index = bangle >> BITSHIFT;

	if ((bangle &= (REMAINDER)) == 0)	// This is to avoid precision problems at 180°
	{
		return double(sinetable[index]);
	}
	else
	{
		return (double(sinetable[index]) * (REMAINDER - bangle) + double(sinetable[index + 1]) * bangle) * (1. / REMAINDER);
	}
}

double FFastTrig::sin(unsigned bangle)
{
	switch (bangle & 0xc0000000)
	{
	default:
		return sinq1(bangle);

	case 0x40000000:
		return sinq1(0x80000000 - bangle);

	case 0x80000000:
		return -sinq1(bangle - 0x80000000);

	case 0xc0000000:
		return -sinq1(0 - bangle);
	}
}


double FFastTrig::cos(unsigned bangle)
{
	switch (bangle & 0xc0000000)
	{
	default:
		return sinq1(0x40000000 - bangle);

	case 0x40000000:
		return -sinq1(bangle - 0x40000000);

	case 0x80000000:
		return -sinq1(0xc0000000 - bangle);

	case 0xc0000000:
		return sinq1(bangle - 0xc0000000);
	}
}

