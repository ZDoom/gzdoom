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
#include "doomdata.h"
#include "d_net.h"
#include "g_level.h"

// We need the player data structure as well.
#include "d_player.h"

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
extern	cvar_t			*gameskill;

// Nightmare mode flag, single player.
extern	BOOL 			respawnmonsters;

// Netgame? Only true if >1 player.
extern	BOOL			netgame;

// Flag: true only if started as net deathmatch.
extern	cvar_t			*deathmatch;

// [RH] Teamplay mode
extern	cvar_t			*teamplay;
		
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
extern cvar_t *snd_SfxVolume;	   // maximum volume for sound
extern cvar_t *snd_MusicVolume;    // maximum volume for music


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
extern	int 			viewheight;
extern	int 			viewwidth;

extern	int				realviewwidth;		// [RH] Physical width of view window
extern	int				realviewheight;		// [RH] Physical height of view window
extern	int				detailxshift;		// [RH] X shift for horizontal detail level
extern	int				detailyshift;		// [RH] Y shift for vertical detail level





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


// Bookkeeping on players - state.
extern	player_t		players[MAXPLAYERS];

// Alive? Disconnected?
extern	BOOL	 		playeringame[MAXPLAYERS];


// Player spawn spots for deathmatch.
extern	int				MaxDeathmatchStarts;
extern	mapthing2_t		*deathmatchstarts;
extern	mapthing2_t* 	deathmatch_p;

// Player spawn spots.
extern	mapthing2_t		playerstarts[MAXPLAYERS];

// Intermission stats.
// Parameters for world map / intermission.
extern	wbstartstruct_t wminfo; 


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


extern	cvar_t 			*mouseSensitivity;
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


extern	ticcmd_t		localcmds[BACKUPTICS];

extern	int 			maketic;
extern	int 			nettics[MAXNETNODES];

extern	ticcmd_t		netcmds[MAXPLAYERS][BACKUPTICS];
extern	int 			ticdup;


// ---- [RH] ----
extern	cvar_t			*developer;

// Use MMX routines? (Only if USEASM is defined)
extern	BOOL			UseMMX;


#ifdef USEASM
void EndMMX (void);

#ifdef _MSC_VER
#define ENDMMX if (UseMMX) __asm emms;
#else
#define ENDMMX if (UseMMX) EndMMX();
#endif

#endif

extern cvar_t *boom_friction;
extern cvar_t *boom_pushers;


// [RH] Miscellaneous info for DeHackEd support
struct DehInfo {
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

extern cvar_t *dmflagsvar;
extern int	   dmflags;

// [RH] Generic information about the current game

struct gameinfo_s
{
	char *gameTitle;
	BOOL pagesArePatches;
	char *titlePage;
	char *demoPage1;
	char *demoPage2;
	char *advisory;
	float titleTime;
	float demoPage1Time;
	float demoPage2Time;
	float advisoryTime;
	char *titleSong;
	char *skyFlatName;
};
typedef struct gameinfo_s gameinfo_t;

extern gameinfo_t gameinfo;

#endif
//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
