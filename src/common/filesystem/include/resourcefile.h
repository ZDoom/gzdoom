

#ifndef __RESFILE_H
#define __RESFILE_H

#include <limits.h>
#include <vector>
#include <string>
#include "fs_files.h"

namespace FileSys {
	
class StringPool;
std::string ExtractBaseName(const char* path, bool include_extension = false);
void strReplace(std::string& str, const char* from, const char* to);

// user context in which the file system gets opened. This also contains a few callbacks to avoid direct dependencies on the engine.
struct LumpFilterInfo
{
	std::vector<std::string> gameTypeFilter;	// this can contain multiple entries

	// The following are for checking if the root directory of a zip can be removed.
	std::vector<std::string> reservedFolders;
	std::vector<std::string> requiredPrefixes;
	std::vector<std::string> embeddings;
	std::vector<std::string> blockednames;			// File names that will never be accepted (e.g. dehacked.exe for Doom)
	std::function<bool(const char*, const char*)> filenamecheck;	// for scanning directories, this allows to eliminate unwanted content.
	std::function<void()> postprocessFunc;
};

enum class FSMessageLevel
{
	Error = 1,
	Warning = 2,
	Attention = 3,
	Message = 4,
	DebugWarn = 5,
	DebugNotify = 6,
};

// pass the text output function as parameter to avoid a hard dependency on higher level code.
using FileSystemMessageFunc = int(*)(FSMessageLevel msglevel, const char* format, ...);


class FResourceFile;

// [RH] Namespaces from BOOM.
// These are needed here in the low level part so that WAD files can be properly set up.
typedef enum {
	ns_hidden = -1,

	ns_global = 0,
	ns_sprites,
	ns_flats,
	ns_colormaps,
	ns_acslibrary,
	ns_newtextures,
	ns_bloodraw,	// no longer used - kept for ZScript.
	ns_bloodsfx,	// no longer used - kept for ZScript.
	ns_bloodmisc,	// no longer used - kept for ZScript.
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
	LUMPF_MAYBEFLAT = 1,	// might be a flat outside F_START/END
	LUMPF_FULLPATH = 2,		// contains a full path. This will trigger extended namespace checks when looking up short names.
	LUMPF_EMBEDDED = 4,		// marks an embedded resource file for later processing.
	LUMPF_SHORTNAME = 8,	// the stored name is a short extension-less name
	LUMPF_COMPRESSED = 16,	// compressed or encrypted, i.e. cannot be read with the container file's reader.
	
	RESFF_MAYBEFLAT = 1,	// might be a flat inside a WAD outside F_START/END
	RESFF_FULLPATH = 2,		// contains a full path. This will trigger extended namespace checks when looking up short names.
	RESFF_EMBEDDED = 4,		// marks an embedded resource file for later processing.
	RESFF_SHORTNAME = 8,	// the stored name is a short extension-less name
	RESFF_COMPRESSED = 16,	// compressed or encrypted, i.e. cannot be read with the container file's reader.
	RESFF_NEEDFILESTART = 32,	// The real position is not known yet and needs to be calculated on access
	
};

struct FResourceLump
{
protected:
	friend class FResourceFile;
	friend class FWadFile;	// this still needs direct access.
	friend class FileData;
	friend class FileSystem;
	friend class FLumpFile;
	friend class FLumpReader;
	friend class FGrpFile;
	friend class F7ZFile;
	friend class FSSIFile;
	friend class FWHResFile;
	friend class FZipFile;
	friend class FPakFile;
	friend class FRFFFile;
	friend class FDirectory;
	friend int lumpcmp(const void* a, const void* b);


	int				LumpSize;
	int				RefCount;
//protected:
	const char*		FullName;
//public:
	uint8_t			Flags;
	char *			Cache;
	FResourceFile *	Owner;

public:
	FResourceLump()
	{
		Cache = NULL;
		Owner = NULL;
		Flags = 0;
		RefCount = 0;
		FullName = "";
		LumpSize = 0;
	}
	virtual ~FResourceLump();

protected:

	virtual FileReader *GetReader();
	virtual FileReader NewReader();
	virtual int GetFileOffset() { return -1; }
	virtual int GetIndexNum() const { return -1; }
	virtual int GetNamespace() const { return 0; }
	void LumpNameSetup(const char* iname, StringPool* allocator);
	void CheckEmbedded(LumpFilterInfo* lfi);

	void *Lock(); // validates the cache and increases the refcount.
	int Unlock(); // decreases the refcount and frees the buffer

	unsigned Size() const{ return LumpSize; }
	int LockCount() const { return RefCount; }
	const char* getName() { return FullName; }
	void clearName() { FullName = ""; }

protected:
	virtual int FillCache() { return -1; }

};

struct FResourceEntry
{
	size_t Length;
	size_t CompressedSize;
	const char* FileName;
	size_t Position;
	int ResourceID;
	uint32_t CRC32;
	uint16_t Flags;
	uint16_t Method;
	int16_t Namespace;
};

class FResourceFile
{
public:
protected:
	FileReader Reader;
	const char* FileName;
	FResourceEntry* Entries = nullptr;
	uint32_t NumLumps;
	char Hash[48];
	StringPool* stringpool;

	FResourceFile(const char *filename, StringPool* sp);
	FResourceFile(const char *filename, FileReader &r, StringPool* sp);

	const char* NormalizeFileName(const char* fn, int fallbackcp = 0);
	void AllocateEntries(int count);

	// for archives that can contain directories
	void GenerateHash();
	void PostProcessArchive(void *lumps, size_t lumpsize, LumpFilterInfo *filter);

private:
	uint32_t FirstLump;

	int FilterLumps(const std::string& filtername, void *lumps, size_t lumpsize, uint32_t max);
	bool FindPrefixRange(const char* filter, void *lumps, size_t lumpsize, uint32_t max, uint32_t &start, uint32_t &end);
	void JunkLeftoverFilters(void *lumps, size_t lumpsize, uint32_t max);
	static FResourceFile *DoOpenResourceFile(const char *filename, FileReader &file, bool containeronly, LumpFilterInfo* filter, FileSystemMessageFunc Printf, StringPool* sp);

public:
	static FResourceFile *OpenResourceFile(const char *filename, FileReader &file, bool containeronly = false, LumpFilterInfo* filter = nullptr, FileSystemMessageFunc Printf = nullptr, StringPool* sp = nullptr);
	static FResourceFile *OpenResourceFile(const char *filename, bool containeronly = false, LumpFilterInfo* filter = nullptr, FileSystemMessageFunc Printf = nullptr, StringPool* sp = nullptr);
	static FResourceFile *OpenDirectory(const char *filename, LumpFilterInfo* filter = nullptr, FileSystemMessageFunc Printf = nullptr, StringPool* sp = nullptr);
	virtual ~FResourceFile();
    // If this FResourceFile represents a directory, the Reader object is not usable so don't return it.
	FileReader *GetContainerReader() { return Reader.isOpen()? &Reader : nullptr; }
	const char* GetFileName() const { return FileName; }
	uint32_t GetFirstEntry() const { return FirstLump; }
	void SetFirstLump(uint32_t f) { FirstLump = f; }
	const char* GetHash() const { return Hash; }

	virtual FResourceLump* GetLump(int no) { throw FileSystemException("GetLump base function called."); }

	int EntryCount() const { return NumLumps; }
	int FindEntry(const char* name);

	size_t Length(uint32_t entry)
	{
		return (entry < NumLumps) ? Entries[entry].Length : 0;
	}
	size_t Offset(uint32_t entry)
	{
		return (entry < NumLumps) ? Entries[entry].Position : 0;
	}

	virtual FileReader GetEntryReader(uint32_t entry, bool newreader = true)
	{
		auto l = GetLump(entry);
		return l ? l->NewReader() : FileReader();
	}

	int GetEntryFlags(uint32_t entry)
	{
		return (entry < NumLumps) ? Entries[entry].Flags : 0;
	}

	int GetEntryNamespace(uint32_t entry)
	{
		return (entry < NumLumps) ? Entries[entry].Namespace : ns_hidden;
	}

	int GetEntryResourceID(uint32_t entry)
	{
		return (entry < NumLumps) ? Entries[entry].ResourceID : -1;
	}

	const char* getName(uint32_t entry)
	{
		return (entry < NumLumps) ? Entries[entry].FileName : nullptr;
	}

	virtual FileData Read(int entry)
	{
		auto fr = GetEntryReader(entry, false);
		return fr.Read();
	}

	virtual FCompressedBuffer GetRawData(uint32_t entry);

	FileReader Destroy()
	{
		auto fr = std::move(Reader);
		delete this;
		return fr;
	}


};


}

#endif
