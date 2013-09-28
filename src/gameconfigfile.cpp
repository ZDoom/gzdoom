/*
** gameconfigfile.cpp
** An .ini parser specifically for zdoom.ini
**
**---------------------------------------------------------------------------
** Copyright 1998-2008 Randy Heit
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
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OFf
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include <stdio.h>
#include <time.h>

#ifdef __APPLE__
#include <CoreServices/CoreServices.h>
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
extern HWND Window;
#define USE_WINDOWS_DWORD
#endif

#include "doomdef.h"
#include "gameconfigfile.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "c_bind.h"
#include "gstrings.h"
#include "m_argv.h"
#include "cmdlib.h"
#include "version.h"
#include "m_misc.h"
#include "v_font.h"
#include "a_pickups.h"
#include "doomstat.h"
#include "i_system.h"
#include "gi.h"

EXTERN_CVAR (Bool, con_centernotify)
EXTERN_CVAR (Int, msg0color)
EXTERN_CVAR (Color, color)
EXTERN_CVAR (Float, dimamount)
EXTERN_CVAR (Int, msgmidcolor)
EXTERN_CVAR (Int, msgmidcolor2)
EXTERN_CVAR (Bool, snd_pitched)
EXTERN_CVAR (Color, am_wallcolor)
EXTERN_CVAR (Color, am_fdwallcolor)
EXTERN_CVAR (Color, am_cdwallcolor)
EXTERN_CVAR (Float, spc_amp)
EXTERN_CVAR (Bool, wi_percents)

FGameConfigFile::FGameConfigFile ()
{
#ifdef __APPLE__
	FString user_docs, user_app_support, local_app_support;
#endif
	FString pathname;

	OkayToWrite = false;	// Do not allow saving of the config before DoGameSetup()
	bMigrating = false;
	bModSetup = false;
	pathname = GetConfigPath (true);
	ChangePathName (pathname);
	LoadConfigFile (MigrateStub, NULL);

	if (!HaveSections ())
	{ // Config file not found; try the old one
		MigrateOldConfig ();
	}

	// If zdoom.ini was read from the program directory, switch
	// to the user directory now. If it was read from the user
	// directory, this effectively does nothing.
	pathname = GetConfigPath (false);
	ChangePathName (pathname);

	// Set default IWAD search paths if none present
	if (!SetSection ("IWADSearch.Directories"))
	{
		SetSection ("IWADSearch.Directories", true);
		SetValueForKey ("Path", ".", true);
		SetValueForKey ("Path", "$DOOMWADDIR", true);
#ifdef __APPLE__
		char cpath[PATH_MAX];
		FSRef folder;
		
		if (noErr == FSFindFolder(kUserDomain, kDocumentsFolderType, kCreateFolder, &folder) &&
			noErr == FSRefMakePath(&folder, (UInt8*)cpath, PATH_MAX))
		{
			user_docs << cpath << "/" GAME_DIR;
			SetValueForKey("Path", user_docs, true);
		}
		else
		{
			SetValueForKey("Path", "~/" GAME_DIR, true);
		}
		if (noErr == FSFindFolder(kUserDomain, kApplicationSupportFolderType, kCreateFolder, &folder) &&
			noErr == FSRefMakePath(&folder, (UInt8*)cpath, PATH_MAX))
		{
			user_app_support << cpath << "/" GAME_DIR;
			SetValueForKey("Path", user_app_support, true);
		}
		SetValueForKey ("Path", "$PROGDIR", true);
		if (noErr == FSFindFolder(kLocalDomain, kApplicationSupportFolderType, kCreateFolder, &folder) &&
			noErr == FSRefMakePath(&folder, (UInt8*)cpath, PATH_MAX))
		{
			local_app_support << cpath << "/" GAME_DIR;
			SetValueForKey("Path", local_app_support, true);
		}
#elif !defined(__unix__)
		SetValueForKey ("Path", "$HOME", true);
		SetValueForKey ("Path", "$PROGDIR", true);
#else
		SetValueForKey ("Path", "~/" GAME_DIR, true);
		SetValueForKey ("Path", SHARE_DIR, true);
#endif
	}

	// Set default search paths if none present
	if (!SetSection ("FileSearch.Directories"))
	{
		SetSection ("FileSearch.Directories", true);
#ifdef __APPLE__
		SetValueForKey ("Path", user_docs, true);
		SetValueForKey ("Path", user_app_support, true);
		SetValueForKey ("Path", "$PROGDIR", true);
		SetValueForKey ("Path", local_app_support, true);
#elif !defined(__unix__)
		SetValueForKey ("Path", "$PROGDIR", true);
#else
		SetValueForKey ("Path", "~/" GAME_DIR, true);
		SetValueForKey ("Path", SHARE_DIR, true);
#endif
		SetValueForKey ("Path", "$DOOMWADDIR", true);
	}

	// Create auto-load sections, so users know what's available.
	// Note that this totem pole is the reverse of the order that
	// they will appear in the file.
	CreateSectionAtStart("Harmony.Autoload");
	CreateSectionAtStart("UrbanBrawl.Autoload");
	CreateSectionAtStart("Chex3.Autoload");
	CreateSectionAtStart("Chex.Autoload");
	CreateSectionAtStart("Strife.Autoload");
	CreateSectionAtStart("HexenDemo.Autoload");
	CreateSectionAtStart("HexenDK.Autoload");
	CreateSectionAtStart("Hexen.Autoload");
	CreateSectionAtStart("Heretic.Autoload");
	CreateSectionAtStart("FreeDM.Autoload");
	CreateSectionAtStart("Freedoom1.Autoload");
	CreateSectionAtStart("Freedoom.Autoload");
	CreateSectionAtStart("Plutonia.Autoload");
	CreateSectionAtStart("TNT.Autoload");
	CreateSectionAtStart("Doom2.Autoload");
	CreateSectionAtStart("Doom1.Autoload");
	CreateSectionAtStart("Doom.Autoload");
	CreateSectionAtStart("Global.Autoload");

	// The same goes for auto-exec files.
	CreateStandardAutoExec("Chex.AutoExec", true);
	CreateStandardAutoExec("Strife.AutoExec", true);
	CreateStandardAutoExec("Hexen.AutoExec", true);
	CreateStandardAutoExec("Heretic.AutoExec", true);
	CreateStandardAutoExec("Doom.AutoExec", true);

	// Move search paths back to the top.
	MoveSectionToStart("FileSearch.Directories");
	MoveSectionToStart("IWADSearch.Directories");

	// Add some self-documentation.
	SetSectionNote("IWADSearch.Directories",
		"# These are the directories to automatically search for IWADs.\n"
		"# Each directory should be on a separate line, preceded by Path=\n");
	SetSectionNote("FileSearch.Directories",
		"# These are the directories to search for wads added with the -file\n"
		"# command line parameter, if they cannot be found with the path\n"
		"# as-is. Layout is the same as for IWADSearch.Directories\n");
	SetSectionNote("Doom.AutoExec",
		"# Files to automatically execute when running the corresponding game.\n"
		"# Each file should be on its own line, preceded by Path=\n\n");
	SetSectionNote("Global.Autoload",
		"# WAD files to always load. These are loaded after the IWAD but before\n"
		"# any files added with -file. Place each file on its own line, preceded\n"
		"# by Path=\n");
	SetSectionNote("Doom.Autoload",
		"# Wad files to automatically load depending on the game and IWAD you are\n"
		"# playing.  You may have have files that are loaded for all similar IWADs\n"
		"# (the game) and files that are only loaded for particular IWADs. For example,\n"
		"# any files listed under Doom.Autoload will be loaded for any version of Doom,\n"
		"# but files listed under Doom2.Autoload will only load when you are\n"
		"# playing Doom 2.\n\n");
}

FGameConfigFile::~FGameConfigFile ()
{
}

void FGameConfigFile::WriteCommentHeader (FILE *file) const
{
	fprintf (file, "# This file was generated by " GAMENAME " %s on %s\n", GetVersionString(), myasctime());
}

void FGameConfigFile::MigrateStub (const char *pathname, FConfigFile *config, void *userdata)
{
	static_cast<FGameConfigFile *>(config)->bMigrating = true;
}

void FGameConfigFile::MigrateOldConfig ()
{
	// Set default key bindings. These will be overridden
	// by the bindings in the config file if it exists.
	C_SetDefaultBindings ();
}

void FGameConfigFile::DoGlobalSetup ()
{
	if (SetSection ("GlobalSettings.Unknown"))
	{
		ReadCVars (CVAR_GLOBALCONFIG);
	}
	if (SetSection ("GlobalSettings"))
	{
		ReadCVars (CVAR_GLOBALCONFIG);
	}
	if (SetSection ("LastRun"))
	{
		const char *lastver = GetValueForKey ("Version");
		if (lastver != NULL)
		{
			double last = atof (lastver);
			if (last < 123.1)
			{
				FBaseCVar *noblitter = FindCVar ("vid_noblitter", NULL);
				if (noblitter != NULL)
				{
					noblitter->ResetToDefault ();
				}
			}
			if (last < 202)
			{
				// Make sure the Hexen hotkeys are accessible by default.
				if (SetSection ("Hexen.Bindings"))
				{
					SetValueForKey ("\\", "use ArtiHealth");
					SetValueForKey ("scroll", "+showscores");
					SetValueForKey ("0", "useflechette");
					SetValueForKey ("9", "use ArtiBlastRadius");
					SetValueForKey ("8", "use ArtiTeleport");
					SetValueForKey ("7", "use ArtiTeleportOther");
					SetValueForKey ("6", "use ArtiPork");
					SetValueForKey ("5", "use ArtiInvulnerability2");
				}
			}
			if (last < 204)
			{ // The old default for vsync was true, but with an unlimited framerate
			  // now, false is a better default.
				FBaseCVar *vsync = FindCVar ("vid_vsync", NULL);
				if (vsync != NULL)
				{
					vsync->ResetToDefault ();
				}
			}
			if (last < 206)
			{ // spc_amp is now a float, not an int.
				if (spc_amp > 16)
				{
					spc_amp = spc_amp / 16.f;
				}
			}
			if (last < 207)
			{ // Now that snd_midiprecache works again, you probably don't want it on.
				FBaseCVar *precache = FindCVar ("snd_midiprecache", NULL);
				if (precache != NULL)
				{
					precache->ResetToDefault();
				}
			}
			if (last < 208)
			{ // Weapon sections are no longer used, so tidy up the config by deleting them.
				const char *name;
				size_t namelen;
				bool more;

				more = SetFirstSection();
				while (more)
				{
					name = GetCurrentSection();
					if (name != NULL && 
						(namelen = strlen(name)) > 12 &&
						strcmp(name + namelen - 12, ".WeaponSlots") == 0)
					{
						more = DeleteCurrentSection();
					}
					else
					{
						more = SetNextSection();
					}
				}
			}
			if (last < 209)
			{
				// menu dimming is now a gameinfo option so switch user override off
				FBaseCVar *dim = FindCVar ("dimamount", NULL);
				if (dim != NULL)
				{
					dim->ResetToDefault ();
				}
			}
			if (last < 210)
			{
				if (SetSection ("Hexen.Bindings"))
				{
					// These 2 were misnamed in earlier versions
					SetValueForKey ("6", "use ArtiPork");
					SetValueForKey ("5", "use ArtiInvulnerability2");
				}
			}
		}
	}
}

void FGameConfigFile::DoGameSetup (const char *gamename)
{
	const char *key;
	const char *value;

	if (bMigrating)
	{
		MigrateOldConfig ();
	}
	sublen = countof(section) - 1 - mysnprintf (section, countof(section), "%s.", gamename);
	subsection = section + countof(section) - sublen - 1;
	section[countof(section) - 1] = '\0';
	
	strncpy (subsection, "UnknownConsoleVariables", sublen);
	if (SetSection (section))
	{
		ReadCVars (0);
	}

	strncpy (subsection, "ConsoleVariables", sublen);
	if (SetSection (section))
	{
		ReadCVars (0);
	}

	if (gameinfo.gametype & GAME_Raven)
	{
		SetRavenDefaults (gameinfo.gametype == GAME_Hexen);
	}

	// The NetServerInfo section will be read and override anything loaded
	// here when it's determined that a netgame is being played.
	strncpy (subsection, "LocalServerInfo", sublen);
	if (SetSection (section))
	{
		ReadCVars (0);
	}

	strncpy (subsection, "Player", sublen);
	if (SetSection (section))
	{
		ReadCVars (0);
	}

	if (!bMigrating)
	{
		C_SetDefaultBindings ();
	}

	strncpy (subsection, "Bindings", sublen);
	if (SetSection (section))
	{
		Bindings.UnbindAll();
		while (NextInSection (key, value))
		{
			Bindings.DoBind (key, value);
		}
	}

	strncpy (subsection, "DoubleBindings", sublen);
	if (SetSection (section))
	{
		DoubleBindings.UnbindAll();
		while (NextInSection (key, value))
		{
			DoubleBindings.DoBind (key, value);
		}
	}

	strncpy (subsection, "AutomapBindings", sublen);
	if (SetSection (section))
	{
		AutomapBindings.UnbindAll();
		while (NextInSection (key, value))
		{
			AutomapBindings.DoBind (key, value);
		}
	}

	strncpy (subsection, "ConsoleAliases", sublen);
	if (SetSection (section))
	{
		const char *name = NULL;
		while (NextInSection (key, value))
		{
			if (stricmp (key, "Name") == 0)
			{
				name = value;
			}
			else if (stricmp (key, "Command") == 0 && name != NULL)
			{
				C_SetAlias (name, value);
				name = NULL;
			}
		}
	}
	OkayToWrite = true;
}

// Like DoGameSetup(), but for mod-specific cvars.
// Called after CVARINFO has been parsed.
void FGameConfigFile::DoModSetup(const char *gamename)
{
	mysnprintf(section, countof(section), "%s.Player.Mod", gamename);
	if (SetSection(section))
	{
		ReadCVars(CVAR_MOD|CVAR_USERINFO|CVAR_IGNORE);
	}
	mysnprintf(section, countof(section), "%s.LocalServerInfo.Mod", gamename);
	if (SetSection (section))
	{
		ReadCVars (CVAR_MOD|CVAR_SERVERINFO|CVAR_IGNORE);
	}
	// Signal that these sections should be rewritten when saving the config.
	bModSetup = true;
}

void FGameConfigFile::ReadNetVars ()
{
	strncpy (subsection, "NetServerInfo", sublen);
	if (SetSection (section))
	{
		ReadCVars (0);
	}
	if (bModSetup)
	{
		mysnprintf(subsection, sublen, "NetServerInfo.Mod");
		if (SetSection(section))
		{
			ReadCVars(CVAR_MOD|CVAR_SERVERINFO|CVAR_IGNORE);
		}
	}
}

// Read cvars from a cvar section of the ini. Flags are the flags to give
// to newly-created cvars that were not already defined.
void FGameConfigFile::ReadCVars (DWORD flags)
{
	const char *key, *value;
	FBaseCVar *cvar;
	UCVarValue val;

	flags |= CVAR_ARCHIVE|CVAR_UNSETTABLE|CVAR_AUTO;
	while (NextInSection (key, value))
	{
		cvar = FindCVar (key, NULL);
		if (cvar == NULL)
		{
			cvar = new FStringCVar (key, NULL, flags);
		}
		val.String = const_cast<char *>(value);
		cvar->SetGenericRep (val, CVAR_String);
	}
}

void FGameConfigFile::ArchiveGameData (const char *gamename)
{
	char section[32*3], *subsection;

	sublen = countof(section) - 1 - mysnprintf (section, countof(section), "%s.", gamename);
	subsection = section + countof(section) - 1 - sublen;

	strncpy (subsection, "Player", sublen);
	SetSection (section, true);
	ClearCurrentSection ();
	C_ArchiveCVars (this, CVAR_ARCHIVE|CVAR_USERINFO);

	if (bModSetup)
	{
		strncpy (subsection + 6, ".Mod", sublen - 6);
		SetSection (section, true);
		ClearCurrentSection ();
		C_ArchiveCVars (this, CVAR_MOD|CVAR_ARCHIVE|CVAR_AUTO|CVAR_USERINFO);
	}

	strncpy (subsection, "ConsoleVariables", sublen);
	SetSection (section, true);
	ClearCurrentSection ();
	C_ArchiveCVars (this, CVAR_ARCHIVE);

	// Do not overwrite the serverinfo section if playing a netgame, and
	// this machine was not the initial host.
	if (!netgame || consoleplayer == 0)
	{
		strncpy (subsection, netgame ? "NetServerInfo" : "LocalServerInfo", sublen);
		SetSection (section, true);
		ClearCurrentSection ();
		C_ArchiveCVars (this, CVAR_ARCHIVE|CVAR_SERVERINFO);

		if (bModSetup)
		{
			strncpy (subsection, netgame ? "NetServerInfo.Mod" : "LocalServerInfo.Mod", sublen);
			SetSection (section, true);
			ClearCurrentSection ();
			C_ArchiveCVars (this, CVAR_MOD|CVAR_ARCHIVE|CVAR_AUTO|CVAR_SERVERINFO);
		}
	}

	strncpy (subsection, "UnknownConsoleVariables", sublen);
	SetSection (section, true);
	ClearCurrentSection ();
	C_ArchiveCVars (this, CVAR_ARCHIVE|CVAR_AUTO);

	strncpy (subsection, "ConsoleAliases", sublen);
	SetSection (section, true);
	ClearCurrentSection ();
	C_ArchiveAliases (this);

	M_SaveCustomKeys (this, section, subsection, sublen);

	strcpy (subsection, "Bindings");
	SetSection (section, true);
	Bindings.ArchiveBindings (this);

	strncpy (subsection, "DoubleBindings", sublen);
	SetSection (section, true);
	DoubleBindings.ArchiveBindings (this);

	strncpy (subsection, "AutomapBindings", sublen);
	SetSection (section, true);
	AutomapBindings.ArchiveBindings (this);
}

void FGameConfigFile::ArchiveGlobalData ()
{
	SetSection ("LastRun", true);
	ClearCurrentSection ();
	SetValueForKey ("Version", LASTRUNVERSION);

	SetSection ("GlobalSettings", true);
	ClearCurrentSection ();
	C_ArchiveCVars (this, CVAR_ARCHIVE|CVAR_GLOBALCONFIG);

	SetSection ("GlobalSettings.Unknown", true);
	ClearCurrentSection ();
	C_ArchiveCVars (this, CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_AUTO);
}

FString FGameConfigFile::GetConfigPath (bool tryProg)
{
	const char *pathval;

	pathval = Args->CheckValue ("-config");
	if (pathval != NULL)
	{
		return FString(pathval);
	}
	return M_GetConfigPath(tryProg);
}

void FGameConfigFile::CreateStandardAutoExec(const char *section, bool start)
{
	if (!SetSection(section))
	{
		FString path = M_GetAutoexecPath();
		SetSection (section, true);
		SetValueForKey ("Path", path.GetChars());
	}
	if (start)
	{
		MoveSectionToStart(section);
	}
}

void FGameConfigFile::AddAutoexec (DArgs *list, const char *game)
{
	char section[64];
	const char *key;
	const char *value;

	mysnprintf (section, countof(section), "%s.AutoExec", game);

	if (bMigrating)
	{
		FBaseCVar *autoexec = FindCVar ("autoexec", NULL);

		if (autoexec != NULL)
		{
			UCVarValue val;
			char *path;

			val = autoexec->GetGenericRep (CVAR_String);
			path = copystring (val.String);
			delete autoexec;
			SetSection (section, true);
			SetValueForKey ("Path", path);
			list->AppendArg (path);
			delete[] path;
		}
	}
	else
	{
		// If <game>.AutoExec section does not exist, create it
		// with a default autoexec.cfg file present.
		CreateStandardAutoExec(section, false);
		// Run any files listed in the <game>.AutoExec section
		if (!SectionIsEmpty())
		{
			while (NextInSection (key, value))
			{
				if (stricmp (key, "Path") == 0 && *value != '\0')
				{
					FString expanded_path = ExpandEnvVars(value);
					if (FileExists(expanded_path))
					{
						list->AppendArg (ExpandEnvVars(value));
					}
				}
			}
		}
	}
}

void FGameConfigFile::SetRavenDefaults (bool isHexen)
{
	UCVarValue val;

	if (bMigrating)
	{
		con_centernotify.ResetToDefault ();
		msg0color.ResetToDefault ();
		color.ResetToDefault ();
	}

	val.Bool = false;
	wi_percents.SetGenericRepDefault (val, CVAR_Bool);
	val.Bool = true;
	con_centernotify.SetGenericRepDefault (val, CVAR_Bool);
	snd_pitched.SetGenericRepDefault (val, CVAR_Bool);
	val.Int = 9;
	msg0color.SetGenericRepDefault (val, CVAR_Int);
	val.Int = CR_WHITE;
	msgmidcolor.SetGenericRepDefault (val, CVAR_Int);
	val.Int = CR_YELLOW;
	msgmidcolor2.SetGenericRepDefault (val, CVAR_Int);

	val.Int = 0x543b17;
	am_wallcolor.SetGenericRepDefault (val, CVAR_Int);
	val.Int = 0xd0b085;
	am_fdwallcolor.SetGenericRepDefault (val, CVAR_Int);
	val.Int = 0x734323;
	am_cdwallcolor.SetGenericRepDefault (val, CVAR_Int);

	// Fix the Heretic/Hexen automap colors so they are correct.
	// (They were wrong on older versions.)
	if (*am_wallcolor == 0x2c1808 && *am_fdwallcolor == 0x887058 && *am_cdwallcolor == 0x4c3820)
	{
		am_wallcolor.ResetToDefault ();
		am_fdwallcolor.ResetToDefault ();
		am_cdwallcolor.ResetToDefault ();
	}

	if (!isHexen)
	{
		val.Int = 0x3f6040;
		color.SetGenericRepDefault (val, CVAR_Int);
	}
}

CCMD (whereisini)
{
	FString path = M_GetConfigPath(false);
	Printf ("%s\n", path.GetChars());
}
