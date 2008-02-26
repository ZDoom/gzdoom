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
#include <assert.h>

#include "doomerrors.h"

#include "d_gui.h"
#include "m_alloc.h"
#include "m_random.h"
#include "doomdef.h"
#include "doomstat.h"
#include "gstrings.h"
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
#include "r_polymost.h"
#include "version.h"
#include "v_text.h"
#include "st_start.h"
#include "templates.h"
#include "teaminfo.h"
#include "hardware.h"
#include "sbarinfo.h"

EXTERN_CVAR(Bool, hud_althud)
void DrawHUD();

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

extern void M_RestoreMode ();
extern void M_SetDefaultMode ();
extern void R_ExecuteSetViewSize ();
extern void G_NewInit ();
extern void SetupPlayerClasses ();

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void D_CheckNetGame ();
void D_ProcessEvents ();
void G_BuildTiccmd (ticcmd_t* cmd);
void D_DoAdvanceDemo ();
void D_AddFile (const char *file);
void D_AddWildFile (const char *pattern);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

void D_DoomLoop ();
static const char *BaseFileSearch (const char *file, const char *ext, bool lookfirstinprogdir=false);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

EXTERN_CVAR (Float, turbo)
EXTERN_CVAR (Int, crosshair)
EXTERN_CVAR (Bool, freelook)
EXTERN_CVAR (Float, m_pitch)
EXTERN_CVAR (Float, m_yaw)
EXTERN_CVAR (Bool, invertmouse)
EXTERN_CVAR (Bool, lookstrafe)

extern gameinfo_t SharewareGameInfo;
extern gameinfo_t RegisteredGameInfo;
extern gameinfo_t RetailGameInfo;
extern gameinfo_t CommercialGameInfo;
extern gameinfo_t HereticGameInfo;
extern gameinfo_t HereticSWGameInfo;
extern gameinfo_t HexenGameInfo;
extern gameinfo_t HexenDKGameInfo;
extern gameinfo_t StrifeGameInfo;
extern gameinfo_t StrifeTeaserGameInfo;
extern gameinfo_t StrifeTeaser2GameInfo;

extern int testingmode;
extern bool setmodeneeded;
extern bool netdemo;
extern int NewWidth, NewHeight, NewBits, DisplayBits;
EXTERN_CVAR (Bool, st_scale)
extern bool gameisdead;
extern bool demorecording;
extern bool M_DemoNoPlay;	// [RH] if true, then skip any demos in the loop
extern bool insave;

extern cycle_t WallCycles, PlaneCycles, MaskedCycles, WallScanCycles;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

CUSTOM_CVAR (Int, fraglimit, 0, CVAR_SERVERINFO)
{
	// Check for the fraglimit being hit because the fraglimit is being
	// lowered below somebody's current frag count.
	if (deathmatch && self > 0)
	{
		for (int i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i] && self <= D_GetFragCount(&players[i]))
			{
				Printf ("%s\n", GStrings("TXT_FRAGLIMIT"));
				G_ExitLevel (0, false);
				break;
			}
		}
	}
}

CVAR (Float, timelimit, 0.f, CVAR_SERVERINFO);
CVAR (Bool, queryiwad, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG);
CVAR (String, defaultiwad, "", CVAR_ARCHIVE|CVAR_GLOBALCONFIG);
CVAR (Int, wipetype, 1, CVAR_ARCHIVE);

bool DrawFSHUD;				// [RH] Draw fullscreen HUD?
wadlist_t *wadfiles;		// [RH] remove limit on # of loaded wads
bool devparm;				// started game with -devparm
const char *D_DrawIcon;	// [RH] Patch name of icon to draw on next refresh
int NoWipe;				// [RH] Allow wipe? (Needs to be set each time)
bool singletics = false;	// debug flag to cancel adaptiveness
char startmap[8];
bool autostart;
bool advancedemo;
FILE *debugfile;
event_t events[MAXEVENTS];
int eventhead;
int eventtail;
gamestate_t wipegamestate = GS_DEMOSCREEN;	// can be -1 to force a wipe
bool PageBlank;
FTexture *Page;
FTexture *Advisory;

cycle_t FrameCycles;

// If autoname is NULL, that's either because that game doesn't allow
// loading of external wads or because it's already caught by the
// general game-specific wads section.
const IWADInfo IWADInfos[NUM_IWAD_TYPES] =
{
	// banner text,								autoname,	fg color,				bg color
	{ "Final Doom: TNT - Evilution",			"TNT",		MAKERGB(168,0,0),		MAKERGB(168,168,168) },
	{ "Final Doom: Plutonia Experiment",		"Plutonia",	MAKERGB(168,0,0),		MAKERGB(168,168,168) },
	{ "Hexen: Beyond Heretic",					NULL,		MAKERGB(240,240,240),	MAKERGB(107,44,24) },
	{ "Hexen: Deathkings of the Dark Citadel",	"HexenDK",	MAKERGB(240,240,240),	MAKERGB(139,68,9) },
	{ "DOOM 2: Hell on Earth",					"Doom2",	MAKERGB(168,0,0),		MAKERGB(168,168,168) },
	{ "Heretic Shareware",						NULL,		MAKERGB(252,252,0),		MAKERGB(168,0,0) },
	{ "Heretic: Shadow of the Serpent Riders",	NULL,		MAKERGB(252,252,0),		MAKERGB(168,0,0) },
	{ "Heretic",								NULL,		MAKERGB(252,252,0),		MAKERGB(168,0,0) },
	{ "DOOM Shareware",							NULL,		MAKERGB(168,0,0),		MAKERGB(168,168,168) },
	{ "The Ultimate DOOM",						"Doom1",	MAKERGB(84,84,84),		MAKERGB(168,168,168) },
	{ "DOOM Registered",						"Doom1",	MAKERGB(84,84,84),		MAKERGB(168,168,168) },
	{ "Strife: Quest for the Sigil",			NULL,		MAKERGB(224,173,153),	MAKERGB(0,107,101) },
	{ "Strife: Teaser (Old Version)",			NULL,		MAKERGB(224,173,153),	MAKERGB(0,107,101) },
	{ "Strife: Teaser (New Version)",			NULL,		MAKERGB(224,173,153),	MAKERGB(0,107,101) }
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
	"strife1.wad",
	"strife0.wad",
	NULL
};

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
		if (testingmode == 1)
		{
			M_SetDefaultMode ();
		}
		else if (testingmode <= I_GetTime(false))
		{
			M_RestoreMode ();
		}
		return;
	}

	for (; eventtail != eventhead ; eventtail = (eventtail+1)&(MAXEVENTS-1))
	{
		ev = &events[eventtail];
		if (C_Responder (ev))
			continue;				// console ate the event
		if (M_Responder (ev))
			continue;				// menu ate the event
		if (testpolymost)
			Polymost_Responder (ev);
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
	if (ev->type == EV_Mouse && !testpolymost && !paused && menuactive == MENU_Off &&
		ConsoleState != c_down && ConsoleState != c_falling)
	{
		if (Button_Mlook.bDown || freelook)
		{
			int look = int(ev->y * m_pitch * mouse_sensitivity * 16.0);
			if (invertmouse)
				look = -look;
			G_AddViewPitch (look);
			events[eventhead].y = 0;
		}
		if (!Button_Strafe.bDown && !lookstrafe)
		{
			G_AddViewAngle (int(ev->x * m_yaw * mouse_sensitivity * 8.0));
			events[eventhead].x = 0;
		}
		if ((events[eventhead].x | events[eventhead].y) == 0)
		{
			return;
		}
	}
	eventhead = (eventhead+1)&(MAXEVENTS-1);
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
	if (sky1texture != 0)
		R_InitSkyMap ();

	if (self & DF_NO_FREELOOK)
	{
		Net_WriteByte (DEM_CENTERVIEW);
	}
	// If nofov is set, force everybody to the arbitrator's FOV.
	if ((self & DF_NO_FOV) && consoleplayer == Net_Arbitrator)
	{
		BYTE fov;

		Net_WriteByte (DEM_FOV);

		// If the game is started with DF_NO_FOV set, the arbitrator's
		// DesiredFOV will not be set when this callback is run, so
		// be sure not to transmit a 0 FOV.
		fov = (BYTE)players[consoleplayer].DesiredFOV;
		if (fov == 0)
		{
			fov = 90;
		}
		Net_WriteByte (fov);
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
CVAR (Flag, sv_noweaponspawn,	dmflags, DF_NO_COOP_WEAPON_SPAWN);
CVAR (Flag, sv_nocrouch,		dmflags, DF_NO_CROUCH);

//==========================================================================
//
// CVAR dmflags2
//
// [RH] From Skull Tag. Some of these were already done as separate cvars
// (such as bfgaiming), but I collected them here like Skull Tag does.
//
//==========================================================================

CVAR (Int, dmflags2, 0, CVAR_SERVERINFO);
CVAR (Flag, sv_weapondrop,			dmflags2, DF2_YES_WEAPONDROP);
CVAR (Flag, sv_noteamswitch,		dmflags2, DF2_NO_TEAM_SWITCH);
CVAR (Flag, sv_doubleammo,			dmflags2, DF2_YES_DOUBLEAMMO);
CVAR (Flag, sv_degeneration,		dmflags2, DF2_YES_DEGENERATION);
CVAR (Flag, sv_nobfgaim,			dmflags2, DF2_NO_FREEAIMBFG);
CVAR (Flag, sv_barrelrespawn,		dmflags2, DF2_BARRELS_RESPAWN);
CVAR (Flag, sv_keepfrags,			dmflags2, DF2_YES_KEEPFRAGS);
CVAR (Flag, sv_norespawn,			dmflags2, DF2_NO_RESPAWN);
CVAR (Flag, sv_losefrag,			dmflags2, DF2_YES_LOSEFRAG);
CVAR (Flag, sv_respawnprotect,		dmflags2, DF2_YES_RESPAWN_INVUL);
CVAR (Flag, sv_samespawnspot,		dmflags2, DF2_SAME_SPAWN_SPOT);
CVAR (Flag, sv_infiniteinventory,	dmflags2, DF2_INFINITE_INVENTORY)

//==========================================================================
//
// CVAR compatflags
//
//==========================================================================

int i_compatflags;	// internal compatflags composed from the compatflags CVAR and MAPINFO settings

CUSTOM_CVAR (Int, compatflags, 0, CVAR_ARCHIVE|CVAR_SERVERINFO)
{
	if (level.info == NULL) i_compatflags = self;
	else i_compatflags = (self & ~level.info->compatmask) | (level.info->compatflags & level.info->compatmask);
}

CVAR (Flag, compat_shortTex,	compatflags, COMPATF_SHORTTEX);
CVAR (Flag, compat_stairs,		compatflags, COMPATF_STAIRINDEX);
CVAR (Flag, compat_limitpain,	compatflags, COMPATF_LIMITPAIN);
CVAR (Flag, compat_silentpickup,compatflags, COMPATF_SILENTPICKUP);
CVAR (Flag, compat_nopassover,	compatflags, COMPATF_NO_PASSMOBJ);
CVAR (Flag, compat_soundslots,	compatflags, COMPATF_MAGICSILENCE);
CVAR (Flag, compat_wallrun,		compatflags, COMPATF_WALLRUN);
CVAR (Flag, compat_notossdrops,	compatflags, COMPATF_NOTOSSDROPS);
CVAR (Flag, compat_useblocking, compatflags, COMPATF_USEBLOCKING);
CVAR (Flag, compat_nodoorlight,	compatflags, COMPATF_NODOORLIGHT);
CVAR (Flag, compat_ravenscroll,	compatflags, COMPATF_RAVENSCROLL);
CVAR (Flag, compat_soundtarget,	compatflags, COMPATF_SOUNDTARGET);
CVAR (Flag, compat_dehhealth,	compatflags, COMPATF_DEHHEALTH);
CVAR (Flag, compat_trace,		compatflags, COMPATF_TRACE);
CVAR (Flag, compat_dropoff,		compatflags, COMPATF_DROPOFF);
CVAR (Flag, compat_boomscroll,	compatflags, COMPATF_BOOMSCROLL);
CVAR (Flag, compat_invisibility,compatflags, COMPATF_INVISIBILITY);

//==========================================================================
//
// D_Display
//
// Draw current display, possibly wiping it from the previous
//
//==========================================================================

void D_Display ()
{
	bool wipe;
	bool hw2d;

	if (nodrawers)
		return; 				// for comparative timing / profiling
	
	cycle_t cycles = 0;
	clock (cycles);

	if (players[consoleplayer].camera == NULL)
	{
		players[consoleplayer].camera = players[consoleplayer].mo;
	}

	if (viewactive)
	{
		R_SetFOV (players[consoleplayer].camera && players[consoleplayer].camera->player ?
			players[consoleplayer].camera->player->FOV : 90.f);
	}

	// [RH] change the screen mode if needed
	if (setmodeneeded)
	{
		// Change screen mode.
		if (Video->SetResolution (NewWidth, NewHeight, NewBits))
		{
			// Recalculate various view parameters.
			setsizeneeded = true;
			// Let the status bar know the screen size changed
			if (StatusBar != NULL)
			{
				StatusBar->ScreenSizeChanged ();
			}
			// Refresh the console.
			C_NewModeAdjust ();
			// Reload crosshair if transitioned to a different size
			crosshair.Callback ();
		}
	}

	RenderTarget = screen;

	// change the view size if needed
	if (setsizeneeded && StatusBar != NULL)
	{
		R_ExecuteSetViewSize ();
	}
	setmodeneeded = false;

	if (screen->Lock (false))
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
	else if (gamestate != wipegamestate && gamestate != GS_FULLCONSOLE && gamestate != GS_TITLELEVEL)
	{ // save the current screen if about to wipe
		BorderNeedRefresh = screen->GetPageCount ();
		if (wipegamestate != GS_FORCEWIPEFADE)
		{
			wipe = screen->WipeStartScreen (wipetype);
		}
		else
		{
			wipe = screen->WipeStartScreen (wipe_Fade);
		}
		wipegamestate = gamestate;
	}
	else
	{
		wipe = false;
	}

	hw2d = false;

	if (testpolymost)
	{
		drawpolymosttest();
		C_DrawConsole(hw2d);
		M_Drawer();
	}
	else
	{
		switch (gamestate)
		{
		case GS_FULLCONSOLE:
			screen->SetBlendingRect(0,0,0,0);
			hw2d = screen->Begin2D(false);
			C_DrawConsole (false);
			M_Drawer ();
			screen->Update ();
			return;

		case GS_LEVEL:
		case GS_TITLELEVEL:
			if (!gametic)
				break;

			if (StatusBar != NULL)
			{
				float blend[4] = { 0, 0, 0, 0 };
				StatusBar->BlendView (blend);
			}
			screen->SetBlendingRect(viewwindowx, viewwindowy,
				viewwindowx + realviewwidth, viewwindowy + realviewheight);
			P_CheckPlayerSprites();
			screen->RenderView(&players[consoleplayer]);
			if ((hw2d = screen->Begin2D(viewactive)))
			{
				// Redraw everything every frame when using 2D accel
				SB_state = screen->GetPageCount();
				BorderNeedRefresh = screen->GetPageCount();
			}
			if (automapactive)
			{
				int saved_ST_Y=ST_Y;
				if (hud_althud && realviewheight == SCREENHEIGHT) ST_Y=realviewheight;
				AM_Drawer ();
				ST_Y = saved_ST_Y;
			}
			if (!automapactive || viewactive)
			{
				R_RefreshViewBorder ();
			}

			if (hud_althud && realviewheight == SCREENHEIGHT)
			{
				if (DrawFSHUD || automapactive) DrawHUD();
				StatusBar->DrawTopStuff (HUD_None);
			}
			else 
			if (realviewheight == SCREENHEIGHT && viewactive)
			{
				StatusBar->Draw (DrawFSHUD ? HUD_Fullscreen : HUD_None);
				StatusBar->DrawTopStuff (DrawFSHUD ? HUD_Fullscreen : HUD_None);
			}
			else
			{
				StatusBar->Draw (HUD_StatusBar);
				StatusBar->DrawTopStuff (HUD_StatusBar);
			}
			CT_Drawer ();
			break;

		case GS_INTERMISSION:
			screen->SetBlendingRect(0,0,0,0);
			hw2d = screen->Begin2D(false);
			WI_Drawer ();
			CT_Drawer ();
			break;

		case GS_FINALE:
			screen->SetBlendingRect(0,0,0,0);
			hw2d = screen->Begin2D(false);
			F_Drawer ();
			CT_Drawer ();
			break;

		case GS_DEMOSCREEN:
			screen->SetBlendingRect(0,0,0,0);
			hw2d = screen->Begin2D(false);
			D_PageDrawer ();
			CT_Drawer ();
			break;

		default:
			break;
		}
	}
	// draw pause pic
	if (paused && menuactive == MENU_Off)
	{
		FTexture *tex;
		int x;

		tex = TexMan[gameinfo.gametype & (GAME_Doom|GAME_Strife) ? "M_PAUSE" : "PAUSED"];
		x = (SCREENWIDTH - tex->GetWidth()*CleanXfac)/2 +
			tex->LeftOffset*CleanXfac;
		screen->DrawTexture (tex, x, 4, DTA_CleanNoMove, true, TAG_DONE);
	}

	// [RH] Draw icon, if any
	if (D_DrawIcon)
	{
		int picnum = TexMan.CheckForTexture (D_DrawIcon, FTexture::TEX_MiscPatch);

		D_DrawIcon = NULL;
		if (picnum >= 0)
		{
			FTexture *tex = TexMan[picnum];
			screen->DrawTexture (tex, 160-tex->GetWidth()/2, 100-tex->GetHeight()/2,
				DTA_320x200, true, TAG_DONE);
		}
		NoWipe = 10;
	}

	if (!wipe || NoWipe < 0)
	{
		NetUpdate ();			// send out any new accumulation
		// normal update
		C_DrawConsole (hw2d);	// draw console
		M_Drawer ();			// menu is drawn even on top of everything
		FStat::PrintStat ();
		screen->Update ();	// page flip or blit buffer
	}
	else
	{
		// wipe update
		int wipestart, nowtime, tics;
		bool done;

		screen->WipeEndScreen ();

		wipestart = I_GetTime (false);

		Net_WriteByte (DEM_WIPEON);
		NetUpdate ();		// send out any new accumulation

		do
		{
			nowtime = I_WaitForTic (wipestart);
			tics = nowtime - wipestart;
			wipestart = nowtime;
			done = screen->WipeDo (tics);
			C_DrawConsole (hw2d);	// console and
			M_Drawer ();			// menu are drawn even on top of wipes
			screen->Update ();		// page flip or blit buffer
			NetUpdate ();
		} while (!done);
		screen->WipeCleanup();

		Net_WriteByte (DEM_WIPEOFF);
	}

	unclock (cycles);
	FrameCycles = cycles;
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
	if (demorecording || demoplayback)
		G_CheckDemoStatus ();
	Net_ClearBuffers ();
	G_NewInit ();
	singletics = false;
	playeringame[0] = 1;
	players[0].playerstate = PST_LIVE;
	gameaction = ga_fullconsole;
	menuactive = MENU_Off;
	insave = false;
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
	int lasttic = 0;

	for (;;)
	{
		try
		{
			// frame syncronous IO operations
			if (gametic > lasttic)
			{
				lasttic = gametic;
				I_StartFrame ();
			}
			
			// process one or more tics
			if (singletics)
			{
				I_StartTic ();
				D_ProcessEvents ();
				G_BuildTiccmd (&netcmds[consoleplayer][maketic%BACKUPTICS]);
				//Added by MC: For some of that bot stuff. The main bot function.
				int i;
				for (i = 0; i < MAXPLAYERS; i++)
				{
					if (playeringame[i] && players[i].isbot && players[i].mo)
					{
						players[i].savedyaw = players[i].mo->angle;
						players[i].savedpitch = players[i].mo->pitch;
					}
				}
				bglobal.Main (maketic%BACKUPTICS);
				for (i = 0; i < MAXPLAYERS; i++)
				{
					if (playeringame[i] && players[i].isbot && players[i].mo)
					{
						players[i].mo->angle = players[i].savedyaw;
						players[i].mo->pitch = players[i].savedpitch;
					}
				}
				if (advancedemo)
					D_DoAdvanceDemo ();
				C_Ticker ();
				M_Ticker ();
				G_Ticker ();
				gametic++;
				maketic++;
				GC::CheckGC ();
				Net_NewMakeTic ();
			}
			else
			{
				TryRunTics (); // will run at least one tic
			}
			// [RH] Use the consoleplayer's camera to update sounds
			S_UpdateSounds (players[consoleplayer].camera);	// move positional sounds
			// Update display, next frame, with current state.
			I_StartTic ();
			D_Display ();
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
	if (Page != NULL)
	{
		screen->DrawTexture (Page, 0, 0,
			DTA_VirtualWidth, Page->GetWidth(),
			DTA_VirtualHeight, Page->GetHeight(),
			DTA_Masked, false,
			DTA_BilinearFilter, true,
			TAG_DONE);
		screen->FillBorder (NULL);
	}
	else
	{
		screen->Clear (0, 0, SCREENWIDTH, SCREENHEIGHT, 0, 0);
		if (!PageBlank)
		{
			screen->DrawText (CR_WHITE, 0, 0, "Page graphic goes here", TAG_DONE);
		}
	}
	if (Advisory != NULL)
	{
		screen->DrawTexture (Advisory, 4, 160, DTA_320x200, true, TAG_DONE);
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
// D_DoStrifeAdvanceDemo
//
//==========================================================================

void D_DoStrifeAdvanceDemo ()
{
	static const char *const fullVoices[6] =
	{
		"svox/pro1", "svox/pro2", "svox/pro3", "svox/pro4", "svox/pro5", "svox/pro6"
	};
	static const char *const teaserVoices[6] =
	{
		"svox/voc91", "svox/voc92", "svox/voc93", "svox/voc94", "svox/voc95", "svox/voc96"
	};
	const char *const *voices = gameinfo.flags & GI_SHAREWARE ? teaserVoices : fullVoices;
	const char *pagename = NULL;

	gamestate = GS_DEMOSCREEN;
	PageBlank = false;

	switch (demosequence)
	{
	default:
	case 0:
		pagetic = 6 * TICRATE;
		pagename = "TITLEPIC";
		if (Wads.CheckNumForName ("d_logo", ns_music) < 0)
		{ // strife0.wad does not have d_logo
			S_StartMusic ("");
		}
		else
		{
			S_StartMusic ("d_logo");
		}
		C_HideConsole ();
		break;

	case 1:
		// [RH] Strife fades to black and then to the Rogue logo, but
		// I think it looks better if it doesn't fade.
		pagetic = 10 * TICRATE/35;
		pagename = "";	// PANEL0, but strife0.wad doesn't have it, so don't use it.
		PageBlank = true;
		S_Sound (CHAN_VOICE, "bishop/active", 1, ATTN_NORM);
		break;

	case 2:
		pagetic = 4 * TICRATE;
		pagename = "RGELOGO";
		break;

	case 3:
		pagetic = 7 * TICRATE;
		pagename = "PANEL1";
		S_Sound (CHAN_VOICE, voices[0], 1, ATTN_NORM);
		S_StartMusic ("d_intro");
		break;

	case 4:
		pagetic = 9 * TICRATE;
		pagename = "PANEL2";
		S_Sound (CHAN_VOICE, voices[1], 1, ATTN_NORM);
		break;

	case 5:
		pagetic = 12 * TICRATE;
		pagename = "PANEL3";
		S_Sound (CHAN_VOICE, voices[2], 1, ATTN_NORM);
		break;

	case 6:
		pagetic = 11 * TICRATE;
		pagename = "PANEL4";
		S_Sound (CHAN_VOICE, voices[3], 1, ATTN_NORM);
		break;

	case 7:
		pagetic = 10 * TICRATE;
		pagename = "PANEL5";
		S_Sound (CHAN_VOICE, voices[4], 1, ATTN_NORM);
		break;

	case 8:
		pagetic = 16 * TICRATE;
		pagename = "PANEL6";
		S_Sound (CHAN_VOICE, voices[5], 1, ATTN_NORM);
		break;

	case 9:
		pagetic = 6 * TICRATE;
		pagename = "vellogo";
		wipegamestate = GS_FORCEWIPEFADE;
		break;

	case 10:
		pagetic = 12 * TICRATE;
		pagename = "CREDIT";
		wipegamestate = GS_FORCEWIPEFADE;
		break;
	}
	if (demosequence++ > 10)
		demosequence = 0;
	if (demosequence == 9 && !(gameinfo.flags & GI_SHAREWARE))
		demosequence = 10;

	if (pagename)
	{
		if (Page != NULL)
		{
			Page->Unload ();
			Page = NULL;
		}
		if (pagename[0])
		{
			Page = TexMan[pagename];
		}
	}
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
	const char *pagename = NULL;

	V_SetBlend (0,0,0,0);
	players[consoleplayer].playerstate = PST_LIVE;	// not reborn
	advancedemo = false;
	usergame = false;				// no save / end game here
	paused = 0;
	gameaction = ga_nothing;

	// [RH] If you want something more dynamic for your title, create a map
	// and name it TITLEMAP. That map will be loaded and used as the title.

	MapData * map = P_OpenMapData("TITLEMAP");
	if (map != NULL)
	{
		delete map;
		G_InitNew ("TITLEMAP", true);
		return;
	}

	if (gameinfo.gametype == GAME_Strife)
	{
		D_DoStrifeAdvanceDemo ();
		return;
	}

	switch (demosequence)
	{
	case 3:
		if (gameinfo.advisoryTime)
		{
			Advisory = TexMan["ADVISOR"];
			demosequence = 1;
			pagetic = (int)(gameinfo.advisoryTime * TICRATE);
			break;
		}
		// fall through to case 1 if no advisory notice

	case 1:
		Advisory = NULL;
		if (!M_DemoNoPlay)
		{
			BorderNeedRefresh = screen->GetPageCount ();
			democount++;
			sprintf (demoname + 4, "%d", democount);
			if (Wads.CheckNumForName (demoname) < 0)
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
		if (Page != NULL)
		{
			Page->Unload ();
		}
		Page = TexMan[pagename];
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
	wadlist_t *wad = (wadlist_t *)M_Malloc (sizeof(*wad) + strlen(file));

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
				if (!(I_FindAttr(&findstate) & FA_DIREC))
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
		{ registered,	&RegisteredGameInfo,	doom },			// DoomRegistered
		{ commercial,	&StrifeGameInfo,		doom2 },		// Strife
		{ commercial,	&StrifeTeaserGameInfo,	doom2 },		// StrifeTeaser
		{ commercial,	&StrifeTeaser2GameInfo,	doom2 },		// StrifeTeaser2
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
		"ENDSTRF",
		"MAP33",
		"INVCURS",
		"E2M1","E2M2","E2M3","E2M4","E2M5","E2M6","E2M7","E2M8","E2M9",
		"E3M1","E3M2","E3M3","E3M4","E3M5","E3M6","E3M7","E3M8","E3M9",
		"DPHOOF","BFGGA0","HEADA1","CYBRA1",
		{ 'S','P','I','D','A','1','D','1' },
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
		Check_endstrf,
		Check_map33,
		Check_invcurs,
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
			header.NumLumps = LittleLong(header.NumLumps);
			if (0 == fseek (f, LittleLong(header.InfoTableOfs), SEEK_SET))
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

	if (lumpsfound[Check_title] && lumpsfound[Check_map60])
	{
		return IWAD_HexenDK;
	}
	else if (lumpsfound[Check_map33] && lumpsfound[Check_endstrf])
	{
		if (lumpsfound[Check_map01])
		{
			return IWAD_Strife;
		}
		else if (lumpsfound[Check_invcurs])
		{
			return IWAD_StrifeTeaser2;
		}
		else
		{
			return IWAD_StrifeTeaser;
		}
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
	int i;
	int numfound;

	numfound = 0;

	slash = (doomwaddir[0] && doomwaddir[strlen (doomwaddir)-1] != '/') ? "/" : "";

	// Search for a pre-defined IWAD
	for (i = IWADNames[0] ? 0 : 1; IWADNames[i]; i++)
	{
		if (wads[i].Path.IsEmpty())
		{
			FString iwad;
			
			iwad.Format ("%s%s%s", doomwaddir, slash, IWADNames[i]);
			FixPathSeperator (iwad.LockBuffer());
			if (FileExists (iwad))
			{
				wads[i].Type = ScanIWAD (iwad);
				if (wads[i].Type != NUM_IWAD_TYPES)
				{
					wads[i].Path = iwad;
					numfound++;
				}
			}
		}
	}

	return numfound;
}

//==========================================================================
//
// ExpandEnvVars
//
// Expands environment variable references in a string. Intended primarily
// for use with IWAD search paths in config files.
//
//==========================================================================

static FString ExpandEnvVars(const char *searchpathstring)
{
	static const char envvarnamechars[] =
		"01234567890"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"_"
		"abcdefghijklmnopqrstuvwxyz";

	if (searchpathstring == NULL)
		return FString ("");

	const char *dollar = strchr (searchpathstring,'$');
	if (dollar == NULL)
	{
		return FString (searchpathstring);
	}

	const char *nextchars = searchpathstring;
	FString out = FString (searchpathstring, dollar - searchpathstring);
	while ( (dollar != NULL) && (*nextchars != 0) )
	{
		size_t length = strspn(dollar + 1, envvarnamechars);
		if (length != 0)
		{
			FString varname = FString (dollar + 1, length);
			if (stricmp (varname, "progdir") == 0)
			{
				out += progdir;
			}
			else
			{
				char *varvalue = getenv (varname);
				if ( (varvalue != NULL) && (strlen(varvalue) != 0) )
				{
					out += varvalue;
				}
			}
		}
		else
		{
			out += '$';
		}
		nextchars = dollar + length + 1;
		dollar = strchr (nextchars, '$');
		if (dollar != NULL)
		{
			out += FString (nextchars, dollar - nextchars);
		}
	}
	if (*nextchars != 0)
	{
		out += nextchars;
	}
	return out;
}

//==========================================================================
//
// CheckIWADinEnvDir
//
// Checks for an IWAD in a path that contains one or more environment
// variables.
//
//==========================================================================

static int CheckIWADinEnvDir (const char *str, WadStuff *wads)
{
	FString expanded = ExpandEnvVars (str);

	if (!expanded.IsEmpty())
	{
		char *dir = expanded.LockBuffer ();
		FixPathSeperator (dir);
		expanded.UnlockBuffer ();
		if (expanded[expanded.Len() - 1] != '/')
		{
			expanded += '/';
		}
		return CheckIWAD (expanded, wads);
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

static EIWADType IdentifyVersion (const char *zdoom_wad)
{
	WadStuff wads[sizeof(IWADNames)/sizeof(char *)];
	size_t foundwads[NUM_IWAD_TYPES] = { 0 };
	const char *iwadparm = Args->CheckValue ("-iwad");
	size_t numwads;
	int pickwad;
	size_t i;
	bool iwadparmfound = false;
	FString custwad;

	if (iwadparm)
	{
		custwad = iwadparm;
		FixPathSeperator (custwad.LockBuffer());
		if (CheckIWAD (custwad, wads))
		{ // -iwad parameter was a directory
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

	if (iwadparm == NULL || wads[0].Path.IsEmpty())
	{
		if (GameConfig->SetSection ("IWADSearch.Directories"))
		{
			const char *key;
			const char *value;

			while (GameConfig->NextInSection (key, value))
			{
				if (stricmp (key, "Path") == 0)
				{
					if (strchr (value, '$') != NULL)
					{
						CheckIWADinEnvDir (value, wads);
					}
#ifdef unix
					else if (*value == '~' && (*(value + 1) == 0 || *(value + 1) == '/'))
					{
						FString homepath = GetUserFile (*(value + 1) ? value + 2 : value + 1, true);
						CheckIWAD (homepath, wads);
					}
#endif
					else
					{
						CheckIWAD (value, wads);
					}
				}
			}
		}
#ifdef _WIN32
		FString steam_path = I_GetSteamPath();
		if (steam_path.IsNotEmpty())
		{
			static const char *const steam_dirs[] =
			{
				"doom 2/base",
				"final doom/base",
				"heretic shadow of the serpent riders/base",
				"hexen/base",
				"hexen deathkings of the dark citadel/base",
				"ultimate doom/base"
			};
			steam_path += "/SteamApps/common/";
			for (i = 0; i < countof(steam_dirs); ++i)
			{
				CheckIWAD (steam_path + steam_dirs[i], wads);
			}
		}
#endif
	}

	if (iwadparm != NULL && !wads[0].Path.IsEmpty())
	{
		iwadparmfound = true;
	}

	for (i = numwads = 0; i < countof(IWADNames); i++)
	{
		if (!wads[i].Path.IsEmpty())
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
					  "1. Place one or more of these wads in the same directory as ZDoom.\n"
					  "2. Edit your zdoom-username.ini and add the directories of your iwads\n"
					  "to the list beneath [IWADSearch.Directories]");
	}

	pickwad = 0;

	if (!iwadparmfound && numwads > 1)
	{
		int defiwad = 0;

		// Locate the user's prefered IWAD, if it was found.
		if (defaultiwad[0] != '\0')
		{
			for (i = 0; i < numwads; ++i)
			{
				FString basename = ExtractFileBase (wads[i].Path);
				if (stricmp (basename, defaultiwad) == 0)
				{
					defiwad = (int)i;
					break;
				}
			}
		}
		pickwad = I_PickIWad (wads, (int)numwads, queryiwad, defiwad);
		if (pickwad >= 0)
		{
			// The newly selected IWAD becomes the new default
			FString basename = ExtractFileBase (wads[pickwad].Path);
			defaultiwad = basename;
		}
	}

	if (pickwad < 0)
		exit (0);

	// zdoom.pk3 must always be the first file loaded and the IWAD second.
	D_AddFile (zdoom_wad);

	if (wads[pickwad].Type == IWAD_HexenDK)
	{ // load hexen.wad before loading hexdd.wad
		D_AddFile (wads[foundwads[IWAD_Hexen]-1].Path);
	}

	SetIWAD (wads[pickwad].Path, wads[pickwad].Type);

	if (wads[pickwad].Type == IWAD_Strife)
	{ // Try to load voices.wad along with strife1.wad
		long lastslash = wads[pickwad].Path.LastIndexOf ('/');
		FString path;

		if (lastslash == -1)
		{
			path = "";//  wads[pickwad].Path;
		}
		else
		{
			path = FString (wads[pickwad].Path.GetChars(), lastslash + 1);
		}
		path += "voices.wad";
		D_AddFile (path);
	}

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

static const char *BaseFileSearch (const char *file, const char *ext, bool lookfirstinprogdir)
{
	static char wad[PATH_MAX];

	if (lookfirstinprogdir)
	{
		sprintf (wad, "%s%s%s", progdir, progdir[strlen (progdir) - 1] != '/' ? "/" : "", file);
		if (FileExists (wad))
		{
			return wad;
		}
	}

	if (FileExists (file))
	{
		sprintf (wad, "%s", file);
		return wad;
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
				FString homepath;

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
					homepath = GetUserFile (*(value + 1) ? value + 2 : value + 1, true);
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
		FString tmp = file;
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
	DArgs *files = Args->GatherFiles (arg, ext, false);

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
	files->Destroy();
	return noDef;
}

//==========================================================================
//
// D_LoadWadSettings
//
// Parses any loaded KEYCONF lumps. These are restricted console scripts
// that can only execute the alias, defaultbind, addkeysection,
// addmenukey, weaponsection, and addslotdefault commands.
//
//==========================================================================

void D_LoadWadSettings ()
{
	char cmd[4096];
	int lump, lastlump = 0;

	ParsingKeyConf = true;

	while ((lump = Wads.FindLump ("KEYCONF", &lastlump)) != -1)
	{
		FMemLump data = Wads.ReadLump (lump);
		const char *eof = (char *)data.GetMem() + Wads.LumpLength (lump);
		const char *conf = (char *)data.GetMem();

		while (conf < eof)
		{
			size_t i;

			// Fetch a line to execute
			for (i = 0; conf + i < eof && conf[i] != '\n'; ++i)
			{
				cmd[i] = conf[i];
			}
			cmd[i] = 0;
			conf += i;
			if (*conf == '\n')
			{
				conf++;
			}

			// Comments begin with //
			char *stop = cmd + i - 1;
			char *comment = cmd;
			int inQuote = 0;

			if (*stop == '\r')
				*stop-- = 0;

			while (comment < stop)
			{
				if (*comment == '\"')
				{
					inQuote ^= 1;
				}
				else if (!inQuote && *comment == '/' && *(comment + 1) == '/')
				{
					break;
				}
				comment++;
			}
			if (comment == cmd)
			{ // Comment at line beginning
				continue;
			}
			else if (comment < stop)
			{ // Comment in middle of line
				*comment = 0;
			}

			AddCommandString (cmd);
		}
	}
	ParsingKeyConf = false;
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
	FString file;
	char *v;
	const char *wad;
	DArgs *execFiles;
	const IWADInfo *iwad_info;

	srand(I_MSTime());

	PClass::StaticInit ();
	atterm (C_DeinitConsole);

	gamestate = GS_STARTUP;

	SetLanguageIDs ();

	rngseed = (DWORD)time (NULL);
	FRandom::StaticClearRandom ();
	M_FindResponseFile ();

	Printf ("M_LoadDefaults: Load system defaults.\n");
	M_LoadDefaults ();			// load before initing other systems

	// [RH] Make sure zdoom.pk3 is always loaded,
	// as it contains magic stuff we need.

	wad = BaseFileSearch (BASEWAD, NULL, true);
	if (wad == NULL)
	{
		I_FatalError ("Cannot find " BASEWAD);
	}

	iwad_info = &IWADInfos[IdentifyVersion(wad)];
	I_SetIWADInfo(iwad_info);
	GameConfig->DoGameSetup (GameNames[gameinfo.gametype]);

	if (!(gameinfo.flags & GI_SHAREWARE))
	{
		// [RH] zvox.wad - A wad I had intended to be automatically generated
		// from Q2's pak0.pak so the female and cyborg player could have
		// voices. I never got around to writing the utility to do it, though.
		// And I probably never will now. But I know at least one person uses
		// it for something else, so this gets to stay here.
		wad = BaseFileSearch ("zvox.wad", NULL);
		if (wad)
			D_AddFile (wad);
	
		// [RH] Add any .wad files in the skins directory
#ifdef unix
		file = SHARE_DIR;
#else
		file = progdir;
#endif
		file += "skins";
		D_AddDirectory (file);
	
		const char *home = getenv ("HOME");
		if (home)
		{
			file = home;
			if (home[strlen(home) - 1] != '/')
			{
				file += '/';
			}
			file += ".zdoom/skins";
			D_AddDirectory (file);
		}
	
	
		// Add common (global) wads
		D_AddConfigWads ("Global.Autoload");

		// Add game-specific wads
		file = GameNames[gameinfo.gametype];
		file += ".Autoload";
		D_AddConfigWads (file);

		// Add IWAD-specific wads
		if (iwad_info->Autoname != NULL)
		{
			file = iwad_info->Autoname;
			file += ".Autoload";
			D_AddConfigWads(file);
		}
	}

	// Run automatically executed files
	execFiles = new DArgs;
	GameConfig->AddAutoexec (execFiles, GameNames[gameinfo.gametype]);
	D_MultiExec (execFiles, true);
	execFiles->Destroy();

	// Run .cfg files at the start of the command line.
	execFiles = Args->GatherFiles (NULL, ".cfg", false);
	D_MultiExec (execFiles, true);
	execFiles->Destroy();

	C_ExecCmdLineParams ();		// [RH] do all +set commands on the command line

	DArgs *files = Args->GatherFiles ("-file", ".wad", true);
	DArgs *files1 = Args->GatherFiles (NULL, ".zip", false);
	DArgs *files2 = Args->GatherFiles (NULL, ".pk3", false);
	DArgs *files3 = Args->GatherFiles (NULL, ".txt", false);
	if (files->NumArgs() > 0 || files1->NumArgs() > 0 || files2->NumArgs() > 0 || files3->NumArgs() > 0)
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
		for (int i = 0; i < files1->NumArgs(); i++)
		{
			D_AddWildFile (files1->GetArg (i));
		}
		for (int i = 0; i < files2->NumArgs(); i++)
		{
			D_AddWildFile (files2->GetArg (i));
		}
		for (int i = 0; i < files3->NumArgs(); i++)
		{
			D_AddWildFile (files3->GetArg (i));
		}
	}
	files->Destroy();
	files1->Destroy();
	files2->Destroy();
	files3->Destroy();

	Printf ("W_Init: Init WADfiles.\n");
	Wads.InitMultipleFiles (&wadfiles);

	// [RH] Initialize localizable strings.
	GStrings.LoadStrings (false);

	// [RH] Moved these up here so that we can do most of our
	//		startup output in a fullscreen console.

	CT_Init ();

	Printf ("I_Init: Setting up machine state.\n");
	I_Init ();

	Printf ("V_Init: allocate screen.\n");
	V_Init ();

	// Base systems have been inited; enable cvar callbacks
	FBaseCVar::EnableCallbacks ();

	Printf ("S_Init: Setting up sound.\n");
	S_Init ();

	Printf ("ST_Init: Init startup screen.\n");
	StartScreen = FStartupScreen::CreateInstance (R_GuesstimateNumTextures() + 5);

	Printf ("P_Init: Checking cmd-line parameters...\n");
	flags = dmflags;
	if (Args->CheckParm ("-nomonsters"))	flags |= DF_NO_MONSTERS;
	if (Args->CheckParm ("-respawn"))		flags |= DF_MONSTERS_RESPAWN;
	if (Args->CheckParm ("-fast"))			flags |= DF_FAST_MONSTERS;

	devparm = !!Args->CheckParm ("-devparm");

	if (Args->CheckParm ("-altdeath"))
	{
		deathmatch = 1;
		flags |= DF_ITEMS_RESPAWN;
	}
	else if (Args->CheckParm ("-deathmatch"))
	{
		deathmatch = 1;
		flags |= DF_WEAPONS_STAY | DF_ITEMS_RESPAWN;
	}

	dmflags = flags;

	// get skill / episode / map from parms
	if (gameinfo.gametype != GAME_Hexen)
	{
		strcpy (startmap, (gameinfo.flags & GI_MAPxx) ? "MAP01" : "E1M1");
	}
	else
	{
		strcpy (startmap, "&wt@01");
	}
	autostart = false;
				
	const char *val = Args->CheckValue ("-skill");
	if (val)
	{
		gameskill = val[0] - '1';
		autostart = true;
	}

	p = Args->CheckParm ("-warp");
	if (p && p < Args->NumArgs() - 1)
	{
		int ep, map;

		if (gameinfo.flags & GI_MAPxx)
		{
			ep = 1;
			map = atoi (Args->GetArg(p+1));
		}
		else 
		{
			ep = atoi (Args->GetArg(p+1));
			map = p < Args->NumArgs() - 2 ? atoi (Args->GetArg(p+2)) : 10;
			if (map < 1 || map > 9)
			{
				map = ep;
				ep = 1;
			}
		}

		strncpy (startmap, CalcMapName (ep, map), 8);
		autostart = true;
	}

	// [RH] Hack to handle +map
	p = Args->CheckParm ("+map");
	if (p && p < Args->NumArgs()-1)
	{
		MapData * map = P_OpenMapData(Args->GetArg (p+1));
		if (map == NULL)
		{
			Printf ("Can't find map %s\n", Args->GetArg (p+1));
		}
		else
		{
			delete map;
			strncpy (startmap, Args->GetArg (p+1), 8);
			Args->GetArg (p)[0] = '-';
			autostart = true;
		}
	}
	if (devparm)
	{
		Printf (GStrings("D_DEVSTR"));
	}

#ifndef unix
	// We do not need to support -cdrom under Unix, because all the files
	// that would go to c:\\zdoomdat are already stored in .zdoom inside
	// the user's home directory.
	if (Args->CheckParm("-cdrom"))
	{
		Printf (GStrings("D_CDROM"));
		mkdir (CDROM_DIR, 0);
	}
#endif

	// turbo option  // [RH] (now a cvar)
	{
		UCVarValue value;
		static char one_hundred[] = "100";

		value.String = Args->CheckValue ("-turbo");
		if (value.String == NULL)
			value.String = one_hundred;
		else
			Printf ("turbo scale: %s%%\n", value.String);

		turbo.SetGenericRepDefault (value, CVAR_String);
	}

	v = Args->CheckValue ("-timer");
	if (v)
	{
		double time = strtod (v, NULL);
		Printf ("Levels will end after %g minute%s.\n", time, time > 1 ? "s" : "");
		timelimit = (float)time;
	}

	v = Args->CheckValue ("-avg");
	if (v)
	{
		Printf ("Austin Virtual Gaming: Levels will end after 20 minutes\n");
		timelimit = 20.f;
	}

	//
	//  Build status bar line!
	//
	if (deathmatch)
		StartScreen->AppendStatusLine("DeathMatch...");
	if (dmflags & DF_NO_MONSTERS)
		StartScreen->AppendStatusLine("No Monsters...");
	if (dmflags & DF_MONSTERS_RESPAWN)
		StartScreen->AppendStatusLine("Respawning...");
	if (autostart)
	{
		FString temp;
		temp.Format ("Warp to map %s, Skill %d ", startmap, gameskill + 1);
		StartScreen->AppendStatusLine(temp);
	}

	// [RH] Now that all text strings are set up,
	// insert them into the level and cluster data.
	G_MakeEpisodes ();
	
	// [RH] Parse through all loaded mapinfo lumps
	Printf ("G_ParseMapInfo: Load map definitions.\n");
	G_ParseMapInfo ();

	// [RH] Parse any SNDINFO lumps
	Printf ("S_InitData: Load sound definitions.\n");
	S_InitData ();

	// [CW] Parse any TEAMINFO lumps
	Printf ("TEAMINFO_Init: Load team definitions.\n");
	TEAMINFO_Init ();

	FActorInfo::StaticInit ();

	// Now that all actors have been defined we can finally set up the weapon slots
	GameConfig->DoWeaponSetup (GameNames[gameinfo.gametype]);

	// [GRB] Initialize player class list
	SetupPlayerClasses ();

	// [RH] Load custom key and weapon settings from WADs
	D_LoadWadSettings ();

	// [GRB] Check if someone used clearplayerclasses but not addplayerclass
	if (PlayerClasses.Size () == 0)
	{
		I_FatalError ("No player classes defined");
	}

	FActorInfo::StaticGameSet ();
	StartScreen->Progress ();

	Printf ("R_Init: Init %s refresh subsystem.\n", GameNames[gameinfo.gametype]);
	StartScreen->LoadingStatus ("Loading graphics", 0x3f);
	R_Init ();

	Printf ("DecalLibrary: Load decals.\n");
	DecalLibrary.Clear ();
	DecalLibrary.ReadAllDecals ();

	// [RH] Try adding .deh and .bex files on the command line.
	// If there are none, try adding any in the config file.

	//if (gameinfo.gametype == GAME_Doom)
	{
		if (!ConsiderPatches ("-deh", ".deh") &&
			!ConsiderPatches ("-bex", ".bex") &&
			(gameinfo.gametype == GAME_Doom) &&
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
	static const char *startupString[5] = {
		"STARTUP1", "STARTUP2", "STARTUP3", "STARTUP4", "STARTUP5"
	};
	for (p = 0; p < 5; ++p)
	{
		const char *str = GStrings[startupString[p]];
		if (str != NULL && str[0] != '\0')
		{
			Printf ("%s\n", str);
		}
	}

	//Added by MC:
	bglobal.getspawned = Args->GatherFiles ("-bots", "", false);
	if (bglobal.getspawned->NumArgs() == 0)
	{
		bglobal.getspawned->Destroy();
		bglobal.getspawned = NULL;
	}
	else
	{
		bglobal.spawn_tries = 0;
		bglobal.wanted_botnum = bglobal.getspawned->NumArgs();
	}

	Printf ("M_Init: Init miscellaneous info.\n");
	M_Init ();

	Printf ("P_Init: Init Playloop state.\n");
	StartScreen->LoadingStatus ("Init game engine", 0x3f);
	P_Init ();

	//SBarInfo support.
	if(Wads.CheckNumForName("SBARINFO") != -1)
	{
		Printf ("ParseSBarInfo: Loading custom status bar definition.\n");
		SBarInfoScript = new SBarInfo(Wads.GetNumForName("SBARINFO")); //load last SBARINFO lump to avoid clashes
		atterm(FreeSBarInfoScript);
	}
	//end most of the SBarInfo stuff

	Printf ("D_CheckNetGame: Checking network game status.\n");
	StartScreen->LoadingStatus ("Checking network game status.", 0x3f);
	D_CheckNetGame ();

	// [RH] Lock any cvars that should be locked now that we're
	// about to begin the game.
	FBaseCVar::EnableNoSet ();

	// [RH] Run any saved commands from the command line or autoexec.cfg now.
	gamestate = GS_FULLCONSOLE;
	Net_NewMakeTic ();
	DThinker::RunThinkers ();
	gamestate = GS_STARTUP;

	// start the apropriate game based on parms
	v = Args->CheckValue ("-record");

	if (v)
	{
		G_RecordDemo (v);
		autostart = true;
	}

	delete StartScreen;
	StartScreen = NULL;
	V_Init2();

	files = Args->GatherFiles ("-playdemo", ".lmp", false);
	if (files->NumArgs() > 0)
	{
		singledemo = true;				// quit after one demo
		G_DeferedPlayDemo (files->GetArg (0));
		D_DoomLoop ();	// never returns
	}
	files->Destroy();

	v = Args->CheckValue ("-timedemo");
	if (v)
	{
		G_TimeDemo (v);
		D_DoomLoop ();	// never returns
	}
		
	v = Args->CheckValue ("-loadgame");
	if (v)
	{
		file = v;
		FixPathSeperator (file);
		DefaultExtension (file, ".zds");
		G_LoadGame (file);
	}

	if (gameaction != ga_loadgame)
	{
		if (autostart || netgame)
		{
			// Do not do any screenwipes when autostarting a game.
			NoWipe = 35;
			CheckWarpTransMap (startmap, true);
			if (demorecording)
				G_BeginRecording (startmap);
			G_InitNew (startmap, false);
		}
		else
		{
			D_StartTitle ();				// start up intro loop
		}
	}
	else if (demorecording)
	{
		G_BeginRecording (NULL);
	}
				
	atterm (D_QuitNetGame);		// killough

	D_DoomLoop ();		// never returns
}

//==========================================================================
//
// FStartupScreen Constructor
//
//==========================================================================

FStartupScreen::FStartupScreen(int max_progress)
{
	MaxPos = max_progress;
	CurPos = 0;
	NotchPos = 0;
}

//==========================================================================
//
// FStartupScreen Destructor
//
//==========================================================================

FStartupScreen::~FStartupScreen()
{
}

//==========================================================================
//
// FStartupScreen :: LoadingStatus
//
// Used by Heretic for the Loading Status "window."
//
//==========================================================================

void FStartupScreen::LoadingStatus(const char *message, int colors)
{
}

//==========================================================================
//
// FStartupScreen :: AppendStatusLine
//
// Used by Heretic for the "status line" at the bottom of the screen.
//
//==========================================================================

void FStartupScreen::AppendStatusLine(const char *status)
{
}

//==========================================================================
//
// STAT fps
//
// Displays statistics about rendering times
//
//==========================================================================

ADD_STAT (fps)
{
	FString out;
	out.Format("frame=%04.1f ms  walls=%04.1f ms  planes=%04.1f ms  masked=%04.1f ms",
		(double)FrameCycles * SecondsPerCycle * 1000,
		(double)WallCycles * SecondsPerCycle * 1000,
		(double)PlaneCycles * SecondsPerCycle * 1000,
		(double)MaskedCycles * SecondsPerCycle * 1000
		);
	return out;
}

//==========================================================================
//
// STAT wallcycles
//
// Displays the minimum number of cycles spent drawing walls
//
//==========================================================================

static cycle_t bestwallcycles = INT_MAX;

ADD_STAT (wallcycles)
{
	FString out;
	if (WallCycles && WallCycles < bestwallcycles)
		bestwallcycles = WallCycles;
	out.Format ("%llu", bestwallcycles);
	return out;
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

#if 1
// To use these, also uncomment the clock/unclock in wallscan
static cycle_t bestscancycles = INT_MAX;

ADD_STAT (scancycles)
{
	FString out;
	if (WallScanCycles && WallScanCycles < bestscancycles)
		bestscancycles = WallScanCycles;
	out.Format ("%llu", bestscancycles);
	return out;
}

CCMD (clearscancycles)
{
	bestscancycles = INT_MAX;
}
#endif
