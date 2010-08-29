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
#include "c_console.h"
#include "s_sound.h"
#include "p_tick.h"
#include "g_game.h"
#include "c_cvars.h"
#include "d_event.h"
#include "hu_stuff.h"
#include "menu/menu.h"
#include "textures/textures.h"


CVAR (Float, snd_menuvolume, 0.6f, CVAR_ARCHIVE)

DMenu *DMenu::CurrentMenu;

FListMenuDescriptor *MainMenu;


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

void M_SetMenu()
{
	DMenu::CurrentMenu = new DListMenu(NULL, MainMenu);
	GC::WriteBarrier(DMenu::CurrentMenu);
}

bool M_Responder (event_t *ev) 
{ 
if (MainMenu == NULL)
{
	FListMenuItem *it;

	// we need to create ourselves a dummy main menu here until the definition system is ready.
	MainMenu = new FListMenuDescriptor;
	MainMenu->mMenuName = "Mainmenu";
	MainMenu->mSelectedItem = 0;
	MainMenu->mDisplayTop = 0;

	it = new FListMenuItemStaticPatch(94, 2, TexMan.CheckForTexture("M_DOOM", FTexture::TEX_MiscPatch));
	MainMenu->mItems.Push(it);
	it = new FListMenuItemPatch(97, 64, 'n', TexMan.CheckForTexture("M_NGAME", FTexture::TEX_MiscPatch), NAME_Startgame);
	MainMenu->mItems.Push(it);
	it = new FListMenuItemPatch(97, 80, 'l', TexMan.CheckForTexture("M_LOADG", FTexture::TEX_MiscPatch), "LoadGameMenu");
	MainMenu->mItems.Push(it);
	it = new FListMenuItemPatch(97, 96, 's', TexMan.CheckForTexture("M_SAVEG", FTexture::TEX_MiscPatch), "SaveGameMenu");
	MainMenu->mItems.Push(it);
	it = new FListMenuItemPatch(97,112, 'o', TexMan.CheckForTexture("M_OPTION", FTexture::TEX_MiscPatch), "OptionsMenu");
	MainMenu->mItems.Push(it);
	it = new FListMenuItemPatch(97,128, 'r', TexMan.CheckForTexture("M_RDTHIS", FTexture::TEX_MiscPatch), "ReadThisMenu");
	MainMenu->mItems.Push(it);
	it = new FListMenuItemPatch(97,144, 'q', TexMan.CheckForTexture("M_QUITG", FTexture::TEX_MiscPatch), "QuitMenu");
	MainMenu->mItems.Push(it);
	MainMenu->mSelectedItem = 1;
	MainMenu->mSelectOfsX = -32;
	MainMenu->mSelectOfsY = -5;
	MainMenu->mSelector = TexMan.CheckForTexture("M_SKULL1", FTexture::TEX_MiscPatch);

}
	if (chatmodeon)
	{
		return false;
	}

	if (DMenu::CurrentMenu != NULL && menuactive == MENU_On) 
	{
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
				M_SetMenu();
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
