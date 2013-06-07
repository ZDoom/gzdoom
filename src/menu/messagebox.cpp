/*
** messagebox.cpp
** Confirmation, notification screns
**
**---------------------------------------------------------------------------
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
#include "d_event.h"
#include "d_gui.h"
#include "v_video.h"
#include "v_text.h"
#include "d_main.h"
#include "gstrings.h"
#include "gi.h"
#include "i_video.h"
#include "st_start.h"
#include "c_dispatch.h"
#include "g_game.h"


extern FSaveGameNode *quickSaveSlot;

class DMessageBoxMenu : public DMenu
{
	DECLARE_CLASS(DMessageBoxMenu, DMenu)

	FBrokenLines *mMessage;
	int mMessageMode;
	int messageSelection;
	int mMouseLeft, mMouseRight, mMouseY;
	FName mAction;

public:

	DMessageBoxMenu(DMenu *parent = NULL, const char *message = NULL, int messagemode = 0, bool playsound = false, FName action = NAME_None);
	void Destroy();
	void Init(DMenu *parent, const char *message, int messagemode, bool playsound = false);
	void Drawer();
	bool Responder(event_t *ev);
	bool MenuEvent(int mkey, bool fromcontroller);
	bool MouseEvent(int type, int x, int y);
	void CloseSound();
	virtual void HandleResult(bool res);
};

IMPLEMENT_CLASS(DMessageBoxMenu)

//=============================================================================
//
//
//
//=============================================================================

DMessageBoxMenu::DMessageBoxMenu(DMenu *parent, const char *message, int messagemode, bool playsound, FName action)
: DMenu(parent)
{
	mAction = action;
	messageSelection = 0;
	mMouseLeft = 140;
	mMouseY = INT_MIN;
	int mr1 = 170 + SmallFont->StringWidth(GStrings["TXT_YES"]);
	int mr2 = 170 + SmallFont->StringWidth(GStrings["TXT_NO"]);
	mMouseRight = MAX(mr1, mr2);

	Init(parent, message, messagemode, playsound);
}

//=============================================================================
//
//
//
//=============================================================================

void DMessageBoxMenu::Init(DMenu *parent, const char *message, int messagemode, bool playsound)
{
	mParentMenu = parent;
	if (message != NULL) 
	{
		if (*message == '$') message = GStrings(message+1);
		mMessage = V_BreakLines(SmallFont, 300, message);
	}
	else mMessage = NULL;
	mMessageMode = messagemode;
	if (playsound)
	{
		S_StopSound (CHAN_VOICE);
		S_Sound (CHAN_VOICE | CHAN_UI, "menu/prompt", snd_menuvolume, ATTN_NONE);
	}
}

//=============================================================================
//
//
//
//=============================================================================

void DMessageBoxMenu::Destroy()
{
	if (mMessage != NULL) V_FreeBrokenLines(mMessage);
	mMessage = NULL;
}

//=============================================================================
//
//
//
//=============================================================================

void DMessageBoxMenu::CloseSound()
{
	S_Sound (CHAN_VOICE | CHAN_UI, 
		DMenu::CurrentMenu != NULL? "menu/backup" : "menu/dismiss", snd_menuvolume, ATTN_NONE);
}

//=============================================================================
//
//
//
//=============================================================================

void DMessageBoxMenu::HandleResult(bool res)
{
	if (mParentMenu != NULL)
	{
		if (mMessageMode == 0)
		{
			if (mAction == NAME_None) 
			{
				mParentMenu->MenuEvent(res? MKEY_MBYes : MKEY_MBNo, false);
				Close();
			}
			else 
			{
				Close();
				if (res) M_SetMenu(mAction, -1);
			}
			CloseSound();
		}
	}
}

//=============================================================================
//
//
//
//=============================================================================

void DMessageBoxMenu::Drawer ()
{
	int i, y;
	PalEntry fade = 0;

	int fontheight = SmallFont->GetHeight();
	//V_SetBorderNeedRefresh();
	//ST_SetNeedRefresh();

	y = 100;

	if (mMessage != NULL)
	{
		for (i = 0; mMessage[i].Width >= 0; i++)
			y -= SmallFont->GetHeight () / 2;

		for (i = 0; mMessage[i].Width >= 0; i++)
		{
			screen->DrawText (SmallFont, CR_UNTRANSLATED, 160 - mMessage[i].Width/2, y, mMessage[i].Text,
				DTA_Clean, true, TAG_DONE);
			y += fontheight;
		}
	}

	if (mMessageMode == 0)
	{
		y += fontheight;
		mMouseY = y;
		screen->DrawText(SmallFont, 
			messageSelection == 0? OptionSettings.mFontColorSelection : OptionSettings.mFontColor, 
			160, y, GStrings["TXT_YES"], DTA_Clean, true, TAG_DONE);
		screen->DrawText(SmallFont, 
			messageSelection == 1? OptionSettings.mFontColorSelection : OptionSettings.mFontColor, 
			160, y + fontheight + 1, GStrings["TXT_NO"], DTA_Clean, true, TAG_DONE);

		if (messageSelection >= 0)
		{
			if ((DMenu::MenuTime%8) < 6)
			{
				screen->DrawText(ConFont, OptionSettings.mFontColorSelection,
					(150 - 160) * CleanXfac + screen->GetWidth() / 2,
					(y + (fontheight + 1) * messageSelection - 100 + fontheight/2 - 5) * CleanYfac + screen->GetHeight() / 2,
					"\xd",
					DTA_CellX, 8 * CleanXfac,
					DTA_CellY, 8 * CleanYfac,
					TAG_DONE);
			}
		}
	}
}

//=============================================================================
//
//
//
//=============================================================================

bool DMessageBoxMenu::Responder(event_t *ev)
{
	if (ev->type == EV_GUI_Event && ev->subtype == EV_GUI_KeyDown)
	{
		if (mMessageMode == 0)
		{
			int ch = tolower(ev->data1);
			if (ch == 'n' || ch == ' ') 
			{
				HandleResult(false);		
				return true;
			}
			else if (ch == 'y') 
			{
				HandleResult(true);
				return true;
			}
		}
		else
		{
			Close();
			return true;
		}
		return false;
	}
	else if (ev->type == EV_KeyDown)
	{
		Close();
		return true;
	}
	return Super::Responder(ev);
}

//=============================================================================
//
//
//
//=============================================================================

bool DMessageBoxMenu::MenuEvent(int mkey, bool fromcontroller)
{
	if (mMessageMode == 0)
	{
		if (mkey == MKEY_Up || mkey == MKEY_Down)
		{
			S_Sound (CHAN_VOICE | CHAN_UI, "menu/cursor", snd_menuvolume, ATTN_NONE);
			messageSelection = !messageSelection;
			return true;
		}
		else if (mkey == MKEY_Enter)
		{
			// 0 is yes, 1 is no
			HandleResult(!messageSelection);
			return true;
		}
		else if (mkey == MKEY_Back)
		{
			HandleResult(false);
			return true;
		}
		return false;
	}
	else
	{
		Close();
		CloseSound();
		return true;
	}
}

//=============================================================================
//
//
//
//=============================================================================

bool DMessageBoxMenu::MouseEvent(int type, int x, int y)
{
	if (mMessageMode == 1)
	{
		if (type == MOUSE_Click)
		{
			return MenuEvent(MKEY_Enter, true);
		}
		return false;
	}
	else
	{
		int sel = -1;
		int fh = SmallFont->GetHeight() + 1;

		// convert x/y from screen to virtual coordinates, according to CleanX/Yfac use in DrawTexture
		x = ((x - (screen->GetWidth() / 2)) / CleanXfac) + 160;
		y = ((y - (screen->GetHeight() / 2)) / CleanYfac) + 100;

		if (x >= mMouseLeft && x <= mMouseRight && y >= mMouseY && y < mMouseY + 2 * fh)
		{
			sel = y >= mMouseY + fh;
		}
		if (sel != -1 && sel != messageSelection)
		{
			//S_Sound (CHAN_VOICE | CHAN_UI, "menu/cursor", snd_menuvolume, ATTN_NONE);
		}
		messageSelection = sel;
		if (type == MOUSE_Release)
		{
			return MenuEvent(MKEY_Enter, true);
		}
		return true;
	}
}

//=============================================================================
//
//
//
//=============================================================================
//=============================================================================
//
//
//
//=============================================================================

class DQuitMenu : public DMessageBoxMenu
{
	DECLARE_CLASS(DQuitMenu, DMessageBoxMenu)

public:

	DQuitMenu(bool playsound = false);
	virtual void HandleResult(bool res);
};

IMPLEMENT_CLASS(DQuitMenu)

//=============================================================================
//
//
//
//=============================================================================

DQuitMenu::DQuitMenu(bool playsound)
{
	int messageindex = gametic % gameinfo.quitmessages.Size();
	FString EndString;
	const char *msg = gameinfo.quitmessages[messageindex];
	if (msg[0] == '$') 
	{
		if (msg[1] == '*')
		{
			EndString = GStrings(msg+2);
		}
		else
		{
			EndString.Format("%s\n\n%s", GStrings(msg+1), GStrings("DOSY"));
		}
	}
	else EndString = gameinfo.quitmessages[messageindex];

	Init(NULL, EndString, 0, playsound);
}

//=============================================================================
//
//
//
//=============================================================================

void DQuitMenu::HandleResult(bool res)
{
	if (res)
	{
		if (!netgame)
		{
			if (gameinfo.quitSound.IsNotEmpty())
			{
				S_Sound (CHAN_VOICE | CHAN_UI, gameinfo.quitSound, snd_menuvolume, ATTN_NONE);
				I_WaitVBL (105);
			}
		}
		ST_Endoom();
	}
	else
	{
		Close();
		CloseSound();
	}
}

//=============================================================================
//
//
//
//=============================================================================

CCMD (menu_quit)
{	// F10
	M_StartControlPanel (true);
	DMenu *newmenu = new DQuitMenu(false);
	newmenu->mParentMenu = DMenu::CurrentMenu;
	M_ActivateMenu(newmenu);
}



//=============================================================================
//
//
//
//=============================================================================
//=============================================================================
//
//
//
//=============================================================================

class DEndGameMenu : public DMessageBoxMenu
{
	DECLARE_CLASS(DEndGameMenu, DMessageBoxMenu)

public:

	DEndGameMenu(bool playsound = false);
	virtual void HandleResult(bool res);
};

IMPLEMENT_CLASS(DEndGameMenu)

//=============================================================================
//
//
//
//=============================================================================

DEndGameMenu::DEndGameMenu(bool playsound)
{
	Init(NULL, GStrings(netgame ? "NETEND" : "ENDGAME"), 0, playsound);
}

//=============================================================================
//
//
//
//=============================================================================

void DEndGameMenu::HandleResult(bool res)
{
	if (res)
	{
		M_ClearMenus ();
		if (!netgame)
		{
			D_StartTitle ();
		}
	}
	else
	{
		Close();
		CloseSound();
	}
}

//=============================================================================
//
//
//
//=============================================================================

CCMD (menu_endgame)
{	// F7
	if (!usergame)
	{
		S_Sound (CHAN_VOICE | CHAN_UI, "menu/invalid", snd_menuvolume, ATTN_NONE);
		return;
	}
		
	//M_StartControlPanel (true);
	S_Sound (CHAN_VOICE | CHAN_UI, "menu/activate", snd_menuvolume, ATTN_NONE);
	DMenu *newmenu = new DEndGameMenu(false);
	newmenu->mParentMenu = DMenu::CurrentMenu;
	M_ActivateMenu(newmenu);
}

//=============================================================================
//
//
//
//=============================================================================
//=============================================================================
//
//
//
//=============================================================================

class DQuickSaveMenu : public DMessageBoxMenu
{
	DECLARE_CLASS(DQuickSaveMenu, DMessageBoxMenu)

public:

	DQuickSaveMenu(bool playsound = false);
	virtual void HandleResult(bool res);
};

IMPLEMENT_CLASS(DQuickSaveMenu)

//=============================================================================
//
//
//
//=============================================================================

DQuickSaveMenu::DQuickSaveMenu(bool playsound)
{
	FString tempstring;

	tempstring.Format(GStrings("QSPROMPT"), quickSaveSlot->Title);
	Init(NULL, tempstring, 0, playsound);
}

//=============================================================================
//
//
//
//=============================================================================

void DQuickSaveMenu::HandleResult(bool res)
{
	if (res)
	{
		G_SaveGame (quickSaveSlot->Filename.GetChars(), quickSaveSlot->Title);
		S_Sound (CHAN_VOICE | CHAN_UI, "menu/dismiss", snd_menuvolume, ATTN_NONE);
		M_ClearMenus();
	}
	else
	{
		Close();
		CloseSound();
	}
}

//=============================================================================
//
//
//
//=============================================================================

CCMD (quicksave)
{	// F6
	if (!usergame || (players[consoleplayer].health <= 0 && !multiplayer))
	{
		S_Sound (CHAN_VOICE | CHAN_UI, "menu/invalid", snd_menuvolume, ATTN_NONE);
		return;
	}

	if (gamestate != GS_LEVEL)
		return;
		
	S_Sound (CHAN_VOICE | CHAN_UI, "menu/activate", snd_menuvolume, ATTN_NONE);
	if (quickSaveSlot == NULL)
	{
		M_StartControlPanel(false);
		M_SetMenu(NAME_Savegamemenu);
		return;
	}
	DMenu *newmenu = new DQuickSaveMenu(false);
	newmenu->mParentMenu = DMenu::CurrentMenu;
	M_ActivateMenu(newmenu);
}

//=============================================================================
//
//
//
//=============================================================================
//=============================================================================
//
//
//
//=============================================================================

class DQuickLoadMenu : public DMessageBoxMenu
{
	DECLARE_CLASS(DQuickLoadMenu, DMessageBoxMenu)

public:

	DQuickLoadMenu(bool playsound = false);
	virtual void HandleResult(bool res);
};

IMPLEMENT_CLASS(DQuickLoadMenu)

//=============================================================================
//
//
//
//=============================================================================

DQuickLoadMenu::DQuickLoadMenu(bool playsound)
{
	FString tempstring;

	tempstring.Format(GStrings("QLPROMPT"), quickSaveSlot->Title);
	Init(NULL, tempstring, 0, playsound);
}

//=============================================================================
//
//
//
//=============================================================================

void DQuickLoadMenu::HandleResult(bool res)
{
	if (res)
	{
		G_LoadGame (quickSaveSlot->Filename.GetChars());
		S_Sound (CHAN_VOICE | CHAN_UI, "menu/dismiss", snd_menuvolume, ATTN_NONE);
		M_ClearMenus();
	}
	else
	{
		Close();
		CloseSound();
	}
}

//=============================================================================
//
//
//
//=============================================================================

CCMD (quickload)
{	// F9
	M_StartControlPanel (true);

	if (netgame)
	{
		M_StartMessage (GStrings("QLOADNET"), 1);
		return;
	}
		
	if (quickSaveSlot == NULL)
	{
		M_StartControlPanel(false);
		// signal that whatever gets loaded should be the new quicksave
		quickSaveSlot = (FSaveGameNode *)1;
		M_SetMenu(NAME_Loadgamemenu);
		return;
	}
	DMenu *newmenu = new DQuickLoadMenu(false);
	newmenu->mParentMenu = DMenu::CurrentMenu;
	M_ActivateMenu(newmenu);
}

//=============================================================================
//
//
//
//=============================================================================

void M_StartMessage(const char *message, int messagemode, FName action)
{
	if (DMenu::CurrentMenu == NULL) 
	{
		// only play a sound if no menu was active before
		M_StartControlPanel(menuactive == MENU_Off);
	}
	DMenu *newmenu = new DMessageBoxMenu(DMenu::CurrentMenu, message, messagemode, false, action);
	newmenu->mParentMenu = DMenu::CurrentMenu;
	M_ActivateMenu(newmenu);
}

