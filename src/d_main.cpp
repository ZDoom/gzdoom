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
//		DOOM main program (D_DoomMain) and game loop (D_DoomLoop),
//		plus functions to determine game mode (shareware, registered),
//		parse command line parameters, configure game parameters (turbo),
//		and call the startup functions.
//
//-----------------------------------------------------------------------------

// HEADER FILES ------------------------------------------------------------

#ifdef _WIN32
#include <direct.h>
#define mkdir(a,b) _mkdir (a)
#else
#include <sys/stat.h>
#endif

#ifdef unix
#include <unistd.h>
#endif

#include <time.h>
#include <math.h>

#include "errors.h"

#include "d_gui.h"
#include "m_alloc.h"
#include "m_random.h"
#include "doomdef.h"
#include "doomstat.h"
#include "gstrings.h"
#include "z_zone.h"
#include "w_wad.h"
#include "s_sound.h"
#include "v_video.h"
#include "f_finale.h"
#include "f_wipe.h"
#include "m_argv.h"
#include "m_misc.h"
#include "m_menu.h"
#include "c_console.h"
#include "c_dispatch.h"
#include "i_system.h"
#include "i_sound.h"
#include "i_video.h"
#include "g_game.h"
#include "hu_stuff.h"
#include "wi_stuff.h"
#include "st_stuff.h"
#include "am_map.h"
#include "p_setup.h"
#include "r_local.h"
#include "r_sky.h"
#include "d_main.h"
#include "d_dehacked.h"
#include "cmdlib.h"
#include "s_sound.h"
#include "m_swap.h"
#include "v_text.h"
#include "gi.h"
#include "b_bot.h"		//Added by MC:
#include "stats.h"
#include "a_doomglobal.h"
#include "gameconfigfile.h"
#include "sbar.h"
#include "decallib.h"

#include "v_text.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

struct GameAtExit
{
	GameAtExit *Next;
	char Command[1];
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

extern void M_RestoreMode ();
extern void R_ExecuteSetViewSize ();
extern void G_NewInit ();

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void D_CheckNetGame ();
void D_ProcessEvents ();
void G_BuildTiccmd (ticcmd_t* cmd);
void D_DoAdvanceDemo ();
void D_AddFile (const char *file);
void D_AddWildFile (const char *pattern);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

void D_DoomLoop ();
static const char *BaseFileSearch (const char *file, const char *ext);
static void STACK_ARGS DoConsoleAtExit ();

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

EXTERN_CVAR (Float, turbo)
EXTERN_CVAR (Int, crosshair)

extern gameinfo_t SharewareGameInfo;
extern gameinfo_t RegisteredGameInfo;
extern gameinfo_t RetailGameInfo;
extern gameinfo_t CommercialGameInfo;
extern gameinfo_t HereticGameInfo;
extern gameinfo_t HereticSWGameInfo;
extern gameinfo_t HexenGameInfo;
extern gameinfo_t HexenDKGameInfo;

extern int testingmode;
extern BOOL setmodeneeded;
extern BOOL netdemo;
extern int NewWidth, NewHeight, NewBits, DisplayBits;
EXTERN_CVAR (Bool, st_scale)
extern BOOL gameisdead;
extern BOOL demorecording;
extern bool M_DemoNoPlay;	// [RH] if true, then skip any demos in the loop

extern cycle_t WallCycles, PlaneCycles, MaskedCycles, WallScanCycles;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

CVAR (Int, fraglimit, 0, CVAR_SERVERINFO);
CVAR (Float, timelimit, 0.f, CVAR_SERVERINFO);
CVAR (Bool, queryiwad, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG);

bool DrawFSHUD;				// [RH] Draw fullscreen HUD?
wadlist_t *wadfiles;		// [RH] remove limit on # of loaded wads
BOOL devparm;				// started game with -devparm
char *D_DrawIcon;			// [RH] Patch name of icon to draw on next refresh
int NoWipe;					// [RH] Allow wipe? (Needs to be set each time)
BOOL singletics = false;	// debug flag to cancel adaptiveness
char startmap[8];
BOOL autostart;
BOOL advancedemo;
FILE *debugfile;
event_t events[MAXEVENTS];
int eventhead;
int eventtail;
gamestate_t wipegamestate = GS_DEMOSCREEN;	// can be -1 to force a wipe
DCanvas *page;

cycle_t FrameCycles;

const char *IWADTypeNames[NUM_IWAD_TYPES] =
{
	"DOOM 2: TNT - Evilution",
	"DOOM 2: Plutonia Experiment",
	"Hexen: Beyond Heretic",
	"Hexen: Deathkings of the Dark Citadel",
	"DOOM 2: Hell on Earth",
	"Heretic Shareware",
	"Heretic: Shadow of the Serpent Riders",
	"Heretic",
	"DOOM Shareware",
	"The Ultimate DOOM",
	"DOOM Registered"
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static wadlist_t **wadtail = &wadfiles;
static int demosequence;
static int pagetic;
static const char *IWADNames[] =
{
	NULL,
	"doom2f.wad",
	"doom2.wad",
	"plutonia.wad",
	"tnt.wad",
	"doomu.wad", // Hack from original Linux version. Not necessary, but I threw it in anyway.
	"doom.wad",
	"doom1.wad",
	"heretic.wad",
	"heretic1.wad",
	"hexen.wad",
	"hexdd.wad",
	NULL
};
static GameAtExit *ExitCmdList;

// CODE --------------------------------------------------------------------

//==========================================================================
//
// D_ProcessEvents
//
// Send all the events of the given timestamp down the responder chain.
// Events are asynchronous inputs generally generated by the game user.
// Events can be discarded if no responder claims them
//
//==========================================================================

void D_ProcessEvents (void)
{
	event_t *ev;
		
	// [RH] If testing mode, do not accept input until test is over
	if (testingmode)
	{
		if (testingmode <= I_GetTime())
		{
			M_RestoreMode ();
		}
		return;
	}
		
	for (; eventtail != eventhead ; eventtail = (++eventtail)&(MAXEVENTS-1))
	{
		ev = &events[eventtail];
		if (C_Responder (ev))
			continue;				// console ate the event
		if (M_Responder (ev))
			continue;				// menu ate the event
		G_Responder (ev);
	}
}

//==========================================================================
//
// D_PostEvent
//
// Called by the I/O functions when input is detected.
//
//==========================================================================

void D_PostEvent (const event_t *ev)
{
	events[eventhead] = *ev;
	eventhead = (++eventhead)&(MAXEVENTS-1);
}

//==========================================================================
//
// CVAR dmflags
//
//==========================================================================

CUSTOM_CVAR (Int, dmflags, 0, CVAR_SERVERINFO)
{
	// In case DF_NO_FREELOOK was changed, reinitialize the sky
	// map. (If no freelook, then no need to stretch the sky.)
	if (textureheight)
		R_InitSkyMap ();

	if (self & DF_NO_FREELOOK)
	{
		C_DoCommand ("centerview");
	}
	// If nofov is set, force everybody to the arbitrator's FOV.
	if ((self & DF_NO_FOV) && consoleplayer == Net_Arbitrator)
	{
		Net_WriteByte (DEM_FOV);
		Net_WriteByte ((BYTE)players[consoleplayer].DesiredFOV);
	}
}

CVAR (Flag, sv_nohealth,		dmflags, DF_NO_HEALTH);
CVAR (Flag, sv_noitems,			dmflags, DF_NO_ITEMS);
CVAR (Flag, sv_weaponstay,		dmflags, DF_WEAPONS_STAY);
CVAR (Flag, sv_falldamage,		dmflags, DF_FORCE_FALLINGHX);
CVAR (Flag, sv_oldfalldamage,	dmflags, DF_FORCE_FALLINGZD);
CVAR (Flag, sv_samelevel,		dmflags, DF_SAME_LEVEL);
CVAR (Flag, sv_spawnfarthest,	dmflags, DF_SPAWN_FARTHEST);
CVAR (Flag, sv_forcerespawn,	dmflags, DF_FORCE_RESPAWN);
CVAR (Flag, sv_noarmor,			dmflags, DF_NO_ARMOR);
CVAR (Flag, sv_noexit,			dmflags, DF_NO_EXIT);
CVAR (Flag, sv_infiniteammo,	dmflags, DF_INFINITE_AMMO);
CVAR (Flag, sv_nomonsters,		dmflags, DF_NO_MONSTERS);
CVAR (Flag, sv_monsterrespawn,	dmflags, DF_MONSTERS_RESPAWN);
CVAR (Flag, sv_itemrespawn,		dmflags, DF_ITEMS_RESPAWN);
CVAR (Flag, sv_fastmonsters,	dmflags, DF_FAST_MONSTERS);
CVAR (Flag, sv_nojump,			dmflags, DF_NO_JUMP);
CVAR (Flag, sv_nofreelook,		dmflags, DF_NO_FREELOOK);
CVAR (Flag, sv_respawnsuper,	dmflags, DF_RESPAWN_SUPER);
CVAR (Flag, sv_nofov,			dmflags, DF_NO_FOV);

//==========================================================================
//
// CVAR dmflags2
//
// [RH] From Skull Tag. Some of these were already done as separate cvars
// (such as bfgaiming), but I collected them here like Skull Tag does.
//
//==========================================================================

CVAR (Int, dmflags2, 0, CVAR_SERVERINFO);
CVAR (Flag, sv_weapondrop,		dmflags2, DF2_YES_WEAPONDROP);
CVAR (Flag, sv_nobfgaim,		dmflags2, DF2_NO_FREEAIMBFG);
CVAR (Flag, sv_respawnprotect,	dmflags2, DF2_YES_INVUL);
CVAR (Flag, sv_barrelrespawn,	dmflags2, DF2_BARRELS_RESPAWN);

//==========================================================================
//
// CVAR compatflags
//
//==========================================================================

CVAR (Int, compatflags, 0, CVAR_SERVERINFO);
CVAR (Flag, compat_shortTex,	compatflags, COMPATF_SHORTTEX);
CVAR (Flag, compat_stairs,		compatflags, COMPATF_STAIRINDEX);
CVAR (Flag, compat_limitpain,	compatflags, COMPATF_LIMITPAIN);
CVAR (Flag, compat_silentpickup,compatflags, COMPATF_SILENTPICKUP);
CVAR (Flag, compat_nopassover,	compatflags, COMPATF_NO_PASSMOBJ);
CVAR (Flag, compat_soundslots,	compatflags, COMPATF_MAGICSILENCE);
CVAR (Flag, compat_wallrun,		compatflags, COMPATF_WALLRUN);
CVAR (Flag, compat_notossdrops,	compatflags, COMPATF_NOTOSSDROPS);

//==========================================================================
//
// D_Display
//
// Draw current display, possibly wiping it from the previous
//
//==========================================================================

void D_Display (bool screenshot)
{
	bool wipe;

	if (nodrawers)
		return; 				// for comparative timing / profiling
	
	cycle_t cycles = 0;
	clock (cycles);

	if (players[consoleplayer].camera == NULL)
	{
		players[consoleplayer].camera = players[consoleplayer].mo;
	}

	if (gamestate == GS_LEVEL && viewactive)
	{
		R_SetFOV (players[consoleplayer].camera && players[consoleplayer].camera->player ?
			players[consoleplayer].camera->player->FOV : 90.f);
	}

	// [RH] change the screen mode if needed
	if (setmodeneeded)
	{
		// Change screen mode.
		if (V_SetResolution (NewWidth, NewHeight, NewBits))
		{
			// Recalculate various view parameters.
			setsizeneeded = true;
			// Let the status bar know the screen size changed
			StatusBar->ScreenSizeChanged ();
			// Refresh the console.
			C_NewModeAdjust ();
			// Reload crosshair if transitioned to a different size
			crosshair.Callback ();
		}
	}

	RenderTarget = screen;

	// change the view size if needed
	if (setsizeneeded)
	{
		R_ExecuteSetViewSize ();
		setmodeneeded = false;
	}

	if (screen->Lock (screenshot))
	{
		SB_state = screen->GetPageCount ();
		BorderNeedRefresh = screen->GetPageCount ();
	}

	// [RH] Allow temporarily disabling wipes
	if (NoWipe)
	{
		BorderNeedRefresh = screen->GetPageCount ();
		NoWipe--;
		wipe = false;
		wipegamestate = gamestate;
	}
	else if (gamestate != wipegamestate && gamestate != GS_FULLCONSOLE)
	{ // save the current screen if about to wipe
		BorderNeedRefresh = screen->GetPageCount ();
		wipe = true;
		wipe_StartScreen ();
		wipegamestate = gamestate;
	}
	else
	{
		wipe = false;
	}

	switch (gamestate)
	{
	case GS_FULLCONSOLE:
		C_DrawConsole ();
		M_Drawer ();
		if (!screenshot)
			screen->Update ();
		return;

	case GS_LEVEL:
		if (!gametic)
			break;

		if (viewactive)
		{
			R_RefreshViewBorder ();
			R_RenderPlayerView (&players[consoleplayer], NetUpdate);
			R_DetailDouble ();		// [RH] Apply detail mode expansion
		}
		if (automapactive)
		{
			AM_Drawer ();
		}
		if (realviewheight == SCREENHEIGHT && viewactive)
		{
			StatusBar->Draw (DrawFSHUD ? HUD_Fullscreen : HUD_None);
		}
		else
		{
			StatusBar->Draw (HUD_StatusBar);
		}
		CT_Drawer ();
		break;

	case GS_INTERMISSION:
		WI_Drawer ();
		break;

	case GS_FINALE:
		F_Drawer ();
		break;

	case GS_DEMOSCREEN:
		D_PageDrawer ();
		break;

	default:
	    break;
	}

	// draw pause pic
	if (paused && !menuactive)
	{
		int lump;

		if (gameinfo.gametype == GAME_Doom)
		{
			lump = W_CheckNumForName ("M_PAUSE");
		}
		else
		{
			lump = W_CheckNumForName ("PAUSED");
		}
		if (lump >= 0)
		{
			patch_t *pause = TileCache[R_CacheTileNum (lump, PU_CACHE)];
			int x = (SCREENWIDTH - SHORT(pause->width)*CleanXfac)/2 +
				SHORT(pause->leftoffset)*CleanXfac;
			screen->DrawPatchCleanNoMove (pause, x, 4);
		}
	}

	// [RH] Draw icon, if any
	if (D_DrawIcon)
	{
		int lump = W_CheckNumForName (D_DrawIcon);

		D_DrawIcon = NULL;
		if (lump >= 0)
		{
			patch_t *p = (patch_t *)W_CacheLumpNum (lump, PU_CACHE);

			screen->DrawPatchIndirect (p, 160-SHORT(p->width)/2, 100-SHORT(p->height)/2);
		}
		NoWipe = 10;
	}

	NetUpdate ();			// send out any new accumulation

	if (!wipe || screenshot)
	{
		// normal update
		C_DrawConsole ();	// draw console
		M_Drawer ();		// menu is drawn even on top of everything
		FStat::PrintStat ();
		if (!screenshot)
			screen->Update ();	// page flip or blit buffer
	}
	else
	{
		// wipe update
		int wipestart, nowtime, tics;
		BOOL done;

		wipe_EndScreen ();
		screen->Unlock ();

		wipestart = I_GetTime () - 1;

		do
		{
			do
			{
				nowtime = I_GetTime ();
				tics = nowtime - wipestart;
			} while (!tics);
			wipestart = nowtime;
			screen->Lock (true);
			done = wipe_ScreenWipe (tics);
			C_DrawConsole ();
			M_Drawer ();			// menu is drawn even on top of wipes
			screen->Update ();		// page flip or blit buffer
		} while (!done);
	}

	unclock (cycles);
	FrameCycles = cycles;
}

//==========================================================================
//
// DoConsoleAtExit
//
// Executes the contents of the atexit cvar, if any, at quit time.
//
//==========================================================================

static void STACK_ARGS DoConsoleAtExit ()
{
	GameAtExit *cmd = ExitCmdList;

	while (cmd != NULL)
	{
		GameAtExit *next = cmd->Next;
		AddCommandString (cmd->Command);
		free (cmd);
		cmd = next;
	}
}

//==========================================================================
//
// CCMD atexit
//
//==========================================================================

CCMD (atexit)
{
	if (argv.argc() == 1)
	{
		Printf ("Registered atexit commands:\n");
		GameAtExit *record = ExitCmdList;
		while (record != NULL)
		{
			Printf ("%s\n", record->Command);
			record = record->Next;
		}
		return;
	}
	for (int i = 1; i < argv.argc(); ++i)
	{
		GameAtExit *record = (GameAtExit *)Malloc (
			sizeof(GameAtExit)+strlen(argv[i]));
		strcpy (record->Command, argv[i]);
		record->Next = ExitCmdList;
		ExitCmdList = record;
	}
}

//==========================================================================
//
// D_ErrorCleanup ()
//
// Cleanup after a recoverable error.
//==========================================================================

void D_ErrorCleanup ()
{
	screen->Unlock ();
	bglobal.RemoveAllBots (true);
	D_QuitNetGame ();
	Net_ClearBuffers ();
	G_NewInit ();
	singletics = false;
	if (demorecording || demoplayback)
		G_CheckDemoStatus ();
	playeringame[0] = 1;
	players[0].playerstate = PST_LIVE;
	gameaction = ga_fullconsole;
	menuactive = false;
}

//==========================================================================
//
// D_DoomLoop
//
// Manages timing and IO, calls all ?_Responder, ?_Ticker, and ?_Drawer,
// calls I_GetTime, I_StartFrame, and I_StartTic
//
//==========================================================================

void D_DoomLoop ()
{
	for (;;)
	{
		try
		{
			// frame syncronous IO operations
			I_StartFrame ();
			
			// process one or more tics
			if (singletics)
			{
				I_StartTic ();
				DObject::BeginFrame ();
				D_ProcessEvents ();
				G_BuildTiccmd (&netcmds[consoleplayer][maketic%BACKUPTICS]);
				//Added by MC: For some of that bot stuff. The main bot function.
				int i;
				for (i = 0; i < MAXPLAYERS; i++)
					if (playeringame[i] && players[i].isbot && players[i].mo)
					{
						players[i].savedyaw = players[i].mo->angle;
						players[i].savedpitch = players[i].mo->pitch;
					}
				bglobal.Main (maketic%BACKUPTICS);
				for (i = 0; i < MAXPLAYERS; i++)
					if (playeringame[i] && players[i].isbot && players[i].mo)
					{
						players[i].mo->angle = players[i].savedyaw;
						players[i].mo->pitch = players[i].savedpitch;
					}
				if (advancedemo)
					D_DoAdvanceDemo ();
				C_Ticker ();
				M_Ticker ();
				G_Ticker ();
				gametic++;
				maketic++;
				DObject::EndFrame ();
				Net_NewMakeTic ();
			}
			else
			{
				TryRunTics (); // will run at least one tic
			}
			// [RH] Use the consoleplayer's camera to update sounds
			S_UpdateSounds (players[consoleplayer].camera);	// move positional sounds
			// Update display, next frame, with current state.
			D_Display (false);
		}
		catch (CRecoverableError &error)
		{
			if (error.GetMessage ())
			{
				Printf (PRINT_BOLD, "\n%s\n", error.GetMessage());
			}
			D_ErrorCleanup ();
		}
	}
}

//==========================================================================
//
// D_PageTicker
//
//==========================================================================

void D_PageTicker (void)
{
	if (--pagetic < 0)
		D_AdvanceDemo ();
}

//==========================================================================
//
// D_PageDrawer
//
//==========================================================================

void D_PageDrawer (void)
{
	if (page)
	{
		page->Blit (0, 0, page->GetWidth(), page->GetHeight(),
			screen, 0, 0, SCREENWIDTH, SCREENHEIGHT);
	}
	else
	{
		screen->Clear (0, 0, SCREENWIDTH, SCREENHEIGHT, 0);
		screen->DrawText (CR_WHITE, 0, 0, "Page graphic goes here");
	}
}

//==========================================================================
//
// D_AdvanceDemo
//
// Called after each demo or intro demosequence finishes
//
//==========================================================================

void D_AdvanceDemo (void)
{
	advancedemo = true;
}

//==========================================================================
//
// D_DoAdvanceDemo
//
//==========================================================================

void D_DoAdvanceDemo (void)
{
	static char demoname[8] = "DEMO1";
	static int democount = 0;
	static int pagecount;
	char *pagename = NULL;

	players[consoleplayer].playerstate = PST_LIVE;	// not reborn
	advancedemo = false;
	usergame = false;				// no save / end game here
	paused = 0;
	gameaction = ga_nothing;

	switch (demosequence)
	{
	case 3:
		if (gameinfo.advisoryTime)
		{
			if (page)
			{
				page->Lock ();
				page->DrawPatch ((patch_t *)W_CacheLumpName ("ADVISOR", PU_CACHE), 4, 160);
			}
			page->Unlock ();
			demosequence = 1;
			pagetic = (int)(gameinfo.advisoryTime * TICRATE);
			break;
		}
		// fall through to case 1 if no advisory notice

	case 1:
		if (!M_DemoNoPlay)
		{
			BorderNeedRefresh = screen->GetPageCount ();
			democount++;
			sprintf (demoname + 4, "%d", democount);
			if (W_CheckNumForName (demoname) < 0)
			{
				demosequence = 0;
				democount = 0;
				// falls through to case 0 below
			}
			else
			{
				G_DeferedPlayDemo (demoname);
				demosequence = 2;
				break;
			}
		}

	default:
	case 0:
		gamestate = GS_DEMOSCREEN;
		pagename = gameinfo.titlePage;
		pagetic = (int)(gameinfo.titleTime * TICRATE);
		S_StartMusic (gameinfo.titleMusic);
		demosequence = 3;
		pagecount = 0;
		C_HideConsole ();
		break;

	case 2:
		pagetic = (int)(gameinfo.pageTime * TICRATE);
		gamestate = GS_DEMOSCREEN;
		if (pagecount == 0)
			pagename = gameinfo.creditPage1;
		else
			pagename = gameinfo.creditPage2;
		pagecount ^= 1;
		demosequence = 1;
		break;
	}

	if (pagename)
	{
		int width, height;
		patch_t *data;

		if (gameinfo.flags & GI_PAGESARERAW)
		{
			data = (patch_t *)W_CacheLumpName (pagename, PU_CACHE);
			width = 320;
			height = 200;
		}
		else
		{
			data = (patch_t *)W_CacheLumpName (pagename, PU_CACHE);
			width = SHORT(data->width);
			height = SHORT(data->height);
		}

		if (page && (page->GetWidth() != width || page->GetHeight() != height))
		{
			delete page;
			page = NULL;
		}

		if (page == NULL)
			page = I_NewStaticCanvas (width, height);

		page->Lock ();
		if (gameinfo.flags & GI_PAGESARERAW)
			page->DrawBlock (0, 0, 320, 200, (byte *)data);
		else
			page->DrawPatch (data, 0, 0);
		page->Unlock ();
	}
}

//==========================================================================
//
// D_StartTitle
//
//==========================================================================

void D_StartTitle (void)
{
	gameaction = ga_nothing;
	demosequence = -1;
	D_AdvanceDemo ();
}

//==========================================================================
//
// Cmd_Endgame
//
// [RH] Quit the current game and go to fullscreen console
//
//==========================================================================

CCMD (endgame)
{
	if (!netgame)
	{
		gameaction = ga_fullconsole;
		demosequence = -1;
	}
}

//==========================================================================
//
// D_AddFile
//
//==========================================================================

void D_AddFile (const char *file)
{
	if (file == NULL)
	{
		return;
	}

	if (!FileExists (file))
	{
		const char *f = BaseFileSearch (file, ".wad");
		if (f == NULL)
		{
			Printf ("Can't find '%s'\n", file);
			return;
		}
		file = f;
	}
	wadlist_t *wad = (wadlist_t *)Z_Malloc (sizeof(*wad) + strlen(file), PU_STATIC, 0);

	*wadtail = wad;
	wad->next = NULL;
	strcpy (wad->name, file);
	wadtail = &wad->next;
}

//==========================================================================
//
// D_AddWildFile
//
//==========================================================================

void D_AddWildFile (const char *value)
{
	const char *wadfile = BaseFileSearch (value, ".wad");

	if (wadfile != NULL)
	{
		D_AddFile (wadfile);
	}
	else
	{ // Try pattern matching
		findstate_t findstate;
		char path[PATH_MAX];
		char *sep;
		void *handle = I_FindFirst (value, &findstate);

		strcpy (path, value);
		sep = strrchr (path, '/');
		if (sep == NULL)
		{
			sep = strrchr (path, '\\');
#ifdef _WIN32
			if (sep == NULL && path[1] == ':')
			{
				sep = path + 1;
			}
#endif
		}

		if (handle != ((void *)-1))
		{
			do
			{
				if (sep == NULL)
				{
					D_AddFile (I_FindName (&findstate));
				}
				else
				{
					strcpy (sep+1, I_FindName (&findstate));
					D_AddFile (path);
				}
			} while (I_FindNext (handle, &findstate) == 0);
		}
		I_FindClose (handle);
	}
}

//==========================================================================
//
// D_AddConfigWads
//
// Adds all files in the specified config file section.
//
//==========================================================================

void D_AddConfigWads (const char *section)
{
	if (GameConfig->SetSection (section))
	{
		const char *key;
		const char *value;
		FConfigFile::Position pos;

		while (GameConfig->NextInSection (key, value))
		{
			if (stricmp (key, "Path") == 0)
			{
				// D_AddWildFile resets GameConfig's position, so remember it
				GameConfig->GetPosition (pos);
				D_AddWildFile (value);
				// Reset GameConfig's position to get next wad
				GameConfig->SetPosition (pos);
			}
		}
	}
}

//==========================================================================
//
// D_AddDirectory
//
// Add all .wad files in a directory. Does not descend into subdirectories.
//
//==========================================================================

static void D_AddDirectory (const char *dir)
{
	char curdir[PATH_MAX];

	if (getcwd (curdir, PATH_MAX))
	{
		char skindir[PATH_MAX];
		findstate_t findstate;
		void *handle;
		size_t stuffstart;

		stuffstart = strlen (dir);
		memcpy (skindir, dir, stuffstart*sizeof(*dir));
		skindir[stuffstart] = 0;

		if (skindir[stuffstart-1] == '/')
		{
			skindir[--stuffstart] = 0;
		}

		if (!chdir (skindir))
		{
			skindir[stuffstart++] = '/';
			if ((handle = I_FindFirst ("*.wad", &findstate)) != (void *)-1)
			{
				do
				{
					if (!(I_FindAttr (&findstate) & FA_DIREC))
					{
						strcpy (skindir + stuffstart, I_FindName (&findstate));
						D_AddFile (skindir);
					}
				} while (I_FindNext (handle, &findstate) == 0);
				I_FindClose (handle);
			}
		}
		chdir (curdir);
	}
}

//==========================================================================
//
// SetIWAD
//
// Sets parameters for the game using the specified IWAD.
//==========================================================================

static void SetIWAD (const char *iwadpath, EIWADType type)
{
	static const struct
	{
		GameMode_t Mode;
		const gameinfo_t *Info;
		GameMission_t Mission;
	} Datas[NUM_IWAD_TYPES] = {
		{ commercial,	&CommercialGameInfo,	pack_tnt },		// Doom2TNT
		{ commercial,	&CommercialGameInfo,	pack_plut },	// Doom2Plutonia
		{ commercial,	&HexenGameInfo,			doom2 },		// Hexen
		{ commercial,	&HexenDKGameInfo,		doom2 },		// HexenDK
		{ commercial,	&CommercialGameInfo,	doom2 },		// Doom2
		{ shareware,	&HereticSWGameInfo,		doom },			// HereticShareware
		{ retail,		&HereticGameInfo,		doom },			// HereticExtended
		{ retail,		&HereticGameInfo,		doom },			// Heretic
		{ shareware,	&SharewareGameInfo,		doom },			// DoomShareware
		{ retail,		&RetailGameInfo,		doom },			// UltimateDoom
		{ registered,	&RegisteredGameInfo,	doom }			// DoomRegistered
	};

	D_AddFile (iwadpath);

	if ((unsigned)type < NUM_IWAD_TYPES)
	{
		gamemode = Datas[type].Mode;
		gameinfo = *Datas[type].Info;
		gamemission = Datas[type].Mission;
		if (type == IWAD_HereticExtended)
		{
			gameinfo.flags |= GI_MENUHACK_EXTENDED;
		}
	}
	else
	{
		gamemode = undetermined;
	}
}

//==========================================================================
//
// ScanIWAD
//
// Scan the contents of an IWAD to determine which one it is
//==========================================================================

static EIWADType ScanIWAD (const char *iwad)
{
	static const char checklumps[][8] =
	{
		"E1M1",
		"E4M1",
		"MAP01",
		"MAP60",
		"TITLE",
		"REDTNT2",
		"CAMO1",
		{ 'E','X','T','E','N','D','E','D'},
		"E2M1","E2M2","E2M3","E2M4","E2M5","E2M6","E2M7","E2M8","E2M9",
		"E3M1","E3M2","E3M3","E3M4","E3M5","E3M6","E3M7","E3M8","E3M9",
		"DPHOOF","BFGGA0","HEADA1","CYBRA1",
		{ 'S','P','I','D','A','1','D','1' }
	};
#define NUM_CHECKLUMPS (sizeof(checklumps)/8)
	enum
	{
		Check_e1m1,
		Check_e4m1,
		Check_map01,
		Check_map60,
		Check_title,
		Check_redtnt2,
		Check_cam01,
		Check_Extended,
		Check_e2m1
	};
	int lumpsfound[NUM_CHECKLUMPS];
	size_t i;
	wadinfo_t header;
	FILE *f;

	memset (lumpsfound, 0, sizeof(lumpsfound));
	if ( (f = fopen (iwad, "rb")) )
	{
		fread (&header, sizeof(header), 1, f);
		if (header.Magic == IWAD_ID || header.Magic == PWAD_ID)
		{
			header.NumLumps = LONG(header.NumLumps);
			if (0 == fseek (f, LONG(header.InfoTableOfs), SEEK_SET))
			{
				for (i = 0; i < (size_t)header.NumLumps; i++)
				{
					wadlump_t lump;
					size_t j;

					if (0 == fread (&lump, sizeof(lump), 1, f))
						break;
					for (j = 0; j < NUM_CHECKLUMPS; j++)
						if (strnicmp (lump.Name, checklumps[j], 8) == 0)
							lumpsfound[j]++;
				}
			}
		}
		fclose (f);
	}

	if (lumpsfound[Check_title] && lumpsfound[Check_title] && lumpsfound[Check_map60])
	{
		return IWAD_HexenDK;
	}
	else if (lumpsfound[Check_map01])
	{
		if (lumpsfound[Check_redtnt2])
		{
			return IWAD_Doom2TNT;
		}
		else if (lumpsfound[Check_cam01])
		{
			return IWAD_Doom2Plutonia;
		}
		else
		{
			if (lumpsfound[Check_title])
			{
				return IWAD_Hexen;
			}
			else
			{
				return IWAD_Doom2;
			}
		}
	}
	else if (lumpsfound[Check_e1m1])
	{
		if (lumpsfound[Check_title])
		{
			if (!lumpsfound[Check_e2m1])
			{
				return IWAD_HereticShareware;
			}
			else
			{
				if (lumpsfound[Check_Extended])
				{
					return IWAD_HereticExtended;
				}
				else
				{
					return IWAD_Heretic;
				}
			}
		}
		else
		{
			for (i = Check_e2m1; i < NUM_CHECKLUMPS; i++)
			{
				if (!lumpsfound[i])
				{
					return IWAD_DoomShareware;
				}
			}
			if (i == NUM_CHECKLUMPS)
			{
				if (lumpsfound[Check_e4m1])
				{
					return IWAD_UltimateDoom;
				}
				else
				{
					return IWAD_DoomRegistered;
				}
			}
		}
	}
	return NUM_IWAD_TYPES;	// Don't know
}

//==========================================================================
//
// CheckIWAD
//
// Tries to find an IWAD from a set of known IWAD names, and checks the
// contents of each one found to determine which game it belongs to.
// Returns the number of new wads found in this pass (does not count wads
// found from a previous call).
// 
//==========================================================================

static int CheckIWAD (const char *doomwaddir, WadStuff *wads)
{
	const char *slash;
	char iwad[512];
	int i;
	int numfound;

	numfound = 0;

	slash = (doomwaddir[0] && doomwaddir[strlen (doomwaddir)-1] != '/') ? "/" : "";

	// Search for a pre-defined IWAD
	for (i = IWADNames[0] ? 0 : 1; IWADNames[i]; i++)
	{
		if (wads[i].Path == NULL)
		{
			sprintf (iwad, "%s%s%s", doomwaddir, slash, IWADNames[i]);
			FixPathSeperator (iwad);
			if (FileExists (iwad))
			{
				wads[i].Type = ScanIWAD (iwad);
				if (wads[i].Type != NUM_IWAD_TYPES)
				{
					wads[i].Path = copystring (iwad);
					numfound++;
				}
			}
		}
	}

	return numfound;
}

//==========================================================================
//
// CheckIWADinEnvDir
//
// Checks for an IWAD in a directory specified in an environment variable.
//
//==========================================================================

static int CheckIWADinEnvDir (const char *envname, WadStuff *wads)
{
	char dir[512];
	const char *envdir = getenv (envname);

	if (envdir)
	{
		strcpy (dir, envdir);
		FixPathSeperator (dir);
		if (dir[strlen(dir) - 1] != '/')
			strcat (dir, "/");
		return CheckIWAD (dir, wads);
	}
	return false;
}

//==========================================================================
//
// IdentifyVersion
//
// Tries to find an IWAD in one of four directories under DOS or Win32:
//	  1. Current directory
//	  2. Executable directory
//	  3. $DOOMWADDIR
//	  4. $HOME
//
// Under UNIX OSes, the search path is:
//	  1. Current directory
//	  2. $DOOMWADDIR
//	  3. $HOME/.zdoom
//	  4. The share directory defined at compile time (/usr/local/share/zdoom)
//
// The search path can be altered by editing the IWADSearch.Directories
// section of the config file.
//
//==========================================================================

static EIWADType IdentifyVersion (void)
{
	WadStuff wads[sizeof(IWADNames)/sizeof(char *)];
	size_t foundwads[NUM_IWAD_TYPES] = { 0 };
	const char *iwadparm = Args.CheckValue ("-iwad");
	char *homepath = NULL;
	size_t numwads;
	int pickwad;
	size_t i;
	bool iwadparmfound = false;

	memset (wads, 0, sizeof(wads));

	if (iwadparm)
	{
		char *custwad = new char[strlen (iwadparm) + 5];
		strcpy (custwad, iwadparm);
		FixPathSeperator (custwad);
		if (CheckIWAD (custwad, wads))
		{ // -iwad parameter was a directory
			delete[] custwad;
			iwadparm = NULL;
		}
		else
		{
			DefaultExtension (custwad, ".wad");
			iwadparm = custwad;
			IWADNames[0] = iwadparm;
			CheckIWAD ("", wads);
		}
	}

	if (iwadparm == NULL || wads[0].Path == NULL)
	{
		if (GameConfig->SetSection ("IWADSearch.Directories"))
		{
			const char *key;
			const char *value;

			while (GameConfig->NextInSection (key, value))
			{
				if (stricmp (key, "Path") == 0)
				{
					if (*value == '$')
					{
						if (stricmp (value + 1, "progdir") == 0)
						{
							CheckIWAD (progdir, wads);
						}
						else
						{
							CheckIWADinEnvDir (value + 1, wads);
						}
					}
#ifdef unix
					else if (*value == '~' && (*(value + 1) == 0 || *(value + 1) == '/'))
					{
						homepath = GetUserFile (*(value + 1) ? value + 2 : value + 1);
						CheckIWAD (homepath, wads);
						delete[] homepath;
						homepath = NULL;
					}
#endif
					else
					{
						CheckIWAD (value, wads);
					}
				}
			}
		}
	}

	if (iwadparm != NULL && wads[0].Path != NULL)
	{
		iwadparmfound = true;
	}

	for (i = numwads = 0; i < sizeof(IWADNames)/sizeof(char *); i++)
	{
		if (wads[i].Path != NULL)
		{
			if (i != numwads)
			{
				wads[numwads] = wads[i];
			}
			foundwads[wads[numwads].Type] = numwads+1;
			numwads++;
		}
	}

	if (foundwads[IWAD_HexenDK] && !foundwads[IWAD_Hexen])
	{ // Cannot play Hexen DK without Hexen
		size_t kill = foundwads[IWAD_HexenDK];
		if (kill != numwads)
		{
			memmove (&wads[kill-1], &wads[kill], numwads - kill);
		}
		numwads--;
	}

	if (numwads == 0)
	{
		I_FatalError ("Cannot find a game IWAD (doom.wad, doom2.wad, heretic.wad, etc.).\n"
					  "Did you install ZDoom properly? You can do either of the following:\n"
					  "\n"
					  "1. Place one or more of these wads in the same directory as ZDoom."
					  "2. Edit your zdoom.ini and add the directories of your iwads\n"
					  "to the list beneath [IWADSearch.Directories]");
	}

	pickwad = 0;

	if (!iwadparmfound && numwads > 1 && queryiwad)
	{
		pickwad = I_PickIWad (wads, (int)numwads);
	}

	if (pickwad < 0)
		exit (0);

	if (wads[pickwad].Type == IWAD_HexenDK)
	{ // load hexen.wad before loading hexdd.wad
		D_AddFile (wads[foundwads[IWAD_Hexen]-1].Path);
	}

	SetIWAD (wads[pickwad].Path, wads[pickwad].Type);

	for (i = 0; i < numwads; i++)
	{
		delete[] wads[i].Path;
	}

	if (homepath)
		delete[] homepath;

	return wads[pickwad].Type;
}

//==========================================================================
//
// BaseFileSearch
//
// If a file does not exist at <file>, looks for it in the directories
// specified in the config file. Returns the path to the file, if found,
// or NULL if it could not be found.
//
//==========================================================================

static const char *BaseFileSearch (const char *file, const char *ext)
{
	static char wad[PATH_MAX];

	if (FileExists (file))
	{
		return file;
	}

	if (GameConfig->SetSection ("FileSearch.Directories"))
	{
		const char *key;
		const char *value;

		while (GameConfig->NextInSection (key, value))
		{
			if (stricmp (key, "Path") == 0)
			{
				const char *dir;
				char *homepath = NULL;

				if (*value == '$')
				{
					if (stricmp (value + 1, "progdir") == 0)
					{
						dir = progdir;
					}
					else
					{
						dir = getenv (value + 1);
					}
				}
#ifdef unix
				else if (*value == '~' && (*(value + 1) == 0 || *(value + 1) == '/'))
				{
					homepath = GetUserFile (*(value + 1) ? value + 2 : value + 1);
					dir = homepath;
				}
#endif
				else
				{
					dir = value;
				}
				if (dir != NULL)
				{
					sprintf (wad, "%s%s%s", dir, dir[strlen (dir) - 1] != '/' ? "/" : "", file);
					if (homepath != NULL)
					{
						delete[] homepath;
						homepath = NULL;
					}
					if (FileExists (wad))
					{
						return wad;
					}
				}
			}
		}
	}

	// Retry, this time with a default extension
	if (ext != NULL)
	{
		static char tmp[PATH_MAX];
		strcpy (tmp, file);
		DefaultExtension (tmp, ext);
		return BaseFileSearch (tmp, NULL);
	}
	return NULL;
}

//==========================================================================
//
// ConsiderPatches
//
// Tries to add any deh/bex patches from the command line.
//
//==========================================================================

bool ConsiderPatches (const char *arg, const char *ext)
{
	bool noDef = false;
	DArgs *files = Args.GatherFiles (arg, ext, false);

	if (files->NumArgs() > 0)
	{
		int i;
		const char *f;

		for (i = 0; i < files->NumArgs(); ++i)
		{
			if ( (f = BaseFileSearch (files->GetArg (i), ".deh")) )
				DoDehPatch (f, false);
			else if ( (f = BaseFileSearch (files->GetArg (i), ".bex")) )
				DoDehPatch (f, false);
		}
		noDef = true;
	}
	delete files;
	return noDef;
}

//==========================================================================
//
// D_MultiExec
//
//==========================================================================

void D_MultiExec (DArgs *list, bool usePullin)
{
	for (int i = 0; i < list->NumArgs(); ++i)
	{
		C_ExecFile (list->GetArg (i), usePullin);
	}
}

//==========================================================================
//
// D_DoomMain
//
//==========================================================================

void D_DoomMain (void)
{
	int p, flags;
	char file[PATH_MAX];
	char *v;
	const char *wad;
	DArgs *execFiles;

	file[PATH_MAX-1] = 0;

#if defined(_MSC_VER) || defined(__GNUC__)
	TypeInfo::StaticInit ();
#endif

	FActorInfo::StaticWeaponInit ();

	atterm (DObject::StaticShutdown);
	atterm (DoConsoleAtExit);

	gamestate = GS_STARTUP;

	SetLanguageIDs ();

	rngseed = (DWORD)time (NULL);
	M_FindResponseFile ();
	M_LoadDefaults ();			// load before initing other systems
	Z_Init ();	// [RH] Init zone heap after loading config file

	// [RH] Make sure zdoom.wad is always loaded,
	// as it contains magic stuff we need.
	wad = BaseFileSearch ("zdoom.wad", NULL);
	if (wad)
		D_AddFile (wad);
	else
		I_FatalError ("Cannot find zdoom.wad");

	I_SetTitleString (IWADTypeNames[IdentifyVersion ()]);
	GameConfig->DoGameSetup (GameNames[gameinfo.gametype]);

	// Run automatically executed files
	execFiles = new DArgs;
	GameConfig->AddAutoexec (execFiles, GameNames[gameinfo.gametype]);
	D_MultiExec (execFiles, true);
	delete execFiles;

	// Run .cfg files at the start of the command line.
	execFiles = Args.GatherFiles (NULL, ".cfg", false);
	D_MultiExec (execFiles, true);
	delete execFiles;

	C_ExecCmdLineParams ();		// [RH] do all +set commands on the command line

	// [RH] zvox.wad - A wad I had intended to be automatically generate
	// from Q2's pak0.pak so the female and cyborg player could have
	// voices. I never got around to writing the utility to do it, though.
	wad = BaseFileSearch ("zvox.wad", NULL);
	if (wad)
		D_AddFile (wad);

	// [RH] Add any .wad files in the skins directory
#ifdef unix
	sprintf (file, "%sskins", SHARE_DIR);
#else
	sprintf (file, "%sskins", progdir);
#endif
	D_AddDirectory (file);

	const char *home = getenv ("HOME");
	if (home)
	{
		sprintf (file, "%s%s.zdoom/skins", home,
			home[strlen(home)-1] == '/' ? "" : "/");
		D_AddDirectory (file);
	}

	if (!(gameinfo.flags & GI_SHAREWARE))
	{
		// Add common (global) wads
		D_AddConfigWads ("Global.Autoload");

		// Add game-specific wads
		sprintf (file, "%s.Autoload", GameNames[gameinfo.gametype]);
		D_AddConfigWads (file);
	}

	DArgs *files = Args.GatherFiles ("-file", ".wad", true);
	if (files->NumArgs() > 0)
	{
		// Check for -file in shareware
		if (gameinfo.flags & GI_SHAREWARE)
		{
			I_FatalError ("You cannot -file with the shareware version. Register!");
		}

		// the files gathered are wadfile/lump names
		for (int i = 0; i < files->NumArgs(); i++)
		{
			D_AddWildFile (files->GetArg (i));
		}
	}
	delete files;

	W_InitMultipleFiles (&wadfiles);

	// [RH] Initialize localizable strings.
	GStrings.LoadStrings (W_GetNumForName ("LANGUAGE"), STRING_TABLE_SIZE, false);
	GStrings.Compact ();

	//P_InitXlat ();

	// [RH] Moved these up here so that we can do most of our
	//		startup output in a fullscreen console.

	CT_Init ();
	I_Init ();
	V_Init ();

	// Base systems have been inited; enable cvar callbacks
	FBaseCVar::EnableCallbacks ();

	// [RH] Now that all text strings are set up,
	// insert them into the level and cluster data.
	G_SetLevelStrings ();
	
	// [RH] Parse through all loaded mapinfo lumps
	G_ParseMapInfo ();

	// [RH] Parse any SNDINFO lumps
	S_ParseSndInfo ();

	FActorInfo::StaticInit ();
	FActorInfo::StaticGameSet ();

	DecalLibrary.Clear ();
	DecalLibrary.ReadAllDecals ();

	// [RH] Try adding .deh and .bex files on the command line.
	// If there are none, try adding any in the config file.

	if (gameinfo.gametype == GAME_Doom)
	{
		if (!ConsiderPatches ("-deh", ".deh") &&
			!ConsiderPatches ("-bex", ".bex") &&
			GameConfig->SetSection ("Doom.DefaultDehacked"))
		{
			const char *key;
			const char *value;

			while (GameConfig->NextInSection (key, value))
			{
				if (stricmp (key, "Path") == 0 && FileExists (value))
				{
					Printf ("Applying patch %s\n", value);
					DoDehPatch (value, true);
				}
			}
		}

		DoDehPatch (NULL, true);	// See if there's a patch in a PWAD
		FinishDehPatch ();			// Create replacements for dehacked pickups
	}

	FActorInfo::StaticSetActorNums ();

	// [RH] User-configurable startup strings. Because BOOM does.
	if (GStrings(STARTUP1)[0])	Printf ("%s\n", GStrings(STARTUP1));
	if (GStrings(STARTUP2)[0])	Printf ("%s\n", GStrings(STARTUP2));
	if (GStrings(STARTUP3)[0])	Printf ("%s\n", GStrings(STARTUP3));
	if (GStrings(STARTUP4)[0])	Printf ("%s\n", GStrings(STARTUP4));
	if (GStrings(STARTUP5)[0])	Printf ("%s\n", GStrings(STARTUP5));

	//Added by MC:
	bglobal.getspawned = Args.GatherFiles ("-bots", "", false);
	if (bglobal.getspawned->NumArgs() == 0)
	{
		delete bglobal.getspawned;
		bglobal.getspawned = NULL;
	}
	else
	{
		bglobal.spawn_tries = 0;
		bglobal.wanted_botnum = bglobal.getspawned->NumArgs();
	}

	flags = dmflags;
		
	if (Args.CheckParm ("-nomonsters"))		flags |= DF_NO_MONSTERS;
	if (Args.CheckParm ("-respawn"))		flags |= DF_MONSTERS_RESPAWN;
	if (Args.CheckParm ("-fast"))			flags |= DF_FAST_MONSTERS;

	devparm = Args.CheckParm ("-devparm");

	if (Args.CheckParm ("-altdeath"))
	{
		deathmatch = 1;
		flags |= DF_ITEMS_RESPAWN;
	}
	else if (Args.CheckParm ("-deathmatch"))
	{
		deathmatch = 1;
		flags |= DF_WEAPONS_STAY | DF_ITEMS_RESPAWN;
	}

	dmflags = flags;

	// get skill / episode / map from parms
	strcpy (startmap, (gameinfo.flags & GI_MAPxx) ? "MAP01" : "E1M1");
	autostart = false;
				
	const char *val = Args.CheckValue ("-skill");
	if (val)
	{
		gameskill = val[0] - '1';
		autostart = true;
	}

	p = Args.CheckParm ("-warp");
	if (p && p < Args.NumArgs() - (1+(gameinfo.flags & GI_MAPxx ? 0 : 1)))
	{
		int ep, map;

		if (gameinfo.flags & GI_MAPxx)
		{
			ep = 1;
			map = atoi (Args.GetArg(p+1));
		}
		else 
		{
			ep = Args.GetArg(p+1)[0]-'0';
			map = Args.GetArg(p+2)[0]-'0';
			if ((unsigned)map > 9)
			{
				map = ep;
				ep = 1;
			}
		}

		strncpy (startmap, CalcMapName (ep, map), 8);
		autostart = true;
	}

	// [RH] Hack to handle +map
	p = Args.CheckParm ("+map");
	if (p && p < Args.NumArgs()-1)
	{
		if (W_CheckNumForName (Args.GetArg (p+1)) == -1)
		{
			Printf ("Can't find map %s\n", Args.GetArg (p+1));
		}
		else
		{
			strncpy (startmap, Args.GetArg (p+1), 8);
			Args.GetArg (p)[0] = '-';
			autostart = true;
		}
	}
	if (devparm)
	{
		Printf (GStrings(D_DEVSTR));
	}

#ifndef unix
	// We do not need to support -cdrom under Unix, because all the files
	// that would go to c:\\zdoomdat are already stored in .zdoom inside
	// the user's home directory.
	if (Args.CheckParm("-cdrom"))
	{
		Printf (GStrings(D_CDROM));
		mkdir ("c:\\zdoomdat", 0);
	}
#endif

	// turbo option  // [RH] (now a cvar)
	{
		UCVarValue value;

		value.String = Args.CheckValue ("-turbo");
		if (value.String == NULL)
			value.String = "100";
		else
			Printf ("turbo scale: %s%%\n", value.String);

		turbo.SetGenericRepDefault (value, CVAR_String);
	}

	v = Args.CheckValue ("-timer");
	if (v)
	{
		double time = strtod (v, NULL);
		Printf ("Levels will end after %g minute%s.\n", time, time > 1 ? "s" : "");
		timelimit = (float)time;
	}

	v = Args.CheckValue ("-avg");
	if (v)
	{
		Printf ("Austin Virtual Gaming: Levels will end after 20 minutes\n");
		timelimit = 20.f;
	}

	Printf ("Init miscellaneous info.\n");
	M_Init ();

	Printf ("Init DOOM refresh subsystem.\n");
	R_Init ();

	Printf ("Init Playloop state.\n");
	P_Init ();

	Printf ("Setting up sound.\n");
	S_Init ();

	Printf ("Checking network game status.\n");
	D_CheckNetGame ();

	Printf ("Init status bar.\n");
	if (gameinfo.gametype == GAME_Doom)
	{
		StatusBar = CreateDoomStatusBar ();
	}
	else if (gameinfo.gametype == GAME_Heretic)
	{
		StatusBar = CreateHereticStatusBar ();
	}
	else if (gameinfo.gametype == GAME_Hexen)
	{
		StatusBar = CreateHexenStatusBar ();
	}
	else
	{
		StatusBar = new FBaseStatusBar (0);
	}
	StatusBar->AttachToPlayer (&players[consoleplayer]);

	// [RH] Lock any cvars that should be locked now that we're
	// about to begin the game.
	FBaseCVar::EnableNoSet ();

	// [RH] Run any saved commands from the command line or autoexec.cfg now.
	gamestate = GS_FULLCONSOLE;
	Net_NewMakeTic ();
	DObject::BeginFrame ();
	DThinker::RunThinkers ();
	DObject::EndFrame ();
	gamestate = GS_STARTUP;

	// start the apropriate game based on parms
	v = Args.CheckValue ("-record");

	if (v)
	{
		G_RecordDemo (v);
		autostart = true;
	}

	files = Args.GatherFiles ("-playdemo", ".lmp", false);
	if (files->NumArgs() > 0)
	{
		singledemo = true;				// quit after one demo
		G_DeferedPlayDemo (files->GetArg (0));
		delete files;
		D_DoomLoop ();	// never returns
	}
	delete files;

	v = Args.CheckValue ("-timedemo");
	if (v)
	{
		G_TimeDemo (v);
		D_DoomLoop ();	// never returns
	}
		
	v = Args.CheckValue ("-loadgame");
	if (v)
	{
		strncpy (file, v, sizeof(file)-1);
		FixPathSeperator (file);
		DefaultExtension (file, ".zds");
		G_LoadGame (file);
	}

	if (gameaction != ga_loadgame)
	{
		BorderNeedRefresh = screen->GetPageCount ();
		if (autostart || netgame)
		{
			G_InitNew (startmap);
		}
		else
		{
			D_StartTitle ();				// start up intro loop
		}
	}

	if (demorecording)
		G_BeginRecording ();
				
	if (Args.CheckParm ("-debugfile"))
	{
		char	filename[20];
		sprintf (filename,"debug%i.txt",consoleplayer);
		Printf ("debug output to: %s\n",filename);
		debugfile = fopen (filename,"w");
	}

	atterm (D_QuitNetGame);		// killough

	D_DoomLoop ();		// never returns
}

//==========================================================================
//
// STAT fps
//
// Displays statistics about rendering times
//
//==========================================================================

ADD_STAT (fps, out)
{
	sprintf (out,
		"frame=%04.1f ms  walls=%04.1f ms  planes=%04.1f ms  masked=%04.1f ms",
		(double)FrameCycles * SecondsPerCycle * 1000,
		(double)WallCycles * SecondsPerCycle * 1000,
		(double)PlaneCycles * SecondsPerCycle * 1000,
		(double)MaskedCycles * SecondsPerCycle * 1000
		);
}

//==========================================================================
//
// STAT wallcycles
//
// Displays the minimum number of cycles spent drawing walls
//
//==========================================================================

static cycle_t bestwallcycles = INT_MAX;

ADD_STAT (wallcycles, out)
{
	if (WallCycles && WallCycles < bestwallcycles)
		bestwallcycles = WallCycles;
	sprintf (out, "%lu", bestwallcycles);
}

//==========================================================================
//
// CCMD clearwallcycles
//
// Resets the count of minimum wall drawing cycles
//
//==========================================================================

CCMD (clearwallcycles)
{
	bestwallcycles = INT_MAX;
}

#if 0
// To use these, also uncomment the clock/unclock in wallscan
static cycle_t bestscancycles = INT_MAX;

ADD_STAT (scancycles, out)
{
	if (WallScanCycles && WallScanCycles < bestscancycles)
		bestscancycles = WallScanCycles;
	sprintf (out, "%d", bestscancycles);
}

CCMD (clearscancycles)
{
	bestscancycles = INT_MAX;
}
#endif
