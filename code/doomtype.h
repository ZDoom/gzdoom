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
// Fixed to use builtin bool type with C++.
#ifdef __cplusplus
typedef bool boolean;
#else
typedef enum {false, true} boolean;
#endif
typedef unsigned char byte;
#endif


// Predefined with some OS.
#ifdef LINUX
#include <values.h>
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
#define USEASM
#endif




#endif
//-----------------------------------------------------------------------------
//
// $Log: doomtype.h,v $
// Revision 1.2  1997/12/29 19:50:48  pekangas
// Ported to WinNT/95 environment using Watcom C 10.6.
// Everything expect joystick support is implemented, but networking is
// untested. Crashes way too often with problems in FixedDiv().
// Compiles with no warnings at warning level 3, uses MIDAS 1.1.1.
//
// Revision 1.1.1.1  1997/12/28 12:59:02  pekangas
// Initial DOOM source release from id Software
//
//
//-----------------------------------------------------------------------------
