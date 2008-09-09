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
	SoundHandle LoadSound(BYTE *sfxdata, int length);
	SoundHandle LoadSoundRaw(BYTE *sfxdata, int length, int frequency, int channels, int bits);
	void UnloadSound (SoundHandle sfx);
	unsigned int GetMSLength(SoundHandle sfx);
	float GetOutputRate();

	// Streaming sounds.
	SoundStream *CreateStream (SoundStreamCallback callback, int buffsamples, int flags, int samplerate, void *userdata);
	SoundStream *OpenStream (const char *filename, int flags, int offset, int length);
	long PlayStream (SoundStream *stream, int volume);
	void StopStream (SoundStream *stream);

	// Starts a sound.
	FSoundChan *StartSound (SoundHandle sfx, float vol, int pitch, int chanflags, FSoundChan *reuse_chan);
	FSoundChan *StartSound3D (SoundHandle sfx, SoundListener *listener, float vol, FRolloffInfo *rolloff, float distscale, int pitch, int priority, const FVector3 &pos, const FVector3 &vel, int channum, int chanflags, FSoundChan *reuse_chan);

	// Stops a sound channel.
	void StopSound (FSoundChan *chan);

	// Returns position of sound on this channel, in samples.
	unsigned int GetPosition(FSoundChan *chan);

	// Synchronizes following sound startups.
	void Sync (bool sync);

	// Pauses or resumes all sound effect channels.
	void SetSfxPaused (bool paused, int slot);

	// Pauses or resumes *every* channel, including environmental reverb.
	void SetInactive (bool inactive);

	// Updates the position of a sound channel.
	void UpdateSoundParams3D (SoundListener *listener, FSoundChan *chan, const FVector3 &pos, const FVector3 &vel);

	void UpdateListener (SoundListener *listener);
	void UpdateSounds ();

	void PrintStatus ();
	void PrintDriversList ();
	FString GatherStats ();
	short *DecodeSample(int outlen, const void *coded, int sizebytes, ECodecType type);

	void DrawWaveDebug(int mode);

private:
	int SFXPaused;
	bool InitSuccess;
	bool DSPLocked;
	QWORD_UNION DSPClock;
	int OutputRate;

	static FMOD_RESULT F_CALLBACK ChannelEndCallback
		(FMOD_CHANNEL *channel, FMOD_CHANNEL_CALLBACKTYPE type, int cmd, unsigned int data1, unsigned int data2);
	static float F_CALLBACK RolloffCallback(FMOD_CHANNEL *channel, float distance);

	void HandleChannelDelay(FMOD::Channel *chan, FSoundChan *reuse_chan, float freq) const;
	FSoundChan *CommonChannelSetup(FMOD::Channel *chan, FSoundChan *reuse_chan) const;
	FMOD_MODE SetChanHeadSettings(SoundListener *listener, FMOD::Channel *chan, const FVector3 &pos, int channum, int chanflags, FMOD_MODE oldmode) const;

	bool Init ();
	void Shutdown ();
	void DumpDriverCaps(FMOD_CAPS caps, int minfrequency, int maxfrequency);

	int DrawChannelGroupOutput(FMOD::ChannelGroup *group, float *wavearray, int width, int height, int y, int mode);
	int DrawSystemOutput(float *wavearray, int width, int height, int y, int mode);

	int DrawChannelGroupWaveData(FMOD::ChannelGroup *group, float *wavearray, int width, int height, int y, bool skip);
	int DrawSystemWaveData(float *wavearray, int width, int height, int y, bool skip);
	void DrawWave(float *wavearray, int x, int y, int width, int height);

	int DrawChannelGroupSpectrum(FMOD::ChannelGroup *group, float *wavearray, int width, int height, int y, bool skip);
	int DrawSystemSpectrum(float *wavearray, int width, int height, int y, bool skip);
	void DrawSpectrum(float *spectrumarray, int x, int y, int width, int height);

	FMOD::System *Sys;
	FMOD::ChannelGroup *SfxGroup, *PausableSfx;
	FMOD::ChannelGroup *MusicGroup;
	FMOD::DSP *WaterLP, *WaterReverb;
	FMOD::DSPConnection *SfxConnection;
	FMOD::DSP *ChannelGroupTargetUnit;
	float LastWaterLP;

	// Just for snd_status display
	int Driver_MinFrequency;
	int Driver_MaxFrequency;
	FMOD_CAPS Driver_Caps;

	friend class FMODStreamCapsule;
};

#endif
