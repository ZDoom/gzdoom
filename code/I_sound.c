/*		midassound.c
 *
 * A Sound module for DOOM using MIDAS Digital Audio System. Based on
 * original i_sound.c from the source distribution.
 * 
 * Petteri Kangaslampi, pekangas@sci.fi
*/


/* Original file header: */
//-----------------------------------------------------------------------------
//
// i_sound.c
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
// DESCRIPTION:
//		System interface for sound.
//
//-----------------------------------------------------------------------------

const char
midassound_rcsid[] = "$Id: midassound.c,v 1.5 1998/01/07 18:44:18 pekangas Exp $";

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <math.h>

/* We'll need to go below the MIDAS API level a bit */
#include "../midas/src/midas/midas.h"
#include "../midas/include/midasdll.h"

#include "z_zone.h"

#include "c_cvars.h"
#include "i_system.h"
#include "i_sound.h"
#include "i_music.h"
#include "m_argv.h"
#include "m_misc.h"
#include "w_wad.h"
#include "i_video.h"

#include "doomdef.h"

cvar_t *snd_samplerate;

// The number of internal mixing channels,
//	the samples calculated for each mixing step,
//	the size of the 16bit, 2 hardware channel (stereo)
//	mixing buffer, and the samplerate of the raw data.

#define NUM_CHANNELS			8

#define SAMPLERATE				11025	// Hz
#define SAMPLESIZE				2		// 16bit

// The actual lengths of all sound effects.
int 			lengths[NUMSFX];

// The channel data pointers, start and end.
unsigned char*	channels[NUM_CHANNELS];


// Time/gametic that the channel started playing,
//	used to determine oldest, which automatically
//	has lowest priority.
// In case number of active sounds exceeds
//	available channels.
int 			channelstart[NUM_CHANNELS];

// The sound in channel handles,
//	determined on registration,
//	might be used to unregister/stop/modify,
//	currently unused.
int 			channelhandles[NUM_CHANNELS];

// SFX id of the playing sound effect.
// Used to catch duplicates (like chainsaw).
int 			channelids[NUM_CHANNELS];

// Surround sounds must be played to completion
boolean channelcritical[NUM_CHANNELS];

/* MIDAS sample play handles for all channels: */
MIDASsamplePlayHandle channelPlayHandles[NUM_CHANNELS];

/* MIDAS sound device channels corresponding to virtual channels */
DWORD midaschannel[NUM_CHANNELS];

static int wavonly = 0;
static int primarysound = 0;
static int nosound = 0;


float volmul;


void MIDASerror(void)
{
	I_Error ("MIDAS Error: %s", MIDASgetErrorMessage(MIDASgetLastError()));
}



/* Loads a sound and adds it to MIDAS - really returns a MIDAS
   sample handle */
void*
getsfx
( char* 		sfxname,
  int*			len,
  int*			pitch)
{
	unsigned char*		sfx;
	int 				size;
	char				name[20];
	int 				sfxlump;
	
	int 		error;
	static sdSample smp;
	unsigned sampleHandle;

	
	// Get the sound data from the WAD, allocate lump
	//	in zone memory.
	sprintf(name, "ds%s", sfxname);

	// Now, there is a severe problem with the
	//	sound handling, in it is not (yet/anymore)
	//	gamemode aware. That means, sounds from
	//	DOOM II will be requested even with DOOM
	//	shareware.
	// The sound list is wired into sounds.c,
	//	which sets the external variable.
	// I do not do runtime patches to that
	//	variable. Instead, we will use a
	//	default sound for replacement.
	if ((sfxlump = W_CheckNumForName(name)) == -1)
		sfxlump = W_GetNumForName("dspistol");
	
	size = W_LumpLength( sfxlump );

	// Debug.
	// fprintf( stderr, "." );
	//fprintf( stderr, " -loading  %s (lump %d, %d bytes)\n",
	//		 sfxname, sfxlump, size );
	//fflush( stderr );
	
	sfx = (unsigned char*)W_CacheLumpNum( sfxlump, PU_STATIC );

	/* Add sound to MIDAS: */
	/* A hack below API level - yeahyeah, we should add support for preparing
	   samples from memory to the API */
	
	/* Build Sound Device sample structure for the sample: */
	smp.sample = sfx+8;
	smp.samplePos = sdSmpConv;
	smp.sampleType = MIDAS_SAMPLE_8BIT_MONO;
	smp.sampleLength = size-8;

	*pitch = ((unsigned short *)sfx)[1];		// Extract pitch from the sound header

	/* No loop: */
	smp.loopMode = sdLoopNone;
	smp.loop1Start = smp.loop1End = 0;
	smp.loop1Type = loopNone;

	/* No loop 2: */
	smp.loop2Start = smp.loop2End = 0;
	smp.loop2Type = loopNone;

	/* Add the sample to the Sound Device: */
	if ( (error = midasSD->AddSample(&smp, 1, &sampleHandle)) != OK )
		I_Error("getsfx: AddSample failed: %s", MIDASgetErrorMessage(error));

	// Remove the cached lump.
	Z_Free( sfx );
	
	// Preserve padded length.
	*len = size-8;

	/* Return sample handle: (damn ugly) */
	return (void*) sampleHandle;
}





//
// This function adds a sound to the
//	list of currently active sounds,
//	which is maintained as a given number
//	(eight, usually) of internal channels.
// Returns a handle.
//
int
addsfx
( int			sfxid,
  int			volume,
  int			seperation,
  int			pitch )
{
	static unsigned short		handlenums = 0;
 
	int 		i;
	int 		rc = -1;
	
	int 		oldest = gametic;
	int 		oldestnum = 0;
	int 		slot;

	MIDASsamplePlayHandle playHandle;
	int vol;
	int pan;

	// Chainsaw troubles.
	// Play these sound effects only one at a time.
	if ( sfxid == sfx_sawup
		 || sfxid == sfx_sawidl
		 || sfxid == sfx_sawful
		 || sfxid == sfx_sawhit
		 || sfxid == sfx_stnmov
		 || sfxid == sfx_pistol  )
	{
		// Loop all channels, check.
		for (i=0 ; i<NUM_CHANNELS ; i++)
		{
			// Active, and using the same SFX?
			if ( (channels[i])
				 && (channelids[i] == sfxid) )
			{
				// Reset.
				channels[i] = 0;
				// We are sure that iff,
				//	there will only be one.
				break;
			}
		}
	}

	// Loop all channels to find oldest SFX.
	for (i=0; (i<NUM_CHANNELS) && (channels[i]); i++)
	{
		if (channelstart[i] < oldest /* && channelcritical[i] == false */)
		{
			oldestnum = i;
			oldest = channelstart[i];
		}
	}

	// Tales from the cryptic.
	// If we found a channel, fine.
	// If not, we simply overwrite the first one, 0.
	// Probably only happens at startup.
	if (i == NUM_CHANNELS)
		slot = oldestnum;
	else
		slot = i;

	// Okay, in the less recent channel,
	//	we will handle the new SFX.
	// Set pointer to raw data.
	channels[slot] = (unsigned char *) S_sfx[sfxid].data;

	// Reset current handle number, limited to 0..100.
	if (!handlenums)
		handlenums = 100;

	// Assign current handle number.
	// Preserved so sounds could be stopped (unused).
	channelhandles[slot] = rc = handlenums++;

	// Should be gametic, I presume.
	channelstart[slot] = gametic;

	/* Calculate MIDAS volume and panning from volume and separation: */
	vol = ((int)(volume*volmul))/4;   /* original range 0-255, MIDAS range 0-64 */
	if (seperation < 0) {
		pan = MIDAS_PAN_SURROUND;
		channelcritical[slot] = true;
	} else {
		pan = (seperation-128) / 2;  /* original 0-255(?), MIDAS -64-64 */
		channelcritical[slot] = false;

		/* Clamp: */
		if ( pan < MIDAS_PAN_LEFT) pan = MIDAS_PAN_LEFT;
		else if ( pan > MIDAS_PAN_RIGHT) pan = MIDAS_PAN_RIGHT;
	}
	if ( vol > 64 ) vol = 64;
	else if ( vol < 0 ) vol = 0;

	// Preserve sound SFX id,
	//	e.g. for avoiding duplicates of chainsaw.
	channelids[slot] = sfxid;

	/* Play the sound: */
	if ( (playHandle = MIDASplaySample((MIDASsample) S_sfx[sfxid].data,
									   midaschannel[slot], 0, pitch, vol, pan)) == 0 )
		MIDASerror();

	channelPlayHandles[slot] = playHandle;

	// You tell me.
/*	[Petteri] We'll return the MIDAS sample playback handle instead
	return rc;*/
	return (int) playHandle;
}





//
// SFX API
// Note: this was called by S_Init.
// However, whatever they did in the
// old DPMS based DOS version, this
// were simply dummies in the Linux
// version.
// See soundserver initdata().
//
void I_SetChannels()
{
}		

 
void I_SetSfxVolume(int volume)
{
	// volume range is 0-15
	// volmul range is 0-1.whatever

	volmul = (volume / 15.0) * (256.0 / 255.0);
}


//
// Retrieve the raw data lump index
//	for a given SFX name.
//
int I_GetSfxLumpNum(sfxinfo_t* sfx)
{
	char namebuf[9];
	sprintf(namebuf, "ds%s", sfx->name);
	return W_GetNumForName(namebuf);
}

//
// Starting a sound means adding it
//	to the current list of active sounds
//	in the internal channels.
// As the SFX info struct contains
//	e.g. a pointer to the raw data,
//	it is ignored.
// As our sound handling does not handle
//	priority, it is ignored.
// Pitching (that is, increased speed of playback)
//	is set, but currently not used by mixing.
//
int
I_StartSound
( int			id,
  int			vol,
  int			sep,
  int			pitch,
  int			priority )
{

  // UNUSED
  priority = 0;

#ifdef DEBUGPRINT  
  fprintf(stderr, "I_StartSound: vol %i, sep %i, pitch %i, pri %i\n",
		  vol, sep, pitch, priority);
#endif	
  
	// Returns a handle (not used).
	id = addsfx( id, vol, sep, pitch );

	return id;
}



void I_StopSound (int handle)
{
  // You need the handle returned by StartSound.
  // Would be looping all channels,
  //  tracking down the handle,
  //  an setting the channel to zero.
		int slot;

#ifdef DEBUGPRINT	 
	fprintf(stderr, "I_StopSound\n");
#endif	  
	/* [Petteri] We'll simply stop the sound with MIDAS (?): */
	if ( !MIDASstopSample((MIDASsamplePlayHandle) handle) )
		MIDASerror();

	for (slot = 0; slot < NUM_CHANNELS; slot++) {
		if (channelPlayHandles[slot] == (MIDASsamplePlayHandle) handle) {
			channelcritical[slot] = false;
			channelPlayHandles[slot] = 0;
			break;
		}
	}
}


int I_SoundIsPlaying(int handle)
{
	int slot;

	int is;
	is = (int) MIDASgetSamplePlayStatus((MIDASsamplePlayHandle) handle);
#ifdef DEBUGPRINT	 
	fprintf(stderr, "I_SoundIsPlaying: %i\n", is);
#endif

	if (!is) {
		for (slot = 0; slot < NUM_CHANNELS; slot++) {
			if (channelPlayHandles[slot] == (MIDASsamplePlayHandle) handle) {
				channelcritical[slot] = false;
				channelPlayHandles[slot] = 0;
				break;
			}
		}
	}

	return is;
}



int I_QrySongPlaying(int handle);


void I_UpdateSound( void )
{
	/* Loop song if necessary: */
	I_QrySongPlaying(1);
}


// 
// This would be used to write out the mixbuffer
//	during each game loop update.
// Updates sound buffer and audio device at runtime. 
// It is called during Timer interrupt with SNDINTR.
// Mixing now done synchronous, and
//	only output be done asynchronous?
//
void
I_SubmitSound(void)
{
  // Write it to DSP device.
/*	write(audio_fd, mixbuffer, SAMPLECOUNT*BUFMUL);*/
}



void
I_UpdateSoundParams
( int	handle,
  int	vol,
  int	sep,
  int	pitch)
{
	int mvol, mpan;
	
	/* Calculate MIDAS volume and panning from volume and separation: */
	mvol = ((int)(vol*volmul))/4;	 /* original range 0-255, MIDAS range 0-64 */
	mpan = (sep-128) / 2;  /* original 0-255(?), MIDAS -64-64 */

	/* Clamp: */
	if ( mvol > 64 ) mvol = 64;
	if ( mvol < 0 ) mvol = 0;
	if ( mpan < -64) mpan = -64;
	if ( mpan > 64) mpan = 64;

	/* Set: */
	if ( !MIDASsetSampleVolume((MIDASsamplePlayHandle) handle, mvol) )
		MIDASerror();
	if ( !MIDASsetSamplePanning((MIDASsamplePlayHandle) handle, mpan) )
		MIDASerror();
	if ( !MIDASsetSampleRate((MIDASsamplePlayHandle) handle, pitch) )
		MIDASerror();
}




void I_ShutdownSound(void)
{
	unsigned i;

	Printf("I_ShutdownSound: Stopping sounds\n");
	for ( i = 0; i < NUM_CHANNELS; i++ ) {
		MIDASstopSample (channelPlayHandles[i]);
		if (midaschannel[i] != MIDAS_ILLEGAL_CHANNEL)
			MIDASfreeSample (midaschannel[i]);
	}
	
	Printf("I_ShutdownSound: Uninitializing MIDAS\n");

	I_ShutdownMusic();
	if ( !MIDASstopBackgroundPlay() )
		MIDASerror();
	if ( !MIDAScloseChannels() )
		MIDASerror();
	if ( !MIDASclose() )
		MIDASerror();
}




extern DWORD Window;  /* window handle from i_main.c (actually HWND) */


void
I_InitSound()
{
	unsigned i;

	I_InitMusic();

	for ( i = 0; i < NUM_CHANNELS; i++ ) {
		midaschannel[i] = MIDAS_ILLEGAL_CHANNEL;
	}

	/* Get command line options: */
	wavonly = !!M_CheckParm("-wavonly");
	primarysound = !!M_CheckParm("-primarysound");
	nosound = !!M_CheckParm("-nosound");
	
	Printf ("I_InitSound: Initializing MIDAS\n");
	
	MIDASstartup();
	
	MIDASsetOption(MIDAS_OPTION_MIXRATE, (int)snd_samplerate->value);
	MIDASsetOption(MIDAS_OPTION_MIXBUFLEN, 200);

	if ( !wavonly )
	{
		MIDASsetOption(MIDAS_OPTION_DSOUND_HWND, Window);
		if ( primarysound )
			MIDASsetOption(MIDAS_OPTION_DSOUND_MODE, MIDAS_DSOUND_PRIMARY);
		else
			MIDASsetOption(MIDAS_OPTION_DSOUND_MODE, MIDAS_DSOUND_STREAM);
	}

	if ( nosound )
		MIDASsetOption(MIDAS_OPTION_FORCE_NO_SOUND, TRUE);	
	
	if ( !MIDASinit() )
		MIDASerror();
	if ( !MIDASstartBackgroundPlay(100) )
		MIDASerror();
	if ( !MIDASopenChannels(NUM_CHANNELS+64) )
		MIDASerror();

	for ( i = 0; i < NUM_CHANNELS; i++ ) {
		channelPlayHandles[i] = 0;
		if (MIDAS_ILLEGAL_CHANNEL == (midaschannel[i] = MIDASallocateChannel ()))
			MIDASerror();
	}

	/* Simply preload all sounds and throw them at MIDAS: */
	Printf("I_InitSound: Loading all sounds\n");
	for ( i = 1; i < NUMSFX; i++ )
	{
		if ( !S_sfx[i].link )
		{
			/* Fine, not an alias, just load it */
			S_sfx[i].data = getsfx(S_sfx[i].name, &lengths[i], &S_sfx[i].pitch);
			/* getsfx() actually returns a MIDAS sample handle */
		}
		else
		{
			/* An alias sound */
			S_sfx[i].data = S_sfx[i].link->data;
			lengths[i] = lengths[(S_sfx[i].link - S_sfx)/sizeof(sfxinfo_t)];
		}
	}

  MIDASsetAmplification (NUM_CHANNELS * 50);

  // Finished initialization.
  Printf("I_InitSound: sound module ready\n");	
}
