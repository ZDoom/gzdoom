/* Extended Module Player
 * Copyright (C) 1996-2022 Claudio Matsuoka and Hipolito Carraro Jr
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

#ifndef LIBXMP_NO_PROWIZARD

#include "prowizard/prowiz.h"
#include "../tempfile.h"

extern struct list_head *checked_format;

static int pw_test(HIO_HANDLE *, char *, const int);
static int pw_load(struct module_data *, HIO_HANDLE *, const int);

const struct format_loader libxmp_loader_pw = {
	"prowizard",
	pw_test,
	pw_load
};

int pw_test_format(HIO_HANDLE *f, char *t, const int start,
		   struct xmp_test_info *info)
{
	const struct pw_format *format;

	format = pw_check(f, info);

	return format ? 0 : -1;
}

static int pw_test(HIO_HANDLE *f, char *t, const int start)
{
	return pw_test_format(f, t, start, NULL);
}

static int pw_load(struct module_data *m, HIO_HANDLE *h, const int start)
{
	struct xmp_module *mod = &m->mod;
	struct xmp_event *event;
	struct mod_header mh;
	uint8 mod_event[4];
	HIO_HANDLE *f;
	FILE *temp;
	const char *name;
	char *temp_name;
	int i, j;

	/* Prowizard depacking */

	if ((temp = make_temp_file(&temp_name)) == NULL) {
		goto err;
	}

	if (pw_wizardry(h, temp, &name) < 0) {
		fclose(temp);
		goto err2;
	}

	/* Module loading */

	D_(D_INFO "loading generated module");
	if ((f = hio_open_file2(temp)) == NULL) {
		goto err2;
	}

	if (hio_seek(f, 0, start) < 0) {
		goto err3;
	}

	hio_read(mh.name, 20, 1, f);
	for (i = 0; i < 31; i++) {
		hio_read(mh.ins[i].name, 22, 1, f);
		mh.ins[i].size = hio_read16b(f);
		mh.ins[i].finetune = hio_read8(f);
		mh.ins[i].volume = hio_read8(f);
		mh.ins[i].loop_start = hio_read16b(f);
		mh.ins[i].loop_size = hio_read16b(f);
	}
	mh.len = hio_read8(f);
	mh.restart = hio_read8(f);
	hio_read(mh.order, 128, 1, f);
	hio_read(mh.magic, 4, 1, f);

	if (memcmp(mh.magic, "M.K.", 4)) {
		goto err3;
	}

	mod->ins = 31;
	mod->smp = mod->ins;
	mod->chn = 4;
	mod->len = mh.len;
	mod->rst = mh.restart;
	memcpy(mod->xxo, mh.order, 128);

	for (i = 0; i < 128; i++) {
		if (mod->xxo[i] > mod->pat)
			mod->pat = mod->xxo[i];
	}

	mod->pat++;

	mod->trk = mod->chn * mod->pat;

	snprintf(mod->name, XMP_NAME_SIZE, "%s", (char *)mh.name);
	snprintf(mod->type, XMP_NAME_SIZE, "%s", name);
	MODULE_INFO();

	if (libxmp_init_instrument(m) < 0) {
		goto err3;
	}

	for (i = 0; i < mod->ins; i++) {
		if (libxmp_alloc_subinstrument(mod, i, 1) < 0)
			goto err3;

		mod->xxs[i].len = 2 * mh.ins[i].size;
		mod->xxs[i].lps = 2 * mh.ins[i].loop_start;
		mod->xxs[i].lpe = mod->xxs[i].lps + 2 * mh.ins[i].loop_size;
		mod->xxs[i].flg = mh.ins[i].loop_size > 1 ? XMP_SAMPLE_LOOP : 0;
		mod->xxi[i].sub[0].fin = (int8) (mh.ins[i].finetune << 4);
		mod->xxi[i].sub[0].vol = mh.ins[i].volume;
		mod->xxi[i].sub[0].pan = 0x80;
		mod->xxi[i].sub[0].sid = i;
		mod->xxi[i].rls = 0xfff;

		if (mod->xxs[i].len > 0)
			mod->xxi[i].nsm = 1;

		libxmp_instrument_name(mod, i, mh.ins[i].name, 22);

		D_(D_INFO "[%2X] %-22.22s %04x %04x %04x %c V%02x %+d",
			     i, mod->xxi[i].name, mod->xxs[i].len,
			     mod->xxs[i].lps, mod->xxs[i].lpe,
			     mh.ins[i].loop_size > 1 ? 'L' : ' ',
			     mod->xxi[i].sub[0].vol,
			     mod->xxi[i].sub[0].fin >> 4);
	}

	if (libxmp_init_pattern(mod) < 0) {
		goto err3;
	}

	/* Load and convert patterns */
	D_(D_INFO "Stored patterns: %d", mod->pat);

	for (i = 0; i < mod->pat; i++) {
		if (libxmp_alloc_pattern_tracks(mod, i, 64) < 0)
			goto err3;

		for (j = 0; j < (64 * 4); j++) {
			event = &EVENT(i, j % 4, j / 4);
			if (hio_read(mod_event, 1, 4, f) < 4)
				goto err3;
			libxmp_decode_protracker_event(event, mod_event);
		}
	}

	m->period_type = PERIOD_MODRNG;

	/* Load samples */

	D_(D_INFO "Stored samples: %d", mod->smp);
	for (i = 0; i < mod->smp; i++) {
		if (libxmp_load_sample(m, f, 0, &mod->xxs[i], NULL) < 0)
			goto err3;
	}

	hio_close(f);
	unlink_temp_file(temp_name);
	return 0;

    err3:
	hio_close(f);
    err2:
	unlink_temp_file(temp_name);
    err:
	return -1;
}

#endif /* LIBXMP_NO_PROWIZARD */
