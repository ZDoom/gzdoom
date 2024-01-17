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

//==========================================================================
//
// Zip file
//
//==========================================================================

class FDirectory : public FResourceFile
{
	const bool nosubdir;
	const char* mBasePath;
	const char** SystemFilePath;


	int AddDirectory(const char* dirpath, LumpFilterInfo* filter, FileSystemMessageFunc Printf);

public:
	FDirectory(const char * dirname, StringPool* sp, bool nosubdirflag = false);
	bool Open(LumpFilterInfo* filter, FileSystemMessageFunc Printf);
	FileReader GetEntryReader(uint32_t entry, int, int) override;
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
		mBasePath = nullptr;
		AllocateEntries((int)list.size());
		SystemFilePath = (const char**)stringpool->Alloc(list.size() * sizeof(const char*));
		for(auto& entry : list)
		{
			if (mBasePath == nullptr)
			{
				// extract the base path from the first entry to cover changes made in ScanDirectory.
				auto full = entry.FilePath.find(entry.FilePathRel);
				std::string path(entry.FilePath, 0, full);
				mBasePath = stringpool->Strdup(path.c_str());
			}
			if (!entry.isDirectory)
			{
				auto fi = entry.FileName;
				for (auto& c : fi) c = tolower(c);
				if (strstr(fi.c_str(), ".orig") || strstr(fi.c_str(), ".bak") || strstr(fi.c_str(), ".cache"))
				{
					// We shouldn't add backup files to the file system
					continue;
				}


				if (filter == nullptr || filter->filenamecheck == nullptr || filter->filenamecheck(fi.c_str(), entry.FilePath.c_str()))
				{
					if (entry.Length > 0x7fffffff)
					{
						Printf(FSMessageLevel::Warning, "%s is larger than 2GB and will be ignored\n", entry.FilePath.c_str());
						continue;
					}
					// for accessing the file we need to retain the original unaltered path.
					// On Linux this is important because its file system is case sensitive,
					// but even on Windows the Unicode normalization is destructive 
					// for some characters and cannot be used for file names.
					// Examples for this are the Turkish 'i's or the German ß.
					SystemFilePath[count] = stringpool->Strdup(entry.FilePathRel.c_str());
					// for internal access we use the normalized form of the relative path.
					// this is fine because the paths that get compared against this will also be normalized.
					Entries[count].FileName = NormalizeFileName(entry.FilePathRel.c_str());
					Entries[count].CompressedSize = Entries[count].Length = entry.Length;
					Entries[count].Flags = RESFF_FULLPATH;
					Entries[count].ResourceID = -1;
					Entries[count].Method = METHOD_STORED;
					Entries[count].Namespace = ns_global;
					Entries[count].Position = count;
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
	PostProcessArchive(filter);
	return true;
}

//==========================================================================
//
//
//
//==========================================================================

FileReader FDirectory::GetEntryReader(uint32_t entry, int readertype, int)
{
	FileReader fr;
	if (entry < NumLumps)
	{
		std::string fn = mBasePath;
		fn += SystemFilePath[Entries[entry].Position];
		fr.OpenFile(fn.c_str());
		if (readertype == READER_CACHED)
		{
			auto data = fr.Read();
			fr.OpenMemoryArray(data);
		}
	}
	return fr;
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
