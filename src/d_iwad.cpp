/*
** d_iwad.cpp
** IWAD detection code
**
**---------------------------------------------------------------------------
** Copyright 1998-2009 Randy Heit
** Copyright 2009 Christoph Oelckers
** Copyright 2017-2025 GZDoom Maintainers and Contributors
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

#include "cmdlib.h"
#include "d_main.h"
#include "engineerrors.h"
#include "filesystem.h"
#include "findfile.h"
#include "fs_findfile.h"
#include "gameconfigfile.h"
#include "gi.h"
#include "gstrings.h"
#include "i_interface.h"
#include "i_system.h"
#include "m_argv.h"
#include "m_misc.h"
#include "sc_man.h"
#include "version.h"

EXTERN_CVAR(Bool, queryiwad);
EXTERN_CVAR(String, queryiwad_key);
EXTERN_CVAR(Bool, disableautoload)
EXTERN_CVAR(Bool, autoloadlights)
EXTERN_CVAR(Bool, autoloadbrightmaps)
EXTERN_CVAR(Bool, autoloadwidescreen)
EXTERN_CVAR(String, language)

CVAR(Bool, i_loadsupportwad, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG) // Disabled in net games.

bool foundprio = false; // global to prevent iwad box from appearing

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
				else if (sc.Compare("SupportWAD"))
				{
					sc.MustGetStringName("=");
					sc.MustGetString();
					iwad->SupportWAD = sc.String;
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
				else if (sc.Compare("NoKeyboardCheats"))
				{
					iwad->nokeyboardcheats = true;
				}
				else if (sc.Compare("SkipBexStringsIfLanguage"))
				{
					iwad->SkipBexStringsIfLanguage = true;
				}
				else if (sc.Compare("Compatibility"))
				{
					sc.MustGetStringName("=");
					do
					{
						sc.MustGetString();
						if(sc.Compare("Poly1")) iwad->flags |= GI_COMPATPOLY1;
						else if(sc.Compare("Poly2")) iwad->flags |= GI_COMPATPOLY2;
						else if(sc.Compare("Shareware")) iwad->flags |= GI_SHAREWARE;
						else if(sc.Compare("Teaser2")) iwad->flags |= GI_TEASER2;
						else if(sc.Compare("Extended")) iwad->flags |= GI_MENUHACK_EXTENDED;
						else if(sc.Compare("Shorttex")) iwad->flags |= GI_COMPATSHORTTEX;
						else if(sc.Compare("Stairs")) iwad->flags |= GI_COMPATSTAIRS;
						else if (sc.Compare("nosectionmerge")) iwad->flags |=  GI_NOSECTIONMERGE;
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
				else if (sc.Compare("DeleteLumps"))
				{
					sc.MustGetStringName("=");
					do
					{
						sc.MustGetString();
						iwad->DeleteLumps.Push(FString(sc.String));
					}
					while (sc.CheckString(","));
				}
				else if (sc.Compare("BannerColors"))
				{
					sc.MustGetStringName("=");
					sc.MustGetString();
					iwad->FgColor = V_GetColor(sc);
					sc.MustGetStringName(",");
					sc.MustGetString();
					iwad->BkColor = V_GetColor(sc);
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
				else if (sc.Compare("LoadLights"))
				{
					sc.MustGetStringName("=");
					sc.MustGetNumber();
					iwad->LoadLights = sc.Number;
				}
				else if (sc.Compare("LoadBrightmaps"))
				{
					sc.MustGetStringName("=");
					sc.MustGetNumber();
					iwad->LoadBrightmaps = sc.Number;
				}
				else if (sc.Compare("LoadWidescreen"))
				{
					sc.MustGetStringName("=");
					sc.MustGetNumber();
					iwad->LoadWidescreen = sc.Number;
				}
				else if (sc.Compare("DiscordAppId"))
				{
					sc.MustGetStringName("=");
					sc.MustGetString();
					iwad->DiscordAppId = sc.String;
				}
				else if (sc.Compare("SteamAppId"))
				{
					sc.MustGetStringName("=");
					sc.MustGetString();
					iwad->SteamAppId = sc.String;
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
void GetReserved(FileSys::LumpFilterInfo& lfi);

FIWadManager::FIWadManager(const char *firstfn, const char *optfn)
{
	FileSystem check;
	std::vector<std::string> fns;
	fns.push_back(firstfn);
	if (optfn) fns.push_back(optfn);
	FileSys::LumpFilterInfo lfi;
	GetReserved(lfi);

	if (check.InitMultipleFiles(fns, &lfi, nullptr))
	{
		// this is for the IWAD picker. As we have a filesystem open here that contains the base files, it is the easiest place to load the strings early.
		GStrings.LoadStrings(check, language);
		int num = check.CheckNumForName("IWADINFO");
		if (num >= 0)
		{
			auto data = check.ReadFile(num);
			ParseIWadInfo("IWADINFO", data.string(), (int)data.size());
		}

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
	FileSystem check;
	check.InitSingleFile(iwad, nullptr);

	mLumpsFound.Resize(mIWadInfos.Size());

	auto CheckFileName = [=](const char *name)
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

	if (check.GetNumEntries() > 0)
	{
		memset(&mLumpsFound[0], 0, mLumpsFound.Size() * sizeof(mLumpsFound[0]));
		for(int ii = 0; ii < check.GetNumEntries(); ii++)
		{

			CheckFileName(check.GetFileShortName(ii));
			auto full = check.GetFileFullName(ii, false);
			if (full && strnicmp(full, "maps/", 5) == 0)
			{
				FString mapname(&full[5], strcspn(&full[5], "."));
				CheckFileName(mapname.GetChars());
			}
		}
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

int FIWadManager::CheckIWADInfo(const char* fn)
{
	FileSystem check;

	FileSys::LumpFilterInfo lfi;
	GetReserved(lfi);

	std::vector<std::string> filenames = { fn };
	if (check.InitMultipleFiles(filenames, &lfi, nullptr))
	{
		int num = check.CheckNumForName("IWADINFO");
		if (num >= 0)
		{
			try
			{

				FIWADInfo result;
				auto data = check.ReadFile(num);
				ParseIWadInfo(fn, data.string(), (int)data.size(), &result);

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
			catch (CRecoverableError & err)
			{
				Printf(TEXTCOLOR_RED "%s: %s\nFile has been removed from the list of IWADs\n", fn, err.what());
				return -1;
			}
		}
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
			else if (stricmp(key, "RecursivePath") == 0)
			{
				FString nice = NicePath(value);
				if (nice.Len() > 0) mRecursiveSearchPaths.Push(nice);
			}
		}
	}
	mSearchPaths.Append(I_GetGogPaths());
	mSearchPaths.Append(I_GetSteamPath());
	mSearchPaths.Append(I_GetBethesdaPath());

	// Unify and remove trailing slashes
	for (auto &str : mSearchPaths)
	{
		FixPathSeperator(str);
		if (str.Back() == '/') str.Truncate(str.Len() - 1);
	}
	for (auto& str : mRecursiveSearchPaths)
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

void FIWadManager::AddIWADCandidates(const char *dir, bool nosubdir)
{
	FileSys::FileList list;

	if (FileSys::ScanDirectory(list, dir, "*", nosubdir))
	{
		for(auto& entry : list)
		{
			if (!entry.isDirectory)
			{
				auto p = strrchr(entry.FileName.c_str(), '.');
				if (p != nullptr)
				{
					// special IWAD extension.
					if (!stricmp(p, ".iwad") || !stricmp(p, ".ipk3") || !stricmp(p, ".ipk7"))
					{
						mFoundWads.Push(FFoundWadInfo{ entry.FilePath.c_str(), "", -1 });
					}
				}
				for (auto &name : mIWadNames)
				{
					if (!name.CompareNoCase(entry.FileName.c_str()))
					{
						mFoundWads.Push(FFoundWadInfo{ entry.FilePath.c_str(), "", -1 });
					}
				}
			}
		}
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

	// Iterating normally will give CheckIWADInfo name conflicts priority to
	// whatever file is found first, rather than the file that the user
	// specifically requests with -iwad, because IdentifyVersion appends
	// the -iwad file to the end of the list. (And it's annoying to change
	// to be the other way around.)
	for (int i = mFoundWads.SSize() - 1; i >= 0; i--)
	{
		auto &p = mFoundWads[i];

		int index;
		auto x = strrchr(p.mFullPath.GetChars(), '.');
		if (x != nullptr && (!stricmp(x, ".iwad") || !stricmp(x, ".ipk3") || !stricmp(x, ".ipk7")))
		{
			index = CheckIWADInfo(p.mFullPath.GetChars());
		}
		else
		{
			index = ScanIWAD(p.mFullPath.GetChars());
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


FString FIWadManager::IWADPathFileSearch(const FString &file)
{
	for(const FString &path : mSearchPaths)
	{
		FString f = path + "/" + file;
		if(FileExists(f)) return f;
	}
	for (const FString& path : mRecursiveSearchPaths)
	{
		FString f = RecursiveFileExists(path, file);
		if (f.IsNotEmpty()) return f;
	}

	return "";
}

int FIWadManager::IdentifyVersion (std::vector<std::string>&wadfiles, const char *iwad, const char *zdoom_wad, const char *optional_wad)
{
	const char *iwadparm = Args->CheckValue ("-iwad");
	FString custwad;

	CollectSearchPaths();

	// Collect all IWADs in the search path
	for (auto &dir : mSearchPaths)
	{
		AddIWADCandidates(dir.GetChars());
	}
	for (auto& dir : mRecursiveSearchPaths)
	{
		AddIWADCandidates(dir.GetChars(), false);
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
				for (const auto& dir : mRecursiveSearchPaths)
				{
					FString fullpath = RecursiveFileExists(dir, custwad);
					if (fullpath.IsNotEmpty())
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

	// Check for symbolic links leading to non-existent files and for files that are unreadable.
	for (unsigned int i = 0; i < mFoundWads.Size(); i++)
	{
		if (!FileExists(mFoundWads[i].mFullPath) || !FileReadable(mFoundWads[i].mFullPath.GetChars())) mFoundWads.Delete(i--);
	}

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
				foundprio = !HoldingQueryKey(queryiwad_key);
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
		const char *gamedir, *cfgfile, *extrasteps = "";

#if defined(_WIN32)
		gamedir = "Documents\\My Games\\" GAMENAME "\\";
		cfgfile = GAMENAMELOWERCASE "-[username].ini"; // I kinda want to grab the actual username here from windows
#elif defined(__APPLE__)
		gamedir = "~/Library/Application Support/" GAMENAMELOWERCASE "/";
		cfgfile = "~/Library/Preferences/" GAMENAMELOWERCASE ".ini";
#elif defined(IS_FLATPAK)
		gamedir = "~/.var/app/" APPID "/.config/" GAMENAMELOWERCASE "/";
		cfgfile = "~/.var/app/" APPID "/.config/" GAMENAMELOWERCASE "/" GAMENAMELOWERCASE ".ini";
		extrasteps = "\n3. Validate your Flatpak permissions, so that Flatpak has access to your directories with wads";
#else
		gamedir = "~/.config/" GAMENAMELOWERCASE "/";
		cfgfile = "~/.config/" GAMENAMELOWERCASE "/" GAMENAMELOWERCASE ".ini";
#endif

		I_FatalError(
			"Cannot find a game IWAD (doom.wad, doom2.wad, heretic.wad, etc.).\n"
			"Did you install " GAMENAME " properly? You can do either of the following:\n"
			"\n"
			"1. Place one or more of these wads in %s\n"
			"2. Edit your %s and add the\n"
			"directories of your iwads to the list beneath [IWADSearch.Directories]"
			"%s\n",
			gamedir, cfgfile, extrasteps
		);
	}
	int pick = 0;

	// Present the IWAD selection box.
	bool alwaysshow = (queryiwad && !Args->CheckParm("-iwad") && !foundprio);

	if (!havepicked && (alwaysshow || picks.Size() > 1))
	{
		TArray<WadStuff> wads;
		for (auto & found : picks)
		{
			WadStuff stuff;
			stuff.Name = mIWadInfos[found.mInfoIndex].Name;
			stuff.Path = ExtractFileBase(found.mFullPath.GetChars());
			wads.Push(stuff);
		}

		int flags = 0;
		if (disableautoload) flags |= 1;
		if (autoloadlights) flags |= 2;
		if (autoloadbrightmaps) flags |= 4;
		if (autoloadwidescreen) flags |= 8;
		if (i_loadsupportwad) flags |= 16;

		FStartupSelectionInfo info = FStartupSelectionInfo(wads, *Args, flags);
		if (I_PickIWad(queryiwad || HoldingQueryKey(queryiwad_key), info))
		{
			pick = info.SaveInfo();
			disableautoload = !!(info.DefaultStartFlags & 1);
			autoloadlights = !!(info.DefaultStartFlags & 2);
			autoloadbrightmaps = !!(info.DefaultStartFlags & 4);
			autoloadwidescreen = !!(info.DefaultStartFlags & 8);
			i_loadsupportwad = !!(info.DefaultStartFlags & 16);
		}
		else
		{
			return -1;
		}
		havepicked = true;
	}

	// zdoom.pk3 must always be the first file loaded and the IWAD second.
	wadfiles.clear();
	D_AddFile (wadfiles, zdoom_wad, true, -1, GameConfig);

	// [SP] Load non-free assets if available. This must be done before the IWAD.
	int iwadnum = 1;
	if (optional_wad && D_AddFile(wadfiles, optional_wad, true, -1, GameConfig, true))
	{
		iwadnum++;
	}

	fileSystem.SetIwadNum(iwadnum);
	if (picks[pick].mRequiredPath.IsNotEmpty())
	{
		D_AddFile (wadfiles, picks[pick].mRequiredPath.GetChars(), true, -1, GameConfig);
		iwadnum++;
	}
	D_AddFile (wadfiles, picks[pick].mFullPath.GetChars(), true, -1, GameConfig);
	fileSystem.SetMaxIwadNum(iwadnum);

	auto info = mIWadInfos[picks[pick].mInfoIndex];

	if(info.SupportWAD.IsNotEmpty())
	{
		// For net games all wads must be explicitly named to make it easier for the host to know
		// exactly what's being loaded.
		if (i_loadsupportwad && !Args->CheckParm("-join") && !Args->CheckParm("-host"))
		{
			FString supportWAD = IWADPathFileSearch(info.SupportWAD);

			if(supportWAD.IsNotEmpty())
			{
				D_AddFile(wadfiles, supportWAD.GetChars(), true, -1, GameConfig, true);
			}
		}
	}

	// Load additional resources from the same directory as the IWAD itself.
	for (unsigned i=0; i < info.Load.Size(); i++)
	{
		FString path;
		if (info.Load[i][0] != ':')
		{
			auto lastslash = picks[pick].mFullPath.LastIndexOf('/');

			if (lastslash == -1)
			{
				path = "";//  wads[pickwad].Path;
			}
			else
			{
				path = FString(picks[pick].mFullPath.GetChars(), lastslash + 1);
			}
			path += info.Load[i];
			D_AddFile(wadfiles, path.GetChars(), true, -1, GameConfig);
		}
		else
		{
			auto wad = BaseFileSearch(info.Load[i].GetChars() + 1, NULL, true, GameConfig);
			if (wad) D_AddFile(wadfiles, wad, true, -1, GameConfig);
		}

	}
	return picks[pick].mInfoIndex;
}


//==========================================================================
//
// Find an IWAD to use for this game
//
//==========================================================================

const FIWADInfo *FIWadManager::FindIWAD(std::vector<std::string>& wadfiles, const char *iwad, const char *basewad, const char *optionalwad)
{
	int iwadType = IdentifyVersion(wadfiles, iwad, basewad, optionalwad);
	if (iwadType == -1) return nullptr;
	//gameiwad = iwadType;
	const FIWADInfo *iwad_info = &mIWadInfos[iwadType];
	if (GameStartupInfo.Name.IsEmpty()) GameStartupInfo.Name = iwad_info->Name;
	if (GameStartupInfo.BkColor == 0 && GameStartupInfo.FgColor == 0)
	{
		GameStartupInfo.BkColor = iwad_info->BkColor;
		GameStartupInfo.FgColor = iwad_info->FgColor;
	}
	if (GameStartupInfo.LoadWidescreen == -1)
		GameStartupInfo.LoadWidescreen = iwad_info->LoadWidescreen;
	if (GameStartupInfo.LoadLights == -1)
		GameStartupInfo.LoadLights = iwad_info->LoadLights;
	if (GameStartupInfo.LoadBrightmaps == -1)
		GameStartupInfo.LoadBrightmaps = iwad_info->LoadBrightmaps;
	if (GameStartupInfo.Type == 0) GameStartupInfo.Type = iwad_info->StartupType;
	if (GameStartupInfo.Song.IsEmpty()) GameStartupInfo.Song = iwad_info->Song;
	if (GameStartupInfo.DiscordAppId.IsEmpty()) GameStartupInfo.DiscordAppId = iwad_info->DiscordAppId;
	if (GameStartupInfo.SteamAppId.IsEmpty()) GameStartupInfo.SteamAppId = iwad_info->SteamAppId;
	I_SetIWADInfo();
	return iwad_info;
}
