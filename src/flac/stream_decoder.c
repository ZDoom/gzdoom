/* libFLAC - Free Lossless Audio Codec library
 * Copyright (C) 2000,2001,2002,2003  Josh Coalson
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * - Neither the name of the Xiph.org Foundation nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h> /* for malloc() */
#include <string.h> /* for memset/memcpy() */
#include "FLAC/assert.h"
#include "protected/stream_decoder.h"
#include "private/bitbuffer.h"
#include "private/bitmath.h"
#include "private/cpu.h"
#include "private/crc.h"
#include "private/fixed.h"
#include "private/format.h"
#include "private/lpc.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef max
#undef max
#endif
#define max(a,b) ((a)>(b)?(a):(b))

/***********************************************************************
 *
 * Private static data
 *
 ***********************************************************************/

static FLAC__byte ID3V2_TAG_[3] = { 'I', 'D', '3' };

/***********************************************************************
 *
 * Private class method prototypes
 *
 ***********************************************************************/

static void set_defaults_(FLAC__StreamDecoder *decoder);
static FLAC__bool allocate_output_(FLAC__StreamDecoder *decoder, unsigned size, unsigned channels);
static FLAC__bool has_id_filtered_(FLAC__StreamDecoder *decoder, FLAC__byte *id);
static FLAC__bool find_metadata_(FLAC__StreamDecoder *decoder);
static FLAC__bool read_metadata_(FLAC__StreamDecoder *decoder);
static FLAC__bool read_metadata_streaminfo_(FLAC__StreamDecoder *decoder, FLAC__bool is_last, unsigned length);
static FLAC__bool read_metadata_seektable_(FLAC__StreamDecoder *decoder, FLAC__bool is_last, unsigned length);
static FLAC__bool read_metadata_vorbiscomment_(FLAC__StreamDecoder *decoder, FLAC__StreamMetadata_VorbisComment *obj);
static FLAC__bool read_metadata_cuesheet_(FLAC__StreamDecoder *decoder, FLAC__StreamMetadata_CueSheet *obj);
static FLAC__bool skip_id3v2_tag_(FLAC__StreamDecoder *decoder);
static FLAC__bool frame_sync_(FLAC__StreamDecoder *decoder);
static FLAC__bool read_frame_(FLAC__StreamDecoder *decoder, FLAC__bool *got_a_frame);
static FLAC__bool read_frame_header_(FLAC__StreamDecoder *decoder);
static FLAC__bool read_subframe_(FLAC__StreamDecoder *decoder, unsigned channel, unsigned bps);
static FLAC__bool read_subframe_constant_(FLAC__StreamDecoder *decoder, unsigned channel, unsigned bps);
static FLAC__bool read_subframe_fixed_(FLAC__StreamDecoder *decoder, unsigned channel, unsigned bps, const unsigned order);
static FLAC__bool read_subframe_lpc_(FLAC__StreamDecoder *decoder, unsigned channel, unsigned bps, const unsigned order);
static FLAC__bool read_subframe_verbatim_(FLAC__StreamDecoder *decoder, unsigned channel, unsigned bps);
static FLAC__bool read_residual_partitioned_rice_(FLAC__StreamDecoder *decoder, unsigned predictor_order, unsigned partition_order, FLAC__EntropyCodingMethod_PartitionedRiceContents *partitioned_rice_contents, FLAC__int32 *residual);
static FLAC__bool read_zero_padding_(FLAC__StreamDecoder *decoder);
static FLAC__bool read_callback_(FLAC__byte buffer[], unsigned *bytes, void *client_data);

/***********************************************************************
 *
 * Private class data
 *
 ***********************************************************************/

typedef struct FLAC__StreamDecoderPrivate {
	FLAC__StreamDecoderReadCallback read_callback;
	FLAC__StreamDecoderWriteCallback write_callback;
	FLAC__StreamDecoderMetadataCallback metadata_callback;
	FLAC__StreamDecoderErrorCallback error_callback;
	void (*local_lpc_restore_signal)(const FLAC__int32 residual[], unsigned data_len, const FLAC__int32 qlp_coeff[], unsigned order, int lp_quantization, FLAC__int32 data[]);
	void (*local_lpc_restore_signal_64bit)(const FLAC__int32 residual[], unsigned data_len, const FLAC__int32 qlp_coeff[], unsigned order, int lp_quantization, FLAC__int32 data[]);
	void (*local_lpc_restore_signal_16bit)(const FLAC__int32 residual[], unsigned data_len, const FLAC__int32 qlp_coeff[], unsigned order, int lp_quantization, FLAC__int32 data[]);
	void *client_data;
	FLAC__BitBuffer *input;
	FLAC__int32 *output[FLAC__MAX_CHANNELS];
	FLAC__int32 *residual[FLAC__MAX_CHANNELS];
	FLAC__EntropyCodingMethod_PartitionedRiceContents partitioned_rice_contents[FLAC__MAX_CHANNELS];
	unsigned output_capacity, output_channels;
	FLAC__uint32 last_frame_number;
	FLAC__uint64 samples_decoded;
	FLAC__bool has_stream_info, has_seek_table;
	FLAC__StreamMetadata stream_info;
	FLAC__StreamMetadata seek_table;
	FLAC__bool metadata_filter[128]; /* MAGIC number 128 == total number of metadata block types == 1 << 7 */
	FLAC__byte *metadata_filter_ids;
	unsigned metadata_filter_ids_count, metadata_filter_ids_capacity; /* units for both are IDs, not bytes */
	FLAC__Frame frame;
	FLAC__bool cached; /* true if there is a byte in lookahead */
	FLAC__CPUInfo cpuinfo;
	FLAC__byte header_warmup[2]; /* contains the sync code and reserved bits */
	FLAC__byte lookahead; /* temp storage when we need to look ahead one byte in the stream */
} FLAC__StreamDecoderPrivate;

/***********************************************************************
 *
 * Public static class data
 *
 ***********************************************************************/

FLAC_API const char * const FLAC__StreamDecoderStateString[] = {
	"FLAC__STREAM_DECODER_SEARCH_FOR_METADATA",
	"FLAC__STREAM_DECODER_READ_METADATA",
	"FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC",
	"FLAC__STREAM_DECODER_READ_FRAME",
	"FLAC__STREAM_DECODER_END_OF_STREAM",
	"FLAC__STREAM_DECODER_ABORTED",
	"FLAC__STREAM_DECODER_UNPARSEABLE_STREAM",
	"FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR",
	"FLAC__STREAM_DECODER_ALREADY_INITIALIZED",
	"FLAC__STREAM_DECODER_INVALID_CALLBACK",
	"FLAC__STREAM_DECODER_UNINITIALIZED"
};

FLAC_API const char * const FLAC__StreamDecoderReadStatusString[] = {
	"FLAC__STREAM_DECODER_READ_STATUS_CONTINUE",
	"FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM",
	"FLAC__STREAM_DECODER_READ_STATUS_ABORT"
};

FLAC_API const char * const FLAC__StreamDecoderWriteStatusString[] = {
	"FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE",
	"FLAC__STREAM_DECODER_WRITE_STATUS_ABORT"
};

FLAC_API const char * const FLAC__StreamDecoderErrorStatusString[] = {
	"FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC",
	"FLAC__STREAM_DECODER_ERROR_STATUS_BAD_HEADER",
	"FLAC__STREAM_DECODER_ERROR_STATUS_FRAME_CRC_MISMATCH"
};

/***********************************************************************
 *
 * Class constructor/destructor
 *
 ***********************************************************************/
FLAC_API FLAC__StreamDecoder *FLAC__stream_decoder_new()
{
	FLAC__StreamDecoder *decoder;
	unsigned i;

	FLAC__ASSERT(sizeof(int) >= 4); /* we want to die right away if this is not true */

	decoder = (FLAC__StreamDecoder*)calloc(1, sizeof(FLAC__StreamDecoder));
	if(decoder == 0) {
		return 0;
	}

	decoder->protected_ = (FLAC__StreamDecoderProtected*)calloc(1, sizeof(FLAC__StreamDecoderProtected));
	if(decoder->protected_ == 0) {
		free(decoder);
		return 0;
	}

	decoder->private_ = (FLAC__StreamDecoderPrivate*)calloc(1, sizeof(FLAC__StreamDecoderPrivate));
	if(decoder->private_ == 0) {
		free(decoder->protected_);
		free(decoder);
		return 0;
	}

	decoder->private_->input = FLAC__bitbuffer_new();
	if(decoder->private_->input == 0) {
		free(decoder->private_);
		free(decoder->protected_);
		free(decoder);
		return 0;
	}

	decoder->private_->metadata_filter_ids_capacity = 16;
	if(0 == (decoder->private_->metadata_filter_ids = (FLAC__byte*)malloc((FLAC__STREAM_METADATA_APPLICATION_ID_LEN/8) * decoder->private_->metadata_filter_ids_capacity))) {
		FLAC__bitbuffer_delete(decoder->private_->input);
		free(decoder->private_);
		free(decoder->protected_);
		free(decoder);
		return 0;
	}

	for(i = 0; i < FLAC__MAX_CHANNELS; i++) {
		decoder->private_->output[i] = 0;
		decoder->private_->residual[i] = 0;
	}

	decoder->private_->output_capacity = 0;
	decoder->private_->output_channels = 0;
	decoder->private_->has_seek_table = false;

	for(i = 0; i < FLAC__MAX_CHANNELS; i++)
		FLAC__format_entropy_coding_method_partitioned_rice_contents_init(&decoder->private_->partitioned_rice_contents[i]);

	set_defaults_(decoder);

	decoder->protected_->state = FLAC__STREAM_DECODER_UNINITIALIZED;

	return decoder;
}

FLAC_API void FLAC__stream_decoder_delete(FLAC__StreamDecoder *decoder)
{
	unsigned i;

	FLAC__ASSERT(0 != decoder);
	FLAC__ASSERT(0 != decoder->protected_);
	FLAC__ASSERT(0 != decoder->private_);
	FLAC__ASSERT(0 != decoder->private_->input);

	FLAC__stream_decoder_finish(decoder);

	if(0 != decoder->private_->metadata_filter_ids)
		free(decoder->private_->metadata_filter_ids);

	FLAC__bitbuffer_delete(decoder->private_->input);

	for(i = 0; i < FLAC__MAX_CHANNELS; i++)
		FLAC__format_entropy_coding_method_partitioned_rice_contents_clear(&decoder->private_->partitioned_rice_contents[i]);

	free(decoder->private_);
	free(decoder->protected_);
	free(decoder);
}

/***********************************************************************
 *
 * Public class methods
 *
 ***********************************************************************/

FLAC_API FLAC__StreamDecoderState FLAC__stream_decoder_init(FLAC__StreamDecoder *decoder)
{
	FLAC__ASSERT(0 != decoder);

	if(decoder->protected_->state != FLAC__STREAM_DECODER_UNINITIALIZED)
		return decoder->protected_->state = FLAC__STREAM_DECODER_ALREADY_INITIALIZED;

	if(0 == decoder->private_->read_callback || 0 == decoder->private_->write_callback || 0 == decoder->private_->metadata_callback || 0 == decoder->private_->error_callback)
		return decoder->protected_->state = FLAC__STREAM_DECODER_INVALID_CALLBACK;

	if(!FLAC__bitbuffer_init(decoder->private_->input))
		return decoder->protected_->state = FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR;

	decoder->private_->last_frame_number = 0;
	decoder->private_->samples_decoded = 0;
	decoder->private_->has_stream_info = false;
	decoder->private_->cached = false;

	/*
	 * get the CPU info and set the function pointers
	 */
	FLAC__cpu_info(&decoder->private_->cpuinfo);
	/* first default to the non-asm routines */
	decoder->private_->local_lpc_restore_signal = FLAC__lpc_restore_signal;
	decoder->private_->local_lpc_restore_signal_64bit = FLAC__lpc_restore_signal_wide;
	decoder->private_->local_lpc_restore_signal_16bit = FLAC__lpc_restore_signal;
	/* now override with asm where appropriate */
#ifndef FLAC__NO_ASM
	if(decoder->private_->cpuinfo.use_asm) {
#ifdef FLAC__CPU_IA32
		FLAC__ASSERT(decoder->private_->cpuinfo.type == FLAC__CPUINFO_TYPE_IA32);
#ifdef FLAC__HAS_NASM
		if(decoder->private_->cpuinfo.data.ia32.mmx) {
			decoder->private_->local_lpc_restore_signal = FLAC__lpc_restore_signal_asm_ia32;
			decoder->private_->local_lpc_restore_signal_16bit = FLAC__lpc_restore_signal_asm_ia32_mmx;
		}
		else {
			decoder->private_->local_lpc_restore_signal = FLAC__lpc_restore_signal_asm_ia32;
			decoder->private_->local_lpc_restore_signal_16bit = FLAC__lpc_restore_signal_asm_ia32;
		}
#endif
#endif
	}
#endif

	if(!FLAC__stream_decoder_reset(decoder))
		return decoder->protected_->state = FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR;

	return decoder->protected_->state;
}

FLAC_API void FLAC__stream_decoder_finish(FLAC__StreamDecoder *decoder)
{
	unsigned i;
	FLAC__ASSERT(0 != decoder);
	if(decoder->protected_->state == FLAC__STREAM_DECODER_UNINITIALIZED)
		return;
	if(0 != decoder->private_->seek_table.data.seek_table.points) {
		free(decoder->private_->seek_table.data.seek_table.points);
		decoder->private_->seek_table.data.seek_table.points = 0;
		decoder->private_->has_seek_table = false;
	}
	FLAC__bitbuffer_free(decoder->private_->input);
	for(i = 0; i < FLAC__MAX_CHANNELS; i++) {
		/* WATCHOUT:
		 * FLAC__lpc_restore_signal_asm_ia32_mmx() requires that the
		 * output arrays have a buffer of up to 3 zeroes in front
		 * (at negative indices) for alignment purposes; we use 4
		 * to keep the data well-aligned.
		 */
		if(0 != decoder->private_->output[i]) {
			free(decoder->private_->output[i]-4);
			decoder->private_->output[i] = 0;
		}
		if(0 != decoder->private_->residual[i]) {
			free(decoder->private_->residual[i]);
			decoder->private_->residual[i] = 0;
		}
	}
	decoder->private_->output_capacity = 0;
	decoder->private_->output_channels = 0;

	set_defaults_(decoder);

	decoder->protected_->state = FLAC__STREAM_DECODER_UNINITIALIZED;
}

FLAC_API FLAC__bool FLAC__stream_decoder_set_read_callback(FLAC__StreamDecoder *decoder, FLAC__StreamDecoderReadCallback value)
{
	FLAC__ASSERT(0 != decoder);
	FLAC__ASSERT(0 != decoder->private_);
	FLAC__ASSERT(0 != decoder->protected_);
	if(decoder->protected_->state != FLAC__STREAM_DECODER_UNINITIALIZED)
		return false;
	decoder->private_->read_callback = value;
	return true;
}

FLAC_API FLAC__bool FLAC__stream_decoder_set_write_callback(FLAC__StreamDecoder *decoder, FLAC__StreamDecoderWriteCallback value)
{
	FLAC__ASSERT(0 != decoder);
	FLAC__ASSERT(0 != decoder->private_);
	FLAC__ASSERT(0 != decoder->protected_);
	if(decoder->protected_->state != FLAC__STREAM_DECODER_UNINITIALIZED)
		return false;
	decoder->private_->write_callback = value;
	return true;
}

FLAC_API FLAC__bool FLAC__stream_decoder_set_metadata_callback(FLAC__StreamDecoder *decoder, FLAC__StreamDecoderMetadataCallback value)
{
	FLAC__ASSERT(0 != decoder);
	FLAC__ASSERT(0 != decoder->private_);
	FLAC__ASSERT(0 != decoder->protected_);
	if(decoder->protected_->state != FLAC__STREAM_DECODER_UNINITIALIZED)
		return false;
	decoder->private_->metadata_callback = value;
	return true;
}

FLAC_API FLAC__bool FLAC__stream_decoder_set_error_callback(FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorCallback value)
{
	FLAC__ASSERT(0 != decoder);
	FLAC__ASSERT(0 != decoder->private_);
	FLAC__ASSERT(0 != decoder->protected_);
	if(decoder->protected_->state != FLAC__STREAM_DECODER_UNINITIALIZED)
		return false;
	decoder->private_->error_callback = value;
	return true;
}

FLAC_API FLAC__bool FLAC__stream_decoder_set_client_data(FLAC__StreamDecoder *decoder, void *value)
{
	FLAC__ASSERT(0 != decoder);
	FLAC__ASSERT(0 != decoder->private_);
	FLAC__ASSERT(0 != decoder->protected_);
	if(decoder->protected_->state != FLAC__STREAM_DECODER_UNINITIALIZED)
		return false;
	decoder->private_->client_data = value;
	return true;
}

FLAC_API FLAC__bool FLAC__stream_decoder_set_metadata_respond(FLAC__StreamDecoder *decoder, FLAC__MetadataType type)
{
	FLAC__ASSERT(0 != decoder);
	FLAC__ASSERT(0 != decoder->private_);
	FLAC__ASSERT(0 != decoder->protected_);
	FLAC__ASSERT((unsigned)type < (1u << FLAC__STREAM_METADATA_TYPE_LEN));
	/* double protection */
	if((unsigned)type >= (1u << FLAC__STREAM_METADATA_TYPE_LEN))
		return false;
	if(decoder->protected_->state != FLAC__STREAM_DECODER_UNINITIALIZED)
		return false;
	decoder->private_->metadata_filter[type] = true;
	if(type == FLAC__METADATA_TYPE_APPLICATION)
		decoder->private_->metadata_filter_ids_count = 0;
	return true;
}

FLAC_API FLAC__bool FLAC__stream_decoder_set_metadata_respond_application(FLAC__StreamDecoder *decoder, const FLAC__byte id[4])
{
	FLAC__ASSERT(0 != decoder);
	FLAC__ASSERT(0 != decoder->private_);
	FLAC__ASSERT(0 != decoder->protected_);
	FLAC__ASSERT(0 != id);
	if(decoder->protected_->state != FLAC__STREAM_DECODER_UNINITIALIZED)
		return false;

	if(decoder->private_->metadata_filter[FLAC__METADATA_TYPE_APPLICATION])
		return true;

	FLAC__ASSERT(0 != decoder->private_->metadata_filter_ids);

	if(decoder->private_->metadata_filter_ids_count == decoder->private_->metadata_filter_ids_capacity) {
		if(0 == (decoder->private_->metadata_filter_ids = (FLAC__byte*)realloc(decoder->private_->metadata_filter_ids, decoder->private_->metadata_filter_ids_capacity * 2)))
			return decoder->protected_->state = FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR;
		decoder->private_->metadata_filter_ids_capacity *= 2;
	}

	memcpy(decoder->private_->metadata_filter_ids + decoder->private_->metadata_filter_ids_count * (FLAC__STREAM_METADATA_APPLICATION_ID_LEN/8), id, (FLAC__STREAM_METADATA_APPLICATION_ID_LEN/8));
	decoder->private_->metadata_filter_ids_count++;

	return true;
}

FLAC_API FLAC__bool FLAC__stream_decoder_set_metadata_respond_all(FLAC__StreamDecoder *decoder)
{
	unsigned i;
	FLAC__ASSERT(0 != decoder);
	FLAC__ASSERT(0 != decoder->private_);
	FLAC__ASSERT(0 != decoder->protected_);
	if(decoder->protected_->state != FLAC__STREAM_DECODER_UNINITIALIZED)
		return false;
	for(i = 0; i < sizeof(decoder->private_->metadata_filter) / sizeof(decoder->private_->metadata_filter[0]); i++)
		decoder->private_->metadata_filter[i] = true;
	decoder->private_->metadata_filter_ids_count = 0;
	return true;
}

FLAC_API FLAC__bool FLAC__stream_decoder_set_metadata_ignore(FLAC__StreamDecoder *decoder, FLAC__MetadataType type)
{
	FLAC__ASSERT(0 != decoder);
	FLAC__ASSERT(0 != decoder->private_);
	FLAC__ASSERT(0 != decoder->protected_);
	FLAC__ASSERT((unsigned)type < (1u << FLAC__STREAM_METADATA_TYPE_LEN));
	/* double protection */
	if((unsigned)type >= (1u << FLAC__STREAM_METADATA_TYPE_LEN))
		return false;
	if(decoder->protected_->state != FLAC__STREAM_DECODER_UNINITIALIZED)
		return false;
	decoder->private_->metadata_filter[type] = false;
	if(type == FLAC__METADATA_TYPE_APPLICATION)
		decoder->private_->metadata_filter_ids_count = 0;
	return true;
}

FLAC_API FLAC__bool FLAC__stream_decoder_set_metadata_ignore_application(FLAC__StreamDecoder *decoder, const FLAC__byte id[4])
{
	FLAC__ASSERT(0 != decoder);
	FLAC__ASSERT(0 != decoder->private_);
	FLAC__ASSERT(0 != decoder->protected_);
	FLAC__ASSERT(0 != id);
	if(decoder->protected_->state != FLAC__STREAM_DECODER_UNINITIALIZED)
		return false;

	if(!decoder->private_->metadata_filter[FLAC__METADATA_TYPE_APPLICATION])
		return true;

	FLAC__ASSERT(0 != decoder->private_->metadata_filter_ids);

	if(decoder->private_->metadata_filter_ids_count == decoder->private_->metadata_filter_ids_capacity) {
		if(0 == (decoder->private_->metadata_filter_ids = (FLAC__byte*)realloc(decoder->private_->metadata_filter_ids, decoder->private_->metadata_filter_ids_capacity * 2)))
			return decoder->protected_->state = FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR;
		decoder->private_->metadata_filter_ids_capacity *= 2;
	}

	memcpy(decoder->private_->metadata_filter_ids + decoder->private_->metadata_filter_ids_count * (FLAC__STREAM_METADATA_APPLICATION_ID_LEN/8), id, (FLAC__STREAM_METADATA_APPLICATION_ID_LEN/8));
	decoder->private_->metadata_filter_ids_count++;

	return true;
}

FLAC_API FLAC__bool FLAC__stream_decoder_set_metadata_ignore_all(FLAC__StreamDecoder *decoder)
{
	FLAC__ASSERT(0 != decoder);
	FLAC__ASSERT(0 != decoder->private_);
	FLAC__ASSERT(0 != decoder->protected_);
	if(decoder->protected_->state != FLAC__STREAM_DECODER_UNINITIALIZED)
		return false;
	memset(decoder->private_->metadata_filter, 0, sizeof(decoder->private_->metadata_filter));
	decoder->private_->metadata_filter_ids_count = 0;
	return true;
}

FLAC_API FLAC__StreamDecoderState FLAC__stream_decoder_get_state(const FLAC__StreamDecoder *decoder)
{
	FLAC__ASSERT(0 != decoder);
	FLAC__ASSERT(0 != decoder->protected_);
	return decoder->protected_->state;
}

FLAC_API unsigned FLAC__stream_decoder_get_channels(const FLAC__StreamDecoder *decoder)
{
	FLAC__ASSERT(0 != decoder);
	FLAC__ASSERT(0 != decoder->protected_);
	return decoder->protected_->channels;
}

FLAC_API FLAC__ChannelAssignment FLAC__stream_decoder_get_channel_assignment(const FLAC__StreamDecoder *decoder)
{
	FLAC__ASSERT(0 != decoder);
	FLAC__ASSERT(0 != decoder->protected_);
	return decoder->protected_->channel_assignment;
}

FLAC_API unsigned FLAC__stream_decoder_get_bits_per_sample(const FLAC__StreamDecoder *decoder)
{
	FLAC__ASSERT(0 != decoder);
	FLAC__ASSERT(0 != decoder->protected_);
	return decoder->protected_->bits_per_sample;
}

FLAC_API unsigned FLAC__stream_decoder_get_sample_rate(const FLAC__StreamDecoder *decoder)
{
	FLAC__ASSERT(0 != decoder);
	FLAC__ASSERT(0 != decoder->protected_);
	return decoder->protected_->sample_rate;
}

FLAC_API unsigned FLAC__stream_decoder_get_blocksize(const FLAC__StreamDecoder *decoder)
{
	FLAC__ASSERT(0 != decoder);
	FLAC__ASSERT(0 != decoder->protected_);
	return decoder->protected_->blocksize;
}

FLAC_API FLAC__bool FLAC__stream_decoder_flush(FLAC__StreamDecoder *decoder)
{
	FLAC__ASSERT(0 != decoder);
	FLAC__ASSERT(0 != decoder->private_);
	FLAC__ASSERT(0 != decoder->protected_);

	if(!FLAC__bitbuffer_clear(decoder->private_->input)) {
		decoder->protected_->state = FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR;
		return false;
	}
	decoder->protected_->state = FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC;

	return true;
}

FLAC_API FLAC__bool FLAC__stream_decoder_reset(FLAC__StreamDecoder *decoder)
{
	FLAC__ASSERT(0 != decoder);
	FLAC__ASSERT(0 != decoder->private_);
	FLAC__ASSERT(0 != decoder->protected_);

	if(!FLAC__stream_decoder_flush(decoder)) {
		decoder->protected_->state = FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR;
		return false;
	}
	decoder->protected_->state = FLAC__STREAM_DECODER_SEARCH_FOR_METADATA;

	decoder->private_->samples_decoded = 0;

	return true;
}

FLAC_API FLAC__bool FLAC__stream_decoder_process_single(FLAC__StreamDecoder *decoder)
{
	FLAC__bool got_a_frame;
	FLAC__ASSERT(0 != decoder);
	FLAC__ASSERT(0 != decoder->protected_);

	while(1) {
		switch(decoder->protected_->state) {
			case FLAC__STREAM_DECODER_SEARCH_FOR_METADATA:
				if(!find_metadata_(decoder))
					return false; /* above function sets the status for us */
				break;
			case FLAC__STREAM_DECODER_READ_METADATA:
				if(!read_metadata_(decoder))
					return false; /* above function sets the status for us */
				else
					return true;
			case FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC:
				if(!frame_sync_(decoder))
					return true; /* above function sets the status for us */
				break;
			case FLAC__STREAM_DECODER_READ_FRAME:
				if(!read_frame_(decoder, &got_a_frame))
					return false; /* above function sets the status for us */
				if(got_a_frame)
					return true; /* above function sets the status for us */
				break;
			case FLAC__STREAM_DECODER_END_OF_STREAM:
			case FLAC__STREAM_DECODER_ABORTED:
				return true;
			default:
				FLAC__ASSERT(0);
				return false;
		}
	}
}

FLAC_API FLAC__bool FLAC__stream_decoder_process_until_end_of_metadata(FLAC__StreamDecoder *decoder)
{
	FLAC__ASSERT(0 != decoder);
	FLAC__ASSERT(0 != decoder->protected_);

	while(1) {
		switch(decoder->protected_->state) {
			case FLAC__STREAM_DECODER_SEARCH_FOR_METADATA:
				if(!find_metadata_(decoder))
					return false; /* above function sets the status for us */
				break;
			case FLAC__STREAM_DECODER_READ_METADATA:
				if(!read_metadata_(decoder))
					return false; /* above function sets the status for us */
				break;
			case FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC:
			case FLAC__STREAM_DECODER_READ_FRAME:
			case FLAC__STREAM_DECODER_END_OF_STREAM:
			case FLAC__STREAM_DECODER_ABORTED:
				return true;
			default:
				FLAC__ASSERT(0);
				return false;
		}
	}
}

FLAC_API FLAC__bool FLAC__stream_decoder_process_until_end_of_stream(FLAC__StreamDecoder *decoder)
{
	FLAC__bool dummy;
	FLAC__ASSERT(0 != decoder);
	FLAC__ASSERT(0 != decoder->protected_);

	while(1) {
		switch(decoder->protected_->state) {
			case FLAC__STREAM_DECODER_SEARCH_FOR_METADATA:
				if(!find_metadata_(decoder))
					return false; /* above function sets the status for us */
				break;
			case FLAC__STREAM_DECODER_READ_METADATA:
				if(!read_metadata_(decoder))
					return false; /* above function sets the status for us */
				break;
			case FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC:
				if(!frame_sync_(decoder))
					return true; /* above function sets the status for us */
				break;
			case FLAC__STREAM_DECODER_READ_FRAME:
				if(!read_frame_(decoder, &dummy))
					return false; /* above function sets the status for us */
				break;
			case FLAC__STREAM_DECODER_END_OF_STREAM:
			case FLAC__STREAM_DECODER_ABORTED:
				return true;
			default:
				FLAC__ASSERT(0);
				return false;
		}
	}
}

/***********************************************************************
 *
 * Protected class methods
 *
 ***********************************************************************/

unsigned FLAC__stream_decoder_get_input_bytes_unconsumed(const FLAC__StreamDecoder *decoder)
{
	FLAC__ASSERT(0 != decoder);
	return FLAC__bitbuffer_get_input_bytes_unconsumed(decoder->private_->input);
}

/***********************************************************************
 *
 * Private class methods
 *
 ***********************************************************************/

void set_defaults_(FLAC__StreamDecoder *decoder)
{
	decoder->private_->read_callback = 0;
	decoder->private_->write_callback = 0;
	decoder->private_->metadata_callback = 0;
	decoder->private_->error_callback = 0;
	decoder->private_->client_data = 0;

	memset(decoder->private_->metadata_filter, 0, sizeof(decoder->private_->metadata_filter));
	decoder->private_->metadata_filter[FLAC__METADATA_TYPE_STREAMINFO] = true;
	decoder->private_->metadata_filter_ids_count = 0;
}

FLAC__bool allocate_output_(FLAC__StreamDecoder *decoder, unsigned size, unsigned channels)
{
	unsigned i;
	FLAC__int32 *tmp;

	if(size <= decoder->private_->output_capacity && channels <= decoder->private_->output_channels)
		return true;

	/* simply using realloc() is not practical because the number of channels may change mid-stream */

	for(i = 0; i < FLAC__MAX_CHANNELS; i++) {
		if(0 != decoder->private_->output[i]) {
			free(decoder->private_->output[i]-4);
			decoder->private_->output[i] = 0;
		}
		if(0 != decoder->private_->residual[i]) {
			free(decoder->private_->residual[i]);
			decoder->private_->residual[i] = 0;
		}
	}

	for(i = 0; i < channels; i++) {
		/* WATCHOUT:
		 * FLAC__lpc_restore_signal_asm_ia32_mmx() requires that the
		 * output arrays have a buffer of up to 3 zeroes in front
		 * (at negative indices) for alignment purposes; we use 4
		 * to keep the data well-aligned.
		 */
		tmp = (FLAC__int32*)malloc(sizeof(FLAC__int32)*(size+4));
		if(tmp == 0) {
			decoder->protected_->state = FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR;
			return false;
		}
		memset(tmp, 0, sizeof(FLAC__int32)*4);
		decoder->private_->output[i] = tmp + 4;

		tmp = (FLAC__int32*)malloc(sizeof(FLAC__int32)*size);
		if(tmp == 0) {
			decoder->protected_->state = FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR;
			return false;
		}
		decoder->private_->residual[i] = tmp;
	}

	decoder->private_->output_capacity = size;
	decoder->private_->output_channels = channels;

	return true;
}

FLAC__bool has_id_filtered_(FLAC__StreamDecoder *decoder, FLAC__byte *id)
{
	unsigned i;

	FLAC__ASSERT(0 != decoder);
	FLAC__ASSERT(0 != decoder->private_);

	for(i = 0; i < decoder->private_->metadata_filter_ids_count; i++)
		if(0 == memcmp(decoder->private_->metadata_filter_ids + i * (FLAC__STREAM_METADATA_APPLICATION_ID_LEN/8), id, (FLAC__STREAM_METADATA_APPLICATION_ID_LEN/8)))
			return true;

	return false;
}

FLAC__bool find_metadata_(FLAC__StreamDecoder *decoder)
{
	FLAC__uint32 x;
	unsigned i, id;
	FLAC__bool first = true;

	FLAC__ASSERT(FLAC__bitbuffer_is_consumed_byte_aligned(decoder->private_->input));

	for(i = id = 0; i < 4; ) {
		if(decoder->private_->cached) {
			x = (FLAC__uint32)decoder->private_->lookahead;
			decoder->private_->cached = false;
		}
		else {
			if(!FLAC__bitbuffer_read_raw_uint32(decoder->private_->input, &x, 8, read_callback_, decoder))
				return false; /* the read_callback_ sets the state for us */
		}
		if(x == FLAC__STREAM_SYNC_STRING[i]) {
			first = true;
			i++;
			id = 0;
			continue;
		}
		if(x == ID3V2_TAG_[id]) {
			id++;
			i = 0;
			if(id == 3) {
				if(!skip_id3v2_tag_(decoder))
					return false; /* the read_callback_ sets the state for us */
			}
			continue;
		}
		if(x == 0xff) { /* MAGIC NUMBER for the first 8 frame sync bits */
			decoder->private_->header_warmup[0] = (FLAC__byte)x;
			if(!FLAC__bitbuffer_read_raw_uint32(decoder->private_->input, &x, 8, read_callback_, decoder))
				return false; /* the read_callback_ sets the state for us */

			/* we have to check if we just read two 0xff's in a row; the second may actually be the beginning of the sync code */
			/* else we have to check if the second byte is the end of a sync code */
			if(x == 0xff) { /* MAGIC NUMBER for the first 8 frame sync bits */
				decoder->private_->lookahead = (FLAC__byte)x;
				decoder->private_->cached = true;
			}
			else if(x >> 2 == 0x3e) { /* MAGIC NUMBER for the last 6 sync bits */
				decoder->private_->header_warmup[1] = (FLAC__byte)x;
				decoder->protected_->state = FLAC__STREAM_DECODER_READ_FRAME;
				return true;
			}
		}
		i = 0;
		if(first) {
			decoder->private_->error_callback(decoder, FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC, decoder->private_->client_data);
			first = false;
		}
	}

	decoder->protected_->state = FLAC__STREAM_DECODER_READ_METADATA;
	return true;
}

FLAC__bool read_metadata_(FLAC__StreamDecoder *decoder)
{
	FLAC__bool is_last;
	FLAC__uint32 i, x, type, length;

	FLAC__ASSERT(FLAC__bitbuffer_is_consumed_byte_aligned(decoder->private_->input));

	if(!FLAC__bitbuffer_read_raw_uint32(decoder->private_->input, &x, FLAC__STREAM_METADATA_IS_LAST_LEN, read_callback_, decoder))
		return false; /* the read_callback_ sets the state for us */
	is_last = x? true : false;

	if(!FLAC__bitbuffer_read_raw_uint32(decoder->private_->input, &type, FLAC__STREAM_METADATA_TYPE_LEN, read_callback_, decoder))
		return false; /* the read_callback_ sets the state for us */

	if(!FLAC__bitbuffer_read_raw_uint32(decoder->private_->input, &length, FLAC__STREAM_METADATA_LENGTH_LEN, read_callback_, decoder))
		return false; /* the read_callback_ sets the state for us */

	if(type == FLAC__METADATA_TYPE_STREAMINFO) {
		if(!read_metadata_streaminfo_(decoder, is_last, length))
			return false;

		decoder->private_->has_stream_info = true;
		if(decoder->private_->metadata_filter[FLAC__METADATA_TYPE_STREAMINFO])
			decoder->private_->metadata_callback(decoder, &decoder->private_->stream_info, decoder->private_->client_data);
	}
	else if(type == FLAC__METADATA_TYPE_SEEKTABLE) {
		if(!read_metadata_seektable_(decoder, is_last, length))
			return false;

		decoder->private_->has_seek_table = true;
		if(decoder->private_->metadata_filter[FLAC__METADATA_TYPE_SEEKTABLE])
			decoder->private_->metadata_callback(decoder, &decoder->private_->seek_table, decoder->private_->client_data);
	}
	else {
		FLAC__bool skip_it = !decoder->private_->metadata_filter[type];
		unsigned real_length = length;
		FLAC__StreamMetadata block;

		block.is_last = is_last;
		block.type = (FLAC__MetadataType)type;
		block.length = length;

		if(type == FLAC__METADATA_TYPE_APPLICATION) {
			if(!FLAC__bitbuffer_read_byte_block_aligned_no_crc(decoder->private_->input, block.data.application.id, FLAC__STREAM_METADATA_APPLICATION_ID_LEN/8, read_callback_, decoder))
				return false; /* the read_callback_ sets the state for us */

			real_length -= FLAC__STREAM_METADATA_APPLICATION_ID_LEN/8;

			if(decoder->private_->metadata_filter_ids_count > 0 && has_id_filtered_(decoder, block.data.application.id))
				skip_it = !skip_it;
		}

		if(skip_it) {
			if(!FLAC__bitbuffer_read_byte_block_aligned_no_crc(decoder->private_->input, 0, real_length, read_callback_, decoder))
				return false; /* the read_callback_ sets the state for us */
		}
		else {
			switch(type) {
				case FLAC__METADATA_TYPE_PADDING:
					/* skip the padding bytes */
					if(!FLAC__bitbuffer_read_byte_block_aligned_no_crc(decoder->private_->input, 0, real_length, read_callback_, decoder))
						return false; /* the read_callback_ sets the state for us */
					break;
				case FLAC__METADATA_TYPE_APPLICATION:
					/* remember, we read the ID already */
					if(real_length > 0) {
						if(0 == (block.data.application.data = (FLAC__byte*)malloc(real_length))) {
							decoder->protected_->state = FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR;
							return false;
						}
						if(!FLAC__bitbuffer_read_byte_block_aligned_no_crc(decoder->private_->input, block.data.application.data, real_length, read_callback_, decoder))
							return false; /* the read_callback_ sets the state for us */
					}
					else
						block.data.application.data = 0;
					break;
				case FLAC__METADATA_TYPE_VORBIS_COMMENT:
					if(!read_metadata_vorbiscomment_(decoder, &block.data.vorbis_comment))
						return false;
					break;
				case FLAC__METADATA_TYPE_CUESHEET:
					if(!read_metadata_cuesheet_(decoder, &block.data.cue_sheet))
						return false;
					break;
				case FLAC__METADATA_TYPE_STREAMINFO:
				case FLAC__METADATA_TYPE_SEEKTABLE:
					FLAC__ASSERT(0);
					break;
				default:
					if(real_length > 0) {
						if(0 == (block.data.unknown.data = (FLAC__byte*)malloc(real_length))) {
							decoder->protected_->state = FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR;
							return false;
						}
						if(!FLAC__bitbuffer_read_byte_block_aligned_no_crc(decoder->private_->input, block.data.unknown.data, real_length, read_callback_, decoder))
							return false; /* the read_callback_ sets the state for us */
					}
					else
						block.data.unknown.data = 0;
					break;
			}
			decoder->private_->metadata_callback(decoder, &block, decoder->private_->client_data);

			/* now we have to free any malloc'ed data in the block */
			switch(type) {
				case FLAC__METADATA_TYPE_PADDING:
					break;
				case FLAC__METADATA_TYPE_APPLICATION:
					if(0 != block.data.application.data)
						free(block.data.application.data);
					break;
				case FLAC__METADATA_TYPE_VORBIS_COMMENT:
					if(0 != block.data.vorbis_comment.vendor_string.entry)
						free(block.data.vorbis_comment.vendor_string.entry);
					if(block.data.vorbis_comment.num_comments > 0)
						for(i = 0; i < block.data.vorbis_comment.num_comments; i++)
							if(0 != block.data.vorbis_comment.comments[i].entry)
								free(block.data.vorbis_comment.comments[i].entry);
					if(0 != block.data.vorbis_comment.comments)
						free(block.data.vorbis_comment.comments);
					break;
				case FLAC__METADATA_TYPE_CUESHEET:
					if(block.data.cue_sheet.num_tracks > 0)
						for(i = 0; i < block.data.cue_sheet.num_tracks; i++)
							if(0 != block.data.cue_sheet.tracks[i].indices)
								free(block.data.cue_sheet.tracks[i].indices);
					if(0 != block.data.cue_sheet.tracks)
						free(block.data.cue_sheet.tracks);
					break;
				case FLAC__METADATA_TYPE_STREAMINFO:
				case FLAC__METADATA_TYPE_SEEKTABLE:
					FLAC__ASSERT(0);
				default:
					if(0 != block.data.unknown.data)
						free(block.data.unknown.data);
					break;
			}
		}
	}

	if(is_last)
		decoder->protected_->state = FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC;

	return true;
}

FLAC__bool read_metadata_streaminfo_(FLAC__StreamDecoder *decoder, FLAC__bool is_last, unsigned length)
{
	FLAC__uint32 x;
	unsigned bits, used_bits = 0;

	FLAC__ASSERT(FLAC__bitbuffer_is_consumed_byte_aligned(decoder->private_->input));

	decoder->private_->stream_info.type = FLAC__METADATA_TYPE_STREAMINFO;
	decoder->private_->stream_info.is_last = is_last;
	decoder->private_->stream_info.length = length;

	bits = FLAC__STREAM_METADATA_STREAMINFO_MIN_BLOCK_SIZE_LEN;
	if(!FLAC__bitbuffer_read_raw_uint32(decoder->private_->input, &x, bits, read_callback_, decoder))
		return false; /* the read_callback_ sets the state for us */
	decoder->private_->stream_info.data.stream_info.min_blocksize = x;
	used_bits += bits;

	bits = FLAC__STREAM_METADATA_STREAMINFO_MAX_BLOCK_SIZE_LEN;
	if(!FLAC__bitbuffer_read_raw_uint32(decoder->private_->input, &x, FLAC__STREAM_METADATA_STREAMINFO_MAX_BLOCK_SIZE_LEN, read_callback_, decoder))
		return false; /* the read_callback_ sets the state for us */
	decoder->private_->stream_info.data.stream_info.max_blocksize = x;
	used_bits += bits;

	bits = FLAC__STREAM_METADATA_STREAMINFO_MIN_FRAME_SIZE_LEN;
	if(!FLAC__bitbuffer_read_raw_uint32(decoder->private_->input, &x, FLAC__STREAM_METADATA_STREAMINFO_MIN_FRAME_SIZE_LEN, read_callback_, decoder))
		return false; /* the read_callback_ sets the state for us */
	decoder->private_->stream_info.data.stream_info.min_framesize = x;
	used_bits += bits;

	bits = FLAC__STREAM_METADATA_STREAMINFO_MAX_FRAME_SIZE_LEN;
	if(!FLAC__bitbuffer_read_raw_uint32(decoder->private_->input, &x, FLAC__STREAM_METADATA_STREAMINFO_MAX_FRAME_SIZE_LEN, read_callback_, decoder))
		return false; /* the read_callback_ sets the state for us */
	decoder->private_->stream_info.data.stream_info.max_framesize = x;
	used_bits += bits;

	bits = FLAC__STREAM_METADATA_STREAMINFO_SAMPLE_RATE_LEN;
	if(!FLAC__bitbuffer_read_raw_uint32(decoder->private_->input, &x, FLAC__STREAM_METADATA_STREAMINFO_SAMPLE_RATE_LEN, read_callback_, decoder))
		return false; /* the read_callback_ sets the state for us */
	decoder->private_->stream_info.data.stream_info.sample_rate = x;
	used_bits += bits;

	bits = FLAC__STREAM_METADATA_STREAMINFO_CHANNELS_LEN;
	if(!FLAC__bitbuffer_read_raw_uint32(decoder->private_->input, &x, FLAC__STREAM_METADATA_STREAMINFO_CHANNELS_LEN, read_callback_, decoder))
		return false; /* the read_callback_ sets the state for us */
	decoder->private_->stream_info.data.stream_info.channels = x+1;
	used_bits += bits;

	bits = FLAC__STREAM_METADATA_STREAMINFO_BITS_PER_SAMPLE_LEN;
	if(!FLAC__bitbuffer_read_raw_uint32(decoder->private_->input, &x, FLAC__STREAM_METADATA_STREAMINFO_BITS_PER_SAMPLE_LEN, read_callback_, decoder))
		return false; /* the read_callback_ sets the state for us */
	decoder->private_->stream_info.data.stream_info.bits_per_sample = x+1;
	used_bits += bits;

	bits = FLAC__STREAM_METADATA_STREAMINFO_TOTAL_SAMPLES_LEN;
	if(!FLAC__bitbuffer_read_raw_uint64(decoder->private_->input, &decoder->private_->stream_info.data.stream_info.total_samples, FLAC__STREAM_METADATA_STREAMINFO_TOTAL_SAMPLES_LEN, read_callback_, decoder))
		return false; /* the read_callback_ sets the state for us */
	used_bits += bits;

	if(!FLAC__bitbuffer_read_byte_block_aligned_no_crc(decoder->private_->input, decoder->private_->stream_info.data.stream_info.md5sum, 16, read_callback_, decoder))
		return false; /* the read_callback_ sets the state for us */
	used_bits += 16*8;

	/* skip the rest of the block */
	FLAC__ASSERT(used_bits % 8 == 0);
	length -= (used_bits / 8);
	if(!FLAC__bitbuffer_read_byte_block_aligned_no_crc(decoder->private_->input, 0, length, read_callback_, decoder))
		return false; /* the read_callback_ sets the state for us */

	return true;
}

FLAC__bool read_metadata_seektable_(FLAC__StreamDecoder *decoder, FLAC__bool is_last, unsigned length)
{
	FLAC__uint32 i, x;
	FLAC__uint64 xx;

	FLAC__ASSERT(FLAC__bitbuffer_is_consumed_byte_aligned(decoder->private_->input));

	decoder->private_->seek_table.type = FLAC__METADATA_TYPE_SEEKTABLE;
	decoder->private_->seek_table.is_last = is_last;
	decoder->private_->seek_table.length = length;

	decoder->private_->seek_table.data.seek_table.num_points = length / FLAC__STREAM_METADATA_SEEKPOINT_LENGTH;

	/* use realloc since we may pass through here several times (e.g. after seeking) */
	if(0 == (decoder->private_->seek_table.data.seek_table.points = (FLAC__StreamMetadata_SeekPoint*)realloc(decoder->private_->seek_table.data.seek_table.points, decoder->private_->seek_table.data.seek_table.num_points * sizeof(FLAC__StreamMetadata_SeekPoint)))) {
		decoder->protected_->state = FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR;
		return false;
	}
	for(i = 0; i < decoder->private_->seek_table.data.seek_table.num_points; i++) {
		if(!FLAC__bitbuffer_read_raw_uint64(decoder->private_->input, &xx, FLAC__STREAM_METADATA_SEEKPOINT_SAMPLE_NUMBER_LEN, read_callback_, decoder))
			return false; /* the read_callback_ sets the state for us */
		decoder->private_->seek_table.data.seek_table.points[i].sample_number = xx;

		if(!FLAC__bitbuffer_read_raw_uint64(decoder->private_->input, &xx, FLAC__STREAM_METADATA_SEEKPOINT_STREAM_OFFSET_LEN, read_callback_, decoder))
			return false; /* the read_callback_ sets the state for us */
		decoder->private_->seek_table.data.seek_table.points[i].stream_offset = xx;

		if(!FLAC__bitbuffer_read_raw_uint32(decoder->private_->input, &x, FLAC__STREAM_METADATA_SEEKPOINT_FRAME_SAMPLES_LEN, read_callback_, decoder))
			return false; /* the read_callback_ sets the state for us */
		decoder->private_->seek_table.data.seek_table.points[i].frame_samples = x;
	}
	length -= (decoder->private_->seek_table.data.seek_table.num_points * FLAC__STREAM_METADATA_SEEKPOINT_LENGTH);
	/* if there is a partial point left, skip over it */
	if(length > 0) {
		/*@@@ do an error_callback() here?  there's an argument for either way */
		if(!FLAC__bitbuffer_read_byte_block_aligned_no_crc(decoder->private_->input, 0, length, read_callback_, decoder))
			return false; /* the read_callback_ sets the state for us */
	}

	return true;
}

FLAC__bool read_metadata_vorbiscomment_(FLAC__StreamDecoder *decoder, FLAC__StreamMetadata_VorbisComment *obj)
{
	FLAC__uint32 i;

	FLAC__ASSERT(FLAC__bitbuffer_is_consumed_byte_aligned(decoder->private_->input));

	/* read vendor string */
	FLAC__ASSERT(FLAC__STREAM_METADATA_VORBIS_COMMENT_ENTRY_LENGTH_LEN == 32);
	if(!FLAC__bitbuffer_read_raw_uint32_little_endian(decoder->private_->input, &obj->vendor_string.length, read_callback_, decoder))
		return false; /* the read_callback_ sets the state for us */
	if(obj->vendor_string.length > 0) {
		if(0 == (obj->vendor_string.entry = (FLAC__byte*)malloc(obj->vendor_string.length))) {
			decoder->protected_->state = FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR;
			return false;
		}
		if(!FLAC__bitbuffer_read_byte_block_aligned_no_crc(decoder->private_->input, obj->vendor_string.entry, obj->vendor_string.length, read_callback_, decoder))
			return false; /* the read_callback_ sets the state for us */
	}
	else
		obj->vendor_string.entry = 0;

	/* read num comments */
	FLAC__ASSERT(FLAC__STREAM_METADATA_VORBIS_COMMENT_NUM_COMMENTS_LEN == 32);
	if(!FLAC__bitbuffer_read_raw_uint32_little_endian(decoder->private_->input, &obj->num_comments, read_callback_, decoder))
		return false; /* the read_callback_ sets the state for us */

	/* read comments */
	if(obj->num_comments > 0) {
		if(0 == (obj->comments = (FLAC__StreamMetadata_VorbisComment_Entry*)malloc(obj->num_comments * sizeof(FLAC__StreamMetadata_VorbisComment_Entry)))) {
			decoder->protected_->state = FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR;
			return false;
		}
		for(i = 0; i < obj->num_comments; i++) {
			FLAC__ASSERT(FLAC__STREAM_METADATA_VORBIS_COMMENT_ENTRY_LENGTH_LEN == 32);
			if(!FLAC__bitbuffer_read_raw_uint32_little_endian(decoder->private_->input, &obj->comments[i].length, read_callback_, decoder))
				return false; /* the read_callback_ sets the state for us */
			if(obj->comments[i].length > 0) {
				if(0 == (obj->comments[i].entry = (FLAC__byte*)malloc(obj->comments[i].length))) {
					decoder->protected_->state = FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR;
					return false;
				}
				if(!FLAC__bitbuffer_read_byte_block_aligned_no_crc(decoder->private_->input, obj->comments[i].entry, obj->comments[i].length, read_callback_, decoder))
					return false; /* the read_callback_ sets the state for us */
			}
			else
				obj->comments[i].entry = 0;
		}
	}
	else {
		obj->comments = 0;
	}

	return true;
}

FLAC__bool read_metadata_cuesheet_(FLAC__StreamDecoder *decoder, FLAC__StreamMetadata_CueSheet *obj)
{
	FLAC__uint32 i, j, x;

	FLAC__ASSERT(FLAC__bitbuffer_is_consumed_byte_aligned(decoder->private_->input));

	memset(obj, 0, sizeof(FLAC__StreamMetadata_CueSheet));

	FLAC__ASSERT(FLAC__STREAM_METADATA_CUESHEET_MEDIA_CATALOG_NUMBER_LEN % 8 == 0);
	if(!FLAC__bitbuffer_read_byte_block_aligned_no_crc(decoder->private_->input, obj->media_catalog_number, FLAC__STREAM_METADATA_CUESHEET_MEDIA_CATALOG_NUMBER_LEN/8, read_callback_, decoder))
		return false; /* the read_callback_ sets the state for us */

	if(!FLAC__bitbuffer_read_raw_uint64(decoder->private_->input, &obj->lead_in, FLAC__STREAM_METADATA_CUESHEET_LEAD_IN_LEN, read_callback_, decoder))
		return false; /* the read_callback_ sets the state for us */

	if(!FLAC__bitbuffer_read_raw_uint32(decoder->private_->input, &x, FLAC__STREAM_METADATA_CUESHEET_IS_CD_LEN, read_callback_, decoder))
		return false; /* the read_callback_ sets the state for us */
	obj->is_cd = x? true : false;

	if(!FLAC__bitbuffer_skip_bits_no_crc(decoder->private_->input, FLAC__STREAM_METADATA_CUESHEET_RESERVED_LEN, read_callback_, decoder))
		return false; /* the read_callback_ sets the state for us */

	if(!FLAC__bitbuffer_read_raw_uint32(decoder->private_->input, &x, FLAC__STREAM_METADATA_CUESHEET_NUM_TRACKS_LEN, read_callback_, decoder))
		return false; /* the read_callback_ sets the state for us */
	obj->num_tracks = x;

	if(obj->num_tracks > 0) {
		if(0 == (obj->tracks = (FLAC__StreamMetadata_CueSheet_Track*)calloc(obj->num_tracks, sizeof(FLAC__StreamMetadata_CueSheet_Track)))) {
			decoder->protected_->state = FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR;
			return false;
		}
		for(i = 0; i < obj->num_tracks; i++) {
			FLAC__StreamMetadata_CueSheet_Track *track = &obj->tracks[i];
			if(!FLAC__bitbuffer_read_raw_uint64(decoder->private_->input, &track->offset, FLAC__STREAM_METADATA_CUESHEET_TRACK_OFFSET_LEN, read_callback_, decoder))
				return false; /* the read_callback_ sets the state for us */

			if(!FLAC__bitbuffer_read_raw_uint32(decoder->private_->input, &x, FLAC__STREAM_METADATA_CUESHEET_TRACK_NUMBER_LEN, read_callback_, decoder))
				return false; /* the read_callback_ sets the state for us */
			track->number = (FLAC__byte)x;

			FLAC__ASSERT(FLAC__STREAM_METADATA_CUESHEET_TRACK_ISRC_LEN % 8 == 0);
			if(!FLAC__bitbuffer_read_byte_block_aligned_no_crc(decoder->private_->input, track->isrc, FLAC__STREAM_METADATA_CUESHEET_TRACK_ISRC_LEN/8, read_callback_, decoder))
				return false; /* the read_callback_ sets the state for us */

			if(!FLAC__bitbuffer_read_raw_uint32(decoder->private_->input, &x, FLAC__STREAM_METADATA_CUESHEET_TRACK_TYPE_LEN, read_callback_, decoder))
				return false; /* the read_callback_ sets the state for us */
			track->type = x;

			if(!FLAC__bitbuffer_read_raw_uint32(decoder->private_->input, &x, FLAC__STREAM_METADATA_CUESHEET_TRACK_PRE_EMPHASIS_LEN, read_callback_, decoder))
				return false; /* the read_callback_ sets the state for us */
			track->pre_emphasis = x;

			if(!FLAC__bitbuffer_skip_bits_no_crc(decoder->private_->input, FLAC__STREAM_METADATA_CUESHEET_TRACK_RESERVED_LEN, read_callback_, decoder))
				return false; /* the read_callback_ sets the state for us */

			if(!FLAC__bitbuffer_read_raw_uint32(decoder->private_->input, &x, FLAC__STREAM_METADATA_CUESHEET_TRACK_NUM_INDICES_LEN, read_callback_, decoder))
				return false; /* the read_callback_ sets the state for us */
			track->num_indices = (FLAC__byte)x;

			if(track->num_indices > 0) {
				if(0 == (track->indices = (FLAC__StreamMetadata_CueSheet_Index*)calloc(track->num_indices, sizeof(FLAC__StreamMetadata_CueSheet_Index)))) {
					decoder->protected_->state = FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR;
					return false;
				}
				for(j = 0; j < track->num_indices; j++) {
					FLAC__StreamMetadata_CueSheet_Index *index = &track->indices[j];
					if(!FLAC__bitbuffer_read_raw_uint64(decoder->private_->input, &index->offset, FLAC__STREAM_METADATA_CUESHEET_INDEX_OFFSET_LEN, read_callback_, decoder))
						return false; /* the read_callback_ sets the state for us */

					if(!FLAC__bitbuffer_read_raw_uint32(decoder->private_->input, &x, FLAC__STREAM_METADATA_CUESHEET_INDEX_NUMBER_LEN, read_callback_, decoder))
						return false; /* the read_callback_ sets the state for us */
					index->number = (FLAC__byte)x;

					if(!FLAC__bitbuffer_skip_bits_no_crc(decoder->private_->input, FLAC__STREAM_METADATA_CUESHEET_INDEX_RESERVED_LEN, read_callback_, decoder))
						return false; /* the read_callback_ sets the state for us */
				}
			}
		}
	}

	return true;
}

FLAC__bool skip_id3v2_tag_(FLAC__StreamDecoder *decoder)
{
	FLAC__uint32 x;
	unsigned i, skip;

	/* skip the version and flags bytes */
	if(!FLAC__bitbuffer_read_raw_uint32(decoder->private_->input, &x, 24, read_callback_, decoder))
		return false; /* the read_callback_ sets the state for us */
	/* get the size (in bytes) to skip */
	skip = 0;
	for(i = 0; i < 4; i++) {
		if(!FLAC__bitbuffer_read_raw_uint32(decoder->private_->input, &x, 8, read_callback_, decoder))
			return false; /* the read_callback_ sets the state for us */
		skip <<= 7;
		skip |= (x & 0x7f);
	}
	/* skip the rest of the tag */
	if(!FLAC__bitbuffer_read_byte_block_aligned_no_crc(decoder->private_->input, 0, skip, read_callback_, decoder))
		return false; /* the read_callback_ sets the state for us */
	return true;
}

FLAC__bool frame_sync_(FLAC__StreamDecoder *decoder)
{
	FLAC__uint32 x;
	FLAC__bool first = true;

	/* If we know the total number of samples in the stream, stop if we've read that many. */
	/* This will stop us, for example, from wasting time trying to sync on an ID3V1 tag. */
	if(decoder->private_->has_stream_info && decoder->private_->stream_info.data.stream_info.total_samples) {
		if(decoder->private_->samples_decoded >= decoder->private_->stream_info.data.stream_info.total_samples) {
			decoder->protected_->state = FLAC__STREAM_DECODER_END_OF_STREAM;
			return true;
		}
	}

	/* make sure we're byte aligned */
	if(!FLAC__bitbuffer_is_consumed_byte_aligned(decoder->private_->input)) {
		if(!FLAC__bitbuffer_read_raw_uint32(decoder->private_->input, &x, FLAC__bitbuffer_bits_left_for_byte_alignment(decoder->private_->input), read_callback_, decoder))
			return false; /* the read_callback_ sets the state for us */
	}

	while(1) {
		if(decoder->private_->cached) {
			x = (FLAC__uint32)decoder->private_->lookahead;
			decoder->private_->cached = false;
		}
		else {
			if(!FLAC__bitbuffer_read_raw_uint32(decoder->private_->input, &x, 8, read_callback_, decoder))
				return false; /* the read_callback_ sets the state for us */
		}
		if(x == 0xff) { /* MAGIC NUMBER for the first 8 frame sync bits */
			decoder->private_->header_warmup[0] = (FLAC__byte)x;
			if(!FLAC__bitbuffer_read_raw_uint32(decoder->private_->input, &x, 8, read_callback_, decoder))
				return false; /* the read_callback_ sets the state for us */

			/* we have to check if we just read two 0xff's in a row; the second may actually be the beginning of the sync code */
			/* else we have to check if the second byte is the end of a sync code */
			if(x == 0xff) { /* MAGIC NUMBER for the first 8 frame sync bits */
				decoder->private_->lookahead = (FLAC__byte)x;
				decoder->private_->cached = true;
			}
			else if(x >> 2 == 0x3e) { /* MAGIC NUMBER for the last 6 sync bits */
				decoder->private_->header_warmup[1] = (FLAC__byte)x;
				decoder->protected_->state = FLAC__STREAM_DECODER_READ_FRAME;
				return true;
			}
		}
		if(first) {
			decoder->private_->error_callback(decoder, FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC, decoder->private_->client_data);
			first = false;
		}
	}

	return true;
}

FLAC__bool read_frame_(FLAC__StreamDecoder *decoder, FLAC__bool *got_a_frame)
{
	unsigned channel;
	unsigned i;
	FLAC__int32 mid, side, left, right;
	FLAC__uint16 frame_crc; /* the one we calculate from the input stream */
	FLAC__uint32 x;

	*got_a_frame = false;

	/* init the CRC */
	frame_crc = 0;
	FLAC__CRC16_UPDATE(decoder->private_->header_warmup[0], frame_crc);
	FLAC__CRC16_UPDATE(decoder->private_->header_warmup[1], frame_crc);
	FLAC__bitbuffer_reset_read_crc16(decoder->private_->input, frame_crc);

	if(!read_frame_header_(decoder))
		return false;
	if(decoder->protected_->state == FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC)
		return true;
	if(!allocate_output_(decoder, decoder->private_->frame.header.blocksize, decoder->private_->frame.header.channels))
		return false;
	for(channel = 0; channel < decoder->private_->frame.header.channels; channel++) {
		/*
		 * first figure the correct bits-per-sample of the subframe
		 */
		unsigned bps = decoder->private_->frame.header.bits_per_sample;
		switch(decoder->private_->frame.header.channel_assignment) {
			case FLAC__CHANNEL_ASSIGNMENT_INDEPENDENT:
				/* no adjustment needed */
				break;
			case FLAC__CHANNEL_ASSIGNMENT_LEFT_SIDE:
				FLAC__ASSERT(decoder->private_->frame.header.channels == 2);
				if(channel == 1)
					bps++;
				break;
			case FLAC__CHANNEL_ASSIGNMENT_RIGHT_SIDE:
				FLAC__ASSERT(decoder->private_->frame.header.channels == 2);
				if(channel == 0)
					bps++;
				break;
			case FLAC__CHANNEL_ASSIGNMENT_MID_SIDE:
				FLAC__ASSERT(decoder->private_->frame.header.channels == 2);
				if(channel == 1)
					bps++;
				break;
			default:
				FLAC__ASSERT(0);
		}
		/*
		 * now read it
		 */
		if(!read_subframe_(decoder, channel, bps))
			return false;
		if(decoder->protected_->state != FLAC__STREAM_DECODER_READ_FRAME) {
			decoder->protected_->state = FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC;
			return true;
		}
	}
	if(!read_zero_padding_(decoder))
		return false;

	/*
	 * Read the frame CRC-16 from the footer and check
	 */
	frame_crc = FLAC__bitbuffer_get_read_crc16(decoder->private_->input);
	if(!FLAC__bitbuffer_read_raw_uint32(decoder->private_->input, &x, FLAC__FRAME_FOOTER_CRC_LEN, read_callback_, decoder))
		return false; /* the read_callback_ sets the state for us */
	if(frame_crc == (FLAC__uint16)x) {
		/* Undo any special channel coding */
		switch(decoder->private_->frame.header.channel_assignment) {
			case FLAC__CHANNEL_ASSIGNMENT_INDEPENDENT:
				/* do nothing */
				break;
			case FLAC__CHANNEL_ASSIGNMENT_LEFT_SIDE:
				FLAC__ASSERT(decoder->private_->frame.header.channels == 2);
				for(i = 0; i < decoder->private_->frame.header.blocksize; i++)
					decoder->private_->output[1][i] = decoder->private_->output[0][i] - decoder->private_->output[1][i];
				break;
			case FLAC__CHANNEL_ASSIGNMENT_RIGHT_SIDE:
				FLAC__ASSERT(decoder->private_->frame.header.channels == 2);
				for(i = 0; i < decoder->private_->frame.header.blocksize; i++)
					decoder->private_->output[0][i] += decoder->private_->output[1][i];
				break;
			case FLAC__CHANNEL_ASSIGNMENT_MID_SIDE:
				FLAC__ASSERT(decoder->private_->frame.header.channels == 2);
				for(i = 0; i < decoder->private_->frame.header.blocksize; i++) {
					mid = decoder->private_->output[0][i];
					side = decoder->private_->output[1][i];
					mid <<= 1;
					if(side & 1) /* i.e. if 'side' is odd... */
						mid++;
					left = mid + side;
					right = mid - side;
					decoder->private_->output[0][i] = left >> 1;
					decoder->private_->output[1][i] = right >> 1;
				}
				break;
			default:
				FLAC__ASSERT(0);
				break;
		}
	}
	else {
		/* Bad frame, emit error and zero the output signal */
		decoder->private_->error_callback(decoder, FLAC__STREAM_DECODER_ERROR_STATUS_FRAME_CRC_MISMATCH, decoder->private_->client_data);
		for(channel = 0; channel < decoder->private_->frame.header.channels; channel++) {
			memset(decoder->private_->output[channel], 0, sizeof(FLAC__int32) * decoder->private_->frame.header.blocksize);
		}
	}

	*got_a_frame = true;

	/* put the latest values into the public section of the decoder instance */
	decoder->protected_->channels = decoder->private_->frame.header.channels;
	decoder->protected_->channel_assignment = decoder->private_->frame.header.channel_assignment;
	decoder->protected_->bits_per_sample = decoder->private_->frame.header.bits_per_sample;
	decoder->protected_->sample_rate = decoder->private_->frame.header.sample_rate;
	decoder->protected_->blocksize = decoder->private_->frame.header.blocksize;

	FLAC__ASSERT(decoder->private_->frame.header.number_type == FLAC__FRAME_NUMBER_TYPE_SAMPLE_NUMBER);
	decoder->private_->samples_decoded = decoder->private_->frame.header.number.sample_number + decoder->private_->frame.header.blocksize;

	/* write it */
	if(decoder->private_->write_callback(decoder, &decoder->private_->frame, (const FLAC__int32 * const *)decoder->private_->output, decoder->private_->client_data) != FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE)
		return false;

	decoder->protected_->state = FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC;
	return true;
}

FLAC__bool read_frame_header_(FLAC__StreamDecoder *decoder)
{
	FLAC__uint32 x;
	FLAC__uint64 xx;
	unsigned i, blocksize_hint = 0, sample_rate_hint = 0;
	FLAC__byte crc8, raw_header[16]; /* MAGIC NUMBER based on the maximum frame header size, including CRC */
	unsigned raw_header_len;
	FLAC__bool is_unparseable = false;
	const FLAC__bool is_known_variable_blocksize_stream = (decoder->private_->has_stream_info && decoder->private_->stream_info.data.stream_info.min_blocksize != decoder->private_->stream_info.data.stream_info.max_blocksize);
	const FLAC__bool is_known_fixed_blocksize_stream = (decoder->private_->has_stream_info && decoder->private_->stream_info.data.stream_info.min_blocksize == decoder->private_->stream_info.data.stream_info.max_blocksize);

	FLAC__ASSERT(FLAC__bitbuffer_is_consumed_byte_aligned(decoder->private_->input));

	/* init the raw header with the saved bits from synchronization */
	raw_header[0] = decoder->private_->header_warmup[0];
	raw_header[1] = decoder->private_->header_warmup[1];
	raw_header_len = 2;

	/*
	 * check to make sure that the reserved bits are 0
	 */
	if(raw_header[1] & 0x03) { /* MAGIC NUMBER */
		is_unparseable = true;
	}

	/*
	 * Note that along the way as we read the header, we look for a sync
	 * code inside.  If we find one it would indicate that our original
	 * sync was bad since there cannot be a sync code in a valid header.
	 */

	/*
	 * read in the raw header as bytes so we can CRC it, and parse it on the way
	 */
	for(i = 0; i < 2; i++) {
		if(!FLAC__bitbuffer_read_raw_uint32(decoder->private_->input, &x, 8, read_callback_, decoder))
			return false; /* the read_callback_ sets the state for us */
		if(x == 0xff) { /* MAGIC NUMBER for the first 8 frame sync bits */
			/* if we get here it means our original sync was erroneous since the sync code cannot appear in the header */
			decoder->private_->lookahead = (FLAC__byte)x;
			decoder->private_->cached = true;
			decoder->private_->error_callback(decoder, FLAC__STREAM_DECODER_ERROR_STATUS_BAD_HEADER, decoder->private_->client_data);
			decoder->protected_->state = FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC;
			return true;
		}
		raw_header[raw_header_len++] = (FLAC__byte)x;
	}

	switch(x = raw_header[2] >> 4) {
		case 0:
			if(is_known_fixed_blocksize_stream)
				decoder->private_->frame.header.blocksize = decoder->private_->stream_info.data.stream_info.min_blocksize;
			else
				is_unparseable = true;
			break;
		case 1:
			decoder->private_->frame.header.blocksize = 192;
			break;
		case 2:
		case 3:
		case 4:
		case 5:
			decoder->private_->frame.header.blocksize = 576 << (x-2);
			break;
		case 6:
		case 7:
			blocksize_hint = x;
			break;
		case 8:
		case 9:
		case 10:
		case 11:
		case 12:
		case 13:
		case 14:
		case 15:
			decoder->private_->frame.header.blocksize = 256 << (x-8);
			break;
		default:
			FLAC__ASSERT(0);
			break;
	}

	switch(x = raw_header[2] & 0x0f) {
		case 0:
			if(decoder->private_->has_stream_info)
				decoder->private_->frame.header.sample_rate = decoder->private_->stream_info.data.stream_info.sample_rate;
			else
				is_unparseable = true;
			break;
		case 1:
		case 2:
		case 3:
			is_unparseable = true;
			break;
		case 4:
			decoder->private_->frame.header.sample_rate = 8000;
			break;
		case 5:
			decoder->private_->frame.header.sample_rate = 16000;
			break;
		case 6:
			decoder->private_->frame.header.sample_rate = 22050;
			break;
		case 7:
			decoder->private_->frame.header.sample_rate = 24000;
			break;
		case 8:
			decoder->private_->frame.header.sample_rate = 32000;
			break;
		case 9:
			decoder->private_->frame.header.sample_rate = 44100;
			break;
		case 10:
			decoder->private_->frame.header.sample_rate = 48000;
			break;
		case 11:
			decoder->private_->frame.header.sample_rate = 96000;
			break;
		case 12:
		case 13:
		case 14:
			sample_rate_hint = x;
			break;
		case 15:
			decoder->private_->error_callback(decoder, FLAC__STREAM_DECODER_ERROR_STATUS_BAD_HEADER, decoder->private_->client_data);
			decoder->protected_->state = FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC;
			return true;
		default:
			FLAC__ASSERT(0);
	}

	x = (unsigned)(raw_header[3] >> 4);
	if(x & 8) {
		decoder->private_->frame.header.channels = 2;
		switch(x & 7) {
			case 0:
				decoder->private_->frame.header.channel_assignment = FLAC__CHANNEL_ASSIGNMENT_LEFT_SIDE;
				break;
			case 1:
				decoder->private_->frame.header.channel_assignment = FLAC__CHANNEL_ASSIGNMENT_RIGHT_SIDE;
				break;
			case 2:
				decoder->private_->frame.header.channel_assignment = FLAC__CHANNEL_ASSIGNMENT_MID_SIDE;
				break;
			default:
				is_unparseable = true;
				break;
		}
	}
	else {
		decoder->private_->frame.header.channels = (unsigned)x + 1;
		decoder->private_->frame.header.channel_assignment = FLAC__CHANNEL_ASSIGNMENT_INDEPENDENT;
	}

	switch(x = (unsigned)(raw_header[3] & 0x0e) >> 1) {
		case 0:
			if(decoder->private_->has_stream_info)
				decoder->private_->frame.header.bits_per_sample = decoder->private_->stream_info.data.stream_info.bits_per_sample;
			else
				is_unparseable = true;
			break;
		case 1:
			decoder->private_->frame.header.bits_per_sample = 8;
			break;
		case 2:
			decoder->private_->frame.header.bits_per_sample = 12;
			break;
		case 4:
			decoder->private_->frame.header.bits_per_sample = 16;
			break;
		case 5:
			decoder->private_->frame.header.bits_per_sample = 20;
			break;
		case 6:
			decoder->private_->frame.header.bits_per_sample = 24;
			break;
		case 3:
		case 7:
			is_unparseable = true;
			break;
		default:
			FLAC__ASSERT(0);
			break;
	}

	if(raw_header[3] & 0x01) { /* this should be a zero padding bit */
		decoder->private_->error_callback(decoder, FLAC__STREAM_DECODER_ERROR_STATUS_BAD_HEADER, decoder->private_->client_data);
		decoder->protected_->state = FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC;
		return true;
	}

	if(blocksize_hint && is_known_variable_blocksize_stream) {
		if(!FLAC__bitbuffer_read_utf8_uint64(decoder->private_->input, &xx, read_callback_, decoder, raw_header, &raw_header_len))
			return false; /* the read_callback_ sets the state for us */
		if(xx == 0xffffffffffffffff) { /* i.e. non-UTF8 code... */
			decoder->private_->lookahead = raw_header[raw_header_len-1]; /* back up as much as we can */
			decoder->private_->cached = true;
			decoder->private_->error_callback(decoder, FLAC__STREAM_DECODER_ERROR_STATUS_BAD_HEADER, decoder->private_->client_data);
			decoder->protected_->state = FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC;
			return true;
		}
		decoder->private_->frame.header.number_type = FLAC__FRAME_NUMBER_TYPE_SAMPLE_NUMBER;
		decoder->private_->frame.header.number.sample_number = xx;
	}
	else {
		if(!FLAC__bitbuffer_read_utf8_uint32(decoder->private_->input, &x, read_callback_, decoder, raw_header, &raw_header_len))
			return false; /* the read_callback_ sets the state for us */
		if(x == 0xffffffff) { /* i.e. non-UTF8 code... */
			decoder->private_->lookahead = raw_header[raw_header_len-1]; /* back up as much as we can */
			decoder->private_->cached = true;
			decoder->private_->error_callback(decoder, FLAC__STREAM_DECODER_ERROR_STATUS_BAD_HEADER, decoder->private_->client_data);
			decoder->protected_->state = FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC;
			return true;
		}
		decoder->private_->last_frame_number = x;
		if(decoder->private_->has_stream_info) {
			decoder->private_->frame.header.number_type = FLAC__FRAME_NUMBER_TYPE_SAMPLE_NUMBER;
			decoder->private_->frame.header.number.sample_number = (FLAC__int64)decoder->private_->stream_info.data.stream_info.min_blocksize * (FLAC__int64)x;
		}
		else {
			is_unparseable = true;
		}
	}

	if(blocksize_hint) {
		if(!FLAC__bitbuffer_read_raw_uint32(decoder->private_->input, &x, 8, read_callback_, decoder))
			return false; /* the read_callback_ sets the state for us */
		raw_header[raw_header_len++] = (FLAC__byte)x;
		if(blocksize_hint == 7) {
			FLAC__uint32 _x;
			if(!FLAC__bitbuffer_read_raw_uint32(decoder->private_->input, &_x, 8, read_callback_, decoder))
				return false; /* the read_callback_ sets the state for us */
			raw_header[raw_header_len++] = (FLAC__byte)_x;
			x = (x << 8) | _x;
		}
		decoder->private_->frame.header.blocksize = x+1;
	}

	if(sample_rate_hint) {
		if(!FLAC__bitbuffer_read_raw_uint32(decoder->private_->input, &x, 8, read_callback_, decoder))
			return false; /* the read_callback_ sets the state for us */
		raw_header[raw_header_len++] = (FLAC__byte)x;
		if(sample_rate_hint != 12) {
			FLAC__uint32 _x;
			if(!FLAC__bitbuffer_read_raw_uint32(decoder->private_->input, &_x, 8, read_callback_, decoder))
				return false; /* the read_callback_ sets the state for us */
			raw_header[raw_header_len++] = (FLAC__byte)_x;
			x = (x << 8) | _x;
		}
		if(sample_rate_hint == 12)
			decoder->private_->frame.header.sample_rate = x*1000;
		else if(sample_rate_hint == 13)
			decoder->private_->frame.header.sample_rate = x;
		else
			decoder->private_->frame.header.sample_rate = x*10;
	}

	/* read the CRC-8 byte */
	if(!FLAC__bitbuffer_read_raw_uint32(decoder->private_->input, &x, 8, read_callback_, decoder))
		return false; /* the read_callback_ sets the state for us */
	crc8 = (FLAC__byte)x;

	if(FLAC__crc8(raw_header, raw_header_len) != crc8) {
		decoder->private_->error_callback(decoder, FLAC__STREAM_DECODER_ERROR_STATUS_BAD_HEADER, decoder->private_->client_data);
		decoder->protected_->state = FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC;
		return true;
	}

	if(is_unparseable) {
		decoder->protected_->state = FLAC__STREAM_DECODER_UNPARSEABLE_STREAM;
		return false;
	}

	return true;
}

FLAC__bool read_subframe_(FLAC__StreamDecoder *decoder, unsigned channel, unsigned bps)
{
	FLAC__uint32 x;
	FLAC__bool wasted_bits;

	if(!FLAC__bitbuffer_read_raw_uint32(decoder->private_->input, &x, 8, read_callback_, decoder)) /* MAGIC NUMBER */
		return false; /* the read_callback_ sets the state for us */

	wasted_bits = (x & 1);
	x &= 0xfe;

	if(wasted_bits) {
		unsigned u;
		if(!FLAC__bitbuffer_read_unary_unsigned(decoder->private_->input, &u, read_callback_, decoder))
			return false; /* the read_callback_ sets the state for us */
		decoder->private_->frame.subframes[channel].wasted_bits = u+1;
		bps -= decoder->private_->frame.subframes[channel].wasted_bits;
	}
	else
		decoder->private_->frame.subframes[channel].wasted_bits = 0;

	/*
	 * Lots of magic numbers here
	 */
	if(x & 0x80) {
		decoder->private_->error_callback(decoder, FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC, decoder->private_->client_data);
		decoder->protected_->state = FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC;
		return true;
	}
	else if(x == 0) {
		if(!read_subframe_constant_(decoder, channel, bps))
			return false;
	}
	else if(x == 2) {
		if(!read_subframe_verbatim_(decoder, channel, bps))
			return false;
	}
	else if(x < 16) {
		decoder->protected_->state = FLAC__STREAM_DECODER_UNPARSEABLE_STREAM;
		return false;
	}
	else if(x <= 24) {
		if(!read_subframe_fixed_(decoder, channel, bps, (x>>1)&7))
			return false;
	}
	else if(x < 64) {
		decoder->protected_->state = FLAC__STREAM_DECODER_UNPARSEABLE_STREAM;
		return false;
	}
	else {
		if(!read_subframe_lpc_(decoder, channel, bps, ((x>>1)&31)+1))
			return false;
	}

	if(wasted_bits) {
		unsigned i;
		x = decoder->private_->frame.subframes[channel].wasted_bits;
		for(i = 0; i < decoder->private_->frame.header.blocksize; i++)
			decoder->private_->output[channel][i] <<= x;
	}

	return true;
}

FLAC__bool read_subframe_constant_(FLAC__StreamDecoder *decoder, unsigned channel, unsigned bps)
{
	FLAC__Subframe_Constant *subframe = &decoder->private_->frame.subframes[channel].data.constant;
	FLAC__int32 x;
	unsigned i;
	FLAC__int32 *output = decoder->private_->output[channel];

	decoder->private_->frame.subframes[channel].type = FLAC__SUBFRAME_TYPE_CONSTANT;

	if(!FLAC__bitbuffer_read_raw_int32(decoder->private_->input, &x, bps, read_callback_, decoder))
		return false; /* the read_callback_ sets the state for us */

	subframe->value = x;

	/* decode the subframe */
	for(i = 0; i < decoder->private_->frame.header.blocksize; i++)
		output[i] = x;

	return true;
}

FLAC__bool read_subframe_fixed_(FLAC__StreamDecoder *decoder, unsigned channel, unsigned bps, const unsigned order)
{
	FLAC__Subframe_Fixed *subframe = &decoder->private_->frame.subframes[channel].data.fixed;
	FLAC__int32 i32;
	FLAC__uint32 u32;
	unsigned u;

	decoder->private_->frame.subframes[channel].type = FLAC__SUBFRAME_TYPE_FIXED;

	subframe->residual = decoder->private_->residual[channel];
	subframe->order = order;

	/* read warm-up samples */
	for(u = 0; u < order; u++) {
		if(!FLAC__bitbuffer_read_raw_int32(decoder->private_->input, &i32, bps, read_callback_, decoder))
			return false; /* the read_callback_ sets the state for us */
		subframe->warmup[u] = i32;
	}

	/* read entropy coding method info */
	if(!FLAC__bitbuffer_read_raw_uint32(decoder->private_->input, &u32, FLAC__ENTROPY_CODING_METHOD_TYPE_LEN, read_callback_, decoder))
		return false; /* the read_callback_ sets the state for us */
	subframe->entropy_coding_method.type = (FLAC__EntropyCodingMethodType)u32;
	switch(subframe->entropy_coding_method.type) {
		case FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE:
			if(!FLAC__bitbuffer_read_raw_uint32(decoder->private_->input, &u32, FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE_ORDER_LEN, read_callback_, decoder))
				return false; /* the read_callback_ sets the state for us */
			subframe->entropy_coding_method.data.partitioned_rice.order = u32;
			subframe->entropy_coding_method.data.partitioned_rice.contents = &decoder->private_->partitioned_rice_contents[channel];
			break;
		default:
			decoder->protected_->state = FLAC__STREAM_DECODER_UNPARSEABLE_STREAM;
			return false;
	}

	/* read residual */
	switch(subframe->entropy_coding_method.type) {
		case FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE:
			if(!read_residual_partitioned_rice_(decoder, order, subframe->entropy_coding_method.data.partitioned_rice.order, &decoder->private_->partitioned_rice_contents[channel], decoder->private_->residual[channel]))
				return false;
			break;
		default:
			FLAC__ASSERT(0);
	}

	/* decode the subframe */
	memcpy(decoder->private_->output[channel], subframe->warmup, sizeof(FLAC__int32) * order);
	FLAC__fixed_restore_signal(decoder->private_->residual[channel], decoder->private_->frame.header.blocksize-order, order, decoder->private_->output[channel]+order);

	return true;
}

FLAC__bool read_subframe_lpc_(FLAC__StreamDecoder *decoder, unsigned channel, unsigned bps, const unsigned order)
{
	FLAC__Subframe_LPC *subframe = &decoder->private_->frame.subframes[channel].data.lpc;
	FLAC__int32 i32;
	FLAC__uint32 u32;
	unsigned u;

	decoder->private_->frame.subframes[channel].type = FLAC__SUBFRAME_TYPE_LPC;

	subframe->residual = decoder->private_->residual[channel];
	subframe->order = order;

	/* read warm-up samples */
	for(u = 0; u < order; u++) {
		if(!FLAC__bitbuffer_read_raw_int32(decoder->private_->input, &i32, bps, read_callback_, decoder))
			return false; /* the read_callback_ sets the state for us */
		subframe->warmup[u] = i32;
	}

	/* read qlp coeff precision */
	if(!FLAC__bitbuffer_read_raw_uint32(decoder->private_->input, &u32, FLAC__SUBFRAME_LPC_QLP_COEFF_PRECISION_LEN, read_callback_, decoder))
		return false; /* the read_callback_ sets the state for us */
	if(u32 == (1u << FLAC__SUBFRAME_LPC_QLP_COEFF_PRECISION_LEN) - 1) {
		decoder->private_->error_callback(decoder, FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC, decoder->private_->client_data);
		decoder->protected_->state = FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC;
		return true;
	}
	subframe->qlp_coeff_precision = u32+1;

	/* read qlp shift */
	if(!FLAC__bitbuffer_read_raw_int32(decoder->private_->input, &i32, FLAC__SUBFRAME_LPC_QLP_SHIFT_LEN, read_callback_, decoder))
		return false; /* the read_callback_ sets the state for us */
	subframe->quantization_level = i32;

	/* read quantized lp coefficiencts */
	for(u = 0; u < order; u++) {
		if(!FLAC__bitbuffer_read_raw_int32(decoder->private_->input, &i32, subframe->qlp_coeff_precision, read_callback_, decoder))
			return false; /* the read_callback_ sets the state for us */
		subframe->qlp_coeff[u] = i32;
	}

	/* read entropy coding method info */
	if(!FLAC__bitbuffer_read_raw_uint32(decoder->private_->input, &u32, FLAC__ENTROPY_CODING_METHOD_TYPE_LEN, read_callback_, decoder))
		return false; /* the read_callback_ sets the state for us */
	subframe->entropy_coding_method.type = (FLAC__EntropyCodingMethodType)u32;
	switch(subframe->entropy_coding_method.type) {
		case FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE:
			if(!FLAC__bitbuffer_read_raw_uint32(decoder->private_->input, &u32, FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE_ORDER_LEN, read_callback_, decoder))
				return false; /* the read_callback_ sets the state for us */
			subframe->entropy_coding_method.data.partitioned_rice.order = u32;
			subframe->entropy_coding_method.data.partitioned_rice.contents = &decoder->private_->partitioned_rice_contents[channel];
			break;
		default:
			decoder->protected_->state = FLAC__STREAM_DECODER_UNPARSEABLE_STREAM;
			return false;
	}

	/* read residual */
	switch(subframe->entropy_coding_method.type) {
		case FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE:
			if(!read_residual_partitioned_rice_(decoder, order, subframe->entropy_coding_method.data.partitioned_rice.order, &decoder->private_->partitioned_rice_contents[channel], decoder->private_->residual[channel]))
				return false;
			break;
		default:
			FLAC__ASSERT(0);
	}

	/* decode the subframe */
	memcpy(decoder->private_->output[channel], subframe->warmup, sizeof(FLAC__int32) * order);
	if(bps + subframe->qlp_coeff_precision + FLAC__bitmath_ilog2(order) <= 32)
		if(bps <= 16 && subframe->qlp_coeff_precision <= 16)
			decoder->private_->local_lpc_restore_signal_16bit(decoder->private_->residual[channel], decoder->private_->frame.header.blocksize-order, subframe->qlp_coeff, order, subframe->quantization_level, decoder->private_->output[channel]+order);
		else
			decoder->private_->local_lpc_restore_signal(decoder->private_->residual[channel], decoder->private_->frame.header.blocksize-order, subframe->qlp_coeff, order, subframe->quantization_level, decoder->private_->output[channel]+order);
	else
		decoder->private_->local_lpc_restore_signal_64bit(decoder->private_->residual[channel], decoder->private_->frame.header.blocksize-order, subframe->qlp_coeff, order, subframe->quantization_level, decoder->private_->output[channel]+order);

	return true;
}

FLAC__bool read_subframe_verbatim_(FLAC__StreamDecoder *decoder, unsigned channel, unsigned bps)
{
	FLAC__Subframe_Verbatim *subframe = &decoder->private_->frame.subframes[channel].data.verbatim;
	FLAC__int32 x, *residual = decoder->private_->residual[channel];
	unsigned i;

	decoder->private_->frame.subframes[channel].type = FLAC__SUBFRAME_TYPE_VERBATIM;

	subframe->data = residual;

	for(i = 0; i < decoder->private_->frame.header.blocksize; i++) {
		if(!FLAC__bitbuffer_read_raw_int32(decoder->private_->input, &x, bps, read_callback_, decoder))
			return false; /* the read_callback_ sets the state for us */
		residual[i] = x;
	}

	/* decode the subframe */
	memcpy(decoder->private_->output[channel], subframe->data, sizeof(FLAC__int32) * decoder->private_->frame.header.blocksize);

	return true;
}

FLAC__bool read_residual_partitioned_rice_(FLAC__StreamDecoder *decoder, unsigned predictor_order, unsigned partition_order, FLAC__EntropyCodingMethod_PartitionedRiceContents *partitioned_rice_contents, FLAC__int32 *residual)
{
	FLAC__uint32 rice_parameter;
	int i;
	unsigned partition, sample, u;
	const unsigned partitions = 1u << partition_order;
	const unsigned partition_samples = partition_order > 0? decoder->private_->frame.header.blocksize >> partition_order : decoder->private_->frame.header.blocksize - predictor_order;

	if(!FLAC__format_entropy_coding_method_partitioned_rice_contents_ensure_size(partitioned_rice_contents, max(6, partition_order))) {
		decoder->protected_->state = FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR;
		return false;
	}

	sample = 0;
	for(partition = 0; partition < partitions; partition++) {
		if(!FLAC__bitbuffer_read_raw_uint32(decoder->private_->input, &rice_parameter, FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE_PARAMETER_LEN, read_callback_, decoder))
			return false; /* the read_callback_ sets the state for us */
		partitioned_rice_contents->parameters[partition] = rice_parameter;
		if(rice_parameter < FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE_ESCAPE_PARAMETER) {
#ifdef FLAC__SYMMETRIC_RICE
			for(u = (partition_order == 0 || partition > 0)? 0 : predictor_order; u < partition_samples; u++, sample++) {
				if(!FLAC__bitbuffer_read_symmetric_rice_signed(decoder->private_->input, &i, rice_parameter, read_callback_, decoder))
					return false; /* the read_callback_ sets the state for us */
				residual[sample] = i;
			}
#else
			u = (partition_order == 0 || partition > 0)? partition_samples : partition_samples - predictor_order;
			if(!FLAC__bitbuffer_read_rice_signed_block(decoder->private_->input, residual + sample, u, rice_parameter, read_callback_, decoder))
				return false; /* the read_callback_ sets the state for us */
			sample += u;
#endif
		}
		else {
			if(!FLAC__bitbuffer_read_raw_uint32(decoder->private_->input, &rice_parameter, FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE_RAW_LEN, read_callback_, decoder))
				return false; /* the read_callback_ sets the state for us */
			partitioned_rice_contents->raw_bits[partition] = rice_parameter;
			for(u = (partition_order == 0 || partition > 0)? 0 : predictor_order; u < partition_samples; u++, sample++) {
				if(!FLAC__bitbuffer_read_raw_int32(decoder->private_->input, &i, rice_parameter, read_callback_, decoder))
					return false; /* the read_callback_ sets the state for us */
				residual[sample] = i;
			}
		}
	}

	return true;
}

FLAC__bool read_zero_padding_(FLAC__StreamDecoder *decoder)
{
	if(!FLAC__bitbuffer_is_consumed_byte_aligned(decoder->private_->input)) {
		FLAC__uint32 zero = 0;
		if(!FLAC__bitbuffer_read_raw_uint32(decoder->private_->input, &zero, FLAC__bitbuffer_bits_left_for_byte_alignment(decoder->private_->input), read_callback_, decoder))
			return false; /* the read_callback_ sets the state for us */
		if(zero != 0) {
			decoder->private_->error_callback(decoder, FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC, decoder->private_->client_data);
			decoder->protected_->state = FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC;
		}
	}
	return true;
}

FLAC__bool read_callback_(FLAC__byte buffer[], unsigned *bytes, void *client_data)
{
	FLAC__StreamDecoder *decoder = (FLAC__StreamDecoder *)client_data;
	FLAC__StreamDecoderReadStatus status;

	status = decoder->private_->read_callback(decoder, buffer, bytes, decoder->private_->client_data);
	if(status == FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM)
		decoder->protected_->state = FLAC__STREAM_DECODER_END_OF_STREAM;
	else if(status == FLAC__STREAM_DECODER_READ_STATUS_ABORT)
		decoder->protected_->state = FLAC__STREAM_DECODER_ABORTED;
	return status == FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}
