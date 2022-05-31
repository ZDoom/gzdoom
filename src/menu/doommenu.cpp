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
#include "d_player.h"
#include "teaminfo.h"
#include "i_time.h"
#include "hwrenderer/scene/hw_drawinfo.h"

EXTERN_CVAR(Int, cl_gfxlocalization)
EXTERN_CVAR(Bool, m_quickexit)
EXTERN_CVAR(Bool, saveloadconfirmation) // [mxd]
EXTERN_CVAR(Bool, quicksaverotation)
EXTERN_CVAR(Bool, show_messages)

CVAR(Bool, m_simpleoptions, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

typedef void(*hfunc)();
DMenu* CreateMessageBoxMenu(DMenu* parent, const char* message, int messagemode, bool playsound, FName action = NAME_None, hfunc handler = nullptr);
bool OkForLocalization(FTextureID texnum, const char* substitute);


FNewGameStartup NewGameStartupInfo;
int LastSkill = -1;


bool M_SetSpecialMenu(FName& menu, int param)
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
		LastSkill = param;

		const char *msg = AllSkills[param].MustConfirmText;
		if (*msg==0) msg = GStrings("NIGHTMARE");
		M_StartMessage (msg, 0, NAME_StartgameConfirmed);
		return false;
	}

	case NAME_Startgame:
		// sent either from skill menu or confirmation screen. Skill gets only set if sent from skill menu
		// Now we can finally start the game. Ugh...
		LastSkill = param;
		NewGameStartupInfo.Skill = param;
		[[fallthrough]];
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
		break;

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

	case NAME_Optionsmenu:
		if (m_simpleoptions) menu = NAME_OptionsmenuSimple;
		break;

	case NAME_OptionsmenuFull:
		menu = NAME_Optionsmenu;
		break;

	case NAME_Readthismenu:
		// [MK] allow us to override the ReadThisMenu class
		menu = gameinfo.HelpMenuClass;
		break;
	}

	DMenuDescriptor** desc = MenuDescriptors.CheckKey(menu);
	if (desc != nullptr)
	{
		if ((*desc)->mNetgameMessage.IsNotEmpty() && netgame && !demoplayback)
		{
			M_StartMessage((*desc)->mNetgameMessage, 1);
			return false;
		}
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
		CleanSWDrawer();
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
		CleanSWDrawer();
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
		Printf(TEXTCOLOR_RED "%s\n", GStrings("MSGOFF"));
		show_messages = false;
	}
	else
	{
		show_messages = true;
		Printf(TEXTCOLOR_RED "%s\n", GStrings("MSGON"));
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

CCMD(resetb2defaults)
{
	C_SetDefaultBindings ();
}


//=============================================================================
//
// Creates the episode menu
// Falls back on an option menu if there's not enough screen space to show all episodes
//
//=============================================================================

void M_StartupEpisodeMenu(FNewGameStartup *gs)
{
	// Build episode menu
	bool success = false;
	bool isOld = false;
	DMenuDescriptor **desc = MenuDescriptors.CheckKey(NAME_Episodemenu);
	if (desc != nullptr)
	{
		if ((*desc)->IsKindOf(RUNTIME_CLASS(DListMenuDescriptor)))
		{
			DListMenuDescriptor *ld = static_cast<DListMenuDescriptor*>(*desc);
			
			// Delete previous contents
			for(unsigned i=0; i<ld->mItems.Size(); i++)
			{
				FName n = ld->mItems[i]->mAction;
				if (n == NAME_Skillmenu)
				{
					isOld = true;
					ld->mItems.Resize(i);
					break;
				}
			}

			
			int posx = (int)ld->mXpos;
			int posy = (int)ld->mYpos;
			int topy = posy;

			// Get lowest y coordinate of any static item in the menu
			for(unsigned i = 0; i < ld->mItems.Size(); i++)
			{
				int y = (int)ld->mItems[i]->GetY();
				if (y < topy) topy = y;
			}

			int spacing = ld->mLinespacing;
			for (unsigned i = 0; i < AllEpisodes.Size(); i++)
			{
				if (AllEpisodes[i].mPicName.IsNotEmpty())
				{
					FTextureID tex = GetMenuTexture(AllEpisodes[i].mPicName);
					if (AllEpisodes[i].mEpisodeName.IsEmpty() || OkForLocalization(tex, AllEpisodes[i].mEpisodeName))
						continue;
				}
				if ((gameinfo.gametype & GAME_DoomStrifeChex) && spacing == 16) spacing = 18;
				break;
			}

			// center the menu on the screen if the top space is larger than the bottom space
			int totalheight = posy + AllEpisodes.Size() * spacing - topy;

			if (totalheight < 190 || AllEpisodes.Size() == 1)
			{
				int newtop = (200 - totalheight) / 2;
				int topdelta = newtop - topy;
				if (topdelta < 0)
				{
					for(unsigned i = 0; i < ld->mItems.Size(); i++)
					{
						ld->mItems[i]->OffsetPositionY(topdelta);
					}
					posy += topdelta;
					ld->mYpos += topdelta;
				}

				if (!isOld) ld->mSelectedItem = ld->mItems.Size();

				for (unsigned i = 0; i < AllEpisodes.Size(); i++)
				{
					DMenuItemBase *it = nullptr;
					if (AllEpisodes[i].mPicName.IsNotEmpty())
					{
						FTextureID tex = GetMenuTexture(AllEpisodes[i].mPicName);
						if (AllEpisodes[i].mEpisodeName.IsEmpty() || OkForLocalization(tex, AllEpisodes[i].mEpisodeName))
							continue;	// We do not measure patch based entries. They are assumed to fit
					}
					const char *c = AllEpisodes[i].mEpisodeName;
					if (*c == '$') c = GStrings(c + 1);
					int textwidth = ld->mFont->StringWidth(c);
					int textright = posx + textwidth;
					if (posx + textright > 320) posx = max(0, 320 - textright);
				}

				for(unsigned i = 0; i < AllEpisodes.Size(); i++)
				{
					DMenuItemBase *it = nullptr;
					if (AllEpisodes[i].mPicName.IsNotEmpty())
					{
						FTextureID tex = GetMenuTexture(AllEpisodes[i].mPicName);
						if (AllEpisodes[i].mEpisodeName.IsEmpty() || OkForLocalization(tex, AllEpisodes[i].mEpisodeName))
							it = CreateListMenuItemPatch(posx, posy, spacing, AllEpisodes[i].mShortcut, tex, NAME_Skillmenu, i);
					}
					if (it == nullptr)
					{
						it = CreateListMenuItemText(posx, posy, spacing, AllEpisodes[i].mShortcut, 
							AllEpisodes[i].mEpisodeName, ld->mFont, ld->mFontColor, ld->mFontColor2, NAME_Skillmenu, i);
					}
					ld->mItems.Push(it);
					posy += spacing;
				}
				if (AllEpisodes.Size() == 1)
				{
					ld->mAutoselect = ld->mSelectedItem;
				}
				success = true;
				for (auto &p : ld->mItems)
				{
					GC::WriteBarrier(*desc, p);
				}
			}
		}
		else return;	// do not recreate the option menu variant, because it is always text based.
	}
	if (!success)
	{
		// Couldn't create the episode menu, either because there's too many episodes or some error occured
		// Create an option menu for episode selection instead.
		DOptionMenuDescriptor *od = Create<DOptionMenuDescriptor>();
		MenuDescriptors[NAME_Episodemenu] = od;
		od->mMenuName = NAME_Episodemenu;
		od->mFont = gameinfo.gametype == GAME_Doom ? BigUpper : BigFont;
		od->mTitle = "$MNU_EPISODE";
		od->mSelectedItem = 0;
		od->mScrollPos = 0;
		od->mClass = nullptr;
		od->mPosition = -15;
		od->mScrollTop = 0;
		od->mIndent = 160;
		od->mDontDim = false;
		GC::WriteBarrier(od);
		for(unsigned i = 0; i < AllEpisodes.Size(); i++)
		{
			auto it = CreateOptionMenuItemSubmenu(AllEpisodes[i].mEpisodeName, "Skillmenu", i);
			od->mItems.Push(it);
			GC::WriteBarrier(od, it);
		}
	}
}

//=============================================================================
//
//
//
//=============================================================================

static void BuildPlayerclassMenu()
{
	bool success = false;

	// Build player class menu
	DMenuDescriptor **desc = MenuDescriptors.CheckKey(NAME_Playerclassmenu);
	if (desc != nullptr)
	{
		if ((*desc)->IsKindOf(RUNTIME_CLASS(DListMenuDescriptor)))
		{
			DListMenuDescriptor *ld = static_cast<DListMenuDescriptor*>(*desc);
			// add player display

			ld->mSelectedItem = ld->mItems.Size();
			
			int posy = (int)ld->mYpos;
			int topy = posy;

			// Get lowest y coordinate of any static item in the menu
			for(unsigned i = 0; i < ld->mItems.Size(); i++)
			{
				int y = (int)ld->mItems[i]->GetY();
				if (y < topy) topy = y;
			}

			// Count the number of items this menu will show
			int numclassitems = 0;
			for (unsigned i = 0; i < PlayerClasses.Size (); i++)
			{
				if (!(PlayerClasses[i].Flags & PCF_NOMENU))
				{
					const char *pname = GetPrintableDisplayName(PlayerClasses[i].Type);
					if (pname != nullptr)
					{
						numclassitems++;
					}
				}
			}

			// center the menu on the screen if the top space is larger than the bottom space
			int totalheight = posy + (numclassitems+1) * ld->mLinespacing - topy;

			if (numclassitems <= 1)
			{
				// create a dummy item that auto-chooses the default class.
				auto it = CreateListMenuItemText(0, 0, 0, 'p', "player", 
					ld->mFont,ld->mFontColor, ld->mFontColor2, NAME_Episodemenu, -1000);
				ld->mAutoselect = ld->mItems.Push(it);
				success = true;
			}
			else if (totalheight <= 190)
			{
				int newtop = (200 - totalheight + topy) / 2;
				int topdelta = newtop - topy;
				if (topdelta < 0)
				{
					for(unsigned i = 0; i < ld->mItems.Size(); i++)
					{
						ld->mItems[i]->OffsetPositionY(topdelta);
					}
					posy -= topdelta;
				}

				int n = 0;
				for (unsigned i = 0; i < PlayerClasses.Size (); i++)
				{
					if (!(PlayerClasses[i].Flags & PCF_NOMENU))
					{
						const char *pname = GetPrintableDisplayName(PlayerClasses[i].Type);
						if (pname != nullptr)
						{
							auto it = CreateListMenuItemText(ld->mXpos, ld->mYpos, ld->mLinespacing, *pname,
								pname, ld->mFont,ld->mFontColor,ld->mFontColor2, NAME_Episodemenu, i);
							ld->mItems.Push(it);
							ld->mYpos += ld->mLinespacing;
							n++;
						}
					}
				}
				if (n > 1 && !gameinfo.norandomplayerclass)
				{
					auto it = CreateListMenuItemText(ld->mXpos, ld->mYpos, ld->mLinespacing, 'r',
						"$MNU_RANDOM", ld->mFont,ld->mFontColor,ld->mFontColor2, NAME_Episodemenu, -1);
					ld->mItems.Push(it);
				}
				if (n == 0)
				{
					const char *pname = GetPrintableDisplayName(PlayerClasses[0].Type);
					if (pname != nullptr)
					{
						auto it = CreateListMenuItemText(ld->mXpos, ld->mYpos, ld->mLinespacing, *pname,
							pname, ld->mFont,ld->mFontColor,ld->mFontColor2, NAME_Episodemenu, 0);
						ld->mItems.Push(it);
					}
				}
				success = true;
				for (auto &p : ld->mItems)
				{
					GC::WriteBarrier(ld, p);
				}
			}
		}
	}
	if (!success)
	{
		// Couldn't create the playerclass menu, either because there's too many episodes or some error occured
		// Create an option menu for class selection instead.
		DOptionMenuDescriptor *od = Create<DOptionMenuDescriptor>();
		MenuDescriptors[NAME_Playerclassmenu] = od;
		od->mMenuName = NAME_Playerclassmenu;
		od->mFont = gameinfo.gametype == GAME_Doom ? BigUpper : BigFont;
		od->mTitle = "$MNU_CHOOSECLASS";
		od->mSelectedItem = 0;
		od->mScrollPos = 0;
		od->mClass = nullptr;
		od->mPosition = -15;
		od->mScrollTop = 0;
		od->mIndent = 160;
		od->mDontDim = false;
		od->mNetgameMessage = "$NEWGAME";
		GC::WriteBarrier(od);
		for (unsigned i = 0; i < PlayerClasses.Size (); i++)
		{
			if (!(PlayerClasses[i].Flags & PCF_NOMENU))
			{
				const char *pname = GetPrintableDisplayName(PlayerClasses[i].Type);
				if (pname != nullptr)
				{
					auto it = CreateOptionMenuItemSubmenu(pname, "Episodemenu", i);
					od->mItems.Push(it);
					GC::WriteBarrier(od, it);
				}
			}
		}
		auto it = CreateOptionMenuItemSubmenu("Random", "Episodemenu", -1);
		od->mItems.Push(it);
		GC::WriteBarrier(od, it);
	}
}

//=============================================================================
//
// Reads any XHAIRS lumps for the names of crosshairs and
// adds them to the display options menu.
//
//=============================================================================

static void InitCrosshairsList()
{
	int lastlump, lump;

	lastlump = 0;

	FOptionValues **opt = OptionValues.CheckKey(NAME_Crosshairs);
	if (opt == nullptr) 
	{
		return;	// no crosshair value list present. No need to go on.
	}

	FOptionValues::Pair *pair = &(*opt)->mValues[(*opt)->mValues.Reserve(1)];
	pair->Value = 0;
	pair->Text = "None";

	while ((lump = fileSystem.FindLump("XHAIRS", &lastlump)) != -1)
	{
		FScanner sc(lump);
		while (sc.GetNumber())
		{
			FOptionValues::Pair value;
			value.Value = sc.Number;
			sc.MustGetString();
			value.Text = sc.String;
			if (value.Value != 0)
			{ // Check if it already exists. If not, add it.
				unsigned int i;

				for (i = 1; i < (*opt)->mValues.Size(); ++i)
				{
					if ((*opt)->mValues[i].Value == value.Value)
					{
						break;
					}
				}
				if (i < (*opt)->mValues.Size())
				{
					(*opt)->mValues[i].Text = value.Text;
				}
				else
				{
					(*opt)->mValues.Push(value);
				}
			}
		}
	}
}

//=============================================================================
//
// With the current workings of the menu system this cannot be done any longer
// from within the respective CCMDs.
//
//=============================================================================

static void InitKeySections()
{
	DMenuDescriptor **desc = MenuDescriptors.CheckKey(NAME_CustomizeControls);
	if (desc != nullptr)
	{
		if ((*desc)->IsKindOf(RUNTIME_CLASS(DOptionMenuDescriptor)))
		{
			DOptionMenuDescriptor *menu = static_cast<DOptionMenuDescriptor*>(*desc);

			for (unsigned i = 0; i < KeySections.Size(); i++)
			{
				FKeySection *sect = &KeySections[i];
				DMenuItemBase *item = CreateOptionMenuItemStaticText(" ");
				menu->mItems.Push(item);
				item = CreateOptionMenuItemStaticText(sect->mTitle, 1);
				menu->mItems.Push(item);
				for (unsigned j = 0; j < sect->mActions.Size(); j++)
				{
					FKeyAction *act = &sect->mActions[j];
					item = CreateOptionMenuItemControl(act->mTitle, act->mAction, &Bindings);
					menu->mItems.Push(item);
				}
			}
			for (auto &p : menu->mItems)
			{
				GC::WriteBarrier(*desc, p);
			}
		}
	}
}


//=============================================================================
//
// Special menus will be created once all engine data is loaded
//
//=============================================================================

void M_CreateGameMenus()
{
	BuildPlayerclassMenu();
	InitCrosshairsList();
	InitKeySections();

	auto opt = OptionValues.CheckKey(NAME_PlayerTeam);
	if (opt != nullptr)
	{
		auto op = *opt; 
		op->mValues.Resize(Teams.Size() + 1);
		op->mValues[0].Value = 0;
		op->mValues[0].Text = "$OPTVAL_NONE";
		for (unsigned i = 0; i < Teams.Size(); i++)
		{
			op->mValues[i+1].Value = i+1;
			op->mValues[i+1].Text = Teams[i].GetName();
		}
	}
	opt = OptionValues.CheckKey(NAME_PlayerClass);
	if (opt != nullptr)
	{
		auto op = *opt;
		int o = 0;
		if (!gameinfo.norandomplayerclass && PlayerClasses.Size() > 1)
		{
			op->mValues.Resize(PlayerClasses.Size()+1);
			op->mValues[0].Value = -1;
			op->mValues[0].Text = "$MNU_RANDOM";
			o = 1;
		}
		else op->mValues.Resize(PlayerClasses.Size());
		for (unsigned i = 0; i < PlayerClasses.Size(); i++)
		{
			op->mValues[i+o].Value = i;
			op->mValues[i+o].Text = GetPrintableDisplayName(PlayerClasses[i].Type);
		}
	}
}

DEFINE_ACTION_FUNCTION(DNewPlayerMenu, UpdateColorsets)
{
	PARAM_PROLOGUE;
	PARAM_POINTER(playerClass, FPlayerClass);

	TArray<int> PlayerColorSets;

	EnumColorSets(playerClass->Type, &PlayerColorSets);

	auto opt = OptionValues.CheckKey(NAME_PlayerColors);
	if (opt != nullptr)
	{
		auto op = *opt;
		op->mValues.Resize(PlayerColorSets.Size() + 1);
		op->mValues[0].Value = -1;
		op->mValues[0].Text = "$OPTVAL_CUSTOM";
		for (unsigned i = 0; i < PlayerColorSets.Size(); i++)
		{
			auto cset = GetColorSet(playerClass->Type, PlayerColorSets[i]);
			op->mValues[i + 1].Value = PlayerColorSets[i];
			op->mValues[i + 1].Text = cset? cset->Name.GetChars() : "?";	// The null case should never happen here.
		}
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(DNewPlayerMenu, UpdateSkinOptions)
{
	PARAM_PROLOGUE;
	PARAM_POINTER(playerClass, FPlayerClass);

	auto opt = OptionValues.CheckKey(NAME_PlayerSkin);
	if (opt != nullptr)
	{
		auto op = *opt;

		if ((GetDefaultByType(playerClass->Type)->flags4 & MF4_NOSKIN) || players[consoleplayer].userinfo.GetPlayerClassNum() == -1)
		{
			op->mValues.Resize(1);
			op->mValues[0].Value = -1;
			op->mValues[0].Text = "$OPTVAL_DEFAULT";
		}
		else
		{
			op->mValues.Clear();
			for (unsigned i = 0; i < Skins.Size(); i++)
			{
				op->mValues.Reserve(1);
				op->mValues.Last().Value = i;
				op->mValues.Last().Text = Skins[i].Name;
			}
		}
	}
	return 0;
}

//=============================================================================
//
// The skill menu must be refeshed each time it starts up
//
//=============================================================================
extern int restart;

void M_StartupSkillMenu(FNewGameStartup *gs)
{
	static int done = -1;
	bool success = false;
	TArray<FSkillInfo*> MenuSkills;
	TArray<int> SkillIndices;
	if (MenuSkills.Size() == 0)
	{
		for (unsigned ind = 0; ind < AllSkills.Size(); ind++)
		{
			if (!AllSkills[ind].NoMenu)
			{
				MenuSkills.Push(&AllSkills[ind]);
				SkillIndices.Push(ind);
			}
		}
	}
	if (MenuSkills.Size() == 0) I_Error("No valid skills for menu found. At least one must be defined.");

	int defskill = LastSkill > -1? LastSkill : DefaultSkill; // use the last selected skill, if available.
	if ((unsigned int)defskill >= MenuSkills.Size())
	{
		defskill = SkillIndices[(MenuSkills.Size() - 1) / 2];
	}
	if (AllSkills[defskill].NoMenu)
	{
		for (defskill = 0; defskill < (int)AllSkills.Size(); defskill++)
		{
			if (!AllSkills[defskill].NoMenu) break;
		}
	}
	int defindex = 0;
	for (unsigned i = 0; i < MenuSkills.Size(); i++)
	{
		if (MenuSkills[i] == &AllSkills[defskill])
		{
			defindex = i;
			break;
		}
	}

	DMenuDescriptor **desc = MenuDescriptors.CheckKey(NAME_Skillmenu);
	if (desc != nullptr)
	{
		if ((*desc)->IsKindOf(RUNTIME_CLASS(DListMenuDescriptor)))
		{
			DListMenuDescriptor *ld = static_cast<DListMenuDescriptor*>(*desc);
			int posx = (int)ld->mXpos;
			int y = (int)ld->mYpos;

			// Delete previous contents
			for(unsigned i=0; i<ld->mItems.Size(); i++)
			{
				FName n = ld->mItems[i]->mAction;
				if (n == NAME_Startgame || n == NAME_StartgameConfirm) 
				{
					ld->mItems.Resize(i);
					break;
				}
			}

			int spacing = ld->mLinespacing;
			//if (done != restart)
			{
				//done = restart;
				ld->mSelectedItem = ld->mItems.Size() + defindex;

				int posy = y;
				int topy = posy;

				// Get lowest y coordinate of any static item in the menu
				for(unsigned i = 0; i < ld->mItems.Size(); i++)
				{
					int y = (int)ld->mItems[i]->GetY();
					if (y < topy) topy = y;
				}

				for (unsigned i = 0; i < MenuSkills.Size(); i++)
				{
					if (MenuSkills[i]->PicName.IsNotEmpty())
					{
						FTextureID tex = GetMenuTexture(MenuSkills[i]->PicName);
						if (MenuSkills[i]->MenuName.IsEmpty() || OkForLocalization(tex, MenuSkills[i]->MenuName))
							continue;
					}
					if ((gameinfo.gametype & GAME_DoomStrifeChex) && spacing == 16) spacing = 18;
					break;
				}

				// center the menu on the screen if the top space is larger than the bottom space
				int totalheight = posy + MenuSkills.Size() * spacing - topy;

				if (totalheight < 190 || MenuSkills.Size() == 1)
				{
					int newtop = (200 - totalheight) / 2;
					int topdelta = newtop - topy;
					if (topdelta < 0)
					{
						for(unsigned i = 0; i < ld->mItems.Size(); i++)
						{
							ld->mItems[i]->OffsetPositionY(topdelta);
						}
						ld->mYpos = y = posy + topdelta;
					}
				}
				else
				{
					// too large
					desc = nullptr;
					done = false;
					goto fail;
				}
			}

			for (unsigned int i = 0; i < MenuSkills.Size(); i++)
			{
				FSkillInfo &skill = *MenuSkills[i];
				DMenuItemBase *li = nullptr;

				FString *pItemText = nullptr;
				if (gs->PlayerClass != nullptr)
				{
					pItemText = skill.MenuNamesForPlayerClass.CheckKey(gs->PlayerClass);
				}

				if (skill.PicName.Len() != 0 && pItemText == nullptr)
				{
					FTextureID tex = GetMenuTexture(skill.PicName);
					if (skill.MenuName.IsEmpty() || OkForLocalization(tex, skill.MenuName))
						continue;
				}
				const char *c = pItemText ? pItemText->GetChars() : skill.MenuName.GetChars();
				if (*c == '$') c = GStrings(c + 1);
				int textwidth = ld->mFont->StringWidth(c);
				int textright = posx + textwidth;
				if (posx + textright > 320) posx = max(0, 320 - textright);
			}

			unsigned firstitem = ld->mItems.Size();
			for(unsigned int i = 0; i < MenuSkills.Size(); i++)
			{
				FSkillInfo &skill = *MenuSkills[i];
				DMenuItemBase *li = nullptr;
				// Using a different name for skills that must be confirmed makes handling this easier.
				FName action = (skill.MustConfirm && !AllEpisodes[gs->Episode].mNoSkill) ?
					NAME_StartgameConfirm : NAME_Startgame;
				FString *pItemText = nullptr;
				if (gs->PlayerClass != nullptr)
				{
					pItemText = skill.MenuNamesForPlayerClass.CheckKey(gs->PlayerClass);
				}

				EColorRange color = (EColorRange)skill.GetTextColor();
				if (color == CR_UNTRANSLATED) color = ld->mFontColor;
				if (skill.PicName.Len() != 0 && pItemText == nullptr)
				{
					FTextureID tex = GetMenuTexture(skill.PicName);
					if (skill.MenuName.IsEmpty() || OkForLocalization(tex, skill.MenuName))
						li = CreateListMenuItemPatch(posx, y, spacing, skill.Shortcut, tex, action, SkillIndices[i]);
				}
				if (li == nullptr)
				{
					li = CreateListMenuItemText(posx, y, spacing, skill.Shortcut,
									pItemText? *pItemText : skill.MenuName, ld->mFont, color,ld->mFontColor2, action, SkillIndices[i]);
				}
				ld->mItems.Push(li);
				GC::WriteBarrier(*desc, li);
				y += spacing;
			}
			if (AllEpisodes[gs->Episode].mNoSkill || MenuSkills.Size() == 1)
			{
				ld->mAutoselect = firstitem + defindex;
			}
			else
			{
				ld->mAutoselect = -1;
			}
			success = true;
		}
	}
	if (success) return;
fail:
	// Option menu fallback for overlong skill lists
	DOptionMenuDescriptor *od;
	if (desc == nullptr)
	{
		od = Create<DOptionMenuDescriptor>();
		MenuDescriptors[NAME_Skillmenu] = od;
		od->mMenuName = NAME_Skillmenu;
		od->mFont = gameinfo.gametype == GAME_Doom ? BigUpper : BigFont;
		od->mTitle = "$MNU_CHOOSESKILL";
		od->mSelectedItem = defindex;
		od->mScrollPos = 0;
		od->mClass = nullptr;
		od->mPosition = -15;
		od->mScrollTop = 0;
		od->mIndent = 160;
		od->mDontDim = false;
		GC::WriteBarrier(od);
	}
	else
	{
		od = static_cast<DOptionMenuDescriptor*>(*desc);
		od->mItems.Clear();
	}
	for(unsigned int i = 0; i < MenuSkills.Size(); i++)
	{
		FSkillInfo &skill = *MenuSkills[i];
		DMenuItemBase *li;
		// Using a different name for skills that must be confirmed makes handling this easier.
		const char *action = (skill.MustConfirm && !AllEpisodes[gs->Episode].mNoSkill) ?
			"StartgameConfirm" : "Startgame";

		FString *pItemText = nullptr;
		if (gs->PlayerClass != nullptr)
		{
			pItemText = skill.MenuNamesForPlayerClass.CheckKey(gs->PlayerClass);
		}
		li = CreateOptionMenuItemSubmenu(pItemText? *pItemText : skill.MenuName, action, SkillIndices[i]);
		od->mItems.Push(li);
		GC::WriteBarrier(od, li);
		if (!done)
		{
			done = true;
			od->mSelectedItem = defindex;
		}
	}
}

//==========================================================================
//
// Defines how graphics substitution is handled.
// 0: Never replace a text-containing graphic with a font-based text.
// 1: Always replace, regardless of any missing information. Useful for testing the substitution without providing full data.
// 2: Only replace for non-default texts, i.e. if some language redefines the string's content, use it instead of the graphic. Never replace a localized graphic.
// 3: Only replace if the string is not the default and the graphic comes from the IWAD. Never replace a localized graphic.
// 4: Like 1, but lets localized graphics pass.
//
// The default is 3, which only replaces known content with non-default texts.
//
//==========================================================================

CUSTOM_CVAR(Int, cl_gfxlocalization, 3, CVAR_ARCHIVE)
{
	if (self < 0 || self > 4) self = 0;
}

bool OkForLocalization(FTextureID texnum, const char* substitute)
{
	if (!texnum.isValid()) return false;

	// First the unconditional settings, 0='never' and 1='always'.
	if (cl_gfxlocalization == 1 || gameinfo.forcetextinmenus) return false;
	if (cl_gfxlocalization == 0 || gameinfo.forcenogfxsubstitution) return true;
	return TexMan.OkForLocalization(texnum, substitute, cl_gfxlocalization);
}

bool  CheckSkipGameOptionBlock(const char *str)
{
	bool filter = false;
	if (!stricmp(str, "ReadThis")) filter |= gameinfo.drawreadthis;
	else if (!stricmp(str, "Swapmenu")) filter |= gameinfo.swapmenu;
	return filter;
}

void SetDefaultMenuColors()
{
	OptionSettings.mTitleColor = V_FindFontColor(gameinfo.mTitleColor);
	OptionSettings.mFontColor = V_FindFontColor(gameinfo.mFontColor);
	OptionSettings.mFontColorValue = V_FindFontColor(gameinfo.mFontColorValue);
	OptionSettings.mFontColorMore = V_FindFontColor(gameinfo.mFontColorMore);
	OptionSettings.mFontColorHeader = V_FindFontColor(gameinfo.mFontColorHeader);
	OptionSettings.mFontColorHighlight = V_FindFontColor(gameinfo.mFontColorHighlight);
	OptionSettings.mFontColorSelection = V_FindFontColor(gameinfo.mFontColorSelection);

	auto cls = PClass::FindClass(gameinfo.HelpMenuClass);
	if (!cls)
		I_FatalError("%s: Undefined help menu class", gameinfo.HelpMenuClass.GetChars());
	if (!cls->IsDescendantOf(RUNTIME_CLASS(DMenu)))
		I_FatalError("'%s' does not inherit from Menu", gameinfo.HelpMenuClass.GetChars());

	cls = PClass::FindClass(gameinfo.MenuDelegateClass);
	if (!cls)
		I_FatalError("%s: Undefined menu delegate class", gameinfo.MenuDelegateClass.GetChars());
	if (!cls->IsDescendantOf("MenuDelegateBase"))
		I_FatalError("'%s' does not inherit from MenuDelegateBase", gameinfo.MenuDelegateClass.GetChars());
	menuDelegate = cls->CreateNew();
}

CCMD (menu_main)
{
	if (gamestate == GS_FULLCONSOLE) gamestate = GS_MENUSCREEN;
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

CCMD (menu_messages)
{
	M_StartControlPanel (true);
	M_SetMenu(NAME_MessageOptions, -1);
}

CCMD (menu_automap)
{
	M_StartControlPanel (true);
	M_SetMenu(NAME_AutomapOptions, -1);
}

CCMD (menu_scoreboard)
{
	M_StartControlPanel (true);
	M_SetMenu(NAME_ScoreboardOptions, -1);
}

CCMD (menu_mapcolors)
{
	M_StartControlPanel (true);
	M_SetMenu(NAME_MapColorMenu, -1);
}

CCMD (menu_keys)
{
	M_StartControlPanel (true);
	M_SetMenu(NAME_CustomizeControls, -1);
}

CCMD (menu_gameplay)
{
	M_StartControlPanel (true);
	M_SetMenu(NAME_GameplayOptions, -1);
}

CCMD (menu_compatibility)
{
	M_StartControlPanel (true);
	M_SetMenu(NAME_CompatibilityOptions, -1);
}

CCMD (menu_mouse)
{
	M_StartControlPanel (true);
	M_SetMenu(NAME_MouseOptions, -1);
}

CCMD (menu_joystick)
{
	M_StartControlPanel (true);
	M_SetMenu(NAME_JoystickOptions, -1);
}

CCMD (menu_sound)
{
	M_StartControlPanel (true);
	M_SetMenu(NAME_SoundOptions, -1);
}

CCMD (menu_advsound)
{
	M_StartControlPanel (true);
	M_SetMenu(NAME_AdvSoundOptions, -1);
}

CCMD (menu_modreplayer)
{
	M_StartControlPanel(true);
	M_SetMenu(NAME_ModReplayerOptions, -1);
}

CCMD (menu_display)
{
	M_StartControlPanel (true);
	M_SetMenu(NAME_VideoOptions, -1);
}

CCMD (menu_video)
{
	M_StartControlPanel (true);
	M_SetMenu(NAME_VideoModeMenu, -1);
}


#ifdef _WIN32
EXTERN_CVAR(Bool, vr_enable_quadbuffered)
#endif

void UpdateVRModes(bool considerQuadBuffered)
{
	FOptionValues** pVRModes = OptionValues.CheckKey("VRMode");
	if (pVRModes == nullptr) return;

	TArray<FOptionValues::Pair>& vals = (*pVRModes)->mValues;
	TArray<FOptionValues::Pair> filteredValues;
	int cnt = vals.Size();
	for (int i = 0; i < cnt; ++i) {
		auto const& mode = vals[i];
		if (mode.Value == 7) {  // Quad-buffered stereo
#ifdef _WIN32
			if (!vr_enable_quadbuffered) continue;
#else
			continue;  // Remove quad-buffered option on Mac and Linux
#endif
			if (!considerQuadBuffered) continue;  // Probably no compatible screen mode was found
		}
		filteredValues.Push(mode);
	}
	vals = filteredValues;
}
