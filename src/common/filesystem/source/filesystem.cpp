/*
** filesystem.cpp
**
**---------------------------------------------------------------------------
** Copyright 1998-2009 Randy Heit
** Copyright 2005-2020 Christoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
**
*/


// HEADER FILES ------------------------------------------------------------

#include <miniz.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <inttypes.h>

#include "resourcefile.h"
#include "fs_filesystem.h"
#include "fs_findfile.h"
#include "md5.hpp"
#include "fs_stringpool.h"

namespace FileSys {
	
// MACROS ------------------------------------------------------------------

#define NULL_INDEX		(0xffffffff)

static void UpperCopy(char* to, const char* from)
{
	int i;

	for (i = 0; i < 8 && from[i]; i++)
		to[i] = toupper(from[i]);
	for (; i < 8; i++)
		to[i] = 0;
}


//djb2
static uint32_t MakeHash(const char* str, size_t length = SIZE_MAX)
{
	uint32_t hash = 5381;
	uint32_t c;
	while (length-- > 0 && (c = *str++)) hash = hash * 33 + (c | 32);
	return hash;
}

static void md5Hash(FileReader& reader, uint8_t* digest) 
{
	using namespace md5;

	md5_state_t state;
	md5_init(&state);
	md5_byte_t buffer[4096];
	while (auto len = reader.Read(buffer, 4096))
	{
		md5_append(&state, buffer, len);
	}
	md5_finish(&state, digest);
}


struct FileSystem::LumpRecord
{
	FResourceFile *resfile;
	LumpShortName shortName;
	const char*	LongName;
	int			resindex;
	int16_t		rfnum;		// this is not necessarily the same as resfile's index!
	int16_t		Namespace;
	int			resourceId;
	int			flags;

	void SetFromLump(FResourceFile* file, int fileindex, int filenum, StringPool* sp, const char* name = nullptr)
	{
		resfile = file;
		resindex = fileindex;
		rfnum = filenum;
		flags = 0;

		auto lflags = file->GetEntryFlags(fileindex);
		if (!name) name = file->getName(fileindex);
		if (lflags & RESFF_SHORTNAME)
		{
			UpperCopy(shortName.String, name);
			shortName.String[8] = 0;
			LongName = "";
			Namespace = file->GetEntryNamespace(fileindex);
			resourceId = -1;
		}
		else if ((lflags & RESFF_EMBEDDED) || !name || !*name)
		{
			shortName.qword = 0;
			LongName = "";
			Namespace = ns_hidden;
			resourceId = -1;
		}
		else
		{
			LongName = name;
			resourceId = file->GetEntryResourceID(fileindex);

			// Map some directories to WAD namespaces.
			// Note that some of these namespaces don't exist in WADS.
			// CheckNumForName will handle any request for these namespaces accordingly.
			Namespace = !strncmp(LongName, "flats/", 6) ? ns_flats :
				!strncmp(LongName, "textures/", 9) ? ns_newtextures :
				!strncmp(LongName, "hires/", 6) ? ns_hires :
				!strncmp(LongName, "sprites/", 8) ? ns_sprites :
				!strncmp(LongName, "voxels/", 7) ? ns_voxels :
				!strncmp(LongName, "colormaps/", 10) ? ns_colormaps :
				!strncmp(LongName, "acs/", 4) ? ns_acslibrary :
				!strncmp(LongName, "voices/", 7) ? ns_strifevoices :
				!strncmp(LongName, "patches/", 8) ? ns_patches :
				!strncmp(LongName, "graphics/", 9) ? ns_graphics :
				!strncmp(LongName, "sounds/", 7) ? ns_sounds :
				!strncmp(LongName, "music/", 6) ? ns_music :
				!strchr(LongName, '/') ? ns_global :
				ns_hidden;

			if (Namespace == ns_hidden) shortName.qword = 0;
			else if (strstr(LongName, ".{"))
			{
				std::string longName = LongName;
				ptrdiff_t encodedResID = longName.find_last_of(".{");
				if (resourceId == -1 && (size_t)encodedResID != std::string::npos)
				{
					const char* p = LongName + encodedResID;
					char* q;
					int id = (int)strtoull(p + 2, &q, 10);	// only decimal numbers allowed here.
					if (q[0] == '}' && (q[1] == '.' || q[1] == 0))
					{
						longName.erase(longName.begin() + encodedResID, longName.begin() + (q - p) + 1);
						resourceId = id;
					}
					LongName = sp->Strdup(longName.c_str());
				}
			}
			auto slash = strrchr(LongName, '/');
			std::string base = slash ? (slash + 1) : LongName;
			auto dot = base.find_last_of('.');
			if (dot != std::string::npos) base.resize(dot);
			UpperCopy(shortName.String, base.c_str());

			// Since '\' can't be used as a file name's part inside a ZIP
			// we have to work around this for sprites because it is a valid
			// frame character.
			if (Namespace == ns_sprites || Namespace == ns_voxels || Namespace == ns_hires)
			{
				char* c;

				while ((c = (char*)memchr(shortName.String, '^', 8)))
				{
					*c = '\\';
				}
			}
		}
	}
};

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void PrintLastError (FileSystemMessageFunc Printf);

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// CODE --------------------------------------------------------------------

FileSystem::FileSystem()
{
}

FileSystem::~FileSystem ()
{
	DeleteAll();
}

void FileSystem::DeleteAll ()
{
	Hashes.clear();
	NumEntries = 0;

	FileInfo.clear();
	for (int i = (int)Files.size() - 1; i >= 0; --i)
	{
		delete Files[i];
	}
	Files.clear();
	if (stringpool != nullptr) delete stringpool;
	stringpool = nullptr;
}

//==========================================================================
//
// InitMultipleFiles
//
// Pass a null terminated list of files to use. All files are optional,
// but at least one file must be found. File names can appear multiple
// times. The name searcher looks backwards, so a later file can
// override an earlier one.
//
//==========================================================================

bool FileSystem::InitSingleFile(const char* filename, FileSystemMessageFunc Printf)
{
	std::vector<std::string> filenames = { filename };
	return InitMultipleFiles(filenames, nullptr, Printf);
}

bool FileSystem::InitMultipleFiles (std::vector<std::string>& filenames, LumpFilterInfo* filter, FileSystemMessageFunc Printf, bool allowduplicates)
{
	int numfiles;

	// the first call here will designate a main thread which may use shared file readers. All other thewads have to open new file handles.
	SetMainThread();
	// open all the files, load headers, and count lumps
	DeleteAll();
	numfiles = 0;

	stringpool = new StringPool(true);
	stringpool->shared = true;	// will be used by all owned resource files.

	// first, check for duplicates
	if (!allowduplicates)
	{
		for (size_t i=0;i<filenames.size(); i++)
		{
			for (size_t j=i+1;j<filenames.size(); j++)
			{
				if (filenames[i] == filenames[j])
				{
					filenames.erase(filenames.begin() + j);
					j--;
				}
			}
		}
	}

	for(size_t i=0;i<filenames.size(); i++)
	{
		AddFile(filenames[i].c_str(), nullptr, filter, Printf);

		if (i == (unsigned)MaxIwadIndex) MoveLumpsInFolder("after_iwad/");
		std::string path = "filter/%s";
		path += Files.back()->GetHash();
		MoveLumpsInFolder(path.c_str());
	}

	NumEntries = (uint32_t)FileInfo.size();
	if (NumEntries == 0)
	{
		return false;
	}
	if (filter && filter->postprocessFunc) filter->postprocessFunc();

	// [RH] Set up hash table
	InitHashChains ();
	return true;
}

//==========================================================================
//
// AddFromBuffer
//
// Adds an in-memory resource to the virtual directory
//
//==========================================================================

int FileSystem::AddFromBuffer(const char* name, char* data, int size, int id, int flags)
{
	FileReader fr;
	FileData blob(data, size);
	fr.OpenMemoryArray(blob);

	// wrap this into a single lump resource file (should be done a little better later.)
	auto rf = new FResourceFile(name, fr, stringpool);
	auto Entries = rf->AllocateEntries(1);
	Entries[0].FileName = rf->NormalizeFileName(ExtractBaseName(name, true).c_str());
	Entries[0].ResourceID = -1;
	Entries[0].Length = size;

	Files.push_back(rf);
	FileInfo.resize(FileInfo.size() + 1);
	FileSystem::LumpRecord* lump_p = &FileInfo.back();
	lump_p->SetFromLump(rf, 0, (int)Files.size() - 1, stringpool);
	return (int)FileInfo.size() - 1;
}

//==========================================================================
//
// AddFile
//
// Files with a .wad extension are wadlink files with multiple lumps,
// other files are single lumps with the base filename for the lump name.
//
// [RH] Removed reload hack
//==========================================================================

void FileSystem::AddFile (const char *filename, FileReader *filer, LumpFilterInfo* filter, FileSystemMessageFunc Printf)
{
	int startlump;
	bool isdir = false;
	FileReader filereader;

	if (filer == nullptr)
	{
		// Does this exist? If so, is it a directory?
		if (!FS_DirEntryExists(filename, &isdir))
		{
			if (Printf)
			{
				Printf(FSMessageLevel::Error, "%s: File or Directory not found\n", filename);
				PrintLastError(Printf);
			}
			return;
		}

		if (!isdir)
		{
			if (!filereader.OpenFile(filename))
			{ // Didn't find file
				if (Printf)
				{
					Printf(FSMessageLevel::Error, "%s: File not found\n", filename);
					PrintLastError(Printf);
				}
				return;
			}
		}
	}
	else filereader = std::move(*filer);

	startlump = NumEntries;

	FResourceFile *resfile;


	if (!isdir)
		resfile = FResourceFile::OpenResourceFile(filename, filereader, false, filter, Printf, stringpool);
	else
		resfile = FResourceFile::OpenDirectory(filename, filter, Printf, stringpool);

	if (resfile != NULL)
	{
		if (Printf) 
			Printf(FSMessageLevel::Message, "adding %s, %d lumps\n", filename, resfile->EntryCount());

		uint32_t lumpstart = (uint32_t)FileInfo.size();

		resfile->SetFirstLump(lumpstart);
		Files.push_back(resfile);
		for (int i = 0; i < resfile->EntryCount(); i++)
		{
			FileInfo.resize(FileInfo.size() + 1);
			FileSystem::LumpRecord* lump_p = &FileInfo.back();
			lump_p->SetFromLump(resfile, i, (int)Files.size() - 1, stringpool);
		}

		for (int i = 0; i < resfile->EntryCount(); i++)
		{
			int flags = resfile->GetEntryFlags(i);
			if (flags & RESFF_EMBEDDED)
			{
				std::string path = filename;
				path += ':';
				path += resfile->getName(i);
				auto embedded = resfile->GetEntryReader(i, READER_CACHED);
				AddFile(path.c_str(), &embedded, filter, Printf);
			}
		}

		return;
	}
}

//==========================================================================
//
// CheckIfResourceFileLoaded
//
// Returns true if the specified file is loaded, false otherwise.
// If a fully-qualified path is specified, then the file must match exactly.
// Otherwise, any file with that name will work, whatever its path.
// Returns the file's index if found, or -1 if not.
//
//==========================================================================

int FileSystem::CheckIfResourceFileLoaded (const char *name) noexcept
{
	unsigned int i;

	if (strrchr (name, '/') != NULL)
	{
		for (i = 0; i < (unsigned)Files.size(); ++i)
		{
			if (stricmp (GetResourceFileFullName (i), name) == 0)
			{
				return i;
			}
		}
	}
	else
	{
		for (i = 0; i < (unsigned)Files.size(); ++i)
		{
			auto pth = ExtractBaseName(GetResourceFileName(i), true);
			if (stricmp (pth.c_str(), name) == 0)
			{
				return i;
			}
		}
	}
	return -1;
}

//==========================================================================
//
// CheckNumForName
//
// Returns -1 if name not found. The version with a third parameter will
// look exclusively in the specified wad for the lump.
//
// [RH] Changed to use hash lookup ala BOOM instead of a linear search
// and namespace parameter
//==========================================================================

int FileSystem::CheckNumForName (const char *name, int space) const
{
	union
	{
		char uname[8];
		uint64_t qname;
	};
	uint32_t i;

	if (name == NULL)
	{
		return -1;
	}

	// Let's not search for names that are longer than 8 characters and contain path separators
	// They are almost certainly full path names passed to this function.
	if (strlen(name) > 8 && strpbrk(name, "/."))
	{
		return -1;
	}

	UpperCopy (uname, name);
	i = FirstLumpIndex[MakeHash(uname, 8) % NumEntries];

	while (i != NULL_INDEX)
	{

		if (FileInfo[i].shortName.qword == qname)
		{
			auto &lump = FileInfo[i];
			if (lump.Namespace == space) break;
			// If the lump is from one of the special namespaces exclusive to Zips
			// the check has to be done differently:
			// If we find a lump with this name in the global namespace that does not come
			// from a Zip return that. WADs don't know these namespaces and single lumps must
			// work as well.
			auto lflags = lump.resfile->GetEntryFlags(lump.resindex);
			if (space > ns_specialzipdirectory && lump.Namespace == ns_global && 
				!((lflags ^lump.flags) & RESFF_FULLPATH)) break;
		}
		i = NextLumpIndex[i];
	}

	return i != NULL_INDEX ? i : -1;
}

int FileSystem::CheckNumForName (const char *name, int space, int rfnum, bool exact) const
{
	union
	{
		char uname[8];
		uint64_t qname;
	};
	uint32_t i;

	if (rfnum < 0)
	{
		return CheckNumForName (name, space);
	}

	UpperCopy (uname, name);
	i = FirstLumpIndex[MakeHash (uname, 8) % NumEntries];

	// If exact is true if will only find lumps in the same WAD, otherwise
	// also those in earlier WADs.

	while (i != NULL_INDEX &&
		(FileInfo[i].shortName.qword != qname || FileInfo[i].Namespace != space ||
		 (exact? (FileInfo[i].rfnum != rfnum) : (FileInfo[i].rfnum > rfnum)) ))
	{
		i = NextLumpIndex[i];
	}

	return i != NULL_INDEX ? i : -1;
}

//==========================================================================
//
// GetNumForName
//
// Calls CheckNumForName, but bombs out if not found.
//
//==========================================================================

int FileSystem::GetNumForName (const char *name, int space) const
{
	int	i;

	i = CheckNumForName (name, space);

	if (i == -1)
		throw FileSystemException("GetNumForName: %s not found!", name);

	return i;
}


//==========================================================================
//
// CheckNumForFullName
//
// Same as above but looks for a fully qualified name from a .zip
// These don't care about namespaces though because those are part
// of the path.
//
//==========================================================================

int FileSystem::CheckNumForFullName (const char *name, bool trynormal, int namespc, bool ignoreext) const
{
	uint32_t i;

	if (name == NULL)
	{
		return -1;
	}
	if (*name == '/') name++;	// ignore leading slashes in file names.
	uint32_t *fli = ignoreext ? FirstLumpIndex_NoExt : FirstLumpIndex_FullName;
	uint32_t *nli = ignoreext ? NextLumpIndex_NoExt : NextLumpIndex_FullName;
	auto len = strlen(name);

	for (i = fli[MakeHash(name) % NumEntries]; i != NULL_INDEX; i = nli[i])
	{
		if (strnicmp(name, FileInfo[i].LongName, len)) continue;
		if (FileInfo[i].LongName[len] == 0) break;	// this is a full match
		if (ignoreext && FileInfo[i].LongName[len] == '.') 
		{
			// is this the last '.' in the last path element, indicating that the remaining part of the name is only an extension?
			if (strpbrk(FileInfo[i].LongName + len + 1, "./") == nullptr) break;	
		}
	}

	if (i != NULL_INDEX) return i;

	if (trynormal && strlen(name) <= 8 && !strpbrk(name, "./"))
	{
		return CheckNumForName(name, namespc);
	}
	return -1;
}

int FileSystem::CheckNumForFullName (const char *name, int rfnum) const
{
	uint32_t i;

	if (rfnum < 0)
	{
		return CheckNumForFullName (name);
	}

	i = FirstLumpIndex_FullName[MakeHash (name) % NumEntries];

	while (i != NULL_INDEX && 
		(stricmp(name, FileInfo[i].LongName) || FileInfo[i].rfnum != rfnum))
	{
		i = NextLumpIndex_FullName[i];
	}

	return i != NULL_INDEX ? i : -1;
}

//==========================================================================
//
// GetNumForFullName
//
// Calls CheckNumForFullName, but bombs out if not found.
//
//==========================================================================

int FileSystem::GetNumForFullName (const char *name) const
{
	int	i;

	i = CheckNumForFullName (name);

	if (i == -1)
		throw FileSystemException("GetNumForFullName: %s not found!", name);

	return i;
}

//==========================================================================
//
// FindFile
//
// Looks up a file by name, either with or without path and extension
//
//==========================================================================

int FileSystem::FindFileWithExtensions(const char* name, const char *const *exts, int count) const
{
	uint32_t i;

	if (name == NULL)
	{
		return -1;
	}
	if (*name == '/') name++;	// ignore leading slashes in file names.
	uint32_t* fli = FirstLumpIndex_NoExt;
	uint32_t* nli = NextLumpIndex_NoExt;
	auto len = strlen(name);

	for (i = fli[MakeHash(name) % NumEntries]; i != NULL_INDEX; i = nli[i])
	{
		if (strnicmp(name, FileInfo[i].LongName, len)) continue;
		if (FileInfo[i].LongName[len] != '.') continue;	// we are looking for extensions but this file doesn't have one.

		auto cp = FileInfo[i].LongName + len + 1;
		// is this the last '.' in the last path element, indicating that the remaining part of the name is only an extension?
		if (strpbrk(cp, "./") != nullptr) continue;	// No, so it cannot be a valid entry.

		for (int j = 0; j < count; j++)
		{
			if (!stricmp(cp, exts[j])) return i;	// found a match
		}
	}
	return -1;
}

//==========================================================================
//
// FindResource
//
// Looks for content based on Blood resource IDs.
//
//==========================================================================

int FileSystem::FindResource (int resid, const char *type, int filenum) const noexcept
{
	uint32_t i;

	if (type == NULL || resid < 0)
	{
		return -1;
	}

	uint32_t* fli = FirstLumpIndex_ResId;
	uint32_t* nli = NextLumpIndex_ResId;

	for (i = fli[resid % NumEntries]; i != NULL_INDEX; i = nli[i])
	{
		if (filenum > 0 && FileInfo[i].rfnum != filenum) continue;
		if (FileInfo[i].resourceId != resid) continue;
		auto extp = strrchr(FileInfo[i].LongName, '.');
		if (!extp) continue;
		if (!stricmp(extp + 1, type)) return i;
	}
	return -1;
}

//==========================================================================
//
// GetResource
//
// Calls GetResource, but bombs out if not found.
//
//==========================================================================

int FileSystem::GetResource (int resid, const char *type, int filenum) const
{
	int	i;

	i = FindResource (resid, type, filenum);

	if (i == -1)
	{
		throw FileSystemException("GetResource: %d of type %s not found!", resid, type);
	}
	return i;
}

//==========================================================================
//
// FileLength
//
// Returns the buffer size needed to load the given lump.
//
//==========================================================================

ptrdiff_t FileSystem::FileLength (int lump) const
{
	if ((size_t)lump >= NumEntries)
	{
		return -1;
	}
	const auto &lump_p = FileInfo[lump];
	return (int)lump_p.resfile->Length(lump_p.resindex);
}

//==========================================================================
//
// 
//
//==========================================================================

int FileSystem::GetFileFlags (int lump)
{
	if ((size_t)lump >= NumEntries)
	{
		return 0;
	}

	const auto& lump_p = FileInfo[lump];
	return lump_p.resfile->GetEntryFlags(lump_p.resindex) ^ lump_p.flags;
}

//==========================================================================
//
// InitHashChains
//
// Prepares the lumpinfos for hashing.
// (Hey! This looks suspiciously like something from Boom! :-)
//
//==========================================================================

void FileSystem::InitHashChains (void)
{
	unsigned int i, j;

	NumEntries = (uint32_t)FileInfo.size();
	Hashes.resize(8 * NumEntries);
	// Mark all buckets as empty
	memset(Hashes.data(), -1, Hashes.size() * sizeof(Hashes[0]));
	FirstLumpIndex = &Hashes[0];
	NextLumpIndex = &Hashes[NumEntries];
	FirstLumpIndex_FullName = &Hashes[NumEntries * 2];
	NextLumpIndex_FullName = &Hashes[NumEntries * 3];
	FirstLumpIndex_NoExt = &Hashes[NumEntries * 4];
	NextLumpIndex_NoExt = &Hashes[NumEntries * 5];
	FirstLumpIndex_ResId = &Hashes[NumEntries * 6];
	NextLumpIndex_ResId = &Hashes[NumEntries * 7];


	// Now set up the chains
	for (i = 0; i < (unsigned)NumEntries; i++)
	{
		j = MakeHash (FileInfo[i].shortName.String, 8) % NumEntries;
		NextLumpIndex[i] = FirstLumpIndex[j];
		FirstLumpIndex[j] = i;

		// Do the same for the full paths
		if (FileInfo[i].LongName[0] != 0)
		{
			j = MakeHash(FileInfo[i].LongName) % NumEntries;
			NextLumpIndex_FullName[i] = FirstLumpIndex_FullName[j];
			FirstLumpIndex_FullName[j] = i;

			std::string nameNoExt = FileInfo[i].LongName;
			auto dot = nameNoExt.find_last_of('.');
			auto slash = nameNoExt.find_last_of('/');
			if ((dot > slash || slash == std::string::npos) && dot != std::string::npos) nameNoExt.resize(dot);

			j = MakeHash(nameNoExt.c_str()) % NumEntries;
			NextLumpIndex_NoExt[i] = FirstLumpIndex_NoExt[j];
			FirstLumpIndex_NoExt[j] = i;

			j = FileInfo[i].resourceId % NumEntries;
			NextLumpIndex_ResId[i] = FirstLumpIndex_ResId[j];
			FirstLumpIndex_ResId[j] = i;

		}
	}
	FileInfo.shrink_to_fit();
	Files.shrink_to_fit();
}

//==========================================================================
//
// should only be called before the hash chains are set up.
// If done later this needs rehashing.
//
//==========================================================================

LumpShortName& FileSystem::GetShortName(int i)
{
	if ((unsigned)i >= NumEntries) throw FileSystemException("GetShortName: Invalid index");
	return FileInfo[i].shortName;
}

void FileSystem::RenameFile(int num, const char* newfn)
{
	if ((unsigned)num >= NumEntries) throw FileSystemException("RenameFile: Invalid index");
	FileInfo[num].LongName = stringpool->Strdup(newfn);
	// This does not alter the short name - call GetShortname to do that!
}

//==========================================================================
//
// MoveLumpsInFolder
//
// Moves all content from the given subfolder of the internal
// resources to the current end of the directory.
// Used to allow modifying content in the base files, this is needed
// so that Hacx and Harmony can override some content that clashes
// with localization, and to inject modifying data into mods, in case
// this is needed for some compatibility requirement.
//
//==========================================================================

void FileSystem::MoveLumpsInFolder(const char *path)
{
	if (FileInfo.size() == 0)
	{
		return;
	}

	auto len = strlen(path);
	auto rfnum = FileInfo.back().rfnum;

	size_t i;
	for (i = 0; i < FileInfo.size(); i++)
	{
		auto& li = FileInfo[i];
		if (li.rfnum >= GetIwadNum()) break;
		if (strnicmp(li.LongName, path, len) == 0)
		{
			auto lic = li;	// make a copy before pushing.
			FileInfo.push_back(lic);
			li.LongName = "";	//nuke the name of the old record.
			li.shortName.qword = 0;
			auto &ln = FileInfo.back();
			ln.SetFromLump(li.resfile, li.resindex, rfnum, stringpool, ln.LongName + len);
		}
	}
}

//==========================================================================
//
// W_FindLump
//
// Find a named lump. Specifically allows duplicates for merging of e.g.
// SNDINFO lumps.
//
//==========================================================================

int FileSystem::FindLump (const char *name, int *lastlump, bool anyns)
{
	if ((size_t)*lastlump >= FileInfo.size()) return -1;
	union
	{
		char name8[8];
		uint64_t qname;
	};

	UpperCopy (name8, name);

	assert(lastlump != NULL && *lastlump >= 0);

	const LumpRecord * last = FileInfo.data() + FileInfo.size();

	LumpRecord * lump_p = FileInfo.data() + *lastlump;

	while (lump_p < last)
	{
		if ((anyns || lump_p->Namespace == ns_global) && lump_p->shortName.qword == qname)
		{
			int lump = int(lump_p - FileInfo.data());
			*lastlump = lump + 1;
			return lump;
		}
		lump_p++;
	}

	*lastlump = NumEntries;
	return -1;
}

//==========================================================================
//
// W_FindLumpMulti
//
// Find a named lump. Specifically allows duplicates for merging of e.g.
// SNDINFO lumps. Returns everything having one of the passed names.
//
//==========================================================================

int FileSystem::FindLumpMulti (const char **names, int *lastlump, bool anyns, int *nameindex)
{
	assert(lastlump != NULL && *lastlump >= 0);

	const LumpRecord * last = FileInfo.data() + FileInfo.size();

	LumpRecord * lump_p = FileInfo.data() + *lastlump;

	while (lump_p < last)
	{
		if (anyns || lump_p->Namespace == ns_global)
		{

			for(const char **name = names; *name != NULL; name++)
			{
				if (!strnicmp(*name, lump_p->shortName.String, 8))
				{
					int lump = int(lump_p - FileInfo.data());
					*lastlump = lump + 1;
					if (nameindex != NULL) *nameindex = int(name - names);
					return lump;
				}
			}
		}
		lump_p++;
	}

	*lastlump = NumEntries;
	return -1;
}

//==========================================================================
//
// W_FindLump
//
// Find a named lump. Specifically allows duplicates for merging of e.g.
// SNDINFO lumps.
//
//==========================================================================

int FileSystem::FindLumpFullName(const char* name, int* lastlump, bool noext)
{
	assert(lastlump != NULL && *lastlump >= 0);

	const LumpRecord * last = FileInfo.data() + FileInfo.size();

	LumpRecord * lump_p = FileInfo.data() + *lastlump;

	if (!noext)
	{
		while (lump_p < last)
		{
			if (!stricmp(name, lump_p->LongName))
			{
				int lump = int(lump_p - FileInfo.data());
				*lastlump = lump + 1;
				return lump;
			}
			lump_p++;
		}
	}
	else
	{
		auto len = strlen(name);
		while (lump_p <= &FileInfo.back())
		{
			auto res = strnicmp(name, lump_p->LongName, len);
			if (res == 0)
			{
				auto p = lump_p->LongName + len;
				if (*p == 0 || (*p == '.' && strpbrk(p + 1, "./") == 0))
				{
					int lump = int(lump_p - FileInfo.data());
					*lastlump = lump + 1;
					return lump;
				}
			}
			lump_p++;
		}
	}


	*lastlump = NumEntries;
	return -1;
}

//==========================================================================
//
// W_CheckLumpName
//
//==========================================================================

bool FileSystem::CheckFileName (int lump, const char *name)
{
	if ((size_t)lump >= NumEntries)
		return false;

	return !strnicmp (FileInfo[lump].shortName.String, name, 8);
}

//==========================================================================
//
// GetLumpName
//
//==========================================================================

const char* FileSystem::GetFileShortName(int lump) const
{
	if ((size_t)lump >= NumEntries)
		return nullptr;
	else
		return FileInfo[lump].shortName.String;
}

//==========================================================================
//
// FileSystem :: GetFileFullName
//
// Returns the lump's full name if it has one or its short name if not.
//
//==========================================================================

const char *FileSystem::GetFileFullName (int lump, bool returnshort) const
{
	if ((size_t)lump >= NumEntries)
		return NULL;
	else if (FileInfo[lump].LongName[0] != 0)
		return FileInfo[lump].LongName;
	else if (returnshort)
		return FileInfo[lump].shortName.String;
	else return nullptr;
}

//==========================================================================
//
// FileSystem :: GetFileFullPath
//
// Returns the name of the lump's wad prefixed to the lump's full name.
//
//==========================================================================

std::string FileSystem::GetFileFullPath(int lump) const
{
	std::string foo;

	if ((size_t) lump <  NumEntries)
	{
		foo = GetResourceFileName(FileInfo[lump].rfnum);
		foo += ':';
		foo += +GetFileFullName(lump);
	}
	return foo;
}

//==========================================================================
//
// GetFileNamespace
//
//==========================================================================

int FileSystem::GetFileNamespace (int lump) const
{
	if ((size_t)lump >= NumEntries)
		return ns_global;
	else
		return FileInfo[lump].Namespace;
}

void FileSystem::SetFileNamespace(int lump, int ns)
{
	if ((size_t)lump < NumEntries) FileInfo[lump].Namespace = ns;
}

//==========================================================================
//
// FileSystem :: GetResourceId
//
// Returns the index number for this lump. This is *not* the lump's position
// in the lump directory, but rather a special value that RFF can associate
// with files. Other archive types will return 0, since they don't have it.
//
//==========================================================================

int FileSystem::GetResourceId(int lump) const
{
	if ((size_t)lump >= NumEntries)
		return -1;
	else
		return FileInfo[lump].resourceId;
}

//==========================================================================
//
// GetResourceType
//
// is equivalent with the extension
//
//==========================================================================

const char *FileSystem::GetResourceType(int lump) const
{
	if ((size_t)lump >= NumEntries)
		return nullptr;
	else
	{
		auto p = strrchr(FileInfo[lump].LongName, '.');
		if (!p) return "";	// has no extension
		if (strchr(p, '/')) return "";	// the '.' is part of a directory.
		return p + 1;
	}
}

//==========================================================================
//
// GetFileContainer
//
//==========================================================================

int FileSystem::GetFileContainer (int lump) const
{
	if ((size_t)lump >= FileInfo.size())
		return -1;
	return FileInfo[lump].rfnum;
}

//==========================================================================
//
// GetFilesInFolder
// 
// Gets all lumps within a single folder in the hierarchy.
// If 'atomic' is set, it treats folders as atomic, i.e. only the
// content of the last found resource file having the given folder name gets used.
//
//==========================================================================

static int folderentrycmp(const void *a, const void *b)
{
	auto A = (FolderEntry*)a;
	auto B = (FolderEntry*)b;
	return strcmp(A->name, B->name);
}

//==========================================================================
//
// 
//
//==========================================================================

unsigned FileSystem::GetFilesInFolder(const char *inpath, std::vector<FolderEntry> &result, bool atomic) const
{
	std::string path = inpath;
	FixPathSeparator(&path.front());
	for (auto& c : path) c = tolower(c);
	if (path.back() != '/') path += '/';
	result.clear();
	for (size_t i = 0; i < FileInfo.size(); i++)
	{
		if (strncmp(FileInfo[i].LongName, path.c_str(), path.length()) == 0)
		{
			// Only if it hasn't been replaced.
			if ((unsigned)CheckNumForFullName(FileInfo[i].LongName) == i)
			{
				FolderEntry fe{ FileInfo[i].LongName, (uint32_t)i };
				result.push_back(fe);
			}
		}
	}
	if (result.size())
	{
		int maxfile = -1;
		if (atomic)
		{
			// Find the highest resource file having content in the given folder.
			for (auto & entry : result)
			{
				int thisfile = GetFileContainer(entry.lumpnum);
				if (thisfile > maxfile) maxfile = thisfile;
			}
			// Delete everything from older files.
			for (int i = (int)result.size() - 1; i >= 0; i--)
			{
				if (GetFileContainer(result[i].lumpnum) != maxfile) result.erase(result.begin() + i);
			}
		}
		qsort(result.data(), result.size(), sizeof(FolderEntry), folderentrycmp);
	}
	return (unsigned)result.size();
}

//==========================================================================
//
// W_ReadFile
//
// Loads the lump into the given buffer, which must be >= W_LumpLength().
//
//==========================================================================

void FileSystem::ReadFile (int lump, void *dest)
{
	auto lumpr = OpenFileReader (lump);
	auto size = lumpr.GetLength ();
	auto numread = lumpr.Read (dest, size);

	if (numread != size)
	{
		throw FileSystemException("W_ReadFile: only read %td of %td on '%s'\n",
			numread, size, FileInfo[lump].LongName);
	}
}


//==========================================================================
//
// ReadFile - variant 2
//
// Loads the lump into a newly created buffer and returns it.
//
//==========================================================================

FileData FileSystem::ReadFile (int lump)
{
	if ((unsigned)lump >= (unsigned)FileInfo.size())
	{
		throw FileSystemException("ReadFile: %u >= NumEntries", lump);
	}
	return FileInfo[lump].resfile->Read(FileInfo[lump].resindex);
}

//==========================================================================
//
// OpenFileReader
//
// uses a more abstract interface to allow for easier low level optimization later
//
//==========================================================================


FileReader FileSystem::OpenFileReader(int lump, int readertype, int readerflags)
{
	if ((unsigned)lump >= (unsigned)FileInfo.size())
	{
		throw FileSystemException("OpenFileReader: %u >= NumEntries", lump);
	}

	auto file = FileInfo[lump].resfile;
	return file->GetEntryReader(FileInfo[lump].resindex, readertype, readerflags);
}

FileReader FileSystem::OpenFileReader(const char* name)
{
	FileReader fr;
	auto lump = CheckNumForFullName(name);
	if (lump >= 0) fr = OpenFileReader(lump);
	return fr;
}

FileReader FileSystem::ReopenFileReader(const char* name, bool alwayscache)
{
	FileReader fr;
	auto lump = CheckNumForFullName(name);
	if (lump >= 0) fr = ReopenFileReader(lump, alwayscache);
	return fr;
}

//==========================================================================
//
// GetFileReader
//
// Retrieves the File reader object to access the given WAD
// Careful: This is only useful for real WAD files!
//
//==========================================================================

FileReader *FileSystem::GetFileReader(int rfnum)
{
	if ((uint32_t)rfnum >= Files.size())
	{
		return NULL;
	}

	return Files[rfnum]->GetContainerReader();
}

//==========================================================================
//
// GetResourceFileName
//
// Returns the name of the given wad.
//
//==========================================================================

const char *FileSystem::GetResourceFileName (int rfnum) const noexcept
{
	const char *name, *slash;

	if ((uint32_t)rfnum >= Files.size())
	{
		return NULL;
	}

	name = Files[rfnum]->GetFileName();
	slash = strrchr (name, '/');
	return (slash != nullptr && slash[1] != 0) ? slash+1 : name;
}

//==========================================================================
//
//
//==========================================================================

int FileSystem::GetFirstEntry (int rfnum) const noexcept
{
	if ((uint32_t)rfnum >= Files.size())
	{
		return 0;
	}

	return Files[rfnum]->GetFirstEntry();
}

//==========================================================================
//
//
//==========================================================================

int FileSystem::GetLastEntry (int rfnum) const noexcept
{
	if ((uint32_t)rfnum >= Files.size())
	{
		return 0;
	}

	return Files[rfnum]->GetFirstEntry() + Files[rfnum]->EntryCount() - 1;
}

//==========================================================================
//
//
//==========================================================================

int FileSystem::GetEntryCount (int rfnum) const noexcept
{
	if ((uint32_t)rfnum >= Files.size())
	{
		return 0;
	}

	return Files[rfnum]->EntryCount();
}


//==========================================================================
//
// GetResourceFileFullName
//
// Returns the name of the given wad, including any path
//
//==========================================================================

const char *FileSystem::GetResourceFileFullName (int rfnum) const noexcept
{
	if ((unsigned int)rfnum >= Files.size())
	{
		return nullptr;
	}

	return Files[rfnum]->GetFileName();
}


//==========================================================================
//
// Clones an existing resource with different properties
//
//==========================================================================

bool FileSystem::CreatePathlessCopy(const char *name, int id, int /*flags*/)
{
	std::string name2 = name, type2, path;

	// The old code said 'filename' and ignored the path, this looked like a bug.
	FixPathSeparator(&name2.front());
	auto lump = FindFile(name2.c_str());
	if (lump < 0) return false;		// Does not exist.

	auto oldlump = FileInfo[lump];
	auto slash = strrchr(oldlump.LongName, '/');

	if (slash == nullptr)
	{
		FileInfo[lump].flags = RESFF_FULLPATH;
		return true;	// already is pathless.
	}


	// just create a new reference to the original data with a different name.
	oldlump.LongName = slash + 1;
	oldlump.resourceId = id;
	oldlump.flags = RESFF_FULLPATH;
	FileInfo.push_back(oldlump);
	return true;
}

//==========================================================================
//
// PrintLastError
//
//==========================================================================

#ifdef _WIN32
//#define WIN32_LEAN_AND_MEAN
//#include <windows.h>

extern "C" {
__declspec(dllimport) unsigned long __stdcall FormatMessageA(
    unsigned long dwFlags,
    const void *lpSource,
    unsigned long dwMessageId,
    unsigned long dwLanguageId,
    char **lpBuffer,
    unsigned long nSize,
    va_list *Arguments
    );
__declspec(dllimport) void * __stdcall LocalFree (void *);
__declspec(dllimport) unsigned long __stdcall GetLastError ();
}

static void PrintLastError (FileSystemMessageFunc Printf)
{
	char *lpMsgBuf;
	FormatMessageA(0x1300 /*FORMAT_MESSAGE_ALLOCATE_BUFFER | 
							FORMAT_MESSAGE_FROM_SYSTEM | 
							FORMAT_MESSAGE_IGNORE_INSERTS*/,
		NULL,
		GetLastError(),
		1 << 10 /*MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT)*/, // Default language
		&lpMsgBuf,
		0,
		NULL 
	);
	Printf (FSMessageLevel::Error, "  %s\n", lpMsgBuf);
	// Free the buffer.
	LocalFree( lpMsgBuf );
}
#else
static void PrintLastError (FileSystemMessageFunc Printf)
{
	Printf(FSMessageLevel::Error, "  %s\n", strerror(errno));
}
#endif


}
