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

// HEADER FILES ------------------------------------------------------------

#include <ctype.h>
#include <fcntl.h>
#include <stdlib.h>
#include <zlib.h>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

#include "doomdef.h"
#include "gstrings.h"
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
#include "m_random.h"
#include "s_sound.h"
#include "doomstat.h"
#include "m_menu.h"
#include "v_text.h"
#include "st_stuff.h"
#include "d_gui.h"
#include "version.h"

#include "lists.h"
#include "gi.h"

// MACROS ------------------------------------------------------------------

#define SKULLXOFF			-32
#define SELECTOR_XOFFSET	(-28)
#define SELECTOR_YOFFSET	(-1)

#define NUM_MENU_ITEMS(m)	(sizeof(m)/sizeof(m[0]))

// TYPES -------------------------------------------------------------------

struct FSaveGameNode : public Node
{
	char Title[SAVESTRINGSIZE];
	char *Filename;
	bool bOldVersion;
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void M_DrawSlider (int x, int y, float min, float max, float cur);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

int  M_StringHeight (const char *string);
void M_ClearMenus ();

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void M_NewGame (int choice);
static void M_Episode (int choice);
static void M_ChooseSkill (int choice);
static void M_LoadGame (int choice);
static void M_SaveGame (int choice);
static void M_Options (int choice);
static void M_EndGame (int choice);
static void M_ReadThis (int choice);
static void M_ReadThisMore (int choice);
static void M_QuitDOOM (int choice);
static void M_GameFiles (int choice);

static void M_FinishReadThis (int choice);
static void M_QuickSave ();
static void M_QuickLoad ();
static void M_LoadSelect (const FSaveGameNode *file);
static void M_SaveSelect (const FSaveGameNode *file);
static void M_ReadSaveStrings ();
static void M_ExtractSaveData (const FSaveGameNode *file);
static void M_UnloadSaveData ();
static void M_InsertSaveNode (FSaveGameNode *node);
static BOOL M_SaveLoadResponder (event_t *ev);
static void M_DeleteSaveResponse (int choice);

static void M_DrawMainMenu ();
static void M_DrawReadThis ();
static void M_DrawNewGame ();
static void M_DrawEpisode ();
static void M_DrawLoad ();
static void M_DrawSave ();

static void M_DrawHereticMainMenu ();
static void M_DrawFiles ();

void M_DrawFrame (int x, int y, int width, int height);
static void M_DrawSaveLoadBorder (int x,int y, int len);
static void M_DrawSaveLoadCommon ();
static void M_SetupNextMenu (oldmenu_t *menudef);
static void M_StartMessage (const char *string, void(*routine)(int), bool input);

// [RH] For player setup menu.
static void M_PlayerSetup (int choice);
static void M_PlayerSetupTicker ();
static void M_PlayerSetupDrawer ();
static void M_DrawPlayerBackdrop (int x, int y);
static void M_EditPlayerName (int choice);
static void M_ChangePlayerTeam (int choice);
static void M_PlayerNameChanged (FSaveGameNode *dummy);
static void M_SlidePlayerRed (int choice);
static void M_SlidePlayerGreen (int choice);
static void M_SlidePlayerBlue (int choice);
static void M_ChangeGender (int choice);
static void M_ChangeSkin (int choice);
static void M_ChangeAutoAim (int choice);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

EXTERN_CVAR (String, name)
EXTERN_CVAR (Int, team)

extern bool		sendpause;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

bool			menuactive;
menustack_t		MenuStack[16];
int				MenuStackDepth;
int				skullAnimCounter;	// skull animation counter
bool			drawSkull;			// [RH] don't always draw skull
bool			M_DemoNoPlay;
bool			OptionsActive;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static char		tempstring[80];
static char		underscore[2];

static FSaveGameNode *quickSaveSlot;	// NULL = no quicksave slot picked!
static int 		messageToPrint;			// 1 = message to be printed
static const char *messageString;		// ...and here is the message string!
static bool		messageLastMenuActive;
static bool		messageNeedsInput;		// timed message = no input from user
static void	  (*messageRoutine)(int response);

static int 		genStringEnter;	// we are going to be entering a savegame string
static int		genStringLen;	// [RH] Max # of chars that can be entered
static void	  (*genStringEnd)(FSaveGameNode *);
static int 		saveSlot;		// which slot to save in
static int 		saveCharIndex;	// which char we're editing

static int		LINEHEIGHT;

static char		savegamestring[SAVESTRINGSIZE];
static char		endstring[160];

static short	itemOn; 			// menu item skull is on
static short	whichSkull; 		// which skull to draw
static int		SkullBaseLump;		// Heretic's spinning skull
static int		MenuTime;
static int		InfoType;

static const char skullName[2][9] = {"M_SKULL1", "M_SKULL2"};	// graphic name of skulls

static oldmenu_t *currentMenu;		// current menudef
static oldmenu_t *TopLevelMenu;		// The main menu everything hangs off of

static DCanvas	*FireScreen;
static byte		FireRemap[256];

static char		*genders[3] = { "male", "female", "cyborg" };
static const TypeInfo *PlayerClass;
static FState	*PlayerState;
static int		PlayerTics;
static int		PlayerRotation;

static DCanvas			*SavePic;
static brokenlines_t	*SaveComment;
static List				SaveGames;
static FSaveGameNode	*TopSaveGame;
static FSaveGameNode	*SelSaveGame;
static FSaveGameNode	NewSaveNode;

// PRIVATE MENU DEFINITIONS ------------------------------------------------

//
// DOOM MENU
//
static oldmenuitem_t MainMenu[]=
{
	{1,0,'n',"M_NGAME",M_NewGame},
	{1,0,'l',"M_LOADG",M_LoadGame},
	{1,0,'s',"M_SAVEG",M_SaveGame},
	{1,0,'o',"M_OPTION",M_Options},		// [RH] Moved
	{1,0,'p',"M_PSETUP",M_PlayerSetup},	// [RH] Player setup
	{1,0,'r',"M_RDTHIS",M_ReadThis},	// Another hickup with Special edition.
	{1,0,'q',"M_QUITG",M_QuitDOOM}
};

static oldmenu_t MainDef =
{
	NUM_MENU_ITEMS (MainMenu),
	MainMenu,
	M_DrawMainMenu,
	97,64,
	0
};

//
// HERETIC MENU
//
static oldmenuitem_t HereticMainMenu[] =
{
	{1,1,'n',"NEW GAME",M_NewGame},
	{1,1,'o',"OPTIONS",M_Options},
	{1,1,'f',"GAME FILES",M_GameFiles},
	{1,1,'p',"PLAYER SETUP",M_PlayerSetup},
	{1,1,'i',"INFO",M_ReadThis},
	{1,1,'q',"QUIT GAME",M_QuitDOOM}
};

static oldmenu_t HereticMainDef =
{
	NUM_MENU_ITEMS (HereticMainMenu),
	HereticMainMenu,
	M_DrawHereticMainMenu,
	110, 56,
	0
};

//
// HEXEN "NEW GAME" MENU
//
static void M_ImSorry (int choice)
{
	M_PopMenuStack ();
}

static oldmenuitem_t SorryMenu[]=
{
	{-1,1,0,"Sorry. Hexen support",NULL},
	{-1,1,0,"is incomplete. Use the",NULL},
	{-1,1,0,"console to start a new",NULL},
	{-1,1,0,"game if you really want",NULL},
	{-1,1,0,"to see what's done.",NULL},
	{ 1,1,0,"",M_ImSorry}
};

static oldmenu_t SorryDef =
{
	6,
	SorryMenu,
	M_DrawEpisode,
	48,40,
	5
};

//
// DOOM EPISODE SELECT
//
static oldmenuitem_t EpisodeMenu[]=
{
	{1,0,'k',"M_EPI1", M_Episode},
	{1,0,'t',"M_EPI2", M_Episode},
	{1,0,'i',"M_EPI3", M_Episode},
	{1,0,'t',"M_EPI4", M_Episode}
};

static oldmenu_t EpiDef =
{
	3,
	EpisodeMenu,		// oldmenuitem_t ->
	M_DrawEpisode,		// drawing routine ->
	48,63,				// x,y
	0	 				// lastOn
};

//
// HERETIC EPISODE SELECT
//
static oldmenuitem_t HereticEpisodeMenu[] =
{
	{1,1,'c',"CITY OF THE DAMNED",M_Episode},
	{1,1,'h',"HELL'S MAW",M_Episode},
	{1,1,'d',"THE DOME OF D'SPARIL",M_Episode},
	{1,1,'o',"THE OSSUARY",M_Episode},
	{1,1,'s',"THE STAGNANT DEMESNE",M_Episode}
};

static oldmenu_t HereticEpiDef =
{
	3,
	HereticEpisodeMenu,
	M_DrawEpisode,
	80,50,
	0
};

//
// GAME FILES
//
static oldmenuitem_t FilesItems[] =
{
	{1,1,'l',"LOAD GAME",M_LoadGame},
	{1,1,'s',"SAVE GAME",M_SaveGame}
};

static oldmenu_t FilesMenu =
{
	NUM_MENU_ITEMS (FilesItems),
	FilesItems,
	M_DrawFiles,
	110,60,
	0
};

//
// DOOM SKILL SELECT
//
static oldmenuitem_t NewGameMenu[]=
{
	{1,0,'i',"M_JKILL",M_ChooseSkill},
	{1,0,'h',"M_ROUGH",M_ChooseSkill},
	{1,0,'h',"M_HURT",M_ChooseSkill},
	{1,0,'u',"M_ULTRA",M_ChooseSkill},
	{1,0,'n',"M_NMARE",M_ChooseSkill}
};

static oldmenu_t NewDef =
{
	NUM_MENU_ITEMS (NewGameMenu),
	NewGameMenu,		// oldmenuitem_t ->
	M_DrawNewGame,		// drawing routine ->
	48,63,				// x,y
	2					// lastOn
};

//
// HERETIC SKILL SELECT
//
static oldmenuitem_t HereticSkillItems[] =
{
	{1,1,'t',"THOU NEEDETH A WET-NURSE",M_ChooseSkill},
	{1,1,'y',"YELLOWBELLIES-R-US",M_ChooseSkill},
	{1,1,'b',"BRINGEST THEM ONETH",M_ChooseSkill},
	{1,1,'t',"THOU ART A SMITE-MEISTER",M_ChooseSkill},
	{1,1,'b',"BLACK PLAGUE POSSESSES THEE",M_ChooseSkill}
};

static oldmenu_t HereticSkillMenu =
{
	NUM_MENU_ITEMS (HereticSkillItems),
	HereticSkillItems,
	M_DrawNewGame,
	38, 30,
	2
};

//
// [RH] Player Setup Menu
//
static oldmenuitem_t PlayerSetupMenu[] =
{
	{ 1,0,'n',NULL,M_EditPlayerName},
	{ 2,0,'t',NULL,M_ChangePlayerTeam},
	{ 2,0,'r',NULL,M_SlidePlayerRed},
	{ 2,0,'g',NULL,M_SlidePlayerGreen},
	{ 2,0,'b',NULL,M_SlidePlayerBlue},
	{ 2,0,'e',NULL,M_ChangeGender},
	{ 2,0,'s',NULL,M_ChangeSkin},
	{ 2,0,'a',NULL,M_ChangeAutoAim}
};

static oldmenu_t PSetupDef =
{
	NUM_MENU_ITEMS (PlayerSetupMenu),
	PlayerSetupMenu,
	M_PlayerSetupDrawer,
	48,	47,
	0
};

//
// Read This! MENU 1 & 2
//
static oldmenuitem_t ReadMenu[] =
{
	{1,0,0,NULL,M_ReadThisMore}
};

static oldmenu_t ReadDef =
{
	1,
	ReadMenu,
	M_DrawReadThis,
	280,185,
	0
};

//
// LOAD GAME MENU
//
static oldmenuitem_t LoadMenu[]=
{
	{1,0,'1',NULL, NULL},
	{1,0,'2',NULL, NULL},
	{1,0,'3',NULL, NULL},
	{1,0,'4',NULL, NULL},
	{1,0,'5',NULL, NULL},
	{1,0,'6',NULL, NULL},
	{1,0,'7',NULL, NULL},
	{1,0,'8',NULL, NULL},
};

static oldmenu_t LoadDef =
{
	NUM_MENU_ITEMS(LoadMenu),
	LoadMenu,
	M_DrawLoad,
	80,54,
	0
};

//
// SAVE GAME MENU
//
static oldmenuitem_t SaveMenu[] =
{
	{1,0,'1',NULL, NULL},
	{1,0,'2',NULL, NULL},
	{1,0,'3',NULL, NULL},
	{1,0,'4',NULL, NULL},
	{1,0,'5',NULL, NULL},
	{1,0,'6',NULL, NULL},
	{1,0,'7',NULL, NULL},
	{1,0,'8',NULL, NULL},
};

static oldmenu_t SaveDef =
{
	NUM_MENU_ITEMS(LoadMenu),
	SaveMenu,
	M_DrawSave,
	80,54,
	0
};

// CODE --------------------------------------------------------------------

// [RH] Most menus can now be accessed directly
// through console commands.
CCMD (menu_main)
{
	M_StartControlPanel (true);
	M_SetupNextMenu (TopLevelMenu);
}

CCMD (menu_load)
{	// F3
	M_StartControlPanel (true);
	M_LoadGame (0);
}

CCMD (menu_save)
{	// F2
	M_StartControlPanel (true);
	M_SaveGame (0);
}

CCMD (menu_help)
{	// F1
	M_StartControlPanel (true);
	M_ReadThis (0);
}

CCMD (quicksave)
{	// F6
	//M_StartControlPanel (true);
	S_Sound (CHAN_VOICE, "menu/activate", 1, ATTN_NONE);
	M_QuickSave();
}

CCMD (quickload)
{	// F9
	//M_StartControlPanel (true);
	S_Sound (CHAN_VOICE, "menu/activate", 1, ATTN_NONE);
	M_QuickLoad();
}

CCMD (menu_endgame)
{	// F7
	//M_StartControlPanel (true);
	S_Sound (CHAN_VOICE, "menu/activate", 1, ATTN_NONE);
	M_EndGame(0);
}

CCMD (menu_quit)
{	// F10
	//M_StartControlPanel (true);
	S_Sound (CHAN_VOICE, "menu/activate", 1, ATTN_NONE);
	M_QuitDOOM(0);
}

CCMD (menu_game)
{
	M_StartControlPanel (true);
	M_NewGame(0);
}
								
CCMD (menu_options)
{
	M_StartControlPanel (true);
	M_Options(0);
}

CCMD (menu_player)
{
	M_StartControlPanel (true);
	M_PlayerSetup(0);
}

CCMD (bumpgamma)
{
	// [RH] Gamma correction tables are now generated
	// on the fly for *any* gamma level.
	// Q: What are reasonable limits to use here?

	float newgamma = Gamma + 0.1;

	if (newgamma > 3.0)
		newgamma = 1.0;

	Gamma = newgamma;
	Printf ("Gamma correction level %g\n", *Gamma);
}

void M_ActivateMenuInput ()
{
	menuactive = true;
}

void M_DeactivateMenuInput ()
{
	menuactive = false;
}

void M_DrawFiles ()
{
}

void M_GameFiles (int choice)
{
	M_SetupNextMenu (&FilesMenu);
}

//
// M_ReadSaveStrings
//
// Find savegames and read their titles
//
void M_ReadSaveStrings ()
{
	if (SaveGames.IsEmpty ())
	{
		long filefirst;
		findstate_t c_file;

		filefirst = I_FindFirst ("*.zds", &c_file);
		if (filefirst != -1)
		{
			do
			{
				FILE *file = fopen (I_FindName (&c_file), "rb");
				if (file != NULL)
				{
					char sig[PATH_MAX];

					if (fread (sig, 1, 16, file) == 16)
					{
						char title[SAVESTRINGSIZE];
						int i, c;
						bool oldVer = true;
						bool addIt = false;

						if (strncmp (sig, SAVESIG, 9) == 0)
						{
							fread (title, 1, SAVESTRINGSIZE, file);
							fseek (file, 8, SEEK_CUR);		// skip map name

							if (strncmp (sig, SAVESIG, 16) == 0)
							{
								// Was saved with a compatible ZDoom version,
								// so check if it's for the current game.
								// If it is, add it. Otherwise, ignore it.
								i = 0;
								do
								{
									c = fgetc (file);
									sig[i++] = c;
								} while (c != EOF && c != 0);
								if (c != EOF && stricmp (sig, W_GetWadName (1)) == 0)
								{
									addIt = true;
									oldVer = false;
								}
							}
							else
							{ // Game is from a different ZDoom version. Add it
							  // just so that the user has a means of deleting it
							  // if desired.
								addIt = true;
							}
						}
						else
						{ // Before 1.23 beta 21, the savesig came after the title
							memcpy (title, sig, 16);
							if (fread (title + 16, 1, SAVESTRINGSIZE-16, file) == SAVESTRINGSIZE-16 &&
								fread (sig, 1, 16, file) == 16 &&
								strncmp (sig, SAVESIG, 9) == 0)
							{ // Game is from an old ZDoom.
								addIt = true;
							}
						}

						if (addIt)
						{
							FSaveGameNode *node = new FSaveGameNode;
							node->Filename = copystring (I_FindName (&c_file));
							node->bOldVersion = oldVer;
							memcpy (node->Title, title, SAVESTRINGSIZE);
							M_InsertSaveNode (node);
						}
					}
					fclose (file);
				}
			} while (I_FindNext (filefirst, &c_file) == 0);
			I_FindClose (filefirst);
		}
	}
	if (SelSaveGame == NULL || SelSaveGame->Succ == NULL)
	{
		SelSaveGame = static_cast<FSaveGameNode *>(SaveGames.Head);
	}
}

void M_InsertSaveNode (FSaveGameNode *node)
{
	FSaveGameNode *probe;

	if (SaveGames.IsEmpty ())
	{
		SaveGames.AddHead (node);
		return;
	}

	if (node->bOldVersion)
	{ // Add node at bottom of list
		probe = static_cast<FSaveGameNode *>(SaveGames.TailPred);
		while (probe->Pred != NULL && probe->bOldVersion &&
			stricmp (node->Title, probe->Title) < 0)
		{
			probe = static_cast<FSaveGameNode *>(probe->Pred);
		}
		node->Insert (probe);
	}
	else
	{ // Add node at top of list
		probe = static_cast<FSaveGameNode *>(SaveGames.Head);
		while (probe->Succ != NULL && !probe->bOldVersion &&
			stricmp (node->Title, probe->Title) > 0)
		{
			probe = static_cast<FSaveGameNode *>(probe->Succ);
		}
		node->InsertBefore (probe);
	}
}

void M_NotifyNewSave (const char *file, const char *title)
{
	FSaveGameNode *node;

	// See if the file is already in our list
	for (node = static_cast<FSaveGameNode *>(SaveGames.Head);
		 node->Succ != NULL;
		 node = static_cast<FSaveGameNode *>(node->Succ))
	{
#ifdef unix
		if (strcmp (node->Filename, file) == 0)
#else
		if (stricmp (node->Filename, file) == 0)
#endif
		{
			strcpy (node->Title, title);
			return;
		}
	}

	node = new FSaveGameNode;
	strcpy (node->Title, title);
	node->Filename = copystring (file);
	node->bOldVersion = false;
	M_InsertSaveNode (node);
	SelSaveGame = node;

	if (quickSaveSlot == NULL)
	{
		quickSaveSlot = node;
	}
}

//
// M_LoadGame & Cie.
//
void M_DrawLoad (void)
{
	if (gameinfo.gametype == GAME_Doom)
	{
		patch_t *title = (patch_t *)W_CacheLumpName ("M_LOADG", PU_CACHE);
		screen->DrawPatchCleanNoMove (title,
			(SCREENWIDTH-SHORT(title)->width*CleanXfac)/2, 20*CleanYfac);
	}
	else
	{
		screen->DrawTextClean (CR_UNTRANSLATED,
			(SCREENWIDTH - screen->StringWidth ("LOAD GAME")*CleanXfac)/2, 10*CleanYfac,
			"LOAD GAME");
	}
	screen->SetFont (SmallFont);
	M_DrawSaveLoadCommon ();
}



//
// Draw border for the savegame description
// [RH] Width of the border is variable
//
void M_DrawSaveLoadBorder (int x, int y, int len)
{
	if (gameinfo.gametype == GAME_Doom)
	{
		int i;

		screen->DrawPatchClean ((patch_t *)W_CacheLumpName ("M_LSLEFT",PU_CACHE), x-8, y+7);
			
		for (i = 0; i < len; i++)
		{
			screen->DrawPatchClean ((patch_t *)W_CacheLumpName ("M_LSCNTR",PU_CACHE), x, y+7);
			x += 8;
		}

		screen->DrawPatchClean ((patch_t *)W_CacheLumpName ("M_LSRGHT",PU_CACHE), x, y+7);
	}
	else
	{
		screen->DrawPatchClean ((patch_t *)W_CacheLumpName ("M_FSLOT",PU_CACHE), x, y+1);
	}
}

static void M_ExtractSaveData (const FSaveGameNode *node)
{
	FILE *file;

	M_UnloadSaveData ();

	if (node != NULL &&
		node->Succ != NULL &&
		node->Filename != NULL &&
		!node->bOldVersion &&
		(file = fopen (node->Filename, "rb")) != NULL)
	{
		WORD x, y;
		int i;

		fseek (file, 16+SAVESTRINGSIZE+8, SEEK_SET);

		// Skip wad names
		x = 1;
		do
		{
			i = fgetc (file);
			if (i == EOF)
			{
				return;
			}
			else if (i == 0)
			{
				x = x + 1;
			}
			else
			{
				x = 0;
			}
		} while (x != 2);

		x = y = 0;

		// Extract comment
		fread (&x, 2, 1, file);
		x = SHORT(x);
		if (x != 0)
		{
			char *comment = new char[x];
			if (fread (comment, 1, x, file) == x)
			{
				SaveComment = V_BreakLines (216*screen->GetWidth()/640/CleanXfac, comment);
			}
			delete[] comment;
		}

		// Extract pic
		fread (&x, 2, 1, file);
		fread (&y, 2, 1, file);
		x = SHORT(x);
		y = SHORT(y);
		if (x * y != 0)
		{
			DWORD len = 0;
			fread (&len, 4, 1, file);
			SavePic = new DSimpleCanvas (x, y);
			if (len == 0)
			{
				SavePic->Lock ();
				fread (SavePic->GetBuffer(), 1, x * y, file);
				SavePic->Unlock ();
			}
			else
			{
				uLong newlen = x * y;
				int r;
				Byte *cprpic = new Byte[len];

				fread (cprpic, 1, len, file);
				SavePic->Lock ();
				r = uncompress (SavePic->GetBuffer(), &newlen, cprpic, len);
				SavePic->Unlock ();
				if (r != Z_OK || newlen != (uLong)(x * y))
				{
					delete SavePic;
					SavePic = NULL;
				}
				delete[] cprpic;
			}
		}
		fclose (file);
	}
}

static void M_UnloadSaveData ()
{
	if (SavePic != NULL)
	{
		delete SavePic;
	}
	if (SaveComment != NULL)
	{
		V_FreeBrokenLines (SaveComment);
	}

	SavePic = NULL;
	SaveComment = NULL;
}

static void M_DrawSaveLoadCommon ()
{
	const int savepicLeft = 10;
	const int savepicTop = 54*CleanYfac;
	const int savepicWidth = 216*screen->GetWidth()/640;
	const int savepicHeight = 135*screen->GetHeight()/400;

	const int rowHeight = (SmallFont->GetHeight() + 1) * CleanYfac;
	const int listboxLeft = savepicLeft + savepicWidth + 14;
	const int listboxTop = savepicTop;
	const int listboxWidth = screen->GetWidth() - listboxLeft - 10;
	const int listboxHeight1 = screen->GetHeight() - listboxTop - 10;
	const int listboxRows = (listboxHeight1 - 1) / rowHeight;
	const int listboxHeight = listboxRows * rowHeight + 1;
	const int listboxRight = listboxLeft + listboxWidth;
	const int listboxBottom = listboxTop + listboxHeight;

	const int commentLeft = savepicLeft;
	const int commentTop = savepicTop + savepicHeight + 16;
	const int commentWidth = savepicWidth;
	const int commentHeight = 51*CleanYfac;
	const int commentRight = commentLeft + commentWidth;
	const int commentBottom = commentTop + commentHeight;

	FSaveGameNode *node;
	int i;
	bool didSeeSelected = false;

	// Draw picture area
	M_DrawFrame (savepicLeft, savepicTop, savepicWidth, savepicHeight);
	if (SavePic != NULL)
	{
		SavePic->Blit (0, 0, SavePic->GetWidth(), SavePic->GetHeight(),
			screen, savepicLeft, savepicTop, savepicWidth, savepicHeight);
	}
	else
	{
		screen->Clear (savepicLeft, savepicTop,
			savepicLeft+savepicWidth, savepicTop+savepicHeight, 0);

		if (!SaveGames.IsEmpty ())
		{
			const char *text =
				(SelSaveGame == NULL || !SelSaveGame->bOldVersion)
				? "No Picture" : "Different\nVersion";
			const int textlen = screen->StringWidth (text)*CleanXfac;

			screen->DrawTextClean (CR_GOLD, savepicLeft+(savepicWidth-textlen)/2,
				savepicTop+(savepicHeight-rowHeight)/2, text);
		}
	}

	// Draw comment area
	M_DrawFrame (commentLeft, commentTop, commentWidth, commentHeight);
	screen->Clear (commentLeft, commentTop, commentRight, commentBottom, 0);
	if (SaveComment != NULL)
	{
		for (i = 0; SaveComment[i].width != -1 && i < 6; ++i)
		{
			screen->DrawTextClean (CR_GOLD, commentLeft, commentTop
				+ SmallFont->GetHeight()*i*CleanYfac, SaveComment[i].string);
		}
	}

	// Draw file area
	do
	{
		M_DrawFrame (listboxLeft, listboxTop, listboxWidth, listboxHeight);
		screen->Clear (listboxLeft, listboxTop, listboxRight, listboxBottom, 0);

		if (SaveGames.IsEmpty ())
		{
			const int textlen = screen->StringWidth ("No files")*CleanXfac;

			screen->DrawTextClean (CR_GOLD, listboxLeft+(listboxWidth-textlen)/2,
				listboxTop+(listboxHeight-rowHeight)/2, "No files");
			return;
		}

		for (i = 0, node = TopSaveGame;
			 i < listboxRows && node->Succ != NULL;
			 ++i, node = static_cast<FSaveGameNode *>(node->Succ))
		{
			if (node == SelSaveGame)
			{
				screen->Clear (listboxLeft, listboxTop+rowHeight*i,
					listboxRight, listboxTop+rowHeight*(i+1),
					ColorMatcher.Pick (genStringEnter ? 255 : 0, 0, genStringEnter ? 0 : 255));
				didSeeSelected = true;
				if (!genStringEnter)
				{
					screen->DrawTextClean (
						SelSaveGame->bOldVersion ? CR_BLUE : CR_WHITE, listboxLeft+1,
						listboxTop+rowHeight*i+CleanYfac, node->Title);
				}
				else
				{
					screen->DrawTextClean (CR_WHITE, listboxLeft+1,
						listboxTop+rowHeight*i+CleanYfac, savegamestring);
					screen->DrawTextClean (CR_WHITE,
						listboxLeft+1+screen->StringWidth (savegamestring)*CleanXfac,
						listboxTop+rowHeight*i+CleanYfac, underscore);
				}
			}
			else
			{
				screen->DrawTextClean (
					node->bOldVersion ? CR_BLUE : CR_TAN, listboxLeft+1,
					listboxTop+rowHeight*i+CleanYfac, node->Title);
			}
		}

		// This is dumb: If the selected node was not visible,
		// scroll down and redraw. M_SaveLoadResponder()
		// guarantees that if the node is not visible, it will
		// always be below the visible list instead of above it.
		// This should not really be done here, but I don't care.

		if (!didSeeSelected)
		{
			for (i = 1; node->Succ != NULL && node != SelSaveGame; ++i)
			{
				node = static_cast<FSaveGameNode *>(node->Succ);
			}
			if (node->Succ == NULL)
			{ // SelSaveGame is invalid
				didSeeSelected = true;
			}
			else
			{
				do
				{
					TopSaveGame = static_cast<FSaveGameNode *>(TopSaveGame->Succ);
				} while (--i);
			}
		}
	} while (!didSeeSelected);
}

// Draw a frame around the specified area using the view border
// frame graphics. The border is drawn outside the area, not in it.
void M_DrawFrame (int left, int top, int width, int height)
{
	gameborder_t *border = gameinfo.border;
	int offset = border->offset;
	int size = border->size;
	int x, y;

	for (x = left; x < left + width; x += size)
	{
		if (x + size > left + width)
			x = left + width - size;
		screen->DrawPatch ((patch_t *)W_CacheLumpName (border->t, PU_CACHE),
			x, top - offset);
		screen->DrawPatch ((patch_t *)W_CacheLumpName (border->b, PU_CACHE),
			x, top + height);
	}
	for (y = top; y < top + height; y += size)
	{
		if (y + size > top + height)
			y = top + height - size;
		screen->DrawPatch ((patch_t *)W_CacheLumpName (border->l, PU_CACHE),
			left - offset, y);
		screen->DrawPatch ((patch_t *)W_CacheLumpName (border->r, PU_CACHE),
			left + width, y);
	}
	// Draw beveled edge.
	screen->DrawPatch ((patch_t *)W_CacheLumpName (border->tl, PU_CACHE),
		left-offset, top-offset);
	
	screen->DrawPatch ((patch_t *)W_CacheLumpName (border->tr, PU_CACHE),
		left+width, top-offset);
	
	screen->DrawPatch ((patch_t *)W_CacheLumpName (border->bl, PU_CACHE),
		left-offset, top+height);
	
	screen->DrawPatch ((patch_t *)W_CacheLumpName (border->br, PU_CACHE),
		left+width, top+height);
}

//
// Selected from DOOM menu
//
void M_LoadGame (int choice)
{
	if (netgame)
	{
		M_StartMessage (GStrings(LOADNET), NULL, false);
		return;
	}
		
	M_SetupNextMenu (&LoadDef);
	drawSkull = false;
	M_ReadSaveStrings ();
	TopSaveGame = static_cast<FSaveGameNode *>(SaveGames.Head);
	M_ExtractSaveData (SelSaveGame);
}


//
//	M_SaveGame & Cie.
//
void M_DrawSave()
{
	if (gameinfo.gametype == GAME_Doom)
	{
		patch_t *title = (patch_t *)W_CacheLumpName ("M_SAVEG", PU_CACHE);
		screen->DrawPatchCleanNoMove (title,
			(SCREENWIDTH-SHORT(title)->width*CleanXfac)/2, 20*CleanYfac);
	}
	else
	{
		screen->DrawTextClean (CR_UNTRANSLATED,
			(SCREENWIDTH - screen->StringWidth ("SAVE GAME")*CleanXfac)/2, 10*CleanYfac,
			"SAVE GAME");
	}
	screen->SetFont (SmallFont);
	M_DrawSaveLoadCommon ();
}

//
// M_Responder calls this when the user is finished
//
void M_DoSave (FSaveGameNode *node)
{
	if (node != &NewSaveNode)
	{
		G_SaveGame (node->Filename, savegamestring);
	}
	else
	{
		// Find an unused filename, and save as that
		char filename[24];
		int i;
		FILE *test;

		for (i = 0;; ++i)
		{
			sprintf (filename, "save%d.zds", i);
			test = fopen (filename, "rb");
			if (test == NULL)
			{
				break;
			}
			fclose (test);
		}
		G_SaveGame (filename, savegamestring);
	}
	M_ClearMenus ();
	BorderNeedRefresh = screen->GetPageCount ();
}

//
// Selected from DOOM menu
//
void M_SaveGame (int choice)
{
	if (!usergame || (players[consoleplayer].health <= 0 && !multiplayer))
	{
		M_StartMessage (GStrings(SAVEDEAD), NULL, false);
		return;
	}
		
	if (gamestate != GS_LEVEL)
		return;
		
	M_SetupNextMenu(&SaveDef);
	drawSkull = false;

	M_ReadSaveStrings();
	SaveGames.AddHead (&NewSaveNode);
	TopSaveGame = static_cast<FSaveGameNode *>(SaveGames.Head);
	if (quickSaveSlot == NULL)
	{
		SelSaveGame = &NewSaveNode;
	}
	else
	{
		SelSaveGame = quickSaveSlot;
	}
	M_ExtractSaveData (SelSaveGame);
}



//
//		M_QuickSave
//
void M_QuickSaveResponse (int ch)
{
	if (ch == 'y')
	{
		M_DoSave (quickSaveSlot);
		S_Sound (CHAN_VOICE, "menu/dismiss", 1, ATTN_NONE);
	}
}

void M_QuickSave ()
{
	if (!usergame || (players[consoleplayer].health <= 0 && !multiplayer))
	{
		S_Sound (CHAN_VOICE, "menu/invalid", 1, ATTN_NONE);
		return;
	}

	if (gamestate != GS_LEVEL)
		return;
		
	if (quickSaveSlot == NULL)
	{
		M_StartControlPanel(false);
		M_SaveGame (0);
		quickSaveSlot = NULL; 	// means to pick a slot now
		return;
	}
	sprintf (tempstring, GStrings(QSPROMPT), quickSaveSlot->Title);
	M_StartMessage (tempstring, M_QuickSaveResponse, true);
}



//
// M_QuickLoad
//
void M_QuickLoadResponse (int ch)
{
	if (ch == 'y')
	{
		M_LoadSelect (quickSaveSlot);
		S_Sound (CHAN_VOICE, "menu/dismiss", 1, ATTN_NONE);
	}
}


void M_QuickLoad ()
{
	if (netgame)
	{
		M_StartMessage (GStrings(QLOADNET), NULL, false);
		return;
	}
		
	if (quickSaveSlot < 0)
	{
		M_StartControlPanel(false);
		M_LoadGame (0);
		return;
	}
	sprintf (tempstring, GStrings(QLPROMPT), quickSaveSlot->Title);
	M_StartMessage (tempstring, M_QuickLoadResponse, true);
}

//
// Read This Menus
//
void M_DrawReadThis ()
{
	if (gameinfo.flags & GI_PAGESARERAW)
	{
		screen->DrawPageBlock ((byte *)W_CacheLumpNum (
			W_GetNumForName (gameinfo.info.indexed.basePage) +
			InfoType, PU_CACHE));
	}
	else
	{
		screen->DrawPatchIndirect ((patch_t *)W_CacheLumpName (
			gameinfo.info.infoPage[InfoType-1], PU_CACHE), 0, 0);
	}
}

//
// M_DrawMainMenu
//
void M_DrawMainMenu (void)
{
	screen->DrawPatchClean ((patch_t *)W_CacheLumpName("M_DOOM",PU_CACHE), 94, 2);
}

void M_DrawHereticMainMenu ()
{
	int frame = (MenuTime / 3) % 18;
	screen->DrawPatchClean ((patch_t *)W_CacheLumpName("M_HTIC",PU_CACHE), 88, 0);
	screen->DrawPatchClean ((patch_t *)W_CacheLumpNum(SkullBaseLump+(17-frame),
		PU_CACHE), 40, 10);
	screen->DrawPatchClean ((patch_t *)W_CacheLumpNum(SkullBaseLump+frame,
		PU_CACHE), 232, 10);
}

//
// M_NewGame
//
void M_DrawNewGame(void)
{
	if (gameinfo.gametype == GAME_Doom)
	{
		screen->DrawPatchClean ((patch_t *)W_CacheLumpName("M_NEWG",PU_CACHE), 96, 14);
		screen->DrawPatchClean ((patch_t *)W_CacheLumpName("M_SKILL",PU_CACHE), 54, 38);
	}
}

void M_NewGame(int choice)
{
	if (netgame && !demoplayback)
	{
		M_StartMessage (GStrings(NEWGAME), NULL, false);
		return;
	}

	if (gameinfo.gametype == GAME_Hexen)
		M_SetupNextMenu (&SorryDef);
	else if (gameinfo.flags & GI_MAPxx)
		M_SetupNextMenu (&NewDef);
	else if (gameinfo.gametype == GAME_Doom)
		M_SetupNextMenu (&EpiDef);
	else
		M_SetupNextMenu (&HereticEpiDef);
}


//
//		M_Episode
//
int 	epi;

void M_DrawEpisode ()
{
	if (gameinfo.gametype == GAME_Doom)
	{
		screen->DrawPatchClean ((patch_t *)W_CacheLumpName (
			"M_EPISOD", PU_CACHE), 54, 38);
	}
}

void M_VerifyNightmare (int ch)
{
	if (ch != 'y')
		return;
		
	gameskill = 4;
	G_DeferedInitNew (CalcMapName (epi+1, 1));
	gamestate = gamestate == GS_FULLCONSOLE ? GS_HIDECONSOLE : gamestate;
	M_ClearMenus ();
}

void M_ChooseSkill (int choice)
{
	if (gameinfo.gametype == GAME_Doom && choice == NewDef.numitems - 1)
	{
		M_StartMessage (GStrings(NIGHTMARE), M_VerifyNightmare, true);
		return;
	}

	gameskill = choice;
	gamestate = gamestate == GS_FULLCONSOLE ? GS_HIDECONSOLE : gamestate;
	G_DeferedInitNew (CalcMapName (epi+1, 1));
	gamestate = gamestate == GS_FULLCONSOLE ? GS_HIDECONSOLE : gamestate;
	M_ClearMenus ();
}

void M_Episode (int choice)
{
	if ((gameinfo.flags & GI_SHAREWARE) && choice)
	{
		M_StartMessage(GStrings(SWSTRING),NULL,false);
		M_SetupNextMenu(&ReadDef);
		return;
	}

	epi = choice;
	if (gameinfo.gametype == GAME_Doom)
		M_SetupNextMenu (&NewDef);
	else
		M_SetupNextMenu (&HereticSkillMenu);
}



void M_Options(int choice)
{
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
		S_Sound (CHAN_VOICE, "menu/invalid", 1, ATTN_NONE);
		return;
	}
		
	if (netgame)
	{
		M_StartMessage(GStrings(NETEND),NULL,false);
		return;
	}
		
	M_StartMessage(GStrings(ENDGAME),M_EndGameResponse,true);
}




//
// M_ReadThis
//
void M_ReadThis (int choice)
{
	drawSkull = false;
	InfoType = 1;
	M_SetupNextMenu (&ReadDef);
}

void M_ReadThisMore (int choice)
{
	InfoType++;
	if (gameinfo.flags & GI_PAGESARERAW)
	{
		if (InfoType >= gameinfo.info.indexed.numPages)
		{
			M_FinishReadThis (0);
		}
	}
	else if (InfoType > 2)
	{
		M_FinishReadThis (0);
	}
}

void M_FinishReadThis (int choice)
{
	drawSkull = true;
	M_PopMenuStack ();
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
			S_Sound (CHAN_VOICE, gameinfo.quitSounds[(gametic>>2)&7],
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
	if (gameinfo.gametype == GAME_Doom)
	{
		sprintf (endstring, "%s\n\n%s",
			GStrings(QUITMSG + (gametic % NUM_QUITMESSAGES)), GStrings(DOSY));
	}
	else
	{
		strcpy (endstring, GStrings(RAVENQUITMSG));
	}

	M_StartMessage (endstring, M_QuitResponse, true);
}


//
// [RH] Player Setup Menu code
//
static void M_PlayerSetup (int choice)
{
	choice = 0;
	strcpy (savegamestring, name);
	M_DemoNoPlay = true;
	if (demoplayback)
		G_CheckDemoStatus ();
	M_SetupNextMenu (&PSetupDef);
	PlayerState = GetDefaultByType (PlayerClass)->SeeState;
	PlayerTics = PlayerState->GetTics();
	if (FireScreen == NULL)
		FireScreen = new DSimpleCanvas (72, 72+5);
}

static void M_PlayerSetupTicker (void)
{
	// Based on code in f_finale.c
	if (--PlayerTics > 0)
		return;

	if (PlayerState->GetTics() == -1 || PlayerState->GetNextState() == NULL)
	{
		PlayerState = GetDefaultByType (PlayerClass)->SeeState;
	}
	else
	{
		PlayerState = PlayerState->GetNextState();
	}
	PlayerTics = PlayerState->GetTics();
}

static void M_PlayerSetupDrawer ()
{
	int x, xo, yo;
	EColorRange label, value;
	DWORD color;

	if (gameinfo.gametype != GAME_Doom)
	{
		xo = 5;
		yo = 5;
		label = CR_GREEN;
		value = CR_UNTRANSLATED;
	}
	else
	{
		xo = yo = 0;
		label = CR_UNTRANSLATED;
		value = CR_GREY;
	}

	// Draw title
	if (gameinfo.gametype == GAME_Doom)
	{
		patch_t *patch = (patch_t *)W_CacheLumpName ("M_PSTTL", PU_CACHE);

		screen->DrawPatchClean (patch,
			160 - (SHORT(patch->width) >> 1),
			PSetupDef.y - (SHORT(patch->height) * 3));
	}
	else
	{
		screen->DrawTextCleanMove (CR_UNTRANSLATED,
			160 - screen->StringWidth ("PLAYER SETUP")/2,
			15,
			"PLAYER SETUP");
	}

	screen->SetFont (SmallFont);

	// Draw player name box
	screen->DrawTextCleanMove (label, PSetupDef.x, PSetupDef.y+yo, "Name");
	M_DrawSaveLoadBorder (PSetupDef.x + 56, PSetupDef.y, MAXPLAYERNAME+1);
	screen->DrawTextCleanMove (CR_UNTRANSLATED, PSetupDef.x + 56 + xo, PSetupDef.y+yo, savegamestring);

	// Draw cursor for player name box
	if (genStringEnter)
		screen->DrawTextCleanMove (CR_UNTRANSLATED,
			PSetupDef.x + screen->StringWidth(savegamestring) + 56+xo,
			PSetupDef.y + yo, underscore);

	// Draw player team setting
	x = screen->StringWidth ("Team") + 8 + PSetupDef.x;
	screen->DrawTextCleanMove (label, PSetupDef.x, PSetupDef.y + LINEHEIGHT+yo, "Team");
	screen->DrawTextCleanMove (value, x, PSetupDef.y + LINEHEIGHT+yo,
		players[consoleplayer].userinfo.team == TEAM_None ? "None" :
		TeamNames[players[consoleplayer].userinfo.team]);

	// Draw player character
	{
		int x = 320 - 88 - 32 + xo, y = PSetupDef.y + LINEHEIGHT*3 - 14 + yo;

		x = (x-160)*CleanXfac+(SCREENWIDTH>>1);
		y = (y-100)*CleanYfac+(SCREENHEIGHT>>1);
		if (!FireScreen)
		{
			screen->Clear (x, y, x + 72 * CleanXfac, y + 72 * CleanYfac+yo, 0);
		}
		else
		{
			// [RH] The following fire code is based on the PTC fire demo
			int a, b;
			byte *from;
			int width, height, pitch;

			FireScreen->Lock ();

			width = FireScreen->GetWidth();
			height = FireScreen->GetHeight();
			pitch = FireScreen->GetPitch();

			from = FireScreen->GetBuffer() + (height - 3) * pitch;
			for (a = 0; a < width; a++, from++)
			{
				*from = *(from + (pitch << 1)) = M_Random();
			}

			from = FireScreen->GetBuffer();
			for (b = 0; b < FireScreen->GetHeight() - 4; b += 2)
			{
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

			M_DrawPlayerBackdrop (x, y - 1);
			FireScreen->Unlock ();
		}
	}
	{
		spriteframe_t *sprframe =
			&sprites[skins[players[consoleplayer].userinfo.skin].sprite].spriteframes[PlayerState->GetFrame()];

		V_ColorMap = translationtables + consoleplayer*256*2;
		screen->DrawTranslatedPatchClean ((patch_t *)W_CacheLumpNum (
			sprframe->lump[PlayerRotation], PU_CACHE),
			320 - 52 - 32 + xo, PSetupDef.y + LINEHEIGHT*3 + 52);

		screen->DrawPatchClean ((patch_t *)W_CacheLumpName ("M_PBOX", PU_CACHE),
			320 - 88 - 32 + 36 + xo, PSetupDef.y + LINEHEIGHT*3 + 22 + yo);

		const char *str = "PRESS " TEXTCOLOR_WHITE "SPACE";
		screen->DrawTextCleanMove (CR_GOLD, 320 - 52 - 32 -
			screen->StringWidth (str)/2,
			PSetupDef.y + LINEHEIGHT*3 + 76, str);
		str = PlayerRotation ? "TO SEE FRONT" : "TO SEE BACK";
		screen->DrawTextCleanMove (CR_GOLD, 320 - 52 - 32 -
			screen->StringWidth (str)/2,
			PSetupDef.y + LINEHEIGHT*3 + 76 + SmallFont->GetHeight (), str);
	}

	// Draw player color sliders
	//V_DrawTextCleanMove (CR_GREY, PSetupDef.x, PSetupDef.y + LINEHEIGHT, "Color");

	screen->DrawTextCleanMove (label, PSetupDef.x, PSetupDef.y + LINEHEIGHT*2+yo, "Red");
	screen->DrawTextCleanMove (label, PSetupDef.x, PSetupDef.y + LINEHEIGHT*3+yo, "Green");
	screen->DrawTextCleanMove (label, PSetupDef.x, PSetupDef.y + LINEHEIGHT*4+yo, "Blue");

	x = screen->StringWidth ("Green") + 8 + PSetupDef.x;
	color = players[consoleplayer].userinfo.color;

	M_DrawSlider (x, PSetupDef.y + LINEHEIGHT*2+yo, 0.0f, 255.0f, RPART(color));
	M_DrawSlider (x, PSetupDef.y + LINEHEIGHT*3+yo, 0.0f, 255.0f, GPART(color));
	M_DrawSlider (x, PSetupDef.y + LINEHEIGHT*4+yo, 0.0f, 255.0f, BPART(color));

	// Draw gender setting
	x = screen->StringWidth ("Gender") + 8 + PSetupDef.x;
	screen->DrawTextCleanMove (label, PSetupDef.x, PSetupDef.y + LINEHEIGHT*5+yo, "Gender");
	screen->DrawTextCleanMove (value, x, PSetupDef.y + LINEHEIGHT*5+yo,
		genders[players[consoleplayer].userinfo.gender]);

	// Draw skin setting
	x = screen->StringWidth ("Skin") + 8 + PSetupDef.x;
	screen->DrawTextCleanMove (label, PSetupDef.x, PSetupDef.y + LINEHEIGHT*6+yo, "Skin");
	screen->DrawTextCleanMove (value, x, PSetupDef.y + LINEHEIGHT*6+yo,
		skins[players[consoleplayer].userinfo.skin].name);

	// Draw autoaim setting
	x = screen->StringWidth ("Autoaim") + 8 + PSetupDef.x;

	screen->DrawTextCleanMove (label, PSetupDef.x, PSetupDef.y + LINEHEIGHT*7+yo, "Autoaim");
	screen->DrawTextCleanMove (value, x, PSetupDef.y + LINEHEIGHT*7+yo,
		autoaim == 0 ? "Never" :
		autoaim <= 0.25 ? "Very Low" :
		autoaim <= 0.5 ? "Low" :
		autoaim <= 1 ? "Medium" :
		autoaim <= 2 ? "High" :
		autoaim <= 3 ? "Very High" : "Always");
}

static void M_DrawPlayerBackdrop (int x, int y)
{
	DCanvas *src = FireScreen;
	DCanvas *dest = screen;
	byte *destline, *srcline;
	const int destwidth = src->GetWidth() * CleanXfac;
	const int destheight = src->GetHeight() * CleanYfac;
	const int desty = y;
	const int destx = x;
	const fixed_t fracxstep = FRACUNIT / CleanXfac;
	const fixed_t fracystep = FRACUNIT / CleanYfac;
	fixed_t fracx, fracy = 0;


	if (fracxstep == FRACUNIT)
	{
		for (y = desty; y < desty + destheight; y++, fracy += fracystep)
		{
			srcline = src->GetBuffer() + (fracy >> FRACBITS) * src->GetPitch();
			destline = dest->GetBuffer() + y * dest->GetPitch() + destx;

			for (x = 0; x < destwidth; x++)
			{
				destline[x] = FireRemap[srcline[x]];
			}
		}
	}
	else
	{
		for (y = desty; y < desty + destheight; y++, fracy += fracystep)
		{
			srcline = src->GetBuffer() + (fracy >> FRACBITS) * src->GetPitch();
			destline = dest->GetBuffer() + y * dest->GetPitch() + destx;
			for (x = fracx = 0; x < destwidth; x++, fracx += fracxstep)
			{
				destline[x] = FireRemap[srcline[fracx >> FRACBITS]];
			}
		}
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
		skin = (skin < (int)numskins - 1) ? skin + 1 : 0;

	cvar_set ("skin", skins[skin].name);
}

static void M_ChangeAutoAim (int choice)
{
	static const float ranges[] = { 0, 0.25, 0.5, 1, 2, 3, 5000 };
	float aim = autoaim;
	int i;

	if (!choice) {
		// Select a lower autoaim

		for (i = 6; i >= 1; i--)
		{
			if (aim >= ranges[i])
			{
				aim = ranges[i - 1];
				break;
			}
		}
	}
	else
	{
		// Select a higher autoaim

		for (i = 5; i >= 0; i--)
		{
			if (aim >= ranges[i])
			{
				aim = ranges[i + 1];
				break;
			}
		}
	}

	autoaim = aim;
}

static void M_EditPlayerName (int choice)
{
	// we are going to be intercepting all chars
	genStringEnter = 1;
	genStringEnd = M_PlayerNameChanged;
	genStringLen = MAXPLAYERNAME;
	
	saveSlot = 0;
	saveCharIndex = strlen (savegamestring);
}

static void M_PlayerNameChanged (FSaveGameNode *dummy)
{
	char command[SAVESTRINGSIZE+8];

	sprintf (command, "name \"%s\"", savegamestring);
	C_DoCommand (command);
}

static void M_ChangePlayerTeam (int choice)
{
	if (!choice)
	{
		if (team == 0)
		{
			team = TEAM_None;
		}
		else if (team == TEAM_None)
		{
			team = NUM_TEAMS-1;
		}
		else
		{
			team = team - 1;
		}
	}
	else
	{
		if (team == NUM_TEAMS-1)
		{
			team = TEAM_None;
		}
		else if (team == TEAM_None)
		{
			team = 0;
		}
		else
		{
			team = team + 1;
		}	
	}
}

static void SendNewColor (int red, int green, int blue)
{
	char command[24];

	sprintf (command, "color \"%02x %02x %02x\"", red, green, blue);
	C_DoCommand (command);
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
void M_StartMessage (const char *string, void (*routine)(int), bool input)
{
	C_HideConsole ();
	messageLastMenuActive = menuactive;
	messageToPrint = 1;
	messageString = string;
	messageRoutine = routine;
	messageNeedsInput = input;
	if (!menuactive)
		M_ActivateMenuInput ();
	if (input)
	{
		S_StopSound ((AActor *)NULL, CHAN_VOICE);
		S_Sound (CHAN_VOICE, "menu/prompt", 1, ATTN_NONE);
	}
	return;
}



//
//		Find string height from hu_font chars
//
int M_StringHeight (const char *string)
{
	int h;
	int height = screen->Font->GetHeight ();
		
	h = height;
	while (*string)
	{
		if ((*string++) == '\n')
			h += height;
	}
				
	return h;
}



//
// CONTROL PANEL
//

//
// M_Responder
//
BOOL M_Responder (event_t *ev)
{
	int ch;
	int i;

	ch = -1;

	if (!menuactive && ev->type == EV_KeyDown)
	{
		// Pop-up menu?
		if (ev->data1 == KEY_ESCAPE)
		{
			M_StartControlPanel (true);
			M_SetupNextMenu (TopLevelMenu);
			return true;
		}
		// If devparm is set, pressing F1 always takes a screenshot no matter
		// what it's bound to. (for those who don't bother to read the docs)
		if (devparm && ev->data1 == KEY_F1)
		{
			G_ScreenShot (NULL);
			return true;
		}
	}
	else if (ev->type == EV_GUI_Event && menuactive && !chatmodeon)
	{
		if (ev->subtype == EV_GUI_KeyDown || ev->subtype == EV_GUI_KeyRepeat)
		{
			ch = ev->data1;
		}
		else if (ev->subtype == EV_GUI_Char && genStringEnter)
		{
			ch = ev->data1;
			if (saveCharIndex < genStringLen &&
				screen->StringWidth (savegamestring) < (genStringLen-1)*8)
			{
				savegamestring[saveCharIndex] = ch;
				savegamestring[++saveCharIndex] = 0;
			}
			return true;
		}
	}
	
	if (OptionsActive && !chatmodeon)
	{
		M_OptResponder (ev);
		return true;
	}
	
	if (ch == -1)
		return false;

	// Save Game string input
	// [RH] and Player Name string input
	if (genStringEnter)
	{
		switch (ch)
		{
		case '\b':
			if (saveCharIndex > 0)
			{
				saveCharIndex--;
				savegamestring[saveCharIndex] = 0;
			}
			break;

		case GK_ESCAPE:
			genStringEnter = 0;
			M_ClearMenus ();
			break;
								
		case '\r':
			if (savegamestring[0])
			{
				genStringEnter = 0;
				if (messageToPrint)
					M_ClearMenus ();
				genStringEnd (SelSaveGame);	// [RH] Function to call when enter is pressed
			}
			break;
		}
		return true;
	}

	// Take care of any messages that need input
	if (messageToPrint)
	{
		ch = tolower (ch);
		if (messageNeedsInput &&
			ch != ' ' && ch != 'n' && ch != 'y' && ch != GK_ESCAPE)
		{
			return false;
		}

		menuactive = messageLastMenuActive;
		messageToPrint = 0;
		if (messageRoutine)
			messageRoutine (ch);

		if (!menuactive)
			M_DeactivateMenuInput ();
		SB_state = screen->GetPageCount ();	// refresh the statbar
		BorderNeedRefresh = screen->GetPageCount ();
		S_Sound (CHAN_VOICE, "menu/dismiss", 1, ATTN_NONE);
		return true;
	}

	if (ch == GK_ESCAPE)
	{
		// [RH] Escape now moves back one menu instead of quitting
		//		the menu system. Thus, backspace is ignored.
		currentMenu->lastOn = itemOn;
		M_PopMenuStack ();
		return true;
	}

	if (currentMenu == &SaveDef || currentMenu == &LoadDef)
	{
		return M_SaveLoadResponder (ev);
	}

	// Keys usable within menu
	switch (ch)
	{
	case GK_DOWN:
		do
		{
			if (itemOn+1 > currentMenu->numitems-1)
				itemOn = 0;
			else itemOn++;
			S_Sound (CHAN_VOICE, "menu/cursor", 1, ATTN_NONE);
		} while(currentMenu->menuitems[itemOn].status==-1);
		return true;

	case GK_UP:
		do
		{
			if (!itemOn)
				itemOn = currentMenu->numitems-1;
			else itemOn--;
			S_Sound (CHAN_VOICE, "menu/cursor", 1, ATTN_NONE);
		} while(currentMenu->menuitems[itemOn].status==-1);
		return true;

	case GK_LEFT:
		if (currentMenu->menuitems[itemOn].routine &&
			currentMenu->menuitems[itemOn].status == 2)
		{
			S_Sound (CHAN_VOICE, "menu/change", 1, ATTN_NONE);
			currentMenu->menuitems[itemOn].routine(0);
		}
		return true;

	case GK_RIGHT:
		if (currentMenu->menuitems[itemOn].routine &&
			currentMenu->menuitems[itemOn].status == 2)
		{
			S_Sound (CHAN_VOICE, "menu/change", 1, ATTN_NONE);
			currentMenu->menuitems[itemOn].routine(1);
		}
		return true;

	case '\r':
		if (currentMenu->menuitems[itemOn].routine &&
			currentMenu->menuitems[itemOn].status)
		{
			currentMenu->lastOn = itemOn;
			if (currentMenu->menuitems[itemOn].status == 2)
			{
				currentMenu->menuitems[itemOn].routine(1);		// right arrow
				S_Sound (CHAN_VOICE, "menu/change", 1, ATTN_NONE);
			}
			else
			{
				currentMenu->menuitems[itemOn].routine(itemOn);
				S_Sound (CHAN_VOICE, "menu/choose", 1, ATTN_NONE);
			}
		}
		return true;

	case ' ':
		if (currentMenu == &PSetupDef)
		{
			PlayerRotation ^= 4;
			break;
		}
		// intentional fall-through

	default:
		if (ch)
		{
			ch = tolower (ch);
			for (i = (itemOn + 1) % currentMenu->numitems;
				 i != itemOn;
				 i = (i + 1) % currentMenu->numitems)
			{
				if (currentMenu->menuitems[i].alphaKey == ch)
				{
					break;
				}
			}
			if (currentMenu->menuitems[i].alphaKey == ch)
			{
				itemOn = i;
				S_Sound (CHAN_VOICE, "menu/cursor", 1, ATTN_NONE);
				return true;
			}
		}
		break;
	}

	// [RH] Menu eats all keydown events while active
	return (ev->subtype == EV_GUI_KeyDown);
}

BOOL M_SaveLoadResponder (event_t *ev)
{
	char workbuf[512];

	if (SelSaveGame != NULL && SelSaveGame->Succ != NULL)
	{
		switch (ev->data1)
		{
		case GK_F1:
			if (SelSaveGame->Filename != NULL)
			{
				sprintf (workbuf, "File on disk:\n%s", SelSaveGame->Filename);
				if (SaveComment != NULL)
				{
					V_FreeBrokenLines (SaveComment);
				}
				SaveComment = V_BreakLines (216*screen->GetWidth()/640/CleanXfac, workbuf);
			}
			break;

		case GK_UP:
			if (SelSaveGame != SaveGames.Head)
			{
				if (SelSaveGame == TopSaveGame)
				{
					TopSaveGame = static_cast<FSaveGameNode *>(TopSaveGame->Pred);
				}
				SelSaveGame = static_cast<FSaveGameNode *>(SelSaveGame->Pred);
			}
			else
			{
				SelSaveGame = static_cast<FSaveGameNode *>(SaveGames.TailPred);
			}
			M_UnloadSaveData ();
			M_ExtractSaveData (SelSaveGame);
			break;

		case GK_DOWN:
			if (SelSaveGame != SaveGames.TailPred)
			{
				SelSaveGame = static_cast<FSaveGameNode *>(SelSaveGame->Succ);
			}
			else
			{
				SelSaveGame = TopSaveGame =
					static_cast<FSaveGameNode *>(SaveGames.Head);
			}
			M_UnloadSaveData ();
			M_ExtractSaveData (SelSaveGame);
			break;

		case GK_DEL:
		case '\b':
			if (SelSaveGame != &NewSaveNode)
			{
				sprintf (endstring, "Do you really want to delete the savegame\n"
					TEXTCOLOR_WHITE "%s" TEXTCOLOR_NORMAL "?\n\nPress Y or N.",
					SelSaveGame->Title);
				M_StartMessage (endstring, M_DeleteSaveResponse, true);
			}
			break;

		case 'N':
			if (currentMenu == &SaveDef)
			{
				SelSaveGame = TopSaveGame = &NewSaveNode;
				M_UnloadSaveData ();
			}
			break;

		case '\r':
			if (currentMenu == &LoadDef)
			{
				M_LoadSelect (SelSaveGame);
			}
			else
			{
				M_SaveSelect (SelSaveGame);
			}
		}
	}

	return (ev->subtype == EV_GUI_KeyDown);
}

static void M_LoadSelect (const FSaveGameNode *file)
{
	G_LoadGame (file->Filename);
	if (gamestate == GS_FULLCONSOLE)
	{
		gamestate = GS_HIDECONSOLE;
	}
	M_ClearMenus ();
	BorderNeedRefresh = screen->GetPageCount ();
	if (quickSaveSlot == NULL)
	{
		quickSaveSlot = SelSaveGame;
	}
}

//
// User wants to save. Start string input for M_Responder
//
static void M_SaveSelect (const FSaveGameNode *file)
{
	// we are going to be intercepting all chars
	genStringEnter = 1;
	genStringEnd = M_DoSave;
	genStringLen = SAVESTRINGSIZE-1;

	if (file != &NewSaveNode)
	{
		strcpy (savegamestring, file->Title);
	}
	else
	{
		savegamestring[0] = 0;
	}
	saveCharIndex = strlen (savegamestring);
}

static void M_DeleteSaveResponse (int choice)
{
	if (choice == 'y')
	{
		FSaveGameNode *next = static_cast<FSaveGameNode *>(SelSaveGame->Succ);
		if (next->Succ == NULL)
		{
			next = static_cast<FSaveGameNode *>(SelSaveGame->Pred);
			if (next->Pred == NULL)
			{
				next = NULL;
			}
		}

		remove (SelSaveGame->Filename);
		M_UnloadSaveData ();
		if (SelSaveGame == TopSaveGame)
		{
			TopSaveGame = next;
		}
		if (quickSaveSlot == SelSaveGame)
		{
			quickSaveSlot = NULL;
		}
		SelSaveGame->Remove ();
		delete[] SelSaveGame->Filename;
		delete SelSaveGame;
		SelSaveGame = next;
		M_ExtractSaveData (SelSaveGame);
	}
}

//
// M_StartControlPanel
//
void M_StartControlPanel (bool makeSound)
{
	// intro might call this repeatedly
	if (menuactive)
		return;
	
	drawSkull = true;
	MenuStackDepth = 0;
	currentMenu = TopLevelMenu;
	itemOn = currentMenu->lastOn;
	C_HideConsole ();				// [RH] Make sure console goes bye bye.
	OptionsActive = false;			// [RH] Make sure none of the options menus appear.
	M_ActivateMenuInput ();

	if (makeSound)
	{
		S_Sound (CHAN_VOICE, "menu/activate", 1, ATTN_NONE);
	}
}


//
// M_Drawer
// Called after the view has been rendered,
// but before it has been blitted.
//
void M_Drawer ()
{
	int i, x, y, max;

	// Horiz. & Vertically center string and print it.
	if (messageToPrint)
	{
		screen->Dim ();
		BorderNeedRefresh = screen->GetPageCount ();
		SB_state = screen->GetPageCount ();

		brokenlines_t *lines = V_BreakLines (320, messageString);
		y = 100;

		for (i = 0; lines[i].width != -1; i++)
			y -= screen->Font->GetHeight () / 2;

		for (i = 0; lines[i].width != -1; i++)
		{
			screen->DrawTextCleanMove (CR_UNTRANSLATED, 160 - lines[i].width/2, y, lines[i].string);
			y += screen->Font->GetHeight ();
		}

		V_FreeBrokenLines (lines);
	}
	else if (menuactive)
	{
		screen->Dim ();
		BorderNeedRefresh = screen->GetPageCount ();
		SB_state = screen->GetPageCount ();

		if (OptionsActive)
		{
			M_OptDrawer ();
		}
		else
		{
			screen->SetFont (BigFont);
			if (currentMenu->routine)
				currentMenu->routine(); 		// call Draw routine
		
			// DRAW MENU
			x = currentMenu->x;
			y = currentMenu->y;
			max = currentMenu->numitems;

			for (i = 0; i < max; i++)
			{
				if (currentMenu->menuitems[i].name)
				{
					if (currentMenu->menuitems[i].fulltext)
					{
						screen->DrawTextCleanMove (CR_UNTRANSLATED, x, y,
							currentMenu->menuitems[i].name);
					}
					else
					{
						screen->DrawPatchClean ((patch_t *)W_CacheLumpName (
							currentMenu->menuitems[i].name ,PU_CACHE), x, y);
					}
				}
				y += LINEHEIGHT;
			}
			screen->SetFont (SmallFont);
			
			// DRAW CURSOR
			if (drawSkull)
			{
				if (gameinfo.gametype == GAME_Doom)
				{
					screen->DrawPatchClean ((patch_t *)W_CacheLumpName (
						skullName[whichSkull], PU_CACHE),
						x + SKULLXOFF, currentMenu->y - 5 + itemOn*LINEHEIGHT);
				}
				else
				{
					screen->DrawPatchClean ((patch_t *)W_CacheLumpName (
						MenuTime & 16 ? "M_SLCTR1" : "M_SLCTR2", PU_CACHE),
						x + SELECTOR_XOFFSET,
						currentMenu->y + itemOn*LINEHEIGHT + SELECTOR_YOFFSET);
				}
			}
		}
	}
}


static void M_ClearSaveStuff ()
{
	M_UnloadSaveData ();
	if (SaveGames.Head == &NewSaveNode)
	{
		SaveGames.RemHead ();
		if (SelSaveGame == &NewSaveNode)
		{
			SelSaveGame = static_cast<FSaveGameNode *>(SaveGames.Head);
		}
		if (TopSaveGame == &NewSaveNode)
		{
			TopSaveGame = static_cast<FSaveGameNode *>(SaveGames.Head);
		}
	}
}

//
// M_ClearMenus
//
void M_ClearMenus ()
{
	if (FireScreen)
	{
		delete FireScreen;
		FireScreen = NULL;
	}
	M_ClearSaveStuff ();
	M_DeactivateMenuInput ();
	MenuStackDepth = 0;
	OptionsActive = false;
	InfoType = 0;
	drawSkull = true;
	M_DemoNoPlay = false;
	BorderNeedRefresh = screen->GetPageCount ();
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
	InfoType = 0;
	M_ClearSaveStuff ();
	if (MenuStackDepth > 1)
	{
		MenuStackDepth -= 2;
		if (MenuStack[MenuStackDepth].isNewStyle)
		{
			OptionsActive = true;
			CurrentMenu = MenuStack[MenuStackDepth].menu.newmenu;
			CurrentItem = CurrentMenu->lastOn;
		}
		else
		{
			OptionsActive = false;
			currentMenu = MenuStack[MenuStackDepth].menu.old;
			itemOn = currentMenu->lastOn;
		}
		drawSkull = MenuStack[MenuStackDepth].drawSkull;
		++MenuStackDepth;
		S_Sound (CHAN_VOICE, "menu/backup", 1, ATTN_NONE);
	}
	else
	{
		M_ClearMenus ();
		S_Sound (CHAN_VOICE, "menu/clear", 1, ATTN_NONE);
	}
}


//
// M_Ticker
//
void M_Ticker (void)
{
	if (!menuactive)
	{
		return;
	}
	MenuTime++;
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
EXTERN_CVAR (Int, screenblocks)

void M_Init (void)
{
	int i;

	if (gameinfo.gametype == GAME_Doom)
	{
		TopLevelMenu = currentMenu = &MainDef;
		PlayerClass = TypeInfo::FindType ("DoomPlayer");
	}
	else
	{
		TopLevelMenu = currentMenu = &HereticMainDef;
		PlayerClass = TypeInfo::FindType ("HereticPlayer");
		PSetupDef.y -= 7;
		LoadDef.y -= 20;
		SaveDef.y -= 20;
	}
	OptionsActive = false;
	menuactive = 0;
	InfoType = 0;
	itemOn = currentMenu->lastOn;
	whichSkull = 0;
	skullAnimCounter = 10;
	drawSkull = true;
	messageToPrint = 0;
	messageString = NULL;
	messageLastMenuActive = menuactive;
	quickSaveSlot = NULL;
	SkullBaseLump = W_CheckNumForName ("M_SKL00");
	strcpy (NewSaveNode.Title, "<New Save Game>");

	underscore[0] = (gameinfo.gametype == GAME_Doom) ? '_' : '[';
	underscore[1] = '\0';

	if (gameinfo.gametype == GAME_Doom)
	{
		LINEHEIGHT = 16;
	}
	else
	{
		LINEHEIGHT = 20;
	}

	if (gameinfo.flags & GI_SHAREWARE)
	{
		EpiDef.numitems = 1;
		HereticEpiDef.numitems = 1;
	}

	switch (gameinfo.flags & GI_MENUHACK)
	{
	case GI_MENUHACK_COMMERCIAL:
		MainMenu[MainDef.numitems-2] = MainMenu[MainDef.numitems-1];
		MainDef.numitems--;
		MainDef.y += 8;
		ReadDef.routine = M_DrawReadThis;
		ReadDef.x = 330;
		ReadDef.y = 165;
		ReadMenu[0].routine = M_FinishReadThis;
		break;
	case GI_MENUHACK_RETAIL:
		// add the fourth episode.
		EpiDef.numitems++;
		break;
	case GI_MENUHACK_EXTENDED:
		HereticEpiDef.numitems = 5;
		HereticEpiDef.y -= LINEHEIGHT;
		break;
	default:
		break;
	}
	M_OptInit ();

	// [RH] Build a palette translation table for the fire
	for (i = 0; i < 255; i++)
		FireRemap[i] = ColorMatcher.Pick (i, 0, 0);
}

