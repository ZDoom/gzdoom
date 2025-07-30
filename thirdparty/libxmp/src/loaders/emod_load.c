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
#include "iff.h"

#define MAGIC_FORM	MAGIC4('F','O','R','M')
#define MAGIC_EMOD	MAGIC4('E','M','O','D')
#define MAGIC_EMIC	MAGIC4('E','M','I','C')

static int emod_test(HIO_HANDLE *, char *, const int);
static int emod_load(struct module_data *, HIO_HANDLE *, const int);

const struct format_loader libxmp_loader_emod = {
	"Quadra Composer",
	emod_test,
	emod_load
};

static int emod_test(HIO_HANDLE * f, char *t, const int start)
{
	if (hio_read32b(f) != MAGIC_FORM)
		return -1;

	hio_read32b(f);

	if (hio_read32b(f) != MAGIC_EMOD)
		return -1;

	if (hio_read32b(f) == MAGIC_EMIC) {
		hio_read32b(f);	/* skip size */
		hio_read16b(f);	/* skip version */
		libxmp_read_title(f, t, 20);
	} else {
		libxmp_read_title(f, t, 0);
	}

	return 0;
}

struct local_data {
	int has_emic;
	int has_patt;
	int has_8smp;
};

static int get_emic(struct module_data *m, int size, HIO_HANDLE * f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	struct local_data *data = (struct local_data *)parm;
	int i, ver;
	uint8 reorder[256];

	/* Sanity check */
	if (data->has_emic) {
		return -1;
	}
	data->has_emic = 1;

	ver = hio_read16b(f);
	hio_read(mod->name, 1, 20, f);
	hio_seek(f, 20, SEEK_CUR);
	mod->bpm = hio_read8(f);
	mod->ins = hio_read8(f);
	mod->smp = mod->ins;

	m->period_type = PERIOD_MODRNG;

	snprintf(mod->type, XMP_NAME_SIZE, "Quadra Composer EMOD v%d", ver);
	MODULE_INFO();

	if (libxmp_init_instrument(m) < 0)
		return -1;

	for (i = 0; i < mod->ins; i++) {
		struct xmp_instrument *xxi = &mod->xxi[i];
		struct xmp_sample *xxs = &mod->xxs[i];
		struct xmp_subinstrument *sub;
		uint8 name[20];

		if (libxmp_alloc_subinstrument(mod, i, 1) < 0)
			return -1;

		sub = &xxi->sub[0];

		hio_read8(f);	/* num */
		sub->vol = hio_read8(f);
		xxs->len = 2 * hio_read16b(f);
		if (hio_read(name, 1, 20, f) < 20)
			return -1;
		libxmp_instrument_name(mod, i, name, 20);
		xxs->flg = hio_read8(f) & 1 ? XMP_SAMPLE_LOOP : 0;
		sub->fin = hio_read8s(f) << 4;
		xxs->lps = 2 * hio_read16b(f);
		xxs->lpe = xxs->lps + 2 * hio_read16b(f);
		hio_read32b(f);	/* ptr */

		xxi->nsm = 1;
		sub->pan = 0x80;
		sub->sid = i;

		D_(D_INFO "[%2X] %-20.20s %05x %05x %05x %c V%02x %+d",
			i, xxi->name, xxs->len, xxs->lps, xxs->lpe,
			xxs->flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
			sub->vol, sub->fin >> 4);
	}

	hio_read8(f);		/* pad */
	mod->pat = hio_read8(f);
	mod->trk = mod->pat * mod->chn;

	if (libxmp_init_pattern(mod) < 0)
		return -1;

	memset(reorder, 0, sizeof(reorder));

	for (i = 0; i < mod->pat; i++) {
		reorder[hio_read8(f)] = i;

		if (libxmp_alloc_pattern_tracks(mod, i, hio_read8(f) + 1) < 0)
			return -1;

		hio_seek(f, 20, SEEK_CUR);	/* skip name */
		hio_read32b(f);	/* ptr */
	}

	mod->len = hio_read8(f);

	D_(D_INFO "Module length: %d", mod->len);

	for (i = 0; i < mod->len; i++)
		mod->xxo[i] = reorder[hio_read8(f)];

	return 0;
}

static int get_patt(struct module_data *m, int size, HIO_HANDLE * f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	struct local_data *data = (struct local_data *)parm;
	struct xmp_event *event;
	int i, j, k;
	uint8 x;

	/* Sanity check */
	if (data->has_patt || !data->has_emic) {
		return -1;
	}
	data->has_patt = 1;

	D_(D_INFO "Stored patterns: %d", mod->pat);

	for (i = 0; i < mod->pat; i++) {
		for (j = 0; j < mod->xxp[i]->rows; j++) {
			for (k = 0; k < mod->chn; k++) {
				event = &EVENT(i, k, j);
				event->ins = hio_read8(f);
				event->note = hio_read8(f) + 1;
				if (event->note != 0)
					event->note += 48;
				event->fxt = hio_read8(f) & 0x0f;
				event->fxp = hio_read8(f);

				/* Fix effects */
				switch (event->fxt) {
				case 0x04:
					x = event->fxp;
					event->fxp =
					    (x & 0xf0) | ((x << 1) & 0x0f);
					break;
				case 0x09:
					event->fxt <<= 1;
					break;
				case 0x0b:
					x = event->fxt;
					event->fxt = 16 * (x / 10) + x % 10;
					break;
				}
			}
		}
	}

	return 0;
}

static int get_8smp(struct module_data *m, int size, HIO_HANDLE * f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	struct local_data *data = (struct local_data *)parm;
	int i;

	/* Sanity check */
	if (data->has_8smp || !data->has_emic) {
		return -1;
	}
	data->has_8smp = 1;

	D_(D_INFO "Stored samples : %d ", mod->smp);

	for (i = 0; i < mod->smp; i++) {
		if (libxmp_load_sample(m, f, 0, &mod->xxs[i], NULL) < 0)
			return -1;
	}

	return 0;
}

static int emod_load(struct module_data *m, HIO_HANDLE * f, const int start)
{
	iff_handle handle;
	struct local_data data;
	int ret;

	LOAD_INIT();

	memset(&data, 0, sizeof(struct local_data));

	hio_read32b(f);		/* FORM */
	hio_read32b(f);
	hio_read32b(f);		/* EMOD */

	handle = libxmp_iff_new();
	if (handle == NULL)
		return -1;

	/* IFF chunk IDs */
	ret = libxmp_iff_register(handle, "EMIC", get_emic);
	ret |= libxmp_iff_register(handle, "PATT", get_patt);
	ret |= libxmp_iff_register(handle, "8SMP", get_8smp);

	if (ret != 0)
		return -1;

	/* Load IFF chunks */
	if (libxmp_iff_load(handle, m, f, &data) < 0) {
		libxmp_iff_release(handle);
		return -1;
	}

	libxmp_iff_release(handle);

	return 0;
}
