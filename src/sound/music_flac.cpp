#include "i_musicinterns.h"
#include "templates.h"

#include <string.h>

#define FLAC__NO_DLL
#include <FLAC++/decoder.h>

class FLACSong::FLACStreamer : protected FLAC::Decoder::Stream
{
public:
	FLACStreamer (FILE *file, char * musiccache, int length);
	~FLACStreamer ();

	bool ServiceStream (void *buff, int len, bool loop);

	unsigned NumChannels, SampleBits, SampleRate;

protected:
	virtual ::FLAC__StreamDecoderReadStatus read_callback(FLAC__byte buffer[], unsigned *bytes);
	virtual ::FLAC__StreamDecoderWriteStatus write_callback(const ::FLAC__Frame *frame, const FLAC__int32 * const buffer[]);
	virtual void metadata_callback(const ::FLAC__StreamMetadata *metadata);
	virtual void error_callback(::FLAC__StreamDecoderErrorStatus status);

	void CopyToStream (void *&sbuff, FLAC__int32 **buffer, size_t ofs, size_t samples);

	FileReader *File;
	long StartPos, EndPos;

	FLAC__int32 *SamplePool[2];
	size_t PoolSize;
	size_t PoolUsed;
	size_t PoolPos;

	void *SBuff;
	size_t SLen;
};

FLACSong::FLACSong (FILE *file, char * musiccache, int length)
	: State (NULL)
{
	State = new FLACStreamer (file, musiccache, length);

	if (State->NumChannels > 0 && State->SampleBits > 0 && State->SampleRate > 0)
	{
		int flags = 0;
		int buffsize = State->SampleRate / 5;

		if (State->SampleBits <= 8)
		{
			flags |= SoundStream::Bits8;
		}
		else
		{
			buffsize <<= 1;
		}
		if (State->NumChannels == 1)
		{
			flags |= SoundStream::Mono;
		}
		else
		{
			buffsize <<= 1;
		}
		m_Stream = GSnd->CreateStream (FillStream, buffsize, flags, State->SampleRate, this);
		if (m_Stream == NULL)
		{
			Printf (PRINT_BOLD, "Could not create music stream.\n");
			return;
		}
	}
}

FLACSong::~FLACSong ()
{
	Stop ();
	if (State != NULL)
	{
		delete State;
	}
}

bool FLACSong::IsValid () const
{
	return m_Stream != NULL;
}

bool FLACSong::IsPlaying ()
{
	return m_Status == STATE_Playing;
}

void FLACSong::Play (bool looping)
{
	m_Status = STATE_Stopped;
	m_Looping = looping;

	if (m_Stream->Play (true, snd_musicvolume))
	{
		m_Status = STATE_Playing;
	}
}

bool FLACSong::FillStream (SoundStream *stream, void *buff, int len, void *userdata)
{
	FLACSong *song = (FLACSong *)userdata;
	return song->State->ServiceStream (buff, len, song->m_Looping);
}

FLACSong::FLACStreamer::FLACStreamer (FILE *iofile, char * musiccache, int length)
	: NumChannels	(0),
	  SampleBits	(0),
	  SampleRate	(0),
	  PoolSize		(0),
	  PoolUsed		(0),
	  PoolPos		(0)
{
	if (iofile != NULL)
	{
		File = new FileReader (iofile, length);
	}
	else
	{
		File = new MemoryReader(musiccache, length);
	}

	StartPos = File->Tell();
	EndPos = StartPos + File->GetLength();
	init ();
	process_until_end_of_metadata ();
}

FLACSong::FLACStreamer::~FLACStreamer ()
{
	if (PoolSize > 0 && SamplePool[0] != NULL)
	{
		delete[] SamplePool[0];
		SamplePool[0] = NULL;
	}
	if (File->GetFile()!=NULL) fclose (File->GetFile());
	delete File;
}

bool FLACSong::FLACStreamer::ServiceStream (void *buff1, int len, bool loop)
{
	void *buff = buff1;
	size_t samples = len;
	size_t poolAvail;
	size_t poolGrab;

	if (NumChannels == 2)
	{
		samples >>= 1;
	}
	if (SampleBits > 8)
	{
		samples >>= 1;
	}

	poolAvail = PoolUsed - PoolPos;
	if (poolAvail > 0)
	{
		poolGrab = MIN (samples, poolAvail);
		CopyToStream (buff, SamplePool, PoolPos, poolGrab);
		PoolPos += poolGrab;
		if (PoolPos == PoolUsed)
		{
			PoolPos = PoolUsed = 0;
		}
		samples -= poolGrab;
	}

	SBuff = buff;
	SLen = samples;

	while (SLen > 0)
	{
		if (get_state() == FLAC__STREAM_DECODER_END_OF_STREAM)
		{
			if (!loop)
			{
				return FALSE;
			}
			File->Seek (StartPos, SEEK_SET);
			reset ();
		}

		if (!process_single ())
		{
			return FALSE;
		}
	}

	return TRUE;
}

void FLACSong::FLACStreamer::CopyToStream (void *&sbuff, FLAC__int32 **buffer, size_t ofs, size_t len)
{
	size_t i;

	if (SampleBits == 16)
	{
		SWORD *buff = (SWORD *)sbuff;
		if (NumChannels == 1)
		{
			for (i = 0; i < len; ++i)
			{
				buff[i] = SWORD(buffer[0][i + ofs]);
			}
			sbuff = buff + i;
		}
		else
		{
			for (i = 0; i < len; ++i)
			{
				buff[i*2] = SWORD(buffer[0][i + ofs]);
				buff[i*2+1] = SWORD(buffer[1][i + ofs]);
			}
			sbuff = buff + i*2;
		}
	}
	else if (SampleBits == 8)
	{
		SBYTE *buff = (SBYTE *)sbuff;
		if (NumChannels == 1)
		{
			for (i = 0; i < len; ++i)
			{
				buff[i] = SBYTE(buffer[0][i + ofs]);
			}
			sbuff = buff + i;
		}
		else
		{
			for (i = 0; i < len; ++i)
			{
				buff[i*2] = SBYTE(buffer[0][i + ofs]);
				buff[i*2+1] = SBYTE(buffer[1][i + ofs]);
			}
			sbuff = buff + i*2;
		}
	}
}

::FLAC__StreamDecoderReadStatus FLACSong::FLACStreamer::read_callback(FLAC__byte buffer[], unsigned *bytes)
{
	if (*bytes > 0)
	{
		long here = File->Tell();
		if (here == EndPos)
		{
			return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
		}
		else
		{
			if (*bytes > (size_t)(EndPos - here))
			{
				*bytes = EndPos - here;
			}
			File->Read (buffer, *bytes);
			return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
		}
	}
	else
	{
		return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
	}
}

::FLAC__StreamDecoderWriteStatus FLACSong::FLACStreamer::write_callback(const ::FLAC__Frame *frame, const FLAC__int32 * const buffer[])
{
	size_t blockSize = frame->header.blocksize;
	size_t blockGrab = 0;
	size_t buffAvail;
	size_t blockOfs;

	buffAvail = SLen;
	blockGrab = MIN (SLen, blockSize);
	CopyToStream (SBuff, (FLAC__int32 **)buffer, 0, blockGrab);
	blockSize -= blockGrab;
	SLen -= blockGrab;
	blockOfs = blockGrab;

	if (blockSize > 0)
	{
		buffAvail = PoolSize - PoolUsed;
		blockGrab = MIN (buffAvail, blockSize);
		memcpy (SamplePool[0] + PoolUsed, buffer[0] + blockOfs, sizeof(buffer[0])*blockGrab);
		if (NumChannels > 1)
		{
			memcpy (SamplePool[1] + PoolUsed, buffer[1] + blockOfs, sizeof(buffer[1])*blockGrab);
		}
		PoolUsed += blockGrab;
	}

	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

void FLACSong::FLACStreamer::metadata_callback(const ::FLAC__StreamMetadata *metadata)
{
	if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO && PoolSize == 0)
	{
		switch (metadata->data.stream_info.bits_per_sample)
		{
			case 8: case 16: break;
			default:
				Printf ("Only FLACs with 8 or 16 bits per sample are supported\n");
				return;
		}
		SampleRate = metadata->data.stream_info.sample_rate;
		NumChannels = (unsigned int)MIN (2u, metadata->data.stream_info.channels);
		SampleBits = metadata->data.stream_info.bits_per_sample;
		PoolSize = metadata->data.stream_info.max_blocksize * 2;

		SamplePool[0] = new FLAC__int32[PoolSize * NumChannels];
		SamplePool[1] = SamplePool[0] + PoolSize;
	}
}

void FLACSong::FLACStreamer::error_callback(::FLAC__StreamDecoderErrorStatus status)
{
	status = status;
}
