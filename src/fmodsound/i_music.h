/*
** i_music.h
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

#ifndef __I_MUSIC_H__
#define __I_MUSIC_H__

#include "doomdef.h"

#include "doomstat.h"

//
//	MUSIC I/O
//
void I_InitMusic ();
void STACK_ARGS I_ShutdownMusic ();
void I_BuildMIDIMenuList (struct value_s **values, float *numValues);

// Volume.
void I_SetMIDIVolume (float volume);
void I_SetMusicVolume (int volume);

// PAUSE game handling.
void I_PauseSong (void *handle);
void I_ResumeSong (void *handle);

// Registers a song handle to song data.
void *I_RegisterSong (int handle, int pos, int len);
void *I_RegisterCDSong (int track, int cdid = 0);

// Called by anything that wishes to start music.
//	Plays a song, and when the song is done,
//	starts playing it again in an endless loop.
void I_PlaySong (void *handle, int looping);

// Stops a song.
void I_StopSong (void *handle);

// See above (register), then think backwards
void I_UnRegisterSong (void *handle);

// Set the current order (position) for a MOD
bool I_SetSongPosition (void *handle, int order);

// Is the song still playing?
bool I_QrySongPlaying (void *handle);

#endif //__I_MUSIC_H__
