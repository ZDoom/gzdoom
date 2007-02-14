#ifndef ALTSOUND_H
#define ALTSOUND_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>

#include "i_sound.h"

class AltSoundRenderer : public SoundRenderer
{
public:
	AltSoundRenderer ();
	~AltSoundRenderer ();
	bool IsValid ();

	void SetSfxVolume (float volume);
	int  SetChannels (int numchans);
	void LoadSound (sfxinfo_t *sfx);
	void UnloadSound (sfxinfo_t *sfx);

	// Streaming sounds. PlayStream returns a channel handle that can be used with StopSound.
	SoundStream *CreateStream (SoundStreamCallback callback, int buffsamples, int flags, int samplerate, void *userdata);
	SoundStream *OpenStream (const char *filename, int flags, int offset, int length);

	// Starts a sound in a particular sound channel.
	long StartSound (sfxinfo_t *sfx, int vol, int sep, int pitch, int channel, bool looping, bool pauseable);

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

	// For use by I_PlayMovie
	void MovieDisableSound ();
	void MovieResumeSound ();

	void UpdateSounds ();

	void PrintStatus ();
	void PrintDriversList ();
	FString GatherStats ();

private:
	struct Channel;
	struct Stream;

	LPDIRECTSOUND lpds;
	LPDIRECTSOUNDBUFFER lpdsb, lpdsbPrimary;

	int Frequency;
	bool SimpleDown;
	int Amp;
	DWORD BufferSamples, BufferBytes;
	DWORD WritePos;
	DWORD BufferTime;
	DWORD MaxWaitTime;
	SDWORD *RenderBuffer;
	HANDLE MixerThread;
	HANDLE MixerEvent;
	bool MixerQuit;

	enum { NUM_PERFMETERS = 32 };
	double PerfMeter[NUM_PERFMETERS];
	int CurPerfMeter;

	Channel *Channels;
	int NumChannels;
	Stream *Streams;
	CRITICAL_SECTION StreamCriticalSection;

	bool DidInit;

	bool Init ();
	void Shutdown ();

	void CopyAndClip (SWORD *buffer, DWORD count, DWORD start);
	void UpdateSound ();
	void AddChannel8 (Channel *chan, DWORD count);
	void AddChannel16 (Channel *chan, DWORD count);
	void AddStream8 (Stream *chan, DWORD count);
	void AddStream16 (Stream *chan, DWORD count);
	static DWORD WINAPI MixerThreadFunc (LPVOID param);

	static SQWORD MixMono8 (SDWORD *dest, const SBYTE *src, DWORD count, SQWORD pos, SQWORD step, int leftvol, int rightvol);
	static SQWORD MixMono16 (SDWORD *dest, const SWORD *src, DWORD count, SQWORD pos, SQWORD step, int leftvol, int rightvol);
	static SQWORD MixStereo8 (SDWORD *dest, const SBYTE *src, DWORD count, SQWORD pos, SQWORD step, int vol);
	static SQWORD MixStereo16 (SDWORD *dest, const SWORD *src, DWORD count, SQWORD pos, SQWORD step, int vol);
};

class AltSoundStream : public SoundStream
{
};

#endif
