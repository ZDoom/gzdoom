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
//		Fixed point arithemtics, implementation.
//
//-----------------------------------------------------------------------------


#ifndef __M_FIXED__
#define __M_FIXED__

#include "doomtype.h"

//
// Fixed point, 32bit as 16.16.
//
#define FRACBITS				16
#define FRACUNIT				(1<<FRACBITS)

typedef int fixed_t;			// fixed 16.16
typedef int fixed8_24;		// fixed 8.24
typedef unsigned int dsfixed_t;		// fixed for span drawer, variable parts

extern "C" fixed_t FixedMul_ASM				(fixed_t a, fixed_t b);
extern "C" fixed_t STACK_ARGS FixedDiv_ASM	(fixed_t a, fixed_t b);
fixed_t FixedMul_C				(fixed_t a, fixed_t b);
fixed_t FixedDiv_C				(fixed_t a, fixed_t b);

#ifdef USEASM
#if defined(__GNUG__)

// killough 5/10/98: In djgpp, use inlined assembly for performance

__inline__ static fixed_t FixedMul(fixed_t a, fixed_t b)
{
  fixed_t result;

  asm("\timull %2\n"
      "\tshrdl $16,%%edx,%0"
      : "=a,a" (result)           // eax is always the result
      : "0,0" (a),                 // eax is also first operand
        "m,r" (b)                  // second operand can be mem or reg
      : "%edx", "%cc"              // edx and condition codes clobbered
      );

  return result;
}

__inline__ static fixed_t FixedDiv(fixed_t a, fixed_t b)
{
  fixed_t result;

  if (abs(a) >> 14 >= abs(b))
    return (a^b)<0 ? MININT : MAXINT;

  asm(" movl %0, %%edx ;"
      " sall $16,%%eax ;"
      " sarl $16,%%edx ;"
      " idivl %2 ;"
      : "=a,a" (result)     // eax is always the result
      : "0,0" (a),          // eax is also the first operand
        "m,r" (b)           // second operand can be mem or reg (not imm)
      : "%edx", "%cc"       // edx and condition codes are clobbered
      );

  return result;
}

#elif defined(_MSC_VER)

__inline fixed_t FixedMul (fixed_t a, fixed_t b)
{
	fixed_t result;
	__asm {
		mov		eax,[a]
		imul	[b]
		shrd	eax,edx,16
		mov		[result],eax
	}
	return result;
}

#if 1
// Inlining FixedDiv with VC++ generates too many bad optimizations.
#define FixedDiv(a,b)			FixedDiv_ASM(a,b)
#else
__inline fixed_t FixedDiv (fixed_t a, fixed_t b)
{
	if (abs(a) >> 14 >= abs(b))
		return (a^b)<0 ? MININT : MAXINT;
	else
	{
		fixed_t result;

		__asm {
			mov		eax,[a]
			mov		edx,[a]
			sar		edx,16
			shl		eax,16
			idiv	[b]
			mov		[result],eax
		}
		return result;
	}
}
#endif

#else

#define FixedMul(a,b)			FixedMul_ASM(a,b)
#define FixedDiv(a,b)			FixedDiv_ASM(a,b)

#endif

#else // !USEASM
#define FixedMul(a,b)			FixedMul_C(a,b)
#define FixedDiv(a,b)			FixedDiv_C(a,b)
#endif

#endif
//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
