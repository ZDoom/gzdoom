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
//		The not so system specific sound interface.
//
//-----------------------------------------------------------------------------


#ifndef __S_SOUND__
#define __S_SOUND__

#include "i_soundinternal.h"

class AActor;
class FScanner;
class FSerializer;
struct FLevelLocals;

#include "s_soundinternal.h"
#include "s_doomsound.h"
#include "name.h"
#include "tarray.h"

enum SndUserFlags
{
	SND_PlayerReserve = 1,
	SND_PlayerCompat = 2,
	SND_PlayerSilent = 4
};

enum // This cannot be remain as this, but for now it has to suffice.
{
	SOURCE_Actor = SOURCE_None+1,		// Sound is coming from an actor.
	SOURCE_Sector,		// Sound is coming from a sector.
	SOURCE_Polyobj,		// Sound is coming from a polyobject.
};

// Per level startup code.
// Kills playing sounds at start of level and starts new music.
//
typedef TMap<FName, FName> MusicAliasMap;
extern MusicAliasMap MusicAliases;

// Called after a level is loaded. Ensures that most sounds are loaded.

// [RH] S_sfx "maintenance" routines
void S_ClearSoundData();
void S_ParseSndInfo (bool redefine);

bool S_AreSoundsEquivalent (AActor *actor, int id1, int id2);
bool S_AreSoundsEquivalent (AActor *actor, const char *name1, const char *name2);
int S_LookupPlayerSound (const char *playerclass, int gender, const char *logicalname);
int S_LookupPlayerSound (const char *playerclass, int gender, FSoundID refid);
int S_FindSkinnedSound (AActor *actor, FSoundID refid);
int S_FindSkinnedSoundEx (AActor *actor, const char *logicalname, const char *extendedname);
int S_AddSound (const char *logicalname, const char *lumpname, FScanner *sc=NULL);	// Add sound by lumpname
int S_AddPlayerSound (const char *playerclass, const int gender, int refid, const char *lumpname);
int S_AddPlayerSound (const char *playerclass, const int gender, int refid, int lumpnum, bool fromskin=false);
int S_AddPlayerSoundExisting (const char *playerclass, const int gender, int refid, int aliasto, bool fromskin=false);
void S_MarkPlayerSounds (AActor *player);
void S_ShrinkPlayerSoundLists ();
unsigned int S_GetMSLength(FSoundID sound);

// [RH] Prints sound debug info to the screen.
//		Modelled after Hexen's noise cheat.


#endif
