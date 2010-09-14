/*
** loadsavemenu.cpp
** The load game and save game menus
**
**---------------------------------------------------------------------------
** Copyright 2001-2010 Randy Heit
** Copyright 2010 Christoph Oelckers
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
#include "lists.h"
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




class DLoadSaveMenu : public DListMenu
{
	DECLARE_CLASS(DLoadSaveMenu, DListMenu)

protected:
	static List SaveGames;
	static FSaveGameNode *TopSaveGame;
	static FSaveGameNode *lastSaveSlot;
	static FSaveGameNode *SelSaveGame;


	friend void M_NotifyNewSave (const char *file, const char *title, bool okForQuicksave);

	static FSaveGameNode *RemoveSaveSlot (FSaveGameNode *file);
	static void UnloadSaveStrings();
	static void InsertSaveNode (FSaveGameNode *node);
	static void ReadSaveStrings ();
	static void NotifyNewSave (const char *file, const char *title, bool okForQuicksave);


	FTexture *SavePic;
	FBrokenLines *SaveComment;
	bool mEntering;
	char savegamestring[SAVESTRINGSIZE];
	bool mWheelScrolled;

	DLoadSaveMenu(DMenu *parent = NULL, FListMenuDescriptor *desc = NULL);
	void Destroy();

	void UnloadSaveData ();
	void ClearSaveStuff ();
	void ExtractSaveData (const FSaveGameNode *node);
	void Drawer ();
	bool MenuEvent (int mkey, bool fromcontroller);
	bool MouseEvent(int type, int x, int y);
	bool Responder(event_t *ev);

};

IMPLEMENT_CLASS(DLoadSaveMenu)

List DLoadSaveMenu::SaveGames;
FSaveGameNode *DLoadSaveMenu::TopSaveGame;
FSaveGameNode *DLoadSaveMenu::lastSaveSlot;
FSaveGameNode *DLoadSaveMenu::SelSaveGame;

FSaveGameNode *quickSaveSlot;

//=============================================================================
//
// Save data maintenance (stored statically)
//
//=============================================================================

FSaveGameNode *DLoadSaveMenu::RemoveSaveSlot (FSaveGameNode *file)
{
	FSaveGameNode *next = static_cast<FSaveGameNode *>(file->Succ);

	if (file == TopSaveGame)
	{
		TopSaveGame = next;
	}
	if (quickSaveSlot == file)
	{
		quickSaveSlot = NULL;
	}
	if (lastSaveSlot == file)
	{
		lastSaveSlot = NULL;
	}
	file->Remove ();
	if (!file->bNoDelete) delete file;
	return next;
}


//=============================================================================
//
//
//
//=============================================================================

void DLoadSaveMenu::UnloadSaveStrings()
{
	while (!SaveGames.IsEmpty())
	{
		RemoveSaveSlot (static_cast<FSaveGameNode *>(SaveGames.Head));
	}
}


//=============================================================================
//
//
//
//=============================================================================

void DLoadSaveMenu::InsertSaveNode (FSaveGameNode *node)
{
	FSaveGameNode *probe;

	if (SaveGames.IsEmpty ())
	{
		SaveGames.AddHead (node);
		return;
	}

	if (node->bOldVersion)
	{ // Add node at bottom of list
		probe = static_cast<FSaveGameNode *>(SaveGames.TailPred);
		while (probe->Pred != NULL && probe->bOldVersion &&
			stricmp (node->Title, probe->Title) < 0)
		{
			probe = static_cast<FSaveGameNode *>(probe->Pred);
		}
		node->Insert (probe);
	}
	else
	{ // Add node at top of list
		probe = static_cast<FSaveGameNode *>(SaveGames.Head);
		while (probe->Succ != NULL && !probe->bOldVersion &&
			stricmp (node->Title, probe->Title) > 0)
		{
			probe = static_cast<FSaveGameNode *>(probe->Succ);
		}
		node->InsertBefore (probe);
	}
}


//=============================================================================
//
// M_ReadSaveStrings
//
// Find savegames and read their titles
//
//=============================================================================

void DLoadSaveMenu::ReadSaveStrings ()
{
	if (SaveGames.IsEmpty ())
	{
		void *filefirst;
		findstate_t c_file;
		FString filter;

		atterm (UnloadSaveStrings);

		filter = G_BuildSaveName ("*.zds", -1);
		filefirst = I_FindFirst (filter.GetChars(), &c_file);
		if (filefirst != ((void *)(-1)))
		{
			do
			{
				// I_FindName only returns the file's name and not its full path
				FString filepath = G_BuildSaveName (I_FindName(&c_file), -1);
				FILE *file = fopen (filepath, "rb");

				if (file != NULL)
				{
					PNGHandle *png;
					char sig[16];
					char title[SAVESTRINGSIZE+1];
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

					if (NULL != (png = M_VerifyPNG (file)))
					{
						char *ver = M_GetPNGText (png, "ZDoom Save Version");
						char *engine = M_GetPNGText (png, "Engine");
						if (ver != NULL)
						{
							if (!M_GetPNGText (png, "Title", title, SAVESTRINGSIZE))
							{
								strncpy (title, I_FindName(&c_file), SAVESTRINGSIZE);
							}
							if (strncmp (ver, SAVESIG, 9) == 0 &&
								atoi (ver+9) >= MINSAVEVER &&
								engine != NULL)
							{
								// Was saved with a compatible ZDoom version,
								// so check if it's for the current game.
								// If it is, add it. Otherwise, ignore it.
								char *iwad = M_GetPNGText (png, "Game WAD");
								if (iwad != NULL)
								{
									if (stricmp (iwad, Wads.GetWadName (FWadCollection::IWAD_FILENUM)) == 0)
									{
										addIt = true;
										oldVer = false;
										missing = !G_CheckSaveGameWads (png, false);
									}
									delete[] iwad;
								}
							}
							else
							{ // An old version
								addIt = true;
							}
							delete[] ver;
						}
						if (engine != NULL)
						{
							delete[] engine;
						}
						delete png;
					}
					else
					{
						fseek (file, 0, SEEK_SET);
						if (fread (sig, 1, 16, file) == 16)
						{

							if (strncmp (sig, "ZDOOMSAVE", 9) == 0)
							{
								if (fread (title, 1, SAVESTRINGSIZE, file) == SAVESTRINGSIZE)
								{
									addIt = true;
								}
							}
							else
							{
								memcpy (title, sig, 16);
								if (fread (title + 16, 1, SAVESTRINGSIZE-16, file) == SAVESTRINGSIZE-16 &&
									fread (sig, 1, 16, file) == 16 &&
									strncmp (sig, "ZDOOMSAVE", 9) == 0)
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
						node->bOldVersion = oldVer;
						node->bMissingWads = missing;
						memcpy (node->Title, title, SAVESTRINGSIZE);
						InsertSaveNode (node);
					}
					fclose (file);
				}
			} while (I_FindNext (filefirst, &c_file) == 0);
			I_FindClose (filefirst);
		}
	}
	if (SelSaveGame == NULL || SelSaveGame->Succ == NULL)
	{
		SelSaveGame = static_cast<FSaveGameNode *>(SaveGames.Head);
	}
}


//=============================================================================
//
//
//
//=============================================================================

void DLoadSaveMenu::NotifyNewSave (const char *file, const char *title, bool okForQuicksave)
{
	FSaveGameNode *node;

	if (file == NULL)
		return;

	ReadSaveStrings ();

	// See if the file is already in our list
	for (node = static_cast<FSaveGameNode *>(SaveGames.Head);
		 node->Succ != NULL;
		 node = static_cast<FSaveGameNode *>(node->Succ))
	{
#ifdef unix
		if (node->Filename.Compare (file) == 0)
#else
		if (node->Filename.CompareNoCase (file) == 0)
#endif
		{
			strcpy (node->Title, title);
			node->bOldVersion = false;
			node->bMissingWads = false;
			break;
		}
	}

	if (node->Succ == NULL)
	{
		node = new FSaveGameNode;
		strcpy (node->Title, title);
		node->Filename = file;
		node->bOldVersion = false;
		node->bMissingWads = false;
		InsertSaveNode (node);
		SelSaveGame = node;
	}

	if (okForQuicksave)
	{
		if (quickSaveSlot == NULL) quickSaveSlot = node;
		lastSaveSlot = node;
	}
}

void M_NotifyNewSave (const char *file, const char *title, bool okForQuicksave)
{
	DLoadSaveMenu::NotifyNewSave(file, title, okForQuicksave);
}

//=============================================================================
//
// End of static savegame maintenance code
//
//=============================================================================

DLoadSaveMenu::DLoadSaveMenu(DMenu *parent, FListMenuDescriptor *desc)
: DListMenu(parent, desc)
{
	ReadSaveStrings();
	mWheelScrolled = false;
}

void DLoadSaveMenu::Destroy()
{
	ClearSaveStuff ();
}

//=============================================================================
//
//
//
//=============================================================================

void DLoadSaveMenu::UnloadSaveData ()
{
	if (SavePic != NULL)
	{
		delete SavePic;
	}
	if (SaveComment != NULL)
	{
		V_FreeBrokenLines (SaveComment);
	}

	SavePic = NULL;
	SaveComment = NULL;
}

//=============================================================================
//
//
//
//=============================================================================

void DLoadSaveMenu::ClearSaveStuff ()
{
	UnloadSaveData();
	if (quickSaveSlot == (FSaveGameNode *)1)
	{
		quickSaveSlot = NULL;
	}
}

//=============================================================================
//
//
//
//=============================================================================

void DLoadSaveMenu::ExtractSaveData (const FSaveGameNode *node)
{
	FILE *file;
	PNGHandle *png;

	UnloadSaveData ();

	if (node != NULL &&
		node->Succ != NULL &&
		!node->Filename.IsEmpty() &&
		!node->bOldVersion &&
		(file = fopen (node->Filename.GetChars(), "rb")) != NULL)
	{
		if (NULL != (png = M_VerifyPNG (file)))
		{
			char *time, *pcomment, *comment;
			size_t commentlen, totallen, timelen;

			// Extract comment
			time = M_GetPNGText (png, "Creation Time");
			pcomment = M_GetPNGText (png, "Comment");
			if (pcomment != NULL)
			{
				commentlen = strlen (pcomment);
			}
			else
			{
				commentlen = 0;
			}
			if (time != NULL)
			{
				timelen = strlen (time);
				totallen = timelen + commentlen + 3;
			}
			else
			{
				timelen = 0;
				totallen = commentlen + 1;
			}
			if (totallen != 0)
			{
				comment = new char[totallen];

				if (timelen)
				{
					memcpy (comment, time, timelen);
					comment[timelen] = '\n';
					comment[timelen+1] = '\n';
					timelen += 2;
				}
				if (commentlen)
				{
					memcpy (comment + timelen, pcomment, commentlen);
				}
				comment[timelen+commentlen] = 0;
				SaveComment = V_BreakLines (SmallFont, 216*screen->GetWidth()/640/CleanXfac, comment);
				delete[] comment;
				delete[] time;
				delete[] pcomment;
			}

			// Extract pic
			SavePic = PNGTexture_CreateFromFile(png, node->Filename);

			delete png;
		}
		fclose (file);
	}
}

//=============================================================================
//
//
//
//=============================================================================

void DLoadSaveMenu::Drawer ()
{
	Super::Drawer();

	const int savepicLeft = 10;
	const int savepicTop = 54*CleanYfac;
	const int savepicWidth = 216*screen->GetWidth()/640;
	const int savepicHeight = 135*screen->GetHeight()/400;

	const int rowHeight = (SmallFont->GetHeight() + 1) * CleanYfac;
	const int listboxLeft = savepicLeft + savepicWidth + 14;
	const int listboxTop = savepicTop;
	const int listboxWidth = screen->GetWidth() - listboxLeft - 10;
	const int listboxHeight1 = screen->GetHeight() - listboxTop - 10;
	const int listboxRows = (listboxHeight1 - 1) / rowHeight;
	const int listboxHeight = listboxRows * rowHeight + 1;
	const int listboxRight = listboxLeft + listboxWidth;
	const int listboxBottom = listboxTop + listboxHeight;

	const int commentLeft = savepicLeft;
	const int commentTop = savepicTop + savepicHeight + 16;
	const int commentWidth = savepicWidth;
	const int commentHeight = (51+(screen->GetHeight()>200?10:0))*CleanYfac;
	const int commentRight = commentLeft + commentWidth;
	const int commentBottom = commentTop + commentHeight;

	FSaveGameNode *node;
	int i;
	bool didSeeSelected = false;

	// Draw picture area
	if (gameaction == ga_loadgame || gameaction == ga_savegame)
	{
		return;
	}

	V_DrawFrame (savepicLeft, savepicTop, savepicWidth, savepicHeight);
	if (SavePic != NULL)
	{
		screen->DrawTexture(SavePic, savepicLeft, savepicTop,
			DTA_DestWidth, savepicWidth,
			DTA_DestHeight, savepicHeight,
			DTA_Masked, false,
			TAG_DONE);
	}
	else
	{
		screen->Clear (savepicLeft, savepicTop,
			savepicLeft+savepicWidth, savepicTop+savepicHeight, 0, 0);

		if (!SaveGames.IsEmpty ())
		{
			const char *text =
				(SelSaveGame == NULL || !SelSaveGame->bOldVersion)
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
	if (SaveComment != NULL)
	{
		// I'm not sure why SaveComment would go NULL in this loop, but I got
		// a crash report where it was NULL when i reached 1, so now I check
		// for that.
		for (i = 0; SaveComment != NULL && SaveComment[i].Width >= 0 && i < 6; ++i)
		{
			screen->DrawText (SmallFont, CR_GOLD, commentLeft, commentTop
				+ SmallFont->GetHeight()*i*CleanYfac, SaveComment[i].Text,
				DTA_CleanNoMove, true, TAG_DONE);
		}
	}

	// Draw file area
	do
	{
		V_DrawFrame (listboxLeft, listboxTop, listboxWidth, listboxHeight);
		screen->Clear (listboxLeft, listboxTop, listboxRight, listboxBottom, 0, 0);

		if (SaveGames.IsEmpty ())
		{
			const char * text = GStrings("MNU_NOFILES");
			const int textlen = SmallFont->StringWidth (text)*CleanXfac;

			screen->DrawText (SmallFont, CR_GOLD, listboxLeft+(listboxWidth-textlen)/2,
				listboxTop+(listboxHeight-rowHeight)/2, text,
				DTA_CleanNoMove, true, TAG_DONE);
			return;
		}

		for (i = 0, node = TopSaveGame;
			 i < listboxRows && node->Succ != NULL;
			 ++i, node = static_cast<FSaveGameNode *>(node->Succ))
		{
			int color;
			if (node->bOldVersion)
			{
				color = CR_BLUE;
			}
			else if (node->bMissingWads)
			{
				color = CR_ORANGE;
			}
			else if (node == SelSaveGame)
			{
				color = CR_WHITE;
			}
			else
			{
				color = CR_TAN;
			}
			if (node == SelSaveGame)
			{
				screen->Clear (listboxLeft, listboxTop+rowHeight*i,
					listboxRight, listboxTop+rowHeight*(i+1), -1,
					mEntering ? MAKEARGB(255,255,0,0) : MAKEARGB(255,0,0,255));
				didSeeSelected = true;
				if (!mEntering)
				{
					screen->DrawText (SmallFont, color,
						listboxLeft+1, listboxTop+rowHeight*i+CleanYfac, node->Title,
						DTA_CleanNoMove, true, TAG_DONE);
				}
				else
				{
					screen->DrawText (SmallFont, CR_WHITE,
						listboxLeft+1, listboxTop+rowHeight*i+CleanYfac, savegamestring,
						DTA_CleanNoMove, true, TAG_DONE);

					screen->DrawText (SmallFont, CR_WHITE,
						listboxLeft+1+SmallFont->StringWidth (savegamestring)*CleanXfac,
						listboxTop+rowHeight*i+CleanYfac, 
						(gameinfo.gametype & (GAME_DoomStrifeChex)) ? "_" : "[",
						DTA_CleanNoMove, true, TAG_DONE);
				}
			}
			else
			{
				screen->DrawText (SmallFont, color,
					listboxLeft+1, listboxTop+rowHeight*i+CleanYfac, node->Title,
					DTA_CleanNoMove, true, TAG_DONE);
			}
		}

		// This is dumb: If the selected node was not visible,
		// scroll down and redraw. M_SaveLoadResponder()
		// guarantees that if the node is not visible, it will
		// always be below the visible list instead of above it.
		// This should not really be done here, but I don't care.

		if (!didSeeSelected)
		{
			// no, this shouldn't be here - and that's why there's now another hack in here
			// so that the mouse scrolling does not get screwed by this code...
			if (mWheelScrolled)
			{
				didSeeSelected = true;
				SelSaveGame = NULL;
				mWheelScrolled = false;
			}
			for (i = 1; node->Succ != NULL && node != SelSaveGame; ++i)
			{
				node = static_cast<FSaveGameNode *>(node->Succ);
			}
			if (node->Succ == NULL)
			{ // SelSaveGame is invalid
				didSeeSelected = true;
			}
			else
			{
				do
				{
					TopSaveGame = static_cast<FSaveGameNode *>(TopSaveGame->Succ);
				} while (--i);
			}
		}
	} while (!didSeeSelected);
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
		if (SelSaveGame == NULL) 
		{
			SelSaveGame = TopSaveGame;
		}
		else if (SelSaveGame->Succ != NULL)
		{
			if (SelSaveGame != SaveGames.Head)
			{
				if (SelSaveGame == TopSaveGame)
				{
					TopSaveGame = static_cast<FSaveGameNode *>(TopSaveGame->Pred);
				}
				SelSaveGame = static_cast<FSaveGameNode *>(SelSaveGame->Pred);
			}
			else
			{
				SelSaveGame = static_cast<FSaveGameNode *>(SaveGames.TailPred);
			}
			UnloadSaveData ();
			ExtractSaveData (SelSaveGame);
		}
		return true;

	case MKEY_Down:
		if (SelSaveGame == NULL) 
		{
			SelSaveGame = TopSaveGame;
		}
		else if (SelSaveGame->Succ != NULL)
		{
			if (SelSaveGame != SaveGames.TailPred)
			{
				SelSaveGame = static_cast<FSaveGameNode *>(SelSaveGame->Succ);
			}
			else
			{
				SelSaveGame = TopSaveGame =
					static_cast<FSaveGameNode *>(SaveGames.Head);
			}
			UnloadSaveData ();
			ExtractSaveData (SelSaveGame);
		}
		return true;

	case MKEY_Enter:
		return false;	// This event will be handled by the subclasses

	case MKEY_MBYes:
	{
		if (SelSaveGame != NULL && SelSaveGame->Succ != NULL)
		{
			FSaveGameNode *next = static_cast<FSaveGameNode *>(SelSaveGame->Succ);
			if (next->Succ == NULL)
			{
				next = static_cast<FSaveGameNode *>(SelSaveGame->Pred);
				if (next->Pred == NULL)
				{
					next = NULL;
				}
			}

			remove (SelSaveGame->Filename.GetChars());
			UnloadSaveData ();
			SelSaveGame = RemoveSaveSlot (SelSaveGame);
			ExtractSaveData (SelSaveGame);
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
	const int savepicLeft = 10;
	const int savepicTop = 54*CleanYfac;
	const int savepicWidth = 216*screen->GetWidth()/640;

	const int rowHeight = (SmallFont->GetHeight() + 1) * CleanYfac;
	const int listboxLeft = savepicLeft + savepicWidth + 14;
	const int listboxTop = savepicTop;
	const int listboxWidth = screen->GetWidth() - listboxLeft - 10;
	const int listboxHeight1 = screen->GetHeight() - listboxTop - 10;
	const int listboxRows = (listboxHeight1 - 1) / rowHeight;
	const int listboxHeight = listboxRows * rowHeight + 1;
	const int listboxRight = listboxLeft + listboxWidth;
	const int listboxBottom = listboxTop + listboxHeight;

	if (x >= listboxLeft && x < listboxLeft + listboxWidth && 
		y >= listboxTop && y < listboxTop + listboxHeight)
	{
		int lineno = (y - listboxTop) / rowHeight;
		FSaveGameNode *top = TopSaveGame;
		while (lineno > 0 && top->Succ != NULL)
		{
			lineno--;
			top = (FSaveGameNode *)top->Succ;
		}
		if (lineno == 0)
		{
			if (SelSaveGame != top)
			{
				SelSaveGame = top;
				UnloadSaveData ();
				ExtractSaveData (SelSaveGame);
				// Sound?
			}
			if (type == MOUSE_Release)
			{
				if (MenuEvent(MKEY_Enter, true))
				{
					return true;
				}
			}
		}
		else SelSaveGame = NULL;
	}
	else
	{
		SelSaveGame = NULL;
	}
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
			if (SelSaveGame != NULL && SelSaveGame->Succ != NULL)
			{
				switch (ev->data1)
				{
				case GK_F1:
					if (!SelSaveGame->Filename.IsEmpty())
					{
						char workbuf[512];

						mysnprintf (workbuf, countof(workbuf), "File on disk:\n%s", SelSaveGame->Filename.GetChars());
						if (SaveComment != NULL)
						{
							V_FreeBrokenLines (SaveComment);
						}
						SaveComment = V_BreakLines (SmallFont, 216*screen->GetWidth()/640/CleanXfac, workbuf);
					}
					return true;

				case GK_DEL:
				case '\b':
					{
						FString EndString;
						EndString.Format("%s" TEXTCOLOR_WHITE "%s" TEXTCOLOR_NORMAL "?\n\n%s",
							GStrings("MNU_DELETESG"), SelSaveGame->Title, GStrings("PRESSYN"));
						M_StartMessage (EndString, 0);
					}
					return true;
				}
			}
		}
		else if (ev->subtype == EV_GUI_WheelUp)
		{
			if (TopSaveGame != SaveGames.Head && TopSaveGame != NULL)
			{
				TopSaveGame = static_cast<FSaveGameNode *>(TopSaveGame->Pred);
				mWheelScrolled = true;
			}
			return true;
		}
		else if (ev->subtype == EV_GUI_WheelDown)
		{
			const int savepicTop = 54*CleanYfac;
			const int listboxTop = savepicTop;
			const int rowHeight = (SmallFont->GetHeight() + 1) * CleanYfac;
			const int listboxHeight1 = screen->GetHeight() - listboxTop - 10;
			const int listboxRows = (listboxHeight1 - 1) / rowHeight;

			FSaveGameNode *node = TopSaveGame;
			if (node != NULL)
			{
				int count = 1;
				while (node != NULL && node != SaveGames.TailPred)
				{
					node = (FSaveGameNode*)node->Succ;
					count++;
				}
				if (count > listboxRows)
				{
					TopSaveGame = (FSaveGameNode*)TopSaveGame->Succ;
					mWheelScrolled = true;
				}
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

class DSaveMenu : public DLoadSaveMenu
{
	DECLARE_CLASS(DSaveMenu, DLoadSaveMenu)

	FSaveGameNode NewSaveNode;

public:

	DSaveMenu(DMenu *parent = NULL, FListMenuDescriptor *desc = NULL);
	void Destroy();
	void DoSave (FSaveGameNode *node);
	bool Responder (event_t *ev);
	bool MenuEvent (int mkey, bool fromcontroller);

};

IMPLEMENT_CLASS(DSaveMenu)


//=============================================================================
//
//
//
//=============================================================================

DSaveMenu::DSaveMenu(DMenu *parent, FListMenuDescriptor *desc)
: DLoadSaveMenu(parent, desc)
{
	strcpy (NewSaveNode.Title, "<New Save Game>");
	NewSaveNode.bNoDelete = true;
	SaveGames.AddHead (&NewSaveNode);
	TopSaveGame = static_cast<FSaveGameNode *>(SaveGames.Head);
	if (lastSaveSlot == NULL)
	{
		SelSaveGame = &NewSaveNode;
	}
	else
	{
		SelSaveGame = lastSaveSlot;
	}
	ExtractSaveData (SelSaveGame);
}

//=============================================================================
//
//
//
//=============================================================================

void DSaveMenu::Destroy()
{
	if (SaveGames.Head == &NewSaveNode)
	{
		SaveGames.RemHead ();
		if (SelSaveGame == &NewSaveNode)
		{
			SelSaveGame = static_cast<FSaveGameNode *>(SaveGames.Head);
		}
		if (TopSaveGame == &NewSaveNode)
		{
			TopSaveGame = static_cast<FSaveGameNode *>(SaveGames.Head);
		}
	}
}

//=============================================================================
//
// 
//
//=============================================================================

void DSaveMenu::DoSave (FSaveGameNode *node)
{
	if (node != &NewSaveNode)
	{
		G_SaveGame (node->Filename.GetChars(), savegamestring);
	}
	else
	{
		// Find an unused filename and save as that
		FString filename;
		int i;
		FILE *test;

		for (i = 0;; ++i)
		{
			filename = G_BuildSaveName ("save", i);
			test = fopen (filename, "rb");
			if (test == NULL)
			{
				break;
			}
			fclose (test);
		}
		G_SaveGame (filename, savegamestring);
	}
	M_ClearMenus();
	BorderNeedRefresh = screen->GetPageCount ();
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
	if (SelSaveGame == NULL || SelSaveGame->Succ == NULL)
	{
		return false;
	}

	if (mkey == MKEY_Enter)
	{
		if (SelSaveGame != &NewSaveNode)
		{
			strcpy (savegamestring, SelSaveGame->Title);
		}
		else
		{
			savegamestring[0] = 0;
		}
		DMenu *input = new DTextEnterMenu(this, savegamestring, SAVESTRINGSIZE, 1, fromcontroller);
		M_ActivateMenu(input);
		mEntering = true;
	}
	else if (mkey == MKEY_Input)
	{
		mEntering = false;
		DoSave(SelSaveGame);
	}
	else if (mkey == MKEY_Abort)
	{
		mEntering = false;
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
		if (SelSaveGame != NULL && SelSaveGame->Succ != NULL)
		{
			switch (ev->data1)
			{
			case GK_DEL:
			case '\b':
				// cannot delete 'new save game' item
				if (SelSaveGame == &NewSaveNode) return true;
				break;

			case 'N':
				SelSaveGame = TopSaveGame = &NewSaveNode;
				UnloadSaveData ();
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

	DLoadMenu(DMenu *parent = NULL, FListMenuDescriptor *desc = NULL);

	bool MenuEvent (int mkey, bool fromcontroller);
};

IMPLEMENT_CLASS(DLoadMenu)


//=============================================================================
//
//
//
//=============================================================================

DLoadMenu::DLoadMenu(DMenu *parent, FListMenuDescriptor *desc)
: DLoadSaveMenu(parent, desc)
{
	TopSaveGame = static_cast<FSaveGameNode *>(SaveGames.Head);
	ExtractSaveData (SelSaveGame);
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
	if (SelSaveGame == NULL || SelSaveGame->Succ == NULL)
	{
		return false;
	}

	if (mkey == MKEY_Enter)
	{
		G_LoadGame (SelSaveGame->Filename.GetChars());
		if (gamestate == GS_FULLCONSOLE)
		{
			gamestate = GS_HIDECONSOLE;
		}
		if (quickSaveSlot == (FSaveGameNode *)1)
		{
			quickSaveSlot = SelSaveGame;
		}
		M_ClearMenus();
		BorderNeedRefresh = screen->GetPageCount ();
		return true;
	}
	return false;
}

