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
// $Log:$
//
// DESCRIPTION:
//		Fixed point implementation.
//
//-----------------------------------------------------------------------------


static const char
rcsid[] = "$Id: m_bbox.c,v 1.1 1997/02/03 22:45:10 b1 Exp $";

#include "doomtype.h"
#include "i_system.h"

#ifdef __GNUG__
#pragma implementation "m_fixed.h"
#endif
#include "m_fixed.h"


#define abs(x) (((x)<0)?(-(x)):(x))


#if !defined(USEASM) || !defined(_M_IX86) || !defined(_MSC_VER)

// C routines

fixed_t FixedMul (fixed_t a, fixed_t b)
{
	return (fixed_t)(((__int64) a * (__int64) b) >> FRACBITS);
}


fixed_t FixedDiv (fixed_t a, fixed_t b)
{
	if ((abs (a) >> 14) >= abs(b))
		return (a^b)<0 ? MININT : MAXINT;

	{
#if 0
		long long c;
		c = ((long long)a<<16) / ((long long)b);
		return (fixed_t) c;
#endif

		double c;

		c = ((double)a) / ((double)b) * FRACUNIT;

		if (c >= 2147483648.0 || c < -2147483648.0)
			I_Error("FixedDiv: divide by zero");
		return (fixed_t) c;
	}
}

#else

// Optimized (?) assembly versions

#pragma warning(disable: 4035)

fixed_t FixedMul (fixed_t a, fixed_t b)
{
	__asm {
		mov		eax,a
		imul	b
		shrd	eax,edx,16
	}
}

fixed_t FixedDiv (fixed_t a, fixed_t b)
{
	if ((abs (a) >> 14) >= abs(b))
		return (a^b)<0 ? MININT : MAXINT;

	__asm {
		mov		eax,a			// u  (eax = aaaaAAAA)
		mov		edx,a			// v  (edx = aaaaAAAA)
		shl		eax,16			// u  (eax = AAAA0000)
		sar		edx,16			// v  (edx = ----aaaa)
		idiv	b				// np
	}
}

#pragma warning(default: 4035)

#endif
