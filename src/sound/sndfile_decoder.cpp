#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define USE_WINDOWS_DWORD
#endif

#include "sndfile_decoder.h"
#include "templates.h"
#include "files.h"
#include "xs_Float.h"
#include "except.h"

#ifdef HAVE_SNDFILE

sf_count_t SndFileDecoder::file_get_filelen(void *user_data)
{
    FileReader *reader = reinterpret_cast<SndFileDecoder*>(user_data)->Reader;
    return reader->GetLength();
}

sf_count_t SndFileDecoder::file_seek(sf_count_t offset, int whence, void *user_data)
{
    FileReader *reader = reinterpret_cast<SndFileDecoder*>(user_data)->Reader;

    if(reader->Seek((long)offset, whence) != 0)
        return -1;
    return reader->Tell();
}

sf_count_t SndFileDecoder::file_read(void *ptr, sf_count_t count, void *user_data)
{
    FileReader *reader = reinterpret_cast<SndFileDecoder*>(user_data)->Reader;
    return reader->Read(ptr, (long)count);
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
#ifdef _MSC_VER
	__try {
#endif
		SF_VIRTUAL_IO sfio = { file_get_filelen, file_seek, file_read, file_write, file_tell };

		Reader = reader;
		SndFile = sf_open_virtual(&sfio, SFM_READ, &SndInfo, this);
		if (SndFile)
		{
			if (SndInfo.channels == 1 || SndInfo.channels == 2)
				return true;

			sf_close(SndFile);
			SndFile = 0;
		}
#ifdef _MSC_VER
	} __except (CheckException(GetExceptionCode())) {
		// this means that the delay loaded decoder DLL was not found.
	}
#endif
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
        size_t todo = MIN<size_t>(frames-total, 64/SndInfo.channels);
        float tmp[64];

        size_t got = (size_t)sf_readf_float(SndFile, tmp, todo);
        if(got < todo) frames = total + got;

        for(size_t i = 0;i < got*SndInfo.channels;i++)
            *out++ = (short)xs_CRoundToInt(clamp(tmp[i] * 32767.f, -32768.f, 32767.f));
        total += got;
    }
    return total * SndInfo.channels * 2;
}

TArray<char> SndFileDecoder::readAll()
{
    if(SndInfo.frames <= 0)
        return SoundDecoder::readAll();

    int framesize = 2 * SndInfo.channels;
    TArray<char> output;

    output.Resize((unsigned)(SndInfo.frames * framesize));
    size_t got = read(&output[0], output.Size());
    output.Resize((unsigned)got);

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
    return (size_t)sf_seek(SndFile, 0, SEEK_CUR);
}

size_t SndFileDecoder::getSampleLength()
{
    return (size_t)((SndInfo.frames > 0) ? SndInfo.frames : 0);
}

#endif
