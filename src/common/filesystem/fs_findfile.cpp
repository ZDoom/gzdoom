/*
** findfile.cpp
** Wrapper around the native directory scanning APIs
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

#include "fs_findfile.h"

#ifndef _WIN32

#include <unistd.h>
#include <fnmatch.h>
#include <sys/stat.h>

static const char *pattern;

static int matchfile(const struct dirent *ent)
{
	return fnmatch(pattern, ent->d_name, FNM_NOESCAPE) == 0;
}

void *FS_FindFirst(const char *const filespec, fs_findstate_t *const fileinfo)
{
	const char* dir;

	const char *const slash = strrchr(filespec, '/');

	if (slash)
	{
		pattern = slash + 1;
		fileinfo->path = std::string(filespec, slash - filespec + 1);
		dir = fileinfo->path.c_str();
	}
	else
	{
		pattern = filespec;
		dir = ".";
	}

	fileinfo->current = 0;
	fileinfo->count = scandir(dir, &fileinfo->namelist, matchfile, alphasort);

	if (fileinfo->count > 0)
	{
		return fileinfo;
	}

	return (void *)-1;
}

int FS_FindNext(void *const handle, fs_findstate_t *const fileinfo)
{
	fs_findstate_t *const state = static_cast<fs_findstate_t *>(handle);

	if (state->current < fileinfo->count)
	{
		return ++state->current < fileinfo->count ? 0 : -1;
	}

	return -1;
}

int FS_FindClose(void *const handle)
{
	fs_findstate_t *const state = static_cast<fs_findstate_t *>(handle);

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

static bool DirEntryExists(const char* pathname, bool* isdir)
{
	if (isdir) *isdir = false;
	struct stat info;
	bool res = stat(pathname, &info) == 0;
	if (isdir) *isdir = !!(info.st_mode & S_IFDIR);
	return res;
}

int FS_FindAttr(fs_findstate_t *const fileinfo)
{
	dirent *const ent = fileinfo->namelist[fileinfo->current];
	const std::string path = fileinfo->path + ent->d_name;
	bool isdir;

	if (DirEntryExists(path.c_str(), &isdir))
	{
		return isdir ? FA_DIREC : 0;
	}

	return 0;
}

std::string FS_FullPath(const char* directory)
{
	// todo
	return directory
}

#else

#include <windows.h>
#include <direct.h>

std::wstring toWide(const char* str)
{
	int len = (int)strlen(str);
	int size_needed = MultiByteToWideChar(CP_UTF8, 0, str, len, nullptr, 0);
	std::wstring wide;
	wide.resize(size_needed);
	MultiByteToWideChar(CP_UTF8, 0, str, len, &wide.front(), size_needed);
	return wide;
}

static std::string toUtf8(const wchar_t* str)
{
	auto len = wcslen(str);
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, str, (int)len, nullptr, 0, nullptr, nullptr);
	std::string utf8;
	utf8.resize(size_needed);
	WideCharToMultiByte(CP_UTF8, 0, str, (int)len, &utf8.front(), size_needed, nullptr, nullptr);
	return utf8;
}

//==========================================================================
//
// FS_FindFirst
//
// Start a pattern matching sequence.
//
//==========================================================================


void *FS_FindFirst(const char *filespec, fs_findstate_t *fileinfo)
{
	static_assert(sizeof(WIN32_FIND_DATAW) == sizeof(fileinfo->FindData), "FindData size mismatch");
	fileinfo->UTF8Name = "";
	return FindFirstFileW(toWide(filespec).c_str(), (LPWIN32_FIND_DATAW)&fileinfo->FindData);
}

//==========================================================================
//
// FS_FindNext
//
// Return the next file in a pattern matching sequence.
//
//==========================================================================

int FS_FindNext(void *handle, fs_findstate_t *fileinfo)
{
	fileinfo->UTF8Name = "";
	return FindNextFileW((HANDLE)handle, (LPWIN32_FIND_DATAW)&fileinfo->FindData) == 0;
}

//==========================================================================
//
// FS_FindClose
//
// Finish a pattern matching sequence.
//
//==========================================================================

int FS_FindClose(void *handle)
{
	return FindClose((HANDLE)handle);
}

//==========================================================================
//
// FS_FindName
//
// Returns the name for an entry
//
//==========================================================================

const char *FS_FindName(fs_findstate_t *fileinfo)
{
	if (fileinfo->UTF8Name.empty())
	{
		fileinfo->UTF8Name = toUtf8(fileinfo->FindData.Name);
	}
	return fileinfo->UTF8Name.c_str();
}

std::string FS_FullPath(const char* directory)
{
	auto wdirectory = _wfullpath(nullptr, toWide(directory).c_str(), _MAX_PATH);
	std::string sdirectory = toUtf8(wdirectory);
	free((void*)wdirectory);
	for (auto& c : sdirectory) if (c == '\\') c = '/';
	return sdirectory;
}
#endif
