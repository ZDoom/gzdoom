#ifndef SNDFILE_DECODER_H
#define SNDFILE_DECODER_H

#include "i_soundinternal.h"

#ifdef HAVE_SNDFILE

#include "sndfile.h"

struct SndFileDecoder : public SoundDecoder
{
    virtual void getInfo(int *samplerate, ChannelConfig *chans, SampleType *type);

    virtual size_t read(char *buffer, size_t bytes);
    virtual TArray<char> readAll();
    virtual bool seek(size_t ms_offset);
    virtual size_t getSampleOffset();
    virtual size_t getSampleLength();

    SndFileDecoder() : SndFile(0) { }
    virtual ~SndFileDecoder();

protected:
    virtual bool open(FileReader *reader);

private:
    SNDFILE *SndFile;
    SF_INFO SndInfo;

    FileReader *Reader;
    static sf_count_t file_get_filelen(void *user_data);
    static sf_count_t file_seek(sf_count_t offset, int whence, void *user_data);
    static sf_count_t file_read(void *ptr, sf_count_t count, void *user_data);
    static sf_count_t file_write(const void *ptr, sf_count_t count, void *user_data);
    static sf_count_t file_tell(void *user_data);

    // Make non-copyable
    SndFileDecoder(const SndFileDecoder &rhs);
    SndFileDecoder& operator=(const SndFileDecoder &rhs);
};

#endif

#endif /* SNDFILE_DECODER_H */
