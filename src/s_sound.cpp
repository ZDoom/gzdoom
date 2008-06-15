// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id: s_sound.c,v 1.3 1998/01/05 16:26:08 pekangas Exp $
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
//
// DESCRIPTION:  none
//
//-----------------------------------------------------------------------------


#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <io.h>
#endif
#include <fcntl.h>
#include "m_alloc.h"

#include "i_system.h"
#include "i_sound.h"
#include "i_music.h"
#include "i_cd.h"
#include "s_sound.h"
#include "s_sndseq.h"
#include "s_playlist.h"
#include "c_dispatch.h"
#include "m_random.h"
#include "w_wad.h"
#include "doomdef.h"
#include "p_local.h"
#include "doomstat.h"
#include "cmdlib.h"
#include "v_video.h"
#include "v_text.h"
#include "a_sharedglobal.h"
#include "gstrings.h"
#include "vectors.h"
#include "gi.h"
#include "templates.h"
#include "zstring.h"
#include "timidity/timidity.h"

// MACROS ------------------------------------------------------------------

#ifdef NeXT
// NeXT doesn't need a binary flag in open call
#define O_BINARY 0
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

#define SELECT_ATTEN(a)			((a)==ATTN_NONE ? 0 : (a)==ATTN_STATIC ? 3 : 1)
#ifndef FIXED2FLOAT
#define FIXED2FLOAT(f)			(((float)(f))/(float)65536)
#endif

#define NORM_PITCH				128
#define NORM_PRIORITY			64
#define NORM_SEP				0

#define S_PITCH_PERTURB 		1
#define S_STEREO_SWING			0.75

// TYPES -------------------------------------------------------------------

struct MusPlayingInfo
{
	FString name;
	void *handle;
	int   baseorder;
	bool  loop;
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

extern float S_GetMusicVolume (const char *music);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

FSoundChan *S_StartSound(fixed_t *pt, AActor *mover, sector_t *sec, int channel,
	FSoundID sound_id, float volume, float attenuation);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static fixed_t P_AproxDistance2(fixed_t *listener, fixed_t x, fixed_t y);
static bool S_CheckSoundLimit(sfxinfo_t *sfx, float pos[3], int NearLimit);
static void S_ActivatePlayList(bool goBack);
static void CalcPosVel(fixed_t *pt, AActor *mover, int constz, float pos[3],
	float vel[3]);

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static bool		MusicPaused;		// whether music is paused
static MusPlayingInfo mus_playing;	// music currently being played
static FString	 LastSong;			// last music that was played
static FPlayList *PlayList;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int sfx_empty;

FSoundChan *Channels;
FSoundChan *FreeChannels;

int S_RolloffType;
float S_MinDistance;
float S_MaxDistanceOrRolloffFactor;
BYTE *S_SoundCurve;
int S_SoundCurveSize;

CVAR (Bool, snd_surround, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)	// [RH] Use surround sounds?
FBoolCVar noisedebug ("noise", false, 0);	// [RH] Print sound debugging info?
CVAR (Int, snd_channels, 32, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)	// number of channels available
CVAR (Bool, snd_flipstereo, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

// CODE --------------------------------------------------------------------

//==========================================================================
//
// P_AproxDistance2
//
//==========================================================================

static fixed_t P_AproxDistance2 (fixed_t *listener, fixed_t x, fixed_t y)
{
	// calculate the distance to sound origin
	//	and clip it if necessary
	if (listener)
	{
		fixed_t adx = abs (listener[0] - x);
		fixed_t ady = abs (listener[1] - y);
		// From _GG1_ p.428. Appox. eucledian distance fast.
		return adx + ady - ((adx < ady ? adx : ady)>>1);
	}
	else
		return 0;
}

static fixed_t P_AproxDistance2 (AActor *listener, fixed_t x, fixed_t y)
{
	return listener ? P_AproxDistance2 (&listener->x, x, y) : 0;
}

//==========================================================================
//
// S_NoiseDebug
//
// [RH] Print sound debug info. Called by status bar.
//==========================================================================

void S_NoiseDebug (void)
{
	FSoundChan *chan;
	fixed_t ox, oy, oz;
	int y, color;

	y = 32 * CleanYfac;
	screen->DrawText (CR_YELLOW, 0, y, "*** SOUND DEBUG INFO ***", TAG_DONE);
	y += 8;

	screen->DrawText (CR_GOLD, 0, y, "name", TAG_DONE);
	screen->DrawText (CR_GOLD, 70, y, "x", TAG_DONE);
	screen->DrawText (CR_GOLD, 120, y, "y", TAG_DONE);
	screen->DrawText (CR_GOLD, 170, y, "z", TAG_DONE);
	screen->DrawText (CR_GOLD, 220, y, "vol", TAG_DONE);
	screen->DrawText (CR_GOLD, 250, y, "dist", TAG_DONE);
	screen->DrawText (CR_GOLD, 290, y, "chan", TAG_DONE);
	y += 8;

	if (Channels == NULL)
	{
		return;
	}

	// Display the last channel first.
	for (chan = Channels; chan->NextChan != NULL; chan = chan->NextChan)
	{ }
	while (y < SCREENHEIGHT - 16)
	{
		char temp[16];
		fixed_t *origin = chan->Pt;

		if (!(chan->ChanFlags & CHAN_IS3D))
		{
			ox = players[consoleplayer].camera->x;
			oy = players[consoleplayer].camera->y;
			oz = players[consoleplayer].camera->z;
		}
		else if (origin)
		{
			ox = origin[0];
			oy = origin[1];
			oz = origin[2];
		}
		else
		{
			ox = chan->X;
			oy = chan->Y;
			oz = chan->Z;
		}
		color = (chan->ChanFlags & CHAN_LOOP) ? CR_BROWN : CR_GREY;
		Wads.GetLumpName (temp, chan->SfxInfo->lumpnum);
		temp[8] = 0;
		screen->DrawText (color, 0, y, temp, TAG_DONE);
		sprintf (temp, "%d", ox >> FRACBITS);
		screen->DrawText (color, 70, y, temp, TAG_DONE);
		sprintf (temp, "%d", oy >> FRACBITS);
		screen->DrawText (color, 120, y, temp, TAG_DONE);
		sprintf (temp, "%d", oz >> FRACBITS);
		screen->DrawText (color, 170, y, temp, TAG_DONE);
		sprintf (temp, "%g", chan->Volume);
		screen->DrawText (color, 220, y, temp, TAG_DONE);
		sprintf (temp, "%d", P_AproxDistance2 (players[consoleplayer].camera, ox, oy) / FRACUNIT);
		screen->DrawText (color, 250, y, temp, TAG_DONE);
		sprintf (temp, "%d", chan->EntChannel);
		screen->DrawText (color, 290, y, temp, TAG_DONE);
		y += 8;
		if (chan->PrevChan == &Channels)
		{
			break;
		}
		chan = (FSoundChan *)((size_t)chan->PrevChan - myoffsetof(FSoundChan, NextChan));
	}
	BorderNeedRefresh = screen->GetPageCount ();
}

static FString LastLocalSndInfo;
static FString LastLocalSndSeq;
void S_AddLocalSndInfo(int lump);

//==========================================================================
//
// S_Init
//
// Initializes sound stuff, including volume. Sets channels, SFX and
// music volume, allocates channel buffer, and sets S_sfx lookup.
//==========================================================================

void S_Init ()
{
	int curvelump;

	atterm (S_Shutdown);

	// remove old data (S_Init can be called multiple times!)
	if (S_SoundCurve != NULL)
	{
		delete[] S_SoundCurve;
	}

	// Heretic and Hexen have sound curve lookup tables. Doom does not.
	curvelump = Wads.CheckNumForName ("SNDCURVE");
	if (curvelump >= 0)
	{
		S_SoundCurveSize = Wads.LumpLength (curvelump);
		S_SoundCurve = new BYTE[S_SoundCurveSize];
		Wads.ReadLump(curvelump, S_SoundCurve);
	}

	// Free all channels for use.
	while (Channels != NULL)
	{
		S_ReturnChannel(Channels);
	}

	// no sounds are playing, and they are not paused
	MusicPaused = false;
}

//==========================================================================
//
// S_InitData
//
//==========================================================================

void S_InitData ()
{
	LastLocalSndInfo = LastLocalSndSeq = "";
	S_ParseSndInfo ();
	S_ParseSndSeq (-1);
	S_ParseReverbDef ();
}

//==========================================================================
//
// S_Shutdown
//
//==========================================================================

void S_Shutdown ()
{
	FSoundChan *chan, *next;

	if (GSnd != NULL)
	{
		while (Channels != NULL)
		{
			GSnd->StopSound(Channels);
		}
		GSnd->UpdateSounds();
	}
	else
	{
		for (chan = Channels; chan != NULL; chan = next)
		{
			next = chan->NextChan;
			delete chan;
		}
		Channels = NULL;
	}
	for (chan = FreeChannels; chan != NULL; chan = next)
	{
		next = chan->NextChan;
		delete chan;
	}
	FreeChannels = NULL;

	if (S_SoundCurve != NULL)
	{
		delete[] S_SoundCurve;
		S_SoundCurve = NULL;
	}
	if (PlayList != NULL)
	{
		delete PlayList;
		PlayList = NULL;
	}
}

//==========================================================================
//
// S_Start
//
// Per level startup code. Kills playing sounds at start of level
// and starts new music.
//==========================================================================

void S_Start ()
{
	if (GSnd)
	{
		// kill all playing sounds at start of level (trust me - a good idea)
		S_StopAllChannels();
		
		// Check for local sound definitions. Only reload if they differ
		// from the previous ones.
		const char *LocalSndInfo;
		const char *LocalSndSeq;
		
		// To be certain better check whether level is valid!
		if (level.info && level.info->soundinfo)
		{
			LocalSndInfo = level.info->soundinfo;
		}
		else
		{
			LocalSndInfo = "";
		}

		if (level.info && level.info->sndseq)
		{
			LocalSndSeq  = level.info->sndseq;
		}
		else
		{
			LocalSndSeq  = "";
		}

		bool parse_ss = false;

		// This level uses a different local SNDINFO
		if (LastLocalSndInfo.CompareNoCase(LocalSndInfo) != 0 || !level.info)
		{
			// First delete the old sound list
			for(unsigned i = 1; i < S_sfx.Size(); i++) 
			{
				GSnd->UnloadSound(&S_sfx[i]);
			}
			
			// Parse the global SNDINFO
			S_ParseSndInfo();
		
			if (*LocalSndInfo)
			{
				// Now parse the local SNDINFO
				int j = Wads.CheckNumForFullName(LocalSndInfo, true);
				if (j>=0) S_AddLocalSndInfo(j);
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
			S_ParseSndSeq(*LocalSndSeq? Wads.CheckNumForFullName(LocalSndSeq, true) : -1);
		}
		else
		
		LastLocalSndInfo = LocalSndInfo;
		LastLocalSndSeq = LocalSndSeq;
	}

	// stop the old music if it has been paused.
	// This ensures that the new music is started from the beginning
	// if it's the same as the last one and it has been paused.
	if (MusicPaused) S_StopMusic(true);

	// start new music for the level
	MusicPaused = false;

	// [RH] This is a lot simpler now.
	if (!savegamerestore)
	{
		if (level.cdtrack == 0 || !S_ChangeCDMusic (level.cdtrack, level.cdid))
			S_ChangeMusic (level.music, level.musicorder);
	}
}

//==========================================================================
//
// S_PrecacheLevel
//
// Like R_PrecacheLevel, but for sounds.
//
//==========================================================================

void S_PrecacheLevel ()
{
	unsigned int i;

	if (GSnd)
	{
		for (i = 0; i < S_sfx.Size(); ++i)
		{
			S_sfx[i].bUsed = false;
		}

		AActor *actor;
		TThinkerIterator<AActor> iterator;

		while ( (actor = iterator.Next ()) != NULL )
		{
			S_sfx[actor->SeeSound].bUsed = true;
			S_sfx[actor->AttackSound].bUsed = true;
			S_sfx[actor->PainSound].bUsed = true;
			S_sfx[actor->DeathSound].bUsed = true;
			S_sfx[actor->ActiveSound].bUsed = true;
			S_sfx[actor->UseSound].bUsed = true;
		}

		for (i = 1; i < S_sfx.Size(); ++i)
		{
			if (S_sfx[i].bUsed)
			{
				S_CacheSound (&S_sfx[i]);
			}
		}
		for (i = 1; i < S_sfx.Size(); ++i)
		{
			if (!S_sfx[i].bUsed && S_sfx[i].link == sfxinfo_t::NO_LINK)
			{
				GSnd->UnloadSound (&S_sfx[i]);
			}
		}
	}
}

//==========================================================================
//
// S_CacheSound
//
//==========================================================================

void S_CacheSound (sfxinfo_t *sfx)
{
	if (GSnd)
	{
		if (sfx->bPlayerReserve)
		{
			return;
		}
		else if (sfx->bRandomHeader)
		{
			S_CacheRandomSound (sfx);
		}
		else
		{
			while (sfx->link != sfxinfo_t::NO_LINK)
			{
				sfx = &S_sfx[sfx->link];
			}
			sfx->bUsed = true;
			GSnd->LoadSound (sfx);
		}
	}
}

//==========================================================================
//
// S_GetChannel
//
// Returns a free channel for the system sound interface.
//
//==========================================================================

FSoundChan *S_GetChannel(void *syschan)
{
	FSoundChan *chan;

	if (FreeChannels != NULL)
	{
		chan = FreeChannels;
		S_UnlinkChannel(chan);
	}
	else
	{
		chan = new FSoundChan;
		memset(chan, 0, sizeof(*chan));
	}
	S_LinkChannel(chan, &Channels);
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

void S_ReturnChannel(FSoundChan *chan)
{
	if (chan->Mover != NULL)
	{
		chan->Mover->SoundChans &= ~(1 << chan->EntChannel);
	}
	S_UnlinkChannel(chan);
	memset(chan, 0, sizeof(*chan));
	S_LinkChannel(chan, &FreeChannels);
}

//==========================================================================
//
// S_UnlinkChannel
//
//==========================================================================

void S_UnlinkChannel(FSoundChan *chan)
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

void S_LinkChannel(FSoundChan *chan, FSoundChan **head)
{
	chan->NextChan = *head;
	if (chan->NextChan != NULL)
	{
		chan->NextChan->PrevChan = &chan->NextChan;
	}
	*head = chan;
	chan->PrevChan = head;
}

// [RH] Split S_StartSoundAtVolume into multiple parts so that sounds can
//		be specified both by id and by name. Also borrowed some stuff from
//		Hexen and parameters from Quake.

//==========================================================================
//
// CalcPosVel
//
// Calculates a sound's position and velocity for 3D sounds.
//=========================================================================

void CalcPosVel (fixed_t *pt, AActor *mover, int constz,
				 float pos[3], float vel[3])
{
	if (mover != NULL)
	{
		vel[0] = FIXED2FLOAT(mover->momx) * TICRATE;
		vel[1] = FIXED2FLOAT(mover->momz) * TICRATE;
		vel[2] = FIXED2FLOAT(mover->momy) * TICRATE;
	}
	else
	{
		vel[0] = vel[1] = vel[2] = 0.f;
	}

	pos[0] = FIXED2FLOAT (pt[0]);
	pos[2] = FIXED2FLOAT (pt[1]);
	if (constz)
	{
		pos[1] = FIXED2FLOAT(players[consoleplayer].camera->z);
		vel[1] = 0.f;
	}
	else
	{
		pos[1] = FIXED2FLOAT(pt[2]);
	}
}

//==========================================================================
//
// S_StartSound
//
// 0 attenuation means full volume over whole level
// 0<attenuation<=1 means to scale the distance by that amount when
//		calculating volume
//==========================================================================

FSoundChan *S_StartSound (fixed_t *pt, AActor *mover, sector_t *sec, int channel,
	FSoundID sound_id, float volume, float attenuation)
{
	sfxinfo_t *sfx;
	int chanflags;
	int basepriority;
	int org_id;
	int pitch;
	fixed_t x, y, z;
	FSoundChan *chan;
	float pos[3];
	float vel[3];

	if (sound_id <= 0 || volume <= 0 || GSnd == NULL)
		return NULL;

	org_id = sound_id;
	chanflags = channel & ~7;

	if (pt == NULL)
	{
		attenuation = 0;
		// Give these variables values, although they don't really matter
		x = y = z = 0;
		fixed_t pt2[3] = { 0, 0, 0 };
		CalcPosVel (pt2, mover, chanflags & CHAN_LISTENERZ, pos, vel);
	}
	else
	{
		x = pt[0];
		y = pt[1];
		z = pt[2];
		CalcPosVel (pt, mover, chanflags & CHAN_LISTENERZ, pos, vel);
	}

	if (chanflags & CHAN_IMMOBILE)
	{
		pt = NULL;
	}
	if (i_compatflags & COMPATF_MAGICSILENCE)
	{ // For people who just can't play without a silent BFG.
		channel = CHAN_WEAPON;
	}
	else
	{
		if ((channel & CHAN_MAYBE_LOCAL) && (i_compatflags & COMPATF_SILENTPICKUP))
		{
			if (mover != NULL && mover != players[consoleplayer].camera)
			{
				return NULL;
			}
		}
		channel &= 7;
	}

	sfx = &S_sfx[sound_id];

	// Scale volume according to SNDINFO data.
	volume = MIN(volume * sfx->Volume, 1.f);
	if (volume <= 0)
		return NULL;

	// When resolving a link we do not want to get the NearLimit of
	// the referenced sound so some additional checks are required
	int NearLimit = sfx->NearLimit;

	// Resolve player sounds, random sounds, and aliases
	while (sfx->link != sfxinfo_t::NO_LINK)
	{
		if (sfx->bPlayerReserve)
		{
			sound_id = FSoundID(S_FindSkinnedSound (mover, sound_id));
			NearLimit = S_sfx[sound_id].NearLimit;
		}
		else if (sfx->bRandomHeader)
		{
			sound_id = FSoundID(S_PickReplacement (sound_id));
			if (NearLimit < 0) NearLimit = S_sfx[sound_id].NearLimit;
		}
		else
		{
			sound_id = FSoundID(sfx->link);
			if (NearLimit < 0) NearLimit = S_sfx[sound_id].NearLimit;
		}
		sfx = &S_sfx[sound_id];
	}

	// If this is a singular sound, don't play it if it's already playing.
	if (sfx->bSingular && S_CheckSingular(sound_id))
		return NULL;

	// If this sound doesn't like playing near itself, don't play it if
	// that's what would happen.
	if (NearLimit > 0 && pt != NULL && mover != players[consoleplayer].camera &&
		S_CheckSoundLimit(sfx, pos, NearLimit))
		return NULL;

	// Make sure the sound is loaded.
	if (sfx->data == NULL)
	{
		GSnd->LoadSound (sfx);
		if (sfx->link != sfxinfo_t::NO_LINK)
		{
			sfx = &S_sfx[sfx->link];
		}
	}

	// The empty sound never plays.
	if (sfx->lumpnum == sfx_empty)
	{
		return NULL;
	}

	// Select priority.
	if (attenuation <= 0 || mover == players[consoleplayer].camera)
	{
		basepriority = 40;
	}
	else
	{
		basepriority = 0;
	}

	if (mover != NULL && channel == CHAN_AUTO)
	{ // Select a channel that isn't already playing something.
		BYTE mask = mover->SoundChans;

		// Try channel 0 first, then travel from channel 7 down.
		if ((mask & 1) == 0)
		{
			channel = 0;
		}
		else
		{
			for (channel = 7; channel > 0; --channel, mask <<= 1)
			{
				if ((mask & 0x80) == 0)
				{
					break;
				}
			}
			if (channel == 0)
			{ // Crap. No free channels.
				return NULL;
			}
		}
	}

	// If this actor is already playing something on the selected channel, stop it.
	if ((mover == NULL && channel != CHAN_AUTO) || (mover != NULL && mover->SoundChans & (1 << channel)))
	{
		for (chan = Channels; chan != NULL; chan = chan->NextChan)
		{
			if (((mover != NULL && chan->Mover == mover) || (chan->Pt == pt)) && chan->EntChannel == channel)
			{
				GSnd->StopSound(chan);
				break;
			}
		}
	}

	// Vary the sfx pitches.
	if (sfx->PitchMask != 0)
	{
		pitch = NORM_PITCH - (M_Random()&sfx->PitchMask) + (M_Random()&sfx->PitchMask);
	}
	else
	{
		pitch = NORM_PITCH;
	}

	if (attenuation > 0)
	{
		chan = GSnd->StartSound3D (sfx, volume, attenuation, pitch, basepriority, pos, vel, sec, channel, chanflags);
		chanflags |= CHAN_IS3D;
	}
	else
	{
		chan = GSnd->StartSound (sfx, volume, pitch, chanflags);
		chanflags |= CHAN_LISTENERZ;
	}
	if (chan != NULL)
	{
		chan->SoundID = sound_id;
		chan->OrgID = org_id;
		chan->Mover = mover;
		chan->Pt = pt != NULL ? pt : &chan->X;
		chan->Sector = sec;
		chan->SfxInfo = sfx;
		chan->EntChannel = channel;
		chan->Volume = volume;
		chan->X = x;
		chan->Y = y;
		chan->Z = z;
		chan->ChanFlags |= chanflags;
		if (mover != NULL)
		{
			mover->SoundChans |= 1 << channel;
		}
	}
	return chan;
}

//==========================================================================
//
// S_Sound - Unpositioned version
//
//==========================================================================

void S_Sound (int channel, FSoundID sound_id, float volume, int attenuation)
{
	S_StartSound ((fixed_t *)NULL, NULL, NULL, channel, sound_id, volume, SELECT_ATTEN(attenuation));
}

//==========================================================================
//
// S_Sound - Actor is source
//
//==========================================================================

void S_Sound (AActor *ent, int channel, FSoundID sound_id, float volume, int attenuation)
{
	if (ent->Sector->Flags & SECF_SILENT)
		return;
	S_StartSound (&ent->x, ent, NULL, channel, sound_id, volume, SELECT_ATTEN(attenuation));
}

//==========================================================================
//
// S_Sound - A random coordinate is source
//
//==========================================================================

void S_Sound (fixed_t *pt, int channel, FSoundID sound_id, float volume, int attenuation)
{
	S_StartSound (pt, NULL, NULL, channel, sound_id, volume, SELECT_ATTEN(attenuation));
}

//==========================================================================
//
// S_Sound - A point is source
//
//==========================================================================

void S_Sound (fixed_t x, fixed_t y, fixed_t z, int channel, FSoundID sound_id, float volume, int attenuation)
{
	fixed_t pt[3];
	pt[0] = x;
	pt[1] = y;
	pt[2] = z;
	S_StartSound (pt, NULL, NULL, channel|CHAN_IMMOBILE, sound_id, volume, SELECT_ATTEN(attenuation));
}

//==========================================================================
//
// S_Sound - An entire sector is source
//
//==========================================================================

void S_Sound (sector_t *sec, int channel, FSoundID sfxid, float volume, int attenuation)
{
	S_StartSound (sec->soundorg, NULL, sec, channel, sfxid, volume, attenuation);
}

//==========================================================================
//
// S_CheckSingular
//
// Returns true if a copy of this sound is already playing.
//
//==========================================================================

bool S_CheckSingular(int sound_id)
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
// playing within 256 units of the new sound, the new sound will not
// start.
//
//==========================================================================

bool S_CheckSoundLimit(sfxinfo_t *sfx, float pos[3], int NearLimit)
{
	FSoundChan *chan;
	int count;
	
	for (chan = Channels, count = 0; chan != NULL && count < NearLimit; chan = chan->NextChan)
	{
		if (chan->SfxInfo == sfx)
		{
			double dx = FIXED2FLOAT(chan->Pt[0]) - pos[0];
			double dy = FIXED2FLOAT(chan->Pt[1]) - pos[2];
			double dz = FIXED2FLOAT(chan->Pt[2]) - pos[1];
			if (dx*dx + dy*dy + dz*dz <= 256.0*256.0)
			{
				count++;
			}
		}
	}
	return count >= NearLimit;
}

//==========================================================================
//
// S_StopSound
//
// Stops a sound from a single source playing on a specific channel.
//==========================================================================

void S_StopSound (fixed_t *pt, int channel)
{
	for (FSoundChan *chan = Channels; chan != NULL; chan = chan->NextChan)
	{
		if (((pt == NULL && chan->Pt == &chan->X) || chan->Pt == pt) &&
			((i_compatflags & COMPATF_MAGICSILENCE) || chan->EntChannel == channel))
		{
			GSnd->StopSound(chan);
		}
	}
}

void S_StopSound (AActor *ent, int channel)
{
	// No need to search every channel if we know it's not playing anything.
	if (ent != NULL && ent->SoundChans & (1 << channel))
	{
		S_StopSound (&ent->x, channel);
	}
}

//==========================================================================
//
// S_StopAllChannels
//
//==========================================================================

void S_StopAllChannels ()
{
	if (GSnd == NULL)
	{
		return;
	}
	SN_StopAllSequences();
	while (Channels != NULL)
	{
		GSnd->StopSound(Channels);
	}
	GSnd->UpdateSounds();
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
	if (from == NULL || GSnd == NULL)
		return;

	fixed_t *frompt = &from->x;
	fixed_t *topt = to ? &to->x : NULL;

	for (FSoundChan *chan = Channels; chan != NULL; chan = chan->NextChan)
	{
		if (chan->Pt == frompt)
		{
			if (to != NULL || !(chan->ChanFlags & CHAN_LOOP))
			{
				chan->Pt = topt ? topt : &chan->X;
				chan->X = frompt[0];
				chan->Y = frompt[1];
				chan->Z = frompt[2];
				chan->Mover = to;
			}
			else
			{
				GSnd->StopSound(chan);
			}
		}
	}
}

//==========================================================================
//
// S_GetSoundPlayingInfo
//
// Is a sound being played by a specific actor/point?
//==========================================================================

bool S_GetSoundPlayingInfo (fixed_t *pt, int sound_id)
{
	if (sound_id > 0)
	{
		for (FSoundChan *chan = Channels; chan != NULL; chan = chan->NextChan)
		{
			if (chan->Pt == pt && chan->OrgID == sound_id)
				return true;
		}
	}
	return false;
}

bool S_GetSoundPlayingInfo (AActor *ent, int sound_id)
{
	return S_GetSoundPlayingInfo (ent ? &ent->x : NULL, sound_id);
}

//==========================================================================
//
// S_IsActorPlayingSomething
//
//==========================================================================

bool S_IsActorPlayingSomething (AActor *actor, int channel, int sound_id)
{
	if (actor->SoundChans == 0)
	{
		return false;
	}

	if (i_compatflags & COMPATF_MAGICSILENCE)
	{
		channel = 0;
	}

	for (FSoundChan *chan = Channels; chan != NULL; chan = chan->NextChan)
	{
		if (chan->Pt == &actor->x)
		{
			if (channel == 0 || chan->EntChannel == channel)
			{
				return sound_id <= 0 || chan->OrgID == sound_id;
			}
		}
	}
	return false;
}

//==========================================================================
//
// S_PauseSound
//
// Stop music and sound effects, during game PAUSE.
//==========================================================================

void S_PauseSound (bool notmusic)
{
	if (!notmusic && mus_playing.handle && !MusicPaused)
	{
		I_PauseSong (mus_playing.handle);
		MusicPaused = true;
	}
	if (GSnd != NULL)
	{
		GSnd->SetSfxPaused (true);
	}
}

//==========================================================================
//
// S_ResumeSound
//
// Resume music and sound effects, after game PAUSE.
//==========================================================================

void S_ResumeSound ()
{
	if (mus_playing.handle && MusicPaused)
	{
		I_ResumeSong (mus_playing.handle);
		MusicPaused = false;
	}
	if (GSnd != NULL)
	{
		GSnd->SetSfxPaused (false);
	}
}

//==========================================================================
//
// S_UpdateSounds
//
// Updates music & sounds
//==========================================================================

void S_UpdateSounds (void *listener_p)
{
	float pos[3], vel[3];

	I_UpdateMusic();

	if (GSnd == NULL)
		return;

	// [RH] Update music and/or playlist. I_QrySongPlaying() must be called
	// to attempt to reconnect to broken net streams and to advance the
	// playlist when the current song finishes.
	if (mus_playing.handle != NULL &&
		!I_QrySongPlaying(mus_playing.handle) &&
		PlayList)
	{
		PlayList->Advance();
		S_ActivatePlayList(false);
	}

	for (FSoundChan *chan = Channels; chan != NULL; chan = chan->NextChan)
	{
		if (chan->ChanFlags & CHAN_IS3D)
		{
			CalcPosVel(chan->Pt, chan->Mover, chan->ChanFlags & CHAN_LISTENERZ, pos, vel);
			GSnd->UpdateSoundParams3D(chan, pos, vel);
		}
	}

	SN_UpdateActiveSequences();

	GSnd->UpdateListener();
	GSnd->UpdateSounds();
}

//==========================================================================
//
// FArchive & << FSoundID &
//
//==========================================================================

FArchive &operator<<(FArchive &arc, FSoundID &sid)
{
	if (arc.IsStoring())
	{
		arc.WriteName((const char *)sid);
	}
	else
	{
		sid = arc.ReadName();
	}
	return arc;
}


//==========================================================================
//
// S_ActivatePlayList
//
// Plays the next song in the playlist. If no songs in the playlist can be
// played, then it is deleted.
//==========================================================================

void S_ActivatePlayList (bool goBack)
{
	int startpos, pos;

	startpos = pos = PlayList->GetPosition ();
	S_StopMusic (true);
	while (!S_ChangeMusic (PlayList->GetSong (pos), 0, false, true))
	{
		pos = goBack ? PlayList->Backup () : PlayList->Advance ();
		if (pos == startpos)
		{
			delete PlayList;
			PlayList = NULL;
			Printf ("Cannot play anything in the playlist.\n");
			return;
		}
	}
}

//==========================================================================
//
// S_ChangeCDMusic
//
// Starts a CD track as music.
//==========================================================================

bool S_ChangeCDMusic (int track, unsigned int id, bool looping)
{
	char temp[32];

	if (id)
	{
		sprintf (temp, ",CD,%d,%x", track, id);
	}
	else
	{
		sprintf (temp, ",CD,%d", track);
	}
	return S_ChangeMusic (temp, 0, looping);
}

//==========================================================================
//
// S_StartMusic
//
// Starts some music with the given name.
//==========================================================================

bool S_StartMusic (const char *m_id)
{
	return S_ChangeMusic (m_id, 0, false);
}

//==========================================================================
//
// S_ChangeMusic
//
// Starts playing a music, possibly looping.
//
// [RH] If music is a MOD, starts it at position order. If name is of the
// format ",CD,<track>,[cd id]" song is a CD track, and if [cd id] is
// specified, it will only be played if the specified CD is in a drive.
//==========================================================================

TArray<char> musiccache;

bool S_ChangeMusic (const char *musicname, int order, bool looping, bool force)
{
	if (!force && PlayList)
	{ // Don't change if a playlist is active
		return false;
	}

	// allow specifying "*" as a placeholder to play the level's default music.
	if (musicname != NULL && !strcmp(musicname, "*"))
	{
		if (gamestate == GS_LEVEL || gamestate == GS_TITLELEVEL)
		{
			musicname = level.music;
			order = level.musicorder;
		}
		else
		{
			musicname = NULL;
		}
	}

	if (musicname == NULL || musicname[0] == 0)
	{
		// Don't choke if the map doesn't have a song attached
		S_StopMusic (true);
		return false;
	}
	
	if (!mus_playing.name.IsEmpty() && stricmp (mus_playing.name, musicname) == 0)
	{
		if (order != mus_playing.baseorder)
		{
			mus_playing.baseorder =
				(I_SetSongPosition (mus_playing.handle, order) ? order : 0);
		}
		return true;
	}

	if (strnicmp (musicname, ",CD,", 4) == 0)
	{
		int track = strtoul (musicname+4, NULL, 0);
		const char *more = strchr (musicname+4, ',');
		unsigned int id = 0;

		if (more != NULL)
		{
			id = strtoul (more+1, NULL, 16);
		}
		S_StopMusic (true);
		mus_playing.handle = I_RegisterCDSong (track, id);
	}
	else
	{
		int lumpnum = -1;
		int offset = 0, length = 0;
		int device = MDEV_DEFAULT;
		void *handle = NULL;

		int *devp = MidiDevices.CheckKey(FName(musicname));
		if (devp != NULL) device = *devp;

		// Strip off any leading file:// component.
		if (strncmp(musicname, "file://", 7) == 0)
		{
			musicname += 7;
		}

		if (!FileExists (musicname))
		{
			if ((lumpnum = Wads.CheckNumForFullName (musicname, true, ns_music)) == -1)
			{
				if (strstr(musicname, "://") > musicname)
				{
					// Looks like a URL; try it as such.
					handle = I_RegisterURLSong(musicname);
					if (handle == NULL)
					{
						Printf ("Could not open \"%s\"\n", musicname);
						return false;
					}
				}
				else
				{
					Printf ("Music \"%s\" not found\n", musicname);
					return false;
				}
			}
			if (handle == NULL)
			{
				if (!Wads.IsUncompressedFile(lumpnum))
				{
					// We must cache the music data and use it from memory.

					// shut down old music before reallocating and overwriting the cache!
					S_StopMusic (true);

					offset = -1;							// this tells the low level code that the music 
															// is being used from memory
					length = Wads.LumpLength (lumpnum);
					if (length == 0)
					{
						return false;
					}
					musiccache.Resize(length);
					Wads.ReadLump(lumpnum, &musiccache[0]);
				}
				else
				{
					offset = Wads.GetLumpOffset (lumpnum);
					length = Wads.LumpLength (lumpnum);
					if (length == 0)
					{
						return false;
					}
				}
			}
		}

		// shutdown old music
		S_StopMusic (true);

		// Just record it if volume is 0
		if (snd_musicvolume <= 0)
		{
			mus_playing.loop = looping;
			mus_playing.name = "";
			mus_playing.baseorder = 0;
			LastSong = musicname;
			return true;
		}

		// load & register it
		if (handle != NULL)
		{
			mus_playing.handle = handle;
		}
		else if (offset != -1)
		{
			mus_playing.handle = I_RegisterSong (lumpnum != -1 ?
				Wads.GetWadFullName (Wads.GetLumpFile (lumpnum)) :
				musicname, NULL, offset, length, device);
		}
		else
		{
			mus_playing.handle = I_RegisterSong (NULL, &musiccache[0], -1, length, device);
		}
	}

	mus_playing.loop = looping;
	mus_playing.name = musicname;
	LastSong = "";

	if (mus_playing.handle != 0)
	{ // play it
		I_PlaySong (mus_playing.handle, looping, S_GetMusicVolume (musicname));
		mus_playing.baseorder =
			(I_SetSongPosition (mus_playing.handle, order) ? order : 0);
		return true;
	}
	return false;
}

//==========================================================================
//
// S_RestartMusic
//
// Must only be called from snd_reset in i_sound.cpp!
//==========================================================================

void S_RestartMusic ()
{
	if (!LastSong.IsEmpty())
	{
		FString song = LastSong;
		LastSong = "";
		S_ChangeMusic (song, mus_playing.baseorder, mus_playing.loop, true);
	}
}

//==========================================================================
//
// S_GetMusic
//
//==========================================================================

int S_GetMusic (char **name)
{
	int order;

	if (mus_playing.name)
	{
		*name = copystring (mus_playing.name);
		order = mus_playing.baseorder;
	}
	else
	{
		*name = NULL;
		order = 0;
	}
	return order;
}

//==========================================================================
//
// S_StopMusic
//
//==========================================================================

void S_StopMusic (bool force)
{
	// [RH] Don't stop if a playlist is active.
	if ((force || PlayList == NULL) && !mus_playing.name.IsEmpty())
	{
		if (MusicPaused)
			I_ResumeSong(mus_playing.handle);

		I_StopSong(mus_playing.handle);
		I_UnRegisterSong(mus_playing.handle);

		LastSong = mus_playing.name;
		mus_playing.name = "";
		mus_playing.handle = 0;
	}
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
		S_Sound (CHAN_AUTO, argv[1], 1.f, ATTN_NONE);
	}
}

//==========================================================================
//
// CCMD idmus
//
//==========================================================================

CCMD (idmus)
{
	level_info_t *info;
	char *map;
	int l;

	if (argv.argc() > 1)
	{
		if (gameinfo.flags & GI_MAPxx)
		{
			l = atoi (argv[1]);
			if (l <= 99)
				map = CalcMapName (0, l);
			else
			{
				Printf ("%s\n", GStrings("STSTR_NOMUS"));
				return;
			}
		}
		else
		{
			map = CalcMapName (argv[1][0] - '0', argv[1][1] - '0');
		}

		if ( (info = FindLevelInfo (map)) )
		{
			if (info->music)
			{
				S_ChangeMusic (info->music, info->musicorder);
				Printf ("%s\n", GStrings("STSTR_MUS"));
			}
		}
		else
		{
			Printf ("%s\n", GStrings("STSTR_NOMUS"));
		}
	}
}

//==========================================================================
//
// CCMD changemus
//
//==========================================================================

CCMD (changemus)
{
	if (argv.argc() > 1)
	{
		if (PlayList)
		{
			delete PlayList;
			PlayList = NULL;
		}
		S_ChangeMusic (argv[1], argv.argc() > 2 ? atoi (argv[2]) : 0);
	}
}

//==========================================================================
//
// CCMD stopmus
//
//==========================================================================

CCMD (stopmus)
{
	if (PlayList)
	{
		delete PlayList;
		PlayList = NULL;
	}
	S_StopMusic (false);
}

//==========================================================================
//
// CCMD cd_play
//
// Plays a specified track, or the entire CD if no track is specified.
//==========================================================================

CCMD (cd_play)
{
	char musname[16];

	if (argv.argc() == 1)
		strcpy (musname, ",CD,");
	else
		sprintf (musname, ",CD,%d", atoi(argv[1]));
	S_ChangeMusic (musname, 0, true);
}

//==========================================================================
//
// CCMD cd_stop
//
//==========================================================================

CCMD (cd_stop)
{
	CD_Stop ();
}

//==========================================================================
//
// CCMD cd_eject
//
//==========================================================================

CCMD (cd_eject)
{
	CD_Eject ();
}

//==========================================================================
//
// CCMD cd_close
//
//==========================================================================

CCMD (cd_close)
{
	CD_UnEject ();
}

//==========================================================================
//
// CCMD cd_pause
//
//==========================================================================

CCMD (cd_pause)
{
	CD_Pause ();
}

//==========================================================================
//
// CCMD cd_resume
//
//==========================================================================

CCMD (cd_resume)
{
	CD_Resume ();
}

//==========================================================================
//
// CCMD playlist
//
//==========================================================================

CCMD (playlist)
{
	int argc = argv.argc();

	if (argc < 2 || argc > 3)
	{
		Printf ("playlist <playlist.m3u> [<position>|shuffle]\n");
	}
	else
	{
		if (PlayList != NULL)
		{
			PlayList->ChangeList (argv[1]);
		}
		else
		{
			PlayList = new FPlayList (argv[1]);
		}
		if (PlayList->GetNumSongs () == 0)
		{
			delete PlayList;
			PlayList = NULL;
		}
		else
		{
			if (argc == 3)
			{
				if (stricmp (argv[2], "shuffle") == 0)
				{
					PlayList->Shuffle ();
				}
				else
				{
					PlayList->SetPosition (atoi (argv[2]));
				}
			}
			S_ActivatePlayList (false);
		}
	}
}

//==========================================================================
//
// CCMD playlistpos
//
//==========================================================================

static bool CheckForPlaylist ()
{
	if (PlayList == NULL)
	{
		Printf ("No playlist is playing.\n");
		return false;
	}
	return true;
}

CCMD (playlistpos)
{
	if (CheckForPlaylist() && argv.argc() > 1)
	{
		PlayList->SetPosition (atoi (argv[1]) - 1);
		S_ActivatePlayList (false);
	}
}

//==========================================================================
//
// CCMD playlistnext
//
//==========================================================================

CCMD (playlistnext)
{
	if (CheckForPlaylist())
	{
		PlayList->Advance ();
		S_ActivatePlayList (false);
	}
}

//==========================================================================
//
// CCMD playlistprev
//
//==========================================================================

CCMD (playlistprev)
{
	if (CheckForPlaylist())
	{
		PlayList->Backup ();
		S_ActivatePlayList (true);
	}
}

//==========================================================================
//
// CCMD playliststatus
//
//==========================================================================

CCMD (playliststatus)
{
	if (CheckForPlaylist ())
	{
		Printf ("Song %d of %d:\n%s\n",
			PlayList->GetPosition () + 1,
			PlayList->GetNumSongs (),
			PlayList->GetSong (PlayList->GetPosition ()));
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
		if (sfxnum != 0)
		{
			S_CacheSound (&S_sfx[sfxnum]);
		}
	}
}
