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
#include <string.h>
#include <vector>
#include <sys/stat.h>

#ifndef _WIN32

#include <limits.h>
#include <stdlib.h>
#ifdef __FreeBSD__
#include <sys/time.h>
#endif
#include <unistd.h>
#include <fnmatch.h>
#include <dirent.h>

#endif

namespace FileSys {
	
enum
{
	ZPATH_MAX = 260
};

#ifndef _WIN32


#else


enum
{
	FA_RDONLY = 1,
	FA_HIDDEN = 2,
	FA_SYSTEM = 4,
	FA_DIREC = 16,
	FA_ARCH = 32,
};

#endif


#ifndef _WIN32

struct findstate_t
{
	std::string path;
	struct dirent** namelist;
	int current;
	int count;
};

static const char* FS_FindName(findstate_t* fileinfo)
{
	return (fileinfo->namelist[fileinfo->current]->d_name);
}

enum
{
	FA_RDONLY = 1,
	FA_HIDDEN = 2,
	FA_SYSTEM = 4,
	FA_DIREC = 8,
	FA_ARCH = 16,
};


static const char *pattern;

static int matchfile(const struct dirent *ent)
{
	return fnmatch(pattern, ent->d_name, FNM_NOESCAPE) == 0;
}

static void *FS_FindFirst(const char *const filespec, findstate_t *const fileinfo)
{
	const char* dir;

	const char *const slash = strrchr(filespec, '/');

	if (slash)
	{
		pattern = slash + 1;
		fileinfo->path = std::string(filespec, 0, slash - filespec + 1);
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

static int FS_FindNext(void *const handle, findstate_t *const fileinfo)
{
	findstate_t *const state = static_cast<findstate_t *>(handle);

	if (state->current < fileinfo->count)
	{
		return ++state->current < fileinfo->count ? 0 : -1;
	}

	return -1;
}

static int FS_FindClose(void *const handle)
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

static int FS_FindAttr(findstate_t *const fileinfo)
{
	dirent *const ent = fileinfo->namelist[fileinfo->current];
	const std::string path = fileinfo->path + ent->d_name;
	bool isdir;

	if (FS_DirEntryExists(path.c_str(), &isdir))
	{
		return isdir ? FA_DIREC : 0;
	}

	return 0;
}

std::string FS_FullPath(const char* directory)
{
	char fullpath[PATH_MAX];

	if (realpath(directory, fullpath) != nullptr)
		return fullpath;

	return directory;
}

static size_t FS_GetFileSize(findstate_t* handle, const char* pathname)
{
	struct stat info;
	bool res = stat(pathname, &info) == 0;
	if (!res || (info.st_mode & S_IFDIR)) return 0;
	return (size_t)info.st_size;
}


#else

#include <windows.h>
#include <direct.h>

std::wstring toWide(const char* str)
{
	int len = (int)strlen(str);
	std::wstring wide;
	if (len > 0)
	{
		int size_needed = MultiByteToWideChar(CP_UTF8, 0, str, len, nullptr, 0);
		wide.resize(size_needed);
		MultiByteToWideChar(CP_UTF8, 0, str, len, &wide.front(), size_needed);
	}
	return wide;
}

static std::string toUtf8(const wchar_t* str)
{
	auto len = wcslen(str);
	std::string utf8;
	if (len > 0)
	{
		int size_needed = WideCharToMultiByte(CP_UTF8, 0, str, (int)len, nullptr, 0, nullptr, nullptr);
		utf8.resize(size_needed);
		WideCharToMultiByte(CP_UTF8, 0, str, (int)len, &utf8.front(), size_needed, nullptr, nullptr);
	}
	return utf8;
}

struct findstate_t
{
	struct FileTime
	{
		uint32_t lo, hi;
	};
	// Mirror WIN32_FIND_DATAW in <winbase.h>. We cannot pull in the Windows header here as it would pollute all consumers' symbol space.
	struct WinData
	{
		uint32_t Attribs;
		FileTime Times[3];
		uint32_t Size[2];
		uint32_t Reserved[2];
		wchar_t Name[ZPATH_MAX];
		wchar_t AltName[14];
	};
	WinData FindData;
	std::string UTF8Name;
};


static int FS_FindAttr(findstate_t* fileinfo)
{
	return fileinfo->FindData.Attribs;
}

//==========================================================================
//
// FS_FindFirst
//
// Start a pattern matching sequence.
//
//==========================================================================


static void *FS_FindFirst(const char *filespec, findstate_t *fileinfo)
{
	static_assert(sizeof(WIN32_FIND_DATAW) == sizeof(fileinfo->FindData), "FindData size mismatch");
	return FindFirstFileW(toWide(filespec).c_str(), (LPWIN32_FIND_DATAW)&fileinfo->FindData);
}

//==========================================================================
//
// FS_FindNext
//
// Return the next file in a pattern matching sequence.
//
//==========================================================================

static int FS_FindNext(void *handle, findstate_t *fileinfo)
{
	return FindNextFileW((HANDLE)handle, (LPWIN32_FIND_DATAW)&fileinfo->FindData) == 0;
}

//==========================================================================
//
// FS_FindClose
//
// Finish a pattern matching sequence.
//
//==========================================================================

static int FS_FindClose(void *handle)
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

static const char *FS_FindName(findstate_t *fileinfo)
{
	fileinfo->UTF8Name = toUtf8(fileinfo->FindData.Name);
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

static size_t FS_GetFileSize(findstate_t* handle, const char* pathname)
{
	return handle->FindData.Size[1] + ((uint64_t)handle->FindData.Size[0] << 32);
}

#endif


//==========================================================================
//
// ScanDirectory
//
//==========================================================================

static bool DoScanDirectory(FileList& list, const char* dirpath, const char* match, const char* relpath, bool nosubdir, bool readhidden)
{
	findstate_t find;

	std::string dirpathn = dirpath;
	FixPathSeparator(&dirpathn.front());
	if (dirpathn[dirpathn.length() - 1] != '/') dirpathn += '/';

	std::string dirmatch = dirpathn;
	dirmatch += match;

	auto handle = FS_FindFirst(dirmatch.c_str(), &find);
	if (handle == ((void*)(-1))) return false;
	FileListEntry fl;
	do
	{
		auto attr = FS_FindAttr(&find);
		if (!readhidden && (attr & FA_HIDDEN))
		{
			// Skip hidden files and directories. (Prevents SVN/git bookkeeping info from being included.)
			continue;
		}
		auto fn = FS_FindName(&find);

		if (attr & FA_DIREC)
		{
			if (fn[0] == '.' &&
				(fn[1] == '\0' ||
					(fn[1] == '.' && fn[2] == '\0')))
			{
				// Do not record . and .. directories.
				continue;
			}
		}

		fl.FileName = fn;
		fl.FilePath = dirpathn + fn;
		fl.FilePathRel = relpath;
		if (fl.FilePathRel.length() > 0) fl.FilePathRel += '/';
		fl.FilePathRel += fn;
		fl.isDirectory = !!(attr & FA_DIREC);
		fl.isReadonly = !!(attr & FA_RDONLY);
		fl.isHidden = !!(attr & FA_HIDDEN);
		fl.isSystem = !!(attr & FA_SYSTEM);
		fl.Length = FS_GetFileSize(&find, fl.FilePath.c_str());
		list.push_back(fl);
		if (!nosubdir && (attr & FA_DIREC))
		{
			DoScanDirectory(list, fl.FilePath.c_str(), match, fl.FilePathRel.c_str(), false, readhidden);
			fl.Length = 0;
		}
	} while (FS_FindNext(handle, &find) == 0);
	FS_FindClose(handle);
	return true;
}


bool ScanDirectory(std::vector<FileListEntry>& list, const char* dirpath, const char* match, bool nosubdir, bool readhidden)
{
	return DoScanDirectory(list, dirpath, match, "", nosubdir, readhidden);
}

//==========================================================================
//
// DirEntryExists
//
// Returns true if the given path exists, be it a directory or a file.
//
//==========================================================================

bool FS_DirEntryExists(const char* pathname, bool* isdir)
{
	if (isdir) *isdir = false;
	if (pathname == NULL || *pathname == 0)
		return false;

#ifndef _WIN32
	struct stat info;
	bool res = stat(pathname, &info) == 0;
#else
	// Windows must use the wide version of stat to preserve non-standard paths.
	auto wstr = toWide(pathname);
	struct _stat64 info;
	bool res = _wstat64(wstr.c_str(), &info) == 0;
#endif
	if (isdir) *isdir = !!(info.st_mode & S_IFDIR);
	return res;
}

}
