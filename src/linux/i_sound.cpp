/*		midassound.c
 *
 * A Sound module for DOOM using MIDAS Digital Audio System. Based on
 * original i_sound.c from the source distribution.
 * 
 * Petteri Kangaslampi, pekangas@sci.fi
 *
 * [RH] Changed most of this with features from Hexen and Quake.
 *		Note to self: copy this to the djgpp dir when I make changes.
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

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include "resource.h"
extern HWND Window;
#endif

#ifndef NOMIDAS
/* We'll need to go below the MIDAS API level a bit */
#include <midas/src/midas/midas.h>
#include <midas/include/midasdll.h>
#endif

#if defined(DJGPP) && !defined(NOMIDAS)
#define TRUE 1		// Make the MIDAS headers happy (and avoid redefining BOOL)
#define FALSE 0
typedef unsigned long DWORD;
#endif

#include "doomtype.h"

#ifdef UNIX
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "m_alloc.h"
#include <math.h>


#include "wave.h"
#include "m_swap.h"
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

#ifdef _WIN32
extern HINSTANCE hInstance;
#endif


BOOL MidasInited;

// [RH] The mixing rate for the sounds.
CVAR (snd_samplerate, "44100", CVAR_ARCHIVE)

// killough 2/21/98: optionally use varying pitched sounds
CVAR (snd_pitched, "0", CVAR_ARCHIVE)
#define PITCH(f,x) (snd_pitched.value ? ((f)*(x))/128 : (f))

// Maps sfx channels onto MIDAS channels
#ifndef NOMIDAS
static struct ChanMap
{
	MIDASsamplePlayHandle	playHandle;		// MIDAS sample play handle
	int						soundID;		// sfx playing on this channel
	DWORD					midasChannel;	// MIDAS channel this one maps to
} *ChannelMap;

#ifdef _WIN32
static int wavonly = 0;
static int primarysound = 0;
#endif
static int numChannels;

void MIDASerror(void)
{
	I_FatalError ("MIDAS Error: %s", MIDASgetErrorMessage(MIDASgetLastError()));
}
#endif	// !NOMIDAS

int _nosound = 0;
static float volmul;


/* Loads a sound and adds it to MIDAS
 * Really returns a MIDAS sample handle */
static void getsfx (sfxinfo_t *sfx)
{
#ifndef NOMIDAS
	char				sndtemp[128];
	byte				*sfxdata;
	byte				*sfxcopy;
	int 				size;
	
	int					i;
	int 				error;
	static sdSample		smp;

	// Get the sound data from the WAD and register it with MIDAS

	// If the sound doesn't exist, try a generic male sound (if
	// this is a player sound) or the empty sound.
	if (sfx->lumpnum == -1) {
		char *basename;
		int sfx_id;

		if (!strnicmp (sfx->name, "player/", 7) &&
			 (basename = strchr (sfx->name + 7, '/'))) {
			sprintf (sndtemp, "player/male/%s", basename+1);
			sfx_id = S_FindSound (sndtemp);
			if (sfx_id != -1)
				sfx->lumpnum = S_sfx[sfx_id].lumpnum;
		}

		if (sfx->lumpnum == -1)
badwave:
			sfx->lumpnum = W_GetNumForName ("dsempty");
	}
	
	// See if there is another sound already initialized with this lump. If so,
	// then set this one up as a link, and don't load the sound again.
	for (i = 0; i < numsfx; i++)
		if (S_sfx[i].data && !S_sfx[i].link && S_sfx[i].lumpnum == sfx->lumpnum) {
			DPrintf ("Linked to %s (%d)\n", S_sfx[i].name, i);
			sfx->link = S_sfx + i;
			sfx->ms = S_sfx[i].ms;
			sfx->data = S_sfx[i].data;
			sfx->normal = S_sfx[i].normal;
			sfx->looping = S_sfx[i].looping;
			return;
		}

	size = W_LumpLength (sfx->lumpnum);
	if (size == 0)
		goto badwave;

	sfxdata = (byte *)W_CacheLumpNum (sfx->lumpnum, PU_CACHE);

	/* Add sound to MIDAS: */
	/* A hack below API level - yeahyeah, we should add support for preparing
	   samples from memory to the API */
	
	if (LONG(*((unsigned int *)sfxdata)) == ID_RIFF) {
		// RIFF WAVE sound
		byte *sfx_p, *sfxend, *wavedata;
		fmt_t fmtchunk;
		unsigned datalen = 0;
		

		if (LONG(((int *)sfxdata)[1]) > size - 8) {
			Printf (PRINT_HIGH, "%s: lump is too short\n", sfx->name);
			goto badwave;
		}
		sfxend = sfxdata + LONG(((int *)sfxdata)[1]) + 8;
		if (LONG(((unsigned int *)sfxdata)[2]) != ID_WAVE) {
			Printf (PRINT_HIGH, "%s: not a WAVE file\n", sfx->name);
			goto badwave;
		}

		sfx_p = sfxdata + 4*3;
		fmtchunk.FormatTag = ~0;
		wavedata = NULL;

		while (sfx_p < sfxend) {
			unsigned int chunkid = LONG(((unsigned int *)sfx_p)[0]);
			unsigned int chunklen = LONG(((unsigned int *)sfx_p)[1]);
			sfx_p += 4*2;
			if (chunkid == ID_fmt) {
				if (chunklen < sizeof(fmtchunk)) {
					fmtchunk.FormatTag = ~1;	// Signal that the chunk was too short,
					continue;		// but continue in case there is another good one
				}
				memcpy (&fmtchunk, sfx_p, sizeof(fmtchunk));
				fmtchunk.FormatTag = SHORT(fmtchunk.FormatTag);
				fmtchunk.Channels = SHORT(fmtchunk.Channels);
				fmtchunk.SamplesPerSec = LONG(fmtchunk.SamplesPerSec);
				fmtchunk.AvgBytesPerSec = LONG(fmtchunk.AvgBytesPerSec);
				fmtchunk.BlockAlign = SHORT(fmtchunk.BlockAlign);
			} else if (chunkid == ID_data) {
				if (fmtchunk.FormatTag == WAVE_FMT_PCM) {
					wavedata = sfx_p;
					datalen = chunklen;
					break;
				} else if (fmtchunk.FormatTag == ~0) {
					Printf (PRINT_HIGH, "%s: no fmt chunk\n", sfx->name);
				} else if (fmtchunk.FormatTag == ~1) {
					Printf (PRINT_HIGH, "%s: fmt chunk too short\n", sfx->name);
				} else {
					Printf (PRINT_HIGH, "%s: unknown format %u\n", sfx->name, fmtchunk.FormatTag);
				}
				goto badwave;
			}
			sfx_p += chunklen;
		}

		if (!wavedata || datalen == 0) {
			Printf (PRINT_HIGH, "%s: no data chunk\n", sfx->name);
			goto badwave;
		}

		if (fmtchunk.Channels > 2) {
			Printf (PRINT_HIGH, "%s: too many channels\n", sfx->name);
			goto badwave;
		}

		if (fmtchunk.Channels < 1) {
			Printf (PRINT_HIGH, "%s: no channels\n", sfx->name);
			goto badwave;
		}

		if (fmtchunk.BlockAlign != 1 &&
			fmtchunk.BlockAlign != 2 &&
			fmtchunk.BlockAlign != 4) {
			Printf (PRINT_HIGH, "%s: bad blockalign (%d)\n", fmtchunk.BlockAlign);
			goto badwave;
		}

		/* Build Sound Device sample structure for the sample: */
		smp.sample = wavedata;
		smp.samplePos = sdSmpConv;
		if (fmtchunk.Channels == 1)
			smp.sampleType = fmtchunk.BlockAlign == 1 ? smp8bitMono : smp16bitMono;
		else
			smp.sampleType = fmtchunk.BlockAlign == 2 ? smp8bitStereo : smp16bitStereo;
		smp.sampleLength = datalen;

		sfx->frequency = fmtchunk.SamplesPerSec;
		sfx->length = datalen;
		sfx->ms = datalen / fmtchunk.BlockAlign;
	} else {
		// DMX sound
		/* Build Sound Device sample structure for the sample: */
		smp.sample = sfxdata+8;
		smp.samplePos = sdSmpConv;
		smp.sampleType = MIDAS_SAMPLE_8BIT_MONO;
		smp.sampleLength = size-8;

		sfx->frequency = ((unsigned short *)sfxdata)[1];	// Extract sample rate from the sound header
		sfx->ms = sfx->length = ((unsigned int *)sfxdata)[1];
		if ((signed)sfx->length > size - 8) {
			Printf (PRINT_HIGH, "%s is missing %d bytes\n", sfx->name, sfx->length - size + 8);
			sfx->ms = sfx->length = size - 8;
		}
	}

	/* No loop 2: */
	smp.loop2Start = smp.loop2End = 0;
	smp.loop2Type = loopNone;

	sfxcopy = (byte *)Malloc (smp.sampleLength);
	memcpy (sfxcopy, smp.sample, smp.sampleLength);
	Z_Free (sfxdata);
	smp.sample = sfxcopy;

	/* Add the sample to the Sound Device: */
	{
		// Avoid using __fastcall for this function
		typedef int (STACK_ARGS *blargh_t)(sdSample*, int, unsigned *);
		blargh_t blargh = (blargh_t)midasSD->AddSample;

		/* No loop: */
		smp.loopMode = sdLoopNone;
		smp.loop1Start = smp.loop1End = 0;
		smp.loop1Type = loopNone;

		if ( (error = blargh (&smp, 0, (unsigned *)&sfx->normal)) != OK)
			I_FatalError ("getsfx: AddSample failed: %s", MIDASgetErrorMessage(error));

		/* With loop: */
		smp.loopMode = sdLoop1;
		smp.loop1Start = 0;
		smp.loop1End = smp.sampleLength;
		smp.loop1Type = loopUnidir;

		if ( (error = blargh (&smp, 0, (unsigned *)&sfx->looping)) != OK)
			I_FatalError ("getsfx: AddSample failed: %s", MIDASgetErrorMessage(error));
	}

	// Remove the cached lump.
	
	if (sfx->frequency == 0)
		sfx->frequency = 11025;
	sfx->ms = (sfx->ms * 1000) / (sfx->frequency);
	sfx->data = sfxcopy;
#endif	// !NOMIDAS
}





//
// SFX API
//
void I_SetChannels (int numchannels)
{
#ifndef NOMIDAS
	int i;

	if (_nosound)
		return;

	if (!MIDASopenChannels (numchannels + (Args.CheckParm ("-nomusic") ? 0 : 64)))
		MIDASerror();

	ChannelMap = (ChanMap *)Z_Malloc (numchannels * sizeof(*ChannelMap), PU_STATIC, 0);
	for (i = 0; i < numchannels; i++)
	{
		if (MIDAS_ILLEGAL_CHANNEL ==
			(ChannelMap[i].midasChannel = MIDASallocateChannel ()))
		{
			MIDASerror ();
		}
		ChannelMap[i].playHandle = 0;
		ChannelMap[i].soundID = -1;
	}

	// Don't play sfx really quiet.
	// Use less amplification for each new channel added.
	MIDASsetAmplification ((int)(log((double)numchannels) * 144));

	numChannels = numchannels;
#endif	// !NOMIDAS
}

 
void I_SetSfxVolume (int volume)
{
	// volume range is 0-15
	// volmul range is 0-1.whatever

	volmul = (float)((volume / 15.0) * (256.0 / 255.0));
}


//
// Starting a sound means adding it
//		to the current list of active sounds
//		in the internal channels.
// As the SFX info struct contains
//		e.g. a pointer to the raw data,
//		it is ignored.
//
int I_StartSound (sfxinfo_t *sfx, int vol, int sep, int pitch, int channel, BOOL looping)
{
#ifndef NOMIDAS
	if (_nosound)
		return 0;

	int id = sfx - S_sfx;
	int volume;
	int pan;

	/* Calculate MIDAS volume and panning from volume and separation: */
	volume = ((int)(vol*volmul))/4;   /* original range 0-255, MIDAS range 0-64 */
	if ( volume > 64 ) volume = 64;
	else if ( volume < 0 ) volume = 0;

	if (sep < 0) {
		pan = MIDAS_PAN_SURROUND;
	} else {
		pan = (sep-128) / 2;  /* original 0-255, MIDAS -64-64 */

		/* Clamp: */
		if ( pan < MIDAS_PAN_LEFT) pan = MIDAS_PAN_LEFT;
		else if ( pan > MIDAS_PAN_RIGHT) pan = MIDAS_PAN_RIGHT;
	}

	ChannelMap[channel].playHandle = MIDASplaySample (
									 looping ? (MIDASsample)sfx->looping
										: (MIDASsample)sfx->normal,
									 ChannelMap[channel].midasChannel,
									 0,
									 PITCH(sfx->frequency,pitch),
									 volume,
									 pan);
	ChannelMap[channel].soundID = id;

	return channel+1;
#else
	return 0;
#endif	// !NOMIDAS
}



void I_StopSound (int handle)
{
#ifndef NOMIDAS
	if (_nosound)
		return;

	handle--;
	if (ChannelMap[handle].playHandle)
	{
		if (!MIDASstopSample (ChannelMap[handle].playHandle))
			MIDASerror();

		ChannelMap[handle].playHandle = 0;
	}
#endif	// !NOMIDAS
}


int I_SoundIsPlaying (int handle)
{
#ifndef NOMIDAS
	int is;

	if (_nosound)
		return 0;

	handle--;
	if (!ChannelMap[handle].playHandle)
		return 0;
	else if (!(is = (int)MIDASgetSamplePlayStatus (ChannelMap[handle].playHandle)))
		ChannelMap[handle].playHandle = 0;

	return is;
#else
	return 0;
#endif	// !NOMIDAS
}


void I_UpdateSoundParams (int handle, int vol, int sep, int pitch)
{
#ifndef NOMIDAS
	int mvol, mpan;
	
	if (_nosound)
		return;

	handle--;
	if (!ChannelMap[handle].playHandle)
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
		MIDASsamplePlayHandle sample = ChannelMap[handle].playHandle;

		if ( !MIDASsetSampleVolume(sample, mvol) ||
			 !MIDASsetSamplePanning(sample, mpan) ||
			 !MIDASsetSampleRate(sample, PITCH(S_sfx[ChannelMap[handle].soundID].frequency,pitch)) )
			MIDASerror();
	}
#endif	// !NOMIDAS
}


void I_LoadSound (struct sfxinfo_struct *sfx)
{
#ifndef NOMIDAS
	if (_nosound)
		return;

	if (!sfx->data)
	{
		int i = sfx - S_sfx;
		DPrintf ("loading sound \"%s\" (%d)\n", sfx->name, i);
		getsfx (sfx);
	}
#endif	// !NOMIDAS
}


#if defined(_WIN32) && !defined(NOMIDAS)

// [RH] Dialog procedure for the error dialog that appears if MIDAS
//		could not be initialized for some reason.
BOOL CALLBACK InitBoxCallback (HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	static const char *messyTemplate =
		"MIDAS could not be initialized.\r\n"
		"(Reason: %s.)\r\n\r\n"
		"If another program is using the sound card, you "
		"can try stopping it and clicking \"Retry.\"\r\n\r\n"
		"Otherwise, you can either click \"No Sound\" to use "
		"ZDoom without sound or click \"Quit\" if you don't "
		"really want to play ZDoom.";

	switch (message)
	{
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
			wParam == IDQUIT)
		{
			EndDialog (hwndDlg, wParam);
			return TRUE;
		}
		break;
	}
	return FALSE;
}
#endif

void I_InitSound (void)
{
	/* Get command line options: */
#ifdef _WIN32
	wavonly = !!Args.CheckParm ("-wavonly");
	primarysound = !!Args.CheckParm ("-primarysound");
#endif
	_nosound = !!Args.CheckParm ("-nosfx") || !!Args.CheckParm ("-nosound");

#ifdef NOMIDAS
	_nosound = true;
#else

#ifdef UNIX
	seteuid (0);
	if (geteuid ())
	    _nosound = true;
#endif

#ifndef DJGPP
	if (!_nosound)	// DOS requires MIDAS for its timer
#endif
	{
		Printf (PRINT_HIGH, "I_InitSound: Initializing MIDAS\n");

		MIDASstartup();
		
		MIDASsetOption(MIDAS_OPTION_MIXRATE, (int)snd_samplerate.value);
		MIDASsetOption(MIDAS_OPTION_MIXBUFLEN, 50);

#ifdef _WIN32
		if (!wavonly)
		{
			MIDASsetOption (MIDAS_OPTION_DSOUND_HWND, (DWORD)Window);
			MIDASsetOption (MIDAS_OPTION_DSOUND_MODE,
				primarysound ? MIDAS_DSOUND_PRIMARY : MIDAS_DSOUND_STREAM);
		}
#endif

		//if (_nosound)
		//	MIDASsetOption(MIDAS_OPTION_FORCE_NO_SOUND, TRUE);	

#ifdef DJGPP
		if (Args.CheckParm ("-m"))
			MIDASconfig ();
#endif

		while (!MIDASinit())
		{
#ifdef _WIN32
			// If MIDAS can't be initialized, give the user some
			// choices other than quit.
			switch (DialogBox (hInstance,
							   MAKEINTRESOURCE(IDD_MIDASINITERROR),
							   (HWND)Window,
							   (DLGPROC)InitBoxCallback))
			{
				case IDC_NOSOUND:
					MIDASclose ();
					_nosound = true;
					break;

				case IDQUIT:
					exit (0);
					break;
			}
#else
			static int errorcount = 0;
			if (!errorcount)
			{
				errorcount++;
				Printf (PRINT_HIGH, "Sound init error: %s\nUsing no sound\n",
					MIDASgetErrorMessage (MIDASgetLastError ()));
				MIDASsetOption (MIDAS_OPTION_FORCE_NO_SOUND, TRUE);
				_nosound = true;
				if (Args.CheckParm ("-m") == 0)
					Printf (PRINT_HIGH, "(Try running with the -m parameter.)\n");
			}
			else
			{
				exit (0);
			}
#endif
		}
	}

	if (!_nosound)
	{
		MidasInited = true;
	}
#ifdef _WIN32
	if (!_nosound)
#endif
	{
		atterm (I_ShutdownSound);
	}

	I_InitMusic();

	if (!_nosound && !MIDASstartBackgroundPlay(100))
		MIDASerror ();

#ifdef UNIX
	seteuid (getuid ());
#endif

#endif	// !NOMIDAS

	// Finished initialization.
	Printf (PRINT_HIGH, "I_InitSound: sound module ready\n");	
}

void STACK_ARGS I_ShutdownSound (void)
{
#ifndef NOMIDAS
	int i, c = 0;
	size_t len = 0;

	if (MidasInited)
	{
		if (ChannelMap)
		{
			for (i = 0; i < numChannels; i++ )
			{
				if (ChannelMap[i].playHandle)
				{
					if (!MIDASstopSample (ChannelMap[i].playHandle))
						MIDASerror ();
				}
			}
			Z_Free (ChannelMap);
			ChannelMap = NULL;
		}

		// [RH] Free all loaded samples
		for (i = 0; i < numsfx; i++)
		{
			if (!S_sfx[i].link)
			{
				if (S_sfx[i].normal)
				{
					MIDASfreeSample ((MIDASsample)S_sfx[i].normal);
					len += S_sfx[i].length;
					c++;
				}
				if (S_sfx[i].looping)
				{
					MIDASfreeSample ((MIDASsample)S_sfx[i].looping);
				}
				if (S_sfx[i].data)
				{
					free (S_sfx[i].data);
				}
			}
			S_sfx[i].data = S_sfx[i].link = NULL;
		}

		if ( !MIDASstopBackgroundPlay() )
			MIDASerror();

// [RH] This seems to cause problems with MIDASclose().
//		if ( !MIDAScloseChannels() )
//			MIDASerror();
	}
	if (!MIDASclose())
		MIDASerror();
#endif	// !NOMIDAS
}
