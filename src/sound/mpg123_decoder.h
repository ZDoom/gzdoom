#ifndef MPG123_DECODER_H
#define MPG123_DECODER_H

#include "i_soundinternal.h"

#ifdef HAVE_MPG123

#ifdef _MSC_VER
typedef int ssize_t;
#endif
#include "mpg123.h"

struct MPG123Decoder : public SoundDecoder
{
    virtual void getInfo(int *samplerate, ChannelConfig *chans, SampleType *type);

    virtual size_t read(char *buffer, size_t bytes);
    virtual bool seek(size_t ms_offset);
    virtual size_t getSampleOffset();
    virtual size_t getSampleLength();

    MPG123Decoder() : MPG123(0) { }
    virtual ~MPG123Decoder();

protected:
    virtual bool open(FileReader *reader);

private:
    mpg123_handle *MPG123;
    bool Done;

    FileReader *Reader;
    int StartOffset;
    static off_t file_lseek(void *handle, off_t offset, int whence);
    static ssize_t file_read(void *handle, void *buffer, size_t bytes);

    // Make non-copyable
    MPG123Decoder(const MPG123Decoder &rhs);
    MPG123Decoder& operator=(const MPG123Decoder &rhs);
};

#endif

#endif /* MPG123_DECODER_H */
