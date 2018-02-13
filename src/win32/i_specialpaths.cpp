/*
** i_specialpaths.cpp
** Gets special system folders where data should be stored. (Windows version)
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

#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <lmcons.h>
#include <shlobj.h>

#include "cmdlib.h"
#include "m_misc.h"
#include "version.h"	// for GAMENAME
#include "i_system.h"

#include "optwin32.h"

// Vanilla MinGW does not have folder ids
#if defined(__MINGW32__) && !defined(__MINGW64_VERSION_MAJOR)
static const GUID FOLDERID_LocalAppData = { 0xf1b32785, 0x6fba, 0x4fcf, 0x9d, 0x55, 0x7b, 0x8e, 0x7f, 0x15, 0x70, 0x91 };
static const GUID FOLDERID_RoamingAppData = { 0x3eb685db, 0x65f9, 0x4cf6, 0xa0, 0x3a, 0xe3, 0xef, 0x65, 0x72, 0x9f, 0x3d };
static const GUID FOLDERID_SavedGames = { 0x4c5c32ff, 0xbb9d, 0x43b0, 0xb5, 0xb4, 0x2d, 0x72, 0xe5, 0x4e, 0xaa, 0xa4 };
static const GUID FOLDERID_Documents = { 0xfdd39ad0, 0x238f, 0x46af, 0xad, 0xb4, 0x6c, 0x85, 0x48, 0x03, 0x69, 0xc7 };
static const GUID FOLDERID_Pictures = { 0x33e28130, 0x4e1e, 0x4676, 0x83, 0x5a, 0x98, 0x39, 0x5c, 0x3b, 0xc3, 0xbb };
#endif

//===========================================================================
//
// IsProgramDirectoryWritable
//
// If the program directory is writable, then dump everything in there for
// historical reasons. Otherwise, known folders get used instead.
//
//===========================================================================

bool UseKnownFolders()
{
	// Cache this value so the semantics don't change during a single run
	// of the program. (e.g. Somebody could add write access while the
	// program is running.)
	static INTBOOL iswritable = -1;
	FString testpath;
	HANDLE file;

	if (iswritable >= 0)
	{
		return !iswritable;
	}
	testpath << progdir << "writest";
	file = CreateFile(testpath, GENERIC_READ | GENERIC_WRITE, 0, NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);
	if (file != INVALID_HANDLE_VALUE)
	{
		CloseHandle(file);
		if (!batchrun) Printf("Using program directory for storage\n");
		iswritable = true;
		return false;
	}
	if (!batchrun) Printf("Using known folders for storage\n");
	iswritable = false;
	return true;
}

//===========================================================================
//
// GetKnownFolder
//
// Returns the known_folder if SHGetKnownFolderPath is available, otherwise
// returns the shell_folder using SHGetFolderPath.
//
//===========================================================================

bool GetKnownFolder(int shell_folder, REFKNOWNFOLDERID known_folder, bool create, FString &path)
{
	using OptWin32::SHGetFolderPathA;
	using OptWin32::SHGetKnownFolderPath;

	char pathstr[MAX_PATH];

	// SHGetKnownFolderPath knows about more folders than SHGetFolderPath, but is
	// new to Vista, hence the reason we support both.
	if (!SHGetKnownFolderPath)
	{
		// NT4 doesn't even have this function.
		if (!SHGetFolderPathA)
			return false;

		if (shell_folder < 0)
		{ // Not supported by SHGetFolderPath
			return false;
		}
		if (create)
		{
			shell_folder |= CSIDL_FLAG_CREATE;
		}
		if (FAILED(SHGetFolderPathA(NULL, shell_folder, NULL, 0, pathstr)))
		{
			return false;
		}
		path = pathstr;
		return true;
	}
	else
	{
		PWSTR wpath;
		if (FAILED(SHGetKnownFolderPath(known_folder, create ? KF_FLAG_CREATE : 0, NULL, &wpath)))
		{
			return false;
		}
		// FIXME: Support Unicode, at least for filenames. This function
		// has no MBCS equivalent, so we have to convert it since we don't
		// support Unicode. :(
		bool converted = false;
		if (WideCharToMultiByte(GetACP(), WC_NO_BEST_FIT_CHARS, wpath, -1,
			pathstr, countof(pathstr), NULL, NULL) > 0)
		{
			path = pathstr;
			converted = true;
		}
		CoTaskMemFree(wpath);
		return converted;
	}
}

//===========================================================================
//
// M_GetAppDataPath													Windows
//
// Returns the path for the AppData folder.
//
//===========================================================================

FString M_GetAppDataPath(bool create)
{
	FString path;

	if (!GetKnownFolder(CSIDL_LOCAL_APPDATA, FOLDERID_LocalAppData, create, path))
	{ // Failed (e.g. On Win9x): use program directory
		path = progdir;
	}
	// Don't use GAME_DIR and such so that ZDoom and its child ports can
	// share the node cache.
	path += "/" GAMENAMELOWERCASE;
	path.Substitute("//", "/");	// needed because progdir ends with a slash.
	if (create)
	{
		CreatePath(path);
	}
	return path;
}

//===========================================================================
//
// M_GetCachePath													Windows
//
// Returns the path for cache GL nodes.
//
//===========================================================================

FString M_GetCachePath(bool create)
{
	FString path;

	if (!GetKnownFolder(CSIDL_LOCAL_APPDATA, FOLDERID_LocalAppData, create, path))
	{ // Failed (e.g. On Win9x): use program directory
		path = progdir;
	}
	// Don't use GAME_DIR and such so that ZDoom and its child ports can
	// share the node cache.
	path += "/zdoom/cache";
	path.Substitute("//", "/");	// needed because progdir ends with a slash.
	if (create)
	{
		CreatePath(path);
	}
	return path;
}

//===========================================================================
//
// M_GetAutoexecPath												Windows
//
// Returns the expected location of autoexec.cfg.
//
//===========================================================================

FString M_GetAutoexecPath()
{
	return "$PROGDIR/autoexec.cfg";
}

//===========================================================================
//
// M_GetCajunPath													Windows
//
// Returns the location of the Cajun Bot definitions.
//
//===========================================================================

FString M_GetCajunPath(const char *botfilename)
{
	FString path;

	path << progdir << "zcajun/" << botfilename;
	if (!FileExists(path))
	{
		path = "";
	}
	return path;
}

//===========================================================================
//
// M_GetConfigPath													Windows
//
// Returns the path to the config file. On Windows, this can vary for reading
// vs writing. i.e. If $PROGDIR/zdoom-<user>.ini does not exist, it will try
// to read from $PROGDIR/zdoom.ini, but it will never write to zdoom.ini.
//
//===========================================================================

FString M_GetConfigPath(bool for_reading)
{
	FString path;
	HRESULT hr;

	path.Format("%s" GAMENAME "_portable.ini", progdir.GetChars());
	if (FileExists(path))
	{
		return path;
	}
	path = "";

	// Construct a user-specific config name
	if (UseKnownFolders() && GetKnownFolder(CSIDL_APPDATA, FOLDERID_RoamingAppData, true, path))
	{
		path += "/" GAME_DIR;
		CreatePath(path);
		path += "/" GAMENAMELOWERCASE ".ini";
	}
	else
	{ // construct "$PROGDIR/zdoom-$USER.ini"
		TCHAR uname[UNLEN+1];
		DWORD unamelen = countof(uname);

		path = progdir;
		hr = GetUserName(uname, &unamelen);
		if (SUCCEEDED(hr) && uname[0] != 0)
		{
			// Is it valid for a user name to have slashes?
			// Check for them and substitute just in case.
			char *probe = uname;
			while (*probe != 0)
			{
				if (*probe == '\\' || *probe == '/')
					*probe = '_';
				++probe;
			}
			path << GAMENAMELOWERCASE "-" << uname << ".ini";
		}
		else
		{ // Couldn't get user name, so just use zdoom.ini
			path += GAMENAMELOWERCASE ".ini";
		}
	}

	// If we are reading the config file, check if it exists. If not, fallback
	// to $PROGDIR/zdoom.ini
	if (for_reading)
	{
		if (!FileExists(path))
		{
			path = progdir;
			path << GAMENAMELOWERCASE ".ini";
		}
	}

	return path;
}

//===========================================================================
//
// M_GetScreenshotsPath												Windows
//
// Returns the path to the default screenshots directory.
//
//===========================================================================

// I'm not sure when FOLDERID_Screenshots was added, but it was probably
// for Windows 8, since it's not in the v7.0 Windows SDK.
static const GUID MyFOLDERID_Screenshots = { 0xb7bede81, 0xdf94, 0x4682, 0xa7, 0xd8, 0x57, 0xa5, 0x26, 0x20, 0xb8, 0x6f };

FString M_GetScreenshotsPath()
{
	FString path;

	if (!UseKnownFolders())
	{
		return progdir;
	}
	else if (GetKnownFolder(-1, MyFOLDERID_Screenshots, true, path))
	{
		path << "/" GAMENAME;
	}
	else if (GetKnownFolder(CSIDL_MYPICTURES, FOLDERID_Pictures, true, path))
	{
		path << "/Screenshots/" GAMENAME;
	}
	else
	{
		return progdir;
	}
	CreatePath(path);
	return path;
}

//===========================================================================
//
// M_GetSavegamesPath												Windows
//
// Returns the path to the default save games directory.
//
//===========================================================================

FString M_GetSavegamesPath()
{
	FString path;

	if (!UseKnownFolders())
	{
		return progdir;
	}
	// Try standard Saved Games folder
	else if (GetKnownFolder(-1, FOLDERID_SavedGames, true, path))
	{
		path << "/" GAMENAME;
	}
	// Try defacto My Documents/My Games folder
	else if (GetKnownFolder(CSIDL_PERSONAL, FOLDERID_Documents, true, path))
	{
		// I assume since this isn't a standard folder, it doesn't have
		// a localized name either.
		path << "/My Games/" GAMENAME;
		CreatePath(path);
	}
	else
	{
		path = progdir;
	}
	return path;
}

//===========================================================================
//
// M_GetDocumentsPath												Windows
//
// Returns the path to the default documents directory.
//
//===========================================================================

FString M_GetDocumentsPath()
{
	FString path;

	// A portable INI means that this storage location should also be portable.
	path.Format("%s" GAMENAME "_portable.ini", progdir.GetChars());
	if (FileExists(path))
	{
		return progdir;
	}

	if (!UseKnownFolders())
	{
		return progdir;
	}
	// Try defacto My Documents/My Games folder
	else if (GetKnownFolder(CSIDL_PERSONAL, FOLDERID_Documents, true, path))
	{
		// I assume since this isn't a standard folder, it doesn't have
		// a localized name either.
		path << "/My Games/" GAMENAME;
		CreatePath(path);
	}
	else
	{
		path = progdir;
	}
	return path;
}
