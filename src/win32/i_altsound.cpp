/*
** 
** 
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

// HEADER FILES ------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#define INITGUID
#include <dsound.h>
#include <malloc.h>
#include <assert.h>

#include "doomtype.h"
#include "c_cvars.h"
#include "templates.h"
#include "i_system.h"
#include "i_music.h"
#include "s_sound.h"
#include "w_wad.h"
#include "z_zone.h"
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
#ifndef __BIG_ENDIAN__
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
	CRITICAL_SECTION CriticalSection;
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void STACK_ARGS I_ShutdownSound_Simple ();
static void CopyAndClip (SWORD *buffer, DWORD count, DWORD start);
static DWORD WINAPI MixerThreadFunc (LPVOID param);
static void UpdateSound ();
static void AddChannel8 (Channel *chan, DWORD count);
static void AddChannel16 (Channel *chan, DWORD count);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern HWND Window;
extern int _nosound;

EXTERN_CVAR (Int, snd_samplerate)
EXTERN_CVAR (Int, snd_buffersize)
EXTERN_CVAR (Bool, snd_pitched)

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static LPDIRECTSOUND lpds;
static LPDIRECTSOUNDBUFFER lpdsb, lpdsbPrimary;

static int Frequency;
static bool SimpleDown;
static int Amp;
static DWORD BufferSamples, BufferBytes;
static DWORD WritePos;
static DWORD BufferTime;
static DWORD MaxWaitTime;
static SDWORD *RenderBuffer;
static HANDLE MixerThread;
static HANDLE MixerEvent;
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
	HRESULT hr;
	WAVEFORMATEX wfx =
	{
		WAVE_FORMAT_PCM,	// wFormatTag
		2,					// nChannels
		snd_samplerate,		// nSamplesPerSec
		snd_samplerate * 4,	// nAvgBytesPerSec
		4,					// nBlockAlign
		16,					// wBitsPerSample
		0					// cbSize
	};
	DSBUFFERDESC dsbdesc = { sizeof(dsbdesc), DSBCAPS_PRIMARYBUFFER };

	lpds = NULL;
	MixerEvent = NULL;
	MixerQuit = false;

	//hr = DirectSoundCreate (NULL, &lpds, NULL);
	hr = CoCreateInstance (CLSID_DirectSound, 0, CLSCTX_INPROC_SERVER, IID_IDirectSound, (void **)&lpds);
	if (FAILED (hr))
	{
		Printf ("Could not create DirectSound interface: %08x\n", hr);
		goto fail;
	}

	hr = lpds->Initialize (0);
	if (FAILED (hr))
	{
		Printf ("Could not initialize DirectSound interface: %08x\n", hr);
		goto fail;
	}

	hr = lpds->SetCooperativeLevel (Window, DSSCL_PRIORITY);
	if (FAILED (hr))
	{
		Printf ("Could not set DirectSound co-op level: %08x\n", hr);
		lpds->Release ();
		goto fail;
	}

	hr = lpds->CreateSoundBuffer (&dsbdesc, &lpdsbPrimary, NULL);
	if (FAILED (hr))
	{
		Printf ("Could not get DirectSound primary buffer: %08x\n", hr);
		lpds->Release ();
		goto fail;
	}

	hr = lpdsbPrimary->SetFormat (&wfx);

	// Now see what format we really got
	hr = lpdsbPrimary->GetFormat (&wfx, sizeof(wfx), NULL);

	Frequency = wfx.nSamplesPerSec;

	BufferSamples = MAX (*snd_buffersize, 30);
	BufferSamples = (wfx.nSamplesPerSec * BufferSamples / 1000);
	BufferBytes = BufferSamples << 2;
	dsbdesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2;
	dsbdesc.dwBufferBytes = BufferBytes;
	dsbdesc.lpwfxFormat = &wfx;
	wfx.wBitsPerSample = 16;	// Let DirectSound worry about 8-bit primary buffers
	wfx.nChannels = 2;			// Also let DirectSound worry about mono primary buffers

	hr = lpds->CreateSoundBuffer (&dsbdesc, &lpdsb, NULL);
	if (FAILED (hr))
	{
		Printf ("Could not create secondary DirectSound buffer: %08x\n", hr);
		goto fail;
	}

	hr = lpdsb->Play (0, 0, DSBPLAY_LOOPING);
	if (FAILED (hr))
	{
		Printf ("Could not play secondary buffer: %08x\n", hr);
		goto fail;
	}

	RenderBuffer = (SDWORD *)malloc (BufferBytes << 1);
	if (RenderBuffer == NULL)
	{
		Printf ("Could not alloc sound render buffer\n");
		goto fail;
	}

	MixerEvent = CreateEvent (NULL, FALSE, FALSE, NULL);
	if (MixerEvent == NULL)
	{
		Printf ("Could not create mixer event: %08x\n", GetLastError());
		goto fail;
	}

	MaxWaitTime = BufferSamples * 333 / wfx.nSamplesPerSec;

	DWORD dummy;
	MixerThread = CreateThread (NULL, 0, MixerThreadFunc, NULL, 0, &dummy);
	if (MixerThread == NULL)
	{
		Printf ("Could not create mixer thread: %08x\n", GetLastError());
		goto fail;
	}
	SetThreadPriority (MixerThread, THREAD_PRIORITY_ABOVE_NORMAL);

	WritePos = 0;
	SimpleDown = false;
	atterm (I_ShutdownSound_Simple);
	snd_sfxvolume.Callback ();

	I_InitMusic ();
	return;

fail:
	if (MixerEvent != NULL)
	{
		CloseHandle (MixerEvent);
		MixerEvent = NULL;
	}
	if (RenderBuffer != NULL)
	{
		free (RenderBuffer);
		RenderBuffer = NULL;
	}
	if (lpds != NULL)
	{
		lpds->Release ();
	}
	SimpleDown = true;
	_nosound = true;
	lpds = NULL;
	I_InitMusic ();
}

//==========================================================================
//
// I_ShutdownSound_Simple
//
//==========================================================================

static void STACK_ARGS I_ShutdownSound_Simple ()
{
	if (lpds != NULL && !SimpleDown)
	{
		MixerQuit = true;
		SetEvent (MixerEvent);
		WaitForSingleObject (MixerThread, INFINITE);
		CloseHandle (MixerEvent);
		MixerEvent = NULL;

		lpds->Release ();
		lpds = NULL;

		free (RenderBuffer);
		RenderBuffer = NULL;

		if (Channels != NULL)
		{
			for (int i = 0; i < NumChannels; ++i)
			{
				DeleteCriticalSection (&Channels[i].CriticalSection);
			}
			delete[] Channels;
			NumChannels = 0;
		}

		for (size_t i = 0; i < S_sfx.Size(); ++i)
		{
			if (S_sfx[i].data != NULL)
			{
				delete[] S_sfx[i].data;
				S_sfx[i].data = NULL;
			}
		}
	}
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
		InitializeCriticalSection (&Channels[i].CriticalSection);
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
	SetEvent (MixerEvent);
}

//==========================================================================
//
// I_StartSound_Simple
//
//==========================================================================

long I_StartSound_Simple (sfxinfo_t *sfx, int vol, int sep, int pitch, int channel, BOOL looping)
{
	if (sfx->data == NULL || Channels == NULL)
	{
		return 0;
	}

	Channel *chan = Channels + channel;
	QWORD step = ((QWORD)PITCH (sfx->frequency, pitch) << 32) / Frequency;
	SDWORD left, right;

	if (sep < -1)
	{
		left = right = 191 * vol / 255;
	}
	else if (sep == -1)
	{
		left = 191 * vol / 255;
		right = -left;
	}
	else
	{
		sep += 1;
		left = vol - ((vol*sep*sep) >> 16);
		sep -= 257;
		right = vol - ((vol*sep*sep) >> 16);
		/*
		left = 256 * (255 - sep) / 255;
		right = 256 * sep / 255;
		*/
	}

	EnterCriticalSection (&chan->CriticalSection);
	chan->Sample = sfx;
	chan->SamplePos = 0;
	chan->SampleStep = step;
	chan->LeftVolume = left;
	chan->RightVolume = right;
	chan->Looping = !!looping;
	LeaveCriticalSection (&chan->CriticalSection);

	return channel + 1;
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
	if (Channels == NULL) return;

	Channel *chan = Channels + handle - 1;

	SDWORD left, right;

	if (sep < -1)
	{
		left = right = 191 * vol / 255;
	}
	else if (sep == -1)
	{
		left = 191 * vol / 255;
		right = -left;
	}
	else
	{
		sep += 1;
		left = vol - ((vol*sep*sep) >> 16);
		sep -= 257;
		right = vol - ((vol*sep*sep) >> 16);
	}

	EnterCriticalSection (&chan->CriticalSection);
	if (chan->Sample != NULL)
	{
		chan->SampleStep = ((QWORD)PITCH (chan->Sample->frequency, pitch) << 32) / Frequency;
		chan->LeftVolume = left;
		chan->RightVolume = right;
	}
	LeaveCriticalSection (&chan->CriticalSection);
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
	
	size = W_LumpLength (sfx->lumpnum);
	if (size == 0)
	{
		sfx->lumpnum = W_GetNumForName ("dsempty");
		size = W_LumpLength (sfx->lumpnum);
		if (size == 0)
		{
			return;
		}
	}

	sfxdata = new BYTE[size];
	W_ReadLump (sfx->lumpnum, sfxdata);
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
		else if (*(DWORD *)sfxdata == ID_RIFF)
		{ // WAVE
			BYTE *sfxend, *sfx_p;
			WAVEFORMAT fmtchunk;

			if (LONG(((SDWORD *)sfxdata)[1]) > size - 8)
			{ // lump is too short
				goto badwave;
			}
			if (((DWORD *)sfxdata)[2] != ID_WAVE)
			{ // not really a WAVE
				goto badwave;
			}

			sfxend = sfxdata + LONG(((DWORD *)sfxdata)[1]) + 8;
			sfx_p = sfxdata + 4*3;
			fmtchunk.wFormatTag = ~0;

			while (sfx_p < sfxend)
			{
				DWORD chunkid = ((DWORD *)sfx_p)[0];
				DWORD chunklen = LONG(((DWORD *)sfx_p)[1]);
				sfx_p += 4*2;
				if (chunkid == ID_fmt)
				{
					if (chunklen < sizeof(fmtchunk))
					{ // fmt chunk is too short
						continue;
					}
					memcpy (&fmtchunk, sfx_p, sizeof(fmtchunk));
					fmtchunk.wFormatTag = SHORT(fmtchunk.wFormatTag);
					fmtchunk.nChannels = SHORT(fmtchunk.nChannels);
					fmtchunk.nSamplesPerSec = SHORT(fmtchunk.nSamplesPerSec);
					fmtchunk.nAvgBytesPerSec = SHORT(fmtchunk.nAvgBytesPerSec);
					fmtchunk.nBlockAlign = SHORT(fmtchunk.nBlockAlign);
				}
				else if (chunkid == ID_data)
				{
					if (fmtchunk.wFormatTag == WAVE_FORMAT_PCM)
					{
						sfxstart = sfx_p;
						len = chunklen;
						sfx->frequency = fmtchunk.nSamplesPerSec;
						stereo = fmtchunk.nChannels > 1;
						sfx->b16bit = (fmtchunk.nBlockAlign >> (stereo?1:0)) == 2;
					}
					break;
				}
				sfx_p += chunklen;
			}
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
		sfx->lumpnum = W_GetNumForName ("dsempty");
		I_LoadSound_Simple (sfx);
	}
	delete[] sfxdata;
}

//==========================================================================
//
// I_UpdateSounds_Simple
//
//==========================================================================

void I_UpdateSounds_Simple ()
{
	SetEvent (MixerEvent);
}

//==========================================================================
//
// MixerThreadFunc
//
// Sits in a loop, periodically updating the sound buffer. The idea to
// use an event to get sound changes to happen at specific times comes
// from one of KB's articles about FR-08's sound system. So is the idea to
// use just a single buffer rather than doublebuffering with position
// notifications. See <http://kebby.org/articles/fr08snd3.html>
//
//==========================================================================

static DWORD WINAPI MixerThreadFunc (LPVOID param)
{
	for (;;)
	{
		WaitForSingleObject (MixerEvent, MaxWaitTime);
		if (MixerQuit)
		{
			return 0;
		}
		UpdateSound ();
	}
}

//==========================================================================
//
// UpdateSound
//
//==========================================================================

static void UpdateSound ()
{
	HRESULT hr;
	DWORD play, write, total;
	SWORD *ptr1; DWORD bytes1;
	SWORD *ptr2; DWORD bytes2;

	hr = lpdsb->GetCurrentPosition (&play, &write);
	if (FAILED (hr))
	{
		return;
	}

	if (play < WritePos)
	{
		total = (BufferBytes - WritePos) + play;
	}
	else
	{
		total = play - WritePos;
	}

	memset (RenderBuffer, 0, total << 1);
	for (int i = 0; i < NumChannels; ++i)
	{
		EnterCriticalSection (&Channels[i].CriticalSection);
		if (Channels[i].Sample != NULL)
		{
			if (Channels[i].Sample->b16bit)
			{
				AddChannel16 (Channels + i, total >> 2);
			}
			else
			{
				AddChannel8 (Channels + i, total >> 2);
			}
		}
		LeaveCriticalSection (&Channels[i].CriticalSection);
	}

	hr = lpdsb->Lock (WritePos, total, (LPVOID *)&ptr1, &bytes1, (LPVOID *)&ptr2, &bytes2, 0);
	if (FAILED (hr))
	{
		return;
	}

	CopyAndClip (ptr1, bytes1 >> 1, 0);
	if (ptr2 != NULL) CopyAndClip (ptr2, bytes2 >> 1, bytes1 >> 1);

	lpdsb->Unlock (ptr1, bytes1, ptr2, bytes2);

	WritePos = play;
	BufferTime += total;
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
	if (HaveCMOV)
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

	DWORD start = count;

	for (count; count; --count)
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

	DWORD start = count;

	for (count; count; --count)
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
