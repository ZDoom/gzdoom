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



#define MAX_SNDNAME	63

struct mobj_s;

//
// SoundFX struct.
//
typedef struct sfxinfo_struct sfxinfo_t;

struct sfxinfo_struct
{
	char		name[MAX_SNDNAME+1];	// [RH] Sound name defined in SNDINFO
	void*		data;					// sound data

	struct sfxinfo_struct *link;

	// this is checked every second to see if sound
	// can be thrown out (if 0, then decrement, if -1,
	// then throw out, if > 0, then it is in use)
	int 		usefulness;

	int 		lumpnum;				// lump number of sfx
	unsigned int ms;					// [RH] length of sfx in milliseconds
	unsigned int next, index;			// [RH] For hashing
	unsigned int frequency;				// [RH] Preferred playback rate
	unsigned int length;				// [RH] Length of the sound in bytes
};

// the complete set of sound effects
extern sfxinfo_t *S_sfx;

// [RH] Number of defined sounds
extern int numsfx;

// Initializes sound stuff, including volume
// Sets channels, SFX and music volume,
//	allocates channel buffer, sets S_sfx lookup.
//
void S_Init (int sfxVolume, int musicVolume);

// Per level startup code.
// Kills playing sounds at start of level,
//	determines music if any, changes music.
//
void S_Start(void);

// Start sound for thing at <ent>
void S_Sound (struct mobj_s *ent, int channel, char *name, float volume, int attenuation);
void S_PositionedSound (int x, int y, int channel, char *name, float volume, int attenuation);
void S_SoundID (struct mobj_s *ent, int channel, int sfxid, float volume, int attenuation);

// sound channels
// channel 0 never willingly overrides
// other channels (1-7) allways override a playing sound on that channel
#define CHAN_AUTO				0
#define CHAN_WEAPON				1
#define CHAN_VOICE				2
#define CHAN_ITEM				3
#define CHAN_BODY				4
// modifier flags
//#define CHAN_NO_PHS_ADD		8	// send to all clients, not just ones in PHS (ATTN 0 will also do this)
//#define CHAN_RELIABLE			16	// send by reliable message, not datagram


// sound attenuation values
#define ATTN_NONE				0	// full volume the entire level
#define ATTN_NORM				1
#define ATTN_IDLE				2
#define ATTN_STATIC				3	// diminish very rapidly with distance
#define ATTN_SURROUND			4	// like ATTN_NONE, but plays in surround sound


// [RH] From Hexen.
//		Prevents too many of the same sound from playing simultaneously.
BOOL S_StopSoundID (int sound_id, int priority);

// Stops a sound emanating from one of an entity's channels
void S_StopSound (struct mobj_s *ent, int channel);

// Stop sound for all channels
void S_StopAllChannels (void);

// Is the sound playing on one of the entity's channels?
BOOL S_GetSoundPlayingInfo (struct mobj_s *ent, int sound_id);

// Moves all sounds from one mobj to another
void S_RelinkSound (struct mobj_s *from, struct mobj_s *to);

// Start music using <music_name>
void S_StartMusic (char *music_name);

// Start music using <music_name>, and set whether looping
void S_ChangeMusic (char *music_name, int looping);

// Stops the music fer sure.
void S_StopMusic (void);

// Stop and resume music, during game PAUSE.
void S_PauseSound (void);
void S_ResumeSound (void);


//
// Updates music & sounds
//
void S_UpdateSounds (void *listener);

void S_SetMusicVolume (int volume);
void S_SetSfxVolume (int volume);


// [RH] Activates an ambient sound. Called when the thing is added to the map.
//		(0-biased)
void S_ActivateAmbient (struct mobj_s *mobj, int ambient);

// [RH] S_sfx "maintenance" routines
void S_ParseSndInfo (void);

void S_HashSounds (void);
int S_FindSound (const char *logicalname);
int S_FindSoundByLump (int lump);
int S_AddSound (char *logicalname, char *lumpname);	// Add sound by lumpname
int S_AddSoundLump (char *logicalname, int lump);	// Add sound by lump index

// [RH] Prints sound debug info to the screen.
//		Modelled after Hexen's noise cheat.
void S_NoiseDebug (void);
extern struct cvar_s *noisedebug;



#endif
