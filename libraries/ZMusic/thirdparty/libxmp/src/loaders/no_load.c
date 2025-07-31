/* Extended Module Player
 * Copyright (C) 1996-2021 Claudio Matsuoka and Hipolito Carraro Jr
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

#include "loader.h"
#include "../period.h"

/* Nir Oren's Liquid Tracker old "NO" format. I have only one NO module,
 * Moti Radomski's "Time after time" from ftp.modland.com.
 */


static int no_test (HIO_HANDLE *, char *, const int);
static int no_load (struct module_data *, HIO_HANDLE *, const int);

const struct format_loader libxmp_loader_no = {
	"Liquid Tracker NO",
	no_test,
	no_load
};

static int no_test(HIO_HANDLE *f, char *t, const int start)
{
	int nsize, pat, chn;
	int i;

	hio_seek(f, start, SEEK_CUR);

	if (hio_read32b(f) != 0x4e4f0000)		/* NO 0x00 0x00 */
		return -1;

	nsize = hio_read8(f);
	if (nsize != 20)
		return -1;

	/* test title */
	for (i = 0; i < nsize; i++) {
		if (hio_read8(f) == 0)
			return -1;
	}

	hio_seek(f, 9, SEEK_CUR);

	/* test number of patterns */
	pat = hio_read8(f);
	if (pat == 0)
		return -1;

	hio_read8(f);

	/* test number of channels */
	chn = hio_read8(f);
	if (chn <= 0 || chn > 16)
		return -1;

	hio_seek(f, start + 5, SEEK_SET);

	libxmp_read_title(f, t, nsize);

	return 0;
}


static const uint8 fx[15] = {
	FX_ARPEGGIO,
	0,
	FX_BREAK,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
};


static int no_load(struct module_data *m, HIO_HANDLE *f, const int start)
{
	struct xmp_module *mod = &m->mod;
	struct xmp_event *event;
	int i, j, k;
	int nsize;

	LOAD_INIT();

	hio_read32b(f);			/* NO 0x00 0x00 */

	libxmp_set_type(m, "Liquid Tracker");

	nsize = hio_read8(f);
	for (i = 0; i < nsize; i++) {
		uint8 x = hio_read8(f);
		if (i < XMP_NAME_SIZE)
			mod->name[i] = x;
	}

	hio_read16l(f);
	hio_read16l(f);
	hio_read16l(f);
	hio_read16l(f);
	hio_read8(f);
	mod->pat = hio_read8(f);
	hio_read8(f);
	mod->chn = hio_read8(f);
	mod->trk = mod->pat * mod->chn;
	hio_read8(f);
	hio_read16l(f);
	hio_read16l(f);
	hio_read8(f);
	mod->ins = mod->smp = 63;

	for (i = 0; i < 256; i++) {
		uint8 x = hio_read8(f);
		if (x == 0xff)
			break;
		mod->xxo[i] = x;
	}
	hio_seek(f, 255 - i, SEEK_CUR);
	mod->len = i;

	m->c4rate = C4_NTSC_RATE;

	MODULE_INFO();

	if (libxmp_init_instrument(m) < 0)
		return -1;

	/* Read instrument names */
	for (i = 0; i < mod->ins; i++) {
		int hasname, c2spd;

		if (libxmp_alloc_subinstrument(mod, i, 1) < 0)
			return -1;

		nsize = hio_read8(f);
		if (hio_error(f)) {
			return -1;
		}

		hasname = 0;
		for (j = 0; j < nsize; j++) {
			uint8 x = hio_read8(f);
			if (x != 0x20)
				hasname = 1;
			if (j < 32)
				mod->xxi[i].name[j] = x;
		}
		if (!hasname)
			mod->xxi[i].name[0] = 0;

		hio_read32l(f);
		hio_read32l(f);
		mod->xxi[i].sub[0].vol = hio_read8(f);
		c2spd = hio_read16l(f);
		mod->xxs[i].len = hio_read16l(f);
		mod->xxs[i].lps = hio_read16l(f);
		mod->xxs[i].lpe = hio_read16l(f);
		hio_read32l(f);
		hio_read16l(f);

		if (mod->xxs[i].len > 0)
			mod->xxi[i].nsm = 1;

		/*
		mod->xxs[i].lps = 0;
		mod->xxs[i].lpe = 0;
		*/
		mod->xxs[i].flg = mod->xxs[i].lpe > 0 ? XMP_SAMPLE_LOOP : 0;
		mod->xxi[i].sub[0].fin = 0;
		mod->xxi[i].sub[0].pan = 0x80;
		mod->xxi[i].sub[0].sid = i;

		D_(D_INFO "[%2X] %-22.22s  %04x %04x %04x %c V%02x %5d",
				i, mod->xxi[i].name,
				mod->xxs[i].len, mod->xxs[i].lps, mod->xxs[i].lpe,
				mod->xxs[i].flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
				mod->xxi[i].sub[0].vol, c2spd);

		libxmp_c2spd_to_note(c2spd, &mod->xxi[i].sub[0].xpo, &mod->xxi[i].sub[0].fin);
	}

	if (libxmp_init_pattern(mod) < 0)
		return -1;

	/* Read and convert patterns */
	D_(D_INFO "Stored patterns: %d ", mod->pat);

	for (i = 0; i < mod->pat; i++) {
		if (libxmp_alloc_pattern_tracks(mod, i, 64) < 0)
			return -1;

		for (j = 0; j < mod->xxp[i]->rows; j++) {
			for (k = 0; k < mod->chn; k++) {
				uint32 x, note, ins, vol, fxt, fxp;

				event = &EVENT (i, k, j);

				x = hio_read32l(f);
				note = x & 0x0000003f;
				ins = (x & 0x00001fc0) >> 6;
				vol = (x & 0x000fe000) >> 13;
				fxt = (x & 0x00f00000) >> 20;
				fxp = (x & 0xff000000) >> 24;

				if (note != 0x3f)
					event->note = 36 + note;
				if (ins != 0x7f)
					event->ins = 1 + ins;
				if (vol != 0x7f)
					event->vol = vol;
				if (fxt != 0x0f) {
					event->fxt = fx[fxt];
					event->fxp = fxp;
				}
			}
		}
	}

	/* Read samples */
	D_(D_INFO "Stored samples: %d", mod->smp);

	for (i = 0; i < mod->ins; i++) {
		if (mod->xxs[i].len == 0)
			continue;
		if (libxmp_load_sample(m, f, SAMPLE_FLAG_UNS, &mod->xxs[i], NULL) < 0)
			return -1;
	}

	m->quirk |= QUIRKS_ST3;
	m->read_event_type = READ_EVENT_ST3;

	return 0;
}
