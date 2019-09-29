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

#include "i_musicinterns.h"
#include "m_argv.h"
#include "w_wad.h"
#include "c_dispatch.h"
#include "templates.h"
#include "stats.h"
#include "c_console.h"
#include "vm.h"
#include "v_text.h"
#include "i_soundfont.h"
#include "s_music.h"
#include "zmusic/zmusic.h"
#include "zmusic/musinfo.h"
#include "streamsources/streamsource.h"
#include "filereadermusicinterface.h"
#include "../libraries/zmusic/midisources/midisource.h"
#include "../libraries/dumb/include/dumb.h"

#define GZIP_ID1		31
#define GZIP_ID2		139
#define GZIP_CM			8
#define GZIP_ID			MAKE_ID(GZIP_ID1,GZIP_ID2,GZIP_CM,0)

#define GZIP_FTEXT		1
#define GZIP_FHCRC		2
#define GZIP_FEXTRA		4
#define GZIP_FNAME		8
#define GZIP_FCOMMENT	16

extern int MUSHeaderSearch(const uint8_t *head, int len);
void I_InitSoundFonts();
extern "C" void dumb_exit();
extern MusPlayingInfo mus_playing;

EXTERN_CVAR (Int, snd_samplerate)
EXTERN_CVAR (Int, snd_mididevice)

static bool MusicDown = true;

static bool ungzip(uint8_t *data, int size, std::vector<uint8_t> &newdata);

MusInfo *currSong;
int		nomusic = 0;

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
		ChangeMusicSetting(ZMusic::snd_musicvolume, nullptr, self);
		if (GSnd != nullptr)
		{
			GSnd->SetMusicVolume(clamp<float>(self * relative_volume * snd_mastervolume, 0, 1));
		}
		// For music not implemented through the digital sound system,
		// let them know about the change.
		if (mus_playing.handle != nullptr)
		{
			mus_playing.handle->MusicVolumeChanged();
		}
		else
		{ // If the music was stopped because volume was 0, start it now.
			S_RestartMusic();
		}
	}
}

//==========================================================================
//
// Callbacks for the music system.
//
//==========================================================================

static void tim_printfunc(int type, int verbosity_level, const char* fmt, ...)
{
	if (verbosity_level >= 3/*Timidity::VERB_DEBUG*/) return;	// Don't waste time on diagnostics.

	va_list args;
	va_start(args, fmt);
	FString msg;
	msg.VFormat(fmt, args);
	va_end(args);

	switch (type)
	{
	case 2:// Timidity::CMSG_ERROR:
		Printf(TEXTCOLOR_RED "%s\n", msg.GetChars());
		break;

	case 1://Timidity::CMSG_WARNING:
		Printf(TEXTCOLOR_YELLOW "%s\n", msg.GetChars());
		break;

	case 0://Timidity::CMSG_INFO:
		DPrintf(DMSG_SPAMMY, "%s\n", msg.GetChars());
		break;
	}
}

static void wm_printfunc(const char* wmfmt, va_list args)
{
	Printf(TEXTCOLOR_RED);
	VPrintf(PRINT_HIGH, wmfmt, args);
}

//==========================================================================
//
// other callbacks
//
//==========================================================================

static short* dumb_decode_vorbis_(int outlen, const void* oggstream, int sizebytes)
{
	return GSnd->DecodeSample(outlen, oggstream, sizebytes, CODEC_Vorbis);
}

static std::string mus_NicePath(const char* str)
{
	FString strv = NicePath(str);
	return strv.GetChars();
}

static const char* mus_pathToSoundFont(const char* sfname, int type)
{
	auto info = sfmanager.FindSoundFont(sfname, type);
	return info ? info->mFilename.GetChars() : nullptr;
}

static MusicIO::SoundFontReaderInterface* mus_openSoundFont(const char* sfname, int type)
{
	return sfmanager.OpenSoundFont(sfname, type);
}


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
	auto lump = Wads.CheckNumForName("GENMIDI", ns_global);
	if (lump < 0) Printf("No GENMIDI lump found. OPL playback not available.");
	auto data = Wads.OpenLumpReader(lump);

	auto genmidi = data.Read();
	if (genmidi.Size() < 8 + 175 * 36 || memcmp(genmidi.Data(), "#OPL_II#", 8)) return;
	SetGenMidi(genmidi.Data()+8);
}

static void SetupWgOpn()
{
	int lump = Wads.CheckNumForFullName("xg.wopn");
	if (lump < 0)
	{
		return;
	}
	FMemLump data = Wads.ReadLump(lump);
	SetWgOpn(data.GetMem(), (uint32_t)data.GetSize());
}

static void SetupDMXGUS()
{
	int lump = Wads.CheckNumForFullName("DMXGUS");
	if (lump < 0)
	{
		return;
	}
	FMemLump data = Wads.ReadLump(lump);
	SetDmxGus(data.GetMem(), (uint32_t)data.GetSize());
}



//==========================================================================
//
//
//
//==========================================================================

void I_InitMusic (void)
{
    I_InitSoundFonts();

	snd_musicvolume.Callback ();

	nomusic = !!Args->CheckParm("-nomusic") || !!Args->CheckParm("-nosound");

#ifdef _WIN32
	I_InitMusicWin32 ();
#endif // _WIN32
	
	MusicDown = false;

	Callbacks callbacks;

	callbacks.Fluid_MessageFunc = Printf;
	callbacks.GUS_MessageFunc = callbacks.Timidity_Messagefunc = tim_printfunc;
	callbacks.WildMidi_MessageFunc = wm_printfunc;
	callbacks.NicePath = mus_NicePath;
	callbacks.PathForSoundfont = mus_pathToSoundFont;
	callbacks.OpenSoundFont = mus_openSoundFont;
	callbacks.DumbVorbisDecode = dumb_decode_vorbis_;

	SetCallbacks(&callbacks);
	SetupGenMidi();
	SetupDMXGUS();
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
	if (onexit)
	{
		// free static data in the backends.
		TimidityPP_Shutdown();
		dumb_exit();
	}
}

//==========================================================================
//
// identify a music lump's type and set up a player for it
//
//==========================================================================

MusInfo *I_RegisterSong (MusicIO::FileInterface *reader, MidiDeviceSetting *device)
{
	MusInfo *info = nullptr;
	StreamSource *streamsource = nullptr;
	const char *fmt;
	uint32_t id[32/4];

	if(reader->read(id, 32) != 32 || reader->seek(-32, FileReader::SeekCur) != 0)
	{
		reader->close();
		return nullptr;
	}
	try
	{
		// Check for gzip compression. Some formats are expected to have players
		// that can handle it, so it simplifies things if we make all songs
		// gzippable.
		if ((id[0] & MAKE_ID(255, 255, 255, 0)) == GZIP_ID)
		{
			// swap out the reader with one that reads the decompressed content.
			auto zreader = new MusicIO::VectorReader([reader](std::vector<uint8_t>& array)
				{
					bool res = false;
					auto len = reader->filelength();
					uint8_t* gzipped = new uint8_t[len];
					if (reader->read(gzipped, len) == len)
					{
						res = ungzip(gzipped, (int)len, array);
					}
					delete[] gzipped;
				});
			reader->close();
			reader = zreader;


			if (reader->read(id, 32) != 32 || reader->seek(-32, FileReader::SeekCur) != 0)
			{
				reader->close();
				return nullptr;
			}
		}

		EMIDIType miditype = IdentifyMIDIType(id, sizeof(id));
		if (miditype != MIDI_NOTMIDI)
		{
			std::vector<uint8_t> data(reader->filelength());
			if (reader->read(data.data(), (long)data.size()) != (long)data.size())
			{
				reader->close();
				return nullptr;
			}
			auto source = CreateMIDISource(data.data(), data.size(), miditype);
			if (source == nullptr)
			{
				reader->close();
				return nullptr;
			}
			if (!source->isValid())
			{
				delete source;
				return nullptr;
			}

			EMidiDevice devtype = device == nullptr ? MDEV_DEFAULT : (EMidiDevice)device->device;
#ifndef _WIN32
			// non-Windows platforms don't support MDEV_MMAPI so map to MDEV_SNDSYS
			if (devtype == MDEV_MMAPI)
				devtype = MDEV_SNDSYS;
#endif

			info = CreateMIDIStreamer(source, devtype, device != nullptr ? device->args.GetChars() : "");
		}

		// Check for CDDA "format"
		else if (id[0] == (('R') | (('I') << 8) | (('F') << 16) | (('F') << 24)))
		{
			uint32_t subid;

			reader->seek(8, SEEK_CUR);
			if (reader->read(&subid, 4) == 4)
			{
				reader->seek(-12, SEEK_CUR);

				if (subid == (('C') | (('D') << 8) | (('D') << 16) | (('A') << 24)))
				{
					// This is a CDDA file
					info = CDDA_OpenSong(reader);
				}
			}
		}

		// Check for various raw OPL formats
		else
		{
			if (
				(id[0] == MAKE_ID('R', 'A', 'W', 'A') && id[1] == MAKE_ID('D', 'A', 'T', 'A')) ||		// Rdos Raw OPL
				(id[0] == MAKE_ID('D', 'B', 'R', 'A') && id[1] == MAKE_ID('W', 'O', 'P', 'L')) ||		// DosBox Raw OPL
				(id[0] == MAKE_ID('A', 'D', 'L', 'I') && *((uint8_t*)id + 4) == 'B'))		// Martin Fernandez's modified IMF
			{
				streamsource = OPL_OpenSong(reader, &oplConfig);
			}
			else if ((id[0] == MAKE_ID('R', 'I', 'F', 'F') && id[2] == MAKE_ID('C', 'D', 'X', 'A')))
			{
				streamsource = XA_OpenSong(reader);	// this takes over the reader.
				reader = nullptr;					// We do not own this anymore.
			}
			// Check for game music
			else if ((fmt = GME_CheckFormat(id[0])) != nullptr && fmt[0] != '\0')
			{
				streamsource = GME_OpenSong(reader, fmt, (int)GSnd->GetOutputRate());
			}
			// Check for module formats
			else
			{
				streamsource = MOD_OpenSong(reader, (int)GSnd->GetOutputRate());
			}
			if (streamsource == nullptr)
			{
				streamsource = SndFile_OpenSong(reader);		// this only takes over the reader if it succeeds. We need to look out for this.
				if (streamsource != nullptr) reader = nullptr;
			}

			if (streamsource)
			{
				info = OpenStreamSong(streamsource);
			}
		}

		if (!info)
		{
			// File could not be identified as music.
			if (reader) reader->close();
			return nullptr;
		}

		if (info && !info->IsValid())
		{
			delete info;
			info = nullptr;
		}
	}
	catch (...)
	{
		// Make sure the reader is closed if this function abnormally terminates
		if (reader) reader->close();
		throw;
	}
	if (reader) reader->close();
	return info;
}

//==========================================================================
//
// play CD music
//
//==========================================================================

MusInfo *I_RegisterCDSong (int track, int id)
{
	MusInfo *info = CD_OpenSong (track, id);

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

static bool ungzip(uint8_t *data, int complen, std::vector<uint8_t> &newdata)
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
    newdata.resize(isize);

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


void I_SetRelativeVolume(float vol)
{
	relative_volume = (float)vol;
	ChangeMusicSetting(ZMusic::relative_volume, nullptr, (float)vol);
	snd_musicvolume.Callback();
}
//==========================================================================
//
// Sets relative music volume. Takes $musicvolume in SNDINFO into consideration
//
//==========================================================================

void I_SetMusicVolume (double factor)
{
	factor = clamp(factor, 0., 2.0);
	I_SetRelativeVolume(factor);
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
		return FString(mus_playing.handle->GetStats().c_str());
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
	if (type == MIDI_NOTMIDI)
	{
		Printf("%s is not MIDI-based.\n", src.GetChars());
		return nullptr;
	}

	auto data = wlump.Read();
	auto source = CreateMIDISource(data.Data(), data.Size(), type);

	if (source == nullptr)
	{
		Printf("%s is not MIDI-based.\n", src.GetChars());
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
		// We must stop the currently playing music to avoid interference between two synths. 
		auto savedsong = mus_playing;
		S_StopMusic(true);
		if (dev == MDEV_DEFAULT && snd_mididevice >= 0) dev = MDEV_FLUIDSYNTH;	// The Windows system synth cannot dump a wave.
		try
		{
			MIDIDumpWave(source, dev, argv.argc() < 6 ? nullptr : argv[6], argv[2], argv.argc() < 4 ? 0 : (int)strtol(argv[3], nullptr, 10), argv.argc() < 5 ? 0 : (int)strtol(argv[4], nullptr, 10));
		}
		catch (const std::runtime_error& err)
		{
			Printf("MIDI dump failed: %s\n", err.what());
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
	if (source == nullptr) return;

	std::vector<uint8_t> midi;
	bool success;

	source->CreateSMF(midi, 1);
	auto f = FileWriter::Open(argv[2]);
	if (f == nullptr)
	{
		Printf("Could not open %s.\n", argv[2]);
		return;
	}
	success = (f->Write(&midi[0], midi.size()) == midi.size());
	delete f;

	if (!success)
	{
		Printf("Could not write to music file %s.\n", argv[2]);
	}
}
