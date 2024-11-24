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
	const char*	Name;
	int			resindex;
	int16_t		rfnum;		// this is not necessarily the same as resfile's index!
	int			resourceId;
	int			flags;

	void SetFromFile(FResourceFile* file, int fileindex, int filenum, StringPool* sp, const char* name = nullptr)
	{
		resfile = file;
		resindex = fileindex;
		rfnum = filenum;
		flags = 0;

		auto lflags = file->GetEntryFlags(fileindex);
		if (!name) name = file->getName(fileindex);
		if ((lflags & RESFF_EMBEDDED) || !name || !*name)
		{
			Name = "";
			resourceId = -1;
		}
		else
		{
			Name = name;
			resourceId = file->GetEntryResourceID(fileindex);

			// allow embedding a resource ID in the name - we need to strip that out here.
			if (strstr(Name, ".{"))
			{
				std::string longName = Name;
				ptrdiff_t encodedResID = longName.find_last_of(".{");
				if (resourceId == -1 && (size_t)encodedResID != std::string::npos)
				{
					const char* p = Name + encodedResID;
					char* q;
					int id = (int)strtoull(p + 2, &q, 10);	// only decimal numbers allowed here.
					if (q[0] == '}' && (q[1] == '.' || q[1] == 0))
					{
						longName.erase(longName.begin() + encodedResID, longName.begin() + (q - p) + 1);
						resourceId = id;
					}
					Name = sp->Strdup(longName.c_str());
				}
			}
			auto slash = strrchr(Name, '/');
			std::string base = slash ? (slash + 1) : Name;
			auto dot = base.find_last_of('.');
			if (dot != std::string::npos) base.resize(dot);
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
// Initialize
//
// Pass a vector of files to use. All files are optional,
// but at least one file must be found. File names can appear multiple
// times. The name searcher looks backwards, so a later file can
// override an earlier one.
//
//==========================================================================

bool FileSystem::InitFiles(std::vector<std::string>& filenames, FileSystemFilterInfo* filter, FileSystemMessageFunc Printf, bool allowduplicates)
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
		for (size_t i = 0; i < filenames.size(); i++)
		{
			for (size_t j = i + 1; j < filenames.size(); j++)
			{
				if (filenames[i] == filenames[j])
				{
					filenames.erase(filenames.begin() + j);
					j--;
				}
			}
		}
	}

	for (size_t i = 0; i < filenames.size(); i++)
	{
		AddFile(filenames[i].c_str(), nullptr, filter, Printf);

		if (i == (unsigned)MaxBaseIndex) MoveFilesInFolder("after_iwad/");
		std::string path = "filter/%s";
		path += Files.back()->GetHash();
		MoveFilesInFolder(path.c_str());
	}

	NumEntries = (uint32_t)FileInfo.size();
	if (NumEntries == 0)
	{
		return false;
	}
	return true;
}

bool FileSystem::Initialize(std::vector<std::string>& filenames, FileSystemFilterInfo* filter, FileSystemMessageFunc Printf, bool allowduplicates)
{
	if (!InitFiles(filenames, filter, Printf, allowduplicates)) return false;
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

	// wrap this into a single filenum resource file (should be done a little better later.)
	auto rf = new FResourceFile(name, fr, stringpool, 0);
	auto Entries = rf->AllocateEntries(1);
	Entries[0].FileName = rf->NormalizeFileName(ExtractBaseName(name, true).c_str());
	Entries[0].ResourceID = -1;
	Entries[0].Length = size;

	Files.push_back(rf);
	FileInfo.resize(FileInfo.size() + 1);
	FileSystem::LumpRecord* file_p = &FileInfo.back();
	file_p->SetFromFile(rf, 0, (int)Files.size() - 1, stringpool);
	return (int)FileInfo.size() - 1;
}

//==========================================================================
//
// AddFile
//
// Files with a .wad extension are wadlink files with multiple lumps,
// other files are single lumps with the base filename for the filenum name.
//
// [RH] Removed reload hack
//==========================================================================

void FileSystem::AddFile (const char *filename, FileReader *filer, FileSystemFilterInfo* filter, FileSystemMessageFunc Printf)
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
			Printf(FSMessageLevel::Message, "adding %s, %d files\n", filename, resfile->EntryCount());

		uint32_t lumpstart = (uint32_t)FileInfo.size();

		resfile->SetFirstFile(lumpstart);
		Files.push_back(resfile);
		for (int i = 0; i < resfile->EntryCount(); i++)
		{
			FileInfo.resize(FileInfo.size() + 1);
			FileSystem::LumpRecord* file_p = &FileInfo.back();
			file_p->SetFromFile(resfile, i, (int)Files.size() - 1, stringpool);
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

int FileSystem::CheckIfContainerLoaded (const char *name) noexcept
{
	unsigned int i;

	if (strrchr (name, '/') != NULL)
	{
		for (i = 0; i < (unsigned)Files.size(); ++i)
		{
			if (stricmp (GetContainerFullName (i), name) == 0)
			{
				return i;
			}
		}
	}
	else
	{
		for (i = 0; i < (unsigned)Files.size(); ++i)
		{
			auto pth = ExtractBaseName(GetContainerName(i), true);
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
// CheckNumForFullName
//
// Same as above but looks for a fully qualified name from a .zip
// These don't care about namespaces though because those are part
// of the path.
//
//==========================================================================

int FileSystem::FindFile (const char *name, bool ignoreext) const
{
	uint32_t i;

	if (name == NULL)
	{
		return -1;
	}
	if (*name == '/') name++;	// ignore leading slashes in file names.
	uint32_t *fli = ignoreext ? FirstFileIndex_NoExt : FirstFileIndex_FullName;
	uint32_t *nli = ignoreext ? NextFileIndex_NoExt : NextFileIndex_FullName;
	auto len = strlen(name);

	for (i = fli[MakeHash(name) % NumEntries]; i != NULL_INDEX; i = nli[i])
	{
		if (strnicmp(name, FileInfo[i].Name, len)) continue;
		if (FileInfo[i].Name[len] == 0) break;	// this is a full match
		if (ignoreext && FileInfo[i].Name[len] == '.') 
		{
			// is this the last '.' in the last path element, indicating that the remaining part of the name is only an extension?
			if (strpbrk(FileInfo[i].Name + len + 1, "./") == nullptr) break;	
		}
	}

	if (i != NULL_INDEX) return i;
	return -1;
}

int FileSystem::GetFileInContainer (const char *name, int rfnum) const
{
	uint32_t i;

	if (rfnum < 0)
	{
		return FindFile (name);
	}

	i = FirstFileIndex_FullName[MakeHash (name) % NumEntries];

	while (i != NULL_INDEX && 
		(stricmp(name, FileInfo[i].Name) || FileInfo[i].rfnum != rfnum))
	{
		i = NextFileIndex_FullName[i];
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

int FileSystem::GetFile (const char *name) const
{
	int	i;

	i = FindFile (name);

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
	uint32_t* fli = FirstFileIndex_NoExt;
	uint32_t* nli = NextFileIndex_NoExt;
	auto len = strlen(name);

	for (i = fli[MakeHash(name) % NumEntries]; i != NULL_INDEX; i = nli[i])
	{
		if (strnicmp(name, FileInfo[i].Name, len)) continue;
		if (FileInfo[i].Name[len] != '.') continue;	// we are looking for extensions but this file doesn't have one.

		auto cp = FileInfo[i].Name + len + 1;
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

	uint32_t* fli = FirstFileIndex_ResId;
	uint32_t* nli = NextFileIndex_ResId;

	for (i = fli[resid % NumEntries]; i != NULL_INDEX; i = nli[i])
	{
		if (filenum > 0 && FileInfo[i].rfnum != filenum) continue;
		if (FileInfo[i].resourceId != resid) continue;
		auto extp = strrchr(FileInfo[i].Name, '.');
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
// Returns the buffer size needed to load the given filenum.
//
//==========================================================================

ptrdiff_t FileSystem::FileLength (int filenum) const
{
	if ((size_t)filenum >= NumEntries)
	{
		return -1;
	}
	const auto &file_p = FileInfo[filenum];
	return (int)file_p.resfile->Length(file_p.resindex);
}

//==========================================================================
//
// 
//
//==========================================================================

int FileSystem::GetFileFlags (int filenum)
{
	if ((size_t)filenum >= NumEntries)
	{
		return 0;
	}

	const auto& file_p = FileInfo[filenum];
	return file_p.resfile->GetEntryFlags(file_p.resindex) ^ file_p.flags;
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
	Hashes.resize(6 * NumEntries);
	// Mark all buckets as empty
	memset(Hashes.data(), -1, Hashes.size() * sizeof(Hashes[0]));
	FirstFileIndex_FullName = &Hashes[NumEntries * 0];
	NextFileIndex_FullName = &Hashes[NumEntries * 1];
	FirstFileIndex_NoExt = &Hashes[NumEntries * 2];
	NextFileIndex_NoExt = &Hashes[NumEntries * 3];
	FirstFileIndex_ResId = &Hashes[NumEntries * 4];
	NextFileIndex_ResId = &Hashes[NumEntries * 5];


	// Now set up the chains
	for (i = 0; i < (unsigned)NumEntries; i++)
	{
		// Do the same for the full paths
		if (FileInfo[i].Name[0] != 0)
		{
			j = MakeHash(FileInfo[i].Name) % NumEntries;
			NextFileIndex_FullName[i] = FirstFileIndex_FullName[j];
			FirstFileIndex_FullName[j] = i;

			std::string nameNoExt = FileInfo[i].Name;
			auto dot = nameNoExt.find_last_of('.');
			auto slash = nameNoExt.find_last_of('/');
			if ((dot > slash || slash == std::string::npos) && dot != std::string::npos) nameNoExt.resize(dot);

			j = MakeHash(nameNoExt.c_str()) % NumEntries;
			NextFileIndex_NoExt[i] = FirstFileIndex_NoExt[j];
			FirstFileIndex_NoExt[j] = i;

			j = FileInfo[i].resourceId % NumEntries;
			NextFileIndex_ResId[i] = FirstFileIndex_ResId[j];
			FirstFileIndex_ResId[j] = i;

		}
	}
	FileInfo.shrink_to_fit();
	Files.shrink_to_fit();
}

void FileSystem::RenameFile(int num, const char* newfn)
{
	if ((unsigned)num >= NumEntries) throw FileSystemException("RenameFile: Invalid index");
	FileInfo[num].Name = stringpool->Strdup(newfn);
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

void FileSystem::MoveFilesInFolder(const char *path)
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
		if (li.rfnum >= GetBaseNum()) break;
		if (strnicmp(li.Name, path, len) == 0)
		{
			auto lic = li;	// make a copy before pushing.
			FileInfo.push_back(lic);
			li.Name = "";	//nuke the name of the old record.
			auto &ln = FileInfo.back();
			ln.SetFromFile(li.resfile, li.resindex, rfnum, stringpool, ln.Name + len);
		}
	}
}

//==========================================================================
//
// W_FindLump
//
// Find a named filenum. Specifically allows duplicates for merging of e.g.
// SNDINFO lumps.
//
//==========================================================================

int FileSystem::FindLumpFullName(const char* name, int* lastlump, bool noext)
{
	assert(lastlump != NULL && *lastlump >= 0);

	const LumpRecord * last = FileInfo.data() + FileInfo.size();

	LumpRecord * file_p = FileInfo.data() + *lastlump;

	if (!noext)
	{
		while (file_p < last)
		{
			if (!stricmp(name, file_p->Name))
			{
				int filenum = int(file_p - FileInfo.data());
				*lastlump = filenum + 1;
				return filenum;
			}
			file_p++;
		}
	}
	else
	{
		auto len = strlen(name);
		while (file_p <= &FileInfo.back())
		{
			auto res = strnicmp(name, file_p->Name, len);
			if (res == 0)
			{
				auto p = file_p->Name + len;
				if (*p == 0 || (*p == '.' && strpbrk(p + 1, "./") == 0))
				{
					int filenum = int(file_p - FileInfo.data());
					*lastlump = filenum + 1;
					return filenum;
				}
			}
			file_p++;
		}
	}


	*lastlump = NumEntries;
	return -1;
}

//==========================================================================
//
// FileSystem :: GetFileName
//
// Returns the filenum's internal name
//
//==========================================================================

const char* FileSystem::GetFileName(int filenum) const
{
	if ((size_t)filenum >= NumEntries)
		return NULL;
	else return FileInfo[filenum].Name;
}

//==========================================================================
//
// FileSystem :: GetFileFullPath
//
// Returns the name of the filenum's wad prefixed to the filenum's full name.
//
//==========================================================================

std::string FileSystem::GetFileFullPath(int filenum) const
{
	std::string foo;

	if ((size_t) filenum <  NumEntries)
	{
		foo = GetContainerName(FileInfo[filenum].rfnum);
		foo += ':';
		foo += +GetFileName(filenum);
	}
	return foo;
}

//==========================================================================
//
// FileSystem :: GetResourceId
//
// Returns the index number for this filenum. This is *not* the filenum's position
// in the filenum directory, but rather a special value that RFF can associate
// with files. Other archive types will return 0, since they don't have it.
//
//==========================================================================

int FileSystem::GetResourceId(int filenum) const
{
	if ((size_t)filenum >= NumEntries)
		return -1;
	else
		return FileInfo[filenum].resourceId;
}

//==========================================================================
//
// GetResourceType
//
// is equivalent with the extension
//
//==========================================================================

const char *FileSystem::GetResourceType(int filenum) const
{
	if ((size_t)filenum >= NumEntries)
		return nullptr;
	else
	{
		auto p = strrchr(FileInfo[filenum].Name, '.');
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

int FileSystem::GetFileContainer (int filenum) const
{
	if ((size_t)filenum >= FileInfo.size())
		return -1;
	return FileInfo[filenum].rfnum;
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
		if (strncmp(FileInfo[i].Name, path.c_str(), path.length()) == 0)
		{
			// Only if it hasn't been replaced.
			if ((unsigned)FindFile(FileInfo[i].Name) == i)
			{
				FolderEntry fe{ FileInfo[i].Name, (uint32_t)i };
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
// Loads the filenum into the given buffer, which must be >= W_LumpLength().
//
//==========================================================================

void FileSystem::ReadFile (int filenum, void *dest)
{
	auto lumpr = OpenFileReader (filenum);
	auto size = lumpr.GetLength ();
	auto numread = lumpr.Read (dest, size);

	if (numread != size)
	{
		throw FileSystemException("W_ReadFile: only read %td of %td on '%s'\n",
			numread, size, FileInfo[filenum].Name);
	}
}


//==========================================================================
//
// ReadFile - variant 2
//
// Loads the filenum into a newly created buffer and returns it.
//
//==========================================================================

FileData FileSystem::ReadFile (int filenum)
{
	if ((unsigned)filenum >= (unsigned)FileInfo.size())
	{
		throw FileSystemException("ReadFile: %u >= NumEntries", filenum);
	}
	return FileInfo[filenum].resfile->Read(FileInfo[filenum].resindex);
}

//==========================================================================
//
// OpenFileReader
//
// uses a more abstract interface to allow for easier low level optimization later
//
//==========================================================================


FileReader FileSystem::OpenFileReader(int filenum, int readertype, int readerflags)
{
	if ((unsigned)filenum >= (unsigned)FileInfo.size())
	{
		throw FileSystemException("OpenFileReader: %u >= NumEntries", filenum);
	}

	auto file = FileInfo[filenum].resfile;
	return file->GetEntryReader(FileInfo[filenum].resindex, readertype, readerflags);
}

FileReader FileSystem::OpenFileReader(const char* name)
{
	FileReader fr;
	auto filenum = FindFile(name);
	if (filenum >= 0) fr = OpenFileReader(filenum);
	return fr;
}

FileReader FileSystem::ReopenFileReader(const char* name, bool alwayscache)
{
	FileReader fr;
	auto filenum = FindFile(name);
	if (filenum >= 0) fr = ReopenFileReader(filenum, alwayscache);
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
// GetContainerName
//
// Returns the name of the given wad.
//
//==========================================================================

const char *FileSystem::GetContainerName (int rfnum) const noexcept
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

int FileSystem::GetContainerFlags(int rfnum) const noexcept
{
	if ((uint32_t)rfnum >= Files.size())
	{
		return 0;
	}

	return Files[rfnum]->GetFlags();
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

const char *FileSystem::GetContainerFullName (int rfnum) const noexcept
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
	auto filenum = FindFile(name2.c_str());
	if (filenum < 0) return false;		// Does not exist.

	auto oldlump = FileInfo[filenum];
	auto slash = strrchr(oldlump.Name, '/');

	if (slash == nullptr)
	{
		FileInfo[filenum].flags = RESFF_FULLPATH;
		return true;	// already is pathless.
	}


	// just create a new reference to the original data with a different name.
	oldlump.Name = slash + 1;
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
