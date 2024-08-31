/*
** i_specialpaths.cpp
** Gets special system folders where data should be stored. (Unix version)
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

#include <sys/stat.h>
#include <sys/types.h>
#include "i_system.h"
#include "cmdlib.h"
#include "printf.h"
#include "engineerrors.h"

#include "version.h"	// for GAMENAME


FString GetUserFile (const char *file)
{
	FString path;
	struct stat info;

	path = NicePath("$HOME/" GAME_DIR "/");

	if (stat (path.GetChars(), &info) == -1)
	{
		struct stat extrainfo;

		// Sanity check for $HOME/.config
		FString configPath = NicePath("$HOME/.config/");
		if (stat (configPath.GetChars(), &extrainfo) == -1)
		{
			if (mkdir (configPath.GetChars(), S_IRUSR | S_IWUSR | S_IXUSR) == -1)
			{
				I_FatalError ("Failed to create $HOME/.config directory:\n%s", strerror(errno));
			}
		}
		else if (!S_ISDIR(extrainfo.st_mode))
		{
			I_FatalError ("$HOME/.config must be a directory");
		}

		// This can be removed after a release or two
		// Transfer the old zdoom directory to the new location
		bool moved = false;
		FString oldpath = NicePath("$HOME/." GAMENAMELOWERCASE "/");
		if (stat (oldpath.GetChars(), &extrainfo) != -1)
		{
			if (rename(oldpath.GetChars(), path.GetChars()) == -1)
			{
				I_Error ("Failed to move old " GAMENAMELOWERCASE " directory (%s) to new location (%s).",
					oldpath.GetChars(), path.GetChars());
			}
			else
				moved = true;
		}

		if (!moved && mkdir (path.GetChars(), S_IRUSR | S_IWUSR | S_IXUSR) == -1)
		{
			I_FatalError ("Failed to create %s directory:\n%s",
				path.GetChars(), strerror (errno));
		}
	}
	else
	{
		if (!S_ISDIR(info.st_mode))
		{
			I_FatalError ("%s must be a directory", path.GetChars());
		}
	}
	path += file;
	return path;
}

//===========================================================================
//
// M_GetAppDataPath														Unix
//
// Returns the path for the AppData folder.
//
//===========================================================================

FString M_GetAppDataPath(bool create)
{
	// Don't use GAME_DIR and such so that ZDoom and its child ports can
	// share the node cache.
	FString path = NicePath("$HOME/.config/" GAMENAMELOWERCASE);
	if (create)
	{
		CreatePath(path.GetChars());
	}
	return path;
}

//===========================================================================
//
// M_GetCachePath														Unix
//
// Returns the path for cache GL nodes.
//
//===========================================================================

FString M_GetCachePath(bool create)
{
	// Don't use GAME_DIR and such so that ZDoom and its child ports can
	// share the node cache.
	FString path = NicePath("$HOME/.config/zdoom/cache");
	if (create)
	{
		CreatePath(path.GetChars());
	}
	return path;
}

//===========================================================================
//
// M_GetAutoexecPath													Unix
//
// Returns the expected location of autoexec.cfg.
//
//===========================================================================

FString M_GetAutoexecPath()
{
	return GetUserFile("autoexec.cfg");
}

//===========================================================================
//
// M_GetConfigPath														Unix
//
// Returns the path to the config file. On Windows, this can vary for reading
// vs writing. i.e. If $PROGDIR/zdoom-<user>.ini does not exist, it will try
// to read from $PROGDIR/zdoom.ini, but it will never write to zdoom.ini.
//
//===========================================================================

FString M_GetConfigPath(bool for_reading)
{
	return GetUserFile(GAMENAMELOWERCASE ".ini");
}

//===========================================================================
//
// M_GetScreenshotsPath													Unix
//
// Returns the path to the default screenshots directory.
//
//===========================================================================

FString M_GetScreenshotsPath()
{
	return NicePath("$HOME/" GAME_DIR "/screenshots/");
}

//===========================================================================
//
// M_GetSavegamesPath													Unix
//
// Returns the path to the default save games directory.
//
//===========================================================================

FString M_GetSavegamesPath()
{
	return NicePath("$HOME/" GAME_DIR "/savegames/");
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
	return NicePath("$HOME/" GAME_DIR "/");
}

//===========================================================================
//
// M_GetDemoPath													Unix
//
// Returns the path to the default demo directory.
//
//===========================================================================

FString M_GetDemoPath()
{
	return M_GetDocumentsPath() + "demo/";
}

//===========================================================================
//
// M_NormalizedPath
//
// Normalizes the given path and returns the result.
//
//===========================================================================

FString M_GetNormalizedPath(const char* path)
{
	char *actualpath;
	actualpath = realpath(path, NULL);
	if (!actualpath) // error ?
		return nullptr;
	FString fullpath = actualpath;
	return fullpath;
}

