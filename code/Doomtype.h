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

typedef unsigned char		BYTE;
typedef unsigned short		WORD;
typedef unsigned long		DWORD;
typedef unsigned __int64	QWORD;

typedef signed char			SBYTE;
typedef signed short		SWORD;
typedef signed long			SDWORD;
typedef signed __int64		SQWORD;

typedef unsigned long		BITFIELD;

// Predefined with some OS.
#ifdef __GNUC__
#include <values.h>
#define __int64 long long
#else
#ifndef MAXCHAR
#define MAXCHAR 		((char)0x7f)
#endif
#ifndef MAXSHORT
#define MAXSHORT		((short)0x7fff)
#endif

// Max pos 32-bit int.
#ifndef MAXINT
#define MAXINT			((int)0x7fffffff)
#endif
#ifndef MAXLONG
#define MAXLONG 		((long)0x7fffffff)
#endif
#ifndef MINCHAR
#define MINCHAR 		((char)0x80)
#endif
#ifndef MINSHORT
#define MINSHORT		((short)0x8000)
#endif

// Max negative 32-bit integer.
#ifndef MININT
#define MININT			((int)0x80000000)
#endif
#ifndef MINLONG
#define MINLONG 		((long)0x80000000)
#endif
#endif


#ifndef NOASM
#ifndef USEASM
#define USEASM 1
#endif
#else
#ifdef USEASM
#undef USEASM
#endif
#endif



// [RH] This gets used all over; define it here:
int STACK_ARGS Printf (int printlevel, const char *, ...);
// [RH] Same here:
int STACK_ARGS DPrintf (const char *, ...);

// game print flags
#define	PRINT_LOW			0		// pickup messages
#define	PRINT_MEDIUM		1		// death messages
#define	PRINT_HIGH			2		// critical messages
#define	PRINT_CHAT			3		// chat messages
#define PRINT_TEAMCHAT		4		// chat messages from a teammate

#endif
