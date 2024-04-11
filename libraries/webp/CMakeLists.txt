cmake_minimum_required(VERSION 3.11)
project(webp)

set(WEBP_SOURCES
	src/dec/alphai_dec.h
	src/dec/alpha_dec.c
	src/dec/buffer_dec.c
	src/dec/common_dec.h
	src/dec/frame_dec.c
	src/dec/idec_dec.c
	src/dec/io_dec.c
	src/dec/quant_dec.c
	src/dec/tree_dec.c
	src/dec/vp8i_dec.h
	src/dec/vp8li_dec.h
	src/dec/vp8l_dec.c
	src/dec/vp8_dec.c
	src/dec/vp8_dec.h
	src/dec/webpi_dec.h
	src/dec/webp_dec.c
	src/demux/anim_decode.c
	src/demux/demux.c
	src/dsp/alpha_processing.c
	src/dsp/alpha_processing_mips_dsp_r2.c
	src/dsp/alpha_processing_neon.c
	src/dsp/alpha_processing_sse2.c
	src/dsp/alpha_processing_sse41.c
	src/dsp/common_sse2.h
	src/dsp/common_sse41.h
	src/dsp/cost.c
	src/dsp/cost_mips32.c
	src/dsp/cost_mips_dsp_r2.c
	src/dsp/cost_neon.c
	src/dsp/cost_sse2.c
	src/dsp/cpu.c
	src/dsp/cpu.h
	src/dsp/dec.c
	src/dsp/dec_clip_tables.c
	src/dsp/dec_mips32.c
	src/dsp/dec_mips_dsp_r2.c
	src/dsp/dec_msa.c
	src/dsp/dec_neon.c
	src/dsp/dec_sse2.c
	src/dsp/dec_sse41.c
	src/dsp/dsp.h
	src/dsp/enc.c
	src/dsp/enc_mips32.c
	src/dsp/enc_mips_dsp_r2.c
	src/dsp/enc_msa.c
	src/dsp/enc_neon.c
	src/dsp/enc_sse2.c
	src/dsp/enc_sse41.c
	src/dsp/filters.c
	src/dsp/filters_mips_dsp_r2.c
	src/dsp/filters_msa.c
	src/dsp/filters_neon.c
	src/dsp/filters_sse2.c
	src/dsp/lossless.c
	src/dsp/lossless.h
	src/dsp/lossless_common.h
	src/dsp/lossless_enc.c
	src/dsp/lossless_enc_mips32.c
	src/dsp/lossless_enc_mips_dsp_r2.c
	src/dsp/lossless_enc_msa.c
	src/dsp/lossless_enc_neon.c
	src/dsp/lossless_enc_sse2.c
	src/dsp/lossless_enc_sse41.c
	src/dsp/lossless_mips_dsp_r2.c
	src/dsp/lossless_msa.c
	src/dsp/lossless_neon.c
	src/dsp/lossless_sse2.c
	src/dsp/lossless_sse41.c
	src/dsp/mips_macro.h
	src/dsp/msa_macro.h
	src/dsp/neon.h
	src/dsp/quant.h
	src/dsp/rescaler.c
	src/dsp/rescaler_mips32.c
	src/dsp/rescaler_mips_dsp_r2.c
	src/dsp/rescaler_msa.c
	src/dsp/rescaler_neon.c
	src/dsp/rescaler_sse2.c
	src/dsp/ssim.c
	src/dsp/ssim_sse2.c
	src/dsp/upsampling.c
	src/dsp/upsampling_mips_dsp_r2.c
	src/dsp/upsampling_msa.c
	src/dsp/upsampling_neon.c
	src/dsp/upsampling_sse2.c
	src/dsp/upsampling_sse41.c
	src/dsp/yuv.c
	src/dsp/yuv.h
	src/dsp/yuv_mips32.c
	src/dsp/yuv_mips_dsp_r2.c
	src/dsp/yuv_neon.c
	src/dsp/yuv_sse2.c
	src/dsp/yuv_sse41.c
	src/enc/alpha_enc.c
	src/enc/analysis_enc.c
	src/enc/backward_references_cost_enc.c
	src/enc/backward_references_enc.c
	src/enc/backward_references_enc.h
	src/enc/config_enc.c
	src/enc/cost_enc.c
	src/enc/cost_enc.h
	src/enc/filter_enc.c
	src/enc/frame_enc.c
	src/enc/histogram_enc.c
	src/enc/histogram_enc.h
	src/enc/iterator_enc.c
	src/enc/near_lossless_enc.c
	src/enc/picture_csp_enc.c
	src/enc/picture_enc.c
	src/enc/picture_psnr_enc.c
	src/enc/picture_rescale_enc.c
	src/enc/picture_tools_enc.c
	src/enc/predictor_enc.c
	src/enc/quant_enc.c
	src/enc/syntax_enc.c
	src/enc/token_enc.c
	src/enc/tree_enc.c
	src/enc/vp8i_enc.h
	src/enc/vp8li_enc.h
	src/enc/vp8l_enc.c
	src/enc/webp_enc.c
	src/mux/animi.h
	src/mux/anim_encode.c
	src/mux/muxedit.c
	src/mux/muxi.h
	src/mux/muxinternal.c
	src/mux/muxread.c
	src/utils/bit_reader_inl_utils.h
	src/utils/bit_reader_utils.c
	src/utils/bit_reader_utils.h
	src/utils/bit_writer_utils.c
	src/utils/bit_writer_utils.h
	src/utils/color_cache_utils.c
	src/utils/color_cache_utils.h
	src/utils/endian_inl_utils.h
	src/utils/filters_utils.c
	src/utils/filters_utils.h
	src/utils/huffman_encode_utils.c
	src/utils/huffman_encode_utils.h
	src/utils/huffman_utils.c
	src/utils/huffman_utils.h
	src/utils/palette.c
	src/utils/palette.h
	src/utils/quant_levels_dec_utils.c
	src/utils/quant_levels_dec_utils.h
	src/utils/quant_levels_utils.c
	src/utils/quant_levels_utils.h
	src/utils/random_utils.c
	src/utils/random_utils.h
	src/utils/rescaler_utils.c
	src/utils/rescaler_utils.h
	src/utils/thread_utils.c
	src/utils/thread_utils.h
	src/utils/utils.c
	src/utils/utils.h
	sharpyuv/sharpyuv.c
	sharpyuv/sharpyuv.h
	sharpyuv/sharpyuv_cpu.c
	sharpyuv/sharpyuv_cpu.h
	sharpyuv/sharpyuv_csp.c
	sharpyuv/sharpyuv_csp.h
	sharpyuv/sharpyuv_dsp.c
	sharpyuv/sharpyuv_dsp.h
	sharpyuv/sharpyuv_gamma.c
	sharpyuv/sharpyuv_gamma.h
	sharpyuv/sharpyuv_neon.c
	sharpyuv/sharpyuv_sse2.c
)

set(WEBP_INCLUDES
	include/webp/decode.h
	include/webp/demux.h
	include/webp/encode.h
	include/webp/format_constants.h
	include/webp/mux.h
	include/webp/mux_types.h
	include/webp/types.h
)

source_group("src" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/.+")
source_group("src\\dec" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/dec/.+")
source_group("src\\demux" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/demux/.+")
source_group("src\\dsp" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/.+")
source_group("src\\enc" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/enc/.+")
source_group("src\\mux" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/mux/.+")
source_group("src\\utils" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/utils/.+")
source_group("sharpyuv" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/sharpyuv/.+")
source_group("include" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/include/webp/.+")
source_group("include\\webp" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/include/webp/core/.+")

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

if(MSVC)
	# Use all cores for compilation
	set(CMAKE_CXX_FLAGS "/MP ${CMAKE_CXX_FLAGS}")

	# Ignore warnings in third party code
	#set_source_files_properties(${WEBP_SOURCES} PROPERTIES COMPILE_FLAGS "/wd4244 /wd4267 /wd4005 /wd4018 -D_CRT_SECURE_NO_WARNINGS")
endif()

add_library(webp STATIC ${WEBP_SOURCES} ${WEBP_INCLUDES})
target_link_libraries(webp ${WEBP_LIBS})
set_target_properties(webp PROPERTIES CXX_STANDARD 17)

if(MSVC)
	set_property(TARGET webp PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
