#pragma once
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	File system  I/O functions.
//
//-----------------------------------------------------------------------------



#include "files.h"
#include "tarray.h"
#include "cmdlib.h"
#include "zstring.h"
#include "resourcefile.h"

class FResourceFile;
struct FResourceLump;
class FGameTexture;

union LumpShortName
{
	char		String[9];

	uint32_t		dword;			// These are for accessing the first 4 or 8 chars of
	uint64_t		qword;			// Name as a unit without breaking strict aliasing rules
};


// A lump in memory.
class FileData
{
public:
	FileData ();

	FileData (const FileData &copy);
	FileData &operator= (const FileData &copy);
	~FileData ();
	void *GetMem () { return Block.Len() == 0 ? NULL : (void *)Block.GetChars(); }
	size_t GetSize () { return Block.Len(); }
	const FString &GetString () const { return Block; }

private:
	FileData (const FString &source);

	FString Block;

	friend class FileSystem;
};

struct FolderEntry
{
	const char *name;
	unsigned lumpnum;
};

class FileSystem
{
public:
	FileSystem ();
	~FileSystem ();

	// The wadnum for the IWAD
	int GetIwadNum() { return IwadIndex; }
	void SetIwadNum(int x) { IwadIndex = x; }

	int GetMaxIwadNum() { return MaxIwadIndex; }
	void SetMaxIwadNum(int x) { MaxIwadIndex = x; }

	void InitSingleFile(const char *filename, bool quiet = false);
	void InitMultipleFiles (TArray<FString> &filenames, bool quiet = false, LumpFilterInfo* filter = nullptr);
	void AddFile (const char *filename, FileReader *wadinfo, bool quiet, LumpFilterInfo* filter);
	int CheckIfResourceFileLoaded (const char *name) noexcept;
	void AddAdditionalFile(const char* filename, FileReader* wadinfo = NULL) {}

	const char *GetResourceFileName (int filenum) const noexcept;
	const char *GetResourceFileFullName (int wadnum) const noexcept;

	int GetFirstEntry(int wadnum) const noexcept;
	int GetLastEntry(int wadnum) const noexcept;
    int GetEntryCount(int wadnum) const noexcept;

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
	int FindFile(const char* name)
	{
		return CheckNumForFullName(name);
	}

	bool FileExists(const char* name)
	{
		return FindFile(name) >= 0;
	}

	bool FileExists(const FString& name)
	{
		return FindFile(name) >= 0;
	}

	bool FileExists(const std::string& name)
	{
		return FindFile(name.c_str()) >= 0;
	}

	LumpShortName& GetShortName(int i);	// may only be called before the hash chains are set up.
	void RenameFile(int num, const char* fn);
	bool CreatePathlessCopy(const char* name, int id, int flags);

	inline int CheckNumForFullName(const FString &name, bool trynormal = false, int namespc = ns_global) { return CheckNumForFullName(name.GetChars(), trynormal, namespc); }
	inline int CheckNumForFullName (const FString &name, int wadfile) { return CheckNumForFullName(name.GetChars(), wadfile); }
	inline int GetNumForFullName (const FString &name) { return GetNumForFullName(name.GetChars()); }

	void SetLinkedTexture(int lump, FGameTexture *tex);
	FGameTexture *GetLinkedTexture(int lump);


	void ReadFile (int lump, void *dest);
	TArray<uint8_t> GetFileData(int lump, int pad = 0);	// reads lump into a writable buffer and optionally adds some padding at the end. (FileData isn't writable!)
	FileData ReadFile (int lump);
	FileData ReadFile (const char *name) { return ReadFile (GetNumForName (name)); }

	inline TArray<uint8_t> LoadFile(const char* name, int padding = 0)
	{
		auto lump = FindFile(name);
		if (lump < 0) return TArray<uint8_t>();
		return GetFileData(lump, padding);
	}

	FileReader OpenFileReader(int lump);		// opens a reader that redirects to the containing file's one.
	FileReader ReopenFileReader(int lump, bool alwayscache = false);		// opens an independent reader.
	FileReader OpenFileReader(const char* name);

	int FindLump (const char *name, int *lastlump, bool anyns=false);		// [RH] Find lumps with duplication
	int FindLumpMulti (const char **names, int *lastlump, bool anyns = false, int *nameindex = NULL); // same with multiple possible names
	int FindLumpFullName(const char* name, int* lastlump, bool noext = false);
	bool CheckFileName (int lump, const char *name);	// [RH] True if lump's name == name

	int FindFileWithExtensions(const char* name, const char* const* exts, int count);
	int FindResource(int resid, const char* type, int filenum = -1) const noexcept;
	int GetResource(int resid, const char* type, int filenum = -1) const;


	static uint32_t LumpNameHash (const char *name);		// [RH] Create hash key from an 8-char name

	int FileLength (int lump) const;
	int GetFileOffset (int lump);					// [RH] Returns offset of lump in the wadfile
	int GetFileFlags (int lump);					// Return the flags for this lump
	void GetFileShortName (char *to, int lump) const;	// [RH] Copies the lump name to to using uppercopy
	void GetFileShortName (FString &to, int lump) const;
	const char* GetFileShortName(int lump) const;
	const char *GetFileFullName (int lump, bool returnshort = true) const;	// [RH] Returns the lump's full name
	FString GetFileFullPath (int lump) const;		// [RH] Returns wad's name + lump's full name
	int GetFileContainer (int lump) const;				// [RH] Returns wadnum for a specified lump
	int GetFileNamespace (int lump) const;			// [RH] Returns the namespace a lump belongs to
	void SetFileNamespace(int lump, int ns);
	int GetResourceId(int lump) const;				// Returns the RFF index number for this lump
	const char* GetResourceType(int lump) const;
	bool CheckFileName (int lump, const char *name) const;	// [RH] Returns true if the names match
	unsigned GetFilesInFolder(const char *path, TArray<FolderEntry> &result, bool atomic) const;

	int GetNumEntries() const
	{
		return NumEntries;
	}

	int GetNumWads() const
	{
		return Files.Size();
	}

	void AddLump(FResourceLump* lump);
	int AddExternalFile(const char *filename);
	int AddFromBuffer(const char* name, const char* type, char* data, int size, int id, int flags);
	FileReader* GetFileReader(int wadnum);	// Gets a FileReader object to the entire WAD
	void InitHashChains();

	// Blood stuff
	FResourceLump* Lookup(const char* name, const char* type);
	FResourceLump* Lookup(unsigned int id, const char* type);

	FResourceLump* GetFileAt(int no);

	const void* Lock(int lump);
	void Unlock(int lump);
	const void* Get(int lump);
	static const void* Lock(FResourceLump* lump);
	static void Unlock(FResourceLump* lump);
	static const void* Load(FResourceLump* lump);;

protected:

	struct LumpRecord;

	TArray<FResourceFile *> Files;
	TArray<LumpRecord> FileInfo;

	TArray<uint32_t> Hashes;	// one allocation for all hash lists.
	uint32_t *FirstLumpIndex;	// [RH] Hashing stuff moved out of lumpinfo structure
	uint32_t *NextLumpIndex;

	uint32_t *FirstLumpIndex_FullName;	// The same information for fully qualified paths from .zips
	uint32_t *NextLumpIndex_FullName;

	uint32_t *FirstLumpIndex_NoExt;	// The same information for fully qualified paths from .zips
	uint32_t *NextLumpIndex_NoExt;

	uint32_t* FirstLumpIndex_ResId;	// The same information for fully qualified paths from .zips
	uint32_t* NextLumpIndex_ResId;

	uint32_t NumEntries = 0;					// Not necessarily the same as FileInfo.Size()
	uint32_t NumWads;

	int IwadIndex = -1;
	int MaxIwadIndex = -1;

private:
	void DeleteAll();
	void MoveLumpsInFolder(const char *);

};

extern FileSystem fileSystem;

