/*
** music_libsndfile.cpp
** Uses libsndfile for streaming music formats
**
**---------------------------------------------------------------------------
** Copyright 2017 Christoph Oelckers
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

#include "i_musicinterns.h"
#include "c_cvars.h"
#include "critsec.h"
#include "v_text.h"
#include "templates.h"
#include "m_fixed.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

class SndFileSong : public StreamSong
{
public:
	SndFileSong(FileReader *reader, SoundDecoder *decoder, uint32_t loop_start, uint32_t loop_end, bool startass, bool endass);
	~SndFileSong();
	bool SetSubsong(int subsong);
	void Play(bool looping, int subsong);
	FString GetStats();
	
protected:
	FCriticalSection CritSec;
	FileReader *Reader;
	SoundDecoder *Decoder;
	int Channels;
	int SampleRate;
	
	uint32_t Loop_Start;
	uint32_t Loop_End;

	int CalcSongLength();

	static bool Read(SoundStream *stream, void *buff, int len, void *userdata);
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

CUSTOM_CVAR(Int, snd_streambuffersize, 64, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 16)
	{
		self = 16;
	}
	else if (self > 1024)
	{
		self = 1024;
	}
}

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
//
// try to find the LOOP_START/LOOP_END tags
//
// This is a brute force implementation, thanks in no snall part
// that no decent documentation of Ogg headers seems to exist and
// all available tag libraries are horrendously bloated.
// So if we want to do this without any new third party dependencies,
// thanks to the lack of anything that would help to do this properly,
// this was the only solution.
//
//==========================================================================

void FindLoopTags(FileReader *fr, uint32_t *start, bool *startass, uint32_t *end, bool *endass)
{
	unsigned char testbuf[256];

	fr->Seek(0, SEEK_SET);
	long got = fr->Read(testbuf, 256);
	auto eqp = testbuf - 1;
	int count;
	while(true)
	{
		unsigned char *c = (unsigned char *)memchr(eqp + 1, '=', 256 - (eqp + 1 - testbuf));
		if (c == nullptr) return;	// If there is no '=' in the first 256 bytes there's also no metadata.

		eqp = c;
		while (*c >= 32 && *c < 127) c--;
		if (*c != 0)
		{
			// doesn't look like a valid tag, so try again
			continue;
		}
		c -= 3;
		int len = c[0] + 256*c[1] + 65536*c[2];
		if (c[3] || len > 1000000 || len < (eqp - c - 3))
		{
			// length looks fishy so retry with the next '='
			continue;
		}
		c -= 4;
		count = c[0] + 256 * c[1];
		if (c[2] || c[3] || count <= 0 || count > 1000)
		{
			// very unlikely to have 1000 tags
			continue;
		}
		c += 4;
		fr->Seek(long(c - testbuf), SEEK_SET);
		break;	// looks like we found something.
	}
	for (int i = 0; i < count; i++)
	{
		int length = 0;
		fr->Read(&length, 4);
		length = LittleLong(length);
		if (length == 0 || length > 1000000) return;	// looks like we lost it...
		if (length > 25)
		{
			// This tag is too long to be a valid time stamp so don't even bother.
			fr->Seek(length, SEEK_CUR);
			continue;
		}
		fr->Read(testbuf, length);
		testbuf[length] = 0;
		if (strnicmp((char*)testbuf, "LOOP_START=", 11) == 0)
		{
			S_ParseTimeTag((char*)testbuf + 11, startass, start);
		}
		else if (strnicmp((char*)testbuf, "LOOP_END=", 9) == 0)
		{
			S_ParseTimeTag((char*)testbuf + 9, endass, end);
		}
	}
}

//==========================================================================
//
// SndFile_OpenSong
//
//==========================================================================

MusInfo *SndFile_OpenSong(FileReader &fr)
{
	uint8_t signature[4];
	
	fr.Seek(0, SEEK_SET);
	fr.Read(signature, 4);
	uint32_t loop_start = 0, loop_end = ~0u;
	bool startass = false, endass = false;
	
	if (!memcmp(signature, "OggS", 4) || !memcmp(signature, "fLaC", 4))
	{
		// Todo: Read loop points from metadata
		FindLoopTags(&fr, &loop_start, &startass, &loop_end, &endass);
	}
	fr.Seek(0, SEEK_SET);
	auto decoder = SoundRenderer::CreateDecoder(&fr);
	if (decoder == nullptr) return nullptr;
	return new SndFileSong(&fr, decoder, loop_start, loop_end, startass, endass);
}

//==========================================================================
//
// SndFileSong - Constructor
//
//==========================================================================

SndFileSong::SndFileSong(FileReader *reader, SoundDecoder *decoder, uint32_t loop_start, uint32_t loop_end, bool startass, bool endass)
{
	ChannelConfig iChannels;
	SampleType Type;
	
	decoder->getInfo(&SampleRate, &iChannels, &Type);

	if (!startass) loop_start = Scale(loop_start, SampleRate, 1000);
	if (!endass) loop_end = Scale(loop_end, SampleRate, 1000);

	Loop_Start = loop_start;
	Loop_End = clamp<uint32_t>(loop_end, 0, (uint32_t)decoder->getSampleLength());
	Reader = reader;
	Decoder = decoder;
	Channels = iChannels == ChannelConfig_Stereo? 2:1;
	m_Stream = GSnd->CreateStream(Read, snd_streambuffersize * 1024, iChannels == ChannelConfig_Stereo? 0 : SoundStream::Mono, SampleRate, this);
}

//==========================================================================
//
// SndFileSong - Destructor
//
//==========================================================================

SndFileSong::~SndFileSong()
{
	Stop();
	if (m_Stream != nullptr)
	{
		delete m_Stream;
		m_Stream = nullptr;
	}
	if (Decoder != nullptr)
	{
		delete Decoder;
	}
	if (Reader != nullptr)
	{
		delete Reader;
	}
}


//==========================================================================
//
// SndFileSong :: Play
//
//==========================================================================

void SndFileSong::Play(bool looping, int track)
{
	m_Status = STATE_Stopped;
	m_Looping = looping;
	if (m_Stream->Play(looping, 1))
	{
		m_Status = STATE_Playing;
	}
}

//==========================================================================
//
// SndFileSong :: SetSubsong
//
//==========================================================================

bool SndFileSong::SetSubsong(int track)
{
	return false;
}

//==========================================================================
//
// SndFileSong :: GetStats
//
//==========================================================================

FString SndFileSong::GetStats()
{
	FString out;
	
	size_t SamplePos;
	
	SamplePos = Decoder->getSampleOffset();
	int time = int (SamplePos / SampleRate);
	
	out.Format(
		"Track: " TEXTCOLOR_YELLOW "%s, %dHz" TEXTCOLOR_NORMAL
		"  Time:" TEXTCOLOR_YELLOW "%02d:%02d" TEXTCOLOR_NORMAL,
		Channels == 2? "Stereo" : "Mono", SampleRate,
		time/60,
		time % 60);
	return out;
}

//==========================================================================
//
// SndFileSong :: Read													STATIC
//
//==========================================================================

bool SndFileSong::Read(SoundStream *stream, void *vbuff, int ilen, void *userdata)
{
	char *buff = (char*)vbuff;
	SndFileSong *song = (SndFileSong *)userdata;
	song->CritSec.Enter();
	
	size_t len = size_t(ilen);
	size_t currentpos = song->Decoder->getSampleOffset();
	size_t framestoread = len / (song->Channels*2);
	bool err = false;
	if (!song->m_Looping)
	{
		size_t maxpos = song->Decoder->getSampleLength();
		if (currentpos == maxpos)
		{
			memset(buff, 0, len);
			song->CritSec.Leave();
			return false;
		}
		if (currentpos + framestoread > maxpos)
		{
			size_t got = song->Decoder->read(buff, (maxpos - currentpos) * song->Channels * 2);
			memset(buff + got, 0, len - got);
		}
		else
		{
			size_t got = song->Decoder->read(buff, len);
			err = (got != len);
		}
	}
	else
	{
		// This looks a bit more complicated than necessary because libmpg123 will not read the full requested length for the last block in the file.
		if (currentpos + framestoread > song->Loop_End)
		{
			size_t endblock = (song->Loop_End - currentpos) * song->Channels * 2;
			size_t endlen = song->Decoder->read(buff, endblock);

			// Even if zero bytes was read give it a chance to start from the beginning
			buff = buff + endlen;
			len -= endlen;
			song->Decoder->seek(song->Loop_Start, false, true);
		}
		while (len > 0)
		{
			size_t readlen = song->Decoder->read(buff, len);
			if (readlen == 0)
			{
				song->CritSec.Leave();
				return false;
			}
			buff += readlen;
			len -= readlen;
			if (len > 0)
			{
				song->Decoder->seek(song->Loop_Start, false, true);
			}
		}
	}
	song->CritSec.Leave();
	return true;
}
