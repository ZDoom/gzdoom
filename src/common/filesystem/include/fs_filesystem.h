#pragma once
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//	File system  I/O functions.
//
//-----------------------------------------------------------------------------



#include "fs_files.h"
#include "resourcefile.h"

namespace FileSys {
	
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
	FileData() { lump = nullptr; }
	const void *GetMem () { return lump->Cache; }
	size_t GetSize () { return lump->LumpSize; }
	const char* GetString () const { return (const char*)lump->Cache; }
	const uint8_t* GetBytes() const { return (const uint8_t*)lump->Cache; }

	FileData& operator = (const FileData& copy) = delete;

	FileData(const FileData& copy)
	{
		lump = copy.lump;
		lump->Lock();
	}

	~FileData()
	{
		if (lump) lump->Unlock();
	}


private:
	FileData(FResourceLump* nlump)
	{
		lump = nlump;
		if (lump) lump->Lock();
	}

	FResourceLump* lump;

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
	FileSystem();
	~FileSystem ();

	// The wadnum for the IWAD
	int GetIwadNum() { return IwadIndex; }
	void SetIwadNum(int x) { IwadIndex = x; }

	int GetMaxIwadNum() { return MaxIwadIndex; }
	void SetMaxIwadNum(int x) { MaxIwadIndex = x; }

	bool InitSingleFile(const char *filename, FileSystemMessageFunc Printf = nullptr);
	bool InitMultipleFiles (std::vector<std::string>& filenames, LumpFilterInfo* filter = nullptr, FileSystemMessageFunc Printf = nullptr, bool allowduplicates = false, FILE* hashfile = nullptr);
	void AddFile (const char *filename, FileReader *wadinfo, LumpFilterInfo* filter, FileSystemMessageFunc Printf, FILE* hashfile);
	int CheckIfResourceFileLoaded (const char *name) noexcept;
	void AddAdditionalFile(const char* filename, FileReader* wadinfo = NULL) {}

	const char *GetResourceFileName (int filenum) const noexcept;
	const char *GetResourceFileFullName (int wadnum) const noexcept;

	int GetFirstEntry(int wadnum) const noexcept;
	int GetLastEntry(int wadnum) const noexcept;
    int GetEntryCount(int wadnum) const noexcept;

	int CheckNumForName (const char *name, int namespc) const;
	int CheckNumForName (const char *name, int namespc, int wadfile, bool exact = true) const;
	int GetNumForName (const char *name, int namespc) const;

	inline int CheckNumForName (const uint8_t *name) const { return CheckNumForName ((const char *)name, ns_global); }
	inline int CheckNumForName (const char *name) const { return CheckNumForName (name, ns_global); }
	inline int CheckNumForName (const uint8_t *name, int ns) const { return CheckNumForName ((const char *)name, ns); }
	inline int GetNumForName (const char *name) const { return GetNumForName (name, ns_global); }
	inline int GetNumForName (const uint8_t *name) const { return GetNumForName ((const char *)name); }
	inline int GetNumForName (const uint8_t *name, int ns) const { return GetNumForName ((const char *)name, ns); }

	int CheckNumForFullName (const char *cname, bool trynormal = false, int namespc = ns_global, bool ignoreext = false) const;
	int CheckNumForFullName (const char *name, int wadfile) const;
	int GetNumForFullName (const char *name) const;
	int FindFile(const char* name) const
	{
		return CheckNumForFullName(name);
	}

	bool FileExists(const char* name) const
	{
		return FindFile(name) >= 0;
	}

	bool FileExists(const std::string& name) const
	{
		return FindFile(name.c_str()) >= 0;
	}

	LumpShortName& GetShortName(int i);	// may only be called before the hash chains are set up.
	void RenameFile(int num, const char* fn);
	bool CreatePathlessCopy(const char* name, int id, int flags);

	void ReadFile (int lump, void *dest);
	// These should only be used if the file data really needs padding.
	FileData ReadFile (int lump);
	FileData ReadFile (const char *name) { return ReadFile (GetNumForName (name)); }
	FileData ReadFileFullName(const char* name) { return ReadFile(GetNumForFullName(name)); }

	FileReader OpenFileReader(int lump);		// opens a reader that redirects to the containing file's one.
	FileReader ReopenFileReader(int lump, bool alwayscache = false);		// opens an independent reader.
	FileReader OpenFileReader(const char* name);

	int FindLump (const char *name, int *lastlump, bool anyns=false);		// [RH] Find lumps with duplication
	int FindLumpMulti (const char **names, int *lastlump, bool anyns = false, int *nameindex = NULL); // same with multiple possible names
	int FindLumpFullName(const char* name, int* lastlump, bool noext = false);
	bool CheckFileName (int lump, const char *name);	// [RH] True if lump's name == name

	int FindFileWithExtensions(const char* name, const char* const* exts, int count) const;
	int FindResource(int resid, const char* type, int filenum = -1) const noexcept;
	int GetResource(int resid, const char* type, int filenum = -1) const;


	static uint32_t LumpNameHash (const char *name);		// [RH] Create hash key from an 8-char name

	int FileLength (int lump) const;
	int GetFileOffset (int lump);					// [RH] Returns offset of lump in the wadfile
	int GetFileFlags (int lump);					// Return the flags for this lump
	const char* GetFileShortName(int lump) const;
	const char *GetFileFullName (int lump, bool returnshort = true) const;	// [RH] Returns the lump's full name
	std::string GetFileFullPath (int lump) const;		// [RH] Returns wad's name + lump's full name
	int GetFileContainer (int lump) const;				// [RH] Returns wadnum for a specified lump
	int GetFileNamespace (int lump) const;			// [RH] Returns the namespace a lump belongs to
	void SetFileNamespace(int lump, int ns);
	int GetResourceId(int lump) const;				// Returns the RFF index number for this lump
	const char* GetResourceType(int lump) const;
	bool CheckFileName (int lump, const char *name) const;	// [RH] Returns true if the names match
	unsigned GetFilesInFolder(const char *path, std::vector<FolderEntry> &result, bool atomic) const;

	int GetNumEntries() const
	{
		return NumEntries;
	}

	int GetNumWads() const
	{
		return (int)Files.size();
	}

	void AddLump(FResourceLump* lump);
	int AddExternalFile(const char *filename);
	int AddFromBuffer(const char* name, const char* type, char* data, int size, int id, int flags);
	FileReader* GetFileReader(int wadnum);	// Gets a FileReader object to the entire WAD
	void InitHashChains();
	FResourceLump* GetFileAt(int no);

protected:

	struct LumpRecord;

	std::vector<FResourceFile *> Files;
	std::vector<LumpRecord> FileInfo;

	std::vector<uint32_t> Hashes;	// one allocation for all hash lists.
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

	StringPool* stringpool;

private:
	void DeleteAll();
	void MoveLumpsInFolder(const char *);

};

}