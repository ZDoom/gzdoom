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




//
// Initializes sound stuff, including volume
// Sets channels, SFX and music volume,
//	allocates channel buffer, sets S_sfx lookup.
//
void
S_Init
( int			sfxVolume,
  int			musicVolume );




//
// Per level startup code.
// Kills playing sounds at start of level,
//	determines music if any, changes music.
//
void S_Start(void);


//
// Start sound for thing at <origin>
//	using <sound_id> from sounds.h
//
void S_StartSound (void *origin, int sound_id);



// Will start a sound at a given volume.
void S_StartSoundAtVolume (void *origin, int sound_id, int volume);

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


// [RH] Parses all SNDINFO lumps. Should be called from I_InitSound()
void S_ParseSndInfo (void);

// [RH] Activates an ambient sound. Called when the thing is added to the map.
//		(0-biased)
void S_ActivateAmbient (void *mobj, int ambient);

// [RH] Deactivates all ambient sounds.
void S_ClearAmbients (void);

#endif
//-----------------------------------------------------------------------------
//
// $Log: s_sound.h,v $
// Revision 1.1.1.1  1997/12/28 12:59:03  pekangas
// Initial DOOM source release from id Software
//
//
//-----------------------------------------------------------------------------
