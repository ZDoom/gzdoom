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

#include "files.h"

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

struct wadlump_t
{
	DWORD		FilePos;
	DWORD		Size;
	char		Name[8];
};

// [RH] Namespaces from BOOM.
typedef enum {
	ns_global = 0,
	ns_sprites,
	ns_flats,
	ns_colormaps,
	ns_acslibrary,
	ns_newtextures,
	ns_bloodraw,
	ns_bloodsfx,
	ns_bloodmisc
} namespace_t;

// [RH] Copy an 8-char string and uppercase it.
void uppercopy (char *to, const char *from);

// A very loose reference to a lump on disk. This is really just a wrapper
// around the main wad's FILE object with a different length recorded. Since
// the two lumps from the same wad share the same FILE, you cannot read from
// both of them independantly.
class FWadLump : public FileReader
{
public:
	FWadLump ();
	FWadLump (const FWadLump &copy);

private:
	FWadLump (const FileReader &reader, long length);
	FWadLump (FILE *file, long length);

	friend class FWadCollection;
};

// A lump in memory. The destructor automatically deletes the memory
// the lump was copied to. Note the copy contstructor is really more of
// a transfer constructor. Once an FMemLump gets copied, the original
// is no longer usable.
class FMemLump
{
public:
	FMemLump ();
#ifdef __GNUC__
	// Not really const! GCC forces me to declare it this way!
	FMemLump (const FMemLump &copy);
	FMemLump &operator= (const FMemLump &copy);
#else
	FMemLump (FMemLump &copy);
	FMemLump &operator= (FMemLump &copy);
#endif
	~FMemLump ();
	void *GetMem () { return (void *)Block; }

private:
	FMemLump (BYTE *data);

	BYTE *Block;

	friend class FWadCollection;
};

class FWadCollection
{
public:
	FWadCollection ();

	void InitMultipleFiles (wadlist_t **filenames);
	void AddFile (const char *filename);
	bool CheckIfWadLoaded (const char *name);

	const char *GetWadName (int wadnum) const;
	const char *GetWadFullName (int wadnum) const;

	int CheckNumForName (const char *name, int namespc);
	int CheckNumForName (const char *name, int namespc, int wadfile);
	int GetNumForName (const char *name);

	inline int CheckNumForName (const byte *name) { return CheckNumForName ((const char *)name, ns_global); }
	inline int CheckNumForName (const char *name) { return CheckNumForName (name, ns_global); }
	inline int CheckNumForName (const byte *name, int ns) { return CheckNumForName ((const char *)name, ns); }
	inline int GetNumForName (const byte *name) { return GetNumForName ((const char *)name); }

	void ReadLump (int lump, void *dest);
	FMemLump ReadLump (int lump);
	FMemLump ReadLump (const char *name) { return ReadLump (GetNumForName (name)); }

	FWadLump OpenLumpNum (int lump);
	FWadLump OpenLumpName (const char *name) { return OpenLumpNum (GetNumForName (name)); }
	FWadLump *ReopenLumpNum (int lump);	// Opens a new, independant FILE

	int FindLump (const char *name, int *lastlump);		// [RH] Find lumps with duplication
	bool CheckLumpName (int lump, const char *name);	// [RH] True if lump's name == name

	static DWORD LumpNameHash (const char *name);		// [RH] Create hash key from an 8-char name

	int LumpLength (int lump) const;
	void GetLumpName (char *to, int lump) const;	// [RH] Copies the lump name to to using uppercopy
	int GetLumpFile (int lump) const;				// [RH] Returns wadnum for a specified lump
	int GetLumpNamespace (int lump) const;			// [RH] Returns the namespace a lump belongs to
	bool CheckLumpName (int lump, const char *name) const;	// [RH] Returns true if the names match

	int GetNumLumps () const;

protected:
	class WadFileRecord;
	struct LumpRecord;

	WORD *FirstLumpIndex;	// [RH] Hashing stuff moved out of lumpinfo structure
	WORD *NextLumpIndex;

	LumpRecord *LumpInfo;
	WadFileRecord **Wads;
	DWORD NumLumps;
	DWORD NumWads;

	void SkinHack (int baselump);
	void InitHashChains ();								// [RH] Set up the lumpinfo hashing

	// [RH] Combine multiple marked ranges of lumps into one.
	void MergeLumps (const char *start, const char *end, int);
	bool IsMarker (const LumpRecord *lump, const char *marker) const;
};

extern FWadCollection Wads;

#endif
