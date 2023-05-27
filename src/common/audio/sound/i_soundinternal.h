#ifndef __SNDINT_H
#define __SNDINT_H

#include <stdio.h>
#include <stdint.h>

#include "vectors.h"
#include "tarray.h"
#include "tflags.h"

enum EChanFlag
{
	// modifier flags
	CHANF_LISTENERZ = 8,
	CHANF_MAYBE_LOCAL = 16,
	CHANF_UI = 32,	// Do not record sound in savegames.
	CHANF_NOPAUSE = 64,	// Do not pause this sound in menus.
	CHANF_AREA = 128,	// Sound plays from all around. Only valid with sector sounds.
	CHANF_LOOP = 256,

	CHANF_PICKUP = CHANF_MAYBE_LOCAL,

	CHANF_NONE = 0,
	CHANF_IS3D = 1,		// internal: Sound is 3D.
	CHANF_EVICTED = 2,		// internal: Sound was evicted.
	CHANF_FORGETTABLE = 4,		// internal: Forget channel data when sound stops.
	CHANF_JUSTSTARTED = 512,	// internal: Sound has not been updated yet.
	CHANF_ABSTIME = 1024,	// internal: Start time is absolute and does not depend on current time.
	CHANF_VIRTUAL = 2048,	// internal: Channel is currently virtual
	CHANF_NOSTOP = 4096,	// only for A_PlaySound. Does not start if channel is playing something.
	CHANF_OVERLAP = 8192, // [MK] Does not stop any sounds in the channel and instead plays over them.
	CHANF_LOCAL = 16384,	// only plays locally for the calling actor
	CHANF_TRANSIENT = 32768,	// Do not record in savegames - used for sounds that get restarted outside the sound system (e.g. ambients in SW and Blood)
	CHANF_FORCE = 65536,		// Start, even if sound is paused.
	CHANF_SINGULAR = 0x20000,		// Only start if no sound of this name is already playing.
};

typedef TFlags<EChanFlag> EChanFlags;
DEFINE_TFLAGS_OPERATORS(EChanFlags)

class FileReader;

// For convenience, this structure matches FMOD_REVERB_PROPERTIES.
// Since I can't very well #include system-specific stuff in the
// main game files, I duplicate it here.
struct REVERB_PROPERTIES
{                
	int			 Instance;
    int			 Environment;
    float        EnvSize;
    float        EnvDiffusion;
    int          Room;
    int          RoomHF;
    int          RoomLF;
    float        DecayTime;
    float        DecayHFRatio;
    float        DecayLFRatio;
    int          Reflections;
    float        ReflectionsDelay;
    float        ReflectionsPan0;
	float        ReflectionsPan1;
	float        ReflectionsPan2;
    int          Reverb;
    float        ReverbDelay;
    float        ReverbPan0;
	float        ReverbPan1;
	float        ReverbPan2;
    float        EchoTime;
    float        EchoDepth;
    float        ModulationTime;
    float        ModulationDepth;
    float        AirAbsorptionHF;
    float        HFReference;
    float        LFReference;
    float        RoomRolloffFactor;
    float        Diffusion;
    float        Density;
    unsigned int Flags;
};

#define REVERB_FLAGS_DECAYTIMESCALE        0x00000001
#define REVERB_FLAGS_REFLECTIONSSCALE      0x00000002
#define REVERB_FLAGS_REFLECTIONSDELAYSCALE 0x00000004
#define REVERB_FLAGS_REVERBSCALE           0x00000008
#define REVERB_FLAGS_REVERBDELAYSCALE      0x00000010
#define REVERB_FLAGS_DECAYHFLIMIT          0x00000020
#define REVERB_FLAGS_ECHOTIMESCALE         0x00000040
#define REVERB_FLAGS_MODULATIONTIMESCALE   0x00000080

struct ReverbContainer
{
	ReverbContainer *Next;
	const char *Name;
	uint16_t ID;
	bool Builtin;
	bool Modified;
	REVERB_PROPERTIES Properties;
	bool SoftwareWater;
};

struct SoundListener
{
	FVector3 position;
	FVector3 velocity;
	float angle;
	bool underwater;
	bool valid;
	ReverbContainer *Environment;
	void* ListenerObject;
};

// Default rolloff information.
struct FRolloffInfo
{
	int RolloffType;
	float MinDistance;
	union { float MaxDistance; float RolloffFactor; };
};

struct SoundHandle
{
	void *data;

	bool isValid() const { return data != NULL; }
	void Clear() { data = NULL; }

	bool operator==(const SoundHandle &rhs) const
	{ return data == rhs.data; }
	bool operator!=(const SoundHandle &rhs) const
	{ return !(*this == rhs); }
};

struct FISoundChannel
{
	void		*SysChannel;	// Channel information from the system interface.
	uint64_t	StartTime;		// Sound start time in DSP clocks.

	// The sound interface doesn't use these directly but it needs to pass them to a
	// callback that can't be passed a sound channel pointer
	FRolloffInfo Rolloff;
	float		DistanceScale;
	float		DistanceSqr;
	bool		ManualRolloff;
	EChanFlags	ChanFlags;
};

class SoundStream;

void S_SetSoundPaused(int state);


#endif
