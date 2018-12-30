//-----------------------------------------------------------------------------
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
	uint32_t		Magic;
	uint32_t		NumLumps;
	uint32_t		InfoTableOfs;
};

struct wadlump_t
{
	uint32_t		FilePos;
	uint32_t		Size;
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
	LUMPF_MAYBEFLAT=1,		// might be a flat outside F_START/END
	LUMPF_ZIPFILE=2,		// contains a full path
	LUMPF_EMBEDDED=4,		// from an embedded WAD
	LUMPF_BLOODCRYPT = 8,	// encrypted
	LUMPF_COMPRESSED = 16,	// compressed
	LUMPF_SEQUENTIAL = 32,	// compressed but a sequential reader can be retrieved.
};


// [RH] Copy an 8-char string and uppercase it.
void uppercopy (char *to, const char *from);

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
	int GetIwadNum() { return IwadIndex; }
	void SetIwadNum(int x) { IwadIndex = x; }

	void InitMultipleFiles (TArray<FString> &filenames);
	void AddFile (const char *filename, FileReader *wadinfo = NULL);
	int CheckIfWadLoaded (const char *name);

	const char *GetWadName (int wadnum) const;
	const char *GetWadFullName (int wadnum) const;

	int GetFirstLump(int wadnum) const;
	int GetLastLump(int wadnum) const;
    int GetLumpCount(int wadnum) const;

	int CheckNumForName (const char *name, int namespc);
	int CheckNumForName (const char *name, int namespc, int wadfile, bool exact = true);
	int GetNumForName (const char *name, int namespc);

	inline int CheckNumForName (const uint8_t *name) { return CheckNumForName ((const char *)name, ns_global); }
	inline int CheckNumForName (const char *name) { return CheckNumForName (name, ns_global); }
	inline int CheckNumForName (const FString &name) { return CheckNumForName (name.GetChars()); }
	inline int CheckNumForName (const uint8_t *name, int ns) { return CheckNumForName ((const char *)name, ns); }
	inline int GetNumForName (const char *name) { return GetNumForName (name, ns_global); }
	inline int GetNumForName (const FString &name) { return GetNumForName (name.GetChars(), ns_global); }
	inline int GetNumForName (const uint8_t *name) { return GetNumForName ((const char *)name); }
	inline int GetNumForName (const uint8_t *name, int ns) { return GetNumForName ((const char *)name, ns); }

	int CheckNumForFullName (const char *name, bool trynormal = false, int namespc = ns_global, bool ignoreext = false);
	int CheckNumForFullName (const char *name, int wadfile);
	int GetNumForFullName (const char *name);

	inline int CheckNumForFullName(const FString &name, bool trynormal = false, int namespc = ns_global) { return CheckNumForFullName(name.GetChars(), trynormal, namespc); }
	inline int CheckNumForFullName (const FString &name, int wadfile) { return CheckNumForFullName(name.GetChars(), wadfile); }
	inline int GetNumForFullName (const FString &name) { return GetNumForFullName(name.GetChars()); }

	void SetLinkedTexture(int lump, FTexture *tex);
	FTexture *GetLinkedTexture(int lump);


	void ReadLump (int lump, void *dest);
	TArray<uint8_t> ReadLumpIntoArray(int lump, int pad = 0);	// reads lump into a writable buffer and optionally adds some padding at the end. (FMemLump isn't writable!)
	FMemLump ReadLump (int lump);
	FMemLump ReadLump (const char *name) { return ReadLump (GetNumForName (name)); }

	FileReader OpenLumpReader(int lump);		// opens a reader that redirects to the containing file's one.
	FileReader ReopenLumpReader(int lump, bool alwayscache = false);		// opens an independent reader.

	int FindLump (const char *name, int *lastlump, bool anyns=false);		// [RH] Find lumps with duplication
	int FindLumpMulti (const char **names, int *lastlump, bool anyns = false, int *nameindex = NULL); // same with multiple possible names
	bool CheckLumpName (int lump, const char *name);	// [RH] True if lump's name == name

	static uint32_t LumpNameHash (const char *name);		// [RH] Create hash key from an 8-char name

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
	FResourceLump *GetLumpRecord(int lump) const;	// Returns the FResourceLump, in case the caller wants to have direct access to the lump cache.
	bool CheckLumpName (int lump, const char *name) const;	// [RH] Returns true if the names match

	bool IsEncryptedFile(int lump) const;

	int GetNumLumps () const;
	int GetNumWads () const;

	int AddExternalFile(const char *filename);

protected:

	struct LumpRecord;

	TArray<FResourceFile *> Files;
	TArray<LumpRecord> LumpInfo;

	TArray<uint32_t> Hashes;	// one allocation for all hash lists.
	uint32_t *FirstLumpIndex;	// [RH] Hashing stuff moved out of lumpinfo structure
	uint32_t *NextLumpIndex;

	uint32_t *FirstLumpIndex_FullName;	// The same information for fully qualified paths from .zips
	uint32_t *NextLumpIndex_FullName;

	uint32_t *FirstLumpIndex_NoExt;	// The same information for fully qualified paths from .zips
	uint32_t *NextLumpIndex_NoExt;

	uint32_t NumLumps = 0;					// Not necessarily the same as LumpInfo.Size()
	uint32_t NumWads;

	int IwadIndex;

	void InitHashChains ();								// [RH] Set up the lumpinfo hashing

private:
	void RenameSprites();
	void RenameNerve();
	void FixMacHexen();
	void DeleteAll();
	FileReader * GetFileReader(int wadnum);	// Gets a FileReader object to the entire WAD
};

extern FWadCollection Wads;

#endif
