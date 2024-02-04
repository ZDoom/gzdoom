/*
** findfile.cpp
** Warpper around the native directory scanning API
**
**---------------------------------------------------------------------------
** Copyright 1998-2016 Randy Heit
** Copyright 2005-2020 Christoph Oelckers
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
*/

#include "findfile.h"
#include "zstring.h"
#include "cmdlib.h"
#include "printf.h"
#include "configfile.h"
#include "i_system.h"
#include "fs_findfile.h"

#ifdef __unix__
#include <sys/stat.h>
#endif // __unix__

//==========================================================================
//
// D_AddFile
//
//==========================================================================

bool D_AddFile(std::vector<std::string>& wadfiles, const char* file, bool check, int position, FConfigFile* config)
{
	if (file == nullptr || *file == '\0')
	{
		return false;
	}
#ifdef __unix__
	// Confirm file exists in filesystem.
	struct stat info;
	bool found = stat(file, &info) == 0;
	FString fullpath = file;
	if (!found)
	{
		// File not found, so split file into path and filename so we can enumerate the path for the file.
		auto lastindex = fullpath.LastIndexOf("/");
		FString basepath = fullpath.Left(lastindex);
		FString filename = fullpath.Right(fullpath.Len() - lastindex - 1);

		// Proceed only if locating a file (i.e. `file` isn't a path to just a directory.)
		if (filename.IsNotEmpty())
		{
			DIR *d;
			struct dirent *dir;
			d = opendir(basepath.GetChars());
			if (d)
			{
				while ((dir = readdir(d)) != NULL)
				{
					if (filename.CompareNoCase(dir->d_name) == 0)
					{
						found = true;
						filename = dir->d_name;
						fullpath = basepath << "/" << filename;
						file = fullpath.GetChars();
						break;
					}
				}
				closedir(d);
				if (!found)
				{
					//Printf("Can't find file '%s' in '%s'\n", filename.GetChars(), basepath.GetChars());
					return false;
				}
			}
			else
			{
				Printf("Can't open directory '%s'\n", basepath.GetChars());
				return false;
			}
		}
	}
#endif

	if (check && !DirEntryExists(file))
	{
		const char* f = BaseFileSearch(file, ".wad", false, config);
		if (f == nullptr)
		{
			Printf("Can't find '%s'\n", file);
			return false;
		}
		file = f;
	}

	std::string f = file;
	for (auto& c : f) if (c == '\\') c = '/';
	if (position == -1) wadfiles.push_back(f);
	else wadfiles.insert(wadfiles.begin() + position, f);
	return true;
}

//==========================================================================
//
// D_AddWildFile
//
//==========================================================================

void D_AddWildFile(std::vector<std::string>& wadfiles, const char* value, const char *extension, FConfigFile* config)
{
	if (value == nullptr || *value == '\0')
	{
		return;
	}
	const char* wadfile = BaseFileSearch(value, extension, false, config);

	if (wadfile != nullptr)
	{
		D_AddFile(wadfiles, wadfile, true, -1, config);
	}
	else 
	{
		// Try pattern matching
		FileSys::FileList list;
		auto path = ExtractFilePath(value);
		auto name = ExtractFileBase(value, true);
		if (path.IsEmpty()) path = ".";
		if (FileSys::ScanDirectory(list, path.GetChars(), name.GetChars(), true))
		{ 
			for(auto& entry : list)
			{
				D_AddFile(wadfiles, entry.FilePath.c_str(), true, -1, config);
			}
		}
	}
}

//==========================================================================
//
// D_AddConfigWads
//
// Adds all files in the specified config file section.
//
//==========================================================================

void D_AddConfigFiles(std::vector<std::string>& wadfiles, const char* section, const char* extension, FConfigFile *config)
{
	if (config && config->SetSection(section))
	{
		const char* key;
		const char* value;
		FConfigFile::Position pos;

		while (config->NextInSection(key, value))
		{
			if (stricmp(key, "Path") == 0)
			{
				// D_AddWildFile resets config's position, so remember it
				config->GetPosition(pos);
				D_AddWildFile(wadfiles, ExpandEnvVars(value).GetChars(), extension, config);
				// Reset config's position to get next wad
				config->SetPosition(pos);
			}
		}
	}
}

//==========================================================================
//
// D_AddDirectory
//
// Add all .wad files in a directory. Does not descend into subdirectories.
//
//==========================================================================

void D_AddDirectory(std::vector<std::string>& wadfiles, const char* dir, const char *filespec, FConfigFile* config)
{
	FileSys::FileList list;
	if (FileSys::ScanDirectory(list, dir, "*.wad", true))
	{
		for (auto& entry : list)
		{
			if (!entry.isDirectory)
			{
				D_AddFile(wadfiles, entry.FilePath.c_str(), true, -1, config);
			}
		}
	}
}

//==========================================================================
//
// BaseFileSearch
//
// If a file does not exist at <file>, looks for it in the directories
// specified in the config file. Returns the path to the file, if found,
// or nullptr if it could not be found.
//
//==========================================================================

static FString BFSwad; // outside the function to evade C++'s insane rules for constructing static variables inside functions.

const char* BaseFileSearch(const char* file, const char* ext, bool lookfirstinprogdir, FConfigFile* config)
{
	if (file == nullptr || *file == '\0')
	{
		return nullptr;
	}
	if (lookfirstinprogdir)
	{
		BFSwad.Format("%s%s%s", progdir.GetChars(), progdir.Back() == '/' ? "" : "/", file);
		if (DirEntryExists(BFSwad.GetChars()))
		{
			return BFSwad.GetChars();
		}
	}

	if (DirEntryExists(file))
	{
		BFSwad.Format("%s", file);
		return BFSwad.GetChars();
	}

	if (config != nullptr && config->SetSection("FileSearch.Directories"))
	{
		const char* key;
		const char* value;

		while (config->NextInSection(key, value))
		{
			if (stricmp(key, "Path") == 0)
			{
				FString dir;

				dir = NicePath(value);
				if (dir.IsNotEmpty())
				{
					BFSwad.Format("%s%s%s", dir.GetChars(), dir.Back() == '/' ? "" : "/", file);
					if (DirEntryExists(BFSwad.GetChars()))
					{
						return BFSwad.GetChars();
					}
				}
			}
		}
	}

	// Retry, this time with a default extension
	if (ext != nullptr)
	{
		FString tmp = file;
		DefaultExtension(tmp, ext);
		return BaseFileSearch(tmp.GetChars(), nullptr, lookfirstinprogdir, config);
	}
	return nullptr;
}

