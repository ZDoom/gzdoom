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

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include "doomdef.h"

#include "doomstat.h"
#include "s_sound.h"
#include "w_wad.h"


extern bool Sound3D;

// Init at program start...
void I_InitSound();

// ... shut down and relase at program termination.
void STACK_ARGS I_ShutdownSound ();

void I_SetSfxVolume (int volume);

//
//	SFX I/O
//

// Initialize channels
int I_SetChannels (int);

// load a sound from disk
void I_LoadSound (sfxinfo_t *sfx);

// Starts a sound in a particular sound channel.
long
I_StartSound
( sfxinfo_t		*sfx,
  int			vol,
  int			sep,
  int			pitch,
  int			channel,
  BOOL			looping );

long
I_StartSound3D
( sfxinfo_t		*sfx,
  float			vol,
  int			pitch,
  int			channel,
  BOOL			looping,
  float			pos[3],
  float			vel[3] );

void I_UpdateListener (AActor *listener);

// Stops a sound channel.
void I_StopSound (int handle);

// Called by S_*() functions
//	to see if a channel is still playing.
// Returns 0 if no longer playing, 1 if playing.
int I_SoundIsPlaying (int handle);

// Updates the volume, separation,
//	and pitch of a sound channel.
void I_UpdateSoundParams (int handle, int vol, int sep, int pitch);
void I_UpdateSoundParams3D (int handle, float pos[3], float vel[3]);


struct FileHandle
{
	FileHandle (int hndl, int start, int size)
		: len (size),
		  pos (0),
		  base (start),
		  bNeedClose (true)
	{
		handle = dup (hndl);
	}

	FileHandle (int lump)
		: pos (0),
		  bNeedClose (false)
	{
		handle = W_FileHandleFromWad (lumpinfo[lump].wadnum);
		base = lumpinfo[lump].position;
		len = lumpinfo[lump].size;
	}

	~FileHandle ()
	{
		if (bNeedClose)
			close (handle);
	}

	int handle;
	int len;
	int pos;
	int base;
	bool bNeedClose;
};

#endif
