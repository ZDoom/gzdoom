// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id: doomtype.h,v 1.2 1997/12/29 19:50:48 pekangas Exp $
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
//		Simple basic typedefs, isolated here to make it easier
//		 separating modules.
//	  
//-----------------------------------------------------------------------------


#ifndef __DOOMTYPE__
#define __DOOMTYPE__


#ifndef __BYTEBOOL__
#define __BYTEBOOL__
// [RH] Some windows includes already defines this
#if !defined(_WINDEF_) && !defined(__wtypes_h__)
typedef int BOOL;
#endif
#ifndef __cplusplus
#define false (0)
#define true (1)
#endif
typedef unsigned char byte;
#endif


#if defined(_MSC_VER) || defined(__WATCOMC__)
#define STACK_ARGS __cdecl
#else
#define STACK_ARGS
#endif

// Predefined with some OS.
#ifdef __GNUC__
#include <values.h>
typedef long long __int64;
#else
/* [Petteri] Don't redefine if we already have these */
#ifndef MAXCHAR
#define MAXCHAR 		((char)0x7f)
#define MAXSHORT		((short)0x7fff)

// Max pos 32-bit int.
#define MAXINT			((int)0x7fffffff)		
#define MAXLONG 		((long)0x7fffffff)
#define MINCHAR 		((char)0x80)
#define MINSHORT		((short)0x8000)

// Max negative 32-bit integer.
#define MININT			((int)0x80000000)		
#define MINLONG 		((long)0x80000000)
#endif
#endif


#ifndef NOASM
#ifndef USEASM
#define USEASM
#endif
#endif



// [RH] This gets used all over; define it here:
int Printf (const char *, ...);
// [RH] Same here:
int DPrintf (const char *, ...);


#endif
