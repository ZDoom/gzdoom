/* libFLAC++ - Free Lossless Audio Codec library
 * Copyright (C) 2002,2003  Josh Coalson
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

#ifndef FLACPP__DECODER_H
#define FLACPP__DECODER_H

#include "export.h"

//#include "FLAC/file_decoder.h"
//#include "FLAC/seekable_stream_decoder.h"
#include "FLAC/stream_decoder.h"


/** \file include/FLAC++/decoder.h
 *
 *  \brief
 *  This module contains the classes which implement the various
 *  decoders.
 *
 *  See the detailed documentation in the
 *  \link flacpp_decoder decoder \endlink module.
 */

/** \defgroup flacpp_decoder FLAC++/decoder.h: decoder classes
 *  \ingroup flacpp
 *
 *  \brief
 *  This module describes the three decoder layers provided by libFLAC++.
 *
 * The libFLAC++ decoder classes are object wrappers around their
 * counterparts in libFLAC.  All three decoding layers available in
 * libFLAC are also provided here.  The interface is very similar;
 * make sure to read the \link flac_decoder libFLAC decoder module \endlink.
 *
 * The only real difference here is that instead of passing in C function
 * pointers for callbacks, you inherit from the decoder class and provide
 * implementations for the callbacks in the derived class; because of this
 * there is no need for a 'client_data' property.
 */

namespace FLAC {
	namespace Decoder {

		// ============================================================
		//
		//  Equivalent: FLAC__StreamDecoder
		//
		// ============================================================

		/** \defgroup flacpp_stream_decoder FLAC++/decoder.h: stream decoder class
		 *  \ingroup flacpp_decoder
		 *
		 *  \brief
		 *  This class wraps the ::FLAC__StreamDecoder.
		 *
		 * See the \link flac_stream_decoder libFLAC stream decoder module \endlink.
		 *
		 * \{
		 */

		/** This class wraps the ::FLAC__StreamDecoder.
		 */
		class FLACPP_API Stream {
		public:
			class FLACPP_API State {
			public:
				inline State(::FLAC__StreamDecoderState state): state_(state) { }
				inline operator ::FLAC__StreamDecoderState() const { return state_; }
				inline const char *as_cstring() const { return ::FLAC__StreamDecoderStateString[state_]; }
				const char *resolved_as_cstring(const Stream &) const;
			protected:
				::FLAC__StreamDecoderState state_;
			};

			Stream();
			virtual ~Stream();

			bool is_valid() const;
			inline operator bool() const { return is_valid(); }

			bool set_metadata_respond(::FLAC__MetadataType type);
			bool set_metadata_respond_application(const FLAC__byte id[4]);
			bool set_metadata_respond_all();
			bool set_metadata_ignore(::FLAC__MetadataType type);
			bool set_metadata_ignore_application(const FLAC__byte id[4]);
			bool set_metadata_ignore_all();

			State get_state() const;
			unsigned get_channels() const;
			::FLAC__ChannelAssignment get_channel_assignment() const;
			unsigned get_bits_per_sample() const;
			unsigned get_sample_rate() const;
			unsigned get_blocksize() const;

			/** Initialize the instance; as with the C interface,
			 *  init() should be called after construction and 'set'
			 *  calls but before any of the 'process' calls.
			 */
			State init();

			void finish();

			bool flush();
			bool reset();

			bool process_single();
			bool process_until_end_of_metadata();
			bool process_until_end_of_stream();
		protected:
			virtual ::FLAC__StreamDecoderReadStatus read_callback(FLAC__byte buffer[], unsigned *bytes) = 0;
			virtual ::FLAC__StreamDecoderWriteStatus write_callback(const ::FLAC__Frame *frame, const FLAC__int32 * const buffer[]) = 0;
			virtual void metadata_callback(const ::FLAC__StreamMetadata *metadata) = 0;
			virtual void error_callback(::FLAC__StreamDecoderErrorStatus status) = 0;

			::FLAC__StreamDecoder *decoder_;
		private:
			static ::FLAC__StreamDecoderReadStatus read_callback_(const ::FLAC__StreamDecoder *decoder, FLAC__byte buffer[], unsigned *bytes, void *client_data);
			static ::FLAC__StreamDecoderWriteStatus write_callback_(const ::FLAC__StreamDecoder *decoder, const ::FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data);
			static void metadata_callback_(const ::FLAC__StreamDecoder *decoder, const ::FLAC__StreamMetadata *metadata, void *client_data);
			static void error_callback_(const ::FLAC__StreamDecoder *decoder, ::FLAC__StreamDecoderErrorStatus status, void *client_data);

			// Private and undefined so you can't use them:
			Stream(const Stream &);
			void operator=(const Stream &);
		};

		/* \} */

#if 0	// [RH] Don't need these for ZDoom

		// ============================================================
		//
		//  Equivalent: FLAC__SeekableStreamDecoder
		//
		// ============================================================

		/** \defgroup flacpp_seekable_stream_decoder FLAC++/decoder.h: seekable stream decoder class
		 *  \ingroup flacpp_decoder
		 *
		 *  \brief
		 *  This class wraps the ::FLAC__SeekableStreamDecoder.
		 *
		 * See the \link flac_seekable_stream_decoder libFLAC seekable stream decoder module \endlink.
		 *
		 * \{
		 */

		/** This class wraps the ::FLAC__SeekableStreamDecoder.
		 */
		class FLACPP_API SeekableStream {
		public:
			class FLACPP_API State {
			public:
				inline State(::FLAC__SeekableStreamDecoderState state): state_(state) { }
				inline operator ::FLAC__SeekableStreamDecoderState() const { return state_; }
				inline const char *as_cstring() const { return ::FLAC__SeekableStreamDecoderStateString[state_]; }
				const char *resolved_as_cstring(const SeekableStream &) const;
			protected:
				::FLAC__SeekableStreamDecoderState state_;
			};

			SeekableStream();
			virtual ~SeekableStream();

			bool is_valid() const;
			inline operator bool() const { return is_valid(); }

			bool set_md5_checking(bool value);
			bool set_metadata_respond(::FLAC__MetadataType type);
			bool set_metadata_respond_application(const FLAC__byte id[4]);
			bool set_metadata_respond_all();
			bool set_metadata_ignore(::FLAC__MetadataType type);
			bool set_metadata_ignore_application(const FLAC__byte id[4]);
			bool set_metadata_ignore_all();

			State get_state() const;
			Stream::State get_stream_decoder_state() const;
			bool get_md5_checking() const;
			unsigned get_channels() const;
			::FLAC__ChannelAssignment get_channel_assignment() const;
			unsigned get_bits_per_sample() const;
			unsigned get_sample_rate() const;
			unsigned get_blocksize() const;

			State init();

			bool finish();

			bool flush();
			bool reset();

			bool process_single();
			bool process_until_end_of_metadata();
			bool process_until_end_of_stream();

			bool seek_absolute(FLAC__uint64 sample);
		protected:
			virtual ::FLAC__SeekableStreamDecoderReadStatus read_callback(FLAC__byte buffer[], unsigned *bytes) = 0;
			virtual ::FLAC__SeekableStreamDecoderSeekStatus seek_callback(FLAC__uint64 absolute_byte_offset) = 0;
			virtual ::FLAC__SeekableStreamDecoderTellStatus tell_callback(FLAC__uint64 *absolute_byte_offset) = 0;
			virtual ::FLAC__SeekableStreamDecoderLengthStatus length_callback(FLAC__uint64 *stream_length) = 0;
			virtual bool eof_callback() = 0;
			virtual ::FLAC__StreamDecoderWriteStatus write_callback(const ::FLAC__Frame *frame, const FLAC__int32 * const buffer[]) = 0;
			virtual void metadata_callback(const ::FLAC__StreamMetadata *metadata) = 0;
			virtual void error_callback(::FLAC__StreamDecoderErrorStatus status) = 0;

			::FLAC__SeekableStreamDecoder *decoder_;
		private:
			static ::FLAC__SeekableStreamDecoderReadStatus read_callback_(const ::FLAC__SeekableStreamDecoder *decoder, FLAC__byte buffer[], unsigned *bytes, void *client_data);
			static ::FLAC__SeekableStreamDecoderSeekStatus seek_callback_(const ::FLAC__SeekableStreamDecoder *decoder, FLAC__uint64 absolute_byte_offset, void *client_data);
			static ::FLAC__SeekableStreamDecoderTellStatus tell_callback_(const ::FLAC__SeekableStreamDecoder *decoder, FLAC__uint64 *absolute_byte_offset, void *client_data);
			static ::FLAC__SeekableStreamDecoderLengthStatus length_callback_(const ::FLAC__SeekableStreamDecoder *decoder, FLAC__uint64 *stream_length, void *client_data);
			static FLAC__bool eof_callback_(const ::FLAC__SeekableStreamDecoder *decoder, void *client_data);
			static ::FLAC__StreamDecoderWriteStatus write_callback_(const ::FLAC__SeekableStreamDecoder *decoder, const ::FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data);
			static void metadata_callback_(const ::FLAC__SeekableStreamDecoder *decoder, const ::FLAC__StreamMetadata *metadata, void *client_data);
			static void error_callback_(const ::FLAC__SeekableStreamDecoder *decoder, ::FLAC__StreamDecoderErrorStatus status, void *client_data);

			// Private and undefined so you can't use them:
			SeekableStream(const SeekableStream &);
			void operator=(const SeekableStream &);
		};

		/* \} */


		// ============================================================
		//
		//  Equivalent: FLAC__FileDecoder
		//
		// ============================================================

		/** \defgroup flacpp_file_decoder FLAC++/decoder.h: file decoder class
		 *  \ingroup flacpp_decoder
		 *
		 *  \brief
		 *  This class wraps the ::FLAC__FileDecoder.
		 *
		 * See the \link flac_file_decoder libFLAC file decoder module \endlink.
		 *
		 * \{
		 */

		/** This class wraps the ::FLAC__FileDecoder.
		 */
		class FLACPP_API File {
		public:
			class FLACPP_API State {
			public:
				inline State(::FLAC__FileDecoderState state): state_(state) { }
				inline operator ::FLAC__FileDecoderState() const { return state_; }
				inline const char *as_cstring() const { return ::FLAC__FileDecoderStateString[state_]; }
				const char *resolved_as_cstring(const File &) const;
			protected:
				::FLAC__FileDecoderState state_;
			};

			File();
			virtual ~File();

			bool is_valid() const;
			inline operator bool() const { return is_valid(); }

			bool set_md5_checking(bool value);
			bool set_filename(const char *value); //!< 'value' may not be \c NULL; use "-" for stdin
			bool set_metadata_respond(::FLAC__MetadataType type);
			bool set_metadata_respond_application(const FLAC__byte id[4]);
			bool set_metadata_respond_all();
			bool set_metadata_ignore(::FLAC__MetadataType type);
			bool set_metadata_ignore_application(const FLAC__byte id[4]);
			bool set_metadata_ignore_all();

			State get_state() const;
			SeekableStream::State get_seekable_stream_decoder_state() const;
			Stream::State get_stream_decoder_state() const;
			bool get_md5_checking() const;
			unsigned get_channels() const;
			::FLAC__ChannelAssignment get_channel_assignment() const;
			unsigned get_bits_per_sample() const;
			unsigned get_sample_rate() const;
			unsigned get_blocksize() const;

			State init();

			bool finish();

			bool process_single();
			bool process_until_end_of_metadata();
			bool process_until_end_of_file();

			bool seek_absolute(FLAC__uint64 sample);
		protected:
			virtual ::FLAC__StreamDecoderWriteStatus write_callback(const ::FLAC__Frame *frame, const FLAC__int32 * const buffer[]) = 0;
			virtual void metadata_callback(const ::FLAC__StreamMetadata *metadata) = 0;
			virtual void error_callback(::FLAC__StreamDecoderErrorStatus status) = 0;

			::FLAC__FileDecoder *decoder_;
		private:
			static ::FLAC__StreamDecoderWriteStatus write_callback_(const ::FLAC__FileDecoder *decoder, const ::FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data);
			static void metadata_callback_(const ::FLAC__FileDecoder *decoder, const ::FLAC__StreamMetadata *metadata, void *client_data);
			static void error_callback_(const ::FLAC__FileDecoder *decoder, ::FLAC__StreamDecoderErrorStatus status, void *client_data);

			// Private and undefined so you can't use them:
			File(const File &);
			void operator=(const File &);
		};

		/* \} */
#endif
	};
};

#endif
