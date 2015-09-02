/*
** d_iwad.cpp
** IWAD detection code
**
**---------------------------------------------------------------------------
** Copyright 1998-2009 Randy Heit
** Copyright 2009 CHristoph Oelckers
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
#include "d_main.h"
#include "gi.h"
#include "cmdlib.h"
#include "doomstat.h"
#include "i_system.h"
#include "w_wad.h"
#include "w_zip.h"
#include "v_palette.h"
#include "m_argv.h"
#include "m_misc.h"
#include "c_cvars.h"
#include "sc_man.h"
#include "v_video.h"
#include "gameconfigfile.h"
#include "resourcefiles/resourcefile.h"
#include "version.h"


CVAR (Bool, queryiwad, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG);
CVAR (String, defaultiwad, "", CVAR_ARCHIVE|CVAR_GLOBALCONFIG);

//==========================================================================
//
// Clear check list
//
//==========================================================================

void FIWadManager::ClearChecks()
{
	mLumpsFound.Resize(mIWads.Size());
	for(unsigned i=0;i<mLumpsFound.Size(); i++)
	{
		mLumpsFound[i] = 0;
	}
}

//==========================================================================
//
// Check one lump
//
//==========================================================================

void FIWadManager::CheckLumpName(const char *name)
{
	for(unsigned i=0; i< mIWads.Size(); i++)
	{
		for(unsigned j=0; j < mIWads[i].Lumps.Size(); j++)
		{
			if (!mIWads[i].Lumps[j].CompareNoCase(name))
			{
				mLumpsFound[i] |= (1<<j);
			}
		}
	}
}

//==========================================================================
//
// Returns check result
//
//==========================================================================

int FIWadManager::GetIWadInfo()
{
	for(unsigned i=0; i< mIWads.Size(); i++)
	{
		if (mLumpsFound[i] == (1 << mIWads[i].Lumps.Size()) - 1)
		{
			return i;
		}
	}
	return -1;
}

//==========================================================================
//
// Parses IWAD definitions
//
//==========================================================================

void FIWadManager::ParseIWadInfo(const char *fn, const char *data, int datasize)
{
	FScanner sc;

	sc.OpenMem("IWADINFO", data, datasize);
	while (sc.GetString())
	{
		if (sc.Compare("IWAD"))
		{
			FIWADInfo *iwad = &mIWads[mIWads.Reserve(1)];
			sc.MustGetStringName("{");
			while (!sc.CheckString("}"))
			{
				sc.MustGetString();
				if (sc.Compare("Name"))
				{
					sc.MustGetStringName("=");
					sc.MustGetString();
					iwad->Name = sc.String;
				}
				else if (sc.Compare("Autoname"))
				{
					sc.MustGetStringName("=");
					sc.MustGetString();
					iwad->Autoname = sc.String;
				}
				else if (sc.Compare("Config"))
				{
					sc.MustGetStringName("=");
					sc.MustGetString();
					iwad->Configname = sc.String;
				}
				else if (sc.Compare("Game"))
				{
					sc.MustGetStringName("=");
					sc.MustGetString();
					if (sc.Compare("Doom")) iwad->gametype = GAME_Doom;
					else if (sc.Compare("Heretic")) iwad->gametype = GAME_Heretic;
					else if (sc.Compare("Hexen")) iwad->gametype = GAME_Hexen;
					else if (sc.Compare("Strife")) iwad->gametype = GAME_Strife;
					else if (sc.Compare("Chex")) iwad->gametype = GAME_Chex;
					else sc.ScriptError(NULL);
				}
				else if (sc.Compare("Mapinfo"))
				{
					sc.MustGetStringName("=");
					sc.MustGetString();
					iwad->MapInfo = sc.String;
				}
				else if (sc.Compare("Compatibility"))
				{
					sc.MustGetStringName("=");
					do
					{
						sc.MustGetString();
						if(sc.Compare("NoTextcolor")) iwad->flags |= GI_NOTEXTCOLOR;
						else if(sc.Compare("Poly1")) iwad->flags |= GI_COMPATPOLY1;
						else if(sc.Compare("Poly2")) iwad->flags |= GI_COMPATPOLY2;
						else if(sc.Compare("Shareware")) iwad->flags |= GI_SHAREWARE;
						else if(sc.Compare("Teaser2")) iwad->flags |= GI_TEASER2;
						else if(sc.Compare("Extended")) iwad->flags |= GI_MENUHACK_EXTENDED;
						else if(sc.Compare("Shorttex")) iwad->flags |= GI_COMPATSHORTTEX;
						else if(sc.Compare("Stairs")) iwad->flags |= GI_COMPATSTAIRS;
						else sc.ScriptError(NULL);
					}
					while (sc.CheckString(","));
				}
				else if (sc.Compare("MustContain"))
				{
					sc.MustGetStringName("=");
					do
					{
						sc.MustGetString();
						iwad->Lumps.Push(FString(sc.String));
					}
					while (sc.CheckString(","));
				}
				else if (sc.Compare("BannerColors"))
				{
					sc.MustGetStringName("=");
					sc.MustGetString();
					iwad->FgColor = V_GetColor(NULL, sc.String);
					sc.MustGetStringName(",");
					sc.MustGetString();
					iwad->BkColor = V_GetColor(NULL, sc.String);
				}
				else if (sc.Compare("Load"))
				{
					sc.MustGetStringName("=");
					do
					{
						sc.MustGetString();
						iwad->Load.Push(FString(sc.String));
					}
					while (sc.CheckString(","));
				}
				else if (sc.Compare("Required"))
				{
					sc.MustGetStringName("=");
					sc.MustGetString();
					iwad->Required = sc.String;
				}
				else
				{
					sc.ScriptError("Unknown keyword '%s'", sc.String);
				}
			}
			if (iwad->MapInfo.IsEmpty())
			{
				// We must at least load the minimum defaults to allow the engine to run.
				iwad->MapInfo = "mapinfo/mindefaults.txt";
			}
		}
		else if (sc.Compare("NAMES"))
		{
			sc.MustGetStringName("{");
			mIWadNames.Push(FString());
			while (!sc.CheckString("}"))
			{
				sc.MustGetString();
				FString wadname = sc.String;
#if defined(_WIN32) || defined(__APPLE__) // Turns out Mac OS X is case insensitive.
				mIWadNames.Push(wadname);
#else
				// check for lowercase, uppercased first letter and full uppercase on Linux etc.
				wadname.ToLower();
				mIWadNames.Push(wadname);
				wadname.LockBuffer()[0] = toupper(wadname[0]);
				wadname.UnlockBuffer();
				mIWadNames.Push(wadname);
				wadname.ToUpper();
				mIWadNames.Push(wadname);
#endif
			}
		}
	}
}

//==========================================================================
//
// Look for IWAD definition lump
//
//==========================================================================

void FIWadManager::ParseIWadInfos(const char *fn)
{
	FResourceFile *resfile = FResourceFile::OpenResourceFile(fn, NULL, true);
	if (resfile != NULL)
	{
		DWORD cnt = resfile->LumpCount();
		for(int i=cnt-1; i>=0; i--)
		{
			FResourceLump *lmp = resfile->GetLump(i);

			if (lmp->Namespace == ns_global && !stricmp(lmp->Name, "IWADINFO"))
			{
				// Found one!
				ParseIWadInfo(resfile->Filename, (const char*)lmp->CacheLump(), lmp->LumpSize);
				break;
			}
		}
		delete resfile;
	}
	if (mIWadNames.Size() == 0 || mIWads.Size() == 0)
	{
		I_FatalError("No IWAD definitions found");
	}
}


//==========================================================================
//
// ScanIWAD
//
// Scan the contents of an IWAD to determine which one it is
//==========================================================================

int FIWadManager::ScanIWAD (const char *iwad)
{
	FResourceFile *iwadfile = FResourceFile::OpenResourceFile(iwad, NULL, true);

	if (iwadfile != NULL)
	{
		ClearChecks();
		for(DWORD ii = 0; ii < iwadfile->LumpCount(); ii++)
		{
			FResourceLump *lump = iwadfile->GetLump(ii);

			CheckLumpName(lump->Name);
			if (lump->FullName.IsNotEmpty())
			{
				if (strnicmp(lump->FullName, "maps/", 5) == 0)
				{
					FString mapname(&lump->FullName[5], strcspn(&lump->FullName[5], "."));
					CheckLumpName(mapname);
				}
			}
		}
		delete iwadfile;
	}
	return GetIWadInfo();
}

//==========================================================================
//
// CheckIWAD
//
// Tries to find an IWAD from a set of known IWAD names, and checks the
// contents of each one found to determine which game it belongs to.
// Returns the number of new wads found in this pass (does not count wads
// found from a previous call).
// 
//==========================================================================

int FIWadManager::CheckIWAD (const char *doomwaddir, WadStuff *wads)
{
	const char *slash;
	int numfound;

	numfound = 0;

	slash = (doomwaddir[0] && doomwaddir[strlen (doomwaddir)-1] != '/') ? "/" : "";

	// Search for a pre-defined IWAD
	for (unsigned i=0; i< mIWadNames.Size(); i++)
	{
		if (mIWadNames[i].IsNotEmpty() && wads[i].Path.IsEmpty())
		{
			FString iwad;
			
			iwad.Format ("%s%s%s", doomwaddir, slash, mIWadNames[i].GetChars());
			FixPathSeperator (iwad);
			if (FileExists (iwad))
			{
				wads[i].Type = ScanIWAD (iwad);
				if (wads[i].Type != -1)
				{
					wads[i].Path = iwad;
					wads[i].Name = mIWads[wads[i].Type].Name;
					numfound++;
				}
			}
		}
	}

	return numfound;
}

//==========================================================================
//
// IdentifyVersion
//
// Tries to find an IWAD in one of four directories under DOS or Win32:
//	  1. Current directory
//	  2. Executable directory
//	  3. $DOOMWADDIR
//	  4. $HOME
//
// Under UNIX OSes, the search path is:
//	  1. Current directory
//	  2. $DOOMWADDIR
//	  3. $HOME/.config/zdoom
//	  4. The share directory defined at compile time (/usr/local/share/zdoom)
//
// The search path can be altered by editing the IWADSearch.Directories
// section of the config file.
//
//==========================================================================

int FIWadManager::IdentifyVersion (TArray<FString> &wadfiles, const char *iwad, const char *zdoom_wad)
{
	TArray<WadStuff> wads;
	TArray<size_t> foundwads;
	const char *iwadparm = Args->CheckValue ("-iwad");
	size_t numwads;
	int pickwad;
	size_t i;
	bool iwadparmfound = false;
	FString custwad;

	wads.Resize(mIWadNames.Size());
	foundwads.Resize(mIWads.Size());
	memset(&foundwads[0], 0, foundwads.Size() * sizeof(foundwads[0]));

	if (iwadparm == NULL && iwad != NULL && *iwad != 0)
	{
		iwadparm = iwad;
	}

	if (iwadparm)
	{
		custwad = iwadparm;
		FixPathSeperator (custwad);
		if (CheckIWAD (custwad, &wads[0]))
		{ // -iwad parameter was a directory
			iwadparm = NULL;
		}
		else
		{
			DefaultExtension (custwad, ".wad");
			iwadparm = custwad;
			mIWadNames[0] = custwad;
			CheckIWAD ("", &wads[0]);
		}
	}

	if (iwadparm == NULL || wads[0].Path.IsEmpty() || mIWads[wads[0].Type].Required.IsNotEmpty())
	{
		if (GameConfig->SetSection ("IWADSearch.Directories"))
		{
			const char *key;
			const char *value;

			while (GameConfig->NextInSection (key, value))
			{
				if (stricmp (key, "Path") == 0)
				{
					FString nice = NicePath(value);
					FixPathSeperator(nice);
					CheckIWAD(nice, &wads[0]);
				}
			}
		}
		TArray<FString> gog_paths = I_GetGogPaths();
		for (i = 0; i < gog_paths.Size(); ++i)
		{
			CheckIWAD (gog_paths[i], &wads[0]);
		}
		TArray<FString> steam_path = I_GetSteamPath();
		for (i = 0; i < steam_path.Size(); ++i)
		{
			CheckIWAD (steam_path[i], &wads[0]);
		}
	}

	if (iwadparm != NULL && !wads[0].Path.IsEmpty())
	{
		iwadparmfound = true;
	}

	for (i = numwads = 0; i < mIWadNames.Size(); i++)
	{
		if (!wads[i].Path.IsEmpty())
		{
			if (i != numwads)
			{
				wads[numwads] = wads[i];
			}
			foundwads[wads[numwads].Type] = numwads + 1;
			numwads++;
		}
	}

	for (unsigned i=0; i<mIWads.Size(); i++)
	{
		if (mIWads[i].Required.IsNotEmpty() && foundwads[i])
		{
			bool found = false;
			// needs to be loaded with another IWAD (HexenDK)
			for (unsigned j=0; j<mIWads.Size(); j++)
			{
				if (!mIWads[i].Required.Compare(mIWads[j].Name))
				{
					if (foundwads[j])
					{
						found = true;
						mIWads[i].preload = j;
					}
					break;
				}
			}
			// The required WAD is not there so this one can't be used and must be deleted from the list
			if (!found)
			{
				size_t kill = foundwads[i];
				for (size_t j = kill; j < numwads; ++j)
				{
					wads[j - 1] = wads[j];
				}
				numwads--;
				foundwads[i] = 0;
				for (unsigned j = 0; j < foundwads.Size(); ++j)
				{
					if (foundwads[j] > kill)
					{
						foundwads[j]--;
					}
				}

			}
		}
	}

	if (numwads == 0)
	{
		I_FatalError ("Cannot find a game IWAD (doom.wad, doom2.wad, heretic.wad, etc.).\n"
					  "Did you install " GAMENAME " properly? You can do either of the following:\n"
					  "\n"
#if defined(_WIN32)
					  "1. Place one or more of these wads in the same directory as " GAMENAME ".\n"
					  "2. Edit your " GAMENAMELOWERCASE "-username.ini and add the directories of your iwads\n"
					  "to the list beneath [IWADSearch.Directories]");
#elif defined(__APPLE__)
					  "1. Place one or more of these wads in ~/Library/Application Support/" GAMENAMELOWERCASE "/\n"
					  "2. Edit your ~/Library/Preferences/" GAMENAMELOWERCASE ".ini and add the directories\n"
					  "of your iwads to the list beneath [IWADSearch.Directories]");
#else
					  "1. Place one or more of these wads in ~/.config/" GAMENAMELOWERCASE "/.\n"
					  "2. Edit your ~/.config/" GAMENAMELOWERCASE "/" GAMENAMELOWERCASE ".ini and add the directories of your\n"
					  "iwads to the list beneath [IWADSearch.Directories]");
#endif
	}

	pickwad = 0;

	if (!iwadparmfound && numwads > 1)
	{
		int defiwad = 0;

		// Locate the user's prefered IWAD, if it was found.
		if (defaultiwad[0] != '\0')
		{
			for (i = 0; i < numwads; ++i)
			{
				FString basename = ExtractFileBase (wads[i].Path);
				if (stricmp (basename, defaultiwad) == 0)
				{
					defiwad = (int)i;
					break;
				}
			}
		}
		pickwad = I_PickIWad (&wads[0], (int)numwads, queryiwad, defiwad);
		if (pickwad >= 0)
		{
			// The newly selected IWAD becomes the new default
			FString basename = ExtractFileBase (wads[pickwad].Path);
			defaultiwad = basename;
		}
	}

	if (pickwad < 0)
		exit (0);

	// zdoom.pk3 must always be the first file loaded and the IWAD second.
	wadfiles.Clear();
	D_AddFile (wadfiles, zdoom_wad);

	if (mIWads[wads[pickwad].Type].preload >= 0)
	{
		D_AddFile (wadfiles, wads[foundwads[mIWads[wads[pickwad].Type].preload]-1].Path);
	}
	D_AddFile (wadfiles, wads[pickwad].Path);

	for (unsigned i=0; i < mIWads[wads[pickwad].Type].Load.Size(); i++)
	{
		long lastslash = wads[pickwad].Path.LastIndexOf ('/');
		FString path;

		if (lastslash == -1)
		{
			path = "";//  wads[pickwad].Path;
		}
		else
		{
			path = FString (wads[pickwad].Path.GetChars(), lastslash + 1);
		}
		path += mIWads[wads[pickwad].Type].Load[i];
		D_AddFile (wadfiles, path);

	}
	return wads[pickwad].Type;
}


//==========================================================================
//
// Find an IWAD to use for this game
//
//==========================================================================

const FIWADInfo *FIWadManager::FindIWAD(TArray<FString> &wadfiles, const char *iwad, const char *basewad)
{
	int iwadType = IdentifyVersion(wadfiles, iwad, basewad);
	//gameiwad = iwadType;
	const FIWADInfo *iwad_info = &mIWads[iwadType];
	if (DoomStartupInfo.Name.IsEmpty()) DoomStartupInfo.Name = iwad_info->Name;
	if (DoomStartupInfo.BkColor == 0 && DoomStartupInfo.FgColor == 0)
	{
		DoomStartupInfo.BkColor = iwad_info->BkColor;
		DoomStartupInfo.FgColor = iwad_info->FgColor;
	}
	I_SetIWADInfo();
	return iwad_info;
}