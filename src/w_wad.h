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
#include "doomdef.h"
#include "tarray.h"

// [RH] Compare wad header as ints instead of chars
#define IWAD_ID		MAKE_ID('I','W','A','D')
#define PWAD_ID		MAKE_ID('P','W','A','D')
#define RFF_ID		MAKE_ID('R','F','F',0x1a)
#define ZIP_ID		MAKE_ID('P','K',3,4)
#define GRP_ID_0	MAKE_ID('K','e','n','S')
#define GRP_ID_1	MAKE_ID('i','l','v','e')
#define GRP_ID_2	MAKE_ID('r','m','a','n')

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

enum
{
	LUMPF_BLOODCRYPT	= 1,	// Lump uses Blood-style encryption
	LUMPF_COMPRESSED	= 2,	// Zip-compressed
	LUMPF_ZIPFILE		= 4,	// Inside a Zip file - used to enforce use of special directories insize Zips
	LUMPF_NEEDFILESTART	= 8,	// Still need to process local file header to find file start inside a zip
	LUMPF_EXTERNAL		= 16,	// Lump is from an external file that won't be kept open permanently
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
	ns_bloodmisc,
	ns_strifevoices,
	ns_hires,

	// These namespaces are only used to mark lumps in special subdirectories
	// so that their contents doesn't interfere with the global namespace.
	// searching for data in these namespaces works differently for lumps coming
	// from Zips or other files.
	ns_specialzipdirectory,
	ns_sounds,
	ns_patches,
	ns_graphics,
	ns_music,
} namespace_t;

// [RH] Copy an 8-char string and uppercase it.
void uppercopy (char *to, const char *from);

// Perform Blood encryption/decryption.
void BloodCrypt (void *data, int key, int len);

// Locate central directory in a zip file.
DWORD Zip_FindCentralDir(FileReader * fin);

// A very loose reference to a lump on disk. This is really just a wrapper
// around the main wad's FILE object with a different length recorded. Since
// two lumps from the same wad share the same FILE, you cannot read from
// both of them independantly.
class FWadLump : public FileReader
{
public:
	FWadLump ();
	FWadLump (const FWadLump &copy);
#ifdef _DEBUG
	FWadLump & operator= (const FWadLump &copy);
#endif
	~FWadLump();

	long Seek (long offset, int origin);
	long Read (void *buffer, long len);
	char *Gets(char *strbuf, int len);

private:
	FWadLump (const FileReader &reader, long length, bool encrypted);
	FWadLump (FILE *file, long length);
	FWadLump (char * data, long length, bool destroy);

	char *SourceData;
	bool DestroySource;
	bool Encrypted;

	friend class FWadCollection;
};


// A lump in memory.
class FMemLump
{
public:
	FMemLump ();

	FMemLump (const FMemLump &copy);
	FMemLump &operator= (const FMemLump &copy);
	~FMemLump ();
	void *GetMem () { return Block.Len() == 0 ? NULL : (void *)Block.GetChars(); }
	size_t GetSize () { return Block.Len(); }
	FString GetString () { return Block; }

private:
	FMemLump (const FString &source);

	FString Block;

	friend class FWadCollection;
};

class FWadCollection
{
public:
	FWadCollection ();
	~FWadCollection ();

	// The wadnum for the IWAD
	enum { IWAD_FILENUM = 1 };

	void InitMultipleFiles (wadlist_t **filenames);
	void AddFile (const char *filename, const char * data=NULL,int length=-1);
	int CheckIfWadLoaded (const char *name);

	const char *GetWadName (int wadnum) const;
	const char *GetWadFullName (int wadnum) const;

	int CheckNumForName (const char *name, int namespc);
	int CheckNumForName (const char *name, int namespc, int wadfile, bool exact = true);
	int GetNumForName (const char *name, int namespc);

	inline int CheckNumForName (const BYTE *name) { return CheckNumForName ((const char *)name, ns_global); }
	inline int CheckNumForName (const char *name) { return CheckNumForName (name, ns_global); }
	inline int CheckNumForName (const BYTE *name, int ns) { return CheckNumForName ((const char *)name, ns); }
	inline int GetNumForName (const char *name) { return GetNumForName (name, ns_global); }
	inline int GetNumForName (const BYTE *name) { return GetNumForName ((const char *)name); }
	inline int GetNumForName (const BYTE *name, int ns) { return GetNumForName ((const char *)name, ns); }

	int CheckNumForFullName (const char *name, bool trynormal = false, int namespc = ns_global);
	int CheckNumForFullName (const char *name, int wadfile);
	int GetNumForFullName (const char *name);


	void ReadLump (int lump, void *dest);
	FMemLump ReadLump (int lump);
	FMemLump ReadLump (const char *name) { return ReadLump (GetNumForName (name)); }

	FWadLump OpenLumpNum (int lump);
	FWadLump OpenLumpName (const char *name) { return OpenLumpNum (GetNumForName (name)); }
	FWadLump *ReopenLumpNum (int lump);	// Opens a new, independent FILE
	
	FileReader * GetFileReader(int wadnum);	// Gets a FileReader object to the entire WAD

	int FindLump (const char *name, int *lastlump, bool anyns=false);		// [RH] Find lumps with duplication
	bool CheckLumpName (int lump, const char *name);	// [RH] True if lump's name == name

	static DWORD LumpNameHash (const char *name);		// [RH] Create hash key from an 8-char name

	int LumpLength (int lump) const;
	int GetLumpOffset (int lump);					// [RH] Returns offset of lump in the wadfile
	int GetLumpFlags (int lump);					// Return the flags for this lump
	void GetLumpName (char *to, int lump) const;	// [RH] Copies the lump name to to using uppercopy
	const char *GetLumpFullName (int lump) const;	// [RH] Returns the lump's full name
	FString GetLumpFullPath (int lump) const;		// [RH] Returns wad's name + lump's full name
	int GetLumpFile (int lump) const;				// [RH] Returns wadnum for a specified lump
	int GetLumpNamespace (int lump) const;			// [RH] Returns the namespace a lump belongs to
	bool CheckLumpName (int lump, const char *name) const;	// [RH] Returns true if the names match

	bool IsUncompressedFile(int lump) const;
	bool IsEncryptedFile(int lump) const;

	int GetNumLumps () const;
	int GetNumWads () const;

	int AddExternalFile(const char *filename);

protected:
	class WadFileRecord;
	struct LumpRecord;

	DWORD *FirstLumpIndex;	// [RH] Hashing stuff moved out of lumpinfo structure
	DWORD *NextLumpIndex;

	DWORD *FirstLumpIndex_FullName;	// The same information for fully qualified paths from .zips
	DWORD *NextLumpIndex_FullName;


	TArray<LumpRecord> LumpInfo;
	TArray<WadFileRecord *>Wads;
	DWORD NumLumps;					// Not necessarily the same as LumpInfo.Size()
	DWORD NumWads;

	void SkinHack (int baselump);
	void InitHashChains ();								// [RH] Set up the lumpinfo hashing

	// [RH] Combine multiple marked ranges of lumps into one.
	int MergeLumps (const char *start, const char *end, int name_space);
	bool IsMarker (const LumpRecord *lump, const char *marker) const;
	void FindStrifeTeaserVoices ();

private:
	static int STACK_ARGS lumpcmp(const void * a, const void * b);
	void ScanForFlatHack (int startlump);
	void RenameSprites (int startlump);
	void SetLumpAddress(LumpRecord *l);
	void DeleteAll();
};

extern FWadCollection Wads;

#endif
