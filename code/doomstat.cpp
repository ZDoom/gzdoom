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



#include "stringtable.h"
#include "doomstat.h"
#include "c_cvars.h"
#include "i_system.h"
#include "g_level.h"
#include "p_local.h"
#include "p_acs.h"

// Localizable strings
FStringTable	GStrings;

// Game speed
EGameSpeed		GameSpeed = SPEED_Normal;

// Game Mode - identify IWAD as shareware, retail etc.
GameMode_t		gamemode = undetermined;
GameMission_t	gamemission = doom;

// Show developer messages if true.
CVAR (Bool, developer, false, 0)

// [RH] Feature control cvars
CVAR (Bool, var_friction, true, CVAR_SERVERINFO);
CVAR (Bool, var_pushers, true, CVAR_SERVERINFO);

CVAR (Bool, alwaysapplydmflags, false, CVAR_SERVERINFO);
CVAR (Float, teamdamage, 0.f, CVAR_SERVERINFO);

CUSTOM_CVAR (String, language, "auto", CVAR_ARCHIVE)
{
	SetLanguageIDs ();
	if (level.behavior != NULL)
	{
		level.behavior->PrepLocale (LanguageIDs[0], LanguageIDs[1],
			LanguageIDs[2], LanguageIDs[3]);
	}
	GStrings.ReloadStrings ();
	GStrings.Compact ();
	G_SetLevelStrings ();
}

// [RH] Network arbitrator
int Net_Arbitrator = 0;