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

/* Loader for Soundtracker 2.6/Ice Tracker modules */

#include "loader.h"

#define MAGIC_MTN_	MAGIC4('M','T','N',0)
#define MAGIC_IT10	MAGIC4('I','T','1','0')

static int ice_test(HIO_HANDLE *, char *, const int);
static int ice_load(struct module_data *, HIO_HANDLE *, const int);

const struct format_loader libxmp_loader_ice = {
	"Soundtracker 2.6/Ice Tracker",
	ice_test,
	ice_load
};

static int ice_test(HIO_HANDLE * f, char *t, const int start)
{
	uint32 magic;

	hio_seek(f, start + 1464, SEEK_SET);
	magic = hio_read32b(f);
	if (magic != MAGIC_MTN_ && magic != MAGIC_IT10)
		return -1;

	hio_seek(f, start + 0, SEEK_SET);
	libxmp_read_title(f, t, 28);

	return 0;
}

struct ice_ins {
	char name[22];		/* Instrument name */
	uint16 len;		/* Sample length / 2 */
	uint8 finetune;		/* Finetune */
	uint8 volume;		/* Volume (0-63) */
	uint16 loop_start;	/* Sample loop start in file */
	uint16 loop_size;	/* Loop size / 2 */
};

struct ice_header {
	char title[20];
	struct ice_ins ins[31];	/* Instruments */
	uint8 len;		/* Size of the pattern list */
	uint8 trk;		/* Number of tracks */
	uint8 ord[128][4];
	uint32 magic;		/* 'MTN\0', 'IT10' */
};

static int ice_load(struct module_data *m, HIO_HANDLE * f, const int start)
{
	struct xmp_module *mod = &m->mod;
	int i, j;
	struct xmp_event *event;
	struct ice_header ih;
	uint8 ev[4];

	LOAD_INIT();

	hio_read(ih.title, 20, 1, f);
	for (i = 0; i < 31; i++) {
		hio_read(ih.ins[i].name, 22, 1, f);
		ih.ins[i].len = hio_read16b(f);
		ih.ins[i].finetune = hio_read8(f);
		ih.ins[i].volume = hio_read8(f);
		ih.ins[i].loop_start = hio_read16b(f);
		ih.ins[i].loop_size = hio_read16b(f);
	}
	ih.len = hio_read8(f);
	ih.trk = hio_read8(f);
	hio_read(ih.ord, 128 * 4, 1, f);
	ih.magic = hio_read32b(f);

	/* Sanity check */
	if (ih.len > 128) {
		return -1;
	}
	for (i = 0; i < ih.len; i++) {
		for (j = 0; j < 4; j++) {
			if (ih.ord[i][j] >= ih.trk)
				return -1;
		}
	}

	if (ih.magic == MAGIC_IT10)
		libxmp_set_type(m, "Ice Tracker");
	else if (ih.magic == MAGIC_MTN_)
		libxmp_set_type(m, "Soundtracker 2.6");
	else
		return -1;

	mod->ins = 31;
	mod->smp = mod->ins;
	mod->pat = ih.len;
	mod->len = ih.len;
	mod->trk = ih.trk;

	strncpy(mod->name, (char *)ih.title, 20);
	MODULE_INFO();

	if (libxmp_init_instrument(m) < 0)
		return -1;

	for (i = 0; i < mod->ins; i++) {
		struct xmp_instrument *xxi;
		struct xmp_sample *xxs;

		if (libxmp_alloc_subinstrument(mod, i, 1) < 0)
			return -1;

		xxi = &mod->xxi[i];
		xxs = &mod->xxs[i];

		xxs->len = 2 * ih.ins[i].len;
		xxs->lps = 2 * ih.ins[i].loop_start;
		xxs->lpe = xxs->lps + 2 * ih.ins[i].loop_size;
		xxs->flg = ih.ins[i].loop_size > 1 ? XMP_SAMPLE_LOOP : 0;
		xxi->sub[0].vol = ih.ins[i].volume;
		/* xxi->sub[0].fin = (int8)(ih.ins[i].finetune << 4); */
		xxi->sub[0].pan = 0x80;
		xxi->sub[0].sid = i;

		if (xxs->len > 0)
			xxi->nsm = 1;

		D_(D_INFO "[%2X] %-22.22s %04x %04x %04x %c %02x %01x",
		   i, ih.ins[i].name, xxs->len, xxs->lps,
		   xxs->lpe, xxs->flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
		   xxi->sub[0].vol, xxi->sub[0].fin >> 4);
	}

	if (libxmp_init_pattern(mod) < 0)
		return -1;

	D_(D_INFO "Stored patterns: %d", mod->pat);

	for (i = 0; i < mod->pat; i++) {
		if (libxmp_alloc_pattern(mod, i) < 0)
			return -1;
		mod->xxp[i]->rows = 64;

		for (j = 0; j < mod->chn; j++) {
			mod->xxp[i]->index[j] = ih.ord[i][j];
		}
		mod->xxo[i] = i;
	}

	D_(D_INFO "Stored tracks: %d", mod->trk);

	for (i = 0; i < mod->trk; i++) {
		if (libxmp_alloc_track(mod, i, 64) < 0)
			return -1;

		for (j = 0; j < mod->xxt[i]->rows; j++) {
			event = &mod->xxt[i]->event[j];
			if (hio_read(ev, 1, 4, f) < 4) {
				D_(D_CRIT "read error at track %d", i);
				return -1;
			}
			libxmp_decode_protracker_event(event, ev);

			if (event->fxt == FX_SPEED) {
				if (MSN(event->fxp) && LSN(event->fxp)) {
					event->fxt = FX_ICE_SPEED;
				}
			}
		}
	}

	m->period_type = PERIOD_MODRNG;

	/* Read samples */

	D_(D_INFO "Stored samples: %d", mod->smp);

	for (i = 0; i < mod->ins; i++) {
		if (mod->xxs[i].len <= 4)
			continue;
		if (libxmp_load_sample(m, f, 0, &mod->xxs[i], NULL) < 0)
			return -1;
	}

	return 0;
}
