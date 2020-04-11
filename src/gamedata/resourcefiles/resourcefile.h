

#ifndef __RESFILE_H
#define __RESFILE_H

#include "files.h"

class FResourceFile;
class FTexture;

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
};

// This holds a compresed Zip entry with all needed info to decompress it.
struct FCompressedBuffer
{
	unsigned mSize;
	unsigned mCompressedSize;
	int mMethod;
	int mZipFlags;
	unsigned mCRC32;
	char *mBuffer;

	bool Decompress(char *destbuffer);
	void Clean()
	{
		mSize = mCompressedSize = 0;
		if (mBuffer != nullptr)
		{
			delete[] mBuffer;
			mBuffer = nullptr;
		}
	}
};

struct FResourceLump
{
	friend class FResourceFile;
	friend class FWadFile;	// this still needs direct access.

	int				LumpSize;
protected:
	FString			FullName;		// only valid for files loaded from a non-wad archive
public:
	uint8_t			Flags;
	int8_t			RefCount;
	char *			Cache;
	FResourceFile *	Owner;

	FResourceLump()
	{
		Cache = NULL;
		Owner = NULL;
		Flags = 0;
		RefCount = 0;
	}

	virtual ~FResourceLump();
	virtual FileReader *GetReader();
	virtual FileReader NewReader();
	virtual int GetFileOffset() { return -1; }
	virtual int GetIndexNum() const { return 0; }
	virtual int GetNamespace() const { return 0; }
	void LumpNameSetup(FString iname);
	void CheckEmbedded();
	virtual FCompressedBuffer GetRawData();

	void *CacheLump();
	int ReleaseCache();

	const char* getName() { return FullName.GetChars(); }

protected:
	virtual int FillCache() { return -1; }

};

class FResourceFile
{
public:
	FileReader Reader;
	FString FileName;
protected:
	uint32_t NumLumps;
	FString Hash;

	FResourceFile(const char *filename);
	FResourceFile(const char *filename, FileReader &r);

	// for archives that can contain directories
	void GenerateHash();
	void PostProcessArchive(void *lumps, size_t lumpsize);

private:
	uint32_t FirstLump;

	int FilterLumps(FString filtername, void *lumps, size_t lumpsize, uint32_t max);
	int FilterLumpsByGameType(int gametype, void *lumps, size_t lumpsize, uint32_t max);
	bool FindPrefixRange(FString filter, void *lumps, size_t lumpsize, uint32_t max, uint32_t &start, uint32_t &end);
	void JunkLeftoverFilters(void *lumps, size_t lumpsize, uint32_t max);
	static FResourceFile *DoOpenResourceFile(const char *filename, FileReader &file, bool quiet, bool containeronly);

public:
	static FResourceFile *OpenResourceFile(const char *filename, FileReader &file, bool quiet = false, bool containeronly = false);
	static FResourceFile *OpenResourceFile(const char *filename, bool quiet = false, bool containeronly = false);
	static FResourceFile *OpenResourceFileFromLump(int lumpnum, bool quiet = false, bool containeronly = false);
	static FResourceFile *OpenDirectory(const char *filename, bool quiet = false);
	virtual ~FResourceFile();
    // If this FResourceFile represents a directory, the Reader object is not usable so don't return it.
    FileReader *GetReader() { return Reader.isOpen()? &Reader : nullptr; }
	uint32_t LumpCount() const { return NumLumps; }
	uint32_t GetFirstLump() const { return FirstLump; }
	void SetFirstLump(uint32_t f) { FirstLump = f; }
	const FString &GetHash() const { return Hash; }


	virtual bool Open(bool quiet) = 0;
	virtual FResourceLump *GetLump(int no) = 0;
	FResourceLump *FindLump(const char *name);
};

struct FUncompressedLump : public FResourceLump
{
	int				Position;

	virtual FileReader *GetReader();
	virtual int FillCache();
	virtual int GetFileOffset() { return Position; }

};


// Base class for uncompressed resource files (WAD, GRP, PAK and single lumps)
class FUncompressedFile : public FResourceFile
{
protected:
	TArray<FUncompressedLump> Lumps;

	FUncompressedFile(const char *filename);
	FUncompressedFile(const char *filename, FileReader &r);
	virtual FResourceLump *GetLump(int no) { return ((unsigned)no < NumLumps)? &Lumps[no] : NULL; }
};


struct FExternalLump : public FResourceLump
{
	FString Filename;

	FExternalLump(const char *_filename, int filesize = -1);
	virtual int FillCache();

};

struct FMemoryFile : public FUncompressedFile
{
	FMemoryFile(const char *_filename, const void *sdata, int length)
		: FUncompressedFile(_filename)
	{
		Reader.OpenMemoryArray(sdata, length);
	}

    bool Open(bool quiet);


};




#endif
