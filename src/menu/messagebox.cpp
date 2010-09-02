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

class DMessageBoxMenu : public DMenu
{
	DECLARE_CLASS(DMessageBoxMenu, DMenu)

	FBrokenLines *mMessage;
	int mMessageMode;
	int messageSelection;

public:

	DMessageBoxMenu(DMenu *parent = NULL, const char *message = NULL, int messagemode = 0, bool playsound = false);
	void Destroy();
	void Init(DMenu *parent, const char *message, int messagemode, bool playsound = false);
	void Drawer();
	bool Responder(event_t *ev);
	bool MenuEvent(int mkey, bool fromcontroller);
	virtual void HandleResult(bool res);
};

IMPLEMENT_CLASS(DMessageBoxMenu)

//=============================================================================
//
//
//
//=============================================================================

DMessageBoxMenu::DMessageBoxMenu(DMenu *parent, const char *message, int messagemode, bool playsound)
: DMenu(parent)
{
	messageSelection = 0;
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
	if (message != NULL) mMessage = V_BreakLines(SmallFont, 300, message);
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

void DMessageBoxMenu::HandleResult(bool res)
{
	if (mParentMenu != NULL)
	{
		if (mMessageMode == 0)
		{
			mParentMenu->MenuEvent(res? MKEY_MBYes : MKEY_MBNo, false);
			Close();
			S_Sound(CHAN_VOICE | CHAN_UI, "menu/dismiss", snd_menuvolume, ATTN_NONE);
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
	//BorderNeedRefresh = screen->GetPageCount ();
	//SB_state = screen->GetPageCount ();

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
		screen->DrawText(SmallFont, CR_UNTRANSLATED, 160, y, GStrings["TXT_YES"], DTA_Clean, true, TAG_DONE);
		screen->DrawText(SmallFont, CR_UNTRANSLATED, 160, y + fontheight + 1, GStrings["TXT_NO"], DTA_Clean, true, TAG_DONE);

#if 0
		if ((DMenu::MenuTime%8) < 6)
		{
			screen->DrawText(ConFont, CR_RED,
				(150 - 160) * CleanXfac + screen->GetWidth() / 2,
				(y + (fontheight + 1) * messageSelection - 100) * CleanYfac + screen->GetHeight() / 2,
				"\xd",
				DTA_CellX, 8 * CleanXfac,
				DTA_CellY, 8 * CleanYfac,
				TAG_DONE);
		}
#else
		int color = (DMenu::MenuTime%8) < 4? CR_RED:CR_GREY;

		screen->DrawText(ConFont, color,
			(150 - 160) * CleanXfac + screen->GetWidth() / 2,
			(y + (fontheight + 1) * messageSelection - 100) * CleanYfac + screen->GetHeight() / 2,
			"\xd",
			DTA_CellX, 8 * CleanXfac,
			DTA_CellY, 8 * CleanYfac,
			TAG_DONE);
#endif
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
	return false;
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
		return true;
	}
}

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
		S_Sound(CHAN_VOICE | CHAN_UI, "menu/dismiss", snd_menuvolume, ATTN_NONE);
	}
}


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
	int messageindex = gametic % gameinfo.quitmessages.Size();
	FString EndString = gameinfo.quitmessages[messageindex];

	if (netgame)
	{
		if(gameinfo.gametype == GAME_Chex)
			EndString = GStrings("CNETEND");
		else
			EndString = GStrings("NETEND");
		return;
	}

	if(gameinfo.gametype == GAME_Chex)
		EndString = GStrings("CENDGAME");
	else
		EndString = GStrings("ENDGAME");


	Init(NULL, EndString, 0, playsound);
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
		D_StartTitle ();
	}
	else
	{
		Close();
		S_Sound(CHAN_VOICE | CHAN_UI, "menu/dismiss", snd_menuvolume, ATTN_NONE);
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


CCMD (menu_endgame)
{	// F7
	if (!usergame)
	{
		S_Sound (CHAN_VOICE | CHAN_UI, "menu/invalid", snd_menuvolume, ATTN_NONE);
		return;
	}
		
	M_StartControlPanel (true);
	DMenu *newmenu = new DEndGameMenu(false);
	newmenu->mParentMenu = DMenu::CurrentMenu;
	M_ActivateMenu(newmenu);
}

