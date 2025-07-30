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

/* Based on the format description written by Harald Zappe.
 * Additional information about Oktalyzer modules from Bernardo
 * Innocenti's XModule 3.4 sources.
 */

#include "loader.h"
#include "iff.h"

static int okt_test(HIO_HANDLE *, char *, const int);
static int okt_load(struct module_data *, HIO_HANDLE *, const int);

const struct format_loader libxmp_loader_okt = {
	"Oktalyzer",
	okt_test,
	okt_load
};

static int okt_test(HIO_HANDLE *f, char *t, const int start)
{
	char magic[8];

	if (hio_read(magic, 1, 8, f) < 8)
		return -1;

	if (strncmp(magic, "OKTASONG", 8))
		return -1;

	libxmp_read_title(f, t, 0);

	return 0;
}

#define OKT_MODE8 0x00		/* 7 bit samples */
#define OKT_MODE4 0x01		/* 8 bit samples */
#define OKT_MODEB 0x02		/* Both */

#define NONE 0xff

struct local_data {
	int mode[36];
	int idx[36];
	int pattern;
	int sample;
	int samples;
	int has_cmod;
	int has_samp;
	int has_slen;
};

static const int fx[32] = {
	NONE,
	FX_PORTA_UP,		/*  1 */
	FX_PORTA_DN,		/*  2 */
	NONE,
	NONE,
	NONE,
	NONE,
	NONE,
	NONE,
	NONE,
	FX_OKT_ARP3,		/* 10 */
	FX_OKT_ARP4,		/* 11 */
	FX_OKT_ARP5,		/* 12 */
	FX_NSLIDE2_DN,		/* 13 */
	NONE,
	NONE,			/* 15 - filter */
	NONE,
	FX_NSLIDE2_UP,		/* 17 */
	NONE,
	NONE,
	NONE,
	FX_NSLIDE_DN,		/* 21 */
	NONE,
	NONE,
	NONE,
	FX_JUMP,		/* 25 */
	NONE,
	NONE,			/* 27 - release */
	FX_SPEED,		/* 28 */
	NONE,
	FX_NSLIDE_UP,		/* 30 */
	FX_VOLSET		/* 31 */
};

static int get_cmod(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	struct local_data *data = (struct local_data *)parm;
	int i;

	/* Sanity check */
	if (data->has_cmod || size < 8) {
		return -1;
	}
	data->has_cmod = 1;

	mod->chn = 0;
	for (i = 0; i < 4; i++) {
		int pan = (((i + 1) / 2) % 2) * 0xff;
		int p = 0x80 + (pan - 0x80) * m->defpan / 100;

		if (hio_read16b(f) == 0) {
			mod->xxc[mod->chn++].pan = p;
		} else {
			mod->xxc[mod->chn].flg |= XMP_CHANNEL_SPLIT | (i << 4);
			mod->xxc[mod->chn++].pan = p;
			mod->xxc[mod->chn].flg |= XMP_CHANNEL_SPLIT | (i << 4);
			mod->xxc[mod->chn++].pan = p;
		}

	}

	return 0;
}

static int get_samp(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	struct local_data *data = (struct local_data *)parm;
	int i, j;
	int looplen;

	/* Sanity check */
	if (data->has_samp || size != 36 * 32) {
		return -1;
	}
	data->has_samp = 1;

	/* Should be always 36 */
	mod->ins = size / 32;	/* sizeof(struct okt_instrument_header); */
	mod->smp = mod->ins;

	if (libxmp_init_instrument(m) < 0)
		return -1;

	for (j = i = 0; i < mod->ins; i++) {
		struct xmp_instrument *xxi = &mod->xxi[i];
		struct xmp_sample *xxs = &mod->xxs[j];
		struct xmp_subinstrument *sub;

		if (libxmp_alloc_subinstrument(mod, i, 1) < 0)
			return -1;

		sub = &xxi->sub[0];

		hio_read(xxi->name, 1, 20, f);

		/* Sample size is always rounded down */
		xxs->len = hio_read32b(f) & ~1;
		xxs->lps = hio_read16b(f) << 1;
		looplen = hio_read16b(f) << 1;
		xxs->lpe = xxs->lps + looplen;
		xxs->flg = looplen > 2 ? XMP_SAMPLE_LOOP : 0;

		sub->vol = hio_read16b(f);
		data->mode[i] = hio_read16b(f);

		sub->pan = 0x80;
		sub->sid = j;

		data->idx[j] = i;

		if (xxs->len > 0) {
			xxi->nsm = 1;
			j++;
		}
	}
	data->samples = j;

	return 0;
}

static int get_spee(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;

	mod->spd = hio_read16b(f);
	mod->bpm = 125;

	return 0;
}

static int get_slen(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	struct local_data *data = (struct local_data *)parm;

	/* Sanity check */
	if (data->has_slen || !data->has_cmod || size < 2) {
		return -1;
	}
	data->has_slen = 1;

	mod->pat = hio_read16b(f);
	mod->trk = mod->pat * mod->chn;

	return 0;
}

static int get_plen(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;

	mod->len = hio_read16b(f);

	/* Sanity check */
	if (mod->len > 256)
		return -1;

	D_(D_INFO "Module length: %d", mod->len);

	return 0;
}

static int get_patt(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;

	if (hio_read(mod->xxo, 1, mod->len, f) != mod->len)
		return -1;

	return 0;
}

static int get_pbod(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	struct local_data *data = (struct local_data *)parm;
	struct xmp_event *e;
	uint16 rows;
	int j;

	/* Sanity check */
	if (!data->has_slen || !data->has_cmod) {
		return -1;
	}

	if (data->pattern >= mod->pat)
		return 0;

	if (!data->pattern) {
		if (libxmp_init_pattern(mod) < 0)
			return -1;
		D_(D_INFO "Stored patterns: %d", mod->pat);
	}

	rows = hio_read16b(f);

	if (libxmp_alloc_pattern_tracks(mod, data->pattern, rows) < 0)
		return -1;

	for (j = 0; j < rows * mod->chn; j++) {
		uint8 note, ins, fxt;

		e = &EVENT(data->pattern, j % mod->chn, j / mod->chn);
		memset(e, 0, sizeof(struct xmp_event));

		note = hio_read8(f);
		ins = hio_read8(f);

		if (note) {
			e->note = 48 + note;
			e->ins = 1 + ins;
		}

		fxt = hio_read8(f);
		if (fxt >= ARRAY_SIZE(fx)) {
			return -1;
		}
		e->fxt = fx[fxt];
		e->fxp = hio_read8(f);

		if ((e->fxt == FX_VOLSET) && (e->fxp > 0x40)) {
			if (e->fxp <= 0x50) {
				e->fxt = FX_VOLSLIDE;
				e->fxp -= 0x40;
			} else if (e->fxp <= 0x60) {
				e->fxt = FX_VOLSLIDE;
				e->fxp = (e->fxp - 0x50) << 4;
			} else if (e->fxp <= 0x70) {
				e->fxt = FX_F_VSLIDE_DN;
				e->fxp = e->fxp - 0x60;
			} else if (e->fxp <= 0x80) {
				e->fxt = FX_F_VSLIDE_UP;
				e->fxp = e->fxp - 0x70;
			}
		}
		if (e->fxt == FX_ARPEGGIO)	/* Arpeggio fixup */
			e->fxp = (((24 - MSN(e->fxp)) % 12) << 4) | LSN(e->fxp);
		if (e->fxt == NONE)
			e->fxt = e->fxp = 0;
	}
	data->pattern++;

	return 0;
}

static int get_sbod(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	struct local_data *data = (struct local_data *)parm;
	int flags = 0;
	int i, sid;

	if (data->sample >= data->samples)
		return 0;

	D_(D_INFO "Stored samples: %d", mod->smp);

	i = data->idx[data->sample];
	if (data->mode[i] == OKT_MODE8 || data->mode[i] == OKT_MODEB)
		flags = SAMPLE_FLAG_7BIT;

	sid = mod->xxi[i].sub[0].sid;
	if (libxmp_load_sample(m, f, flags, &mod->xxs[sid], NULL) < 0)
		return -1;

	data->sample++;

	return 0;
}

static int okt_load(struct module_data *m, HIO_HANDLE * f, const int start)
{
	iff_handle handle;
	struct local_data data;
	int ret;

	LOAD_INIT();

	hio_seek(f, 8, SEEK_CUR);	/* OKTASONG */

	handle = libxmp_iff_new();
	if (handle == NULL)
		return -1;

	memset(&data, 0, sizeof(struct local_data));

	/* IFF chunk IDs */
	ret = libxmp_iff_register(handle, "CMOD", get_cmod);
	ret |= libxmp_iff_register(handle, "SAMP", get_samp);
	ret |= libxmp_iff_register(handle, "SPEE", get_spee);
	ret |= libxmp_iff_register(handle, "SLEN", get_slen);
	ret |= libxmp_iff_register(handle, "PLEN", get_plen);
	ret |= libxmp_iff_register(handle, "PATT", get_patt);
	ret |= libxmp_iff_register(handle, "PBOD", get_pbod);
	ret |= libxmp_iff_register(handle, "SBOD", get_sbod);

	if (ret != 0)
		return -1;

	libxmp_set_type(m, "Oktalyzer");

	MODULE_INFO();

	/* Load IFF chunks */
	if (libxmp_iff_load(handle, m, f, &data) < 0) {
		libxmp_iff_release(handle);
		return -1;
	}

	libxmp_iff_release(handle);

	m->period_type = PERIOD_MODRNG;

	return 0;
}
