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
	void LoadSound (sfxinfo_t *sfx);
	void UnloadSound (sfxinfo_t *sfx);
	unsigned int GetMSLength(sfxinfo_t *sfx);

	// Streaming sounds.
	SoundStream *CreateStream (SoundStreamCallback callback, int buffsamples, int flags, int samplerate, void *userdata);
	SoundStream *OpenStream (const char *filename, int flags, int offset, int length);
	long PlayStream (SoundStream *stream, int volume);
	void StopStream (SoundStream *stream);

	// Starts a sound.
	FSoundChan *StartSound (sfxinfo_t *sfx, float vol, int pitch, int chanflags);
	FSoundChan *StartSound3D (sfxinfo_t *sfx, float vol, float distscale, int pitch, int priority, float pos[3], float vel[3], int chanflags);

	// Stops a sound channel.
	void StopSound (FSoundChan *chan);

	// Pauses or resumes all sound effect channels.
	void SetSfxPaused (bool paused);

	// Updates the position of a sound channel.
	void UpdateSoundParams3D (FSoundChan *chan, float pos[3], float vel[3]);

	// For use by I_PlayMovie
	void MovieDisableSound ();
	void MovieResumeSound ();

	void UpdateListener ();
	void UpdateSounds ();

	void PrintStatus ();
	void PrintDriversList ();
	FString GatherStats ();
	void ResetEnvironment ();

private:
	unsigned int DriverCaps;
	int OutputType;
	bool SFXPaused;
	bool InitSuccess;

	static FMOD_RESULT F_CALLBACK ChannelEndCallback
		(FMOD_CHANNEL *channel, FMOD_CHANNEL_CALLBACKTYPE type, int cmd, unsigned int data1, unsigned int data2);
	static float F_CALLBACK RolloffCallback(FMOD_CHANNEL *channel, float distance);

	FSoundChan *CommonChannelSetup(FMOD::Channel *chan) const;
	FMOD_MODE SetChanHeadSettings(FMOD::Channel *chan, sfxinfo_t *sfx, float pos[3], int chanflags, FMOD_MODE oldmode) const;
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
