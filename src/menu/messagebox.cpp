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

#include <ctype.h>
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


class DBrokenLines : public DObject
{
	DECLARE_ABSTRACT_CLASS(DBrokenLines, DObject)

public:
	FBrokenLines *mBroken;
	unsigned int mCount;

	DBrokenLines(FBrokenLines *broken, unsigned int count)
	{
		mBroken = broken;
		mCount = count;
	}

	void OnDestroy() override
	{
		V_FreeBrokenLines(mBroken);
	}
};


EXTERN_CVAR (Bool, saveloadconfirmation) // [mxd]

class DMessageBoxMenu : public DMenu
{
	DECLARE_CLASS(DMessageBoxMenu, DMenu)
public:
	DBrokenLines *mMessage;
	int mMessageMode;
	int messageSelection;
	int mMouseLeft, mMouseRight, mMouseY;
	FName mAction;
	void (*Handler)();

public:

	DMessageBoxMenu(DMenu *parent = NULL, const char *message = NULL, int messagemode = 0, bool playsound = false, FName action = NAME_None, void (*hnd)() = nullptr);
	void Init(DMenu *parent, const char *message, int messagemode, bool playsound = false, void(*hnd)() = nullptr);
};

IMPLEMENT_CLASS(DMessageBoxMenu, false, false)

//=============================================================================
//
//
//
//=============================================================================

DMessageBoxMenu::DMessageBoxMenu(DMenu *parent, const char *message, int messagemode, bool playsound, FName action, void (*hnd)())
: DMenu(parent)
{
	mAction = action;
	messageSelection = 0;
	mMouseLeft = 140;
	mMouseY = INT_MIN;
	int mr1 = 170 + SmallFont->StringWidth(GStrings["TXT_YES"]);
	int mr2 = 170 + SmallFont->StringWidth(GStrings["TXT_NO"]);
	mMouseRight = MAX(mr1, mr2);

	Init(parent, message, messagemode, playsound, hnd);
}

//=============================================================================
//
//
//
//=============================================================================

void DMessageBoxMenu::Init(DMenu *parent, const char *message, int messagemode, bool playsound, void (*hnd)())
{
	mParentMenu = parent;
	if (message != NULL) 
	{
		if (*message == '$') message = GStrings(message+1);
		unsigned count;
		auto Message = V_BreakLines(SmallFont, 300, message, true, &count);
		mMessage = new DBrokenLines(Message, count);
		GC::WriteBarrier(mMessage);
		mMessage->ObjectFlags |= OF_Fixed;
	}
	else mMessage = NULL;
	mMessageMode = messagemode;
	if (playsound)
	{
		S_StopSound (CHAN_VOICE);
		S_Sound (CHAN_VOICE | CHAN_UI, "menu/prompt", snd_menuvolume, ATTN_NONE);
	}
	Handler = hnd;
}

typedef void(*hfunc)();
DEFINE_ACTION_FUNCTION(DMessageBoxMenu, CallHandler)
{
	PARAM_PROLOGUE;
	PARAM_POINTERTYPE(Handler, hfunc);
	Handler();
	return 0;
}

//=============================================================================
//
//
//
//=============================================================================

CCMD (menu_quit)
{	// F10
	M_StartControlPanel (true);

	int messageindex = gametic % gameinfo.quitmessages.Size();
	FString EndString;
	const char *msg = gameinfo.quitmessages[messageindex];
	if (msg[0] == '$')
	{
		if (msg[1] == '*')
		{
			EndString = GStrings(msg + 2);
		}
		else
		{
			EndString.Format("%s\n\n%s", GStrings(msg + 1), GStrings("DOSY"));
		}
	}
	else EndString = gameinfo.quitmessages[messageindex];

	DMenu *newmenu = new DMessageBoxMenu(CurrentMenu, EndString, 0, false, NAME_None, []()
	{
		if (!netgame)
		{
			if (gameinfo.quitSound.IsNotEmpty())
			{
				S_Sound(CHAN_VOICE | CHAN_UI, gameinfo.quitSound, snd_menuvolume, ATTN_NONE);
				I_WaitVBL(105);
			}
		}
		ST_Endoom();
	});


	M_ActivateMenu(newmenu);
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

	FString tempstring = GStrings(netgame ? "NETEND" : "ENDGAME");
	DMenu *newmenu = new DMessageBoxMenu(CurrentMenu, tempstring, 0, false, NAME_None, []()
	{
		M_ClearMenus();
		if (!netgame)
		{
			D_StartTitle();
		}
	});

	M_ActivateMenu(newmenu);
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
		
	if (savegameManager.quickSaveSlot == NULL)
	{
		S_Sound(CHAN_VOICE | CHAN_UI, "menu/activate", snd_menuvolume, ATTN_NONE);
		M_StartControlPanel(false);
		M_SetMenu(NAME_Savegamemenu);
		return;
	}
	
	// [mxd]. Just save the game, no questions asked.
	if (!saveloadconfirmation)
	{
		G_SaveGame(savegameManager.quickSaveSlot->Filename.GetChars(), savegameManager.quickSaveSlot->SaveTitle.GetChars());
		return;
	}

	S_Sound(CHAN_VOICE | CHAN_UI, "menu/activate", snd_menuvolume, ATTN_NONE);

	FString tempstring;
	tempstring.Format(GStrings("QSPROMPT"), savegameManager.quickSaveSlot->SaveTitle.GetChars());

	DMenu *newmenu = new DMessageBoxMenu(CurrentMenu, tempstring, 0, false, NAME_None, []()
	{
		G_SaveGame(savegameManager.quickSaveSlot->Filename.GetChars(), savegameManager.quickSaveSlot->SaveTitle.GetChars());
		S_Sound(CHAN_VOICE | CHAN_UI, "menu/dismiss", snd_menuvolume, ATTN_NONE);
		M_ClearMenus();
	});

	M_ActivateMenu(newmenu);
}

//=============================================================================
//
//
//
//=============================================================================

CCMD (quickload)
{	// F9
	if (netgame)
	{
		M_StartControlPanel(true);
		M_StartMessage (GStrings("QLOADNET"), 1);
		return;
	}
		
	if (savegameManager.quickSaveSlot == NULL)
	{
		M_StartControlPanel(true);
		// signal that whatever gets loaded should be the new quicksave
		savegameManager.quickSaveSlot = (FSaveGameNode *)1;
		M_SetMenu(NAME_Loadgamemenu);
		return;
	}

	// [mxd]. Just load the game, no questions asked.
	if (!saveloadconfirmation)
	{
		G_LoadGame(savegameManager.quickSaveSlot->Filename.GetChars());
		return;
	}
	FString tempstring;
	tempstring.Format(GStrings("QLPROMPT"), savegameManager.quickSaveSlot->SaveTitle.GetChars());

	M_StartControlPanel(true);

	DMenu *newmenu = new DMessageBoxMenu(CurrentMenu, tempstring, 0, false, NAME_None, []()
	{
		G_LoadGame(savegameManager.quickSaveSlot->Filename.GetChars());
		S_Sound(CHAN_VOICE | CHAN_UI, "menu/dismiss", snd_menuvolume, ATTN_NONE);
		M_ClearMenus();
	});
	M_ActivateMenu(newmenu);
}

//=============================================================================
//
//
//
//=============================================================================

void M_StartMessage(const char *message, int messagemode, FName action)
{
	if (CurrentMenu == NULL) 
	{
		// only play a sound if no menu was active before
		M_StartControlPanel(menuactive == MENU_Off);
	}
	DMenu *newmenu = new DMessageBoxMenu(CurrentMenu, message, messagemode, false, action);
	newmenu->mParentMenu = CurrentMenu;
	M_ActivateMenu(newmenu);
}

DEFINE_ACTION_FUNCTION(DMenu, StartMessage)
{
	PARAM_PROLOGUE;
	PARAM_STRING(msg);
	PARAM_INT(mode);
	PARAM_NAME_DEF(action);
	M_StartMessage(msg, mode, action);
	return 0;
}


DEFINE_FIELD(DMessageBoxMenu, mMessageMode);
DEFINE_FIELD(DMessageBoxMenu, messageSelection);
DEFINE_FIELD(DMessageBoxMenu, mMouseLeft);
DEFINE_FIELD(DMessageBoxMenu, mMouseRight); 
DEFINE_FIELD(DMessageBoxMenu, mMouseY);
DEFINE_FIELD(DMessageBoxMenu, mAction);
DEFINE_FIELD(DMessageBoxMenu, Handler);
DEFINE_FIELD(DMessageBoxMenu, mMessage);
