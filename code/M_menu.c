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

static const char
rcsid[] = "$Id: m_menu.c,v 1.7 1997/02/03 22:45:10 b1 Exp $";

#if defined(NORMALUNIX)
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <ctype.h>
#elif defined(_WIN32)
#include <io.h>
#include <fcntl.h>
#include <ctype.h>
#endif


#include "doomdef.h"
#include "dstrings.h"

#include "c_console.h"
#include "c_dispatch.h"

#include "d_main.h"

#include "i_system.h"
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



extern patch_t* 		hu_font[HU_FONTSIZE];

extern boolean			chat_on;				// in heads-up code

// temp for screenblocks (0-9)
int 					screenSize; 			

// -1 = no quicksave slot picked!
int 					quickSaveSlot;			

 // 1 = message to be printed
int 					messageToPrint;
// ...and here is the message string!
char*					messageString;			

// message x & y
int 					messx;					
int 					messy;
int 					messageLastMenuActive;

// timed message = no input from user
boolean 				messageNeedsInput;	   

void	(*messageRoutine)(int response);

#define SAVESTRINGSIZE	24

// we are going to be entering a savegame string
int 					saveStringEnter;			  
int 					saveSlot;		// which slot to save in
int 					saveCharIndex;	// which char we're editing
// old save description before edit
char					saveOldString[SAVESTRINGSIZE];	

boolean 				inhelpscreens;
boolean 				menuactive;

#define SKULLXOFF		-32
#define LINEHEIGHT		16

extern boolean			sendpause;
char					savegamestrings[10][SAVESTRINGSIZE];

char	endstring[160];


//
// MENU TYPEDEFS
//
typedef struct
{
	// 0 = no cursor here, 1 = ok, 2 = arrows ok
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
boolean			drawSkull;				// [RH] don't always draw skull

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
int  M_StringWidth(char *string);
int  M_StringHeight(char *string);
void M_StartControlPanel(void);
void M_StartMessage(char *string,void *routine,boolean input);
void M_StopMessage(void);
void M_ClearMenus (void);




//
// DOOM MENU
//
enum
{
	newgame = 0,
	options,
	loadgame,
	savegame,
	readthis,
	quitdoom,
	main_end
} main_e;

menuitem_t MainMenu[]=
{
	{1,"M_NGAME",M_NewGame,'n'},
	{1,"M_OPTION",M_Options,'o'},
	{1,"M_LOADG",M_LoadGame,'l'},
	{1,"M_SAVEG",M_SaveGame,'s'},
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
// OPTIONS MENU
//
// [RH] This menu is now handled in m_options.c
//
boolean OptionsActive;

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
	load_end
} load_e;

menuitem_t LoadMenu[]=
{
	{1,"", M_LoadSelect,'1'},
	{1,"", M_LoadSelect,'2'},
	{1,"", M_LoadSelect,'3'},
	{1,"", M_LoadSelect,'4'},
	{1,"", M_LoadSelect,'5'},
	{1,"", M_LoadSelect,'6'}
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
	{1,"", M_SaveSelect,'6'}
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

void Cmd_Bumpgamma (void *plyr, int argc, char **argv)
{
	// [RH] Gamma correction tables are now generated
	// on the fly for *any* gamma level.
	// Q: What are reasonable limits to use here?

	double newgamma = gammalevel + 0.1;
	char cmd[32];

	if (newgamma > 3.0)
		newgamma = 1.0;

	sprintf (cmd, "gamma %g", newgamma);
	AddCommandString (cmd);
}

//
// M_ReadSaveStrings
//	read the strings from the savegame files
//
void M_ReadSaveStrings(void)
{
	int 			handle;
	int 			count;
	int 			i;
	char	name[256];
		
	for (i = 0;i < load_end;i++)
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
	int 			i;
		
	V_DrawPatchClean (72,28,0,W_CacheLumpName("M_LOADG",PU_CACHE));
	for (i = 0;i < load_end; i++)
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
	int 			i;
		
	V_DrawPatchClean (x-8,y+7,0,W_CacheLumpName("M_LSLEFT",PU_CACHE));
		
	for (i = 0;i < 24;i++)
	{
		V_DrawPatchClean (x,y+7,0,W_CacheLumpName("M_LSCNTR",PU_CACHE));
		x += 8;
	}

	V_DrawPatchClean (x,y+7,0,W_CacheLumpName("M_LSRGHT",PU_CACHE));
}



//
// User wants to load this game
//
void M_LoadSelect(int choice)
{
	char	name[256];
		
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
	int 			i;
		
	V_DrawPatchClean (72,28,0,W_CacheLumpName("M_SAVEG",PU_CACHE));
	for (i = 0;i < load_end; i++)
	{
		M_DrawSaveLoadBorder(LoadDef.x,LoadDef.y+LINEHEIGHT*i);
		V_DrawRedTextClean(LoadDef.x,LoadDef.y+LINEHEIGHT*i,savegamestrings[i]);
	}
		
	if (saveStringEnter)
	{
		i = M_StringWidth(savegamestrings[saveSlot]);
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
	saveStringEnter = 1;
	
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
		V_DrawPatchIndirect (0,0,0,W_CacheLumpName("HELP",PU_CACHE));
		break;
	  case shareware:
	  case registered:
	  case retail:
		V_DrawPatchIndirect (0,0,0,W_CacheLumpName("HELP1",PU_CACHE));
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
		V_DrawPatchIndirect (0,0,0,W_CacheLumpName("CREDIT",PU_CACHE));
		break;
	  case shareware:
	  case registered:
		V_DrawPatchIndirect (0,0,0,W_CacheLumpName("HELP2",PU_CACHE));
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
	V_DrawPatchClean (94,2,0,W_CacheLumpName("M_DOOM",PU_CACHE));
}




//
// M_NewGame
//
void M_DrawNewGame(void)
{
	V_DrawPatchClean (96,14,0,W_CacheLumpName("M_NEWG",PU_CACHE));
	V_DrawPatchClean (54,38,0,W_CacheLumpName("M_SKILL",PU_CACHE));
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
	V_DrawPatchClean (54,38,0,W_CacheLumpName("M_EPISOD",PU_CACHE));
}

void M_VerifyNightmare(int ch)
{
	if (ch != 'y')
		return;
				
	G_DeferedInitNew(nightmare,CalcMapName (epi+1, 1));
	M_ClearMenus ();
}

void M_ChooseSkill(int choice)
{
	if (choice == nightmare)
	{
		M_StartMessage(NIGHTMARE,M_VerifyNightmare,true);
		return;
	}
		
	G_DeferedInitNew((float)choice,CalcMapName (epi+1, 1));
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
char	msgNames[2][9]			= {"M_MSGOFF","M_MSGON"};


void M_DrawOptions(void)
{
	V_DrawPatchClean (108,15,0,W_CacheLumpName("M_OPTTTL",PU_CACHE));
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
int 	quitsounds[8] =
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

int 	quitsounds2[8] =
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




void M_QuitDOOM(int choice)
{
  // We pick index 0 which is language sensitive,
  //  or one at random, between 1 and maximum number.
  if (language != english )
	sprintf(endstring,"%s\n\n"DOSY, endmsg[0] );
  else
	sprintf(endstring,"%s\n\n"DOSY, endmsg[ (gametic%(NUM_QUITMESSAGES-2))+1 ]);
  
  M_StartMessage(endstring,M_QuitResponse,true);
}




//
//		Menu Functions
//
void
M_DrawEmptyCell
( menu_t*		menu,
  int			item )
{
	V_DrawPatchClean (menu->x - 10,		menu->y+item*LINEHEIGHT - 1, 0,
					   W_CacheLumpName("M_CELL1",PU_CACHE));
}

void
M_DrawSelCell
( menu_t*		menu,
  int			item )
{
	V_DrawPatchClean (menu->x - 10,		menu->y+item*LINEHEIGHT - 1, 0,
					   W_CacheLumpName("M_CELL2",PU_CACHE));
}


void
M_StartMessage
( char* 		string,
  void* 		routine,
  boolean		input )
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



void M_StopMessage(void)
{
	menuactive = messageLastMenuActive;
	messageToPrint = 0;
}



//
// Find string width from hu_font chars
//
int M_StringWidth(char* string)
{
	int 			w = 0;
	int 			c;
		
	while (*string) {
		c = toupper((*string++) & 0x7f) - HU_FONTSTART;
		if (c < 0 || c >= HU_FONTSIZE)
			w += 4;
		else
			w += SHORT (hu_font[c]->width);
	}
				
	return w;
}



//
//		Find string height from hu_font chars
//
int M_StringHeight(char* string)
{
	int 			h;
	int 			height = SHORT(hu_font[0]->height);
		
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
boolean M_Responder (event_t* ev)
{
	int 			ch, ch2;
	int 			i;
	static	int 	joywait = 0;
	static	int 	mousewait = 0;
	static	int 	mousey = 0;
	static	int 	lasty = 0;
	static	int 	mousex = 0;
	static	int 	lastx = 0;
		
	ch = ch2 = -1;
		
	if (menuactive && OptionsActive) {
		if (ev->type == ev_keydown) {
			OptionsActive = M_OptResponder (ev->data1);
			return true;
		} else {
			return false;
		}
	}

	if (ev->type == ev_joystick && joywait < I_GetTime())
	{
		if (ev->data3 == -1)
		{
			ch = KEY_UPARROW;
			joywait = I_GetTime() + 5;
		}
		else if (ev->data3 == 1)
		{
			ch = KEY_DOWNARROW;
			joywait = I_GetTime() + 5;
		}
				
		if (ev->data2 == -1)
		{
			ch = KEY_LEFTARROW;
			joywait = I_GetTime() + 2;
		}
		else if (ev->data2 == 1)
		{
			ch = KEY_RIGHTARROW;
			joywait = I_GetTime() + 2;
		}
	}
	else
	{
		if (ev->type == ev_mouse && mousewait < I_GetTime())
		{
			mousey += ev->data3;
			if (mousey < lasty-30)
			{
				ch = KEY_DOWNARROW;
				mousewait = I_GetTime() + 5;
				mousey = lasty -= 30;
			}
			else if (mousey > lasty+30)
			{
				ch = KEY_UPARROW;
				mousewait = I_GetTime() + 5;
				mousey = lasty += 30;
			}
				
			mousex += ev->data2;
			if (mousex < lastx-30)
			{
				ch = KEY_LEFTARROW;
				mousewait = I_GetTime() + 5;
				mousex = lastx -= 30;
			}
			else if (mousex > lastx+30)
			{
				ch = KEY_RIGHTARROW;
				mousewait = I_GetTime() + 5;
				mousex = lastx += 30;
			}
		}
		else
			if (ev->type == ev_keydown)
			{
				switch (ev->data1) {
					case KEY_JOY1:
					case KEY_MOUSE1:
						ch = KEY_ENTER;
						break;
					case KEY_JOY2:
					case KEY_MOUSE2:
						ch = KEY_BACKSPACE;
						break;
					default:
						ch = ev->data1; 		// scancode
						ch2 = ev->data2;		// ASCII
				}
			}
	}
	
	if (ch == -1 && !menuactive)
		return false;

	
	// Save Game string input
	if (saveStringEnter)
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
			saveStringEnter = 0;
			strcpy(&savegamestrings[saveSlot][0],saveOldString);
			break;
								
		  case KEY_ENTER:
			saveStringEnter = 0;
			if (savegamestrings[saveSlot][0])
				M_DoSave(saveSlot);
			break;
								
		  default:
			ch = toupper(ch2);
			if (ch != 32)
				if (ch-HU_FONTSTART < 0 || ch-HU_FONTSTART >= HU_FONTSIZE)
					break;
			if (ch >= 32 && ch <= 127 &&
				saveCharIndex < SAVESTRINGSIZE-1 &&
				M_StringWidth(savegamestrings[saveSlot]) <
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
		
	if (devparm && ch == KEY_F1)
	{
		G_ScreenShot ();
		return true;
	}
				
	
	// [RH] F-Keys are now just normal keys that can be bound
	
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
				
	  case KEY_ESCAPE:
		currentMenu->lastOn = itemOn;
		M_ClearMenus ();
		S_StartSound(ORIGIN_AMBIENT,sfx_swtchx);
		return true;
				
	  case KEY_BACKSPACE:
		currentMenu->lastOn = itemOn;
		if (currentMenu->prevMenu)
		{
			currentMenu = currentMenu->prevMenu;
			itemOn = currentMenu->lastOn;
			S_StartSound(ORIGIN_AMBIENT,sfx_swtchn);
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
}


//
// M_Drawer
// Called after the view has been rendered,
// but before it has been blitted.
//
void M_Drawer (void)
{
	static short		x;
	static short		y;
	short				i;
	short				max;
	char				string[40];
	int 				start;

	inhelpscreens = false;

	
	// Horiz. & Vertically center string and print it.
	if (messageToPrint)
	{
		V_DimScreen (0);
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
								
			x = 160 - M_StringWidth(string)/2;
			V_DrawRedTextClean(x,y,string);
			y += SHORT(hu_font[0]->height);
		}
		return;
	}

	if (!menuactive)
		return;

	V_DimScreen (0);

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
				V_DrawPatchClean (x,y,0,
								   W_CacheLumpName(currentMenu->menuitems[i].name ,PU_CACHE));
			y += LINEHEIGHT;
		}

		
		// DRAW SKULL
		if (drawSkull) {
			V_DrawPatchClean(x + SKULLXOFF,currentMenu->y - 5 + itemOn*LINEHEIGHT, 0,
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
	
}

