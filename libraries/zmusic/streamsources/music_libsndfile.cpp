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

#include <mutex>
#include <algorithm>
#include "streamsource.h"
#include "zmusic/sounddecoder.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

class SndFileSong : public StreamSource
{
public:
	SndFileSong(SoundDecoder *decoder, uint32_t loop_start, uint32_t loop_end, bool startass, bool endass);
	~SndFileSong();
	std::string GetStats() override;
	SoundStreamInfo GetFormat() override;
	bool GetData(void *buffer, size_t len) override;
	
protected:
	SoundDecoder *Decoder;
	int Channels;
	int SampleRate;
	
	uint32_t Loop_Start;
	uint32_t Loop_End;

	int CalcSongLength();
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
//
// S_ParseTimeTag
//
// Passed the value of a loop point tag, converts it to numbers.
//
// This may be of the form 00:00:00.00 (HH:MM:SS.ss) to specify by play
// time. Various parts may be left off. The only requirement is that it
// contain a colon. e.g. To start the loop at 20 seconds in, you can use
// ":20", "0:20", "00:00:20", ":20.0", etc. Values after the decimal are
// fractions of a second.
//
// If you don't include a colon but just have a raw number, then it's
// the number of PCM samples at which to loop.
//
// Returns true if the tag made sense, false if not.
//
//==========================================================================

bool S_ParseTimeTag(const char* tag, bool* as_samples, unsigned int* time)
{
	const int time_count = 3;
	const char* bit = tag;
	char ms[3] = { 0 };
	unsigned int times[time_count] = { 0 };
	int ms_pos = 0, time_pos = 0;
	bool pcm = true, in_ms = false;

	for (bit = tag; *bit != '\0'; ++bit)
	{
		if (*bit >= '0' && *bit <= '9')
		{
			if (in_ms)
			{
				// Ignore anything past three fractional digits.
				if (ms_pos < 3)
				{
					ms[ms_pos++] = *bit - '0';
				}
			}
			else
			{
				times[time_pos] = times[time_pos] * 10 + *bit - '0';
			}
		}
		else if (*bit == ':')
		{
			if (in_ms)
			{ // If we already specified milliseconds, we can't take any more parts.
				return false;
			}
			pcm = false;
			if (++time_pos == time_count)
			{ // Time too long. (Seriously, starting the loop days in?)
				return false;
			}
		}
		else if (*bit == '.')
		{
			if (pcm || in_ms)
			{ // It doesn't make sense to have fractional PCM values.
			  // It also doesn't make sense to have more than one dot.
				return false;
			}
			in_ms = true;
		}
		else
		{ // Anything else: We don't understand this.
			return false;
		}
	}
	if (pcm)
	{
		*as_samples = true;
		*time = times[0];
	}
	else
	{
		unsigned int mytime = 0;

		// Add in hours, minutes, and seconds
		for (int i = 0; i <= time_pos; ++i)
		{
			mytime = mytime * 60 + times[i];
		}

		// Add in milliseconds
		mytime = mytime * 1000 + ms[0] * 100 + ms[1] * 10 + ms[2];

		*as_samples = false;
		*time = mytime;
	}
	return true;
}

//==========================================================================
//
// Try to find the LOOP_START/LOOP_END tags in a Vorbis Comment block
//
// We have to parse through the FLAC or Ogg headers manually, since sndfile
// doesn't provide proper access to the comments and we'd rather not require
// using libFLAC and libvorbisfile directly.
//
//==========================================================================

static void ParseVorbisComments(MusicIO::FileInterface *fr, uint32_t *start, bool *startass, uint32_t *end, bool *endass)
{
	uint8_t vc_data[4];

	// The VC block starts with a 32LE integer for the vendor string length,
	// followed by the vendor string
	if(fr->read(vc_data, 4) != 4)
		return;
	uint32_t vndr_len = vc_data[0] | (vc_data[1]<<8) | (vc_data[2]<<16) | (vc_data[3]<<24);

	// Skip vendor string
	if(fr->seek(vndr_len, SEEK_CUR) == -1)
		return;

	// Following the vendor string is a 32LE integer for the number of
	// comments, followed by each comment.
	if(fr->read(vc_data, 4) != 4)
		return;
	size_t count = vc_data[0] | (vc_data[1]<<8) | (vc_data[2]<<16) | (vc_data[3]<<24);

	for(size_t i = 0; i < count; i++)
	{
		// Each comment is a 32LE integer for the comment length, followed by
		// the comment text (not null terminated!)
		if(fr->read(vc_data, 4) != 4)
			return;
		uint32_t length = vc_data[0] | (vc_data[1]<<8) | (vc_data[2]<<16) | (vc_data[3]<<24);

		if(length >= 128)
		{
			// If the comment is "big", skip it
			if(fr->seek(length, SEEK_CUR) == -1)
				return;
			continue;
		}

		char strdat[128];
		if(fr->read(strdat, length) != (long)length)
			return;
		strdat[length] = 0;

		if(strnicmp(strdat, "LOOP_START=", 11) == 0)
			S_ParseTimeTag(strdat + 11, startass, start);
		else if(strnicmp(strdat, "LOOP_END=", 9) == 0)
			S_ParseTimeTag(strdat + 9, endass, end);
	}
}

static void FindFlacComments(MusicIO::FileInterface *fr, uint32_t *loop_start, bool *startass, uint32_t *loop_end, bool *endass)
{
	// Already verified the fLaC marker, so we're 4 bytes into the file
	bool lastblock = false;
	uint8_t header[4];

	while(!lastblock && fr->read(header, 4) == 4)
	{
		// The first byte of the block header contains the type and a flag
		// indicating the last metadata block
		char blocktype = header[0]&0x7f;
		lastblock = !!(header[0]&0x80);
		// Following the type is a 24BE integer for the size of the block
		uint32_t blocksize = (header[1]<<16) | (header[2]<<8) | header[3];

		// FLAC__METADATA_TYPE_VORBIS_COMMENT is 4
		if(blocktype == 4)
		{
			ParseVorbisComments(fr, loop_start, startass, loop_end, endass);
			return;
		}

		if(fr->seek(blocksize, SEEK_CUR) == -1)
			break;
	}
}

static void FindOggComments(MusicIO::FileInterface *fr, uint32_t *loop_start, bool *startass, uint32_t *loop_end, bool *endass)
{
	uint8_t ogghead[27];

	// We already read and verified the OggS marker, so skip the first 4 bytes
	// of the Ogg page header.
	while(fr->read(ogghead+4, 23) == 23)
	{
		// The 19th byte of the Ogg header is a 32LE integer for the page
		// number, and the 27th is a uint8 for the number of segments in the
		// page.
		uint32_t ogg_pagenum = ogghead[18] | (ogghead[19]<<8) | (ogghead[20]<<16) |
		                       (ogghead[21]<<24);
		uint8_t ogg_segments = ogghead[26];

		// Following the Ogg page header is a series of uint8s for the length of
		// each segment in the page. The page segment data follows contiguously
		// after.
		uint8_t segsizes[256];
		if(fr->read(segsizes, ogg_segments) != ogg_segments)
			break;

		// Find the segment with the Vorbis Comment packet (type 3)
		for(int i = 0; i < ogg_segments; ++i)
		{
			uint8_t segsize = segsizes[i];

			if(segsize > 16)
			{
				uint8_t vorbhead[7];
				if(fr->read(vorbhead, 7) != 7)
					return;

				if(vorbhead[0] == 3 && memcmp(vorbhead+1, "vorbis", 6) == 0)
				{
					// If the packet is 'laced', it spans multiple segments (a
					// segment size of 255 indicates the next segment continues
					// the packet, ending with a size less than 255). Vorbis
					// packets always start and end on segment boundaries. A
					// packet that's an exact multiple of 255 ends with a
					// segment of 0 size.
					while(segsize == 255 && ++i < ogg_segments)
						segsize = segsizes[i];

					// TODO: A Vorbis packet can theoretically span multiple
					// Ogg pages (e.g. start in the last segment of one page
					// and end in the first segment of a following page). That
					// will require extra logic to decode as the VC block will
					// be broken up with non-Vorbis data in-between. For now,
					// just handle the common case where it's all in one page.
					if(i < ogg_segments)
						ParseVorbisComments(fr, loop_start, startass, loop_end, endass);
					return;
				}

				segsize -= 7;
			}
			if(fr->seek(segsize, SEEK_CUR) == -1)
				return;
		}

		// Don't keep looking after the third page
		if(ogg_pagenum >= 2)
			break;

		if(fr->read(ogghead, 4) != 4 || memcmp(ogghead, "OggS", 4) != 0)
			break;
	}
}

void FindLoopTags(MusicIO::FileInterface *fr, uint32_t *start, bool *startass, uint32_t *end, bool *endass)
{
	uint8_t signature[4];

	fr->read(signature, 4);
	if(memcmp(signature, "fLaC", 4) == 0)
		FindFlacComments(fr, start, startass, end, endass);
	else if(memcmp(signature, "OggS", 4) == 0)
		FindOggComments(fr, start, startass, end, endass);
}


//==========================================================================
//
// SndFile_OpenSong
//
//==========================================================================

StreamSource *SndFile_OpenSong(MusicIO::FileInterface *fr)
{
	fr->seek(0, SEEK_SET);

	uint32_t loop_start = 0, loop_end = ~0u;
	bool startass = false, endass = false;
	FindLoopTags(fr, &loop_start, &startass, &loop_end, &endass);

	fr->seek(0, SEEK_SET);
	auto decoder = SoundDecoder::CreateDecoder(fr);
	if (decoder == nullptr) return nullptr;	// If this fails the file reader has not been taken over and the caller needs to clean up. This is to allow further analysis of the passed file.
	return new SndFileSong(decoder, loop_start, loop_end, startass, endass);
}

//==========================================================================
//
// SndFileSong - Constructor
//
//==========================================================================

static int32_t Scale(int32_t a, int32_t b, int32_t c)
{
	return (int32_t)(((int64_t)a * b) / c);
}

SndFileSong::SndFileSong(SoundDecoder *decoder, uint32_t loop_start, uint32_t loop_end, bool startass, bool endass)
{
	ChannelConfig iChannels;
	SampleType Type;
	
	decoder->getInfo(&SampleRate, &iChannels, &Type);

	if (!startass) loop_start = Scale(loop_start, SampleRate, 1000);
	if (!endass) loop_end = Scale(loop_end, SampleRate, 1000);

	const uint32_t sampleLength = (uint32_t)decoder->getSampleLength();
	Loop_Start = loop_start;
	Loop_End = sampleLength == 0 ? loop_end : std::min<uint32_t>(loop_end, sampleLength);
	Decoder = decoder;
	Channels = iChannels == ChannelConfig_Stereo? 2:1;
}

SoundStreamInfo SndFileSong::GetFormat()
{
	// deal with this once the configuration is handled better.
	return { 64/*snd_streambuffersize*/ * 1024, SampleRate, -Channels };
}

//==========================================================================
//
// SndFileSong - Destructor
//
//==========================================================================

SndFileSong::~SndFileSong()
{
	if (Decoder != nullptr)
	{
		delete Decoder;
	}
}

//==========================================================================
//
// SndFileSong :: GetStats
//
//==========================================================================

std::string SndFileSong::GetStats()
{
	char out[80];
	
	size_t SamplePos;
	
	SamplePos = Decoder->getSampleOffset();
	int time = int (SamplePos / SampleRate);
	
	snprintf(out, 80,
		"Track: %s, %dHz  Time: %02d:%02d",
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

bool SndFileSong::GetData(void *vbuff, size_t len)
{
	char *buff = (char*)vbuff;
	
	size_t currentpos = Decoder->getSampleOffset();
	size_t framestoread = len / (Channels*2);
	bool err = false;
	if (!m_Looping)
	{
		size_t maxpos = Decoder->getSampleLength();
		if (currentpos == maxpos)
		{
			memset(buff, 0, len);
			return false;
		}
		if (currentpos + framestoread > maxpos)
		{
			size_t got = Decoder->read(buff, (maxpos - currentpos) * Channels * 2);
			memset(buff + got, 0, len - got);
		}
		else
		{
			size_t got = Decoder->read(buff, len);
			err = (got != len);
		}
	}
	else
	{
		// This looks a bit more complicated than necessary because libmpg123 will not read the full requested length for the last block in the file.
		if (currentpos + framestoread > Loop_End)
		{
			// Loop can be very short, make sure the current position doesn't exceed it
			if (currentpos < Loop_End)
			{
				size_t endblock = (Loop_End - currentpos) * Channels * 2;
				size_t endlen = Decoder->read(buff, endblock);

				// Even if zero bytes was read give it a chance to start from the beginning
				buff += endlen;
				len -= endlen;
			}

			Decoder->seek(Loop_Start, false, true);
		}
		while (len > 0)
		{
			size_t readlen = Decoder->read(buff, len);
			if (readlen == 0)
			{
				return false;
			}
			buff += readlen;
			len -= readlen;
			if (len > 0)
			{
				Decoder->seek(Loop_Start, false, true);
			}
		}
	}
	return true;
}
