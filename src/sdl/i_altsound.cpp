/*
** i_altsound.cpp
**
** Simple, no frills DirectSound player.
**
**---------------------------------------------------------------------------
** Copyright 2002 Randy Heit
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

// HEADER FILES ------------------------------------------------------------

#include <malloc.h>
#include <assert.h>

#include "doomtype.h"
#include "c_cvars.h"
#include "templates.h"
#include "i_system.h"
#include "i_music.h"
#include "s_sound.h"
#include "w_wad.h"
#include "m_swap.h"

// MACROS ------------------------------------------------------------------

#define PITCH(f,x)	(snd_pitched ? ((f)*(x))/128 : (f))
#define ID_RIFF		MAKE_ID('R','I','F','F')
#define ID_WAVE		MAKE_ID('W','A','V','E')
#define ID_fmt		MAKE_ID('f','m','t',' ')
#define ID_data		MAKE_ID('d','a','t','a')

// TYPES -------------------------------------------------------------------

struct Channel
{
	sfxinfo_t *Sample;
	union
	{
		QWORD SamplePos;
		struct
		{
#ifndef WORDS_BIGENDIAN
			DWORD LowPos, HighPos;
#else
			DWORD HighPos, LowPos;
#endif
		};
	};
	QWORD SampleStep;
	SDWORD LeftVolume;
	SDWORD RightVolume;
	bool Looping;
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void STACK_ARGS I_ShutdownSound_Simple ();
static void CopyAndClip (SWORD *buffer, DWORD count, DWORD start);
static void UpdateSound ();
static void AddChannel8 (Channel *chan, DWORD count);
static void AddChannel16 (Channel *chan, DWORD count);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int _nosound;

EXTERN_CVAR (Int, snd_samplerate)
EXTERN_CVAR (Int, snd_buffersize)
EXTERN_CVAR (Bool, snd_pitched)

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int Frequency;
static bool SimpleDown;
static int Amp;
static DWORD BufferSamples, BufferBytes;
static DWORD WritePos;
static DWORD BufferTime;
static DWORD MaxWaitTime;
static SDWORD *RenderBuffer;
static bool MixerQuit;

static Channel *Channels;
static int NumChannels;

// CODE --------------------------------------------------------------------

//==========================================================================
//
// I_InitSound_Simple
//
//==========================================================================

void I_InitSound_Simple ()
{
	SimpleDown = true;
	_nosound = true;
	I_InitMusic ();
}

//==========================================================================
//
// I_ShutdownSound_Simple
//
//==========================================================================

static void STACK_ARGS I_ShutdownSound_Simple ()
{
	SimpleDown = true;
}

//==========================================================================
//
// I_MovieDisableSound_Simple
//
//==========================================================================

void I_MovieDisableSound_Simple ()
{
	I_ShutdownSound_Simple ();
}

//==========================================================================
//
// I_SetChannels_Simple
//
//==========================================================================

int I_SetChannels_Simple (int numchannels)
{
	int i;

	Channels = new Channel[numchannels];
	NumChannels = numchannels;

	for (i = 0; i < numchannels; ++i)
	{
		Channels[i].Sample = NULL;
		Channels[i].SamplePos = 0;
		Channels[i].SampleStep = 0;
		Channels[i].LeftVolume = 0;
		Channels[i].RightVolume = 0;
		Channels[i].Looping = false;
	}

	return numchannels;
}

//==========================================================================
//
// I_SetSFXVolume_Simple
//
//==========================================================================

void I_SetSFXVolume_Simple (float volume)
{
	Amp = (int)(volume * 256.0);
}

//==========================================================================
//
// I_StartSound_Simple
//
//==========================================================================

long I_StartSound_Simple (sfxinfo_t *sfx, int vol, int sep, int pitch, int channel, BOOL looping)
{
	return 0;
}

//==========================================================================
//
// I_StopSound_Simple
//
//==========================================================================

void I_StopSound_Simple (int handle)
{
	if (Channels == NULL) return;

	Channel *chan = Channels + handle - 1;

	chan->Sample = NULL;
}

//==========================================================================
//
// I_SoundIsPlaying_Simple
//
//==========================================================================

int I_SoundIsPlaying_Simple (int handle)
{
	if (Channels == NULL) return 0;

	Channel *chan = Channels + handle - 1;

	return chan->Sample != NULL;
}

//==========================================================================
//
// I_UpdateSoundParams_Simple
//
//==========================================================================

void I_UpdateSoundParams_Simple (int handle, int vol, int sep, int pitch)
{
	return;
}

//==========================================================================
//
// I_LoadSound_Simple
//
//==========================================================================

void I_LoadSound_Simple (sfxinfo_t *sfx)
{
	bool stereo = false;
	SDWORD len;
	BYTE *sfxdata, *sfxstart;
	SDWORD size;
	
	size = Wads.LumpLength (sfx->lumpnum);
	if (size == 0)
	{
		sfx->lumpnum = Wads.GetNumForName ("dsempty");
		size = Wads.LumpLength (sfx->lumpnum);
		if (size == 0)
		{
			return;
		}
	}

	sfxdata = new BYTE[size];
	Wads.ReadLump (sfx->lumpnum, sfxdata);
	sfxstart = NULL;
	len = ((SDWORD *)sfxdata)[1];

	if (sfx->bLoadRAW || *(DWORD *)sfxdata == ID_RIFF ||
		(sfxdata[0] == 3 && sfxdata[1] == 0 && len <= size - 8))
	{
		if (sfx->bLoadRAW)
		{ // raw
			len = size;
			sfx->frequency = (sfx->bForce22050 ? 22050 : 11025);
			sfxstart = sfxdata;
			sfx->b16bit = false;
		}
		else
		{ // DMX
			sfx->frequency = SHORT(((WORD *)sfxdata)[1]);
			if (sfx->frequency == 0)
			{
				sfx->frequency = 11025;
			}
			sfxstart = sfxdata + 8;
			sfx->b16bit = false;
		}
		if (sfxstart != NULL)
		{
			if (!sfx->b16bit && !stereo)
			{ // Convert 8-bit mono to signed
				sfx->length = len;
				sfx->data = new BYTE[len];
				for (SDWORD i = 0; i < len; ++i)
				{
					((SBYTE *)sfx->data)[i] = sfxstart[i] - 128;
				}
			}
			else if (!sfx->b16bit && stereo)
			{ // Convert 8-bit mono to signed and merge into one channel
				sfx->length = len / 2;
				sfx->data = new BYTE[len / 2];
				for (SDWORD i = 0; i < len/2; ++i)
				{
					((SBYTE *)sfx->data)[i] = (sfxstart[i*2] + sfxstart[i*2+1] - 256) / 2;
				}
			}
			else if (sfx->b16bit && !stereo)
			{ // Copy 16-bit mono as-is
				sfx->length = len / 2;
				sfx->data = new BYTE[len];
				for (SDWORD i = 0; i < len/2; ++i)
				{
					((SWORD *)sfx->data)[i] = SHORT(((SWORD *)sfxstart)[i]);
				}
			}
			else
			{ // Merge 16-bit stereo into one channel
				sfx->length = len / 4;
				sfx->data = new BYTE[len / 2];
				for (SDWORD i = 0; i < len/4; ++i)
				{
					((SWORD *)sfx->data)[i] = (SHORT(((SWORD *)sfxstart)[i*2]) + SHORT(((SWORD *)sfxstart)[i*2+1])) / 2;
				}
			}
			sfx->ms = sfx->length * 1000 / sfx->frequency;
		}
		DPrintf ("sound loaded: %d Hz %d samples\n", sfx->frequency, sfx->length);
	}
	if (sfx->data == NULL)
	{
badwave:
		sfx->lumpnum = Wads.GetNumForName ("dsempty");
		I_LoadSound_Simple (sfx);
	}
	delete[] sfxdata;
}

//==========================================================================
//
// I_UnloadSound_Simple
//
//==========================================================================

void I_UnloadSound_Simple (sfxinfo_t *sfx)
{
	if (sfx->data != NULL)
	{
		delete sfx->data;
		sfx->data = NULL;
	}
}

//==========================================================================
//
// I_UpdateSounds_Simple
//
//==========================================================================

void I_UpdateSounds_Simple ()
{
}


//==========================================================================
//
// CopyAndClip
//
//==========================================================================

static void CopyAndClip (SWORD *buffer, DWORD count, DWORD start)
{
	SDWORD *from = RenderBuffer + start;
#if defined(_MSC_VER) && defined(USEASM)
	if (CPU.bCMOV)
	{
		__asm
		{
			mov ecx, count
			mov esi, from
			mov edi, buffer
			lea esi, [esi+ecx*4]
			lea edi, [edi+ecx*2]
			shr ecx, 1
			neg ecx
			mov edx, 0x00007fff
looper:		mov eax, [esi+ecx*8]
			mov ebx, [esi+ecx*8+4]
			sar eax, 8
			sar ebx, 8

			// Clamp high
			cmp eax, edx
			cmovg eax, edx
			cmp ebx, edx
			cmovg ebx, edx

			// Clamp low
			not edx
			cmp eax, edx
			cmovl eax, edx
			cmp ebx, edx
			cmovl ebx, edx

			not edx
			mov [edi+ecx*4], ax
			mov [edi+ecx*4+2], bx

			inc ecx
			jnz looper
		}
		return;
	}
#endif
	for (; count; --count)
	{
		SDWORD val = *from++ >> 2;
		if (val > 32767)
		{
			val = 32767;
		}
		else if (val < -32768)
		{
			val = -32768;
		}

		*buffer++ = (SWORD)val;
	}
}

//==========================================================================
//
// AddChannel8
//
//==========================================================================

static void AddChannel8 (Channel *chan, DWORD count)
{
	SBYTE *from;
	QWORD step;
	DWORD stop;
	SDWORD *buffer, left, right;

	if ((chan->LeftVolume | chan->RightVolume) == 0)
	{
		return;
	}

	buffer = RenderBuffer;
	from = (SBYTE *)chan->Sample->data;
	stop = chan->Sample->length;

	left = chan->LeftVolume * Amp;
	right = chan->RightVolume * Amp;
	step = chan->SampleStep;

	for (; count; --count)
	{
		SDWORD sample = (SDWORD)from[chan->HighPos];
		chan->SamplePos += step;
		buffer[0] += sample * left;
		buffer[1] += sample * right;
		buffer += 2;
		if (chan->HighPos >= stop)
		{
			if (chan->Looping)
			{
				chan->HighPos -= chan->Sample->length;
			}
			else
			{
				chan->Sample = NULL;
				break;
			}
		}
	}
}

//==========================================================================
//
// AddChannel16
//
//==========================================================================

static void AddChannel16 (Channel *chan, DWORD count)
{
	SWORD *from;
	QWORD step;
	DWORD stop;
	SDWORD *buffer, left, right;

	if ((chan->LeftVolume | chan->RightVolume) == 0)
	{
		return;
	}

	buffer = RenderBuffer;
	from = (SWORD *)chan->Sample->data;
	stop = chan->Sample->length;

	left = (chan->LeftVolume * Amp) >> 8;
	right = (chan->RightVolume * Amp) >> 8;
	step = chan->SampleStep;

	for (; count; --count)
	{
		SDWORD sample = (SDWORD)from[chan->HighPos];
		chan->SamplePos += step;
		buffer[0] += sample * left;
		buffer[1] += sample * right;
		buffer += 2;
		if (chan->HighPos >= stop)
		{
			if (chan->Looping)
			{
				chan->HighPos -= chan->Sample->length;
			}
			else
			{
				chan->Sample = NULL;
				break;
			}
		}
	}
}
