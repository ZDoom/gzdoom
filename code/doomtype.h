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

#include <limits.h>

#ifndef __BYTEBOOL__
#define __BYTEBOOL__
// [RH] Some windows includes already define this
#if !defined(_WINDEF_) && !defined(__wtypes_h__)
typedef int BOOL;
#endif
typedef unsigned char byte;
#endif

#ifdef __GNUC__
#define GCCPRINTF(stri,firstargi)	__attribute__((format(printf,stri,firstargi)))
#else
#define GCCPRINTF(a,b)
#endif


#if defined(_MSC_VER) || defined(__WATCOMC__)
#define STACK_ARGS __cdecl
#else
#define STACK_ARGS
#endif

#if defined(_MSC_VER)
#define NOVTABLE __declspec(novtable)
#else
#define NOVTABLE
#endif

#if defined(__GNUC__)
#define __int64 long long
#endif

typedef unsigned char		BYTE;
typedef signed char			SBYTE;

typedef unsigned short		WORD;
typedef signed short		SWORD;

typedef unsigned long		DWORD;
typedef signed long			SDWORD;

typedef unsigned __int64	QWORD;
typedef signed __int64		SQWORD;

// a 64-bit constant
#ifdef __GNUC__
#define CONST64(v) (v##LL)
#define UCONST64(v) (v##ULL)
#else
#define CONST64(v) ((SQWORD)(v))
#define UCONST64(v) ((QWORD)(v))
#endif

typedef DWORD				BITFIELD;

//
// Fixed point, 32bit as 16.16.
//
#define FRACBITS			16
#define FRACUNIT			(1<<FRACBITS)

typedef SDWORD				fixed_t;
typedef DWORD				dsfixed_t;		// fixedpt used by span drawer

#define FIXED_MAX			0x7fffffff
#define FIXED_MIN			0x80000000

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
int STACK_ARGS Printf (int printlevel, const char *, ...) GCCPRINTF(2,3);
int STACK_ARGS Printf (const char *, ...) GCCPRINTF(1,2);

// [RH] Same here:
int STACK_ARGS DPrintf (const char *, ...) GCCPRINTF(1,2);

// game print flags
#define	PRINT_LOW			0		// pickup messages
#define	PRINT_MEDIUM		1		// death messages
#define	PRINT_HIGH			2		// critical messages
#define	PRINT_CHAT			3		// chat messages
#define PRINT_TEAMCHAT		4		// chat messages from a teammate

#endif
