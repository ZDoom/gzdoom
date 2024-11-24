#pragma once
#include "fs_filesystem.h"

using FileSys::FResourceFile;

// [RH] Namespaces from BOOM.
// These are needed here in the low level part so that WAD files can be properly set up.
enum namespace_t : int {
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
	ns_maybeflat,

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
};


// extended class that adds Doom's very special short name lookup to the file system.
class FileSystem : public FileSys::FileSystem
{
private:
	struct FileEntry
	{
		char ShortName[9];
		uint8_t Namespace;
		uint8_t Flags;
		uint32_t HashFirst;
		uint32_t HashNext;
	};

	const int wadflags = FileSys::FResourceFile::NO_EXTENSIONS | FileSys::FResourceFile::NO_FOLDERS | FileSys::FResourceFile::SHORTNAMES;
	const uint32_t NULL_INDEX = 0xffffffff;

	TArray<FileEntry> files;
	int skin_namespc = ns_firstskin;


	void Start(FileSys::FileSystemMessageFunc Printf);
	void SetupName(int fileindex);
	bool IsMarker(int lump, const char* marker) noexcept;
	void SetNamespace(int filenum, const char* startmarker, const char* endmarker, namespace_t space, FileSys::FileSystemMessageFunc Printf, bool flathack = false);
	void SkinHack(int filenum, FileSys::FileSystemMessageFunc Printf);
	void SetupNamespace(int filenum, FileSys::FileSystemMessageFunc Printf);

	using Super = FileSys::FileSystem;

public:
	enum
	{
		LUMPF_MAYBEFLAT = 1,
		LUMPF_FULLPATH = 2,
	};

	bool InitFiles(std::vector<std::string>& filenames, FileSys::FileSystemFilterInfo* filter = nullptr, FileSys::FileSystemMessageFunc Printf = nullptr, bool allowduplicates = false) override;
	void InitHashChains() override;

	int CheckNumForAnyName(const char* cname, namespace_t namespc = ns_global) const;

	int CheckNumForName(const char* name, int namespc) const;
	int CheckNumForName(const char* name, int namespc, int wadfile, bool exact = true) const;
	int GetNumForName(const char* name, int namespc) const;

	inline int CheckNumForName(const uint8_t* name) const { return CheckNumForName((const char*)name, ns_global); }
	inline int CheckNumForName(const char* name) const { return CheckNumForName(name, ns_global); }
	inline int CheckNumForName(const uint8_t* name, int ns) const { return CheckNumForName((const char*)name, ns); }
	inline int GetNumForName(const char* name) const { return GetNumForName(name, ns_global); }
	inline int GetNumForName(const uint8_t* name) const { return GetNumForName((const char*)name); }
	inline int GetNumForName(const uint8_t* name, int ns) const { return GetNumForName((const char*)name, ns); }

	int FindLump(const char* name, int* lastlump, bool anyns = false);		// [RH] Find lumps with duplication
	int FindLumpMulti(const char** names, int* lastlump, bool anyns = false, int* nameindex = nullptr); // same with multiple possible names

	bool CheckFileName(int lump, const char* name) const noexcept
	{
		if ((size_t)lump >= files.size())
			return false;

		return !strnicmp(files[lump].ShortName, name, 8);
	}

	const char* GetFileShortName(int lump) const noexcept
	{
		if ((size_t)lump >= files.size())
			return nullptr;
		else
			return files[lump].ShortName;
	}

	int GetFileNamespace(int lump) const noexcept
	{
		if ((size_t)lump >= files.size())
			return ns_global;
		else
			return files[lump].Namespace;
	}

	void SetFileNamespace(int lump, int ns) noexcept
	{
		if ((size_t)lump < files.size()) files[lump].Namespace = ns;
	}

	// This is only for code that wants to edit the names before the game starts.
	char* GetShortName(int i);

};

inline FileSystem fileSystem;

