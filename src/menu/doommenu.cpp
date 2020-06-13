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

#include "c_dispatch.h"
#include "d_gui.h"
#include "c_buttons.h"
#include "c_console.h"
#include "c_bind.h"
#include "d_eventbase.h"
#include "g_input.h"
#include "configfile.h"
#include "gstrings.h"
#include "menu.h"
#include "vm.h"
#include "v_video.h"
#include "i_system.h"
#include "types.h"
#include "texturemanager.h"
#include "v_draw.h"
#include "vm.h"
#include "gamestate.h"
#include "i_interface.h" 
#include "gi.h"
#include "g_game.h"
#include "g_level.h"
#include "d_event.h"
#include "p_tick.h"
#include "st_start.h"
#include "d_main.h"
#include "i_system.h"
#include "doommenu.h"
#include "r_utility.h"
#include "gameconfigfile.h"

EXTERN_CVAR(Int, cl_gfxlocalization)
EXTERN_CVAR(Bool, m_quickexit)
EXTERN_CVAR(Bool, saveloadconfirmation) // [mxd]
EXTERN_CVAR(Bool, quicksaverotation)
EXTERN_CVAR(Bool, show_messages)

typedef void(*hfunc)();
DMenu* CreateMessageBoxMenu(DMenu* parent, const char* message, int messagemode, bool playsound, FName action = NAME_None, hfunc handler = nullptr);
bool OkForLocalization(FTextureID texnum, const char* substitute);
void D_ToggleHud();
void I_WaitVBL(int count);

extern bool hud_toggled;


FNewGameStartup NewGameStartupInfo;


bool M_SetSpecialMenu(FName menu, int param)
{
	// some menus need some special treatment
	switch (menu.GetIndex())
	{
	case NAME_Mainmenu:
		if (gameinfo.gametype & GAME_DoomStrifeChex)	// Raven's games always used text based menus
		{
			if (gameinfo.forcetextinmenus)	// If text is forced, this overrides any check.
			{
				menu = NAME_MainmenuTextOnly;
			}
			else if (cl_gfxlocalization != 0 && !gameinfo.forcenogfxsubstitution)
			{
				// For these games we must check up-front if they get localized because in that case another template must be used.
				DMenuDescriptor **desc = MenuDescriptors.CheckKey(NAME_Mainmenu);
				if (desc != nullptr)
				{
					if ((*desc)->IsKindOf(RUNTIME_CLASS(DListMenuDescriptor)))
					{
						DListMenuDescriptor *ld = static_cast<DListMenuDescriptor*>(*desc);
						if (ld->mFromEngine)
						{
							// This assumes that replacing one graphic will replace all of them.
							// So this only checks the "New game" entry for localization capability.
							FTextureID texid = TexMan.CheckForTexture("M_NGAME", ETextureType::MiscPatch);
							if (!OkForLocalization(texid, "$MNU_NEWGAME"))
							{
								menu = NAME_MainmenuTextOnly;
							}
						}
					}
				}
			}
		}
		break;
	case NAME_Episodemenu:
		// sent from the player class menu
		NewGameStartupInfo.Skill = -1;
		NewGameStartupInfo.Episode = -1;
		NewGameStartupInfo.PlayerClass = 
			param == -1000? nullptr :
			param == -1? "Random" : GetPrintableDisplayName(PlayerClasses[param].Type).GetChars();
		M_StartupEpisodeMenu(&NewGameStartupInfo);	// needs player class name from class menu (later)
		break;

	case NAME_Skillmenu:
		// sent from the episode menu

		if ((gameinfo.flags & GI_SHAREWARE) && param > 0)
		{
			// Only Doom and Heretic have multi-episode shareware versions.
			M_StartMessage(GStrings("SWSTRING"), 1);
			return false;
		}

		NewGameStartupInfo.Episode = param;
		M_StartupSkillMenu(&NewGameStartupInfo);	// needs player class name from class menu (later)
		break;

	case NAME_StartgameConfirm:
	{
		// sent from the skill menu for a skill that needs to be confirmed
		NewGameStartupInfo.Skill = param;

		const char *msg = AllSkills[param].MustConfirmText;
		if (*msg==0) msg = GStrings("NIGHTMARE");
		M_StartMessage (msg, 0, NAME_StartgameConfirmed);
		return false;
	}

	case NAME_Startgame:
		// sent either from skill menu or confirmation screen. Skill gets only set if sent from skill menu
		// Now we can finally start the game. Ugh...
		NewGameStartupInfo.Skill = param;
	case NAME_StartgameConfirmed:

		G_DeferedInitNew (&NewGameStartupInfo);
		if (gamestate == GS_FULLCONSOLE)
		{
			gamestate = GS_HIDECONSOLE;
			gameaction = ga_newgame;
		}
		M_ClearMenus ();
		return false;

	case NAME_Savegamemenu:
		if (!usergame || (players[consoleplayer].health <= 0 && !multiplayer) || gamestate != GS_LEVEL)
		{
			// cannot save outside the game.
			M_StartMessage (GStrings("SAVEDEAD"), 1);
			return false;
		}

	case NAME_Quitmenu:
		// The separate menu class no longer exists but the name still needs support for existing mods.
		C_DoCommand("menu_quit");
		return false;

	case NAME_EndGameMenu:
		// The separate menu class no longer exists but the name still needs support for existing mods.
		void ActivateEndGameMenu();
		ActivateEndGameMenu();
		return false;

	case NAME_Playermenu:
		menu = NAME_NewPlayerMenu;	// redirect the old player menu to the new one.
		break;
	}

	// End of special checks
	return true;
}

//=============================================================================
//
//
//
//=============================================================================

void M_StartControlPanel(bool makeSound, bool scaleoverride)
{
	if (hud_toggled)
		D_ToggleHud();

	// intro might call this repeatedly
	if (CurrentMenu != nullptr)
		return;

	P_CheckTickerPaused();

	if (makeSound)
	{
		S_Sound(CHAN_VOICE, CHANF_UI, "menu/activate", snd_menuvolume, ATTN_NONE);
	}
	M_DoStartControlPanel(scaleoverride);
}


//==========================================================================
//
// M_Dim
//
// Applies a colored overlay to the entire screen, with the opacity
// determined by the dimamount cvar.
//
//==========================================================================

CUSTOM_CVAR(Float, dimamount, -1.f, CVAR_ARCHIVE)
{
	if (self < 0.f && self != -1.f)
	{
		self = -1.f;
	}
	else if (self > 1.f)
	{
		self = 1.f;
	}
}
CVAR(Color, dimcolor, 0xffd700, CVAR_ARCHIVE)

void System_M_Dim()
{
	PalEntry dimmer;
	float amount;

	if (dimamount >= 0)
	{
		dimmer = PalEntry(dimcolor);
		amount = dimamount;
	}
	else
	{
		dimmer = gameinfo.dimcolor;
		amount = gameinfo.dimamount;
	}

	Dim(twod, dimmer, amount, 0, 0, twod->GetWidth(), twod->GetHeight());
}


//=============================================================================
//
//
//
//=============================================================================

CCMD (menu_quit)
{	// F10
	if (m_quickexit)
	{
		ST_Endoom();
	}

	M_StartControlPanel (true);

	const size_t messageindex = static_cast<size_t>(gametic) % gameinfo.quitmessages.Size();
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

	DMenu *newmenu = CreateMessageBoxMenu(CurrentMenu, EndString, 0, false, NAME_None, []()
	{
		if (!netgame)
		{
			if (gameinfo.quitSound.IsNotEmpty())
			{
				S_Sound(CHAN_VOICE, CHANF_UI, gameinfo.quitSound, snd_menuvolume, ATTN_NONE);
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

void ActivateEndGameMenu()
{
	FString tempstring = GStrings(netgame ? "NETEND" : "ENDGAME");
	DMenu *newmenu = CreateMessageBoxMenu(CurrentMenu, tempstring, 0, false, NAME_None, []()
	{
		M_ClearMenus();
		if (!netgame)
		{
			if (demorecording)
				G_CheckDemoStatus();
			D_StartTitle();
		}
	});

	M_ActivateMenu(newmenu);
}

CCMD (menu_endgame)
{	// F7
	if (!usergame)
	{
		S_Sound (CHAN_VOICE, CHANF_UI, "menu/invalid", snd_menuvolume, ATTN_NONE);
		return;
	}
		
	//M_StartControlPanel (true);
	S_Sound (CHAN_VOICE, CHANF_UI, "menu/activate", snd_menuvolume, ATTN_NONE);

	ActivateEndGameMenu();
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
		S_Sound (CHAN_VOICE, CHANF_UI, "menu/invalid", snd_menuvolume, ATTN_NONE);
		return;
	}

	if (gamestate != GS_LEVEL)
		return;

	// If the quick save rotation is enabled, it handles the save slot.
	if (quicksaverotation)
	{
		G_DoQuickSave();
		return;
	}
		
	if (savegameManager.quickSaveSlot == NULL || savegameManager.quickSaveSlot == (FSaveGameNode*)1)
	{
		S_Sound(CHAN_VOICE, CHANF_UI, "menu/activate", snd_menuvolume, ATTN_NONE);
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

	S_Sound(CHAN_VOICE, CHANF_UI, "menu/activate", snd_menuvolume, ATTN_NONE);

	FString tempstring = GStrings("QSPROMPT");
	tempstring.Substitute("%s", savegameManager.quickSaveSlot->SaveTitle.GetChars());

	DMenu *newmenu = CreateMessageBoxMenu(CurrentMenu, tempstring, 0, false, NAME_None, []()
	{
		G_SaveGame(savegameManager.quickSaveSlot->Filename.GetChars(), savegameManager.quickSaveSlot->SaveTitle.GetChars());
		S_Sound(CHAN_VOICE, CHANF_UI, "menu/dismiss", snd_menuvolume, ATTN_NONE);
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
		
	if (savegameManager.quickSaveSlot == NULL || savegameManager.quickSaveSlot == (FSaveGameNode*)1)
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
	FString tempstring = GStrings("QLPROMPT");
	tempstring.Substitute("%s", savegameManager.quickSaveSlot->SaveTitle.GetChars());

	M_StartControlPanel(true);

	DMenu *newmenu = CreateMessageBoxMenu(CurrentMenu, tempstring, 0, false, NAME_None, []()
	{
		G_LoadGame(savegameManager.quickSaveSlot->Filename.GetChars());
		S_Sound(CHAN_VOICE, CHANF_UI, "menu/dismiss", snd_menuvolume, ATTN_NONE);
		M_ClearMenus();
	});
	M_ActivateMenu(newmenu);
}




//
//		Toggle messages on/off
//
CCMD (togglemessages)
{
	if (show_messages)
	{
		Printf (128, "%s\n", GStrings("MSGOFF"));
		show_messages = false;
	}
	else
	{
		Printf (128, "%s\n", GStrings("MSGON"));
		show_messages = true;
	}
}

EXTERN_CVAR (Int, screenblocks)

CCMD (sizedown)
{
	screenblocks = screenblocks - 1;
	S_Sound (CHAN_VOICE, CHANF_UI, "menu/change", snd_menuvolume, ATTN_NONE);
}

CCMD (sizeup)
{
	screenblocks = screenblocks + 1;
	S_Sound (CHAN_VOICE, CHANF_UI, "menu/change", snd_menuvolume, ATTN_NONE);
}

CCMD(reset2defaults)
{
	C_SetDefaultBindings ();
	C_SetCVarsToDefaults ();
	R_SetViewSize (screenblocks);
}

CCMD(reset2saved)
{
	GameConfig->DoGlobalSetup ();
	GameConfig->DoGameSetup (gameinfo.ConfigName);
	GameConfig->DoModSetup (gameinfo.ConfigName);
	R_SetViewSize (screenblocks);
}


