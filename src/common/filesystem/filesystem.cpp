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

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "m_argv.h"
#include "cmdlib.h"
#include "filesystem.h"
#include "m_crc32.h"
#include "printf.h"
#include "md5.h"

extern	FILE* hashfile;

// MACROS ------------------------------------------------------------------

#define NULL_INDEX		(0xffffffff)


struct FileSystem::LumpRecord
{
	FResourceLump *lump;
	FGameTexture* linkedTexture;
	LumpShortName shortName;
	FString		longName;
	int			rfnum;
	int			Namespace;
	int			resourceId;
	int			flags;

	void SetFromLump(int filenum, FResourceLump* lmp)
	{
		lump = lmp;
		rfnum = filenum;
		linkedTexture = nullptr;
		flags = 0;

		if (lump->Flags & LUMPF_SHORTNAME)
		{
			uppercopy(shortName.String, lump->getName());
			shortName.String[8] = 0;
			longName = "";
			Namespace = lump->GetNamespace();
			resourceId = -1;
		}
		else if ((lump->Flags & LUMPF_EMBEDDED) || !lump->getName() || !*lump->getName())
		{
			shortName.qword = 0;
			longName = "";
			Namespace = ns_hidden;
			resourceId = -1;
		}
		else
		{
			longName = lump->getName();
			resourceId = lump->GetIndexNum();

			// Map some directories to WAD namespaces.
			// Note that some of these namespaces don't exist in WADS.
			// CheckNumForName will handle any request for these namespaces accordingly.
			Namespace = !strncmp(longName.GetChars(), "flats/", 6) ? ns_flats :
				!strncmp(longName.GetChars(), "textures/", 9) ? ns_newtextures :
				!strncmp(longName.GetChars(), "hires/", 6) ? ns_hires :
				!strncmp(longName.GetChars(), "sprites/", 8) ? ns_sprites :
				!strncmp(longName.GetChars(), "voxels/", 7) ? ns_voxels :
				!strncmp(longName.GetChars(), "colormaps/", 10) ? ns_colormaps :
				!strncmp(longName.GetChars(), "acs/", 4) ? ns_acslibrary :
				!strncmp(longName.GetChars(), "voices/", 7) ? ns_strifevoices :
				!strncmp(longName.GetChars(), "patches/", 8) ? ns_patches :
				!strncmp(longName.GetChars(), "graphics/", 9) ? ns_graphics :
				!strncmp(longName.GetChars(), "sounds/", 7) ? ns_sounds :
				!strncmp(longName.GetChars(), "music/", 6) ? ns_music :
				!strchr(longName.GetChars(), '/') ? ns_global :
				ns_hidden;

			if (Namespace == ns_hidden) shortName.qword = 0;
			else
			{
				long slash = longName.LastIndexOf('/');
				FString base = (slash >= 0) ? longName.Mid(slash + 1) : longName;
				auto dot = base.LastIndexOf('.');
				if (dot >= 0) base.Truncate(dot);
				uppercopy(shortName.String, base);
				shortName.String[8] = 0;

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
	}
};

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void PrintLastError ();

// PUBLIC DATA DEFINITIONS -------------------------------------------------

FileSystem fileSystem;

// CODE --------------------------------------------------------------------

FileSystem::FileSystem()
{
	// This is needed to initialize the LumpRecord array, which depends on data only available here.
}

FileSystem::~FileSystem ()
{
	DeleteAll();
}

void FileSystem::DeleteAll ()
{
	Hashes.Clear();
	NumEntries = 0;

	// explicitly delete all manually added lumps.
	for (auto &frec : FileInfo)
	{
		if (frec.rfnum == -1) delete frec.lump;
	}
	FileInfo.Clear();
	for (int i = Files.Size() - 1; i >= 0; --i)
	{
		delete Files[i];
	}
	Files.Clear();
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

void FileSystem::InitSingleFile(const char* filename, bool quiet)
{
	TArray<FString> filenames;
	filenames.Push(filename);
	InitMultipleFiles(filenames, true);
}

void FileSystem::InitMultipleFiles (TArray<FString> &filenames, bool quiet, LumpFilterInfo* filter)
{
	int numfiles;

	// open all the files, load headers, and count lumps
	DeleteAll();
	numfiles = 0;

	for(unsigned i=0;i<filenames.Size(); i++)
	{
		int baselump = NumEntries;
		AddFile (filenames[i], nullptr, quiet, filter);
		
		if (i == (unsigned)MaxIwadIndex) MoveLumpsInFolder("after_iwad/");
		FStringf path("filter/%s", Files.Last()->GetHash().GetChars());
		MoveLumpsInFolder(path);
	}
	
	NumEntries = FileInfo.Size();
	if (NumEntries == 0)
	{
		if (!quiet) I_FatalError("W_InitMultipleFiles: no files found");
		else return;
	}
	if (filter && filter->postprocessFunc) filter->postprocessFunc();

	// [RH] Set up hash table
	InitHashChains ();
}

//==========================================================================
//
// AddLump
//
// Adds a given lump to the directory. Does not perform rehashing
//
//==========================================================================

void FileSystem::AddLump(FResourceLump *lump)
{
	FileSystem::LumpRecord *lumprec = &FileInfo[FileInfo.Reserve(1)];
	lumprec->SetFromLump(-1, lump);
}

//-----------------------------------------------------------------------
//
// Adds an external file to the lump list but not to the hash chains
// It's just a simple means to assign a lump number to some file so that
// the texture manager can read from it.
//
//-----------------------------------------------------------------------

int FileSystem::AddExternalFile(const char *filename)
{
	FResourceLump *lump = new FExternalLump(filename);
	AddLump(lump);
	return FileInfo.Size() - 1;	// later
}

//==========================================================================
//
// AddFromBuffer
//
// Adds an in-memory resource to the virtual directory
//
//==========================================================================

int FileSystem::AddFromBuffer(const char* name, const char* type, char* data, int size, int id, int flags)
{
	FStringf fullname("%s.%s", name, type);
	auto newlump = new FMemoryLump(data, size);
	newlump->LumpNameSetup(fullname);
	AddLump(newlump);
	FileInfo.Last().resourceId = id;
	return FileInfo.Size()-1;
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

void FileSystem::AddFile (const char *filename, FileReader *filer, bool quiet, LumpFilterInfo* filter)
{
	int startlump;
	bool isdir = false;
	FileReader filereader;

	if (filer == nullptr)
	{
		// Does this exist? If so, is it a directory?
		if (!DirEntryExists(filename, &isdir))
		{
			if (!quiet)
			{
				Printf(TEXTCOLOR_RED "%s: File or Directory not found\n", filename);
				PrintLastError();
			}
			return;
		}

		if (!isdir)
		{
			if (!filereader.OpenFile(filename))
			{ // Didn't find file
				if (!quiet)
				{
					Printf(TEXTCOLOR_RED "%s: File not found\n", filename);
					PrintLastError();
				}
				return;
			}
		}
	}
	else filereader = std::move(*filer);

	if (!batchrun && !quiet) Printf (" adding %s", filename);
	startlump = NumEntries;

	FResourceFile *resfile;
	
	if (!isdir)
		resfile = FResourceFile::OpenResourceFile(filename, filereader, quiet, false, filter);
	else
		resfile = FResourceFile::OpenDirectory(filename, quiet, filter);

	if (resfile != NULL)
	{
		if (!quiet && !batchrun) Printf(", %d lumps\n", resfile->LumpCount());

		uint32_t lumpstart = FileInfo.Size();

		resfile->SetFirstLump(lumpstart);
		for (uint32_t i=0; i < resfile->LumpCount(); i++)
		{
			FResourceLump *lump = resfile->GetLump(i);
			FileSystem::LumpRecord *lump_p = &FileInfo[FileInfo.Reserve(1)];
			lump_p->SetFromLump(Files.Size(), lump);
		}

		Files.Push(resfile);

		for (uint32_t i=0; i < resfile->LumpCount(); i++)
		{
			FResourceLump *lump = resfile->GetLump(i);
			if (lump->Flags & LUMPF_EMBEDDED)
			{
				FString path;
				path.Format("%s:%s", filename, lump->getName());
				auto embedded = lump->NewReader();
				AddFile(path, &embedded, quiet, filter);
			}
		}

		if (hashfile && !quiet)
		{
			uint8_t cksum[16];
			char cksumout[33];
			memset(cksumout, 0, sizeof(cksumout));

			if (filereader.isOpen())
			{
				MD5Context md5;
				filereader.Seek(0, FileReader::SeekSet);
				md5Update(filereader, md5, (unsigned)filereader.GetLength());
				md5.Final(cksum);

				for (size_t j = 0; j < sizeof(cksum); ++j)
				{
					sprintf(cksumout + (j * 2), "%02X", cksum[j]);
				}

				fprintf(hashfile, "file: %s, hash: %s, size: %d\n", filename, cksumout, (int)filereader.GetLength());
			}

			else
				fprintf(hashfile, "file: %s, Directory structure\n", filename);

			for (uint32_t i = 0; i < resfile->LumpCount(); i++)
			{
				FResourceLump *lump = resfile->GetLump(i);

				if (!(lump->Flags & LUMPF_EMBEDDED))
				{
					MD5Context md5;
					auto reader = lump->NewReader();
					md5Update(reader, md5, lump->LumpSize);
					md5.Final(cksum);

					for (size_t j = 0; j < sizeof(cksum); ++j)
					{
						sprintf(cksumout + (j * 2), "%02X", cksum[j]);
					}

					fprintf(hashfile, "file: %s, lump: %s, hash: %s, size: %d\n", filename, lump->getName(), cksumout, lump->LumpSize);
				}
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
		for (i = 0; i < Files.Size(); ++i)
		{
			if (stricmp (GetResourceFileFullName (i), name) == 0)
			{
				return i;
			}
		}
	}
	else
	{
		for (i = 0; i < Files.Size(); ++i)
		{
			auto pth = ExtractFileBase(GetResourceFileName(i), true);
			if (stricmp (pth.GetChars(), name) == 0)
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

int FileSystem::CheckNumForName (const char *name, int space)
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

	uppercopy (uname, name);
	i = FirstLumpIndex[LumpNameHash (uname) % NumEntries];

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
			if (space > ns_specialzipdirectory && lump.Namespace == ns_global && 
				!((lump.lump->Flags ^lump.flags) & LUMPF_FULLPATH)) break;
		}
		i = NextLumpIndex[i];
	}

	return i != NULL_INDEX ? i : -1;
}

int FileSystem::CheckNumForName (const char *name, int space, int rfnum, bool exact)
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

	uppercopy (uname, name);
	i = FirstLumpIndex[LumpNameHash (uname) % NumEntries];

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

int FileSystem::GetNumForName (const char *name, int space)
{
	int	i;

	i = CheckNumForName (name, space);

	if (i == -1)
		I_Error ("GetNumForName: %s not found!", name);

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

int FileSystem::CheckNumForFullName (const char *name, bool trynormal, int namespc, bool ignoreext)
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

	for (i = fli[MakeKey(name) % NumEntries]; i != NULL_INDEX; i = nli[i])
	{
		if (strnicmp(name, FileInfo[i].longName, len)) continue;
		if (FileInfo[i].longName[len] == 0) break;	// this is a full match
		if (ignoreext && FileInfo[i].longName[len] == '.') 
		{
			// is this the last '.' in the last path element, indicating that the remaining part of the name is only an extension?
			if (strpbrk(FileInfo[i].longName.GetChars() + len + 1, "./") == nullptr) break;	
		}
	}

	if (i != NULL_INDEX) return i;

	if (trynormal && strlen(name) <= 8 && !strpbrk(name, "./"))
	{
		return CheckNumForName(name, namespc);
	}
	return -1;
}

int FileSystem::CheckNumForFullName (const char *name, int rfnum)
{
	uint32_t i;

	if (rfnum < 0)
	{
		return CheckNumForFullName (name);
	}

	i = FirstLumpIndex_FullName[MakeKey (name) % NumEntries];

	while (i != NULL_INDEX && 
		(stricmp(name, FileInfo[i].longName) || FileInfo[i].rfnum != rfnum))
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

int FileSystem::GetNumForFullName (const char *name)
{
	int	i;

	i = CheckNumForFullName (name);

	if (i == -1)
		I_Error ("GetNumForFullName: %s not found!", name);

	return i;
}

//==========================================================================
//
// FindFile
//
// Looks up a file by name, either with or without path and extension
//
//==========================================================================

int FileSystem::FindFileWithExtensions(const char* name, const char *const *exts, int count)
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

	for (i = fli[MakeKey(name) % NumEntries]; i != NULL_INDEX; i = nli[i])
	{
		if (strnicmp(name, FileInfo[i].longName, len)) continue;
		if (FileInfo[i].longName[len] != '.') continue;	// we are looking for extensions but this file doesn't have one.

		auto cp = FileInfo[i].longName.GetChars() + len + 1;
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
		auto extp = strrchr(FileInfo[i].longName, '.');
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
		I_Error("GetResource: %d of type %s not found!", resid, type);
	}
	return i;
}

//==========================================================================
//
// link a texture with a given lump
//
//==========================================================================

void FileSystem::SetLinkedTexture(int lump, FGameTexture *tex)
{
	if ((size_t)lump < NumEntries)
	{
		FileInfo[lump].linkedTexture = tex;
	}
}

//==========================================================================
//
// retrieve linked texture
//
//==========================================================================

FGameTexture *FileSystem::GetLinkedTexture(int lump)
{
	if ((size_t)lump < NumEntries)
	{
		return FileInfo[lump].linkedTexture;
	}
	return NULL;
}

//==========================================================================
//
// FileLength
//
// Returns the buffer size needed to load the given lump.
//
//==========================================================================

int FileSystem::FileLength (int lump) const
{
	if ((size_t)lump >= NumEntries)
	{
		return -1;
	}
	return FileInfo[lump].lump->LumpSize;
}

//==========================================================================
//
// GetFileOffset
//
// Returns the offset from the beginning of the file to the lump.
// Returns -1 if the lump is compressed or can't be read directly
//
//==========================================================================

int FileSystem::GetFileOffset (int lump)
{
	if ((size_t)lump >= NumEntries)
	{
		return -1;
	}
	return FileInfo[lump].lump->GetFileOffset();
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

	return FileInfo[lump].lump->Flags ^ FileInfo[lump].flags;
}

//==========================================================================
//
// LumpNameHash
//
// NOTE: s should already be uppercase, in contrast to the BOOM version.
//
// Hash function used for lump names.
// Must be mod'ed with table size.
// Can be used for any 8-character names.
//
//==========================================================================

uint32_t FileSystem::LumpNameHash (const char *s)
{
	const uint32_t *table = GetCRCTable ();;
	uint32_t hash = 0xffffffff;
	int i;

	for (i = 8; i > 0 && *s; --i, ++s)
	{
		hash = CRC1 (hash, *s, table);
	}
	return hash ^ 0xffffffff;
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

	NumEntries = FileInfo.Size();
	Hashes.Resize(8 * NumEntries);
	// Mark all buckets as empty
	memset(Hashes.Data(), -1, Hashes.Size() * sizeof(Hashes[0]));
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
		j = LumpNameHash (FileInfo[i].shortName.String) % NumEntries;
		NextLumpIndex[i] = FirstLumpIndex[j];
		FirstLumpIndex[j] = i;

		// Do the same for the full paths
		if (FileInfo[i].longName.IsNotEmpty())
		{
			j = MakeKey(FileInfo[i].longName) % NumEntries;
			NextLumpIndex_FullName[i] = FirstLumpIndex_FullName[j];
			FirstLumpIndex_FullName[j] = i;

			FString nameNoExt = FileInfo[i].longName;
			auto dot = nameNoExt.LastIndexOf('.');
			auto slash = nameNoExt.LastIndexOf('/');
			if (dot > slash) nameNoExt.Truncate(dot);

			j = MakeKey(nameNoExt) % NumEntries;
			NextLumpIndex_NoExt[i] = FirstLumpIndex_NoExt[j];
			FirstLumpIndex_NoExt[j] = i;

			j = FileInfo[i].resourceId % NumEntries;
			NextLumpIndex_ResId[i] = FirstLumpIndex_ResId[j];
			FirstLumpIndex_ResId[j] = i;

		}
	}
	FileInfo.ShrinkToFit();
	Files.ShrinkToFit();
}

//==========================================================================
//
// should only be called before the hash chains are set up.
// If done later this needs rehashing.
//
//==========================================================================

LumpShortName& FileSystem::GetShortName(int i)
{
	if ((unsigned)i >= NumEntries) I_Error("GetShortName: Invalid index");
	return FileInfo[i].shortName;
}

void FileSystem::RenameFile(int num, const char* newfn)
{
	if ((unsigned)num >= NumEntries) I_Error("RenameFile: Invalid index");
	FileInfo[num].longName = newfn;
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

static FResourceLump placeholderLump;

void FileSystem::MoveLumpsInFolder(const char *path)
{
	if (FileInfo.Size() == 0)
	{
		return;
	}
	
	auto len = strlen(path);
	auto rfnum = FileInfo.Last().rfnum;
	
	unsigned i;
	for (i = 0; i < FileInfo.Size(); i++)
	{
		auto& li = FileInfo[i];
		if (li.rfnum >= GetIwadNum()) break;
		if (li.longName.Left(len).CompareNoCase(path) == 0)
		{
			FileInfo.Push(li);
			li.lump = &placeholderLump;			// Make the old entry point to something empty. We cannot delete the lump record here because it'd require adjustment of all indices in the list.
			auto &ln = FileInfo.Last();
			ln.lump->LumpNameSetup(ln.longName.Mid(len));
			ln.SetFromLump(rfnum, ln.lump);
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
	union
	{
		char name8[8];
		uint64_t qname;
	};
	LumpRecord *lump_p;

	uppercopy (name8, name);

	assert(lastlump != NULL && *lastlump >= 0);
	lump_p = &FileInfo[*lastlump];
	while (lump_p < &FileInfo[NumEntries])
	{
		if ((anyns || lump_p->Namespace == ns_global) && lump_p->shortName.qword == qname)
		{
			int lump = int(lump_p - &FileInfo[0]);
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
	LumpRecord *lump_p;

	assert(lastlump != NULL && *lastlump >= 0);
	lump_p = &FileInfo[*lastlump];
	while (lump_p < &FileInfo[NumEntries])
	{
		if (anyns || lump_p->Namespace == ns_global)
		{
			
			for(const char **name = names; *name != NULL; name++)
			{
				if (!strnicmp(*name, lump_p->shortName.String, 8))
				{
					int lump = int(lump_p - &FileInfo[0]);
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
	auto lump_p = &FileInfo[*lastlump];

	if (!noext)
	{
		while (lump_p < &FileInfo[NumEntries])
		{
			if (!stricmp(name, lump_p->longName))
			{
				int lump = int(lump_p - &FileInfo[0]);
				*lastlump = lump + 1;
				return lump;
			}
			lump_p++;
		}
	}
	else
	{
		auto len = strlen(name);
		while (lump_p < &FileInfo[NumEntries])
		{
			auto res = strnicmp(name, lump_p->longName, len);
			if (res == 0)
			{
				auto p = lump_p->longName.GetChars() + len;
				if (*p == 0 || (*p == '.' && strpbrk(p + 1, "./") == 0))
				{
					int lump = int(lump_p - &FileInfo[0]);
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

void FileSystem::GetFileShortName (char *to, int lump) const
{
	if ((size_t)lump >= NumEntries)
		*to = 0;
	else
		uppercopy (to, FileInfo[lump].shortName.String);
}

const char* FileSystem::GetFileShortName(int lump) const
{
	if ((size_t)lump >= NumEntries)
		return nullptr;
	else
		return FileInfo[lump].shortName.String;
}

void FileSystem::GetFileShortName(FString &to, int lump) const
{
	if ((size_t)lump >= NumEntries)
		to = FString();
	else {
		to = FileInfo[lump].shortName.String;
		to.ToUpper();
	}
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
	else if (FileInfo[lump].longName.IsNotEmpty())
		return FileInfo[lump].longName;
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

FString FileSystem::GetFileFullPath(int lump) const
{
	FString foo;

	if ((size_t) lump <  NumEntries)
	{
		foo << GetResourceFileName(FileInfo[lump].rfnum) << ':' << GetFileFullName(lump);
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
		auto p = strrchr(FileInfo[lump].longName.GetChars(), '.');
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
	if ((size_t)lump >= FileInfo.Size())
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

unsigned FileSystem::GetFilesInFolder(const char *inpath, TArray<FolderEntry> &result, bool atomic) const
{
	FString path = inpath;
	FixPathSeperator(path);
	path.ToLower();
	if (path[path.Len() - 1] != '/') path += '/';
	result.Clear();
	for (unsigned i = 0; i < FileInfo.Size(); i++)
	{
		if (FileInfo[i].longName.IndexOf(path) == 0)
		{
			// Only if it hasn't been replaced.
			if ((unsigned)fileSystem.CheckNumForFullName(FileInfo[i].longName) == i)
			{
				result.Push({ FileInfo[i].longName.GetChars(), i });
			}
		}
	}
	if (result.Size())
	{
		int maxfile = -1;
		if (atomic)
		{
			// Find the highest resource file having content in the given folder.
			for (auto & entry : result)
			{
				int thisfile = fileSystem.GetFileContainer(entry.lumpnum);
				if (thisfile > maxfile) maxfile = thisfile;
			}
			// Delete everything from older files.
			for (int i = result.Size() - 1; i >= 0; i--)
			{
				if (fileSystem.GetFileContainer(result[i].lumpnum) != maxfile) result.Delete(i);
			}
		}
		qsort(result.Data(), result.Size(), sizeof(FolderEntry), folderentrycmp);
	}
	return result.Size();
}

//==========================================================================
//
// GetFileData
//
// Loads the lump into a TArray and returns it.
//
//==========================================================================

TArray<uint8_t> FileSystem::GetFileData(int lump, int pad)
{
	if ((size_t)lump >= FileInfo.Size())
		return TArray<uint8_t>();

	auto lumpr = OpenFileReader(lump);
	auto size = lumpr.GetLength();
	TArray<uint8_t> data(size + pad, true);
	auto numread = lumpr.Read(data.Data(), size);

	if (numread != size)
	{
		I_Error("GetFileData: only read %ld of %ld on lump %i\n",
			numread, size, lump);
	}
	if (pad > 0) memset(&data[size], 0, pad);
	return data;
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
		I_Error ("W_ReadFile: only read %ld of %ld on lump %i\n",
			numread, size, lump);	
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
	return FileData(FString(ELumpNum(lump)));
}

//==========================================================================
//
// OpenFileReader
//
// uses a more abstract interface to allow for easier low level optimization later
//
//==========================================================================


FileReader FileSystem::OpenFileReader(int lump)
{
	if ((unsigned)lump >= (unsigned)FileInfo.Size())
	{
		I_Error("OpenFileReader: %u >= NumEntries", lump);
	}

	auto rl = FileInfo[lump].lump;
	auto rd = rl->GetReader();

	if (rl->RefCount == 0 && rd != nullptr && !rd->GetBuffer() && !(rl->Flags & LUMPF_COMPRESSED))
	{
		FileReader rdr;
		rdr.OpenFilePart(*rd, rl->GetFileOffset(), rl->LumpSize);
		return rdr;
	}
	return rl->NewReader();	// This always gets a reader to the cache
}

FileReader FileSystem::ReopenFileReader(int lump, bool alwayscache)
{
	if ((unsigned)lump >= (unsigned)FileInfo.Size())
	{
		I_Error("ReopenFileReader: %u >= NumEntries", lump);
	}

	auto rl = FileInfo[lump].lump;
	auto rd = rl->GetReader();

	if (rl->RefCount == 0 && rd != nullptr && !rd->GetBuffer() && !alwayscache && !(rl->Flags & LUMPF_COMPRESSED))
	{
		int fileno = fileSystem.GetFileContainer(lump);
		const char *filename = fileSystem.GetResourceFileFullName(fileno);
		FileReader fr;
		if (fr.OpenFile(filename, rl->GetFileOffset(), rl->LumpSize))
		{
			return fr;
		}
	}
	return rl->NewReader();	// This always gets a reader to the cache
}

FileReader FileSystem::OpenFileReader(const char* name)
{
	auto lump = CheckNumForFullName(name);
	if (lump < 0) return FileReader();
	else return OpenFileReader(lump);
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
	if ((uint32_t)rfnum >= Files.Size())
	{
		return NULL;
	}

	return Files[rfnum]->GetReader();
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

	if ((uint32_t)rfnum >= Files.Size())
	{
		return NULL;
	}

	name = Files[rfnum]->FileName;
	slash = strrchr (name, '/');
	return (slash != NULL && slash[1] != 0) ? slash+1 : name;
}

//==========================================================================
//
//
//==========================================================================

int FileSystem::GetFirstEntry (int rfnum) const noexcept
{
	if ((uint32_t)rfnum >= Files.Size())
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
	if ((uint32_t)rfnum >= Files.Size())
	{
		return 0;
	}

	return Files[rfnum]->GetFirstEntry() + Files[rfnum]->LumpCount() - 1;
}

//==========================================================================
//
//
//==========================================================================

int FileSystem::GetEntryCount (int rfnum) const noexcept
{
	if ((uint32_t)rfnum >= Files.Size())
	{
		return 0;
	}
	
	return Files[rfnum]->LumpCount();
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
	if ((unsigned int)rfnum >= Files.Size())
	{
		return nullptr;
	}

	return Files[rfnum]->FileName;
}


//==========================================================================
//
// Clones an existing resource with different properties
//
//==========================================================================

bool FileSystem::CreatePathlessCopy(const char *name, int id, int /*flags*/)
{
	FString name2=name, type2, path;

	// The old code said 'filename' and ignored the path, this looked like a bug.
	FixPathSeperator(name2);
	auto lump = FindFile(name2);
	if (lump < 0) return false;		// Does not exist.

	auto oldlump = FileInfo[lump];
	int slash = oldlump.longName.LastIndexOf('/');

	if (slash == -1)
	{
		FileInfo[lump].flags = LUMPF_FULLPATH;
		return true;	// already is pathless.
	}


	// just create a new reference to the original data with a different name.
	oldlump.longName = oldlump.longName.Mid(slash + 1);
	oldlump.resourceId = id;
	oldlump.flags = LUMPF_FULLPATH;
	FileInfo.Push(oldlump);
	return true;
}

// FileData -----------------------------------------------------------------

FileData::FileData ()
{
}

FileData::FileData (const FileData &copy)
{
	Block = copy.Block;
}

FileData &FileData::operator = (const FileData &copy)
{
	Block = copy.Block;
	return *this;
}

FileData::FileData (const FString &source)
: Block (source)
{
}

FileData::~FileData ()
{
}

FString::FString (ELumpNum lumpnum)
{
	auto lumpr = fileSystem.OpenFileReader ((int)lumpnum);
	auto size = lumpr.GetLength ();
	AllocBuffer (1 + size);
	auto numread = lumpr.Read (&Chars[0], size);
	Chars[size] = '\0';

	if (numread != size)
	{
		I_Error ("ConstructStringFromLump: Only read %ld of %ld bytes on lump %i (%s)\n",
			numread, size, lumpnum, fileSystem.GetFileFullName((int)lumpnum));
	}
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

static void PrintLastError ()
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
	Printf (TEXTCOLOR_RED "  %s\n", lpMsgBuf);
	// Free the buffer.
	LocalFree( lpMsgBuf );
}
#else
static void PrintLastError ()
{
	Printf (TEXTCOLOR_RED "  %s\n", strerror(errno));
}
#endif

//==========================================================================
//
// NBlood style lookup functions
//
//==========================================================================

FResourceLump* FileSystem::GetFileAt(int no)
{
	return FileInfo[no].lump;
}

