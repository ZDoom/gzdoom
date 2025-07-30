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

/*
 * A module packer created by Shaun Southern. Samples are stored in a
 * separate file. File prefixes are mfp for song and smp for samples. For
 * more information see http://www.exotica.org.uk/wiki/Magnetic_Fields_Packer
 */

#include "loader.h"

static int mfp_test(HIO_HANDLE *, char *, const int);
static int mfp_load(struct module_data *, HIO_HANDLE *, const int);

const struct format_loader libxmp_loader_mfp = {
	"Magnetic Fields Packer",
	mfp_test,
	mfp_load
};

static int mfp_test(HIO_HANDLE *f, char *t, const int start)
{
	uint8 buf[384];
	int i, len, lps, lsz;

	if (HIO_HANDLE_TYPE(f) != HIO_HANDLE_TYPE_FILE)
		return -1;

	if (hio_read(buf, 1, 384, f) < 384)
		return -1;

	/* check restart byte */
	if (buf[249] != 0x7f)
		return -1;

	for (i = 0; i < 31; i++) {
		/* check size */
		len = readmem16b(buf + i * 8);
		if (len > 0x7fff)
			return -1;

		/* check finetune */
		if (buf[i * 8 + 2] & 0xf0)
			return -1;

		/* check volume */
		if (buf[i * 8 + 3] > 0x40)
			return -1;

		/* check loop start */
		lps = readmem16b(buf + i * 8 + 4);
		if (lps > len)
			return -1;

		/* check loop size */
		lsz = readmem16b(buf + i * 8 + 6);
		if (lps + lsz - 1 > len)
			return -1;

		if (len > 0 && lsz == 0)
			return -1;
	}

	if (buf[248] != readmem16b(buf + 378))
		return -1;

	if (readmem16b(buf + 378) != readmem16b(buf + 380))
		return -1;

	libxmp_read_title(f, t, 0);

	return 0;
}

static int mfp_load(struct module_data *m, HIO_HANDLE *f, const int start)
{
	struct xmp_module *mod = &m->mod;
	int i, j, k, x, y;
	struct xmp_event *event;
	char smp_filename[XMP_MAXPATH];
	HIO_HANDLE *s;
	int size1 /*, size2*/;
	int pat_addr, pat_table[128][4];
	uint8 buf[1024], mod_event[4];
	int row;

	LOAD_INIT();

	libxmp_set_type(m, "Magnetic Fields Packer");
	MODULE_INFO();

	mod->chn = 4;
	mod->ins = mod->smp = 31;

	if (libxmp_init_instrument(m) < 0)
		return -1;

	for (i = 0; i < 31; i++) {
		int loop_size;

		if (libxmp_alloc_subinstrument(mod, i, 1) < 0)
			return -1;

		mod->xxs[i].len = 2 * hio_read16b(f);
		mod->xxi[i].sub[0].fin = (int8)(hio_read8(f) << 4);
		mod->xxi[i].sub[0].vol = hio_read8(f);
		mod->xxs[i].lps = 2 * hio_read16b(f);
		loop_size = hio_read16b(f);

		mod->xxs[i].lpe = mod->xxs[i].lps + 2 * loop_size;
		mod->xxs[i].flg = loop_size > 1 ? XMP_SAMPLE_LOOP : 0;
		mod->xxi[i].sub[0].pan = 0x80;
		mod->xxi[i].sub[0].sid = i;
		mod->xxi[i].rls = 0xfff;

		if (mod->xxs[i].len > 0)
			mod->xxi[i].nsm = 1;

		D_(D_INFO "[%2X] %04x %04x %04x %c V%02x %+d",
			i, mod->xxs[i].len, mod->xxs[i].lps,
			mod->xxs[i].lpe,
			loop_size > 1 ? 'L' : ' ',
			mod->xxi[i].sub[0].vol, mod->xxi[i].sub[0].fin >> 4);
	}

	mod->len = mod->pat = hio_read8(f);
	hio_read8(f);		/* restart */

	for (i = 0; i < 128; i++) {
		mod->xxo[i] = hio_read8(f);
	}

	if (hio_error(f)) {
		return -1;
	}

	mod->trk = mod->pat * mod->chn;

	/* Read and convert patterns */

	if (libxmp_init_pattern(mod) < 0)
		return -1;

	size1 = hio_read16b(f);
	/* size2 = */ hio_read16b(f);

	for (i = 0; i < size1; i++) {		/* Read pattern table */
		for (j = 0; j < 4; j++) {
			pat_table[i][j] = hio_read16b(f);
		}
	}

	D_(D_INFO "Stored patterns: %d ", mod->pat);

	pat_addr = hio_tell(f);

	for (i = 0; i < mod->pat; i++) {
		if (libxmp_alloc_pattern_tracks(mod, i, 64) < 0)
			return -1;

		for (j = 0; j < 4; j++) {
			size_t len;
			hio_seek(f, pat_addr + pat_table[i][j], SEEK_SET);

			len = hio_read(buf, 1, 1024, f);

			for (row = k = 0; k < 4; k++) {
				for (x = 0; x < 4; x++) {
					for (y = 0; y < 4; y++, row++) {
						event = &EVENT(i, j, row);

						if (k >= len ||
						    buf[k] + x >= len ||
						    buf[buf[k] + x] + y >= len ||
						    buf[buf[buf[k] + x] + y] * 2 + 4 > len) {
							D_(D_CRIT "read error at pat %d", i);
							return -1;
						}
						memcpy(mod_event, &buf[buf[buf[buf[k] + x] + y] * 2], 4);
						libxmp_decode_protracker_event(event, mod_event);
					}
				}
			}
		}
	}

	/* Read samples */
	D_(D_INFO "Loading samples: %d", mod->ins);

	/* first check smp.filename */
	if (strlen(m->basename) < 5 || m->basename[3] != '.') {
		D_(D_CRIT "invalid filename %s", m->basename);
		goto err;
	}

	m->basename[0] = 's';
	m->basename[1] = 'm';
	m->basename[2] = 'p';
	snprintf(smp_filename, XMP_MAXPATH, "%s%s", m->dirname, m->basename);
	if ((s = hio_open(smp_filename, "rb")) == NULL) {
		/* handle .set filenames like in Kid Chaos*/
		if (strchr(m->basename, '-')) {
			char *p = strrchr(smp_filename, '-');
			if (p != NULL)
				strcpy(p, ".set");
		}
		if ((s = hio_open(smp_filename, "rb")) == NULL) {
			D_(D_CRIT "can't open sample file %s", smp_filename);
			goto err;
		}
	}

	for (i = 0; i < mod->ins; i++) {
		if (libxmp_load_sample(m, s, SAMPLE_FLAG_FULLREP,
				&mod->xxs[mod->xxi[i].sub[0].sid], NULL) < 0) {
			free(s);
			return -1;
		}
	}

	hio_close(s);

	m->period_type = PERIOD_MODRNG;

	return 0;

    err:
	for (i = 0; i < mod->ins; i++) {
		mod->xxi[i].nsm = 0;
		memset(&mod->xxs[i], 0, sizeof(struct xmp_sample));
	}

	return 0;
}
