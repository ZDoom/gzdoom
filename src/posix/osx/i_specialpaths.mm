/*
** i_specialpaths.mm
** Gets special system folders where data should be stored. (macOS version)
**
**---------------------------------------------------------------------------
** Copyright 2013-2016 Randy Heit
** Copyright 2016 Christoph Oelckers
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
*/

#include <CoreServices/CoreServices.h>

#include "cmdlib.h"
#include "m_misc.h"
#include "version.h"	// for GAMENAME

//===========================================================================
//
// M_GetAppDataPath													macOS
//
// Returns the path for the AppData folder.
//
//===========================================================================

FString M_GetAppDataPath(bool create)
{
	FString path;

	char pathstr[PATH_MAX];
	FSRef folder;

	if (noErr == FSFindFolder(kUserDomain, kApplicationSupportFolderType, create ? kCreateFolder : 0, &folder) &&
		noErr == FSRefMakePath(&folder, (UInt8*)pathstr, PATH_MAX))
	{
		path = pathstr;
	}
	else
	{
		path = progdir;
	}
	path += "/" GAMENAMELOWERCASE;
	if (create) CreatePath(path);
	return path;
}

//===========================================================================
//
// M_GetCachePath													macOS
//
// Returns the path for cache GL nodes.
//
//===========================================================================

FString M_GetCachePath(bool create)
{
	FString path;

	char pathstr[PATH_MAX];
	FSRef folder;

	if (noErr == FSFindFolder(kUserDomain, kApplicationSupportFolderType, create ? kCreateFolder : 0, &folder) &&
		noErr == FSRefMakePath(&folder, (UInt8*)pathstr, PATH_MAX))
	{
		path = pathstr;
	}
	else
	{
		path = progdir;
	}
	path += "/zdoom/cache";
	if (create) CreatePath(path);
	return path;
}

//===========================================================================
//
// M_GetAutoexecPath												macOS
//
// Returns the expected location of autoexec.cfg.
//
//===========================================================================

FString M_GetAutoexecPath()
{
	FString path;

	char cpath[PATH_MAX];
	FSRef folder;
	
	if (noErr == FSFindFolder(kUserDomain, kDocumentsFolderType, kCreateFolder, &folder) &&
		noErr == FSRefMakePath(&folder, (UInt8*)cpath, PATH_MAX))
	{
		path << cpath << "/" GAME_DIR "/autoexec.cfg";
	}
	return path;
}

//===========================================================================
//
// M_GetCajunPath													macOS
//
// Returns the location of the Cajun Bot definitions.
//
//===========================================================================

FString M_GetCajunPath(const char *botfilename)
{
	FString path;

	// Just copies the Windows code. Should this be more Mac-specific?
	path << progdir << "zcajun/" << botfilename;
	if (!FileExists(path))
	{
		path = "";
	}
	return path;
}

//===========================================================================
//
// M_GetConfigPath													macOS
//
// Returns the path to the config file. On Windows, this can vary for reading
// vs writing. i.e. If $PROGDIR/zdoom-<user>.ini does not exist, it will try
// to read from $PROGDIR/zdoom.ini, but it will never write to zdoom.ini.
//
//===========================================================================

FString M_GetConfigPath(bool for_reading)
{
	char cpath[PATH_MAX];
	FSRef folder;
	
	if (noErr == FSFindFolder(kUserDomain, kPreferencesFolderType, kCreateFolder, &folder) &&
		noErr == FSRefMakePath(&folder, (UInt8*)cpath, PATH_MAX))
	{
		FString path;
		path << cpath << "/" GAMENAMELOWERCASE ".ini";
		return path;
	}
	// Ungh.
	return GAMENAMELOWERCASE ".ini";
}

//===========================================================================
//
// M_GetScreenshotsPath												macOS
//
// Returns the path to the default screenshots directory.
//
//===========================================================================

FString M_GetScreenshotsPath()
{
	FString path;
	char cpath[PATH_MAX];
	FSRef folder;
	
	if (noErr == FSFindFolder(kUserDomain, kDocumentsFolderType, kCreateFolder, &folder) &&
		noErr == FSRefMakePath(&folder, (UInt8*)cpath, PATH_MAX))
	{
		path << cpath << "/" GAME_DIR "/Screenshots/";
	}
	else
	{
		path = "~/";
	}
	return path;
}

//===========================================================================
//
// M_GetSavegamesPath												macOS
//
// Returns the path to the default save games directory.
//
//===========================================================================

FString M_GetSavegamesPath()
{
	FString path;
	char cpath[PATH_MAX];
	FSRef folder;

	if (noErr == FSFindFolder(kUserDomain, kDocumentsFolderType, kCreateFolder, &folder) &&
		noErr == FSRefMakePath(&folder, (UInt8*)cpath, PATH_MAX))
	{
		path << cpath << "/" GAME_DIR "/Savegames/";
	}
	return path;
}

//===========================================================================
//
// M_GetDocumentsPath												Unix
//
// Returns the path to the default documents directory.
//
//===========================================================================

FString M_GetDocumentsPath()
{
	FString path;
	char cpath[PATH_MAX];
	FSRef folder;

	if (noErr == FSFindFolder(kUserDomain, kDocumentsFolderType, kCreateFolder, &folder) &&
		noErr == FSRefMakePath(&folder, (UInt8*)cpath, PATH_MAX))
	{
		path << cpath << "/" GAME_DIR "/";
	}
	return path;
}
