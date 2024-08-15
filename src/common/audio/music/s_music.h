
#ifndef __S_MUSIC__
#define __S_MUSIC__

#include "zstring.h"
#include "tarray.h"
#include "name.h"
#include "files.h"
#include <zmusic.h>

class SoundStream;


enum MusicCustomStreamType : bool {
	MusicSamples16bit,
	MusicSamplesFloat
};
int MusicEnabled();
typedef bool(*StreamCallback)(SoundStream* stream, void* buff, int len, void* userdata);
SoundStream *S_CreateCustomStream(size_t size, int samplerate, int numchannels, MusicCustomStreamType sampletype, StreamCallback cb, void *userdata);
void S_StopCustomStream(SoundStream* stream);
void S_PauseAllCustomStreams(bool on);

struct MusicCallbacks
{
	FString(*LookupFileName)(const char* fn, int &order);
	int(*FindMusic)(const char* fn);
};
void S_SetMusicCallbacks(MusicCallbacks* cb);

void S_CreateStream();
void S_PauseStream(bool pause);
void S_StopStream();
void S_SetStreamVolume(float vol);


//
void S_InitMusic ();
void S_ResetMusic ();


// Start music using <music_name>
bool S_StartMusic (const char *music_name);

// Start music using <music_name>, and set whether looping
bool S_ChangeMusic (const char *music_name, int order=0, bool looping=true, bool force=false);

// Check if <music_name> exists
bool MusicExists(const char* music_name);

void S_RestartMusic ();
void S_MIDIDeviceChanged(int newdev);

int S_GetMusic (const char **name);

// Stops the music for sure.
void S_StopMusic (bool force);

// Stop and resume music, during game PAUSE.
void S_PauseMusic ();
void S_ResumeMusic ();

//
// Updates music & sounds
//
void S_UpdateMusic ();

struct MidiDeviceSetting
{
	int device;
	FString args;
};

typedef TMap<int, MidiDeviceSetting> MidiDeviceMap;
typedef TMap<int, float> MusicVolumeMap;

extern MidiDeviceMap MidiDevices;
extern MusicVolumeMap MusicVolumes;
extern MusicCallbacks mus_cb;

struct MusPlayingInfo
{
	FString name;
	ZMusic_MusicStream handle;
	int   lumpnum;
	int   baseorder;
	float musicVolume;
	bool  loop;
	bool isfloat;
	FString	 LastSong;			// last music that was played
	FString hash;				// for setting replay gain while playing.
};

extern MusPlayingInfo mus_playing;

extern float relative_volume, saved_relative_volume;


#endif
