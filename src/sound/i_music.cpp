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
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include "mus2midi.h"
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
#include "templates.h"
#include "stats.h"
#include "timidity/timidity.h"
#include "vm.h"

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

extern int MUSHeaderSearch(const uint8_t *head, int len);

EXTERN_CVAR (Int, snd_samplerate)
EXTERN_CVAR (Int, snd_mididevice)

static bool MusicDown = true;

static bool ungzip(uint8_t *data, int size, TArray<uint8_t> &newdata);

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

//==========================================================================
//
//
//
//==========================================================================

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
		atterm (I_ShutdownMusicExit);
	
#ifndef _WIN32
		signal (SIGCHLD, ChildSigHandler);
#endif
	}
	MusicDown = false;
}


//==========================================================================
//
//
//
//==========================================================================

void I_ShutdownMusic(bool onexit)
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
	if (onexit) WildMidi_Shutdown();
}

void I_ShutdownMusicExit()
{
	I_ShutdownMusic(true);
}


//==========================================================================
//
//
//
//==========================================================================

MusInfo::MusInfo()
: m_Status(STATE_Stopped), m_Looping(false), m_NotStartedYet(true)
{
}

MusInfo::~MusInfo ()
{
	if (currSong == this) currSong = NULL;
}

//==========================================================================
//
// starts playing this song
//
//==========================================================================

void MusInfo::Start(bool loop, float rel_vol, int subsong)
{
	if (nomusic) return;

	if (rel_vol > 0.f)
	{
		float factor = relative_volume / saved_relative_volume;
		saved_relative_volume = rel_vol;
		relative_volume = saved_relative_volume * factor;
	}
	Stop ();
	Play (loop, subsong);
	m_NotStartedYet = false;
	
	if (m_Status == MusInfo::STATE_Playing)
		currSong = this;
	else
		currSong = NULL;
		
	// Notify the sound system of the changed relative volume
	snd_musicvolume.Callback();
}

//==========================================================================
//
//
//
//==========================================================================

bool MusInfo::SetPosition (unsigned int ms)
{
	return false;
}

bool MusInfo::IsMIDI() const
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

void MusInfo::GMEDepthChanged(float val)
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

void MusInfo::WildMidiSetOption(int opt, int set)
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

//==========================================================================
//
// create a streamer based on MIDI file type
//
//==========================================================================

static MIDIStreamer *CreateMIDIStreamer(FileReader &reader, EMidiDevice devtype, EMIDIType miditype, const char *args)
{
	switch (miditype)
	{
	case MIDI_MUS:
		return new MUSSong2(reader, devtype, args);

	case MIDI_MIDI:
		return new MIDISong2(reader, devtype, args);

	case MIDI_HMI:
		return new HMISong(reader, devtype, args);

	case MIDI_XMI:
		return new XMISong(reader, devtype, args);

	default:
		return NULL;
	}
}

//==========================================================================
//
// identify MIDI file type
//
//==========================================================================

static EMIDIType IdentifyMIDIType(uint32_t *id, int size)
{
	// Check for MUS format
	// Tolerate sloppy wads by searching up to 32 bytes for the header
	if (MUSHeaderSearch((uint8_t*)id, size) >= 0)
	{
		return MIDI_MUS;
	}
	// Check for HMI format
	else 
	if (id[0] == MAKE_ID('H','M','I','-') &&
		id[1] == MAKE_ID('M','I','D','I') &&
		id[2] == MAKE_ID('S','O','N','G'))
	{
		return MIDI_HMI;
	}
	// Check for HMP format
	else
	if (id[0] == MAKE_ID('H','M','I','M') &&
		id[1] == MAKE_ID('I','D','I','P'))
	{
		return MIDI_HMI;
	}
	// Check for XMI format
	else
	if ((id[0] == MAKE_ID('F','O','R','M') &&
		 id[2] == MAKE_ID('X','D','I','R')) ||
		((id[0] == MAKE_ID('C','A','T',' ') || id[0] == MAKE_ID('F','O','R','M')) &&
		 id[2] == MAKE_ID('X','M','I','D')))
	{
		return MIDI_XMI;
	}
	// Check for MIDI format
	else if (id[0] == MAKE_ID('M','T','h','d'))
	{
		return MIDI_MIDI;
	}
	else
	{
		return MIDI_NOTMIDI;
	}
}

//==========================================================================
//
// identify a music lump's type and set up a player for it
//
//==========================================================================

MusInfo *I_RegisterSong (FileReader *reader, MidiDeviceSetting *device)
{
	MusInfo *info = NULL;
	const char *fmt;
	uint32_t id[32/4];

	if (nomusic)
	{
		delete reader;
		return 0;
	}

	if(reader->Read(id, 32) != 32 || reader->Seek(-32, SEEK_CUR) != 0)
	{
		delete reader;
		return 0;
	}

    // Check for gzip compression. Some formats are expected to have players
    // that can handle it, so it simplifies things if we make all songs
    // gzippable.
	if ((id[0] & MAKE_ID(255, 255, 255, 0)) == GZIP_ID)
	{
		int len = reader->GetLength();
		uint8_t *gzipped = new uint8_t[len];
		if (reader->Read(gzipped, len) != len)
		{
			delete[] gzipped;
			delete reader;
			return NULL;
		}
		delete reader;

		MemoryArrayReader *memreader = new MemoryArrayReader(NULL, 0);
		if (!ungzip(gzipped, len, memreader->GetArray()))
		{
			delete[] gzipped;
			delete memreader;
			return 0;
		}
		delete[] gzipped;
		memreader->UpdateLength();

		if (memreader->Read(id, 32) != 32 || memreader->Seek(-32, SEEK_CUR) != 0)
		{
			delete memreader;
			return 0;
		}
		reader = memreader;
	}

	EMIDIType miditype = IdentifyMIDIType(id, sizeof(id));
	if (miditype != MIDI_NOTMIDI)
	{
		EMidiDevice devtype = device == NULL? MDEV_DEFAULT : (EMidiDevice)device->device;
#ifndef _WIN32
		// non-Windows platforms don't support MDEV_MMAPI so map to MDEV_SNDSYS
		if (devtype == MDEV_MMAPI)
			devtype = MDEV_SNDSYS;
#endif

retry_as_sndsys:
		info = CreateMIDIStreamer(*reader, devtype, miditype, device != NULL? device->args.GetChars() : "");
		if (info != NULL && !info->IsValid())
		{
			delete info;
			info = NULL;
		}
		if (info == NULL && devtype != MDEV_SNDSYS && snd_mididevice < 0)
		{
			devtype = MDEV_SNDSYS;
			goto retry_as_sndsys;
		}
#ifdef _WIN32
		if (info == NULL && devtype != MDEV_MMAPI && snd_mididevice >= 0)
		{
			info = CreateMIDIStreamer(*reader, MDEV_MMAPI, miditype, "");
		}
#endif
	}

	// Check for various raw OPL formats
	else if (
		(id[0] == MAKE_ID('R','A','W','A') && id[1] == MAKE_ID('D','A','T','A')) ||		// Rdos Raw OPL
		(id[0] == MAKE_ID('D','B','R','A') && id[1] == MAKE_ID('W','O','P','L')) ||		// DosBox Raw OPL
		(id[0] == MAKE_ID('A','D','L','I') && *((uint8_t *)id + 4) == 'B'))		// Martin Fernandez's modified IMF
	{
		info = new OPLMUSSong (*reader, device != NULL? device->args.GetChars() : "");
	}
	// Check for game music
	else if ((fmt = GME_CheckFormat(id[0])) != NULL && fmt[0] != '\0')
	{
		info = GME_OpenSong(*reader, fmt);
	}
	// Check for module formats
	else
	{
		info = MOD_OpenSong(*reader);
	}
	if (info == nullptr)
	{
		info = SndFile_OpenSong(*reader);
		if (info != nullptr) reader = nullptr;
	}

    if (info == NULL)
    {
        // Check for CDDA "format"
        if (id[0] == (('R')|(('I')<<8)|(('F')<<16)|(('F')<<24)))
        {
            uint32_t subid;

            reader->Seek(8, SEEK_CUR);
            if (reader->Read (&subid, 4) != 4)
            {
                delete reader;
                return 0;
            }
            reader->Seek(-12, SEEK_CUR);

            if (subid == (('C')|(('D')<<8)|(('D')<<16)|(('A')<<24)))
            {
                // This is a CDDA file
                info = new CDDAFile (*reader);
            }
        }

        // no support in sound system => no modules/streams
        // 1024 bytes is an arbitrary restriction. It's assumed that anything
        // smaller than this can't possibly be a valid music file if it hasn't
        // been identified already, so don't even bother trying to load it.
        // Of course MIDIs shorter than 1024 bytes should pass.
        if (info == NULL && (reader->GetLength() >= 1024 || id[0] == MAKE_ID('M','T','h','d')))
        {
            // Let the sound system figure out what it is.
            info = new StreamSong (reader);
			// Assumed ownership
			reader = NULL;
        }
    }

	if (reader != NULL) delete reader;

	if (info && !info->IsValid ())
	{
		delete info;
		info = NULL;
	}

	return info;
}

//==========================================================================
//
// play CD music
//
//==========================================================================

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

static bool ungzip(uint8_t *data, int complen, TArray<uint8_t> &newdata)
{
	const uint8_t *max = data + complen - 8;
	const uint8_t *compstart = data + 10;
	uint8_t flags = data[3];
	unsigned isize;
	z_stream stream;
	int err;

	// Find start of compressed data stream
	if (flags & GZIP_FEXTRA)
	{
		compstart += 2 + LittleShort(*(uint16_t *)(data + 10));
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
		return false;
	}

	// Decompress
	isize = LittleLong(*(uint32_t *)(data + complen - 4));
    newdata.Resize(isize);

	stream.next_in = (Bytef *)compstart;
	stream.avail_in = (uInt)(max - compstart);
	stream.next_out = &newdata[0];
	stream.avail_out = isize;
	stream.zalloc = (alloc_func)0;
	stream.zfree = (free_func)0;

	err = inflateInit2(&stream, -MAX_WBITS);
	if (err != Z_OK)
	{
		return false;
	}
	err = inflate(&stream, Z_FINISH);
	if (err != Z_STREAM_END)
	{
		inflateEnd(&stream);
		return false;
	}
	err = inflateEnd(&stream);
	if (err != Z_OK)
	{
		return false;
	}
	return true;
}

//==========================================================================
//
// 
//
//==========================================================================

void I_UpdateMusic()
{
	if (currSong != NULL)
	{
		currSong->Update();
	}
}

//==========================================================================
//
// Sets relative music volume. Takes $musicvolume in SNDINFO into consideration
//
//==========================================================================

void I_SetMusicVolume (float factor)
{
	factor = clamp<float>(factor, 0, 2.0f);
	relative_volume = saved_relative_volume * factor;
	snd_musicvolume.Callback();
}

DEFINE_ACTION_FUNCTION(DObject, SetMusicVolume)
{
	PARAM_PROLOGUE;
	PARAM_FLOAT(vol);
	I_SetMusicVolume((float)vol);
	return 0;
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

//==========================================================================
//
// CCMD writemidi
//
// If the currently playing song is a MIDI variant, write it to disk.
// If successful, the current song will restart, since MIDI file generation
// involves a simulated playthrough of the song.
//
//==========================================================================

CCMD (writemidi)
{
	if (argv.argc() != 2)
	{
		Printf("Usage: writemidi <filename>");
		return;
	}
	if (currSong == NULL)
	{
		Printf("No song is currently playing.\n");
		return;
	}
	if (!currSong->IsMIDI())
	{
		Printf("Current song is not MIDI-based.\n");
		return;
	}

	TArray<uint8_t> midi;
	bool success;

	static_cast<MIDIStreamer *>(currSong)->CreateSMF(midi, 1);
	auto f = FileWriter::Open(argv[1]);
	if (f == NULL)
	{
		Printf("Could not open %s.\n", argv[1]);
		return;
	}
	success = (f->Write(&midi[0], midi.Size()) == (size_t)midi.Size());
	delete f;

	if (!success)
	{
		Printf("Could not write to music file.\n");
	}
}
