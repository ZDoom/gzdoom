/*
** i_sound.h
**
**---------------------------------------------------------------------------
** Copyright 1998-2001 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/


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
void I_UpdateSounds ();

// Stops a sound channel.
void I_StopSound (int handle);

// Called by S_*() functions to see if a channel is still playing.
// Returns 0 if no longer playing, 1 if playing.
int I_SoundIsPlaying (int handle);

// Updates the volume, separation, and pitch of a sound channel.
void I_UpdateSoundParams (int handle, int vol, int sep, int pitch);
void I_UpdateSoundParams3D (int handle, float pos[3], float vel[3]);


// For use by I_PlayMovie
void I_MovieDisableSound ();
void I_MovieResumeSound ();

#endif
