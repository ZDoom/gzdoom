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

// Start sound for thing at <origin> using <sound_id> from sounds.h
// [RH] macro-fied
#define S_StartSound(o,n,p) S_StartSoundAtVolume (o,n,p,255)
#define S_StartSfx(o,n,p) S_StartSfxAtVolume (o,n,p,255)

// Will start a sound at a given volume.
void S_StartSoundAtVolume (void *origin, char *name, int pri, int volume);
void S_StartSfxAtVolume (void *origin, int sfxid, int pri, int volume);

#define ORIGIN_SURROUNDBIT (128)
#define ORIGIN_AMBIENT	(NULL)					// Sound is not attenuated
#define ORIGIN_AMBIENT2 ((void *)1)				// [RH] Same as ORIGIN_AMBIENT, just diff. channel
#define ORIGIN_AMBIENT3 ((void *)2)
#define ORIGIN_AMBIENT4 ((void *)3)
#define ORIGIN_SURROUND ((void *)(0|128))		// [RH] Sound is not attenuated and played surround
#define	ORIGIN_SURROUND2 ((void *)(1|128))
#define ORIGIN_SURROUND3 ((void *)(2|128))
#define ORIGIN_SURROUND4 ((void *)(3|128))
#define ORIGIN_WORLDAMBIENTS ((void *)8)		// Used by ambient sounds

#define ORIGIN_STARTOFNORMAL	((void *)256)	// [RH] Used internally

// [RH] Simplified world ambient playing. Will cycle through all
//		ORIGIN_WORLDAMBIENTS positions automatically.
void S_StartAmbient (char *name, int volume, int surround);


// Stop sound for thing at <origin>
void S_StopSound (void *origin);

// Start music using <music_name>
void S_StartMusic (char *music_name);

// Start music using <music_name>,
//	and set whether looping
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
void S_ActivateAmbient (void *mobj, int ambient);

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
//-----------------------------------------------------------------------------
//
// $Log: s_sound.h,v $
// Revision 1.1.1.1  1997/12/28 12:59:03  pekangas
// Initial DOOM source release from id Software
//
//
//-----------------------------------------------------------------------------
