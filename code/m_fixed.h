// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// DESCRIPTION:
//		Fixed point arithmetics, implementation.
//
//-----------------------------------------------------------------------------


#ifndef __M_FIXED__
#define __M_FIXED__

#include <stdlib.h>
#include "doomtype.h"

//
// Fixed point, 32bit as 16.16.
//
#define FRACBITS				16
#define FRACUNIT				(1<<FRACBITS)

typedef int fixed_t;		// fixed 16.16
typedef unsigned int dsfixed_t;	// fixedpt used by span drawer

extern "C" fixed_t FixedMul_ASM				(fixed_t a, fixed_t b);
extern "C" fixed_t STACK_ARGS FixedDiv_ASM	(fixed_t a, fixed_t b);
fixed_t FixedMul_C				(fixed_t a, fixed_t b);
fixed_t FixedDiv_C				(fixed_t a, fixed_t b);

#if defined(ALPHA)
#include "alphainlines.h"
#elif defined(__GNUG__)
#include "gccinlines.h"
#elif defined(_MSC_VER)
#include "mscinlines.h"
#else
#include "basicinlines.h"
#endif

#define MAKESAFEDIVSCALE(x) \
	inline SDWORD SafeDivScale##x (SDWORD a, SDWORD b) \
	{ \
		if (abs(a) >> (31-x) >= abs (b)) \
			return (a^b)<0 ? MININT : MAXINT; \
		return DivScale##x (a, b); \
	}

MAKESAFEDIVSCALE(1)
MAKESAFEDIVSCALE(2)
MAKESAFEDIVSCALE(3)
MAKESAFEDIVSCALE(4)
MAKESAFEDIVSCALE(5)
MAKESAFEDIVSCALE(6)
MAKESAFEDIVSCALE(7)
MAKESAFEDIVSCALE(8)
MAKESAFEDIVSCALE(9)
MAKESAFEDIVSCALE(10)
MAKESAFEDIVSCALE(11)
MAKESAFEDIVSCALE(12)
MAKESAFEDIVSCALE(13)
MAKESAFEDIVSCALE(14)
MAKESAFEDIVSCALE(15)
MAKESAFEDIVSCALE(16)
MAKESAFEDIVSCALE(17)
MAKESAFEDIVSCALE(18)
MAKESAFEDIVSCALE(19)
MAKESAFEDIVSCALE(20)
MAKESAFEDIVSCALE(21)
MAKESAFEDIVSCALE(22)
MAKESAFEDIVSCALE(23)
MAKESAFEDIVSCALE(24)
MAKESAFEDIVSCALE(25)
MAKESAFEDIVSCALE(26)
MAKESAFEDIVSCALE(27)
MAKESAFEDIVSCALE(28)
MAKESAFEDIVSCALE(29)
MAKESAFEDIVSCALE(30)
MAKESAFEDIVSCALE(31)
MAKESAFEDIVSCALE(32)
#undef MAKESAFEDIVSCALE

inline fixed_t FixedMul (fixed_t a, fixed_t b)
{
	return MulScale16 (a, b);
}

inline fixed_t FixedDiv (fixed_t a, fixed_t b)
{
	return SafeDivScale16 (a, b);
}

#endif