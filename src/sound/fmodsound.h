#ifndef FMODSOUND_H
#define FMODSOUND_H

#include "i_sound.h"
#include "fmod_wrap.h"

class FMODSoundRenderer : public SoundRenderer
{
public:
	FMODSoundRenderer ();
	~FMODSoundRenderer ();
	bool IsValid ();

	void SetSfxVolume (float volume);
	void SetMusicVolume (float volume);
	int  GetNumChannels ();
	void LoadSound (sfxinfo_t *sfx);
	void UnloadSound (sfxinfo_t *sfx);

	// Streaming sounds. PlayStream returns a channel handle that can be used with StopSound.
	SoundStream *CreateStream (SoundStreamCallback callback, int buffsamples, int flags, int samplerate, void *userdata);
	SoundStream *OpenStream (const char *filename, int flags, int offset, int length);
	long PlayStream (SoundStream *stream, int volume);
	void StopStream (SoundStream *stream);

	// Starts a sound in a particular sound channel.
	long StartSound (sfxinfo_t *sfx, float vol, float sep, int pitch, int channel, bool looping, bool pauseable);
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
	void UpdateSoundParams (long handle, float vol, float sep, int pitch);
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
		FMOD::Channel *channelID;
		bool bIsLooping;
	} *ChannelMap;

	int NumChannels;
	unsigned int DriverCaps;
	int OutputType;
	bool SFXPaused;

	void DoLoad (void **slot, sfxinfo_t *sfx);
	void getsfx (sfxinfo_t *sfx);

	bool Init ();
	void Shutdown ();
	void DumpDriverCaps(FMOD_CAPS caps, int minfrequency, int maxfrequency);

	FMOD::System *Sys;
	FMOD::ChannelGroup *SfxGroup, *PausableSfx;
	FMOD::ChannelGroup *MusicGroup;

	// Just for snd_status display
	int Driver_MinFrequency;
	int Driver_MaxFrequency;
	FMOD_CAPS Driver_Caps;

	friend class FMODStreamCapsule;
};

#endif
