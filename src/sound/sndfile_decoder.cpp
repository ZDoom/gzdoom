#include "sndfile_decoder.h"
#include "files.h"

#ifdef HAVE_SNDFILE
sf_count_t SndFileDecoder::file_get_filelen(void *user_data)
{
    FileReader *reader = reinterpret_cast<SndFileDecoder*>(user_data)->Reader;
    return reader->GetLength();
}

sf_count_t SndFileDecoder::file_seek(sf_count_t offset, int whence, void *user_data)
{
    FileReader *reader = reinterpret_cast<SndFileDecoder*>(user_data)->Reader;

    if(reader->Seek(offset, whence) != 0)
        return -1;
    return reader->Tell();
}

sf_count_t SndFileDecoder::file_read(void *ptr, sf_count_t count, void *user_data)
{
    FileReader *reader = reinterpret_cast<SndFileDecoder*>(user_data)->Reader;
    return reader->Read(ptr, count);
}

sf_count_t SndFileDecoder::file_write(const void *ptr, sf_count_t count, void *user_data)
{
    return -1;
}

sf_count_t SndFileDecoder::file_tell(void *user_data)
{
    FileReader *reader = reinterpret_cast<SndFileDecoder*>(user_data)->Reader;
    return reader->Tell();
}


SndFileDecoder::~SndFileDecoder()
{
    if(SndFile)
        sf_close(SndFile);
    SndFile = 0;
}

bool SndFileDecoder::open(FileReader *reader)
{
    SF_VIRTUAL_IO sfio = { file_get_filelen, file_seek, file_read, file_write, file_tell };

    Reader = reader;
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

size_t SndFileDecoder::getSampleLength()
{
    return (SndInfo.frames > 0) ? SndInfo.frames : 0;
}

#endif
