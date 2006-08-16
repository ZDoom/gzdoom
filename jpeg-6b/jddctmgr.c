/*
* jddctmgr.c
*
* Copyright (C) 1994-1996, Thomas G. Lane.
* This file is part of the Independent JPEG Group's software.
* For conditions of distribution and use, see the accompanying README file.
*
* This file contains the inverse-DCT management logic.
* This code selects a particular IDCT implementation to be used,
* and it performs related housekeeping chores.  No code in this file
* is executed per IDCT step, only during output pass setup.
*
* Note that the IDCT routines are responsible for performing coefficient
* dequantization as well as the IDCT proper.  This module sets up the
* dequantization multiplier table needed by the IDCT routine.
*/

#define JPEG_INTERNALS
#include "jinclude.h"
#include "jpeglib.h"
#include "jdct.h"		/* Private declarations for DCT subsystem */


/*
* The decompressor input side (jdinput.c) saves away the appropriate
* quantization table for each component at the start of the first scan
* involving that component.  (This is necessary in order to correctly
* decode files that reuse Q-table slots.)
* When we are ready to make an output pass, the saved Q-table is converted
* to a multiplier table that will actually be used by the IDCT routine.
* The multiplier table contents are IDCT-method-dependent.  To support
* application changes in IDCT method between scans, we can remake the
* multiplier tables if necessary.
* In buffered-image mode, the first output pass may occur before any data
* has been seen for some components, and thus before their Q-tables have
* been saved away.  To handle this case, multiplier tables are preset
* to zeroes; the result of the IDCT will be a neutral gray level.
*/


/* Private subobject for this module */

typedef struct {
	struct jpeg_inverse_dct pub;	/* public fields */

	/* This array contains the IDCT method code that each multiplier table
	* is currently set up for, or -1 if it's not yet set up.
	* The actual multiplier tables are pointed to by dct_table in the
	* per-component comp_info structures.
	*/
	int cur_method[MAX_COMPONENTS];
} my_idct_controller;

typedef my_idct_controller * my_idct_ptr;


/* Allocated multiplier tables: big enough for any supported variant */

typedef union {
	ISLOW_MULT_TYPE islow_array[DCTSIZE2];
} multiplier_table;


/* The current scaled-IDCT routines require ISLOW-style multiplier tables,
* so be sure to compile that code if either ISLOW or SCALING is requested.
*/
#ifdef DCT_ISLOW_SUPPORTED
#define PROVIDE_ISLOW_TABLES
#endif


/*
* Prepare for an output pass.
* Here we select the proper IDCT routine for each component and build
* a matching multiplier table.
*/

METHODDEF(void)
start_pass (j_decompress_ptr cinfo)
{
	my_idct_ptr idct = (my_idct_ptr) cinfo->idct;
	int ci, i;
	jpeg_component_info *compptr;
	int method = 0;
	inverse_DCT_method_ptr method_ptr = NULL;
	JQUANT_TBL * qtbl;

	for (ci = 0, compptr = cinfo->comp_info; ci < cinfo->num_components;
		ci++, compptr++) {
			/* Select the proper IDCT routine for this component's scaling */
			switch (compptr->DCT_scaled_size) {
	case DCTSIZE:
		switch (cinfo->dct_method) {
#ifdef DCT_ISLOW_SUPPORTED
	case JDCT_ISLOW:
		method_ptr = jpeg_idct_islow;
		method = JDCT_ISLOW;
		break;
#endif
	default:
		ERREXIT(cinfo, JERR_NOT_COMPILED);
		break;
		}
		break;
	default:
		ERREXIT1(cinfo, JERR_BAD_DCTSIZE, compptr->DCT_scaled_size);
		break;
			}
			idct->pub.inverse_DCT[ci] = method_ptr;
			/* Create multiplier table from quant table.
			* However, we can skip this if the component is uninteresting
			* or if we already built the table.  Also, if no quant table
			* has yet been saved for the component, we leave the
			* multiplier table all-zero; we'll be reading zeroes from the
			* coefficient controller's buffer anyway.
			*/
			if (! compptr->component_needed || idct->cur_method[ci] == method)
				continue;
			qtbl = compptr->quant_table;
			if (qtbl == NULL)		/* happens if no data yet for component */
				continue;
			idct->cur_method[ci] = method;
			switch (method) {
#ifdef PROVIDE_ISLOW_TABLES
	case JDCT_ISLOW:
		{
			/* For LL&M IDCT method, multipliers are equal to raw quantization
			* coefficients, but are stored as ints to ensure access efficiency.
			*/
			ISLOW_MULT_TYPE * ismtbl = (ISLOW_MULT_TYPE *) compptr->dct_table;
			for (i = 0; i < DCTSIZE2; i++) {
				ismtbl[i] = (ISLOW_MULT_TYPE) qtbl->quantval[i];
			}
		}
		break;
#endif
	default:
		ERREXIT(cinfo, JERR_NOT_COMPILED);
		break;
			}
	}
}


/*
* Initialize IDCT manager.
*/

GLOBAL(void)
jinit_inverse_dct (j_decompress_ptr cinfo)
{
	my_idct_ptr idct;
	int ci;
	jpeg_component_info *compptr;

	idct = (my_idct_ptr)
		(*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
		SIZEOF(my_idct_controller));
	cinfo->idct = (struct jpeg_inverse_dct *) idct;
	idct->pub.start_pass = start_pass;

	for (ci = 0, compptr = cinfo->comp_info; ci < cinfo->num_components;
		ci++, compptr++) {
			/* Allocate and pre-zero a multiplier table for each component */
			compptr->dct_table =
				(*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
				SIZEOF(multiplier_table));
			MEMZERO(compptr->dct_table, SIZEOF(multiplier_table));
			/* Mark multiplier table not yet set up for any method */
			idct->cur_method[ci] = -1;
	}
}
