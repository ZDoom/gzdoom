/*
** s_sound.cpp
** Main sound engine
**
**---------------------------------------------------------------------------
** Copyright 1998-2016 Randy Heit
** Copyright 2002-2019 Christoph Oelckers
** All rights reserved.
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


#include "s_soundinternal.h"
#include "m_swap.h"
#include "superfasthash.h"
#include "s_music.h"
#include "m_random.h"
#include "printf.h"
#include "c_cvars.h"
#include "gamestate.h"

CVARD(Bool, snd_enabled, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG, "enables/disables sound effects")
CVAR(Bool, i_soundinbackground, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, i_pauseinbackground, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
// killough 2/21/98: optionally use varying pitched sounds
CVAR(Bool, snd_pitched, false, CVAR_ARCHIVE)

int SoundEnabled()
{
	return snd_enabled && !nosound && !nosfx;
}

enum
{
	DEFAULT_PITCH = 128,
};
static FRandom pr_soundpitch ("SoundPitch");
SoundEngine* soundEngine;

//==========================================================================
//
// S_Init
//
//==========================================================================

void SoundEngine::Init(TArray<uint8_t> &curve)
{
	StopAllChannels();
	// Free all channels for use.
	while (Channels != NULL)
	{
		ReturnChannel(Channels);
	}
	S_SoundCurve = std::move(curve);
}

//==========================================================================
//
// SoundEngine::Clear
//
//==========================================================================

void SoundEngine::Clear()
{
	StopAllChannels();
	UnloadAllSounds();
	S_sfx.Clear();
	ClearRandoms();
}

//==========================================================================
//
// S_Shutdown
//
//==========================================================================

void SoundEngine::Shutdown ()
{
	FSoundChan *chan, *next;

	StopAllChannels();

	for (chan = FreeChannels; chan != NULL; chan = next)
	{
		next = chan->NextChan;
		delete chan;
	}
	FreeChannels = NULL;
}

//==========================================================================
//
// MarkUsed
//
//==========================================================================

void SoundEngine::MarkUsed(FSoundID sid)
{
	if (isValidSoundId(sid))
	{
		S_sfx[sid.index()].bUsed = true;
	}
}

//==========================================================================
//
// Cache all marked sounds
//
//==========================================================================

void SoundEngine::CacheMarkedSounds()
{
	// Don't unload sounds that are playing right now.
	for (FSoundChan* chan = Channels; chan != nullptr; chan = chan->NextChan)
	{
		MarkUsed(chan->SoundID);
	}

	for (unsigned i = 1; i < S_sfx.Size(); ++i)
	{
		if (S_sfx[i].bUsed)
		{
			CacheSound(&S_sfx[i]);
		}
	}
	for (unsigned i = 1; i < S_sfx.Size(); ++i)
	{
		if (!S_sfx[i].bUsed && S_sfx[i].link == sfxinfo_t::NO_LINK)
		{
			UnloadSound(&S_sfx[i]);
		}
	}
}

//==========================================================================
//
// S_CacheSound
//
//==========================================================================

void SoundEngine::CacheSound (sfxinfo_t *sfx)
{
	if (GSnd && !sfx->bTentative)
	{
		while (!sfx->bRandomHeader && isValidSoundId(sfx->link))
		{
			sfx = &S_sfx[sfx->link.index()];
		}
		if (sfx->bRandomHeader)
		{
			CacheRandomSound(sfx);
		}
		else
		{
			LoadSound(sfx);
			sfx->bUsed = true;
		}
	}
}

//==========================================================================
//
// S_UnloadSound
//
//==========================================================================

void SoundEngine::UnloadSound (sfxinfo_t *sfx)
{
	if (sfx->data.isValid())
	{
		GSnd->UnloadSound(sfx->data);
		DPrintf(DMSG_NOTIFY, "Unloaded sound \"%s\" (%td)\n", sfx->name.GetChars(), sfx - &S_sfx[0]);
	}
	sfx->data.Clear();
}

//==========================================================================
//
// S_GetChannel
//
// Returns a free channel for the system sound interface.
//
//==========================================================================

FSoundChan *SoundEngine::GetChannel(void *syschan)
{
	FSoundChan *chan;

	if (FreeChannels != NULL)
	{
		chan = FreeChannels;
		UnlinkChannel(chan);
	}
	else
	{
		chan = new FSoundChan;
		memset(chan, 0, sizeof(*chan));
	}
	LinkChannel(chan, &Channels);
	chan->SysChannel = syschan;
	return chan;
}

//==========================================================================
//
// S_ReturnChannel
//
// Returns a channel to the free pool.
//
//==========================================================================

void SoundEngine::ReturnChannel(FSoundChan *chan)
{
	UnlinkChannel(chan);
	memset(chan, 0, sizeof(*chan));
	LinkChannel(chan, &FreeChannels);
}

//==========================================================================
//
// S_UnlinkChannel
//
//==========================================================================

void SoundEngine::UnlinkChannel(FSoundChan *chan)
{
	*(chan->PrevChan) = chan->NextChan;
	if (chan->NextChan != NULL)
	{
		chan->NextChan->PrevChan = chan->PrevChan;
	}
}

//==========================================================================
//
// S_LinkChannel
//
//==========================================================================

void SoundEngine::LinkChannel(FSoundChan *chan, FSoundChan **head)
{
	chan->NextChan = *head;
	if (chan->NextChan != NULL)
	{
		chan->NextChan->PrevChan = &chan->NextChan;
	}
	*head = chan;
	chan->PrevChan = head;
}

//==========================================================================
//
//
//
//==========================================================================

TArray<FSoundChan*> SoundEngine::AllActiveChannels()
{
	TArray<FSoundChan*> chans;

	for (auto chan = Channels; chan != nullptr; chan = chan->NextChan)
	{
		// If the sound is forgettable, this is as good a time as
		// any to forget about it. And if it's a UI sound, it shouldn't
		// be stored in the savegame.
		if (!(chan->ChanFlags & (CHANF_FORGETTABLE | CHANF_UI | CHANF_TRANSIENT)))
		{
			chans.Push(chan);
		}
	}
	return chans;
}

//==========================================================================
//
//
//
//==========================================================================

FString SoundEngine::ListSoundChannels()
{
	FString output;
	FSoundChan* chan;
	int count = 0;
	for (chan = Channels; chan != nullptr; chan = chan->NextChan)
	{
		if (!(chan->ChanFlags & CHANF_EVICTED))
		{
			FVector3 chanorigin;

			CalcPosVel(chan, &chanorigin, nullptr);

			output.AppendFormat("%s at (%1.5f, %1.5f, %1.5f)\n", (const char*)S_sfx[chan->SoundID.index()].name.GetChars(), chanorigin.X, chanorigin.Y, chanorigin.Z);
			count++;
		}
	}
	output.AppendFormat("%d sounds playing\n", count);
	return output;
}

// [RH] Split S_StartSoundAtVolume into multiple parts so that sounds can
//		be specified both by id and by name. Also borrowed some stuff from
//		Hexen and parameters from Quake.

//==========================================================================
//
// CalcPosVel
//
// Retrieves a sound's position and velocity for 3D sounds. This version
// is for an already playing sound.
//
//=========================================================================

void SoundEngine::CalcPosVel(FSoundChan *chan, FVector3 *pos, FVector3 *vel)
{
	CalcPosVel(chan->SourceType, chan->Source, chan->Point,	chan->EntChannel, chan->ChanFlags, chan->OrgID, pos, vel, chan);
}

bool SoundEngine::ValidatePosVel(const FSoundChan* const chan, const FVector3& pos, const FVector3& vel)
{
	return ValidatePosVel(chan->SourceType, chan->Source, pos, vel);
}

//==========================================================================
//
//
//
//==========================================================================

FSoundID SoundEngine::ResolveSound(const void *, int, FSoundID soundid, float &attenuation)
{
	const sfxinfo_t &sfx = S_sfx[soundid.index()];

	if (sfx.bRandomHeader)
	{
		// Random sounds attenuate based on the original (random) sound as well as the chosen one.
		attenuation *= sfx.Attenuation;
		return PickReplacement (soundid);
	}
	else
	{
		return sfx.link;
	}
}

//==========================================================================
//
//
//
//==========================================================================

static float CalcPitch(int pitchmask, float defpitch, float defpitchmax)
{
	if (defpitch > 0.0)	// $PitchSet overrides $PitchShift
	{
		if (defpitchmax > 0.0 && defpitch != defpitchmax)
		{
			defpitch = (float)pr_soundpitch.GenRand_Real1() * (defpitchmax - defpitch) + defpitch;
		}
		return defpitch;
	}

	// Vary the sfx pitches. Overridden by $PitchSet and A_StartSound.
	if (pitchmask != 0 && snd_pitched)
	{
		return (DEFAULT_PITCH - (pr_soundpitch() & pitchmask) + (pr_soundpitch() & pitchmask)) / (float)DEFAULT_PITCH;
	}
	return 1.f;
}

//==========================================================================
//
// S_StartSound
//
// 0 attenuation means full volume over whole primaryLevel->
// 0 < attenuation means to scale the distance by that amount when
//		calculating volume.
//
//==========================================================================

FSoundChan *SoundEngine::StartSound(int type, const void *source,
	const FVector3 *pt, int channel, EChanFlags flags, FSoundID sound_id, float volume, float attenuation,
	FRolloffInfo *forcedrolloff, float spitch, float startTime)
{
	sfxinfo_t *sfx;
	EChanFlags chanflags = flags;
	int basepriority;
	FSoundID org_id;
	FSoundChan *chan;
	FVector3 pos, vel;
	FRolloffInfo *rolloff;

	if (!isValidSoundId(sound_id) || volume <= 0 || nosfx || !SoundEnabled() || blockNewSounds)
		return NULL;

	// prevent crashes.
	if (type == SOURCE_Unattached && pt == nullptr) type = SOURCE_None;

	org_id = sound_id;

	CalcPosVel(type, source, &pt->X, channel, chanflags, sound_id, &pos, &vel, nullptr);

	if (!ValidatePosVel(type, source, pos, vel))
	{
		return nullptr;
	}

	sfx = &S_sfx[sound_id.index()];

	// Scale volume according to SNDINFO data.
	volume = min(volume * sfx->Volume, 1.f);
	if (volume <= 0)
		return NULL;

	// When resolving a link we do not want to get the NearLimit of
	// the referenced sound so some additional checks are required
	int near_limit = sfx->NearLimit;
	float limit_range = sfx->LimitRange;
	float defpitch = sfx->DefPitch;
	float defpitchmax = sfx->DefPitchMax;
	rolloff = &sfx->Rolloff;

	// Resolve player sounds, random sounds, and aliases
	while (sfx->link != sfxinfo_t::NO_LINK)
	{
		sound_id = ResolveSound(source, type, sound_id, attenuation);
		if (!isValidSoundId(sound_id)) return nullptr;
		auto newsfx = &S_sfx[sound_id.index()];
		if (newsfx != sfx)
		{
			if (near_limit < 0)
			{
				near_limit = newsfx->NearLimit;
				limit_range = newsfx->LimitRange;
				defpitch = newsfx->DefPitch;
				defpitchmax = newsfx->DefPitchMax;
			}
			if (rolloff->MinDistance == 0)
			{
				rolloff = &newsfx->Rolloff;
			}
			sfx = newsfx;
		}
		else return nullptr; // nothing got replaced, prevent an endless loop,

	}

	// Attenuate the attenuation based on the sound.
	attenuation *= sfx->Attenuation;

	// The passed rolloff overrides any sound-specific rolloff.
	if (forcedrolloff != NULL && forcedrolloff->MinDistance != 0)
	{
		rolloff = forcedrolloff;
	}

	// If no valid rolloff was set, use the global default.
	if (rolloff->MinDistance == 0)
	{
		rolloff = &S_Rolloff;
	}

	// If this is a singular sound, don't play it if it's already playing.
	if ((sfx->bSingular || (flags & CHANF_SINGULAR)) && CheckSingular(sound_id))
	{
		chanflags |= CHANF_EVICTED;
	}

	// If the sound is unpositioned or comes from the listener, it is
	// never limited.
	if (type == SOURCE_None || source == listener.ListenerObject)
	{
		near_limit = 0;
	}

	// If this sound doesn't like playing near itself, don't play it if that's what would happen.
	if (near_limit > 0 && CheckSoundLimit(sfx, pos, near_limit, limit_range, type, source, channel, attenuation))
	{
		chanflags |= CHANF_EVICTED;
	}

	// If the sound is blocked and not looped, return now. If the sound
	// is blocked and looped, pretend to play it so that it can
	// eventually play for real.
	if ((chanflags & (CHANF_EVICTED | CHANF_LOOP)) == CHANF_EVICTED)
	{
		return NULL;
	}

	// Make sure the sound is loaded.
	sfx = LoadSound(sfx);

	// The empty sound never plays.
	if (sfx->lumpnum == sfx_empty)
	{
		return NULL;
	}

	// Select priority.
	if (type == SOURCE_None || source == listener.ListenerObject)
	{
		basepriority = 80;
	}
	else
	{
		basepriority = 0;
	}

	int seen = 0;
	if (source != NULL && channel == CHAN_AUTO)
	{
		// In the old sound system, 'AUTO' hijacked one of the other channels.
		// Now, with CHANF_OVERLAP at our disposal that isn't needed anymore. Just set the flag and let all sounds play on channel 0.
		chanflags |= CHANF_OVERLAP;
	}

	// If this actor is already playing something on the selected channel, stop it.
	if (!(chanflags & CHANF_OVERLAP) && type != SOURCE_None && ((source == NULL && channel != CHAN_AUTO) || (source != NULL && IsChannelUsed(type, source, channel, &seen))))
	{
		for (chan = Channels; chan != NULL; chan = chan->NextChan)
		{
			if (chan->SourceType == type && chan->EntChannel == channel)
			{
				const bool foundit = (type == SOURCE_Unattached)
					? (chan->Point[0] == pt->X && chan->Point[2] == pt->Z && chan->Point[1] == pt->Y)
					: (chan->Source == source);

				if (foundit)
				{
					StopChannel(chan);
				}
			}
		}
	}

	// sound is paused and a non-looped sound is being started.
	// Such a sound would play right after unpausing which wouldn't sound right.
	if (!(chanflags & CHANF_LOOP) && !(chanflags & (CHANF_UI|CHANF_NOPAUSE|CHANF_FORCE)) && SoundPaused)
	{
		return NULL;
	}

	float pitch = spitch > 0 ? spitch : CalcPitch(sfx->PitchMask, defpitch, defpitchmax);
	if (chanflags & CHANF_EVICTED)
	{
		chan = NULL;
	}
	else 
	{
		int startflags = 0;
		if (chanflags & CHANF_LOOP) startflags |= SNDF_LOOP;
		if (chanflags & CHANF_AREA) startflags |= SNDF_AREA;
		if (chanflags & (CHANF_UI|CHANF_NOPAUSE)) startflags |= SNDF_NOPAUSE;
		if (chanflags & CHANF_UI) startflags |= SNDF_NOREVERB;

		float sfxlength = (float)GSnd->GetMSLength(sfx->data) / 1000.f;
		startTime = (startflags & SNDF_LOOP)
				? (sfxlength > 0 ? fmodf(startTime, sfxlength) : 0.f)
				: clamp(startTime, 0.f, sfxlength);

		if (attenuation > 0 && type != SOURCE_None)
		{
			chan = (FSoundChan*)GSnd->StartSound3D (sfx->data, &listener, float(volume), rolloff, float(attenuation), pitch, basepriority, pos, vel, channel, startflags, NULL, startTime);
		}
		else
		{
			chan = (FSoundChan*)GSnd->StartSound (sfx->data, float(volume), pitch, startflags, NULL, startTime);
		}
	}
	if (chan == NULL && (chanflags & CHANF_LOOP))
	{
		chan = (FSoundChan*)GetChannel(NULL);
		GSnd->MarkStartTime(chan);
		chanflags |= CHANF_EVICTED;
	}
	if (attenuation > 0 && type != SOURCE_None)
	{
		chanflags |= CHANF_IS3D | CHANF_JUSTSTARTED;
	}
	else
	{
		chanflags |= CHANF_LISTENERZ | CHANF_JUSTSTARTED;
	}
	if (chan != NULL)
	{
		chan->SoundID = sound_id;
		chan->OrgID = org_id;
		chan->EntChannel = channel;
		chan->Volume = float(volume);
		chan->ChanFlags |= chanflags;
		chan->NearLimit = near_limit;
		chan->LimitRange = limit_range;
		chan->Pitch = pitch;
		chan->Priority = basepriority;
		chan->DistanceScale = float(attenuation);
		chan->SourceType = type;
		chan->UserData = 0;
		if (type == SOURCE_Unattached)
		{
			chan->Point[0] = pt->X; chan->Point[1] = pt->Y; chan->Point[2] = pt->Z;
		}
		else if (type != SOURCE_None)
		{
			chan->Source = source;
		}
	}

	return chan;
}

//==========================================================================
//
// S_RestartSound
//
// Attempts to restart looping sounds that were evicted from their channels.
//
//==========================================================================

void SoundEngine::RestartChannel(FSoundChan *chan)
{
	assert(chan->ChanFlags & CHANF_EVICTED);

	FSoundChan *ochan;
	sfxinfo_t *sfx = &S_sfx[chan->SoundID.index()];

	// If this is a singular sound, don't play it if it's already playing.
	if (sfx->bSingular && CheckSingular(chan->SoundID))
		return;

	sfx = LoadSound(sfx);

	// The empty sound never plays.
	if (sfx->lumpnum == sfx_empty)
	{
		return;
	}

	EChanFlags oldflags = chan->ChanFlags;

	int startflags = 0;
	if (chan->ChanFlags & CHANF_LOOP) startflags |= SNDF_LOOP;
	if (chan->ChanFlags & CHANF_AREA) startflags |= SNDF_AREA;
	if (chan->ChanFlags & (CHANF_UI|CHANF_NOPAUSE)) startflags |= SNDF_NOPAUSE;
	if (chan->ChanFlags & CHANF_ABSTIME) startflags |= SNDF_ABSTIME;

	if (chan->ChanFlags & CHANF_IS3D)
	{
		FVector3 pos, vel;

		CalcPosVel(chan, &pos, &vel);

		if (!ValidatePosVel(chan, pos, vel))
		{
			return;
		}

		// If this sound doesn't like playing near itself, don't play it if
		// that's what would happen.
		if (chan->NearLimit > 0 && CheckSoundLimit(&S_sfx[chan->SoundID.index()], pos, chan->NearLimit, chan->LimitRange, 0, NULL, 0, chan->DistanceScale))
		{
			return;
		}

		chan->ChanFlags &= ~(CHANF_EVICTED|CHANF_ABSTIME);
        ochan = (FSoundChan*)GSnd->StartSound3D(sfx->data, &listener, chan->Volume, &chan->Rolloff, chan->DistanceScale, chan->Pitch,
            chan->Priority, pos, vel, chan->EntChannel, startflags, chan);
	}
	else
	{
		chan->ChanFlags &= ~(CHANF_EVICTED|CHANF_ABSTIME);
		ochan = (FSoundChan*)GSnd->StartSound(sfx->data, chan->Volume, chan->Pitch, startflags, chan);
	}
	assert(ochan == NULL || ochan == chan);
	if (ochan == NULL)
	{
		chan->ChanFlags = oldflags;
	}
}

//==========================================================================
//
// S_LoadSound
//
// Returns a pointer to the sfxinfo with the actual sound data.
//
//==========================================================================

sfxinfo_t *SoundEngine::LoadSound(sfxinfo_t *sfx)
{
	if (GSnd->IsNull()) return sfx;

	while (!sfx->data.isValid())
	{
		unsigned int i;

		if (sfx->lumpnum == sfx_empty)
		{
			return sfx;
		}

		// See if there is another sound already initialized with this lump. If so,
		// then set this one up as a link, and don't load the sound again.
		for (i = 0; i < S_sfx.Size(); i++)
		{
			if (S_sfx[i].data.isValid() && S_sfx[i].link == sfxinfo_t::NO_LINK && S_sfx[i].lumpnum == sfx->lumpnum &&
				(!sfx->bLoadRAW || (sfx->RawRate == S_sfx[i].RawRate)))	// Raw sounds with different sample rates may not share buffers, even if they use the same source data.
			{
				DPrintf (DMSG_NOTIFY, "Linked %s to %s (%d)\n", sfx->name.GetChars(), S_sfx[i].name.GetChars(), i);
				sfx->link = FSoundID::fromInt(i);
				// This is necessary to avoid using the rolloff settings of the linked sound if its
				// settings are different.
				if (sfx->Rolloff.MinDistance == 0) sfx->Rolloff = S_Rolloff;
				return &S_sfx[i];
			}
		}

		DPrintf(DMSG_NOTIFY, "Loading sound \"%s\" (%td)\n", sfx->name.GetChars(), sfx - &S_sfx[0]);

		auto sfxdata = ReadSound(sfx->lumpnum);
		int size = (int)sfxdata.size();
		if (size > 8)
		{
			auto sfxp = sfxdata.data();
			int32_t dmxlen = LittleLong(((int32_t *)sfxp)[1]);

			// If the sound is voc, use the custom loader.
			if (memcmp (sfxp, "Creative Voice File", 19) == 0)
			{
				sfx->data = GSnd->LoadSoundVoc(sfxp, size);
			}
			// If the sound is raw, just load it as such.
			else if (sfx->bLoadRAW)
			{
				sfx->data = GSnd->LoadSoundRaw(sfxp, size, sfx->RawRate, 1, 8, sfx->LoopStart);
			}
			// Otherwise, try the sound as DMX format.
			else if (((uint8_t *)sfxp)[0] == 3 && ((uint8_t *)sfxp)[1] == 0 && dmxlen <= size - 8)
			{
				int frequency = LittleShort(((uint16_t *)sfxp)[1]);
				if (frequency == 0) frequency = 11025;
				sfx->data = GSnd->LoadSoundRaw(sfxp+8, dmxlen, frequency, 1, 8, sfx->LoopStart);
			}
			// If that fails, let the sound system try and figure it out.
			else
			{
				sfx->data = GSnd->LoadSound(sfxp, size, sfx->LoopStart, sfx->LoopEnd);
			}
		}

		if (!sfx->data.isValid())
		{
			if (sfx->lumpnum != sfx_empty)
			{
				sfx->lumpnum = sfx_empty;
				continue;
			}
		}
		break;
	}
	return sfx;
}

//==========================================================================
//
// S_CheckSingular
//
// Returns true if a copy of this sound is already playing.
//
//==========================================================================

bool SoundEngine::CheckSingular(FSoundID sound_id)
{
	for (FSoundChan *chan = Channels; chan != NULL; chan = chan->NextChan)
	{
		if (chan->OrgID == sound_id)
		{
			return true;
		}
	}
	return false;
}

//==========================================================================
//
// S_CheckSoundLimit
//
// Limits the number of nearby copies of a sound that can play near
// each other. If there are NearLimit instances of this sound already
// playing within sqrt(limit_range) (typically 256 units) of the new sound, the
// new sound will not start.
//
// If an actor is specified, and it is already playing the same sound on
// the same channel, this sound will not be limited. In this case, we're
// restarting an already playing sound, so there's no need to limit it.
//
// Returns true if the sound should not play.
//
//==========================================================================

bool SoundEngine::CheckSoundLimit(sfxinfo_t *sfx, const FVector3 &pos, int near_limit, float limit_range,
	int sourcetype, const void *actor, int channel, float attenuation)
{
	FSoundChan *chan;
	int count;

	for (chan = Channels, count = 0; chan != NULL && count < near_limit; chan = chan->NextChan)
	{
		if (chan->ChanFlags & CHANF_FORGETTABLE) continue;
		if (!(chan->ChanFlags & CHANF_EVICTED) && &S_sfx[chan->SoundID.index()] == sfx)
		{
			FVector3 chanorigin;

			if (actor != NULL && chan->EntChannel == channel &&
				chan->SourceType == sourcetype && chan->Source == actor)
			{ // We are restarting a playing sound. Always let it play.
				return false;
			}

			CalcPosVel(chan, &chanorigin, NULL);
			// scale the limit distance with the attenuation. An attenuation of 0 means the limit distance is infinite and all sounds within the level are inside the limit.
			float attn = min(chan->DistanceScale, attenuation);
			if (attn <= 0 || (chanorigin - pos).LengthSquared() <= limit_range / attn)
			{
				count++;
			}
		}
	}
	return count >= near_limit;
}

//==========================================================================
//
// S_StopSound
//
// Stops an unpositioned sound from playing on a specific channel.
//
//==========================================================================

void SoundEngine::StopSoundID(FSoundID sound_id)
{
	FSoundChan* chan = Channels;
	while (chan != NULL)
	{
		FSoundChan* next = chan->NextChan;
		if (sound_id == chan->OrgID)
		{
			StopChannel(chan);
		}
		chan = next;
	}
}

//==========================================================================
//
// S_StopSound
//
// Stops an unpositioned sound from playing on a specific channel.
//
//==========================================================================

void SoundEngine::StopSound (int channel, FSoundID sound_id)
{
	FSoundChan *chan = Channels;
	while (chan != NULL)
	{
		FSoundChan *next = chan->NextChan;
		if ((chan->SourceType == SOURCE_None && (sound_id == INVALID_SOUND || sound_id == chan->OrgID)) && (channel == CHAN_AUTO || channel == chan->EntChannel))
		{
			StopChannel(chan);
		}
		chan = next;
	}
}

//==========================================================================
//
// S_StopSound
//
// Stops a sound from a single actor from playing on a specific channel.
//
//==========================================================================

void SoundEngine::StopSound(int sourcetype, const void* actor, int channel, FSoundID sound_id)
{
	FSoundChan* chan = Channels;
	while (chan != NULL)
	{
		FSoundChan* next = chan->NextChan;
		if (chan->SourceType == sourcetype &&
			chan->Source == actor &&
			(sound_id == INVALID_SOUND? (chan->EntChannel == channel || channel < 0) : (chan->OrgID == sound_id)))
		{
			StopChannel(chan);
		}
		chan = next;
	}
}

//==========================================================================
//
// S_StopAllActorSounds
//
// Stops all sounds on an actor.
//
//==========================================================================

void SoundEngine::StopActorSounds(int sourcetype, const void* actor, int chanmin, int chanmax)
{
	const bool all = (chanmin == 0 && chanmax == 0);
	if (chanmax < chanmin) std::swap(chanmin, chanmax);

	FSoundChan* chan = Channels;
	while (chan != nullptr)
	{
		FSoundChan* next = chan->NextChan;
		if (chan->SourceType == sourcetype &&
			chan->Source == actor &&
			(all || (chan->EntChannel >= chanmin && chan->EntChannel <= chanmax)))
		{
			StopChannel(chan);
		}
		chan = next;
	}
}

//==========================================================================
//
// S_StopAllChannels
//
//==========================================================================

void SoundEngine::StopAllChannels ()
{
	FSoundChan *chan = Channels;
	while (chan != NULL)
	{
		FSoundChan *next = chan->NextChan;
		StopChannel(chan);
		chan = next;
	}

	if (GSnd)
		GSnd->UpdateSounds();
}

//==========================================================================
//
// S_RelinkSound
//
// Moves all the sounds from one thing to another. If the destination is
// NULL, then the sound becomes a positioned sound.
//==========================================================================

void SoundEngine::RelinkSound (int sourcetype, const void *from, const void *to, const FVector3 *optpos)
{
	if (from == NULL)
		return;

	FSoundChan *chan = Channels;
	while (chan != NULL)
	{
		FSoundChan *next = chan->NextChan;
		if (chan->SourceType == sourcetype && chan->Source == from)
		{
			if (to != NULL)
			{
				chan->Source = to;
			}
			else if (!(chan->ChanFlags & CHANF_LOOP) && optpos)
			{
				chan->Source = NULL;
				chan->SourceType = SOURCE_Unattached;
				chan->Point[0] = optpos->X;
				chan->Point[1] = optpos->Y;
				chan->Point[2] = optpos->Z;
			}
			else
			{
				StopChannel(chan);
			}
		}
		chan = next;
	}
}


//==========================================================================
//
// S_ChangeSoundVolume
//
//==========================================================================

void SoundEngine::ChangeSoundVolume(int sourcetype, const void *source, int channel, double dvolume)
{
	float volume = float(dvolume);
	// don't let volume get out of bounds
	if (volume < 0.0)
		volume = 0.0;
	else if (volume > 1.0)
		volume = 1.0;

	for (FSoundChan *chan = Channels; chan != NULL; chan = chan->NextChan)
	{
		if (chan->SourceType == sourcetype &&
			chan->Source == source &&
			(chan->EntChannel == channel || channel == -1))
		{
			GSnd->ChannelVolume(chan, volume);
			chan->Volume = volume;
		}
	}
	return;
}

void SoundEngine::SetVolume(FSoundChan* chan, float volume)
{
	if (volume < 0.0) volume = 0.0;
	else if (volume > 1.0) volume = 1.0;

	assert(chan != nullptr);
	GSnd->ChannelVolume(chan, volume);
	chan->Volume = volume;
}

//==========================================================================
//
// S_ChangeSoundPitch
//
//==========================================================================

void SoundEngine::ChangeSoundPitch(int sourcetype, const void *source, int channel, double pitch, FSoundID sound_id)
{
	for (FSoundChan *chan = Channels; chan != NULL; chan = chan->NextChan)
	{
		if (chan->SourceType == sourcetype &&
			chan->Source == source &&
			(sound_id == INVALID_SOUND? (chan->EntChannel == channel) : (chan->OrgID == sound_id)))
		{
			SetPitch(chan, (float)pitch);
		}
	}
	return;
}

void SoundEngine::SetPitch(FSoundChan *chan, float pitch)
{
	assert(chan != nullptr);
	GSnd->ChannelPitch(chan, max(0.0001f, pitch));
	chan->Pitch = pitch;
}

//==========================================================================
//
// S_GetSoundPlayingInfo
//
// Is a sound being played by a specific emitter?
//==========================================================================

int SoundEngine::GetSoundPlayingInfo (int sourcetype, const void *source, FSoundID sound_id, int chann)
{
	int count = 0;
	if (sound_id.isvalid())
	{
		for (FSoundChan *chan = Channels; chan != NULL; chan = chan->NextChan)
		{
			if (chann != -1 && chann != chan->EntChannel) continue;
			if (chan->OrgID == sound_id && (sourcetype == SOURCE_Any ||
				(chan->SourceType == sourcetype &&
				chan->Source == source)))
			{
				count++;
			}
		}
	}
	else
	{
		for (FSoundChan* chan = Channels; chan != NULL; chan = chan->NextChan)
		{
			if (chann != -1 && chann != chan->EntChannel) continue;
			if ((sourcetype == SOURCE_Any || (chan->SourceType == sourcetype &&	chan->Source == source)))
			{
				count++;
			}
		}
	}
	return count;
}

//==========================================================================
//
// S_IsChannelUsed
//
// Returns true if the channel is in use. Also fills in a bitmask of
// channels seen while scanning for this one, to make searching for unused
// channels faster. Initialize seen to 0 for the first call.
//
//==========================================================================

bool SoundEngine::IsChannelUsed(int sourcetype, const void *actor, int channel, int *seen)
{
	if (*seen & (1 << channel))
	{
		return true;
	}
	for (FSoundChan *chan = Channels; chan != NULL; chan = chan->NextChan)
	{
		if (chan->SourceType == sourcetype && chan->Source == actor)
		{
			*seen |= 1 << chan->EntChannel;
			if (chan->EntChannel == channel)
			{
				return true;
			}
		}
	}
	return false;
}

//==========================================================================
//
// S_IsActorPlayingSomething
//
//==========================================================================

bool SoundEngine::IsSourcePlayingSomething (int sourcetype, const void *actor, int channel, FSoundID sound_id)
{
	for (FSoundChan *chan = Channels; chan != NULL; chan = chan->NextChan)
	{
		if (chan->SourceType == sourcetype && (sourcetype == SOURCE_None || sourcetype == SOURCE_Unattached || chan->Source == actor))
		{
			if ((channel == 0 || chan->EntChannel == channel) && (sound_id == INVALID_SOUND || chan->OrgID == sound_id))
			{
				return true;
			}
		}
	}
	return false;
}

//==========================================================================
//
// S_EvictAllChannels
//
// Forcibly evicts all channels so that there are none playing, but all
// information needed to restart them is retained.
//
//==========================================================================

void SoundEngine::EvictAllChannels()
{
	FSoundChan *chan, *next;

	for (chan = Channels; chan != NULL; chan = next)
	{
		next = chan->NextChan;

		if (!(chan->ChanFlags & CHANF_EVICTED))
		{
			chan->ChanFlags |= CHANF_EVICTED;
			if (chan->SysChannel != NULL)
			{
				if (!(chan->ChanFlags & CHANF_ABSTIME))
				{
					chan->StartTime = GSnd ? GSnd->GetPosition(chan) : 0;
					chan->ChanFlags |= CHANF_ABSTIME;
				}
				StopChannel(chan);
			}
//			assert(chan->NextChan == next);
		}
	}
}

//==========================================================================
//
// S_RestoreEvictedChannel
//
// Recursive helper for S_RestoreEvictedChannels().
//
//==========================================================================

void SoundEngine::RestoreEvictedChannel(FSoundChan *chan)
{
	if (chan == NULL)
	{
		return;
	}
	RestoreEvictedChannel(chan->NextChan);
	if (chan->ChanFlags & CHANF_EVICTED)
	{
		RestartChannel(chan);
		if (!(chan->ChanFlags & CHANF_LOOP))
		{
			if (chan->ChanFlags & CHANF_EVICTED)
			{ // Still evicted and not looping? Forget about it.
				ReturnChannel(chan);
			}
			else if (!(chan->ChanFlags & CHANF_JUSTSTARTED))
			{ // Should this sound become evicted again, it's okay to forget about it.
				chan->ChanFlags |= CHANF_FORGETTABLE;
			}
		}
	}
	else if (chan->SysChannel == NULL && (chan->ChanFlags & (CHANF_FORGETTABLE | CHANF_LOOP)) == CHANF_FORGETTABLE)
	{
		ReturnChannel(chan);
	}
}

//==========================================================================
//
// S_RestoreEvictedChannels
//
// Restarts as many evicted channels as possible. Any channels that could
// not be started and are not looping are moved to the free pool.
//
//==========================================================================

void SoundEngine::RestoreEvictedChannels()
{
	// Restart channels in the same order they were originally played.
	RestoreEvictedChannel(Channels);
}

//==========================================================================
//
// S_UpdateSounds
//
// Updates music & sounds
//==========================================================================

void SoundEngine::UpdateSounds(int time)
{
	FVector3 pos, vel;

	for (FSoundChan* chan = Channels; chan != NULL; chan = chan->NextChan)
	{
		if ((chan->ChanFlags & (CHANF_EVICTED | CHANF_IS3D)) == CHANF_IS3D)
		{
			CalcPosVel(chan, &pos, &vel);

			if (ValidatePosVel(chan, pos, vel))
			{
				GSnd->UpdateSoundParams3D(&listener, chan, !!(chan->ChanFlags & CHANF_AREA), pos, vel);
			}
		}
		chan->ChanFlags &= ~CHANF_JUSTSTARTED;
	}

	GSnd->UpdateListener(&listener);
	GSnd->UpdateSounds();

	if (time >= RestartEvictionsAt)
	{
		RestartEvictionsAt = 0;
		RestoreEvictedChannels();
	}
}

//==========================================================================
//
// S_GetRolloff
//
//==========================================================================

float SoundEngine::GetRolloff(const FRolloffInfo* rolloff, float distance)
{
	if (rolloff == NULL)
	{
		return 0;
	}
	if (distance <= rolloff->MinDistance)
	{
		return 1.f;
	}
	// Logarithmic rolloff has no max distance where it goes silent.
	if (rolloff->RolloffType == ROLLOFF_Log)
	{
		return rolloff->MinDistance / (rolloff->MinDistance + rolloff->RolloffFactor * (distance - rolloff->MinDistance));
	}
	if (distance >= rolloff->MaxDistance)
	{
		return 0.f;
	}

	float volume = (rolloff->MaxDistance - distance) / (rolloff->MaxDistance - rolloff->MinDistance);
	if (rolloff->RolloffType == ROLLOFF_Linear)
	{
		return volume;
	}

	if (rolloff->RolloffType == ROLLOFF_Custom && S_SoundCurve.Size() > 0)
	{
		return S_SoundCurve[int(S_SoundCurve.Size() * (1.f - volume))] / 127.f;
	}
	return (powf(10.f, volume) - 1.f) / 9.f;
}

//==========================================================================
//
// S_ChannelEnded (callback for sound interface code)
//
//==========================================================================

void SoundEngine::ChannelEnded(FISoundChannel *ichan)
{
	FSoundChan *schan = static_cast<FSoundChan*>(ichan);
	bool evicted;

	if (schan != NULL)
	{
		// If the sound was stopped with GSnd->StopSound(), then we know
		// it wasn't evicted. Otherwise, if it's looping, it must have
		// been evicted. If it's not looping, then it was evicted if it
		// didn't reach the end of its playback.
		if (schan->ChanFlags & CHANF_FORGETTABLE)
		{
			evicted = false;
		}
		else if (schan->ChanFlags & (CHANF_LOOP | CHANF_EVICTED))
		{
			evicted = true;
		}
		else
		{ 
			unsigned int pos = GSnd->GetPosition(schan);
			unsigned int len = GSnd->GetSampleLength(S_sfx[schan->SoundID.index()].data);
			if (pos == 0)
			{
				evicted = !!(schan->ChanFlags & CHANF_JUSTSTARTED);
			}
			else
			{
				evicted = (pos < len);
			}
		}
		if (!evicted)
		{
			schan->ChanFlags &= ~CHANF_EVICTED;
		}
		else
		{
			schan->ChanFlags |= CHANF_EVICTED;
			schan->SysChannel = NULL;
		}

	}
}

//==========================================================================
//
// 
//
//==========================================================================

void SoundEngine::SoundDone(FISoundChannel* ichan)
{
	FSoundChan* schan = static_cast<FSoundChan*>(ichan);
	if (schan != NULL)
	{
		ReturnChannel(schan);
	}
}

//==========================================================================
//
// S_ChannelVirtualChanged (callback for sound interface code)
//
//==========================================================================

void SoundEngine::ChannelVirtualChanged(FISoundChannel *ichan, bool is_virtual)
{
	FSoundChan *schan = static_cast<FSoundChan*>(ichan);
	if (is_virtual)
	{
		schan->ChanFlags |= CHANF_VIRTUAL;
	}
	else
	{
		schan->ChanFlags &= ~CHANF_VIRTUAL;
	}
}

//==========================================================================
//
// StopChannel
//
//==========================================================================

void SoundEngine::StopChannel(FSoundChan *chan)
{
	if (chan == NULL)
		return;

	if (chan->SysChannel != NULL)
	{
		// S_EvictAllChannels() will set the CHAN_EVICTED flag to indicate
		// that it wants to keep all the channel information around.
		if (!(chan->ChanFlags & CHANF_EVICTED))
		{
			chan->ChanFlags |= CHANF_FORGETTABLE;
		}
		if (GSnd) GSnd->StopChannel(chan);
	}
	else
	{
		ReturnChannel(chan);
	}
}

void SoundEngine::UnloadAllSounds()
{
	for (unsigned i = 0; i < S_sfx.Size(); i++)
	{
		UnloadSound(&S_sfx[i]);
	}
}

void SoundEngine::Reset()
{
	EvictAllChannels();
	I_CloseSound();
	I_InitSound();
	RestoreEvictedChannels();
}


//==========================================================================
//
// S_FindSound
//
// Given a logical name, find the sound's index in S_sfx.
//==========================================================================

FSoundID SoundEngine::FindSound(const char* logicalname)
{
	if (!logicalname) return NO_SOUND;
	FName name(logicalname, true);
	if (name == NAME_None) return NO_SOUND;
	auto p = SoundMap.CheckKey(name);
	return p ? *p : NO_SOUND;
}

FSoundID SoundEngine::FindSoundByResID(int resid)
{
	auto p = ResIdMap.CheckKey(resid);
	return p ? *p : NO_SOUND;
}

//==========================================================================
//
// S_FindSoundNoHash
//
// Given a logical name, find the sound's index in S_sfx without
// using the hash table.
//==========================================================================

FSoundID SoundEngine::FindSoundNoHash(const char* logicalname)
{
	if (!logicalname) return NO_SOUND;
	FName name(logicalname, true);
	if (name == NAME_None) return NO_SOUND;

	for (unsigned i = 1; i < S_sfx.Size(); i++)
	{
		if (S_sfx[i].name == name)
		{
			return FSoundID::fromInt(i);
		}
	}
	return NO_SOUND;
}

//==========================================================================
//
// S_FindSoundByResIDNoHash
//
// same with resource IDs.
//==========================================================================

FSoundID SoundEngine::FindSoundByResIDNoHash(int resid)
{
	unsigned int i;

	for (i = 1; i < S_sfx.Size(); i++)
	{
		if (S_sfx[i].ResourceId == resid)
		{
			return FSoundID::fromInt(i);
		}
	}
	return NO_SOUND;
}

//==========================================================================
//
// S_FindSoundByLump
//
// Given a sound lump, find the sound's index in S_sfx.
//==========================================================================

FSoundID SoundEngine::FindSoundByLump(int lump)
{
	if (lump != -1)
	{
		unsigned int i;

		for (i = 1; i < S_sfx.Size(); i++)
			if (S_sfx[i].lumpnum == lump)
				return FSoundID::fromInt(i);
	}
	return NO_SOUND;
}

//==========================================================================
//
// S_AddSoundLump
//
// Adds a new sound mapping to S_sfx.
//==========================================================================

FSoundID SoundEngine::AddSoundLump(const char* logicalname, int lump, int CurrentPitchMask, int resid, int nearlimit)
{
	S_sfx.Reserve(1);
	sfxinfo_t &newsfx = S_sfx.Last();

	newsfx.name = logicalname;
	newsfx.lumpnum = lump;
	newsfx.PitchMask = CurrentPitchMask;
	newsfx.NearLimit = nearlimit;
	newsfx.ResourceId = resid;
	newsfx.bTentative = false;
	auto id = FSoundID::fromInt(S_sfx.Size() - 1);
	return id;
}


//==========================================================================
//
// S_FindSoundTentative
//
// Given a logical name, find the sound's index in S_sfx without
// using the hash table. If it does not exist, a new sound without
// an associated lump is created.
//==========================================================================

FSoundID SoundEngine::FindSoundTentative(const char* name, int nearlimit)
{
	auto id = FindSoundNoHash(name);
	if (id == NO_SOUND)
	{
		id = AddSoundLump(name, -1, 0, -1, nearlimit);
		S_sfx[id.index()].bTentative = true;
	}
	return id;
}


//==========================================================================
//
// S_CacheRandomSound
//
// Loads all sounds a random sound might play.
//
//==========================================================================

void SoundEngine::CacheRandomSound(sfxinfo_t* sfx)
{
	if (sfx->bRandomHeader)
	{
		const FRandomSoundList* list = &S_rnd[sfx->link.index()];
		for (unsigned i = 0; i < list->Choices.Size(); ++i)
		{
			sfx = &S_sfx[list->Choices[i].index()];
			sfx->bUsed = true;
			CacheSound(&S_sfx[list->Choices[i].index()]);
		}
	}
}

//==========================================================================
//
// S_GetSoundMSLength
//
// Returns duration of sound
// GZDoom does not use this due to player sound handling
//
//==========================================================================

unsigned int SoundEngine::GetMSLength(FSoundID sound)
{
	if (!isValidSoundId(sound))
	{
		return 0;
	}

	sfxinfo_t* sfx = &S_sfx[sound.index()];

	// Resolve player sounds, random sounds, and aliases
	if (sfx->link != sfxinfo_t::NO_LINK)
	{
		if (sfx->bRandomHeader)
		{
			// Hm... What should we do here?
			// Pick the longest or the shortest sound?
			// I think the longest one makes more sense.

			int length = 0;
			const FRandomSoundList* list = &S_rnd[sfx->link.index()];

			for (auto& me : list->Choices)
			{
				// unfortunately we must load all sounds to find the longest one... :(
				int thislen = GetMSLength(me);
				if (thislen > length) length = thislen;
			}
			return length;
		}
		else
		{
			sfx = &S_sfx[sfx->link.index()];
		}
	}

	sfx = LoadSound(sfx);
	if (sfx != NULL) return GSnd->GetMSLength(sfx->data);
	else return 0;
}

//==========================================================================
//
// S_PickReplacement
//
// Picks a replacement sound from the associated random list. If this sound
// is not the head of a random list, then the sound passed is returned.
//==========================================================================

FSoundID SoundEngine::PickReplacement(FSoundID refid)
{
	while (S_sfx[refid.index()].bRandomHeader)
	{
		const FRandomSoundList* list = &S_rnd[S_sfx[refid.index()].link.index()];
		refid = list->Choices[rand() % int(list->Choices.Size())];
	}
	return refid;
}

//==========================================================================
//
// S_HashSounds
//
// Fills in the next and index fields of S_sfx to form a working hash table.
//==========================================================================

void SoundEngine::HashSounds()
{
	S_sfx.ShrinkToFit();
	SoundMap.Clear();
	ResIdMap.Clear();

	for (unsigned i = 1; i < S_sfx.Size(); i++)
	{
		SoundMap.Insert(S_sfx[i].name, FSoundID::fromInt(i));
		if (S_sfx[i].ResourceId != -1)
		{
			ResIdMap.Insert(S_sfx[i].ResourceId, FSoundID::fromInt(i));
		}
	}
	S_rnd.ShrinkToFit();


}

void SoundEngine::AddRandomSound(FSoundID Owner, TArray<FSoundID> list)
{
	auto index = S_rnd.Reserve(1);
	auto& random = S_rnd.Last();
	random.Choices = std::move(list);
	random.Owner = Owner;
	S_sfx[Owner.index()].link = FSoundID::fromInt(index);
	S_sfx[Owner.index()].bRandomHeader = true;
	S_sfx[Owner.index()].NearLimit = -1;
}

void S_SoundReset()
{
	S_StopMusic(true);
	soundEngine->Reset();
	S_RestartMusic();
}

//==========================================================================
//
// CCMD cachesound <sound name>
//
//==========================================================================

#include "s_music.h"
#include "vm.h"
#include "c_dispatch.h"
#include "stats.h"
#include "i_net.h"
#include "i_interface.h"


CCMD(cachesound)
{
	if (argv.argc() < 2)
	{
		Printf("Usage: cachesound <sound> ...\n");
		return;
	}
	for (int i = 1; i < argv.argc(); ++i)
	{
		FSoundID sfxnum = S_FindSound(argv[i]);
		if (sfxnum != NO_SOUND)
		{
			soundEngine->CacheSound(sfxnum);
		}
	}
}


CCMD(listsoundchannels)
{
	Printf("%s", soundEngine->ListSoundChannels().GetChars());
}

// intentionally moved here to keep the s_music include out of the rest of the file.

//==========================================================================
//
// S_PauseSound
//
// Stop music and sound effects, during game PAUSE.
//==========================================================================

void S_PauseSound(bool notmusic, bool notsfx)
{
	if (!notmusic)
	{
		S_PauseMusic();
	}
	if (!notsfx)
	{
		soundEngine->SetPaused(true);
		GSnd->SetSfxPaused(true, 0);
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

void S_ResumeSound(bool notsfx)
{
	S_ResumeMusic();
	if (!notsfx)
	{
		soundEngine->SetPaused(false);
		GSnd->SetSfxPaused(false, 0);
	}
}

DEFINE_ACTION_FUNCTION(DObject, S_ResumeSound)
{
	PARAM_PROLOGUE;
	PARAM_BOOL(notsfx);
	S_ResumeSound(notsfx);
	return 0;
}

//==========================================================================
//
// S_SetSoundPaused
//
// Called with state non-zero when the app is active, zero when it isn't.
//
//==========================================================================

void S_SetSoundPaused(int state)
{
	if (!netgame && (i_pauseinbackground))
	{
		pauseext = !state;
	}

	if ((state || i_soundinbackground) && !pauseext)
	{
		S_ResumeSound(true);
		if (GSnd != nullptr)
		{
			GSnd->SetInactive(SoundRenderer::INACTIVE_Active);
		}
	}
	else
	{
		S_PauseSound(false, true);
		if (GSnd != nullptr)
		{
			GSnd->SetInactive(gamestate == GS_LEVEL || gamestate == GS_TITLELEVEL ?
				SoundRenderer::INACTIVE_Complete :
				SoundRenderer::INACTIVE_Mute);
		}
	}
}



CCMD(snd_status)
{
	GSnd->PrintStatus();
}

CCMD(snd_listdrivers)
{
	GSnd->PrintDriversList();
}

CCMD(listsounds)
{
	for (unsigned i = 1; i < soundEngine->GetNumSounds(); i++)
	{
		auto S_sfx = soundEngine->GetSfx(FSoundID::fromInt(i));
		Printf("%04d: %s\n", i, S_sfx->name.GetChars());
	}
}

ADD_STAT(sound)
{
	return GSnd->GatherStats();
}


