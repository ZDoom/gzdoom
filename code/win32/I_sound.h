// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id: i_sound.h,v 1.1.1.1 1997/12/28 12:59:03 pekangas Exp $
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
//
// DESCRIPTION:
//		System interface, sound.
//
//-----------------------------------------------------------------------------

#ifndef __I_SOUND__
#define __I_SOUND__

#include "doomdef.h"

#include "doomstat.h"
#include "s_sound.h"



// Init at program start...
void I_InitSound();

// ... shut down and relase at program termination.
void STACK_ARGS I_ShutdownSound (void);

void I_SetSfxVolume (int volume);

//
//	SFX I/O
//

// Initialize channels
void I_SetChannels (int);

// load a sound from disk
void I_LoadSound (struct sfxinfo_struct *sfx);

// Starts a sound in a particular sound channel.
int
I_StartSound
( struct sfxinfo_struct *sfx,
  int			vol,
  int			sep,
  int			pitch,
  int			channel );


// Stops a sound channel.
void I_StopSound(int handle);

// Called by S_*() functions
//	to see if a channel is still playing.
// Returns 0 if no longer playing, 1 if playing.
int I_SoundIsPlaying(int handle);

// Updates the volume, separation,
//	and pitch of a sound channel.
void
I_UpdateSoundParams
( int			handle,
  int			vol,
  int			sep,
  int			pitch );

#endif
