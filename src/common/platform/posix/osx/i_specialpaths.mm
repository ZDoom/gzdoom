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

#import <Foundation/NSFileManager.h>

#include "cmdlib.h"
#include "version.h"	// for GAMENAME
#include "i_specialpaths.h"

FString M_GetMacAppSupportPath(const bool create);

static FString GetSpecialPath(const NSSearchPathDirectory kind, const BOOL create = YES, const NSSearchPathDomainMask domain = NSUserDomainMask)
{
	NSURL* url = [[NSFileManager defaultManager] URLForDirectory:kind
														inDomain:domain
											   appropriateForURL:nil
														  create:create
														   error:nil];
	char utf8path[PATH_MAX];

	if ([url getFileSystemRepresentation:utf8path
							   maxLength:sizeof utf8path])
	{
		return utf8path;
	}

	return FString();
}

FString M_GetMacAppSupportPath(const bool create)
{
	return GetSpecialPath(NSApplicationSupportDirectory, create);
}

void M_GetMacSearchDirectories(FString& user_docs, FString& user_app_support, FString& local_app_support)
{
	FString path = GetSpecialPath(NSDocumentDirectory);
	user_docs = path.IsEmpty()
		? "~/" GAME_DIR
		: (path + "/" GAME_DIR);

#define LIBRARY_APPSUPPORT "/Library/Application Support/"

	path = M_GetMacAppSupportPath();
	user_app_support = path.IsEmpty()
		? "~" LIBRARY_APPSUPPORT GAME_DIR
		: (path + "/" GAME_DIR);

	path = GetSpecialPath(NSApplicationSupportDirectory, YES, NSLocalDomainMask);
	local_app_support = path.IsEmpty()
		? LIBRARY_APPSUPPORT GAME_DIR
		: (path + "/" GAME_DIR);

#undef LIBRARY_APPSUPPORT
}


//===========================================================================
//
// M_GetAppDataPath													macOS
//
// Returns the path for the AppData folder.
//
//===========================================================================

FString M_GetAppDataPath(bool create)
{
	FString path = M_GetMacAppSupportPath(create);

	if (path.IsEmpty())
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
	FString path = M_GetMacAppSupportPath(create);

	if (path.IsEmpty())
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
	FString path = GetSpecialPath(NSDocumentDirectory);

	if (path.IsNotEmpty())
	{
		path += "/" GAME_DIR "/autoexec.cfg";
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
	FString path = GetSpecialPath(NSLibraryDirectory);

	if (path.IsNotEmpty())
	{
		// There seems to be no way to get Preferences path via NSFileManager
		path += "/Preferences/";
		CreatePath(path);

		if (!DirExists(path))
		{
			path = FString();
		}
	}

	return path + GAMENAMELOWERCASE ".ini";
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
	FString path = GetSpecialPath(NSDocumentDirectory);

	if (path.IsEmpty())
	{
		path = "~/";
	}
	else
	{
		path += "/" GAME_DIR "/Screenshots/";
	}
	CreatePath(path);
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
	FString path = GetSpecialPath(NSDocumentDirectory);

	if (path.IsNotEmpty())
	{
		path += "/" GAME_DIR "/Savegames/";
	}

	return path;
}

//===========================================================================
//
// M_GetDocumentsPath												macOS
//
// Returns the path to the default documents directory.
//
//===========================================================================

FString M_GetDocumentsPath()
{
	FString path = GetSpecialPath(NSDocumentDirectory);

	if (path.IsNotEmpty())
	{
		path += "/" GAME_DIR "/";
	}

	CreatePath(path);
	return path;
}

//===========================================================================
//
// M_GetDemoPath													macOS
//
// Returns the path to the default demo directory.
//
//===========================================================================

FString M_GetDemoPath()
{
	FString path = GetSpecialPath(NSDocumentDirectory);

	if (path.IsNotEmpty())
	{
		path += "/" GAME_DIR "/Demos/";
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
	NSString *str = [NSString stringWithUTF8String:path];
	NSString *out;
	if ([str completePathIntoString:&out caseSensitive:NO matchesIntoArray:nil filterTypes:nil])
	{
		return out.UTF8String;
	}
	return path;
}
