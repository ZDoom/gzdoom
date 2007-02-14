#ifndef FMODSOUND_H
#define FMODSOUND_H

#include "i_sound.h"
#include <fmod.h>

class FMODSoundRenderer : public SoundRenderer
{
public:
	FMODSoundRenderer ();
	~FMODSoundRenderer ();
	bool IsValid ();

	void SetSfxVolume (float volume);
	int  SetChannels (int numchans);
	void LoadSound (sfxinfo_t *sfx);
	void UnloadSound (sfxinfo_t *sfx);

	// Streaming sounds. PlayStream returns a channel handle that can be used with StopSound.
	SoundStream *CreateStream (SoundStreamCallback callback, int buffsamples, int flags, int samplerate, void *userdata);
	SoundStream *OpenStream (const char *filename, int flags, int offset, int length);
	long PlayStream (SoundStream *stream, int volume);
	void StopStream (SoundStream *stream);

	// Tracker modules.
	SoundTrackerModule *OpenModule (const char *file, int offset, int length);

	// Starts a sound in a particular sound channel.
	long StartSound (sfxinfo_t *sfx, int vol, int sep, int pitch, int channel, bool looping, bool pauseable);
	long StartSound3D (sfxinfo_t *sfx, float vol, int pitch, int channel, bool looping, float pos[3], float vel[3], bool pauseable);

	// Stops a sound channel.
	void StopSound (long handle);

	// Stops all sounds.
	void StopAllChannels ();

	// Pauses or resumes all sound effect channels.
	void SetSfxPaused (bool paused);

	// Returns true if the channel is still playing a sound.
	bool IsPlayingSound (long handle);

	// Updates the volume, separation, and pitch of a sound channel.
	void UpdateSoundParams (long handle, int vol, int sep, int pitch);
	void UpdateSoundParams3D (long handle, float pos[3], float vel[3]);

	// For use by I_PlayMovie
	void MovieDisableSound ();
	void MovieResumeSound ();

	void UpdateListener (AActor *listener);

	void PrintStatus ();
	void PrintDriversList ();
	FString GatherStats ();
	void ResetEnvironment ();

private:
	// Maps sfx channels onto FMOD channels
	struct ChanMap
	{
		int soundID;		// sfx playing on this channel
		long channelID;
		bool bIsLooping;
		bool bIs3D;
		bool bIsPauseable;
		unsigned int lastPos;
	} *ChannelMap;

	int NumChannels;
	unsigned int DriverCaps;
	int OutputType;
	bool DidInit;

	int PutSampleData (FSOUND_SAMPLE *sample, const BYTE *data, int len, unsigned int mode);
	void DoLoad (void **slot, sfxinfo_t *sfx);
	void getsfx (sfxinfo_t *sfx);
	FSOUND_SAMPLE *CheckLooping (sfxinfo_t *sfx, bool looped);
	void UncheckSound (sfxinfo_t *sfx, bool looped);

	bool Init ();
	void Shutdown ();
};

#endif
