#include <string.h>

#include "sample_flac.h"
#include "w_wad.h"
#include "templates.h"

FLACSampleLoader::FLACSampleLoader (sfxinfo_t *sfx)
	: NumChannels	(0),
	  SampleBits	(0),
	  SampleRate	(0),
	  NumSamples	(0),
	  File			(Wads.OpenLumpNum (sfx->lumpnum)),
	  Sfx			(sfx)
{
	StartPos = File.Tell();
	EndPos = StartPos + File.GetLength();

	init ();
	process_until_end_of_metadata ();
}

FSOUND_SAMPLE *FLACSampleLoader::LoadSample (unsigned int samplemode)
{
	if (NumSamples == 0)
	{
		DPrintf ("FLAC has unknown number of samples\n");
		return NULL;
	}

	FSOUND_SAMPLE *sample;
	unsigned int mode;
	unsigned int bits;

	Sfx->frequency = SampleRate;
	bits = (SampleBits <= 8) ? FSOUND_8BITS : FSOUND_16BITS;
	mode = FSOUND_SIGNED | FSOUND_MONO | FSOUND_LOOP_OFF;

	sample = FSOUND_Sample_Alloc (FSOUND_FREE, NumSamples,
		samplemode | bits | mode, SampleRate, 255, FSOUND_STEREOPAN, 255);
	if (sample == NULL)
	{
		bits ^= FSOUND_8BITS | FSOUND_16BITS;
		sample = FSOUND_Sample_Alloc (FSOUND_FREE, NumSamples,
			samplemode | bits | mode, SampleRate, 255, FSOUND_STEREOPAN, 255);
		if (sample == NULL)
		{
			return sample;
		}
	}

	void *ptr1, *ptr2;
	unsigned int len1, len2;

	if (!FSOUND_Sample_Lock (sample, 0,
		bits & FSOUND_8BITS ? NumSamples : NumSamples * 2,
		&ptr1, &ptr2, &len1, &len2))
	{
		FSOUND_Sample_Free (sample);
		DPrintf ("Failed locking FLAC sample\n");
		return NULL;
	}

	SBuff = ptr1;
	SBuff2 = ptr2;
	SLen = len1;
	SLen2 = len2;
	Dest8 = (bits & FSOUND_8BITS) ? true : false;

	if (!process_until_end_of_stream ())
	{
		FSOUND_Sample_Unlock (sample, ptr1, ptr2, len1, len2);
		FSOUND_Sample_Free (sample);
		DPrintf ("Failed loading FLAC sample\n");
		return NULL;
	}

	FSOUND_Sample_Unlock (sample, ptr1, ptr2, len1, len2);
	return sample;
}

BYTE *FLACSampleLoader::ReadSample (SDWORD *numbytes)
{
	if (NumSamples == 0)
	{
		DPrintf ("FLAC has unknown number of samples\n");
		return NULL;
	}

	BYTE *sfxdata;

	Sfx->frequency = SampleRate;
	Sfx->b16bit = (SampleBits > 8);

	sfxdata = new BYTE[NumSamples << Sfx->b16bit];

	SBuff = sfxdata;
	SBuff2 = NULL;
	SLen = NumSamples;
	SLen2 = 0;
	Dest8 = !Sfx->b16bit;
	*numbytes = NumSamples << Sfx->b16bit;

	if (!process_until_end_of_stream ())
	{
		DPrintf ("Failed loading FLAC sample\n");
		delete[] sfxdata;
		return NULL;
	}

	return sfxdata;
}

FLACSampleLoader::~FLACSampleLoader ()
{
}

void FLACSampleLoader::CopyToSample (size_t ofs, FLAC__int32 **buffer, size_t ilen)
{
	size_t i;
	size_t len = MIN<size_t> (ilen, SLen);
	FLAC__int32 *buffer0 = buffer[0] + ofs;

	if (SampleBits == 16)
	{
		if (!Dest8)
		{
			SWORD *buff = (SWORD *)SBuff;

			if (NumChannels == 1)
			{
				// Mono16 -> Mono16
				for (i = 0; i < len; ++i)
				{
					buff[i] = SWORD(buffer0[i]);
				}
			}
			else
			{
				// Stereo16 -> Mono16
				FLAC__int32 *buffer1 = buffer[1] + ofs;

				for (i = 0; i < len; ++i)
				{
					buff[i] = (buffer0[i] + buffer1[i]) / 2;
				}
			}
			SBuff = buff + i;
		}
		else
		{
			SBYTE *buff = (SBYTE *)SBuff;

			if (NumChannels == 1)
			{
				// Mono16 -> Mono8
				for (i = 0; i < len; ++i)
				{
					buff[i] = buffer0[i] / 256;
				}
			}
			else
			{
				// Stereo16 -> Mono8
				FLAC__int32 *buffer1 = buffer[1] + ofs;

				for (i = 0; i < len; ++i)
				{
					buff[i] = (buffer0[i] + buffer1[i]) / 512;
				}
			}
			SBuff = buff + i;
		}
	}
	else if (SampleBits == 8)
	{
		if (Dest8)
		{
			SBYTE *buff = (SBYTE *)SBuff;

			if (NumChannels == 1)
			{
				// Mono8 -> Mono8
				for (i = 0; i < len; ++i)
				{
					buff[i] = SBYTE(buffer0[i]);
				}
			}
			else
			{
				// Stereo8 -> Mono8
				FLAC__int32 *buffer1 = buffer[1] + ofs;

				for (i = 0; i < len; ++i)
				{
					buff[i*2] = SBYTE(buffer0[i]);
					buff[i*2+1] = SBYTE(buffer1[i]);
				}
			}
			SBuff = buff + i;
		}
		else
		{
			SWORD *buff = (SWORD *)SBuff;

			if (NumChannels == 1)
			{
				// Mono8 -> Mono16
				for (i = 0; i < len; ++i)
				{
					buff[i] = buffer0[i] * 257;
				}
			}
			else
			{
				// Stereo8 -> Mono16
				FLAC__int32 *buffer1 = buffer[1] + ofs;

				for (i = 0; i < len; ++i)
				{
					buff[i*2] = ((buffer0[i] + buffer1[i]) / 2) * 257;
				}
			}
			SBuff = buff + i;
		}
	}

	// If the lock was split into two buffers, do the other one next.
	// (Will this ever happen? Don't think so, but...)
	if (ilen > len)
	{
		SBuff = SBuff2;
		SLen = SLen2;
		SBuff2 = NULL;
		SLen2 = 0;
		if (SLen > 0)
		{
			CopyToSample (len, buffer, ilen - len);
		}
	}
}

::FLAC__StreamDecoderReadStatus FLACSampleLoader::read_callback(FLAC__byte buffer[], unsigned *bytes)
{
	if (*bytes > 0)
	{
		long here = File.Tell();

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
			File.Read (buffer, *bytes);
			return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
		}
	}
	else
	{
		return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
	}
}

::FLAC__StreamDecoderWriteStatus FLACSampleLoader::write_callback(const ::FLAC__Frame *frame, const FLAC__int32 * const buffer[])
{
	CopyToSample (0, (FLAC__int32 **)buffer, frame->header.blocksize);

	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

void FLACSampleLoader::metadata_callback(const ::FLAC__StreamMetadata *metadata)
{
	if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO)
	{
		switch (metadata->data.stream_info.bits_per_sample)
		{
			case 8: case 16: break;
			default:
				DPrintf ("Only FLACs with 8 or 16 bits per sample are supported\n");
				return;
		}
		if (metadata->data.stream_info.total_samples > 0xFFFFFFFF)
		{
			DPrintf ("FLAC is too long\n");
		}
		SampleRate = metadata->data.stream_info.sample_rate;
		NumChannels = (unsigned int)MIN (2u, metadata->data.stream_info.channels);
		SampleBits = metadata->data.stream_info.bits_per_sample;
		NumSamples = (unsigned int)metadata->data.stream_info.total_samples;
	}
}

void FLACSampleLoader::error_callback(::FLAC__StreamDecoderErrorStatus status)
{
	status = status;
}
