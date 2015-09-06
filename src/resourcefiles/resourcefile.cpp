/*
** resourcefile.cpp
**
** Base classes for resource file management
**
**---------------------------------------------------------------------------
** Copyright 2009 Christoph Oelckers
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

#include "resourcefile.h"
#include "cmdlib.h"
#include "w_wad.h"
#include "doomerrors.h"
#include "gi.h"
#include "doomstat.h"


//==========================================================================
//
// FileReader that reads from a lump's cache
//
//==========================================================================

class FLumpReader : public MemoryReader
{
	FResourceLump *source;

public:
	FLumpReader(FResourceLump *src)
		: MemoryReader(NULL, src->LumpSize), source(src)
	{
		src->CacheLump();
		bufptr = src->Cache;
	}

	~FLumpReader()
	{
		source->ReleaseCache();
	}
};


//==========================================================================
//
// Base class for resource lumps
//
//==========================================================================

FResourceLump::~FResourceLump()
{
	if (Cache != NULL && RefCount >= 0)
	{
		delete [] Cache;
		Cache = NULL;
	}
	Owner = NULL;
}


//==========================================================================
//
// Sets up the lump name information for anything not coming from a WAD file.
//
//==========================================================================

void FResourceLump::LumpNameSetup(FString iname)
{
	long slash = iname.LastIndexOf('/');
	FString base = (slash >= 0) ? iname.Mid(slash + 1) : iname;
	base.Truncate(base.LastIndexOf('.'));
	uppercopy(Name, base);
	Name[8] = 0;
	FullName = iname;

	// Map some directories to WAD namespaces.
	// Note that some of these namespaces don't exist in WADS.
	// CheckNumForName will handle any request for these namespaces accordingly.
	Namespace =	!strncmp(iname, "flats/", 6)		? ns_flats :
				!strncmp(iname, "textures/", 9)		? ns_newtextures :
				!strncmp(iname, "hires/", 6)		? ns_hires :
				!strncmp(iname, "sprites/", 8)		? ns_sprites :
				!strncmp(iname, "voxels/", 7)		? ns_voxels :
				!strncmp(iname, "colormaps/", 10)	? ns_colormaps :
				!strncmp(iname, "acs/", 4)			? ns_acslibrary :
				!strncmp(iname, "voices/", 7)		? ns_strifevoices :
				!strncmp(iname, "patches/", 8)		? ns_patches :
				!strncmp(iname, "graphics/", 9)		? ns_graphics :
				!strncmp(iname, "sounds/", 7)		? ns_sounds :
				!strncmp(iname, "music/", 6)		? ns_music : 
				!strchr(iname, '/')					? ns_global :
				ns_hidden;
	
	// Anything that is not in one of these subdirectories or the main directory 
	// should not be accessible through the standard WAD functions but only through 
	// the ones which look for the full name.
	if (Namespace == ns_hidden)
	{
		memset(Name, 0, 8);
	}

	// Since '\' can't be used as a file name's part inside a ZIP
	// we have to work around this for sprites because it is a valid
	// frame character.
	else if (Namespace == ns_sprites || Namespace == ns_voxels)
	{
		char *c;

		while ((c = (char*)memchr(Name, '^', 8)))
		{
			*c = '\\';
		}
	}
}

//==========================================================================
//
// Checks for embedded resource files
//
//==========================================================================

static bool IsWadInFolder(const FResourceFile* const archive, const char* const resPath)
{
	// Checks a special case when <somefile.wad> was put in
	// <myproject> directory inside <myproject.zip>

	if (NULL == archive)
	{
		return false;
	}

    const FString dirName = ExtractFileBase(archive->Filename);
	const FString fileName = ExtractFileBase(resPath, true);
	const FString filePath = dirName + '/' + fileName;

	return 0 == filePath.CompareNoCase(resPath);
}

void FResourceLump::CheckEmbedded()
{
	// Checks for embedded archives
	const char *c = strstr(FullName, ".wad");
	if (c && strlen(c) == 4 && (!strchr(FullName, '/') || IsWadInFolder(Owner, FullName)))
	{
		// Mark all embedded WADs
		Flags |= LUMPF_EMBEDDED;
		memset(Name, 0, 8);
	}
	/* later
	else
	{
		if (c==NULL) c = strstr(Name, ".zip");
		if (c==NULL) c = strstr(Name, ".pk3");
		if (c==NULL) c = strstr(Name, ".7z");
		if (c==NULL) c = strstr(Name, ".pak");
		if (c && strlen(c) <= 4)
		{
			// Mark all embedded archives in any directory
			Flags |= LUMPF_EMBEDDED;
			memset(Name, 0, 8);
		}
	}
	*/

}


//==========================================================================
//
// Returns the owner's FileReader if it can be used to access this lump
//
//==========================================================================

FileReader *FResourceLump::GetReader()
{
	return NULL;
}

//==========================================================================
//
// Returns a file reader to the lump's cache
//
//==========================================================================

FileReader *FResourceLump::NewReader()
{
	return new FLumpReader(this);
}

//==========================================================================
//
// Caches a lump's content and increases the reference counter
//
//==========================================================================

void *FResourceLump::CacheLump()
{
	if (Cache != NULL)
	{
		if (RefCount > 0) RefCount++;
	}
	else if (LumpSize > 0)
	{
		FillCache();
	}
	return Cache;
}

//==========================================================================
//
// Decrements reference counter and frees lump if counter reaches 0
//
//==========================================================================

int FResourceLump::ReleaseCache()
{
	if (LumpSize > 0 && RefCount > 0)
	{
		if (--RefCount == 0)
		{
			delete [] Cache;
			Cache = NULL;
		}
	}
	return RefCount;
}

//==========================================================================
//
// Opens a resource file
//
//==========================================================================

typedef FResourceFile * (*CheckFunc)(const char *filename, FileReader *file, bool quiet);

FResourceFile *CheckWad(const char *filename, FileReader *file, bool quiet);
FResourceFile *CheckGRP(const char *filename, FileReader *file, bool quiet);
FResourceFile *CheckRFF(const char *filename, FileReader *file, bool quiet);
FResourceFile *CheckPak(const char *filename, FileReader *file, bool quiet);
FResourceFile *CheckZip(const char *filename, FileReader *file, bool quiet);
FResourceFile *Check7Z(const char *filename, FileReader *file, bool quiet);
FResourceFile *CheckLump(const char *filename, FileReader *file, bool quiet);
FResourceFile *CheckDir(const char *filename, FileReader *file, bool quiet);

static CheckFunc funcs[] = { CheckWad, CheckZip, Check7Z, CheckPak, CheckGRP, CheckRFF, CheckLump };

FResourceFile *FResourceFile::OpenResourceFile(const char *filename, FileReader *file, bool quiet)
{
	if (file == NULL)
	{
		try
		{
			file = new FileReader(filename);
		}
		catch (CRecoverableError &)
		{
			return NULL;
		}
	}
	for(size_t i = 0; i < countof(funcs); i++)
	{
		FResourceFile *resfile = funcs[i](filename, file, quiet);
		if (resfile != NULL) return resfile;
	}
	return NULL;
}

FResourceFile *FResourceFile::OpenDirectory(const char *filename, bool quiet)
{
	return CheckDir(filename, NULL, quiet);
}

//==========================================================================
//
// Resource file base class
//
//==========================================================================

FResourceFile::FResourceFile(const char *filename, FileReader *r)
{
	if (filename != NULL) Filename = copystring(filename);
	else Filename = NULL;
	Reader = r;
}


FResourceFile::~FResourceFile()
{
	if (Filename != NULL) delete [] Filename;
	delete Reader;
}

int STACK_ARGS lumpcmp(const void * a, const void * b)
{
	FResourceLump * rec1 = (FResourceLump *)a;
	FResourceLump * rec2 = (FResourceLump *)b;

	return rec1->FullName.CompareNoCase(rec2->FullName);
}

//==========================================================================
//
// FResourceFile :: PostProcessArchive
//
// Sorts files by name.
// For files named "filter/<game>/*": Using the same filter rules as config
// autoloading, move them to the end and rename them without the "filter/"
// prefix. Filtered files that don't match are deleted.
//
//==========================================================================

void FResourceFile::PostProcessArchive(void *lumps, size_t lumpsize)
{
	// Entries in archives are sorted alphabetically
	qsort(lumps, NumLumps, lumpsize, lumpcmp);

	// Filter out lumps using the same names as the Autoload.* sections
	// in the ini file use. We reduce the maximum lump concidered after
	// each one so that we don't risk refiltering already filtered lumps.
	DWORD max = NumLumps;
	max -= FilterLumpsByGameType(gameinfo.gametype, lumps, lumpsize, max);

	long len;
	int lastpos = -1;
	FString file;

	while ((len = LumpFilterIWAD.IndexOf('.', lastpos+1)) > 0)
	{
		max -= FilterLumps(LumpFilterIWAD.Left(len), lumps, lumpsize, max);
		lastpos = len;
	}
	JunkLeftoverFilters(lumps, lumpsize, max);
}

//==========================================================================
//
// FResourceFile :: FilterLumps
//
// Finds any lumps between [0,<max>) that match the pattern
// "filter/<filtername>/*" and moves them to the end of the lump list.
// Returns the number of lumps moved.
//
//==========================================================================

int FResourceFile::FilterLumps(FString filtername, void *lumps, size_t lumpsize, DWORD max)
{
	FString filter;
	DWORD start, end;

	if (filtername.IsEmpty())
	{
		return 0;
	}
	filter << "filter/" << filtername << '/';
	if (FindPrefixRange(filter, lumps, lumpsize, max, start, end))
	{
		void *from = (BYTE *)lumps + start * lumpsize;

		// Remove filter prefix from every name
		void *lump_p = from;
		for (DWORD i = start; i < end; ++i, lump_p = (BYTE *)lump_p + lumpsize)
		{
			FResourceLump *lump = (FResourceLump *)lump_p;
			assert(lump->FullName.CompareNoCase(filter, (int)filter.Len()) == 0);
			lump->LumpNameSetup(lump->FullName.Mid(filter.Len()));
		}

		// Move filtered lumps to the end of the lump list.
		size_t count = (end - start) * lumpsize;
		void *to = (BYTE *)lumps + NumLumps * lumpsize - count;
		assert (to >= from);

		if (from != to)
		{
			// Copy filtered lumps to a temporary buffer.
			BYTE *filteredlumps = new BYTE[count];
			memcpy(filteredlumps, from, count);

			// Shift lumps left to make room for the filtered ones at the end.
			memmove(from, (BYTE *)from + count, (NumLumps - end) * lumpsize);

			// Copy temporary buffer to newly freed space.
			memcpy(to, filteredlumps, count);

			delete[] filteredlumps;
		}
	}
	return end - start;
}

//==========================================================================
//
// FResourceFile :: FilterLumpsByGameType
//
// Matches any lumps that match "filter/game-<gametype>/*". Includes
// inclusive gametypes like Raven.
//
//==========================================================================

int FResourceFile::FilterLumpsByGameType(int type, void *lumps, size_t lumpsize, DWORD max)
{
	static const struct { int match; const char *name; } blanket[] =
	{
		{ GAME_Raven,			"game-Raven" },
		{ GAME_DoomStrifeChex,	"game-DoomStrifeChex" },
		{ GAME_DoomChex,		"game-DoomChex" },
		{ GAME_Any, NULL }
	};
	if (type == 0)
	{
		return 0;
	}
	int count = 0;
	for (int i = 0; blanket[i].name != NULL; ++i)
	{
		if (type & blanket[i].match)
		{
			count += FilterLumps(blanket[i].name, lumps, lumpsize, max);
		}
	}
	FString filter = "game-";
	filter += GameNames[type];
	return count + FilterLumps(filter, lumps, lumpsize, max);
}

//==========================================================================
//
// FResourceFile :: JunkLeftoverFilters
//
// Deletes any lumps beginning with "filter/" that were not matched.
//
//==========================================================================

void FResourceFile::JunkLeftoverFilters(void *lumps, size_t lumpsize, DWORD max)
{
	DWORD start, end;
	if (FindPrefixRange("filter/", lumps, lumpsize, max, start, end))
	{
		// Since the resource lumps may contain non-POD data besides the
		// full name, we "delete" them by erasing their names so they
		// can't be found.
		void *stop = (BYTE *)lumps + end * lumpsize;
		for (void *p = (BYTE *)lumps + start * lumpsize; p < stop; p = (BYTE *)p + lumpsize)
		{
			FResourceLump *lump = (FResourceLump *)p;
			lump->FullName = 0;
			lump->Name[0] = '\0';
			lump->Namespace = ns_hidden;
		}
	}
}

//==========================================================================
//
// FResourceFile :: FindPrefixRange
//
// Finds a range of lumps that start with the prefix string. <start> is left
// indicating the first matching one. <end> is left at one plus the last
// matching one.
//
//==========================================================================

bool FResourceFile::FindPrefixRange(FString filter, void *lumps, size_t lumpsize, DWORD maxlump, DWORD &start, DWORD &end)
{
	DWORD min, max, mid, inside;
	FResourceLump *lump;
	int cmp;

	end = start = 0;

	// Pretend that our range starts at 1 instead of 0 so that we can avoid
	// unsigned overflow if the range starts at the first lump.
	lumps = (BYTE *)lumps - lumpsize;

	// Binary search to find any match at all.
	min = 1, max = maxlump;
	while (min <= max)
	{
		mid = min + (max - min) / 2;
		lump = (FResourceLump *)((BYTE *)lumps + mid * lumpsize);
		cmp = lump->FullName.CompareNoCase(filter, (int)filter.Len());
		if (cmp == 0)
			break;
		else if (cmp < 0)
			min = mid + 1;
		else		
			max = mid - 1;
	}
	if (max < min)
	{ // matched nothing
		return false;
	}

	// Binary search to find first match.
	inside = mid;
	min = 1, max = mid;
	while (min <= max)
	{
		mid = min + (max - min) / 2;
		lump = (FResourceLump *)((BYTE *)lumps + mid * lumpsize);
		cmp = lump->FullName.CompareNoCase(filter, (int)filter.Len());
		// Go left on matches and right on misses.
		if (cmp == 0)
			max = mid - 1;
		else
			min = mid + 1;
	}
	start = mid + (cmp != 0) - 1;

	// Binary search to find last match.
	min = inside, max = maxlump;
	while (min <= max)
	{
		mid = min + (max - min) / 2;
		lump = (FResourceLump *)((BYTE *)lumps + mid * lumpsize);
		cmp = lump->FullName.CompareNoCase(filter, (int)filter.Len());
		// Go right on matches and left on misses.
		if (cmp == 0)
			min = mid + 1;
		else
			max = mid - 1;
	}
	end = mid - (cmp != 0);
	return true;
}

//==========================================================================
//
// Needs to be virtual in the base class. Implemented only for WADs
//
//==========================================================================

void FResourceFile::FindStrifeTeaserVoices ()
{
}


//==========================================================================
//
// Caches a lump's content and increases the reference counter
//
//==========================================================================

FileReader *FUncompressedLump::GetReader()
{
	Owner->Reader->Seek(Position, SEEK_SET);
	return Owner->Reader;
}

//==========================================================================
//
// Caches a lump's content and increases the reference counter
//
//==========================================================================

int FUncompressedLump::FillCache()
{
	const char * buffer = Owner->Reader->GetBuffer();

	if (buffer != NULL)
	{
		// This is an in-memory file so the cache can point directly to the file's data.
		Cache = const_cast<char*>(buffer) + Position;
		RefCount = -1;
		return -1;
	}

	Owner->Reader->Seek(Position, SEEK_SET);
	Cache = new char[LumpSize];
	Owner->Reader->Read(Cache, LumpSize);
	RefCount = 1;
	return 1;
}

//==========================================================================
//
// Base class for uncompressed resource files
//
//==========================================================================

FUncompressedFile::FUncompressedFile(const char *filename, FileReader *r)
: FResourceFile(filename, r)
{
	Lumps = NULL;
}

FUncompressedFile::~FUncompressedFile()
{
	if (Lumps != NULL) delete [] Lumps;
}



//==========================================================================
//
// external lump
//
//==========================================================================

FExternalLump::FExternalLump(const char *_filename, int filesize)
{
	filename = _filename? copystring(_filename) : NULL;

	if (filesize == -1)
	{
		FILE *f = fopen(_filename,"rb");
		if (f != NULL)
		{
			fseek(f, 0, SEEK_END);
			LumpSize = ftell(f);
			fclose(f);
		}
		else
		{
			LumpSize = 0;
		}
	}
	else
	{
		LumpSize = filesize;
	}
}


FExternalLump::~FExternalLump()
{
	if (filename != NULL) delete [] filename;
}

//==========================================================================
//
// Caches a lump's content and increases the reference counter
// For external lumps this reopens the file each time it is accessed
//
//==========================================================================

int FExternalLump::FillCache()
{
	Cache = new char[LumpSize];
	FILE *f = fopen(filename, "rb");
	if (f != NULL)
	{
		fread(Cache, 1, LumpSize, f);
		fclose(f);
	}
	else
	{
		memset(Cache, 0, LumpSize);
	}
	RefCount = 1;
	return 1;
}

