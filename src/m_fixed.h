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

extern "C" fixed_t FixedMul_ASM				(fixed_t a, fixed_t b);
extern "C" fixed_t STACK_ARGS FixedDiv_ASM	(fixed_t a, fixed_t b);
fixed_t FixedMul_C				(fixed_t a, fixed_t b);
fixed_t FixedDiv_C				(fixed_t a, fixed_t b);

#if defined(__GNUG__)
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
			return (a^b)<0 ? FIXED_MIN : FIXED_MAX; \
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
#undef MAKESAFEDIVSCALE

inline SDWORD SafeDivScale31 (SDWORD a, SDWORD b)
{
	if (abs(a) >= abs (b))
		return (a^b)<0 ? FIXED_MIN : FIXED_MAX;
	return DivScale32 (a, b);
}

inline SDWORD SafeDivScale32 (SDWORD a, SDWORD b)
{
	if (abs(a) >= abs (b) >> 1)
		return (a^b)<0 ? FIXED_MIN : FIXED_MAX;
	return DivScale32 (a, b);
}

#define FixedMul MulScale16
#define FixedDiv SafeDivScale16

#endif
