/*
** i_altsound.cpp
**
** Simple, no frills DirectSound player.
**
**---------------------------------------------------------------------------
** Copyright 2002-2006 Randy Heit
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

// HEADER FILES ------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>
#include <malloc.h>
#include <assert.h>

#define USE_WINDOWS_DWORD
#include "altsound.h"
#include "doomtype.h"
#include "c_cvars.h"
#include "templates.h"
#include "i_system.h"
#include "i_music.h"
#include "s_sound.h"
#include "w_wad.h"
#include "m_swap.h"
#include "sample_flac.h"
#include "stats.h"

// MACROS ------------------------------------------------------------------

#define PITCH(f,x)	(snd_pitched ? ((f)*(x))/128 : (f))
#define ID_RIFF		MAKE_ID('R','I','F','F')
#define ID_WAVE		MAKE_ID('W','A','V','E')
#define ID_fmt		MAKE_ID('f','m','t',' ')
#define ID_data		MAKE_ID('d','a','t','a')
#define ID_fLaC		MAKE_ID('f','L','a','C')

// TYPES -------------------------------------------------------------------

struct AltSoundRenderer::Channel
{
	sfxinfo_t *Sample;
	SQWORD SamplePos;
	SQWORD SampleStep;
	SDWORD LeftVolume;
	SDWORD RightVolume;
	bool Looping;
	bool bPaused;
	bool bIsPauseable;
	CRITICAL_SECTION CriticalSection;
};

struct AltSoundRenderer::Stream : public SoundStream
{
	Stream (AltSoundRenderer *renderer, SoundStreamCallback callback, int buffbytes,
			int flags, int samplerate, int mixfreq, void *userdata)
		: Renderer(renderer), Next(NULL), Prev(NULL),
		  Paused(true), DidFirstRun(false), Callback(callback), UserData(userdata)
	{
		InitializeCriticalSection (&CriticalSection);
		SamplePos = 0;
		SampleStep = ((QWORD)samplerate << 32) / mixfreq;
		Volume = 256;
		Bits8 = !!(flags & SoundStream::Bits8);
		Mono = !!(flags & SoundStream::Mono);

		BufferLen = buffbytes;
		Buffer = new BYTE[buffbytes];
	}

	~Stream ()
	{
		if (Prev != NULL)
		{
			if (Renderer != NULL) EnterCriticalSection (&Renderer->StreamCriticalSection);
			*Prev = Next;
			if (Renderer != NULL) LeaveCriticalSection (&Renderer->StreamCriticalSection);
		}
		if (Buffer != NULL) delete[] Buffer;
		DeleteCriticalSection (&CriticalSection);
	}

	bool Play (bool looping, float volume)
	{
		EnterCriticalSection (&CriticalSection);
		Paused = false;
		SamplePos = 0;
		LeaveCriticalSection (&CriticalSection);
		return true;
	}

	void Stop ()
	{
		EnterCriticalSection (&CriticalSection);
		Paused = true;
		DidFirstRun = false;
		SamplePos = 0;
		LeaveCriticalSection (&CriticalSection);
	}

	void SetVolume (float volume)
	{
		EnterCriticalSection (&CriticalSection);
		Volume = int(volume * 256.f);
		LeaveCriticalSection (&CriticalSection);
	}

	bool SetPaused (bool paused)
	{
		EnterCriticalSection (&CriticalSection);
		Paused = paused;
		LeaveCriticalSection (&CriticalSection);
		return true;
	}

	unsigned int GetPosition ()
	{
		// Only needed for streams created with OpenStream, which aren't supported
		return 0;
	}

	AltSoundRenderer *Renderer;
	Stream *Next, **Prev;
	bool Paused, DidFirstRun;
	SoundStreamCallback Callback;
	void *UserData;
	bool Bits8;
	bool Mono;
	int BufferLen;
	BYTE *Buffer;

	SQWORD SamplePos;
	SQWORD SampleStep;
	SDWORD Volume;
	CRITICAL_SECTION CriticalSection;
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern HWND Window;

EXTERN_CVAR (Int, snd_samplerate)
EXTERN_CVAR (Int, snd_buffersize)
EXTERN_CVAR (Bool, snd_pitched)

// PUBLIC DATA DEFINITIONS -------------------------------------------------

CVAR (Int, snd_interpolate, 1, CVAR_ARCHIVE)

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
//
// AltSoundRenderer Constructor
//
//==========================================================================

AltSoundRenderer::AltSoundRenderer ()
{
	DidInit = false;
	NumChannels = 0;
	Channels = NULL;
	RenderBuffer = NULL;
	DidInit = Init ();
}

//==========================================================================
//
// AltSoundRenderer Destructor
//
//==========================================================================

AltSoundRenderer::~AltSoundRenderer ()
{
	Shutdown ();
}

//==========================================================================
//
// AltSoundRenderer :: IsValid
//
//==========================================================================

bool AltSoundRenderer::IsValid ()
{
	return DidInit;
}

//==========================================================================
//
// AltSoundRenderer :: Init
//
//==========================================================================

bool AltSoundRenderer::Init ()
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
	Streams = NULL;
	for (int i = 0; i < NUM_PERFMETERS; ++i)
	{
		PerfMeter[i] = 0.0;
	}
	CurPerfMeter = 0;

	InitializeCriticalSection (&StreamCriticalSection);

	//hr = DirectSoundCreate (NULL, &lpds, NULL);
	hr = CoCreateInstance (CLSID_DirectSound, 0, CLSCTX_INPROC_SERVER, IID_IDirectSound, (void **)&lpds);
	if (FAILED (hr))
	{
		Printf ("Could not create DirectSound interface: %08lx\n", hr);
		goto fail;
	}

	hr = lpds->Initialize (0);
	if (FAILED (hr))
	{
		Printf ("Could not initialize DirectSound interface: %08lx\n", hr);
		goto fail;
	}

	hr = lpds->SetCooperativeLevel (Window, DSSCL_PRIORITY);
	if (FAILED (hr))
	{
		Printf ("Could not set DirectSound co-op level: %08lx\n", hr);
		lpds->Release ();
		goto fail;
	}

	hr = lpds->CreateSoundBuffer (&dsbdesc, &lpdsbPrimary, NULL);
	if (FAILED (hr))
	{
		Printf ("Could not get DirectSound primary buffer: %08lx\n", hr);
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
		Printf ("Could not create secondary DirectSound buffer: %08lx\n", hr);
		goto fail;
	}

	hr = lpdsb->Play (0, 0, DSBPLAY_LOOPING);
	if (FAILED (hr))
	{
		Printf ("Could not play secondary buffer: %08lx\n", hr);
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
		Printf ("Could not create mixer event: %08lx\n", GetLastError());
		goto fail;
	}

	MaxWaitTime = BufferSamples * 333 / wfx.nSamplesPerSec;

	DWORD dummy;
	MixerThread = CreateThread (NULL, 0, MixerThreadFunc, this, 0, &dummy);
	if (MixerThread == NULL)
	{
		Printf ("Could not create mixer thread: %08lx\n", GetLastError());
		goto fail;
	}
	SetThreadPriority (MixerThread, THREAD_PRIORITY_ABOVE_NORMAL);

	WritePos = 0;
	return true;

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
		lpds = NULL;
	}
	return false;
}

//==========================================================================
//
// AltSoundRenderer :: Shutdown
//
//==========================================================================

void AltSoundRenderer::Shutdown ()
{
	if (lpds != NULL && DidInit)
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

		for (unsigned int i = 0; i < S_sfx.Size(); ++i)
		{
			if (S_sfx[i].data != NULL)
			{
				delete[] (BYTE *)S_sfx[i].data;
				S_sfx[i].data = NULL;
			}
		}
		while (Streams != NULL)
		{
			delete Streams;
		}
	}
	DeleteCriticalSection (&StreamCriticalSection);
	DidInit = false;
}

//==========================================================================
//
// AltSoundRenderer :: MovieDisableSound
//
//==========================================================================

void AltSoundRenderer::MovieDisableSound ()
{
	I_ShutdownMusic ();
	Shutdown ();
}

//==========================================================================
//
// AltSoundRenderer :: MovieResumeSound
//
//==========================================================================

void AltSoundRenderer::MovieResumeSound ()
{
	Init ();
	S_Init ();
	S_RestartMusic ();
}

//==========================================================================
//
// AltSoundRenderer :: SetChannels
//
//==========================================================================

int AltSoundRenderer::SetChannels (int numchannels)
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
// AltSoundRenderer :: SetSfxVolume
//
//==========================================================================

void AltSoundRenderer :: SetSfxVolume (float volume)
{
	Amp = int(volume * 256.0);
	SetEvent (MixerEvent);
}

//==========================================================================
//
// AltSoundRenderer :: StartSound
//
//==========================================================================

long AltSoundRenderer::StartSound (sfxinfo_t *sfx, int vol, int sep, int pitch, int channel, bool looping, bool pauseable)
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
	chan->bPaused = false;
	chan->bIsPauseable = pauseable;
	LeaveCriticalSection (&chan->CriticalSection);

	return channel + 1;
}

//==========================================================================
//
// AltSoundRenderer :: StopSound
//
//==========================================================================

void AltSoundRenderer::StopSound (long handle)
{
	if (Channels == NULL) return;

	Channel *chan = Channels + handle - 1;

	chan->Sample = NULL;
}

//==========================================================================
//
// AltSoundRenderer :: StopAllChannels
//
//==========================================================================

void AltSoundRenderer::StopAllChannels ()
{
	if (Channels != NULL)
	{
		for (int i = 0; i < NumChannels; ++i)
		{
			Channels[i].Sample = NULL;
		}
	}
}

//==========================================================================
//
// AltSoundRenderer :: SetSfxPaused
//
//==========================================================================

void AltSoundRenderer::SetSfxPaused (bool paused)
{
	if (Channels == NULL) return;

	for (int i = 0; i < NumChannels; ++i)
	{
		if (Channels[i].bIsPauseable)
		{
			Channels[i].bPaused = paused;
		}
	}
}

//==========================================================================
//
// AltSoundRenderer :: IsPlayingSound
//
//==========================================================================

bool AltSoundRenderer::IsPlayingSound (long handle)
{
	if (Channels == NULL) return 0;

	Channel *chan = Channels + handle - 1;

	return chan->Sample != NULL;
}

//==========================================================================
//
// AltSoundRenderer :: UpdateSoundParams
//
//==========================================================================

void AltSoundRenderer :: UpdateSoundParams (long handle, int vol, int sep, int pitch)
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
// AltSoundRenderer :: LoadSound
//
//==========================================================================

void AltSoundRenderer::LoadSound (sfxinfo_t *sfx)
{
	bool stereo = false, signed8 = false;
	SDWORD len;
	BYTE *sfxdata, *sfxstart;
	SDWORD size;

	if (sfx->data != NULL)
	{
		return;
	}

	size = sfx->lumpnum >= 0 ? Wads.LumpLength (sfx->lumpnum) : 0;
	if (size == 0)
	{
		sfx->lumpnum = Wads.GetNumForName ("dsempty", ns_sounds);
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

	if (sfx->bLoadRAW || *(DWORD *)sfxdata == ID_RIFF || *(DWORD *)sfxdata == ID_fLaC ||
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

			if (LittleLong(((SDWORD *)sfxdata)[1]) > size - 8)
			{ // lump is too short
				goto badwave;
			}
			if (((DWORD *)sfxdata)[2] != ID_WAVE)
			{ // not really a WAVE
				goto badwave;
			}

			sfxend = sfxdata + LittleLong(((DWORD *)sfxdata)[1]) + 8;
			sfx_p = sfxdata + 4*3;
			fmtchunk.wFormatTag = ~0;

			while (sfx_p < sfxend)
			{
				DWORD chunkid = ((DWORD *)sfx_p)[0];
				DWORD chunklen = LittleLong(((DWORD *)sfx_p)[1]);
				sfx_p += 4*2;
				if (chunkid == ID_fmt)
				{
					if (chunklen < sizeof(fmtchunk))
					{ // fmt chunk is too short
						continue;
					}
					memcpy (&fmtchunk, sfx_p, sizeof(fmtchunk));
					fmtchunk.wFormatTag = LittleShort(fmtchunk.wFormatTag);
					fmtchunk.nChannels = LittleShort(fmtchunk.nChannels);
					fmtchunk.nSamplesPerSec = LittleShort(fmtchunk.nSamplesPerSec);
					fmtchunk.nAvgBytesPerSec = LittleShort(fmtchunk.nAvgBytesPerSec);
					fmtchunk.nBlockAlign = LittleShort(fmtchunk.nBlockAlign);
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
		else if (*(DWORD *)sfxdata == ID_fLaC)
		{
			delete[] sfxdata;
			FLACSampleLoader loader (sfx);
			signed8 = true;
			sfxstart = sfxdata = loader.ReadSample (&len);
			if (sfxdata == NULL)
			{
				goto badwave;
			}
		}
		else
		{ // DMX
			sfx->frequency = LittleShort(((WORD *)sfxdata)[1]);
			if (sfx->frequency == 0)
			{
				sfx->frequency = 11025;
			}
			sfxstart = sfxdata + 8;
			sfx->b16bit = false;
		}
		if (sfxstart != NULL)
		{
			if (!sfx->b16bit && signed8)
			{ // Copy signed 8-bit mono as-is
				sfx->length = len;
				sfx->data = new BYTE[len + 2];
				memcpy (sfx->data, sfxstart, len);
			}
			else if (!sfx->b16bit && !stereo)
			{ // Convert 8-bit mono to signed
				sfx->length = len;
				sfx->data = new BYTE[len + 2];
				for (SDWORD i = 0; i < len; ++i)
				{
					((SBYTE *)sfx->data)[i] = sfxstart[i] - 128;
				}
			}
			else if (!sfx->b16bit && stereo)
			{ // Convert 8-bit stereo to signed and merge into one channel
				sfx->length = len / 2;
				sfx->data = new BYTE[len / 2 + 2];
				for (SDWORD i = 0; i < len/2; ++i)
				{
					((SBYTE *)sfx->data)[i] = (sfxstart[i*2] + sfxstart[i*2+1] - 256) / 2;
				}
			}
			else if (sfx->b16bit && !stereo)
			{ // Copy 16-bit mono as-is
				sfx->length = len / 2;
				sfx->data = new BYTE[len + 4];
				for (SDWORD i = 0; i < len/2; ++i)
				{
					((SWORD *)sfx->data)[i] = LittleShort(((SWORD *)sfxstart)[i]);
				}
			}
			else
			{ // Merge 16-bit stereo into one channel
				sfx->length = len / 4;
				sfx->data = new BYTE[len / 2 + 4];
				for (SDWORD i = 0; i < len/4; ++i)
				{
					((SWORD *)sfx->data)[i] = (LittleShort(((SWORD *)sfxstart)[i*2]) + LittleShort(((SWORD *)sfxstart)[i*2+1])) / 2;
				}
			}
			sfx->ms = sfx->length * 1000 / sfx->frequency;
		}
		DPrintf ("sound loaded: %d Hz %d samples\n", sfx->frequency, sfx->length);
	}
	if (sfx->data == NULL)
	{
badwave:
		if (sfxdata != NULL)
		{
			delete[] sfxdata;
			sfxdata = NULL;
		}
		sfx->lumpnum = Wads.GetNumForName ("dsempty", ns_sounds);
		LoadSound (sfx);
	}
	if (sfxdata != NULL) delete[] sfxdata;
}

//==========================================================================
//
// AltSoundRenderer :: UnloadSound
//
//==========================================================================

void AltSoundRenderer::UnloadSound (sfxinfo_t *sfx)
{
	if (sfx->data != NULL)
	{
		delete[] (BYTE *)sfx->data;
		sfx->data = NULL;
	}
}

//==========================================================================
//
// AltSoundRenderer :: UpdateSounds
//
//==========================================================================

void AltSoundRenderer::UpdateSounds ()
{
	SetEvent (MixerEvent);
}

//==========================================================================
//
// AltSoundRenderer :: MixerThreadFunc
//
// Sits in a loop, periodically updating the sound buffer. The idea to
// use an event to get sound changes to happen at specific times comes
// from one of KB's articles about FR-08's sound system. So is the idea to
// use just a single buffer rather than doublebuffering with position
// notifications. See <http://kebby.org/articles/fr08snd3.html>
//
//==========================================================================

DWORD WINAPI AltSoundRenderer::MixerThreadFunc (LPVOID param)
{
	AltSoundRenderer *me = (AltSoundRenderer *)param;
	for (;;)
	{
		WaitForSingleObject (me->MixerEvent, me->MaxWaitTime);
		if (me->MixerQuit)
		{
			return 0;
		}
		me->UpdateSound ();
	}
}

//==========================================================================
//
// AltSoundRenderer :: UpdateSound
//
//==========================================================================

void AltSoundRenderer::UpdateSound ()
{
	HRESULT hr;
	DWORD play, write, total;
	SWORD *ptr1; DWORD bytes1;
	SWORD *ptr2; DWORD bytes2;
	cycle_t meter = 0;

	clock(meter);

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
		if (Channels[i].Sample != NULL && !Channels[i].bPaused)
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
	EnterCriticalSection (&StreamCriticalSection);
	for (Stream *stream = Streams; stream != NULL; stream = stream->Next)
	{
		if (!stream->Paused)
		{
			if (stream->Bits8)
			{
				AddStream8 (stream, total >> 2);
			}
			else
			{
				AddStream16 (stream, total >> 2);
			}
		}
	}
	LeaveCriticalSection (&StreamCriticalSection);

	hr = lpdsb->Lock (WritePos, total, (LPVOID *)&ptr1, &bytes1, (LPVOID *)&ptr2, &bytes2, 0);
	if (FAILED (hr))
	{
		goto addperf;
	}

	CopyAndClip (ptr1, bytes1 >> 1, 0);
	if (ptr2 != NULL) CopyAndClip (ptr2, bytes2 >> 1, bytes1 >> 1);

	lpdsb->Unlock (ptr1, bytes1, ptr2, bytes2);

	WritePos = play;
	BufferTime += total;

addperf:
	unclock(meter);

	// % of CPU is determined as the time spent to calculate this chunk
	// divided by the time it takes to play the chunk. I'm not sure I'm
	// doing this right, since I get numbers that fluctuate a lot.

	PerfMeter[CurPerfMeter] = meter * SecondsPerCycle * double(Frequency) / double(total/4);
	CurPerfMeter = (CurPerfMeter + 1) & (NUM_PERFMETERS-1);
}

//==========================================================================
//
// AltSoundRenderer :: CopyAndClip
//
//==========================================================================

void AltSoundRenderer::CopyAndClip (SWORD *buffer, DWORD count, DWORD start)
{
	SDWORD *from = RenderBuffer + start;
#if defined(_MSC_VER) && defined(USEASM)
	if (CPU.bMMX)
	{
		__asm
		{
			mov ecx, count
			mov esi, from
			and	ecx, ~15
			mov edi, buffer
			lea esi, [esi+ecx*4]
			lea edi, [edi+ecx*2]
			neg ecx

loopermmx:	movq  mm0, [esi+ecx*4]
			movq  mm1, [esi+ecx*4+8]
			psrad mm0, 8
			movq  mm2, [esi+ecx*4+16]
			psrad mm1, 8
			movq  mm3, [esi+ecx*4+24]
			psrad mm2, 8
			movq  mm4, [esi+ecx*4+32]
			psrad mm3, 8
			movq  mm5, [esi+ecx*4+40]
			psrad mm4, 8
			movq  mm6, [esi+ecx*4+48]
			psrad mm5, 8
			movq  mm7, [esi+ecx*4+56]

			packssdw mm1,mm0
			psrad    mm6, 8
			movq     [edi+ecx*2],mm1
			packssdw mm3,mm2
			psrad    mm7, 8
			packssdw mm5,mm4
			movq     [edi+ecx*2+8],mm3
			packssdw mm7,mm6
			movq     [edi+ecx*2+16],mm5
			movq     [edi+ecx*2+24],mm7
            
			add ecx, 16
			jl loopermmx

			emms
			mov from, esi
			mov buffer, edi
		}
		count &= 15;
		if (count == 0)
			return;
	}
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
		SDWORD val = *from++ >> 8;
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
// AltSoundRenderer :: AddChannel8
//
//==========================================================================

void AltSoundRenderer::AddChannel8 (Channel *chan, DWORD count)
{
	SBYTE *from;
	QWORD step;
	SQWORD stop;
	SDWORD *buffer, left, right;
	SQWORD pos;

	if ((chan->LeftVolume | chan->RightVolume) == 0)
	{
		return;
	}

	buffer = RenderBuffer;
	from = (SBYTE *)chan->Sample->data;
	stop = SQWORD(chan->Sample->length) << 32;
	pos = chan->SamplePos;

	left = chan->LeftVolume * Amp * 257 / 256;
	right = chan->RightVolume * Amp * 257 / 256;
	step = chan->SampleStep;

	// LoadSample allocates an extra byte at the end of the sample just
	// so that we can alter it for interpolated sounds.
	from[chan->Sample->length] = chan->Looping ? from[0] : 0;
	from[chan->Sample->length+1] = chan->Looping ? from[1] : 0;
	while (count > 0)
	{
		DWORD avail = DWORD((stop - pos + step - 1) / step);
		if (avail > count) avail = count;
		pos = MixMono8 (buffer, from, avail, pos, step, left, right);
		buffer += avail * 2;
		count -= avail;
		if (pos >= stop)
		{
			if (chan->Looping)
			{
				pos -= stop;
			}
			else
			{
				chan->Sample = NULL;
				break;
			}
		}
	}
	chan->SamplePos = pos;
}

//==========================================================================
//
// AltSoundRenderer :: AddChannel16
//
//==========================================================================

void AltSoundRenderer::AddChannel16 (Channel *chan, DWORD count)
{
	SWORD *from;
	QWORD step;
	SQWORD stop;
	SDWORD *buffer, left, right;
	SQWORD pos;

	if ((chan->LeftVolume | chan->RightVolume) == 0)
	{
		return;
	}

	buffer = RenderBuffer;
	from = (SWORD *)chan->Sample->data;
	stop = SQWORD(chan->Sample->length) << 32;

	left = (chan->LeftVolume * Amp) >> 8;
	right = (chan->RightVolume * Amp) >> 8;
	step = chan->SampleStep;
	pos = chan->SamplePos;

	// LoadSample allocates an extra byte at the end of the sample just
	// so that we can alter it for interpolated sounds.
	from[chan->Sample->length] = chan->Looping ? from[0] : 0;
	from[chan->Sample->length+1] = chan->Looping ? from[1] : 0;
	while (count > 0)
	{
		DWORD avail = DWORD((stop - pos + step - 1) / step);
		if (avail > count) avail = count;
		pos = MixMono16 (buffer, from, avail, pos, step, left, right);
		buffer += avail * 2;
		count -= avail;
		if (pos >= stop)
		{
			if (chan->Looping)
			{
				pos -= stop;
			}
			else
			{
				chan->Sample = NULL;
				break;
			}
		}
	}
	chan->SamplePos = pos;
}

//==========================================================================
//
// AltSoundRenderer :: CreateStream
//
//==========================================================================

SoundStream *AltSoundRenderer::CreateStream (SoundStreamCallback callback,
	int buffbytes, int flags, int samplerate, void *userdata)
{
	Stream *stream = new Stream (this, callback, buffbytes, flags, samplerate, Frequency, userdata);

	EnterCriticalSection (&StreamCriticalSection);
	stream->Prev = &Streams;
	stream->Next = Streams;
	Streams = stream;
	LeaveCriticalSection (&StreamCriticalSection);
	return stream;
}

//==========================================================================
//
// AltSoundRenderer :: OpenStream
//
//==========================================================================

SoundStream *AltSoundRenderer::OpenStream (const char *filename, int flags, int offset, int length)
{
	return NULL;
}

//==========================================================================
//
// AltSoundRenderer :: AddStream8
//
//==========================================================================

void AltSoundRenderer::AddStream8 (Stream *stream, DWORD count)
{
	SBYTE *from;
	QWORD step;
	SQWORD stop;
	SDWORD *buffer, vol;
	SQWORD pos;

	if (stream->Volume == 0)
	{
		return;
	}

	buffer = RenderBuffer;
	from = (SBYTE *)stream->Buffer;
	if (stream->Mono)
	{
		stop = SQWORD(stream->BufferLen - 4) << 32;
	}
	else
	{
		stop = SQWORD(stream->BufferLen / 2 - 8) << 32;
	}

	vol = stream->Volume * Amp * 257 / 256;
	step = stream->SampleStep;
	pos = stream->SamplePos;

	if (!stream->DidFirstRun)
	{
		if (!stream->Callback (stream, stream->Buffer, stream->BufferLen, stream->UserData))
		{
			stream->Stop ();
			return;
		}
		stream->DidFirstRun = true;
	}

	while (count > 0)
	{
		DWORD avail = DWORD((stop - pos + step - 1) / step);
		if (avail > count) avail = count;
		if (stream->Mono)
		{
			pos = MixMono8 (buffer, from, avail, pos, step, vol, vol);
		}
		else
		{
			pos = MixStereo8 (buffer, from, avail, pos, step, vol);
		}
		buffer += avail * 2;
		count -= avail;
		if (pos >= stop)
		{
			pos -= stop;
			const int extra = stream->Mono ? 4 : 8;
			memcpy (stream->Buffer, stream->Buffer + stream->BufferLen - extra, extra);
			if (!stream->Callback (stream, stream->Buffer + extra, stream->BufferLen - extra, stream->UserData))
			{
				stream->Stop ();
				return;
			}
		}
	}
	stream->SamplePos = pos;
}

//==========================================================================
//
// AltSoundRenderer :: AddStream16
//
//
//==========================================================================

void AltSoundRenderer::AddStream16 (Stream *stream, DWORD count)
{
	SWORD *from;
	QWORD step;
	SQWORD stop;
	SDWORD *buffer, vol;
	SQWORD pos;

	if (stream->Volume == 0)
	{
		return;
	}

	buffer = RenderBuffer;
	from = (SWORD *)stream->Buffer;
	if (stream->Mono)
	{
		stop = SQWORD(stream->BufferLen / 2 - 4) << 32;
	}
	else
	{
		stop = SQWORD(stream->BufferLen / 4 - 8) << 32;
	}

	vol = (stream->Volume * Amp) >> 8;
	step = stream->SampleStep;
	pos = stream->SamplePos;

	if (!stream->DidFirstRun)
	{
		if (!stream->Callback (stream, stream->Buffer, stream->BufferLen, stream->UserData))
		{
			stream->Stop ();
			return;
		}
		stream->DidFirstRun = true;
	}

	while (count > 0)
	{
		DWORD avail = int((stop - pos + step - 1) / step);
		if (avail > count) avail = count;
		if (stream->Mono)
		{
			pos = MixMono16 (buffer, from, avail, pos, step, vol, vol);
		}
		else
		{
			pos = MixStereo16 (buffer, from, avail, pos, step, vol);
		}
		buffer += avail * 2;
		count -= avail;
		if (pos >= stop)
		{
			pos -= stop;
			const int extra = stream->Mono ? 8 : 16;
			memcpy (stream->Buffer, stream->Buffer + stream->BufferLen - extra, extra);
			if (!stream->Callback (stream, stream->Buffer + extra, stream->BufferLen - extra, stream->UserData))
			{
				stream->Stop ();
				return;
			}
		}
	}
	stream->SamplePos = pos;
}

//==========================================================================
//
// AltSoundRenderer :: PrintStatus
//
//==========================================================================

void AltSoundRenderer::PrintStatus ()
{
	Printf ("Using non-FMOD sound renderer.\n");
}

//==========================================================================
//
// AltSoundRenderer :: PrintDriversList
//
//==========================================================================

void AltSoundRenderer::PrintDriversList ()
{
	Printf ("No user-selectable drivers.\n");
}

//==========================================================================
//
// AltSoundRenderer :: GatherStats
//
//==========================================================================

FString AltSoundRenderer::GatherStats ()
{
	int i, countc, counts, totals;
	for (i = countc = 0; i < NumChannels; ++i)
	{
		if (Channels[i].Sample != NULL)
		{
			countc++;
		}
	}
	EnterCriticalSection (&StreamCriticalSection);
	counts = totals = 0;
	for (Stream *stream = Streams; stream != NULL; stream = stream->Next)
	{
		totals++;
		if (!stream->Paused)
		{
			counts++;
		}
	}
	LeaveCriticalSection (&StreamCriticalSection);
	double perf = 0.0;
	for (i = 0; i < NUM_PERFMETERS; ++i)
	{
		perf += PerfMeter[i];
	}
	FString out;
	out.Format ("%2d/%2d channels, %d/%d streams, %.2f%%", countc, NumChannels, counts, totals, perf*100.0/NUM_PERFMETERS);
	return out;
}

#endif
