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

#include "engineerrors.h"
#include "version.h"
#include "i_sound.h"
#include "stats.h"
#include "v_text.h"
#include "utf8.h"

#include "d_main.h"
#include "d_net.h"
#include "g_game.h"
#include "c_dispatch.h"

#include "gameconfigfile.h"
#include "v_font.h"
#include "g_level.h"
#include "doomstat.h"
#include "bitmap.h"
#include "cmdlib.h"
#include "i_interface.h"


//TODO maybe move this code to a separate cpp file, so that there isn't code duplication between the win32 and posix backends
static void PSR_FindEndBlock(FScanner &sc)
{
	int depth = 1;
	do
	{
		if(sc.CheckToken('}'))
			--depth;
		else if(sc.CheckToken('{'))
			++depth;
		else
			sc.MustGetAnyToken();
	}
	while(depth);
}

static TArray<FString> ParseSteamRegistry(const char* path)
{
	TArray<FString> result;
	FScanner sc;
	if (sc.OpenFile(path))
	{
		sc.SetCMode(true);

		sc.MustGetToken(TK_StringConst);
		sc.MustGetToken('{');
		// Get a list of possible install directories.
		while(sc.GetToken() && sc.TokenType != '}')
		{
			sc.TokenMustBe(TK_StringConst);
			sc.MustGetToken('{');

			while(sc.GetToken() && sc.TokenType != '}')
			{
				sc.TokenMustBe(TK_StringConst);
				FString key(sc.String);
				if(key.CompareNoCase("path") == 0)
				{
					sc.MustGetToken(TK_StringConst);
					result.Push(FString(sc.String) + "/steamapps/common");
					PSR_FindEndBlock(sc);
					break;
				}
				else if(sc.CheckToken('{'))
				{
					PSR_FindEndBlock(sc);
				}
				else
				{
					sc.MustGetToken(TK_StringConst);
				}
			}
		}
	}
	return result;
}

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

	// Look for Doom I Enhanced
	gamepath = gogregistrypath + L"\\2015545325";
	if (QueryPathKey(HKEY_LOCAL_MACHINE, gamepath.c_str(), L"Path", path))
	{
		result.Push(path + "/DOOM_Data/StreamingAssets");	// in a subdirectory
	}

	// Look for Doom II
	gamepath = gogregistrypath + L"\\1435848814";
	if (QueryPathKey(HKEY_LOCAL_MACHINE, gamepath.c_str(), L"Path", path))
	{
		result.Push(path + "/doom2");	// in a subdirectory
		// If direct support for the Master Levels is ever added, they are in path + /master/wads
	}

	// Look for Doom II Enhanced
	gamepath = gogregistrypath + L"\\1426071866";
	if (QueryPathKey(HKEY_LOCAL_MACHINE, gamepath.c_str(), L"Path", path))
	{
		result.Push(path + "/DOOM II_Data/StreamingAssets");	// in a subdirectory
	}

	// Look for Doom + Doom II
	gamepath = gogregistrypath + L"\\1413291984";
	if (QueryPathKey(HKEY_LOCAL_MACHINE, gamepath.c_str(), L"Path", path))
	{
		result.Push(path);	// directly in install folder
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

	// Look for Heretic: SOTSR
	gamepath = gogregistrypath + L"\\1290366318";
	if (QueryPathKey(HKEY_LOCAL_MACHINE, gamepath.c_str(), L"Path", path))
	{
		result.Push(path);	// directly in install folder
	}
	
	// Look for Hexen: Beyond Heretic
	gamepath = gogregistrypath + L"\\1247951670";
	if (QueryPathKey(HKEY_LOCAL_MACHINE, gamepath.c_str(), L"Path", path))
	{
		result.Push(path);	// directly in install folder
	}

	// Look for Hexen: Death Kings
	gamepath = gogregistrypath + L"\\1983497091";
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
		"ultimate doom/base/doom2",                          // 2024 Update
		"ultimate doom/base/tnt",                            // 2024 Update
		"ultimate doom/base/plutonia",                       // 2024 Update
		"DOOM 3 BFG Edition/base/wads",
		"Strife",
		"Ultimate Doom/rerelease/DOOM_Data/StreamingAssets", // 2019 Unity port (previous-re-release branch in Doom + Doom II app)
		"Ultimate Doom/rerelease",                           // 2024 KEX Port
		"Doom 2/rerelease/DOOM II_Data/StreamingAssets",
		"Doom 2/finaldoombase",
        "Master Levels of Doom/doom2"
	};

	FString steamPath;

	if (!QueryPathKey(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath", steamPath))
	{
		if (!QueryPathKey(HKEY_LOCAL_MACHINE, L"Software\\Valve\\Steam", L"InstallPath", steamPath))
			return result;
	}

	try
	{
		TArray<FString> paths = ParseSteamRegistry((steamPath + "/config/libraryfolders.vdf").GetChars());

		for (FString& path : paths)
		{
			path.ReplaceChars('\\', '/');
			path += "/";
		}

		paths.Push(steamPath + "/steamapps/common/");

		for (unsigned int i = 0; i < countof(steam_dirs); ++i)
		{
			for (const FString& path : paths)
			{
				result.Push(path + steam_dirs[i]);
			}
		}
	}
	catch (const CRecoverableError& err)
	{
		// don't abort on errors in here. Just return an empty path.
	}

	return result;
}

//==========================================================================
//
// I_GetBethesdaPath
//
// Check the registry for the path to the Bethesda.net Launcher, so that we
// can search for IWADs that were bought from Bethesda.net.
//
//==========================================================================

TArray<FString> I_GetBethesdaPath()
{
	TArray<FString> result;
	static const char* const bethesda_dirs[] =
	{
		"DOOM_Classic_2019/base",
		"DOOM_Classic_2019/rerelease/DOOM_Data/StreamingAssets",
		"DOOM_II_Classic_2019/base",
		"DOOM_II_Classic_2019/rerelease/DOOM II_Data/StreamingAssets",
		"DOOM 3 BFG Edition/base/wads",
		"Heretic Shadow of the Serpent Riders/base",
		"Hexen/base",
		"Hexen Deathkings of the Dark Citadel/base"

		// Alternate DOS versions of Doom and Doom II (referred to as "Original" in the
		// Bethesda Launcher). While the DOS versions that come with the Unity ports are
		// unaltered, these use WADs from the European PSN versions. These WADs are currently
		// misdetected by GZDoom: DOOM.WAD is detected as the Unity version (which it's not),
		// while DOOM2.WAD is detected as the original DOS release despite having Doom 3: BFG
		// Edition's censored secret level titles (albeit only in the title patches, not in
		// the automap). Unfortunately, these WADs have exactly the same lump names as the WADs
		// they're misdetected as, so it's not currently possible to distinguish them using
		// GZDoom's current IWAD detection system. To prevent them from possibly overriding the
		// real Unity DOOM.WAD and DOS DOOM2.WAD, these paths have been commented out.
		//"Ultimate DOOM/base",
		//"DOOM II/base",

		// Doom Eternal includes DOOM.WAD and DOOM2.WAD, but they're the same misdetected
		// PSN versions used by the alternate DOS releases above.
		//"Doom Eternal/base/classicwads"
	};

#ifdef _WIN64
	const wchar_t *bethesdaregistrypath = L"Software\\Wow6432Node\\Bethesda Softworks\\Bethesda.net";
#else
	// If a 32-bit ZDoom runs on a 64-bit Windows, this will be transparently and
	// automatically redirected to the Wow6432Node address instead, so this address
	// should be safe to use in all cases.
	const wchar_t *bethesdaregistrypath = L"Software\\Bethesda Softworks\\Bethesda.net";
#endif

	FString path;
	if (!QueryPathKey(HKEY_LOCAL_MACHINE, bethesdaregistrypath, L"installLocation", path))
	{
		return result;
	}
	path += "/games/";

	for (unsigned int i = 0; i < countof(bethesda_dirs); ++i)
	{
		result.Push(path + bethesda_dirs[i]);
	}

	return result;
}
