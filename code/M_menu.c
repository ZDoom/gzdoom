// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//		DOOM selection menu, options, episode etc.
//		Sliders and icons. Kinda widget stuff.
//
//-----------------------------------------------------------------------------


#include <ctype.h>
#include <fcntl.h>
#include <stdlib.h>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

#include "doomdef.h"
#include "dstrings.h"
#include "c_consol.h"
#include "c_dispch.h"
#include "d_main.h"
#include "i_system.h"
#include "i_input.h"
#include "i_video.h"
#include "z_zone.h"
#include "v_video.h"
#include "w_wad.h"
#include "r_local.h"
#include "hu_stuff.h"
#include "g_game.h"
#include "m_argv.h"
#include "m_swap.h"
#include "m_random.h"
#include "s_sound.h"
#include "doomstat.h"
#include "m_menu.h"
#include "v_text.h"
#include "st_stuff.h"

#include "gi.h"

extern patch_t* 	hu_font[HU_FONTSIZE];

// temp for screenblocks (0-9)
int 				screenSize; 			

// -1 = no quicksave slot picked!
int 				quickSaveSlot;			

 // 1 = message to be printed
int 				messageToPrint;
// ...and here is the message string!
char*				messageString;			

// message x & y
int 				messx;					
int 				messy;
int 				messageLastMenuActive;

// timed message = no input from user
BOOL 				messageNeedsInput;	   

void	(*messageRoutine)(int response);

#define SAVESTRINGSIZE	24

// we are going to be entering a savegame string
int 				genStringEnter;
int					genStringLen;	// [RH] Max # of chars that can be entered
void	(*genStringEnd)(int slot);
int 				saveSlot;		// which slot to save in
int 				saveCharIndex;	// which char we're editing
// old save description before edit
char				saveOldString[SAVESTRINGSIZE];	

BOOL 				menuactive;

#define SKULLXOFF	-32
#define LINEHEIGHT	16

extern BOOL			sendpause;
char				savegamestrings[10][SAVESTRINGSIZE];

char				endstring[160];

menustack_t			MenuStack[16];
int					MenuStackDepth;

short				itemOn; 			// menu item skull is on
short				skullAnimCounter;	// skull animation counter
short				whichSkull; 		// which skull to draw
BOOL				drawSkull;			// [RH] don't always draw skull

// graphic name of skulls
char				skullName[2][9] = {"M_SKULL1", "M_SKULL2"};

// current menudef
oldmenu_t *currentMenu;						  

//
// PROTOTYPES
//
void M_NewGame(int choice);
void M_Episode(int choice);
void M_ChooseSkill(int choice);
void M_LoadGame(int choice);
void M_SaveGame(int choice);
void M_Options(int choice);
void M_EndGame(int choice);
void M_ReadThis(int choice);
void M_ReadThis2(int choice);
void M_QuitDOOM(int choice);

void M_ChangeDetail(int choice);
void M_StartGame(int choice);
void M_Sound(int choice);

void M_FinishReadThis(int choice);
void M_LoadSelect(int choice);
void M_SaveSelect(int choice);
void M_ReadSaveStrings(void);
void M_QuickSave(void);
void M_QuickLoad(void);

void M_DrawMainMenu(void);
void M_DrawReadThis1(void);
void M_DrawReadThis2(void);
void M_DrawNewGame(void);
void M_DrawEpisode(void);
void M_DrawOptions(void);
void M_DrawSound(void);
void M_DrawLoad(void);
void M_DrawSave(void);

void M_DrawSaveLoadBorder(int x,int y, int len);
void M_SetupNextMenu(oldmenu_t *menudef);
void M_DrawEmptyCell(oldmenu_t *menu,int item);
void M_DrawSelCell(oldmenu_t *menu,int item);
int  M_StringHeight(char *string);
void M_StartControlPanel(void);
void M_StartMessage(char *string,void *routine,BOOL input);
void M_StopMessage(void);
void M_ClearMenus (void);

// [RH] For player setup menu.
static void M_PlayerSetup (int choice);
static void M_PlayerSetupTicker (void);
static void M_PlayerSetupDrawer (void);
static void M_EditPlayerName (int choice);
static void M_EditPlayerTeam (int choice);
static void M_PlayerTeamChanged (int choice);
static void M_PlayerNameChanged (int choice);
static void M_SlidePlayerRed (int choice);
static void M_SlidePlayerGreen (int choice);
static void M_SlidePlayerBlue (int choice);
static void M_ChangeGender (int choice);
static void M_ChangeSkin (int choice);
static void M_ChangeAutoAim (int choice);
BOOL M_DemoNoPlay;

static screen_t FireScreen;
static BOOL FireGood;


//
// DOOM MENU
//
enum
{
	newgame = 0,
	loadgame,
	savegame,
	options,						// [RH] Moved
	playersetup,					// [RH] Player setup
	readthis,
	quitdoom,
	main_end
} main_e;

oldmenuitem_t MainMenu[]=
{
	{1,"M_NGAME",M_NewGame,'n'},
	{1,"M_LOADG",M_LoadGame,'l'},
	{1,"M_SAVEG",M_SaveGame,'s'},
	{1,"M_OPTION",M_Options,'o'},	// [RH] Moved
	{1,"M_PSETUP",M_PlayerSetup,'p'},	// [RH] Player setup
	// Another hickup with Special edition.
	{1,"M_RDTHIS",M_ReadThis,'r'},
	{1,"M_QUITG",M_QuitDOOM,'q'}
};

oldmenu_t MainDef =
{
	main_end,
	MainMenu,
	M_DrawMainMenu,
	97,64,
	0
};


//
// EPISODE SELECT
//
enum
{
	ep1,
	ep2,
	ep3,
	ep4,
	ep_end
} episodes_e;

oldmenuitem_t EpisodeMenu[]=
{
	{1,"M_EPI1", M_Episode,'k'},
	{1,"M_EPI2", M_Episode,'t'},
	{1,"M_EPI3", M_Episode,'i'},
	{1,"M_EPI4", M_Episode,'t'}
};

oldmenu_t EpiDef =
{
	ep4,	 			// # of menu items
	EpisodeMenu,		// oldmenuitem_t ->
	M_DrawEpisode,		// drawing routine ->
	48,63,				// x,y
	ep1 				// lastOn
};

//
// NEW GAME
//
enum
{
	killthings,
	toorough,
	hurtme,
	violence,
	nightmare,
	newg_end
} newgame_e;

oldmenuitem_t NewGameMenu[]=
{
	{1,"M_JKILL",		M_ChooseSkill, 'i'},
	{1,"M_ROUGH",		M_ChooseSkill, 'h'},
	{1,"M_HURT",		M_ChooseSkill, 'h'},
	{1,"M_ULTRA",		M_ChooseSkill, 'u'},
	{1,"M_NMARE",		M_ChooseSkill, 'n'}
};

oldmenu_t NewDef =
{
	newg_end,			// # of menu items
	NewGameMenu,		// oldmenuitem_t ->
	M_DrawNewGame,		// drawing routine ->
	48,63,				// x,y
	hurtme				// lastOn
};


//
// [RH] Player Setup Menu
//
byte FireRemap[256];

enum
{
	playername,
	playerteam,
	playerred,
	playergreen,
	playerblue,
	playersex,
	playerskin,
	playeraim,
	psetup_end
} psetup_e;

oldmenuitem_t PlayerSetupMenu[] =
{
	{ 1,"", M_EditPlayerName, 'n' },
	{ 1,"", M_EditPlayerTeam, 't' },
	{ 2,"", M_SlidePlayerRed, 'r' },
	{ 2,"", M_SlidePlayerGreen, 'g' },
	{ 2,"", M_SlidePlayerBlue, 'b' },
	{ 2,"", M_ChangeGender, 'e' },
	{ 2,"", M_ChangeSkin, 's' },
	{ 2,"", M_ChangeAutoAim, 'a' }
};

oldmenu_t PSetupDef = {
	psetup_end,
	PlayerSetupMenu,
	M_PlayerSetupDrawer,
	48,	47,
	playername
};

//
// OPTIONS MENU
//
// [RH] This menu is now handled in m_options.c
//
BOOL OptionsActive;

oldmenu_t OptionsDef =
{
	0,
	NULL,
	NULL,
	0,0,
	0
};

//
// Read This! MENU 1 & 2
//
enum
{
	rdthsempty1,
	read1_end
} read_e;

oldmenuitem_t ReadMenu1[] =
{
	{1,"",M_ReadThis2,0}
};

oldmenu_t	ReadDef1 =
{
	read1_end,
	ReadMenu1,
	M_DrawReadThis1,
	280,185,
	0
};

enum
{
	rdthsempty2,
	read2_end
} read_e2;

oldmenuitem_t ReadMenu2[]=
{
	{1,"",M_FinishReadThis,0}
};

oldmenu_t ReadDef2 =
{
	read2_end,
	ReadMenu2,
	M_DrawReadThis2,
	330,175,
	0
};

//
// LOAD GAME MENU
//
enum
{
	load1,
	load2,
	load3,
	load4,
	load5,
	load6,
	load7,
	load8,
	load_end
} load_e;

oldmenuitem_t LoadMenu[]=
{
	{1,"", M_LoadSelect,'1'},
	{1,"", M_LoadSelect,'2'},
	{1,"", M_LoadSelect,'3'},
	{1,"", M_LoadSelect,'4'},
	{1,"", M_LoadSelect,'5'},
	{1,"", M_LoadSelect,'6'},
	{1,"", M_LoadSelect,'7'},
	{1,"", M_LoadSelect,'8'},
};

oldmenu_t LoadDef =
{
	load_end,
	LoadMenu,
	M_DrawLoad,
	80,54,
	0
};

//
// SAVE GAME MENU
//
oldmenuitem_t SaveMenu[]=
{
	{1,"", M_SaveSelect,'1'},
	{1,"", M_SaveSelect,'2'},
	{1,"", M_SaveSelect,'3'},
	{1,"", M_SaveSelect,'4'},
	{1,"", M_SaveSelect,'5'},
	{1,"", M_SaveSelect,'6'},
	{1,"", M_SaveSelect,'7'},
	{1,"", M_SaveSelect,'8'}
};

oldmenu_t SaveDef =
{
	load_end,
	SaveMenu,
	M_DrawSave,
	80,54,
	0
};


// [RH] Most menus can now be accessed directly
// through console commands.
void Cmd_Menu_Main (void *plyr, int argc, char **argv)
{
	M_StartControlPanel ();
	M_SetupNextMenu (&MainDef);
	S_Sound (NULL, CHAN_VOICE, "switches/normbutn", 1, ATTN_NONE);
}

void Cmd_Menu_Load (void *plyr, int argc, char **argv)
{	// F3
	M_StartControlPanel ();
	S_Sound (NULL, CHAN_VOICE, "switches/normbutn", 1, ATTN_NONE);
	M_LoadGame(0);
}

void Cmd_Menu_Save (void *plyr, int argc, char **argv)
{	// F2
	M_StartControlPanel ();
	S_Sound (NULL, CHAN_VOICE, "switches/normbutn", 1, ATTN_NONE);
	M_SaveGame(0);
}

void Cmd_Menu_Help (void *plyr, int argc, char **argv)
{	// F1
	M_StartControlPanel ();
	drawSkull = false;
	M_SetupNextMenu (&ReadDef1);
	S_Sound (NULL, CHAN_VOICE, "switches/normbutn", 1, ATTN_NONE);
}

void Cmd_Quicksave (void *plyr, int argc, char **argv)
{	// F6
	S_Sound (NULL, CHAN_VOICE, "switches/normbutn", 1, ATTN_NONE);
	M_StartControlPanel ();
	M_QuickSave();
}

void Cmd_Quickload (void *plyr, int argc, char **argv)
{	// F9
	S_Sound (NULL, CHAN_VOICE, "switches/normbutn", 1, ATTN_NONE);
	M_StartControlPanel ();
	M_QuickLoad();
}

void Cmd_Menu_Endgame (void *plyr, int argc, char **argv)
{	// F7
	S_Sound (NULL, CHAN_VOICE, "switches/normbutn", 1, ATTN_NONE);
	M_StartControlPanel ();
	M_EndGame(0);
}

void Cmd_Menu_Quit (void *plyr, int argc, char **argv)
{	// F10
	S_Sound (NULL, CHAN_VOICE, "switches/normbutn", 1, ATTN_NONE);
	M_StartControlPanel ();
	M_QuitDOOM(0);
}

void Cmd_Menu_Game (void *plyr, int argc, char **argv)
{
	M_StartControlPanel ();
	S_Sound (NULL, CHAN_VOICE, "switches/normbutn", 1, ATTN_NONE);
	M_NewGame(0);
}
								
void Cmd_Menu_Options (void *plyr, int argc, char **argv)
{
	M_StartControlPanel ();
	S_Sound (NULL, CHAN_VOICE, "switches/normbutn", 1, ATTN_NONE);
	M_Options(0);
}

void Cmd_Menu_Player (void *plyr, int argc, char **argv)
{
	M_StartControlPanel ();
	S_Sound (NULL, CHAN_VOICE, "switches/normbutn", 1, ATTN_NONE);
	M_PlayerSetup(0);
}

void Cmd_Bumpgamma (void *plyr, int argc, char **argv)
{
	// [RH] Gamma correction tables are now generated
	// on the fly for *any* gamma level.
	// Q: What are reasonable limits to use here?

	float newgamma = gammalevel->value + 0.1;

	if (newgamma > 3.0)
		newgamma = 1.0;

	SetCVarFloat (gammalevel, newgamma);
	Printf (PRINT_HIGH, "Gamma correction level %g\n", gammalevel->value);
}

//
// M_ReadSaveStrings
//	read the strings from the savegame files
//
void M_ReadSaveStrings(void)
{
	int 	handle;
	int 	count;
	int 	i;
	char	name[256];
		
	for (i = 0; i < load_end; i++)
	{
		G_BuildSaveName (name, i);

		handle = open (name, O_RDONLY | 0, 0666);
		if (handle == -1)
		{
			strcpy (&savegamestrings[i][0], EMPTYSTRING);
			LoadMenu[i].status = 0;
			continue;
		}
		count = read (handle, &savegamestrings[i], SAVESTRINGSIZE);
		close (handle);
		LoadMenu[i].status = 1;
	}
}


//
// M_LoadGame & Cie.
//
void M_DrawLoad(void)
{
	int i;
		
	V_DrawPatchClean (72, 28, &screen, W_CacheLumpName ("M_LOADG",PU_CACHE));
	for (i = 0; i < load_end; i++)
	{
		M_DrawSaveLoadBorder (LoadDef.x, LoadDef.y+LINEHEIGHT*i, 24);
		V_DrawTextCleanMove (CR_RED, LoadDef.x, LoadDef.y+LINEHEIGHT*i, savegamestrings[i]);
	}
}



//
// Draw border for the savegame description
// [RH] Width of the border is variable
//
void M_DrawSaveLoadBorder (int x, int y, int len)
{
	int i;
		
	V_DrawPatchClean (x-8, y+7, &screen, W_CacheLumpName ("M_LSLEFT",PU_CACHE));
		
	for (i = 0; i < len; i++)
	{
		V_DrawPatchClean (x, y+7, &screen, W_CacheLumpName ("M_LSCNTR",PU_CACHE));
		x += 8;
	}

	V_DrawPatchClean (x, y+7, &screen, W_CacheLumpName ("M_LSRGHT",PU_CACHE));
}



//
// User wants to load this game
//
void M_LoadSelect (int choice)
{
	char name[256];

	G_BuildSaveName (name, choice);
	G_LoadGame (name);
	gamestate = gamestate == GS_FULLCONSOLE ? GS_HIDECONSOLE : gamestate;
	M_ClearMenus ();
	BorderNeedRefresh = true;
	if (quickSaveSlot == -2)
	{
		quickSaveSlot = choice;
	}
}

//
// Selected from DOOM menu
//
void M_LoadGame (int choice)
{
	if (netgame)
	{
		M_StartMessage(LOADNET,NULL,false);
		return;
	}
		
	M_SetupNextMenu(&LoadDef);
	M_ReadSaveStrings();
}


//
//	M_SaveGame & Cie.
//
void M_DrawSave(void)
{
	int i;
		
	V_DrawPatchClean (72,28,&screen,W_CacheLumpName("M_SAVEG",PU_CACHE));
	for (i = 0; i < load_end; i++)
	{
		M_DrawSaveLoadBorder(LoadDef.x,LoadDef.y+LINEHEIGHT*i,24);
		V_DrawTextCleanMove (CR_RED, LoadDef.x,LoadDef.y+LINEHEIGHT*i,savegamestrings[i]);
	}
		
	if (genStringEnter)
	{
		i = V_StringWidth(savegamestrings[saveSlot]);
		V_DrawTextCleanMove (CR_RED, LoadDef.x + i,LoadDef.y+LINEHEIGHT*saveSlot,"_");
	}
}

//
// M_Responder calls this when user is finished
//
void M_DoSave (int slot)
{
	G_SaveGame (slot,savegamestrings[slot]);
	M_ClearMenus ();
	BorderNeedRefresh = true;
	// PICK QUICKSAVE SLOT YET?
	if (quickSaveSlot == -2)
		quickSaveSlot = slot;
}

//
// User wants to save. Start string input for M_Responder
//
void M_SaveSelect (int choice)
{
	// we are going to be intercepting all chars
	genStringEnter = 1;
	genStringEnd = M_DoSave;
	genStringLen = SAVESTRINGSIZE-1;
	
	saveSlot = choice;
	strcpy(saveOldString,savegamestrings[choice]);
	if (!strcmp(savegamestrings[choice],EMPTYSTRING))
		savegamestrings[choice][0] = 0;
	saveCharIndex = strlen(savegamestrings[choice]);
}

//
// Selected from DOOM menu
//
void M_SaveGame (int choice)
{
	if (!usergame)
	{
		M_StartMessage(SAVEDEAD,NULL,false);
		return;
	}
		
	if (gamestate != GS_LEVEL)
		return;
		
	M_SetupNextMenu(&SaveDef);
	M_ReadSaveStrings();
}



//
//		M_QuickSave
//
char	tempstring[80];

void M_QuickSaveResponse(int ch)
{
	if (ch == 'y')
	{
		M_DoSave(quickSaveSlot);
		S_Sound (NULL, CHAN_VOICE, "switches/exitbutn", 1, ATTN_NONE);
	}
}

void M_QuickSave(void)
{
	if (!usergame)
	{
		S_Sound (NULL, CHAN_VOICE, "player/male/grunt1", 1, ATTN_NONE);
		return;
	}

	if (gamestate != GS_LEVEL)
		return;
		
	if (quickSaveSlot < 0)
	{
		M_StartControlPanel();
		M_ReadSaveStrings();
		M_SetupNextMenu(&SaveDef);
		quickSaveSlot = -2; 	// means to pick a slot now
		return;
	}
	sprintf(tempstring,QSPROMPT,savegamestrings[quickSaveSlot]);
	M_StartMessage(tempstring,M_QuickSaveResponse,true);
}



//
// M_QuickLoad
//
void M_QuickLoadResponse(int ch)
{
	if (ch == 'y')
	{
		M_LoadSelect(quickSaveSlot);
		S_Sound (NULL, CHAN_VOICE, "switches/exitbutn", 1, ATTN_NONE);
	}
}


void M_QuickLoad(void)
{
	if (netgame)
	{
		M_StartMessage(QLOADNET,NULL,false);
		return;
	}
		
	if (quickSaveSlot < 0)
	{
		M_StartControlPanel();
		M_LoadGame (0);
		return;
	}
	sprintf(tempstring,QLPROMPT,savegamestrings[quickSaveSlot]);
	M_StartMessage(tempstring,M_QuickLoadResponse,true);
}




//
// Read This Menus
// Had a "quick hack to fix romero bug"
//
void M_DrawReadThis1(void)
{
	V_DrawPatchIndirect (0, 0, &screen,
		W_CacheLumpName (gameinfo.info.infoPage[0], PU_CACHE));
}



//
// Read This Menus - optional second page.
//
void M_DrawReadThis2(void)
{
	V_DrawPatchIndirect (0, 0, &screen,
		W_CacheLumpName (gameinfo.info.infoPage[1], PU_CACHE));
}


//
// M_DrawMainMenu
//
void M_DrawMainMenu(void)
{
	V_DrawPatchClean (94,2,&screen,W_CacheLumpName("M_DOOM",PU_CACHE));
}




//
// M_NewGame
//
void M_DrawNewGame(void)
{
	V_DrawPatchClean (96,14,&screen,W_CacheLumpName("M_NEWG",PU_CACHE));
	V_DrawPatchClean (54,38,&screen,W_CacheLumpName("M_SKILL",PU_CACHE));
}

void M_NewGame(int choice)
{
	if (netgame && !demoplayback)
	{
		M_StartMessage(NEWGAME,NULL,false);
		return;
	}
		
	if (gameinfo.flags & GI_MAPxx)
		M_SetupNextMenu(&NewDef);
	else
		M_SetupNextMenu(&EpiDef);
}


//
//		M_Episode
//
int 	epi;

void M_DrawEpisode(void)
{
	V_DrawPatchClean (54,38,&screen,W_CacheLumpName("M_EPISOD",PU_CACHE));
}

void M_VerifyNightmare(int ch)
{
	if (ch != 'y')
		return;
		
	SetCVarFloat (gameskill, nightmare);
	G_DeferedInitNew (CalcMapName (epi+1, 1));
	gamestate = gamestate == GS_FULLCONSOLE ? GS_HIDECONSOLE : gamestate;
	M_ClearMenus ();
}

void M_ChooseSkill(int choice)
{
	if (choice == nightmare)
	{
		M_StartMessage(NIGHTMARE,M_VerifyNightmare,true);
		return;
	}

	SetCVarFloat (gameskill, (float)choice);
	gamestate = gamestate == GS_FULLCONSOLE ? GS_HIDECONSOLE : gamestate;
	G_DeferedInitNew (CalcMapName (epi+1, 1));
	gamestate = gamestate == GS_FULLCONSOLE ? GS_HIDECONSOLE : gamestate;
	M_ClearMenus ();
}

void M_Episode (int choice)
{
	if ((gameinfo.flags & GI_SHAREWARE) && choice)
	{
		M_StartMessage(SWSTRING,NULL,false);
		M_SetupNextMenu(&ReadDef1);
		return;
	}

	epi = choice;
	M_SetupNextMenu(&NewDef);
}



//
// M_Options
//
void M_DrawOptions(void)
{
	V_DrawPatchClean (108,15,&screen,W_CacheLumpName("M_OPTTTL",PU_CACHE));
}

void M_Options(int choice)
{
	//M_SetupNextMenu(&OptionsDef);
	OptionsActive = M_StartOptionsMenu();
}




//
// M_EndGame
//
void M_EndGameResponse(int ch)
{
	if (ch != 'y')
		return;
				
	currentMenu->lastOn = itemOn;
	M_ClearMenus ();
	D_StartTitle ();
}

void M_EndGame(int choice)
{
	choice = 0;
	if (!usergame)
	{
		S_Sound (NULL, CHAN_VOICE, "player/male/grunt1", 1, ATTN_NONE);
		return;
	}
		
	if (netgame)
	{
		M_StartMessage(NETEND,NULL,false);
		return;
	}
		
	M_StartMessage(ENDGAME,M_EndGameResponse,true);
}




//
// M_ReadThis
//
void M_ReadThis(int choice)
{
	choice = 0;
	drawSkull = false;
	M_SetupNextMenu(&ReadDef1);
}

void M_ReadThis2(int choice)
{
	choice = 0;
	drawSkull = false;
	M_SetupNextMenu(&ReadDef2);
}

void M_FinishReadThis(int choice)
{
	choice = 0;
	drawSkull = true;
	MenuStackDepth = 0;
	M_SetupNextMenu(&MainDef);
}

//
// M_QuitDOOM
//

void M_QuitResponse(int ch)
{
	if (ch != 'y')
		return;
	if (!netgame)
	{
		if (gameinfo.quitSounds)
		{
			S_Sound (NULL, CHAN_VOICE, gameinfo.quitSounds[(gametic>>2)&7],
				1, ATTN_SURROUND);
			I_WaitVBL (105);
		}
	}
	exit (0);
}

void M_QuitDOOM (int choice)
{
  // We pick index 0 which is language sensitive,
  //  or one at random, between 1 and maximum number.
  if (language != english )
	sprintf(endstring,"%s\n\n%s", endmsg[0], DOSY );
  else
	sprintf(endstring,"%s\n\n%s", endmsg[ (gametic%(NUM_QUITMESSAGES-2))+1 ], DOSY);
  
  M_StartMessage(endstring,M_QuitResponse,true);
}


//
// [RH] Player Setup Menu code
//
void M_DrawSlider (int x, int y, float min, float max, float cur);

static char *genders[3] = { "male", "female", "cyborg" };
static state_t *PlayerState;
static int PlayerTics;

extern cvar_t *name, *team;

static void M_PlayerSetup (int choice)
{
	choice = 0;
	strcpy (savegamestrings[0], name->string);
	strcpy (savegamestrings[1], team->string);
	M_DemoNoPlay = true;
	if (demoplayback)
		G_CheckDemoStatus ();
	M_SetupNextMenu (&PSetupDef);
	PlayerState = &states[mobjinfo[MT_PLAYER].seestate];
	PlayerTics = PlayerState->tics;
	if (!FireGood)
		FireGood = V_AllocScreen (&FireScreen, 72, 72 + 5, 8);
}

static void M_PlayerSetupTicker (void)
{
	// Based on code in f_finale.c
	if (--PlayerTics > 0)
		return;

	if (PlayerState->tics == -1 || PlayerState->nextstate == S_NULL) {
		PlayerState = &states[mobjinfo[MT_PLAYER].seestate];
	} else {
		PlayerState = &states[PlayerState->nextstate];
	}
	PlayerTics = PlayerState->tics;
}

static void M_PlayerSetupDrawer (void)
{
	// Draw title
	{
		patch_t *patch = W_CacheLumpName ("M_PSTTL", PU_CACHE);

		V_DrawPatchClean (160 - (SHORT(patch->width) >> 1),
						  PSetupDef.y - (SHORT(patch->height) * 3),
						  &screen, patch);
	}

	// Draw player name box
	V_DrawTextCleanMove (CR_RED, PSetupDef.x, PSetupDef.y, "Name");
	M_DrawSaveLoadBorder (PSetupDef.x + 56, PSetupDef.y, MAXPLAYERNAME+1);
	V_DrawTextCleanMove (CR_RED, PSetupDef.x + 56, PSetupDef.y, savegamestrings[0]);

	// Draw player team box
	V_DrawTextCleanMove (CR_RED, PSetupDef.x, PSetupDef.y + LINEHEIGHT, "Team");
	M_DrawSaveLoadBorder (PSetupDef.x + 56, PSetupDef.y + LINEHEIGHT, MAXPLAYERNAME+1);
	V_DrawTextCleanMove (CR_RED, PSetupDef.x + 56, PSetupDef.y + LINEHEIGHT, savegamestrings[1]);

	// Draw cursor for either of the above
	if (genStringEnter)
		V_DrawTextCleanMove (CR_RED, PSetupDef.x + V_StringWidth(savegamestrings[saveSlot]) + 56, PSetupDef.y + ((saveSlot == 0) ? 0 : LINEHEIGHT), "_");

	// Draw player character
	{
		int x = 320 - 88 - 32, y = PSetupDef.y + LINEHEIGHT*3 - 14;

		x = (x-160)*CleanXfac+(screen.width>>1);
		y = (y-100)*CleanYfac+(screen.height>>1);
		if (!FireGood) {
			V_Clear (x, y, x + 72 * CleanXfac, y + 72 * CleanYfac, &screen, 34);
		} else {
			// [RH] The following fire code is based on the PTC fire demo
			int a, b;
			byte *from;
			int width, height, pitch;

			V_LockScreen (&FireScreen);

			width = FireScreen.width;
			height = FireScreen.height;
			pitch = FireScreen.pitch;

			from = FireScreen.buffer + (height - 3) * pitch;
			for (a = 0; a < width; a++, from++) {
				*from = *(from + (pitch << 1)) = M_Random();
			}

			from = FireScreen.buffer;
			for (b = 0; b < FireScreen.height - 4; b += 2) {
				byte *pixel = from;

				// special case: first pixel on line
				byte *p = pixel + (pitch << 1);
				unsigned int top = *p + *(p + width - 1) + *(p + 1);
				unsigned int bottom = *(pixel + (pitch << 2));
				unsigned int c1 = (top + bottom) >> 2;
				if (c1 > 1) c1--;
				*pixel = c1;
				*(pixel + pitch) = (c1 + bottom) >> 1;
				pixel++;

				// main line loop
				for (a = 1; a < width-1; a++)
				{
					// sum top pixels
					p = pixel + (pitch << 1);
					top = *p + *(p - 1) + *(p + 1);

					// bottom pixel
					bottom = *(pixel + (pitch << 2));

					// combine pixels
					c1 = (top + bottom) >> 2;
					if (c1 > 1) c1--;

					// store pixels
					*pixel = c1;
					*(pixel + pitch) = (c1 + bottom) >> 1;		// interpolate

					// next pixel
					pixel++;
				}

				// special case: last pixel on line
				p = pixel + (pitch << 1);
				top = *p + *(p - 1) + *(p - width + 1);
				bottom = *(pixel + (pitch << 2));
				c1 = (top + bottom) >> 2;
				if (c1 > 1) c1--;
				*pixel = c1;
				*(pixel + pitch) = (c1 + bottom) >> 1;

				// next line
				from += pitch << 1;
			}

			y--;
			pitch = screen.pitch;
			switch (CleanXfac) {
				case 1:
					for (b = 0; b < FireScreen.height; b++) {
						byte *to = screen.buffer + y * screen.pitch + x;
						from = FireScreen.buffer + b * FireScreen.pitch;
						y += CleanYfac;

						for (a = 0; a < FireScreen.width; a++, to++, from++) {
							int c;
							for (c = CleanYfac; c; c--)
								*(to + pitch*c) = FireRemap[*from];
						}
					}
					break;

				case 2:
					for (b = 0; b < FireScreen.height; b++) {
						byte *to = screen.buffer + y * screen.pitch + x;
						from = FireScreen.buffer + b * FireScreen.pitch;
						y += CleanYfac;

						for (a = 0; a < FireScreen.width; a++, to += 2, from++) {
							int c;
							for (c = CleanYfac; c; c--) {
								*(to + pitch*c) = FireRemap[*from];
								*(to + pitch*c + 1) = FireRemap[*from];
							}
						}
					}
					break;

				case 3:
					for (b = 0; b < FireScreen.height; b++) {
						byte *to = screen.buffer + y * screen.pitch + x;
						from = FireScreen.buffer + b * FireScreen.pitch;
						y += CleanYfac;

						for (a = 0; a < FireScreen.width; a++, to += 3, from++) {
							int c;
							for (c = CleanYfac; c; c--) {
								*(to + pitch*c) = FireRemap[*from];
								*(to + pitch*c + 1) = FireRemap[*from];
								*(to + pitch*c + 2) = FireRemap[*from];
							}
						}
					}
					break;

				case 4:
				default:
					for (b = 0; b < FireScreen.height; b++) {
						byte *to = screen.buffer + y * screen.pitch + x;
						from = FireScreen.buffer + b * FireScreen.pitch;
						y += CleanYfac;

						for (a = 0; a < FireScreen.width; a++, to += 4, from++) {
							int c;
							for (c = CleanYfac; c; c--) {
								*(to + pitch*c) = FireRemap[*from];
								*(to + pitch*c + 1) = FireRemap[*from];
								*(to + pitch*c + 2) = FireRemap[*from];
								*(to + pitch*c + 3) = FireRemap[*from];
							}
						}
					}
					break;
			}
			V_UnlockScreen (&FireScreen);
		}
	}
	{
		spriteframe_t *sprframe =
			&sprites[skins[players[consoleplayer].userinfo.skin].sprite].spriteframes[PlayerState->frame & FF_FRAMEMASK];

		V_ColorMap = translationtables + consoleplayer * 256;
		V_DrawTranslatedPatchClean (320 - 52 - 32, PSetupDef.y + LINEHEIGHT*3 + 46,
									&screen,
									W_CacheLumpNum (sprframe->lump[0], PU_CACHE));
	}
	V_DrawPatchClean (320 - 88 - 32 + 36, PSetupDef.y + LINEHEIGHT*3 + 22,
					  &screen,
					  W_CacheLumpName ("M_PBOX", PU_CACHE));

	// Draw player color sliders
	//V_DrawTextCleanMove (CR_GREY, PSetupDef.x, PSetupDef.y + LINEHEIGHT, "Color");

	V_DrawTextCleanMove (CR_RED, PSetupDef.x, PSetupDef.y + LINEHEIGHT*2, "Red");
	V_DrawTextCleanMove (CR_RED, PSetupDef.x, PSetupDef.y + LINEHEIGHT*3, "Green");
	V_DrawTextCleanMove (CR_RED, PSetupDef.x, PSetupDef.y + LINEHEIGHT*4, "Blue");

	{
		int x = V_StringWidth ("Green") + 8 + PSetupDef.x;
		int color = players[consoleplayer].userinfo.color;

		M_DrawSlider (x, PSetupDef.y + LINEHEIGHT*2, 0.0f, 255.0f, RPART(color));
		M_DrawSlider (x, PSetupDef.y + LINEHEIGHT*3, 0.0f, 255.0f, GPART(color));
		M_DrawSlider (x, PSetupDef.y + LINEHEIGHT*4, 0.0f, 255.0f, BPART(color));
	}

	// Draw gender setting
	{
		int x = V_StringWidth ("Gender") + 8 + PSetupDef.x;
		V_DrawTextCleanMove (CR_RED, PSetupDef.x, PSetupDef.y + LINEHEIGHT*5, "Gender");
		V_DrawTextCleanMove (CR_GREY, x, PSetupDef.y + LINEHEIGHT*5,
							  genders[players[consoleplayer].userinfo.gender]);
	}

	// Draw skin setting
	{
		int x = V_StringWidth ("Skin") + 8 + PSetupDef.x;
		V_DrawTextCleanMove (CR_RED, PSetupDef.x, PSetupDef.y + LINEHEIGHT*6, "Skin");
		V_DrawTextCleanMove (CR_GREY, x, PSetupDef.y + LINEHEIGHT*6,
							  skins[players[consoleplayer].userinfo.skin].name);
	}

	// Draw autoaim setting
	{
		int x = V_StringWidth ("Autoaim") + 8 + PSetupDef.x;
		float aim = autoaim->value;

		V_DrawTextCleanMove (CR_RED, PSetupDef.x, PSetupDef.y + LINEHEIGHT*7, "Autoaim");
		V_DrawTextCleanMove (CR_GREY, x, PSetupDef.y + LINEHEIGHT*7,
			aim == 0 ? "Never" :
			aim <= 0.25 ? "Very Low" :
			aim <= 0.5 ? "Low" :
			aim <= 1 ? "Medium" :
			aim <= 2 ? "High" :
			aim <= 3 ? "Very High" : "Always");
	}
}

static void M_ChangeGender (int choice)
{
	int gender = players[consoleplayer].userinfo.gender;

	if (!choice)
		gender = (gender == 0) ? 2 : gender - 1;
	else
		gender = (gender == 2) ? 0 : gender + 1;

	cvar_set ("gender", genders[gender]);
}

static void M_ChangeSkin (int choice)
{
	int skin = players[consoleplayer].userinfo.skin;

	if (!choice)
		skin = (skin == 0) ? numskins - 1 : skin - 1;
	else
		skin = (skin < numskins - 1) ? skin + 1 : 0;

	cvar_set ("skin", skins[skin].name);
}

static void M_ChangeAutoAim (int choice)
{
	static const float ranges[] = { 0, 0.25, 0.5, 1, 2, 3, 5000 };
	float aim = autoaim->value;
	int i;

	if (!choice) {
		// Select a lower autoaim

		for (i = 6; i >= 1; i--) {
			if (aim >= ranges[i]) {
				aim = ranges[i - 1];
				break;
			}
		}
	} else {
		// Select a higher autoaim

		for (i = 5; i >= 0; i--) {
			if (aim >= ranges[i]) {
				aim = ranges[i + 1];
				break;
			}
		}
	}

	SetCVarFloat (autoaim, aim);
}

static void M_EditPlayerName (int choice)
{
	// we are going to be intercepting all chars
	genStringEnter = 1;
	genStringEnd = M_PlayerNameChanged;
	genStringLen = MAXPLAYERNAME;
	
	saveSlot = 0;
	strcpy(saveOldString,savegamestrings[0]);
	if (!strcmp(savegamestrings[0],EMPTYSTRING))
		savegamestrings[0][0] = 0;
	saveCharIndex = strlen(savegamestrings[0]);
}

static void M_PlayerNameChanged (int choice)
{
	char command[SAVESTRINGSIZE+8];

	sprintf (command, "name \"%s\"", savegamestrings[0]);
	AddCommandString (command);
}

static void M_EditPlayerTeam (int choice)
{
	// we are going to be intercepting all chars
	genStringEnter = 1;
	genStringEnd = M_PlayerTeamChanged;
	genStringLen = MAXPLAYERNAME;
	
	saveSlot = 1;
	strcpy(saveOldString,savegamestrings[1]);
	if (!strcmp(savegamestrings[1],EMPTYSTRING))
		savegamestrings[1][0] = 0;
	saveCharIndex = strlen(savegamestrings[1]);
}

static void M_PlayerTeamChanged (int choice)
{
	char command[SAVESTRINGSIZE+8];

	sprintf (command, "team \"%s\"", savegamestrings[1]);
	AddCommandString (command);
}


static void SendNewColor (int red, int green, int blue)
{
	char command[24];

	sprintf (command, "color \"%02x %02x %02x\"", red, green, blue);
	AddCommandString (command);
}

static void M_SlidePlayerRed (int choice)
{
	int color = players[consoleplayer].userinfo.color;
	int red = RPART(color);

	if (choice == 0) {
		red -= 16;
		if (red < 0)
			red = 0;
	} else {
		red += 16;
		if (red > 255)
			red = 255;
	}

	SendNewColor (red, GPART(color), BPART(color));
}

static void M_SlidePlayerGreen (int choice)
{
	int color = players[consoleplayer].userinfo.color;
	int green = GPART(color);

	if (choice == 0) {
		green -= 16;
		if (green < 0)
			green = 0;
	} else {
		green += 16;
		if (green > 255)
			green = 255;
	}

	SendNewColor (RPART(color), green, BPART(color));
}

static void M_SlidePlayerBlue (int choice)
{
	int color = players[consoleplayer].userinfo.color;
	int blue = BPART(color);

	if (choice == 0) {
		blue -= 16;
		if (blue < 0)
			blue = 0;
	} else {
		blue += 16;
		if (blue > 255)
			blue = 255;
	}

	SendNewColor (RPART(color), GPART(color), blue);
}


//
//		Menu Functions
//
void M_DrawEmptyCell (oldmenu_t *menu, int item)
{
	V_DrawPatchClean (menu->x - 10,		menu->y+item*LINEHEIGHT - 1, &screen,
					   W_CacheLumpName("M_CELL1",PU_CACHE));
}

void M_DrawSelCell (oldmenu_t *menu, int item)
{
	V_DrawPatchClean (menu->x - 10,		menu->y+item*LINEHEIGHT - 1, &screen,
					   W_CacheLumpName("M_CELL2",PU_CACHE));
}


void M_StartMessage (char *string, void *routine, BOOL input)
{
	C_HideConsole ();
	messageLastMenuActive = menuactive;
	messageToPrint = 1;
	messageString = string;
	messageRoutine = routine;
	messageNeedsInput = input;
	menuactive = true;
	return;
}



void M_StopMessage (void)
{
	menuactive = messageLastMenuActive;
	messageToPrint = 0;
}


//
//		Find string height from hu_font chars
//
int M_StringHeight(char* string)
{
	int h;
	int height = SHORT(hu_font[0]->height);
		
	h = height;
	while (*string)
		if ((*string++) == '\n')
			h += height;
				
	return h;
}



//
// CONTROL PANEL
//

//
// M_Responder
//
BOOL M_Responder (event_t* ev)
{
	int ch, ch2;
	int i;

	ch = ch2 = -1;

	if (ev->type == ev_keydown) {
		ch = ev->data1; 		// scancode
		ch2 = ev->data2;		// ASCII
	}
	
	if (ch == -1 || chatmodeon)
		return false;

	if (menuactive && OptionsActive) {
		M_OptResponder (ev);
		return true;
	}
	
	// Save Game string input
	// [RH] and Player Name string input
	if (genStringEnter)
	{
		switch(ch)
		{
		  case KEY_BACKSPACE:
			if (saveCharIndex > 0)
			{
				saveCharIndex--;
				savegamestrings[saveSlot][saveCharIndex] = 0;
			}
			break;

		  case KEY_ESCAPE:
			genStringEnter = 0;
			M_ClearMenus ();
			strcpy(&savegamestrings[saveSlot][0],saveOldString);
			break;
								
		  case KEY_ENTER:
			genStringEnter = 0;
			M_ClearMenus ();
			if (savegamestrings[saveSlot][0])
				genStringEnd(saveSlot);	// [RH] Function to call when enter is pressed
			break;
								
		  default:
			ch = toupper(ev->data3);	// [RH] Use user keymap
			if (ch != 32)
				if (ch-HU_FONTSTART < 0 || ch-HU_FONTSTART >= HU_FONTSIZE)
					break;
			if (ch >= 32 && ch <= 127 &&
				saveCharIndex < genStringLen &&
				V_StringWidth(savegamestrings[saveSlot]) <
				(genStringLen-1)*8)
			{
				savegamestrings[saveSlot][saveCharIndex++] = ch;
				savegamestrings[saveSlot][saveCharIndex] = 0;
			}
			break;
		}
		return true;
	}
	
	// Take care of any messages that need input
	if (messageToPrint)
	{
		if (messageNeedsInput == true &&
			!(ch2 == ' ' || ch2 == 'n' || ch2 == 'y' || ch == KEY_ESCAPE))
			return false;
				
		menuactive = messageLastMenuActive;
		messageToPrint = 0;
		if (messageRoutine)
			messageRoutine(ch2);
						
		menuactive = false;
		SB_state = -1;	// refresh the statbar
		BorderNeedRefresh = true;
		S_Sound (NULL, CHAN_VOICE, "switches/exitbutn", 1, ATTN_NONE);
		return true;
	}
		
	// [RH] F-Keys are now just normal keys that can be bound,
	//		so they aren't checked here anymore.
	
	// If devparm is set, pressing F1 always takes a screenshot no matter
	// what it's bound to. (for those who don't bother to read the docs)
	if (devparm && ch == KEY_F1) {
		G_ScreenShot (NULL);
		return true;
	}

	// Pop-up menu?
	if (!menuactive)
	{
		if (ch == KEY_ESCAPE)
		{
			M_StartControlPanel ();
			M_SetupNextMenu (&MainDef);
			S_Sound (NULL, CHAN_VOICE, "switches/normbutn", 1, ATTN_NONE);
			return true;
		}
		return false;
	}

	
	// Keys usable within menu
	switch (ch)
	{
	  case KEY_DOWNARROW:
		do
		{
			if (itemOn+1 > currentMenu->numitems-1)
				itemOn = 0;
			else itemOn++;
			S_Sound (NULL, CHAN_VOICE, "plats/pt1_stop", 1, ATTN_NONE);
		} while(currentMenu->menuitems[itemOn].status==-1);
		return true;
				
	  case KEY_UPARROW:
		do
		{
			if (!itemOn)
				itemOn = currentMenu->numitems-1;
			else itemOn--;
			S_Sound (NULL, CHAN_VOICE, "plats/pt1_stop", 1, ATTN_NONE);
		} while(currentMenu->menuitems[itemOn].status==-1);
		return true;

	  case KEY_LEFTARROW:
		if (currentMenu->menuitems[itemOn].routine &&
			currentMenu->menuitems[itemOn].status == 2)
		{
			S_Sound (NULL, CHAN_VOICE, "plats/pt1_mid", 1, ATTN_NONE);
			currentMenu->menuitems[itemOn].routine(0);
		}
		return true;
				
	  case KEY_RIGHTARROW:
		if (currentMenu->menuitems[itemOn].routine &&
			currentMenu->menuitems[itemOn].status == 2)
		{
			S_Sound (NULL, CHAN_VOICE, "plats/pt1_mid", 1, ATTN_NONE);
			currentMenu->menuitems[itemOn].routine(1);
		}
		return true;

	  case KEY_ENTER:
		if (currentMenu->menuitems[itemOn].routine &&
			currentMenu->menuitems[itemOn].status)
		{
			currentMenu->lastOn = itemOn;
			if (currentMenu->menuitems[itemOn].status == 2)
			{
				currentMenu->menuitems[itemOn].routine(1);		// right arrow
				S_Sound (NULL, CHAN_VOICE, "plats/pt1_mid", 1, ATTN_NONE);
			}
			else
			{
				currentMenu->menuitems[itemOn].routine(itemOn);
				S_Sound (NULL, CHAN_VOICE, "weapons/pistol", 1, ATTN_NONE);
			}
		}
		return true;
				
	  // [RH] Escape now moves back one menu instead of
	  //	  quitting the menu system. Thus, backspace
	  //	  is now ignored.
	  case KEY_ESCAPE:
		currentMenu->lastOn = itemOn;
		M_PopMenuStack ();
		return true;
		
	  default:
		if (ch2) {
			for (i = itemOn+1;i < currentMenu->numitems;i++)
				if (currentMenu->menuitems[i].alphaKey == ch2)
				{
					itemOn = i;
					S_Sound (NULL, CHAN_VOICE, "plats/pt1_stop", 1, ATTN_NONE);
					return true;
				}
			for (i = 0;i <= itemOn;i++)
				if (currentMenu->menuitems[i].alphaKey == ch2)
				{
					itemOn = i;
					S_Sound (NULL, CHAN_VOICE, "plats/pt1_stop", 1, ATTN_NONE);
					return true;
				}
		}
		break;
		
	}

	// [RH] Menu now eats all keydown events while active
	if (ev->type == ev_keydown)
		return true;
	else
		return false;
}



//
// M_StartControlPanel
//
void M_StartControlPanel (void)
{
	// intro might call this repeatedly
	if (menuactive)
		return;
	
	drawSkull = true;
	MenuStackDepth = 0;
	menuactive = 1;
	currentMenu = &MainDef;
	itemOn = currentMenu->lastOn;
	C_HideConsole ();				// [RH] Make sure console goes bye bye.
	OptionsActive = false;			// [RH] Make sure none of the options menus appear.
	I_PauseMouse ();				// [RH] Give the mouse back in windowed modes.
}


//
// M_Drawer
// Called after the view has been rendered,
// but before it has been blitted.
//
void M_Drawer (void)
{
	static short	x;
	static short	y;
	short			i;
	short			max;
	char			string[80];
	int 			start;

	// Horiz. & Vertically center string and print it.
	if (messageToPrint)
	{
		V_DimScreen (&screen);
		BorderNeedRefresh = true;
		SB_state = -1;

		start = 0;
		y = 100 - M_StringHeight(messageString)/2;
		while(*(messageString+start))
		{
			for (i = 0;i < (int)strlen(messageString+start);i++)
				if (*(messageString+start+i) == '\n')
				{
					memset(string,0,80);
					strncpy(string,messageString+start,i);
					start += i+1;
					break;
				}
								
			if (i == (int)strlen(messageString+start))
			{
				strcpy(string,messageString+start);
				start += i;
			}
								
			x = 160 - V_StringWidth(string)/2;
			V_DrawTextCleanMove (CR_RED, x,y,string);
			y += SHORT(hu_font[0]->height);
		}
		return;
	}

	if (!menuactive)
		return;

	V_DimScreen (&screen);
	BorderNeedRefresh = true;
	SB_state = -1;

	if (OptionsActive) {
		M_OptDrawer ();
	} else {
		if (currentMenu->routine)
			currentMenu->routine(); 		// call Draw routine
	
		// DRAW MENU
		x = currentMenu->x;
		y = currentMenu->y;
		max = currentMenu->numitems;

		for (i=0;i<max;i++)
		{
			if (currentMenu->menuitems[i].name[0])
				V_DrawPatchClean (x,y,&screen,
								   W_CacheLumpName(currentMenu->menuitems[i].name ,PU_CACHE));
			y += LINEHEIGHT;
		}

		
		// DRAW SKULL
		if (drawSkull) {
			V_DrawPatchClean(x + SKULLXOFF,currentMenu->y - 5 + itemOn*LINEHEIGHT, &screen,
							 W_CacheLumpName(skullName[whichSkull],PU_CACHE));
		}
	}
}


//
// M_ClearMenus
//
void M_ClearMenus (void)
{
	if (FireGood) {
		V_FreeScreen (&FireScreen);
		FireGood = false;
	}
	menuactive = MenuStackDepth = 0;
	drawSkull = true;
	M_DemoNoPlay = false;
	C_HideConsole ();		// [RH] Hide the console if we can.
	I_ResumeMouse ();		// [RH] Recapture the mouse in windowed modes.
	BorderNeedRefresh = true;
	// if (!netgame && usergame && paused)
	//		 sendpause = true;
}




//
// M_SetupNextMenu
//
void M_SetupNextMenu (oldmenu_t *menudef)
{
	MenuStack[MenuStackDepth].menu.old = menudef;
	MenuStack[MenuStackDepth].isNewStyle = false;
	MenuStack[MenuStackDepth].drawSkull = drawSkull;
	MenuStackDepth++;

	currentMenu = menudef;
	itemOn = currentMenu->lastOn;
}


void M_PopMenuStack (void)
{
	M_DemoNoPlay = false;
	if (MenuStackDepth > 1) {
		MenuStackDepth -= 2;
		if (MenuStack[MenuStackDepth].isNewStyle) {
			OptionsActive = true;
			CurrentMenu = MenuStack[MenuStackDepth].menu.new;
			CurrentItem = CurrentMenu->lastOn;
		} else {
			OptionsActive = false;
			currentMenu = MenuStack[MenuStackDepth].menu.old;
			itemOn = currentMenu->lastOn;
		}
		drawSkull = MenuStack[MenuStackDepth].drawSkull;
		MenuStackDepth++;
		S_Sound (NULL, CHAN_VOICE, "switches/normbutn", 1, ATTN_NONE);
	} else {
		M_ClearMenus ();
		S_Sound (NULL, CHAN_VOICE, "switches/exitbutn", 1, ATTN_NONE);
	}
}


//
// M_Ticker
//
void M_Ticker (void)
{
	if (--skullAnimCounter <= 0)
	{
		whichSkull ^= 1;
		skullAnimCounter = 8;
	}
	if (currentMenu == &PSetupDef)
		M_PlayerSetupTicker ();
}


//
// M_Init
//
extern cvar_t *screenblocks;

void M_Init (void)
{
	int i;

	currentMenu = &MainDef;
	OptionsActive = false;
	menuactive = 0;
	itemOn = currentMenu->lastOn;
	whichSkull = 0;
	skullAnimCounter = 10;
	drawSkull = true;
	screenSize = (int)screenblocks->value - 3;
	messageToPrint = 0;
	messageString = NULL;
	messageLastMenuActive = menuactive;
	quickSaveSlot = -1;

	// Here we could catch other version dependencies,
	//	like HELP1/2, and four episodes.
	switch (gameinfo.flags & GI_MENUHACK)
	{
		case GI_MENUHACK_COMMERCIAL:
			// This is used because DOOM 2 had only one HELP
			//	page. I use CREDIT as second page now, but
			//	kept this hack for educational purposes.
			MainMenu[readthis] = MainMenu[quitdoom];
			MainDef.numitems--;
			MainDef.y += 8;
			ReadDef1.routine = M_DrawReadThis1;
			ReadDef1.x = 330;
			ReadDef1.y = 165;
			ReadMenu1[0].routine = M_FinishReadThis;
			break;
		case GI_MENUHACK_RETAIL:
			// add the fourth episode.
			EpiDef.numitems++;
			break;
		case GI_MENUHACK_EXTENDED:
//			EpisodeMenu.itemCount = 5;
//			EpisodeMenu.y -= ITEM_HEIGHT;
			break;
		default:
			break;
	}
	M_OptInit ();

	// [RH] Build a palette translation table for the fire
	for (i = 0; i < 255; i++)
		FireRemap[i] = BestColor (DefaultPalette->basecolors, i, 0, 0, DefaultPalette->numcolors);
}

