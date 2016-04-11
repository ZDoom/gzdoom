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

// Disable warning about using unsized arrays in structs. It supports it just
// fine, and so do Clang and GCC, but the latter two don't warn about it.
#pragma warning(disable:4200)
#endif

#include <limits.h>
#include "tarray.h"
#include "name.h"
#include "zstring.h"

class PClassActor;
typedef TMap<int, PClassActor *> FClassMap;

// Since this file is included by everything, it seems an appropriate place
// to check the NOASM/USEASM macros.

// There are three assembly-related macros:
//
//		NOASM	- Assembly code is disabled
//		X86_ASM	- Using ia32 assembly code
//		X64_ASM	- Using amd64 assembly code
//
// Note that these relate only to using the pure assembly code. Inline
// assembly may still be used without respect to these macros, as
// deemed appropriate.

#ifndef NOASM
// Select the appropriate type of assembly code to use.

#if defined(_M_IX86) || defined(__i386__)

#define X86_ASM
#ifdef X64_ASM
#undef X64_ASM
#endif

#elif defined(_M_X64) || defined(__amd64__)

#define X64_ASM
#ifdef X86_ASM
#undef X86_ASM
#endif

#else

#define NOASM

#endif

#endif

#ifdef NOASM
// Ensure no assembly macros are defined if NOASM is defined.

#ifdef X86_ASM
#undef X86_ASM
#endif

#ifdef X64_ASM
#undef X64_ASM
#endif

#endif


#if defined(_MSC_VER)
#define NOVTABLE __declspec(novtable)
#else
#define NOVTABLE
#endif

#if defined(__clang__)
#if defined(__has_feature) && __has_feature(address_sanitizer)
#define NO_SANITIZE __attribute__((no_sanitize("address")))
#else
#define NO_SANITIZE
#endif
#else
#define NO_SANITIZE
#endif

#if defined(__GNUC__)
// With versions of GCC newer than 4.2, it appears it was determined that the
// cost of an unaligned pointer on PPC was high enough to add padding to the
// end of packed structs.  For whatever reason __packed__ and pragma pack are
// handled differently in this regard. Note that this only needs to be applied
// to types which are used in arrays or sizeof is needed. This also prevents
// code from taking references to the struct members.
#define FORCE_PACKED __attribute__((__packed__))
#else
#define FORCE_PACKED
#endif

#include "basictypes.h"

extern bool batchrun;

// Bounding box coordinate storage.
enum
{
	BOXTOP,
	BOXBOTTOM,
	BOXLEFT,
	BOXRIGHT
};		// bbox coordinates


// [RH] This gets used all over; define it here:
int Printf (int printlevel, const char *, ...) GCCPRINTF(2,3);
int Printf (const char *, ...) GCCPRINTF(1,2);

// [RH] Same here:
int DPrintf (const char *, ...) GCCPRINTF(1,2);

extern "C" int mysnprintf(char *buffer, size_t count, const char *format, ...) GCCPRINTF(3,4);
extern "C" int myvsnprintf(char *buffer, size_t count, const char *format, va_list argptr) GCCFORMAT(3);


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
#define PRINT_LOG				5				// only to logfile
#define PRINT_BOLD				200				// What Printf_Bold used

struct PalEntry
{
	PalEntry () {}
	PalEntry (uint32 argb) { d = argb; }
	operator uint32 () const { return d; }
	PalEntry &operator= (uint32 other) { d = other; return *this; }
	PalEntry InverseColor() const { PalEntry nc; nc.a = a; nc.r = 255 - r; nc.g = 255 - g; nc.b = 255 - b; return nc; }
#ifdef __BIG_ENDIAN__
	PalEntry (BYTE ir, BYTE ig, BYTE ib) : a(0), r(ir), g(ig), b(ib) {}
	PalEntry (BYTE ia, BYTE ir, BYTE ig, BYTE ib) : a(ia), r(ir), g(ig), b(ib) {}
	union
	{
		struct
		{
			BYTE a,r,g,b;
		};
		uint32 d;
	};
#else
	PalEntry (BYTE ir, BYTE ig, BYTE ib) : b(ib), g(ig), r(ir), a(0) {}
	PalEntry (BYTE ia, BYTE ir, BYTE ig, BYTE ib) : b(ib), g(ig), r(ir), a(ia) {}
	union
	{
		struct
		{
			BYTE b,g,r,a;
		};
		uint32 d;
	};
#endif
};

class FArchive;
class PClassInventory;

class FTextureID
{
	friend class FTextureManager;
	friend FArchive &operator<< (FArchive &arc, FTextureID &tex);
	friend FTextureID GetHUDIcon(PClassInventory *cls);
	friend void R_InitSpriteDefs();

public:
	FTextureID() throw() {}
	bool isNull() const { return texnum == 0; }
	bool isValid() const { return texnum > 0; }
	bool Exists() const { return texnum >= 0; }
	void SetInvalid() { texnum = -1; }
	void SetNull() { texnum = 0; }
	bool operator ==(const FTextureID &other) const { return texnum == other.texnum; }
	bool operator !=(const FTextureID &other) const { return texnum != other.texnum; }
	FTextureID operator +(int offset) throw();
	int GetIndex() const { return texnum; }	// Use this only if you absolutely need the index!

											// The switch list needs these to sort the switches by texture index
	int operator -(FTextureID other) const { return texnum - other.texnum; }
	bool operator < (FTextureID other) const { return texnum < other.texnum; }
	bool operator > (FTextureID other) const { return texnum > other.texnum; }

protected:
	FTextureID(int num) { texnum = num; }
private:
	int texnum;
};



// Screenshot buffer image data types
enum ESSType
{
	SS_PAL,
	SS_RGB,
	SS_BGRA
};

// always use our own definition for consistency.
#ifdef M_PI
#undef M_PI
#endif

const double M_PI = 3.14159265358979323846;	// matches value in gcc v2 math.h

template <typename T, size_t N>
char ( &_ArraySizeHelper( T (&array)[N] ))[N];

#define countof( array ) (sizeof( _ArraySizeHelper( array ) ))

// Auto-registration sections for GCC.
// Apparently, you cannot do string concatenation inside section attributes.
#ifdef __MACH__
#define SECTION_AREG "__DATA,areg"
#define SECTION_CREG "__DATA,creg"
#define SECTION_GREG "__DATA,greg"
#define SECTION_MREG "__DATA,mreg"
#define SECTION_YREG "__DATA,yreg"
#else
#define SECTION_AREG "areg"
#define SECTION_CREG "creg"
#define SECTION_GREG "greg"
#define SECTION_MREG "mreg"
#define SECTION_YREG "yreg"
#endif

#endif
