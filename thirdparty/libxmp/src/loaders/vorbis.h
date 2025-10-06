#ifndef DEBUG  /* not a debug build */
#ifndef NDEBUG
#define NDEBUG /* disable assert()s */
#endif
#endif

#define STB_VORBIS_NO_PUSHDATA_API
#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_COMMENTS
#define STB_VORBIS_NO_SEEK_API
#define STB_VORBIS_NO_FLOAT_CONVERSION

/* change namespace from stb_ to libxmp_ for public functions: */
#define stb_vorbis_get_info libxmp_vorbis_get_info
#define stb_vorbis_get_comment libxmp_vorbis_get_comment
#define stb_vorbis_get_error libxmp_vorbis_get_error
#define stb_vorbis_close libxmp_vorbis_close
#define stb_vorbis_get_sample_offset libxmp_vorbis_get_sample_offset
#define stb_vorbis_get_file_offset libxmp_vorbis_get_file_offset
#define stb_vorbis_open_pushdata libxmp_vorbis_open_pushdata
#define stb_vorbis_decode_frame_pushdata libxmp_vorbis_decode_frame_pushdata
#define stb_vorbis_flush_pushdata libxmp_vorbis_flush_pushdata
#define stb_vorbis_decode_filename libxmp_vorbis_decode_filename
#define stb_vorbis_decode_memory libxmp_vorbis_decode_memory
#define stb_vorbis_open_memory libxmp_vorbis_open_memory
#define stb_vorbis_open_filename libxmp_vorbis_open_filename
#define stb_vorbis_open_file libxmp_vorbis_open_file
#define stb_vorbis_open_file_section libxmp_vorbis_open_file_section
#define stb_vorbis_seek_frame libxmp_vorbis_seek_frame
#define stb_vorbis_seek libxmp_vorbis_seek
#define stb_vorbis_seek_start libxmp_vorbis_seek_start
#define stb_vorbis_stream_length_in_samples libxmp_vorbis_stream_length_in_samples
#define stb_vorbis_stream_length_in_seconds libxmp_vorbis_stream_length_in_seconds
#define stb_vorbis_get_frame_float libxmp_vorbis_get_frame_float
#define stb_vorbis_get_frame_short_interleaved libxmp_vorbis_get_frame_short_interleaved
#define stb_vorbis_get_frame_short libxmp_vorbis_get_frame_short
#define stb_vorbis_get_samples_float_interleaved libxmp_vorbis_get_samples_float_interleaved
#define stb_vorbis_get_samples_float libxmp_vorbis_get_samples_float
#define stb_vorbis_get_samples_short_interleaved libxmp_vorbis_get_samples_short_interleaved
#define stb_vorbis_get_samples_short libxmp_vorbis_get_samples_short

#ifndef STB_VORBIS_C
/* client: */
#define STB_VORBIS_HEADER_ONLY
#include "vorbis.c"

#else

#ifdef _MSC_VER
#pragma warning(disable:4456) /* shadowing (hides previous local decl) */
#pragma warning(disable:4457) /* shadowing (hides function parameter.) */
#endif

#endif
