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

#ifdef __GNUG__
#pragma interface
#endif



// ------------------------
// Command line parameters.
//
extern	boolean nomonsters; 	// checkparm of -nomonsters
extern	boolean respawnparm;	// checkparm of -respawn
extern	int 			fastparm;		// checkparm of -fast

extern	boolean devparm;		// DEBUG: launched with -devparm



// -----------------------------------------------------
// Game Mode - identify IWAD as shareware, retail etc.
//
extern GameMode_t		gamemode;
extern GameMission_t	gamemission;

// Set if homebrew PWAD stuff has been added.
extern	boolean modifiedgame;


// -------------------------------------------
// Language.
extern	Language_t	 language;


// -------------------------------------------
// Selected skill type, map etc.
//

// Defaults for menu, methinks.
extern	skill_t 		startskill;
extern	int 			startepisode;
extern	int 			startmap;

extern	boolean 		autostart;

// Selected by user. 
extern	cvar_t			*gameskill;

// Nightmare mode flag, single player.
extern	boolean 		respawnmonsters;

// Netgame? Only true if >1 player.
extern	boolean netgame;

// Flag: true only if started as net deathmatch.
// An enum might handle altdeath/cooperative better.
extern	boolean deathmatch; 	
		
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
extern	boolean statusbaractive;

extern	boolean automapactive;	// In AutoMap mode?
extern	boolean menuactive; 	// Menu overlayed?
extern	boolean paused; 		// Game Pause?


extern	boolean 		viewactive;

extern	boolean 		nodrawers;
extern	boolean 		noblit;

extern	int 			viewwindowx;
extern	int 			viewwindowy;
extern	int 			viewheight;
extern	int 			viewwidth;
extern	int 			scaledviewwidth;






// This one is related to the 3-screen display mode.
// ANG90 = left side, ANG270 = right
extern	int 	viewangleoffset;

// Player taking events, and displaying.
extern	int 	consoleplayer;	
extern	int 	displayplayer;


extern level_locals_t level;


// --------------------------------------
// DEMO playback/recording related stuff.
// No demo, there is a human player in charge?
// Disable save/end game?
extern	boolean usergame;

//?
extern	boolean demoplayback;
extern	boolean demorecording;

// Quit after playing a demo from cmdline.
extern	boolean 		singledemo; 	




//?
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
extern	boolean 		playeringame[MAXPLAYERS];


// Player spawn spots for deathmatch.
extern	int				MaxDeathmatchStarts;
extern	mapthing_t		*deathmatchstarts;
extern	mapthing_t* 	deathmatch_p;

// Player spawn spots.
extern	mapthing_t		playerstarts[MAXPLAYERS];

// Intermission stats.
// Parameters for world map / intermission.
extern	wbstartstruct_t 		wminfo; 


// LUT of ammunition limits for each kind.
// This doubles with BackPack powerup item.
extern	int 			maxammo[NUMAMMO];





//-----------------------------------------
// Internal parameters, used for engine.
//

// File handling stuff.
extern	FILE*			debugfile;

// if true, load all graphics at level load
extern	boolean 		precache;


// wipegamestate can be set to -1
//	to force a wipe on the next draw
extern	gamestate_t 	wipegamestate;

extern	cvar_t 			*mouseSensitivity;
//?
// debug flag to cancel adaptiveness
extern	boolean 		singletics; 	

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
extern	int 			rndindex;

extern	int 			maketic;
extern	int 			nettics[MAXNETNODES];

extern	ticcmd_t		netcmds[MAXPLAYERS][BACKUPTICS];
extern	int 			ticdup;


// ---- [RH] ----
extern	cvar_t			*developer;
// Handy macro to print a string if developer cvar is set
#define DEVONLY(f,a,b,c) if (developer->value) { f (a,b,c); }


// Use MMX routines? (Only if USEASM is defined)
extern	boolean			UseMMX;

#ifdef USEASM
void EndMMX (void);
#endif


// Miscellaneous info for DeHackEd support

extern int deh_StartHealth;
extern int deh_StartBullets;
extern int deh_MaxHealth;
extern int deh_MaxArmor;
extern int deh_GreenAC;
extern int deh_BlueAC;
extern int deh_MaxSoulsphere;
extern int deh_SoulsphereHealth;
extern int deh_MegasphereHealth;
extern int deh_GodHealth;
extern int deh_FAArmor;
extern int deh_FAAC;
extern int deh_KFAArmor;
extern int deh_KFAAC;
extern int deh_BFGCells;
extern int deh_Infight;


#endif
//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
