#include <fmod.h>
#include <FLAC++/decoder.h>
#include "s_sound.h"

class FLACSampleLoader : protected FLAC::Decoder::Stream
{
public:
	FLACSampleLoader (sfxinfo_t *sfx);
	~FLACSampleLoader ();

	FSOUND_SAMPLE *LoadSample (unsigned int samplemode);

	unsigned NumChannels, SampleBits, SampleRate, NumSamples;

protected:
	virtual ::FLAC__StreamDecoderReadStatus read_callback(FLAC__byte buffer[], unsigned *bytes);
	virtual ::FLAC__StreamDecoderWriteStatus write_callback(const ::FLAC__Frame *frame, const FLAC__int32 * const buffer[]);
	virtual void metadata_callback(const ::FLAC__StreamMetadata *metadata);
	virtual void error_callback(::FLAC__StreamDecoderErrorStatus status);

	void CopyToSample (size_t ofs, FLAC__int32 **buffer, size_t samples);

	const FLAC__byte *MemStart;
	const FLAC__byte *MemEnd;
	const FLAC__byte *MemPos;

	void *SBuff, *SBuff2;
	unsigned int SLen, SLen2;
	sfxinfo_t *Sfx;
	bool Dest8;
};
