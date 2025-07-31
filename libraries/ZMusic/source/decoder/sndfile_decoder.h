#ifndef SNDFILE_DECODER_H
#define SNDFILE_DECODER_H

#include "zmusic/sounddecoder.h"

#ifdef HAVE_SNDFILE

#ifndef DYN_SNDFILE
#include "sndfile.h"
#else
#include "../thirdparty/sndfile.h"
#endif

struct SndFileDecoder : public SoundDecoder
{
    virtual void getInfo(int *samplerate, ChannelConfig *chans, SampleType *type) override;

    virtual size_t read(char *buffer, size_t bytes) override;
    virtual std::vector<uint8_t> readAll() override;
    virtual bool seek(size_t ms_offset, bool ms, bool mayrestart) override;
	virtual size_t getSampleOffset() override;
    virtual size_t getSampleLength() override;

    SndFileDecoder() = default;
    // Make non-copyable
    SndFileDecoder(const SndFileDecoder& rhs) = delete;
    SndFileDecoder& operator=(const SndFileDecoder& rhs) = delete;

    virtual ~SndFileDecoder();

protected:
    virtual bool open(MusicIO::FileInterface *reader) override;

private:
    SNDFILE *SndFile = nullptr;
    SF_INFO SndInfo;
	MusicIO::FileInterface* Reader = nullptr;

    static sf_count_t file_get_filelen(void *user_data);
    static sf_count_t file_seek(sf_count_t offset, int whence, void *user_data);
    static sf_count_t file_read(void *ptr, sf_count_t count, void *user_data);
    static sf_count_t file_write(const void *ptr, sf_count_t count, void *user_data);
    static sf_count_t file_tell(void *user_data);
};

#else
#include "../thirdparty/sndfile.h"
#endif

#endif /* SNDFILE_DECODER_H */
