//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1999-2016 Randy Heit
// Copyright 2002-2016 Christoph Oelckers
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------
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

#include "c_cvars.h"

// -----------------------
// Game speed.
//
enum EGameSpeed
{
	SPEED_Normal,
	SPEED_Fast,
};
extern EGameSpeed GameSpeed;


// ------------------------
// Command line parameters.
//
extern	bool			devparm;		// DEBUG: launched with -devparm



// -----------------------------------------------------
// Game Mode - identify IWAD as shareware, retail etc.
//
// -------------------------------------------
// Selected skill type, map etc.
//

extern	FString			startmap;			// [RH] Actual map name now

extern	bool 			autostart;

extern	FString			StoredWarp;			// [RH] +warp at the command line

// Selected by user. 
EXTERN_CVAR (Int, gameskill);
extern	int				NextSkill;			// [RH] Skill to use at next level load

// Netgame? Only true if >1 player.
extern	bool			netgame;

// Bot game? Like netgame, but doesn't involve network communication.
extern	bool			multiplayer;

// [SP] Map dm/coop implementation - invokes fake multiplayer without bots
extern	bool			multiplayernext;

// Flag: true only if started as net deathmatch.
EXTERN_CVAR (Int, deathmatch)

// [RH] Pretend as deathmatch for purposes of dmflags
EXTERN_CVAR (Bool, alwaysapplydmflags)

// [RH] Teamplay mode
EXTERN_CVAR (Bool, teamplay)

// [RH] Friendly fire amount
EXTERN_CVAR (Float, teamdamage)

// [RH] The class the player will spawn as in single player,
// in case using a random class with Hexen.
extern int SinglePlayerClass[/*MAXPLAYERS*/];

// -------------------------
// Internal parameters for sound rendering.

EXTERN_CVAR (Float, snd_mastervolume)	// maximum master volume
EXTERN_CVAR (Float, snd_sfxvolume)		// maximum volume for sound
EXTERN_CVAR (Float, snd_musicvolume)	// maximum volume for music


// -------------------------
// Status flags for refresh.
//

enum EMenuState : int
{
	MENU_Off,			// Menu is closed
	MENU_On,			// Menu is opened
	MENU_WaitKey,		// Menu is opened and waiting for a key in the controls menu
	MENU_OnNoPause,		// Menu is opened but does not pause the game
};

extern	bool			automapactive;	// In AutoMap mode?
extern	EMenuState		menuactive; 	// Menu overlayed?
extern	int				paused; 		// Game Pause?
extern	bool			pauseext;


extern	bool			viewactive;

extern	bool	 		nodrawers;
extern	bool	 		noblit;

extern	int 			viewwindowx;
extern	int 			viewwindowy;
extern	int 			viewheight;
extern	int		 		viewwidth;





// Player taking events. i.e. The local player.
extern	int				consoleplayer;	


// --------------------------------------
// DEMO playback/recording related stuff.
// No demo, there is a human player in charge?
// Disable save/end game?
extern	bool			usergame;

extern	FString			newdemoname;
extern	FString			newdemomap;
extern	bool			demoplayback;
extern	bool			demorecording;
extern	int				demover;

// Quit after playing a demo from cmdline.
extern	bool			singledemo; 	

extern	int				SaveVersion;




//-----------------------------
// Internal parameters, fixed.
// These are set by the engine, and not changed
//	according to user inputs. Partly load from
//	WAD, partly set at startup time.



extern	int 			gametic;


// Alive? Disconnected?
extern	bool	 		playeringame[/*MAXPLAYERS*/];


//-----------------------------------------
// Internal parameters, used for engine.
//

// File handling stuff.
extern	FILE*			debugfile;
extern	FILE*			hashfile;

// if true, load all graphics at level load
extern	bool	 		precache;


//-------
//REFRESH
//-------

extern bool setsizeneeded;


EXTERN_CVAR (Float, mouse_sensitivity)
//?
// debug flag to cancel adaptiveness
extern	bool	 		singletics; 	



// Needed to store the number of the dummy sky flat.
// Used for rendering,
//	as well as tracking projectiles etc.



// ---- [RH] ----
EXTERN_CVAR (Int, developer)

extern bool ToggleFullscreen;

extern int Net_Arbitrator;

EXTERN_CVAR (Bool, var_friction)


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
	char PlayerSprite[5];
	uint8_t ExplosionStyle;
	double ExplosionAlpha;
	int NoAutofreeze;
	int BFGCells;
};
extern DehInfo deh;
EXTERN_CVAR (Int, infighting)

// [RH] Deathmatch flags

EXTERN_CVAR (Int, dmflags);
EXTERN_CVAR (Int, dmflags2);	// [BC]

EXTERN_CVAR (Int, compatflags);
EXTERN_CVAR (Int, compatflags2);

// Filters from AddAutoloadFiles(). Used to filter files from archives.
extern FString LumpFilterIWAD;

#endif
