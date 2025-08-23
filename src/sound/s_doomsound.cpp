/*
** s_doomsound.cpp
**
** Game dependent part of the sound engine.
**
**---------------------------------------------------------------------------
**
** Copyright 1999-2016 Randy Heit
** Copyright 2002-2019 Christoph Oelckers
** Copyright 2017-2025 GZDoom Maintainers and Contributors
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
**---------------------------------------------------------------------------
**
*/ 

#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <io.h>
#endif

#include "a_sharedglobal.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "cmdlib.h"
#include "d_player.h"
#include "doomstat.h"
#include "filesystem.h"
#include "g_game.h"
#include "g_levellocals.h"
#include "gi.h"
#include "gstrings.h"
#include "i_music.h"
#include "i_sound.h"
#include "i_soundinternal.h"
#include "m_argv.h"
#include "m_random.h"
#include "p_local.h"
#include "po_man.h"
#include "printf.h"
#include "s_music.h"
#include "s_sndseq.h"
#include "s_sound.h"
#include "serializer_doom.h"
#include "v_draw.h"
#include "v_font.h"
#include "vm.h"

// EXTERNAL DATA DEFINITIONS -------------------------------------------------

EXTERN_CVAR(Bool, haptics_do_action);

// PUBLIC DATA DEFINITIONS -------------------------------------------------

static FString LastLocalSndInfo;
static FString LastLocalSndSeq;
void S_AddLocalSndInfo(int lump);

class DoomSoundEngine : public SoundEngine
{
	// client specific parts of the sound engine go in this class.

	void CalcPosVel(int type, const void* source, const float pt[3], int channum, int chanflags, FSoundID soundid, FVector3* pos, FVector3* vel, FSoundChan *) override;
	bool ValidatePosVel(int sourcetype, const void* source, const FVector3& pos, const FVector3& vel);
	TArray<uint8_t> ReadSound(int lumpnum);
	FSoundID PickReplacement(FSoundID refid);
	FSoundID ResolveSound(const void *ent, int type, FSoundID soundid, float &attenuation) override;
	void CacheSound(sfxinfo_t* sfx) override;
	void StopChannel(FSoundChan* chan) override;
	FSoundID AddSoundLump(const char* logicalname, int lump, int CurrentPitchMask, int resid = -1, int nearlimit = 2) override
	{
		auto ndx = SoundEngine::AddSoundLump(logicalname, lump, CurrentPitchMask, resid, nearlimit);
		S_sfx[ndx.index()].UserData.Resize(1);
		S_sfx[ndx.index()].UserData[0] = 0;
		return ndx;
	}
	bool CheckSoundLimit(sfxinfo_t* sfx, const FVector3& pos, int near_limit, float limit_range, int sourcetype, const void* actor, int channel, float attenuation) override
	{
		if (sourcetype != SOURCE_Actor) actor = nullptr; //ZDoom did this.
		return SoundEngine::CheckSoundLimit(sfx, pos, near_limit, limit_range, sourcetype, actor, channel, attenuation);
	}

public:
	DoomSoundEngine() = default;
	void NoiseDebug(void);
	void PrintSoundList();
};

//==========================================================================
//
// LookupMusic
//
// resolves aliases and special names
//
//==========================================================================

static FString LookupMusic(const char* musicname, int& order)
{
	// allow specifying "*" as a placeholder to play the level's default music.
	if (musicname != nullptr && !strcmp(musicname, "*"))
	{
		if (gamestate == GS_LEVEL || gamestate == GS_TITLELEVEL)
		{
			musicname = primaryLevel->Music.GetChars();
			order = primaryLevel->musicorder;
		}
		else
		{
			musicname = nullptr;
		}
	}

	if (musicname == nullptr || musicname[0] == 0)
	{
		// got nothing, return nothing.
		return "";
	}

	if (strnicmp(musicname, ",CD,", 4) == 0)
	{
		static bool warned = false;
		if (!warned)
			Printf(TEXTCOLOR_RED "CD Audio no longer supported\n");
		warned = true;
		return "";
	}

	if (*musicname == '/') musicname++;

	FString DEH_Music;
	if (musicname[0] == '$')
	{
		// handle dehacked replacement.
		// Any music name defined this way needs to be prefixed with 'D_' because
		// Doom.exe does not contain the prefix so these strings don't either.
		const char* mus_string = GStrings.CheckString(musicname + 1);
		if (mus_string != nullptr)
		{
			DEH_Music << "D_" << mus_string;
			musicname = DEH_Music.GetChars();
		}
	}

	FName* aliasp = MusicAliases.CheckKey(musicname);
	if (aliasp != nullptr)
	{
		if (*aliasp == NAME_None)
		{
			order = -1;
			return "";	// flagged to be ignored
		}
		musicname = aliasp->GetChars();
	}
	return musicname;
}

//==========================================================================
//
// FindMusic
//
// loops up a music resource according to the engine's rules
//
//==========================================================================

static int FindMusic(const char* musicname)
{
	int lumpnum = fileSystem.CheckNumForFullName(musicname);
	if (lumpnum == -1) lumpnum = fileSystem.CheckNumForName(musicname, FileSys::ns_music);
	return lumpnum;
}

//==========================================================================
//
// S_Init
//
// Initializes sound stuff, including volume. Sets channels, SFX and
// music volume, allocates channel buffer, and sets S_sfx lookup.
//==========================================================================

void S_Init()
{
	// Hook up the music player with the engine specific customizations.
	static MusicCallbacks cb = { LookupMusic, FindMusic };
	S_SetMusicCallbacks(&cb);

	// Must be up before I_InitSound.
	if (!soundEngine)
	{
		soundEngine = new DoomSoundEngine;
	}

	I_InitSound();
	I_InitMusic(Args->CheckParm("-nomusic") || Args->CheckParm("-nosound"));

	// Heretic and Hexen have sound curve lookup tables. Doom does not.
	int curvelump = fileSystem.CheckNumForName("SNDCURVE");
	TArray<uint8_t> curve;
	if (curvelump >= 0)
	{
		curve.Resize(fileSystem.FileLength(curvelump));
		fileSystem.ReadFile(curvelump, curve.Data());
	}
	soundEngine->Init(curve);
}

//==========================================================================
//
// S_Shutdown
//
//==========================================================================

void S_Shutdown()
{
	S_StopMusic(true);

	for (auto Level : AllLevels())
	{
		SN_StopAllSequences(Level);
	}

	if (soundEngine)
	{
		soundEngine->Shutdown();
		delete soundEngine;
		soundEngine = nullptr;
	}

	mus_playing.LastSong = "";	// If this isn't reset here, the song would attempt to resume at the most inpopportune time...

	if (GSnd != nullptr)
	{
		I_CloseSound();
	}
}

//==========================================================================
//
// S_Start
//
// Per level startup code. Kills playing sounds at start of level
// and starts new music.
//==========================================================================

void S_Start()
{
	if (GSnd && soundEngine)
	{
		// kill all playing sounds at start of level (trust me - a good idea)
		soundEngine->StopAllChannels();

		// Check for local sound definitions. Only reload if they differ
		// from the previous ones.
		FString LocalSndInfo;
		FString LocalSndSeq;

		// To be certain better check whether level is valid!
		if (primaryLevel->info)
		{
			LocalSndInfo = primaryLevel->info->SoundInfo;
			LocalSndSeq = primaryLevel->info->SndSeq;
		}

		bool parse_ss = false;

		// This level uses a different local SNDINFO
		if (LastLocalSndInfo.CompareNoCase(LocalSndInfo) != 0 || !primaryLevel->info)
		{
			soundEngine->UnloadAllSounds();

			// Parse the global SNDINFO
			S_ParseSndInfo(true);

			if (LocalSndInfo.IsNotEmpty())
			{
				// Now parse the local SNDINFO
				int j = fileSystem.CheckNumForFullName(LocalSndInfo.GetChars(), true);
				if (j >= 0) S_AddLocalSndInfo(j);
			}

			// Also reload the SNDSEQ if the SNDINFO was replaced!
			parse_ss = true;
		}
		else if (LastLocalSndSeq.CompareNoCase(LocalSndSeq) != 0)
		{
			parse_ss = true;
		}

		if (parse_ss)
		{
			S_ParseSndSeq(LocalSndSeq.IsNotEmpty() ? fileSystem.CheckNumForFullName(LocalSndSeq.GetChars(), true) : -1);
		}

		LastLocalSndInfo = LocalSndInfo;
		LastLocalSndSeq = LocalSndSeq;
	}
}

//==========================================================================
//
// S_PrecacheLevel
//
// Like R_PrecacheLevel, but for sounds.
//
//==========================================================================

void S_PrecacheLevel(FLevelLocals* Level)
{
	if (GSnd && Level == primaryLevel)
	{
		soundEngine->MarkAllUnused();

		AActor* actor;
		auto iterator = Level->GetThinkerIterator<AActor>();

		// Precache all sounds known to be used by the currently spawned actors.
		while ((actor = iterator.Next()) != nullptr)
		{
			IFVIRTUALPTR(actor, AActor, MarkPrecacheSounds)
			{
				VMValue params[1] = { actor };
				VMCall(func, params, 1, nullptr, 0);
			}
		}
		for (auto snd : gameinfo.PrecachedSounds)
		{
			soundEngine->MarkUsed(snd);
		}
		// Precache all extra sounds requested by this map.
		for (auto snd : primaryLevel->info->PrecacheSounds)
		{
			soundEngine->MarkUsed(snd);
		}
		soundEngine->CacheMarkedSounds();
	}
}

//==========================================================================
//
// S_InitData
//
//==========================================================================

void S_InitData()
{
	LastLocalSndInfo = LastLocalSndSeq = "";
	S_ParseSndInfo(false);
	S_ParseSndSeq(-1);
}

//==========================================================================
//
// S_Sound - Unpositioned version
//
//==========================================================================

void S_SoundPitch(int channel, EChanFlags flags, FSoundID sound_id, float volume, float attenuation, float pitch, float startTime)
{
	soundEngine->StartSound(SOURCE_None, nullptr, nullptr, channel, flags, sound_id, volume, attenuation, nullptr, pitch, startTime);
}

void S_Sound(int channel, EChanFlags flags, FSoundID sound_id, float volume, float attenuation)
{
	soundEngine->StartSound (SOURCE_None, nullptr, nullptr, channel, flags, sound_id, volume, attenuation, 0, 0.f);
}

DEFINE_ACTION_FUNCTION(DObject, S_Sound)
{
	PARAM_PROLOGUE;
	PARAM_SOUND(id);
	PARAM_INT(channel);
	PARAM_FLOAT(volume);
	PARAM_FLOAT(attn);
	PARAM_FLOAT(pitch);
	PARAM_FLOAT(startTime);
	S_SoundPitch(channel & 7, EChanFlags::FromInt(channel & ~7), id, static_cast<float>(volume), static_cast<float>(attn), static_cast<float>(pitch), static_cast<float>(startTime));
	return 0;
}

DEFINE_ACTION_FUNCTION(DObject, S_StartSound)
{
	PARAM_PROLOGUE;
	PARAM_SOUND(id);
	PARAM_INT(channel);
	PARAM_INT(flags);
	PARAM_FLOAT(volume);
	PARAM_FLOAT(attn);
	PARAM_FLOAT(pitch);
	PARAM_FLOAT(startTime);
	S_SoundPitch(channel, EChanFlags::FromInt(flags), id, static_cast<float>(volume), static_cast<float>(attn), static_cast<float>(pitch), static_cast<float>(startTime));
	return 0;
}

static void S_StartSoundAt(double x, double y, double z, int sound_id, int channel, int flags, double volume, double attenuation, double pitch, double startTime)
{
	FVector3 pos = { (float)x, (float)z, (float)y };
	soundEngine->StartSound(SOURCE_Unattached, nullptr, &pos, channel, EChanFlags::FromInt(flags), FSoundID::fromInt(sound_id), (float)volume, (float)attenuation, nullptr, (float)pitch, (float)startTime);
}

DEFINE_ACTION_FUNCTION_NATIVE(DObject, S_StartSoundAt, S_StartSoundAt)
{
	PARAM_PROLOGUE;
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_FLOAT(z);
	PARAM_INT(id);
	PARAM_INT(channel);
	PARAM_INT(flags);
	PARAM_FLOAT(volume);
	PARAM_FLOAT(attn);
	PARAM_FLOAT(pitch);
	PARAM_FLOAT(startTime);
	S_StartSoundAt(x, y, z, id, channel, flags, volume, attn, pitch, startTime);
	return 0;
}

//==========================================================================
//
//
//
//==========================================================================

void DoomSoundEngine::CacheSound(sfxinfo_t* sfx)
{
	if (!(sfx->UserData[0] & SND_PlayerReserve)) SoundEngine::CacheSound(sfx);
}

//==========================================================================
//
//
//
//==========================================================================

FSoundID DoomSoundEngine::ResolveSound(const void * ent, int type, FSoundID soundid, float &attenuation)
{
	auto sfx = &S_sfx[soundid.index()];
	if (sfx->UserData[0] & SND_PlayerReserve)
	{
		AActor *src;
		if (type != SOURCE_Actor) src = nullptr;
		else src = (AActor*)ent;
		return S_FindSkinnedSound(src, soundid);
	}
	return SoundEngine::ResolveSound(ent, type, soundid, attenuation);
}

//==========================================================================
//
// Common checking code for the actor sound functions
//
//==========================================================================

static bool VerifyActorSound(AActor* ent, FSoundID& sound_id, int& channel, EChanFlags flags)
{
	if (ent == nullptr || ent->ObjectFlags & OF_EuthanizeMe || ent->Sector->Flags & SECF_SILENT || 
		ent->Level != primaryLevel)
		return false;

	if ((flags & CHANF_MAYBE_LOCAL) && (compatflags & COMPATF_SILENTPICKUP))
	{
		if (!soundEngine->isListener(ent))
		{
			return false;
		}
	}

	if (compatflags & COMPATF_MAGICSILENCE)
	{ // For people who just can't play without a silent BFG.
		channel = CHAN_WEAPON;
	}
	return true;
}

//==========================================================================
//
// Common checking code for the actor sound functions
//
//==========================================================================

void DoomSoundEngine::StopChannel(FSoundChan* chan)
{
	if (chan && chan->SysChannel != NULL && !(chan->ChanFlags & CHANF_EVICTED) && chan->SourceType == SOURCE_Actor)
	{
		chan->Source = NULL;
	}
	SoundEngine::StopChannel(chan);
}

//==========================================================================
//
// S_Sound - An actor is source
//
//==========================================================================

void S_SoundPitchActor(AActor *ent, int channel, EChanFlags flags, FSoundID sound_id, float volume, float attenuation, float pitch, float startTime)
{
#if 0
	// sound source debug printout
	Printf("sound '%s' from '%s'\n", soundEngine->GetSoundName(sound_id), ent->GetClass()->TypeName.GetChars());
#endif

	if (flags & CHANF_NORUMBLE)
	{
		// remove conflicting flag
		if (flags & CHANF_RUMBLE)
		{
			flags = (EChanFlag)(flags - CHANF_RUMBLE);
		}
	}
	else if (!(flags & CHANF_RUMBLE))
	{
		// add flag if needed
		if (haptics_do_action && (
			// sound from self
			(ent->player && (ent->player->mo == players[consoleplayer].mo || ent->player->mo == players[consoleplayer].camera)) ||
			// sound from self missile
			((ent->flags & MF_MISSILE)
				&& ent->target && ent->target->player
				&& (ent->target->player->mo == players[consoleplayer].mo || ent->target->player->mo == players[consoleplayer].camera))
		)) flags |= CHANF_RUMBLE;
	}

	if (VerifyActorSound(ent, sound_id, channel, flags))
		soundEngine->StartSound (SOURCE_Actor, ent, nullptr, channel, flags, sound_id, volume, attenuation, 0, pitch, startTime);
}

void S_Sound(AActor *ent, int channel, EChanFlags flags, FSoundID sound_id, float volume, float attenuation)
{
	S_SoundPitchActor(ent, channel, flags, sound_id, volume, attenuation, 0.f);
}

//==========================================================================
//
// S_SoundMinMaxDist - An actor is source
//
// Attenuation is specified as min and max distances, rather than a scalar.
//
//==========================================================================

void S_SoundMinMaxDist(AActor *ent, int channel, EChanFlags flags, FSoundID sound_id, float volume, float mindist, float maxdist)
{
	if (VerifyActorSound(ent, sound_id, channel, flags))
	{
		FRolloffInfo rolloff;

		rolloff.RolloffType = ROLLOFF_Linear;
		rolloff.MinDistance = mindist;
		rolloff.MaxDistance = maxdist;
		soundEngine->StartSound(SOURCE_Actor, ent, nullptr, channel, flags, sound_id, volume, 1, &rolloff);
	}
}

//==========================================================================
//
// S_Sound - A polyobject is source
//
//==========================================================================

void S_Sound (const FPolyObj *poly, int channel, EChanFlags flags, FSoundID sound_id, float volume, float attenuation)
{
	if (poly->Level != primaryLevel) return;
	soundEngine->StartSound (SOURCE_Polyobj, poly, nullptr, channel, flags, sound_id, volume, attenuation);
}

//==========================================================================
//
// S_Sound - A point is source
//
//==========================================================================

void S_Sound(FLevelLocals *Level, const DVector3 &pos, int channel, EChanFlags flags, FSoundID sound_id, float volume, float attenuation)
{
	if (Level != primaryLevel) return;
	// The sound system switches Y and Z around.
	FVector3 p((float)pos.X, (float)pos.Z, (float)pos.Y);
	soundEngine->StartSound (SOURCE_Unattached, nullptr, &p, channel, flags, sound_id, volume, attenuation);
}

//==========================================================================
//
// S_Sound - An entire sector is source
//
//==========================================================================

void S_Sound (const sector_t *sec, int channel, EChanFlags flags, FSoundID sfxid, float volume, float attenuation)
{
	if (sec->Level != primaryLevel) return;
	soundEngine->StartSound (SOURCE_Sector, sec, nullptr, channel, flags, sfxid, volume, attenuation);
}

//==========================================================================
//
// S_PlaySound - Subfunction used by ACS and DECORATE
//
// Has a local parameter to make the sound audible only to the source
//
//==========================================================================

void S_PlaySoundPitch(AActor *a, int chan, EChanFlags flags, FSoundID sid, float vol, float atten, float pitch, float startTime = 0.f)
{
	if (a == nullptr || a->Sector->Flags & SECF_SILENT || a->Level != primaryLevel)
		return;

	if (!(flags & CHANF_LOCAL))
	{
		if (!(flags & CHANF_NOSTOP) || !S_IsActorPlayingSomething(a, chan, sid))
		{
			S_SoundPitchActor(a, chan, flags, sid, vol, atten, pitch, startTime);
		}
	}
	else
	{
		if (a->CheckLocalView())
		{
			if (!(flags & CHANF_NOSTOP) || !soundEngine->IsSourcePlayingSomething(SOURCE_None, nullptr, chan, sid))
			{
				S_SoundPitch(chan, flags, sid, vol, ATTN_NONE, pitch, startTime);
			}
		}
	}
}

void S_PlaySound(AActor *a, int chan, EChanFlags flags, FSoundID sid, float vol, float atten)
{
	S_PlaySoundPitch(a, chan, flags, sid, vol, atten, 0.f, 0.f);
}

void A_StartSound(AActor *self, int soundid, int channel, int flags, double volume, double attenuation, double pitch, double startTime)
{
	S_PlaySoundPitch(self, channel, EChanFlags::FromInt(flags), FSoundID::fromInt(soundid), (float)volume, (float)attenuation, (float)pitch, (float)startTime);
}

void A_PlaySound(AActor* self, int soundid, int channel, double volume, int looping, double attenuation, int local, double pitch)
{
	if (looping) channel |= CHANF_LOOP | CHANF_NOSTOP;
	if (local) channel |= CHANF_LOCAL;
	A_StartSound(self, soundid, channel & 7, channel & ~7, volume, attenuation, pitch, 0.0f);
}

//==========================================================================
//
// S_StopSound
//
// Stops a sound from a single actor from playing on a specific channel.
//
//==========================================================================

void S_StopSound (AActor *actor, int channel)
{
	soundEngine->StopSound(SOURCE_Actor, actor, (compatflags & COMPATF_MAGICSILENCE) ? -1 : channel);
}

//==========================================================================
//
// S_StopAllActorSounds
//
// Stops all sounds on an actor.
//
//==========================================================================

void S_StopActorSounds(AActor *actor, int chanmin, int chanmax)
{
	soundEngine->StopActorSounds(SOURCE_Actor, actor, chanmin, chanmax);
}

//==========================================================================
//
// S_StopSound
//
// Stops a sound from a single sector from playing on a specific channel.
//
//==========================================================================

void S_StopSound (const sector_t *sec, int channel)
{
	soundEngine->StopSound(SOURCE_Sector, sec, (compatflags & COMPATF_MAGICSILENCE) ? -1 : channel);
}

//==========================================================================
//
// S_StopSound
//
// Stops a sound from a single polyobject from playing on a specific channel.
//
//==========================================================================

void S_StopSound (const FPolyObj *poly, int channel)
{
	soundEngine->StopSound(SOURCE_Polyobj, poly, (compatflags & COMPATF_MAGICSILENCE) ? -1 : channel);
}

//==========================================================================
//
// S_RelinkSound
//
// Moves all the sounds from one thing to another. If the destination is
// nullptr, then the sound becomes a positioned sound.
//==========================================================================

void S_RelinkSound (AActor *from, AActor *to)
{

	FVector3 p = from->SoundPos();
	soundEngine->RelinkSound(SOURCE_Actor, from, to, !(compatflags2 & COMPATF2_SOUNDCUTOFF)? &p : nullptr);
}

//==========================================================================
//
// S_ChangeSoundVolume
//
//==========================================================================

void S_ChangeActorSoundVolume(AActor *actor, int channel, double dvolume)
{
	soundEngine->ChangeSoundVolume(SOURCE_Actor, actor, (compatflags & COMPATF_MAGICSILENCE)? -1 : channel, dvolume);
}

//==========================================================================
//
// S_ChangeSoundPitch
//
//==========================================================================

void S_ChangeActorSoundPitch(AActor *actor, int channel, double pitch)
{
	soundEngine->ChangeSoundPitch(SOURCE_Actor, actor, channel, pitch);
}

//==========================================================================
//
// S_GetSoundPlayingInfo
//
// Is a sound being played by a specific emitter?
//==========================================================================

bool S_GetSoundPlayingInfo (const AActor *actor, FSoundID sound_id)
{
	return soundEngine->GetSoundPlayingInfo(SOURCE_Actor, actor, sound_id);
}

bool S_GetSoundPlayingInfo (const sector_t *sec, FSoundID sound_id)
{
	return soundEngine->GetSoundPlayingInfo(SOURCE_Sector, sec, sound_id);
}

bool S_GetSoundPlayingInfo (const FPolyObj *poly, FSoundID sound_id)
{
	return soundEngine->GetSoundPlayingInfo(SOURCE_Polyobj, poly, sound_id);
}

 //==========================================================================
//
// S_IsActorPlayingSomething
//
//==========================================================================

bool S_IsActorPlayingSomething (AActor *actor, int channel, FSoundID sound_id)
{
	if (compatflags & COMPATF_MAGICSILENCE)
	{
		channel = CHAN_AUTO; // checks all channels
	}
	return soundEngine->IsSourcePlayingSomething(SOURCE_Actor, actor, channel, sound_id);
}

//==========================================================================
//
// Sets the internal listener structure
//
//==========================================================================

static void S_SetListener(AActor *listenactor)
{
	SoundListener listener;
	if (listenactor != nullptr)
	{
		listener.angle = (float)listenactor->Angles.Yaw.Radians();
		/*
		listener.velocity.X = listenactor->vel.x * (TICRATE/65536.f);
		listener.velocity.Y = listenactor->vel.z * (TICRATE/65536.f);
		listener.velocity.Z = listenactor->vel.y * (TICRATE/65536.f);
		*/
		listener.velocity.Zero();
		listener.position = listenactor->SoundPos();
		listener.underwater = listenactor->waterlevel == 3;
		assert(primaryLevel->Zones.Size() > listenactor->Sector->ZoneNumber);
		listener.Environment = primaryLevel->Zones[listenactor->Sector->ZoneNumber].Environment;
		listener.valid = true;
	}
	else
	{
		listener.angle = 0;
		listener.position.Zero();
		listener.velocity.Zero();
		listener.underwater = false;
		listener.Environment = nullptr;
		listener.valid = false;
	}
	listener.ListenerObject = listenactor;
	soundEngine->SetListener(listener);
}

//==========================================================================
//
// S_UpdateSounds
//
// Updates music & sounds
//==========================================================================

void S_UpdateSounds (AActor *listenactor)
{
	// should never happen
	S_SetListener(listenactor);
	
	for (auto Level : AllLevels())
	{
		SN_UpdateActiveSequences(Level);
	}

	soundEngine->UpdateSounds(primaryLevel->time);
}

//==========================================================================
//
// Although saving the sound system's state is supposed to be an engine
// feature, the specifics cannot be set up there, this needs to be on the client side.
//
//==========================================================================

static FSerializer &Serialize(FSerializer &arc, const char *key, FSoundChan &chan, FSoundChan *def)
{
	if (arc.BeginObject(key))
	{
		arc("sourcetype", chan.SourceType)
			("soundid", chan.SoundID)
			("orgid", chan.OrgID)
			("volume", chan.Volume)
			("distancescale", chan.DistanceScale)
			("pitch", chan.Pitch)
			("chanflags", chan.ChanFlags)
			("entchannel", chan.EntChannel)
			("priority", chan.Priority)
			("nearlimit", chan.NearLimit)
			("starttime", chan.StartTime)
			("rolloftype", chan.Rolloff.RolloffType)
			("rolloffmin", chan.Rolloff.MinDistance)
			("rolloffmax", chan.Rolloff.MaxDistance)
			("limitrange", chan.LimitRange);

		if (SaveVersion < 4560) chan.Pitch /= 128.f;

		switch (chan.SourceType)
		{
		case SOURCE_None:										break;
		case SOURCE_Actor:		{ AActor* s = (AActor*)chan.Source;  arc("actor", s); chan.Source = s; break; }
		case SOURCE_Sector:		{ auto s = (sector_t*)chan.Source; arc("sector", s); chan.Source = s; break; }
		case SOURCE_Polyobj: { auto s = (FPolyObj*)chan.Source; arc("poly", s); chan.Source = s; break; }
		case SOURCE_Unattached:	arc.Array("point", chan.Point, 3); break;
		default:				I_Error("Unknown sound source type %d\n", chan.SourceType);	break;
		}
		arc.EndObject();
	}
	return arc;
}

//==========================================================================
//
// S_SerializeSounds
//
//==========================================================================

void S_SerializeSounds(FSerializer &arc)
{
	FSoundChan *chan;

	GSnd->Sync(true);

	if (arc.isWriting())
	{
		// Count channels and accumulate them so we can store them in
		// reverse order. That way, they will be in the same order when
		// reloaded later as they are now.
		TArray<FSoundChan*> chans = soundEngine->AllActiveChannels();

		if (chans.Size() > 0 && arc.BeginArray("sounds"))
		{
			for (unsigned int i = chans.Size(); i-- != 0; )
			{
				// Replace start time with sample position.
				uint64_t start = chans[i]->StartTime;
				chans[i]->StartTime = GSnd ? GSnd->GetPosition(chans[i]) : 0;
				arc(nullptr, *chans[i]);
				chans[i]->StartTime = start;
			}
			arc.EndArray();
		}
	}
	else
	{
		unsigned int count;

		soundEngine->StopAllChannels();
		if (arc.BeginArray("sounds"))
		{
			count = arc.ArraySize();
			for (unsigned int i = 0; i < count; ++i)
			{
				chan = (FSoundChan*)soundEngine->GetChannel(nullptr);
				arc(nullptr, *chan);
				// Sounds always start out evicted when restored from a save.
				chan->ChanFlags |= CHANF_EVICTED | CHANF_ABSTIME;
			}
			arc.EndArray();
		}
		// The two tic delay is to make sure any screenwipes have finished.
		// This needs to be two because the game is run for one tic before
		// the wipe so that it can produce a screen to wipe to. So if we
		// only waited one tic to restart the sounds, they would start
		// playing before the wipe, and depending on the synchronization
		// between the main thread and the mixer thread at the time, the
		// sounds might be heard briefly before pausing for the wipe.
		soundEngine->SetRestartTime(primaryLevel->time + 2);
	}
	GSnd->Sync(false);
	GSnd->UpdateSounds();
}

//==========================================================================
//
// CalcSectorSoundOrg
//
// Returns the perceived sound origin for a sector. If the listener is
// inside the sector, then the origin is their location. Otherwise, the
// origin is from the nearest wall on the sector.
//
//==========================================================================

static void CalcSectorSoundOrg(const DVector3& listenpos, const sector_t* sec, int channum, FVector3& pos)
{
	if (!(sec->Level->i_compatflags & COMPATF_SECTORSOUNDS))
	{
		// Are we inside the sector? If yes, the closest point is the one we're on.
		if (primaryLevel->PointInSector(listenpos.X, listenpos.Y) == sec)
		{
			pos.X = (float)listenpos.X;
			pos.Z = (float)listenpos.Y;
		}
		else
		{
			// Find the closest point on the sector's boundary lines and use
			// that as the perceived origin of the sound.
			DVector2 xy;
			sec->ClosestPoint(listenpos.XY(), xy);
			pos.X = (float)xy.X;
			pos.Z = (float)xy.Y;
		}
	}
	else
	{
		pos.X = float(sec->centerspot.X);
		pos.Z = float(sec->centerspot.Y);
	}

	// Set sound vertical position based on channel.
	if (channum == CHAN_FLOOR)
	{
		pos.Y = (float)min<double>(sec->floorplane.ZatPoint(listenpos), listenpos.Z);
	}
	else if (channum == CHAN_CEILING)
	{
		pos.Y = (float)max<double>(sec->ceilingplane.ZatPoint(listenpos), listenpos.Z);
	}
	else if (channum == CHAN_INTERIOR)
	{
		pos.Y = (float)clamp<double>(listenpos.Z, sec->floorplane.ZatPoint(listenpos), sec->ceilingplane.ZatPoint(listenpos));
	}
}

//==========================================================================
//
// CalcPolySoundOrg
//
// Returns the perceived sound origin for a polyobject. This is similar to
// CalcSectorSoundOrg, except there is no special case for being "inside"
// a polyobject, so the sound literally comes from the polyobject's walls.
// Vertical position of the sound always comes from the visible wall.
//
//==========================================================================

static void CalcPolyobjSoundOrg(const DVector3& listenpos, const FPolyObj* poly, FVector3& pos)
{
	side_t* side;
	sector_t* sec;

	DVector2 ppos;
	poly->ClosestPoint(listenpos.XY(), ppos, &side);
	pos.X = (float)ppos.X;
	pos.Z = (float)ppos.Y;
	sec = side->sector;
	pos.Y = (float)clamp<double>(listenpos.Z, sec->floorplane.ZatPoint(listenpos), sec->ceilingplane.ZatPoint(listenpos));
}

//=========================================================================
//
// CalcPosVel
//
// The game specific part of the sound updater.
//
//=========================================================================

void DoomSoundEngine::CalcPosVel(int type, const void* source, const float pt[3], int channum, int chanflags, FSoundID soundid, FVector3* pos, FVector3* vel, FSoundChan *)
{
	if (pos != nullptr)
	{
		DVector3 listenpos;
		int pgroup;
		AActor* listener = players[consoleplayer].camera;

		if (listener != nullptr)
		{
			listenpos = listener->Pos();
			*pos = listener->SoundPos();
			pgroup = listener->Sector->PortalGroup;
		}
		else
		{
			listenpos.Zero();
			pos->Zero();
			pgroup = 0;
		}
		if (vel) vel->Zero();

		// [BL] Moved this case out of the switch statement to make code easier
		//      on static analysis.
		if (type == SOURCE_Unattached)
		{
			sector_t* sec = primaryLevel->PointInSector(pt[0], pt[2]);
			DVector2 disp = primaryLevel->Displacements.getOffset(pgroup, sec->PortalGroup);
			pos->X = pt[0] - (float)disp.X;
			pos->Y = !(chanflags & CHANF_LISTENERZ) ? pt[1] : (float)listenpos.Z;
			pos->Z = pt[2] - (float)disp.Y;
		}
		else
		{
			switch (type)
			{
			case SOURCE_None:
			default:
				break;

			case SOURCE_Actor:
			{
				auto actor = (AActor*)source;
				//assert(actor != nullptr);
				if (actor != nullptr)
				{
					DVector2 disp = primaryLevel->Displacements.getOffset(pgroup, actor->Sector->PortalGroup);
					DVector3 posi = actor->Pos() - disp;
					*pos = { (float)posi.X, (float)posi.Z, (float)posi.Y };
					if (vel)
					{
						vel->X = float(actor->Vel.X * TICRATE);
						vel->Y = float(actor->Vel.Z * TICRATE);
						vel->Z = float(actor->Vel.Y * TICRATE);
					}
				}
				break;
			}

			case SOURCE_Sector:
			{
				auto sector = (sector_t*)source;
				assert(sector != nullptr);
				if (sector != nullptr)
				{
					DVector2 disp = primaryLevel->Displacements.getOffset(pgroup, sector->PortalGroup);
					if (chanflags & CHANF_AREA)
					{
						// listener must be reversely offset to calculate the proper sound origin.
						CalcSectorSoundOrg(listenpos + disp, sector, channum, *pos);
						pos->X -= (float)disp.X;
						pos->Z -= (float)disp.Y;
					}
					else
					{

						pos->X = (float)(sector->centerspot.X - disp.X);
						pos->Z = (float)(sector->centerspot.Y - disp.Y);
						chanflags |= CHANF_LISTENERZ;
					}
				}
				break;
			}

			case SOURCE_Polyobj:
			{
				auto poly = (FPolyObj*)source;
				assert(poly != nullptr);
				if (poly != nullptr)
				{
					DVector2 disp = primaryLevel->Displacements.getOffset(pgroup, poly->CenterSubsector->sector->PortalGroup);
					CalcPolyobjSoundOrg(listenpos + disp, poly, *pos);
					pos->X -= (float)disp.X;
					pos->Z -= (float)disp.Y;
				}
				break;
			}
			}

			if ((chanflags & CHANF_LISTENERZ) && players[consoleplayer].camera != nullptr)
			{
				pos->Y = (float)listenpos.Z;
			}
		}
	}
}

//==========================================================================
//
// ValidatePosVel
//
//==========================================================================

inline bool Validate(const float value, const float limit)
{
	return value >= -limit && value <= limit;
}

static bool Validate(const FVector3& value, const float limit, const char* const name, const AActor* const actor)
{
	const bool valid =
		Validate(value.X, limit)
		&& Validate(value.Y, limit)
		&& Validate(value.Z, limit);

	if (!valid)
	{
		// Sound position and velocity have Y and Z axes swapped comparing to map coordinate system
		Printf(TEXTCOLOR_RED "Invalid sound %s " TEXTCOLOR_WHITE "(%f, %f, %f)", name, value.X, value.Z, value.Y);

		if (actor == nullptr)
		{
			Printf("\n");
		}
		else
		{
			Printf(TEXTCOLOR_RED " for actor of class " TEXTCOLOR_WHITE "%s\n", actor->GetClass()->TypeName.GetChars());
		}
	}

	return valid;
}

bool DoomSoundEngine::ValidatePosVel(int sourcetype, const void* source, const FVector3& pos, const FVector3& vel)
{
	if (sourcetype != SOURCE_Actor) return true;
	// The actual limit for map coordinates
	auto actor = (AActor*)source;
	static const float POSITION_LIMIT = 1024.f * 1024.f;
	const bool valid = Validate(pos, POSITION_LIMIT, "position", actor);

	// The maximum velocity is enough to travel through entire map in one tic
	static const float VELOCITY_LIMIT = 2 * POSITION_LIMIT * TICRATE;
	return Validate(vel, VELOCITY_LIMIT, "velocity", actor) && valid;
}

//==========================================================================
//
// This is to avoid hardscoding the dependency on the file system into the sound engine
// 
//==========================================================================

TArray<uint8_t> DoomSoundEngine::ReadSound(int lumpnum)
{
	auto wlump = fileSystem.OpenFileReader(lumpnum);
	TArray<uint8_t> buffer(wlump.GetLength(), true);
	auto len = wlump.Read(buffer.data(), buffer.size());
	buffer.Resize(len);
	return buffer;
}

//==========================================================================
//
// S_PickReplacement
//
// This is overridden to use a synchronized RNG.
// 
//==========================================================================

static FCRandom pr_randsound("RandSound");

FSoundID DoomSoundEngine::PickReplacement(FSoundID refid)
{
	while (S_sfx[refid.index()].bRandomHeader)
	{
		const FRandomSoundList* list = &S_rnd[S_sfx[refid.index()].link.index()];
		refid = list->Choices[pr_randsound(list->Choices.Size())];
	}
	return refid;
}

//==========================================================================
//
// S_NoiseDebug
//
// [RH] Print sound debug info. Called by status bar.
//==========================================================================

void DoomSoundEngine::NoiseDebug()
{
	FSoundChan* chan;
	FVector3 listener;
	FVector3 origin;
	int y, color;

	y = 32 * CleanYfac;
	DrawText(twod, NewConsoleFont, CR_YELLOW, 0, y, "*** SOUND DEBUG INFO ***", TAG_DONE);
	y += NewConsoleFont->GetHeight();

	DrawText(twod, NewConsoleFont, CR_GOLD, 0, y, "name", TAG_DONE);
	DrawText(twod, NewConsoleFont, CR_GOLD, 70, y, "x", TAG_DONE);
	DrawText(twod, NewConsoleFont, CR_GOLD, 120, y, "y", TAG_DONE);
	DrawText(twod, NewConsoleFont, CR_GOLD, 170, y, "z", TAG_DONE);
	DrawText(twod, NewConsoleFont, CR_GOLD, 220, y, "vol", TAG_DONE);
	DrawText(twod, NewConsoleFont, CR_GOLD, 260, y, "dist", TAG_DONE);
	DrawText(twod, NewConsoleFont, CR_GOLD, 300, y, "chan", TAG_DONE);
	DrawText(twod, NewConsoleFont, CR_GOLD, 340, y, "pri", TAG_DONE);
	DrawText(twod, NewConsoleFont, CR_GOLD, 380, y, "flags", TAG_DONE);
	DrawText(twod, NewConsoleFont, CR_GOLD, 460, y, "aud", TAG_DONE);
	DrawText(twod, NewConsoleFont, CR_GOLD, 520, y, "pos", TAG_DONE);
	y += NewConsoleFont->GetHeight();

	if (Channels == nullptr)
	{
		return;
	}

	listener = players[consoleplayer].camera->SoundPos();

	// Display the oldest channel first.
	for (chan = Channels; chan->NextChan != nullptr; chan = chan->NextChan)
	{
	}
	while (y < twod->GetHeight() - 16)
	{
		char temp[32];

		SoundEngine::CalcPosVel(chan, &origin, nullptr);
		color = (chan->ChanFlags & CHANF_LOOP) ? CR_BROWN : CR_GREY;

		// Name
		auto tname = fileSystem.GetFileShortName(S_sfx[chan->SoundID.index()].lumpnum);
		DrawText(twod, NewConsoleFont, color, 0, y, tname, TAG_DONE);

		if (!(chan->ChanFlags & CHANF_IS3D))
		{
			DrawText(twod, NewConsoleFont, color, 70, y, "---", TAG_DONE);		// X
			DrawText(twod, NewConsoleFont, color, 120, y, "---", TAG_DONE);	// Y
			DrawText(twod, NewConsoleFont, color, 170, y, "---", TAG_DONE);	// Z
			DrawText(twod, NewConsoleFont, color, 260, y, "---", TAG_DONE);	// Distance
		}
		else
		{
			// X coordinate
			mysnprintf(temp, countof(temp), "%.0f", origin.X);
			DrawText(twod, NewConsoleFont, color, 70, y, temp, TAG_DONE);

			// Y coordinate
			mysnprintf(temp, countof(temp), "%.0f", origin.Z);
			DrawText(twod, NewConsoleFont, color, 120, y, temp, TAG_DONE);

			// Z coordinate
			mysnprintf(temp, countof(temp), "%.0f", origin.Y);
			DrawText(twod, NewConsoleFont, color, 170, y, temp, TAG_DONE);

			// Distance
			if (chan->DistanceScale > 0)
			{
				mysnprintf(temp, countof(temp), "%.0f", (origin - listener).Length());
				DrawText(twod, NewConsoleFont, color, 260, y, temp, TAG_DONE);
			}
			else
			{
				DrawText(twod, NewConsoleFont, color, 260, y, "---", TAG_DONE);
			}
		}

		// Volume
		mysnprintf(temp, countof(temp), "%.2g", chan->Volume);
		DrawText(twod, NewConsoleFont, color, 220, y, temp, TAG_DONE);

		// Channel
		mysnprintf(temp, countof(temp), "%d", chan->EntChannel);
		DrawText(twod, NewConsoleFont, color, 300, y, temp, TAG_DONE);

		// Priority
		mysnprintf(temp, countof(temp), "%d", chan->Priority);
		DrawText(twod, NewConsoleFont, color, 340, y, temp, TAG_DONE);

		// Flags
		mysnprintf(temp, countof(temp), "%s3%sZ%sU%sM%sN%sA%sL%sE%sV",
			(chan->ChanFlags & CHANF_IS3D) ? TEXTCOLOR_GREEN : TEXTCOLOR_BLACK,
			(chan->ChanFlags & CHANF_LISTENERZ) ? TEXTCOLOR_GREEN : TEXTCOLOR_BLACK,
			(chan->ChanFlags & CHANF_UI) ? TEXTCOLOR_GREEN : TEXTCOLOR_BLACK,
			(chan->ChanFlags & CHANF_MAYBE_LOCAL) ? TEXTCOLOR_GREEN : TEXTCOLOR_BLACK,
			(chan->ChanFlags & CHANF_NOPAUSE) ? TEXTCOLOR_GREEN : TEXTCOLOR_BLACK,
			(chan->ChanFlags & CHANF_AREA) ? TEXTCOLOR_GREEN : TEXTCOLOR_BLACK,
			(chan->ChanFlags & CHANF_LOOP) ? TEXTCOLOR_GREEN : TEXTCOLOR_BLACK,
			(chan->ChanFlags & CHANF_EVICTED) ? TEXTCOLOR_GREEN : TEXTCOLOR_BLACK,
			(chan->ChanFlags & CHANF_VIRTUAL) ? TEXTCOLOR_GREEN : TEXTCOLOR_BLACK);
		DrawText(twod, NewConsoleFont, color, 380, y, temp, TAG_DONE);

		// Audibility
		mysnprintf(temp, countof(temp), "%.4f", GSnd->GetAudibility(chan));
		DrawText(twod, NewConsoleFont, color, 460, y, temp, TAG_DONE);

		// Position
		mysnprintf(temp, countof(temp), "%u", GSnd->GetPosition(chan));
		DrawText(twod, NewConsoleFont, color, 520, y, temp, TAG_DONE);

		y += NewConsoleFont->GetHeight();
		if (chan->PrevChan == &Channels)
		{
			break;
		}
		chan = (FSoundChan*)((size_t)chan->PrevChan - myoffsetof(FSoundChan, NextChan));
	}
}

ADD_STAT(sounddebug)
{
	static_cast<DoomSoundEngine*>(soundEngine)->NoiseDebug();
	return "";
}

//==========================================================================
//
// CCMD soundlist
//
//==========================================================================

void DoomSoundEngine::PrintSoundList()
{
	unsigned int i;

	for (i = 0; i < soundEngine->GetNumSounds(); i++)
	{
		const sfxinfo_t* sfx = soundEngine->GetSfx(FSoundID::fromInt(i));
		if (sfx->bRandomHeader)
		{
			Printf("%3d. %s -> #%d {", i, sfx->name.GetChars(), sfx->link.index());
			const FRandomSoundList* list = &S_rnd[sfx->link.index()];
			for (auto& me : list->Choices)
			{
				Printf(" %s ", S_sfx[me.index()].name.GetChars());
			}
			Printf("}\n");
		}
		else if (sfx->UserData[0] & SND_PlayerReserve)
		{
			Printf("%3d. %s <<player sound %d>>\n", i, sfx->name.GetChars(), sfx->link.index());
		}
		else if (S_sfx[i].lumpnum != -1)
		{
			Printf("%3d. %s (%s)\n", i, sfx->name.GetChars(), fileSystem.GetFileShortName(sfx->lumpnum));
		}
		else if (S_sfx[i].link != sfxinfo_t::NO_LINK)
		{
			Printf("%3d. %s -> %s\n", i, sfx->name.GetChars(), S_sfx[sfx->link.index()].name.GetChars());
		}
		else
		{
			Printf("%3d. %s **not present**\n", i, sfx->name.GetChars());
		}
		Printf("    PitchMask = %d\n", sfx->PitchMask);
	}
}

CCMD(soundlist)
{
	static_cast<DoomSoundEngine*>(soundEngine)->PrintSoundList();
}

//==========================================================================
//
// CCMD playsound
//
//==========================================================================

CCMD (playsound)
{
	if (argv.argc() > 1)
	{
		FSoundID id = S_FindSound(argv[1]);
		if (!id.isvalid())
		{
			Printf("'%s' is not a sound\n", argv[1]);
		}
		else
		{
			S_Sound (CHAN_AUTO, CHANF_UI, id, 1.f, ATTN_NONE);
		}
	}
}

//==========================================================================
//
// CCMD loopsound
//
//==========================================================================

CCMD (loopsound)
{
	if (players[consoleplayer].mo != nullptr && !netgame && argv.argc() > 1)
	{
		FSoundID id = S_FindSound(argv[1]);
		if (!id.isvalid())
		{
			Printf("'%s' is not a sound\n", argv[1]);
		}
		else
		{
			AActor *icon = Spawn(primaryLevel, "SpeakerIcon", players[consoleplayer].mo->PosPlusZ(32.), ALLOW_REPLACE);
			if (icon != nullptr)
			{
				S_Sound(icon, CHAN_BODY, CHANF_LOOP, id, 1.f, ATTN_IDLE);
			}
		}
	}
}

CCMD(snd_reset)
{
	S_SoundReset();
}

