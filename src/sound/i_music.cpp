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
#include "mus2midi.h"
#endif

#include <zlib.h>

#include "i_musicinterns.h"
#include "m_argv.h"
#include "w_wad.h"
#include "c_dispatch.h"
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
void I_InitSoundFonts();

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
		if (GSnd != nullptr)
		{
			GSnd->SetMusicVolume(clamp<float>(self * relative_volume, 0, 1));
		}
		// For music not implemented through the digital sound system,
		// let them know about the change.
		if (currSong != nullptr)
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

    I_InitSoundFonts();

	snd_musicvolume.Callback ();

	nomusic = !!Args->CheckParm("-nomusic") || !!Args->CheckParm("-nosound");

#ifdef _WIN32
	I_InitMusicWin32 ();
#endif // _WIN32
	
	if (!setatterm)
	{
		setatterm = true;
		atterm (I_ShutdownMusicExit);
	
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
		assert (currSong == nullptr);
	}
	Timidity::FreeAll();
	if (onexit)
	{
		WildMidi_Shutdown();
		TimidityPP_Shutdown();
	}
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
	if (currSong == this) currSong = nullptr;
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
		currSong = nullptr;
		
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
	return nullptr;
}

MusInfo *MusInfo::GetWaveDumper(const char *filename, int rate)
{
	return nullptr;
}

//==========================================================================
//
// create a source based on MIDI file type
//
//==========================================================================

static MIDISource *CreateMIDISource(FileReader &reader, EMIDIType miditype)
{
	MIDISource *source = nullptr;
	switch (miditype)
	{
	case MIDI_MUS:
		return new MUSSong2(reader);

	case MIDI_MIDI:
		return new MIDISong2(reader);

	case MIDI_HMI:
		return new HMISong(reader);

	case MIDI_XMI:
		return new XMISong(reader);

	default:
		return nullptr;
	}
}

//==========================================================================
//
// create a streamer
//
//==========================================================================

static MIDIStreamer *CreateMIDIStreamer(EMidiDevice devtype, const char *args)
{
	auto me = new MIDIStreamer(devtype, args);
	return me;
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

MusInfo *I_RegisterSong (FileReader &reader, MidiDeviceSetting *device)
{
	MusInfo *info = nullptr;
	const char *fmt;
	uint32_t id[32/4];

	if (nomusic)
	{
		return nullptr;
	}

	if(reader.Read(id, 32) != 32 || reader.Seek(-32, FileReader::SeekCur) != 0)
	{
		return nullptr;
	}

    // Check for gzip compression. Some formats are expected to have players
    // that can handle it, so it simplifies things if we make all songs
    // gzippable.
	if ((id[0] & MAKE_ID(255, 255, 255, 0)) == GZIP_ID)
	{

		if (!reader.OpenMemoryArray([&reader](TArray<uint8_t> &array)
		{
			bool res = false;
			auto len = reader.GetLength();
			uint8_t *gzipped = new uint8_t[len];
			if (reader.Read(gzipped, len) == len)
			{
				res = ungzip(gzipped, (int)len, array);
			}
			delete[] gzipped;
			return res;
		}))
		{
			return nullptr;
		}

		if (reader.Read(id, 32) != 32 || reader.Seek(-32, FileReader::SeekCur) != 0)
		{
			return nullptr;
		}
	}

	EMIDIType miditype = IdentifyMIDIType(id, sizeof(id));
	if (miditype != MIDI_NOTMIDI)
	{
		auto source = CreateMIDISource(reader, miditype);
		if (source == nullptr) return nullptr;
		if (!source->isValid())
		{
			delete source;
			return nullptr;
		}
		
		EMidiDevice devtype = device == nullptr? MDEV_DEFAULT : (EMidiDevice)device->device;
#ifndef _WIN32
		// non-Windows platforms don't support MDEV_MMAPI so map to MDEV_SNDSYS
		if (devtype == MDEV_MMAPI)
			devtype = MDEV_SNDSYS;
#endif

		MIDIStreamer *streamer = CreateMIDIStreamer(devtype, device != nullptr? device->args.GetChars() : "");
		if (streamer == nullptr)
		{
			delete source;
			return nullptr;
		}
		streamer->SetMIDISource(source);
		info = streamer;
	}

	// Check for various raw OPL formats
	else if (
		(id[0] == MAKE_ID('R','A','W','A') && id[1] == MAKE_ID('D','A','T','A')) ||		// Rdos Raw OPL
		(id[0] == MAKE_ID('D','B','R','A') && id[1] == MAKE_ID('W','O','P','L')) ||		// DosBox Raw OPL
		(id[0] == MAKE_ID('A','D','L','I') && *((uint8_t *)id + 4) == 'B'))		// Martin Fernandez's modified IMF
	{
		info = new OPLMUSSong (reader, device != nullptr? device->args.GetChars() : "");
	}
	// Check for game music
	else if ((fmt = GME_CheckFormat(id[0])) != nullptr && fmt[0] != '\0')
	{
		info = GME_OpenSong(reader, fmt);
	}
	// Check for module formats
	else
	{
		info = MOD_OpenSong(reader);
	}
	if (info == nullptr)
	{
		info = SndFile_OpenSong(reader);
	}

    if (info == nullptr)
    {
        // Check for CDDA "format"
        if (id[0] == (('R')|(('I')<<8)|(('F')<<16)|(('F')<<24)))
        {
            uint32_t subid;

            reader.Seek(8, FileReader::SeekCur);
            if (reader.Read (&subid, 4) != 4)
            {
                return nullptr;
            }
            reader.Seek(-12, FileReader::SeekCur);

            if (subid == (('C')|(('D')<<8)|(('D')<<16)|(('A')<<24)))
            {
                // This is a CDDA file
                info = new CDDAFile (reader);
            }
        }
    }

	if (info && !info->IsValid ())
	{
		delete info;
		info = nullptr;
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
		info = nullptr;
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
	if (currSong != nullptr)
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
		relative_volume = (float)strtod(argv[1], nullptr);
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
	if (currSong != nullptr)
	{
		return currSong->GetStats();
	}
	return "No song playing";
}

//==========================================================================
//
// Common loader for the dumpers.
//
//==========================================================================
extern MusPlayingInfo mus_playing;

static MIDISource *GetMIDISource(const char *fn)
{
	FString src = fn;
	if (src.Compare("*") == 0) src = mus_playing.name;

	auto lump = Wads.CheckNumForName(src, ns_music);
	if (lump < 0) lump = Wads.CheckNumForFullName(src);
	if (lump < 0)
	{
		Printf("Cannot find MIDI lump %s.\n", src.GetChars());
		return nullptr;
	}

	auto wlump = Wads.OpenLumpReader(lump);
	uint32_t id[32 / 4];

	if (wlump.Read(id, 32) != 32 || wlump.Seek(-32, FileReader::SeekCur) != 0)
	{
		Printf("Unable to read lump %s\n", src.GetChars());
		return nullptr;
	}

	auto type = IdentifyMIDIType(id, 32);
	auto source = CreateMIDISource(wlump, type);

	if (source == nullptr)
	{
		Printf("%s is not MIDI-based.\n", src.GetChars());
		return nullptr;
	}
	return source;
}
//==========================================================================
//
// CCMD writeopl
//
// If the current song can be played with OPL instruments, dump it to
// the specified file on disk.
//
//==========================================================================

UNSAFE_CCMD (writeopl)
{
	if (argv.argc() >= 3 && argv.argc() <= 7)
	{
		auto source = GetMIDISource(argv[1]);
		if (source == nullptr) return;

		// We must stop the currently playing music to avoid interference between two synths. 
		auto savedsong = mus_playing;
		S_StopMusic(true);
		auto streamer = new MIDIStreamer(MDEV_OPL, nullptr);
		streamer->SetMIDISource(source);
		streamer->DumpOPL(argv[2], argv.argc() <4 ? 0 : (int)strtol(argv[3], nullptr, 10));
		delete streamer;
		S_ChangeMusic(savedsong.name, savedsong.baseorder, savedsong.loop, true);

	}
	else
	{
		Printf("Usage: writeopl <midi> <filename> [subsong]\n"
			" - use '*' as song name to dump the currently playing song\n"
			" - use 0 for subsong to play the default\n");
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

UNSAFE_CCMD (writewave)
{
	if (argv.argc() >= 3 && argv.argc() <= 7)
	{
		auto source = GetMIDISource(argv[1]);
		if (source == nullptr) return;

		EMidiDevice dev = MDEV_DEFAULT;

		if (argv.argc() >= 6)
		{
			if (!stricmp(argv[5], "WildMidi")) dev = MDEV_WILDMIDI;
			else if (!stricmp(argv[5], "GUS")) dev = MDEV_GUS;
			else if (!stricmp(argv[5], "Timidity") || !stricmp(argv[5], "Timidity++")) dev = MDEV_TIMIDITY;
			else if (!stricmp(argv[5], "FluidSynth")) dev = MDEV_FLUIDSYNTH;
			else if (!stricmp(argv[5], "OPL")) dev = MDEV_OPL;
			else
			{
				Printf("%s: Unknown MIDI device\n", argv[5]);
				return;
			}
		}
		// We must stop the currently playing music to avoid interference between two synths. 
		auto savedsong = mus_playing;
		S_StopMusic(true);
		if (dev == MDEV_DEFAULT && snd_mididevice >= 0) dev = MDEV_FLUIDSYNTH;	// The Windows system synth cannot dump a wave.
		auto streamer = new MIDIStreamer(dev, argv.argc() < 6 ? nullptr : argv[6]);
		streamer->SetMIDISource(source);
		streamer->DumpWave(argv[2], argv.argc() <4? 0: (int)strtol(argv[3], nullptr, 10), argv.argc() <5 ? 0 : (int)strtol(argv[4], nullptr, 10));
		delete streamer;
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
	if (source == nullptr) return;

	TArray<uint8_t> midi;
	bool success;

	source->CreateSMF(midi, 1);
	auto f = FileWriter::Open(argv[2]);
	if (f == nullptr)
	{
		Printf("Could not open %s.\n", argv[2]);
		return;
	}
	success = (f->Write(&midi[0], midi.Size()) == (size_t)midi.Size());
	delete f;

	if (!success)
	{
		Printf("Could not write to music file %s.\n", argv[2]);
	}
}
