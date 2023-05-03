/*
** i_music.cpp
** Plays music
**
**---------------------------------------------------------------------------
** Copyright 1998-2010 Randy Heit
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

#ifndef _WIN32
#include <sys/types.h>
#include <sys/wait.h>
#endif

#include <zlib.h>

#include <zmusic.h>
#include "m_argv.h"
#include "filesystem.h"
#include "c_dispatch.h"

#include "stats.h"
#include "cmdlib.h"
#include "c_cvars.h"
#include "c_console.h"
#include "v_text.h"
#include "i_sound.h"
#include "i_soundfont.h"
#include "s_music.h"
#include "filereadermusicinterface.h"



void I_InitSoundFonts();

EXTERN_CVAR (Int, snd_samplerate)
EXTERN_CVAR (Int, snd_mididevice)
EXTERN_CVAR(Float, snd_mastervolume)
int		nomusic = 0;

//==========================================================================
//
// CVAR snd_musicvolume
//
// Maximum volume of MOD/stream music.
//==========================================================================

CUSTOM_CVARD(Float, snd_musicvolume, 0.5, CVAR_ARCHIVE|CVAR_GLOBALCONFIG, "controls music volume")
{
	if (self < 0.f)
		self = 0.f;
	else if (self > 1.f)
		self = 1.f;
	else
	{
		// Set general music volume.
		ChangeMusicSetting(zmusic_snd_musicvolume, nullptr, self);
		if (GSnd != nullptr)
		{
			GSnd->SetMusicVolume(clamp<float>(self * relative_volume * snd_mastervolume, 0, 1));
		}
		// For music not implemented through the digital sound system,
		// let them know about the change.
		if (mus_playing.handle != nullptr)
		{
			ZMusic_VolumeChanged(mus_playing.handle);
		}
		else
		{ // If the music was stopped because volume was 0, start it now.
			S_RestartMusic();
		}
	}
}

CUSTOM_CVARD(Bool, mus_enabled, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG, "enables/disables music")
{
	if (self) S_RestartMusic();
	else S_StopMusic(true);
}

//==========================================================================
//
// Callbacks for the music system.
//
//==========================================================================

static void zmusic_printfunc(int severity, const char* msg)
{
	if (severity >= ZMUSIC_MSG_FATAL)
	{
		I_FatalError("%s", msg);
	}
	else if (severity >= ZMUSIC_MSG_ERROR)
	{
		Printf(TEXTCOLOR_RED "%s\n", msg);
	}
	else if (severity >= ZMUSIC_MSG_WARNING)
	{
		Printf(TEXTCOLOR_YELLOW "%s\n", msg);
	}
	else if (severity >= ZMUSIC_MSG_NOTIFY)
	{
		DPrintf(DMSG_SPAMMY, "%s\n", msg);
	}
}

static FString strv;
static const char *mus_NicePath(const char* str)
{
	strv = NicePath(str);
	return strv.GetChars();
}

static const char* mus_pathToSoundFont(const char* sfname, int type)
{
	auto info = sfmanager.FindSoundFont(sfname, type);
	return info ? info->mFilename.GetChars() : nullptr;
}

static void* mus_openSoundFont(const char* sfname, int type)
{
	return sfmanager.OpenSoundFont(sfname, type);
}

static ZMusicCustomReader* mus_sfopenfile(void* handle, const char* fn)
{
	return reinterpret_cast<FSoundFontReader*>(handle)->open_interface(fn);
}

static void mus_sfaddpath(void *handle, const char* path)
{
	reinterpret_cast<FSoundFontReader*>(handle)->AddPath(path);
}

static void mus_sfclose(void* handle)
{
	reinterpret_cast<FSoundFontReader*>(handle)->close();
}

#ifndef ZMUSIC_LITE
//==========================================================================
//
// Pass some basic working data to the music backend
// We do this once at startup for everything available.
//
//==========================================================================

static void SetupGenMidi()
{
	// The OPL renderer should not care about where this comes from.
	// Note: No I_Error here - this needs to be consistent with the rest of the music code.
	auto lump = fileSystem.CheckNumForName("GENMIDI", ns_global);
	if (lump < 0)
	{
		Printf("No GENMIDI lump found. OPL playback not available.\n");
		return;
	}
	auto data = fileSystem.OpenFileReader(lump);

	auto genmidi = data.Read();
	if (genmidi.Size() < 8 + 175 * 36 || memcmp(genmidi.Data(), "#OPL_II#", 8)) return;
	ZMusic_SetGenMidi(genmidi.Data()+8);
}

static void SetupWgOpn()
{
	int lump = fileSystem.CheckNumForFullName("xg.wopn");
	if (lump < 0)
	{
		return;
	}
	FileData data = fileSystem.ReadFile(lump);
	ZMusic_SetWgOpn(data.GetMem(), (uint32_t)data.GetSize());
}

static void SetupDMXGUS()
{
	int lump = fileSystem.CheckNumForName("DMXGUSC", ns_global);
	if (lump < 0) lump = fileSystem.CheckNumForName("DMXGUS", ns_global);
	if (lump < 0)
	{
		return;
	}
	FileData data = fileSystem.ReadFile(lump);
	ZMusic_SetDmxGus(data.GetMem(), (uint32_t)data.GetSize());
}

#endif

//==========================================================================
//
//
//
//==========================================================================

void I_InitMusic(void)
{
    I_InitSoundFonts();

	snd_musicvolume->Callback ();

	nomusic = !!Args->CheckParm("-nomusic") || !!Args->CheckParm("-nosound");

	snd_mididevice->Callback();

	ZMusicCallbacks callbacks{};

	callbacks.MessageFunc = zmusic_printfunc;
	callbacks.NicePath = mus_NicePath;
	callbacks.PathForSoundfont = mus_pathToSoundFont;
	callbacks.OpenSoundFont = mus_openSoundFont;
	callbacks.SF_OpenFile = mus_sfopenfile;
	callbacks.SF_AddToSearchPath = mus_sfaddpath;
	callbacks.SF_Close = mus_sfclose;

	ZMusic_SetCallbacks(&callbacks);
#ifndef ZMUSIC_LITE
	SetupGenMidi();
	SetupDMXGUS();
	SetupWgOpn();
#endif
}


//==========================================================================
//
// 
//
//==========================================================================

void I_SetRelativeVolume(float vol)
{
	relative_volume = (float)vol;
	ChangeMusicSetting(zmusic_relative_volume, nullptr, (float)vol);
	snd_musicvolume->Callback();
}
//==========================================================================
//
// Sets relative music volume. Takes $musicvolume in SNDINFO into consideration
//
//==========================================================================

void I_SetMusicVolume (double factor)
{
	factor = clamp(factor, 0., 2.0);
	I_SetRelativeVolume((float)factor);
}

//==========================================================================
//
// test a relative music volume
//
//==========================================================================

CCMD(testmusicvol)
{
	if (argv.argc() > 1) 
	{
		I_SetRelativeVolume((float)strtod(argv[1], nullptr));
	}
	else
		Printf("Current relative volume is %1.2f\n", relative_volume);
}

//==========================================================================
//
// STAT music
//
//==========================================================================

ADD_STAT(music)
{
	if (mus_playing.handle != nullptr)
	{
		return ZMusic_GetStats(mus_playing.handle);
	}
	return "No song playing";
}

//==========================================================================
//
// Common loader for the dumpers.
//
//==========================================================================

static ZMusic_MidiSource GetMIDISource(const char *fn)
{
	FString src = fn;
	if (src.Compare("*") == 0) src = mus_playing.name;

	auto lump = fileSystem.CheckNumForName(src, ns_music);
	if (lump < 0) lump = fileSystem.CheckNumForFullName(src);
	if (lump < 0)
	{
		Printf("Cannot find MIDI lump %s.\n", src.GetChars());
		return nullptr;
	}

	auto wlump = fileSystem.OpenFileReader(lump);

	uint32_t id[32 / 4];

	if (wlump.Read(id, 32) != 32 || wlump.Seek(-32, FileReader::SeekCur) != 0)
	{
		Printf("Unable to read lump %s\n", src.GetChars());
		return nullptr;
	}
	auto type = ZMusic_IdentifyMIDIType(id, 32);
	if (type == MIDI_NOTMIDI)
	{
		Printf("%s is not MIDI-based.\n", src.GetChars());
		return nullptr;
	}

	auto data = wlump.Read();
	auto source = ZMusic_CreateMIDISource(data.Data(), data.Size(), type);

	if (source == nullptr)
	{
		Printf("Unable to open %s: %s\n", src.GetChars(), ZMusic_GetLastError());
		return nullptr;
	}
	return source;
}

//==========================================================================
//
// CCMD writewave
//
// If the current song can be represented as a waveform, dump it to
// the specified file on disk. The sample rate parameter is merely a
// suggestion, and the dumper is free to ignore it.
//
//==========================================================================

UNSAFE_CCMD (writewave)
{
	if (argv.argc() >= 3 && argv.argc() <= 7)
	{
		auto source = GetMIDISource(argv[1]);
		if (source == nullptr) return;

		EMidiDevice dev = MDEV_DEFAULT;
#ifndef ZMUSIC_LITE
		if (argv.argc() >= 6)
		{
			if (!stricmp(argv[5], "WildMidi")) dev = MDEV_WILDMIDI;
			else if (!stricmp(argv[5], "GUS")) dev = MDEV_GUS;
			else if (!stricmp(argv[5], "Timidity") || !stricmp(argv[5], "Timidity++")) dev = MDEV_TIMIDITY;
			else if (!stricmp(argv[5], "FluidSynth")) dev = MDEV_FLUIDSYNTH;
			else if (!stricmp(argv[5], "OPL")) dev = MDEV_OPL;
			else if (!stricmp(argv[5], "OPN")) dev = MDEV_OPN;
			else if (!stricmp(argv[5], "ADL")) dev = MDEV_ADL;
			else
			{
				Printf("%s: Unknown MIDI device\n", argv[5]);
				return;
			}
		}
#endif
		// We must stop the currently playing music to avoid interference between two synths. 
		auto savedsong = mus_playing;
		S_StopMusic(true);
		if (dev == MDEV_DEFAULT && snd_mididevice >= 0) dev = MDEV_FLUIDSYNTH;	// The Windows system synth cannot dump a wave.
		if (!ZMusic_MIDIDumpWave(source, dev, argv.argc() < 6 ? nullptr : argv[6], argv[2], argv.argc() < 4 ? 0 : (int)strtol(argv[3], nullptr, 10), argv.argc() < 5 ? 0 : (int)strtol(argv[4], nullptr, 10)))
		{
			Printf("MIDI dump of %s failed: %s\n",argv[1], ZMusic_GetLastError());
		}

		S_ChangeMusic(savedsong.name, savedsong.baseorder, savedsong.loop, true);
	}
	else
	{
		Printf ("Usage: writewave <midi> <filename> [subsong] [sample rate] [synth] [soundfont]\n"
		" - use '*' as song name to dump the currently playing song\n"
		" - use 0 for subsong and sample rate to play the default\n");
	}
}

//==========================================================================
//
// CCMD writemidi
//
// Writes a given MIDI song to disk. This does not affect playback anymore,
// like older versions did.
//
//==========================================================================

UNSAFE_CCMD(writemidi)
{
	if (argv.argc() != 3)
	{
		Printf("Usage: writemidi <midisong> <filename> - use '*' as song name to dump the currently playing song\n");
		return;
	}
	auto source = GetMIDISource(argv[1]);
	if (source == nullptr)
	{
		Printf("Unable to open %s: %s\n", argv[1], ZMusic_GetLastError());
		return;
	}
	if (!ZMusic_WriteSMF(source, argv[1], 1))
	{
		Printf("Unable to write %s\n", argv[1]);
	}
}
