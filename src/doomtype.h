// Emacs style mode select       -*- C++ -*- 
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
//              Simple basic typedefs, isolated here to make it easier
//               separating modules.
//        
//-----------------------------------------------------------------------------


#ifndef __DOOMTYPE__
#define __DOOMTYPE__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef _MSC_VER
// VC++ does not define PATH_MAX, but the Windows headers do define MAX_PATH.
// However, we want to avoid including the Windows headers in most of the
// source files, so we can't use it. So define PATH_MAX to be what MAX_PATH
// currently is:
#define PATH_MAX 260
#endif

#include <limits.h>
#include "zstring.h"
#include "name.h"

// Since this file is included by everything, it seems an appropriate place
// to check the NOASM/USEASM macros.
#if (!defined(_M_IX86) && !defined(__i386__)) || defined(__APPLE__)
// The assembly code requires an x86 processor.
// And needs to be tweaked for Mach-O before enabled on Macs.
#define NOASM
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

#ifdef _MSC_VER
typedef __int8					SBYTE;
typedef unsigned __int8			BYTE;
typedef __int16					SWORD;
typedef unsigned __int16		WORD;
typedef __int32					SDWORD;
typedef unsigned __int32		uint32;
typedef __int64					SQWORD;
typedef unsigned __int64		QWORD;
#else
#include <stdint.h>

typedef int8_t					SBYTE;
typedef uint8_t					BYTE;
typedef int16_t					SWORD;
typedef uint16_t				WORD;
typedef int32_t					SDWORD;
typedef uint32_t				uint32;
typedef int64_t					SQWORD;
typedef uint64_t				QWORD;
#endif

// windef.h, included by windows.h, has its own incompatible definition
// of DWORD as a long. In files that mix Doom and Windows code, you
// must define USE_WINDOWS_DWORD before including doomtype.h so that
// you are aware that those files have a different DWORD than the rest
// of the source.

#ifndef USE_WINDOWS_DWORD
typedef uint32					DWORD;
#endif
typedef uint32					BITFIELD;
typedef int						INTBOOL;

// a 64-bit constant
#ifdef __GNUC__
#define CONST64(v) (v##LL)
#define UCONST64(v) (v##ULL)
#else
#define CONST64(v) ((SQWORD)(v))
#define UCONST64(v) ((QWORD)(v))
#endif

#if !defined(GUID_DEFINED)
#define GUID_DEFINED
typedef struct _GUID
{
    DWORD	Data1;
    WORD	Data2;
    WORD	Data3;
    BYTE	Data4[8];
} GUID;
#endif

//
// Fixed point, 32bit as 16.16.
//
#define FRACBITS                        16
#define FRACUNIT                        (1<<FRACBITS)

typedef SDWORD                          fixed_t;
typedef DWORD                           dsfixed_t;              // fixedpt used by span drawer

#define FIXED_MAX                       (signed)(0x7fffffff)
#define FIXED_MIN                       (signed)(0x80000000)

#define DWORD_MIN						((uint32)0)
#define DWORD_MAX						((uint32)0xffffffff)


#ifdef __GNUC__
#define GCCPRINTF(stri,firstargi)       __attribute__((format(printf,stri,firstargi)))
#define GCCFORMAT(stri)                         __attribute__((format(printf,stri,0)))
#define GCCNOWARN                                       __attribute__((unused))
#else
#define GCCPRINTF(a,b)
#define GCCFORMAT(a)
#define GCCNOWARN
#endif


// [RH] This gets used all over; define it here:
int STACK_ARGS Printf (int printlevel, const char *, ...) GCCPRINTF(2,3);
int STACK_ARGS Printf (const char *, ...) GCCPRINTF(1,2);

// [RH] Same here:
int STACK_ARGS DPrintf (const char *, ...) GCCPRINTF(1,2);

// game print flags
enum
{
	PRINT_LOW,		// pickup messages
	PRINT_MEDIUM,	// death messages
	PRINT_HIGH,		// critical messages
	PRINT_CHAT,		// chat messages
	PRINT_TEAMCHAT	// chat messages from a teammate
};
#define PRINT_LOW				0				// pickup messages
#define PRINT_MEDIUM			1				// death messages
#define PRINT_HIGH				2				// critical messages
#define PRINT_CHAT				3				// chat messages
#define PRINT_TEAMCHAT			4				// chat messages from a teammate
#define PRINT_BOLD				200				// What Printf_Bold used

struct PalEntry
{
	PalEntry () {}
	PalEntry (DWORD argb) { *(DWORD *)this = argb; }
	operator DWORD () const { return *(DWORD *)this; }
	PalEntry &operator= (DWORD other) { *(DWORD *)this = other; return *this; }

#ifdef WORDS_BIGENDIAN
	PalEntry (BYTE ir, BYTE ig, BYTE ib) : a(0), r(ir), g(ig), b(ib) {}
	PalEntry (BYTE ia, BYTE ir, BYTE ig, BYTE ib) : a(ia), r(ir), g(ig), b(ib) {}
	BYTE a,r,g,b;
#else
	PalEntry (BYTE ir, BYTE ig, BYTE ib) : b(ib), g(ig), r(ir), a(0) {}
	PalEntry (BYTE ia, BYTE ir, BYTE ig, BYTE ib) : b(ib), g(ig), r(ir), a(ia) {}
	BYTE b,g,r,a;
#endif
};

#ifndef M_PI
#define M_PI		3.14159265358979323846	// matches value in gcc v2 math.h
#endif

template <typename T, size_t N>
char ( &_ArraySizeHelper( T (&array)[N] ))[N];

#define countof( array ) (sizeof( _ArraySizeHelper( array ) ))

#endif
