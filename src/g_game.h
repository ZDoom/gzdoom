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
//	 Duh.
// 
//-----------------------------------------------------------------------------


#ifndef __G_GAME__
#define __G_GAME__

struct event_t;

#include "dobjgc.h"
#include "name.h"
#include "gamestate.h"


// wipegamestate can be set to -1
//	to force a wipe on the next draw
extern gamestate_t wipegamestate;



class AActor;
struct FLevelLocals;

//
// GAME
//
enum
{
	PPS_FORCERANDOM			= 1,
	PPS_NOBLOCKINGCHECK		= 2,
};

void G_DeferedPlayDemo (const char* demo);

// Can be called by the startup code or M_Responder,
// calls P_SetupLevel or W_EnterWorld.
void G_LoadGame (const char* name, bool hidecon=false);

void G_DoLoadGame (void);

// Called by M_Responder.
void G_SaveGame (const char *filename, const char *description);
// Called by messagebox
void G_DoQuickSave ();

// Only called by startup code.
void G_RecordDemo (const char* name);

void G_BeginRecording (const char *startmap);

void G_PlayDemo (char* name);
void G_TimeDemo (const char* name);
bool G_CheckDemoStatus (void);

void G_Ticker (void);
bool G_Responder (event_t*	ev);

void G_ScreenShot (const char* filename);
void G_StartSlideshow(FLevelLocals *Level, FName whichone);

class FSerializer;
bool G_CheckSaveGameWads (FSerializer &arc, bool printwarn);

enum EFinishLevelType
{
	FINISH_SameHub,
	FINISH_NextHub,
	FINISH_NoHub
};

void G_PlayerFinishLevel (int player, EFinishLevelType mode, int flags);

void G_DoPlayerPop(int playernum);

// Adds pitch to consoleplayer's viewpitch and clamps it
void G_AddViewPitch (int look, bool mouse = false);

// Adds to consoleplayer's viewangle if allowed
void G_AddViewAngle (int yaw, bool mouse = false);

class FBaseCVar;
FBaseCVar* G_GetUserCVar(int playernum, const char* cvarname);

class DIntermissionController;
struct level_info_t;
void RunIntermission(level_info_t* oldlevel, level_info_t* newlevel, DIntermissionController* intermissionScreen, DObject* statusScreen, std::function<void(bool)> completionf);

extern const AActor *SendItemUse, *SendItemDrop;
extern int SendItemDropAmount;


#endif
