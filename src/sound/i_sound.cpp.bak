/*
** i_sound.cpp
** System interface for sound; uses fmod.dll
**
**---------------------------------------------------------------------------
** Copyright 1998-2001 Randy Heit
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

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include "resource.h"
extern HWND Window;
extern HINSTANCE g_hInst;
#else
#define FALSE 0
#define TRUE 1
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "doomtype.h"
#include "m_alloc.h"
#include <math.h>


#include <fmod.h>

#include "m_swap.h"
#include "z_zone.h"
#include "stats.h"

#include "c_cvars.h"
#include "c_dispatch.h"
#include "i_system.h"
#include "i_sound.h"
#include "i_music.h"
#include "m_argv.h"
#include "m_misc.h"
#include "w_wad.h"
#include "i_video.h"
#include "s_sound.h"

#include "doomdef.h"

#define SCALE3D		96.f
#define INVSCALE3D	(1.f/SCALE3D)
#define ROLLOFF3D	0.5f
#define DOPPLER3D	1.f

extern void CalcPosVel (fixed_t *pt, AActor *mover, int constz, float pos[3], float vel[3]);

static const char *OutputNames[] =
{
	"No sound",
	"Windows Multimedia",
	"DirectSound",
	"A3D",
	"OSS (Open Sound System)",
	"ESD (Enlightenment Sound Daemon)",
	"ALSA (Advanced Linux Sound Architecture)"
};
static const char *MixerNames[] =
{
	"Auto",
	"Non-MMX blendmode",
	"Pentium MMX",
	"PPro MMX",
	"Quality auto",
	"Quality FPU",
	"Quality Pentium MMX",
	"Quality PPro MMX"
};

EXTERN_CVAR (Float, snd_sfxvolume)
CVAR (Int, snd_samplerate, 44100, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Int, snd_buffersize, 0, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Int, snd_driver, 0, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (String, snd_output, "default", CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, snd_3d, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, snd_waterreverb, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, snd_fpumixer, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

// killough 2/21/98: optionally use varying pitched sounds
CVAR (Bool, snd_pitched, false, CVAR_ARCHIVE)
#define PITCH(f,x) (snd_pitched ? ((f)*(x))/128 : (f))

// Maps sfx channels onto FMOD channels
static struct ChanMap
{
	int soundID;		// sfx playing on this channel
	long channelID;
	bool bIsLooping;
	bool bIs3D;
	unsigned int lastPos;
} *ChannelMap;

int _nosound = 0;
bool Sound3D;

static int numChannels;
static unsigned int DriverCaps;
static int OutputType;

static bool SoundDown = true;

#if 0
static const char *FmodErrors[] =
{
	"No errors",
	"Cannot call this command after FSOUND_Init.  Call FSOUND_Close first.",
	"This command failed because FSOUND_Init was not called",
	"Error initializing output device.",
	"Error initializing output device, but more specifically, the output device is already in use and cannot be reused.",
	"Playing the sound failed.",
	"Soundcard does not support the features needed for this soundsystem (16bit stereo output)",
	"Error setting cooperative level for hardware.",
	"Error creating hardware sound buffer.",
	"File not found",
	"Unknown file format",
	"Error loading file",
	"Not enough memory ",
	"The version number of this file format is not supported",
	"Incorrect mixer selected",
	"An invalid parameter was passed to this function",
	"Tried to use a3d and not an a3d hardware card, or dll didnt exist, try another output type.",
	"Tried to use an EAX command on a non EAX enabled channel or output.",
	"Failed to allocate a new channel"
};
#endif

// Simple file loader for FSOUND using an already-opened
// file handle. Also supports loading from a WAD opened
// in w_wad.cpp.

static unsigned int _cdecl FIO_Open (const char *name)
{
	return (unsigned int)name;
}

static void _cdecl FIO_Close (unsigned int handle)
{
	if (handle)
	{
		delete (FileHandle *)handle;
	}
}

static int _cdecl FIO_Read (void *buffer, int size, unsigned int handle)
{
	if (handle)
	{
		FileHandle *file = (FileHandle *)handle;
		if (size + file->pos > file->len)
			size = file->len - file->pos;
		if (size < 0)
		{
			size = 0;
		}
		else if (size > 0)
		{
			if (lseek (file->handle, file->base + file->pos, SEEK_SET) == -1)
				return 0;
			size = read (file->handle, buffer, size);
			file->pos += size;
		}
		return size;
	}
	return 0;
}

static int _cdecl FIO_Seek (unsigned int handle, int pos, signed char mode)
{
	if (handle)
	{
		FileHandle *file = (FileHandle *)handle;

		switch (mode)
		{
		case SEEK_SET:
			file->pos = pos;
			break;
		case SEEK_CUR:
			file->pos += pos;
			break;
		case SEEK_END:
			file->pos = file->len + pos;
			break;
		}
		if (file->pos < 0)
		{
			file->pos = 0;
			return -1;
		}
		if (file->pos > file->len)
		{
			file->pos = file->len;
			return -1;
		}
		return 0;
	}
	return -1;
}

static int _cdecl FIO_Tell (unsigned int handle)
{
	return handle ? ((FileHandle *)handle)->pos : 0;
}

void Enable_FSOUND_IO_Loader ()
{
	typedef unsigned int	(_cdecl *OpenCallback)(const char *name);
	typedef void			(_cdecl *CloseCallback)(unsigned int handle);
	typedef int				(_cdecl *ReadCallback)(void *buffer, int size, unsigned int handle);
	typedef int				(_cdecl *SeekCallback)(unsigned int handle, int pos, signed char mode);
	typedef int				(_cdecl *TellCallback)(unsigned int handle);
	
	FSOUND_File_SetCallbacks	
		((OpenCallback)FIO_Open,
		 (CloseCallback)FIO_Close,
		 (ReadCallback)FIO_Read,
		 (SeekCallback)FIO_Seek,
		 (TellCallback)FIO_Tell);
}

void Disable_FSOUND_IO_Loader ()
{
	FSOUND_File_SetCallbacks (NULL, NULL, NULL, NULL, NULL);
}

// FSOUND_Sample_Upload seems to mess up the signedness of sound data when
// uploading to hardware buffers. The pattern is not particularly predictable,
// so this is a replacement for it that loads the data manually. Source data
// is mono, unsigned, 8-bit. Output is mono, signed, 8- or 16-bit.

static int PutSampleData (FSOUND_SAMPLE *sample, BYTE *data, int len,
	unsigned int mode)
{
	if (mode & FSOUND_2D)
	{
		return FSOUND_Sample_Upload (sample, data,
			FSOUND_8BITS|FSOUND_MONO|FSOUND_UNSIGNED);
	}
	else if (FSOUND_Sample_GetMode (sample) & FSOUND_8BITS)
	{
		void *ptr1, *ptr2;
		unsigned int len1, len2;

		if (FSOUND_Sample_Lock (sample, 0, len, &ptr1, &ptr2, &len1, &len2))
		{
			int i;
			BYTE *ptr;
			int len;

			for (i = 0, ptr = (BYTE *)ptr1, len = len1;
				 i < 2 && ptr && len;
				i++, ptr = (BYTE *)ptr2, len = len2)
			{
				int j;
				for (j = 0; j < len; j++)
				{
					ptr[j] = *data++ - 128;
				}
			}
			FSOUND_Sample_Unlock (sample, ptr1, ptr2, len1, len2);
			return TRUE;
		}
		else
		{
			return FALSE;
		}
	}
	else
	{
		void *ptr1, *ptr2;
		unsigned int len1, len2;

		if (FSOUND_Sample_Lock (sample, 0, len*2, &ptr1, &ptr2, &len1, &len2))
		{
			int i;
			SWORD *ptr;
			int len;

			for (i = 0, ptr = (SWORD *)ptr1, len = len1/2;
				 i < 2 && ptr && len;
				i++, ptr = (SWORD *)ptr2, len = len2/2)
			{
				int j;
				for (j = 0; j < len; j++)
				{
					ptr[j] = ((*data<<8)|(*data)) - 32768;
					data++;
				}
			}
			FSOUND_Sample_Unlock (sample, ptr1, ptr2, len1, len2);
			return TRUE;
		}
		else
		{
			return FALSE;
		}
	}
	return TRUE;
}

static void DoLoad (void **slot, sfxinfo_t *sfx)
{
	int size;
	int errcount;
	unsigned long samplemode;

	samplemode = Sound3D ? FSOUND_HW3D : FSOUND_2D;

	errcount = 0;
	while (errcount < 2)
	{
		if (errcount)
			sfx->lumpnum = W_GetNumForName ("dsempty");

		size = W_LumpLength (sfx->lumpnum);
		if (size == 0)
		{
			errcount++;
			continue;
		}

		byte *sfxdata = (byte *)W_CacheLumpNum (sfx->lumpnum, PU_CACHE);
		SDWORD len = ((SDWORD *)sfxdata)[1];

		// If the sound is raw, just load it as such.
		// Otherwise, try the sound as DMX format.
		// If that fails, let FMOD try and figure it out.
		if (sfx->bLoadRAW ||
			(((BYTE *)sfxdata)[0] == 3 && ((BYTE *)sfxdata)[1] == 0 && len <= size - 8))
		{
			FSOUND_SAMPLE *sample;
			BYTE *sfxstart;
			unsigned int bits;

			if (sfx->bLoadRAW)
			{
				len = W_LumpLength (sfx->lumpnum);
				sfx->frequency = (sfx->bForce22050 ? 22050 : 11025);
				sfxstart = sfxdata;
			}
			else
			{
				sfx->frequency = ((WORD *)sfxdata)[1];
				if (sfx->frequency == 0)
				{
					sfx->frequency = 11025;
				}
				sfxstart = sfxdata + 8;
			}
			sfx->ms = sfx->length = len;

			bits = FSOUND_8BITS;
			do
			{
				sample = FSOUND_Sample_Alloc (FSOUND_FREE, len,
					samplemode|bits|FSOUND_LOOP_OFF|FSOUND_MONO,
					sfx->frequency, 255, FSOUND_STEREOPAN, 255);
			} while (sample == NULL && (bits <<= 1) == FSOUND_16BITS);

			if (sample == NULL)
			{
				DPrintf ("Failed to allocate sample: %d\n", FSOUND_GetError ());
				errcount++;
				continue;
			}

			if (!PutSampleData (sample, sfxstart, len, samplemode))
			{
				DPrintf ("Failed to upload sample: %d\n", FSOUND_GetError ());
				FSOUND_Sample_Free (sample);
				sample = NULL;
				errcount++;
				continue;
			}
			*slot = sample;
		}
		else
		{
			*slot = FSOUND_Sample_Load (FSOUND_FREE, (char *)sfxdata,
				samplemode|FSOUND_LOADMEMORY, size);
			if (*slot != NULL)
			{
				int probe;

				FSOUND_Sample_GetDefaults ((FSOUND_SAMPLE *)sfx->data, &probe,
					NULL, NULL, NULL);

				sfx->frequency = probe;
				sfx->ms = FSOUND_Sample_GetLength ((FSOUND_SAMPLE *)sfx->data);
				sfx->length = sfx->ms;
			}
		}
		break;
	}

	if (sfx->data)
	{
		sfx->ms = (sfx->ms * 1000) / (sfx->frequency);
		DPrintf ("sound loaded: %d Hz %d samples\n", sfx->frequency, sfx->length);
	}
}

static void getsfx (sfxinfo_t *sfx)
{
	unsigned int i;

	// Get the sound data from the WAD and register it with sound library

	// If the sound doesn't exist, replace it with the empty sound.
	if (sfx->lumpnum == -1)
	{
		sfx->lumpnum = W_GetNumForName ("dsempty");
	}
	
	// See if there is another sound already initialized with this lump. If so,
	// then set this one up as a link, and don't load the sound again.
	for (i = 0; i < S_sfx.Size (); i++)
	{
		if (S_sfx[i].data && S_sfx[i].link == sfxinfo_t::NO_LINK && S_sfx[i].lumpnum == sfx->lumpnum)
		{
			DPrintf ("Linked to %s (%d)\n", S_sfx[i].name, i);
			sfx->link = i;
			sfx->ms = S_sfx[i].ms;
			sfx->data = S_sfx[i].data;
			sfx->altdata = S_sfx[i].altdata;
			return;
		}
	}

	sfx->bHaveLoop = false;
	sfx->normal = 0;
	sfx->looping = 0;
	sfx->altdata = NULL;
	DoLoad (&sfx->data, sfx);
}


// Right now, FMOD's biggest shortcoming compared to MIDAS is that it does not
// support multiple samples with the same sample data. Thus, if we want to
// play a looped and non-looped version of the same sound, we need to create
// two copies of it. Fortunately, most sounds will either be played looped or
// not, but not both at the same time, so this really isn't too much of a
// problem. This function juggles the sample between looping and non-looping,
// creating a copy if necessary. It also increments the appropriate use
// counter.
//
// Update: FMOD 3.3 added FSOUND_SetLoopMode to set a channel's looping status,
// but that only works with software channels. So I think I will continue to
// do this even though I don't *have* to anymore.

static FSOUND_SAMPLE *CheckLooping (sfxinfo_t *sfx, BOOL looped)
{
	if (looped)
	{
		sfx->looping++;
		if (sfx->bHaveLoop)
		{
			return (FSOUND_SAMPLE *)(sfx->altdata ? sfx->altdata : sfx->data);
		}
		else
		{
			if (sfx->normal == 0)
			{
				sfx->bHaveLoop = true;
				FSOUND_Sample_SetMode ((FSOUND_SAMPLE *)sfx->data, FSOUND_LOOP_NORMAL);
				return (FSOUND_SAMPLE *)sfx->data;
			}
		}
	}
	else
	{
		sfx->normal++;
		if (sfx->altdata || !sfx->bHaveLoop)
		{
			return (FSOUND_SAMPLE *)sfx->data;
		}
		else
		{
			if (sfx->looping == 0)
			{
				sfx->bHaveLoop = false;
				FSOUND_Sample_SetMode ((FSOUND_SAMPLE *)sfx->data, FSOUND_LOOP_OFF);
				return (FSOUND_SAMPLE *)sfx->data;
			}
		}
	}

	// If we get here, we need to create an alternate version of the sample.
	FSOUND_Sample_SetMode ((FSOUND_SAMPLE *)sfx->data, FSOUND_LOOP_OFF);
	DoLoad (&sfx->altdata, sfx);
	FSOUND_Sample_SetMode ((FSOUND_SAMPLE *)sfx->altdata, FSOUND_LOOP_NORMAL);
	sfx->bHaveLoop = true;
	return (FSOUND_SAMPLE *)(looped ? sfx->altdata : sfx->data);
}

void UncheckSound (sfxinfo_t *sfx, BOOL looped)
{
	if (looped)
	{
		if (sfx->looping > 0)
			sfx->looping--;
	}
	else
	{
		if (sfx->normal > 0)
			sfx->normal--;
	}
}

//
// SFX API
//

//==========================================================================
//
// CVAR snd_sfxvolume
//
// Maximum volume of a sound effect.
//==========================================================================

CUSTOM_CVAR (Float, snd_sfxvolume, 0.5f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL)
{
	if (self < 0.f)
		self = 0.f;
	else if (self > 1.f)
		self = 1.f;
	else
	{
		FSOUND_SetSFXMasterVolume ((int)(self * 255.f));
		// FMOD apparently resets absolute volume channels when setting master vol
		snd_musicvolume.Callback ();
	}
}

//
// vol range is 0-255
// sep range is 0-255, -1 for surround, -2 for full vol middle
//
long I_StartSound (sfxinfo_t *sfx, int vol, int sep, int pitch, int channel, BOOL looping)
{
	if (_nosound || !ChannelMap)
		return 0;

	int id = sfx - &S_sfx[0];
	long volume;
	long pan;
	long freq;
	long chan;

	if (sep < 0)
	{
		pan = 128; // FSOUND_STEREOPAN is too loud relative to everything else
				   // when we don't want positioned sounds, so use center panning instead.
	}
	else
	{
		pan = sep;
	}

	freq = PITCH(sfx->frequency,pitch);
	volume = vol;

	chan = FSOUND_PlaySoundEx (FSOUND_FREE, CheckLooping (sfx, looping), NULL, true);
	
	if (chan != -1)
	{
		//FSOUND_SetReserved (chan, TRUE);
		FSOUND_SetSurround (chan, sep == -1 ? TRUE : FALSE);
		//printf ("%ld\n", freq);
		FSOUND_SetFrequency (chan, freq);
		FSOUND_SetVolume (chan, vol);
		FSOUND_SetPan (chan, pan);
		FSOUND_SetPaused (chan, false);
		ChannelMap[channel].channelID = chan;
		ChannelMap[channel].soundID = id;
		ChannelMap[channel].bIsLooping = looping ? true : false;
		ChannelMap[channel].lastPos = 0;
		ChannelMap[channel].bIs3D = false;
		return channel + 1;
	}

	DPrintf ("Sound %s failed to play: %d\n", sfx->name, FSOUND_GetError ());
	return 0;
}

long I_StartSound3D (sfxinfo_t *sfx, float vol, int pitch, int channel,
	BOOL looping, float pos[3], float vel[3])
{
	if (_nosound || !Sound3D || !ChannelMap)
		return 0;

	float lpos[3], lvel[3];
	int id = sfx - &S_sfx[0];
	long freq;
	long chan;

	freq = PITCH(sfx->frequency,pitch);

	lpos[0] = pos[0] * INVSCALE3D;
	lpos[1] = pos[1] * INVSCALE3D;
	lpos[2] = pos[2] * INVSCALE3D;
	lvel[0] = vel[0] * INVSCALE3D;
	lvel[1] = vel[1] * INVSCALE3D;
	lvel[2] = vel[2] * INVSCALE3D;

	FSOUND_SAMPLE *sample = CheckLooping (sfx, looping);

	chan = FSOUND_PlaySoundEx (FSOUND_FREE, sample, NULL, true);

	if (chan != -1)
	{
		//FSOUND_SetReserved (chan, TRUE);
		FSOUND_SetFrequency (chan, freq);
		FSOUND_SetVolume (chan, (int)(vol * 255.f));
		FSOUND_3D_SetAttributes (chan, lpos, lvel);
		FSOUND_SetPaused (chan, false);
		ChannelMap[channel].channelID = chan;
		ChannelMap[channel].soundID = id;
		ChannelMap[channel].bIsLooping = looping ? true : false;
		ChannelMap[channel].lastPos = 0;
		ChannelMap[channel].bIs3D = true;
		return channel + 1;
	}

	DPrintf ("Sound %s failed to play: %d (%d)\n", sfx->name, FSOUND_GetError (), FSOUND_GetChannelsPlaying ());
	return 0;
}

void I_StopSound (int handle)
{
	if (_nosound || !handle || !ChannelMap)
		return;

	handle--;
	if (ChannelMap[handle].soundID != -1)
	{
		FSOUND_StopSound (ChannelMap[handle].channelID);
		//FSOUND_SetReserved (ChannelMap[handle].channelID, FALSE);
		UncheckSound (&S_sfx[ChannelMap[handle].soundID], ChannelMap[handle].bIsLooping);
		ChannelMap[handle].soundID = -1;
	}
}


int I_SoundIsPlaying (int handle)
{
	if (_nosound || !handle || !ChannelMap)
		return 0;

	handle--;
	if (ChannelMap[handle].soundID == -1)
	{
		return 0;
	}
	else
	{
		int is;

		// FSOUND_IsPlaying does not work with A3D
		if (OutputType != FSOUND_OUTPUT_A3D)
		{
			is = FSOUND_IsPlaying (ChannelMap[handle].channelID);
		}
		else
		{
			unsigned int pos;
			if (ChannelMap[handle].bIsLooping)
			{
				is = TRUE;
			}
			else
			{
				pos = FSOUND_GetCurrentPosition (ChannelMap[handle].channelID);
				is = pos >= ChannelMap[handle].lastPos &&
					pos <= S_sfx[ChannelMap[handle].soundID].length;
				ChannelMap[handle].lastPos = pos;
			}
		}
		if (!is)
		{
			//FSOUND_SetReserved (ChannelMap[handle].channelID, FALSE);
			UncheckSound (&S_sfx[ChannelMap[handle].soundID], ChannelMap[handle].bIsLooping);
			ChannelMap[handle].soundID = -1;
		}
		return is;
	}
}


void I_UpdateSoundParams (int handle, int vol, int sep, int pitch)
{
	if (_nosound || !handle || !ChannelMap)
		return;

	handle--;
	if (ChannelMap[handle].soundID == -1)
		return;

	long volume;
	long pan;
	long freq;

	freq = PITCH(S_sfx[ChannelMap[handle].soundID].frequency, pitch);
	volume = vol;

	if (sep < 0)
	{
		pan = 128; //FSOUND_STEREOPAN
	}
	else
	{
		pan = sep;
	}

	FSOUND_SetSurround (ChannelMap[handle].channelID, sep == -1 ? TRUE : FALSE);
	FSOUND_SetPan (ChannelMap[handle].channelID, pan);
	FSOUND_SetVolume (ChannelMap[handle].channelID, volume);
	FSOUND_SetFrequency (ChannelMap[handle].channelID, freq);
}

void I_UpdateSoundParams3D (int handle, float pos[3], float vel[3])
{
	if (_nosound || !handle || !ChannelMap)
		return;

	handle--;
	if (ChannelMap[handle].soundID == -1)
		return;

	float lpos[3], lvel[3];
	lpos[0] = pos[0] * INVSCALE3D;
	lpos[1] = pos[1] * INVSCALE3D;
	lpos[2] = pos[2] * INVSCALE3D;
	lvel[0] = vel[0] * INVSCALE3D;
	lvel[1] = vel[1] * INVSCALE3D;
	lvel[2] = vel[2] * INVSCALE3D;
	FSOUND_3D_SetAttributes (ChannelMap[handle].channelID, lpos, lvel);
}

void I_UpdateListener (AActor *listener)
{
	float angle;
	float vel[3];
	float pos[3];

	if (Sound3D && ChannelMap)
	{
		vel[0] = listener->momx * (TICRATE*INVSCALE3D/65536.f);
		vel[1] = listener->momy * (TICRATE*INVSCALE3D/65536.f);
		vel[2] = listener->momz * (TICRATE*INVSCALE3D/65536.f);
		pos[0] = listener->x * (INVSCALE3D/65536.f);
		pos[2] = listener->y * (INVSCALE3D/65536.f);
		pos[1] = listener->z * (INVSCALE3D/65536.f);

		angle = (float)(listener->angle) * ((float)PI / 2147483648.f);

		// Move sounds that are not meant to be heard in 3D so
		// that they remain on top of the listener.

		for (int i = 0; i < numChannels; i++)
		{
			if (ChannelMap[i].soundID != -1 && !ChannelMap[i].bIs3D)
			{
				FSOUND_3D_SetAttributes (ChannelMap[i].channelID,
					pos, vel);
			}
		}

		FSOUND_3D_Listener_SetAttributes (pos, vel,
			cosf (angle), 0.f, sinf (angle), 0.f, 1.f, 0.f);

		if (DriverCaps & FSOUND_CAPS_EAX2)
		{
			static FSOUND_REVERB_PROPERTIES water = FSOUND_PRESET_UNDERWATER;
			static FSOUND_REVERB_PROPERTIES off = FSOUND_PRESET_OFF;
			static bool wasUnderwater = false;
			bool underwater;

			underwater = (listener->waterlevel == 3 && snd_waterreverb);
			if (underwater != wasUnderwater)
			{
				FSOUND_REVERB_PROPERTIES *props = underwater ? &water : &off;

				DPrintf ("Reverb Environment %d\n", props->Environment);
				FSOUND_Reverb_SetProperties (props);
				wasUnderwater = underwater;
			}
		}

		FSOUND_3D_Update ();
	}
}

void I_LoadSound (sfxinfo_t *sfx)
{
	if (_nosound)
		return;

	if (!sfx->data)
	{
		int i = sfx - &S_sfx[0];
		DPrintf ("loading sound \"%s\" (%d)\n", sfx->name, i);
		getsfx (sfx);
	}
}

#ifdef _WIN32
// [RH] Dialog procedure for the error dialog that appears if FMOD
//		could not be initialized for some reason.
BOOL CALLBACK InitBoxCallback (HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_COMMAND:
		if (wParam == IDOK ||
			wParam == IDC_NOSOUND ||
			wParam == IDCANCEL)
		{
			EndDialog (hwndDlg, wParam);
			return TRUE;
		}
		break;
	}
	return FALSE;
}
#endif

static char FModLog (char success)
{
	if (success)
	{
		Printf (" succeeded\n");
	}
	else
	{
		Printf (" failed (error %d)\n", FSOUND_GetError());
	}
	return success;
}

void I_InitSound ()
{
#ifdef _WIN32
	static const FSOUND_OUTPUTTYPES outtypes[2] =
	{ FSOUND_OUTPUT_DSOUND, FSOUND_OUTPUT_WINMM };
	const int maxtrynum = 2;
#else
	static const FSOUND_OUTPUTTYPES outtypes[3] =
	{ FSOUND_OUTPUT_ALSA, FSOUND_OUTPUT_OSS, FSOUND_OUTPUT_ESD };
	const int maxtrynum = 3;
#endif
	bool trya3d = false;

	/* Get command line options: */
	_nosound = !!Args.CheckParm ("-nosfx") || !!Args.CheckParm ("-nosound");

	if (_nosound)
	{
		I_InitMusic ();
		return;
	}

	int outindex;
	int trynum;

#ifdef _WIN32
	if (stricmp (snd_output, "dsound") == 0 || stricmp (snd_output, "directsound") == 0)
	{
		outindex = 0;
	}
	else if (stricmp (snd_output, "winmm") == 0 || stricmp (snd_output, "waveout") == 0)
	{
		outindex = 1;
	}
	else
	{
		// If snd_3d is true, try for a3d output if snd_output was not recognized above.
		// However, if running under NT 4.0, a3d will only be tried if specifically requested.
		outindex = (OSPlatform == os_WinNT) ? 1 : 0;
#if 0
		// FMOD 3.6 no longer supports a3d. Keep this code here in case support comes back.
		if (stricmp (snd_output, "a3d") == 0 || (outindex == 0 && snd_3d))
		{
			trya3d = true;
		}
#endif
	}
#else
	if (stricmp (snd_output, "oss") == 0)
	{
		outindex = 2;
	}
	else if (stricmp (snd_output, "esd") == 0 ||
			 stricmp (snd_output, "esound") == 0)
	{
		outindex = 1;
	}
	else
	{
		outindex = 0;
	}
#endif
	
	Printf ("I_InitSound: Initializing FMOD\n");
#ifdef _WIN32
	FSOUND_SetHWND (Window);
#endif

	if (snd_fpumixer)
	{
		FSOUND_SetMixer (FSOUND_MIXER_QUALITY_FPU);
	}
	else
	{
		FSOUND_SetMixer (FSOUND_MIXER_AUTODETECT);
	}

	// clamp snd_samplerate to FMOD's limits
	if (snd_samplerate < 4000)
	{
		snd_samplerate = 4000;
	}
	else if (snd_samplerate > 65535)
	{
		snd_samplerate = 65535;
	}

#ifdef _WIN32
	if (OSPlatform == os_WinNT)
	{
		// If running Windows NT 4, we need to initialize DirectSound before
		// using WinMM. If we don't, then FSOUND_Close will corrupt a
		// heap. This might just be the Audigy's drivers--I don't know why
		// it happens. At least the fix is simple enough. I only need to
		// initialize DirectSound once, and then I can initialize/close
		// WinMM as many times as I want.
		//
		// Yes, using WinMM under NT 4 is a good idea. I can get latencies as
		// low as 20 ms with WinMM, but with DirectSound I need to have the
		// latency as high as 120 ms to avoid crackling--quite the opposite
		// from the other Windows versions with real DirectSound support.

		static bool initedDSound = false;

		if (!initedDSound)
		{
			FSOUND_SetOutput (FSOUND_OUTPUT_DSOUND);
			if (FSOUND_GetOutput () == FSOUND_OUTPUT_DSOUND)
			{
				if (FSOUND_Init (snd_samplerate, 64, 0))
				{
					Sleep (50);
					FSOUND_Close ();
					initedDSound = true;
				}
			}
		}
	}
#endif

	while (!_nosound)
	{
		trynum = 0;
		while (trynum < maxtrynum)
		{
			long outtype = /*trya3d ? FSOUND_OUTPUT_A3D :*/
						   outtypes[(outindex+trynum) % maxtrynum];

			Printf ("  Setting %s output", OutputNames[outtype]);
			FModLog (FSOUND_SetOutput (outtype));
			if (FSOUND_GetOutput() != outtype)
			{
				Printf ("  Got %s output instead.\n", OutputNames[FSOUND_GetOutput()]);
#if 0
				if (trya3d)
				{
					trya3d = false;
				}
				else
#endif
				{
					++trynum;
				}
				continue;
			}
			Printf ("  Setting driver %d", *snd_driver);
			FModLog (FSOUND_SetDriver (snd_driver));
			if (FSOUND_GetOutput() != outtype)
			{
				Printf ("   Output changed to %s\n   Trying driver 0",
					OutputNames[FSOUND_GetOutput()]);
				FSOUND_SetOutput (outtype);
				FModLog (FSOUND_SetDriver (0));
			}
			if (snd_buffersize)
			{
				Printf ("  Setting buffer size %d", *snd_buffersize);
				FModLog (FSOUND_SetBufferSize (snd_buffersize));
			}
			FSOUND_GetDriverCaps (FSOUND_GetDriver(), &DriverCaps);
			Printf ("  Initialization");
			if (!FModLog (FSOUND_Init (snd_samplerate, 64, FSOUND_INIT_USEDEFAULTMIDISYNTH)))
			{
#if 0
				if (trya3d)
				{
					trya3d = false;
				}
				else
#endif
				{
					trynum++;
				}
			}
			else
			{
				break;
			}
		}
		if (trynum < 2)
		{ // Initialized successfully
			break;
		}
#ifdef _WIN32
		// If sound cannot be initialized, give the user some options.
		switch (DialogBox (g_hInst,
						   MAKEINTRESOURCE(IDD_FMODINITFAILED),
						   (HWND)Window,
						   (DLGPROC)InitBoxCallback))
		{
		case IDC_NOSOUND:
			_nosound = true;
			break;

		case IDCANCEL:
			exit (0);
			break;
		}
#else
		Printf ("Sound init failed. Using nosound.\n");
		_nosound = true;
#endif
	}

	if (!_nosound)
	{
		Enable_FSOUND_IO_Loader ();
		OutputType = FSOUND_GetOutput ();
		if (snd_3d)
		{
			Sound3D = true;
			FSOUND_3D_Listener_SetRolloffFactor (ROLLOFF3D);
			FSOUND_3D_Listener_SetDopplerFactor (DOPPLER3D);
			if (!(DriverCaps & FSOUND_CAPS_HARDWARE))
			{
				Printf ("Using software 3D sound\n");
			}
		}
		else
		{
			Sound3D = false;
		}
		snd_sfxvolume.Callback ();

		static bool didthis = false;
		if (!didthis)
		{
			didthis = true;
			atterm (I_ShutdownSound);
		}
		SoundDown = false;
	}

	I_InitMusic ();

	snd_sfxvolume.Callback ();
}

int I_SetChannels (int numchannels)
{
	int i;

	if (_nosound)
	{
		return numchannels;
	}

	// If using 3D sound, use all the hardware channels available,
	// regardless of what the user sets with snd_channels. If there
	// are fewer than 8 hardware channels, then force software.
	if (Sound3D)
	{
		int hardChans = FSOUND_GetNumHardwareChannels ();

		if (hardChans < 8)
		{
			Sound3D = false;
		}
		else
		{
			numchannels = hardChans;
		}
	}

	ChannelMap = new ChanMap[numchannels];
	for (i = 0; i < numchannels; i++)
	{
		ChannelMap[i].soundID = -1;
	}

	numChannels = numchannels;
	return numchannels;
}

/*
inline void check()
{
	static char bah[32];
	int hah = _heapchk ();
	sprintf (bah, "%d\n", hah);
	OutputDebugString (bah);
}
*/

void STACK_ARGS I_ShutdownSound (void)
{
	if (!_nosound && !SoundDown)
	{
		size_t i;

		SoundDown = true;

//check();
		FSOUND_StopSound (FSOUND_ALL);
//check();
		if (ChannelMap)
		{
			delete[] ChannelMap;
			ChannelMap = NULL;
		}
//check();

		// Free all loaded samples
		for (i = 0; i < S_sfx.Size (); i++)
		{
			S_sfx[i].data = NULL;
			S_sfx[i].altdata = NULL;
			S_sfx[i].bHaveLoop = false;
		}

		FSOUND_Close ();
//check();
	}
}

CCMD (snd_status)
{
	if (_nosound)
	{
		Printf ("sound is not active\n");
		return;
	}

	int output = FSOUND_GetOutput ();
	int driver = FSOUND_GetDriver ();
	int mixer = FSOUND_GetMixer ();

	Printf ("Output: %s\n", OutputNames[output]);
	Printf ("Driver: %d (%s)\n", driver,
		FSOUND_GetDriverName (driver));
	Printf ("Mixer: %s\n", MixerNames[mixer]);
	if (Sound3D)
	{
		Printf ("Using 3D sound\n");
	}
	if (DriverCaps)
	{
		Printf ("Driver caps: 0x%x\n", DriverCaps);
		if (DriverCaps & FSOUND_CAPS_HARDWARE)
		{
			Printf ("Hardware channels: %d\n", FSOUND_GetNumHardwareChannels ());
		}
	}
}

void I_MovieDisableSound ()
{
	//OutputDebugString ("away\n");
	I_ShutdownMusic ();
	I_ShutdownSound ();
}

void I_MovieResumeSound ()
{
	//OutputDebugString ("come back\n");
	I_InitSound ();
	S_Init ();
	S_RestartMusic ();
}

CCMD (snd_reset)
{
	I_MovieDisableSound ();
	I_MovieResumeSound ();
}

CCMD (snd_listdrivers)
{
	long numdrivers = FSOUND_GetNumDrivers ();
	long i;

	for (i = 0; i < numdrivers; i++)
	{
		Printf ("%ld. %s\n", i, FSOUND_GetDriverName (i));
	}
}

ADD_STAT (sound, out)
{
	if (_nosound)
	{
		strcpy (out, "no sound");
	}
	else
	{
		sprintf (out, "%d channels, %.2f%% CPU", FSOUND_GetChannelsPlaying(),
			FSOUND_GetCPUUsage());
	}
}
