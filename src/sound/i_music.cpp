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
#include "s_sound.h"
#include "m_swap.h"
#include "i_cd.h"
#include "tempfiles.h"
#include "templates.h"
#include "stats.h"
#include "timidity/timidity.h"

#define GZIP_ID1		31
#define GZIP_ID2		139
#define GZIP_CM			8
#define GZIP_ID			MAKE_ID(GZIP_ID1,GZIP_ID2,GZIP_CM,0)

#define GZIP_FTEXT		1
#define GZIP_FHCRC		2
#define GZIP_FEXTRA		4
#define GZIP_FNAME		8
#define GZIP_FCOMMENT	16

enum EMIDIType
{
	MIDI_NOTMIDI,
	MIDI_MIDI,
	MIDI_HMI,
	MIDI_XMI,
	MIDI_MUS
};

extern int MUSHeaderSearch(const BYTE *head, int len);

EXTERN_CVAR (Int, snd_samplerate)
EXTERN_CVAR (Int, snd_mididevice)

static bool MusicDown = true;

static BYTE *ungzip(BYTE *data, int *size);

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
		// Set general music volume.
		if (GSnd != NULL)
		{
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

bool MusInfo::SetPosition (unsigned int ms)
{
	return false;
}

bool MusInfo::SetSubsong (int subsong)
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

void MusInfo::FluidSettingInt(const char *, int)
{
}

void MusInfo::FluidSettingNum(const char *, double)
{
}

void MusInfo::FluidSettingStr(const char *, const char *)
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

MusInfo *MusInfo::GetWaveDumper(const char *filename, int rate)
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


void I_PlaySong (void *handle, int _looping, float rel_vol, int subsong)
{
	MusInfo *info = (MusInfo *)handle;

	if (!info || nomusic)
		return;

	saved_relative_volume = relative_volume = rel_vol;
	info->Stop ();
	info->Play (!!_looping, subsong);
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

MusInfo *I_RegisterURLSong (const char *url)
{
	StreamSong *song;

	song = new StreamSong(url, 0, 0);
	if (song->IsValid())
	{
		return song;
	}
	delete song;
	return NULL;
}

static MusInfo *CreateMIDISong(FILE *file, const char *filename, BYTE *musiccache, int offset, int len, EMIDIDevice devtype, EMIDIType miditype)
{
	if (devtype == MIDI_Timidity)
	{
		assert(miditype == MIDI_MIDI);
		return new TimiditySong(file, musiccache, len);
	}
	else if (devtype >= MIDI_Null)
	{
		assert(miditype == MIDI_MIDI);
		if (musiccache != NULL)
		{
			return new StreamSong((char *)musiccache, -1, len);
		}
		else
		{
			return new StreamSong(filename, offset, len);
		}
	}
	else if (miditype == MIDI_MUS)
	{
		return new MUSSong2(file, musiccache, len, devtype);
	}
	else if (miditype == MIDI_MIDI)
	{
		return new MIDISong2(file, musiccache, len, devtype);
	}
	else if (miditype == MIDI_HMI)
	{
		return new HMISong(file, musiccache, len, devtype);
	}
	else if (miditype == MIDI_XMI)
	{
		return new XMISong(file, musiccache, len, devtype);
	}
	return NULL;
}

MusInfo *I_RegisterSong (const char *filename, BYTE *musiccache, int offset, int len, int device)
{
	FILE *file;
	MusInfo *info = NULL;
	union
	{
		DWORD id[32/4];
		BYTE idstr[32];
	};
	const char *fmt;
	BYTE *ungzipped;
	int i;

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
		if (len < 32)
		{
			return 0;
		}
		if (fread (id, 4, 32/4, file) != 32/4)
		{
			fclose (file);
			return 0;
		}
		fseek (file, -32, SEEK_CUR);
	}
	else 
	{
		file = NULL;
		if (len < 32)
		{
			return 0;
		}
		for (i = 0; i < 32/4; ++i)
		{
			id[i] = ((DWORD *)musiccache)[i];
		}
	}

#ifndef _WIN32
	// non-Windows platforms don't support MDEV_MMAPI so map to MDEV_FMOD
	if (device == MDEV_MMAPI)
		device = MDEV_FMOD;
#endif

	// Check for gzip compression. Some formats are expected to have players
	// that can handle it, so it simplifies things if we make all songs
	// gzippable.
	ungzipped = NULL;
	if ((id[0] & MAKE_ID(255,255,255,0)) == GZIP_ID)
	{
		if (offset != -1)
		{
			BYTE *gzipped = new BYTE[len];
			if (fread(gzipped, 1, len, file) != (size_t)len)
			{
				delete[] gzipped;
				fclose(file);
				return NULL;
			}
			ungzipped = ungzip(gzipped, &len);
			delete[] gzipped;
		}
		else
		{
			ungzipped = ungzip(musiccache, &len);
		}
		if (ungzipped == NULL)
		{
			fclose(file);
			return NULL;
		}
		musiccache = ungzipped;
		for (i = 0; i < 32/4; ++i)
		{
			id[i] = ((DWORD *)musiccache)[i];
		}
	}

	EMIDIType miditype = MIDI_NOTMIDI;

	// Check for MUS format
	// Tolerate sloppy wads by searching up to 32 bytes for the header
	if (MUSHeaderSearch(idstr, sizeof(idstr)) >= 0)
	{
		miditype = MIDI_MUS;
	}
	// Check for HMI format
	else 
	if (id[0] == MAKE_ID('H','M','I','-') &&
		id[1] == MAKE_ID('M','I','D','I') &&
		id[2] == MAKE_ID('S','O','N','G'))
	{
		miditype = MIDI_HMI;
	}
	// Check for HMP format
	else
	if (id[0] == MAKE_ID('H','M','I','M') &&
		id[1] == MAKE_ID('I','D','I','P'))
	{
		miditype = MIDI_HMI;
	}
	// Check for XMI format
	else
	if ((id[0] == MAKE_ID('F','O','R','M') &&
		 id[2] == MAKE_ID('X','D','I','R')) ||
		((id[0] == MAKE_ID('C','A','T',' ') || id[0] == MAKE_ID('F','O','R','M')) &&
		 id[2] == MAKE_ID('X','M','I','D')))
	{
		miditype = MIDI_XMI;
	}
	// Check for MIDI format
	else if (id[0] == MAKE_ID('M','T','h','d'))
	{
		miditype = MIDI_MIDI;
	}

	if (miditype != MIDI_NOTMIDI)
	{
		TArray<BYTE> midi;
		/* MIDI are played as:
			- OPL: 
				- if explicitly selected by $mididevice 
				- when snd_mididevice  is -3 and no midi device is set for the song

			- Timidity: 
				- if explicitly selected by $mididevice 
				- when snd_mididevice  is -2 and no midi device is set for the song

			- FMod:
				- if explicitly selected by $mididevice 
				- when snd_mididevice  is -1 and no midi device is set for the song
				- as fallback when both OPL and Timidity failed unless snd_mididevice is >= 0

			- MMAPI (Win32 only):
				- if explicitly selected by $mididevice (non-Win32 redirects this to FMOD)
				- when snd_mididevice  is >= 0 and no midi device is set for the song
				- as fallback when both OPL and Timidity failed and snd_mididevice is >= 0
		*/
		EMIDIDevice devtype = MIDI_Null;

		// Choose the type of MIDI device we want.
		if (device == MDEV_FMOD || (snd_mididevice == -1 && device == MDEV_DEFAULT))
		{
			devtype = MIDI_FMOD;
		}
		else if (device == MDEV_TIMIDITY || (snd_mididevice == -2 && device == MDEV_DEFAULT))
		{
			devtype = MIDI_Timidity;
		}
		else if (device == MDEV_OPL || (snd_mididevice == -3 && device == MDEV_DEFAULT))
		{
			devtype = MIDI_OPL;
		}
		else if (device == MDEV_GUS || (snd_mididevice == -4 && device == MDEV_DEFAULT))
		{
			devtype = MIDI_GUS;
		}
#ifdef HAVE_FLUIDSYNTH
		else if (device == MDEV_FLUIDSYNTH || (snd_mididevice == -5 && device == MDEV_DEFAULT))
		{
			devtype = MIDI_Fluid;
		}
#endif
#ifdef _WIN32
		else
		{
			devtype = MIDI_Win;
		}
#endif

retry_as_fmod:
		if (devtype >= MIDI_Null)
		{
			// Convert to standard MIDI for external sequencers.
			MIDIStreamer *streamer;

			if (miditype == MIDI_MIDI)
			{
				streamer = new MIDISong2(file, musiccache, len, MIDI_Null);
			}
			else if (miditype == MIDI_MUS)
			{
				streamer = new MUSSong2(file, musiccache, len, MIDI_Null);
			}
			else if (miditype == MIDI_XMI)
			{
				streamer = new XMISong(file, musiccache, len, MIDI_Null);
			}
			else
			{
				assert(miditype == MIDI_HMI);
				streamer = new HMISong(file, musiccache, len, MIDI_Null);
			}
			if (streamer->IsValid())
			{
				streamer->CreateSMF(midi);
				miditype = MIDI_MIDI;
				musiccache = &midi[0];
				len = midi.Size();
				if (file != NULL)
				{
					fclose(file);
					file = NULL;
				}
			}
			delete streamer;
		}
		info = CreateMIDISong(file, filename, musiccache, offset, len, devtype, miditype);
		if (info != NULL && !info->IsValid())
		{
			delete info;
			info = NULL;
		}
		if (info == NULL && devtype != MIDI_FMOD && snd_mididevice < 0)
		{
			devtype = MIDI_FMOD;
			goto retry_as_fmod;
		}
#ifdef _WIN32
		if (info == NULL && devtype != MIDI_Win && snd_mididevice >= 0)
		{
			info = CreateMIDISong(file, filename, musiccache, offset, len, MIDI_Win, miditype);
		}
#endif
	}

	// Check for various raw OPL formats
	else if (
		(id[0] == MAKE_ID('R','A','W','A') && id[1] == MAKE_ID('D','A','T','A')) ||		// Rdos Raw OPL
		(id[0] == MAKE_ID('D','B','R','A') && id[1] == MAKE_ID('W','O','P','L')) ||		// DosBox Raw OPL
		(id[0] == MAKE_ID('A','D','L','I') && *((BYTE *)id + 4) == 'B'))		// Martin Fernandez's modified IMF
	{
		info = new OPLMUSSong (file, musiccache, len);
	}
	// Check for game music
	else if ((fmt = GME_CheckFormat(id[0])) != NULL && fmt[0] != '\0')
	{
		info = GME_OpenSong(file, musiccache, len, fmt);
	}
	// Check for module formats
	else
	{
		info = MOD_OpenSong(file, musiccache, len);
	}

	if (info == NULL)
	{
		// Check for CDDA "format"
		if (id[0] == (('R')|(('I')<<8)|(('F')<<16)|(('F')<<24)))
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
		if (info == NULL && (len >= 1024 || id[0] == MAKE_ID('M','T','h','d')))
		{
			// Let FMOD figure out what it is.
			if (file != NULL)
			{
				fclose (file);
				file = NULL;
			}
			info = new StreamSong (offset >= 0 ? filename : (const char *)musiccache, offset, len);
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
	if (ungzipped != NULL)
	{
		delete[] ungzipped;
	}

	return info;
}

MusInfo *I_RegisterCDSong (int track, int id)
{
	MusInfo *info = new CDSong (track, id);

	if (info && !info->IsValid ())
	{
		delete info;
		info = NULL;
	}

	return info;
}

//==========================================================================
//
// ungzip
//
// VGZ files are compressed with gzip, so we need to uncompress them before
// handing them to GME.
//
//==========================================================================

BYTE *ungzip(BYTE *data, int *complen)
{
	const BYTE *max = data + *complen - 8;
	const BYTE *compstart = data + 10;
	BYTE flags = data[3];
	unsigned isize;
	BYTE *newdata;
	z_stream stream;
	int err;

	// Find start of compressed data stream
	if (flags & GZIP_FEXTRA)
	{
		compstart += 2 + LittleShort(*(WORD *)(data + 10));
	}
	if (flags & GZIP_FNAME)
	{
		while (compstart < max && *compstart != 0)
		{
			compstart++;
		}
	}
	if (flags & GZIP_FCOMMENT)
	{
		while (compstart < max && *compstart != 0)
		{
			compstart++;
		}
	}
	if (flags & GZIP_FHCRC)
	{
		compstart += 2;
	}
	if (compstart >= max - 1)
	{
		return NULL;
	}

	// Decompress
	isize = LittleLong(*(DWORD *)(data + *complen - 4));
	newdata = new BYTE[isize];

	stream.next_in = (Bytef *)compstart;
	stream.avail_in = (uInt)(max - compstart);
	stream.next_out = newdata;
	stream.avail_out = isize;
	stream.zalloc = (alloc_func)0;
	stream.zfree = (free_func)0;

	err = inflateInit2(&stream, -MAX_WBITS);
	if (err != Z_OK)
	{
		delete[] newdata;
		return NULL;
	}

	err = inflate(&stream, Z_FINISH);
	if (err != Z_STREAM_END)
	{
		inflateEnd(&stream);
		delete[] newdata;
		return NULL;
	}
	err = inflateEnd(&stream);
	if (err != Z_OK)
	{
		delete[] newdata;
		return NULL;
	}
	*complen = isize;
	return newdata;
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
				dumper->Play(false, 0);		// FIXME: Remember subsong.
				delete dumper;
			}
		}
	}
	else
	{
		Printf ("Usage: writeopl <filename>\n");
	}
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

CCMD (writewave)
{
	if (argv.argc() >= 2 && argv.argc() <= 3)
	{
		if (currSong == NULL)
		{
			Printf ("No song is currently playing.\n");
		}
		else
		{
			MusInfo *dumper = currSong->GetWaveDumper(argv[1], argv.argc() == 3 ? atoi(argv[2]) : 0);
			if (dumper == NULL)
			{
				Printf ("Current song cannot be saved as wave data.\n");
			}
			else
			{
				dumper->Play(false, 0);		// FIXME: Remember subsong
				delete dumper;
			}
		}
	}
	else
	{
		Printf ("Usage: writewave <filename> [sample rate]");
	}
}
