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
// DESCRIPTION:
//	 All the global variables that store the internal state.
//	 Theoretically speaking, the internal state of the engine
//	  should be found by looking at the variables collected
//	  here, and every relevant module will have to include
//	  this header file.
//	 In practice, things are a bit messy.
//
//-----------------------------------------------------------------------------


#ifndef __D_STATE__
#define __D_STATE__

// We need globally shared data structures,
//	for defining the global state variables.
// We need the player data structure as well.
//#include "d_player.h"

#include "doomdata.h"
#include "d_net.h"
#include "g_level.h"

// We also need the definition of a cvar
#include "c_cvars.h"




// ------------------------
// Command line parameters.
//
extern	BOOL			devparm;		// DEBUG: launched with -devparm



// -----------------------------------------------------
// Game Mode - identify IWAD as shareware, retail etc.
//
extern GameMode_t		gamemode;
extern GameMission_t	gamemission;

// Set if homebrew PWAD stuff has been added.
extern	BOOL			modifiedgame;


// -------------------------------------------
// Language.
extern	Language_t		language;


// -------------------------------------------
// Selected skill type, map etc.
//

extern	char			startmap[8];		// [RH] Actual map name now

extern	BOOL 			autostart;

// Selected by user. 
EXTERN_CVAR (gameskill)

// Nightmare mode flag, single player.
extern	BOOL 			respawnmonsters;

// Netgame? Only true if >1 player.
extern	BOOL			netgame;

// Bot game? Like netgame, but doesn't involve network communication.
extern	BOOL			multiplayer;

// Flag: true only if started as net deathmatch.
EXTERN_CVAR (deathmatch)

// [RH] Pretend as deathmatch for purposes of dmflags
EXTERN_CVAR (alwaysapplydmflags)

// [RH] Teamplay mode
EXTERN_CVAR (teamplay)
		
// -------------------------
// Internal parameters for sound rendering.
// These have been taken from the DOS version,
//	but are not (yet) supported with Linux
//	(e.g. no sound volume adjustment with menu.

// These are not used, but should be (menu).
// From m_menu.c:
//	Sound FX volume has default, 0 - 15
//	Music volume has default, 0 - 15
// These are multiplied by 8.
EXTERN_CVAR (snd_sfxvolume)				// maximum volume for sound
EXTERN_CVAR (snd_musicvolume)			// maximum volume for music


// -------------------------
// Status flags for refresh.
//

// Depending on view size - no status bar?
// Note that there is no way to disable the
//	status bar explicitely.
extern	BOOL			statusbaractive;

extern	BOOL			automapactive;	// In AutoMap mode?
extern	BOOL			menuactive; 	// Menu overlayed?
extern	BOOL			paused; 		// Game Pause?


extern	BOOL			viewactive;

extern	BOOL	 		nodrawers;
extern	BOOL	 		noblit;

extern	int 			viewwindowx;
extern	int 			viewwindowy;
extern	"C" int 		viewheight;
extern	"C" int 		viewwidth;

extern	"C" int			realviewwidth;		// [RH] Physical width of view window
extern	"C" int			realviewheight;		// [RH] Physical height of view window
extern	"C" int			detailxshift;		// [RH] X shift for horizontal detail level
extern	"C" int			detailyshift;		// [RH] Y shift for vertical detail level





// This one is related to the 3-screen display mode.
// ANG90 = left side, ANG270 = right
extern	int				viewangleoffset;

// Player taking events, and displaying.
extern	int				consoleplayer;	
extern	int				displayplayer;


extern level_locals_t level;


// --------------------------------------
// DEMO playback/recording related stuff.
// No demo, there is a human player in charge?
// Disable save/end game?
extern	BOOL			usergame;

extern	BOOL			demoplayback;
extern	BOOL			demorecording;
extern	int				demover;

// Quit after playing a demo from cmdline.
extern	BOOL			singledemo; 	




extern	gamestate_t 	gamestate;






//-----------------------------
// Internal parameters, fixed.
// These are set by the engine, and not changed
//	according to user inputs. Partly load from
//	WAD, partly set at startup time.



extern	int 			gametic;


// Alive? Disconnected?
extern	bool	 		playeringame[MAXPLAYERS];


// Player spawn spots for deathmatch.
extern	int				MaxDeathmatchStarts;
extern	mapthing2_t		*deathmatchstarts;
extern	mapthing2_t*	deathmatch_p;

// Player spawn spots.
extern	mapthing2_t		playerstarts[MAXPLAYERS];

// Intermission stats.
// Parameters for world map / intermission.
extern	struct wbstartstruct_s wminfo; 


// LUT of ammunition limits for each kind.
// This doubles with BackPack powerup item.
extern	int 			maxammo[NUMAMMO];





//-----------------------------------------
// Internal parameters, used for engine.
//

// File handling stuff.
extern	FILE*			debugfile;

// if true, load all graphics at level load
extern	BOOL	 		precache;


//-------
//REFRESH
//-------

// wipegamestate can be set to -1
//	to force a wipe on the next draw
extern gamestate_t wipegamestate;
extern BOOL setsizeneeded;
extern BOOL setmodeneeded;

extern BOOL BorderNeedRefresh;
extern BOOL BorderTopRefresh;


EXTERN_CVAR (mouse_sensitivity)
//?
// debug flag to cancel adaptiveness
extern	BOOL	 		singletics; 	

extern	int 			bodyqueslot;



// Needed to store the number of the dummy sky flat.
// Used for rendering,
//	as well as tracking projectiles etc.
extern int				skyflatnum;



// Netgame stuff (buffers and pointers, i.e. indices).

// This is ???
extern	doomcom_t*		doomcom;

// This points inside doomcom.
extern	doomdata_t* 	netbuffer;		


extern	struct ticcmd_t		localcmds[BACKUPTICS];

extern	int 			maketic;
extern	int 			nettics[MAXNETNODES];

extern	ticcmd_t		netcmds[MAXPLAYERS][BACKUPTICS];
extern	int 			ticdup;


// ---- [RH] ----
EXTERN_CVAR (developer)

extern int Net_Arbitrator;

// Use MMX routines? (Only if USEASM is defined)
extern	BOOL			UseMMX;


#ifdef USEASM
extern "C" void EndMMX (void);

#ifdef _MSC_VER
#define ENDMMX if (UseMMX) __asm emms;
#else
#define ENDMMX if (UseMMX) EndMMX();
#endif

#endif

EXTERN_CVAR (var_friction)
EXTERN_CVAR (var_pushers)


// [RH] Miscellaneous info for DeHackEd support
struct DehInfo
{
	int StartHealth;
	int StartBullets;
	int MaxHealth;
	int MaxArmor;
	int GreenAC;
	int BlueAC;
	int MaxSoulsphere;
	int SoulsphereHealth;
	int MegasphereHealth;
	int GodHealth;
	int FAArmor;
	int FAAC;
	int KFAArmor;
	int KFAAC;
	int BFGCells;
	int Infight;
};
extern struct DehInfo deh;

// [RH] Deathmatch flags

extern int dmflags;

#endif
