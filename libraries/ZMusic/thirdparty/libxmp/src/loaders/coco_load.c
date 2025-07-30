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

static int coco_test (HIO_HANDLE *, char *, const int);
static int coco_load (struct module_data *, HIO_HANDLE *, const int);

const struct format_loader libxmp_loader_coco = {
	"Coconizer",
	coco_test,
	coco_load
};

static int check_cr(uint8 *s, int n)
{
	while (n--) {
		if (*s++ == 0x0d)
			return 0;
	}

	return -1;
}

static int coco_test(HIO_HANDLE *f, char *t, const int start)
{
	uint8 x, buf[20];
	uint32 y;
	int n, i;

	x = hio_read8(f);

	/* check number of channels */
	if (x != 0x84 && x != 0x88)
		return -1;

	if (hio_read(buf, 1, 20, f) != 20)	/* read title */
		return -1;
	if (check_cr(buf, 20) != 0)
		return -1;

	n = hio_read8(f);			/* instruments */
	if (n <= 0 || n > 100)
		return -1;

	hio_read8(f);			/* sequences */
	hio_read8(f);			/* patterns */

	y = hio_read32l(f);
	if (y < 64 || y > 0x00100000)	/* offset of sequence table */
		return -1;

	y = hio_read32l(f);			/* offset of patterns */
	if (y < 64 || y > 0x00100000)
		return -1;

	for (i = 0; i < n; i++) {
		int ofs = hio_read32l(f);
		int len = hio_read32l(f);
		int vol = hio_read32l(f);
		int lps = hio_read32l(f);
		int lsz = hio_read32l(f);

		if (ofs < 64 || ofs > 0x00100000)
			return -1;

		if (vol < 0 || vol > 0xff)
			return -1;

		if (len < 0 || lps < 0 || lsz < 0)
			return -1;
		if (len > 0x00100000 || lps > 0x00100000 || lsz > 0x00100000)
			return -1;

		if (lps > 0 && lps + lsz - 1 > len)
			return -1;

		hio_read(buf, 1, 11, f);
		hio_read8(f);	/* unused */
	}

	hio_seek(f, start + 1, SEEK_SET);
	libxmp_read_title(f, t, 20);

#if 0
	for (i = 0; i < 20; i++) {
		if (t[i] == 0x0d)
			t[i] = 0;
	}
#endif

	return 0;
}


static void fix_effect(struct xmp_event *e)
{
	switch (e->fxt) {
	case 0x00:			/* 00 xy Normal play or Arpeggio */
		e->fxt = FX_ARPEGGIO;
		/* x: first halfnote to add
		   y: second halftone to subtract */
		break;
	case 0x01:			/* 01 xx Slide Pitch Up (until Amis Max), Frequency+InfoByte*64*/
	case 0x05:			/* 05 xx Slide Pitch Up (no limit), Frequency+InfoByte*16 */
		e->fxt = FX_PORTA_UP;
		break;
	case 0x02:			/* 02 xx Slide Pitch Down (until Amis Min), Frequency-InfoByte*64*/
	case 0x06:			/* 06 xx Slide Pitch Down (0 limit),  Frequency-InfoByte*16 */
		e->fxt = FX_PORTA_DN;
		break;
	case 0x03:			/* 03 xx Fine Volume Up */
		e->fxt = FX_F_VSLIDE_UP;
		break;
	case 0x04:			/* 04 xx Fine Volume Down */
		e->fxt = FX_F_VSLIDE_DN;
		break;
	case 0x07:			/* 07 xy Set Stereo Position */
		/* y: stereo position (1-7,ignored). 1=left 4=center 7=right */
		if (e->fxp>0 && e->fxp<8) {
			e->fxt = FX_SETPAN;
			e->fxp = 42*e->fxp-40;
		} else
			e->fxt = e->fxp = 0;
		break;
	case 0x08:			/* 08 xx Start Auto Fine Volume Up */
	case 0x09:			/* 09 xx Start Auto Fine Volume Down */
	case 0x0a:			/* 0A xx Start Auto Pitch Up */
	case 0x0b:			/* 0B xx Start Auto Pitch Down */
		e->fxt = e->fxp = 0; /* FIXME */
		break;
	case 0x0c:			/* 0C xx Set Volume */
		e->fxt = FX_VOLSET;
		e->fxp = 0xff - e->fxp;
		break;
	case 0x0d:			/* 0D xy Pattern Break */
		e->fxt = FX_BREAK;
		break;
	case 0x0e:			/* 0E xx Position Jump */
		e->fxt = FX_JUMP;
		break;
	case 0x0f:			/* 0F xx Set Speed */
		e->fxt = FX_SPEED;
		break;
	case 0x10:			/* 10 xx Unused */
		e->fxt = e->fxp = 0;
		break;
	case 0x11:			/* 11 xx Fine Slide Pitch Up */
	case 0x12:			/* 12 xx Fine Slide Pitch Down */
		e->fxt = e->fxp = 0; /* FIXME */
		break;
	case 0x13:			/* 13 xx Volume Up */
		e->fxt = FX_VOLSLIDE_UP;
		break;
	case 0x14:			/* 14 xx Volume Down */
		e->fxt = FX_VOLSLIDE_DN;
		break;
	default:
		e->fxt = e->fxp = 0;
	}
}

static int coco_load(struct module_data *m, HIO_HANDLE *f, const int start)
{
	struct xmp_module *mod = &m->mod;
	struct xmp_event *event;
	int i, j, k;
	int seq_ptr, pat_ptr, smp_ptr[100];

	LOAD_INIT();

	mod->chn = hio_read8(f) & 0x3f;
	libxmp_read_title(f, mod->name, 20);

	for (i = 0; i < 20; i++) {
		if (mod->name[i] == 0x0d)
			mod->name[i] = 0;
	}

	libxmp_set_type(m, "Coconizer");

	mod->ins = mod->smp = hio_read8(f);
	mod->len = hio_read8(f);
	mod->pat = hio_read8(f);
	mod->trk = mod->pat * mod->chn;

	seq_ptr = hio_read32l(f);
	pat_ptr = hio_read32l(f);

	if (hio_error(f)) {
		return -1;
	}

	MODULE_INFO();

	if (libxmp_init_instrument(m) < 0)
		return -1;

	m->vol_table = libxmp_arch_vol_table;
	m->volbase = 0xff;

	for (i = 0; i < mod->ins; i++) {
		if (libxmp_alloc_subinstrument(mod, i, 1) < 0)
			return -1;

		smp_ptr[i] = hio_read32l(f);
		mod->xxs[i].len = hio_read32l(f);
		mod->xxi[i].sub[0].vol = 0xff - hio_read32l(f);
		mod->xxi[i].sub[0].pan = 0x80;
		mod->xxs[i].lps = hio_read32l(f);
		mod->xxs[i].lpe = mod->xxs[i].lps + hio_read32l(f);
		if (mod->xxs[i].lpe)
			mod->xxs[i].lpe -= 1;
		mod->xxs[i].flg = mod->xxs[i].lps > 0 ?  XMP_SAMPLE_LOOP : 0;
		hio_read(mod->xxi[i].name, 1, 11, f);
		for (j = 0; j < 11; j++) {
			if (mod->xxi[i].name[j] == 0x0d)
				mod->xxi[i].name[j] = 0;
		}
		hio_read8(f);	/* unused */
		mod->xxi[i].sub[0].sid = i;

		if (mod->xxs[i].len > 0)
			mod->xxi[i].nsm = 1;

		if (hio_error(f)) {
			return -1;
		}

		D_(D_INFO "[%2X] %-10.10s  %05x %05x %05x %c V%02x",
				i, mod->xxi[i].name,
				mod->xxs[i].len, mod->xxs[i].lps, mod->xxs[i].lpe,
				mod->xxs[i].flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
				mod->xxi[i].sub[0].vol);
	}

	/* Sequence */

	hio_seek(f, start + seq_ptr, SEEK_SET);
	for (i = 0; ; i++) {
		uint8 x = hio_read8(f);
		if (x == 0xff)
			break;
		if (i < mod->len)
			mod->xxo[i] = x;
	}

	/* Patterns */

	if (libxmp_init_pattern(mod) < 0)
		return -1;

	D_(D_INFO "Stored patterns: %d", mod->pat);

	if (hio_seek(f, start + pat_ptr, SEEK_SET) < 0)
		return -1;

	for (i = 0; i < mod->pat; i++) {
		if (libxmp_alloc_pattern_tracks(mod, i, 64) < 0)
			return -1;

		for (j = 0; j < 64; j++) {
			for (k = 0; k < mod->chn; k++) {
				event = &EVENT(i, k, j);
				event->fxp = hio_read8(f);
				event->fxt = hio_read8(f);
				event->ins = hio_read8(f);
				event->note = hio_read8(f);
				if (event->note)
					event->note += 12;

				if (hio_error(f)) {
					return -1;
				}

				fix_effect(event);
			}
		}
	}

	/* Read samples */

	D_(D_INFO "Stored samples : %d", mod->smp);

	for (i = 0; i < mod->ins; i++) {
		if (mod->xxi[i].nsm == 0)
			continue;

		hio_seek(f, start + smp_ptr[i], SEEK_SET);
		if (libxmp_load_sample(m, f, SAMPLE_FLAG_VIDC, &mod->xxs[i], NULL) < 0)
			return -1;
	}

	for (i = 0; i < mod->chn; i++) {
		mod->xxc[i].pan = DEFPAN((((i + 3) / 2) % 2) * 0xff);
	}

	return 0;
}
