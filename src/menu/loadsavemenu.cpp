/*
** loadsavemenu.cpp
** The load game and save game menus
**
**---------------------------------------------------------------------------
** Copyright 2001-2010 Randy Heit
** Copyright 2010-2020 Christoph Oelckers
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

#include "doommenu.h"
#include "version.h"
#include "g_game.h"
#include "m_png.h"
#include "filesystem.h"
#include "v_text.h"
#include "gstrings.h"
#include "serializer.h"
#include "vm.h"
#include "i_system.h"
#include "v_video.h"
#include "fs_findfile.h"
#include "v_draw.h"

// Save name length limit for old binary formats.
#define OLDSAVESTRINGSIZE		24

EXTERN_CVAR(Int, save_sort_order)

//=============================================================================
//
// M_ReadSaveStrings
//
// Find savegames and read their titles
//
//=============================================================================

void FSavegameManager::ReadSaveStrings()
{
	// re-read list if forced to sort again
	static int old_save_sort_order = 0;
	if (old_save_sort_order != save_sort_order)
	{
		ClearSaveGames();
		old_save_sort_order = save_sort_order;
	}

	if (SaveGames.Size() == 0)
	{
		FString filter;

		LastSaved = LastAccessed = -1;
		quickSaveSlot = nullptr;
		FileSys::FileList list;
		if (FileSys::ScanDirectory(list, G_GetSavegamesFolder().GetChars(), "*." SAVEGAME_EXT, true))
		{
			for (auto& entry : list)
			{
				std::unique_ptr<FResourceFile> savegame(FResourceFile::OpenResourceFile(entry.FilePath.c_str(), true));
				if (savegame != nullptr)
				{
					bool oldVer = false;
					bool missing = false;
					auto info = savegame->FindEntry("info.json");
					if (info < 0)
					{
						// savegame info not found. This is not a savegame so leave it alone.
						continue;
					}
					auto data = savegame->Read(info);
					FSerializer arc;
					if (arc.OpenReader(data.string(), data.size()))
					{
						int savever = 0;
						arc("Save Version", savever);
						FString engine = arc.GetString("Engine");
						FString iwad = arc.GetString("Game WAD");
						FString title = arc.GetString("Title");
						FString creationtime = arc.GetString("Creation Time");


						if (engine.Compare(GAMESIG) != 0 || savever > SAVEVER)
						{
							// different engine or newer version:
							// not our business. Leave it alone.
							continue;
						}

						if (savever < MINSAVEVER)
						{
							// old, incompatible savegame. List as not usable.
							oldVer = true;
						}
						else if (iwad.CompareNoCase(fileSystem.GetContainerName(fileSystem.GetBaseNum())) == 0)
						{
							missing = !G_CheckSaveGameWads(arc, false);
						}
						else
						{
							// different game. Skip this.
							continue;
						}

						FSaveGameNode *node = new FSaveGameNode;
						node->Filename = entry.FilePath.c_str();
						node->bOldVersion = oldVer;
						node->bMissingWads = missing;
						node->SaveTitle = title;
						node->CreationTime = creationtime;
						InsertSaveNode(node);
					}
				}
			}
		}
	}
}

//=============================================================================
//
// Loads the savegame
//
//=============================================================================

void FSavegameManager::PerformLoadGame(const char *fn, bool flag)
{
	G_LoadGame(fn, flag);
}

//=============================================================================
//
// 
//
//=============================================================================

void FSavegameManager::PerformSaveGame(const char *fn, const char *savegamestring)
{
	G_SaveGame(fn, savegamestring);
}

//=============================================================================
//
//
//
//=============================================================================

FString FSavegameManager::ExtractSaveComment(FSerializer &arc)
{
	FString comment;

	FString time = arc.GetString("Creation Time");
	FString pcomment = arc.GetString("Comment");

	comment = time;
	if (time.Len() > 0) comment += "\n";
	comment += pcomment;
	return comment;
}

FString FSavegameManager::BuildSaveName(const char* prefix, int slot)
{
	return G_BuildSaveName(FStringf("%s%02d", prefix, slot).GetChars());
}

//=============================================================================
//
//
//
//=============================================================================

FSavegameManager savegameManager;

DEFINE_ACTION_FUNCTION(FSavegameManager, GetManager)
{
	PARAM_PROLOGUE;
	ACTION_RETURN_POINTER(&savegameManager);
}

