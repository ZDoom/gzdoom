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
	
struct FolderEntry
{
	const char *name;
	unsigned lumpnum;
};

class FileSystem
{
public:
	FileSystem();
	virtual ~FileSystem ();
	// do not copy!
	FileSystem(const FileSystem& other) = delete;
	FileSystem& operator =(const FileSystem& other) = delete;

	bool Initialize(std::vector<std::string>& filenames, FileSystemFilterInfo* filter = nullptr, FileSystemMessageFunc Printf = nullptr, bool allowduplicates = false);

	// The wadnum for the IWAD
	int GetIwadNum() { return IwadIndex; }
	void SetIwadNum(int x) { IwadIndex = x; }

	int GetMaxIwadNum() { return MaxIwadIndex; }
	void SetMaxIwadNum(int x) { MaxIwadIndex = x; }

	int CheckIfResourceFileLoaded (const char *name) noexcept;
	void AddAdditionalFile(const char* filename, FileReader* wadinfo = NULL) {}

	const char *GetResourceFileName (int filenum) const noexcept;
	const char *GetResourceFileFullName (int wadnum) const noexcept;

	int GetFirstEntry(int wadnum) const noexcept;
	int GetLastEntry(int wadnum) const noexcept;
    int GetEntryCount(int wadnum) const noexcept;
	int GetResourceFileFlags(int wadnum) const noexcept;

	int CheckNumForFullName (const char *cname, bool ignoreext = false) const;
	int CheckNumForFullNameInFile (const char *name, int wadfile) const;
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

	void RenameFile(int num, const char* fn);
	bool CreatePathlessCopy(const char* name, int id, int flags);

	void ReadFile (int lump, void *dest);
	// These should only be used if the file data really needs padding.
	FileData ReadFile (int lump);
	FileData ReadFileFullName(const char* name) { return ReadFile(GetNumForFullName(name)); }

	FileReader OpenFileReader(int lump, int readertype, int readerflags);		// opens a reader that redirects to the containing file's one.
	FileReader OpenFileReader(const char* name);
	FileReader ReopenFileReader(const char* name, bool alwayscache = false);
	FileReader OpenFileReader(int lump)
	{
		return OpenFileReader(lump, READER_SHARED, READERFLAG_SEEKABLE);
	}

	FileReader ReopenFileReader(int lump, bool alwayscache = false)
	{
		return OpenFileReader(lump, alwayscache ? READER_CACHED : READER_NEW, READERFLAG_SEEKABLE);
	}


	int FindLumpFullName(const char* name, int* lastlump, bool noext = false);

	int FindFileWithExtensions(const char* name, const char* const* exts, int count) const;
	int FindResource(int resid, const char* type, int filenum = -1) const noexcept;
	int GetResource(int resid, const char* type, int filenum = -1) const;


	ptrdiff_t FileLength(int lump) const;
	int GetFileFlags (int lump);					// Return the flags for this lump
	const char* GetFileName(int lump) const;	// Gets uninterpreted name from the FResourceFile
	std::string GetFileFullPath (int lump) const;		// [RH] Returns wad's name + lump's full name
	int GetFileContainer (int lump) const;			
	// [RH] Returns wadnum for a specified lump
	int GetResourceId(int lump) const;				// Returns the RFF index number for this lump
	const char* GetResourceType(int lump) const;
	unsigned GetFilesInFolder(const char *path, std::vector<FolderEntry> &result, bool atomic) const;

	int GetNumEntries() const
	{
		return NumEntries;
	}

	int GetNumWads() const
	{
		return (int)Files.size();
	}

	int AddFromBuffer(const char* name, char* data, int size, int id, int flags);
	FileReader* GetFileReader(int wadnum);	// Gets a FileReader object to the entire WAD

protected:

	struct LumpRecord;
	const uint32_t NULL_INDEX = 0xffffffff;

	std::vector<FResourceFile *> Files;
	std::vector<LumpRecord> FileInfo;

	std::vector<uint32_t> Hashes;	// one allocation for all hash lists.

	uint32_t *FirstLumpIndex_FullName = nullptr;	// The same information for fully qualified paths from .zips
	uint32_t *NextLumpIndex_FullName = nullptr;

	uint32_t *FirstLumpIndex_NoExt = nullptr;	// The same information for fully qualified paths from .zips
	uint32_t *NextLumpIndex_NoExt = nullptr;

	uint32_t* FirstLumpIndex_ResId = nullptr;	// The same information for fully qualified paths from .zips
	uint32_t* NextLumpIndex_ResId = nullptr;

	uint32_t NumEntries = 0;					// Not necessarily the same as FileInfo.Size()
	uint32_t NumWads = 0;

	int IwadIndex = -1;
	int MaxIwadIndex = -1;

	StringPool* stringpool = nullptr;

private:
	void DeleteAll();
	void MoveLumpsInFolder(const char *);
	void AddFile(const char* filename, FileReader* wadinfo, FileSystemFilterInfo* filter, FileSystemMessageFunc Printf);
protected:

	// These two functions must be overridden by subclasses which want to extend the file system.
	virtual bool InitFiles(std::vector<std::string>& filenames, FileSystemFilterInfo* filter = nullptr, FileSystemMessageFunc Printf = nullptr, bool allowduplicates = false);
public:
	virtual void InitHashChains();

};

//djb2 hash algorithm with case insensitivity hack
inline static uint32_t MakeHash(const char* str, size_t length = SIZE_MAX)
{
	uint32_t hash = 5381;
	uint32_t c;
	while (length-- > 0 && (c = *str++)) hash = hash * 33 + (c | 32);
	return hash;
}


}