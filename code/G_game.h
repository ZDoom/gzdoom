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
//	 Duh.
// 
//-----------------------------------------------------------------------------


#ifndef __G_GAME__
#define __G_GAME__

#include "doomdef.h"
#include "d_event.h"



//
// GAME
//
void G_DeathMatchSpawnPlayer (int playernum);

void G_DeferedPlayDemo (char* demo);

// Can be called by the startup code or M_Responder,
// calls P_SetupLevel or W_EnterWorld.
void G_LoadGame (char* name);

void G_DoLoadGame (void);

// Called by M_Responder.
void G_BuildSaveName (char *name, int slot);
void G_SaveGame (int slot, char* description);

// Only called by startup code.
void G_RecordDemo (char* name);

void G_BeginRecording (void);

void G_PlayDemo (char* name);
void G_TimeDemo (char* name);
BOOL G_CheckDemoStatus (void);

void G_WorldDone (void);

void G_Ticker (void);
BOOL G_Responder (event_t*	ev);

void G_ScreenShot (char *filename);

void G_PlayerFinishLevel (int player);

#define BODYQUESIZE 	32
struct mobj_s;
extern struct mobj_s *bodyque[BODYQUESIZE]; 
extern int bodyqueslot; 


#endif
