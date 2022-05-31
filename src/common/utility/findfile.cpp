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

#ifndef _WIN32

#include <unistd.h>
#include <fnmatch.h>
#include <sys/stat.h>

#include "cmdlib.h"

static const char *pattern;

static int matchfile(const struct dirent *ent)
{
	return fnmatch(pattern, ent->d_name, FNM_NOESCAPE) == 0;
}

void *I_FindFirst(const char *const filespec, findstate_t *const fileinfo)
{
	FString dir;

	const char *const slash = strrchr(filespec, '/');

	if (slash)
	{
		pattern = slash + 1;
		dir = FString(filespec, slash - filespec + 1);
		fileinfo->path = dir;
	}
	else
	{
		pattern = filespec;
		dir = ".";
	}

	fileinfo->current = 0;
	fileinfo->count = scandir(dir.GetChars(), &fileinfo->namelist, matchfile, alphasort);

	if (fileinfo->count > 0)
	{
		return fileinfo;
	}

	return (void *)-1;
}

int I_FindNext(void *const handle, findstate_t *const fileinfo)
{
	findstate_t *const state = static_cast<findstate_t *>(handle);

	if (state->current < fileinfo->count)
	{
		return ++state->current < fileinfo->count ? 0 : -1;
	}

	return -1;
}

int I_FindClose(void *const handle)
{
	findstate_t *const state = static_cast<findstate_t *>(handle);

	if (handle != (void *)-1 && state->count > 0)
	{
		for (int i = 0; i < state->count; ++i)
		{
			free(state->namelist[i]);
		}

		free(state->namelist);
		state->namelist = nullptr;
		state->count = 0;
	}

	return 0;
}

int I_FindAttr(findstate_t *const fileinfo)
{
	dirent *const ent = fileinfo->namelist[fileinfo->current];
	const FString path = fileinfo->path + ent->d_name;
	bool isdir;

	if (DirEntryExists(path, &isdir))
	{
		return isdir ? FA_DIREC : 0;
	}

	return 0;
}

#else

#include <windows.h>
#include <direct.h>

//==========================================================================
//
// I_FindFirst
//
// Start a pattern matching sequence.
//
//==========================================================================


void *I_FindFirst(const char *filespec, findstate_t *fileinfo)
{
	static_assert(sizeof(WIN32_FIND_DATAW) == sizeof(fileinfo->FindData), "FindData size mismatch");
	auto widespec = WideString(filespec);
	fileinfo->UTF8Name = "";
	return FindFirstFileW(widespec.c_str(), (LPWIN32_FIND_DATAW)&fileinfo->FindData);
}

//==========================================================================
//
// I_FindNext
//
// Return the next file in a pattern matching sequence.
//
//==========================================================================

int I_FindNext(void *handle, findstate_t *fileinfo)
{
	fileinfo->UTF8Name = "";
	return !FindNextFileW((HANDLE)handle, (LPWIN32_FIND_DATAW)&fileinfo->FindData);
}

//==========================================================================
//
// I_FindClose
//
// Finish a pattern matching sequence.
//
//==========================================================================

int I_FindClose(void *handle)
{
	return FindClose((HANDLE)handle);
}

//==========================================================================
//
// I_FindName
//
// Returns the name for an entry
//
//==========================================================================

const char *I_FindName(findstate_t *fileinfo)
{
	if (fileinfo->UTF8Name.IsEmpty()) fileinfo->UTF8Name = fileinfo->FindData.Name;
	return fileinfo->UTF8Name.GetChars();
}

#endif

//==========================================================================
//
// D_AddFile
//
//==========================================================================

bool D_AddFile(TArray<FString>& wadfiles, const char* file, bool check, int position, FConfigFile* config)
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
					Printf("Can't find file '%s' in '%s'\n", filename.GetChars(), basepath.GetChars());
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

	FString f = file;
	FixPathSeperator(f);
	if (position == -1) wadfiles.Push(f);
	else wadfiles.Insert(position, f);
	return true;
}

//==========================================================================
//
// D_AddWildFile
//
//==========================================================================

void D_AddWildFile(TArray<FString>& wadfiles, const char* value, const char *extension, FConfigFile* config)
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
	{ // Try pattern matching
		findstate_t findstate;
		char path[ZPATH_MAX];
		char* sep;
		void* handle = I_FindFirst(value, &findstate);

		strcpy(path, value);
		sep = strrchr(path, '/');
		if (sep == nullptr)
		{
			sep = strrchr(path, '\\');
#ifdef _WIN32
			if (sep == nullptr && path[1] == ':')
			{
				sep = path + 1;
			}
#endif
		}

		if (handle != ((void*)-1))
		{
			do
			{
				if (!(I_FindAttr(&findstate) & FA_DIREC))
				{
					if (sep == nullptr)
					{
						D_AddFile(wadfiles, I_FindName(&findstate), true, -1, config);
					}
					else
					{
						strcpy(sep + 1, I_FindName(&findstate));
						D_AddFile(wadfiles, path, true, -1, config);
					}
				}
			} while (I_FindNext(handle, &findstate) == 0);
		}
		I_FindClose(handle);
	}
}

//==========================================================================
//
// D_AddConfigWads
//
// Adds all files in the specified config file section.
//
//==========================================================================

void D_AddConfigFiles(TArray<FString>& wadfiles, const char* section, const char* extension, FConfigFile *config)
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
				D_AddWildFile(wadfiles, ExpandEnvVars(value), extension, config);
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

void D_AddDirectory(TArray<FString>& wadfiles, const char* dir, const char *filespec, FConfigFile* config)
{
	char curdir[ZPATH_MAX];

	if (getcwd(curdir, ZPATH_MAX))
	{
		char skindir[ZPATH_MAX];
		findstate_t findstate;
		void* handle;
		size_t stuffstart;

		stuffstart = strlen(dir);
		memcpy(skindir, dir, stuffstart * sizeof(*dir));
		skindir[stuffstart] = 0;

		if (skindir[stuffstart - 1] == '/')
		{
			skindir[--stuffstart] = 0;
		}

		if (!chdir(skindir))
		{
			skindir[stuffstart++] = '/';
			if ((handle = I_FindFirst(filespec, &findstate)) != (void*)-1)
			{
				do
				{
					if (!(I_FindAttr(&findstate) & FA_DIREC))
					{
						strcpy(skindir + stuffstart, I_FindName(&findstate));
						D_AddFile(wadfiles, skindir, true, -1, config);
					}
				} while (I_FindNext(handle, &findstate) == 0);
				I_FindClose(handle);
			}
		}
		chdir(curdir);
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

const char* BaseFileSearch(const char* file, const char* ext, bool lookfirstinprogdir, FConfigFile* config)
{
	static char wad[ZPATH_MAX];

	if (file == nullptr || *file == '\0')
	{
		return nullptr;
	}
	if (lookfirstinprogdir)
	{
		mysnprintf(wad, countof(wad), "%s%s%s", progdir.GetChars(), progdir.Back() == '/' ? "" : "/", file);
		if (DirEntryExists(wad))
		{
			return wad;
		}
	}

	if (DirEntryExists(file))
	{
		mysnprintf(wad, countof(wad), "%s", file);
		return wad;
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
					mysnprintf(wad, countof(wad), "%s%s%s", dir.GetChars(), dir.Back() == '/' ? "" : "/", file);
					if (DirEntryExists(wad))
					{
						return wad;
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
		return BaseFileSearch(tmp, nullptr, lookfirstinprogdir, config);
	}
	return nullptr;
}

