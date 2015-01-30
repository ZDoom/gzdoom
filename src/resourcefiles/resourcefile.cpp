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
	if (FullName != NULL)
	{
		delete [] FullName;
		FullName = NULL;
	}
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

void FResourceLump::LumpNameSetup(const char *iname)
{
	const char *lname = strrchr(iname,'/');
	lname = (lname == NULL) ? iname : lname + 1;
	FString base = lname;
	base = base.Left(base.LastIndexOf('.'));
	uppercopy(Name, base);
	Name[8] = 0;
	FullName = copystring(iname);

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
				-1;
	
	// Anything that is not in one of these subdirectories or the main directory 
	// should not be accessible through the standard WAD functions but only through 
	// the ones which look for the full name.
	if (Namespace == -1)
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

