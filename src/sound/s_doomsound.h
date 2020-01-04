#pragma once

// Information about one playing sound.
struct sector_t;
struct FPolyObj;
struct FLevelLocals;

void S_Init();
void S_InitData();
void S_Start();
void S_Shutdown();

void S_UpdateSounds(AActor* listenactor);
void S_SetSoundPaused(int state);

void S_PrecacheLevel(FLevelLocals* l);

// Start sound for thing at <ent>
void S_Sound(int channel, EChanFlags flags, FSoundID sfxid, float volume, float attenuation);
void S_SoundPitch(int channel, EChanFlags flags, FSoundID sfxid, float volume, float attenuation, float pitch);


void S_Sound (AActor *ent, int channel, EChanFlags flags, FSoundID sfxid, float volume, float attenuation);
void S_SoundMinMaxDist (AActor *ent, int channel, EChanFlags flags, FSoundID sfxid, float volume, float mindist, float maxdist);
void S_Sound (const FPolyObj *poly, int channel, EChanFlags flags, FSoundID sfxid, float volume, float attenuation);
void S_Sound (const sector_t *sec, int channel, EChanFlags flags, FSoundID sfxid, float volume, float attenuation);
void S_Sound(FLevelLocals *Level, const DVector3 &pos, int channel, EChanFlags flags, FSoundID sfxid, float volume, float attenuation);

void S_SoundPitchActor (AActor *ent, int channel, EChanFlags flags, FSoundID sfxid, float volume, float attenuation, float pitch);

// [Nash] Used by ACS and DECORATE
void S_PlaySound(AActor *a, int chan, EChanFlags flags, FSoundID sid, float vol, float atten);

// Stops a sound emanating from one of an emitter's channels.
void S_StopSound (AActor *ent, int channel);
void S_StopSound (const sector_t *sec, int channel);
void S_StopSound (const FPolyObj *poly, int channel);

// Moves all sounds from one mobj to another
void S_RelinkSound (AActor *from, AActor *to);

// Is the sound playing on one of the emitter's channels?
bool S_GetSoundPlayingInfo (const AActor *actor, int sound_id);
bool S_GetSoundPlayingInfo (const sector_t *sector, int sound_id);
bool S_GetSoundPlayingInfo (const FPolyObj *poly, int sound_id);

int S_IsActorPlayingSomething (AActor *actor, int channel, int sound_id);

// Change a playing sound's volume
void S_ChangeActorSoundVolume(AActor *actor, int channel, double volume);

// Change a playing sound's pitch
void S_ChangeActorSoundPitch(AActor *actor, int channel, double pitch);

// Stores/retrieves playing channel information in an archive.
void S_SerializeSounds(FSerializer &arc);

void A_PlaySound(AActor *self, int soundid, int channel, double volume, int looping, double attenuation, int local, double pitch);
void A_StartSound(AActor* self, int soundid, int channel, int flags, double volume, double attenuation,  double pitch);
static void S_SetListener(AActor *listenactor);
void S_SoundReset();
void S_ResumeSound(bool state);
void S_PauseSound(bool state1, bool state);
void S_NoiseDebug();

inline void S_StopSound(int chan)
{
	soundEngine->StopSound(chan);
}

inline void S_StopAllChannels()
{
	soundEngine->StopAllChannels();
}

inline const char* S_GetSoundName(FSoundID id)
{
	return soundEngine->GetSoundName(id);
}

inline int S_FindSound(const char* logicalname)
{
	return soundEngine->FindSound(logicalname);
}

inline int S_FindSoundByResID(int rid)
{
	return soundEngine->FindSoundByResID(rid);
}
