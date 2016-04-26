#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define USE_WINDOWS_DWORD
#endif

#include "mpg123_decoder.h"
#include "files.h"
#include "except.h"

#ifdef HAVE_MPG123
static bool inited = false;


off_t MPG123Decoder::file_lseek(void *handle, off_t offset, int whence)
{
    MPG123Decoder *self = reinterpret_cast<MPG123Decoder*>(handle);
    FileReader *reader = self->Reader;

    if(whence == SEEK_SET)
        offset += self->StartOffset;
    else if(whence == SEEK_CUR)
    {
        if(offset < 0 && reader->Tell()+offset < self->StartOffset)
            return -1;
    }
    else if(whence == SEEK_END)
    {
        if(offset < 0 && reader->GetLength()+offset < self->StartOffset)
            return -1;
    }

    if(reader->Seek(offset, whence) != 0)
        return -1;
    return reader->Tell() - self->StartOffset;
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
#ifdef _MSC_VER
		__try {
#endif
			if(mpg123_init() != MPG123_OK)
				return false;
			inited = true;
#ifdef _MSC_VER
        } __except (CheckException(GetExceptionCode())) {
			// this means that the delay loaded decoder DLL was not found.
			return false;
		}
#endif
    }

    Reader = reader;
    StartOffset = 0;

    char data[10];
    if(file_read(this, data, 10) != 10)
        return false;

    int start_offset = 0;
    // Check for ID3 tags and skip them
    if(memcmp(data, "ID3", 3) == 0 &&
       (BYTE)data[3] <= 4 && (BYTE)data[4] != 0xff &&
       (data[5]&0x0f) == 0 && (data[6]&0x80) == 0 &&
       (data[7]&0x80) == 0 && (data[8]&0x80) == 0 &&
       (data[9]&0x80) == 0)
    {
        // ID3v2
        start_offset  = (data[6]<<21) | (data[7]<<14) |
                        (data[8]<< 7) | (data[9]    );
        start_offset += ((data[5]&0x10) ? 20 : 10);
    }

    StartOffset = start_offset;
    if(file_lseek(this, 0, SEEK_SET) != 0)
        return false;

    // Check for a frame header
    bool frame_ok = false;
    if(file_read(this, data, 3) == 3)
    {
        if((BYTE)data[0] == 0xff &&
           ((data[1]&0xfe) == 0xfa/*MPEG-1*/ || (data[1]&0xfe) == 0xf2/*MPEG-2*/))
        {
            int brate_idx = (data[2]>>4) & 0x0f;
            int srate_idx = (data[2]>>2) & 0x03;
            if(brate_idx != 0 && brate_idx != 15 && srate_idx != 3)
                frame_ok = (file_lseek(this, 0, SEEK_SET) == 0);
        }
    }

    if(frame_ok)
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

bool MPG123Decoder::seek(size_t ms_offset)
{
    int enc, channels;
    long srate;

    if(mpg123_getformat(MPG123, &srate, &channels, &enc) == MPG123_OK)
    {
        size_t smp_offset = (size_t)((double)ms_offset / 1000. * srate);
        if(mpg123_seek(MPG123, (off_t)smp_offset, SEEK_SET) >= 0)
        {
            Done = false;
            return true;
        }
    }
    return false;
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
