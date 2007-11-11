/*
** i_sound.cpp
** System interface for sound; uses fmod.dll
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
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
#define USE_WINDOWS_DWORD
#else
#define FALSE 0
#define TRUE 1
#endif

#include "templates.h"
#include "fmodsound.h"
#include "c_cvars.h"
#include "i_system.h"
#include "gi.h"
#include "actor.h"
#include "r_state.h"
#include "w_wad.h"
#include "sample_flac.h"
#include "i_music.h"
#include "i_musicinterns.h"

// killough 2/21/98: optionally use varying pitched sounds
#define PITCH(f,x) (snd_pitched ? ((f)*(x))/128 : (f))

extern int MAX_SND_DIST;
const int S_CLIPPING_DIST = 1200;
const int S_CLOSE_DIST = 160;

CVAR (Int, snd_driver, 0, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, snd_3d, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, snd_waterreverb, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, snd_fpumixer, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

EXTERN_CVAR (String, snd_output)
EXTERN_CVAR (Float, snd_musicvolume)
EXTERN_CVAR (Int, snd_buffersize)
EXTERN_CVAR (Int, snd_samplerate)
EXTERN_CVAR (Bool, snd_pitched)

static const ReverbContainer *PrevEnvironment;
ReverbContainer *ForcedEnvironment;

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

//===========================================================================
//
// The container for a FSOUND_STREAM.
//
//===========================================================================

class FMODStreamCapsule : public SoundStream
{
public:
	FMODStreamCapsule (FSOUND_STREAM *stream)
		: Stream(stream), UserData(NULL), Callback(NULL), Channel(-1) {}

	FMODStreamCapsule (void *udata, SoundStreamCallback callback)
		: Stream(NULL), UserData(udata), Callback(callback), Channel(-1) {}

	~FMODStreamCapsule ()
	{
		if (Stream != NULL) FSOUND_Stream_Close (Stream);
	}

	void SetStream (FSOUND_STREAM *stream)
	{
		Stream = stream;
	}

	bool Play (bool looping, float volume)
	{
		FSOUND_Stream_SetMode(Stream, looping? FSOUND_LOOP_NORMAL : FSOUND_LOOP_OFF);
		Channel = FSOUND_Stream_PlayEx (FSOUND_FREE, Stream, NULL, true);
		if (Channel != -1)
		{
			FSOUND_SetVolumeAbsolute (Channel, clamp<int>((int)(volume * relative_volume * 255), 0, 255));
			FSOUND_SetPan (Channel, FSOUND_STEREOPAN);
			FSOUND_SetPaused (Channel, false);
			return true;
		}
		return false;
	}

	void Stop ()
	{
		FSOUND_Stream_Stop (Stream);
	}

	bool SetPaused (bool paused)
	{
		if (Channel != -1)
		{
			return !!FSOUND_SetPaused (Channel, paused);
		}
		return false;
	}

	unsigned int GetPosition ()
	{
		return FSOUND_Stream_GetPosition (Stream);
	}

	void SetVolume (float volume)
	{
		if (Channel != -1)
		{
			FSOUND_SetVolumeAbsolute (Channel, clamp<int>((int)(volume * relative_volume * 255), 0, 255));
		}
	}

	static signed char F_CALLBACKAPI MyCallback (FSOUND_STREAM *stream, void *buff, int len, void *userdata)
	{
		FMODStreamCapsule *me = (FMODStreamCapsule *)userdata;
		if (me->Callback == NULL)
		{
			return FALSE;
		}
		return me->Callback (me, buff, len, me->UserData);
	}

private:
	FSOUND_STREAM *Stream;
	void *UserData;
	SoundStreamCallback Callback;
	int Channel;
};

//===========================================================================
//
// The container for a FMUSIC_MODULE.
//
//===========================================================================

class FMUSICCapsule : public SoundTrackerModule
{
public:
	FMUSICCapsule (FMUSIC_MODULE *mod) : Module (mod) {}
	~FMUSICCapsule () { FMUSIC_FreeSong (Module); }

	bool Play ()
	{
		return !!FMUSIC_PlaySong (Module);
	}

	void Stop ()
	{
		FMUSIC_StopSong (Module);
	}

	void SetVolume (float volume)
	{
		FMUSIC_SetMasterVolume (Module, clamp<int>((int)(volume * relative_volume * 256), 0, 256));
	}

	bool SetPaused (bool paused)
	{
		return !!FMUSIC_SetPaused (Module, paused);
	}

	bool IsPlaying ()
	{
		return !!FMUSIC_IsPlaying (Module);
	}

	bool IsFinished ()
	{
		return !!FMUSIC_IsFinished (Module);
	}

	bool SetOrder (int order)
	{
		return !!FMUSIC_SetOrder (Module, order);
	}

private:
	FMUSIC_MODULE *Module;
};

//===========================================================================
//
// The interface the game uses to talk to FMOD.
//
//===========================================================================

FMODSoundRenderer::FMODSoundRenderer ()
{
	DidInit = Init ();
}

FMODSoundRenderer::~FMODSoundRenderer ()
{
	Shutdown ();
}

bool FMODSoundRenderer::IsValid ()
{
	return DidInit;
}

bool FMODSoundRenderer::Init ()
{
#ifdef _WIN32
	static const FSOUND_OUTPUTTYPES outtypes[2] =
	{ FSOUND_OUTPUT_DSOUND, FSOUND_OUTPUT_WINMM };
	const int maxtrynum = 2;
#else
	static const FSOUND_OUTPUTTYPES outtypes[3] =
	{ FSOUND_OUTPUT_OSS, FSOUND_OUTPUT_ALSA, FSOUND_OUTPUT_ESD };
	const int maxtrynum = 3;
#endif
#if 0
	bool trya3d = false;
#endif

	int outindex;
	int trynum;
	bool nosound = false;

	ChannelMap = NULL;
	NumChannels = 0;
	PrevEnvironment = DefaultEnvironments[0];

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
		outindex = (OSPlatform == os_WinNT4) ? 1 : 0;
#if 0
		// FMOD 3.6 no longer supports a3d. Keep this code here in case support comes back.
		if (stricmp (snd_output, "a3d") == 0 || (outindex == 0 && snd_3d))
		{
			trya3d = true;
		}
#endif
	}
#else
	if (stricmp (snd_output, "alsa") == 0)
	{
		outindex = 1;
	}
	else if (stricmp (snd_output, "esd") == 0 ||
			 stricmp (snd_output, "esound") == 0)
	{
		outindex = 2;
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

#ifdef _WIN32
	if (OSPlatform == os_WinNT4)
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

	while (!nosound)
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
			if (!FModLog (FSOUND_Init (snd_samplerate, 64, FSOUND_INIT_DSOUND_DEFERRED)))
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
	}

	if (!nosound)
	{
		OutputType = FSOUND_GetOutput ();
		if (snd_3d)
		{
			Sound3D = true;
			if (gameinfo.gametype == GAME_Doom || gameinfo.gametype == GAME_Strife)
			{ 
				FSOUND_3D_SetRolloffFactor (1.7f);
			}
			else if (gameinfo.gametype == GAME_Heretic)
			{
				FSOUND_3D_SetRolloffFactor (1.24f);
			}
			else
			{
				FSOUND_3D_SetRolloffFactor (0.96f);
			}
			FSOUND_3D_SetDopplerFactor (1.f);
			FSOUND_3D_SetDistanceFactor (100.f);	// Distance factor only effects doppler!
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
	}
	return !nosound;
}

void FMODSoundRenderer::Shutdown ()
{
	if (DidInit)
	{
		unsigned int i;

		FSOUND_StopSound (FSOUND_ALL);
		if (ChannelMap)
		{
			delete[] ChannelMap;
			ChannelMap = NULL;
		}
		NumChannels = 0;

		// Free all loaded samples
		for (i = 0; i < S_sfx.Size (); i++)
		{
			S_sfx[i].data = NULL;
			S_sfx[i].altdata = NULL;
			S_sfx[i].bHaveLoop = false;
		}

		FSOUND_Close ();
		DidInit = false;
	}
}

void FMODSoundRenderer::PrintStatus ()
{
	int output = FSOUND_GetOutput ();
	int driver = FSOUND_GetDriver ();
	int mixer = FSOUND_GetMixer ();
	int num2d, num3d, total;

	Printf ("Output: %s\n", OutputNames[output]);
	Printf ("Driver: %d (%s)\n", driver, FSOUND_GetDriverName (driver));
	Printf ("Mixer: %s\n", MixerNames[mixer]);
	if (DriverCaps)
	{
		Printf ("Driver caps:");
		if (DriverCaps & FSOUND_CAPS_HARDWARE)	Printf (" HARDWARE");
		if (DriverCaps & FSOUND_CAPS_EAX2)		Printf (" EAX2");
		if (DriverCaps & FSOUND_CAPS_EAX3)		Printf (" EAX3");
		Printf ("\n");
	}
	FSOUND_GetNumHWChannels (&num2d, &num3d, &total);
	Printf ("Hardware 2D channels: %d\n", num2d);
	Printf ("Hardware 3D channels: %d\n", num3d);
	Printf ("Total hardware channels: %d\n", total);
	if (Sound3D)
	{
		Printf ("\nUsing 3D sound\n");
	}
}

void FMODSoundRenderer::PrintDriversList ()
{
	int numdrivers = FSOUND_GetNumDrivers ();

	for (int i = 0; i < numdrivers; i++)
	{
		Printf ("%d. %s\n", i, FSOUND_GetDriverName (i));
	}
}

FString FMODSoundRenderer::GatherStats ()
{
	FString out;
	out.Format ("%d channels, %.2f%% CPU", FSOUND_GetChannelsPlaying(),
		FSOUND_GetCPUUsage());
	return out;
}

void FMODSoundRenderer::MovieDisableSound ()
{
	I_ShutdownMusic ();
	Shutdown ();
}

void FMODSoundRenderer::MovieResumeSound ()
{
	Init ();
	S_Init ();
	S_RestartMusic ();
}

void FMODSoundRenderer::SetSfxVolume (float volume)
{
	FSOUND_SetSFXMasterVolume (int(volume * 255.f));
	// FMOD apparently resets absolute volume channels when setting master vol
	snd_musicvolume.Callback ();
}

int FMODSoundRenderer::SetChannels (int numchannels)
{
	int i;

	// If using 3D sound, use all the hardware channels available,
	// regardless of what the user sets with snd_channels. If there
	// are fewer than 8 hardware channels, then force software.
	if (Sound3D)
	{
		int hardChans;
		
		FSOUND_GetNumHWChannels (NULL, &hardChans, NULL);

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

	NumChannels = numchannels;
	return numchannels;
}

SoundStream *FMODSoundRenderer::CreateStream (SoundStreamCallback callback, int buffbytes, int flags, int samplerate, void *userdata)
{
	FMODStreamCapsule *capsule = new FMODStreamCapsule (userdata, callback);
	unsigned int mode = FSOUND_2D | FSOUND_SIGNED;
	mode |= (flags & SoundStream::Mono) ? FSOUND_MONO : FSOUND_STEREO;
	mode |= (flags & SoundStream::Bits8) ? FSOUND_8BITS : FSOUND_16BITS;
	FSOUND_STREAM *stream = FSOUND_Stream_Create (FMODStreamCapsule::MyCallback, buffbytes, mode, samplerate, capsule);
	if (stream != NULL)
	{
		capsule->SetStream (stream);
		return capsule;
	}
	delete capsule;
	return NULL;
}

SoundStream *FMODSoundRenderer::OpenStream (const char *filename_or_data, int flags, int offset, int length)
{
	unsigned int mode = FSOUND_NORMAL | FSOUND_2D;
	if (flags & SoundStream::Loop) mode |= FSOUND_LOOP_NORMAL;
	FSOUND_STREAM *stream;
	
	if (offset==-1)
	{
		mode |= FSOUND_LOADMEMORY;
		offset=0;
	}
	stream = FSOUND_Stream_Open (filename_or_data, mode, offset, length);

	if (stream != NULL)
	{
		return new FMODStreamCapsule (stream);
	}
	return NULL;
}

SoundTrackerModule *FMODSoundRenderer::OpenModule (const char *filename_or_data, int offset, int length)
{
	FMUSIC_MODULE *mod;

	int mode = FSOUND_LOOP_NORMAL;
	if (offset==-1)
	{
		mode |= FSOUND_LOADMEMORY;
		offset=0;
	}

	mod = FMUSIC_LoadSongEx (filename_or_data, offset, length, mode, NULL, 0);

	if (mod != NULL)
	{
		return new FMUSICCapsule (mod);
	}
	return NULL;
}

//
// vol range is 0-255
// sep range is 0-255, -1 for surround, -2 for full vol middle
//
long FMODSoundRenderer::StartSound (sfxinfo_t *sfx, int vol, int sep, int pitch, int channel, bool looping, bool pauseable)
{
	if (!ChannelMap)
		return 0;

	int id = int(sfx - &S_sfx[0]);
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
		FSOUND_SetSurround (chan, sep == -1 ? TRUE : FALSE);
		FSOUND_SetFrequency (chan, freq);
		FSOUND_SetVolume (chan, vol);
		FSOUND_SetPan (chan, pan);
		FSOUND_SetPaused (chan, false);
		ChannelMap[channel].channelID = chan;
		ChannelMap[channel].soundID = id;
		ChannelMap[channel].bIsLooping = looping ? true : false;
		ChannelMap[channel].lastPos = 0;
		ChannelMap[channel].bIs3D = false;
		ChannelMap[channel].bIsPauseable = pauseable;
		return channel + 1;
	}

	DPrintf ("Sound %s failed to play: %d\n", sfx->name.GetChars(), FSOUND_GetError ());
	return 0;
}

long FMODSoundRenderer::StartSound3D (sfxinfo_t *sfx, float vol, int pitch, int channel,
	bool looping, float pos[3], float vel[3], bool pauseable)
{
	if (!Sound3D || !ChannelMap)
		return 0;

	int id = int(sfx - &S_sfx[0]);
	long freq;
	long chan;

	freq = PITCH(sfx->frequency,pitch);

	FSOUND_SAMPLE *sample = CheckLooping (sfx, looping);

	chan = FSOUND_PlaySoundEx (FSOUND_FREE, sample, NULL, true);

	if (chan != -1)
	{
		//FSOUND_SetReserved (chan, TRUE);
		FSOUND_SetFrequency (chan, freq);
		FSOUND_SetVolume (chan, (int)(vol * 255.f));
		FSOUND_3D_SetAttributes (chan, pos, vel);
		FSOUND_SetPaused (chan, false);
		ChannelMap[channel].channelID = chan;
		ChannelMap[channel].soundID = id;
		ChannelMap[channel].bIsLooping = looping ? true : false;
		ChannelMap[channel].lastPos = 0;
		ChannelMap[channel].bIs3D = true;
		ChannelMap[channel].bIsPauseable = pauseable;
		return channel + 1;
	}

	DPrintf ("Sound %s failed to play: %d (%d)\n", sfx->name.GetChars(), FSOUND_GetError (), FSOUND_GetChannelsPlaying ());
	return 0;
}

void FMODSoundRenderer::StopSound (long handle)
{
	if (!handle || !ChannelMap)
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

void FMODSoundRenderer::StopAllChannels ()
{
	for (long i = 1; i <= NumChannels; ++i)
	{
		StopSound (i);
	}
}


void FMODSoundRenderer::SetSfxPaused (bool paused)
{
	for (int i = 0; i < NumChannels; ++i)
	{
		if (ChannelMap[i].soundID != -1)
		{
			if (ChannelMap[i].bIsPauseable)
			{
				FSOUND_SetPaused (ChannelMap[i].channelID, paused);
			}
		}
	}
}

bool FMODSoundRenderer::IsPlayingSound (long handle)
{
	if (!handle || !ChannelMap)
		return false;

	handle--;
	if (ChannelMap[handle].soundID == -1)
	{
		return false;
	}
	else
	{
		bool is;

		// FSOUND_IsPlaying does not work with A3D
		if (OutputType != FSOUND_OUTPUT_A3D)
		{
			is = !!FSOUND_IsPlaying (ChannelMap[handle].channelID);
		}
		else
		{
			unsigned int pos;
			if (ChannelMap[handle].bIsLooping)
			{
				is = true;
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

void FMODSoundRenderer::UpdateSoundParams (long handle, int vol, int sep, int pitch)
{
	if (!handle || !ChannelMap)
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

void FMODSoundRenderer::UpdateSoundParams3D (long handle, float pos[3], float vel[3])
{
	if (!handle || !ChannelMap)
		return;

	handle--;
	if (ChannelMap[handle].soundID == -1)
		return;

	FSOUND_3D_SetAttributes (ChannelMap[handle].channelID, pos, vel);
}

void FMODSoundRenderer::ResetEnvironment ()
{
	PrevEnvironment = NULL;
}

void FMODSoundRenderer::UpdateListener (AActor *listener)
{
	float angle;
	float pos[3], vel[3];
	float lpos[3];

	if (Sound3D && ChannelMap)
	{
		vel[0] = listener->momx * (TICRATE/65536.f);
		vel[1] = listener->momy * (TICRATE/65536.f);
		vel[2] = listener->momz * (TICRATE/65536.f);
		pos[0] = listener->x / 65536.f;
		pos[2] = listener->y / 65536.f;
		pos[1] = listener->z / 65536.f;

		// Move sounds that are not meant to be heard in 3D so
		// that they remain on top of the listener.
		
		for (int i = 0; i < NumChannels; i++)
		{
			if (ChannelMap[i].soundID != -1 && !ChannelMap[i].bIs3D)
			{
				FSOUND_3D_SetAttributes (ChannelMap[i].channelID, pos, vel);
			}
		}

		// Sounds that are right on top of the listener can produce
		// weird results depending on the environment, so position
		// the listener back slightly from its true location.

		angle = (float)(listener->angle) * ((float)PI / 2147483648.f);

		lpos[0] = pos[0] - .5f * cosf (angle);
		lpos[2] = pos[2] - .5f * sinf (angle);
		lpos[1] = pos[1];

		FSOUND_3D_Listener_SetAttributes (lpos, vel,
			cosf (angle), 0.f, sinf (angle), 0.f, 1.f, 0.f);

		//if (DriverCaps & (FSOUND_CAPS_EAX2|FSOUND_CAPS_EAX3))
		{
			bool underwater;
			const ReverbContainer *env;

			if (ForcedEnvironment)
			{
				env = ForcedEnvironment;
			}
			else
			{
				underwater = (listener->waterlevel == 3 && snd_waterreverb);
				assert (zones != NULL);
				env = zones[listener->Sector->ZoneNumber].Environment;
				if (env == NULL)
				{
					env = DefaultEnvironments[0];
				}
				if (env == DefaultEnvironments[0] && underwater)
				{
					env = DefaultEnvironments[22];
				}
			}
			if (env != PrevEnvironment || env->Modified)
			{
				DPrintf ("Reverb Environment %s\n", env->Name);
				const_cast<ReverbContainer*>(env)->Modified = false;
				FSOUND_Reverb_SetProperties ((FSOUND_REVERB_PROPERTIES *)(&env->Properties));
				PrevEnvironment = env;
			}
		}

		FSOUND_Update ();
	}
}

void FMODSoundRenderer::LoadSound (sfxinfo_t *sfx)
{
	if (!sfx->data)
	{
		DPrintf ("loading sound \"%s\" (%d) ", sfx->name.GetChars(), sfx - &S_sfx[0]);
		getsfx (sfx);
	}
}

void FMODSoundRenderer::UnloadSound (sfxinfo_t *sfx)
{
	if (sfx->data == NULL)
		return;

	sfx->bHaveLoop = false;
	sfx->normal = 0;
	sfx->looping = 0;
	if (sfx->altdata != NULL)
	{
		FSOUND_Sample_Free ((FSOUND_SAMPLE *)sfx->altdata);
		sfx->altdata = NULL;
	}
	if (sfx->data != NULL)
	{
		FSOUND_Sample_Free ((FSOUND_SAMPLE *)sfx->data);
		sfx->data = NULL;
	}

	DPrintf ("Unloaded sound \"%s\" (%d)\n", sfx->name.GetChars(), sfx - &S_sfx[0]);
}

// FSOUND_Sample_Upload seems to mess up the signedness of sound data when
// uploading to hardware buffers. The pattern is not particularly predictable,
// so this is a replacement for it that loads the data manually. Source data
// is mono, unsigned, 8-bit. Output is mono, signed, 8- or 16-bit.

int FMODSoundRenderer::PutSampleData (FSOUND_SAMPLE *sample, const BYTE *data, int len, unsigned int mode)
{
	/*if (mode & FSOUND_2D)
	{
		return FSOUND_Sample_Upload (sample, const_cast<BYTE *>(data),
			FSOUND_8BITS|FSOUND_MONO|FSOUND_UNSIGNED);
	}
	else*/ if (FSOUND_Sample_GetMode (sample) & FSOUND_8BITS)
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
}

void FMODSoundRenderer::DoLoad (void **slot, sfxinfo_t *sfx)
{
	BYTE *sfxdata;
	int size;
	int errcount;
	unsigned long samplemode;

	samplemode = Sound3D ? FSOUND_HW3D : FSOUND_2D;
	sfxdata = NULL;

	errcount = 0;
	while (errcount < 2)
	{
		if (sfxdata != NULL)
		{
			delete[] sfxdata;
			sfxdata = NULL;
		}

		if (errcount)
			sfx->lumpnum = Wads.GetNumForName ("dsempty", ns_sounds);

		size = Wads.LumpLength (sfx->lumpnum);
		if (size == 0)
		{
			errcount++;
			continue;
		}

		FWadLump wlump = Wads.OpenLumpNum (sfx->lumpnum);
		sfxdata = new BYTE[size];
		wlump.Read (sfxdata, size);
		SDWORD len = ((SDWORD *)sfxdata)[1];

		// If the sound is raw, just load it as such.
		// Otherwise, try the sound as DMX format.
		// If that fails, let FMOD try and figure it out.
		if (sfx->bLoadRAW ||
			(((BYTE *)sfxdata)[0] == 3 && ((BYTE *)sfxdata)[1] == 0 && len <= size - 8))
		{
			FSOUND_SAMPLE *sample;
			const BYTE *sfxstart;
			unsigned int bits;

			if (sfx->bLoadRAW)
			{
				len = Wads.LumpLength (sfx->lumpnum);
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
			if (((BYTE *)sfxdata)[0] == 'f' && ((BYTE *)sfxdata)[1] == 'L' &&
				 ((BYTE *)sfxdata)[2] == 'a' && ((BYTE *)sfxdata)[3] == 'C')
			{
				FLACSampleLoader loader (sfx);
				*slot = loader.LoadSample (samplemode);
				if (*slot == NULL && FSOUND_GetError() == FMOD_ERR_CREATEBUFFER && samplemode == FSOUND_HW3D)
				{
					DPrintf ("Trying to fall back to software sample\n");
					*slot = FSOUND_Sample_Load (FSOUND_FREE, (char *)sfxdata, FSOUND_2D, 0, size);
				}
			}
			else
			{
				*slot = FSOUND_Sample_Load (FSOUND_FREE, (char *)sfxdata,
					samplemode|FSOUND_LOADMEMORY, 0, size);
				if (*slot == NULL && FSOUND_GetError() == FMOD_ERR_CREATEBUFFER && samplemode == FSOUND_HW3D)
				{
					DPrintf ("Trying to fall back to software sample\n");
					*slot = FSOUND_Sample_Load (FSOUND_FREE, (char *)sfxdata, FSOUND_2D|FSOUND_LOADMEMORY, 0, size);
				}
			}
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
		DPrintf ("[%d Hz %d samples]\n", sfx->frequency, sfx->length);

		if (Sound3D)
		{
			// Match s_sound.cpp min distance.
			// Max distance is irrelevant.
			FSOUND_Sample_SetMinMaxDistance ((FSOUND_SAMPLE *)sfx->data,
				(float)S_CLOSE_DIST, (float)MAX_SND_DIST*2);
		}
	}

	if (sfxdata != NULL)
	{
		delete[] sfxdata;
	}
}

void FMODSoundRenderer::getsfx (sfxinfo_t *sfx)
{
	unsigned int i;

	// Get the sound data from the WAD and register it with sound library

	// If the sound doesn't exist, replace it with the empty sound.
	if (sfx->lumpnum == -1)
	{
		sfx->lumpnum = Wads.GetNumForName ("dsempty", ns_sounds);
	}
	
	// See if there is another sound already initialized with this lump. If so,
	// then set this one up as a link, and don't load the sound again.
	for (i = 0; i < S_sfx.Size (); i++)
	{
		if (S_sfx[i].data && S_sfx[i].link == sfxinfo_t::NO_LINK && S_sfx[i].lumpnum == sfx->lumpnum)
		{
			DPrintf ("Linked to %s (%d)\n", S_sfx[i].name.GetChars(), i);
			sfx->link = i;
			sfx->ms = S_sfx[i].ms;
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

FSOUND_SAMPLE *FMODSoundRenderer::CheckLooping (sfxinfo_t *sfx, bool looped)
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

void FMODSoundRenderer::UncheckSound (sfxinfo_t *sfx, bool looped)
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
