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
// Dolby Pro Logic code by Gavino.
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
#include "z_zone.h"
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

// MACROS ------------------------------------------------------------------

#ifdef NeXT
// NeXT doesn't need a binary flag in open call
#define O_BINARY 0
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

#define SELECT_ATTEN(a)			((a)==ATTN_NONE ? 0 : (a)==ATTN_SURROUND ? -1 : \
								 (a)==ATTN_STATIC ? 3 : 1)
#ifndef FIXED2FLOAT
#define FIXED2FLOAT(f)			(((float)(f))/(float)65536)
#endif

#define PRIORITY_MAX_ADJUST		10
#define DIST_ADJUST				(MAX_SND_DIST/PRIORITY_MAX_ADJUST)

#define NORM_PITCH				128
#define NORM_PRIORITY			64
#define NORM_SEP				128
#define NONE_SEP				-2

#define S_PITCH_PERTURB 		1
#define S_STEREO_SWING			(96<<FRACBITS)

// TYPES -------------------------------------------------------------------

typedef struct
{
	AActor		*mover;		// Used for velocity
	fixed_t		*pt;		// origin of sound
	fixed_t		x,y,z;		// Origin if pt is NULL
	sfxinfo_t*	sfxinfo;	// sound information (if null, channel avail.)
	long 		handle;		// handle of the sound being played
	int			sound_id;	// sound id of playing sound
	int			org_id;		// sound id of sound used to start this channel
	int			entchannel;	// entity's sound channel
	int			basepriority;
	float		attenuation;
	float		volume;
	int			pitch;
	int			priority;
	int			tag;
	bool		loop;
	bool		is3d;
	bool		constz;
} channel_t;

struct MusPlayingInfo
{
	char *name;
	int   handle;
	int   baseorder;
	bool  loop;
};

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static fixed_t P_AproxDistance2 (fixed_t *listener, fixed_t x, fixed_t y);
static void S_StopChannel (int cnum);
static void S_StartSound (fixed_t *pt, AActor *mover, int channel,
	int sound_id, float volume, float attenuation, BOOL looping, int tag=0);
static void S_ActivatePlayList (bool goBack);
static void CalcPosVel (fixed_t *pt, AActor *mover, int constz, float pos[3],
	float vel[3]);

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int MAX_SND_DIST;
static channel_t *Channel;			// the set of channels available
static BOOL		mus_paused;			// whether songs are paused
static MusPlayingInfo mus_playing;	// music currently being played
static char		*LastSong;			// last music that was played
static byte		*SoundCurve;
static int		nextcleanup;
static FPlayList *PlayList;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// [RH] Hacks for pitch variance
int sfx_sawup, sfx_sawidl, sfx_sawful, sfx_sawhit;
int sfx_itemup, sfx_tink;

int sfx_empty;

int numChannels;

CVAR (Bool, snd_surround, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)	// [RH] Use surround sounds?
FBoolCVar noisedebug ("noise", false, 0);	// [RH] Print sound debugging info?
CVAR (Int, snd_channels, 12, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)	// number of channels available
CVAR (Bool, sv_ihatesounds, false, CVAR_SERVERINFO)
CVAR (Bool, snd_dolby, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
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
	fixed_t ox, oy;
	int i, y, color;

	y = 32 * CleanYfac;
	if (gametic & 16)
		screen->DrawText (CR_TAN, 0, y, "*** SOUND DEBUG INFO ***");
	y += 8;

	screen->DrawText (CR_GREY, 0, y, "name");
	screen->DrawText (CR_GREY, 70, y, "x");
	screen->DrawText (CR_GREY, 120, y, "y");
	screen->DrawText (CR_GREY, 170, y, "vol");
	screen->DrawText (CR_GREY, 200, y, "pri");
	screen->DrawText (CR_GREY, 240, y, "dist");
	screen->DrawText (CR_GREY, 280, y, "chan");
	y += 8;

	for (i = 0; i < numChannels && y < SCREENHEIGHT - 16; i++, y += 8)
	{
		if (Channel[i].sfxinfo)
		{
			char temp[16];
			fixed_t *origin = Channel[i].pt;

			if (Channel[i].attenuation <= 0)
			{
				ox = players[consoleplayer].camera->x;
				oy = players[consoleplayer].camera->y;
			}
			else if (origin)
			{
				ox = origin[0];
				oy = origin[1];
			}
			else
			{
				ox = Channel[i].x;
				oy = Channel[i].y;
			}
			color = Channel[i].loop ? CR_BROWN : CR_GREY;
			strcpy (temp, lumpinfo[Channel[i].sfxinfo->lumpnum].name);
			temp[8] = 0;
			screen->DrawText (color, 0, y, temp);
			sprintf (temp, "%ld", ox / FRACUNIT);
			screen->DrawText (color, 70, y, temp);
			sprintf (temp, "%ld", oy / FRACUNIT);
			screen->DrawText (color, 120, y, temp);
			sprintf (temp, "%g", Channel[i].volume);
			screen->DrawText (color, 170, y, temp);
			sprintf (temp, "%d", Channel[i].priority);
			screen->DrawText (color, 200, y, temp);
			sprintf (temp, "%ld", P_AproxDistance2 (players[consoleplayer].camera, ox, oy) / FRACUNIT);
			screen->DrawText (color, 240, y, temp);
			sprintf (temp, "%d", Channel[i].entchannel);
			screen->DrawText (color, 280, y, temp);
		}
		else
		{
			screen->DrawText (CR_GREY, 0, y, "------");
		}
	}
	BorderNeedRefresh = screen->GetPageCount ();
}

//==========================================================================
//
// S_Init
//
// Initializes sound stuff, including volume. Sets channels, SFX and
// music volume, allocates channel buffer, and sets S_sfx lookup.
//==========================================================================

void S_Init ()
{
	int i;
	int curvelump;

	curvelump = W_GetNumForName ("SNDCURVE");
	SoundCurve = (byte *)W_CacheLumpNum (curvelump, PU_STATIC);
	MAX_SND_DIST = W_LumpLength (curvelump);

	Printf ("S_Init\n");

	// [RH] Read in sound sequences
	S_ParseSndSeq ();

	// Allocating the virtual channels in zone memory
	numChannels = I_SetChannels (snd_channels);
	Z_FreeTags (PU_SOUNDCHANNELS, PU_SOUNDCHANNELS);
	Channel = (channel_t *) Z_Malloc (numChannels*sizeof(channel_t), PU_SOUNDCHANNELS, 0);

	// Free all channels for use
	for (i = 0; i < numChannels; i++)
	{
		Channel[i].pt = NULL;
		Channel[i].handle = 0;
		Channel[i].sound_id = -1;
		Channel[i].priority = 0;
		Channel[i].sfxinfo = 0;
	}
	
	// no sounds are playing, and they are not paused
	mus_paused = 0;

	// Note that sounds have not been cached (yet).
	for (i=1; (size_t)i < S_sfx.Size (); i++)
		S_sfx[i].usefulness = -1;
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
	int cnum;

	// kill all playing sounds at start of level (trust me - a good idea)
	for (cnum = 0; cnum < numChannels; cnum++)
	{
		if (Channel[cnum].sfxinfo)
		{
			S_StopChannel (cnum);
		}
	}

	// start new music for the level
	mus_paused = 0;

	// [RH] This is a lot simpler now.
	if (!savegamerestore)
	{
		if (level.cdtrack == 0 || !S_ChangeCDMusic (level.cdtrack, level.cdid))
			S_ChangeMusic (level.music, level.musicorder);
	}
	
	nextcleanup = 15;
}

// [RH] Split S_StartSoundAtVolume into multiple parts so that sounds can
//		be specified both by id and by name. Also borrowed some stuff from
//		Hexen and parameters from Quake.

//==========================================================================
//
// CalcPosVel
//
// Calculates a sounds position and velocity for 3D sounds.
//=========================================================================

void CalcPosVel (fixed_t *pt, AActor *mover, int constz,
				 float pos[3], float vel[3])
{
	if (mover != NULL && 0)
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
// -1 attenuation means full volume in surround over whole level
// 0<attenuation<=1 means to scale the distance by that amount when
//		calculating volume
//==========================================================================

static void S_StartSound (fixed_t *pt, AActor *mover, int channel,
	int sound_id, float volume, float attenuation, BOOL looping, int tag)
{
	sfxinfo_t *sfx;
	int dist, vol;
	int i;
	int chanflags;
	int basepriority, priority;
	int sep;
	int org_id;
	fixed_t x, y, z;
	angle_t angle;

	static int sndcount = 0;
	int chan;

	if (sound_id == 0 || volume <= 0)
		return;

	org_id = sound_id;

	if (pt == NULL)
	{
		if (attenuation > 0)
			attenuation = 0;
	}
	else
	{
		x = pt[0];
		y = pt[1];
		z = pt[2];
	}

	chanflags = channel & ~7;
	if (chanflags & CHAN_IMMOBILE)
	{
		pt = NULL;
	}
	if (sv_ihatesounds)
	{ // For people who just can't play without a silent BFG.
		channel = 1;
	}
	else
	{
		channel &= 7;
	}
	if (volume > 1)
		volume = 1;

	if (attenuation == 0)
	{
		sep = NONE_SEP;
		dist = 0;
	}
	else if (attenuation < 0)
	{
		sep = snd_surround ? -1 : NONE_SEP;
		dist = 0;
	}
	else
	{
		// calculate the distance before other stuff so we can throw out
		// sounds that are beyond the hearing range.
		dist = (int)(FIXED2FLOAT
				(P_AproxDistance2 (players[consoleplayer].camera, x, y))
				* attenuation);
		if (dist >= MAX_SND_DIST)
		{
			if (level.flags & LEVEL_NOSOUNDCLIPPING)
				dist = MAX_SND_DIST;
			else
				return;	// sound is beyond the hearing range...
		}
		else if (dist < 0)
			dist = 0;
		sep = -3;	// Calculate separation later on
	}

	sfx = &S_sfx[sound_id];

	// Resolve player sounds, random sounds, and aliases
	while (sfx->link != sfxinfo_t::NO_LINK)
	{
		if (sfx->bPlayerReserve)
		{
			sound_id = S_FindSkinnedSound (mover, sound_id);
		}
		else if (sfx->bRandomHeader)
		{
			sound_id = S_PickReplacement (sound_id);
		}
		else
		{
			sound_id = sfx->link;
		}
		sfx = &S_sfx[sound_id];
	}
	
	if (!sfx->data)
	{
		I_LoadSound (sfx);
		if (sfx->link != sfxinfo_t::NO_LINK)
		{
			sfx = &S_sfx[sfx->link];
		}
	}

	if (sfx->lumpnum == sfx_empty)
	{
		basepriority = -1000;
	}
	else if (attenuation <= 0)
	{
		basepriority = 200;
	}
	else
	{
		switch (channel)
		{
		case CHAN_WEAPON:
			basepriority = 100;
			break;
		case CHAN_VOICE:
			basepriority = 75;
			break;
		default:
		case CHAN_BODY:
			basepriority = 50;
			break;
		case CHAN_ITEM:
			basepriority = 25;
			break;
		}
		if (attenuation == 1)
			basepriority += 50;
		if (dist == 0)
			basepriority += 30;
	}
	priority = basepriority * (PRIORITY_MAX_ADJUST - (dist/DIST_ADJUST));

	if (!S_StopSoundID (sound_id, priority, tag))
		return;	// other sounds have greater priority

	if (pt)
	{
		for (i = 0; i < numChannels; i++)
		{
			if (Channel[i].sfxinfo &&
				pt == Channel[i].pt &&
				Channel[i].entchannel == channel)
			{
				break;
			}
		}
		if (i < numChannels && channel != CHAN_AUTO)
		{
			S_StopChannel (i);
		}
		else if (channel == CHAN_AUTO)
		{
			int chansused[8], freechan;

			memset (chansused, 0, sizeof(chansused));
			freechan = numChannels;

			for (i = 0; i < numChannels; i++)
			{
				if (!Channel[i].sfxinfo)
				{
					freechan = i;
				}
				else if (pt == Channel[i].pt)
				{
					chansused[Channel[i].entchannel] = 1;
				}
			}
			for (i = 7; i >= 0; i--)
			{
				if (!chansused[i])
					break;
			}
			if (i < 0)
				return;		// entity has no free channels
			channel = i;
			i = freechan;
		}
	}
	else
	{
		i = numChannels;
	}

	if (i >= numChannels)
	{
		for (i = 0; i < numChannels; i++)
		{
			if (Channel[i].sfxinfo == NULL)
			{
				break;
			}
		}
		if (i >= numChannels)
		{
			// look for a lower priority sound to replace.
			sndcount++;
			if (sndcount >= numChannels)
				sndcount = 0;
			for (chan = 0; chan < numChannels; chan++)
			{
				i = (sndcount + chan) % numChannels;
				if (priority > Channel[i].priority)
				{
					chan = -1;	//denote that sound should be replaced.
					break;
				}
			}
			if (chan != -1)
			{
				return;	// no free channels.
			}
			else
			{
				// replace the lower priority sound.
				S_StopChannel (i);
			}
		}
	}

	vol = (int)(SoundCurve[dist]*volume*2);
	if (sep == -3)
	{
		AActor *listener = players[consoleplayer].camera;
		if (listener == NULL)
		{
			sep = NONE_SEP;
		}
		else if (dist == 0)
		{
			sep = NORM_SEP;
		}
		else
		{
			angle = R_PointToAngle2 (listener->x, listener->y, x, y);
			if (angle > listener->angle)
				angle = angle - listener->angle;
			else
				angle = angle + (ANGLE_MAX - listener->angle);
			angle >>= ANGLETOFINESHIFT;
			sep = NORM_SEP - (FixedMul (S_STEREO_SWING, finesine[angle])>>FRACBITS);
			if (snd_flipstereo)
			{
				sep = 255-sep;
			}
			if (snd_dolby)
			{
				int rearsep = NORM_SEP -
					(FixedMul (S_STEREO_SWING, finecosine[angle])>>FRACBITS);
				if (rearsep > 128)
				{
					sep = -1;
					attenuation = -1;
				}
			}
		}
	}

	// hacks to vary the sfx pitches
	if (sound_id == sfx_sawup)
		Channel[i].pitch = NORM_PITCH + 8 - (M_Random()&15);
	else if (sound_id != sfx_itemup && sound_id != sfx_tink)
		Channel[i].pitch = NORM_PITCH + 16 - (M_Random()&31);
	else
		Channel[i].pitch = NORM_PITCH;

	if (Sound3D && attenuation > 0)
	{
		float pos[3];
		float vel[3];

		if (pt)
		{
			CalcPosVel (pt, mover, chanflags & CHAN_LISTENERZ,  pos, vel);
		}
		else
		{
			fixed_t pt2[3];
			pt2[0] = x;
			pt2[1] = y;
			pt2[2] = z;
			CalcPosVel (pt2, mover, chanflags & CHAN_LISTENERZ,  pos, vel);
		}
		Channel[i].handle = I_StartSound3D (sfx, volume, Channel[i].pitch, i, looping, pos, vel);
		Channel[i].is3d = true;
		Channel[i].constz = !!(chanflags & CHAN_LISTENERZ);
	}
	else
	{
		Channel[i].handle = I_StartSound (sfx, vol, sep, Channel[i].pitch, i, looping);
		Channel[i].is3d = false;
		Channel[i].constz = true;
	}
	Channel[i].sound_id = sound_id;
	Channel[i].org_id = org_id;
	Channel[i].mover = mover;
	Channel[i].pt = pt ? pt : &Channel[i].x;
	Channel[i].sfxinfo = sfx;
	Channel[i].priority = priority;
	Channel[i].basepriority = basepriority;
	Channel[i].entchannel = channel;
	Channel[i].attenuation = attenuation;
	Channel[i].volume = volume;
	Channel[i].x = x;
	Channel[i].y = y;
	Channel[i].z = z;
	Channel[i].tag = tag;
	Channel[i].loop = looping ? true : false;

	if (sfx->usefulness < 0)
		sfx->usefulness = 1;
	else
		sfx->usefulness++;
}

//==========================================================================
//
// S_SoundID
//
//==========================================================================

void S_SoundID (int channel, int sound_id, float volume, int attenuation)
{
	S_StartSound ((fixed_t *)NULL, NULL, channel, sound_id, volume, SELECT_ATTEN(attenuation), false);
}

void S_SoundID (AActor *ent, int channel, int sound_id, float volume, int attenuation)
{
	if (ent->Sector->MoreFlags & SECF_SILENT)
		return;
	S_StartSound (&ent->x, ent, channel, sound_id, volume, SELECT_ATTEN(attenuation), false);
}

void S_SoundID (fixed_t *pt, int channel, int sound_id, float volume, int attenuation, int tag)
{
	S_StartSound (pt, NULL, channel, sound_id, volume, SELECT_ATTEN(attenuation), false, tag);
}

//==========================================================================
//
// S_LoopedSoundID
//
//==========================================================================

void S_LoopedSoundID (AActor *ent, int channel, int sound_id, float volume, int attenuation)
{
	if (ent->Sector->MoreFlags & SECF_SILENT)
		return;
	S_StartSound (&ent->x, ent, channel, sound_id, volume, SELECT_ATTEN(attenuation), true);
}

void S_LoopedSoundID (fixed_t *pt, int channel, int sound_id, float volume, int attenuation, int tag)
{
	S_StartSound (pt, NULL, channel, sound_id, volume, SELECT_ATTEN(attenuation), true, tag);
}

//==========================================================================
//
// S_StartNamedSound
//
//==========================================================================

void S_StartNamedSound (AActor *ent, fixed_t *pt, int channel, 
	const char *name, float volume, float attenuation, BOOL looping, int tag=0)
{
	int sfx_id;
	
	if (name == NULL ||
		(ent != NULL && ent->Sector->MoreFlags & SECF_SILENT))
	{
		return;
	}

	sfx_id = S_FindSound (name);
	if (sfx_id == 0)
		DPrintf ("Unknown sound %s\n", name);

	if (ent)
		S_StartSound (&ent->x, ent, channel, sfx_id, volume, attenuation, looping, tag);
	else
		S_StartSound (pt, NULL, channel, sfx_id, volume, attenuation, looping, tag);
}

//==========================================================================
//
// S_Sound
//
//==========================================================================

void S_Sound (int channel, const char *name, float volume, int attenuation)
{
	S_StartNamedSound ((AActor *)NULL, NULL, channel, name, volume, SELECT_ATTEN(attenuation), false);
}

void S_Sound (AActor *ent, int channel, const char *name, float volume, int attenuation)
{
	S_StartNamedSound (ent, NULL, channel, name, volume, SELECT_ATTEN(attenuation), false);
}

void S_Sound (fixed_t *pt, int channel, const char *name, float volume, int attenuation, int tag)
{
	S_StartNamedSound (NULL, pt, channel, name, volume, SELECT_ATTEN(attenuation), false, tag);
}

void S_Sound (fixed_t x, fixed_t y, int channel, const char *name, float volume, int attenuation)
{
	fixed_t pt[3];
	pt[0] = x;
	pt[1] = y;
	S_StartNamedSound (NULL, pt, channel|CHAN_LISTENERZ|CHAN_IMMOBILE,
		name, volume, SELECT_ATTEN(attenuation), false);
}

//==========================================================================
//
// S_LoopedSound
//
//==========================================================================

void S_LoopedSound (AActor *ent, int channel, const char *name, float volume, int attenuation)
{
	S_StartNamedSound (ent, NULL, channel, name, volume, SELECT_ATTEN(attenuation), true);
}

//==========================================================================
//
// S_StopSoundID
//
// from Hexen (albeit, modified a bit)
// Stops more than <limit> copies of the sound from playing at once.
//==========================================================================

BOOL S_StopSoundID (int sound_id, int priority, int tag)
{
	int i;
	int lp; //least priority
	int limit;
	int found;

	if (tag == 0)
	{
		return true;
	}

	limit = 2;
	lp = -1;
	found = 0;
	for (i = 0; i < numChannels; i++)
	{
		if (Channel[i].sound_id == sound_id &&
			Channel[i].sfxinfo &&
			Channel[i].tag == tag)
		{
			found++; //found one.  Now, should we replace it??
			if (priority > Channel[i].priority)
			{ // if we're gonna kill one, then this'll be it
				lp = i;
				priority = Channel[i].priority;
			}
		}
	}
	if (found < limit)
	{
		return true;
	}
	else if (lp == -1)
	{
		return false; // don't replace any sounds
	}
	if (Channel[lp].handle)
	{
		S_StopChannel (lp);
	}
	return true;
}

//==========================================================================
//
// S_StopSound
//
// Stops a sound from a single source playing on a specific channel.
//==========================================================================

void S_StopSound (fixed_t *pt, int channel)
{
	int i;

	for (i = 0; i < numChannels; ++i)
	{
		if (Channel[i].sfxinfo &&
			Channel[i].pt == pt &&
			(sv_ihatesounds || Channel[i].entchannel == channel))
		{
			S_StopChannel (i);
		}
	}
}

void S_StopSound (AActor *ent, int channel)
{
	S_StopSound (&ent->x, channel);
}

//==========================================================================
//
// S_StopAllChannels
//
//==========================================================================

void S_StopAllChannels ()
{
	int i;

	for (i = 0; i < numChannels; i++)
		if (Channel[i].sfxinfo)
			S_StopChannel (i);
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
	int i;

	if (!from)
		return;

	fixed_t *frompt = &from->x;
	fixed_t *topt = to ? &to->x : NULL;

	for (i = 0; i < numChannels; i++)
	{
		if (Channel[i].pt == frompt)
		{
			if (to != NULL || !Channel[i].loop)
			{
				Channel[i].pt = topt ? topt : &Channel[i].x;
				Channel[i].x = frompt[0];
				Channel[i].y = frompt[1];
				Channel[i].z = frompt[2];
			}
			else
			{
				S_StopChannel (i);
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
	int i;

	if (sound_id != 0)
	{
		for (i = 0; i < numChannels; i++)
		{
			if (Channel[i].pt == pt && Channel[i].org_id == sound_id)
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

bool S_IsActorPlayingSomething (AActor *actor, int channel)
{
	int i;

	if (sv_ihatesounds)
	{
		channel = 0;
	}

	for (i = 0; i < numChannels; ++i)
	{
		if (Channel[i].pt == &actor->x)
		{
			if (channel == 0 || Channel[i].entchannel == channel)
			{
				return true;
			}
		}
	}
	return false;
}

//==========================================================================
//
// S_PauseSound
//
// Stop music, during game PAUSE.
//==========================================================================

void S_PauseSound ()
{
	if (mus_playing.handle && !mus_paused)
	{
		I_PauseSong (mus_playing.handle);
		mus_paused = true;
	}
}

//==========================================================================
//
// S_ResumeSound
//
// Resume music, after game PAUSE.
//==========================================================================

void S_ResumeSound ()
{
	if (mus_playing.handle && mus_paused)
	{
		I_ResumeSong (mus_playing.handle);
		mus_paused = false;
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
	fixed_t *listener;
	fixed_t x, y;
	int i, dist, vol;
	angle_t angle;
	int sep;

	listener = listener_p ? &((AActor *)listener_p)->x : NULL;

	// [RH] Update playlist
	if (PlayList &&
		mus_playing.handle &&
		!I_QrySongPlaying (mus_playing.handle))
	{
		PlayList->Advance ();
		S_ActivatePlayList (false);
	}

	for (i = 0; i < numChannels; ++i)
	{
		if (!Channel[i].sfxinfo)
			continue;

		if (!I_SoundIsPlaying (Channel[i].handle))
		{
			S_StopChannel (i);
		}
		if (Channel[i].sound_id == -1 ||
			(Channel[i].pt == listener && !Sound3D) ||
			Channel[i].attenuation <= 0)
		{
			continue;
		}
		else
		{
			if (!Channel[i].pt)
			{
				x = Channel[i].x;
				y = Channel[i].y;
			}
			else
			{
				x = Channel[i].pt[0];
				y = Channel[i].pt[1];
			}
			dist = (int)(FIXED2FLOAT(P_AproxDistance2 (listener, x, y))
					* Channel[i].attenuation);
			if (dist >= MAX_SND_DIST)
			{
				if (!(level.flags & LEVEL_NOSOUNDCLIPPING))
				{
					S_StopChannel (i);
					continue;
				}
				else
					dist = MAX_SND_DIST;
			}
			else if (dist < 0)
			{
				dist = 0;
			}
			vol = (int)(SoundCurve[dist]*Channel[i].volume*2);
			if (dist > 0)
			{
				angle = R_PointToAngle2(listener[0], listener[1], x, y);
				if (angle > players[consoleplayer].camera->angle)
					angle = angle - players[consoleplayer].camera->angle;
				else
					angle = angle + (ANGLE_MAX - players[consoleplayer].camera->angle);
				angle >>= ANGLETOFINESHIFT;
				sep = NORM_SEP - (FixedMul (S_STEREO_SWING, finesine[angle])>>FRACBITS);
				if (snd_flipstereo)
				{
					sep = 255-sep;
				}
				if (snd_dolby)
				{
					int rearsep = NORM_SEP -
						(FixedMul (S_STEREO_SWING, finecosine[angle])>>FRACBITS);
					if (rearsep > 128)
					{
						sep = -1;
					}
				}
			}
			else
			{
				sep = NORM_SEP;
			}
			if (!Channel[i].is3d)
			{
				I_UpdateSoundParams (Channel[i].handle, vol, sep, Channel[i].pitch);
			}
			else
			{
				float pos[3], vel[3];
				CalcPosVel (Channel[i].pt, Channel[i].mover, Channel[i].constz, pos, vel);
				I_UpdateSoundParams3D (Channel[i].handle, pos, vel);
			}
			Channel[i].priority = Channel[i].basepriority * (PRIORITY_MAX_ADJUST-(dist/DIST_ADJUST));
		}
	}

	SN_UpdateActiveSequences ();

	if (Sound3D && players[consoleplayer].camera)
	{
		I_UpdateListener (players[consoleplayer].camera);
	}
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

bool S_StartMusic (char *m_id)
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

bool S_ChangeMusic (const char *musicname, int order, bool looping, bool force)
{
	int lumpnum = -1;
	int len;
	int handle;
	int pos;

	if (!force && PlayList)
	{ // Don't change if a playlist is active
		return false;
	}

	if (musicname == NULL)
	{
		// Don't choke if the map doesn't have a song attached
		S_StopMusic (true);
		return false;
	}

	if (mus_playing.name && stricmp (mus_playing.name, musicname) == 0)
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
		char *more = strchr (musicname+4, ',');
		unsigned int id = 0;

		if (more != NULL)
		{
			id = strtoul (more+1, NULL, 16);
		}
		S_StopMusic (true);
		if (mus_playing.name)
		{
			delete[] mus_playing.name;
		}
		mus_playing.handle = I_RegisterCDSong (track, id);
	}
	else
	{
		if (-1 == (handle = open (musicname, O_BINARY|O_RDONLY)))
		{
			if ((lumpnum = W_CheckNumForName (musicname)) == -1)
			{
				Printf ("Music \"%s\" not found\n", musicname);
				return false;
			}
			else
			{
				handle = W_FileHandleFromWad (lumpinfo[lumpnum].wadnum);
				pos = lumpinfo[lumpnum].position;
				len = lumpinfo[lumpnum].size;
			}
		}
		else
		{
#ifdef _WIN32
			lseek (handle, 0, SEEK_END);
			len = tell (handle);
#else
			len = lseek (handle, 0, SEEK_END);
#endif
			lseek (handle, 0, SEEK_SET);
			pos = 0;
		}

		// shutdown old music
		S_StopMusic (true);

		// load & register it
		if (mus_playing.name)
		{
			delete[] mus_playing.name;
		}
		mus_playing.handle = I_RegisterSong (handle, pos, len);
	}

	mus_playing.loop = looping;

	if (lumpnum == -1)
	{
		close (handle);
	}

	if (mus_playing.handle != 0)
	{ // play it
		mus_playing.name = copystring (musicname);
		I_PlaySong (mus_playing.handle, looping);
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
	if (LastSong != NULL)
	{
		S_ChangeMusic (LastSong, mus_playing.baseorder, mus_playing.loop, true);
		delete[] LastSong;
		LastSong = NULL;
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
	if ((force || PlayList == NULL) && mus_playing.name)
	{
		if (mus_paused)
			I_ResumeSong(mus_playing.handle);

		I_StopSong(mus_playing.handle);
		I_UnRegisterSong(mus_playing.handle);

		if (LastSong)
		{
			delete[] LastSong;
		}

		LastSong = mus_playing.name;
		mus_playing.name = NULL;
		mus_playing.handle = 0;
	}
}

//==========================================================================
//
// S_StopChannel
//
//==========================================================================

static void S_StopChannel (int cnum)
{

	int 		i;
	channel_t*	c = &Channel[cnum];

	if (c->sfxinfo)
	{
		// stop the sound playing
		I_StopSound (c->handle);

		// check to see
		//	if other channels are playing the sound
		for (i = 0; i < numChannels; i++)
		{
			if (cnum != i && c->sfxinfo == Channel[i].sfxinfo)
			{
				break;
			}
		}
		
		// degrade usefulness of sound data
		c->sfxinfo->usefulness--;

		c->sfxinfo = NULL;
		c->handle = 0;
		c->sound_id = -1;
		c->pt = NULL;
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
				Printf ("%s\n", GStrings(STSTR_NOMUS));
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
				Printf ("%s\n", GStrings(STSTR_MUS));
			}
		}
		else
		{
			Printf ("%s\n", GStrings(STSTR_NOMUS));
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
