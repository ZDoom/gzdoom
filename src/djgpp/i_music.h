#ifndef __I_MUSIC_H__
#define __I_MUSIC_H__

#include "doomdef.h"

#include "doomstat.h"

//
//	MUSIC I/O
//
void I_InitMusic(void);
void I_ShutdownMusic(void);
// Volume.
void I_SetMIDIVolume (float volume);
void I_SetMusicVolume(int volume);
// PAUSE game handling.
void I_PauseSong(int handle);
void I_ResumeSong(int handle);
// Registers a song handle to song data.
int I_RegisterSong(void *data, int length);
// Called by anything that wishes to start music.
//	plays a song, and when the song is done,
//	starts playing it again in an endless loop.
// Horrible thing to do, considering.
void I_PlaySong (int handle, int looping);
// Stops a song over 3 seconds.
void I_StopSong(int handle);
// See above (register), then think backwards
void I_UnRegisterSong(int handle);

// Called from WndProc when MIDI is finished
void I_RestartSong (void);

#endif //__I_MUSIC_H__