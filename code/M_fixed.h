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



//
// Fixed point, 32bit as 16.16.
//
#define FRACBITS				16
#define FRACUNIT				(1<<FRACBITS)

typedef int fixed_t;

fixed_t FixedMul_ASM			(fixed_t a, fixed_t b);
fixed_t FixedDiv_ASM			(fixed_t a, fixed_t b);
fixed_t FixedMul_C				(fixed_t a, fixed_t b);
fixed_t FixedDiv_C				(fixed_t a, fixed_t b);

#ifdef	USEASM
#define FixedMul(a,b)			FixedMul_ASM(a,b)
#define FixedDiv(a,b)			FixedDiv_ASM(a,b)
#else
#define FixedMul(a,b)			FixedMul_C(a,b)
#define FixedDiv(a,b)			FixedDiv_C(a,b)
#endif

#endif
//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
