/*
 ** i_music.cpp
 ** Plays music
 **
 **---------------------------------------------------------------------------
 ** Copyright 1998-2016 Randy Heit
 ** Copyright 2005-2019 Christoph Oelckers
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

#include <stdint.h>
#include <vector>
#include <string>
#include <zlib.h>
#include "m_swap.h"
#include "zmusic_internal.h"
#include "midiconfig.h"
#include "musinfo.h"
#include "streamsources/streamsource.h"
#include "midisources/midisource.h"

#define GZIP_ID1		31
#define GZIP_ID2		139
#define GZIP_CM			8
#define GZIP_ID			MAKE_ID(GZIP_ID1,GZIP_ID2,GZIP_CM,0)

#define GZIP_FTEXT		1
#define GZIP_FHCRC		2
#define GZIP_FEXTRA		4
#define GZIP_FNAME		8
#define GZIP_FCOMMENT	16

class MIDIDevice;
class OPLmusicFile;
class StreamSource;
class MusInfo;

MusInfo *OpenStreamSong(StreamSource *source);
const char *GME_CheckFormat(uint32_t header);
MusInfo* CDDA_OpenSong(MusicIO::FileInterface* reader);
MusInfo* CD_OpenSong(int track, int id);
MusInfo* CreateMIDIStreamer(MIDISource *source, EMidiDevice devtype, const char* args);

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
// identify a music lump's type and set up a player for it
//
//==========================================================================

static  MusInfo *ZMusic_OpenSongInternal (MusicIO::FileInterface *reader, EMidiDevice device, const char *Args)
{
	MusInfo *info = nullptr;
	StreamSource *streamsource = nullptr;
	const char *fmt;
	uint32_t id[32/4];
	
	if(reader->read(id, 32) != 32 || reader->seek(-32, SEEK_CUR) != 0)
	{
		SetError("Unable to read header");
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
			
			
			if (reader->read(id, 32) != 32 || reader->seek(-32, SEEK_CUR) != 0)
			{
				reader->close();
				return nullptr;
			}
		}
		
		EMIDIType miditype = ZMusic_IdentifyMIDIType(id, sizeof(id));
		if (miditype != MIDI_NOTMIDI)
		{
			std::vector<uint8_t> data(reader->filelength());
			if (reader->read(data.data(), (long)data.size()) != (long)data.size())
			{
				SetError("Failed to read MIDI data");
				reader->close();
				return nullptr;
			}
			auto source = ZMusic_CreateMIDISource(data.data(), data.size(), miditype);
			if (source == nullptr)
			{
				reader->close();
				return nullptr;
			}
			if (!source->isValid())
			{
				SetError("Invalid data in MIDI file");
				delete source;
				return nullptr;
			}

#ifndef HAVE_SYSTEM_MIDI
			// some platforms don't support MDEV_STANDARD so map to MDEV_SNDSYS
			if (device == MDEV_STANDARD)
				device = MDEV_SNDSYS;
#endif
			
			info = CreateMIDIStreamer(source, device, Args? Args : "");
		}
		
		// Check for CDDA "format"
		else if ((id[0] == MAKE_ID('R', 'I', 'F', 'F') && id[2] == MAKE_ID('C', 'D', 'D', 'A')))
		{
			// This is a CDDA file
			info = CDDA_OpenSong(reader);
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
				streamsource = GME_OpenSong(reader, fmt, miscConfig.snd_outputrate);
			}
			// Check for module formats
			else
			{
				streamsource = MOD_OpenSong(reader, miscConfig.snd_outputrate);
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
			SetError("Unable to identify as music");
			return nullptr;
		}
		
		if (info && !info->IsValid())
		{
			delete info;
			SetError("Unable to identify as music");
			info = nullptr;
		}
		if (reader) reader->close();
		return info;
	}
	catch (const std::exception &ex)
	{
		// Make sure the reader is closed if this function abnormally terminates
		if (reader) reader->close();
		SetError(ex.what());
		return nullptr;
	}
}

DLL_EXPORT ZMusic_MusicStream ZMusic_OpenSongFile(const char* filename, EMidiDevice device, const char* Args)
{
	auto f = MusicIO::utf8_fopen(filename, "rb");
	if (!f)
	{
		SetError("File not found");
		return nullptr;
	}
	auto fr = new MusicIO::StdioFileReader;
	fr->f = f;
	return ZMusic_OpenSongInternal(fr, device, Args);
}

DLL_EXPORT ZMusic_MusicStream ZMusic_OpenSongMem(const void* mem, size_t size, EMidiDevice device, const char* Args)
{
	if (!mem || !size)
	{
		SetError("Invalid data");
		return nullptr;
	}
	// Data must be copied because it may be used as a streaming source and we cannot guarantee that the client memory stays valid. We also have no means to free it.
	auto mr = new MusicIO::VectorReader((uint8_t*)mem, (long)size);
	return ZMusic_OpenSongInternal(mr, device, Args);
}

DLL_EXPORT ZMusic_MusicStream ZMusic_OpenSong(ZMusicCustomReader* reader, EMidiDevice device, const char* Args)
{
	if (!reader)
	{
		SetError("No reader protocol specified");
		return nullptr;
	}
	auto cr = new CustomFileReader(reader);	// Oh no! We just put another wrapper around the client's wrapper!
	return ZMusic_OpenSongInternal(cr, device, Args);
}


//==========================================================================
//
// play CD music
//
//==========================================================================

DLL_EXPORT MusInfo *ZMusic_OpenCDSong (int track, int id)
{
	MusInfo *info = CD_OpenSong (track, id);
	
	if (info && !info->IsValid ())
	{
		delete info;
		info = nullptr;
		SetError("Unable to open CD Audio");
	}
	
	return info;
}

//==========================================================================
//
// streaming callback
//
//==========================================================================

DLL_EXPORT bool ZMusic_FillStream(MusInfo* song, void* buff, int len)
{
	if (song == nullptr) return false;
	std::lock_guard<std::mutex> lock(song->CritSec);
	return song->ServiceStream(buff, len);
}

//==========================================================================
//
// starts playback
//
//==========================================================================

DLL_EXPORT bool ZMusic_Start(MusInfo *song, int subsong, bool loop)
{
	if (!song) return true;	// Starting a null song is not an error! It just won't play anything.
	try
	{
		song->Play(loop, subsong);
		return true;
	}
	catch (const std::exception & ex)
	{
		SetError(ex.what());
		return false;
	}
}

//==========================================================================
//
// Utilities
//
//==========================================================================

DLL_EXPORT void ZMusic_Pause(MusInfo *song)
{
	if (!song) return;
	song->Pause();
}

DLL_EXPORT void ZMusic_Resume(MusInfo *song)
{
	if (!song) return;
	song->Resume();
}

DLL_EXPORT void ZMusic_Update(MusInfo *song)
{
	if (!song) return;
	song->Update();
}

DLL_EXPORT bool ZMusic_IsPlaying(MusInfo *song)
{
	if (!song) return false;
	return song->IsPlaying();
}

DLL_EXPORT void ZMusic_Stop(MusInfo *song)
{
	if (!song) return;
	std::lock_guard<std::mutex> lock(song->CritSec);
	song->Stop();
}

DLL_EXPORT bool ZMusic_SetSubsong(MusInfo *song, int subsong)
{
	if (!song) return false;
	std::lock_guard<std::mutex> lock(song->CritSec);
	return song->SetSubsong(subsong);
}

DLL_EXPORT bool ZMusic_IsLooping(MusInfo *song)
{
	if (!song) return false;
	return song->m_Looping;
}

DLL_EXPORT bool ZMusic_IsMIDI(MusInfo *song)
{
	if (!song) return false;
	return song->IsMIDI();
}

DLL_EXPORT void ZMusic_GetStreamInfo(MusInfo *song, SoundStreamInfo *fmt)
{
	if (!fmt) return;
	if (!song) *fmt = {};
	std::lock_guard<std::mutex> lock(song->CritSec);
	*fmt = song->GetStreamInfo();
}

DLL_EXPORT void ZMusic_Close(MusInfo *song)
{
	if (!song) return;
	delete song;
}

DLL_EXPORT void ZMusic_VolumeChanged(MusInfo *song)
{
	if (!song) return;
	std::lock_guard<std::mutex> lock(song->CritSec);
	song->MusicVolumeChanged();
}

static std::string staticErrorMessage;

DLL_EXPORT const char *ZMusic_GetStats(MusInfo *song)
{
	if (!song) return "";
	std::lock_guard<std::mutex> lock(song->CritSec);
	staticErrorMessage = song->GetStats();
	return staticErrorMessage.c_str();
}

void SetError(const char* msg)
{
	staticErrorMessage = msg;
}

DLL_EXPORT const char* ZMusic_GetLastError()
{
	return staticErrorMessage.c_str();
}

DLL_EXPORT bool ZMusic_WriteSMF(MIDISource* source, const char *fn, int looplimit)
{
	std::vector<uint8_t> midi;
	bool success;

	if (!source) return false;
	source->CreateSMF(midi, 1);
	auto f = MusicIO::utf8_fopen(fn, "wt");
	if (f == nullptr) return false;
	success = (fwrite(&midi[0], 1, midi.size(), f) == midi.size());
	delete f;
	return success;
}
