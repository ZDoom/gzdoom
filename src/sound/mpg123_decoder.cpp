/*
** mpg123_decoder.cpp
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

#include "mpg123_decoder.h"
#include "files.h"
#include "i_module.h"
#include "cmdlib.h"

#ifdef HAVE_MPG123


FModule MPG123Module{"MPG123"};

#include "mpgload.h"

#ifdef _WIN32
#define MPG123LIB "libmpg123-0.dll"
#elif defined(__APPLE__)
#define MPG123LIB "libmpg123.0.dylib"
#else
#define MPG123LIB "libmpg123.so.0"
#endif

bool IsMPG123Present()
{
#if !defined DYN_MPG123
	return true;
#else
	static bool cached_result = false;
	static bool done = false;

	if (!done)
	{
		done = true;
		cached_result = MPG123Module.Load({NicePath("$PROGDIR/" MPG123LIB), MPG123LIB});
	}
	return cached_result;
#endif
}


static bool inited = false;


off_t MPG123Decoder::file_lseek(void *handle, off_t offset, int whence)
{
    FileReader *reader = reinterpret_cast<MPG123Decoder*>(handle)->Reader;

    if(whence == SEEK_CUR)
    {
        if(offset < 0 && reader->Tell()+offset < 0)
            return -1;
    }
    else if(whence == SEEK_END)
    {
        if(offset < 0 && reader->GetLength()+offset < 0)
            return -1;
    }

    if(reader->Seek(offset, whence) != 0)
        return -1;
    return reader->Tell();
}

ssize_t MPG123Decoder::file_read(void *handle, void *buffer, size_t bytes)
{
    FileReader *reader = reinterpret_cast<MPG123Decoder*>(handle)->Reader;
    return (ssize_t)reader->Read(buffer, (long)bytes);
}


MPG123Decoder::~MPG123Decoder()
{
    if(MPG123)
    {
        mpg123_close(MPG123);
        mpg123_delete(MPG123);
        MPG123 = 0;
    }
}

bool MPG123Decoder::open(FileReader *reader)
{
    if(!inited)
    {
		if (!IsMPG123Present()) return false;
		if(mpg123_init() != MPG123_OK) return false;
		inited = true;
    }

    Reader = reader;

    {
        MPG123 = mpg123_new(NULL, NULL);
        if(mpg123_replace_reader_handle(MPG123, file_read, file_lseek, NULL) == MPG123_OK &&
           mpg123_open_handle(MPG123, this) == MPG123_OK)
        {
            int enc, channels;
            long srate;

            if(mpg123_getformat(MPG123, &srate, &channels, &enc) == MPG123_OK)
            {
                if((channels == 1 || channels == 2) && srate > 0 &&
                   mpg123_format_none(MPG123) == MPG123_OK &&
                   mpg123_format(MPG123, srate, channels, MPG123_ENC_SIGNED_16) == MPG123_OK)
                {
                    // All OK
                    Done = false;
                    return true;
                }
            }
            mpg123_close(MPG123);
        }
        mpg123_delete(MPG123);
        MPG123 = 0;
    }

    return false;
}

void MPG123Decoder::getInfo(int *samplerate, ChannelConfig *chans, SampleType *type)
{
    int enc = 0, channels = 0;
    long srate = 0;

    mpg123_getformat(MPG123, &srate, &channels, &enc);

    *samplerate = srate;

    if(channels == 2)
        *chans = ChannelConfig_Stereo;
    else
        *chans = ChannelConfig_Mono;

    *type = SampleType_Int16;
}

size_t MPG123Decoder::read(char *buffer, size_t bytes)
{
    size_t amt = 0;
    while(!Done && bytes > 0)
    {
        size_t got = 0;
        int ret = mpg123_read(MPG123, (unsigned char*)buffer, bytes, &got);

        bytes -= got;
        buffer += got;
        amt += got;

        if(ret == MPG123_NEW_FORMAT || ret == MPG123_DONE || got == 0)
        {
            Done = true;
            break;
        }
    }
    return amt;
}

bool MPG123Decoder::seek(size_t ms_offset, bool ms, bool mayrestart)
{
	int enc, channels;
	long srate;

	if (!mayrestart || ms_offset > 0)
	{
		if (mpg123_getformat(MPG123, &srate, &channels, &enc) == MPG123_OK)
		{
			size_t smp_offset = ms ? (size_t)((double)ms_offset / 1000. * srate) : ms_offset;
			if (mpg123_seek(MPG123, (off_t)smp_offset, SEEK_SET) >= 0)
			{
				Done = false;
				return true;
			}
		}
		return false;
	}
	else
	{
		// Restart the song instead of rewinding. A rewind seems to cause distortion when done repeatedly.
		// offset is intentionally ignored here.
		if (MPG123)
		{
			mpg123_close(MPG123);
			mpg123_delete(MPG123);
			MPG123 = 0;
		}
		Reader->Seek(0, SEEK_SET);
		return open(Reader);
	}
}
size_t MPG123Decoder::getSampleOffset()
{
    return mpg123_tell(MPG123);
}

size_t MPG123Decoder::getSampleLength()
{
    off_t len = mpg123_length(MPG123);
    return (len > 0) ? len : 0;
}

#endif
