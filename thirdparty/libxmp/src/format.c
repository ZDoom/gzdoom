/* Extended Module Player
 * Copyright (C) 1996-2023 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "format.h"
#ifndef LIBXMP_NO_PROWIZARD
#include "loaders/prowizard/prowiz.h"
#endif

const struct format_loader *const format_loaders[NUM_FORMATS + 2] = {
	&libxmp_loader_xm,
	&libxmp_loader_mod,
#ifndef LIBXMP_CORE_DISABLE_IT
	&libxmp_loader_it,
#endif
	&libxmp_loader_s3m,
#ifndef LIBXMP_CORE_PLAYER
	&libxmp_loader_flt,
	&libxmp_loader_st,
	&libxmp_loader_stm,
	&libxmp_loader_stx,
	&libxmp_loader_mtm,
	&libxmp_loader_ice,
	&libxmp_loader_imf,
	&libxmp_loader_ptm,
	&libxmp_loader_mdl,
	&libxmp_loader_ult,
	&libxmp_loader_liq,
	&libxmp_loader_no,
	&libxmp_loader_masi,
	&libxmp_loader_masi16,
	&libxmp_loader_muse,
	&libxmp_loader_gal5,
	&libxmp_loader_gal4,
	&libxmp_loader_amf,
	&libxmp_loader_asylum,
	&libxmp_loader_gdm,
	&libxmp_loader_mmd1,
	&libxmp_loader_mmd3,
	&libxmp_loader_med2,
	&libxmp_loader_med3,
	&libxmp_loader_med4,
	/* &libxmp_loader_dmf, */
	&libxmp_loader_chip,
	&libxmp_loader_rtm,
	&libxmp_loader_pt3,
	/* &libxmp_loader_tcb, */
	&libxmp_loader_dt,
	/* &libxmp_loader_gtk, */
	/* &libxmp_loader_dtt, */
	&libxmp_loader_mgt,
	&libxmp_loader_arch,
	&libxmp_loader_sym,
	&libxmp_loader_digi,
	&libxmp_loader_dbm,
	&libxmp_loader_emod,
	&libxmp_loader_okt,
	&libxmp_loader_sfx,
	&libxmp_loader_far,
	&libxmp_loader_umx,
	&libxmp_loader_hmn,
	&libxmp_loader_stim,
	&libxmp_loader_coco,
	/* &libxmp_loader_mtp, */
	&libxmp_loader_ims,
	&libxmp_loader_669,
	&libxmp_loader_fnk,
	/* &libxmp_loader_amd, */
	/* &libxmp_loader_rad, */
	/* &libxmp_loader_hsc, */
	&libxmp_loader_mfp,
	&libxmp_loader_abk,
	/* &libxmp_loader_alm, */
	/* &libxmp_loader_polly, */
	/* &libxmp_loader_stc, */
	&libxmp_loader_xmf,
#ifndef LIBXMP_NO_PROWIZARD
	&libxmp_loader_pw,
#endif
#endif /* LIBXMP_CORE_PLAYER */
	NULL /* list teminator */
};

static const char *_farray[NUM_FORMATS + NUM_PW_FORMATS + 1] = { NULL };

const char *const *format_list(void)
{
	int count, i;

	if (_farray[0] == NULL) {
		for (count = i = 0; format_loaders[i] != NULL; i++) {
#ifndef LIBXMP_NO_PROWIZARD
			if (strcmp(format_loaders[i]->name, "prowizard") == 0) {
				int j;

				for (j = 0; pw_formats[j] != NULL; j++) {
					_farray[count++] = pw_formats[j]->name;
				}
				continue;
			}
#endif
			_farray[count++] = format_loaders[i]->name;
		}

		_farray[count] = NULL;
	}

	return _farray;
}
