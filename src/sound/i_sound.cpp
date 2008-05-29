/*
** i_sound.cpp
** Stubs for sound interfaces.
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
#include "v_text.h"
#include "gi.h"

#include "doomdef.h"

EXTERN_CVAR (Float, snd_sfxvolume)
CVAR (Int, snd_samplerate, 0, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Int, snd_buffersize, 0, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (String, snd_output, "default", CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

// killough 2/21/98: optionally use varying pitched sounds
CVAR (Bool, snd_pitched, false, CVAR_ARCHIVE)

SoundRenderer *GSnd;


//
// SFX API
//

//==========================================================================
//
// CVAR snd_sfxvolume
//
// Maximum volume of a sound effect.
//==========================================================================

CUSTOM_CVAR (Float, snd_sfxvolume, 1.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL)
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
	bool nosound = !!Args->CheckParm ("-nosfx") || !!Args->CheckParm ("-nosound");

	if (nosound)
	{
		I_InitMusic ();
		return;
	}

	GSnd = new FMODSoundRenderer;

	if (!GSnd->IsValid ())
	{
		delete GSnd;
		GSnd = NULL;
		Printf (TEXTCOLOR_RED"Sound init failed. Using nosound.\n");
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
		return "No sound";
	}
}

SoundRenderer::SoundRenderer ()
{
}

SoundRenderer::~SoundRenderer ()
{
}

FString SoundRenderer::GatherStats ()
{
	return "No stats for this sound renderer.";
}

short *SoundRenderer::DecodeSample(int outlen, const void *coded, int sizebytes, ECodecType type)
{
	return NULL;
}

void SoundRenderer::DrawWaveDebug(int mode)
{
}

SoundStream::~SoundStream ()
{
}

bool SoundStream::SetPosition(int pos)
{
	return false;
}

FString SoundStream::GetStats()
{
	return "No stream stats available.";
}
