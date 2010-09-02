/*
** menu.cpp
** Menu base class and global interface
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

#include "doomdef.h"
#include "doomstat.h"
#include "c_dispatch.h"
#include "d_gui.h"
#include "d_player.h"
#include "g_level.h"
#include "c_console.h"
#include "s_sound.h"
#include "p_tick.h"
#include "g_game.h"
#include "c_cvars.h"
#include "d_event.h"
#include "v_video.h"
#include "hu_stuff.h"
#include "gi.h"
#include "menu/menu.h"
#include "textures/textures.h"


CVAR (Float, snd_menuvolume, 0.6f, CVAR_ARCHIVE)

DMenu *DMenu::CurrentMenu;
int DMenu::MenuTime;

FListMenuDescriptor *MainMenu;
FGameStartup GameStartupInfo;
EMenuState		menuactive;
bool			M_DemoNoPlay;

//============================================================================
//
// DMenu base class
//
//============================================================================

IMPLEMENT_POINTY_CLASS (DMenu)
	DECLARE_POINTER(mParentMenu)
END_POINTERS

DMenu::DMenu(DMenu *parent) 
{
	mParentMenu = parent;
	GC::WriteBarrier(this, parent);
}
	
bool DMenu::Responder (event_t *ev) 
{ 
	return false; 
}

bool DMenu::MenuEvent (int mkey, bool fromcontroller)
{
	switch (mkey)
	{
	case MKEY_Back:
	{
		Close();
		S_Sound (CHAN_VOICE | CHAN_UI, 
			DMenu::CurrentMenu != NULL? "menu/backup" : "menu/clear", snd_menuvolume, ATTN_NONE);
		return true;
	}
	}
	return false;
}

void DMenu::Close ()
{
	assert(DMenu::CurrentMenu == this);
	DMenu::CurrentMenu = mParentMenu;
	Destroy();
	if (DMenu::CurrentMenu != NULL)
	{
		GC::WriteBarrier(DMenu::CurrentMenu);
	}
	else
	{
		M_ClearMenus ();
	}
}


void DMenu::Ticker () 
{
}

void DMenu::Drawer () 
{
}

bool DMenu::DimAllowed()
{
	return true;
}

bool DMenu::TranslateKeyboardEvents()
{
	return true;
}




void M_StartControlPanel (bool makeSound)
{
	// intro might call this repeatedly
	if (DMenu::CurrentMenu != NULL)
		return;

	C_HideConsole ();				// [RH] Make sure console goes bye bye.
	menuactive = MENU_On;
	// Pause sound effects before we play the menu switch sound.
	// That way, it won't be paused.
	P_CheckTickerPaused ();

	if (makeSound)
	{
		S_Sound (CHAN_VOICE | CHAN_UI, "menu/activate", snd_menuvolume, ATTN_NONE);
	}
}

void M_ActivateMenu(DMenu *menu)
{
	DMenu::CurrentMenu = menu;
	GC::WriteBarrier(DMenu::CurrentMenu);
}

void M_SetMenu(FName menu, int param)
{
	// some menus need some special treatment
	switch (menu)
	{
	case NAME_Episodemenu:
		// sent from the player class menu
		GameStartupInfo.Skill = -1;
		GameStartupInfo.Episode = -1;
		GameStartupInfo.PlayerClass = 
			param == -1? "Random" : PlayerClasses[param].Type->Meta.GetMetaString (APMETA_DisplayName);
		break;

	case NAME_Skillmenu:
		// sent from the episode menu
		GameStartupInfo.Episode = param;
		M_StartupSkillMenu(&GameStartupInfo);	// needs player class name from class menu (later)
		break;

	case NAME_StartgameConfirm:
		// sent from the skill menu
		GameStartupInfo.Skill = param;
		// Todo: 
		break;

	case NAME_Startgame:
		// sent either from skill menu or confirmation screen. Skill gets only set if sent from skill menu
		// Now we can finally start the game. Ugh...
		if (GameStartupInfo.Skill == -1) GameStartupInfo.Skill = param;

		G_DeferedInitNew (&GameStartupInfo);
		if (gamestate == GS_FULLCONSOLE)
		{
			gamestate = GS_HIDECONSOLE;
			gameaction = ga_newgame;
		}
		M_ClearMenus ();
		return;

	case NAME_Savemenu:
		if (!usergame || (players[consoleplayer].health <= 0 && !multiplayer))
		{
			// cannot save outside the game.
			//M_StartMessage (GStrings("SAVEDEAD"), NULL);
			return;
		}

	}

	FMenuDescriptor **desc = MenuDescriptors.CheckKey(menu);
	if (desc != NULL)
	{
		if ((*desc)->mType == MDESC_ListMenu)
		{
			FListMenuDescriptor *ld = static_cast<FListMenuDescriptor*>(*desc);
			if (ld->mAutoselect >= 0 && ld->mAutoselect < (int)ld->mItems.Size())
			{
				// recursively activate the autoselected item without ever creating this menu.
				ld->mItems[ld->mAutoselect]->Activate();
			}
			else
			{
				const PClass *cls = ld->mClass == NULL? RUNTIME_CLASS(DListMenu) : ld->mClass;

				DListMenu *newmenu = (DListMenu *)cls->CreateNew();
				newmenu->Init(DMenu::CurrentMenu, ld);
				M_ActivateMenu(newmenu);
			}
		}
		return;
	}
	else
	{
		const PClass *menuclass = PClass::FindClass(menu);
		if (menuclass != NULL)
		{
			if (menuclass->IsDescendantOf(RUNTIME_CLASS(DMenu)))
			{
				DMenu *newmenu = (DMenu*)menuclass->CreateNew();
				newmenu->mParentMenu = DMenu::CurrentMenu;
				M_ActivateMenu(newmenu);
				return;
			}
		}
	}
	Printf("Attempting to open menu of unknown type '%s'\n", menu.GetChars());
}

bool M_Responder (event_t *ev) 
{ 
	if (chatmodeon)
	{
		return false;
	}

	if (DMenu::CurrentMenu != NULL && menuactive == MENU_On) 
	{
		if (ev->type == EV_GUI_Event && ev->subtype == EV_GUI_KeyDown &&
			DMenu::CurrentMenu->TranslateKeyboardEvents())
		{
			// improve this later. For now it just has to work
			int mkey = -1;
			bool fromcontroller = false;
			switch (ev->data1)
			{
				case GK_ESCAPE:			mkey = MKEY_Back;		break;
				case GK_RETURN:			mkey = MKEY_Enter;		break;
				case GK_UP:				mkey = MKEY_Up;			break;
				case GK_DOWN:			mkey = MKEY_Down;		break;
				case GK_LEFT:			mkey = MKEY_Left;		break;
				case GK_RIGHT:			mkey = MKEY_Right;		break;
				case GK_BACKSPACE:		mkey = MKEY_Clear;		break;
				case GK_PGUP:			mkey = MKEY_PageUp;		break;
				case GK_PGDN:			mkey = MKEY_PageDown;	break;
				case GK_HOME:			mkey = MKEY_Enter; fromcontroller = true;	break; // for testing the input grid
			}
			if (mkey != -1)
			{
				return DMenu::CurrentMenu->MenuEvent(mkey, fromcontroller);
			}
		}

		return DMenu::CurrentMenu->Responder(ev);
	}
	else
	{
		if (ev->type == EV_KeyDown)
		{
			// Pop-up menu?
			if (ev->data1 == KEY_ESCAPE)
			{
				M_StartControlPanel(true);
				M_SetMenu(NAME_Mainmenu, -1);
				return true;
			}
			// If devparm is set, pressing F1 always takes a screenshot no matter
			// what it's bound to. (for those who don't bother to read the docs)
			if (devparm && ev->data1 == KEY_F1)
			{
				G_ScreenShot(NULL);
				return true;
			}
			return false;
		}
	}
	return false;
}

void M_Ticker (void) 
{
	DMenu::MenuTime++;
	if (DMenu::CurrentMenu != NULL && menuactive == MENU_On) 
		DMenu::CurrentMenu->Ticker();
}

void M_Drawer (void) 
{
	player_t *player = &players[consoleplayer];
	AActor *camera = player->camera;
	PalEntry fade = 0;

	if (!screen->Accel2D && camera != NULL && (gamestate == GS_LEVEL || gamestate == GS_TITLELEVEL))
	{
		if (camera->player != NULL)
		{
			player = camera->player;
		}
		fade = PalEntry (BYTE(player->BlendA*255), BYTE(player->BlendR*255), BYTE(player->BlendG*255), BYTE(player->BlendB*255));
	}


	if (DMenu::CurrentMenu != NULL && menuactive == MENU_On) 
	{
		//if (DMenu::CurrentMenu::DimAllowed()) screen->Dim(fade);
		DMenu::CurrentMenu->Drawer();
	}
}

void M_ClearMenus ()
{
	/*
	M_DeactivateMenuInput ();
	InfoType = 0;
	*/
	M_DemoNoPlay = false;
	if (DMenu::CurrentMenu != NULL)
	{
		DMenu::CurrentMenu->Destroy();
		DMenu::CurrentMenu = NULL;
	}
	BorderNeedRefresh = screen->GetPageCount ();
	menuactive = MENU_Off;
}

void M_Init (void) 
{
	M_ParseMenuDefs();
}


// CODE --------------------------------------------------------------------

// [RH] Most menus can now be accessed directly
// through console commands.
CCMD (menu_main)
{
	M_StartControlPanel(true);
	M_SetMenu(NAME_Mainmenu, -1);
}

CCMD (menu_load)
{	// F3
	M_StartControlPanel (true);
	M_SetMenu(NAME_Loadgamemenu, -1);
}

CCMD (menu_save)
{	// F2
	M_StartControlPanel (true);
	M_SetMenu(NAME_Savegamemenu, -1);
}

CCMD (menu_help)
{	// F1
	M_StartControlPanel (true);
	M_SetMenu(NAME_Readthismenu, -1);
}

CCMD (menu_quit)
{	// F10
	//M_StartControlPanel (true);
	S_Sound (CHAN_VOICE | CHAN_UI, "menu/activate", snd_menuvolume, ATTN_NONE);
	M_SetMenu(NAME_Quitmenu, -1);
}

CCMD (menu_game)
{
	M_StartControlPanel (true);
	M_SetMenu(NAME_Playerclassmenu, -1);	// The playerclass menu is the first in the 'start game' chain
}
								
CCMD (menu_options)
{
	M_StartControlPanel (true);
	M_SetMenu(NAME_Optionsmenu, -1);
}

CCMD (menu_player)
{
	M_StartControlPanel (true);
	M_SetMenu(NAME_Playermenu, -1);
}


