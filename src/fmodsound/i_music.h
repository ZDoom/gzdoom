#ifndef __I_MUSIC_H__
#define __I_MUSIC_H__

#include "doomdef.h"

#include "doomstat.h"

//
//	MUSIC I/O
//
void I_InitMusic ();
void STACK_ARGS I_ShutdownMusic ();

// Volume.
void I_SetMIDIVolume (float volume);
void I_SetMusicVolume (int volume);

// PAUSE game handling.
void I_PauseSong (long handle);
void I_ResumeSong (long handle);

// Registers a song handle to song data.
long I_RegisterSong (int handle, int pos, int len);
long I_RegisterCDSong (int track, int cdid = 0);

// Called by anything that wishes to start music.
//	Plays a song, and when the song is done,
//	starts playing it again in an endless loop.
void I_PlaySong (long handle, int looping);

// Stops a song.
void I_StopSong (long handle);

// See above (register), then think backwards
void I_UnRegisterSong (long handle);

// Set the current order (position) for a MOD
bool I_SetSongPosition (long handle, int order);

// Is the song still playing?
bool I_QrySongPlaying (long handle);

#endif //__I_MUSIC_H__
