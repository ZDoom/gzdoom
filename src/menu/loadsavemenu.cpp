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
#include "i_system.h"
#include "version.h"
#include "g_game.h"
#include "m_png.h"
#include "w_wad.h"
#include "v_text.h"
#include "d_event.h"
#include "gstrings.h"
#include "v_palette.h"
#include "doomstat.h"
#include "gi.h"
#include "d_gui.h"
#include "serializer.h"
#include "resourcefiles/resourcefile.h"


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

				FResourceFile *savegame = FResourceFile::OpenResourceFile(filepath, nullptr, true, true);
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
					FSerializer arc;
					if (arc.OpenReader((const char *)data, info->LumpSize))
					{
						int savever = 0;
						FString engine;
						FString iwad;
						FString title;

						arc("Save Version", savever);
						arc("Engine", engine);
						arc("Game WAD", iwad);
						arc("Title", title);

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
						else if (iwad.CompareNoCase(Wads.GetWadName(FWadCollection::IWAD_FILENUM)) == 0)
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
					FILE *file = fopen(filepath, "rb");
					if (file != nullptr)
					{
						PNGHandle *png;
						char sig[16];
						char title[SAVESTRINGSIZE + 1];
						bool oldVer = true;
						bool addIt = false;
						bool missing = false;

						// ZDoom 1.23 betas 21-33 have the savesig first.
						// Earlier versions have the savesig second.
						// Later versions have the savegame encapsulated inside a PNG.
						//
						// Old savegame versions are always added to the menu so
						// the user can easily delete them if desired.

						title[SAVESTRINGSIZE] = 0;


						if (nullptr != (png = M_VerifyPNG(file)))
						{
							char *ver = M_GetPNGText(png, "ZDoom Save Version");
							if (ver != nullptr)
							{
								// An old version
								if (!M_GetPNGText(png, "Title", title, SAVESTRINGSIZE))
								{
									strncpy(title, I_FindName(&c_file), SAVESTRINGSIZE);
								}
								addIt = true;
								delete[] ver;
							}
							delete png;
						}
						else
						{
							fseek(file, 0, SEEK_SET);
							if (fread(sig, 1, 16, file) == 16)
							{

								if (strncmp(sig, "ZDOOMSAVE", 9) == 0)
								{
									if (fread(title, 1, SAVESTRINGSIZE, file) == SAVESTRINGSIZE)
									{
										addIt = true;
									}
								}
								else
								{
									memcpy(title, sig, 16);
									if (fread(title + 16, 1, SAVESTRINGSIZE - 16, file) == SAVESTRINGSIZE - 16 &&
										fread(sig, 1, 16, file) == 16 &&
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
						fclose(file);
					}
				}
			} while (I_FindNext(filefirst, &c_file) == 0);
			I_FindClose(filefirst);
		}
	}
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
	V_SetBorderNeedRefresh();
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
		FILE *test;

		for (i = 0;; ++i)
		{
			filename = G_BuildSaveName("save", i);
			test = fopen(filename, "rb");
			if (test == nullptr)
			{
				break;
			}
			fclose(test);
		}
		G_SaveGame(filename, savegamestring);
	}
	M_ClearMenus();
	V_SetBorderNeedRefresh();
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
			index = LastAccessed + 1;
		}
	}

	UnloadSaveData();

	if ((unsigned)index < SaveGames.Size() &&
		(node = SaveGames[index]) &&
		!node->Filename.IsEmpty() &&
		!node->bOldVersion &&
		(resf = FResourceFile::OpenResourceFile(node->Filename.GetChars(), nullptr, true)) != nullptr)
	{
		FResourceLump *info = resf->FindLump("info.json");
		if (info == nullptr)
		{
			// this should not happen because the file has already been verified.
			return index;
		}
		void *data = info->CacheLump();
		FSerializer arc;
		if (arc.OpenReader((const char *)data, info->LumpSize))
		{
			FString time, pcomment, comment;

			arc("Creation Time", time);
			arc("Comment", pcomment);

			comment = time;
			if (time.Len() > 0) comment += "\n\n";
			comment += pcomment;

			SaveComment = V_BreakLines(SmallFont, WindowSize, comment.GetChars());

			// Extract pic
			FResourceLump *pic = resf->FindLump("savepic.png");
			if (pic != nullptr)
			{
				FileReader *reader = pic->NewReader();
				if (reader != nullptr)
				{
					// copy to a memory buffer which gets accessed through a memory reader and PNGHandle.
					// We cannot use the actual lump as backing for the texture because that requires keeping the
					// savegame file open.
					SavePicData.Resize(pic->LumpSize);
					reader->Read(&SavePicData[0], pic->LumpSize);
					reader = new MemoryReader(&SavePicData[0], SavePicData.Size());
					PNGHandle *png = M_VerifyPNG(reader);
					if (png != nullptr)
					{
						SavePic = PNGTexture_CreateFromFile(png, node->Filename);
						currentSavePic = reader;	// must be kept so that the texture can read from it.
						delete png;
						if (SavePic->GetWidth() == 1 && SavePic->GetHeight() == 1)
						{
							delete SavePic;
							SavePic = nullptr;
							delete currentSavePic;
							currentSavePic = nullptr;
							SavePicData.Clear();
						}
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
	if (SaveComment != nullptr)
	{
		V_FreeBrokenLines(SaveComment);
	}
	if (currentSavePic != nullptr)
	{
		delete currentSavePic;
	}

	SavePic = nullptr;
	SaveComment = nullptr;
	currentSavePic = nullptr;
	SavePicData.Clear();
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

//=============================================================================
//
//
//
//=============================================================================

void FSavegameManager::DrawSaveComment(FFont *font, int cr, int x, int y, int scalefactor)
{
	int sx = CleanXfac;
	int sy = CleanYfac;

	CleanXfac = CleanYfac = scalefactor;

	// I'm not sure why SaveComment would go nullptr in this loop, but I got
	// a crash report where it was nullptr when i reached 1, so now I check
	// for that.
	for (int i = 0; SaveComment != nullptr && SaveComment[i].Width >= 0 && i < 6; ++i)
	{
		screen->DrawText(font, cr, x, y + font->GetHeight() * i * scalefactor, SaveComment[i].Text,	DTA_CleanNoMove, true, TAG_DONE);
	}

	CleanXfac = sx;
	CleanYfac = sy;
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
		char workbuf[512];

		mysnprintf(workbuf, countof(workbuf), "File on disk:\n%s", SaveGames[Selected]->Filename.GetChars());
		if (SaveComment != nullptr)
		{
			V_FreeBrokenLines(SaveComment);
		}
		SaveComment = V_BreakLines(SmallFont, WindowSize, workbuf);
	}
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

FSaveGameNode *FSavegameManager::GetSavegame(unsigned i)
{
	return SaveGames[i];
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




FSavegameManager savegameManager;




class DLoadSaveMenu : public DListMenu
{
	DECLARE_CLASS(DLoadSaveMenu, DListMenu)

public:

	FSavegameManager *manager;

	int Selected;
	int TopItem;


	int savepicLeft;
	int savepicTop;
	int savepicWidth;
	int savepicHeight;
	int rowHeight;
	int listboxLeft;
	int listboxTop;
	int listboxWidth;
	
	int listboxRows;
	int listboxHeight;
	int listboxRight;
	int listboxBottom;

	int commentLeft;
	int commentTop;
	int commentWidth;
	int commentHeight;
	int commentRight;
	int commentBottom;

	// this needs to be kept in memory so that the texture can access it when it needs to.
	bool mEntering;
	DTextEnterMenu *mInput = nullptr;

	DLoadSaveMenu(DMenu *parent = nullptr, DListMenuDescriptor *desc = nullptr);
	void OnDestroy() override;

	void Drawer ();
	bool MenuEvent (int mkey, bool fromcontroller);
	bool MouseEvent(int type, int x, int y);
	bool Responder(event_t *ev);
};

IMPLEMENT_CLASS(DLoadSaveMenu, false, false)



//=============================================================================
//
// End of static savegame maintenance code
//
//=============================================================================

DLoadSaveMenu::DLoadSaveMenu(DMenu *parent, DListMenuDescriptor *desc)
: DListMenu(parent, desc)
{
	manager = &savegameManager;
	manager->ReadSaveStrings();

	savepicLeft = 10;
	savepicTop = 54*CleanYfac;
	savepicWidth = 216*screen->GetWidth()/640;
	savepicHeight = 135*screen->GetHeight()/400;
	manager->WindowSize = savepicWidth;

	rowHeight = (SmallFont->GetHeight() + 1) * CleanYfac;
	listboxLeft = savepicLeft + savepicWidth + 14;
	listboxTop = savepicTop;
	listboxWidth = screen->GetWidth() - listboxLeft - 10;
	int listboxHeight1 = screen->GetHeight() - listboxTop - 10;
	listboxRows = (listboxHeight1 - 1) / rowHeight;
	listboxHeight = listboxRows * rowHeight + 1;
	listboxRight = listboxLeft + listboxWidth;
	listboxBottom = listboxTop + listboxHeight;

	commentLeft = savepicLeft;
	commentTop = savepicTop + savepicHeight + 16;
	commentWidth = savepicWidth;
	commentHeight = (51+(screen->GetHeight()>200?10:0))*CleanYfac;
	commentRight = commentLeft + commentWidth;
	commentBottom = commentTop + commentHeight;
}

void DLoadSaveMenu::OnDestroy()
{
	manager->ClearSaveStuff ();
	Super::OnDestroy();
}

//=============================================================================
//
//
//
//=============================================================================

void DLoadSaveMenu::Drawer ()
{
	Super::Drawer();

	FSaveGameNode *node;
	int i;
	unsigned j;
	bool didSeeSelected = false;

	// Draw picture area
	if (gameaction == ga_loadgame || gameaction == ga_loadgamehidecon || gameaction == ga_savegame)
	{
		return;
	}

	V_DrawFrame (savepicLeft, savepicTop, savepicWidth, savepicHeight);
	if (!manager->DrawSavePic(savepicLeft, savepicTop, savepicWidth, savepicHeight))
	{
		screen->Clear (savepicLeft, savepicTop,
			savepicLeft+savepicWidth, savepicTop+savepicHeight, 0, 0);

		if (manager->SavegameCount() > 0)
		{
			const char *text =
				(Selected == -1 || !manager->GetSavegame(Selected)->bOldVersion)
				? GStrings("MNU_NOPICTURE") : GStrings("MNU_DIFFVERSION");
			const int textlen = SmallFont->StringWidth (text)*CleanXfac;

			screen->DrawText (SmallFont, CR_GOLD, savepicLeft+(savepicWidth-textlen)/2,
				savepicTop+(savepicHeight-rowHeight)/2, text,
				DTA_CleanNoMove, true, TAG_DONE);
		}
	}

	// Draw comment area
	V_DrawFrame (commentLeft, commentTop, commentWidth, commentHeight);
	screen->Clear (commentLeft, commentTop, commentRight, commentBottom, 0, 0);

	// manager->DrawSaveComment(SmallFont, CR_GOLD, commentLeft, commentTop, CleanYfac);

	// Draw file area
	V_DrawFrame (listboxLeft, listboxTop, listboxWidth, listboxHeight);
	screen->Clear (listboxLeft, listboxTop, listboxRight, listboxBottom, 0, 0);

	if (manager->SavegameCount() == 0)
	{
		const char * text = GStrings("MNU_NOFILES");
		const int textlen = SmallFont->StringWidth (text)*CleanXfac;

		screen->DrawText (SmallFont, CR_GOLD, listboxLeft+(listboxWidth-textlen)/2,
			listboxTop+(listboxHeight-rowHeight)/2, text,
			DTA_CleanNoMove, true, TAG_DONE);
		return;
	}

	for (i = 0, j = TopItem; i < listboxRows && j < manager->SavegameCount(); i++,j++)
	{
		int color;
		node = manager->GetSavegame(j);
		if (node->bOldVersion)
		{
			color = CR_BLUE;
		}
		else if (node->bMissingWads)
		{
			color = CR_ORANGE;
		}
		else if ((int)j == Selected)
		{
			color = CR_WHITE;
		}
		else
		{
			color = CR_TAN;
		}

		if ((int)j == Selected)
		{
			screen->Clear (listboxLeft, listboxTop+rowHeight*i,
				listboxRight, listboxTop+rowHeight*(i+1), -1,
				mEntering ? MAKEARGB(255,255,0,0) : MAKEARGB(255,0,0,255));
			didSeeSelected = true;
			if (!mEntering)
			{
				screen->DrawText (SmallFont, color,
					listboxLeft+1, listboxTop+rowHeight*i+CleanYfac, node->SaveTitle.GetChars(),
					DTA_CleanNoMove, true, TAG_DONE);
			}
			else
			{
				FString s = mInput->GetText() + SmallFont->GetCursor();
				screen->DrawText (SmallFont, CR_WHITE,
					listboxLeft+1, listboxTop+rowHeight*i+CleanYfac, s,
					DTA_CleanNoMove, true, TAG_DONE);
			}
		}
		else
		{
			screen->DrawText (SmallFont, color,
				listboxLeft+1, listboxTop+rowHeight*i+CleanYfac, node->SaveTitle.GetChars(),
				DTA_CleanNoMove, true, TAG_DONE);
		}
	}
} 

//=============================================================================
//
//
//
//=============================================================================

bool DLoadSaveMenu::MenuEvent (int mkey, bool fromcontroller)
{
	switch (mkey)
	{
	case MKEY_Up:
		if (manager->SavegameCount() > 1)
		{
			if (Selected == -1) Selected = TopItem;
			else
			{
				if (--Selected < 0) Selected = manager->SavegameCount()-1;
				if (Selected < TopItem) TopItem = Selected;
				else if (Selected >= TopItem + listboxRows) TopItem = MAX(0, Selected - listboxRows + 1);
			}
			manager->UnloadSaveData ();
			manager->ExtractSaveData (Selected);
		}
		return true;

	case MKEY_Down:
		if (manager->SavegameCount() > 1)
		{
			if (Selected == -1) Selected = TopItem;
			else
			{
				if (unsigned(++Selected) >= manager->SavegameCount()) Selected = 0;
				if (Selected < TopItem) TopItem = Selected;
				else if (Selected >= TopItem + listboxRows) TopItem = MAX(0, Selected - listboxRows + 1);
			}
			manager->UnloadSaveData ();
			manager->ExtractSaveData (Selected);
		}
		return true;

	case MKEY_PageDown:
		if (manager->SavegameCount() > 1)
		{
			if (TopItem >= (int)manager->SavegameCount() - listboxRows)
			{
				TopItem = 0;
				if (Selected != -1) Selected = 0;
			}
			else
			{
				TopItem = MIN<int>(TopItem + listboxRows, manager->SavegameCount() - listboxRows);
				if (TopItem > Selected && Selected != -1) Selected = TopItem;
			}
			manager->UnloadSaveData ();
			manager->ExtractSaveData (Selected);
		}
		return true;

	case MKEY_PageUp:
		if (manager->SavegameCount() > 1)
		{
			if (TopItem == 0)
			{
				TopItem = manager->SavegameCount() - listboxRows;
				if (Selected != -1) Selected = TopItem;
			}
			else
			{
				TopItem = MAX(TopItem - listboxRows, 0);
				if (Selected >= TopItem + listboxRows) Selected = TopItem;
			}
			manager->UnloadSaveData ();
			manager->ExtractSaveData (Selected);
		}
		return true;

	case MKEY_Enter:
		return false;	// This event will be handled by the subclasses

	case MKEY_MBYes:
	{
		if ((unsigned)Selected < manager->SavegameCount())
		{
			Selected = manager->RemoveSaveSlot (Selected);
		}
		return true;
	}

	default:
		return Super::MenuEvent(mkey, fromcontroller);
	}
}

//=============================================================================
//
//
//
//=============================================================================

bool DLoadSaveMenu::MouseEvent(int type, int x, int y)
{
	if (x >= listboxLeft && x < listboxLeft + listboxWidth && 
		y >= listboxTop && y < listboxTop + listboxHeight)
	{
		int lineno = (y - listboxTop) / rowHeight;

		if (TopItem + lineno < (int)manager->SavegameCount())
		{
			Selected = TopItem + lineno;
			manager->UnloadSaveData ();
			manager->ExtractSaveData (Selected);
			if (type == MOUSE_Release)
			{
				if (MenuEvent(MKEY_Enter, true))
				{
					return true;
				}
			}
		}
		else Selected = -1;
	}
	else Selected = -1;

	return Super::MouseEvent(type, x, y);
}

//=============================================================================
//
//
//
//=============================================================================

bool DLoadSaveMenu::Responder (event_t *ev)
{
	if (ev->type == EV_GUI_Event)
	{
		if (ev->subtype == EV_GUI_KeyDown)
		{
			if ((unsigned)Selected < manager->SavegameCount())
			{
				switch (ev->data1)
				{
				case GK_F1:
					manager->SetFileInfo(Selected);
					return true;

				case GK_DEL:
				case '\b':
					{
						FString EndString;
						EndString.Format("%s" TEXTCOLOR_WHITE "%s" TEXTCOLOR_NORMAL "?\n\n%s",
							GStrings("MNU_DELETESG"), manager->GetSavegame(Selected)->SaveTitle.GetChars(), GStrings("PRESSYN"));
						M_StartMessage (EndString, 0);
					}
					return true;
				}
			}
		}
		else if (ev->subtype == EV_GUI_WheelUp)
		{
			if (TopItem > 0) TopItem--;
			return true;
		}
		else if (ev->subtype == EV_GUI_WheelDown)
		{
			if (TopItem < (int)manager->SavegameCount() - listboxRows) TopItem++;
			return true;
		}
	}
	return Super::Responder(ev);
}


//=============================================================================
//
//
//
//=============================================================================

class DSaveMenu : public DLoadSaveMenu
{
	DECLARE_CLASS(DSaveMenu, DLoadSaveMenu)

public:

	DSaveMenu(DMenu *parent = nullptr, DListMenuDescriptor *desc = nullptr);
	void OnDestroy() override;
	bool Responder (event_t *ev);
	bool MenuEvent (int mkey, bool fromcontroller);

};

IMPLEMENT_CLASS(DSaveMenu, false, false)


//=============================================================================
//
//
//
//=============================================================================

DSaveMenu::DSaveMenu(DMenu *parent, DListMenuDescriptor *desc)
: DLoadSaveMenu(parent, desc)
{
	manager->InsertNewSaveNode();
	TopItem = 0;
	Selected = manager->ExtractSaveData (-1);
}

//=============================================================================
//
//
//
//=============================================================================

void DSaveMenu::OnDestroy()
{
	if (manager->RemoveNewSaveNode())
	{
		Selected--;
	}
	Super::OnDestroy();
}

//=============================================================================
//
//
//
//=============================================================================

bool DSaveMenu::MenuEvent (int mkey, bool fromcontroller)
{
	if (Super::MenuEvent(mkey, fromcontroller)) 
	{
		return true;
	}
	if (Selected == -1)
	{
		return false;
	}

	if (mkey == MKEY_Enter)
	{
		const char *SavegameString = (Selected != 0)? manager->GetSavegame(Selected)->SaveTitle.GetChars() : "";
		mInput = new DTextEnterMenu(this, SavegameString, SAVESTRINGSIZE, 1, fromcontroller);
		M_ActivateMenu(mInput);
		mEntering = true;
	}
	else if (mkey == MKEY_Input)
	{
		mEntering = false;
		manager->DoSave(Selected, mInput->GetText());
		mInput = nullptr;
	}
	else if (mkey == MKEY_Abort)
	{
		mEntering = false;
		mInput = nullptr;
	}
	return false;
}

//=============================================================================
//
//
//
//=============================================================================

bool DSaveMenu::Responder (event_t *ev)
{
	if (ev->subtype == EV_GUI_KeyDown)
	{
		if (Selected != -1)
		{
			switch (ev->data1)
			{
			case GK_DEL:
			case '\b':
				// cannot delete 'new save game' item
				if (Selected == 0) return true;
				break;

			case 'N':
				Selected = TopItem = 0;
				manager->UnloadSaveData ();
				return true;
			}
		}
	}
	return Super::Responder(ev);
}

//=============================================================================
//
//
//
//=============================================================================

class DLoadMenu : public DLoadSaveMenu
{
	DECLARE_CLASS(DLoadMenu, DLoadSaveMenu)

public:

	DLoadMenu(DMenu *parent = nullptr, DListMenuDescriptor *desc = nullptr);

	bool MenuEvent (int mkey, bool fromcontroller);
};

IMPLEMENT_CLASS(DLoadMenu, false, false)


//=============================================================================
//
//
//
//=============================================================================

DLoadMenu::DLoadMenu(DMenu *parent, DListMenuDescriptor *desc)
: DLoadSaveMenu(parent, desc)
{
	TopItem = 0;
	Selected = manager->ExtractSaveData (-1);

}

//=============================================================================
//
//
//
//=============================================================================

bool DLoadMenu::MenuEvent (int mkey, bool fromcontroller)
{
	if (Super::MenuEvent(mkey, fromcontroller)) 
	{
		return true;
	}
	if (Selected == -1 || manager->SavegameCount() == 0)
	{
		return false;
	}

	if (mkey == MKEY_Enter)
	{
		manager->LoadSavegame(Selected);
		return true;
	}
	return false;
}


DEFINE_FIELD(FSaveGameNode, SaveTitle);
DEFINE_FIELD(FSaveGameNode, Filename);
DEFINE_FIELD(FSaveGameNode, bOldVersion);
DEFINE_FIELD(FSaveGameNode, bMissingWads);
DEFINE_FIELD(FSaveGameNode, bNoDelete);

DEFINE_FIELD(FSavegameManager, WindowSize);
DEFINE_FIELD(FSavegameManager, quickSaveSlot);

DEFINE_FIELD(DLoadSaveMenu, manager);
DEFINE_FIELD(DLoadSaveMenu, Selected);
DEFINE_FIELD(DLoadSaveMenu, TopItem);
DEFINE_FIELD(DLoadSaveMenu, savepicLeft);
DEFINE_FIELD(DLoadSaveMenu, savepicTop);
DEFINE_FIELD(DLoadSaveMenu, savepicWidth);
DEFINE_FIELD(DLoadSaveMenu, savepicHeight);
DEFINE_FIELD(DLoadSaveMenu, rowHeight);
DEFINE_FIELD(DLoadSaveMenu, listboxLeft);
DEFINE_FIELD(DLoadSaveMenu, listboxTop);
DEFINE_FIELD(DLoadSaveMenu, listboxWidth);
DEFINE_FIELD(DLoadSaveMenu, listboxRows);
DEFINE_FIELD(DLoadSaveMenu, listboxHeight);
DEFINE_FIELD(DLoadSaveMenu, listboxRight);
DEFINE_FIELD(DLoadSaveMenu, listboxBottom);
DEFINE_FIELD(DLoadSaveMenu, commentLeft);
DEFINE_FIELD(DLoadSaveMenu, commentTop);
DEFINE_FIELD(DLoadSaveMenu, commentWidth);
DEFINE_FIELD(DLoadSaveMenu, commentHeight);
DEFINE_FIELD(DLoadSaveMenu, commentRight);
DEFINE_FIELD(DLoadSaveMenu, commentBottom);
DEFINE_FIELD(DLoadSaveMenu, mEntering);
DEFINE_FIELD(DLoadSaveMenu, mInput);
