/*
** i_system.cpp
** Timers, pre-console output, IWAD selection, and misc system routines.
**
**---------------------------------------------------------------------------
** Copyright 1998-2009 Randy Heit
** Copyright (C) 2007-2012 Skulltag Development Team
** Copyright (C) 2007-2016 Zandronum Development Team
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
** 4. Redistributions in any form must be accompanied by information on how to
**    obtain complete source code for the software and any accompanying software
**    that uses the software. The source code must either be included in the
**    distribution or be available for no more than the cost of distribution plus
**    a nominal fee, and must be freely redistributable under reasonable
**    conditions. For an executable file, complete source code means the source
**    code for all modules it contains. It does not include source code for
**    modules or files that typically accompany the major components of the
**    operating system on which the executable file runs.
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <process.h>
#include <time.h>
#include <map>

#include <stdarg.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <richedit.h>
#include <wincrypt.h>
#include <shlwapi.h>

#include "printf.h"

#include "version.h"
#include "i_sound.h"
#include "stats.h"
#include "v_text.h"
#include "utf8.h"

#include "d_main.h"
#include "d_net.h"
#include "g_game.h"
#include "c_dispatch.h"
#include "templates.h"
#include "gameconfigfile.h"
#include "v_font.h"
#include "g_level.h"
#include "doomstat.h"
#include "bitmap.h"
#include "cmdlib.h"
#include "i_interface.h"

//==========================================================================
//
// QueryPathKey
//
// Returns the value of a registry key into the output variable value.
//
//==========================================================================

static bool QueryPathKey(HKEY key, const wchar_t *keypath, const wchar_t *valname, FString &value)
{
	HKEY pathkey;
	DWORD pathtype;
	DWORD pathlen;
	LONG res;

	value = "";
	if(ERROR_SUCCESS == RegOpenKeyEx(key, keypath, 0, KEY_QUERY_VALUE, &pathkey))
	{
		if (ERROR_SUCCESS == RegQueryValueEx(pathkey, valname, 0, &pathtype, NULL, &pathlen) &&
			pathtype == REG_SZ && pathlen != 0)
		{
			// Don't include terminating null in count
			TArray<wchar_t> chars(pathlen + 1, true);
			res = RegQueryValueEx(pathkey, valname, 0, NULL, (LPBYTE)chars.Data(), &pathlen);
			if (res == ERROR_SUCCESS) value = FString(chars.Data());
		}
		RegCloseKey(pathkey);
	}
	return value.IsNotEmpty();
}

//==========================================================================
//
// I_GetGogPaths
//
// Check the registry for GOG installation paths, so we can search for IWADs
// that were bought from GOG.com. This is a bit different from the Steam
// version because each game has its own independent installation path, no
// such thing as <steamdir>/SteamApps/common/<GameName>.
//
//==========================================================================

TArray<FString> I_GetGogPaths()
{
	TArray<FString> result;
	FString path;
	std::wstring gamepath;

#ifdef _WIN64
	std::wstring gogregistrypath = L"Software\\Wow6432Node\\GOG.com\\Games";
#else
	// If a 32-bit ZDoom runs on a 64-bit Windows, this will be transparently and
	// automatically redirected to the Wow6432Node address instead, so this address
	// should be safe to use in all cases.
	std::wstring gogregistrypath = L"Software\\GOG.com\\Games";
#endif

	// Look for Ultimate Doom
	gamepath = gogregistrypath + L"\\1435827232";
	if (QueryPathKey(HKEY_LOCAL_MACHINE, gamepath.c_str(), L"Path", path))
	{
		result.Push(path);	// directly in install folder
	}

	// Look for Doom II
	gamepath = gogregistrypath + L"\\1435848814";
	if (QueryPathKey(HKEY_LOCAL_MACHINE, gamepath.c_str(), L"Path", path))
	{
		result.Push(path + "/doom2");	// in a subdirectory
		// If direct support for the Master Levels is ever added, they are in path + /master/wads
	}

	// Look for Final Doom
	gamepath = gogregistrypath + L"\\1435848742";
	if (QueryPathKey(HKEY_LOCAL_MACHINE, gamepath.c_str(), L"Path", path))
	{
		// in subdirectories
		result.Push(path + "/TNT");
		result.Push(path + "/Plutonia");
	}

	// Look for Doom 3: BFG Edition
	gamepath = gogregistrypath + L"\\1135892318";
	if (QueryPathKey(HKEY_LOCAL_MACHINE, gamepath.c_str(), L"Path", path))
	{
		result.Push(path + "/base/wads");	// in a subdirectory
	}

	// Look for Strife: Veteran Edition
	gamepath = gogregistrypath + L"\\1432899949";
	if (QueryPathKey(HKEY_LOCAL_MACHINE, gamepath.c_str(), L"Path", path))
	{
		result.Push(path);	// directly in install folder
	}

	return result;
}

//==========================================================================
//
// I_GetSteamPath
//
// Check the registry for the path to Steam, so that we can search for
// IWADs that were bought with Steam.
//
//==========================================================================

TArray<FString> I_GetSteamPath()
{
	TArray<FString> result;
	static const char *const steam_dirs[] =
	{
		"doom 2/base",
		"final doom/base",
		"heretic shadow of the serpent riders/base",
		"hexen/base",
		"hexen deathkings of the dark citadel/base",
		"ultimate doom/base",
		"DOOM 3 BFG Edition/base/wads",
		"Strife"
	};

	FString path;

	if (!QueryPathKey(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath", path))
	{
		if (!QueryPathKey(HKEY_LOCAL_MACHINE, L"Software\\Valve\\Steam", L"InstallPath", path))
			return result;
	}
	path += "/SteamApps/common/";

	for(unsigned int i = 0; i < countof(steam_dirs); ++i)
	{
		result.Push(path + steam_dirs[i]);
	}

	return result;
}
