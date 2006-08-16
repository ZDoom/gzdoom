/*
* jdpostct.c
*
* Copyright (C) 1994-1996, Thomas G. Lane.
* This file is part of the Independent JPEG Group's software.
* For conditions of distribution and use, see the accompanying README file.
*
* This file contains the decompression postprocessing controller.
* This controller manages the upsampling, color conversion, and color
* quantization/reduction steps; specifically, it controls the buffering
* between upsample/color conversion and color quantization/reduction.
*
* If no color quantization/reduction is required, then this module has no
* work to do, and it just hands off to the upsample/color conversion code.
* An integrated upsample/convert/quantize process would replace this module
* entirely.
*/

#define JPEG_INTERNALS
#include "jinclude.h"
#include "jpeglib.h"


/* Private buffer controller object */

typedef struct {
	struct jpeg_d_post_controller pub; /* public fields */
} my_post_controller;

typedef my_post_controller * my_post_ptr;


/*
* Initialize for a processing pass.
*/

METHODDEF(void)
start_pass_dpost (j_decompress_ptr cinfo, J_BUF_MODE pass_mode)
{
	my_post_ptr post = (my_post_ptr) cinfo->post;

	switch (pass_mode) {
	case JBUF_PASS_THRU:
		/* For single-pass processing without color quantization,
		* I have no work to do; just call the upsampler directly.
		*/
		post->pub.post_process_data = cinfo->upsample->upsample;
		break;
	default:
		ERREXIT(cinfo, JERR_BAD_BUFFER_MODE);
		break;
	}
}


/*
* Initialize postprocessing controller.
*/

GLOBAL(void)
jinit_d_post_controller (j_decompress_ptr cinfo, boolean need_full_buffer)
{
	my_post_ptr post;

	post = (my_post_ptr)
		(*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
		SIZEOF(my_post_controller));
	cinfo->post = (struct jpeg_d_post_controller *) post;
	post->pub.start_pass = start_pass_dpost;
}
