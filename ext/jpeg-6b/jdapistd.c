/*
* jdapistd.c
*
* Copyright (C) 1994-1996, Thomas G. Lane.
* This file is part of the Independent JPEG Group's software.
* For conditions of distribution and use, see the accompanying README file.
*
* This file contains application interface code for the decompression half
* of the JPEG library.  These are the "standard" API routines that are
* used in the normal full-decompression case.  They are not used by a
* transcoding-only application.  Note that if an application links in
* jpeg_start_decompress, it will end up linking in the entire decompressor.
* We thus must separate this file from jdapimin.c to avoid linking the
* whole decompression library into a transcoder.
*/

#define JPEG_INTERNALS
#include "jinclude.h"
#include "jpeglib.h"


/* Forward declarations */
LOCAL(boolean) output_pass_setup JPP((j_decompress_ptr cinfo));


/*
* Decompression initialization.
* jpeg_read_header must be completed before calling this.
*
* If a multipass operating mode was selected, this will do all but the
* last pass, and thus may take a great deal of time.
*
* Returns FALSE if suspended.  The return value need be inspected only if
* a suspending data source is used.
*/

GLOBAL(boolean)
jpeg_start_decompress (j_decompress_ptr cinfo)
{
	if (cinfo->global_state == DSTATE_READY) {
		/* First call: initialize master control, select active modules */
		jinit_master_decompress(cinfo);
		cinfo->global_state = DSTATE_PRELOAD;
	}
	if (cinfo->global_state == DSTATE_PRELOAD) {
		/* If file has multiple scans, absorb them all into the coef buffer */
		if (cinfo->inputctl->has_multiple_scans) {
#ifdef D_MULTISCAN_FILES_SUPPORTED
			for (;;) {
				int retcode;
				/* Call progress monitor hook if present */
				if (cinfo->progress != NULL)
					(*cinfo->progress->progress_monitor) ((j_common_ptr) cinfo);
				/* Absorb some more input */
				retcode = (*cinfo->inputctl->consume_input) (cinfo);
				if (retcode == JPEG_SUSPENDED)
					return FALSE;
				if (retcode == JPEG_REACHED_EOI)
					break;
				/* Advance progress counter if appropriate */
				if (cinfo->progress != NULL &&
					(retcode == JPEG_ROW_COMPLETED || retcode == JPEG_REACHED_SOS)) {
						if (++cinfo->progress->pass_counter >= cinfo->progress->pass_limit) {
							/* jdmaster underestimated number of scans; ratchet up one scan */
							cinfo->progress->pass_limit += (long) cinfo->total_iMCU_rows;
						}
				}
			}
#else
			ERREXIT(cinfo, JERR_NOT_COMPILED);
#endif /* D_MULTISCAN_FILES_SUPPORTED */
		}
		cinfo->output_scan_number = cinfo->input_scan_number;
	} else if (cinfo->global_state != DSTATE_PRESCAN)
		ERREXIT1(cinfo, JERR_BAD_STATE, cinfo->global_state);
	/* Perform any dummy output passes, and set up for the final pass */
	return output_pass_setup(cinfo);
}


/*
* Set up for an output pass, and perform any dummy pass(es) needed.
* Common subroutine for jpeg_start_decompress and jpeg_start_output.
* Entry: global_state = DSTATE_PRESCAN only if previously suspended.
* Exit: If done, returns TRUE and sets global_state for proper output mode.
*       If suspended, returns FALSE and sets global_state = DSTATE_PRESCAN.
*/

LOCAL(boolean)
output_pass_setup (j_decompress_ptr cinfo)
{
	if (cinfo->global_state != DSTATE_PRESCAN) {
		/* First call: do pass setup */
		(*cinfo->master->prepare_for_output_pass) (cinfo);
		cinfo->output_scanline = 0;
		cinfo->global_state = DSTATE_PRESCAN;
	}
	/* Ready for application to drive output pass through
	* jpeg_read_scanlines or jpeg_read_raw_data.
	*/
	cinfo->global_state = DSTATE_SCANNING;
	return TRUE;
}


/*
* Read some scanlines of data from the JPEG decompressor.
*
* The return value will be the number of lines actually read.
* This may be less than the number requested in several cases,
* including bottom of image, data source suspension, and operating
* modes that emit multiple scanlines at a time.
*
* Note: we warn about excess calls to jpeg_read_scanlines() since
* this likely signals an application programmer error.  However,
* an oversize buffer (max_lines > scanlines remaining) is not an error.
*/

GLOBAL(JDIMENSION)
jpeg_read_scanlines (j_decompress_ptr cinfo, JSAMPARRAY scanlines,
					 JDIMENSION max_lines)
{
	JDIMENSION row_ctr;

	if (cinfo->global_state != DSTATE_SCANNING)
		ERREXIT1(cinfo, JERR_BAD_STATE, cinfo->global_state);
	if (cinfo->output_scanline >= cinfo->output_height) {
		WARNMS(cinfo, JWRN_TOO_MUCH_DATA);
		return 0;
	}

	/* Call progress monitor hook if present */
	if (cinfo->progress != NULL) {
		cinfo->progress->pass_counter = (long) cinfo->output_scanline;
		cinfo->progress->pass_limit = (long) cinfo->output_height;
		(*cinfo->progress->progress_monitor) ((j_common_ptr) cinfo);
	}

	/* Process some data */
	row_ctr = 0;
	(*cinfo->main->process_data) (cinfo, scanlines, &row_ctr, max_lines);
	cinfo->output_scanline += row_ctr;
	return row_ctr;
}
