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
#include "mod.h"
#include "../period.h"

static int chip_test(HIO_HANDLE *, char *, const int);
static int chip_load(struct module_data *, HIO_HANDLE *, const int);

const struct format_loader libxmp_loader_chip = {
	"Chiptracker",
	chip_test,
	chip_load
};

static int chip_test(HIO_HANDLE *f, char *t, const int start)
{
	char buf[4];

	hio_seek(f, start + 952, SEEK_SET);
	if (hio_read(buf, 1, 4, f) < 4)
		return -1;

	/* Also RASP? */
	if (memcmp(buf, "KRIS", 4) != 0)
		return -1;

	hio_seek(f, start + 0, SEEK_SET);
	libxmp_read_title(f, t, 20);

	return 0;
}

static int chip_load(struct module_data *m, HIO_HANDLE *f, const int start)
{
	struct xmp_module *mod = &m->mod;
	struct mod_header mh;
	uint8 *tidx;
	int i, j, tnum;

	LOAD_INIT();

	tidx = (uint8 *) calloc(1, 1024);
	if (tidx == NULL) {
		goto err;
	}

	hio_read(mh.name, 20, 1, f);
	hio_read16b(f);

	for (i = 0; i < 31; i++) {
		hio_read(mh.ins[i].name, 22, 1, f);
		mh.ins[i].size = hio_read16b(f);
		mh.ins[i].finetune = hio_read8(f);
		mh.ins[i].volume = hio_read8(f);
		mh.ins[i].loop_start = hio_read16b(f);
		mh.ins[i].loop_size = hio_read16b(f);
	}

	hio_read(mh.magic, 4, 1, f);
	mh.len = hio_read8(f);

	/* Sanity check */
	if (mh.len > 128) {
		goto err2;
	}

	mh.restart = hio_read8(f);
	hio_read(tidx, 1024, 1, f);
	hio_read16b(f);

	mod->chn = 4;
	mod->ins = 31;
	mod->smp = mod->ins;
	mod->len = mh.len;
	mod->pat = mh.len;
	mod->rst = mh.restart;

	tnum = 0;
	for (i = 0; i < mod->len; i++) {
		mod->xxo[i] = i;

		for (j = 0; j < 4; j++) {
			int t = tidx[2 * (4 * i + j)];
			if (t > tnum)
				tnum = t;
		}
	}

	mod->trk = tnum + 1;

	strncpy(mod->name, (char *)mh.name, 20);
	libxmp_set_type(m, "Chiptracker");
	MODULE_INFO();

	if (libxmp_init_instrument(m) < 0)
		goto err2;

	for (i = 0; i < mod->ins; i++) {
		struct xmp_instrument *xxi = &mod->xxi[i];
		struct xmp_sample *xxs = &mod->xxs[i];
		struct xmp_subinstrument *sub;

		if (libxmp_alloc_subinstrument(mod, i, 1) < 0)
			goto err2;

		sub = &xxi->sub[0];

		xxs->len = 2 * mh.ins[i].size;
		xxs->lps = mh.ins[i].loop_start;
		xxs->lpe = xxs->lps + 2 * mh.ins[i].loop_size;
		xxs->flg = mh.ins[i].loop_size > 1 ? XMP_SAMPLE_LOOP : 0;
		sub->fin = (int8) (mh.ins[i].finetune << 4);
		sub->vol = mh.ins[i].volume;
		sub->pan = 0x80;
		sub->sid = i;

		if (xxs->len > 0)
			xxi->nsm = 1;

		libxmp_instrument_name(mod, i, mh.ins[i].name, 22);
	}

	if (libxmp_init_pattern(mod) < 0)
		goto err2;

	for (i = 0; i < mod->len; i++) {
		if (libxmp_alloc_pattern(mod, i) < 0)
			goto err2;
		mod->xxp[i]->rows = 64;

		for (j = 0; j < 4; j++) {
			int t = tidx[2 * (4 * i + j)];
			mod->xxp[i]->index[j] = t;
		}
	}

	/* Load and convert tracks */
	D_(D_INFO "Stored tracks: %d", mod->trk);

	for (i = 0; i < mod->trk; i++) {
		if (libxmp_alloc_track(mod, i, 64) < 0)
			goto err2;

		for (j = 0; j < 64; j++) {
			struct xmp_event *event = &mod->xxt[i]->event[j];
			uint8 e[4];

			if (hio_read(e, 1, 4, f) < 4) {
				D_(D_CRIT "read error in track %d", i);
				goto err2;
			}
			if (e[0] && e[0] != 0xa8)
				event->note = 13 + e[0] / 2;
			event->ins = e[1];
			event->fxt = e[2] & 0x0f;
			event->fxp = e[3];
		}
	}

	m->period_type = PERIOD_MODRNG;

	/* Load samples */

	D_(D_INFO "Stored samples: %d", mod->smp);

	for (i = 0; i < mod->smp; i++) {
		if (mod->xxs[i].len == 0)
			continue;

		if (libxmp_load_sample(m, f, SAMPLE_FLAG_FULLREP, &mod->xxs[i], NULL) < 0)
			goto err2;
	}

	free(tidx);

	return 0;

    err2:
	free(tidx);
    err:
	return -1;
}
