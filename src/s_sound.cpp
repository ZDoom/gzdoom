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
#define NORM_SEP				0
#define NONE_SEP				-2

#define S_PITCH_PERTURB 		1
#define S_STEREO_SWING			0.25f

/* Sound curve parameters for Doom */

// Maximum sound distance
const int S_CLIPPING_DIST = 1200;

// Sounds closer than this to the listener are maxed out.
// Originally: 200.
const int S_CLOSE_DIST =	160;

const float S_ATTENUATOR =	S_CLIPPING_DIST - S_CLOSE_DIST;

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
	bool		loop;
	bool		is3d;
	bool		constz;
	int			time;		// time when sound started playing
} channel_t;

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

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static fixed_t P_AproxDistance2 (fixed_t *listener, fixed_t x, fixed_t y);
static void S_StopChannel (int cnum);
static void S_StartSound (fixed_t *pt, AActor *mover, int channel,
	int sound_id, float volume, float attenuation, bool looping);
static void S_ActivatePlayList (bool goBack);
static void CalcPosVel (fixed_t *pt, AActor *mover, int constz, float pos[3],
	float vel[3]);

// PRIVATE DATA DEFINITIONS ------------------------------------------------

int MAX_SND_DIST;
static channel_t *Channel;			// the set of channels available
static bool		SoundPaused;		// whether sound effects are paused
static bool		MusicPaused;		// whether music is paused
static MusPlayingInfo mus_playing;	// music currently being played
static FString	 LastSong;			// last music that was played
static float	*SoundCurve;
static int		nextcleanup;
static FPlayList *PlayList;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int sfx_empty;

int numChannels;

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
	fixed_t ox, oy;
	int i, y, color;

	y = 32 * CleanYfac;
	screen->DrawText (CR_YELLOW, 0, y, "*** SOUND DEBUG INFO ***", TAG_DONE);
	y += 8;

	screen->DrawText (CR_GOLD, 0, y, "name", TAG_DONE);
	screen->DrawText (CR_GOLD, 70, y, "x", TAG_DONE);
	screen->DrawText (CR_GOLD, 120, y, "y", TAG_DONE);
	screen->DrawText (CR_GOLD, 170, y, "vol", TAG_DONE);
	screen->DrawText (CR_GOLD, 200, y, "pri", TAG_DONE);
	screen->DrawText (CR_GOLD, 240, y, "dist", TAG_DONE);
	screen->DrawText (CR_GOLD, 280, y, "chan", TAG_DONE);
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
			Wads.GetLumpName (temp, Channel[i].sfxinfo->lumpnum);
			temp[8] = 0;
			screen->DrawText (color, 0, y, temp, TAG_DONE);
			sprintf (temp, "%d", ox >> FRACBITS);
			screen->DrawText (color, 70, y, temp, TAG_DONE);
			sprintf (temp, "%d", oy >> FRACBITS);
			screen->DrawText (color, 120, y, temp, TAG_DONE);
			sprintf (temp, "%g", Channel[i].volume);
			screen->DrawText (color, 170, y, temp, TAG_DONE);
			sprintf (temp, "%d", Channel[i].priority);
			screen->DrawText (color, 200, y, temp, TAG_DONE);
			sprintf (temp, "%d", P_AproxDistance2 (players[consoleplayer].camera, ox, oy) / FRACUNIT);
			screen->DrawText (color, 240, y, temp, TAG_DONE);
			sprintf (temp, "%d", Channel[i].entchannel);
			screen->DrawText (color, 280, y, temp, TAG_DONE);
		}
		else
		{
			screen->DrawText (CR_GREY, 0, y, "------", TAG_DONE);
		}
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
	int i;
	int curvelump;

	atterm (S_Shutdown);

	// remove old data (S_Init can be called multiple times!)
	if (SoundCurve)
	{
		delete[] SoundCurve;
	}

	// Heretic and Hexen have sound curve lookup tables. Doom does not.
	curvelump = Wads.CheckNumForName ("SNDCURVE");
	if (curvelump >= 0)
	{
		MAX_SND_DIST = Wads.LumpLength (curvelump);
		SoundCurve = new float[MAX_SND_DIST];
		FMemLump lump = Wads.ReadLump(curvelump);
		BYTE *lumpmem = (BYTE *)lump.GetMem();

		// The maximum value in a SNDCURVE lump is 127, so scale it to
		// fit our sound system's volume levels. Also, FMOD's volumes
		// are linear, whereas we want the "dumb" logarithmic volumes
		// for our "2D" sounds.
		for (i = 0; i < S_CLIPPING_DIST; ++i)
		{
			SoundCurve[i] = (powf(10.f, (lumpmem[i] / 127.f)) - 1.f) / 9.f;
		}
	}
	else
	{
		MAX_SND_DIST = S_CLIPPING_DIST;
		SoundCurve = new float[S_CLIPPING_DIST];
		for (i = 0; i < S_CLIPPING_DIST; ++i)
		{
			SoundCurve[i] = (powf(10.f, MIN(1.f, (S_CLIPPING_DIST - i) / S_ATTENUATOR)) - 1.f) / 9.f;
		}
	}

	// Allocating the virtual channels
	numChannels = GSnd ? GSnd->GetNumChannels() : 0;
	if (Channel != NULL)
	{
		delete[] Channel;
	}
	Channel = new channel_t[numChannels];

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
	MusicPaused = false;
	SoundPaused = false;

	// Note that sounds have not been cached (yet).
//	for (i=1; (size_t)i < S_sfx.Size (); i++)
//		S_sfx[i].usefulness = -1;
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
	S_ParseSndEax ();
}

//==========================================================================
//
// S_Shutdown
//
//==========================================================================

void S_Shutdown ()
{
	if (Channel != NULL)
	{
		delete[] Channel;
		Channel = NULL;
		numChannels = 0;
	}
	if (SoundCurve != NULL)
	{
		delete[] SoundCurve;
		SoundCurve = NULL;
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
	int cnum;

	if (GSnd)
	{
		// kill all playing sounds at start of level (trust me - a good idea)
		for (cnum = 0; cnum < numChannels; cnum++)
		{
			if (Channel[cnum].sfxinfo)
			{
				S_StopChannel (cnum);
			}
		}
		
		// Check for local sound definitions. Only reload if they differ
		// from the previous ones.
		
		// To be certain better check whether level is valid!
		char * LocalSndInfo= level.info? level.info->soundinfo : (char*)"";
		char * LocalSndSeq = level.info? level.info->sndseq : (char*)"";
		bool parse_ss=false;

		// This level uses a different local SNDINFO
		if (LastLocalSndInfo.CompareNoCase(LocalSndInfo) != 0 || !level.info)
		{
			// First delete the old sound list
			for(unsigned i=1;i<S_sfx.Size();i++) 
			{
				GSnd->UnloadSound(&S_sfx[i]);
			}
			
			// Parse the global SNDINFO
			S_ParseSndInfo();
		
			if (*LocalSndInfo)
			{
				// Now parse the local SNDINFO
				int j = Wads.CheckNumForName(LocalSndInfo);
				if (j>=0) S_AddLocalSndInfo(j);
			}

			// Also reload the SNDSEQ if the SNDINFO was replaced!
			parse_ss=true;
		}
		else if (LastLocalSndSeq.CompareNoCase(LocalSndSeq) != 0)
		{
			parse_ss=true;
		}
		if (parse_ss)
		{
			S_ParseSndSeq(*LocalSndSeq? Wads.CheckNumForName(LocalSndSeq) : -1);
		}
		else
		
		LastLocalSndInfo = LocalSndInfo;
		LastLocalSndSeq = LocalSndSeq;
	}

	SoundPaused = false;

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
	
	nextcleanup = 15;
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
	int sound_id, float volume, float attenuation, bool looping)
{
	sfxinfo_t *sfx;
	int dist;
	float vol;
	int i;
	int chanflags;
	int basepriority, priority;
	float sep;
	int org_id;
	fixed_t x, y, z;
	angle_t angle;

	static int sndcount = 0;
	int chan;

	if (sound_id <= 0 || volume <= 0 || GSnd == NULL)
		return;

	org_id = sound_id;

	if (pt == NULL)
	{
		if (attenuation > 0)
			attenuation = 0;
		// Give these variables values, although they don't really matter
		x = y = z = 0;
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
	if (i_compatflags & COMPATF_MAGICSILENCE)
	{ // For people who just can't play without a silent BFG.
		channel = 1;
	}
	else
	{
		if ((channel & CHAN_MAYBE_LOCAL) && (i_compatflags & COMPATF_SILENTPICKUP))
		{
			if (mover != 0 && mover != players[consoleplayer].camera)
			{
				return;
			}
		}
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
		sep = NONE_SEP;
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
			return;	// sound is beyond the hearing range...
		}
		else if (dist < 0)
		{
			dist = 0;
		}
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
		GSnd->LoadSound (sfx);
		if (sfx->link != sfxinfo_t::NO_LINK)
		{
			sfx = &S_sfx[sfx->link];
		}
	}

	if (sfx->lumpnum == sfx_empty)
	{
		basepriority = -1000;
		return;
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

	if (pt != NULL && !S_StopSoundID (sound_id, priority, sfx->MaxChannels, sfx->bSingular, x, y))
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

	vol = SoundCurve[dist] * volume;
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
			sep = NORM_SEP - S_STEREO_SWING * sin((angle >> 1) * M_PI / 2147483648.0);
			if (snd_flipstereo)
			{
				sep = -sep;
			}
		}
	}

	// vary the sfx pitches
	if (sfx->PitchMask != 0)
	{
		Channel[i].pitch = NORM_PITCH-(M_Random()&sfx->PitchMask)+(M_Random()&sfx->PitchMask);
	}
	else
	{
		Channel[i].pitch = NORM_PITCH;
	}

	if (GSnd->Sound3D && attenuation > 0)
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
		Channel[i].handle = GSnd->StartSound3D (sfx, volume, Channel[i].pitch, i, looping, pos, vel, !(chanflags & CHAN_NOPAUSE));
		Channel[i].is3d = true;
		Channel[i].constz = !!(chanflags & CHAN_LISTENERZ);
	}
	else
	{
		Channel[i].handle = GSnd->StartSound (sfx, vol, sep, Channel[i].pitch, i, looping, !(chanflags & CHAN_NOPAUSE));
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
	Channel[i].loop = looping ? true : false;
	Channel[i].time = gametic;

//	if (sfx->usefulness < 0)
//		sfx->usefulness = 1;
//	else
//		sfx->usefulness++;
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

void S_SoundID (fixed_t *pt, int channel, int sound_id, float volume, int attenuation)
{
	S_StartSound (pt, NULL, channel, sound_id, volume, SELECT_ATTEN(attenuation), false);
}

void S_SoundID (fixed_t x, fixed_t y, fixed_t z, int channel, int sound_id, float volume, int attenuation)
{
	fixed_t pt[3];
	pt[0] = x;
	pt[1] = y;
	pt[2] = z;
	S_StartSound (pt, NULL, channel|CHAN_IMMOBILE, sound_id, volume, SELECT_ATTEN(attenuation), false);
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

void S_LoopedSoundID (fixed_t *pt, int channel, int sound_id, float volume, int attenuation)
{
	S_StartSound (pt, NULL, channel, sound_id, volume, SELECT_ATTEN(attenuation), true);
}

//==========================================================================
//
// S_StartNamedSound
//
//==========================================================================

void S_StartNamedSound (AActor *ent, fixed_t *pt, int channel, 
	const char *name, float volume, float attenuation, bool looping)
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
		S_StartSound (&ent->x, ent, channel, sfx_id, volume, attenuation, looping);
	else
		S_StartSound (pt, NULL, channel, sfx_id, volume, attenuation, looping);
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

void S_Sound (fixed_t *pt, int channel, const char *name, float volume, int attenuation)
{
	S_StartNamedSound (NULL, pt, channel, name, volume, SELECT_ATTEN(attenuation), false);
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

bool S_StopSoundID (int sound_id, int priority, int limit, bool singular, fixed_t x, fixed_t y)
{
	int i;
	int lp; //least priority
	int found;

	if (limit <= 0)
	{
		return true;
	}

	lp = -1;
	found = 0;
	for (i = 0; i < numChannels; i++)
	{
		if (Channel[i].sound_id == sound_id)
		{
			if (singular)
			{
				// This sound is already playing, so don't start it again.
				return false;
			}
			if (Channel[i].sfxinfo &&
				P_AproxDistance (Channel[i].pt[0] - x, Channel[i].pt[1] - y) < 256*FRACUNIT)
			{
				found++; //found one.  Now, should we replace it??
				if (priority > Channel[i].priority)
				{ // if we're gonna kill one, then this'll be it
					lp = i;
					priority = Channel[i].priority;
				}
			}
		}
	}
	if (found < limit)
	{
		return true;
	}
	if (lp == -1)
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
			((pt == NULL && Channel[i].pt == &Channel[i].x) || Channel[i].pt == pt) &&
			((i_compatflags & COMPATF_MAGICSILENCE) || Channel[i].entchannel == channel))
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
	SN_StopAllSequences ();
	for (int i = 0; i < numChannels; i++)
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

	if (sound_id > 0)
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

bool S_IsActorPlayingSomething (AActor *actor, int channel, int sound_id)
{
	int i;

	if (i_compatflags & COMPATF_MAGICSILENCE)
	{
		channel = 0;
	}

	// Resolve player sounds, random sounds, and aliases
	if (sound_id > 0)
	{
		while (S_sfx[sound_id].link != sfxinfo_t::NO_LINK)
		{
			if (S_sfx[sound_id].bPlayerReserve)
			{
				sound_id = S_FindSkinnedSound (actor, sound_id);
			}
			else if (S_sfx[sound_id].bRandomHeader)
			{
				// This can't really be checked properly
				// so return true if the channel is playing something, no matter what.
				sound_id = -1;
				break;
			}
			else
			{
				sound_id = S_sfx[sound_id].link;
			}
		}
	}

	for (i = 0; i < numChannels; ++i)
	{
		if (Channel[i].pt == &actor->x)
		{
			if (channel == 0 || Channel[i].entchannel == channel)
			{
				return sound_id < 0 || Channel[i].sound_id == sound_id;
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
	if (GSnd != NULL && !SoundPaused)
	{
		GSnd->SetSfxPaused (true);
		SoundPaused = true;
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
	if (GSnd != NULL && SoundPaused)
	{
		GSnd->SetSfxPaused (false);
		SoundPaused = false;
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
	int i, dist;
	angle_t angle;
	float vol, sep;

	I_UpdateMusic();

	if (GSnd == NULL)
		return;

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

		if (!GSnd->IsPlayingSound (Channel[i].handle))
		{
			S_StopChannel (i);
		}
		if (Channel[i].sound_id == -1 ||
			(Channel[i].pt == listener && !GSnd->Sound3D) ||
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
				S_StopChannel (i);
				continue;
			}
			else if (dist < 0)
			{
				dist = 0;
			}
			vol = SoundCurve[dist] * Channel[i].volume;
			if (dist > 0)
			{
				angle = R_PointToAngle2(listener[0], listener[1], x, y);
				if (angle > players[consoleplayer].camera->angle)
					angle = angle - players[consoleplayer].camera->angle;
				else
					angle = angle + (ANGLE_MAX - players[consoleplayer].camera->angle);
				sep = NORM_SEP - S_STEREO_SWING * sin((angle >> 1) * M_PI / 2147483648.0);
				if (snd_flipstereo)
				{
					sep = -sep;
				}
			}
			else
			{
				sep = NORM_SEP;
			}
			if (!Channel[i].is3d)
			{
				GSnd->UpdateSoundParams (Channel[i].handle, vol, sep, Channel[i].pitch);
			}
			else
			{
				float pos[3], vel[3];
				CalcPosVel (Channel[i].pt, Channel[i].mover, Channel[i].constz, pos, vel);
				GSnd->UpdateSoundParams3D (Channel[i].handle, pos, vel);
			}
			Channel[i].priority = Channel[i].basepriority * (PRIORITY_MAX_ADJUST-(dist/DIST_ADJUST));
		}
	}

	SN_UpdateActiveSequences ();

	if (GSnd->Sound3D && players[consoleplayer].camera)
	{
		GSnd->UpdateListener (players[consoleplayer].camera);
	}

	GSnd->UpdateSounds ();
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
		int offset, length;
		int device = -1;
	

		int * devp = MidiDevices.CheckKey(FName(musicname));
		if (devp != NULL) device = *devp;

		if (!FileExists (musicname))
		{
			if ((lumpnum = Wads.CheckNumForName (musicname, ns_music)) == -1)
			{
				if ((lumpnum = Wads.CheckNumForFullName (musicname)) == -1)
				{
					Printf ("Music \"%s\" not found\n", musicname);
					return false;
				}
			}
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
					offset = 0;
					return false;
				}
				musiccache.Resize(length);
				Wads.ReadLump(lumpnum, &musiccache[0]);
			}
			else
			{
				offset = Wads.GetLumpOffset (lumpnum);
				length = Wads.LumpLength (lumpnum);
			}
		}
		else
		{
			offset = 0;
			length = 0;
		}

		// shutdown old music
		S_StopMusic (true);

		// load & register it
		if (offset!=-1)
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

	if (mus_playing.handle != 0)
	{ // play it
		mus_playing.name = musicname;
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
		S_ChangeMusic (LastSong, mus_playing.baseorder, mus_playing.loop, true);
		LastSong = "";
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
// S_StopChannel
//
//==========================================================================

static void S_StopChannel (int cnum)
{

//	int 		i;
	channel_t*	c = &Channel[cnum];

	if (c->sfxinfo)
	{
		// stop the sound playing
		if (GSnd != NULL) GSnd->StopSound (c->handle);

		// check to see
		//	if other channels are playing the sound
/*		for (i = 0; i < numChannels; i++)
		{
			if (cnum != i && c->sfxinfo == Channel[i].sfxinfo)
			{
				break;
			}
		}
*/		
		// degrade usefulness of sound data
//		c->sfxinfo->usefulness--;

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
		int sfxnum = S_FindSound (argv[i]);
		if (sfxnum > 0)
		{
			S_CacheSound (&S_sfx[sfxnum]);
		}
	}
}
