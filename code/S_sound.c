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
#include "m_alloc.h"

#include "i_system.h"
#include "i_sound.h"
#include "i_music.h"
#include "s_sound.h"
#include "s_sndseq.h"
#include "c_dispch.h"
#include "z_zone.h"
#include "m_random.h"
#include "w_wad.h"
#include "doomdef.h"
#include "p_local.h"
#include "doomstat.h"
#include "cmdlib.h"
#include "v_video.h"
#include "v_text.h"


#define MAX_SND_DIST			2025
#define PRIORITY_MAX_ADJUST		10
#define DIST_ADJUST				(MAX_SND_DIST/PRIORITY_MAX_ADJUST)

#define NORM_PITCH				128
#define NORM_PRIORITY			64
#define NORM_SEP				128

#define S_PITCH_PERTURB 		1
#define S_STEREO_SWING			(96<<FRACBITS)


typedef struct
{
	mobj_t		*mo;		// origin of sound
	fixed_t		x,y;		// Origin if mo is NULL
	sfxinfo_t*	sfxinfo;	// sound information (if null, channel avail.)
	int 		handle;		// handle of the sound being played
	int			sound_id;
	int			entchannel;	// entity's sound channel
	int			basepriority;
	int			attenuation;
	float		volume;
	int			pitch;
	int			priority;
	BOOL		loop;
} channel_t;

// [RH] Hacks for pitch variance
int sfx_sawup, sfx_sawidl, sfx_sawful, sfx_sawhit;
int sfx_itemup, sfx_tink;

int sfx_plasma, sfx_chngun, sfx_chainguy, sfx_empty;

// [RH] Use surround sounds?
cvar_t *snd_surround;

// [RH] Print sound debugging info?
cvar_t *noisedebug;

// the set of channels available
static channel_t *Channel;

// Maximum volume of a sound effect.
// Internal default is max out of 0-15.
cvar_t 			*snd_SfxVolume;

// Maximum volume of MOD music.
cvar_t 			*snd_MusicVolume; 

// Maximum volume of MIDI/MUS music.
cvar_t			*snd_MidiVolume;

// whether songs are mus_paused
static BOOL		mus_paused; 	

// music currently being played
static struct {
	char *name;
	int   handle;
} mus_playing;

// number of channels available
cvar_t 			*snd_channels;
int				numChannels;

static byte		*SoundCurve;

static int		nextcleanup;


static fixed_t P_AproxDistance2 (mobj_t *listener, fixed_t x, fixed_t y)
{
	// calculate the distance to sound origin
	//	and clip it if necessary
	fixed_t adx = abs (listener->x - x);
	fixed_t ady = abs (listener->y - y);
	// From _GG1_ p.428. Appox. eucledian distance fast.
	return adx + ady - ((adx < ady ? adx : ady)>>1);
}

//
// [RH] Print sound debug info. Called from D_Display()
//
void S_NoiseDebug (void)
{
	fixed_t ox, oy;
	int i, y, color;

	y = 32 * CleanYfac;
	if (gametic & 16)
		V_DrawText (CR_TAN, 0, y, "*** SOUND DEBUG INFO ***");
	y += 8;

	V_DrawText (CR_GREY, 0, y, "name");
	V_DrawText (CR_GREY, 70, y, "mo.x");
	V_DrawText (CR_GREY, 120, y, "mo.y");
	V_DrawText (CR_GREY, 170, y, "id");
	V_DrawText (CR_GREY, 200, y, "pri");
	V_DrawText (CR_GREY, 240, y, "dist");
	V_DrawText (CR_GREY, 280, y, "chan");
	y += 8;

	for (i = 0; i < numChannels && y < screen.height - 16; i++, y += 8) {
		if (Channel[i].sfxinfo) {
			char temp[16];
			mobj_t *origin = Channel[i].mo;

			if (Channel[i].attenuation == ATTN_NONE ||
				Channel[i].attenuation == ATTN_SURROUND) {
				ox = players[consoleplayer].camera->x;
				oy = players[consoleplayer].camera->y;
			} else if (origin) {
				ox = origin->x;
				oy = origin->y;
			} else {
				ox = Channel[i].x;
				oy = Channel[i].y;
			}
			color = Channel[i].loop ? CR_BROWN : CR_GREY;
			strcpy (temp, lumpinfo[Channel[i].sfxinfo->lumpnum].name);
			temp[8] = 0;
			V_DrawText (color, 0, y, temp);
			sprintf (temp, "%d", ox / FRACUNIT);
			V_DrawText (color, 70, y, temp);
			sprintf (temp, "%d", oy / FRACUNIT);
			V_DrawText (color, 120, y, temp);
			sprintf (temp, "%ld", Channel[i].sfxinfo - S_sfx);
			V_DrawText (color, 170, y, temp);
			sprintf (temp, "%d", Channel[i].priority);
			V_DrawText (color, 200, y, temp);
			sprintf (temp, "%d", P_AproxDistance2 (players[consoleplayer].camera, ox, oy) / FRACUNIT);
			V_DrawText (color, 240, y, temp);
			sprintf (temp, "%d", Channel[i].entchannel);
			V_DrawText (color, 280, y, temp);
		} else {
			V_DrawText (CR_GREY, 0, y, "------");
		}
	}
	BorderNeedRefresh = true;
}


//
// Internals.
//
static void S_StopChannel (int cnum);


static void ChangeMusicVol (cvar_t *var)
{
	S_SetMusicVolume ((int)var->value);
}

static void ChangeSfxVol (cvar_t *var)
{
	S_SetSfxVolume ((int)var->value);
}

static void ChangeMidiVol (cvar_t *var)
{
	I_SetMIDIVolume (var->value);
}

//
// Initializes sound stuff, including volume
// Sets channels, SFX and music volume,
// allocates channel buffer, sets S_sfx lookup.
//
void S_Init (int sfxVolume, int musicVolume)
{
	int i;

	SoundCurve = W_CacheLumpName ("SNDCURVE", PU_STATIC);
	snd_MidiVolume->u.callback = ChangeMidiVol;
	snd_MusicVolume->u.callback = ChangeMusicVol;
	snd_SfxVolume->u.callback = ChangeSfxVol;
	ChangeMidiVol (snd_MidiVolume);

	Printf (PRINT_HIGH, "S_Init: default sfx volume %d\n", sfxVolume);

	// [RH] Read in sound sequences
	S_ParseSndSeq ();

	S_SetSfxVolume(sfxVolume);
	S_SetMusicVolume(musicVolume);

	// Allocating the internal channels for mixing
	// (the maximum numer of sounds rendered
	// simultaneously) within zone memory.
	numChannels = (int)snd_channels->value;
	Channel = (channel_t *) Z_Malloc(numChannels*sizeof(channel_t), PU_STATIC, 0);
	for (i = 0; i < numChannels; i++)
	{
		Channel[i].mo = NULL;
		Channel[i].handle = 0;
		Channel[i].sound_id = -1;
		Channel[i].priority = 0;
	}
	I_SetChannels (numChannels);
	
	// Free all channels for use
	for (i=0 ; i<numChannels ; i++)
		Channel[i].sfxinfo = 0;
	
	// no sounds are playing, and they are not mus_paused
	mus_paused = 0;

	// Note that sounds have not been cached (yet).
	for (i=1; i < numsfx; i++)
		S_sfx[i].usefulness = -1;
}



//
// Per level startup code.
// Kills playing sounds at start of level,
// determines music if any, changes music.
//
void S_Start (void)
{
	int cnum;

	// kill all playing sounds at start of level
	//	(trust me - a good idea)
	for (cnum = 0; cnum < numChannels; cnum++)
		if (Channel[cnum].sfxinfo)
			S_StopChannel (cnum);
	
	// start new music for the level
	mus_paused = 0;
	
	// [RH] This is a lot simpler now.
	S_ChangeMusic (level.music, true);
	
	nextcleanup = 15;
}


// [RH] Split S_StartSoundAtVolume into multiple parts so that sounds can
//		be specified both by id and by name. Also borrowed some stuff from
//		Hexen and parameters from Quake.
static void S_StartSound (mobj_t *ent, fixed_t x, fixed_t y, int channel,
						  int sound_id, float volume, int attenuation, BOOL looping)
{
	sfxinfo_t *sfx;
	int dist, vol;
	int i;
	int basepriority, priority;
	int sep;
	angle_t angle;

	static int sndcount = 0;
	int chan;

	if (sound_id == -1 || volume <= 0)
		return;
	if (ent == NULL) {
		if (attenuation != ATTN_SURROUND)
			attenuation = ATTN_NONE;
	} else if (ent == (mobj_t *)(~0)) {
		ent = NULL;
	} else {
		x = ent->x;
		y = ent->y;
	}

	channel &= 7;
	if (volume > 1)
		volume = 1;

	if (attenuation == ATTN_NONE) {
		sep = NORM_SEP;
		dist = 0;
	} else if (attenuation == ATTN_SURROUND) {
		sep = -1;
		dist = 0;
	} else {
		// calculate the distance before other stuff so we can throw out
		// sounds that are beyond the hearing range.
		dist = P_AproxDistance2 (players[consoleplayer].camera, x, y) >> FRACBITS;
		if (attenuation == ATTN_STATIC)
			dist *= 3;
		if (dist >= MAX_SND_DIST) {
			if (level.flags & LEVEL_NOSOUNDCLIPPING)
				dist = MAX_SND_DIST;
			else
				return;	// sound is beyond the hearing range...
		} else if (dist < 0)
			dist = 0;
		sep = -2;	// Calculate separation later on
	}

	sfx = &S_sfx[sound_id];
	if (sfx->link)
		sfx = sfx->link;
	if (!sfx->data) {
		I_LoadSound (sfx);
		if (sfx->link)
			sfx = sfx->link;
	}

	if (sfx->lumpnum == sfx_empty) {
		basepriority = -1000;
	} else if (attenuation == ATTN_NONE || attenuation == ATTN_SURROUND) {
		basepriority = 200;
	} else {
		switch (channel) {
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
		if (attenuation == ATTN_NORM)
			basepriority += 50;
		if (dist == 0)
			basepriority += 30;
	}
	priority = basepriority * (PRIORITY_MAX_ADJUST - (dist/DIST_ADJUST));

	if (!S_StopSoundID (sound_id, priority))
		return;	// other sounds have greater priority

	if (ent) {
		for (i = 0; i < numChannels; i++) {
			if (Channel[i].sfxinfo &&
				ent == Channel[i].mo &&
				Channel[i].entchannel == channel) {
				break;
			}
		}
		if (i < numChannels && channel != CHAN_AUTO) {
			S_StopChannel (i);
		} else if (channel == CHAN_AUTO) {
			int chansused[8], freechan;

			memset (chansused, 0, sizeof(chansused));
			freechan = numChannels;

			for (i = 0; i < numChannels; i++) {
				if (!Channel[i].sfxinfo) {
					freechan = i;
				} else if (ent == Channel[i].mo) {
					chansused[Channel[i].entchannel] = 1;
				}
			}
			for (i = 7; i >= 0; i--) {
				if (!chansused[i])
					break;
			}
			if (i < 0)
				return;		// entity has no free channels
			channel = i;
			i = freechan;
		}
	} else {
		i = numChannels;
	}

	if (i >= numChannels) {
		for (i = 0; i < numChannels; i++) {
			if (Channel[i].sfxinfo == NULL) {
				break;
			}
		}
		if (i >= numChannels) {
			// look for a lower priority sound to replace.
			sndcount++;
			if (sndcount >= numChannels)
				sndcount = 0;
			for (chan = 0; chan < numChannels; chan++) {
				i = (sndcount + chan) % numChannels;
				if (priority >= Channel[i].priority) {
					chan = -1;	//denote that sound should be replaced.
					break;
				}
			}
			if (chan != -1) {
				return;	// no free channels.
			} else {
				// replace the lower priority sound.
				S_StopChannel (i);
			}
		}
	}

	vol = (int)(SoundCurve[dist]*volume*2);
	if (sep == -2) {
		mobj_t *listener = players[consoleplayer].camera;
		if (!listener || dist == 0) {
			sep = NORM_SEP;
		} else {
			angle = R_PointToAngle2 (listener->x, listener->y, x, y);
			if (angle > listener->angle)
				angle = angle - listener->angle;
			else
				angle = angle + (ANGLE_MAX - listener->angle);
			angle >>= ANGLETOFINESHIFT;
			sep = NORM_SEP - (FixedMul (S_STEREO_SWING, finesine[angle])>>FRACBITS);
		}
	}

	// hacks to vary the sfx pitches
	if (sound_id == sfx_sawup)
		Channel[i].pitch = NORM_PITCH + 8 - (M_Random()&15);
	else if (sound_id != sfx_itemup && sound_id != sfx_tink)
		Channel[i].pitch = NORM_PITCH + 16 - (M_Random()&31);
	else
		Channel[i].pitch = NORM_PITCH;

	Channel[i].handle = I_StartSound (sfx, vol, sep, Channel[i].pitch, i, looping);
	Channel[i].sound_id = sound_id;
	Channel[i].mo = ent;
	Channel[i].sfxinfo = sfx;
	Channel[i].priority = priority;
	Channel[i].basepriority = basepriority;
	Channel[i].entchannel = channel;
	Channel[i].attenuation = attenuation;
	Channel[i].volume = volume;
	Channel[i].x = x;
	Channel[i].y = y;
	Channel[i].loop = looping;

	if (sfx->usefulness < 0)
		sfx->usefulness = 1;
	else
		sfx->usefulness++;
}

void S_SoundID (mobj_t *ent, int channel, int sound_id, float volume, int attenuation)
{
	S_StartSound (ent, 0, 0, channel, sound_id, volume, attenuation, false);
}

void S_LoopedSoundID (mobj_t *ent, int channel, int sound_id, float volume, int attenuation)
{
	S_StartSound (ent, 0, 0, channel, sound_id, volume, attenuation, true);
}

static void S_StartNamedSound (mobj_t *ent, fixed_t x, fixed_t y, int channel, 
							   char *name, float volume, int attenuation, BOOL looping)
{
	int sfx_id;
	
	if (*name == '*') {
		// Sexed sound
		char nametemp[128];
		const char templat[] = "player/%s/%s";
		const char *genders[] = { "male", "female", "cyborg" };
		player_t *player;

		sfx_id = -1;
		if ( (player = ent->player) ) {
			sprintf (nametemp, templat, skins[player->userinfo.skin].name, name + 1);
			sfx_id = S_FindSound (nametemp);
			if (sfx_id == -1) {
				sprintf (nametemp, templat, genders[player->userinfo.gender], name + 1);
				sfx_id = S_FindSound (nametemp);
			}
		}
		if (sfx_id == -1) {
			sprintf (nametemp, templat, "male", name + 1);
			sfx_id = S_FindSound (nametemp);
		}
	} else
		sfx_id = S_FindSound (name);

	if (sfx_id == -1)
		DPrintf ("Unknown sound %s\n", name);

	S_StartSound (ent, x, y, channel, sfx_id, volume, attenuation, looping);
}

void S_Sound (mobj_t *ent, int channel, char *name, float volume, int attenuation)
{
	S_StartNamedSound (ent, 0, 0, channel, name, volume, attenuation, false);
}

void S_LoopedSound (mobj_t *ent, int channel, char *name, float volume, int attenuation)
{
	S_StartNamedSound (ent, 0, 0, channel, name, volume, attenuation, true);
}

void S_PositionedSound (fixed_t x, fixed_t y, int channel, char *name, float volume, int attenuation)
{
	S_StartNamedSound ((mobj_t *)(~0), x, y, channel, name, volume, attenuation, false);
}

// S_StopSoundID from Hexen (albeit, modified somewhat)
BOOL S_StopSoundID (int sound_id, int priority)
{
	int i;
	int lp; //least priority
	int limit;
	int found;

	if (sound_id == sfx_plasma ||
		sound_id == sfx_chngun ||
		sound_id == sfx_chainguy) {
		limit = numChannels / 2 + 1;
	} else {
		limit = 2;
	}
	lp = -1;
	found = 0;
	for (i = 0; i < numChannels; i++)
	{
		if (Channel[i].sound_id == sound_id && Channel[i].sfxinfo)
		{
			found++; //found one.  Now, should we replace it??
			if (priority >= Channel[i].priority)
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

void S_StopSound (mobj_t *ent, int channel)
{
	int i;

	for (i = 0; i < numChannels; i++)
		if (Channel[i].sfxinfo
			&& Channel[i].mo == ent
			&& Channel[i].entchannel == channel)
			S_StopChannel (i);
}

void S_StopAllChannels (void)
{
	int i;

	for (i = 0; i < numChannels; i++)
		if (Channel[i].sfxinfo)
			S_StopChannel (i);
}

// Moves all the sounds from one thing to another. If the destination is
// NULL, then the sound becomes a positioned sound.
void S_RelinkSound (mobj_t *from, mobj_t *to)
{
	int i;

	if (!from)
		return;

	for (i = 0; i < numChannels; i++) {
		if (Channel[i].mo == from) {
			Channel[i].mo = to;
			Channel[i].x = from->x;
			Channel[i].y = from->y;
		}
	}
}

BOOL S_GetSoundPlayingInfo (struct mobj_s *ent, int sound_id)
{
	int i;

	for (i = 0; i < numChannels; i++)
	{
		if (Channel[i].mo == ent && Channel[i].sound_id == sound_id)
			return true;
	}
	return false;
}

//
// Stop and resume music, during game PAUSE.
//
void S_PauseSound (void)
{
	if (mus_playing.handle && !mus_paused)
	{
		I_PauseSong(mus_playing.handle);
		mus_paused = true;
	}
}

void S_ResumeSound (void)
{
	if (mus_playing.handle && mus_paused)
	{
		I_ResumeSong (mus_playing.handle);
		mus_paused = false;
	}
}

//
// Updates music & sounds
//
void S_UpdateSounds (void *listener_p)
{
	mobj_t *listener = (mobj_t*)listener_p;
	fixed_t x, y;
	int i, dist, vol;
	angle_t angle;
	int sep;

	for (i = 0; i < numChannels; i++)
	{
		if (!Channel[i].sfxinfo)
			continue;

		if (!I_SoundIsPlaying (Channel[i].handle))
		{
			S_StopChannel (i);
		}
		if (Channel[i].sound_id == -1
			|| Channel[i].mo == listener || Channel[i].attenuation == ATTN_NONE
			|| Channel[i].attenuation == ATTN_SURROUND)
		{
			continue;
		}
		else
		{
			if (!Channel[i].mo) {
				x = Channel[i].x;
				y = Channel[i].y;
			} else {
				x = Channel[i].mo->x;
				y = Channel[i].mo->y;
			}
			dist = P_AproxDistance2 (listener, x, y) >> FRACBITS;
			if (Channel[i].attenuation == ATTN_STATIC)
				dist *= 3;
			if (dist >= MAX_SND_DIST)
			{
				if (!(level.flags & LEVEL_NOSOUNDCLIPPING)) {
					S_StopChannel (i);
					continue;
				} else
					dist = MAX_SND_DIST;
			} else if (dist < 0) {
				dist = 0;
			}
			vol = (int)(SoundCurve[dist]*Channel[i].volume*2);
			if (dist > 0) {
				angle = R_PointToAngle2(listener->x, listener->y, x, y);
				if (angle > players[consoleplayer].camera->angle)
					angle = angle - players[consoleplayer].camera->angle;
				else
					angle = angle + (ANGLE_MAX - players[consoleplayer].camera->angle);
				angle >>= ANGLETOFINESHIFT;
				sep = NORM_SEP - (FixedMul (S_STEREO_SWING, finesine[angle])>>FRACBITS);
			} else {
				sep = NORM_SEP;
			}
			I_UpdateSoundParams (Channel[i].handle, vol, sep, Channel[i].pitch);
			Channel[i].priority = Channel[i].basepriority * (PRIORITY_MAX_ADJUST-(dist/DIST_ADJUST));
		}
	}

	SN_UpdateActiveSequences ();
}

void S_SetMusicVolume(int volume)
{
	if (volume < 0 || volume > 127)
	{
		Printf (PRINT_HIGH, "Attempt to set music volume at %d", volume);
	} else {
		I_SetMusicVolume (127);
		I_SetMusicVolume (volume);
	}
}

void S_SetSfxVolume(int volume)
{

	if (volume < 0 || volume > 127)
		Printf (PRINT_HIGH, "Attempt to set sfx volume at %d", volume);
	else
		I_SetSfxVolume (volume);
}

//
// Starts some music with the music id found in sounds.h.
//
void S_StartMusic(char *m_id)
{
	S_ChangeMusic(m_id, false);
}

// [RH] S_ChangeMusic() now accepts the name of the music lump.
// It's up to the caller to figure out what that name is.
void S_ChangeMusic (char *musicname, int looping)
{
	int lumpnum;
	void *data;
	int len;
	FILE *f;

	if (mus_playing.name && !stricmp (mus_playing.name, musicname))
		return;

	if (*musicname == 0) {
		// Don't choke if the map doesn't have a song attached
		S_StopMusic ();
		return;
	}

	if (!(f = fopen (musicname, "rb"))) {
		if ((lumpnum = W_CheckNumForName (musicname)) == -1) {
			char name[9];
			strncpy (name, musicname, 8);
			name[8] = 0;
			Printf (PRINT_HIGH, "Music lump \"%s\" not found\n", name);
			return;
		}
	} else {
		lumpnum = -1;
		len = Q_filelength (f);
		data = Malloc (len);
		fread (data, len, 1, f);
		fclose (f);
	}

	// shutdown old music
	S_StopMusic();

	// load & register it
	if (mus_playing.name)
		free (mus_playing.name);
	mus_playing.name = copystring (musicname);
	if (lumpnum != -1) {
		mus_playing.handle = I_RegisterSong(W_CacheLumpNum(lumpnum, PU_CACHE), W_LumpLength (lumpnum));
	} else {
		mus_playing.handle = I_RegisterSong(data, len);
		free (data);
	}

	// play it
	I_PlaySong(mus_playing.handle, looping);
}

void S_StopMusic (void)
{
	if (mus_playing.name)
	{
		if (mus_paused)
			I_ResumeSong(mus_playing.handle);

		I_StopSong(mus_playing.handle);
		I_UnRegisterSong(mus_playing.handle);

		free (mus_playing.name);
		mus_playing.name = NULL;
		mus_playing.handle = 0;
	}
}

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
		c->mo = NULL;
	}
}


// [RH] ===============================
//
//	Ambient sound and SNDINFO routines
//
// =============================== [RH]

sfxinfo_t *S_sfx;	// [RH] This is no longer defined in sounds.c
static int maxsfx;	// [RH] Current size of S_sfx array.
int numsfx;			// [RH] Current number of sfx defined.

static struct AmbientSound {
	unsigned	type;		// type of ambient sound
	int			periodmin;	// # of tics between repeats
	int			periodmax;	// max # of tics for random ambients
	int			volume;		// relative volume of sound
	char		sound[MAX_SNDNAME+1]; // Logical name of sound to play
} Ambients[256];

#define RANDOM		1
#define PERIODIC	2
#define CONTINUOUS	3
#define POSITIONAL	4
#define SURROUND	16

void S_HashSounds (void)
{
	int i;
	unsigned j;

	// Mark all buckets as empty
	for (i = 0; i < numsfx; i++)
		S_sfx[i].index = -1;

	// Now set up the chains
	for (i = 0; i < numsfx; i++) {
		j = MakeKey (S_sfx[i].name) % (unsigned)numsfx;
		S_sfx[i].next = S_sfx[j].index;
		S_sfx[j].index = i;
	}
}

int S_FindSound (const char *logicalname)
{
	int i;

	i = S_sfx[MakeKey (logicalname) % (unsigned)numsfx].index;

	while ((i != -1) && strnicmp (S_sfx[i].name, logicalname, MAX_SNDNAME))
		i = S_sfx[i].next;

	return i;
}

int S_FindSoundByLump (int lump)
{
	if (lump != -1) {
		int i;

		for (i = 0; i < numsfx; i++)
			if (S_sfx[i].lumpnum == lump)
				return i;
	}
	return -1;
}

int S_AddSoundLump (char *logicalname, int lump)
{
	if (numsfx == maxsfx) {
		maxsfx = maxsfx ? maxsfx*2 : 128;
		S_sfx = Realloc (S_sfx, maxsfx * sizeof(*S_sfx));
	}

	// logicalname MUST be < MAX_SNDNAME chars long
	strcpy (S_sfx[numsfx].name, logicalname);
	S_sfx[numsfx].data = NULL;
	S_sfx[numsfx].link = NULL;
	S_sfx[numsfx].usefulness = 0;
	S_sfx[numsfx].lumpnum = lump;
	return numsfx++;
}

int S_AddSound (char *logicalname, char *lumpname)
{
	int sfxid;

	// If the sound has already been defined, change the old definition.
	for (sfxid = 0; sfxid < numsfx; sfxid++)
		if (0 == stricmp (logicalname, S_sfx[sfxid].name))
			break;

	// Otherwise, prepare a new one.
	if (sfxid == numsfx) {
		sfxid = S_AddSoundLump (logicalname, W_CheckNumForName (lumpname));
	} else {
		S_sfx[sfxid].lumpnum = W_CheckNumForName (lumpname);
	}

	return sfxid;
}

// S_ParseSndInfo
// Parses all loaded SNDINFO lumps.
void S_ParseSndInfo (void)
{
	int lastlump, lump;
	char *sndinfo;
	char *data;

	lastlump = 0;
	while ((lump = W_FindLump ("SNDINFO", &lastlump)) != -1) {
		sndinfo = W_CacheLumpNum (lump, PU_CACHE);

		while ( (data = COM_Parse (sndinfo)) ) {
			if (com_token[0] == ';') {
				// Handle comments from Hexen MAPINFO lumps
				while (*sndinfo && *sndinfo != ';')
					sndinfo++;
				while (*sndinfo && *sndinfo != '\n')
					sndinfo++;
				continue;
			}
			sndinfo = data;
			if (com_token[0] == '$') {
				// com_token is a command

				if (!stricmp (com_token + 1, "ambient")) {
					// $ambient <num> <logical name> [point|surround] <type> [secs] <relative volume>
					struct AmbientSound *ambient, dummy;
					int index;

					sndinfo = COM_Parse (sndinfo);
					index = atoi (com_token);
					if (index < 0 || index > 255) {
						Printf (PRINT_HIGH, "Bad ambient index (%d)\n", index);
						ambient = &dummy;
					} else {
						ambient = Ambients + index;
					}
					memset (ambient, 0, sizeof(struct AmbientSound));

					sndinfo = COM_Parse (sndinfo);
					strncpy (ambient->sound, com_token, MAX_SNDNAME);
					ambient->sound[MAX_SNDNAME] = 0;

					sndinfo = COM_Parse (sndinfo);
					if (!stricmp (com_token, "point")) {
						ambient->type = POSITIONAL;
						sndinfo = COM_Parse (sndinfo);
					} else {
						if (!stricmp (com_token, "surround")) {
							ambient->type = SURROUND;
							sndinfo = COM_Parse (sndinfo);
						}
					}

					if (!stricmp (com_token, "continuous")) {
						ambient->type |= CONTINUOUS;
					} else if (!stricmp (com_token, "random")) {
						ambient->type |= RANDOM;
						sndinfo = COM_Parse (sndinfo);
						ambient->periodmin = (int)(atof (com_token) * TICRATE);
						sndinfo = COM_Parse (sndinfo);
						ambient->periodmax = (int)(atof (com_token) * TICRATE);
					} else if (!stricmp (com_token, "periodic")) {
						ambient->type |= PERIODIC;
						sndinfo = COM_Parse (sndinfo);
						ambient->periodmin = (int)(atof (com_token) * TICRATE);
					} else {
						Printf (PRINT_HIGH, "Unknown ambient type (%s)\n", com_token);
					}

					sndinfo = COM_Parse (sndinfo);
					ambient->volume = (int)(atof (com_token) * 255.0);
					if (ambient->volume > 255)
						ambient->volume = 255;
					else if (ambient->volume < 0)
						ambient->volume = 0;
				} else if (!stricmp (com_token + 1, "map")) {
					// Hexen-style $MAP command
					level_info_t *info;

					sndinfo = COM_Parse (sndinfo);
					sprintf (com_token, "MAP%02d", atoi (com_token));
					info = FindLevelInfo (com_token);
					sndinfo = COM_Parse (sndinfo);
					if (info->mapname[0])
						uppercopy (info->music, com_token);
				} else {
					Printf (PRINT_HIGH, "Unknown SNDINFO command %s\n", com_token);
					while (*sndinfo != '\n' && *sndinfo != '\0')
						sndinfo++;
				}
			} else {
				// com_token is a logical sound mapping
				char name[MAX_SNDNAME+1];

				strncpy (name, com_token, MAX_SNDNAME);
				name[MAX_SNDNAME] = 0;
				sndinfo = COM_Parse (sndinfo);
				S_AddSound (name, com_token);
			}
		}
	}
	S_HashSounds ();

	// [RH] Hack for pitch varying
	sfx_sawup = S_FindSound ("weapons/sawup");
	sfx_sawidl = S_FindSound ("weapons/sawidle");
	sfx_sawful = S_FindSound ("weapons/sawfull");
	sfx_sawhit = S_FindSound ("weapons/sawhit");
	sfx_itemup = S_FindSound ("misc/i_pkup");
	sfx_tink = S_FindSound ("misc/chat2");

	sfx_plasma = S_FindSound ("weapons/plasmaf");
	sfx_chngun = S_FindSound ("weapons/chngun");
	sfx_chainguy = S_FindSound ("chainguy/attack");
	sfx_empty = W_CheckNumForName ("dsempty");
}

void Cmd_Soundlist (void *plyr, int argc, char **argv)
{
	char lumpname[9];
	int i;

	lumpname[8] = 0;
	for (i = 0; i < numsfx; i++)
		if (S_sfx[i].lumpnum != -1) {
			strncpy (lumpname, lumpinfo[S_sfx[i].lumpnum].name, 8);
			Printf (PRINT_HIGH, "%3d. %s (%s)\n", i+1, S_sfx[i].name, lumpname);
		} else
			Printf (PRINT_HIGH, "%3d. %s **not present**\n", i+1, S_sfx[i].name);
}

void Cmd_Soundlinks (void *plyr, int argc, char **argv)
{
	int i;

	for (i = 0; i < numsfx; i++)
		if (S_sfx[i].link)
			Printf (PRINT_HIGH, "%s -> %s\n", S_sfx[i].name, S_sfx[i].link->name);
}

static void SetTicker (int *tics, struct AmbientSound *ambient)
{
	if ((ambient->type & CONTINUOUS) == CONTINUOUS) {
		*tics = 1;
	} else if (ambient->type & RANDOM) {
		*tics = (int)(((float)rand() / (float)RAND_MAX) *
				(float)(ambient->periodmax - ambient->periodmin)) +
				ambient->periodmin;
	} else {
		*tics = ambient->periodmin;
	}
}

void A_Ambient (mobj_t *actor)
{
	struct AmbientSound *ambient = &Ambients[actor->args[0]];

	if ((ambient->type & CONTINUOUS) == CONTINUOUS)
	{
		if (S_GetSoundPlayingInfo (actor, S_FindSound (ambient->sound)))
			return;

		if (ambient->sound[0]) {
			S_LoopedSound (actor, CHAN_BODY, ambient->sound, ambient->volume,
						   ambient->type & POSITIONAL ? ATTN_NORM :
						   (ambient->type & SURROUND ? ATTN_SURROUND : ATTN_NONE));

			SetTicker (&actor->tics, ambient);
		} else {
			P_RemoveMobj (actor);
		}
	}
	else
	{
		if (ambient->sound[0]) {
			S_Sound (actor, CHAN_BODY, ambient->sound, ambient->volume,
					 ambient->type & POSITIONAL ? ATTN_NORM :
						(ambient->type & SURROUND ? ATTN_SURROUND : ATTN_NONE));

			SetTicker (&actor->tics, ambient);
		} else {
			P_RemoveMobj (actor);
		}
	}
}

void S_ActivateAmbient (mobj_t *origin, int ambient)
{
	struct AmbientSound *amb = &Ambients[ambient];

	if (!(amb->type & 3) && !amb->periodmin) {
		sfxinfo_t *sfx = S_sfx + S_FindSound (amb->sound);

		// Make sure the sound has been loaded so we know how long it is
		if (!sfx->data)
			I_LoadSound (sfx);
		amb->periodmin = (sfx->ms * TICRATE) / 1000;
	}

	if (amb->type & (RANDOM|PERIODIC))
		SetTicker (&origin->tics, amb);
	else
		origin->tics = 1;
}
