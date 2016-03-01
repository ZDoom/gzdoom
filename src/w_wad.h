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

class FResourceFile;
struct FResourceLump;
class FTexture;

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

#define IWAD_ID		MAKE_ID('I','W','A','D')
#define PWAD_ID		MAKE_ID('P','W','A','D')


// [RH] Namespaces from BOOM.
typedef enum {
	ns_hidden = -1,

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
	ns_voxels,

	// These namespaces are only used to mark lumps in special subdirectories
	// so that their contents doesn't interfere with the global namespace.
	// searching for data in these namespaces works differently for lumps coming
	// from Zips or other files.
	ns_specialzipdirectory,
	ns_sounds,
	ns_patches,
	ns_graphics,
	ns_music,

	ns_firstskin,
} namespace_t;

enum ELumpFlags
{
	LUMPF_MAYBEFLAT=1,
	LUMPF_ZIPFILE=2,
	LUMPF_EMBEDDED=4,
	LUMPF_BLOODCRYPT = 8,
};


// [RH] Copy an 8-char string and uppercase it.
void uppercopy (char *to, const char *from);

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
	FWadLump (FResourceLump *Lump, bool alwayscache = false);
	FWadLump(int lumpnum, FResourceLump *lump);

	FResourceLump *Lump;

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

	void InitMultipleFiles (TArray<FString> &filenames);
	void AddFile (const char *filename, FileReader *wadinfo = NULL);
	int CheckIfWadLoaded (const char *name);

	const char *GetWadName (int wadnum) const;
	const char *GetWadFullName (int wadnum) const;

	int GetFirstLump(int wadnum) const;
	int GetLastLump(int wadnum) const;

	int CheckNumForName (const char *name, int namespc);
	int CheckNumForName (const char *name, int namespc, int wadfile, bool exact = true);
	int GetNumForName (const char *name, int namespc);

	inline int CheckNumForName (const BYTE *name) { return CheckNumForName ((const char *)name, ns_global); }
	inline int CheckNumForName (const char *name) { return CheckNumForName (name, ns_global); }
	inline int CheckNumForName (const FString &name) { return CheckNumForName (name.GetChars()); }
	inline int CheckNumForName (const BYTE *name, int ns) { return CheckNumForName ((const char *)name, ns); }
	inline int GetNumForName (const char *name) { return GetNumForName (name, ns_global); }
	inline int GetNumForName (const FString &name) { return GetNumForName (name.GetChars(), ns_global); }
	inline int GetNumForName (const BYTE *name) { return GetNumForName ((const char *)name); }
	inline int GetNumForName (const BYTE *name, int ns) { return GetNumForName ((const char *)name, ns); }

	int CheckNumForFullName (const char *name, bool trynormal = false, int namespc = ns_global);
	int CheckNumForFullName (const char *name, int wadfile);
	int GetNumForFullName (const char *name);

	inline int CheckNumForFullName(const FString &name, bool trynormal = false, int namespc = ns_global) { return CheckNumForFullName(name.GetChars(), trynormal, namespc); }
	inline int CheckNumForFullName (const FString &name, int wadfile) { return CheckNumForFullName(name.GetChars(), wadfile); }
	inline int GetNumForFullName (const FString &name) { return GetNumForFullName(name.GetChars()); }

	void SetLinkedTexture(int lump, FTexture *tex);
	FTexture *GetLinkedTexture(int lump);


	void ReadLump (int lump, void *dest);
	FMemLump ReadLump (int lump);
	FMemLump ReadLump (const char *name) { return ReadLump (GetNumForName (name)); }

	FWadLump OpenLumpNum (int lump);
	FWadLump OpenLumpName (const char *name) { return OpenLumpNum (GetNumForName (name)); }
	FWadLump *ReopenLumpNum (int lump);	// Opens a new, independent FILE
	FWadLump *ReopenLumpNumNewFile (int lump);	// Opens a new, independent FILE
	
	FileReader * GetFileReader(int wadnum);	// Gets a FileReader object to the entire WAD

	int FindLump (const char *name, int *lastlump, bool anyns=false);		// [RH] Find lumps with duplication
	int FindLumpMulti (const char **names, int *lastlump, bool anyns = false, int *nameindex = NULL); // same with multiple possible names
	bool CheckLumpName (int lump, const char *name);	// [RH] True if lump's name == name

	static DWORD LumpNameHash (const char *name);		// [RH] Create hash key from an 8-char name

	int LumpLength (int lump) const;
	int GetLumpOffset (int lump);					// [RH] Returns offset of lump in the wadfile
	int GetLumpFlags (int lump);					// Return the flags for this lump
	void GetLumpName (char *to, int lump) const;	// [RH] Copies the lump name to to using uppercopy
	void GetLumpName (FString &to, int lump) const;
	const char *GetLumpFullName (int lump) const;	// [RH] Returns the lump's full name
	FString GetLumpFullPath (int lump) const;		// [RH] Returns wad's name + lump's full name
	int GetLumpFile (int lump) const;				// [RH] Returns wadnum for a specified lump
	int GetLumpNamespace (int lump) const;			// [RH] Returns the namespace a lump belongs to
	int GetLumpIndexNum (int lump) const;			// Returns the RFF index number for this lump
	bool CheckLumpName (int lump, const char *name) const;	// [RH] Returns true if the names match

	bool IsUncompressedFile(int lump) const;
	bool IsEncryptedFile(int lump) const;

	int GetNumLumps () const;
	int GetNumWads () const;

	int AddExternalFile(const char *filename);

protected:

	struct LumpRecord;

	TArray<FResourceFile *> Files;
	TArray<LumpRecord> LumpInfo;

	DWORD *FirstLumpIndex;	// [RH] Hashing stuff moved out of lumpinfo structure
	DWORD *NextLumpIndex;

	DWORD *FirstLumpIndex_FullName;	// The same information for fully qualified paths from .zips
	DWORD *NextLumpIndex_FullName;

	DWORD NumLumps;					// Not necessarily the same as LumpInfo.Size()
	DWORD NumWads;

	void SkinHack (int baselump);
	void InitHashChains ();								// [RH] Set up the lumpinfo hashing

private:
	void RenameSprites();
	void RenameNerve();
	void FixMacHexen();
	void DeleteAll();
};

extern FWadCollection Wads;

#endif
