// Emacs style mode select   -*- C++ -*- 
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
//	WAD I/O functions.
//
//-----------------------------------------------------------------------------


#ifndef __W_WAD__
#define __W_WAD__


// [RH] Compare wad header as ints instead of chars
#ifdef __BIG_ENDIAN__
#define IWAD_ID (('I'<<24)|('W'<<16)|('A'<<8)|('D'))
#define PWAD_ID (('P'<<24)|('W'<<16)|('A'<<8)|('D'))
#define RFF_ID (('R'<<24)|('F'<<16)|('F'<<8)|(0x1a))
#else
#define IWAD_ID (('I')|('W'<<8)|('A'<<16)|('D'<<24))
#define PWAD_ID (('P')|('W'<<8)|('A'<<16)|('D'<<24))
#define RFF_ID (('R')|('F'<<8)|('F'<<16)|(0x1a<<24))
#endif

// [RH] Remove limit on number of WAD files
struct wadlist_t
{
	wadlist_t *next;
	char name[1];	// +size of string
};
extern wadlist_t *wadfiles;


//
// TYPES
//
struct wadinfo_t
{
	// Should be "IWAD" or "PWAD".
	DWORD		Magic;
	DWORD		NumLumps;
	DWORD		InfoTableOfs;
};

struct rffinfo_t
{
	// Should be "RFF\x18"
	DWORD		Magic;
	DWORD		Version;
	DWORD		DirOfs;
	DWORD		NumLumps;
};

struct wadlump_t
{
	DWORD		FilePos;
	DWORD		Size;
	char		Name[8];
};

struct rfflump_t
{
	BYTE		IDontKnow[16];
	DWORD		FilePos;
	DWORD		Size;
	BYTE		IStillDontKnow[8];
	BYTE		Flags;
	char		Extension[3];
	char		Name[8];
	BYTE		WhatIsThis[4];
};

//
// WADFILE I/O related stuff.
//
struct lumpinfo_t
{
	char		name[8];
	int			position;
	int			size;
	int			namespc;
	short		wadnum;
	WORD		flags;
};

enum
{
	LUMPF_BLOODCRYPT = 1	// Lump uses Blood-style encryption
};

// [RH] Namespaces from BOOM.
typedef enum {
	ns_global = 0,
	ns_sprites,
	ns_flats,
	ns_colormaps,
	ns_acslibrary,
	ns_bloodraw,
	ns_bloodsfx,
	ns_bloodmisc
} namespace_t;

extern	lumpinfo_t*	lumpinfo;
extern	int			numlumps;
extern	int			numwads;

extern	WORD		*FirstLumpIndex;	// [RH] Move hashing stuff out of
extern	WORD		*NextLumpIndex;		// lumpinfo structure

void W_InitMultipleFiles (wadlist_t **filenames);
int W_FakeLump (const char *filename);

const char *W_GetWadName (int wadnum);
const char *W_GetWadFullName (int wadnum);
bool W_CheckIfWadLoaded (const char *wadname);

int W_CheckNumForName (const char *name, int namespc);
int W_CheckNumForName (const char *name, int namespc, int wadfile);
int W_GetNumForName (const char *name);

inline int W_CheckNumForName (const byte *name) { return W_CheckNumForName ((const char *)name, ns_global); }
inline int W_CheckNumForName (const char *name) { return W_CheckNumForName (name, ns_global); }
inline int W_CheckNumForName (const byte *name, int ns) { return W_CheckNumForName ((const char *)name, ns); }
inline int W_GetNumForName (const byte *name) { return W_GetNumForName ((const char *)name); }

int W_LumpLength (int lump);
void W_ReadLump (int lump, void *dest);

void *W_MapLumpNum (int lump, bool write);
inline const void *W_MapLumpNum (int lump)
{
	return W_MapLumpNum (lump, false);
}

inline void *W_MapLumpName (const char *name, bool write)
{
	return W_MapLumpNum (W_GetNumForName (name), write);
}
inline const void *W_MapLumpName (const char *name)
{
	return W_MapLumpNum (W_GetNumForName (name), false);
}

void W_UnMapLump (const void *block);

void W_Profile (const char *fname);

int W_FindLump (const char *name, int *lastlump);	// [RH]	Find lumps with duplication
BOOL W_CheckLumpName (int lump, const char *name);	// [RH] True if lump's name == name

DWORD W_LumpNameHash (const char *name);			// [RH] Create hash key from an 8-char name
void W_InitHashChains (void);						// [RH] Set up the lumpinfo hashing

// [RH] Combine multiple marked ranges of lumps into one.
void W_MergeLumps (const char *start, const char *end, int);

// [RH] Copy an 8-char string and uppercase it.
void uppercopy (char *to, const char *from);

// [RH] Copies the lump name to to using uppercopy
void W_GetLumpName (char *to, int lump);

// [RH] Returns wadnum for a specified lump
int W_GetLumpFile (int lump);

#endif
