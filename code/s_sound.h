// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id: s_sound.h,v 1.1.1.1 1997/12/28 12:59:03 pekangas Exp $
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
//		The not so system specific sound interface.
//
//-----------------------------------------------------------------------------


#ifndef __S_SOUND__
#define __S_SOUND__

#include "m_fixed.h"


#define MAX_SNDNAME	63

class AActor;

//
// SoundFX struct.
//
struct sfxinfo_t
{
	char		name[MAX_SNDNAME+1];	// [RH] Sound name defined in SNDINFO
	int 		lumpnum;				// lump number of sfx

	// Next five fields are for use by i_sound.cpp. A non-null data means the
	// sound has been loaded. The other fields are dependant on whether MIDAS
	// or FMOD is used for sound.
	void*		data;
	void*		altdata;
	long		normal;
	long		looping;
	WORD		bHaveLoop:1;

	WORD		bRandomHeader:1;
	WORD		bPlayerReserve:1;

	WORD		link;

	enum { NO_LINK = 0xffff };

	// this is checked every second to see if sound
	// can be thrown out (if 0, then decrement, if -1,
	// then throw out, if > 0, then it is in use)
	int 		usefulness;

	unsigned int ms;					// [RH] length of sfx in milliseconds
	unsigned int next, index;			// [RH] For hashing
	unsigned int frequency;				// [RH] Preferred playback rate
	unsigned int length;				// [RH] Length of the sound in samples
};

// the complete set of sound effects
extern TArray<sfxinfo_t> S_sfx;

// Initializes sound stuff, including volume
// Sets channels, SFX and music volume,
//	allocates channel buffer, sets S_sfx lookup.
//
void S_Init (int sfxVolume, int musicVolume);

// Per level startup code.
// Kills playing sounds at start of level,
//	determines music if any, changes music.
//
void S_Start ();

// Start sound for thing at <ent>
void S_Sound (int channel, const char *name, float volume, int attenuation);
void S_Sound (AActor *ent, int channel, const char *name, float volume, int attenuation);
void S_Sound (fixed_t *pt, int channel, const char *name, float volume, int attenuation);
void S_Sound (fixed_t x, fixed_t y, int channel, const char *name, float volume, int attenuation);
void S_LoopedSound (AActor *ent, int channel, const char *name, float volume, int attenuation);
void S_LoopedSound (fixed_t *pt, int channel, const char *name, float volume, int attenuation);
void S_SoundID (int channel, int sfxid, float volume, int attenuation);
void S_SoundID (AActor *ent, int channel, int sfxid, float volume, int attenuation);
void S_SoundID (fixed_t *pt, int channel, int sfxid, float volume, int attenuation);
void S_LoopedSoundID (AActor *ent, int channel, int sfxid, float volume, int attenuation);
void S_LoopedSoundID (fixed_t *pt, int channel, int sfxid, float volume, int attenuation);

// Returns true if the max of the named sound are already playing.
// Does not handle player sounds.
bool S_CheckSound (const char *name);
bool S_CheckSound (int sfxid);

// sound channels
// channel 0 never willingly overrides
// other channels (1-7) always override a playing sound on that channel
#define CHAN_AUTO				0
#define CHAN_WEAPON				1
#define CHAN_VOICE				2
#define CHAN_ITEM				3
#define CHAN_BODY				4
// modifier flags
#define CHAN_LISTENERZ			8
#define CHAN_IMMOBILE			16


// sound attenuation values
#define ATTN_NONE				0	// full volume the entire level
#define ATTN_NORM				1
#define ATTN_IDLE				2
#define ATTN_STATIC				3	// diminish very rapidly with distance
#define ATTN_SURROUND			4	// like ATTN_NONE, but plays in surround sound

int S_PickReplacement (int refid);

// [RH] From Hexen.
//		Prevents too many of the same sound from playing simultaneously.
BOOL S_StopSoundID (int sound_id, int priority);

// Stops a sound emanating from one of an entity's channels
void S_StopSound (AActor *ent, int channel);
void S_StopSound (fixed_t *pt, int channel);

// Stop sound for all channels
void S_StopAllChannels (void);

// Is the sound playing on one of the entity's channels?
bool S_GetSoundPlayingInfo (AActor *ent, int sound_id);
bool S_GetSoundPlayingInfo (fixed_t *pt, int sound_id);

// Moves all sounds from one mobj to another
void S_RelinkSound (AActor *from, AActor *to);

// Start music using <music_name>
bool S_StartMusic (char *music_name);

// Start music using <music_name>, and set whether looping
bool S_ChangeMusic (const char *music_name, int order=0, bool looping=true, bool force=false);

// Start playing a cd track as music
bool S_ChangeCDMusic (int track, unsigned int id=0, bool looping=true);

void S_RestartMusic ();

int S_GetMusic (char **name);

// Stops the music for sure.
void S_StopMusic (bool force);

// Stop and resume music, during game PAUSE.
void S_PauseSound ();
void S_ResumeSound ();

//
// Updates music & sounds
//
void S_UpdateSounds (void *listener);

void S_SetMusicVolume (int volume);
void S_SetSfxVolume (int volume);


// [RH] S_sfx "maintenance" routines
void S_ParseSndInfo ();

void S_HashSounds ();
int S_FindSound (const char *logicalname);
int S_LookupPlayerSound (const char *playerclass, int gender, const char *logicalname);
int S_LookupPlayerSound (const char *playerclass, int gender, int refid);
int S_FindSkinnedSound (AActor *actor, const char *logicalname);
int S_FindSkinnedSound (AActor *actor, int refid);
int S_FindSoundByLump (int lump);
int S_AddSound (const char *logicalname, const char *lumpname);	// Add sound by lumpname
int S_AddSoundLump (const char *logicalname, int lump);	// Add sound by lump index
int S_AddPlayerSound (const char *playerclass, const int gender, int refid, const char *lumpname);
int S_AddPlayerSound (const char *playerclass, const int gender, int refid, int lumpnum);
int S_AddPlayerSoundExisting (const char *playerclass, const int gender, int refid, int aliasto);
void S_ShrinkPlayerSoundLists ();

// [RH] Prints sound debug info to the screen.
//		Modelled after Hexen's noise cheat.
void S_NoiseDebug ();


#endif
