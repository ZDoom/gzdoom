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

	// The container for the IWAD
	int GetBaseNum() { return BaseIndex; }
	void SetBaseNum(int x) { BaseIndex = x; }

	int GetMaxBaseNum() { return MaxBaseIndex; }
	void SetMaxBaseNum(int x) { MaxBaseIndex = x; }

	int CheckIfContainerLoaded (const char *name) noexcept;
	void AddAdditionalFile(const char* filename, FileReader* wadinfo = NULL) {}

	const char *GetContainerName (int container) const noexcept;
	const char *GetContainerFullName (int container) const noexcept;

	int GetFirstEntry(int container) const noexcept;
	int GetLastEntry(int container) const noexcept;
    int GetEntryCount(int container) const noexcept;
	int GetContainerFlags(int container) const noexcept;

	int FindFile (const char *cname, bool ignoreext = false) const;
	int GetFileInContainer (const char *name, int wadfile) const;
	int GetFile (const char *name) const;

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

	void ReadFile (int filenum, void *dest);
	// These should only be used if the file data really needs padding.
	FileData ReadFile (int filenum);
	FileData ReadFileFullName(const char* name) { return ReadFile(GetFile(name)); }

	FileReader OpenFileReader(int filenum, int readertype, int readerflags);		// opens a reader that redirects to the containing file's one.
	FileReader OpenFileReader(const char* name);
	FileReader ReopenFileReader(const char* name, bool alwayscache = false);
	FileReader OpenFileReader(int filenum)
	{
		return OpenFileReader(filenum, READER_SHARED, READERFLAG_SEEKABLE);
	}

	FileReader ReopenFileReader(int filenum, bool alwayscache = false)
	{
		return OpenFileReader(filenum, alwayscache ? READER_CACHED : READER_NEW, READERFLAG_SEEKABLE);
	}


	int FindLumpFullName(const char* name, int* lastlump, bool noext = false);

	int FindFileWithExtensions(const char* name, const char* const* exts, int count) const;
	int FindResource(int resid, const char* type, int filenum = -1) const noexcept;
	int GetResource(int resid, const char* type, int filenum = -1) const;


	ptrdiff_t FileLength(int filenum) const;
	int GetFileFlags (int filenum);					// Return the flags for this filenum
	const char* GetFileName(int filenum) const;	// Gets uninterpreted name from the FResourceFile
	std::string GetFileFullPath (int filenum) const;		// [RH] Returns wad's name + filenum's full name
	int GetFileContainer (int filenum) const;			
	// [RH] Returns container for a specified filenum
	int GetResourceId(int filenum) const;				// Returns the RFF index number for this filenum
	const char* GetResourceType(int filenum) const;
	unsigned GetFilesInFolder(const char *path, std::vector<FolderEntry> &result, bool atomic) const;

	int GetFileCount() const
	{
		return NumEntries;
	}

	int GetContainerCount() const
	{
		return (int)Files.size();
	}

	int AddFromBuffer(const char* name, char* data, int size, int id, int flags);
	FileReader* GetFileReader(int container);	// Gets a FileReader object to the entire WAD

protected:

	struct LumpRecord;
	const uint32_t NULL_INDEX = 0xffffffff;

	std::vector<FResourceFile *> Files;
	std::vector<LumpRecord> FileInfo;

	std::vector<uint32_t> Hashes;	// one allocation for all hash lists.

	uint32_t *FirstFileIndex_FullName = nullptr;	// The same information for fully qualified paths from .zips
	uint32_t *NextFileIndex_FullName = nullptr;

	uint32_t *FirstFileIndex_NoExt = nullptr;	// The same information for fully qualified paths from .zips
	uint32_t *NextFileIndex_NoExt = nullptr;

	uint32_t* FirstFileIndex_ResId = nullptr;	// The same information for fully qualified paths from .zips
	uint32_t* NextFileIndex_ResId = nullptr;

	uint32_t NumEntries = 0;					// Not necessarily the same as FileInfo.Size()
	uint32_t NumWads = 0;

	int BaseIndex = -1;
	int MaxBaseIndex = -1;

	StringPool* stringpool = nullptr;

private:
	void DeleteAll();
	void MoveFilesInFolder(const char *);
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