/*
** sndfile_decoder.cpp
**
**---------------------------------------------------------------------------
** Copyright 2008-2010 Chris Robinson
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

#include <algorithm>
#include "sndfile_decoder.h"
#include "loader/i_module.h"

#ifdef HAVE_SNDFILE

FModule SndFileModule{"SndFile"};

#include "sndload.h"


#ifdef _WIN32
static const char* libnames[] = { "sndfile.dll", "libsndfile-1.dll" };
#elif defined(__APPLE__)
static const char* libnames[] = { "libsndfile.1.dylib" };
#else
static const char* libnames[] = { "libsndfile.so.1" };
#endif

extern "C" int IsSndFilePresent()
{
#if !defined DYN_SNDFILE
	return true;
#else
	static bool cached_result = false;
	static bool done = false;

	if (!done)
	{
		done = true;
        for (auto libname : libnames)
        {
            auto abspath = FModule_GetProgDir() + "/" + libname;
            cached_result = SndFileModule.Load({ abspath.c_str(), libname });
            if (cached_result) break;
        }
	}
	return cached_result;
#endif
}

sf_count_t SndFileDecoder::file_get_filelen(void *user_data)
{
    auto &reader = reinterpret_cast<SndFileDecoder*>(user_data)->Reader;
	return reader->filelength();
}

sf_count_t SndFileDecoder::file_seek(sf_count_t offset, int whence, void *user_data)
{
	auto &reader = reinterpret_cast<SndFileDecoder*>(user_data)->Reader;

    if(reader->seek((long)offset, whence) != 0)
        return -1;
    return reader->tell();
}

sf_count_t SndFileDecoder::file_read(void *ptr, sf_count_t count, void *user_data)
{
	auto &reader = reinterpret_cast<SndFileDecoder*>(user_data)->Reader;
	return reader->read(ptr, (long)count);
}

sf_count_t SndFileDecoder::file_write(const void *ptr, sf_count_t count, void *user_data)
{
    return -1;
}

sf_count_t SndFileDecoder::file_tell(void *user_data)
{
	auto &reader = reinterpret_cast<SndFileDecoder*>(user_data)->Reader;
	return reader->tell();
}


SndFileDecoder::~SndFileDecoder()
{
    if(SndFile)
        sf_close(SndFile);
    SndFile = 0;

	if (Reader) Reader->close();
	Reader = nullptr;
}

bool SndFileDecoder::open(MusicIO::FileInterface *reader)
{
	if (!IsSndFilePresent()) return false;
	
	SF_VIRTUAL_IO sfio = { file_get_filelen, file_seek, file_read, file_write, file_tell };

	Reader = reader;
	SndInfo.format = 0;
	SndFile = sf_open_virtual(&sfio, SFM_READ, &SndInfo, this);
	if (SndFile)
	{
		if (SndInfo.channels == 1 || SndInfo.channels == 2)
			return true;

		sf_close(SndFile);
		SndFile = 0;
	}
	Reader = nullptr;	// need to give it back.
	return false;
}

void SndFileDecoder::getInfo(int *samplerate, ChannelConfig *chans, SampleType *type)
{
    *samplerate = SndInfo.samplerate;

    if(SndInfo.channels == 2)
        *chans = ChannelConfig_Stereo;
    else
        *chans = ChannelConfig_Mono;

    *type = SampleType_Int16;
}

size_t SndFileDecoder::read(char *buffer, size_t bytes)
{
    short *out = (short*)buffer;
    size_t frames = bytes / SndInfo.channels / 2;
    size_t total = 0;

    // It seems libsndfile has a bug with converting float samples from Vorbis
    // to the 16-bit shorts we use, which causes some PCM samples to overflow
    // and wrap, creating static. So instead, read the samples as floats and
    // convert to short ourselves.
    // Use a loop to convert a handful of samples at a time, avoiding a heap
    // allocation for temporary storage. 64 at a time works, though maybe it
    // could be more.
    while(total < frames)
    {
        size_t todo = std::min<size_t>(frames-total, 64/SndInfo.channels);
        float tmp[64];

        size_t got = (size_t)sf_readf_float(SndFile, tmp, todo);
        if(got < todo) frames = total + got;

        for(size_t i = 0;i < got*SndInfo.channels;i++)
            *out++ = (short)std::max(std::min(tmp[i] * 32767.f, 32767.f), -32768.f);
        total += got;
    }
    return total * SndInfo.channels * 2;
}

std::vector<uint8_t> SndFileDecoder::readAll()
{
    if(SndInfo.frames <= 0)
        return SoundDecoder::readAll();

    int framesize = 2 * SndInfo.channels;
	std::vector<uint8_t> output;

    output.resize((unsigned)(SndInfo.frames * framesize));
    size_t got = read((char*)&output[0], output.size());
    output.resize((unsigned)got);

    return output;
}

bool SndFileDecoder::seek(size_t ms_offset, bool ms, bool /*mayrestart*/)
{
    size_t smp_offset = ms? (size_t)((double)ms_offset / 1000. * SndInfo.samplerate) : ms_offset;
    if(sf_seek(SndFile, smp_offset, SEEK_SET) < 0)
        return false;
    return true;
}

size_t SndFileDecoder::getSampleOffset()
{
    return (size_t)sf_seek(SndFile, 0, SEEK_CUR);
}

size_t SndFileDecoder::getSampleLength()
{
    return (size_t)((SndInfo.frames > 0) ? SndInfo.frames : 0);
}

// band-aid for FluidSynth, which is C, not C++ and cannot use the module interface.
#ifdef DYN_SNDFILE

#undef sf_open_virtual
extern "C" SNDFILE * sf_open_virtual(SF_VIRTUAL_IO * sfvirtual, int mode, SF_INFO * sfinfo, void* user_data)
{
    return p_sf_open_virtual(sfvirtual, mode, sfinfo, user_data);
}

extern "C" const char* sf_strerror(SNDFILE * sndfile)
{
    return p_sf_strerror(sndfile);
}

extern "C" sf_count_t sf_readf_short(SNDFILE * sndfile, short* ptr, sf_count_t frames)
{
    return p_sf_readf_short(sndfile, ptr, frames);
}

#undef sf_close
extern "C" int sf_close(SNDFILE * sndfile)
{
    return p_sf_close(sndfile);
}

#endif


#else // in case someone decided to build without sndfile support

extern "C" int IsSndFilePresent()
{
    return false;
}

extern "C" SNDFILE * sf_open_virtual(SF_VIRTUAL_IO * sfvirtual, int mode, SF_INFO * sfinfo, void* user_data)
{
    return nullptr;
}

extern "C" const char* sf_strerror(SNDFILE * sndfile)
{
    return "no sndfile support";
}

extern "C" sf_count_t sf_readf_short(SNDFILE * sndfile, short* ptr, sf_count_t frames)
{
    return 0;
}

extern "C" int sf_close(SNDFILE * sndfile)
{
    return 0;
}

#endif

