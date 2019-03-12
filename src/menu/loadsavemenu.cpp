/*
** loadsavemenu.cpp
** The load game and save game menus
**
**---------------------------------------------------------------------------
** Copyright 2001-2010 Randy Heit
** Copyright 2010-2017 Christoph Oelckers
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

#include "menu/menu.h"
#include "version.h"
#include "g_game.h"
#include "m_png.h"
#include "w_wad.h"
#include "v_text.h"
#include "gstrings.h"
#include "serializer.h"
#include "vm.h"
#include "i_system.h"

// Save name length limit for old binary formats.
#define OLDSAVESTRINGSIZE		24

//=============================================================================
//
// Save data maintenance 
//
//=============================================================================

void FSavegameManager::ClearSaveGames()
{
	for (unsigned i = 0; i<SaveGames.Size(); i++)
	{
		if (!SaveGames[i]->bNoDelete)
			delete SaveGames[i];
	}
	SaveGames.Clear();
}

FSavegameManager::~FSavegameManager()
{
	ClearSaveGames();
}

//=============================================================================
//
// Save data maintenance 
//
//=============================================================================

int FSavegameManager::RemoveSaveSlot(int index)
{
	int listindex = SaveGames[0]->bNoDelete ? index - 1 : index;
	if (listindex < 0) return index;

	remove(SaveGames[index]->Filename.GetChars());
	UnloadSaveData();

	FSaveGameNode *file = SaveGames[index];

	if (quickSaveSlot == SaveGames[index])
	{
		quickSaveSlot = nullptr;
	}
	if (!file->bNoDelete) delete file;

	if (LastSaved == listindex) LastSaved = -1;
	else if (LastSaved > listindex) LastSaved--;
	if (LastAccessed == listindex) LastAccessed = -1;
	else if (LastAccessed > listindex) LastAccessed--;

	SaveGames.Delete(index);
	if ((unsigned)index >= SaveGames.Size()) index--;
	ExtractSaveData(index);
	return index;
}

DEFINE_ACTION_FUNCTION(FSavegameManager, RemoveSaveSlot)
{
	PARAM_SELF_STRUCT_PROLOGUE(FSavegameManager);
	PARAM_INT(sel);
	ACTION_RETURN_INT(self->RemoveSaveSlot(sel));
}


//=============================================================================
//
//
//
//=============================================================================

int FSavegameManager::InsertSaveNode(FSaveGameNode *node)
{
	if (SaveGames.Size() == 0)
	{
		return SaveGames.Push(node);
	}

	if (node->bOldVersion)
	{ // Add node at bottom of list
		return SaveGames.Push(node);
	}
	else
	{	// Add node at top of list
		unsigned int i;
		for (i = 0; i < SaveGames.Size(); i++)
		{
			if (SaveGames[i]->bOldVersion || node->SaveTitle.CompareNoCase(SaveGames[i]->SaveTitle) <= 0)
			{
				break;
			}
		}
		SaveGames.Insert(i, node);
		return i;
	}
}

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
		filter = G_BuildSaveName("*." SAVEGAME_EXT, -1);
		filefirst = I_FindFirst(filter.GetChars(), &c_file);
		if (filefirst != ((void *)(-1)))
		{
			do
			{
				// I_FindName only returns the file's name and not its full path
				FString filepath = G_BuildSaveName(I_FindName(&c_file), -1);

				FResourceFile *savegame = FResourceFile::OpenResourceFile(filepath, true, true);
				if (savegame != nullptr)
				{
					bool oldVer = false;
					bool missing = false;
					FResourceLump *info = savegame->FindLump("info.json");
					if (info == nullptr)
					{
						// savegame info not found. This is not a savegame so leave it alone.
						delete savegame;
						continue;
					}
					void *data = info->CacheLump();
					FSerializer arc(nullptr);
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
							delete savegame;
							continue;
						}

						if (savever < MINSAVEVER)
						{
							// old, incompatible savegame. List as not usable.
							oldVer = true;
						}
						else if (iwad.CompareNoCase(Wads.GetWadName(Wads.GetIwadNum())) == 0)
						{
							missing = !G_CheckSaveGameWads(arc, false);
						}
						else
						{
							// different game. Skip this.
							delete savegame;
							continue;
						}

						FSaveGameNode *node = new FSaveGameNode;
						node->Filename = filepath;
						node->bOldVersion = oldVer;
						node->bMissingWads = missing;
						node->SaveTitle = title;
						InsertSaveNode(node);
						delete savegame;
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

DEFINE_ACTION_FUNCTION(FSavegameManager, ReadSaveStrings)
{
	PARAM_SELF_STRUCT_PROLOGUE(FSavegameManager);
	self->ReadSaveStrings();
	return 0;
}


//=============================================================================
//
//
//
//=============================================================================

void FSavegameManager::NotifyNewSave(const FString &file, const FString &title, bool okForQuicksave)
{
	FSaveGameNode *node;

	if (file.IsEmpty())
		return;

	ReadSaveStrings();

	// See if the file is already in our list
	for (unsigned i = 0; i<SaveGames.Size(); i++)
	{
		FSaveGameNode *node = SaveGames[i];
#ifdef __unix__
		if (node->Filename.Compare(file) == 0)
#else
		if (node->Filename.CompareNoCase(file) == 0)
#endif
		{
			node->SaveTitle = title;
			node->bOldVersion = false;
			node->bMissingWads = false;
			if (okForQuicksave)
			{
				if (quickSaveSlot == nullptr) quickSaveSlot = node;
				LastAccessed = LastSaved = i;
			}
			return;
		}
	}

	node = new FSaveGameNode;
	node->SaveTitle = title;
	node->Filename = file;
	node->bOldVersion = false;
	node->bMissingWads = false;
	int index = InsertSaveNode(node);

	if (okForQuicksave)
	{
		if (quickSaveSlot == nullptr) quickSaveSlot = node;
		LastAccessed = LastSaved = index;
	}
}

//=============================================================================
//
// Loads the savegame
//
//=============================================================================

void FSavegameManager::LoadSavegame(int Selected)
{
	G_LoadGame(SaveGames[Selected]->Filename.GetChars(), true);
	if (quickSaveSlot == (FSaveGameNode*)1)
	{
		quickSaveSlot = SaveGames[Selected];
	}
	M_ClearMenus();
	LastAccessed = Selected;
}

DEFINE_ACTION_FUNCTION(FSavegameManager, LoadSavegame)
{
	PARAM_SELF_STRUCT_PROLOGUE(FSavegameManager);
	PARAM_INT(sel);
	self->LoadSavegame(sel);
	return 0;
}

//=============================================================================
//
// 
//
//=============================================================================

void FSavegameManager::DoSave(int Selected, const char *savegamestring)
{
	if (Selected != 0)
	{
		auto node = SaveGames[Selected];
		G_SaveGame(node->Filename.GetChars(), savegamestring);
	}
	else
	{
		// Find an unused filename and save as that
		FString filename;
		int i;

		for (i = 0;; ++i)
		{
			filename = G_BuildSaveName("save", i);
			if (!FileExists(filename))
			{
				break;
			}
		}
		G_SaveGame(filename, savegamestring);
	}
	M_ClearMenus();
}

DEFINE_ACTION_FUNCTION(FSavegameManager, DoSave)
{
	PARAM_SELF_STRUCT_PROLOGUE(FSavegameManager);
	PARAM_INT(sel);
	PARAM_STRING(name);
	self->DoSave(sel, name);
	return 0;
}

//=============================================================================
//
//
//
//=============================================================================

unsigned FSavegameManager::ExtractSaveData(int index)
{
	FResourceFile *resf;
	FSaveGameNode *node;

	if (index == -1)
	{
		if (SaveGames.Size() > 0 && SaveGames[0]->bNoDelete)
		{
			index = LastSaved + 1;
		}
		else
		{
			index = LastAccessed < 0? 0 : LastAccessed;
		}
	}

	UnloadSaveData();

	if ((unsigned)index < SaveGames.Size() &&
		(node = SaveGames[index]) &&
		!node->Filename.IsEmpty() &&
		!node->bOldVersion &&
		(resf = FResourceFile::OpenResourceFile(node->Filename.GetChars(), true)) != nullptr)
	{
		FResourceLump *info = resf->FindLump("info.json");
		if (info == nullptr)
		{
			// this should not happen because the file has already been verified.
			return index;
		}
		void *data = info->CacheLump();
		FSerializer arc(nullptr);
		if (arc.OpenReader((const char *)data, info->LumpSize))
		{
			FString comment;

			FString time = arc.GetString("Creation Time");
			FString pcomment = arc.GetString("Comment");

			comment = time;
			if (time.Len() > 0) comment += "\n";
			comment += pcomment;
			SaveCommentString = comment;

			// Extract pic
			FResourceLump *pic = resf->FindLump("savepic.png");
			if (pic != nullptr)
			{
				FileReader picreader;

				picreader.OpenMemoryArray([=](TArray<uint8_t> &array)
				{
					auto cache = pic->CacheLump();
					array.Resize(pic->LumpSize);
					memcpy(&array[0], cache, pic->LumpSize);
					pic->ReleaseCache();
					return true;
				});
				PNGHandle *png = M_VerifyPNG(picreader);
				if (png != nullptr)
				{
					SavePic = PNGTexture_CreateFromFile(png, node->Filename);
					delete png;
					if (SavePic->GetDisplayWidth() == 1 && SavePic->GetDisplayHeight() == 1)
					{
						delete SavePic;
						SavePic = nullptr;
						SavePicData.Clear();
					}
				}
			}
		}
		delete resf;
	}
	return index;
}

DEFINE_ACTION_FUNCTION(FSavegameManager, ExtractSaveData)
{
	PARAM_SELF_STRUCT_PROLOGUE(FSavegameManager);
	PARAM_INT(sel);
	ACTION_RETURN_INT(self->ExtractSaveData(sel));
}

//=============================================================================
//
//
//
//=============================================================================

void FSavegameManager::UnloadSaveData()
{
	if (SavePic != nullptr)
	{
		delete SavePic;
	}

	SaveCommentString = "";
	SavePic = nullptr;
	SavePicData.Clear();
}

DEFINE_ACTION_FUNCTION(FSavegameManager, UnloadSaveData)
{
	PARAM_SELF_STRUCT_PROLOGUE(FSavegameManager);
	self->UnloadSaveData();
	return 0;
}

//=============================================================================
//
//
//
//=============================================================================

void FSavegameManager::ClearSaveStuff()
{
	UnloadSaveData();
	if (quickSaveSlot == (FSaveGameNode*)1)
	{
		quickSaveSlot = nullptr;
	}
}

DEFINE_ACTION_FUNCTION(FSavegameManager, ClearSaveStuff)
{
	PARAM_SELF_STRUCT_PROLOGUE(FSavegameManager);
	self->ClearSaveStuff();
	return 0;
}

//=============================================================================
//
//
//
//=============================================================================

bool FSavegameManager::DrawSavePic(int x, int y, int w, int h)
{
	if (SavePic == nullptr) return false;
	screen->DrawTexture(SavePic, x, y, 	DTA_DestWidth, w, DTA_DestHeight, h, DTA_Masked, false,	TAG_DONE);
	return true;
}

DEFINE_ACTION_FUNCTION(FSavegameManager, DrawSavePic)
{
	PARAM_SELF_STRUCT_PROLOGUE(FSavegameManager);
	PARAM_INT(x);
	PARAM_INT(y);
	PARAM_INT(w);
	PARAM_INT(h);
	ACTION_RETURN_BOOL(self->DrawSavePic(x, y, w, h));
}

//=============================================================================
//
//
//
//=============================================================================

void FSavegameManager::SetFileInfo(int Selected)
{
	if (!SaveGames[Selected]->Filename.IsEmpty())
	{
		SaveCommentString.Format("File on disk:\n%s", SaveGames[Selected]->Filename.GetChars());
	}
}

DEFINE_ACTION_FUNCTION(FSavegameManager, SetFileInfo)
{
	PARAM_SELF_STRUCT_PROLOGUE(FSavegameManager);
	PARAM_INT(i);
	self->SetFileInfo(i);
	return 0;
}


//=============================================================================
//
//
//
//=============================================================================

unsigned FSavegameManager::SavegameCount()
{
	return SaveGames.Size();
}

DEFINE_ACTION_FUNCTION(FSavegameManager, SavegameCount)
{
	PARAM_SELF_STRUCT_PROLOGUE(FSavegameManager);
	ACTION_RETURN_INT(self->SavegameCount());
}

//=============================================================================
//
//
//
//=============================================================================

FSaveGameNode *FSavegameManager::GetSavegame(int i)
{
	return SaveGames[i];
}

DEFINE_ACTION_FUNCTION(FSavegameManager, GetSavegame)
{
	PARAM_SELF_STRUCT_PROLOGUE(FSavegameManager);
	PARAM_INT(i);
	ACTION_RETURN_POINTER(self->GetSavegame(i));
}

//=============================================================================
//
//
//
//=============================================================================

void FSavegameManager::InsertNewSaveNode()
{
	NewSaveNode.SaveTitle = GStrings["NEWSAVE"];
	NewSaveNode.bNoDelete = true;
	SaveGames.Insert(0, &NewSaveNode);
}

DEFINE_ACTION_FUNCTION(FSavegameManager, InsertNewSaveNode)
{
	PARAM_SELF_STRUCT_PROLOGUE(FSavegameManager);
	self->InsertNewSaveNode();
	return 0;
}

//=============================================================================
//
//
//
//=============================================================================

bool FSavegameManager::RemoveNewSaveNode()
{
	if (SaveGames[0] == &NewSaveNode)
	{
		SaveGames.Delete(0);
		return true;
	}
	return false;
}

DEFINE_ACTION_FUNCTION(FSavegameManager, RemoveNewSaveNode)
{
	PARAM_SELF_STRUCT_PROLOGUE(FSavegameManager);
	ACTION_RETURN_INT(self->RemoveNewSaveNode());
}


FSavegameManager savegameManager;

DEFINE_ACTION_FUNCTION(FSavegameManager, GetManager)
{
	PARAM_PROLOGUE;
	ACTION_RETURN_POINTER(&savegameManager);
}



DEFINE_FIELD(FSaveGameNode, SaveTitle);
DEFINE_FIELD(FSaveGameNode, Filename);
DEFINE_FIELD(FSaveGameNode, bOldVersion);
DEFINE_FIELD(FSaveGameNode, bMissingWads);
DEFINE_FIELD(FSaveGameNode, bNoDelete);

DEFINE_FIELD(FSavegameManager, WindowSize);
DEFINE_FIELD(FSavegameManager, quickSaveSlot);
DEFINE_FIELD(FSavegameManager, SaveCommentString);

