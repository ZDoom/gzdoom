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


#define S_MAX_VOLUME			255

// when to clip out sounds
// Does not fit the large outdoor areas.
#define S_CLIPPING_DIST 		(1200*0x10000)

// Distance to origin when sounds should be maxed out.
// This should relate to movement clipping resolution
// (see BLOCKMAP handling).
// Originally: (200*0x10000).

#define S_CLOSE_DIST			(160<<FRACBITS)
#define S_ATTENUATOR			((S_CLIPPING_DIST-S_CLOSE_DIST)>>(FRACBITS))

#define NORM_PITCH				128
#define NORM_PRIORITY			64
#define NORM_SEP				128

#define S_PITCH_PERTURB 		1
#define S_STEREO_SWING			(96<<FRACBITS)


typedef struct
{
	sfxinfo_t*	sfxinfo;	// sound information (if null, channel avail.)
	void*		origin;		// origin of sound
	int 		handle;		// handle of the sound being played
	int			volume;		// [RH] volume of sound at origin
	int			priority;	// [RH] priority of sound
} channel_t;

// [RH] Hacks for pitch variance
int sfx_sawup, sfx_sawidl, sfx_sawful, sfx_sawhit;
int sfx_itemup, sfx_tink;

// [RH] Use surround sounds?
cvar_t *snd_surround;

// [RH] Print sound debugging info?
cvar_t *noisedebug;

// the set of channels available
static channel_t *channels;

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

// following is set
//	by the defaults code in M_misc:
// number of channels available
cvar_t 			*snd_channels;
int				numChannels;

static int		nextcleanup;


//
// [RH] Separated out of S_UpdateSoundParams
//
static fixed_t P_ApproxDistance (mobj_t *one, mobj_t *two)
{
	// calculate the distance to sound origin
	//	and clip it if necessary
	fixed_t adx = abs (one->x - two->x);
	fixed_t ady = abs (one->y - two->y);
	// From _GG1_ p.428. Appox. eucledian distance fast.
	return adx + ady - ((adx < ady ? adx : ady)>>1);
}

//
// [RH] Print sound debug info. Called from D_Display()
//
void S_NoiseDebug (void)
{
	int i, y;

	y = 32;
	if (level.time & 16)
		V_DrawText (0, y, "*** SOUND DEBUG INFO ***");
	y += 8;

	V_DrawText (0, y, "name");
	V_DrawText (100, y, "mo.x");
	V_DrawText (150, y, "mo.y");
	V_DrawText (200, y, "id");
	V_DrawText (230, y, "pri");
	V_DrawText (260, y, "dist");
	y += 8;

	for (i = 0; i < numChannels && y < screens[0].height - 16; i++, y += 8) {
		if (channels[i].sfxinfo) {
			char temp[16];
			mobj_t *origin = (mobj_t *)channels[i].origin;

			if ((unsigned int)origin < (unsigned int)ORIGIN_STARTOFNORMAL)
				origin = players[consoleplayer].camera;

			strcpy (temp, lumpinfo[channels[i].sfxinfo->lumpnum].name);
			temp[8] = 0;
			V_DrawText (0, y, temp);
			sprintf (temp, "%d", origin->x / FRACUNIT);
			V_DrawText (100, y, temp);
			sprintf (temp, "%d", origin->y / FRACUNIT);
			V_DrawText (150, y, temp);
			sprintf (temp, "%d", channels[i].sfxinfo - S_sfx);
			V_DrawText (200, y, temp);
			sprintf (temp, "%d", channels[i].priority);
			V_DrawText (230, y, temp);
			sprintf (temp, "%d", P_ApproxDistance (players[consoleplayer].camera, origin) / FRACUNIT);
			V_DrawText (260, y, temp);
		} else {
			V_DrawText (0, y, "------");
		}
	}
}


//
// Internals.
//
static int S_getChannel (void *origin, sfxinfo_t *sfxinfo, int pri);
static int S_AdjustSoundParams (mobj_t *listener, mobj_t *source,
								int *vol, int *sep, int *pitch );
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

	snd_MidiVolume->u.callback = ChangeMidiVol;
	snd_MusicVolume->u.callback = ChangeMusicVol;
	snd_SfxVolume->u.callback = ChangeSfxVol;
	ChangeMidiVol (snd_MidiVolume);

	Printf ("S_Init: default sfx volume %d\n", sfxVolume);

	// [RH] Read in sound sequences (TODO)
	//S_ParseSndSeq ();

	// Whatever these did with DMX, these are rather dummies now.
	I_SetChannels();
	
	S_SetSfxVolume(sfxVolume);
	S_SetMusicVolume(musicVolume);

	// Allocating the internal channels for mixing
	// (the maximum numer of sounds rendered
	// simultaneously) within zone memory.
	numChannels = (int)snd_channels->value;
	channels = (channel_t *) Z_Malloc(numChannels*sizeof(channel_t), PU_STATIC, 0);
	
	// Free all channels for use
	for (i=0 ; i<numChannels ; i++)
		channels[i].sfxinfo = 0;
	
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
void S_Start(void)
{
	int cnum;

	// kill all playing sounds at start of level
	//	(trust me - a good idea)
	for (cnum = 0; cnum < numChannels; cnum++)
		if (channels[cnum].sfxinfo)
			S_StopChannel (cnum);
	
	// start new music for the level
	mus_paused = 0;
	
	// [RH] This is a lot simpler now.
	S_ChangeMusic (level.music, true);
	
	nextcleanup = 15;
}


// [RH] Split S_StartSoundAtVolume into two parts so that sounds can
//		be specified both by id and by name.
void S_StartSfxAtVolume (void *origin_p, int sfx_id, int pri, int volume)
{
	int 		rc;
	int 		sep;
	int 		pitch;
	int 		priority;
	sfxinfo_t*	sfx;
	int 		cnum;
	
	mobj_t* 	origin = (mobj_t *) origin_p;
	
	if (sfx_id == -1) {
		S_StopSound (origin);
		return;
	}
	
	// Debug.
	/*fprintf( stderr,
			   "S_StartSoundAtVolume: playing sound %d (%s)\n",
			   sfx_id, S_sfx[sfx_id].name );*/
	

	sfx = &S_sfx[sfx_id];
	
	// Initialize sound parameters
	pitch = NORM_PITCH;
	priority = NORM_PRIORITY;

	// Check to see if it is audible,
	//	and if not, modify the params
	if (((unsigned int)origin >= (unsigned int)ORIGIN_STARTOFNORMAL) &&
		origin != players[consoleplayer].camera)	// [RH] Use camera
	{
		rc = S_AdjustSoundParams(players[consoleplayer].camera,	// [RH] Use camera
								 origin,
								 &volume,
								 &sep,
								 &pitch);
				
		if (!rc)
			return;

		if ( origin->x == players[consoleplayer].camera->x	// [RH] use camera
				 && origin->y == players[consoleplayer].camera->y)
		{
			sep = NORM_SEP;
		}
#if 0
		else
		{
			// [RH] Don't play more than two sounds w/ approximately
			//		the same origin. (Doesn't work.)
			int similarseps = 0;
			int i;

			for (i = 0; i < numChannels; i++) {
				if (channels[i].sfxinfo && channels[i].origin &&
					P_ApproxDistance (channels[i].origin, origin) < 64)
					similarseps++;
			}
			if (similarseps > 1)
				return;
		}
#endif
	} else if (((unsigned int)origin < (unsigned int)ORIGIN_STARTOFNORMAL) &&
				(((unsigned int)origin) & 128) &&
				snd_surround->value) {
		sep = -1;
	} else {
		sep = NORM_SEP;
	}
	// hacks to vary the sfx pitches
	if (sfx_id == sfx_sawup	|| sfx_id == sfx_sawhit ||
		sfx_id == sfx_sawidl || sfx_id == sfx_sawful)
		pitch += 8 - (M_Random()&15);
	else if (sfx_id != sfx_itemup && sfx_id != sfx_tink)
		pitch += 16 - (M_Random()&31);

	// kill old sound
	S_StopSound (origin);

	// try to find a channel
	cnum = S_getChannel (origin, sfx, pri);
	
	if (cnum < 0)
		return;

	if (sfx->link)
		sfx = sfx->link;

	//
	// This is supposed to handle the loading/caching.
	// For some odd reason, the caching is done nearly
	//	each time the sound is needed?
	//
	
	// cache data if necessary
	if (!sfx->data)
	{
		//Printf ("S_StartSoundAtVolume: 16bit and not pre-cached - wtf?\n");
		I_LoadSound (sfx);
		if (sfx->link)
			sfx = sfx->link;

		// DOS remains, 8bit handling
		//sfx->data = (void *) W_CacheLumpNum(sfx->lumpnum, PU_MUSIC);
		// fprintf( stderr,
		//			 "S_StartSoundAtVolume: loading %d (lump %d) : 0x%x\n",
		//			 sfx_id, sfx->lumpnum, (int)sfx->data );
		
	}
	
	// increase the usefulness
	if (sfx->usefulness++ < 0)
		sfx->usefulness = 1;
	
	// [RH] Store the sound's optimal volume
	channels[cnum].volume = volume;

	// Assigns the handle to one of the channels in the
	//	mix/output buffer.
	channels[cnum].handle = I_StartSound(sfx,
										 /*sfx->data,*/
										 volume,
										 sep,
										 pitch,
										 priority);
}

void S_StartSoundAtVolume (void *origin_p, char *name, int pri, int volume)
{
	int sfx_id;
	
	if (*name == '*') {
		// Sexed sound
		char nametemp[128];
		const char templat[] = "player/%s/%s";
		const char *genders[] = { "male", "female", "neuter" };
		player_t *player;

		sfx_id = -1;
		if (origin_p >= ORIGIN_STARTOFNORMAL && (player = ((mobj_t *)origin_p)->player)) {
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

	S_StartSfxAtVolume (origin_p, sfx_id, pri, volume);
}


void S_StopSound (void *origin)
{
	int cnum;

	for (cnum = 0; cnum < numChannels; cnum++)
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
			if (S_sfx[i].usefulness < 1 && S_sfx[i].usefulness > -1)
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
	
	for (cnum = 0; cnum < numChannels; cnum++)
	{
		c = &channels[cnum];
		sfx = c->sfxinfo;

		if (c->sfxinfo)
		{
			if (I_SoundIsPlaying (c->handle))
			{
				// initialize parameters
				volume = c->volume;
				pitch = NORM_PITCH;
				sep = NORM_SEP;

				// check non-local sounds for distance clipping
				//	or modify their params
				if (c->origin >= ORIGIN_STARTOFNORMAL && listener_p != c->origin)
				{
					audible = S_AdjustSoundParams(listener,
												  c->origin,
												  &volume,
												  &sep,
												  &pitch);
					
					if (!audible)
					{
						S_StopChannel (cnum);
					}
					else
						I_UpdateSoundParams (c->handle, volume, sep, pitch);
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
	} else {
		I_SetMusicVolume (127);
		I_SetMusicVolume (volume);
	}
}



void S_SetSfxVolume(int volume)
{

	if (volume < 0 || volume > 127)
		Printf ("Attempt to set sfx volume at %d", volume);
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
			Printf ("Music lump \"%s\" not found\n", musicname);
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


void S_StopMusic(void)
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




static void S_StopChannel(int cnum)
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
static int S_AdjustSoundParams (mobj_t *listener, mobj_t *source,
								int *vol, int *sep, int *pitch)
{
	fixed_t 	approx_dist;
	angle_t 	angle;

	approx_dist = P_ApproxDistance (listener, source);
	
	if (!(level.flags & LEVEL_NOSOUNDCLIPPING) && approx_dist > S_CLIPPING_DIST)
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
		//*vol = 255;
	} 
	else 
	{
		if (level.flags & LEVEL_NOSOUNDCLIPPING)
		{
			if (approx_dist > S_CLIPPING_DIST)
				approx_dist = S_CLIPPING_DIST;
		}

		// distance effect
		*vol = ((*vol)
				* ((S_CLIPPING_DIST - approx_dist)>>FRACBITS))
			/ S_ATTENUATOR; 
	}

	return (*vol > 0);
}




//
// S_getChannel :
//	 If none available, return -1.	Otherwise channel #.
//
static int S_getChannel (void *origin, sfxinfo_t *sfxinfo, int pri)
{
	// channel number to use
	int cnum;
	channel_t *c;

	// Find an open channel
	for (cnum = 0; cnum < numChannels; cnum++)
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
		for (cnum = 0; cnum < numChannels; cnum++)
			if (channels[cnum].priority >= pri) break;

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
	c->priority = pri;

	return cnum;
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
						Printf ("Bad ambient index (%d)\n", index);
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
						Printf ("Unknown ambient type (%s)\n", com_token);
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
					Printf ("Unknown SNDINFO command %s\n", com_token);
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
}

void Cmd_Soundlist (void *plyr, int argc, char **argv)
{
	char lumpname[9];
	int i;

	lumpname[8] = 0;
	for (i = 0; i < numsfx; i++)
		if (S_sfx[i].lumpnum != -1) {
			strncpy (lumpname, lumpinfo[S_sfx[i].lumpnum].name, 8);
			Printf ("%3d. %s (%s)\n", i+1, S_sfx[i].name, lumpname);
		} else
			Printf ("%3d. %s **not present**\n", i+1, S_sfx[i].name);
}

void Cmd_Soundlinks (void *plyr, int argc, char **argv)
{
	int i;

	for (i = 0; i < numsfx; i++)
		if (S_sfx[i].link)
			Printf ("%s -> %s\n", S_sfx[i].name, S_sfx[i].link->name);
}

void S_StartAmbient (char *name, int volume, BOOL surround)
{
	static int last = -1;

	if (++last >= 64)
		last = 0;

	S_StartSoundAtVolume ((mobj_t *)((8+last)|(surround?128:0)), name, 32, volume);
}

static void SetTicker (int *tics, struct AmbientSound *ambient)
{
	if (ambient->type & RANDOM) {
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

	if (ambient->sound[0]) {
		if (ambient->type & POSITIONAL)
			S_StartSoundAtVolume (actor, ambient->sound, 32, ambient->volume);
		else
			S_StartAmbient (ambient->sound, ambient->volume, ambient->type & SURROUND);

		SetTicker (&actor->tics, ambient);
	} else {
		P_RemoveMobj (actor);
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
