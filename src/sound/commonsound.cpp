/*
** commonsound.cpp
** Common definitions for all sound system backends.
**
**---------------------------------------------------------------------------
** Copyright 1998-2009 Randy Heit
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

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
extern HWND Window;
#define USE_WINDOWS_DWORD
#else
#define FALSE 0
#define TRUE 1
#endif
#ifdef __APPLE__
#include <stdlib.h>
#elif __sun
#include <alloca.h>
#else
#include <malloc.h>
#endif

#include "except.h"
#include "templates.h"
#include "i_sound.h"
#include "c_cvars.h"
#include "i_system.h"
#include "i_music.h"
#include "v_text.h"
#include "v_video.h"
#include "v_palette.h"
#include "cmdlib.h"
#include "s_sound.h"
#include "files.h"

// MACROS ------------------------------------------------------------------

// killough 2/21/98: optionally use varying pitched sounds
#define PITCH(freq,pitch) (snd_pitched ? ((freq)*(pitch))/128.f : float(freq))

// Just some extra for music and whatever
#define NUM_EXTRA_SOFTWARE_CHANNELS		1

#define MAX_CHANNELS				256

#define SPECTRUM_SIZE				256

// PUBLIC DATA DEFINITIONS -------------------------------------------------

ReverbContainer *ForcedEnvironment;

CVAR (Int, snd_driver, 0, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Int, snd_buffercount, 0, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, snd_hrtf, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, snd_waterreverb, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (String, snd_resampler, "Linear", CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (String, snd_speakermode, "Auto", CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (String, snd_output_format, "PCM-16", CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (String, snd_midipatchset, "", CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, snd_profile, false, 0)

// Underwater low-pass filter cutoff frequency. Set to 0 to disable the filter.
CUSTOM_CVAR (Float, snd_waterlp, 250, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	// Clamp to the DSP unit's limits.
	if (*self < 10 && *self != 0)
	{
		self = 10;
	}
	else if (*self > 22000)
	{
		self = 22000;
	}
}
