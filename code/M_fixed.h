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

typedef int fixed_t;

fixed_t
#ifndef _MSC_VER
STACK_ARGS
#endif
		FixedMul_ASM			(fixed_t a, fixed_t b);
fixed_t STACK_ARGS FixedDiv_ASM	(fixed_t a, fixed_t b);
fixed_t FixedMul_C				(fixed_t a, fixed_t b);
fixed_t FixedDiv_C				(fixed_t a, fixed_t b);

#ifdef USEASM
#ifdef DJGPP

// killough 5/10/98: In djgpp, use inlined assembly for performance

__inline__ static fixed_t FixedMul(fixed_t a, fixed_t b)
{
  fixed_t result;

  asm("  imull %2 ;"
      "  shrdl $16,%%edx,%0 ;"
      : "=a,=a" (result)           // eax is always the result
      : "0,0" (a),                 // eax is also first operand
        "m,r" (b)                  // second operand can be mem or reg
      : "%edx", "%cc"              // edx and condition codes clobbered
      );

  return result;
}

// killough 5/10/98: In djgpp, use inlined assembly for performance

__inline__ static fixed_t FixedDiv(fixed_t a, fixed_t b)
{
  fixed_t result;

  if (abs(a) >> 14 >= abs(b))
    return (a^b)<0 ? MININT : MAXINT;

  asm(" movl %0, %%edx ;"
      " sall $16,%%eax ;"
      " sarl $16,%%edx ;"
      " idivl %2 ;"
      : "=a,=a" (result)    // eax is always the result
      : "0,0" (a),          // eax is also the first operand
        "m,r" (b)           // second operand can be mem or reg (not imm)
      : "%edx", "%cc"       // edx and condition codes are clobbered
      );

  return result;
}

#else // DJGPP
#define FixedMul(a,b)			FixedMul_ASM(a,b)
#define FixedDiv(a,b)			FixedDiv_ASM(a,b)
#endif // DJGPP

#else // USEASM
#define FixedMul(a,b)			FixedMul_C(a,b)
#define FixedDiv(a,b)			FixedDiv_C(a,b)
#endif

#endif
//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
