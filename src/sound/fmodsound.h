#ifndef FMODSOUND_H
#define FMODSOUND_H

#include "i_sound.h"

#ifndef NO_FMOD
#include "fmod_wrap.h"

class FMODSoundRenderer : public SoundRenderer
{
public:
	FMODSoundRenderer ();
	~FMODSoundRenderer ();
	bool IsValid ();

	void SetSfxVolume (float volume);
	void SetMusicVolume (float volume);
	std::pair<SoundHandle,bool> LoadSound(BYTE *sfxdata, int length, bool monoize);
	std::pair<SoundHandle,bool> LoadSoundRaw(BYTE *sfxdata, int length, int frequency, int channels, int bits, int loopstart, int loopend = -1, bool monoize = false);
	void UnloadSound (SoundHandle sfx);
	unsigned int GetMSLength(SoundHandle sfx);
	unsigned int GetSampleLength(SoundHandle sfx);
	float GetOutputRate();

	// Streaming sounds.
	SoundStream *CreateStream (SoundStreamCallback callback, int buffsamples, int flags, int samplerate, void *userdata);
    SoundStream *OpenStream (FileReader *reader, int flags);
	SoundStream *OpenStream (const char *url, int flags);

	// Starts a sound.
	FISoundChannel *StartSound (SoundHandle sfx, float vol, int pitch, int chanflags, FISoundChannel *reuse_chan);
	FISoundChannel *StartSound3D (SoundHandle sfx, SoundListener *listener, float vol, FRolloffInfo *rolloff, float distscale, int pitch, int priority, const FVector3 &pos, const FVector3 &vel, int channum, int chanflags, FISoundChannel *reuse_chan);

	// Stops a sound channel.
	void StopChannel (FISoundChannel *chan);

	// Changes a channel's volume.
	void ChannelVolume (FISoundChannel *chan, float volume);

	// Marks a channel's start time without actually playing it.
	void MarkStartTime (FISoundChannel *chan);

	// Returns position of sound on this channel, in samples.
	unsigned int GetPosition(FISoundChannel *chan);

	// Gets a channel's audibility (real volume).
	float GetAudibility(FISoundChannel *chan);

	// Synchronizes following sound startups.
	void Sync (bool sync);

	// Pauses or resumes all sound effect channels.
	void SetSfxPaused (bool paused, int slot);

	// Pauses or resumes *every* channel, including environmental reverb.
	void SetInactive (EInactiveState inactive);

	// Updates the position of a sound channel.
	void UpdateSoundParams3D (SoundListener *listener, FISoundChannel *chan, bool areasound, const FVector3 &pos, const FVector3 &vel);

	void UpdateListener (SoundListener *listener);
	void UpdateSounds ();

	void PrintStatus ();
	void PrintDriversList ();
	FString GatherStats ();
	short *DecodeSample(int outlen, const void *coded, int sizebytes, ECodecType type);

	void DrawWaveDebug(int mode);

private:
	DWORD ActiveFMODVersion;
	int SFXPaused;
	bool InitSuccess;
	bool DSPLocked;
	QWORD_UNION DSPClock;
	int OutputRate;

	static FMOD_RESULT F_CALLBACK ChannelCallback(FMOD_CHANNEL *channel, FMOD_CHANNEL_CALLBACKTYPE type, void *data1, void *data2);
	static float F_CALLBACK RolloffCallback(FMOD_CHANNEL *channel, float distance);

	bool HandleChannelDelay(FMOD::Channel *chan, FISoundChannel *reuse_chan, int flags, float freq) const;
	FISoundChannel *CommonChannelSetup(FMOD::Channel *chan, FISoundChannel *reuse_chan) const;
	FMOD_MODE SetChanHeadSettings(SoundListener *listener, FMOD::Channel *chan, const FVector3 &pos, bool areasound, FMOD_MODE oldmode) const;

	bool ReconnectSFXReverbUnit();
	void InitCreateSoundExInfo(FMOD_CREATESOUNDEXINFO *exinfo) const;
	FMOD_RESULT SetSystemReverbProperties(const REVERB_PROPERTIES *props);

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

	typedef char spk[4];
	static const spk SpeakerNames4[4], SpeakerNamesMore[8];
	void DrawSpeakerLabels(const spk *labels, int y, int width, int count);

	FMOD::System *Sys;
	FMOD::ChannelGroup *SfxGroup, *PausableSfx;
	FMOD::ChannelGroup *MusicGroup;
	FMOD::DSP *WaterLP, *WaterReverb;
	FMOD::DSPConnection *SfxConnection;
	FMOD::DSPConnection *ChannelGroupTargetUnitOutput;
	FMOD::DSP *ChannelGroupTargetUnit;
	FMOD::DSP *SfxReverbPlaceholder;
	bool SfxReverbHooked;
	float LastWaterLP;
	unsigned int OutputPlugin;

	// Just for snd_status display
	int Driver_MinFrequency;
	int Driver_MaxFrequency;
	FMOD_CAPS Driver_Caps;

	friend class FMODStreamCapsule;
};

#endif
#endif
