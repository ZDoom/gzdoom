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


#ifndef _WIN32

#include <fnmatch.h>

static const char *pattern;

#if defined(__APPLE__) && MAC_OS_X_VERSION_MAX_ALLOWED < 1080
static int matchfile(struct dirent *ent)
#else
static int matchfile(const struct dirent *ent)
#endif
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