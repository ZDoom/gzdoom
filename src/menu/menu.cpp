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
#include "d_gui.h"
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

FListMenuDescriptor *MainMenu;

void M_ClearMenus ();


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
}
	
DMenu::~DMenu() 
{
}

bool DMenu::Responder (event_t *ev) 
{ 
	return false; 
}

bool DMenu::MenuEvent (int mkey)
{
	switch (mkey)
	{
	case MKEY_Back:
	{
		DMenu *thismenu = DMenu::CurrentMenu;
		DMenu::CurrentMenu = DMenu::CurrentMenu->mParentMenu;
		if (DMenu::CurrentMenu != NULL)
		{
			GC::WriteBarrier(DMenu::CurrentMenu);
			S_Sound (CHAN_VOICE | CHAN_UI, "menu/backup", snd_menuvolume, ATTN_NONE);
		}
		else
		{
			M_ClearMenus ();
			S_Sound (CHAN_VOICE | CHAN_UI, "menu/clear", snd_menuvolume, ATTN_NONE);
		}
		thismenu->Destroy();
		return true;
	}
	}
	return false;
}


void DMenu::Ticker () 
{
}

void DMenu::Drawer () 
{
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

void M_SetMenu(FName menu)
{
	FMenuDescriptor **desc = MenuDescriptors.CheckKey(menu);
	if (desc != NULL)
	{
		if ((*desc)->mType == MDESC_ListMenu)
		{
			DMenu *newmenu = new DListMenu(NULL, static_cast<FListMenuDescriptor*>(*desc));
			newmenu->mParentMenu = DMenu::CurrentMenu;
			DMenu::CurrentMenu = newmenu;
			GC::WriteBarrier(DMenu::CurrentMenu);
			GC::WriteBarrier(DMenu::CurrentMenu, DMenu::CurrentMenu->mParentMenu);
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
				// todo
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
		if (ev->type == EV_KeyDown)
		{
			// improve this later. For now it just has to work
			int mkey = -1;
			switch (ev->data1)
			{
				case KEY_ESCAPE:			mkey = MKEY_Back;		break;
				case KEY_ENTER:				mkey = MKEY_Enter;		break;
				case KEY_UPARROW:			mkey = MKEY_Up;			break;
				case KEY_DOWNARROW:			mkey = MKEY_Down;		break;
				case KEY_LEFTARROW:			mkey = MKEY_Left;		break;
				case KEY_RIGHTARROW:		mkey = MKEY_Right;		break;
				case KEY_BACKSPACE:			mkey = MKEY_Clear;		break;
				case KEY_PGUP:				mkey = MKEY_PageUp;		break;
				case KEY_PGDN:				mkey = MKEY_PageDown;	break;
			}
			if (mkey != -1)
			{
				return DMenu::CurrentMenu->MenuEvent(mkey);
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
				M_SetMenu(gameinfo.mainmenu);
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
	if (DMenu::CurrentMenu != NULL && menuactive == MENU_On) 
		DMenu::CurrentMenu->Ticker();
}

void M_Drawer (void) 
{
	if (DMenu::CurrentMenu != NULL && menuactive == MENU_On) 
		DMenu::CurrentMenu->Drawer();
}

void M_ClearMenus ()
{
	/*
	M_ClearSaveStuff ();
	M_DeactivateMenuInput ();
	MenuStackDepth = 0;
	OptionsActive = false;
	InfoType = 0;
	drawSkull = true;
	M_DemoNoPlay = false;
	*/
	BorderNeedRefresh = screen->GetPageCount ();
	menuactive = MENU_Off;
}

void M_Init (void) 
{
	M_ParseMenuDefs();
}


