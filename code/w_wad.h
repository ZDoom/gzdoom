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
#else
#define IWAD_ID (('I')|('W'<<8)|('A'<<16)|('D'<<24))
#define PWAD_ID (('P')|('W'<<8)|('A'<<16)|('D'<<24))
#endif

// [RH] Remove limit on number of WAD files
typedef struct wadlist_s {
	struct wadlist_s *next;
	char name[1];	// +size of string
} wadlist_t;
extern wadlist_t *wadfiles;


//
// TYPES
//
typedef struct
{
	// Should be "IWAD" or "PWAD".
	unsigned	identification;
	int			numlumps;
	int			infotableofs;

} wadinfo_t;


typedef struct
{
	int			filepos;
	int			size;
	char		name[8];

} filelump_t;

//
// WADFILE I/O related stuff.
//
typedef struct lumpinfo_s
{
	char		name[8];
	int			position;
	int			size;
	int			namespc;

	int			handle;
} lumpinfo_t;

// [RH] Namespaces from BOOM.
typedef enum {
	ns_global = 0,
	ns_sprites,
	ns_flats,
	ns_colormaps
} namespace_t;

extern	void**		lumpcache;
extern	lumpinfo_t*	lumpinfo;
extern	int			numlumps;

extern	WORD		*FirstLumpIndex;	// [RH] Move hashing stuff out of
extern	WORD		*NextLumpIndex;		// lumpinfo structure

void W_InitMultipleFiles (wadlist_t **filenames);

int W_FileHandleFromWad (int wadnum);

int W_CheckNumForName (const char *name, int namespc);
int W_GetNumForName (const char *name);

inline int W_CheckNumForName (const byte *name) { return W_CheckNumForName ((const char *)name, ns_global); }
inline int W_CheckNumForName (const char *name) { return W_CheckNumForName (name, ns_global); }
inline int W_CheckNumForName (const byte *name, int ns) { return W_CheckNumForName ((const char *)name, ns); }
inline int W_GetNumForName (const byte *name) { return W_GetNumForName ((const char *)name); }

int W_LumpLength (int lump);
void W_ReadLump (int lump, void *dest);

void *W_CacheLumpNum (int lump, int tag);

// [RH] W_CacheLumpName() is now a macro
#define W_CacheLumpName(name,tag) W_CacheLumpNum (W_GetNumForName(name), (tag))

void W_Profile (const char *fname);

int W_FindLump (const char *name, int *lastlump);	// [RH]	Find lumps with duplication
BOOL W_CheckLumpName (int lump, const char *name);	// [RH] True if lump's name == name

unsigned W_LumpNameHash (const char *name);			// [RH] Create hash key from an 8-char name
void W_InitHashChains (void);						// [RH] Set up the lumpinfo hashing

// [RH] Combine multiple marked ranges of lumps into one.
void W_MergeLumps (const char *start, const char *end, int);

// [RH] Copy an 8-char string and uppercase it.
void uppercopy (char *to, const char *from);

// [RH] Copies the lump name to to using uppercopy
void W_GetLumpName (char *to, int lump);

// [RH] Returns file handle for specified lump
int W_GetLumpFile (int lump);

#endif
