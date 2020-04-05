//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1999-2016 Randy Heit
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------
//

#include <fnmatch.h>

#ifdef __APPLE__
#include <AvailabilityMacros.h>
#endif // __APPLE__

#include "cmdlib.h"
#include "d_protocol.h"
#include "i_system.h"
#include "gameconfigfile.h"
#include "x86.h"


void I_Tactile(int /*on*/, int /*off*/, int /*total*/)
{
}

static ticcmd_t emptycmd;

ticcmd_t *I_BaseTiccmd()
{
	return &emptycmd;
}

//
// I_Init
//
void I_Init()
{
	extern void CalculateCPUSpeed();

	CheckCPUID(&CPU);
	CalculateCPUSpeed();
	DumpCPUInfo(&CPU);
}


bool I_WriteIniFailed()
{
	printf("The config file %s could not be saved:\n%s\n", GameConfig->GetPathName(), strerror(errno));
	return false; // return true to retry
}


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


TArray<FString> I_GetGogPaths()
{
	// GOG's Doom games are Windows only at the moment
	return TArray<FString>();
}
