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

#include "s_sound.h"

#include "doomstat.h"

// Data.
#include "sounds.h"

#include "m_menu.h"

#include "v_text.h"


extern patch_t* 	hu_font[HU_FONTSIZE];

extern BOOL			chat_on;				// in heads-up code

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
void	(*genStringEnd)(int slot);
int 				saveSlot;		// which slot to save in
int 				saveCharIndex;	// which char we're editing
// old save description before edit
char				saveOldString[SAVESTRINGSIZE];	

BOOL 				inhelpscreens;
BOOL 				menuactive;

#define SKULLXOFF	-32
#define LINEHEIGHT	16

extern BOOL			sendpause;
char				savegamestrings[10][SAVESTRINGSIZE];

char	endstring[160];


//
// MENU TYPEDEFS
//
typedef struct
{
	// -1 = no cursor here, 1 = ok, 2 = arrows ok
	short		status;
	
	char		name[10];
	
	// choice = menu item #.
	// if status = 2,
	//	 choice=0:leftarrow,1:rightarrow
	void		(*routine)(int choice);
	
	// hotkey in menu
	char		alphaKey;						
} menuitem_t;



typedef struct menu_s
{
	short				numitems;		// # of menu items
	struct menu_s*		prevMenu;		// previous menu
	menuitem_t* 		menuitems;		// menu items
	void				(*routine)();	// draw routine
	short				x;
	short				y;				// x,y of menu
	short				lastOn; 		// last item user was on in menu
} menu_t;

short			itemOn; 				// menu item skull is on
short			skullAnimCounter;		// skull animation counter
short			whichSkull; 			// which skull to draw
BOOL			drawSkull;				// [RH] don't always draw skull

// graphic name of skulls
// warning: initializer-string for array of chars is too long
char	skullName[2][/*8*/9] = {"M_SKULL1","M_SKULL2"};

// current menudef
menu_t* currentMenu;						  

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

void M_DrawSaveLoadBorder(int x,int y);
void M_SetupNextMenu(menu_t *menudef);
void M_DrawEmptyCell(menu_t *menu,int item);
void M_DrawSelCell(menu_t *menu,int item);
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
static void M_PlayerNameChanged (int choice);
static void M_SlidePlayerRed (int choice);
static void M_SlidePlayerGreen (int choice);
static void M_SlidePlayerBlue (int choice);
static void M_ChangeAutoAim (int choice);


// [RH] Used to make left and right arrows repeat.
static int Lefting = -1, Righting = -1;


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

menuitem_t MainMenu[]=
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

menu_t	MainDef =
{
	main_end,
	NULL,
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

menuitem_t EpisodeMenu[]=
{
	{1,"M_EPI1", M_Episode,'k'},
	{1,"M_EPI2", M_Episode,'t'},
	{1,"M_EPI3", M_Episode,'i'},
	{1,"M_EPI4", M_Episode,'t'}
};

menu_t	EpiDef =
{
	ep_end, 			// # of menu items
	&MainDef,			// previous menu
	EpisodeMenu,		// menuitem_t ->
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

menuitem_t NewGameMenu[]=
{
	{1,"M_JKILL",		M_ChooseSkill, 'i'},
	{1,"M_ROUGH",		M_ChooseSkill, 'h'},
	{1,"M_HURT",		M_ChooseSkill, 'h'},
	{1,"M_ULTRA",		M_ChooseSkill, 'u'},
	{1,"M_NMARE",		M_ChooseSkill, 'n'}
};

menu_t	NewDef =
{
	newg_end,			// # of menu items
	&EpiDef,			// previous menu
	NewGameMenu,		// menuitem_t ->
	M_DrawNewGame,		// drawing routine ->
	48,63,				// x,y
	hurtme				// lastOn
};


//
// [RH] Player Setup Menu
//

enum
{
	playername,
	playerpad1,
	playerpad2,
	playerred,
	playergreen,
	playerblue,
	playerpad3,
	playeraim,
	psetup_end
} psetup_e;

menuitem_t PlayerSetupMenu[] =
{
	{ 1,"", M_EditPlayerName, 'n' },
	{ -1,"",NULL, 0 },
	{ -1,"",NULL, 0 },
	{ 2,"", M_SlidePlayerRed, 'r' },
	{ 2,"", M_SlidePlayerGreen, 'g' },
	{ 2,"", M_SlidePlayerBlue, 'b' },
	{ -1,"",NULL, 0 },
	{ 2,"", M_ChangeAutoAim, 'a' }
};

menu_t PSetupDef = {
	psetup_end,
	&MainDef,
	PlayerSetupMenu,
	M_PlayerSetupDrawer,
	48,	63,
	playername
};

//
// OPTIONS MENU
//
// [RH] This menu is now handled in m_options.c
//
BOOL OptionsActive;

menu_t	OptionsDef =
{
	0,
	&MainDef,
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

menuitem_t ReadMenu1[] =
{
	{1,"",M_ReadThis2,0}
};

menu_t	ReadDef1 =
{
	read1_end,
	&MainDef,
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

menuitem_t ReadMenu2[]=
{
	{1,"",M_FinishReadThis,0}
};

menu_t	ReadDef2 =
{
	read2_end,
	&ReadDef1,
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

menuitem_t LoadMenu[]=
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

menu_t	LoadDef =
{
	load_end,
	&MainDef,
	LoadMenu,
	M_DrawLoad,
	80,54,
	0
};

//
// SAVE GAME MENU
//
menuitem_t SaveMenu[]=
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

menu_t	SaveDef =
{
	load_end,
	&MainDef,
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
	S_StartSound(ORIGIN_AMBIENT,sfx_swtchn);
}

void Cmd_Menu_Load (void *plyr, int argc, char **argv)
{	// F3
	M_StartControlPanel();
	S_StartSound(ORIGIN_AMBIENT,sfx_swtchn);
	M_LoadGame(0);
}

void Cmd_Menu_Save (void *plyr, int argc, char **argv)
{	// F2
	M_StartControlPanel();
	S_StartSound(ORIGIN_AMBIENT,sfx_swtchn);
	M_SaveGame(0);
}

void Cmd_Menu_Help (void *plyr, int argc, char **argv)
{	// F1
	M_StartControlPanel ();

	if ( gamemode == retail )
	  currentMenu = &ReadDef2;
	else
	  currentMenu = &ReadDef1;
	
	itemOn = 0;
	drawSkull = false;
	S_StartSound(ORIGIN_AMBIENT,sfx_swtchn);
}

void Cmd_Quicksave (void *plyr, int argc, char **argv)
{	// F6
	S_StartSound(ORIGIN_AMBIENT,sfx_swtchn);
	M_QuickSave();
}

void Cmd_Quickload (void *plyr, int argc, char **argv)
{	// F9
	S_StartSound(ORIGIN_AMBIENT,sfx_swtchn);
	M_QuickLoad();
}

void Cmd_Menu_Endgame (void *plyr, int argc, char **argv)
{	// F7
	S_StartSound(ORIGIN_AMBIENT,sfx_swtchn);
	M_EndGame(0);
}

void Cmd_Menu_Quit (void *plyr, int argc, char **argv)
{	// F10
	S_StartSound(ORIGIN_AMBIENT,sfx_swtchn);
	M_QuitDOOM(0);
}

void Cmd_Menu_Game (void *plyr, int argc, char **argv)
{
	M_StartControlPanel ();
	S_StartSound(ORIGIN_AMBIENT,sfx_swtchn);
	M_NewGame(0);
}
								
void Cmd_Menu_Options (void *plyr, int argc, char **argv)
{
	M_StartControlPanel ();
	S_StartSound(ORIGIN_AMBIENT,sfx_swtchn);
	M_Options(0);
}

void Cmd_Menu_Player (void *plyr, int argc, char **argv)
{
	M_StartControlPanel ();
	S_StartSound(ORIGIN_AMBIENT,sfx_swtchn);
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
	Printf ("Gamma correction level %g\n", gammalevel->value);
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
		
	for (i = 0; i < load_end;i++)
	{
		if (M_CheckParm("-cdrom"))
			sprintf(name,"c:\\doomdata\\"SAVEGAMENAME"%d.dsg",i);
		else
			sprintf(name,SAVEGAMENAME"%d.dsg",i);

		handle = open (name, O_RDONLY | 0, 0666);
		if (handle == -1)
		{
			strcpy(&savegamestrings[i][0],EMPTYSTRING);
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
		
	V_DrawPatchClean (72,28,&screens[0],W_CacheLumpName("M_LOADG",PU_CACHE));
	for (i = 0; i < load_end; i++)
	{
		M_DrawSaveLoadBorder(LoadDef.x,LoadDef.y+LINEHEIGHT*i);
		V_DrawRedTextClean(LoadDef.x,LoadDef.y+LINEHEIGHT*i,savegamestrings[i]);
	}
}



//
// Draw border for the savegame description
//
void M_DrawSaveLoadBorder(int x,int y)
{
	int i;
		
	V_DrawPatchClean (x-8,y+7,&screens[0],W_CacheLumpName("M_LSLEFT",PU_CACHE));
		
	for (i = 0;i < 24;i++)
	{
		V_DrawPatchClean (x,y+7,&screens[0],W_CacheLumpName("M_LSCNTR",PU_CACHE));
		x += 8;
	}

	V_DrawPatchClean (x,y+7,&screens[0],W_CacheLumpName("M_LSRGHT",PU_CACHE));
}



//
// User wants to load this game
//
void M_LoadSelect (int choice)
{
	char name[256];
		
	if (M_CheckParm("-cdrom"))
		sprintf(name,"c:\\doomdata\\"SAVEGAMENAME"%d.dsg",choice);
	else
		sprintf(name,SAVEGAMENAME"%d.dsg",choice);
	G_LoadGame (name);
	M_ClearMenus ();
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
		
	V_DrawPatchClean (72,28,&screens[0],W_CacheLumpName("M_SAVEG",PU_CACHE));
	for (i = 0; i < load_end; i++)
	{
		M_DrawSaveLoadBorder(LoadDef.x,LoadDef.y+LINEHEIGHT*i);
		V_DrawRedTextClean(LoadDef.x,LoadDef.y+LINEHEIGHT*i,savegamestrings[i]);
	}
		
	if (genStringEnter)
	{
		i = V_StringWidth(savegamestrings[saveSlot]);
		V_DrawRedTextClean(LoadDef.x + i,LoadDef.y+LINEHEIGHT*saveSlot,"_");
	}
}

//
// M_Responder calls this when user is finished
//
void M_DoSave(int slot)
{
	G_SaveGame (slot,savegamestrings[slot]);
	M_ClearMenus ();

	// PICK QUICKSAVE SLOT YET?
	if (quickSaveSlot == -2)
		quickSaveSlot = slot;
}

//
// User wants to save. Start string input for M_Responder
//
void M_SaveSelect(int choice)
{
	// we are going to be intercepting all chars
	genStringEnter = 1;
	genStringEnd = M_DoSave;
	
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
		S_StartSound(ORIGIN_AMBIENT,sfx_swtchx);
	}
}

void M_QuickSave(void)
{
	if (!usergame)
	{
		S_StartSound(ORIGIN_AMBIENT,sfx_oof);
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
		S_StartSound(ORIGIN_AMBIENT,sfx_swtchx);
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
		M_StartMessage(QSAVESPOT,NULL,false);
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
	inhelpscreens = true;
	switch ( gamemode )
	{
	  case commercial:
		V_DrawPatchIndirect (0,0,&screens[0],W_CacheLumpName("HELP",PU_CACHE));
		break;
	  case shareware:
	  case registered:
	  case retail:
		V_DrawPatchIndirect (0,0,&screens[0],W_CacheLumpName("HELP1",PU_CACHE));
		break;
	  default:
		break;
	}
	return;
}



//
// Read This Menus - optional second page.
//
void M_DrawReadThis2(void)
{
	inhelpscreens = true;
	switch ( gamemode )
	{
	  case retail:
	  case commercial:
		// This hack keeps us from having to change menus.
		V_DrawPatchIndirect (0,0,&screens[0],W_CacheLumpName("CREDIT",PU_CACHE));
		break;
	  case shareware:
	  case registered:
		V_DrawPatchIndirect (0,0,&screens[0],W_CacheLumpName("HELP2",PU_CACHE));
		break;
	  default:
		break;
	}
	return;
}


//
// M_DrawMainMenu
//
void M_DrawMainMenu(void)
{
	V_DrawPatchClean (94,2,&screens[0],W_CacheLumpName("M_DOOM",PU_CACHE));
}




//
// M_NewGame
//
void M_DrawNewGame(void)
{
	V_DrawPatchClean (96,14,&screens[0],W_CacheLumpName("M_NEWG",PU_CACHE));
	V_DrawPatchClean (54,38,&screens[0],W_CacheLumpName("M_SKILL",PU_CACHE));
}

void M_NewGame(int choice)
{
	if (netgame && !demoplayback)
	{
		M_StartMessage(NEWGAME,NULL,false);
		return;
	}
		
	if ( gamemode == commercial )
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
	V_DrawPatchClean (54,38,&screens[0],W_CacheLumpName("M_EPISOD",PU_CACHE));
}

void M_VerifyNightmare(int ch)
{
	if (ch != 'y')
		return;
		
	SetCVarFloat (gameskill, nightmare);
	G_DeferedInitNew (CalcMapName (epi+1, 1));
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
	G_DeferedInitNew (CalcMapName (epi+1, 1));
	M_ClearMenus ();
}

void M_Episode(int choice)
{
	if ( (gamemode == shareware)
		 && choice)
	{
		M_StartMessage(SWSTRING,NULL,false);
		M_SetupNextMenu(&ReadDef1);
		return;
	}

	// Yet another hack...
	if ( (gamemode == registered)
		 && (choice > 2))
	{
	  Printf ("M_Episode: 4th episode requires UltimateDOOM\n");
	  choice = 0;
	}
		 
	epi = choice;
	M_SetupNextMenu(&NewDef);
}



//
// M_Options
//
void M_DrawOptions(void)
{
	V_DrawPatchClean (108,15,&screens[0],W_CacheLumpName("M_OPTTTL",PU_CACHE));
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
		S_StartSound(ORIGIN_AMBIENT,sfx_oof);
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
	M_SetupNextMenu(&ReadDef1);
	drawSkull = false;
}

void M_ReadThis2(int choice)
{
	choice = 0;
	M_SetupNextMenu(&ReadDef2);
	drawSkull = false;
}

void M_FinishReadThis(int choice)
{
	choice = 0;
	M_SetupNextMenu(&MainDef);
}




//
// M_QuitDOOM
//
int quitsounds[8] =
{
	sfx_pldeth,
	sfx_dmpain,
	sfx_popain,
	sfx_slop,
	sfx_telept,
	sfx_posit1,
	sfx_posit3,
	sfx_sgtatk
};

int quitsounds2[8] =
{
	sfx_vilact,
	sfx_getpow,
	sfx_boscub,
	sfx_slop,
	sfx_skeswg,
	sfx_kntdth,
	sfx_bspact,
	sfx_sgtatk
};



void M_QuitResponse(int ch)
{
	if (ch != 'y')
		return;
	if (!netgame)
	{
		if (gamemode == commercial)
			S_StartSound(ORIGIN_SURROUND,quitsounds2[(gametic>>2)&7]);
		else
			S_StartSound(ORIGIN_SURROUND,quitsounds[(gametic>>2)&7]);
		I_WaitVBL(105);
	}
	I_Quit ();
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

static state_t *PlayerState;
static int PlayerTics;

extern cvar_t *name;

static void M_PlayerSetup (int choice)
{
	choice = 0;
	strncpy (savegamestrings[0], name->string, 23);
	savegamestrings[0][23] = 0;
	M_SetupNextMenu (&PSetupDef);
	PlayerState = &states[mobjinfo[MT_PLAYER].seestate];
	PlayerTics = PlayerState->tics;
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
						  &screens[0], patch);
	}

	// Draw player name box
	V_DrawWhiteTextClean (PSetupDef.x + 8, PSetupDef.y - 12, "Your name");
	M_DrawSaveLoadBorder (PSetupDef.x + 8, PSetupDef.y);
	V_DrawRedTextClean (PSetupDef.x + 8, PSetupDef.y, savegamestrings[0]);
	if (genStringEnter)
		V_DrawRedTextClean (PSetupDef.x + V_StringWidth(savegamestrings[0]) + 8, PSetupDef.y, "_");

	// Draw player character
	{
		int x = 320 - 88 - 32, y = PSetupDef.y + LINEHEIGHT*3 - 20;

		x = (x-160)*CleanXfac+(screens[0].width>>1);
		y = (y-100)*CleanYfac+(screens[0].height>>1);
		V_Clear (x, y, x + 72 * CleanXfac, y + 72 * CleanYfac, &screens[0], 0);
	}
	{
		spriteframe_t *sprframe =
			&sprites[PlayerState->sprite].spriteframes[PlayerState->frame & FF_FRAMEMASK];

		V_ColorMap = translationtables + consoleplayer * 256;
		V_DrawTranslatedPatchClean (320 - 52 - 32, PSetupDef.y + LINEHEIGHT*3 + 40,
									&screens[0],
									W_CacheLumpNum (sprframe->lump[0] + firstspritelump, PU_CACHE));
	}
	V_DrawPatchClean (320 - 88 - 32 + 36, PSetupDef.y + LINEHEIGHT*3 + 16,
					  &screens[0],
					  W_CacheLumpName ("M_PBOX", PU_CACHE));

	// Draw player color sliders
	V_DrawWhiteTextClean (PSetupDef.x, PSetupDef.y + LINEHEIGHT*2, "Your color");

	V_DrawRedTextClean (PSetupDef.x, PSetupDef.y + LINEHEIGHT*3, "Red");
	V_DrawRedTextClean (PSetupDef.x, PSetupDef.y + LINEHEIGHT*4, "Green");
	V_DrawRedTextClean (PSetupDef.x, PSetupDef.y + LINEHEIGHT*5, "Blue");

	{
		int x = V_StringWidth ("Green") + 8 + PSetupDef.x;
		int color = players[consoleplayer].userinfo->color;

		M_DrawSlider (x, PSetupDef.y + LINEHEIGHT*3, 0.0f, 255.0f, RPART(color));
		M_DrawSlider (x, PSetupDef.y + LINEHEIGHT*4, 0.0f, 255.0f, GPART(color));
		M_DrawSlider (x, PSetupDef.y + LINEHEIGHT*5, 0.0f, 255.0f, BPART(color));
	}

	// Draw autoaim setting
	{
		int x = V_StringWidth ("Autoaim") + 8 + PSetupDef.x;
		float aim = autoaim->value;

		V_DrawRedTextClean (PSetupDef.x, PSetupDef.y + LINEHEIGHT*7, "Autoaim");
		V_DrawWhiteTextClean (x, PSetupDef.y + LINEHEIGHT*7,
			aim == 0 ? "Never" :
			aim <= 0.25 ? "Very Low" :
			aim <= 0.5 ? "Low" :
			aim <= 1 ? "Medium" :
			aim <= 2 ? "High" :
			aim <= 3 ? "Very High" : "Always");
	}
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

static void SendNewColor (int red, int green, int blue)
{
	char command[24];

	sprintf (command, "color \"%02x %02x %02x\"", red, green, blue);
	AddCommandString (command);
}

static void M_SlidePlayerRed (int choice)
{
	int color = players[consoleplayer].userinfo->color;
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
	int color = players[consoleplayer].userinfo->color;
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
	int color = players[consoleplayer].userinfo->color;
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
void M_DrawEmptyCell (menu_t *menu, int item)
{
	V_DrawPatchClean (menu->x - 10,		menu->y+item*LINEHEIGHT - 1, &screens[0],
					   W_CacheLumpName("M_CELL1",PU_CACHE));
}

void M_DrawSelCell (menu_t *menu, int item)
{
	V_DrawPatchClean (menu->x - 10,		menu->y+item*LINEHEIGHT - 1, &screens[0],
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
	
	// [RH] Repeat left and right arrow keys
	if (ev->type == ev_keydown) {
		if (ev->data1 == KEY_LEFTARROW)
			Lefting = KeyRepeatDelay >> 1;
		else if (ev->data1 == KEY_RIGHTARROW)
			Righting = KeyRepeatDelay >> 1;
	} else if (ev->type == ev_keyup) {
		if (ev->data1 == KEY_LEFTARROW)
			Lefting = -1;
		else if (ev->data1 == KEY_RIGHTARROW)
			Righting = -1;
	}

	if (ch == -1 && !menuactive && Lefting == -1 && Righting == -1)
		return false;

	if (menuactive && OptionsActive) {
		if (ev->type == ev_keydown) {
			OptionsActive = M_OptResponder (ev);
			return true;
		} else {
			return false;
		}
	}

	// [RH] Ignore key up events totally.
	if (ev->type == ev_keyup)
		return false;
	
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
			strcpy(&savegamestrings[saveSlot][0],saveOldString);
			break;
								
		  case KEY_ENTER:
			genStringEnter = 0;
			if (savegamestrings[saveSlot][0])
				genStringEnd(saveSlot);	// [RH] M_DoSave() or M_PlayerNameChanged
			break;
								
		  default:
			ch = toupper(ev->data3);	// [RH] Use user keymap
			if (ch != 32)
				if (ch-HU_FONTSTART < 0 || ch-HU_FONTSTART >= HU_FONTSIZE)
					break;
			if (ch >= 32 && ch <= 127 &&
				saveCharIndex < SAVESTRINGSIZE-1 &&
				V_StringWidth(savegamestrings[saveSlot]) <
				(SAVESTRINGSIZE-2)*8)
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
		S_StartSound(ORIGIN_AMBIENT,sfx_swtchx);
		return true;
	}
		
	// [RH] F-Keys are now just normal keys that can be bound,
	//		so they aren't checked here anymore.
	
	// Pop-up menu?
	if (!menuactive)
	{
		if (ch == KEY_ESCAPE)
		{
			M_StartControlPanel ();
			S_StartSound(ORIGIN_AMBIENT,sfx_swtchn);
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
			S_StartSound(ORIGIN_AMBIENT,sfx_pstop);
		} while(currentMenu->menuitems[itemOn].status==-1);
		return true;
				
	  case KEY_UPARROW:
		do
		{
			if (!itemOn)
				itemOn = currentMenu->numitems-1;
			else itemOn--;
			S_StartSound(ORIGIN_AMBIENT,sfx_pstop);
		} while(currentMenu->menuitems[itemOn].status==-1);
		return true;

	  case KEY_LEFTARROW:
		if (currentMenu->menuitems[itemOn].routine &&
			currentMenu->menuitems[itemOn].status == 2)
		{
			S_StartSound(ORIGIN_AMBIENT,sfx_stnmov);
			currentMenu->menuitems[itemOn].routine(0);
		}
		return true;
				
	  case KEY_RIGHTARROW:
		if (currentMenu->menuitems[itemOn].routine &&
			currentMenu->menuitems[itemOn].status == 2)
		{
			S_StartSound(ORIGIN_AMBIENT,sfx_stnmov);
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
				S_StartSound(ORIGIN_AMBIENT,sfx_stnmov);
			}
			else
			{
				currentMenu->menuitems[itemOn].routine(itemOn);
				S_StartSound(ORIGIN_AMBIENT,sfx_pistol);
			}
		}
		return true;
				
	  // [RH] Escape now moves back one menu instead of
	  //	  quitting the menu system. Thus, backspace
	  //	  is now ignored.
	  case KEY_ESCAPE:
		currentMenu->lastOn = itemOn;
		if (currentMenu->prevMenu)
		{
			currentMenu = currentMenu->prevMenu;
			itemOn = currentMenu->lastOn;
			S_StartSound(ORIGIN_AMBIENT,sfx_swtchn);
		} else {
			M_ClearMenus ();
			S_StartSound(ORIGIN_AMBIENT,sfx_swtchx);
		}
		return true;
		
	  default:
		for (i = itemOn+1;i < currentMenu->numitems;i++)
			if (currentMenu->menuitems[i].alphaKey == ch2)
			{
				itemOn = i;
				S_StartSound(ORIGIN_AMBIENT,sfx_pstop);
				return true;
			}
		for (i = 0;i <= itemOn;i++)
			if (currentMenu->menuitems[i].alphaKey == ch2)
			{
				itemOn = i;
				S_StartSound(ORIGIN_AMBIENT,sfx_pstop);
				return true;
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
	
	menuactive = 1;
	currentMenu = &MainDef; 		// JDC
	itemOn = currentMenu->lastOn;	// JDC
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
	char			string[40];
	int 			start;

	inhelpscreens = false;

	
	// Horiz. & Vertically center string and print it.
	if (messageToPrint)
	{
		V_DimScreen (&screens[0]);
		start = 0;
		y = 100 - M_StringHeight(messageString)/2;
		while(*(messageString+start))
		{
			for (i = 0;i < (int)strlen(messageString+start);i++)
				if (*(messageString+start+i) == '\n')
				{
					memset(string,0,40);
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
			V_DrawRedTextClean(x,y,string);
			y += SHORT(hu_font[0]->height);
		}
		return;
	}

	if (!menuactive)
		return;

	V_DimScreen (&screens[0]);

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
				V_DrawPatchClean (x,y,&screens[0],
								   W_CacheLumpName(currentMenu->menuitems[i].name ,PU_CACHE));
			y += LINEHEIGHT;
		}

		
		// DRAW SKULL
		if (drawSkull) {
			V_DrawPatchClean(x + SKULLXOFF,currentMenu->y - 5 + itemOn*LINEHEIGHT, &screens[0],
							 W_CacheLumpName(skullName[whichSkull],PU_CACHE));
		}
	}
}


//
// M_ClearMenus
//
void M_ClearMenus (void)
{
	menuactive = 0;
	drawSkull = true;
	I_ResumeMouse ();		// [RH] Recapture the mouse in windowed modes.
	// if (!netgame && usergame && paused)
	//		 sendpause = true;
}




//
// M_SetupNextMenu
//
void M_SetupNextMenu(menu_t *menudef)
{
	currentMenu = menudef;
	itemOn = currentMenu->lastOn;
	drawSkull = true;
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

	// [RH] Make left and right arrow keys repeat
	if (Lefting != -1 && --Lefting == 0) {
		event_t ev;

		ev.type = ev_keydown;
		ev.data1 = KEY_LEFTARROW;
		Lefting = 2;
		M_Responder (&ev);
	} else if (Righting != -1 && --Righting == 0) {
		event_t ev;

		ev.type = ev_keydown;
		ev.data1 = KEY_RIGHTARROW;
		Righting = 2;
		M_Responder (&ev);
	}
}


//
// M_Init
//
void M_Init (void)
{
	extern cvar_t *screenblocks;

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

  
	switch ( gamemode )
	{
	  case commercial:
		// This is used because DOOM 2 had only one HELP
		//	page. I use CREDIT as second page now, but
		//	kept this hack for educational purposes.
		MainMenu[readthis] = MainMenu[quitdoom];
		MainDef.numitems--;
		MainDef.y += 8;
		NewDef.prevMenu = &MainDef;
		ReadDef1.routine = M_DrawReadThis1;
		ReadDef1.x = 330;
		ReadDef1.y = 165;
		ReadMenu1[0].routine = M_FinishReadThis;
		break;
	  case shareware:
		// Episode 2 and 3 are handled,
		//	branching to an ad screen.
	  case registered:
		// We need to remove the fourth episode.
		EpiDef.numitems--;
		break;
	  case retail:
		// We are fine.
	  default:
		break;
	}
	M_OptInit ();
}

