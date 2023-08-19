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
#include "fs_findfile.h"

#ifdef _WIN32
std::wstring toWide(const char* str);
#endif

//==========================================================================
//
// Zip Lump
//
//==========================================================================

struct FDirectoryLump : public FResourceLump
{
	FileReader NewReader() override;
	int FillCache() override;

	std::string mFullPath;
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

	int AddDirectory(const char* dirpath, LumpFilterInfo* filter, FileSystemMessageFunc Printf);
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
	: FResourceFile(""), nosubdir(nosubdirflag)
{
	FileName = FS_FullPath(directory);
	if (FileName[FileName.length()-1] != '/') FileName += '/';
}

//==========================================================================
//
// Windows version
//
//==========================================================================

static bool FS_GetFileInfo(const char* pathname, size_t* size)
{
#ifndef _WIN32
	struct stat info;
	bool res = stat(pathname, &info) == 0;
#else
	// Windows must use the wide version of stat to preserve non-ASCII paths.
	struct _stat64 info;
	bool res = _wstat64(toWide(pathname).c_str(), &info) == 0;
#endif
	if (!res || (info.st_mode & S_IFDIR)) return false;
	if (size) *size = (size_t)info.st_size;
	return res;
}

//==========================================================================
//
// Windows version
//
//==========================================================================

int FDirectory::AddDirectory(const char *dirpath, LumpFilterInfo* filter, FileSystemMessageFunc Printf)
{
	void * handle;
	int count = 0;

	std::string dirmatch = dirpath;
	dirmatch += '*';
	fs_findstate_t find;

	handle = FS_FindFirst(dirmatch.c_str(), &find);
	if (handle == ((void *)(-1)))
	{
		Printf(FSMessageLevel::Error, "Could not scan '%s': %s\n", dirpath, strerror(errno));
	}
	else
	{
		do
		{
			// FS_FindName only returns the file's name and not its full path
			auto attr = FS_FindAttr(&find);
			if (attr & FA_HIDDEN)
			{
				// Skip hidden files and directories. (Prevents SVN/Git bookkeeping
				// info from being included.)
				continue;
			}
			const char* fi = FS_FindName(&find);
			if (attr &  FA_DIREC)
			{
				if (nosubdir || (fi[0] == '.' &&
								 (fi[1] == '\0' ||
								  (fi[1] == '.' && fi[2] == '\0'))))
				{
					// Do not record . and .. directories.
					continue;
				}
				std::string newdir = dirpath;
				newdir += fi;
				newdir += '/';
				count += AddDirectory(newdir.c_str(), filter, Printf);
			}
			else
			{
				if (strstr(fi, ".orig") || strstr(fi, ".bak") || strstr(fi, ".cache"))
				{
					// We shouldn't add backup files to the file system
					continue;
				}
				size_t size = 0;
				std::string fn = dirpath;
				fn += fi;

				if (filter->filenamecheck == nullptr || filter->filenamecheck(fi, fn.c_str()))
				{
					if (FS_GetFileInfo(fn.c_str(), &size))
					{
						if (size > 0x7fffffff)
						{
							Printf(FSMessageLevel::Warning, "%s is larger than 2GB and will be ignored\n", fn.c_str());
						}
						AddEntry(fn.c_str(), (int)size);
						count++;
					}
				}
			}

		} while (FS_FindNext (handle, &find) == 0);
		FS_FindClose (handle);
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
	NumLumps = AddDirectory(FileName.c_str(), filter, Printf);
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
	std::string name = fullpath + FileName.length();
	for (auto& c : name) c = tolower(c);

	// The lump's name is only the part relative to the main directory
	lump_p->LumpNameSetup(name.c_str());
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
	fr.OpenFile(mFullPath.c_str());
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
	if (!fr.OpenFile(mFullPath.c_str()))
	{
		throw FileSystemException("unable to open file");
	}
	auto read = fr.Read(Cache, LumpSize);
	if (read != LumpSize)
	{
		throw FileSystemException("only read %d of %d bytes", (int)read, (int)LumpSize);
	}
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

