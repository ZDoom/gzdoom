#include "mpg123_decoder.h"

#ifdef HAVE_MPG123
static bool inited = false;


off_t MPG123Decoder::file_lseek(void *handle, off_t offset, int whence)
{
    MPG123Decoder *self = reinterpret_cast<MPG123Decoder*>(handle);

    long cur = ftell(self->File);
    if(cur < 0) return -1;

    switch(whence)
    {
        case SEEK_SET:
            if(offset < 0 || offset > (off_t)self->FileLength)
                return -1;
            cur = offset;
            break;

        case SEEK_CUR:
            cur -= self->FileOffset;
            if((offset > 0 && (off_t)(self->FileLength-cur) < offset) ||
               (offset < 0 && (off_t)cur < -offset))
                return -1;
            cur += offset;
            break;

        case SEEK_END:
            if(offset > 0 || -offset > (off_t)self->FileLength)
                return -1;
            cur = self->FileLength + offset;
            break;

        default:
            return -1;
    }

    if(fseek(self->File, cur + self->FileOffset, SEEK_SET) != 0)
        return -1;
    return cur;
}

ssize_t MPG123Decoder::file_read(void *handle, void *buffer, size_t bytes)
{
    MPG123Decoder *self = reinterpret_cast<MPG123Decoder*>(handle);

    long cur = ftell(self->File);
    if(cur < 0) return -1;

    cur -= self->FileOffset;
    if(bytes > (size_t)(self->FileLength-cur))
        bytes = self->FileLength-cur;

    return fread(buffer, 1, bytes, self->File);
}


off_t MPG123Decoder::mem_lseek(void *handle, off_t offset, int whence)
{
    MPG123Decoder *self = reinterpret_cast<MPG123Decoder*>(handle);

    switch(whence)
    {
        case SEEK_SET:
            if(offset < 0 || offset > (off_t)self->MemLength)
                return -1;
            self->MemPos = offset;
            break;

        case SEEK_CUR:
            if((offset > 0 && (off_t)(self->MemLength-self->MemPos) < offset) ||
               (offset < 0 && (off_t)self->MemPos < -offset))
                return -1;
            self->MemPos += offset;
            break;

        case SEEK_END:
            if(offset > 0 || -offset > (off_t)self->MemLength)
                return -1;
            self->MemPos = self->MemLength + offset;
            break;

        default:
            return -1;
    }

    return self->MemPos;
}

ssize_t MPG123Decoder::mem_read(void *handle, void *buffer, size_t bytes)
{
    MPG123Decoder *self = reinterpret_cast<MPG123Decoder*>(handle);

    if(bytes > self->MemLength-self->MemPos)
        bytes = self->MemLength-self->MemPos;

    memcpy(buffer, self->MemData+self->MemPos, bytes);
    self->MemPos += bytes;

    return bytes;
}


MPG123Decoder::~MPG123Decoder()
{
    if(MPG123)
    {
        mpg123_close(MPG123);
        mpg123_delete(MPG123);
        MPG123 = 0;
    }
    if(File)
        fclose(File);
    File = 0;
}

bool MPG123Decoder::open(const char *data, size_t length)
{
    if(!inited)
    {
        if(mpg123_init() != MPG123_OK)
            return false;
        inited = true;
    }

    // Check for ID3 tags and skip them
    if(length > 10 && memcmp(data, "ID3", 3) == 0 &&
       (BYTE)data[3] <= 4 && (BYTE)data[4] != 0xff &&
       (data[5]&0x0f) == 0 && (data[6]&0x80) == 0 &&
       (data[7]&0x80) == 0 && (data[8]&0x80) == 0 &&
       (data[9]&0x80) == 0)
    {
        // ID3v2
        int start_offset;
        start_offset = (data[6]<<21) | (data[7]<<14) |
                       (data[8]<< 7) | (data[9]    );
        start_offset += ((data[5]&0x10) ? 20 : 10);
        length -= start_offset;
        data += start_offset;
    }

    if(length > 128 && memcmp(data+length-128, "TAG", 3) == 0) // ID3v1
        length -= 128;

    // Check for a frame header
    bool frame_ok = false;
    if(length > 3)
    {
        if((BYTE)data[0] == 0xff &&
           ((data[1]&0xfe) == 0xfa/*MPEG-1*/ || (data[1]&0xfe) == 0xf2/*MPEG-2*/))
        {
            int brate_idx = (data[2]>>4) & 0x0f;
            int srate_idx = (data[2]>>2) & 0x03;
            if(brate_idx != 0 && brate_idx != 15 && srate_idx != 3)
                frame_ok = true;
        }
    }

    if(frame_ok)
    {
        MemData = data;
        MemPos = 0;
        MemLength = length;

        MPG123 = mpg123_new(NULL, NULL);
        if(mpg123_replace_reader_handle(MPG123, mem_read, mem_lseek, NULL) == MPG123_OK &&
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

bool MPG123Decoder::open(const char *fname, size_t offset, size_t length)
{
    if(!inited)
    {
        if(mpg123_init() != MPG123_OK)
            return false;
        inited = true;
    }

    FileLength = length;
    FileOffset = offset;
    File = fopen(fname, "rb");
    if(!File || fseek(File, FileOffset, SEEK_SET) != 0)
        return false;

    char data[10];
    if(file_read(this, data, 10) != 10)
        return false;

    // Check for ID3 tags and skip them
    if(memcmp(data, "ID3", 3) == 0 &&
       (BYTE)data[3] <= 4 && (BYTE)data[4] != 0xff &&
       (data[5]&0x0f) == 0 && (data[6]&0x80) == 0 &&
       (data[7]&0x80) == 0 && (data[8]&0x80) == 0 &&
       (data[9]&0x80) == 0)
    {
        // ID3v2
        int start_offset;
        start_offset  = (data[6]<<21) | (data[7]<<14) |
                        (data[8]<< 7) | (data[9]    );
        start_offset += ((data[5]&0x10) ? 20 : 10);
        offset += start_offset;
        length -= start_offset;
    }

    if(file_lseek(this, -128, SEEK_END) > 0 && memcmp(data, "TAG", 3) == 0) // ID3v1
        length -= 128;

    FileLength = length;
    FileOffset = offset;
    if(file_lseek(this, 0, SEEK_SET) != 0)
        return false;

    // Check for a frame header
    bool frame_ok = false;
    if(file_read(this, data, 3) != 3)
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
        if(mpg123_seek(MPG123, smp_offset, SEEK_SET) >= 0)
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

#endif
