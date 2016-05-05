// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// DESCRIPTION:
//		The not so system specific sound interface.
//
//-----------------------------------------------------------------------------


#ifndef __S_SOUND__
#define __S_SOUND__

#include "doomtype.h"
#include "i_soundinternal.h"
#include "dobject.h"

class AActor;
class FScanner;

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

	BYTE		PitchMask;
	SWORD		NearLimit;				// 0 means unlimited
	float		LimitRange;				// Range for sound limiting (squared for faster computations)

	WORD		bRandomHeader:1;
	WORD		bPlayerReserve:1;
	WORD		bLoadRAW:1;
	WORD		bPlayerCompat:1;
	WORD		b16bit:1;
	WORD		bUsed:1;
	WORD		bSingular:1;
	WORD		bTentative:1;
	WORD		bPlayerSilent:1;		// This player sound is intentionally silent.

	WORD		RawRate;				// Sample rate to use when bLoadRAW is true

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

int S_FindSound (const char *logicalname);

// the complete set of sound effects
extern TArray<sfxinfo_t> S_sfx;

// An index into the S_sfx[] array.
class FSoundID
{
public:
	FSoundID()
	{
		ID = 0;
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
	FSoundID(const FSoundID &other)
	{
		ID = other.ID;
	}
	FSoundID &operator=(const FSoundID &other)
	{
		ID = other.ID;
		return *this;
	}
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

	FSoundID &operator=(const FSoundID &other)
	{
		return FSoundID::operator=(other);
	}
	FSoundID &operator=(const char *name)
	{
		return FSoundID::operator=(name);
	}
	FSoundID &operator=(const FString &name)
	{
		return FSoundID::operator=(name);
	}
};

FArchive &operator<<(FArchive &arc, FSoundID &sid);

extern FRolloffInfo S_Rolloff;
extern BYTE *S_SoundCurve;
extern int S_SoundCurveSize;

// Information about one playing sound.
struct sector_t;
struct FPolyObj;
struct FSoundChan : public FISoundChannel
{
	FSoundChan	*NextChan;	// Next channel in this list.
	FSoundChan **PrevChan;	// Previous channel in this list.
	FSoundID	SoundID;	// Sound ID of playing sound.
	FSoundID	OrgID;		// Sound ID of sound used to start this channel.
	float		Volume;
	int			ChanFlags;
	SWORD		Pitch;		// Pitch variation.
	BYTE		EntChannel;	// Actor's sound channel.
	SBYTE		Priority;
	SWORD		NearLimit;
	BYTE		SourceType;
	float		LimitRange;
	union
	{
		AActor			*Actor;		// Used for position and velocity.
		const sector_t	*Sector;	// Sector for area sounds.
		const FPolyObj	*Poly;		// Polyobject sound source.
		float			 Point[3];	// Sound is not attached to any source.
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

// Per level startup code.
// Kills playing sounds at start of level and starts new music.
//
void S_Start ();

// Called after a level is loaded. Ensures that most sounds are loaded.
void S_PrecacheLevel ();

// Loads a sound, including any random sounds it might reference.
void S_CacheSound (sfxinfo_t *sfx);

// Start sound for thing at <ent>
void S_Sound (int channel, FSoundID sfxid, float volume, float attenuation);
void S_Sound (AActor *ent, int channel, FSoundID sfxid, float volume, float attenuation);
void S_SoundMinMaxDist (AActor *ent, int channel, FSoundID sfxid, float volume, float mindist, float maxdist);
void S_Sound (const FPolyObj *poly, int channel, FSoundID sfxid, float volume, float attenuation);
void S_Sound (const sector_t *sec, int channel, FSoundID sfxid, float volume, float attenuation);
void S_Sound(const DVector3 &pos, int channel, FSoundID sfxid, float volume, float attenuation);

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

#define CHAN_AUTO				0
#define CHAN_WEAPON				1
#define CHAN_VOICE				2
#define CHAN_ITEM				3
#define CHAN_BODY				4

// Channel alias for sector sounds. These define how listener height is
// used when calculating 3D sound volume.
#define CHAN_FLOOR				1	// Sound comes from the floor.
#define CHAN_CEILING			2	// Sound comes from the ceiling.
#define CHAN_FULLHEIGHT			3	// Sound comes entire height of the sector.
#define CHAN_INTERIOR			4	// Sound comes height between floor and ceiling.

// modifier flags
#define CHAN_LISTENERZ			8
#define CHAN_MAYBE_LOCAL		16
#define CHAN_UI					32	// Do not record sound in savegames.
#define CHAN_NOPAUSE			64	// Do not pause this sound in menus.
#define CHAN_AREA				128	// Sound plays from all around. Only valid with sector sounds.
#define CHAN_LOOP				256

#define CHAN_PICKUP				(CHAN_ITEM|CHAN_MAYBE_LOCAL)

#define CHAN_IS3D				1	// internal: Sound is 3D.
#define CHAN_EVICTED			2	// internal: Sound was evicted.
#define CHAN_FORGETTABLE		4	// internal: Forget channel data when sound stops.
#define CHAN_JUSTSTARTED		512	// internal: Sound has not been updated yet.
#define CHAN_ABSTIME			1024// internal: Start time is absolute and does not depend on current time.
#define CHAN_VIRTUAL			2048// internal: Channel is currently virtual

// sound attenuation values
#define ATTN_NONE				0.f	// full volume the entire level
#define ATTN_NORM				1.f
#define ATTN_IDLE				1.001f
#define ATTN_STATIC				3.f	// diminish very rapidly with distance

int S_PickReplacement (int refid);
void S_CacheRandomSound (sfxinfo_t *sfx);

// Checks if a copy of this sound is already playing.
bool S_CheckSingular (int sound_id);

// Stops a sound emanating from one of an emitter's channels.
void S_StopSound (AActor *ent, int channel);
void S_StopSound (const sector_t *sec, int channel);
void S_StopSound (const FPolyObj *poly, int channel);

// Stops an origin-less sound from playing from this channel.
void S_StopSound (int channel);

// Stop sound for all channels
void S_StopAllChannels (void);

// Is the sound playing on one of the emitter's channels?
bool S_GetSoundPlayingInfo (const AActor *actor, int sound_id);
bool S_GetSoundPlayingInfo (const sector_t *sector, int sound_id);
bool S_GetSoundPlayingInfo (const FPolyObj *poly, int sound_id);

bool S_IsActorPlayingSomething (AActor *actor, int channel, int sound_id);

// Change a playing sound's volume
bool S_ChangeSoundVolume(AActor *actor, int channel, float volume);

// Moves all sounds from one mobj to another
void S_RelinkSound (AActor *from, AActor *to);

// Stores/retrieves playing channel information in an archive.
void S_SerializeSounds(FArchive &arc);

// Start music using <music_name>
bool S_StartMusic (const char *music_name);

// Start music using <music_name>, and set whether looping
bool S_ChangeMusic (const char *music_name, int order=0, bool looping=true, bool force=false);

// Start playing a cd track as music
bool S_ChangeCDMusic (int track, unsigned int id=0, bool looping=true);

void S_RestartMusic ();

void S_MIDIDeviceChanged();

int S_GetMusic (char **name);

// Stops the music for sure.
void S_StopMusic (bool force);

// Stop and resume music, during game PAUSE.
void S_PauseSound (bool notmusic, bool notsfx);
void S_ResumeSound (bool notsfx);
void S_SetSoundPaused (int state);

//
// Updates music & sounds
//
void S_UpdateSounds (AActor *listener);

void S_RestoreEvictedChannels();

// [RH] S_sfx "maintenance" routines
void S_ParseSndInfo (bool redefine);
void S_ParseReverbDef ();
void S_UnloadReverbDef ();

void S_HashSounds ();
int S_FindSoundNoHash (const char *logicalname);
bool S_AreSoundsEquivalent (AActor *actor, int id1, int id2);
bool S_AreSoundsEquivalent (AActor *actor, const char *name1, const char *name2);
int S_LookupPlayerSound (const char *playerclass, int gender, const char *logicalname);
int S_LookupPlayerSound (const char *playerclass, int gender, FSoundID refid);
int S_FindSkinnedSound (AActor *actor, FSoundID refid);
int S_FindSkinnedSoundEx (AActor *actor, const char *logicalname, const char *extendedname);
int S_FindSoundByLump (int lump);
int S_AddSound (const char *logicalname, const char *lumpname, FScanner *sc=NULL);	// Add sound by lumpname
int S_AddSoundLump (const char *logicalname, int lump);	// Add sound by lump index
int S_AddPlayerSound (const char *playerclass, const int gender, int refid, const char *lumpname);
int S_AddPlayerSound (const char *playerclass, const int gender, int refid, int lumpnum, bool fromskin=false);
int S_AddPlayerSoundExisting (const char *playerclass, const int gender, int refid, int aliasto, bool fromskin=false);
void S_MarkPlayerSounds (const char *playerclass);
void S_ShrinkPlayerSoundLists ();
void S_UnloadSound (sfxinfo_t *sfx);
sfxinfo_t *S_LoadSound(sfxinfo_t *sfx);
unsigned int S_GetMSLength(FSoundID sound);
void S_ParseMusInfo();
bool S_ParseTimeTag(const char *tag, bool *as_samples, unsigned int *time);

// [RH] Prints sound debug info to the screen.
//		Modelled after Hexen's noise cheat.
void S_NoiseDebug ();

extern ReverbContainer *Environments;
extern ReverbContainer *DefaultEnvironments[26];

class FArchive;
FArchive &operator<< (FArchive &arc, ReverbContainer *&env);

void S_SetEnvironment (const ReverbContainer *settings);
ReverbContainer *S_FindEnvironment (const char *name);
ReverbContainer *S_FindEnvironment (int id);
void S_AddEnvironment (ReverbContainer *settings);

enum EMidiDevice
{
	MDEV_DEFAULT = -1,
	MDEV_MMAPI = 0,
	MDEV_OPL = 1,
	MDEV_SNDSYS = 2,
	MDEV_TIMIDITY = 3,
	MDEV_FLUIDSYNTH = 4,
	MDEV_GUS = 5,
	MDEV_WILDMIDI = 6,
};

struct MidiDeviceSetting
{
	int device;
	FString args;

	MidiDeviceSetting()
	{
		device = MDEV_DEFAULT;
	}
};

typedef TMap<FName, FName> MusicAliasMap;
typedef TMap<FName, MidiDeviceSetting> MidiDeviceMap;

extern MusicAliasMap MusicAliases;
extern MidiDeviceMap MidiDevices;

#endif
