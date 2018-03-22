#ifndef OALSOUND_H
#define OALSOUND_H

#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <unordered_map>

#include "i_sound.h"
#include "s_sound.h"
#include "menu/menu.h"

#ifndef NO_OPENAL

#ifdef DYN_OPENAL
#define AL_NO_PROTOTYPES
#include "thirdparty/al.h"
#include "thirdparty/alc.h"
#else
#include "al.h"
#include "alc.h"
#endif

#include "efx.h"

#ifndef ALC_ENUMERATE_ALL_EXT
#define ALC_ENUMERATE_ALL_EXT 1
#define ALC_DEFAULT_ALL_DEVICES_SPECIFIER        0x1012
#define ALC_ALL_DEVICES_SPECIFIER                0x1013
#endif

#ifndef ALC_EXT_disconnect
#define ALC_EXT_disconnect 1
#define ALC_CONNECTED                            0x313
#endif

#ifndef ALC_SOFT_HRTF
#define ALC_SOFT_HRTF 1
#define ALC_HRTF_SOFT                            0x1992
#define ALC_DONT_CARE_SOFT                       0x0002
#define ALC_HRTF_STATUS_SOFT                     0x1993
#define ALC_HRTF_DISABLED_SOFT                   0x0000
#define ALC_HRTF_ENABLED_SOFT                    0x0001
#define ALC_HRTF_DENIED_SOFT                     0x0002
#define ALC_HRTF_REQUIRED_SOFT                   0x0003
#define ALC_HRTF_HEADPHONES_DETECTED_SOFT        0x0004
#define ALC_HRTF_UNSUPPORTED_FORMAT_SOFT         0x0005
#define ALC_NUM_HRTF_SPECIFIERS_SOFT             0x1994
#define ALC_HRTF_SPECIFIER_SOFT                  0x1995
#define ALC_HRTF_ID_SOFT                         0x1996
typedef const ALCchar* (ALC_APIENTRY*LPALCGETSTRINGISOFT)(ALCdevice *device, ALCenum paramName, ALCsizei index);
typedef ALCboolean (ALC_APIENTRY*LPALCRESETDEVICESOFT)(ALCdevice *device, const ALCint *attribs);
#ifdef AL_ALEXT_PROTOTYPES
ALC_API const ALCchar* ALC_APIENTRY alcGetStringiSOFT(ALCdevice *device, ALCenum paramName, ALCsizei index);
ALC_API ALCboolean ALC_APIENTRY alcResetDeviceSOFT(ALCdevice *device, const ALCint *attribs);
#endif
#endif

#ifndef AL_EXT_source_distance_model
#define AL_EXT_source_distance_model 1
#define AL_SOURCE_DISTANCE_MODEL                 0x200
#endif

#ifndef AL_SOFT_loop_points
#define AL_SOFT_loop_points 1
#define AL_LOOP_POINTS_SOFT                      0x2015
#endif

#ifndef AL_EXT_float32
#define AL_EXT_float32 1
#define AL_FORMAT_MONO_FLOAT32                   0x10010
#define AL_FORMAT_STEREO_FLOAT32                 0x10011
#endif

#ifndef AL_EXT_MCFORMATS
#define AL_EXT_MCFORMATS 1
#define AL_FORMAT_QUAD8                          0x1204
#define AL_FORMAT_QUAD16                         0x1205
#define AL_FORMAT_QUAD32                         0x1206
#define AL_FORMAT_REAR8                          0x1207
#define AL_FORMAT_REAR16                         0x1208
#define AL_FORMAT_REAR32                         0x1209
#define AL_FORMAT_51CHN8                         0x120A
#define AL_FORMAT_51CHN16                        0x120B
#define AL_FORMAT_51CHN32                        0x120C
#define AL_FORMAT_61CHN8                         0x120D
#define AL_FORMAT_61CHN16                        0x120E
#define AL_FORMAT_61CHN32                        0x120F
#define AL_FORMAT_71CHN8                         0x1210
#define AL_FORMAT_71CHN16                        0x1211
#define AL_FORMAT_71CHN32                        0x1212
#endif

#ifndef AL_EXT_SOURCE_RADIUS
#define AL_EXT_SOURCE_RADIUS 1
#define AL_SOURCE_RADIUS                         0x1031
#endif

#ifndef AL_SOFT_source_resampler
#define AL_SOFT_source_resampler 1
#define AL_NUM_RESAMPLERS_SOFT                   0x1210
#define AL_DEFAULT_RESAMPLER_SOFT                0x1211
#define AL_SOURCE_RESAMPLER_SOFT                 0x1212
#define AL_RESAMPLER_NAME_SOFT                   0x1213
typedef const ALchar* (AL_APIENTRY*LPALGETSTRINGISOFT)(ALenum pname, ALsizei index);
#ifdef AL_ALEXT_PROTOTYPES
AL_API const ALchar* AL_APIENTRY alGetStringiSOFT(ALenum pname, ALsizei index);
#endif
#endif

#ifndef AL_SOFT_source_spatialize
#define AL_SOFT_source_spatialize
#define AL_SOURCE_SPATIALIZE_SOFT                0x1214
#define AL_AUTO_SOFT                             0x0002
#endif


class OpenALSoundStream;

class OpenALSoundRenderer : public SoundRenderer
{
public:
	OpenALSoundRenderer();
	virtual ~OpenALSoundRenderer();

	virtual void SetSfxVolume(float volume);
	virtual void SetMusicVolume(float volume);
	virtual std::pair<SoundHandle, bool> LoadSound(uint8_t *sfxdata, int length, bool monoize, FSoundLoadBuffer *buffer);
	virtual std::pair<SoundHandle,bool> LoadSoundBuffered(FSoundLoadBuffer *buffer,  bool monoize);
	virtual std::pair<SoundHandle,bool> LoadSoundRaw(uint8_t *sfxdata, int length, int frequency, int channels, int bits, int loopstart, int loopend = -1, bool monoize = false);
	virtual void UnloadSound(SoundHandle sfx);
	virtual unsigned int GetMSLength(SoundHandle sfx);
	virtual unsigned int GetSampleLength(SoundHandle sfx);
	virtual float GetOutputRate();

	// Streaming sounds.
	virtual SoundStream *CreateStream(SoundStreamCallback callback, int buffbytes, int flags, int samplerate, void *userdata);
	virtual SoundStream *OpenStream(FileReader &reader, int flags);

	// Starts a sound.
	virtual FISoundChannel *StartSound(SoundHandle sfx, float vol, int pitch, int chanflags, FISoundChannel *reuse_chan);
	virtual FISoundChannel *StartSound3D(SoundHandle sfx, SoundListener *listener, float vol, FRolloffInfo *rolloff, float distscale, int pitch, int priority, const FVector3 &pos, const FVector3 &vel, int channum, int chanflags, FISoundChannel *reuse_chan);

	// Changes a channel's volume.
	virtual void ChannelVolume(FISoundChannel *chan, float volume);

	// Stops a sound channel.
	virtual void StopChannel(FISoundChannel *chan);

	// Returns position of sound on this channel, in samples.
	virtual unsigned int GetPosition(FISoundChannel *chan);

	// Synchronizes following sound startups.
	virtual void Sync(bool sync);

	// Pauses or resumes all sound effect channels.
	virtual void SetSfxPaused(bool paused, int slot);

	// Pauses or resumes *every* channel, including environmental reverb.
	virtual void SetInactive(SoundRenderer::EInactiveState inactive);

	// Updates the volume, separation, and pitch of a sound channel.
	virtual void UpdateSoundParams3D(SoundListener *listener, FISoundChannel *chan, bool areasound, const FVector3 &pos, const FVector3 &vel);

	virtual void UpdateListener(SoundListener *);
	virtual void UpdateSounds();

	virtual void MarkStartTime(FISoundChannel*);
	virtual float GetAudibility(FISoundChannel*);


	virtual bool IsValid();
	virtual void PrintStatus();
	virtual void PrintDriversList();
	virtual FString GatherStats();

private:
    struct {
        bool EXT_EFX;
        bool EXT_disconnect;
        bool SOFT_HRTF;
        bool SOFT_pause_device;
    } ALC;
    struct {
        bool EXT_source_distance_model;
        bool EXT_SOURCE_RADIUS;
        bool SOFT_deferred_updates;
        bool SOFT_loop_points;
        bool SOFT_source_resampler;
        bool SOFT_source_spatialize;
    } AL;

	// EFX Extension function pointer variables. Loaded after context creation
	// if EFX is supported. These pointers may be context- or device-dependant,
	// thus can't be static
	// Effect objects
	LPALGENEFFECTS alGenEffects;
	LPALDELETEEFFECTS alDeleteEffects;
	LPALISEFFECT alIsEffect;
	LPALEFFECTI alEffecti;
	LPALEFFECTIV alEffectiv;
	LPALEFFECTF alEffectf;
	LPALEFFECTFV alEffectfv;
	LPALGETEFFECTI alGetEffecti;
	LPALGETEFFECTIV alGetEffectiv;
	LPALGETEFFECTF alGetEffectf;
	LPALGETEFFECTFV alGetEffectfv;
	// Filter objects
	LPALGENFILTERS alGenFilters;
	LPALDELETEFILTERS alDeleteFilters;
	LPALISFILTER alIsFilter;
	LPALFILTERI alFilteri;
	LPALFILTERIV alFilteriv;
	LPALFILTERF alFilterf;
	LPALFILTERFV alFilterfv;
	LPALGETFILTERI alGetFilteri;
	LPALGETFILTERIV alGetFilteriv;
	LPALGETFILTERF alGetFilterf;
	LPALGETFILTERFV alGetFilterfv;
	// Auxiliary slot objects
	LPALGENAUXILIARYEFFECTSLOTS alGenAuxiliaryEffectSlots;
	LPALDELETEAUXILIARYEFFECTSLOTS alDeleteAuxiliaryEffectSlots;
	LPALISAUXILIARYEFFECTSLOT alIsAuxiliaryEffectSlot;
	LPALAUXILIARYEFFECTSLOTI alAuxiliaryEffectSloti;
	LPALAUXILIARYEFFECTSLOTIV alAuxiliaryEffectSlotiv;
	LPALAUXILIARYEFFECTSLOTF alAuxiliaryEffectSlotf;
	LPALAUXILIARYEFFECTSLOTFV alAuxiliaryEffectSlotfv;
	LPALGETAUXILIARYEFFECTSLOTI alGetAuxiliaryEffectSloti;
	LPALGETAUXILIARYEFFECTSLOTIV alGetAuxiliaryEffectSlotiv;
	LPALGETAUXILIARYEFFECTSLOTF alGetAuxiliaryEffectSlotf;
	LPALGETAUXILIARYEFFECTSLOTFV alGetAuxiliaryEffectSlotfv;

    ALvoid (AL_APIENTRY*alDeferUpdatesSOFT)(void);
    ALvoid (AL_APIENTRY*alProcessUpdatesSOFT)(void);

    LPALGETSTRINGISOFT alGetStringiSOFT;

    void (ALC_APIENTRY*alcDevicePauseSOFT)(ALCdevice *device);
    void (ALC_APIENTRY*alcDeviceResumeSOFT)(ALCdevice *device);

    void BackgroundProc();
    void AddStream(OpenALSoundStream *stream);
    void RemoveStream(OpenALSoundStream *stream);

	void LoadReverb(const ReverbContainer *env);
	void FreeSource(ALuint source);
	void PurgeStoppedSources();
	static FSoundChan *FindLowestChannel();
	void ForceStopChannel(FISoundChannel *chan);

    std::thread StreamThread;
    std::mutex StreamLock;
    std::condition_variable StreamWake;
    std::atomic<bool> QuitThread;

	ALCdevice *Device;
	ALCcontext *Context;

	TArray<ALuint> Sources;

	ALfloat SfxVolume;
	ALfloat MusicVolume;

	int SFXPaused;
	TArray<ALuint> FreeSfx;
	TArray<ALuint> PausableSfx;
	TArray<ALuint> ReverbSfx;
	TArray<ALuint> SfxGroup;

	int UpdateTimeMS;
	using SourceTimeMap = std::unordered_map<ALuint,int64_t>;
	SourceTimeMap FadingSources;

	const ReverbContainer *PrevEnvironment;

    typedef TMap<uint16_t,ALuint> EffectMap;
    typedef TMapIterator<uint16_t,ALuint> EffectMapIter;
    ALuint EnvSlot;
    ALuint EnvFilters[2];
    EffectMap EnvEffects;

    bool WasInWater;

    TArray<OpenALSoundStream*> Streams;
    friend class OpenALSoundStream;

	ALCdevice *InitDevice();
};

#endif // NO_OPENAL

#endif
