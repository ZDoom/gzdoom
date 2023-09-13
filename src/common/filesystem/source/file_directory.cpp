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
#include "fs_stringpool.h"

namespace FileSys {
	
std::string FS_FullPath(const char* directory);

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

	const char* mFullPath;
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
	void AddEntry(const char *fullpath, const char* relpath, int size);

public:
	FDirectory(const char * dirname, StringPool* sp, bool nosubdirflag = false);
	bool Open(LumpFilterInfo* filter, FileSystemMessageFunc Printf);
	virtual FResourceLump *GetLump(int no) { return ((unsigned)no < NumLumps)? &Lumps[no] : NULL; }
};



//==========================================================================
//
// 
//
//==========================================================================

FDirectory::FDirectory(const char * directory, StringPool* sp, bool nosubdirflag)
	: FResourceFile("", sp), nosubdir(nosubdirflag)
{
	auto fn = FS_FullPath(directory);
	if (fn.back() != '/') fn += '/';
	FileName = stringpool->Strdup(fn.c_str());
}

//==========================================================================
//
// Windows version
//
//==========================================================================

int FDirectory::AddDirectory(const char *dirpath, LumpFilterInfo* filter, FileSystemMessageFunc Printf)
{
	int count = 0;

	FileList list;
	if (!ScanDirectory(list, dirpath, "*"))
	{
		Printf(FSMessageLevel::Error, "Could not scan '%s': %s\n", dirpath, strerror(errno));
	}
	else
	{
		for(auto& entry : list)
		{
			if (!entry.isDirectory)
			{
				auto fi = entry.FileName;
				for (auto& c : fi) c = tolower(c);
				if (strstr(fi.c_str(), ".orig") || strstr(fi.c_str(), ".bak") || strstr(fi.c_str(), ".cache"))
				{
					// We shouldn't add backup files to the file system
					continue;
				}

				if (filter->filenamecheck == nullptr || filter->filenamecheck(fi.c_str(), entry.FilePath.c_str()))
				{
					if (entry.Length > 0x7fffffff)
					{
						Printf(FSMessageLevel::Warning, "%s is larger than 2GB and will be ignored\n", entry.FilePath.c_str());
						continue;
					}
					AddEntry(entry.FilePath.c_str(), entry.FilePathRel.c_str(), (int)entry.Length);
					count++;
				}
			}
		}
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
	NumLumps = AddDirectory(FileName, filter, Printf);
	PostProcessArchive(&Lumps[0], sizeof(FDirectoryLump), filter);
	return true;
}

//==========================================================================
//
//
//
//==========================================================================

void FDirectory::AddEntry(const char *fullpath, const char* relpath, int size)
{
	FDirectoryLump *lump_p = &Lumps[Lumps.Reserve(1)];

	// Store the full path here so that we can access the file later, even if it is from a filter directory.
	lump_p->mFullPath = stringpool->Strdup(fullpath);

	// [mxd] Convert name to lowercase
	std::string name = relpath;
	for (auto& c : name) c = tolower(c);

	// The lump's name is only the part relative to the main directory
	lump_p->LumpNameSetup(name.c_str(), stringpool);
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

FResourceFile *CheckDir(const char *filename, bool nosubdirflag, LumpFilterInfo* filter, FileSystemMessageFunc Printf, StringPool* sp)
{
	auto rf = new FDirectory(filename, sp, nosubdirflag);
	if (rf->Open(filter, Printf)) return rf;
	delete rf;
	return nullptr;
}

}
