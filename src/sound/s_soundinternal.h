#pragma once

#include "i_soundinternal.h"


//
// SoundFX struct.
//
struct sfxinfo_t
{
	// Next field is for use by the system sound interface.
	// A non-null data means the sound has been loaded.
	SoundHandle	data;
    // Also for the sound interface. Used for 3D positional
    // sounds, may be the same as data.
    SoundHandle data3d;

	FString		name;					// [RH] Sound name defined in SNDINFO
	int 		lumpnum;				// lump number of sfx

	unsigned int next, index;			// [RH] For hashing
	float		Volume;

	uint8_t		ResourceId;				// Resource ID as implemented by Blood. Not used by Doom but added for completeness.
	uint8_t		PitchMask;
	int16_t		NearLimit;				// 0 means unlimited
	float		LimitRange;				// Range for sound limiting (squared for faster computations)

	unsigned		bRandomHeader:1;
	unsigned		bPlayerReserve:1;
	unsigned		bLoadRAW:1;
	unsigned		bPlayerCompat:1;
	unsigned		b16bit:1;
	unsigned		bUsed:1;
	unsigned		bSingular:1;
	unsigned		bTentative:1;
	unsigned		bPlayerSilent:1;		// This player sound is intentionally silent.

	int		RawRate;				// Sample rate to use when bLoadRAW is true

	int			LoopStart;				// -1 means no specific loop defined

	unsigned int link;
	enum { NO_LINK = 0xffffffff };

	FRolloffInfo	Rolloff;
	float		Attenuation;			// Multiplies the attenuation passed to S_Sound.

	void		MarkUsed();				// Marks this sound as used.
};

// Rolloff types
enum
{
	ROLLOFF_Doom,		// Linear rolloff with a logarithmic volume scale
	ROLLOFF_Linear,		// Linear rolloff with a linear volume scale
	ROLLOFF_Log,		// Logarithmic rolloff (standard hardware type)
	ROLLOFF_Custom		// Lookup volume from SNDCURVE
};

int S_FindSound(const char *logicalname);
int S_FindSoundByResID(int snd_id);

// the complete set of sound effects
extern TArray<sfxinfo_t> S_sfx;

// An index into the S_sfx[] array.
class FSoundID
{
public:
	FSoundID() = default;
	
	static FSoundID byResId(int ndx)
	{
		return FSoundID(S_FindSoundByResID(ndx)); 
	}
	FSoundID(int id)
	{
		ID = id;
	}
	FSoundID(const char *name)
	{
		ID = S_FindSound(name);
	}
	FSoundID(const FString &name)
	{
		ID = S_FindSound(name.GetChars());
	}
	FSoundID(const FSoundID &other) = default;
	FSoundID &operator=(const FSoundID &other) = default;
	FSoundID &operator=(const char *name)
	{
		ID = S_FindSound(name);
		return *this;
	}
	FSoundID &operator=(const FString &name)
	{
		ID = S_FindSound(name.GetChars());
		return *this;
	}
	bool operator !=(FSoundID other) const
	{
		return ID != other.ID;
	}
	bool operator !=(int other) const
	{
		return ID != other;
	}
	operator int() const
	{
		return ID;
	}
	operator FString() const
	{
		return ID ? S_sfx[ID].name : "";
	}
	operator const char *() const
	{
		return ID ? S_sfx[ID].name.GetChars() : NULL;
	}
	void MarkUsed() const
	{
		S_sfx[ID].MarkUsed();
	}
private:
	int ID;
protected:
	enum EDummy { NoInit };
	FSoundID(EDummy) {}
};
 
 class FSoundIDNoInit : public FSoundID
{
public:
	FSoundIDNoInit() : FSoundID(NoInit) {}
	using FSoundID::operator=;
};

extern FRolloffInfo S_Rolloff;
extern TArray<uint8_t> S_SoundCurve;


class AActor;
struct sector_t;
struct FPolyObj;
struct FSoundChan : public FISoundChannel
{
	FSoundChan	*NextChan;	// Next channel in this list.
	FSoundChan **PrevChan;	// Previous channel in this list.
	FSoundID	SoundID;	// Sound ID of playing sound.
	FSoundID	OrgID;		// Sound ID of sound used to start this channel.
	float		Volume;
	int16_t		Pitch;		// Pitch variation.
	uint8_t		EntChannel;	// Actor's sound channel.
	int8_t		Priority;
	int16_t		NearLimit;
	uint8_t		SourceType;
	float		LimitRange;
	union
	{
		const void *Source;
		float Point[3];	// Sound is not attached to any source.
	};
};

extern FSoundChan *Channels;

void S_ReturnChannel(FSoundChan *chan);
void S_EvictAllChannels();

void S_StopChannel(FSoundChan *chan);
void S_LinkChannel(FSoundChan *chan, FSoundChan **head);
void S_UnlinkChannel(FSoundChan *chan);

// Initializes sound stuff, including volume
// Sets channels, SFX and music volume,
//	allocates channel buffer, sets S_sfx lookup.
//
void S_Init ();
void S_InitData ();
void S_Shutdown ();

// Loads a sound, including any random sounds it might reference.
void S_CacheSound (sfxinfo_t *sfx);

// sound channels
// channel 0 never willingly overrides
// other channels (1-7) always override a playing sound on that channel
//
// CHAN_AUTO searches down from channel 7 until it finds a channel not in use
// CHAN_WEAPON is for weapons
// CHAN_VOICE is for oof, sight, or other voice sounds
// CHAN_ITEM is for small things and item pickup
// CHAN_BODY is for generic body sounds
// CHAN_PICKUP can optionally be set as a local sound only for "compatibility"

enum
{
	CHAN_AUTO				= 0,
	CHAN_WEAPON				= 1,
	CHAN_VOICE				= 2,
	CHAN_ITEM				= 3,
	CHAN_BODY				= 4,
	CHAN_5					= 5,
	CHAN_6					= 6,
	CHAN_7					= 7,

	// Channel alias for sector sounds. These define how listener height is
	// used when calculating 3D sound volume.
	CHAN_FLOOR				= 1,	// Sound comes from the floor.
	CHAN_CEILING			= 2,	// Sound comes from the ceiling.
	CHAN_FULLHEIGHT			= 3,	// Sound comes entire height of the sector.
	CHAN_INTERIOR			= 4,	// Sound comes height between floor and ceiling.

	// modifier flags
	CHAN_LISTENERZ			= 8,
	CHAN_MAYBE_LOCAL		= 16,
	CHAN_UI					= 32,	// Do not record sound in savegames.
	CHAN_NOPAUSE			= 64,	// Do not pause this sound in menus.
	CHAN_AREA				= 128,	// Sound plays from all around. Only valid with sector sounds.
	CHAN_LOOP				= 256,

	CHAN_PICKUP				= (CHAN_ITEM|CHAN_MAYBE_LOCAL),

	CHAN_IS3D				= 1,		// internal: Sound is 3D.
	CHAN_EVICTED			= 2,		// internal: Sound was evicted.
	CHAN_FORGETTABLE		= 4,		// internal: Forget channel data when sound stops.
	CHAN_JUSTSTARTED		= 512,	// internal: Sound has not been updated yet.
	CHAN_ABSTIME			= 1024,	// internal: Start time is absolute and does not depend on current time.
	CHAN_VIRTUAL			= 2048,	// internal: Channel is currently virtual
	CHAN_NOSTOP				= 4096,	// only for A_PlaySound. Does not start if channel is playing something.
};

// sound attenuation values
#define ATTN_NONE				0.f	// full volume the entire level
#define ATTN_NORM				1.f
#define ATTN_IDLE				1.001f
#define ATTN_STATIC				3.f	// diminish very rapidly with distance

// Checks if a copy of this sound is already playing.
bool S_CheckSingular (int sound_id);

enum // This cannot be remain as this, but for now it has to suffice.
{
	SOURCE_None,		// Sound is always on top of the listener.
	SOURCE_Actor,		// Sound is coming from an actor.
	SOURCE_Sector,		// Sound is coming from a sector.
	SOURCE_Polyobj,		// Sound is coming from a polyobject.
	SOURCE_Unattached,	// Sound is not attached to any particular emitter.
};


//
// Updates music & sounds
//
void S_UpdateSounds (int time);

FSoundChan* S_StartSound(int sourcetype, const void* source,
	const FVector3* pt, int channel, FSoundID sound_id, float volume, float attenuation, FRolloffInfo* rolloff = nullptr, float spitch = 0.0f);

// Stops an origin-less sound from playing from this channel.
void S_StopSound(int channel);
void S_StopSound(int sourcetype, const void* actor, int channel);

void S_RelinkSound(int sourcetype, const void* from, const void* to, const FVector3* optpos);
void S_ChangeSoundVolume(int sourcetype, const void *source, int channel, double dvolume);
void S_ChangeSoundPitch(int sourcetype, const void *source, int channel, double pitch);
bool S_IsSourcePlayingSomething (int sourcetype, const void *actor, int channel, int sound_id);

// Stop and resume music, during game PAUSE.
void S_PauseSound (bool notmusic, bool notsfx);
void S_ResumeSound (bool notsfx);
void S_SetSoundPaused (int state);
bool S_GetSoundPlayingInfo(int sourcetype, const void* source, int sound_id);
void S_UnloadAllSounds();
void S_Reset();

extern ReverbContainer *Environments;
extern ReverbContainer *DefaultEnvironments[26];

void S_ParseReverbDef ();
void S_UnloadReverbDef ();
void S_SetEnvironment (const ReverbContainer *settings);
ReverbContainer *S_FindEnvironment (const char *name);
ReverbContainer *S_FindEnvironment (int id);
void S_AddEnvironment (ReverbContainer *settings);
	
