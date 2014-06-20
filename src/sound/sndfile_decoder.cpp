#include "i_soundinternal.h"

#ifdef HAVE_SNDFILE
sf_count_t SndFileDecoder::file_get_filelen(void *user_data)
{
    SndFileDecoder *self = reinterpret_cast<SndFileDecoder*>(user_data);
    return self->FileLength;
}

sf_count_t SndFileDecoder::file_seek(sf_count_t offset, int whence, void *user_data)
{
    SndFileDecoder *self = reinterpret_cast<SndFileDecoder*>(user_data);

    long cur = ftell(self->File);
    if(cur < 0) return -1;

    switch(whence)
    {
        case SEEK_SET:
            if(offset < 0 || offset > (sf_count_t)self->FileLength)
                return -1;
            cur = offset;
            break;

        case SEEK_CUR:
            cur -= self->FileOffset;
            if((offset > 0 && (sf_count_t)(self->FileLength-cur) < offset) ||
               (offset < 0 && (sf_count_t)cur < -offset))
                return -1;
            cur += offset;
            break;

        case SEEK_END:
            if(offset > 0 || -offset > (sf_count_t)self->FileLength)
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

sf_count_t SndFileDecoder::file_read(void *ptr, sf_count_t count, void *user_data)
{
    SndFileDecoder *self = reinterpret_cast<SndFileDecoder*>(user_data);

    long cur = ftell(self->File);
    if(cur < 0) return -1;

    cur -= self->FileOffset;
    if(count > (sf_count_t)(self->FileLength-cur))
        count = self->FileLength-cur;

    return fread(ptr, 1, count, self->File);
}

sf_count_t SndFileDecoder::file_write(const void *ptr, sf_count_t count, void *user_data)
{
    return -1;
}

sf_count_t SndFileDecoder::file_tell(void *user_data)
{
    SndFileDecoder *self = reinterpret_cast<SndFileDecoder*>(user_data);

    long cur = ftell(self->File);
    if(cur < 0 || (unsigned long)cur < self->FileLength)
        return -1;
    return cur - self->FileOffset;
}


sf_count_t SndFileDecoder::mem_get_filelen(void *user_data)
{
    SndFileDecoder *self = reinterpret_cast<SndFileDecoder*>(user_data);
    return self->MemLength;
}

sf_count_t SndFileDecoder::mem_seek(sf_count_t offset, int whence, void *user_data)
{
    SndFileDecoder *self = reinterpret_cast<SndFileDecoder*>(user_data);

    switch(whence)
    {
        case SEEK_SET:
            if(offset < 0 || offset > (sf_count_t)self->MemLength)
                return -1;
            self->MemPos = offset;
            break;

        case SEEK_CUR:
            if((offset > 0 && (sf_count_t)(self->MemLength-self->MemPos) < offset) ||
               (offset < 0 && (sf_count_t)self->MemPos < -offset))
                return -1;
            self->MemPos += offset;
            break;

        case SEEK_END:
            if(offset > 0 || -offset > (sf_count_t)self->MemLength)
                return -1;
            self->MemPos = self->MemLength + offset;
            break;

        default:
            return -1;
    }

    return self->MemPos;
}

sf_count_t SndFileDecoder::mem_read(void *ptr, sf_count_t count, void *user_data)
{
    SndFileDecoder *self = reinterpret_cast<SndFileDecoder*>(user_data);

    if(count > (sf_count_t)(self->MemLength-self->MemPos))
        count = self->MemLength-self->MemPos;

    memcpy(ptr, self->MemData+self->MemPos, count);
    self->MemPos += count;

    return count;
}

sf_count_t SndFileDecoder::mem_write(const void *ptr, sf_count_t count, void *user_data)
{
    return -1;
}

sf_count_t SndFileDecoder::mem_tell(void *user_data)
{
    SndFileDecoder *self = reinterpret_cast<SndFileDecoder*>(user_data);
    return self->MemPos;
}


SndFileDecoder::~SndFileDecoder()
{
    if(SndFile)
        sf_close(SndFile);
    SndFile = 0;
    if(File)
        fclose(File);
    File = 0;
}

bool SndFileDecoder::open(const char *data, size_t length)
{
    SF_VIRTUAL_IO sfio = { mem_get_filelen, mem_seek, mem_read, mem_write, mem_tell };

    MemData = data;
    MemPos = 0;
    MemLength = length;
    SndFile = sf_open_virtual(&sfio, SFM_READ, &SndInfo, this);
    if(SndFile)
    {
        if(SndInfo.channels == 1 || SndInfo.channels == 2)
            return true;

        sf_close(SndFile);
        SndFile = 0;
    }

    return false;
}

bool SndFileDecoder::open(const char *fname, size_t offset, size_t length)
{
    SF_VIRTUAL_IO sfio = { file_get_filelen, file_seek, file_read, file_write, file_tell };

    FileOffset = offset;
    FileLength = length;
    File = fopen(fname, "rb");
    if(File)
    {
        if(fseek(File, offset, SEEK_SET) == 0)
        {
            SndFile = sf_open_virtual(&sfio, SFM_READ, &SndInfo, this);
            if(SndFile)
            {
                if(SndInfo.channels == 1 || SndInfo.channels == 2)
                    return true;

                sf_close(SndFile);
                SndFile = 0;
            }
        }
        fclose(File);
        File = 0;
    }

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
    int framesize = 2 * SndInfo.channels;
    return sf_readf_short(SndFile, (short*)buffer, bytes/framesize) * framesize;
}

std::vector<char> SndFileDecoder::readAll()
{
    if(SndInfo.frames <= 0)
        return SoundDecoder::readAll();

    int framesize = 2 * SndInfo.channels;
    std::vector<char> output;

    output.resize(SndInfo.frames * framesize);
    size_t got = read(&output[0], output.size());
    output.resize(got);

    return output;
}

bool SndFileDecoder::seek(size_t ms_offset)
{
    size_t smp_offset = (size_t)((double)ms_offset / 1000. * SndInfo.samplerate);
    if(sf_seek(SndFile, smp_offset, SEEK_SET) < 0)
        return false;
    return true;
}

size_t SndFileDecoder::getSampleOffset()
{
    return sf_seek(SndFile, 0, SEEK_CUR);
}

#endif
