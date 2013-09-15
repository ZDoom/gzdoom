#ifdef __APPLE__
#include <CoreServices/CoreServices.h>
#endif

#ifdef _WIN32
#include <windows.h>
#include <lmcons.h>
#include <shlobj.h>
#define USE_WINDOWS_DWORD
#endif

#include "cmdlib.h"
#include "m_misc.h"

#if defined(_WIN32)

//===========================================================================
//
// M_GetCachePath													Windows
//
// Returns the path for cache GL nodes.
//
//===========================================================================

FString M_GetCachePath()
{
	FString path;

	char pathstr[MAX_PATH];
	if (0 != SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, pathstr))
	{ // Failed (e.g. On Win9x): use program directory
		path = progdir;
	}
	else
	{
		path = pathstr;
	}
	// Don't use GAME_DIR and such so that ZDoom and its child ports can
	// share the node cache.
	path += "/zdoom/cache";
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

	TCHAR uname[UNLEN+1];
	DWORD unamelen = countof(uname);

	// Because people complained, try for a user-specific .ini in the program directory first.
	// If that is not writeable, use the one in the home directory instead.
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

		path = progdir;
		path += "zdoom-";
		path += uname;
		path += ".ini";
		if (for_reading)
		{
			if (!FileExists(path.GetChars()))
			{
				path = "";
			}
		}
		else
		{ // check if writeable
			FILE *checker = fopen (path.GetChars(), "a");
			if (checker == NULL)
			{
				path = "";
			}
			else
			{
				fclose (checker);
			}
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
	path = progdir;
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
	path = progdir;
	return path;
}

#elif defined(__APPLE__)

//===========================================================================
//
// M_GetCachePath													Mac OS X
//
// Returns the path for cache GL nodes.
//
//===========================================================================

FString M_GetCachePath()
{
	FString path;

	char pathstr[PATH_MAX];
	FSRef folder;

	if (noErr == FSFindFolder(kLocalDomain, kApplicationSupportFolderType, kCreateFolder, &folder) &&
		noErr == FSRefMakePath(&folder, (UInt8*)pathstr, PATH_MAX))
	{
		path = pathstr;
	}
	else
	{
		path = progdir;
	}
	path += "/zdoom/cache";
	return path;
}

//===========================================================================
//
// M_GetAutoexecPath												Mac OS X
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
// M_GetCajunPath													Mac OS X
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
// M_GetConfigPath													Mac OS X
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
		path << cpath << "/zdoom.ini";
		return path;
	}
	// Ungh.
	return "zdoom.ini";
}

//===========================================================================
//
// M_GetScreenshotsPath												Mac OS X
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
// M_GetSavegamesPath												Mac OS X
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

#else // Linux, et al.

FString GetUserFile(const char *file)
{
	FString path;
	struct stat info;

	path = NicePath("~/" GAME_DIR "/");

	if (stat(path, &info) == -1)
	{
		struct stat extrainfo;

		// Sanity check for ~/.config
		FString configPath = NicePath("~/.config/");
		if (stat(configPath, &extrainfo) == -1)
		{
			if (mkdir(configPath, S_IRUSR | S_IWUSR | S_IXUSR) == -1)
			{
				I_FatalError("Failed to create ~/.config directory:\n%s", strerror(errno));
			}
		}
		else if (!S_ISDIR(extrainfo.st_mode))
		{
			I_FatalError("~/.config must be a directory");
		}

		// This can be removed after a release or two
		// Transfer the old zdoom directory to the new location
		bool moved = false;
		FString oldpath = NicePath("~/.zdoom/");
		if (stat #if defined(__unix__)
FString GetUserFile (const char *file)
{
	FString path;
	struct stat info;

	path = NicePath("~/" GAME_DIR "/");

	if (stat (path, &info) == -1)
	{
		struct stat extrainfo;

		// Sanity check for ~/.config
		FString configPath = NicePath("~/.config/");
		if (stat (configPath, &extrainfo) == -1)
		{
			if (mkdir (configPath, S_IRUSR | S_IWUSR | S_IXUSR) == -1)
			{
				I_FatalError ("Failed to create ~/.config directory:\n%s", strerror(errno));
			}
		}
		else if (!S_ISDIR(extrainfo.st_mode))
		{
			I_FatalError ("~/.config must be a directory");
		}

		// This can be removed after a release or two
		// Transfer the old zdoom directory to the new location
		bool moved = false;
		FString oldpath = NicePath("~/.zdoom/");
		if (stat (oldpath, &extrainfo) != -1)
		{
			if (rename(oldpath, path) == -1)
			{
				I_Error ("Failed to move old zdoom directory (%s) to new location (%s).",
					oldpath.GetChars(), path.GetChars());
			}
			else
				moved = true;
		}

		if (!moved && mkdir (path, S_IRUSR | S_IWUSR | S_IXUSR) == -1)
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
// M_GetCachePath														Unix
//
// Returns the path for cache GL nodes.
//
//===========================================================================

FString M_GetCachePath()
{
	// Don't use GAME_DIR and such so that ZDoom and its child ports can
	// share the node cache.
	return NicePath("~/.config/zdoom/cache");
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
// M_GetCajunPath														Unix
//
// Returns the location of the Cajun Bot definitions.
//
//===========================================================================

FString M_GetCajunPath(const char *botfilename)
{
	FString path;

	// Check first in ~/.config/zdoom./botfilename.
	path = GetUserFile(BOTFILENAME);
	if (!FileExists(tmp))
	{
		// Then check in SHARE_DIR/botfilename. (Some allowance for Macs
		// should probably be made here.)
		path = SHARE_DIR BOTFILENAME;
		if (!FileExists(path))
		{
			path ="";
		}
	}
	return path;
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
	return GetUserFile("zdoom.ini");
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
	return "~/" GAME_DIR "/screenshots/";
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
	return = "~/" GAME_DIR;
}

#endif
