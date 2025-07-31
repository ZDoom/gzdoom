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

/* Loader for Slamtilt modules based on the format description
 * written by Sylvain Chipaux (Asle/ReDoX). Get the Slamtilt demo
 * from game/demo in Aminet.
 */

/* Tested with the Slamtilt modules sent by Sipos Attila */

#include "loader.h"

#define MAGIC_STIM	MAGIC4('S','T','I','M')

static int stim_test(HIO_HANDLE *, char *, const int);
static int stim_load(struct module_data *, HIO_HANDLE *, const int);

const struct format_loader libxmp_loader_stim = {
	"Slamtilt",
	stim_test,
	stim_load
};

static int stim_test(HIO_HANDLE *f, char *t, const int start)
{
	if (hio_read32b(f) != MAGIC_STIM)
		return -1;

	if (hio_read16b(f) > 16)
		return -1;

	libxmp_read_title(f, t, 0);

	return 0;
}

struct stim_instrument {
	uint16 size;		/* Lenght of the sample (/2) */
	uint8 finetune;		/* Finetune (as ptk) */
	uint8 volume;		/* Volume (as ptk) */
	uint16 loop_start;	/* Loop start (/2) */
	uint16 loop_size;	/* Loop lenght (/2) */
};

struct stim_header {
	uint32 id;		/* "STIM" ID string */
	uint32 smpaddr;		/* Address of the sample descriptions */
	uint32 unknown[2];
	uint16 nos;		/* Number of samples (?) */
	uint16 len;		/* Size of pattern list */
	uint16 pat;		/* Number of patterns saved */
	uint8 order[128];	/* Pattern list */
	uint32 pataddr[64];	/* Pattern addresses (add 0xc) */
};

static int stim_load(struct module_data *m, HIO_HANDLE *f, const int start)
{
	struct xmp_module *mod = &m->mod;
	int i, j, k;
	struct xmp_event *event;
	struct stim_header sh;
	struct stim_instrument si;
	uint8 b1, b2, b3;

	LOAD_INIT();

	sh.id = hio_read32b(f);
	sh.smpaddr = hio_read32b(f);
	hio_read32b(f);
	hio_read32b(f);
	sh.nos = hio_read16b(f);
	sh.len = hio_read16b(f);
	sh.pat = hio_read16b(f);
	if (hio_read(sh.order, 128, 1, f) == 0) {
		return -1;
	}

	/* Sanity check */
	if (sh.nos > 31 || sh.len > 128 || sh.pat > 64) {
		return -1;
	}

	for (i = 0; i < 64; i++) {
		sh.pataddr[i] = hio_read32b(f) + 0x0c;
		if (sh.pataddr[i] > 0x00100000)
			return -1;
	}

	mod->chn = 4;
	mod->len = sh.len;
	mod->pat = sh.pat;
	mod->ins = sh.nos;
	mod->smp = mod->ins;
	mod->trk = mod->pat * mod->chn;

	for (i = 0; i < mod->len; i++)
		mod->xxo[i] = sh.order[i];

	libxmp_set_type(m, "Slamtilt");

	MODULE_INFO();

	if (libxmp_init_pattern(mod) < 0)
		return -1;

	/* Load and convert patterns */
	D_(D_INFO "Stored patterns: %d", mod->pat);

	for (i = 0; i < mod->pat; i++) {
		if (libxmp_alloc_pattern_tracks(mod, i, 64) < 0)
			return -1;

		hio_seek(f, start + sh.pataddr[i] + 8, SEEK_SET);

		for (j = 0; j < 4; j++) {
			for (k = 0; k < 64; k++) {
				event = &EVENT(i, j, k);
				b1 = hio_read8(f);

				if (b1 & 0x80) {
					k += b1 & 0x7f;
					continue;
				}

				/* STIM event format:
				 *
				 *     __ Fx __
				 *    /        \
				 *   ||        ||
				 *  0000 0000  0000 0000  0000 0000
				 *  |  |    |    |     |  |       |
				 *  |   \  /      \   /    \     /
				 *  |    smp      note      Fx Val
				 *  |
				 *  Description bit set to 0.
				 */

				b2 = hio_read8(f);
				b3 = hio_read8(f);

				if ((event->note = b2 & 0x3f) != 0)
					event->note += 47;
				event->ins = b1 & 0x1f;
				event->fxt = ((b2 >> 4) & 0x0c) | (b1 >> 5);
				event->fxp = b3;

				libxmp_disable_continue_fx(event);
			}
		}
	}

	if (libxmp_init_instrument(m) < 0)
		return -1;

	D_(D_INFO "Stored samples: %d", mod->smp);

	hio_seek(f, start + sh.smpaddr + mod->smp * 4, SEEK_SET);

	for (i = 0; i < mod->smp; i++) {
		si.size = hio_read16b(f);
		si.finetune = hio_read8(f);
		si.volume = hio_read8(f);
		si.loop_start = hio_read16b(f);
		si.loop_size = hio_read16b(f);

		if (libxmp_alloc_subinstrument(mod, i, 1) < 0)
			return -1;

		mod->xxs[i].len = 2 * si.size;
		mod->xxs[i].lps = 2 * si.loop_start;
		mod->xxs[i].lpe = mod->xxs[i].lps + 2 * si.loop_size;
		mod->xxs[i].flg = si.loop_size > 1 ? XMP_SAMPLE_LOOP : 0;
		mod->xxi[i].sub[0].fin = (int8) (si.finetune << 4);
		mod->xxi[i].sub[0].vol = si.volume;
		mod->xxi[i].sub[0].pan = 0x80;
		mod->xxi[i].sub[0].sid = i;
		mod->xxi[i].rls = 0xfff;

		if (mod->xxs[i].len > 0)
			mod->xxi[i].nsm = 1;

		D_(D_INFO "[%2X] %04x %04x %04x %c V%02x %+d",
			       i, mod->xxs[i].len, mod->xxs[i].lps,
			       mod->xxs[i].lpe, si.loop_size > 1 ? 'L' : ' ',
			       mod->xxi[i].sub[0].vol, mod->xxi[i].sub[0].fin >> 4);

		if (!mod->xxs[i].len)
			continue;

		if (libxmp_load_sample(m, f, 0, &mod->xxs[i], NULL) < 0)
			return -1;
	}

	m->period_type = PERIOD_MODRNG;

	return 0;
}
