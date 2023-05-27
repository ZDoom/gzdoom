#pragma once

#include "i_sound.h"
#include "name.h"

enum
{
	sfx_empty = -1
};



// Rolloff types
enum
{
	ROLLOFF_Doom,		// Linear rolloff with a logarithmic volume scale
	ROLLOFF_Linear,		// Linear rolloff with a linear volume scale
	ROLLOFF_Log,		// Logarithmic rolloff (standard hardware type)
	ROLLOFF_Custom		// Lookup volume from SNDCURVE
};


// An index into the S_sfx[] array.
class FSoundID
{
public:
	FSoundID() = default;

private:
	constexpr FSoundID(int id) : ID(id)
	{
	}
public:
	static constexpr FSoundID fromInt(int i)
	{
		return FSoundID(i);
	}
	FSoundID(const FSoundID &other) = default;
	FSoundID &operator=(const FSoundID &other) = default;
	bool operator !=(FSoundID other) const
	{
		return ID != other.ID;
	}
	bool operator ==(FSoundID other) const
	{
		return ID == other.ID;
	}
	bool operator ==(int other) const = delete;
	bool operator !=(int other) const = delete;
	constexpr int index() const
	{
		return ID;
	}
	constexpr bool isvalid() const
	{
		return ID > 0;
	}
private:

	int ID;
};

constexpr FSoundID NO_SOUND = FSoundID::fromInt(0);
constexpr FSoundID INVALID_SOUND = FSoundID::fromInt(-1);

 struct FRandomSoundList
 {
	 TArray<FSoundID> Choices;
	 FSoundID Owner = NO_SOUND;
 };


 //
// SoundFX struct.
//
 struct sfxinfo_t
 {
	 // Next field is for use by the system sound interface.
	 // A non-null data means the sound has been loaded.
	 SoundHandle	data{};

	 FName		name;								// [RH] Sound name defined in SNDINFO
	 int 		lumpnum = sfx_empty;				// lump number of sfx

	 float		Volume = 1.f;

	 int			ResourceId = -1;					// Resource ID as implemented by Blood. Not used by Doom but added for completeness.
	 float		LimitRange = 256 * 256;				// Range for sound limiting (squared for faster computations)
	 float		DefPitch = 0.f;						// A defined pitch instead of a random one the sound plays at, similar to A_StartSound.
	 float		DefPitchMax = 0.f;					// Randomized range with stronger control over pitch itself.

	 int16_t		NearLimit = 4;						// 0 means unlimited.
	 int16_t		UserVal = 0;					// repurpose this gap for something useful
	 uint8_t		PitchMask = 0;
	 bool		bRandomHeader = false;
	 bool		bLoadRAW = false;
	 bool		b16bit = false;
	 bool		bUsed = false;
	 bool		bSingular = false;
	 bool		bTentative = true;
	 bool		bExternal = false;

	 int			RawRate = 0;				// Sample rate to use when bLoadRAW is true
	 int			LoopStart = -1;				// -1 means no specific loop defined
	 int			LoopEnd = -1;				// -1 means no specific loop defined
	 float		Attenuation = 1.f;			// Multiplies the attenuation passed to S_Sound.

	 FSoundID link = NO_LINK;
	 constexpr static FSoundID NO_LINK = FSoundID::fromInt(-1);

	 TArray<int> UserData;
	 FRolloffInfo	Rolloff{};
 };


struct FSoundChan : public FISoundChannel
{
	FSoundChan	*NextChan;	// Next channel in this list.
	FSoundChan **PrevChan;	// Previous channel in this list.
	FSoundID	SoundID;	// Sound ID of playing sound.
	FSoundID	OrgID;		// Sound ID of sound used to start this channel.
	float		Volume;
	int 		EntChannel;	// Actor's sound channel.
	int			UserData;	// Not used by the engine, the caller can use this to store some additional info.
	float		Pitch;		// Pitch variation.
	int16_t		NearLimit;
	int8_t		Priority;
	uint8_t		SourceType;
	float		LimitRange;
	const void *Source;
	float Point[3];	// Sound is not attached to any source.
};


// sound channels
// channel 0 never willingly overrides
// other channels (1-7) always override a playing sound on that channel
//
// CHAN_AUTO searches down from channel 7 until it finds a channel not in use
// CHAN_WEAPON is for weapons
// CHAN_VOICE is for oof, sight, or other voice sounds
// CHAN_ITEM is for small things and item pickup
// CHAN_BODY is for generic body sounds

enum EChannel
{
	CHAN_AUTO = 0,
	CHAN_WEAPON = 1,
	CHAN_VOICE = 2,
	CHAN_ITEM = 3,
	CHAN_BODY = 4,
	CHAN_5 = 5,
	CHAN_6 = 6,
	CHAN_7 = 7,
};



// sound attenuation values
#define ATTN_NONE				0.f	// full volume the entire level
#define ATTN_NORM				1.f
#define ATTN_IDLE				1.001f
#define ATTN_STATIC				3.f	// diminish very rapidly with distance

enum // The core source types, implementations may extend this list as they see fit.
{
	SOURCE_Any = -1,	// Input for check functions meaning 'any source'
	SOURCE_Unattached,	// Sound is not attached to any particular emitter.
	SOURCE_None,		// Sound is always on top of the listener.
};


extern ReverbContainer *Environments;
extern ReverbContainer *DefaultEnvironments[26];

void S_ParseReverbDef ();
void S_UnloadReverbDef ();
void S_SetEnvironment (const ReverbContainer *settings);
ReverbContainer *S_FindEnvironment (const char *name);
ReverbContainer *S_FindEnvironment (int id);
void S_AddEnvironment (ReverbContainer *settings);

class SoundEngine
{
protected:
	bool SoundPaused = false;		// whether sound is paused
	int RestartEvictionsAt = 0;	// do not restart evicted channels before this time
	SoundListener listener{};

	FSoundChan* Channels = nullptr;
	FSoundChan* FreeChannels = nullptr;

	// the complete set of sound effects
	TArray<sfxinfo_t> S_sfx;
	FRolloffInfo S_Rolloff{};
	TArray<uint8_t> S_SoundCurve;
	TMap<FName, FSoundID> SoundMap;
	TMap<int, FSoundID> ResIdMap;
	TArray<FRandomSoundList> S_rnd;
	bool blockNewSounds = false;

private:
	void LinkChannel(FSoundChan* chan, FSoundChan** head);
	void UnlinkChannel(FSoundChan* chan);
	void ReturnChannel(FSoundChan* chan);
	void RestartChannel(FSoundChan* chan);
	void RestoreEvictedChannel(FSoundChan* chan);

	bool IsChannelUsed(int sourcetype, const void* actor, int channel, int* seen);
	// This is the actual sound positioning logic which needs to be provided by the client.
	virtual void CalcPosVel(int type, const void* source, const float pt[3], int channel, int chanflags, FSoundID chanSound, FVector3* pos, FVector3* vel, FSoundChan *chan) = 0;
	// This can be overridden by the clent to provide some diagnostics. The default lets everything pass.
	virtual bool ValidatePosVel(int sourcetype, const void* source, const FVector3& pos, const FVector3& vel) { return true; }

	bool ValidatePosVel(const FSoundChan* const chan, const FVector3& pos, const FVector3& vel);

	// Checks if a copy of this sound is already playing.
	bool CheckSingular(FSoundID sound_id);
	virtual TArray<uint8_t> ReadSound(int lumpnum) = 0;

protected:
	virtual bool CheckSoundLimit(sfxinfo_t* sfx, const FVector3& pos, int near_limit, float limit_range, int sourcetype, const void* actor, int channel, float attenuation);
	virtual FSoundID ResolveSound(const void *ent, int srctype, FSoundID soundid, float &attenuation);

public:
	virtual ~SoundEngine()
	{
		Shutdown();
	}
	void EvictAllChannels();

	void BlockNewSounds(bool on)
	{
		blockNewSounds = on;
	}

	virtual void StopChannel(FSoundChan* chan);
	sfxinfo_t* LoadSound(sfxinfo_t* sfx);
	sfxinfo_t* GetWritableSfx(FSoundID snd)
	{
		if ((unsigned)snd.index() >= S_sfx.Size()) return nullptr;
		return &S_sfx[snd.index()];
	}

	const sfxinfo_t* GetSfx(FSoundID snd)
	{
		return GetWritableSfx(snd);
	}

	unsigned GetNumSounds() const
	{
		return S_sfx.Size();
	}

	sfxinfo_t* AllocateSound()
	{
		return &S_sfx[S_sfx.Reserve(1)];
	}

	// Initializes sound stuff, including volume
	// Sets channels, SFX and music volume,
	//	allocates channel buffer, sets S_sfx lookup.
	//
	void Init(TArray<uint8_t> &sndcurve);
	void InitData();
	void Clear();
	void Shutdown();

	void StopAllChannels(void);
	void SetPitch(FSoundChan* chan, float dpitch);
	void SetVolume(FSoundChan* chan, float vol);

	FSoundChan* GetChannel(void* syschan);
	void RestoreEvictedChannels();
	void CalcPosVel(FSoundChan* chan, FVector3* pos, FVector3* vel);

	// Loads a sound, including any random sounds it might reference.
	virtual void CacheSound(sfxinfo_t* sfx);
	void CacheSound(FSoundID sfx) { CacheSound(&S_sfx[sfx.index()]); }
	void UnloadSound(sfxinfo_t* sfx);
	void UnloadSound(int sfx)
	{
		UnloadSound(&S_sfx[sfx]);
	}

	void UpdateSounds(int time);

	FSoundChan* StartSound(int sourcetype, const void* source,
		const FVector3* pt, int channel, EChanFlags flags, FSoundID sound_id, float volume, float attenuation, FRolloffInfo* rolloff = nullptr, float spitch = 0.0f, float startTime = 0.0f);

	// Stops an origin-less sound from playing from this channel.
	void StopSoundID(FSoundID sound_id);
	void StopSound(int channel, FSoundID sound_id = INVALID_SOUND);
	void StopSound(int sourcetype, const void* actor, int channel, FSoundID sound_id = INVALID_SOUND);
	void StopActorSounds(int sourcetype, const void* actor, int chanmin, int chanmax);

	void RelinkSound(int sourcetype, const void* from, const void* to, const FVector3* optpos);
	void ChangeSoundVolume(int sourcetype, const void* source, int channel, double dvolume);
	void ChangeSoundPitch(int sourcetype, const void* source, int channel, double pitch, FSoundID sound_id = INVALID_SOUND);
	bool IsSourcePlayingSomething(int sourcetype, const void* actor, int channel, FSoundID sound_id = INVALID_SOUND);

	// Stop and resume music, during game PAUSE.
	int GetSoundPlayingInfo(int sourcetype, const void* source, FSoundID sound_id, int chan = -1);
	void UnloadAllSounds();
	void Reset();
	void MarkUsed(FSoundID num);
	void CacheMarkedSounds();
	TArray<FSoundChan*> AllActiveChannels();
	virtual void SetSoundPaused(int state) {}

	void MarkAllUnused()
	{
		for (auto & s: S_sfx) s.bUsed = false;
	}

	bool isListener(const void* object) const
	{
		return object && listener.ListenerObject == object;
	}
	void SetListener(SoundListener& l)
	{
		listener = l;
	}
	const SoundListener& GetListener() const
	{
		return listener;
	}
	void SetRestartTime(int time)
	{
		RestartEvictionsAt = time;
	}
	void SetPaused(bool on)
	{
		SoundPaused = on;
	}
	FSoundChan* GetChannels()
	{
		return Channels;
	}
	const char *GetSoundName(FSoundID id)
	{
		return !id.isvalid() ? "" : S_sfx[id.index()].name.GetChars();
	}
	FRolloffInfo& GlobalRolloff() // this is meant for sound list generators, not for gaining cheap access to the sound engine's innards.
	{
		return S_Rolloff;
	}
	FRandomSoundList *ResolveRandomSound(sfxinfo_t* sfx)
	{
		return &S_rnd[sfx->link.index()];
	}
	void ClearRandoms()
	{
		S_rnd.Clear();
	}
	int *GetUserData(FSoundID snd)
	{
		return S_sfx[snd.index()].UserData.Data();
	}
	bool isValidSoundId(FSoundID sid)
	{
		int id = sid.index();
		return id > 0 && id < (int)S_sfx.Size() && !S_sfx[id].bTentative && (S_sfx[id].lumpnum != sfx_empty || S_sfx[id].bRandomHeader || S_sfx[id].link != sfxinfo_t::NO_LINK);
	}

	template<class func> bool EnumerateChannels(func callback)
	{
		FSoundChan* chan = Channels;
		while (chan)
		{
			auto next = chan->NextChan;
			int res = callback(chan);
			if (res) return res > 0;
			chan = next;
		}
		return false;
	}

	void SetDefaultRolloff(FRolloffInfo* ro)
	{
		S_Rolloff = *ro;
	}

	void ChannelVirtualChanged(FISoundChannel* ichan, bool is_virtual);
	FString ListSoundChannels();

	// Allow this to be overridden for special needs.
	virtual float GetRolloff(const FRolloffInfo* rolloff, float distance);
	virtual void ChannelEnded(FISoundChannel* ichan); // allows the client to do bookkeeping on the sound.
	virtual void SoundDone(FISoundChannel* ichan); // gets called when the sound has been completely taken down.

	// Lookup utilities.
	FSoundID FindSound(const char* logicalname);
	FSoundID FindSoundByResID(int rid);
	FSoundID FindSoundNoHash(const char* logicalname);
	FSoundID FindSoundByResIDNoHash(int rid);
	FSoundID FindSoundByLump(int lump);
	virtual FSoundID AddSoundLump(const char* logicalname, int lump, int CurrentPitchMask, int resid = -1, int nearlimit = 2);
	FSoundID FindSoundTentative(const char* name, int nearlimit = 2);
	void CacheRandomSound(sfxinfo_t* sfx);
	unsigned int GetMSLength(FSoundID sound);
	FSoundID PickReplacement(FSoundID refid);
	void HashSounds();
	void AddRandomSound(FSoundID Owner, TArray<FSoundID> list);
};


extern SoundEngine* soundEngine;

struct FReverbField
{
	int Min, Max;
	float REVERB_PROPERTIES::* Float;
	int REVERB_PROPERTIES::* Int;
	unsigned int Flag;
};


inline FSoundID S_FindSoundByResID(int ndx)
{
	return soundEngine->FindSoundByResID(ndx);
}

inline FSoundID S_FindSound(const char* name)
{
	return soundEngine->FindSound(name);
}

int SoundEnabled();
