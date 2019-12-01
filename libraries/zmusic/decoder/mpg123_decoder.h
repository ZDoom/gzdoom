#ifndef MPG123_DECODER_H
#define MPG123_DECODER_H

#include "zmusic/sounddecoder.h"

#ifdef HAVE_MPG123

#ifdef _MSC_VER
#include <stddef.h>
typedef ptrdiff_t ssize_t;
#endif

#ifndef DYN_MPG123
#include "mpg123.h"
#else
#include "thirdparty/mpg123.h"
#endif

struct MPG123Decoder : public SoundDecoder
{
	virtual void getInfo(int* samplerate, ChannelConfig* chans, SampleType* type) override;

	virtual size_t read(char* buffer, size_t bytes) override;
	virtual bool seek(size_t ms_offset, bool ms, bool mayrestart) override;
	virtual size_t getSampleOffset() override;
	virtual size_t getSampleLength() override;

	MPG123Decoder() : MPG123(0) { }
	virtual ~MPG123Decoder();

protected:
    virtual bool open(MusicIO::FileInterface *reader) override;

private:
    mpg123_handle *MPG123;
    bool Done;

	MusicIO::FileInterface* Reader;
	static off_t file_lseek(void *handle, off_t offset, int whence);
    static ssize_t file_read(void *handle, void *buffer, size_t bytes);

    // Make non-copyable
    MPG123Decoder(const MPG123Decoder &rhs);
    MPG123Decoder& operator=(const MPG123Decoder &rhs);
};

#endif

#endif /* MPG123_DECODER_H */
