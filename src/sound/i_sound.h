/*
** i_sound.h
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
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

#include "s_sound.h"

class SoundStream
{
public:
	virtual ~SoundStream ();

	enum
	{	// For CreateStream
		Mono = 1,
		Bits8 = 2,

		// For OpenStream
		Loop = 4
	};

	virtual bool Play (bool looping, float volume) = 0;
	virtual void Stop () = 0;
	virtual void SetVolume (float volume) = 0;
	virtual bool SetPaused (bool paused) = 0;
	virtual unsigned int GetPosition () = 0;
};

class SoundTrackerModule
{
public:
	virtual ~SoundTrackerModule ();

	virtual bool Play () = 0;
	virtual void Stop () = 0;
	virtual void SetVolume (float volume) = 0;
	virtual bool SetPaused (bool paused) = 0;
	virtual bool IsPlaying () = 0;
	virtual bool IsFinished () = 0;
	virtual bool SetOrder (int order) = 0;
};

typedef bool (*SoundStreamCallback)(SoundStream *stream, void *buff, int len, void *userdata);

class SoundRenderer
{
public:
	SoundRenderer ();
	virtual ~SoundRenderer ();

	virtual void SetSfxVolume (float volume) = 0;
	virtual int  SetChannels (int numchans) = 0;	// Initialize channels
	virtual void LoadSound (sfxinfo_t *sfx) = 0;	// load a sound from disk
	virtual void UnloadSound (sfxinfo_t *sfx) = 0;	// unloads a sound from memory

	// Streaming sounds. PlayStream returns a channel handle that can be used with StopSound.
	virtual SoundStream *CreateStream (SoundStreamCallback callback, int buffbytes, int flags, int samplerate, void *userdata) = 0;
	virtual SoundStream *OpenStream (const char *filename, int flags, int offset, int length) = 0;

	// Tracker modules.
	virtual SoundTrackerModule *OpenModule (const char *file, int offset, int length);

	// Starts a sound in a particular sound channel.
	virtual long StartSound (sfxinfo_t *sfx, int vol, int sep, int pitch, int channel, bool looping, bool pauseable) = 0;
	virtual long StartSound3D (sfxinfo_t *sfx, float vol, int pitch, int channel, bool looping, float pos[3], float vel[3], bool pauseable);

	// Stops a sound channel.
	virtual void StopSound (long handle) = 0;

	// Stops all sounds.
	virtual void StopAllChannels () = 0;

	// Pauses or resumes all sound effect channels.
	virtual void SetSfxPaused (bool paused) = 0;

	// Returns true if the channel is still playing a sound.
	virtual bool IsPlayingSound (long handle) = 0;

	// Updates the volume, separation, and pitch of a sound channel.
	virtual void UpdateSoundParams (long handle, int vol, int sep, int pitch) = 0;
	virtual void UpdateSoundParams3D (long handle, float pos[3], float vel[3]);

	// For use by I_PlayMovie
	virtual void MovieDisableSound () = 0;
	virtual void MovieResumeSound () = 0;

	virtual void UpdateListener (AActor *listener);
	virtual void UpdateSounds ();

	virtual bool IsValid () = 0;
	virtual void PrintStatus () = 0;
	virtual void PrintDriversList () = 0;
	virtual FString GatherStats ();
	virtual void ResetEnvironment ();

	bool Sound3D;
};

extern SoundRenderer *GSnd;

void I_InitSound ();
void I_ShutdownSound ();

#endif
