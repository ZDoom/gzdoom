#ifndef SNDFILE_DECODER_H
#define SNDFILE_DECODER_H

#include "i_soundinternal.h"

#ifdef HAVE_SNDFILE

#include "sndfile.h"

struct SndFileDecoder : public SoundDecoder
{
    virtual void getInfo(int *samplerate, ChannelConfig *chans, SampleType *type);

    virtual size_t read(char *buffer, size_t bytes);
    virtual std::vector<char> readAll();
    virtual bool seek(size_t ms_offset);
    virtual size_t getSampleOffset();

    SndFileDecoder() : SndFile(0), File(0) { }
    virtual ~SndFileDecoder();

protected:
    virtual bool open(const char *data, size_t length);
    virtual bool open(const char *fname, size_t offset, size_t length);

private:
    SNDFILE *SndFile;
    SF_INFO SndInfo;

    FILE *File;
    size_t FileLength;
    size_t FileOffset;
    static sf_count_t file_get_filelen(void *user_data);
    static sf_count_t file_seek(sf_count_t offset, int whence, void *user_data);
    static sf_count_t file_read(void *ptr, sf_count_t count, void *user_data);
    static sf_count_t file_write(const void *ptr, sf_count_t count, void *user_data);
    static sf_count_t file_tell(void *user_data);

    const char *MemData;
    size_t MemLength;
    size_t MemPos;
    static sf_count_t mem_get_filelen(void *user_data);
    static sf_count_t mem_seek(sf_count_t offset, int whence, void *user_data);
    static sf_count_t mem_read(void *ptr, sf_count_t count, void *user_data);
    static sf_count_t mem_write(const void *ptr, sf_count_t count, void *user_data);
    static sf_count_t mem_tell(void *user_data);

    // Make non-copyable
    SndFileDecoder(const SndFileDecoder &rhs);
    SndFileDecoder& operator=(const SndFileDecoder &rhs);
};

#endif

#endif /* SNDFILE_DECODER_H */
