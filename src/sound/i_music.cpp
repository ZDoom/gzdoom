/*
** i_music.cpp
** Plays music
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
#else
#include <SDL.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <wordexp.h>
#include <stdio.h>
#include "mus2midi.h"
#define FALSE 0
#define TRUE 1
extern void ChildSigHandler (int signum);
#endif

#include <ctype.h>
#include <assert.h>
#include <stdio.h>

#include "i_musicinterns.h"
#include "doomtype.h"
#include "m_argv.h"
#include "i_music.h"
#include "w_wad.h"
#include "c_console.h"
#include "c_dispatch.h"
#include "i_system.h"
#include "i_sound.h"
#include "m_swap.h"
#include "i_cd.h"
#include "tempfiles.h"
#include "templates.h"
#include "stats.h"
#include "timidity/timidity.h"

EXTERN_CVAR (Int, snd_samplerate)
EXTERN_CVAR (Int, snd_mididevice)

static bool MusicDown = true;

MusInfo *currSong;
int		nomusic = 0;
float	relative_volume = 1.f;
float	saved_relative_volume = 1.0f;	// this could be used to implement an ACS FadeMusic function

//==========================================================================
//
// CVAR snd_musicvolume
//
// Maximum volume of MOD/stream music.
//==========================================================================

CUSTOM_CVAR (Float, snd_musicvolume, 0.5f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 0.f)
		self = 0.f;
	else if (self > 1.f)
		self = 1.f;
	else
	{
		if (GSnd != NULL)
		{
			// Set general music volume.
			GSnd->SetMusicVolume(clamp<float>(self * relative_volume, 0, 1));
		}
		// For music not implemented through the digital sound system,
		// let them know about the change.
		if (currSong != NULL)
		{
			currSong->MusicVolumeChanged();
		}
		else
		{ // If the music was stopped because volume was 0, start it now.
			S_RestartMusic();
		}
	}
}

MusInfo::MusInfo()
: m_Status(STATE_Stopped), m_Looping(false), m_NotStartedYet(true)
{
}

MusInfo::~MusInfo ()
{
}

bool MusInfo::SetPosition (int order)
{
	return false;
}

void MusInfo::Update ()
{
}

void MusInfo::MusicVolumeChanged()
{
}

void MusInfo::TimidityVolumeChanged()
{
}

FString MusInfo::GetStats()
{
	return "No stats available for this song";
}

MusInfo *MusInfo::GetOPLDumper(const char *filename)
{
	return NULL;
}

void I_InitMusic (void)
{
	static bool setatterm = false;

	Timidity::LoadConfig();

	snd_musicvolume.Callback ();

	nomusic = !!Args->CheckParm("-nomusic") || !!Args->CheckParm("-nosound");

#ifdef _WIN32
	I_InitMusicWin32 ();
#endif // _WIN32
	
	if (!setatterm)
	{
		setatterm = true;
		atterm (I_ShutdownMusic);
	
#ifndef _WIN32
		signal (SIGCHLD, ChildSigHandler);
#endif
	}
	MusicDown = false;
}


void I_ShutdownMusic(void)
{
	if (MusicDown)
		return;
	MusicDown = true;
	if (currSong)
	{
		S_StopMusic (true);
		assert (currSong == NULL);
	}
	Timidity::FreeAll();
#ifdef _WIN32
	I_ShutdownMusicWin32();
#endif // _WIN32
}


void I_PlaySong (void *handle, int _looping, float rel_vol)
{
	MusInfo *info = (MusInfo *)handle;

	if (!info || nomusic)
		return;

	saved_relative_volume = relative_volume = rel_vol;
	info->Stop ();
	info->Play (_looping ? true : false);
	info->m_NotStartedYet = false;
	
	if (info->m_Status == MusInfo::STATE_Playing)
		currSong = info;
	else
		currSong = NULL;
		
	// Notify the sound system of the changed relative volume
	snd_musicvolume.Callback();
}


void I_PauseSong (void *handle)
{
	MusInfo *info = (MusInfo *)handle;

	if (info)
		info->Pause ();
}

void I_ResumeSong (void *handle)
{
	MusInfo *info = (MusInfo *)handle;

	if (info)
		info->Resume ();
}

void I_StopSong (void *handle)
{
	MusInfo *info = (MusInfo *)handle;
	
	if (info)
		info->Stop ();

	if (info == currSong)
		currSong = NULL;
}

void I_UnRegisterSong (void *handle)
{
	MusInfo *info = (MusInfo *)handle;

	if (info)
	{
		delete info;
	}
}

void *I_RegisterSong (const char *filename, char *musiccache, int offset, int len, int device)
{
	FILE *file;
	MusInfo *info = NULL;
	DWORD id;

	if (nomusic)
	{
		return 0;
	}

	if (offset != -1)
	{
		file = fopen (filename, "rb");
		if (file == NULL)
		{
			return 0;
		}

		if (len == 0 && offset == 0)
		{
			fseek (file, 0, SEEK_END);
			len = ftell (file);
			fseek (file, 0, SEEK_SET);
		}
		else
		{
			fseek (file, offset, SEEK_SET);
		}

		if (fread (&id, 4, 1, file) != 1)
		{
			fclose (file);
			return 0;
		}
		fseek (file, -4, SEEK_CUR);
	}
	else 
	{
		file = NULL;
		memcpy(&id, &musiccache[0], 4);
	}

#ifndef _WIN32
	// non-windows platforms don't support MDEV_MIDI so map to MDEV_FMOD
	if (device == MDEV_MMAPI) device = MDEV_FMOD;
#endif


	// Check for MUS format
	if (id == MAKE_ID('M','U','S',0x1a))
	{
		if (GSnd != NULL)
		{
			/*	MUS are played as:
			- OPL: 
				- if explicitly selected by $mididevice 
				- when snd_mididevice  is -3 and no midi device is set for the song

			  Timidity: 
				- if explicitly selected by $mididevice 
				- when snd_mididevice  is -2 and no midi device is set for the song

			  FMod:
				- if explicitly selected by $mididevice 
				- when snd_mididevice  is -1 and no midi device is set for the song
				- as fallback when both OPL and Timidity failed unless snd_mididevice is >= 0

			  MMAPI (Win32 only):
				- if explicitly selected by $mididevice (non-Win32 redirects this to FMOD)
				- when snd_mididevice  is >= 0 and no midi device is set for the song
				- as fallback when both OPL and Timidity failed and snd_mididevice is >= 0
			*/
			if ((snd_mididevice == -3 && device == MDEV_DEFAULT) || device == MDEV_OPL)
			{
				info = new MUSSong2 (file, musiccache, len, MIDI_OPL);
			}
			else if (device == MDEV_TIMIDITY || (device == MDEV_DEFAULT && snd_mididevice == -2))
			{
				info = new TimiditySong (file, musiccache, len);
			}
			else if ((snd_mididevice == -4 && device == MDEV_DEFAULT) && GSnd != NULL)
			{
				info = new MUSSong2(file, musiccache, len, MIDI_Timidity);
			}
			if (info != NULL && !info->IsValid())
			{
				delete info;
				info = NULL;
				device = MDEV_DEFAULT;
			}
			if (info == NULL && (snd_mididevice == -1 || device == MDEV_FMOD) && device != MDEV_MMAPI)
			{
				TArray<BYTE> midi;
				bool midi_made = false;

				if (file == NULL)
				{
					midi_made = ProduceMIDI((BYTE *)musiccache, midi);
				}
				else
				{
					BYTE *mus = new BYTE[len];
					size_t did_read = fread(mus, 1, len, file);
					if (did_read == (size_t)len)
					{
						midi_made = ProduceMIDI(mus, midi);
					}
					fseek(file, -(long)did_read, SEEK_CUR);
					delete[] mus;
				}
				if (midi_made)
				{
					info = new StreamSong((char *)&midi[0], -1, midi.Size());
					if (!info->IsValid())
					{
						delete info;
						info = NULL;
					}
				}
			}
		}
#ifdef _WIN32
		if (info == NULL)
		{
			info = new MUSSong2 (file, musiccache, len, MIDI_Win);
		}
#endif // _WIN32
	}
	// Check for MIDI format
	else 
	{

		if (id == MAKE_ID('M','T','h','d'))
		{
			// This is a midi file

			/*	MIDI are played as:
			  OPL: 
				- if explicitly selected by $mididevice 
				- when snd_mididevice  is -3 and no midi device is set for the song

			  Timidity: 
				- if explicitly selected by $mididevice 
				- when snd_mididevice  is -2 and no midi device is set for the song

			  FMod:
				- if explicitly selected by $mididevice 
				- when snd_mididevice  is -1 and no midi device is set for the song
				- as fallback when Timidity failed unless snd_mididevice is >= 0

			  MMAPI (Win32 only):
				- if explicitly selected by $mididevice (non-Win32 redirects this to FMOD)
				- when snd_mididevice  is >= 0 and no midi device is set for the song
				- as fallback when Timidity failed and snd_mididevice is >= 0
			*/
			if ((device == MDEV_OPL || (snd_mididevice == -3 && device == MDEV_DEFAULT)) && GSnd != NULL)
			{
				info = new MIDISong2 (file, musiccache, len, MIDI_OPL);
			}
			else if ((device == MDEV_TIMIDITY || (snd_mididevice == -2 && device == MDEV_DEFAULT)) && GSnd != NULL)
			{
				info = new TimiditySong (file, musiccache, len);
			}
			else if ((snd_mididevice == -4 && device == MDEV_DEFAULT) && GSnd != NULL)
			{
				info = new MIDISong2(file, musiccache, len, MIDI_Timidity);
			}
			if (info != NULL && !info->IsValid())
			{
				delete info;
				info = NULL;
				device = MDEV_DEFAULT;
			}
#ifdef _WIN32
			if (info == NULL && device != MDEV_FMOD && (snd_mididevice >= 0 || device == MDEV_MMAPI))
			{
				info = new MIDISong2 (file, musiccache, len, MIDI_Win);
			}
#endif // _WIN32
		}
		// Check for various raw OPL formats
		else if (len >= 12 &&
			(id == MAKE_ID('R','A','W','A') ||		// Rdos Raw OPL
			 id == MAKE_ID('D','B','R','A') ||		// DosBox Raw OPL
			  id == MAKE_ID('A','D','L','I')))		// Martin Fernandez's modified IMF
		{
			DWORD fullsig[2];

			if (file != NULL)
			{
				if (fread (fullsig, 4, 2, file) != 2)
				{
					fclose (file);
					return 0;
				}
				fseek (file, -8, SEEK_CUR);
			}
			else
			{
				memcpy(fullsig, musiccache, 8);
			}

			if ((fullsig[0] == MAKE_ID('R','A','W','A') && fullsig[1] == MAKE_ID('D','A','T','A')) ||
				(fullsig[0] == MAKE_ID('D','B','R','A') && fullsig[1] == MAKE_ID('W','O','P','L')) ||
				(fullsig[0] == MAKE_ID('A','D','L','I') && (fullsig[1] & MAKE_ID(255,255,0,0)) == MAKE_ID('B',1,0,0)))
			{
				info = new OPLMUSSong (file, musiccache, len);
			}
		}
	}

	if (info == NULL)
	{
		// Check for CDDA "format"
		if (id == (('R')|(('I')<<8)|(('F')<<16)|(('F')<<24)))
		{
			if (file != NULL)
			{
				DWORD subid;

				fseek (file, 8, SEEK_CUR);
				if (fread (&subid, 4, 1, file) != 1)
				{
					fclose (file);
					return 0;
				}
				fseek (file, -12, SEEK_CUR);

				if (subid == (('C')|(('D')<<8)|(('D')<<16)|(('A')<<24)))
				{
					// This is a CDDA file
					info = new CDDAFile (file, len);
				}
			}
		}

		// no FMOD => no modules/streams
		// 1024 bytes is an arbitrary restriction. It's assumed that anything
		// smaller than this can't possibly be a valid music file if it hasn't
		// been identified already, so don't even bother trying to load it.
		// Of course MIDIs shorter than 1024 bytes should pass.
		if (info == NULL && GSnd != NULL && (len >= 1024 || id == MAKE_ID('M','T','h','d')))
		{
			// Let FMOD figure out what it is.
			if (file != NULL)
			{
				fclose (file);
				file = NULL;
			}
			info = new StreamSong (offset >=0 ? filename : musiccache, offset, len);
		}
	}


	if (info && !info->IsValid ())
	{
		delete info;
		info = NULL;
	}

	if (file != NULL)
	{
		fclose (file);
	}

	return info;
}

void *I_RegisterCDSong (int track, int id)
{
	MusInfo *info = new CDSong (track, id);

	if (info && !info->IsValid ())
	{
		delete info;
		info = NULL;
	}

	return info;
}

void I_UpdateMusic()
{
	if (currSong != NULL)
	{
		currSong->Update();
	}
}

// Is the song playing?
bool I_QrySongPlaying (void *handle)
{
	MusInfo *info = (MusInfo *)handle;

	return info ? info->IsPlaying () : false;
}

// Change to a different part of the song
bool I_SetSongPosition (void *handle, int order)
{
	MusInfo *info = (MusInfo *)handle;
	return info ? info->SetPosition (order) : false;
}

// Sets relative music volume. Takes $musicvolume in SNDINFO into consideration
void I_SetMusicVolume (float factor)
{
	factor = clamp<float>(factor, 0, 2.0f);
	relative_volume = saved_relative_volume * factor;
	snd_musicvolume.Callback();
}

CCMD(testmusicvol)
{
	if (argv.argc() > 1) 
	{
		relative_volume = (float)strtod(argv[1], NULL);
		snd_musicvolume.Callback();
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
	if (currSong != NULL)
	{
		return currSong->GetStats();
	}
	return "No song playing";
}

//==========================================================================
//
// CCMD writeopl
//
// If the current song can be played with OPL instruments, dump it to
// the specified file on disk.
//
//==========================================================================

CCMD (writeopl)
{
	if (argv.argc() == 2)
	{
		if (currSong == NULL)
		{
			Printf ("No song is currently playing.\n");
		}
		else
		{
			MusInfo *dumper = currSong->GetOPLDumper(argv[1]);
			if (dumper == NULL)
			{
				Printf ("Current song cannot be saved as OPL data.\n");
			}
			else
			{
				dumper->Play(false);
				delete dumper;
			}
		}
	}
	else
	{
		Printf ("Usage: writeopl <filename>");
	}
}
