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

#include "doomtype.h"
#include "i_soundinternal.h"

class AActor;
class FScanner;
class FSerializer;
struct FLevelLocals;

#include "s_soundinternal.h"

// Per level startup code.
// Kills playing sounds at start of level and starts new music.
//
void S_Start ();

// Called after a level is loaded. Ensures that most sounds are loaded.
void S_PrecacheLevel (FLevelLocals *l);

// Start sound for thing at <ent>
void S_Sound (int channel, FSoundID sfxid, float volume, float attenuation);

void S_SoundPitch (int channel, FSoundID sfxid, float volume, float attenuation, float pitch);

struct FSoundLoadBuffer;
int S_PickReplacement (int refid);
void S_CacheRandomSound (sfxinfo_t *sfx);

// Stop sound for all channels
void S_StopAllChannels (void);
void S_SetPitch(FSoundChan *chan, float dpitch);

void S_RestoreEvictedChannels();

// [RH] S_sfx "maintenance" routines
void S_ClearSoundData();
void S_ParseSndInfo (bool redefine);

void S_HashSounds ();
int S_FindSoundNoHash (const char *logicalname);
bool S_AreSoundsEquivalent (AActor *actor, int id1, int id2);
bool S_AreSoundsEquivalent (AActor *actor, const char *name1, const char *name2);
int S_LookupPlayerSound (const char *playerclass, int gender, const char *logicalname);
int S_LookupPlayerSound (const char *playerclass, int gender, FSoundID refid);
int S_FindSkinnedSound (AActor *actor, FSoundID refid);
int S_FindSkinnedSoundEx (AActor *actor, const char *logicalname, const char *extendedname);
int S_FindSoundByLump (int lump);
int S_AddSound (const char *logicalname, const char *lumpname, FScanner *sc=NULL);	// Add sound by lumpname
int S_AddSoundLump (const char *logicalname, int lump);	// Add sound by lump index
int S_AddPlayerSound (const char *playerclass, const int gender, int refid, const char *lumpname);
int S_AddPlayerSound (const char *playerclass, const int gender, int refid, int lumpnum, bool fromskin=false);
int S_AddPlayerSoundExisting (const char *playerclass, const int gender, int refid, int aliasto, bool fromskin=false);
void S_MarkPlayerSounds (AActor *player);
void S_ShrinkPlayerSoundLists ();
void S_UnloadSound (sfxinfo_t *sfx);
sfxinfo_t *S_LoadSound(sfxinfo_t *sfx, FSoundLoadBuffer *pBuffer = nullptr);
unsigned int S_GetMSLength(FSoundID sound);

// [RH] Prints sound debug info to the screen.
//		Modelled after Hexen's noise cheat.
void S_NoiseDebug ();

#include "s_doomsound.h"

#endif
