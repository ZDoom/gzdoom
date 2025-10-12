/* Extended Module Player
 * Copyright (C) 1996-2024 Claudio Matsuoka and Hipolito Carraro Jr
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
#include "iff.h"

#define MAGIC_MUSX	MAGIC4('M','U','S','X')
#define MAGIC_MNAM	MAGIC4('M','N','A','M')
#define MAGIC_SNAM	MAGIC4('S','N','A','M')
#define MAGIC_SVOL	MAGIC4('S','V','O','L')
#define MAGIC_SLEN	MAGIC4('S','L','E','N')
#define MAGIC_ROFS	MAGIC4('R','O','F','S')
#define MAGIC_RLEN	MAGIC4('R','L','E','N')
#define MAGIC_SDAT	MAGIC4('S','D','A','T')


static int arch_test (HIO_HANDLE *, char *, const int);
static int arch_load (struct module_data *, HIO_HANDLE *, const int);


const struct format_loader libxmp_loader_arch = {
	"Archimedes Tracker",
	arch_test,
	arch_load
};

/*
 * Linear (0 to 0x40) to logarithmic volume conversion.
 * This is only used for the Protracker-compatible "linear volume" effect in
 * Andy Southgate's StasisMod.  In this implementation linear and logarithmic
 * volumes can be freely intermixed.
 */
static const uint8 lin_table[65]={
	0x00, 0x48, 0x64, 0x74, 0x82, 0x8a, 0x92, 0x9a,
	0xa2, 0xa6, 0xaa, 0xae, 0xb2, 0xb6, 0xea, 0xbe,
	0xc2, 0xc4, 0xc6, 0xc8, 0xca, 0xcc, 0xce, 0xd0,
	0xd2, 0xd4, 0xd6, 0xd8, 0xda, 0xdc, 0xde, 0xe0,
	0xe2, 0xe2, 0xe4, 0xe4, 0xe6, 0xe6, 0xe8, 0xe8,
	0xea, 0xea, 0xec, 0xec, 0xee, 0xee, 0xf0, 0xf0,
	0xf2, 0xf2, 0xf4, 0xf4, 0xf6, 0xf6, 0xf8, 0xf8,
	0xfa, 0xfa, 0xfc, 0xfc, 0xfe, 0xfe, 0xfe, 0xfe,
	0xfe
};

#if 0
static uint8 convert_vol(uint8 vol) {
/*	return pow(2,6.0-(255.0-vol)/32)+.5; */
	return vol_table[vol];
}
#endif

static int arch_test(HIO_HANDLE *f, char *t, const int start)
{
	if (hio_read32b(f) != MAGIC_MUSX) {
		return -1;
	}

	hio_read32l(f);

	while (!hio_eof(f)) {
		uint32 id = hio_read32b(f);
		uint32 len = hio_read32l(f);

		/* Sanity check */
		if (len > 0x100000) {
			return -1;
		}

		if (id == MAGIC_MNAM) {
			libxmp_read_title(f, t, 32);
			return 0;
		}

		hio_seek(f, len, SEEK_CUR);
	}

	libxmp_read_title(f, t, 0);

	return 0;
}


struct local_data {
    int year, month, day;
    int pflag, sflag, max_ins, max_pat;
    int has_mvox;
    int has_pnum;
    uint8 ster[8], rows[64];
};

static void fix_effect(struct xmp_event *e)
{
#if 0
	/* for debugging */
	printf ("%c%02x ", e->fxt["0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"], e->fxp);
#endif
	switch (e->fxt) {
	case 0x00:			/* 00 xy Normal play or Arpeggio */
		e->fxt = FX_ARPEGGIO;
		/* x: first halfnote to add
		   y: second halftone to subtract */
		break;
	case 0x01:			/* 01 xx Slide Up */
		e->fxt = FX_PORTA_UP;
		break;
	case 0x02:			/* 02 xx Slide Down */
		e->fxt = FX_PORTA_DN;
		break;
	case 0x03:			/* 03 xx Tone Portamento */
		e->fxt = FX_TONEPORTA;
		break;
	case 0x0b:			/* 0B xx Break Pattern */
		e->fxt = FX_BREAK;
		break;
	case 0x0c:
		/* Set linear volume */
		if (e->fxp <= 64) {
			e->fxt = FX_VOLSET;
			e->fxp = lin_table[e->fxp];
		} else {
			e->fxp = e->fxt = 0;
		}
		break;
	case 0x0e:			/* 0E xy Set Stereo */
	case 0x19: /* StasisMod's non-standard set panning effect */
		/* y: stereo position (1-7,ignored). 1=left 4=center 7=right */
		if (e->fxp>0 && e->fxp<8) {
			e->fxt = FX_SETPAN;
			e->fxp = 42*e->fxp-40;
		} else
			e->fxt = e->fxp = 0;
		break;
	case 0x10:			/* 10 xx Volume Slide Up */
		e->fxt = FX_VOLSLIDE_UP;
		break;
	case 0x11:			/* 11 xx Volume Slide Down */
		e->fxt = FX_VOLSLIDE_DN;
		break;
	case 0x13:			/* 13 xx Position Jump */
		e->fxt = FX_JUMP;
		break;
	case 0x15:			/* 15 xy Line Jump. (not in manual) */
		/* Jump to line 10*x+y in same pattern. (10*x+y>63 ignored) */
		if (MSN(e->fxp) * 10 + LSN(e->fxp) < 64) {
			e->fxt = FX_LINE_JUMP;
			e->fxp = MSN(e->fxp) * 10 + LSN(e->fxp);
		} else {
			e->fxt = e->fxp = 0;
		}
		break;
	case 0x1c:			/* 1C xy Set Speed */
		e->fxt = FX_SPEED;
		break;
	case 0x1f:			/* 1F xx Set Volume */
		e->fxt = FX_VOLSET;
		/* all volumes are logarithmic */
		/* e->fxp = convert_vol (e->fxp); */
		break;
	default:
		e->fxt = e->fxp = 0;
	}
}

static int get_tinf(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	struct local_data *data = (struct local_data *)parm;
	int x;

	x = hio_read8(f);
	data->year = ((x & 0xf0) >> 4) * 10 + (x & 0x0f);
	x = hio_read8(f);
	data->year += ((x & 0xf0) >> 4) * 1000 + (x & 0x0f) * 100;

	x = hio_read8(f);
	data->month = ((x & 0xf0) >> 4) * 10 + (x & 0x0f);

	x = hio_read8(f);
	data->day = ((x & 0xf0) >> 4) * 10 + (x & 0x0f);

	return 0;
}

static int get_mvox(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	struct local_data *data = (struct local_data *)parm;
	uint32 chn;

	chn = hio_read32l(f);

	/* Sanity check */
	if (chn < 1 || chn > 8 || data->has_mvox) {
		return -1;
	}

	mod->chn = chn;
	data->has_mvox = 1;
	return 0;
}

static int get_ster(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	struct local_data *data = (struct local_data *)parm;
	int i;

	if (hio_read(data->ster, 1, 8, f) != 8) {
		return -1;
	}

	for (i = 0; i < mod->chn; i++) {
		if (data->ster[i] > 0 && data->ster[i] < 8) {
			mod->xxc[i].pan = 42 * data->ster[i] - 40;
		}
	}

	return 0;
}

static int get_mnam(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;

	if (hio_read(mod->name, 1, 32, f) != 32)
		return -1;

	return 0;
}

static int get_anam(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	/*hio_read(m->author, 1, 32, f); */

	return 0;
}

static int get_mlen(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	uint32 len;

	len = hio_read32l(f);

	/* Sanity check */
	if (len > 0xff)
		return -1;

	mod->len = len;
	return 0;
}

static int get_pnum(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	struct local_data *data = (struct local_data *)parm;
	uint32 pat;

	pat = hio_read32l(f);

	/* Sanity check */
	if (pat < 1 || pat > 64 || data->has_pnum)
		return -1;

	mod->pat = pat;
	data->has_pnum = 1;
	return 0;
}

static int get_plen(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	struct local_data *data = (struct local_data *)parm;

	if (hio_read(data->rows, 1, 64, f) != 64)
		return -1;

	return 0;
}

static int get_sequ(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;

	hio_read(mod->xxo, 1, 128, f);
	libxmp_set_type(m, "Archimedes Tracker");
	MODULE_INFO();

	return 0;
}

static int get_patt(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	struct local_data *data = (struct local_data *)parm;
	int i, j, k;
	struct xmp_event *event;

	/* Sanity check */
	if (!data->has_mvox || !data->has_pnum) {
		return -1;
	}

	if (!data->pflag) {
		D_(D_INFO "Stored patterns: %d", mod->pat);
		data->pflag = 1;
		data->max_pat = 0;
		mod->trk = mod->pat * mod->chn;

		if (libxmp_init_pattern(mod) < 0)
			return -1;
	}

	/* Sanity check */
	if (data->max_pat >= mod->pat || data->max_pat >= 64)
		return -1;

        i = data->max_pat;

	if (libxmp_alloc_pattern_tracks(mod, i, data->rows[i]) < 0)
		return -1;

	for (j = 0; j < data->rows[i]; j++) {
		for (k = 0; k < mod->chn; k++) {
			event = &EVENT(i, k, j);

			event->fxp = hio_read8(f);
			event->fxt = hio_read8(f);
			event->ins = hio_read8(f);
			event->note = hio_read8(f);

			if (event->note)
				event->note += 48;

			fix_effect(event);
		}
	}

	data->max_pat++;

	return 0;
}

static int get_samp(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	struct local_data *data = (struct local_data *)parm;
	int i;

	if (!data->sflag) {
		mod->smp = mod->ins = 36;
		if (libxmp_init_instrument(m) < 0)
			return -1;

		D_(D_INFO "Instruments: %d", mod->ins);

		data->sflag = 1;
		data->max_ins = 0;
	}

	/* FIXME: More than 36 sample slots used.  Unfortunately we
	 * have no way to handle this without two passes, and it's
	 * officially supposed to be 36, so ignore the rest.
	 */
	if (data->max_ins >= 36)
		return 0;

	i = data->max_ins;

	mod->xxi[i].nsm = 1;
	if (libxmp_alloc_subinstrument(mod, i, 1) < 0)
		return -1;

	if (hio_read32b(f) != MAGIC_SNAM)	/* SNAM */
		return -1;

	{
		/* should usually be 0x14 but zero is not unknown */
		int name_len = hio_read32l(f);

		/* Sanity check */
		if (name_len < 0 || name_len > 32)
			return -1;

		hio_read(mod->xxi[i].name, 1, name_len, f);
	}

	if (hio_read32b(f) != MAGIC_SVOL)	/* SVOL */
		return -1;
	hio_read32l(f);
	/* mod->xxi[i].sub[0].vol = convert_vol(hio_read32l(f)); */
	mod->xxi[i].sub[0].vol = hio_read32l(f) & 0xff;

	if (hio_read32b(f) != MAGIC_SLEN)	/* SLEN */
		return -1;
	hio_read32l(f);
	mod->xxs[i].len = hio_read32l(f);

	if (hio_read32b(f) != MAGIC_ROFS)	/* ROFS */
		return -1;
	hio_read32l(f);
	mod->xxs[i].lps = hio_read32l(f);

	if (hio_read32b(f) != MAGIC_RLEN)	/* RLEN */
		return -1;
	hio_read32l(f);
	mod->xxs[i].lpe = hio_read32l(f);

	if (hio_read32b(f) != MAGIC_SDAT)	/* SDAT */
		return -1;
	hio_read32l(f);
	hio_read32l(f);	/* 0x00000000 */

	mod->xxi[i].sub[0].sid = i;
	mod->xxi[i].sub[0].pan = 0x80;

	m->vol_table = libxmp_arch_vol_table;
	m->volbase = 0xff;

	/* Clean bad loops */
	if (mod->xxs[i].lps < 0 || mod->xxs[i].lps >= mod->xxs[i].len) {
		mod->xxs[i].lps = mod->xxs[i].lpe = 0;
	}

	if (mod->xxs[i].lpe > 2) {
		if (mod->xxs[i].lpe > mod->xxs[i].len - mod->xxs[i].lps) {
			mod->xxs[i].lpe = mod->xxs[i].len - mod->xxs[i].lps;
		}
		mod->xxs[i].flg = XMP_SAMPLE_LOOP;
		mod->xxs[i].lpe = mod->xxs[i].lps + mod->xxs[i].lpe;
	} else if (mod->xxs[i].lpe == 2 && mod->xxs[i].lps > 0) {
		/* non-zero repeat offset and repeat length of 2
		 * means loop to end of sample */
		mod->xxs[i].flg = XMP_SAMPLE_LOOP;
		mod->xxs[i].lpe = mod->xxs[i].len;
	}

	if (libxmp_load_sample(m, f, SAMPLE_FLAG_VIDC, &mod->xxs[i], NULL) < 0)
		return -1;

	D_(D_INFO "[%2X] %-20.20s %05x %05x %05x %c V%02x",
				i, mod->xxi[i].name,
				mod->xxs[i].len,
				mod->xxs[i].lps,
				mod->xxs[i].lpe,
				mod->xxs[i].flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
				mod->xxi[i].sub[0].vol);

	data->max_ins++;

	return 0;
}

static int arch_load(struct module_data *m, HIO_HANDLE *f, const int start)
{
	struct xmp_module *mod = &m->mod;
	iff_handle handle;
	int i;
	struct local_data data;

	LOAD_INIT();

	hio_read32b(f);	/* MUSX */
	hio_read32b(f);

	memset(&data, 0, sizeof(struct local_data));

	handle = libxmp_iff_new();
	if (handle == NULL)
		return -1;

	/* IFF chunk IDs */
	libxmp_iff_register(handle, "TINF", get_tinf);
	libxmp_iff_register(handle, "MVOX", get_mvox);
	libxmp_iff_register(handle, "STER", get_ster);
	libxmp_iff_register(handle, "MNAM", get_mnam);
	libxmp_iff_register(handle, "ANAM", get_anam);
	libxmp_iff_register(handle, "MLEN", get_mlen);
	libxmp_iff_register(handle, "PNUM", get_pnum);
	libxmp_iff_register(handle, "PLEN", get_plen);
	libxmp_iff_register(handle, "SEQU", get_sequ);
	libxmp_iff_register(handle, "PATT", get_patt);
	libxmp_iff_register(handle, "SAMP", get_samp);

	libxmp_iff_set_quirk(handle, IFF_LITTLE_ENDIAN);

	/* Load IFF chunks */
	if (libxmp_iff_load(handle, m, f, &data) < 0) {
		libxmp_iff_release(handle);
		return -1;
	}

	libxmp_iff_release(handle);

	for (i = 0; i < mod->chn; i++) {
		mod->xxc[i].pan = DEFPAN((((i + 3) / 2) % 2) * 0xff);
	}

	return 0;
}

