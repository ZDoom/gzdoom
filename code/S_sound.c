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
// $Log: s_sound.c,v $
// Revision 1.3  1998/01/05 16:26:08  pekangas
// Added music using Win32 MIDI MCI device
//
// Revision 1.2  1997/12/29 19:51:24  pekangas
// Ported to WinNT/95 environment using Watcom C 10.6.
// Everything expect joystick support is implemented, but networking is
// untested. Crashes way too often with problems in FixedDiv().
// Compiles with no warnings at warning level 3, uses MIDAS 1.1.1.
//
// Revision 1.1.1.1  1997/12/28 12:59:03  pekangas
// Initial DOOM source release from id Software
//
//
// DESCRIPTION:  none
//
//-----------------------------------------------------------------------------


const char
s_sound_rcsid[] = "$Id: s_sound.c,v 1.3 1998/01/05 16:26:08 pekangas Exp $";



#include <stdio.h>
#include <stdlib.h>

#include "i_system.h"
#include "i_sound.h"
#include "i_music.h"
#include "sounds.h"
#include "s_sound.h"

#include "z_zone.h"
#include "m_random.h"
#include "w_wad.h"

#include "doomdef.h"
#include "p_local.h"

#include "doomstat.h"


// Purpose?
const char snd_prefixen[]
= { 'P', 'P', 'A', 'S', 'S', 'S', 'M', 'M', 'M', 'S', 'S', 'S' };

#define S_MAX_VOLUME			127

// when to clip out sounds
// Does not fit the large outdoor areas.
#define S_CLIPPING_DIST 		(1200*0x10000)

// Distance tp origin when sounds should be maxed out.
// This should relate to movement clipping resolution
// (see BLOCKMAP handling).
// Originally: (200*0x10000).
#define S_CLOSE_DIST			(160*0x10000)


#define S_ATTENUATOR			((S_CLIPPING_DIST-S_CLOSE_DIST)>>(FRACBITS+4))

// Adjustable by menu.
#define NORM_VOLUME 			snd_MaxVolume

#define NORM_PITCH				128
#define NORM_PRIORITY			64
#define NORM_SEP				128

#define S_PITCH_PERTURB 		1
#define S_STEREO_SWING			(96*0x10000)

// percent attenuation from front to back
#define S_IFRACVOL				30

#define NA						0
#define S_NUMCHANNELS			2


typedef struct
{
	// sound information (if null, channel avail.)
	sfxinfo_t*	sfxinfo;

	// origin of sound
	void*		origin;

	// handle of the sound being played
	int 		handle;
	
} channel_t;


// the set of channels available
static channel_t*		channels;

// These are not used, but should be (menu).
// Maximum volume of a sound effect.
// Internal default is max out of 0-15.
cvar_t 			*snd_SfxVolume;

// Maximum volume of music. Almost useless so far.
cvar_t 			*snd_MusicVolume; 



// whether songs are mus_paused
static boolean			mus_paused; 	

// music currently being played
static struct {
	char *name;
	int   handle;
	void *data;
} mus_playing;

// following is set
//	by the defaults code in M_misc:
// number of channels available
cvar_t 					*snd_channels;
int						numChannels;

static int				nextcleanup;



//
// Internals.
//
int
S_getChannel
( void* 		origin,
  sfxinfo_t*	sfxinfo );


int
S_AdjustSoundParams
( mobj_t*		listener,
  mobj_t*		source,
  int*			vol,
  int*			sep,
  int*			pitch );

void S_StopChannel(int cnum);



//
// Initializes sound stuff, including volume
// Sets channels, SFX and music volume,
// allocates channel buffer, sets S_sfx lookup.
//
void S_Init (int sfxVolume, int musicVolume)
{
	int i;

	fprintf( stderr, "S_Init: default sfx volume %d\n", sfxVolume);

	// Whatever these did with DMX, these are rather dummies now.
	I_SetChannels();
	
	S_SetSfxVolume(sfxVolume);
	// No music with Linux - another dummy.
	S_SetMusicVolume(musicVolume);

	// Allocating the internal channels for mixing
	// (the maximum numer of sounds rendered
	// simultaneously) within zone memory.
	numChannels = (int)snd_channels->value;
	channels =
		(channel_t *) Z_Malloc(numChannels*sizeof(channel_t), PU_STATIC, 0);
	
	// Free all channels for use
	for (i=0 ; i<numChannels ; i++)
		channels[i].sfxinfo = 0;
	
	// no sounds are playing, and they are not mus_paused
	mus_paused = 0;

	// Note that sounds have not been cached (yet).
	for (i=1 ; i<NUMSFX ; i++)
		S_sfx[i].lumpnum = S_sfx[i].usefulness = -1;
}




//
// Per level startup code.
// Kills playing sounds at start of level,
// determines music if any, changes music.
//
void S_Start(void)
{
	int cnum;
	int mnum;

	// kill all playing sounds at start of level
	//	(trust me - a good idea)
	for (cnum=0 ; cnum<numChannels ; cnum++)
		if (channels[cnum].sfxinfo)
			S_StopChannel(cnum);
	
	// start new music for the level
	mus_paused = 0;
	
	if (gamemode == commercial)
		mnum = mus_runnin + gamemap - 1;
	else
	{
		int spmus[]=
		{
			// Song - Who? - Where?
			
			mus_e3m4, // American 		e4m1
			mus_e3m2, // Romero 		e4m2
			mus_e3m3, // Shawn			e4m3
			mus_e1m5, // American 		e4m4
			mus_e2m7, // Tim			e4m5
			mus_e2m4, // Romero 		e4m6
			mus_e2m6, // J.Anderson 	e4m7 CHIRON.WAD
			mus_e2m5, // Shawn			e4m8
			mus_e1m9	// Tim			e4m9
		};
		
		if (gameepisode < 4)
			mnum = mus_e1m1 + (gameepisode-1)*9 + gamemap-1;
		else
			mnum = spmus[gamemap-1];
		}
	
	// HACK FOR COMMERCIAL
	//	if (commercial && mnum > mus_e3m9)
	//			mnum -= mus_e3m9;
	
	S_ChangeMusic(S_music[mnum].name, true);
	
	nextcleanup = 15;
}





void S_StartSoundAtVolume (void *origin_p, int sfx_id, int volume)
{

	int 					rc;
	int 					sep;
	int 					pitch;
	int 					priority;
	sfxinfo_t*		sfx;
	int 					cnum;
	
	mobj_t* 			origin = (mobj_t *) origin_p;
	
	
	// Debug.
	/*fprintf( stderr,
			   "S_StartSoundAtVolume: playing sound %d (%s)\n",
			   sfx_id, S_sfx[sfx_id].name );*/
	
	// check for bogus sound #
	if (sfx_id < 1 || sfx_id > NUMSFX)
		I_Error("Bad sfx #: %d", sfx_id);
	
	sfx = &S_sfx[sfx_id];
	
	// Initialize sound parameters
	if (sfx->link)
	{
		pitch = (sfx->pitch - NORM_PITCH) * 8 + sfx->link->pitch;
		priority = sfx->priority;
		volume += sfx->volume;
		
		if (volume < 1)
			return;
	}
	else
	{
		pitch = sfx->pitch;
		priority = NORM_PRIORITY;
	}


	// Check to see if it is audible,
	//	and if not, modify the params
	if (((unsigned int)origin > 2) && origin != players[displayplayer].mo)
	{
		rc = S_AdjustSoundParams(players[displayplayer].mo,
								 origin,
								 &volume,
								 &sep,
								 &pitch);
				
		if ( origin->x == players[displayplayer].mo->x
				 && origin->y == players[displayplayer].mo->y)
		{
			sep = NORM_SEP;
		}
		
		if (!rc)
			return;
	} else if (origin == ORIGIN_SURROUND) {
		sep = -1;
	} else {
		sep = NORM_SEP;
	}
	// hacks to vary the sfx pitches
	if (sfx_id >= sfx_sawup
			&& sfx_id <= sfx_sawhit)
	{
		pitch += 64 - (M_Random()&127);
	}

	// kill old sound
	S_StopSound(origin);

	// try to find a channel
	cnum = S_getChannel(origin, sfx);
	
	if (cnum<0)
		return;

	//
	// This is supposed to handle the loading/caching.
	// For some odd reason, the caching is done nearly
	//	each time the sound is needed?
	//
	
	// get lumpnum if necessary
	if (sfx->lumpnum < 0)
		sfx->lumpnum = I_GetSfxLumpNum(sfx);

	// cache data if necessary
	if (!sfx->data)
	{
		Printf ("S_StartSoundAtVolume: 16bit and not pre-cached - wtf?\n");

		// DOS remains, 8bit handling
		//sfx->data = (void *) W_CacheLumpNum(sfx->lumpnum, PU_MUSIC);
		// fprintf( stderr,
		//			 "S_StartSoundAtVolume: loading %d (lump %d) : 0x%x\n",
		//			 sfx_id, sfx->lumpnum, (int)sfx->data );
		
	}
	
	// increase the usefulness
	if (sfx->usefulness++ < 0)
		sfx->usefulness = 1;
	
	// Assigns the handle to one of the channels in the
	//	mix/output buffer.
	channels[cnum].handle = I_StartSound(sfx_id,
										 /*sfx->data,*/
										 volume,
										 sep,
										 pitch,
										 priority);
}

void S_StartSound (void *origin, int sfx_id)
{
	S_StartSoundAtVolume(origin, sfx_id, 255);
}




void S_StopSound(void *origin)
{

	int cnum;

	for (cnum=0 ; cnum<numChannels ; cnum++)
	{
		if (channels[cnum].sfxinfo && channels[cnum].origin == origin)
		{
			S_StopChannel(cnum);
			break;
		}
	}
}









//
// Stop and resume music, during game PAUSE.
//
void S_PauseSound(void)
{
	if (mus_playing.handle && !mus_paused)
	{
		I_PauseSong(mus_playing.handle);
		mus_paused = true;
	}
}

void S_ResumeSound(void)
{
	if (mus_playing.handle && mus_paused)
	{
		I_ResumeSong(mus_playing.handle);
		mus_paused = false;
	}
}


//
// Updates music & sounds
//
void S_UpdateSounds(void* listener_p)
{
	int 		audible;
	int 		cnum;
	int 		volume;
	int 		sep;
	int 		pitch;
	sfxinfo_t*	sfx;
	channel_t*	c;
	
	mobj_t* 	listener = (mobj_t*)listener_p;


	
	// Clean up unused data.
	// This is currently not done for 16bit (sounds cached static).
	// DOS 8bit remains. 
	/*if (gametic > nextcleanup)
	{
		for (i=1 ; i<NUMSFX ; i++)
		{
			if (S_sfx[i].usefulness < 1
				&& S_sfx[i].usefulness > -1)
			{
				if (--S_sfx[i].usefulness == -1)
				{
					Z_ChangeTag(S_sfx[i].data, PU_CACHE);
					S_sfx[i].data = 0;
				}
			}
		}
		nextcleanup = gametic + 15;
	}*/
	
	for (cnum=0 ; cnum<numChannels ; cnum++)
	{
		c = &channels[cnum];
		sfx = c->sfxinfo;

		if (c->sfxinfo)
		{
			if (I_SoundIsPlaying(c->handle))
			{
				// initialize parameters
				volume = 255;
				pitch = sfx->pitch;
				sep = NORM_SEP;

				if (sfx->link)
				{
					pitch = (pitch - NORM_PITCH) * 8 + sfx->link->pitch;
					volume += sfx->volume;
					if (volume < 1)
					{
						S_StopChannel(cnum);
						continue;
					}
				}

				// check non-local sounds for distance clipping
				//	or modify their params
				if ((unsigned int)c->origin > 2u && listener_p != c->origin)
				{
					audible = S_AdjustSoundParams(listener,
												  c->origin,
												  &volume,
												  &sep,
												  &pitch);
					
					if (!audible)
					{
						S_StopChannel(cnum);
					}
					else
						I_UpdateSoundParams(c->handle, volume, sep, pitch);
				}
			}
			else
			{
				// if channel is allocated but sound has stopped,
				//	free it
				S_StopChannel(cnum);
			}
		}
	}
	// kill music if it is a single-play && finished
	// if ( 	mus_playing
	//		&& !I_QrySongPlaying(mus_playing->handle)
	//		&& !mus_paused )
	// S_StopMusic();
}


void S_SetMusicVolume(int volume)
{
	if (volume < 0 || volume > 127)
	{
		Printf ("Attempt to set music volume at %d", volume);
	}	 

	I_SetMusicVolume(127);
	I_SetMusicVolume(volume);
}



void S_SetSfxVolume(int volume)
{

	if (volume < 0 || volume > 127)
		Printf ("Attempt to set sfx volume at %d", volume);

	I_SetSfxVolume (volume);
}

//
// Starts some music with the music id found in sounds.h.
//
void S_StartMusic(char *m_id)
{
	S_ChangeMusic(m_id, false);
}

/* [Petteri] Hack: music lump number: */
extern int musicLump;

// [RH] S_ChangeMusic() now accepts the name of
// the music lump. It's now up to the caller to
// figure out what that name is.
// TODO: Move the information from S_music
// into a MAPINFO-type lump.
void S_ChangeMusic (char *musicname, int looping)
{
	int lumpnum;

	if ((lumpnum = W_CheckNumForName (musicname)) == -1) {
		Printf ("Music lump \"%s\" not found\n", musicname);
		return;
	}

	if (mus_playing.name && !stricmp (mus_playing.name, musicname))
		return;

	// shutdown old music
	S_StopMusic();

	/* [Petteri]: */
	musicLump = lumpnum;

	// load & register it
	if (mus_playing.name)
		free (mus_playing.name);
	mus_playing.name = copystring (musicname);
	mus_playing.data = (void *)W_CacheLumpNum(lumpnum, PU_MUSIC);
	mus_playing.handle = I_RegisterSong(mus_playing.data);

	// play it
	I_PlaySong(mus_playing.handle, looping);
}


void S_StopMusic(void)
{
	if (mus_playing.name)
	{
		if (mus_paused)
			I_ResumeSong(mus_playing.handle);

		I_StopSong(mus_playing.handle);
		I_UnRegisterSong(mus_playing.handle);
		Z_ChangeTag(mus_playing.data, PU_CACHE);
		
		mus_playing.data = 0;
		free (mus_playing.name);
		mus_playing.name = NULL;
		mus_playing.handle = 0;
	}
}




void S_StopChannel(int cnum)
{

	int 		i;
	channel_t*	c = &channels[cnum];

	if (c->sfxinfo)
	{
		// stop the sound playing
		if (I_SoundIsPlaying(c->handle))
		{
			I_StopSound(c->handle);
		}

		// check to see
		//	if other channels are playing the sound
		for (i=0 ; i<numChannels ; i++)
		{
			if (cnum != i
				&& c->sfxinfo == channels[i].sfxinfo)
			{
				break;
			}
		}
		
		// degrade usefulness of sound data
		c->sfxinfo->usefulness--;

		c->sfxinfo = 0;
	}
}



//
// Changes volume, stereo-separation, and pitch variables
//	from the norm of a sound effect to be played.
// If the sound is not audible, returns a 0.
// Otherwise, modifies parameters and returns 1.
//
int
S_AdjustSoundParams
( mobj_t*		listener,
  mobj_t*		source,
  int*			vol,
  int*			sep,
  int*			pitch )
{
	fixed_t 	approx_dist;
	fixed_t 	adx;
	fixed_t 	ady;
	angle_t 	angle;

	// calculate the distance to sound origin
	//	and clip it if necessary
	adx = abs(listener->x - source->x);
	ady = abs(listener->y - source->y);

	// From _GG1_ p.428. Appox. eucledian distance fast.
	approx_dist = adx + ady - ((adx < ady ? adx : ady)>>1);
	
	if (gamemap != 8
		&& approx_dist > S_CLIPPING_DIST)
	{
		return 0;
	}
	
	// angle of source to listener
	angle = R_PointToAngle2(listener->x,
							listener->y,
							source->x,
							source->y);

	if (angle > listener->angle)
		angle = angle - listener->angle;
	else
		angle = angle + (0xffffffff - listener->angle);

	angle >>= ANGLETOFINESHIFT;

	// stereo separation
	*sep = 128 - (FixedMul(S_STEREO_SWING,finesine[angle])>>FRACBITS);

	// volume calculation
	if (approx_dist < S_CLOSE_DIST)
	{
		*vol = 255;
	}
	else if (gamemap == 8)
	{
		if (approx_dist > S_CLIPPING_DIST)
			approx_dist = S_CLIPPING_DIST;

		*vol = (15
				* ((S_CLIPPING_DIST - approx_dist)>>FRACBITS))
			/ S_ATTENUATOR; 
	}
	else
	{
		// distance effect
		*vol = (15
				* ((S_CLIPPING_DIST - approx_dist)>>FRACBITS))
			/ S_ATTENUATOR; 
	}
	
	return (*vol > 0);
}




//
// S_getChannel :
//	 If none available, return -1.	Otherwise channel #.
//
int
S_getChannel
( void* 		origin,
  sfxinfo_t*	sfxinfo )
{
	// channel number to use
	int 		cnum;
	
	channel_t*	c;

	// Find an open channel
	for (cnum=0 ; cnum<numChannels ; cnum++)
	{
		if (!channels[cnum].sfxinfo)
			break;
		else if (origin &&	channels[cnum].origin ==  origin)
		{
			S_StopChannel(cnum);
			break;
		}
	}

	// None available
	if (cnum == numChannels)
	{
		// Look for lower priority
		for (cnum=0 ; cnum<numChannels ; cnum++)
			if (channels[cnum].sfxinfo->priority >= sfxinfo->priority) break;

		if (cnum == numChannels)
		{
			// No lower priority.  Sorry, Charlie.	 
			return -1;
		}
		else
		{
			// Otherwise, kick out lower priority.
			S_StopChannel(cnum);
		}
	}

	c = &channels[cnum];

	// channel is decided to be cnum.
	c->sfxinfo = sfxinfo;
	c->origin = origin;

	return cnum;
}
