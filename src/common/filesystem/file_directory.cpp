/*
** file_directory.cpp
**
**---------------------------------------------------------------------------
** Copyright 2008-2009 Randy Heit
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


#include <sys/stat.h>

#include "resourcefile.h"
#include "cmdlib.h"
#include "findfile.h"

//==========================================================================
//
// Zip Lump
//
//==========================================================================

struct FDirectoryLump : public FResourceLump
{
	FileReader NewReader() override;
	int FillCache() override;

	FString mFullPath;
};


//==========================================================================
//
// Zip file
//
//==========================================================================

class FDirectory : public FResourceFile
{
	TArray<FDirectoryLump> Lumps;
	const bool nosubdir;

	int AddDirectory(const char* dirpath, FileSystemMessageFunc Printf);
	void AddEntry(const char *fullpath, int size);

public:
	FDirectory(const char * dirname, bool nosubdirflag = false);
	bool Open(LumpFilterInfo* filter, FileSystemMessageFunc Printf);
	virtual FResourceLump *GetLump(int no) { return ((unsigned)no < NumLumps)? &Lumps[no] : NULL; }
};



//==========================================================================
//
// 
//
//==========================================================================

FDirectory::FDirectory(const char * directory, bool nosubdirflag)
: FResourceFile(NULL), nosubdir(nosubdirflag)
{
	FString dirname;

	#ifdef _WIN32
		directory = _fullpath(NULL, directory, _MAX_PATH);
	#else
		// Todo for Linux: Resolve the path before using it
	#endif
	dirname = directory;
#ifdef _WIN32
	free((void *)directory);
#endif
	dirname.Substitute("\\", "/");
	if (dirname[dirname.Len()-1] != '/') dirname += '/';
	FileName = dirname;
}


//==========================================================================
//
// Windows version
//
//==========================================================================

int FDirectory::AddDirectory(const char *dirpath, FileSystemMessageFunc Printf)
{
	void * handle;
	int count = 0;

	FString dirmatch = dirpath;
	findstate_t find;
	dirmatch += '*';

	handle = I_FindFirst(dirmatch.GetChars(), &find);
	if (handle == ((void *)(-1)))
	{
		Printf(FSMessageLevel::Error, "Could not scan '%s': %s\n", dirpath, strerror(errno));
	}
	else
	{
		do
		{
			// I_FindName only returns the file's name and not its full path
			auto attr = I_FindAttr(&find);
			if (attr & FA_HIDDEN)
			{
				// Skip hidden files and directories. (Prevents SVN bookkeeping
				// info from being included.)
				continue;
			}
			FString fi = I_FindName(&find);
			if (attr &  FA_DIREC)
			{
				if (nosubdir || (fi[0] == '.' &&
								 (fi[1] == '\0' ||
								  (fi[1] == '.' && fi[2] == '\0'))))
				{
					// Do not record . and .. directories.
					continue;
				}
				FString newdir = dirpath;
				newdir << fi << '/';
				count += AddDirectory(newdir, Printf);
			}
			else
			{
				if (strstr(fi, ".orig") || strstr(fi, ".bak") || strstr(fi, ".cache"))
				{
					// We shouldn't add backup files to the file system
					continue;
				}
				size_t size = 0;
				FString fn = FString(dirpath) + fi;

				// The next one is courtesy of EDuke32. :(
				// Putting cache files in the application directory is very bad style.
				// Unfortunately, having a garbage file named "texture" present will cause serious problems down the line.
				if (!stricmp(fi, "textures"))
				{
					FILE* f = fopen(fn, "rb");
					if (f)
					{
						char check[3]{};
						fread(check, 1, 3, f);
						if (!memcmp(check, "LZ4", 3)) continue;
					}
				}

				if (GetFileInfo(fn, &size, nullptr))
				{
					AddEntry(fn, (int)size);
					count++;
				}
			}

		} while (I_FindNext (handle, &find) == 0);
		I_FindClose (handle);
	}
	return count;
}

//==========================================================================
//
//
//
//==========================================================================

bool FDirectory::Open(LumpFilterInfo* filter, FileSystemMessageFunc Printf)
{
	NumLumps = AddDirectory(FileName, Printf);
	PostProcessArchive(&Lumps[0], sizeof(FDirectoryLump), filter);
	return true;
}

//==========================================================================
//
//
//
//==========================================================================

void FDirectory::AddEntry(const char *fullpath, int size)
{
	FDirectoryLump *lump_p = &Lumps[Lumps.Reserve(1)];

	// Store the full path here so that we can access the file later, even if it is from a filter directory.
	lump_p->mFullPath = fullpath;

	// [mxd] Convert name to lowercase
	FString name = fullpath + strlen(FileName);
	name.ToLower();

	// The lump's name is only the part relative to the main directory
	lump_p->LumpNameSetup(name);
	lump_p->LumpSize = size;
	lump_p->Owner = this;
	lump_p->Flags = 0;
	lump_p->CheckEmbedded(nullptr);
}


//==========================================================================
//
//
//
//==========================================================================

FileReader FDirectoryLump::NewReader()
{
	FileReader fr;
	fr.OpenFile(mFullPath);
	return fr;
}

//==========================================================================
//
//
//
//==========================================================================

int FDirectoryLump::FillCache()
{
	FileReader fr;
	Cache = new char[LumpSize];
	if (!fr.OpenFile(mFullPath))
	{
		memset(Cache, 0, LumpSize);
		return 0;
	}
	fr.Read(Cache, LumpSize);
	RefCount = 1;
	return 1;
}

//==========================================================================
//
// File open
//
//==========================================================================

FResourceFile *CheckDir(const char *filename, bool nosubdirflag, LumpFilterInfo* filter, FileSystemMessageFunc Printf)
{
	auto rf = new FDirectory(filename, nosubdirflag);
	if (rf->Open(filter, Printf)) return rf;
	delete rf;
	return nullptr;
}

