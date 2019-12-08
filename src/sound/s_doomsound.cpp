/*
** doomspund.cpp
**
** Game dependent part of the sound engine.
**
**---------------------------------------------------------------------------
**
** Copyright 1999-2016 Randy Heit
** Copyright 2002-2019 Christoph Oelckers
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
**---------------------------------------------------------------------------
**
*/ 


#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <io.h>
#endif

#include "i_system.h"
#include "i_sound.h"
#include "i_music.h"
#include "s_sound.h"
#include "s_sndseq.h"
#include "s_playlist.h"
#include "c_dispatch.h"
#include "m_random.h"
#include "w_wad.h"
#include "p_local.h"
#include "doomstat.h"
#include "cmdlib.h"
#include "v_video.h"
#include "v_text.h"
#include "a_sharedglobal.h"
#include "gstrings.h"
#include "gi.h"
#include "po_man.h"
#include "serializer.h"
#include "d_player.h"
#include "g_levellocals.h"
#include "vm.h"
#include "g_game.h"
#include "s_music.h"


extern SoundListener listener;
extern int RestartEvictionsAt;	// do not restart evicted channels before this time
extern bool	SoundPaused;		// whether sound is paused
void CalcPosVel(FSoundChan* chan, FVector3* pos, FVector3* vel);

//==========================================================================
//
// S_Sound - Unpositioned version
//
//==========================================================================

void S_SoundPitch(int channel, FSoundID sound_id, float volume, float attenuation, float pitch)
{
	S_StartSound(SOURCE_None, nullptr, nullptr, channel, sound_id, volume, attenuation, 0, pitch);
}

void S_Sound(int channel, FSoundID sound_id, float volume, float attenuation)
{
	S_StartSound (SOURCE_None, nullptr, nullptr, channel, sound_id, volume, attenuation, 0, 0.f);
}

DEFINE_ACTION_FUNCTION(DObject, S_Sound)
{
	PARAM_PROLOGUE;
	PARAM_SOUND(id);
	PARAM_INT(channel);
	PARAM_FLOAT(volume);
	PARAM_FLOAT(attn);
	PARAM_FLOAT(pitch);
	S_SoundPitch(channel, id, static_cast<float>(volume), static_cast<float>(attn), static_cast<float>(pitch));
	return 0;
}

//==========================================================================
//
// S_Sound - An actor is source
//
//==========================================================================
void S_SoundPitchActor(AActor *ent, int channel, FSoundID sound_id, float volume, float attenuation, float pitch)
{
	if (ent == nullptr || ent->Sector->Flags & SECF_SILENT || ent->Level != primaryLevel)
		return;

	if (S_sfx[sound_id].bPlayerReserve)
	{
		sound_id = FSoundID(S_FindSkinnedSound(ent, sound_id));
		if (sound_id <= 0) return;
	}

	S_StartSound (SOURCE_Actor, ent, nullptr, channel, sound_id, volume, attenuation, 0, pitch);
}

void S_Sound(AActor *ent, int channel, FSoundID sound_id, float volume, float attenuation)
{
	S_SoundPitchActor(ent, channel, sound_id, volume, attenuation, 0.f);
}

//==========================================================================
//
// S_SoundMinMaxDist - An actor is source
//
// Attenuation is specified as min and max distances, rather than a scalar.
//
//==========================================================================

void S_SoundMinMaxDist(AActor *ent, int channel, FSoundID sound_id, float volume, float mindist, float maxdist)
{
	if (ent == nullptr || ent->Sector->Flags & SECF_SILENT || ent->Level != primaryLevel)
		return;

	if (S_sfx[sound_id].bPlayerReserve)
	{
		sound_id = FSoundID(S_FindSkinnedSound(ent, sound_id));
		if (sound_id <= 0) return;
	}


	FRolloffInfo rolloff;

	rolloff.RolloffType = ROLLOFF_Linear;
	rolloff.MinDistance = mindist;
	rolloff.MaxDistance = maxdist;
	S_StartSound(SOURCE_Actor, ent, nullptr, channel, sound_id, volume, 1, &rolloff);
}

//==========================================================================
//
// S_Sound - A polyobject is source
//
//==========================================================================

void S_Sound (const FPolyObj *poly, int channel, FSoundID sound_id, float volume, float attenuation)
{
	if (poly->Level != primaryLevel) return;
	S_StartSound (SOURCE_Polyobj, poly, nullptr, channel, sound_id, volume, attenuation);
}

//==========================================================================
//
// S_Sound - A point is source
//
//==========================================================================

void S_Sound(FLevelLocals *Level, const DVector3 &pos, int channel, FSoundID sound_id, float volume, float attenuation)
{
	if (Level != primaryLevel) return;
	// The sound system switches Y and Z around.
	FVector3 p((float)pos.X, (float)pos.Z, (float)pos.Y);
	S_StartSound (SOURCE_Unattached, nullptr, &p, channel, sound_id, volume, attenuation);
}

//==========================================================================
//
// S_Sound - An entire sector is source
//
//==========================================================================

void S_Sound (const sector_t *sec, int channel, FSoundID sfxid, float volume, float attenuation)
{
	if (sec->Level != primaryLevel) return;
	S_StartSound (SOURCE_Sector, sec, nullptr, channel, sfxid, volume, attenuation);
}

//==========================================================================
//
// S_PlaySound - Subfunction used by ACS and DECORATE
//
// Has a local parameter to make the sound audible only to the source
//
//==========================================================================

void S_PlaySoundPitch(AActor *a, int chan, FSoundID sid, float vol, float atten, bool local, float pitch)
{
	if (a == nullptr || a->Sector->Flags & SECF_SILENT || a->Level != primaryLevel)
		return;

	if (!local)
	{
		S_SoundPitchActor(a, chan, sid, vol, atten, pitch);
	}
	else
	{
		if (a->CheckLocalView())
		{
			S_SoundPitch(chan, sid, vol, ATTN_NONE, pitch);
		}
	}
}

void S_PlaySound(AActor *a, int chan, FSoundID sid, float vol, float atten, bool local)
{
	S_PlaySoundPitch(a, chan, sid, vol, atten, local, 0.f);
}

void A_PlaySound(AActor *self, int soundid, int channel, double volume, int looping, double attenuation, int local, double pitch)
{
	if (!looping)
	{
		if (!(channel & CHAN_NOSTOP) || !S_IsActorPlayingSomething(self, channel & 7, soundid))
		{
			S_PlaySoundPitch(self, channel, soundid, (float)volume, (float)attenuation, local, (float)pitch);
		}
	}
	else
	{
		if (!S_IsActorPlayingSomething(self, channel & 7, soundid))
		{
			S_PlaySoundPitch(self, channel | CHAN_LOOP, soundid, (float)volume, (float)attenuation, local, (float)pitch);
		}
	}
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
	S_StopSound(SOURCE_Actor, actor, (compatflags & COMPATF_MAGICSILENCE) ? -1 : channel);
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
	S_StopSound(SOURCE_Sector, sec, (compatflags & COMPATF_MAGICSILENCE) ? -1 : channel);
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
	S_StopSound(SOURCE_Polyobj, poly, (compatflags & COMPATF_MAGICSILENCE) ? -1 : channel);
}

//==========================================================================
//
// S_RelinkSound
//
// Moves all the sounds from one thing to another. If the destination is
// NULL, then the sound becomes a positioned sound.
//==========================================================================

void S_RelinkSound (AActor *from, AActor *to)
{

	FVector3 p = from->SoundPos();
	S_RelinkSound(SOURCE_Actor, from, to, !(compatflags2 & COMPATF2_SOUNDCUTOFF)? &p : nullptr);
}

//==========================================================================
//
// S_ChangeSoundVolume
//
//==========================================================================

void S_ChangeActorSoundVolume(AActor *actor, int channel, double dvolume)
{
	S_ChangeSoundVolume(SOURCE_Actor, actor, (compatflags & COMPATF_MAGICSILENCE)? -1 : channel, dvolume);
}

//==========================================================================
//
// S_ChangeSoundPitch
//
//==========================================================================

void S_ChangeActorSoundPitch(AActor *actor, int channel, double pitch)
{
	for (FSoundChan *chan = Channels; chan != NULL; chan = chan->NextChan)
	{
		if (chan->SourceType == SOURCE_Actor &&
			chan->Source == actor &&
			chan->EntChannel == channel)
		{
			S_SetPitch(chan, (float)pitch);
			return;
		}
	}
	return;
}

//==========================================================================
//
// S_GetSoundPlayingInfo
//
// Is a sound being played by a specific emitter?
//==========================================================================

bool S_GetSoundPlayingInfo (const AActor *actor, int sound_id)
{
	return S_GetSoundPlayingInfo(SOURCE_Actor, actor, sound_id);
}

bool S_GetSoundPlayingInfo (const sector_t *sec, int sound_id)
{
	return S_GetSoundPlayingInfo(SOURCE_Sector, sec, sound_id);
}

bool S_GetSoundPlayingInfo (const FPolyObj *poly, int sound_id)
{
	return S_GetSoundPlayingInfo(SOURCE_Polyobj, poly, sound_id);
}

 //==========================================================================
//
// S_IsActorPlayingSomething
//
//==========================================================================

bool S_IsActorPlayingSomething (AActor *actor, int channel, int sound_id)
{
	if (compatflags & COMPATF_MAGICSILENCE)
	{
		channel = CHAN_AUTO;
	}
	return S_IsSourcePlayingSomething(SOURCE_Actor, actor, channel, sound_id);
}

//==========================================================================
//
// Sets the internal listener structure
//
//==========================================================================

static void S_SetListener(AActor *listenactor)
{
	if (listenactor != NULL)
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
		listener.Environment = NULL;
		listener.valid = false;
	}
	listener.ListenerObject = listenactor;
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

	S_UpdateSounds(primaryLevel->time);
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
		TArray<FSoundChan *> chans;

		// Count channels and accumulate them so we can store them in
		// reverse order. That way, they will be in the same order when
		// reloaded later as they are now.
		for (chan = Channels; chan != NULL; chan = chan->NextChan)
		{
			// If the sound is forgettable, this is as good a time as
			// any to forget about it. And if it's a UI sound, it shouldn't
			// be stored in the savegame.
			if (!(chan->ChanFlags & (CHAN_FORGETTABLE | CHAN_UI)))
			{
				chans.Push(chan);
			}
		}
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

		S_StopAllChannels();
		if (arc.BeginArray("sounds"))
		{
			count = arc.ArraySize();
			for (unsigned int i = 0; i < count; ++i)
			{
				chan = (FSoundChan*)S_GetChannel(NULL);
				arc(nullptr, *chan);
				// Sounds always start out evicted when restored from a save.
				chan->ChanFlags |= CHAN_EVICTED | CHAN_ABSTIME;
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
		RestartEvictionsAt = primaryLevel->time + 2;
	}
	GSnd->Sync(false);
	GSnd->UpdateSounds();
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
		FSoundID id = argv[1];
		if (id == 0)
		{
			Printf("'%s' is not a sound\n", argv[1]);
		}
		else
		{
			S_Sound (CHAN_AUTO | CHAN_UI, id, 1.f, ATTN_NONE);
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
	if (players[consoleplayer].mo != NULL && !netgame && argv.argc() > 1)
	{
		FSoundID id = argv[1];
		if (id == 0)
		{
			Printf("'%s' is not a sound\n", argv[1]);
		}
		else
		{
			AActor *icon = Spawn(primaryLevel, "SpeakerIcon", players[consoleplayer].mo->PosPlusZ(32.), ALLOW_REPLACE);
			if (icon != NULL)
			{
				S_Sound(icon, CHAN_BODY | CHAN_LOOP, id, 1.f, ATTN_IDLE);
			}
		}
	}
}

//==========================================================================
//
// CCMD cachesound <sound name>
//
//==========================================================================

CCMD (cachesound)
{
	if (argv.argc() < 2)
	{
		Printf ("Usage: cachesound <sound> ...\n");
		return;
	}
	for (int i = 1; i < argv.argc(); ++i)
	{
		FSoundID sfxnum = argv[i];
		if (sfxnum != FSoundID(0))
		{
			S_CacheSound (&S_sfx[sfxnum]);
		}
	}
}


CCMD(listsoundchannels)
{	
	FSoundChan *chan;
	int count = 0;
	for (chan = Channels; chan != NULL; chan = chan->NextChan)
	{
		if (!(chan->ChanFlags & CHAN_EVICTED))
		{
			FVector3 chanorigin;

			CalcPosVel(chan, &chanorigin, NULL);

			Printf("%s at (%1.5f, %1.5f, %1.5f)\n", (const char*)chan->SoundID, chanorigin.X, chanorigin.Y, chanorigin.Z);
			count++;
		}
	}
	Printf("%d sounds playing\n", count);
}

// intentionally moved here to keep the s_music include out of the rest of the file.

//==========================================================================
//
// S_PauseSound
//
// Stop music and sound effects, during game PAUSE.
//==========================================================================
#include "s_music.h"

void S_PauseSound (bool notmusic, bool notsfx)
{
	if (!notmusic)
	{
		S_PauseMusic();
	}
	if (!notsfx)
	{
		SoundPaused = true;
		GSnd->SetSfxPaused (true, 0);
	}
}

DEFINE_ACTION_FUNCTION(DObject, S_PauseSound)
{
	PARAM_PROLOGUE;
	PARAM_BOOL(notmusic);
	PARAM_BOOL(notsfx);
	S_PauseSound(notmusic, notsfx);
	return 0;
}

//==========================================================================
//
// S_ResumeSound
//
// Resume music and sound effects, after game PAUSE.
//==========================================================================

void S_ResumeSound (bool notsfx)
{
	S_ResumeMusic();
	if (!notsfx)
	{
		SoundPaused = false;
		GSnd->SetSfxPaused (false, 0);
	}
}

DEFINE_ACTION_FUNCTION(DObject, S_ResumeSound)
{
	PARAM_PROLOGUE;
	PARAM_BOOL(notsfx);
	S_ResumeSound(notsfx);
	return 0;
}


CCMD (snd_status)
{
	GSnd->PrintStatus ();
}

CCMD (snd_reset)
{
	S_SoundReset();
}

void S_SoundReset()
{
	S_StopMusic(true);
	S_Reset();
	S_RestartMusic();
}

CCMD (snd_listdrivers)
{
	GSnd->PrintDriversList ();
}

ADD_STAT (sound)
{
	return GSnd->GatherStats ();
}
