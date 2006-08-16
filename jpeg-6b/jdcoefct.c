/*
* jdcoefct.c
*
* Copyright (C) 1994-1997, Thomas G. Lane.
* This file is part of the Independent JPEG Group's software.
* For conditions of distribution and use, see the accompanying README file.
*
* This file contains the coefficient buffer controller for decompression.
* This controller is the top level of the JPEG decompressor proper.
* The coefficient buffer lies between entropy decoding and inverse-DCT steps.
*
* In buffered-image mode, this controller is the interface between
* input-oriented processing and output-oriented processing.
* Also, the input side (only) is used when reading a file for transcoding.
*/

#define JPEG_INTERNALS
#include "jinclude.h"
#include "jpeglib.h"

/* Private buffer controller object */

typedef struct {
	struct jpeg_d_coef_controller pub; /* public fields */

	/* These variables keep track of the current location of the input side. */
	/* cinfo->input_iMCU_row is also used for this. */
	JDIMENSION MCU_ctr;		/* counts MCUs processed in current row */
	int MCU_vert_offset;		/* counts MCU rows within iMCU row */
	int MCU_rows_per_iMCU_row;	/* number of such rows needed */

	/* The output side's location is represented by cinfo->output_iMCU_row. */

	/* In single-pass modes, it's sufficient to buffer just one MCU.
	* We allocate a workspace of D_MAX_BLOCKS_IN_MCU coefficient blocks,
	* and let the entropy decoder write into that workspace each time.
	* (On 80x86, the workspace is FAR even though it's not really very big;
	* this is to keep the module interfaces unchanged when a large coefficient
	* buffer is necessary.)
	* In multi-pass modes, this array points to the current MCU's blocks
	* within the virtual arrays; it is used only by the input side.
	*/
	JBLOCKROW MCU_buffer[D_MAX_BLOCKS_IN_MCU];

#ifdef D_MULTISCAN_FILES_SUPPORTED
	/* In multi-pass modes, we need a virtual block array for each component. */
	jvirt_barray_ptr whole_image[MAX_COMPONENTS];
#endif
} my_coef_controller;

typedef my_coef_controller * my_coef_ptr;

/* Forward declarations */
METHODDEF(int) decompress_onepass
JPP((j_decompress_ptr cinfo, JSAMPIMAGE output_buf));
#ifdef D_MULTISCAN_FILES_SUPPORTED
METHODDEF(int) decompress_data
JPP((j_decompress_ptr cinfo, JSAMPIMAGE output_buf));
#endif


LOCAL(void)
start_iMCU_row (j_decompress_ptr cinfo)
/* Reset within-iMCU-row counters for a new row (input side) */
{
	my_coef_ptr coef = (my_coef_ptr) cinfo->coef;

	/* In an interleaved scan, an MCU row is the same as an iMCU row.
	* In a noninterleaved scan, an iMCU row has v_samp_factor MCU rows.
	* But at the bottom of the image, process only what's left.
	*/
	if (cinfo->comps_in_scan > 1) {
		coef->MCU_rows_per_iMCU_row = 1;
	} else {
		if (cinfo->input_iMCU_row < (cinfo->total_iMCU_rows-1))
			coef->MCU_rows_per_iMCU_row = cinfo->cur_comp_info[0]->v_samp_factor;
		else
			coef->MCU_rows_per_iMCU_row = cinfo->cur_comp_info[0]->last_row_height;
	}

	coef->MCU_ctr = 0;
	coef->MCU_vert_offset = 0;
}


/*
* Initialize for an input processing pass.
*/

METHODDEF(void)
start_input_pass (j_decompress_ptr cinfo)
{
	cinfo->input_iMCU_row = 0;
	start_iMCU_row(cinfo);
}


/*
* Initialize for an output processing pass.
*/

METHODDEF(void)
start_output_pass (j_decompress_ptr cinfo)
{
	cinfo->output_iMCU_row = 0;
}


/*
* Decompress and return some data in the single-pass case.
* Always attempts to emit one fully interleaved MCU row ("iMCU" row).
* Input and output must run in lockstep since we have only a one-MCU buffer.
* Return value is JPEG_ROW_COMPLETED, JPEG_SCAN_COMPLETED, or JPEG_SUSPENDED.
*
* NB: output_buf contains a plane for each component in image,
* which we index according to the component's SOF position.
*/

METHODDEF(int)
decompress_onepass (j_decompress_ptr cinfo, JSAMPIMAGE output_buf)
{
	my_coef_ptr coef = (my_coef_ptr) cinfo->coef;
	JDIMENSION MCU_col_num;	/* index of current MCU within row */
	JDIMENSION last_MCU_col = cinfo->MCUs_per_row - 1;
	JDIMENSION last_iMCU_row = cinfo->total_iMCU_rows - 1;
	int blkn, ci, xindex, yindex, yoffset, useful_width;
	JSAMPARRAY output_ptr;
	JDIMENSION start_col, output_col;
	jpeg_component_info *compptr;
	inverse_DCT_method_ptr inverse_DCT;

	/* Loop to process as much as one whole iMCU row */
	for (yoffset = coef->MCU_vert_offset; yoffset < coef->MCU_rows_per_iMCU_row;
		yoffset++) {
			for (MCU_col_num = coef->MCU_ctr; MCU_col_num <= last_MCU_col;
				MCU_col_num++) {
					/* Try to fetch an MCU.  Entropy decoder expects buffer to be zeroed. */
					MEMZERO((void *) coef->MCU_buffer[0],
						(size_t) (cinfo->blocks_in_MCU * SIZEOF(JBLOCK)));
					if (! (*cinfo->entropy->decode_mcu) (cinfo, coef->MCU_buffer)) {
						/* Suspension forced; update state counters and exit */
						coef->MCU_vert_offset = yoffset;
						coef->MCU_ctr = MCU_col_num;
						return JPEG_SUSPENDED;
					}
					/* Determine where data should go in output_buf and do the IDCT thing.
					* We skip dummy blocks at the right and bottom edges (but blkn gets
					* incremented past them!).  Note the inner loop relies on having
					* allocated the MCU_buffer[] blocks sequentially.
					*/
					blkn = 0;			/* index of current DCT block within MCU */
					for (ci = 0; ci < cinfo->comps_in_scan; ci++) {
						compptr = cinfo->cur_comp_info[ci];
						/* Don't bother to IDCT an uninteresting component. */
						if (! compptr->component_needed) {
							blkn += compptr->MCU_blocks;
							continue;
						}
						inverse_DCT = cinfo->idct->inverse_DCT[compptr->component_index];
						useful_width = (MCU_col_num < last_MCU_col) ? compptr->MCU_width
							: compptr->last_col_width;
						output_ptr = output_buf[compptr->component_index] +
							yoffset * compptr->DCT_scaled_size;
						start_col = MCU_col_num * compptr->MCU_sample_width;
						for (yindex = 0; yindex < compptr->MCU_height; yindex++) {
							if (cinfo->input_iMCU_row < last_iMCU_row ||
								yoffset+yindex < compptr->last_row_height) {
									output_col = start_col;
									for (xindex = 0; xindex < useful_width; xindex++) {
										(*inverse_DCT) (cinfo, compptr,
											(JCOEFPTR) coef->MCU_buffer[blkn+xindex],
											output_ptr, output_col);
										output_col += compptr->DCT_scaled_size;
									}
							}
							blkn += compptr->MCU_width;
							output_ptr += compptr->DCT_scaled_size;
						}
					}
			}
			/* Completed an MCU row, but perhaps not an iMCU row */
			coef->MCU_ctr = 0;
	}
	/* Completed the iMCU row, advance counters for next one */
	cinfo->output_iMCU_row++;
	if (++(cinfo->input_iMCU_row) < cinfo->total_iMCU_rows) {
		start_iMCU_row(cinfo);
		return JPEG_ROW_COMPLETED;
	}
	/* Completed the scan */
	(*cinfo->inputctl->finish_input_pass) (cinfo);
	return JPEG_SCAN_COMPLETED;
}


/*
* Dummy consume-input routine for single-pass operation.
*/

METHODDEF(int)
dummy_consume_data (j_decompress_ptr cinfo)
{
	return JPEG_SUSPENDED;	/* Always indicate nothing was done */
}


#ifdef D_MULTISCAN_FILES_SUPPORTED

/*
* Consume input data and store it in the full-image coefficient buffer.
* We read as much as one fully interleaved MCU row ("iMCU" row) per call,
* ie, v_samp_factor block rows for each component in the scan.
* Return value is JPEG_ROW_COMPLETED, JPEG_SCAN_COMPLETED, or JPEG_SUSPENDED.
*/

METHODDEF(int)
consume_data (j_decompress_ptr cinfo)
{
	my_coef_ptr coef = (my_coef_ptr) cinfo->coef;
	JDIMENSION MCU_col_num;	/* index of current MCU within row */
	int blkn, ci, xindex, yindex, yoffset;
	JDIMENSION start_col;
	JBLOCKARRAY buffer[MAX_COMPS_IN_SCAN];
	JBLOCKROW buffer_ptr;
	jpeg_component_info *compptr;

	/* Align the virtual buffers for the components used in this scan. */
	for (ci = 0; ci < cinfo->comps_in_scan; ci++) {
		compptr = cinfo->cur_comp_info[ci];
		buffer[ci] = (*cinfo->mem->access_virt_barray)
			((j_common_ptr) cinfo, coef->whole_image[compptr->component_index],
			cinfo->input_iMCU_row * compptr->v_samp_factor,
			(JDIMENSION) compptr->v_samp_factor, TRUE);
		/* Note: entropy decoder expects buffer to be zeroed,
		* but this is handled automatically by the memory manager
		* because we requested a pre-zeroed array.
		*/
	}

	/* Loop to process one whole iMCU row */
	for (yoffset = coef->MCU_vert_offset; yoffset < coef->MCU_rows_per_iMCU_row;
		yoffset++) {
			for (MCU_col_num = coef->MCU_ctr; MCU_col_num < cinfo->MCUs_per_row;
				MCU_col_num++) {
					/* Construct list of pointers to DCT blocks belonging to this MCU */
					blkn = 0;			/* index of current DCT block within MCU */
					for (ci = 0; ci < cinfo->comps_in_scan; ci++) {
						compptr = cinfo->cur_comp_info[ci];
						start_col = MCU_col_num * compptr->MCU_width;
						for (yindex = 0; yindex < compptr->MCU_height; yindex++) {
							buffer_ptr = buffer[ci][yindex+yoffset] + start_col;
							for (xindex = 0; xindex < compptr->MCU_width; xindex++) {
								coef->MCU_buffer[blkn++] = buffer_ptr++;
							}
						}
					}
					/* Try to fetch the MCU. */
					if (! (*cinfo->entropy->decode_mcu) (cinfo, coef->MCU_buffer)) {
						/* Suspension forced; update state counters and exit */
						coef->MCU_vert_offset = yoffset;
						coef->MCU_ctr = MCU_col_num;
						return JPEG_SUSPENDED;
					}
			}
			/* Completed an MCU row, but perhaps not an iMCU row */
			coef->MCU_ctr = 0;
	}
	/* Completed the iMCU row, advance counters for next one */
	if (++(cinfo->input_iMCU_row) < cinfo->total_iMCU_rows) {
		start_iMCU_row(cinfo);
		return JPEG_ROW_COMPLETED;
	}
	/* Completed the scan */
	(*cinfo->inputctl->finish_input_pass) (cinfo);
	return JPEG_SCAN_COMPLETED;
}


/*
* Decompress and return some data in the multi-pass case.
* Always attempts to emit one fully interleaved MCU row ("iMCU" row).
* Return value is JPEG_ROW_COMPLETED, JPEG_SCAN_COMPLETED, or JPEG_SUSPENDED.
*
* NB: output_buf contains a plane for each component in image.
*/

METHODDEF(int)
decompress_data (j_decompress_ptr cinfo, JSAMPIMAGE output_buf)
{
	my_coef_ptr coef = (my_coef_ptr) cinfo->coef;
	JDIMENSION last_iMCU_row = cinfo->total_iMCU_rows - 1;
	JDIMENSION block_num;
	int ci, block_row, block_rows;
	JBLOCKARRAY buffer;
	JBLOCKROW buffer_ptr;
	JSAMPARRAY output_ptr;
	JDIMENSION output_col;
	jpeg_component_info *compptr;
	inverse_DCT_method_ptr inverse_DCT;

	/* Force some input to be done if we are getting ahead of the input. */
	while (cinfo->input_scan_number < cinfo->output_scan_number ||
		(cinfo->input_scan_number == cinfo->output_scan_number &&
		cinfo->input_iMCU_row <= cinfo->output_iMCU_row)) {
			if ((*cinfo->inputctl->consume_input)(cinfo) == JPEG_SUSPENDED)
				return JPEG_SUSPENDED;
	}

	/* OK, output from the virtual arrays. */
	for (ci = 0, compptr = cinfo->comp_info; ci < cinfo->num_components;
		ci++, compptr++) {
			/* Don't bother to IDCT an uninteresting component. */
			if (! compptr->component_needed)
				continue;
			/* Align the virtual buffer for this component. */
			buffer = (*cinfo->mem->access_virt_barray)
				((j_common_ptr) cinfo, coef->whole_image[ci],
				cinfo->output_iMCU_row * compptr->v_samp_factor,
				(JDIMENSION) compptr->v_samp_factor, FALSE);
			/* Count non-dummy DCT block rows in this iMCU row. */
			if (cinfo->output_iMCU_row < last_iMCU_row)
				block_rows = compptr->v_samp_factor;
			else {
				/* NB: can't use last_row_height here; it is input-side-dependent! */
				block_rows = (int) (compptr->height_in_blocks % compptr->v_samp_factor);
				if (block_rows == 0) block_rows = compptr->v_samp_factor;
			}
			inverse_DCT = cinfo->idct->inverse_DCT[ci];
			output_ptr = output_buf[ci];
			/* Loop over all DCT blocks to be processed. */
			for (block_row = 0; block_row < block_rows; block_row++) {
				buffer_ptr = buffer[block_row];
				output_col = 0;
				for (block_num = 0; block_num < compptr->width_in_blocks; block_num++) {
					(*inverse_DCT) (cinfo, compptr, (JCOEFPTR) buffer_ptr,
						output_ptr, output_col);
					buffer_ptr++;
					output_col += compptr->DCT_scaled_size;
				}
				output_ptr += compptr->DCT_scaled_size;
			}
	}

	if (++(cinfo->output_iMCU_row) < cinfo->total_iMCU_rows)
		return JPEG_ROW_COMPLETED;
	return JPEG_SCAN_COMPLETED;
}

#endif /* D_MULTISCAN_FILES_SUPPORTED */


/*
* Initialize coefficient buffer controller.
*/

GLOBAL(void)
jinit_d_coef_controller (j_decompress_ptr cinfo, boolean need_full_buffer)
{
	my_coef_ptr coef;

	coef = (my_coef_ptr)
		(*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
		SIZEOF(my_coef_controller));
	cinfo->coef = (struct jpeg_d_coef_controller *) coef;
	coef->pub.start_input_pass = start_input_pass;
	coef->pub.start_output_pass = start_output_pass;

	/* Create the coefficient buffer. */
	if (need_full_buffer) {
#ifdef D_MULTISCAN_FILES_SUPPORTED
		/* Allocate a full-image virtual array for each component, */
		/* padded to a multiple of samp_factor DCT blocks in each direction. */
		/* Note we ask for a pre-zeroed array. */
		int ci, access_rows;
		jpeg_component_info *compptr;

		for (ci = 0, compptr = cinfo->comp_info; ci < cinfo->num_components;
			ci++, compptr++) {
				access_rows = compptr->v_samp_factor;
				coef->whole_image[ci] = (*cinfo->mem->request_virt_barray)
					((j_common_ptr) cinfo, JPOOL_IMAGE, TRUE,
					(JDIMENSION) jround_up((long) compptr->width_in_blocks,
					(long) compptr->h_samp_factor),
					(JDIMENSION) jround_up((long) compptr->height_in_blocks,
					(long) compptr->v_samp_factor),
					(JDIMENSION) access_rows);
		}
		coef->pub.consume_data = consume_data;
		coef->pub.decompress_data = decompress_data;
		coef->pub.coef_arrays = coef->whole_image; /* link to virtual arrays */
#else
		ERREXIT(cinfo, JERR_NOT_COMPILED);
#endif
	} else {
		/* We only need a single-MCU buffer. */
		JBLOCKROW buffer;
		int i;

		buffer = (JBLOCKROW)
			(*cinfo->mem->alloc_large) ((j_common_ptr) cinfo, JPOOL_IMAGE,
			D_MAX_BLOCKS_IN_MCU * SIZEOF(JBLOCK));
		for (i = 0; i < D_MAX_BLOCKS_IN_MCU; i++) {
			coef->MCU_buffer[i] = buffer + i;
		}
		coef->pub.consume_data = dummy_consume_data;
		coef->pub.decompress_data = decompress_onepass;
		coef->pub.coef_arrays = NULL; /* flag for no virtual arrays */
	}
}
