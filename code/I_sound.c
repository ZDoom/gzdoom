/*		midassound.c
 *
 * A Sound module for DOOM using MIDAS Digital Audio System. Based on
 * original i_sound.c from the source distribution.
 * 
 * Petteri Kangaslampi, pekangas@sci.fi
 *
 * [RH] Changed most of this to be more BOOM-like since the original
 *		version kept around a lot of the Linux version's software
 *		stuff which we can let MIDAS handle instead.
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


#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

#include "resource.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <malloc.h>
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
#include "s_sound.h"

#include "doomdef.h"

extern HINSTANCE hInstance;

BOOL MidasInited;

// [RH] The mixing rate of the sounds.
cvar_t *snd_samplerate;

// [RH] The number of channels to use for sound effects.
//		We always allocate 64 more than this if music is enabled.
extern cvar_t *snd_channels;

// killough 2/21/98: optionally use varying pitched sounds

cvar_t *snd_pitched;	// [RH] Use a cvar to control it!
#define PITCH(f,x) (snd_pitched->value ? ((f)*(x))/128 : (f))

// [RH] Not really the number of channels. That's
//		defined with the snd_channels cvar.
#define NUM_CHANNELS			256

/* MIDAS sample play handles for all psuedo-channels: */
static MIDASsamplePlayHandle channels[NUM_CHANNELS];

// [RH] The sfx playing on each psuedo-channel.
static int sfx_ids[NUM_CHANNELS];

// The actual lengths of all sound effects.
// [RH] I can't see that this is used anywhere.
static unsigned int lengths[NUMSFX];

// [RH] The playback frequency of all sound effects.
static unsigned int frequencies[NUMSFX];

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
void *getsfx (char *sfxname, unsigned int *len, int *frequency, unsigned *ms)
{
	unsigned char*		sfx;
	int 				size;
	char				name[20];
	int 				sfxlump;
	
	int 				error;
	static sdSample		smp;
	unsigned			sampleHandle;

	
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

	*frequency = ((unsigned short *)sfx)[1];	// Extract sample rate from the sound header
	if (*frequency == 0)
		*frequency = 11025;

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

	*ms = ((size - 8)*1000) / (*frequency);

	/* Return sample handle: (damn ugly) */
	return (void*) sampleHandle;
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

	volmul = (float)((volume / 15.0) * (256.0 / 255.0));
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
//		to the current list of active sounds
//		in the internal channels.
// As the SFX info struct contains
//		e.g. a pointer to the raw data,
//		it is ignored.
// [RH] Unlike the original Linux code, priority
//		is now meaningful (Q: but is it really useful?).
//		This code is based on the code from BOOM and NTDOOM.
//
int I_StartSound (int id, int vol, int sep, int pitch, int priority)
{
	static int handle;

	// move up one slot, with wraparound
	if (++handle >= NUM_CHANNELS)
		handle = 0;

	// destroy anything still in the slot
	if (channels[handle])
		MIDASstopSample (channels[handle]);

	// Start the sound
	{
		int volume;
		int pan;

		/* Calculate MIDAS volume and panning from volume and separation: */
		volume = ((int)(vol*volmul))/4;   /* original range 0-255, MIDAS range 0-64 */
		if ( volume > 64 ) volume = 64;
		else if ( volume < 0 ) volume = 0;

		if (sep < 0) {
			pan = MIDAS_PAN_SURROUND;
			priority = 255;		// [RH] Surround sounds are always high priority
		} else {
			priority = 128 - priority;	// [RH] Doom priorities are reverse of MIDAS.
			pan = (sep-128) / 2;  /* original 0-255, MIDAS -64-64 */

			/* Clamp: */
			if ( pan < MIDAS_PAN_LEFT) pan = MIDAS_PAN_LEFT;
			else if ( pan > MIDAS_PAN_RIGHT) pan = MIDAS_PAN_RIGHT;
		}

		channels[handle] = MIDASplaySample ((MIDASsample)S_sfx[id].data,
											MIDAS_CHANNEL_AUTO,
											priority,
											PITCH(frequencies[id],pitch),
											volume,
											pan);
		sfx_ids[handle] = id;
	}

	// Reference for s_sound.c to use when calling functions below
	return handle;
}



void I_StopSound (int handle)
{
	// You need the handle returned by StartSound.
	// Would be looping all channels,
	//  tracking down the handle,
	//  an setting the channel to zero.

	/* [Petteri] We'll simply stop the sound with MIDAS (?): */
	if ( !MIDASstopSample(channels[handle]) )
		MIDASerror();

	channels[handle] = 0;
}


int I_SoundIsPlaying(int handle)
{
	int is;

	if (!channels[handle])
		return 0;
	else if (!(is = (int)MIDASgetSamplePlayStatus (channels[handle])))
		channels[handle] = 0;

	return is;
}



void I_UpdateSound( void )
{
}


// 
// This would be used to write out the mixbuffer
//	during each game loop update.
// Updates sound buffer and audio device at runtime. 
// It is called during Timer interrupt with SNDINTR.
// Mixing now done synchronous, and
//	only output be done asynchronous?
//
void I_SubmitSound(void)
{
}



void I_UpdateSoundParams (int handle, int vol, int sep, int pitch)
{
	int mvol, mpan;
	
	if (!channels[handle])
		return;

	/* Calculate MIDAS volume and panning from volume and separation: */
	mvol = ((int)(vol*volmul))/4;	 /* original range 0-255, MIDAS range 0-64 */
	mpan = (sep-128) / 2;			 /* original 0-255, MIDAS -64-64 */

	/* Clamp: */
	if ( mvol > 64 ) mvol = 64;
	if ( mvol < 0 ) mvol = 0;
	if ( mpan < -64) mpan = -64;
	if ( mpan > 64) mpan = 64;

	/* Set: */
	{
		MIDASsamplePlayHandle sample = channels[handle];

		if ( !MIDASsetSampleVolume(sample, mvol) ||
			 !MIDASsetSamplePanning(sample, mpan) ||
			 !MIDASsetSampleRate(sample, PITCH(frequencies[sfx_ids[handle]],pitch)) )
			MIDASerror();
	}
}




extern DWORD Window;  /* window handle from i_main.c (actually HWND) */


// [RH] Dialog procedure for the error dialog that appears if MIDAS
//		could not be initialized for some reason.
BOOL CALLBACK InitBoxCallback (HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	static const char *messyTemplate =
		"MIDAS could not be initialized.\r\n"
		"(Reason: %s.)\r\n\r\n"
		"If another program is using the sound card, you "
		"can try stoping it and clicking \"Retry.\"\r\n\r\n"
		"Otherwise, you can either click \"No Sound\" to use "
		"ZDoom without sound or click \"Quit\" if you don't "
		"really want to play ZDoom.";

	switch (message) {
		case WM_INITDIALOG:
			{
				char *midaserr = MIDASgetErrorMessage (MIDASgetLastError ());
				char messyText[2048];

				sprintf (messyText, messyTemplate, midaserr);

				SetDlgItemText (hwndDlg, IDC_ERRORMESSAGE, messyText);
			}
			return TRUE;

		case WM_COMMAND:
			if (wParam == IDOK ||
				wParam == IDC_NOSOUND ||
				wParam == IDQUIT) {
				EndDialog (hwndDlg, wParam);
				return TRUE;
			}
			break;
	}
	return FALSE;
}


void I_InitSound (void)
{
	int needchannels;
	unsigned i;

	// [RH] Parse any SNDINFO lumps
	//		(This really isn't the place for this.)
	S_ParseSndInfo();

	memset (channels, 0, NUM_CHANNELS * sizeof(MIDASsamplePlayHandle));

	I_InitMusic();

	/* Get command line options: */
	wavonly = !!M_CheckParm("-wavonly");
	primarysound = !!M_CheckParm("-primarysound");
	nosound = !!M_CheckParm("-nosfx");
	
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
	
	while (!MIDASinit()) {
		// [RH] If MIDAS can't be initialized, give the user some
		//		choices other than quit.
		switch (DialogBox (hInstance,
						   MAKEINTRESOURCE(IDD_MIDASINITERROR),
						   (HWND)Window,
						   (DLGPROC)InitBoxCallback)) {
			case IDC_NOSOUND:
				MIDASsetOption (MIDAS_OPTION_FORCE_NO_SOUND, TRUE);
				break;

			case IDQUIT:
				I_Quit ();
				break;
		}
	}

	MidasInited = true;

	if ( !MIDASstartBackgroundPlay(100) )
		MIDASerror();

	needchannels = (int)(snd_channels->value);

	// [RH] Allocate 64 channels for MOD music
	if (!MIDASopenChannels (needchannels + (M_CheckParm ("-nomusic") ? 0 : 64)))
		MIDASerror();

	// [RH] Let MIDAS decide what channels to play sfx on.
	if (!MIDASallocAutoEffectChannels (needchannels))
		MIDASerror ();

	/* Simply preload all sounds and throw them at MIDAS: */
	Printf("I_InitSound: Loading all sounds\n");
	for ( i = 1; i < NUMSFX; i++ )
	{
		if ( !S_sfx[i].link )
		{
			/* Fine, not an alias, just load it */
			S_sfx[i].data = getsfx(S_sfx[i].name, &lengths[i], &frequencies[i], &S_sfx[i].ms);
			/* getsfx() actually returns a MIDAS sample handle */
		}
		else
		{
			/* An alias sound */
			S_sfx[i].data = S_sfx[i].link->data;
			lengths[i] = lengths[(S_sfx[i].link - S_sfx)/sizeof(sfxinfo_t)];
		}
	}

	// [RH] Don't play sfx really quiet.
	//		Use less amplification for each new channel added.
	MIDASsetAmplification ((int)(log((double)needchannels) * 144));

	// Finished initialization.
	Printf("I_InitSound: sound module ready\n");	
}




void I_ShutdownSound(void)
{
	unsigned i;

	if (MidasInited) {
		Printf("I_ShutdownSound: Stopping sounds\n");
		for (i = 0; i < NUM_CHANNELS; i++ ) {
			if (channels[i]) {
				if (!MIDASstopSample (channels[i]))
					MIDASerror ();
				channels[i] = 0;
			}
		}

		Printf("I_ShutdownSound: Uninitializing MIDAS\n");

		I_ShutdownMusic();
		if (!MIDASfreeAutoEffectChannels ())
			MIDASerror ();
		if ( !MIDASstopBackgroundPlay() )
			MIDASerror();
// [RH] This seems to cause problems with MIDASclose().
//		if ( !MIDAScloseChannels() )
//			MIDASerror();
	}
	if ( !MIDASclose() )
		MIDASerror();
}
