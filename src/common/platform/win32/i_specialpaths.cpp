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

#include <windows.h>
#include <lmcons.h>
#include <shlobj.h>
#include <Shlwapi.h>
#include <VersionHelpers.h>

#include "i_specialpaths.h"
#include "printf.h"
#include "cmdlib.h"
#include "findfile.h"
#include "version.h"	// for GAMENAME
#include "gstrings.h"
#include "i_mainwindow.h"
#include "engineerrors.h"


static int isportable = -1;

//===========================================================================
//
// IsProgramDirectoryWritable
//
// If the program directory is writable, then dump everything in there for
// historical reasons. Otherwise, known folders get used instead.
//
//===========================================================================

bool IsPortable()
{
	// Cache this value so the semantics don't change during a single run
	// of the program. (e.g. Somebody could add write access while the
	// program is running.)
	HANDLE file;

	if (isportable >= 0)
	{
		return !!isportable;
	}

	// Consider 'Program Files' read only without actually checking.
	bool found = false;
	for (auto p : { L"ProgramFiles", L"ProgramFiles(x86)" })
	{
		wchar_t buffer1[256];
		if (GetEnvironmentVariable(p, buffer1, 256))
		{
			FString envpath(buffer1);
			FixPathSeperator(envpath);
			if (progdir.MakeLower().IndexOf(envpath.MakeLower()) == 0)
			{
				isportable = false;
				return false;
			}
		}
	}

	// A portable INI means that this storage location should also be portable if the file can be written to.
	FStringf path("%s" GAMENAMELOWERCASE "_portable.ini", progdir.GetChars());
	if (FileExists(path))
	{
		file = CreateFile(path.WideString().c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (file != INVALID_HANDLE_VALUE)
		{
			CloseHandle(file);
			if (!batchrun) Printf("Using portable configuration\n");
			isportable = true;
			return true;
		}
	}

	isportable = false;
	return false;
}

//===========================================================================
//
// GetKnownFolder
//
// Returns the known_folder from SHGetKnownFolderPath
//
//===========================================================================

FString GetKnownFolder(int shell_folder, REFKNOWNFOLDERID known_folder, bool create)
{
	PWSTR wpath;
	if (FAILED(SHGetKnownFolderPath(known_folder, create ? KF_FLAG_CREATE : 0, NULL, &wpath)))
	{
		// This should never be triggered unless the OS was compromised
		I_FatalError("Unable to retrieve known folder.");
	}
	FString path = FString(wpath);
	FixPathSeperator(path);
	CoTaskMemFree(wpath);
	return path;
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
	FString path = GetKnownFolder(CSIDL_LOCAL_APPDATA, FOLDERID_LocalAppData, create);

	path += "/" GAMENAMELOWERCASE;
	if (create)
	{
		CreatePath(path.GetChars());
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
	FString path = GetKnownFolder(CSIDL_LOCAL_APPDATA, FOLDERID_LocalAppData, create);

	// Don't use GAME_DIR and such so that ZDoom and its child ports can
	// share the node cache.
	path += "/zdoom/cache";
	if (create)
	{
		CreatePath(path.GetChars());
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
// M_GetOldConfigPath
//
// Check if we have a config in a place that's no longer used.
// 
//===========================================================================

FString M_GetOldConfigPath(int& type)
{
	FString path;
	HRESULT hr;

	// construct "$PROGDIR/-$USER.ini"
	WCHAR uname[UNLEN + 1];
	DWORD unamelen = UNLEN;

	path = progdir;
	hr = GetUserNameW(uname, &unamelen);
	if (SUCCEEDED(hr) && uname[0] != 0)
	{
		// Is it valid for a user name to have slashes?
		// Check for them and substitute just in case.
		auto probe = uname;
		while (*probe != 0)
		{
			if (*probe == '\\' || *probe == '/')
				*probe = '_';
			++probe;
		}
		path << GAMENAMELOWERCASE "-" << FString(uname) << ".ini";
		type = 0;
		if (FileExists(path))
			return path;
	}

	// Check in app data where this was previously stored.
	// We actually prefer to store the config in a more visible place so this is no longer used.
	path = GetKnownFolder(CSIDL_APPDATA, FOLDERID_RoamingAppData, true);
	path += "/" GAME_DIR "/" GAMENAMELOWERCASE ".ini";
	type = 1;
	if (FileExists(path))
		return path;

	return "";
}

//===========================================================================
//
// M_MigrateOldConfig
//
// Ask the user what to do with their old config.
// 
//===========================================================================

int M_MigrateOldConfig()
{
	int selection = IDCANCEL;
	auto globalstr = L"Move to Users/ folder";
	auto portablestr = L"Convert to portable installation";
	auto cancelstr = L"Cancel";
	auto titlestr = L"Migrate existing configuration";
	auto infostr = L"" GAMENAME " found a user specific config in the game folder";
	const TASKDIALOG_BUTTON buttons[] = { {IDYES, globalstr}, {IDNO, portablestr}, {IDCANCEL, cancelstr} };
	TASKDIALOGCONFIG taskDialogConfig = {};
	taskDialogConfig.cbSize = sizeof(TASKDIALOGCONFIG);
	taskDialogConfig.pszMainIcon = TD_WARNING_ICON;
	taskDialogConfig.pButtons = buttons;
	taskDialogConfig.cButtons = countof(buttons);
	taskDialogConfig.pszWindowTitle = titlestr;
	taskDialogConfig.pszContent = infostr;
	taskDialogConfig.hwndParent = mainwindow.GetHandle();
	taskDialogConfig.dwFlags = TDF_USE_COMMAND_LINKS;
	TaskDialogIndirect(&taskDialogConfig, &selection, NULL, NULL);
	if (selection == IDYES || selection == IDNO) return selection;
	throw CExitEvent(3);
}

//===========================================================================
//
// M_GetConfigPath													Windows
//
// Returns the path to the config file. On Windows, this can vary for reading
// vs writing. i.e. If the user specific ini does not exist, it will try
// to read from a neutral version, but never write to it.
//
//===========================================================================

FString M_GetConfigPath(bool for_reading)
{
	if (IsPortable())
	{
		return FStringf("%s" GAMENAMELOWERCASE "_portable.ini", progdir.GetChars());
	}

	// Construct a user-specific config name
	FString path = GetKnownFolder(CSIDL_APPDATA, FOLDERID_Documents, true);
	path += "/My Games/" GAME_DIR;
	CreatePath(path.GetChars());
	path += "/" GAMENAMELOWERCASE ".ini";
	if (!for_reading || FileExists(path))
		return path;

	// No config was found in the accepted locations. 
	// Look in previously valid places to see if we have something we can migrate

	int type = 0;
	FString oldpath = M_GetOldConfigPath(type);
	if (!oldpath.IsEmpty())
	{
		if (type == 0)
		{
			// If we find a local per-user config, ask the user what to do with it.
			int action = M_MigrateOldConfig();
			if (action == IDNO)
			{
				path.Format("%s" GAMENAMELOWERCASE "_portable.ini", progdir.GetChars());
				isportable = true;
			}
		}
		bool res = MoveFileExW(WideString(oldpath.GetChars()).c_str(), WideString(path.GetChars()).c_str(), MOVEFILE_COPY_ALLOWED);
		if (res) return path;
		else return oldpath;	// if we cannot move, just use the config where it was. It won't be written back, though and never be used again if a new one gets saved.
	}

	// Fall back to the global template if nothing was found.
	// If we are reading the config file, check if it exists. If not, fallback to base version.
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

FString M_GetScreenshotsPath()
{
	FString path;

	if (IsPortable())
	{
		path << progdir << "Screenshots/";
	}
	else if (IsWindows8OrGreater())
	{
		path = GetKnownFolder(-1, FOLDERID_Screenshots, true);

		path << "/" GAMENAME "/";
	}
	else 
	{
		path = GetKnownFolder(CSIDL_MYPICTURES, FOLDERID_Pictures, true);
		path << "/Screenshots/" GAMENAME "/";
	}
	CreatePath(path.GetChars());
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

	if (IsPortable())
	{
		path << progdir << "Save/";
	}
	// Try standard Saved Games folder
	else
	{
		path = GetKnownFolder(-1, FOLDERID_SavedGames, true);
		path << "/" GAMENAME "/";
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

	if (IsPortable())
	{
		return progdir;
	}
	// Try defacto My Documents/My Games folder
	else 
	{
		// I assume since this isn't a standard folder, it doesn't have a localized name either.
		path = GetKnownFolder(CSIDL_PERSONAL, FOLDERID_Documents, true);
		path << "/My Games/" GAMENAME "/";
		CreatePath(path.GetChars());
	}
	return path;
}

//===========================================================================
//
// M_GetDemoPath												Windows
//
// Returns the path to the default demp directory.
//
//===========================================================================

FString M_GetDemoPath()
{
	FString path;

	// A portable INI means that this storage location should also be portable.
	if (IsPortable())
	{
		path << progdir << "Demos/";
	}
	else
	// Try defacto My Documents/My Games folder
	{
		// I assume since this isn't a standard folder, it doesn't have a localized name either.
		path = GetKnownFolder(CSIDL_PERSONAL, FOLDERID_Documents, true);
		path << "/My Games/" GAMENAME "/";
	}

	return path;
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
	std::wstring wpath = WideString(path);
	wchar_t buffer[MAX_PATH];
	GetFullPathNameW(wpath.c_str(), MAX_PATH, buffer, nullptr);
	FString result(buffer);
	FixPathSeperator(result);
	return result;
}
