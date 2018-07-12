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
#ifdef _WIN32
#include <io.h>
#else
#include <fts.h>
#endif

#include "resourcefile.h"
#include "cmdlib.h"



//==========================================================================
//
// Zip Lump
//
//==========================================================================

struct FDirectoryLump : public FResourceLump
{
	virtual FileReader NewReader();
	virtual int FillCache();

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

	int AddDirectory(const char *dirpath);
	void AddEntry(const char *fullpath, int size);

public:
	FDirectory(const char * dirname);
	bool Open(bool quiet);
	virtual FResourceLump *GetLump(int no) { return ((unsigned)no < NumLumps)? &Lumps[no] : NULL; }
};



//==========================================================================
//
// 
//
//==========================================================================

FDirectory::FDirectory(const char * directory)
: FResourceFile(NULL)
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
	dirname.ReplaceChars('\\', '/');
	if (dirname[dirname.Len()-1] != '/') dirname += '/';
	Filename = copystring(dirname);
}


#ifdef _WIN32
//==========================================================================
//
// Windows version
//
//==========================================================================

int FDirectory::AddDirectory(const char *dirpath)
{
	struct _finddata_t fileinfo;
	intptr_t handle;
	FString dirmatch;
	int count = 0;

	dirmatch = dirpath;
	dirmatch += '*';
	
	if ((handle = _findfirst(dirmatch, &fileinfo)) == -1)
	{
		Printf("Could not scan '%s': %s\n", dirpath, strerror(errno));
	}
	else
	{
		do
		{
			if (fileinfo.attrib & _A_HIDDEN)
			{
				// Skip hidden files and directories. (Prevents SVN bookkeeping
				// info from being included.)
				continue;
			}
			if (fileinfo.attrib & _A_SUBDIR)
			{

				if (fileinfo.name[0] == '.' &&
					(fileinfo.name[1] == '\0' ||
					 (fileinfo.name[1] == '.' && fileinfo.name[2] == '\0')))
				{
					// Do not record . and .. directories.
					continue;
				}
				FString newdir = dirpath;
				newdir << fileinfo.name << '/';
				count += AddDirectory(newdir);
			}
			else
			{
				if (strstr(fileinfo.name, ".orig") || strstr(fileinfo.name, ".bak"))
				{
					// We shouldn't add backup files to the lump directory
					continue;
				}

				AddEntry(FString(dirpath) + fileinfo.name, fileinfo.size);
				count++;
			}
		} while (_findnext(handle, &fileinfo) == 0);
		_findclose(handle);
	}
	return count;
}

#else

//==========================================================================
//
// add_dirs
// 4.4BSD version
//
//==========================================================================

int FDirectory::AddDirectory(const char *dirpath)
{
	char *argv [2] = { NULL, NULL };
	argv[0] = new char[strlen(dirpath)+1];
	strcpy(argv[0], dirpath);
	FTS *fts;
	FTSENT *ent;
	int count = 0;

	fts = fts_open(argv, FTS_LOGICAL, NULL);
	if (fts == NULL)
	{
		Printf("Failed to start directory traversal: %s\n", strerror(errno));
		return 0;
	}

	const size_t namepos = strlen(Filename);
	FString pathfix;

	while ((ent = fts_read(fts)) != NULL)
	{
		if (ent->fts_info == FTS_D && ent->fts_name[0] == '.')
		{
			// Skip hidden directories. (Prevents SVN bookkeeping
			// info from being included.)
			fts_set(fts, ent, FTS_SKIP);
		}
		if (ent->fts_info == FTS_D && ent->fts_level == 0)
		{
			continue;
		}
		if (ent->fts_info != FTS_F)
		{
			// We're only interested in remembering files.
			continue;
		}

		// Some implementations add an extra separator between
		// root of the hierarchy and entity's path.
		// It needs to be removed in order to resolve
		// lumps' relative paths properly.
		const char* path = ent->fts_path;

		if ('/' == path[namepos])
		{
			pathfix = FString(path, namepos);
			pathfix.AppendCStrPart(&path[namepos + 1], ent->fts_pathlen - namepos - 1);

			path = pathfix.GetChars();
		}

		AddEntry(path, ent->fts_statp->st_size);
		count++;
	}
	fts_close(fts);
	delete[] argv[0];
	return count;
}
#endif


//==========================================================================
//
//
//
//==========================================================================

bool FDirectory::Open(bool quiet)
{
	NumLumps = AddDirectory(Filename);
	if (!quiet) Printf(", %d lumps\n", NumLumps);
	PostProcessArchive(&Lumps[0], sizeof(FDirectoryLump));
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
	FString name = fullpath + strlen(Filename);
	name.ToLower();

	// The lump's name is only the part relative to the main directory
	lump_p->LumpNameSetup(name);
	lump_p->LumpSize = size;
	lump_p->Owner = this;
	lump_p->Flags = 0;
	lump_p->CheckEmbedded();
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

FResourceFile *CheckDir(const char *filename, bool quiet)
{
	FResourceFile *rf = new FDirectory(filename);
	if (rf->Open(quiet)) return rf;
	delete rf;
	return NULL;
}

