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
extern HINSTANCE g_hInst;
#define USE_WINDOWS_DWORD
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

#include "fmodsound.h"
#ifdef _WIN32
#include "altsound.h"
#endif

#include "m_swap.h"
#include "stats.h"
#include "files.h"
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
#include "gi.h"

#include "doomdef.h"

EXTERN_CVAR (Float, snd_sfxvolume)
CVAR (Int, snd_samplerate, 44100, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Int, snd_buffersize, 0, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (String, snd_output, "default", CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

// killough 2/21/98: optionally use varying pitched sounds
CVAR (Bool, snd_pitched, false, CVAR_ARCHIVE)

// Maps sfx channels onto FMOD channels
static struct ChanMap
{
	int soundID;		// sfx playing on this channel
	long channelID;
	bool bIsLooping;
	bool bIs3D;
	unsigned int lastPos;
} *ChannelMap;

SoundRenderer *GSnd;

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
	else if (GSnd != NULL)
	{
		GSnd->SetSfxVolume (self);
	}
}


void I_InitSound ()
{
	/* Get command line options: */
	bool nosound = !!Args.CheckParm ("-nosfx") || !!Args.CheckParm ("-nosound");

	if (nosound)
	{
		I_InitMusic ();
		return;
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
	if (stricmp (snd_output, "alternate") == 0)
	{
		GSnd = new AltSoundRenderer;
	}
	else
	{
		GSnd = new FMODSoundRenderer;
		if (!GSnd->IsValid ())
		{
			delete GSnd;
			GSnd = new AltSoundRenderer;
		}
	}
#else
	GSnd = new FMODSoundRenderer;
#endif

	if (!GSnd->IsValid ())
	{
		delete GSnd;
		GSnd = NULL;
		Printf ("Sound init failed. Using nosound.\n");
	}
	I_InitMusic ();
	snd_sfxvolume.Callback ();
}


void I_ShutdownSound (void)
{
	if (GSnd != NULL)
	{
		delete GSnd;
		GSnd = NULL;
	}
}

CCMD (snd_status)
{
	if (GSnd == NULL)
	{
		Printf ("sound is not active\n");
	}
	else
	{
		GSnd->PrintStatus ();
	}
}

CCMD (snd_reset)
{
	SoundRenderer *snd = GSnd;
	if (snd != NULL)
	{
		snd->MovieDisableSound ();
		GSnd = NULL;
	}
	I_InitSound ();
	S_Init ();
	S_RestartMusic ();
	if (snd != NULL) delete snd;
}

CCMD (snd_listdrivers)
{
	if (GSnd != NULL)
	{
		GSnd->PrintDriversList ();
	}
	else
	{
		Printf ("Sound is inactive.\n");
	}
}

ADD_STAT (sound)
{
	if (GSnd != NULL)
	{
		return GSnd->GatherStats ();
	}
	else
	{
		return "no sound";
	}
}

SoundRenderer::SoundRenderer ()
: Sound3D (false)
{
}

SoundRenderer::~SoundRenderer ()
{
}

SoundTrackerModule *SoundRenderer::OpenModule (const char *file, int offset, int length)
{
	return NULL;
}

long SoundRenderer::StartSound3D (sfxinfo_t *sfx, float vol, int pitch, int channel, bool looping, float pos[3], float vel[3], bool pauseable)
{
	return 0;
}

void SoundRenderer::UpdateSoundParams3D (long handle, float pos[3], float vel[3])
{
}

void SoundRenderer::UpdateListener (AActor *listener)
{
}

void SoundRenderer::UpdateSounds ()
{
}

FString SoundRenderer::GatherStats ()
{
	return "No stats for this sound renderer.";
}

void SoundRenderer::ResetEnvironment ()
{
}

SoundStream::~SoundStream ()
{
}

SoundTrackerModule::~SoundTrackerModule ()
{
}

