

#ifndef __RESFILE_H
#define __RESFILE_H

#include <limits.h>
#include <vector>
#include <string>
#include "fs_files.h"
#include "fs_decompress.h"

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
	RESFF_MAYBEFLAT = 1,	// might be a flat inside a WAD outside F_START/END
	RESFF_FULLPATH = 2,		// contains a full path. This will trigger extended namespace checks when looking up short names.
	RESFF_EMBEDDED = 4,		// marks an embedded resource file for later processing.
	RESFF_SHORTNAME = 8,	// the stored name is a short extension-less name
	RESFF_COMPRESSED = 16,	// compressed or encrypted, i.e. cannot be read with the container file's reader.
	RESFF_NEEDFILESTART = 32,	// The real position is not known yet and needs to be calculated on access
};

enum EReaderType
{
	READER_SHARED = 0,	// returns a view into the parent's reader.
	READER_NEW = 1,		// opens a new file handle
	READER_CACHED = 2,	// returns a MemoryArrayReader
	READERFLAG_SEEKABLE = 1	// ensure the reader is seekable.
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

void SetMainThread();

class FResourceFile
{
public:
	FResourceFile(const char* filename, StringPool* sp);
	FResourceFile(const char* filename, FileReader& r, StringPool* sp);
	const char* NormalizeFileName(const char* fn, int fallbackcp = 0);
	FResourceEntry* AllocateEntries(int count);
	void GenerateHash();
	void PostProcessArchive(LumpFilterInfo* filter);
protected:
	FileReader Reader;
	const char* FileName;
	FResourceEntry* Entries = nullptr;
	uint32_t NumLumps;
	char Hash[48];
	StringPool* stringpool;

	// for archives that can contain directories
	virtual void SetEntryAddress(uint32_t entry)
	{
		Entries[entry].Flags &= ~RESFF_NEEDFILESTART;
	}
	bool IsFileInFolder(const char* const resPath);
	void CheckEmbedded(uint32_t entry, LumpFilterInfo* lfi);

private:
	uint32_t FirstLump;

	int FilterLumps(const std::string& filtername, uint32_t max);
	bool FindPrefixRange(const char* filter, uint32_t max, uint32_t &start, uint32_t &end);
	void JunkLeftoverFilters(uint32_t max);
	void FindCommonFolder(LumpFilterInfo* filter);
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

	// default is the safest reader type.
	virtual FileReader GetEntryReader(uint32_t entry, int readertype = READER_NEW, int flags = READERFLAG_SEEKABLE);

	int GetEntryFlags(uint32_t entry)
	{
		return (entry < NumLumps) ? Entries[entry].Flags : 0;
	}

	int GetEntryNamespace(uint32_t entry)
	{
		return (entry < NumLumps) ? Entries[entry].Namespace : (int)ns_hidden;
	}

	int GetEntryResourceID(uint32_t entry)
	{
		return (entry < NumLumps) ? Entries[entry].ResourceID : -1;
	}

	const char* getName(uint32_t entry)
	{
		return (entry < NumLumps) ? Entries[entry].FileName : nullptr;
	}

	virtual FileData Read(uint32_t entry);

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
