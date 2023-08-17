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
#include "findfile.h"
#include "v_draw.h"

// Save name length limit for old binary formats.
#define OLDSAVESTRINGSIZE		24

//=============================================================================
//
// M_ReadSaveStrings
//
// Find savegames and read their titles
//
//=============================================================================

void FSavegameManager::ReadSaveStrings()
{
	if (SaveGames.Size() == 0)
	{
		void *filefirst;
		findstate_t c_file;
		FString filter;

		LastSaved = LastAccessed = -1;
		quickSaveSlot = nullptr;
		filter = G_BuildSaveName("*");
		filefirst = I_FindFirst(filter.GetChars(), &c_file);
		if (filefirst != ((void *)(-1)))
		{
			do
			{
				// I_FindName only returns the file's name and not its full path
				FString filepath = G_BuildSaveName(I_FindName(&c_file));

				std::unique_ptr<FResourceFile> savegame(FResourceFile::OpenResourceFile(filepath, true));
				if (savegame != nullptr)
				{
					bool oldVer = false;
					bool missing = false;
					FResourceLump *info = savegame->FindLump("info.json");
					if (info == nullptr)
					{
						// savegame info not found. This is not a savegame so leave it alone.
						continue;
					}
					void *data = info->Lock();
					FSerializer arc;
					if (arc.OpenReader((const char *)data, info->LumpSize))
					{
						int savever = 0;
						arc("Save Version", savever);
						FString engine = arc.GetString("Engine");
						FString iwad = arc.GetString("Game WAD");
						FString title = arc.GetString("Title");


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
						else if (iwad.CompareNoCase(fileSystem.GetResourceFileName(fileSystem.GetIwadNum())) == 0)
						{
							missing = !G_CheckSaveGameWads(arc, false);
						}
						else
						{
							// different game. Skip this.
							continue;
						}

						FSaveGameNode *node = new FSaveGameNode;
						node->Filename = filepath;
						node->bOldVersion = oldVer;
						node->bMissingWads = missing;
						node->SaveTitle = title;
						InsertSaveNode(node);
					}

				}
				else // check for old formats.
				{
					FileReader file;
					if (file.OpenFile(filepath))
					{
						PNGHandle *png;
						char sig[16];
						char title[OLDSAVESTRINGSIZE + 1];
						bool oldVer = true;
						bool addIt = false;
						bool missing = false;

						// ZDoom 1.23 betas 21-33 have the savesig first.
						// Earlier versions have the savesig second.
						// Later versions have the savegame encapsulated inside a PNG.
						//
						// Old savegame versions are always added to the menu so
						// the user can easily delete them if desired.

						title[OLDSAVESTRINGSIZE] = 0;

						if (nullptr != (png = M_VerifyPNG(file)))
						{
							char *ver = M_GetPNGText(png, "ZDoom Save Version");
							if (ver != nullptr)
							{
								// An old version
								if (!M_GetPNGText(png, "Title", title, OLDSAVESTRINGSIZE))
								{
									strncpy(title, I_FindName(&c_file), OLDSAVESTRINGSIZE);
								}
								addIt = true;
								delete[] ver;
							}
							delete png;
						}
						else
						{
							file.Seek(0, FileReader::SeekSet);
							if (file.Read(sig, 16) == 16)
							{

								if (strncmp(sig, "ZDOOMSAVE", 9) == 0)
								{
									if (file.Read(title, OLDSAVESTRINGSIZE) == OLDSAVESTRINGSIZE)
									{
										addIt = true;
									}
								}
								else
								{
									memcpy(title, sig, 16);
									if (file.Read(title + 16, OLDSAVESTRINGSIZE - 16) == OLDSAVESTRINGSIZE - 16 &&
										file.Read(sig, 16) == 16 &&
										strncmp(sig, "ZDOOMSAVE", 9) == 0)
									{
										addIt = true;
									}
								}
							}
						}

						if (addIt)
						{
							FSaveGameNode *node = new FSaveGameNode;
							node->Filename = filepath;
							node->bOldVersion = true;
							node->bMissingWads = false;
							node->SaveTitle = title;
							InsertSaveNode(node);
						}
					}
				}
			} while (I_FindNext(filefirst, &c_file) == 0);
			I_FindClose(filefirst);
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
	return G_BuildSaveName(FStringf("%s%02d", prefix, slot));
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

