// Emacs style mode select   -*- C++ -*- 
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
//      Put all global state variables here.
//
//-----------------------------------------------------------------------------



#include "doomstat.h"
#include "c_cvars.h"


// Game Mode - identify IWAD as shareware, retail etc.
GameMode_t		gamemode = undetermined;
GameMission_t	gamemission = doom;

// Language.
Language_t		language = english;

// Set if homebrew PWAD stuff has been added.
BOOL			modifiedgame;

// Show developer messages if true.
cvar_t			*developer;

// [RH] Feature control cvars
cvar_t *boom_friction, *boom_pushers;

// [RH] Deathmatch flags
cvar_t			*dmflagsvar;
int				dmflags;		// Copy of dmflagsvar->value, but as an integer.
cvar_t			*fakedmatch;