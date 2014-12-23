/*
** i_steam.cpp
**
**---------------------------------------------------------------------------
** Copyright 2013 Braden Obrzut
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
**
*/

#include <sys/stat.h>

#ifdef __APPLE__
#include <CoreServices/CoreServices.h>
#endif // __APPLE__

#include "doomerrors.h"
#include "d_main.h"
#include "zstring.h"
#include "sc_man.h"

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
static void PSR_SkipBlock(FScanner &sc)
{
	sc.MustGetToken('{');
	PSR_FindEndBlock(sc);
}
static bool PSR_FindAndEnterBlock(FScanner &sc, const char* keyword)
{
	// Finds a block with a given keyword and then enter it (opening brace)
	// Should be closed with PSR_FindEndBlock
	while(sc.GetToken())
	{
		if(sc.TokenType == '}')
		{
			sc.UnGet();
			return false;
		}

		sc.TokenMustBe(TK_StringConst);
		if(!sc.Compare(keyword))
		{
			if(!sc.CheckToken(TK_StringConst))
				PSR_SkipBlock(sc);
		}
		else
		{
			sc.MustGetToken('{');
			return true;
		}
	}
	return false;
}
static TArray<FString> PSR_ReadBaseInstalls(FScanner &sc)
{
	TArray<FString> result;

	// Get a list of possible install directories.
	while(sc.GetToken())
	{
		if(sc.TokenType == '}')
			break;

		sc.TokenMustBe(TK_StringConst);
		FString key(sc.String);
		if(key.Left(18).CompareNoCase("BaseInstallFolder_") == 0)
		{
			sc.MustGetToken(TK_StringConst);
			result.Push(FString(sc.String) + "/steamapps/common");
		}
		else
		{
			if(sc.CheckToken('{'))
				PSR_FindEndBlock(sc);
			else
				sc.MustGetToken(TK_StringConst);
		}
	}

	return result;
}
static TArray<FString> ParseSteamRegistry(const char* path)
{
	TArray<FString> dirs;

	// Read registry data
	FScanner sc;
	sc.OpenFile(path);
	sc.SetCMode(true);

	// Find the SteamApps listing
	if(PSR_FindAndEnterBlock(sc, "InstallConfigStore"))
	{
		if(PSR_FindAndEnterBlock(sc, "Software"))
		{
			if(PSR_FindAndEnterBlock(sc, "Valve"))
			{
				if(PSR_FindAndEnterBlock(sc, "Steam"))
				{
					dirs = PSR_ReadBaseInstalls(sc);
				}
				PSR_FindEndBlock(sc);
			}
			PSR_FindEndBlock(sc);
		}
		PSR_FindEndBlock(sc);
	}

	return dirs;
}

static struct SteamAppInfo
{
	const char* const BasePath;
	const int AppID;
} AppInfo[] =
{
	/*{"doom 2/base", 2300},
	{"final doom/base", 2290},
	{"heretic shadow of the serpent riders/base", 2390},
	{"hexen/base", 2360},
	{"hexen deathkings of the dark citadel/base", 2370},
	{"ultimate doom/base", 2280},
	{"DOOM 3 BFG Edition/base/wads", 208200},*/
	{"Strife", 317040}
};

TArray<FString> I_GetSteamPath()
{
	TArray<FString> result;
	TArray<FString> SteamInstallFolders;

	// Linux and OS X actually allow the user to install to any location, so
	// we need to figure out on an app-by-app basis where the game is installed.
	// To do so, we read the virtual registry.
#ifdef __APPLE__
	FString appSupportPath;

	{
		char cpath[PATH_MAX];
		FSRef folder;

		if (noErr == FSFindFolder(kUserDomain, kApplicationSupportFolderType, kCreateFolder, &folder) &&
			noErr == FSRefMakePath(&folder, (UInt8*)cpath, PATH_MAX))
		{
			appSupportPath = cpath;
		}
	}

	FString regPath = appSupportPath + "/Steam/config/config.vdf";
	try
	{
		SteamInstallFolders = ParseSteamRegistry(regPath);
	}
	catch(class CDoomError &error)
	{
		// If we can't parse for some reason just pretend we can't find anything.
		return result;
	}

	SteamInstallFolders.Push(appSupportPath + "/Steam/SteamApps/common");
#else
	char* home = getenv("HOME");
	if(home != NULL && *home != '\0')
	{
		FString regPath;
		regPath.Format("%s/.local/share/Steam/config/config.vdf", home);
		try
		{
			SteamInstallFolders = ParseSteamRegistry(regPath);
		}
		catch(class CDoomError &error)
		{
			// If we can't parse for some reason just pretend we can't find anything.
			return result;
		}

		regPath.Format("%s/.local/share/Steam/SteamApps/common", home);
		SteamInstallFolders.Push(regPath);
	}
#endif

	for(unsigned int i = 0;i < SteamInstallFolders.Size();++i)
	{
		for(unsigned int app = 0;app < countof(AppInfo);++app)
		{
			struct stat st;
			FString candidate(SteamInstallFolders[i] + "/" + AppInfo[app].BasePath);
			if(stat(candidate, &st) == 0 && S_ISDIR(st.st_mode))
				result.Push(candidate);
		}
	}

	return result;
}
