#pragma once

#include "doomtype.h"
#include "vectors.h"
#include "sc_man.h"
#include "resourcefiles/file_zip.h"
#include "g_mapinfo.h"


extern bool savegamerestore;

void G_InitNew (const char *mapname, bool bTitleLevel);

// Can be called by the startup code or M_Responder.
// A normal game starts at map 1,
// but a warp test can start elsewhere
void G_DeferedInitNew (const char *mapname, int skill = -1);
struct FGameStartup;
void G_DeferedInitNew (FGameStartup *gs);

enum 
{
	CHANGELEVEL_KEEPFACING = 1,
	CHANGELEVEL_RESETINVENTORY = 2,
	CHANGELEVEL_NOMONSTERS = 4,
	CHANGELEVEL_CHANGESKILL = 8,
	CHANGELEVEL_NOINTERMISSION = 16,
	CHANGELEVEL_RESETHEALTH = 32,
	CHANGELEVEL_PRERAISEWEAPON = 64,
};

void G_DoLoadLevel (const FString &MapName, int position, bool autosave, bool newGame);

void G_ClearSnapshots (void);
void P_RemoveDefereds ();
void G_ReadSnapshots (FResourceFile *);
void G_WriteSnapshots (TArray<FString> &, TArray<FCompressedBuffer> &);
void G_WriteVisited(FSerializer &arc);
void G_ReadVisited(FSerializer &arc);
void G_ClearHubInfo();

