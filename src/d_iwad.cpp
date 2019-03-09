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
#include "m_argv.h"
#include "m_misc.h"
#include "sc_man.h"
#include "v_video.h"
#include "gameconfigfile.h"
#include "resourcefiles/resourcefile.h"
#include "version.h"
#include "doomerrors.h"
#include "v_text.h"

CVAR (Bool, queryiwad, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG);
CVAR (String, defaultiwad, "", CVAR_ARCHIVE|CVAR_GLOBALCONFIG);

//==========================================================================
//
// Parses IWAD definitions
//
//==========================================================================

void FIWadManager::ParseIWadInfo(const char *fn, const char *data, int datasize, FIWADInfo *result)
{
	FScanner sc;
	int numblocks = 0;

	sc.OpenMem("IWADINFO", data, datasize);
	while (sc.GetString())
	{
		if (sc.Compare("IWAD"))
		{
			numblocks++;
			if (result && numblocks > 1)
			{
				sc.ScriptMessage("Multiple IWAD records ignored");
				// Skip the rest.
				break;
			}
				
			FIWADInfo *iwad = result ? result : &mIWadInfos[mIWadInfos.Reserve(1)];
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
				else if (sc.Compare("IWadname"))
				{
					sc.MustGetStringName("=");
					sc.MustGetString();
					iwad->IWadname = sc.String;
					if (sc.CheckString(","))
					{
						sc.MustGetNumber();
						iwad->prio = sc.Number;
					}
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
						else if (sc.Compare("NoBigFont")) iwad->flags |= GI_IGNOREBIGFONTLUMP;
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
					iwad->FgColor = V_GetColor(NULL, sc);
					sc.MustGetStringName(",");
					sc.MustGetString();
					iwad->BkColor = V_GetColor(NULL, sc);
				}
				else if (sc.Compare("IgnoreTitlePatches"))
				{
					sc.MustGetStringName("=");
					sc.MustGetNumber();
					if (sc.Number) iwad->flags |= GI_IGNORETITLEPATCHES;
					else iwad->flags &= ~GI_IGNORETITLEPATCHES;
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
				else if (sc.Compare("StartupType"))
				{
					sc.MustGetStringName("=");
					sc.MustGetString();
					FString sttype = sc.String;
					if (!sttype.CompareNoCase("DOOM"))
						iwad->StartupType = FStartupInfo::DoomStartup;
					else if (!sttype.CompareNoCase("HERETIC"))
						iwad->StartupType = FStartupInfo::HereticStartup;
					else if (!sttype.CompareNoCase("HEXEN"))
						iwad->StartupType = FStartupInfo::HexenStartup;
					else if (!sttype.CompareNoCase("STRIFE"))
						iwad->StartupType = FStartupInfo::StrifeStartup;
					else iwad->StartupType = FStartupInfo::DefaultStartup;
				}
				else if (sc.Compare("StartupSong"))
				{
					sc.MustGetStringName("=");
					sc.MustGetString();
					iwad->Song = sc.String;
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
		else if (result == nullptr && sc.Compare("NAMES"))
		{
			sc.MustGetStringName("{");
			mIWadNames.Push(FString());
			while (!sc.CheckString("}"))
			{
				sc.MustGetString();
				FString wadname = sc.String;
				mIWadNames.Push(wadname);
			}
		}
		else if (result == nullptr && sc.Compare("ORDER"))
		{
			sc.MustGetStringName("{");
			while (!sc.CheckString("}"))
			{
				sc.MustGetString();
				mOrderNames.Push(sc.String);
			}
		}
		else
		{
			sc.ScriptError("Unknown keyword '%s'", sc.String);
		}
	}
}

//==========================================================================
//
// Look for IWAD definition lump
//
//==========================================================================

FIWadManager::FIWadManager(const char *fn)
{
	FResourceFile *resfile = FResourceFile::OpenResourceFile(fn, true);
	if (resfile != NULL)
	{
		uint32_t cnt = resfile->LumpCount();
		for(int i=cnt-1; i>=0; i--)
		{
			FResourceLump *lmp = resfile->GetLump(i);

			if (lmp->Namespace == ns_global && !stricmp(lmp->Name, "IWADINFO"))
			{
				// Found one!
				ParseIWadInfo(resfile->FileName, (const char*)lmp->CacheLump(), lmp->LumpSize);
				break;
			}
		}
		delete resfile;
	}
	if (mIWadNames.Size() == 0 || mIWadInfos.Size() == 0)
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
	FResourceFile *iwadfile = FResourceFile::OpenResourceFile(iwad, true);

	mLumpsFound.Resize(mIWadInfos.Size());

	auto CheckLumpName = [=](const char *name)
	{
		for (unsigned i = 0; i< mIWadInfos.Size(); i++)
		{
			for (unsigned j = 0; j < mIWadInfos[i].Lumps.Size(); j++)
			{
				if (!mIWadInfos[i].Lumps[j].CompareNoCase(name))
				{
					mLumpsFound[i] |= (1 << j);
				}
			}
		}
	};

	if (iwadfile != NULL)
	{
		memset(&mLumpsFound[0], 0, mLumpsFound.Size() * sizeof(mLumpsFound[0]));
		for(uint32_t ii = 0; ii < iwadfile->LumpCount(); ii++)
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
	for (unsigned i = 0; i< mIWadInfos.Size(); i++)
	{
		if (mLumpsFound[i] == (1 << mIWadInfos[i].Lumps.Size()) - 1)
		{
			DPrintf(DMSG_NOTIFY, "Identified %s as %s\n", iwad, mIWadInfos[i].Name.GetChars());
			return i;
		}
	}
	return -1;
}

//==========================================================================
//
// Look for IWAD definition lump
//
//==========================================================================

int FIWadManager::CheckIWADInfo(const char *fn)
{
	FResourceFile *resfile = FResourceFile::OpenResourceFile(fn, true);
	if (resfile != NULL)
	{
		uint32_t cnt = resfile->LumpCount();
		for (int i = cnt - 1; i >= 0; i--)
		{
			FResourceLump *lmp = resfile->GetLump(i);

			if (lmp->Namespace == ns_global && !stricmp(lmp->Name, "IWADINFO"))
			{
				// Found one!
				try
				{
					FIWADInfo result;
					ParseIWadInfo(resfile->FileName, (const char*)lmp->CacheLump(), lmp->LumpSize, &result);
					delete resfile;

					for (unsigned i = 0, count = mIWadInfos.Size(); i < count; ++i)
					{
						if (mIWadInfos[i].Name == result.Name)
						{
							return i;
						}
					}

					mOrderNames.Push(result.Name);
					return mIWadInfos.Push(result);
				}
				catch (CRecoverableError &err)
				{
					delete resfile;
					Printf(TEXTCOLOR_RED "%s: %s\nFile has been removed from the list of IWADs\n", fn, err.GetMessage());
					return -1;
				}
				break;
			}
		}
		delete resfile;
		Printf(TEXTCOLOR_RED "%s: Unable to find IWADINFO\nFile has been removed from the list of IWADs\n", fn);
		return -1;
	}
	Printf(TEXTCOLOR_RED "%s: Unable to open as resource file.\nFile has been removed from the list of IWADs\n", fn);
	return -1;
}

//==========================================================================
//
// CollectSearchPaths
//
// collect all paths in a local array for easier management
//
//==========================================================================

void FIWadManager::CollectSearchPaths()
{
	if (GameConfig->SetSection("IWADSearch.Directories"))
	{
		const char *key;
		const char *value;

		while (GameConfig->NextInSection(key, value))
		{
			if (stricmp(key, "Path") == 0)
			{
				FString nice = NicePath(value);
				if (nice.Len() > 0) mSearchPaths.Push(nice);
			}
		}
	}
	mSearchPaths.Append(I_GetGogPaths());
	mSearchPaths.Append(I_GetSteamPath());

	// Unify and remove trailing slashes
	for (auto &str : mSearchPaths)
	{
		FixPathSeperator(str);
		if (str.Back() == '/') str.Truncate(str.Len() - 1);
	}
}

//==========================================================================
//
// AddIWADCandidates
//
// scans the given directory and adds all potential IWAD candidates
//
//==========================================================================

void FIWadManager::AddIWADCandidates(const char *dir)
{
	void *handle;
	findstate_t findstate;
	FStringf slasheddir("%s/", dir);
	FString findmask = slasheddir + "*.*";
	if ((handle = I_FindFirst(findmask, &findstate)) != (void *)-1)
	{
		do
		{
			if (!(I_FindAttr(&findstate) & FA_DIREC))
			{
				auto FindName = I_FindName(&findstate);
				auto p = strrchr(FindName, '.');
				if (p != nullptr)
				{
					// special IWAD extension.
					if (!stricmp(p, ".iwad") || !stricmp(p, ".ipk3") || !stricmp(p, ".ipk7"))
					{
						mFoundWads.Push(FFoundWadInfo{ slasheddir + FindName, "", -1 });
					}
				}
				for (auto &name : mIWadNames)
				{
					if (!stricmp(name, FindName))
					{
						mFoundWads.Push(FFoundWadInfo{ slasheddir + FindName, "", -1 });
					}
				}
			}
		} while (I_FindNext(handle, &findstate) == 0);
		I_FindClose(handle);
	}
}

//==========================================================================
//
// ValidateIWADs
//
// validate all found candidates and eliminate the bogus ones.
//
//==========================================================================

void FIWadManager::ValidateIWADs()
{
	TArray<int> returns;
	unsigned originalsize = mIWadInfos.Size();
	for (auto &p : mFoundWads)
	{
		int index;
		auto x = strrchr(p.mFullPath, '.');
		if (x != nullptr && (!stricmp(x, ".iwad") || !stricmp(x, ".ipk3") || !stricmp(x, ".ipk7")))
		{
			index = CheckIWADInfo(p.mFullPath);
		}
		else
		{
			index = ScanIWAD(p.mFullPath);
		}
		p.mInfoIndex = index;
	}
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

static bool havepicked = false;

int FIWadManager::IdentifyVersion (TArray<FString> &wadfiles, const char *iwad, const char *zdoom_wad, const char *optional_wad)
{
	const char *iwadparm = Args->CheckValue ("-iwad");
	FString custwad;

	CollectSearchPaths();

	// Collect all IWADs in the search path
	for (auto &dir : mSearchPaths)
	{
		AddIWADCandidates(dir);
	}
	unsigned numFoundWads = mFoundWads.Size();

	if (iwadparm)
	{
		const char* const extensions[] = { ".wad", ".pk3", ".iwad", ".ipk3", ".ipk7" };

		for (auto ext : extensions)
		{
			// Check if the given IWAD has an absolute path, in which case the search path will be ignored.
			custwad = iwadparm;
			FixPathSeperator(custwad);
			DefaultExtension(custwad, ext);
			bool isAbsolute = (custwad[0] == '/');
#ifdef _WIN32
			isAbsolute |= (custwad.Len() >= 2 && custwad[1] == ':');
#endif
			if (isAbsolute)
			{
				if (FileExists(custwad)) mFoundWads.Push({ custwad, "", -1 });
			}
			else
			{
				for (auto &dir : mSearchPaths)
				{
					FStringf fullpath("%s/%s", dir.GetChars(), custwad.GetChars());
					if (FileExists(fullpath))
					{
						mFoundWads.Push({ fullpath, "", -1 });
					}
				}
			}

			if (mFoundWads.Size() != numFoundWads)
			{
				// Found IWAD with guessed extension
				break;
			}
		}
	}
	// -iwad not found or not specified. Revert back to standard behavior.
	if (mFoundWads.Size() == numFoundWads) iwadparm = nullptr;

	// Now check if what got collected actually is an IWAD.
	ValidateIWADs();

	// Check for required dependencies.
	for (unsigned i = 0; i < mFoundWads.Size(); i++)
	{
		auto infndx = mFoundWads[i].mInfoIndex;
		if (infndx >= 0)
		{
			auto &wadinfo = mIWadInfos[infndx];
			if (wadinfo.Required.IsNotEmpty())
			{
				bool found = false;
				// needs to be loaded with another IWAD (HexenDK)
				for (unsigned j = 0; j < mFoundWads.Size(); j++)
				{
					auto inf2ndx = mFoundWads[j].mInfoIndex;
					if (inf2ndx >= 0)
					{
						if (!mIWadInfos[infndx].Required.Compare(mIWadInfos[inf2ndx].Name))
						{
							mFoundWads[i].mRequiredPath = mFoundWads[j].mFullPath;
							break;
						}
					}
				}
				// The required dependency was not found. Skip this IWAD.
				if (mFoundWads[i].mRequiredPath.IsEmpty()) mFoundWads[i].mInfoIndex = -1;
			}
		}
	}
	TArray<FFoundWadInfo> picks;
	if (numFoundWads < mFoundWads.Size())
	{
		// We have a -iwad parameter. Pick the first usable IWAD we found through that.
		for (unsigned i = numFoundWads; i < mFoundWads.Size(); i++)
		{
			if (mFoundWads[i].mInfoIndex >= 0)
			{
				picks.Push(mFoundWads[i]);
				break;
			}
		}
	}
	else if (iwad != nullptr && *iwad != 0)
	{
		int pickedprio = -1;
		// scan the list of found IWADs for a matching one for the current PWAD.
		for (auto &found : mFoundWads)
		{
			if (found.mInfoIndex >= 0 && mIWadInfos[found.mInfoIndex].IWadname.CompareNoCase(iwad) == 0 && mIWadInfos[found.mInfoIndex].prio > pickedprio)
			{
				picks.Clear();
				picks.Push(found);
				pickedprio = mIWadInfos[found.mInfoIndex].prio;
			}
		}
	}
	if (picks.Size() == 0)
	{
		// Now sort what we found and discard all duplicates.
		for (unsigned i = 0; i < mOrderNames.Size(); i++)
		{
			bool picked = false;
			for (int j = 0; j < (int)mFoundWads.Size(); j++)
			{
				if (mFoundWads[j].mInfoIndex >= 0)
				{
					if (mIWadInfos[mFoundWads[j].mInfoIndex].Name.Compare(mOrderNames[i]) == 0)
					{
						if (!picked)
						{
							picked = true;
							picks.Push(mFoundWads[j]);
						}
						mFoundWads.Delete(j--);
					}
				}
			}
		}
		// What's left is IWADs with their own IWADINFO. Copy those in order of discovery.
		for (auto &entry : mFoundWads)
		{
			if (entry.mInfoIndex >= 0) picks.Push(entry);
		}
	}

	// If we still haven't found a suitable IWAD let's error out.
	if (picks.Size() == 0)
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
	int pick = 0;

	// We got more than one so present the IWAD selection box.
	if (picks.Size() > 1)
	{
		// Locate the user's prefered IWAD, if it was found.
		if (defaultiwad[0] != '\0')
		{
			for (unsigned i = 0; i < picks.Size(); ++i)
			{
				FString &basename = mIWadInfos[picks[i].mInfoIndex].Name;
				if (stricmp(basename, defaultiwad) == 0)
				{
					pick = i;
					break;
				}
			}
		}
		if (picks.Size() > 1)
		{
			if (!havepicked)
			{
				TArray<WadStuff> wads;
				for (auto & found : picks)
				{
					WadStuff stuff;
					stuff.Name = mIWadInfos[found.mInfoIndex].Name;
					stuff.Path = ExtractFileBase(found.mFullPath);
					wads.Push(stuff);
				}
				pick = I_PickIWad(&wads[0], (int)wads.Size(), queryiwad, pick);
				if (pick >= 0)
				{
					// The newly selected IWAD becomes the new default
					defaultiwad = mIWadInfos[picks[pick].mInfoIndex].Name;
				}
				else
				{
					exit(0);
				}
				havepicked = true;
			}
		}
	}

	// zdoom.pk3 must always be the first file loaded and the IWAD second.
	wadfiles.Clear();
	D_AddFile (wadfiles, zdoom_wad);

	// [SP] Load non-free assets if available. This must be done before the IWAD.
	if (D_AddFile(wadfiles, optional_wad))
		Wads.SetIwadNum(2);
	else
		Wads.SetIwadNum(1);

	if (picks[pick].mRequiredPath.IsNotEmpty())
	{
		D_AddFile (wadfiles, picks[pick].mRequiredPath);
	}
	D_AddFile (wadfiles, picks[pick].mFullPath);

	auto info = mIWadInfos[picks[pick].mInfoIndex];
	// Load additional resources from the same directory as the IWAD itself.
	for (unsigned i=0; i < info.Load.Size(); i++)
	{
		long lastslash = picks[pick].mFullPath.LastIndexOf ('/');
		FString path;

		if (lastslash == -1)
		{
			path = "";//  wads[pickwad].Path;
		}
		else
		{
			path = FString (picks[pick].mFullPath.GetChars(), lastslash + 1);
		}
		path += info.Load[i];
		D_AddFile (wadfiles, path);

	}
	return picks[pick].mInfoIndex;
}


//==========================================================================
//
// Find an IWAD to use for this game
//
//==========================================================================

const FIWADInfo *FIWadManager::FindIWAD(TArray<FString> &wadfiles, const char *iwad, const char *basewad, const char *optionalwad)
{
	int iwadType = IdentifyVersion(wadfiles, iwad, basewad, optionalwad);
	//gameiwad = iwadType;
	const FIWADInfo *iwad_info = &mIWadInfos[iwadType];
	if (DoomStartupInfo.Name.IsEmpty()) DoomStartupInfo.Name = iwad_info->Name;
	if (DoomStartupInfo.BkColor == 0 && DoomStartupInfo.FgColor == 0)
	{
		DoomStartupInfo.BkColor = iwad_info->BkColor;
		DoomStartupInfo.FgColor = iwad_info->FgColor;
	}
	if (DoomStartupInfo.Type == 0) DoomStartupInfo.Type = iwad_info->StartupType;
	if (DoomStartupInfo.Song.IsEmpty()) DoomStartupInfo.Song = iwad_info->Song;
	I_SetIWADInfo();
	return iwad_info;
}
