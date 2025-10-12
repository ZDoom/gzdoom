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

#include "common.h"
#include "period.h"
#include "player.h"
#include "hio.h"
#include "loaders/loader.h"


struct xmp_instrument *libxmp_get_instrument(struct context_data *ctx, int ins)
{
	struct smix_data *smix = &ctx->smix;
	struct module_data *m = &ctx->m;
	struct xmp_module *mod = &m->mod;
	struct xmp_instrument *xxi;

	if (ins < 0) {
		xxi = NULL;
	} else if (ins < mod->ins) {
		xxi = &mod->xxi[ins];
	} else if (ins < mod->ins + smix->ins) {
		xxi = &smix->xxi[ins - mod->ins];
	} else {
		xxi = NULL;
	}

	return xxi;
}

struct xmp_sample *libxmp_get_sample(struct context_data *ctx, int smp)
{
	struct smix_data *smix = &ctx->smix;
	struct module_data *m = &ctx->m;
	struct xmp_module *mod = &m->mod;
	struct xmp_sample *xxs;

	if (smp < 0) {
		xxs = NULL;
	} else if (smp < mod->smp) {
		xxs = &mod->xxs[smp];
	} else if (smp < mod->smp + smix->smp) {
		xxs = &smix->xxs[smp - mod->smp];
	} else {
		xxs = NULL;
	}

	return xxs;
}

int xmp_start_smix(xmp_context opaque, int chn, int smp)
{
	struct context_data *ctx = (struct context_data *)opaque;
	struct smix_data *smix = &ctx->smix;

	if (ctx->state > XMP_STATE_LOADED) {
		return -XMP_ERROR_STATE;
	}

	smix->xxi = (struct xmp_instrument *) calloc(smp, sizeof(struct xmp_instrument));
	if (smix->xxi == NULL) {
		goto err;
	}
	smix->xxs = (struct xmp_sample *) calloc(smp, sizeof(struct xmp_sample));
	if (smix->xxs == NULL) {
		goto err1;
	}

	smix->chn = chn;
	smix->ins = smix->smp = smp;

	return 0;

    err1:
	free(smix->xxi);
	smix->xxi = NULL;
    err:
	return -XMP_ERROR_INTERNAL;
}

int xmp_smix_play_instrument(xmp_context opaque, int ins, int note, int vol, int chn)
{
	struct context_data *ctx = (struct context_data *)opaque;
	struct player_data *p = &ctx->p;
	struct smix_data *smix = &ctx->smix;
	struct module_data *m = &ctx->m;
	struct xmp_module *mod = &m->mod;
	struct xmp_event *event;

	if (ctx->state < XMP_STATE_PLAYING) {
		return -XMP_ERROR_STATE;
	}

	if (chn >= smix->chn || chn < 0 || ins >= mod->ins || ins < 0) {
		return -XMP_ERROR_INVALID;
	}

	if (note == 0) {
		note = 60;		/* middle C note number */
	}

	event = &p->inject_event[mod->chn + chn];
	memset(event, 0, sizeof (struct xmp_event));
	event->note = (note < XMP_MAX_KEYS) ? note + 1 : note;
	event->ins = ins + 1;
	event->vol = vol + 1;
	event->_flag = 1;

	return 0;
}

int xmp_smix_play_sample(xmp_context opaque, int ins, int note, int vol, int chn)
{
	struct context_data *ctx = (struct context_data *)opaque;
	struct player_data *p = &ctx->p;
	struct smix_data *smix = &ctx->smix;
	struct module_data *m = &ctx->m;
	struct xmp_module *mod = &m->mod;
	struct xmp_event *event;

	if (ctx->state < XMP_STATE_PLAYING) {
		return -XMP_ERROR_STATE;
	}

	if (chn >= smix->chn || chn < 0 || ins >= smix->ins || ins < 0) {
		return -XMP_ERROR_INVALID;
	}

	if (note == 0) {
		note = 60;		/* middle C note number */
	}

	event = &p->inject_event[mod->chn + chn];
	memset(event, 0, sizeof (struct xmp_event));
	event->note = (note < XMP_MAX_KEYS) ? note + 1 : note;
	event->ins = mod->ins + ins + 1;
	event->vol = vol + 1;
	event->_flag = 1;

	return 0;
}

int xmp_smix_channel_pan(xmp_context opaque, int chn, int pan)
{
	struct context_data *ctx = (struct context_data *)opaque;
	struct player_data *p = &ctx->p;
	struct smix_data *smix = &ctx->smix;
	struct module_data *m = &ctx->m;
	struct channel_data *xc;

	if (chn >= smix->chn || pan < 0 || pan > 255) {
		return -XMP_ERROR_INVALID;
	}

	xc = &p->xc_data[m->mod.chn + chn];
	xc->pan.val = pan;

	return 0;
}

int xmp_smix_load_sample(xmp_context opaque, int num, const char *path)
{
	struct context_data *ctx = (struct context_data *)opaque;
	struct smix_data *smix = &ctx->smix;
	struct module_data *m = &ctx->m;
	struct xmp_instrument *xxi;
	struct xmp_sample *xxs;
	HIO_HANDLE *h;
	uint32 magic;
	int chn, rate, bits, size;
	int retval = -XMP_ERROR_INTERNAL;

	if (num >= smix->ins) {
		retval = -XMP_ERROR_INVALID;
		goto err;
	}

	xxi = &smix->xxi[num];
	xxs = &smix->xxs[num];

	h = hio_open(path, "rb");
	if (h == NULL) {
		retval = -XMP_ERROR_SYSTEM;
		goto err;
	}

	/* Init instrument */

	xxi->sub = (struct xmp_subinstrument *) calloc(1, sizeof(struct xmp_subinstrument));
	if (xxi->sub == NULL) {
		retval = -XMP_ERROR_SYSTEM;
		goto err1;
	}

	xxi->vol = m->volbase;
	xxi->nsm = 1;
	xxi->sub[0].sid = num;
	xxi->sub[0].vol = xxi->vol;
	xxi->sub[0].pan = 0x80;

	/* Load sample */

	magic = hio_read32b(h);
	if (magic != 0x52494646) {	/* RIFF */
		retval = -XMP_ERROR_FORMAT;
		goto err2;
	}

	if (hio_seek(h, 22, SEEK_SET) < 0) {
		retval = -XMP_ERROR_SYSTEM;
		goto err2;
	}
	chn = hio_read16l(h);
	if (chn != 1) {
		retval = -XMP_ERROR_FORMAT;
		goto err2;
	}

	rate = hio_read32l(h);
	if (rate == 0) {
		retval = -XMP_ERROR_FORMAT;
		goto err2;
	}

	if (hio_seek(h, 34, SEEK_SET) < 0) {
		retval = -XMP_ERROR_SYSTEM;
		goto err2;
	}
	bits = hio_read16l(h);
	if (bits == 0) {
		retval = -XMP_ERROR_FORMAT;
		goto err2;
	}

	if (hio_seek(h, 40, SEEK_SET) < 0) {
		retval = -XMP_ERROR_SYSTEM;
		goto err2;
	}
	size = hio_read32l(h);
	if (size == 0) {
		retval = -XMP_ERROR_FORMAT;
		goto err2;
	}

	libxmp_c2spd_to_note(rate, &xxi->sub[0].xpo, &xxi->sub[0].fin);

	xxs->len = 8 * size / bits;
	xxs->lps = 0;
	xxs->lpe = 0;
	xxs->flg = bits == 16 ? XMP_SAMPLE_16BIT : 0;

	xxs->data = (unsigned char *) malloc(size + 8);
	if (xxs->data == NULL) {
		retval = -XMP_ERROR_SYSTEM;
		goto err2;
	}

	/* ugly hack to make the interpolator happy */
	memset(xxs->data, 0, 4);
	memset(xxs->data + 4 + size, 0, 4);
	xxs->data += 4;

	if (hio_seek(h, 44, SEEK_SET) < 0) {
		retval = -XMP_ERROR_SYSTEM;
		goto err2;
	}
	if (hio_read(xxs->data, 1, size, h) != size) {
		retval = -XMP_ERROR_SYSTEM;
		goto err2;
	}
	hio_close(h);

	return 0;

    err2:
	free(xxi->sub);
	xxi->sub = NULL;
    err1:
	hio_close(h);
    err:
	return retval;
}

int xmp_smix_release_sample(xmp_context opaque, int num)
{
	struct context_data *ctx = (struct context_data *)opaque;
	struct smix_data *smix = &ctx->smix;

	if (num >= smix->ins) {
		return -XMP_ERROR_INVALID;
	}

	libxmp_free_sample(&smix->xxs[num]);
	free(smix->xxi[num].sub);

	smix->xxs[num].data = NULL;
	smix->xxi[num].sub = NULL;

	return 0;
}

void xmp_end_smix(xmp_context opaque)
{
	struct context_data *ctx = (struct context_data *)opaque;
	struct smix_data *smix = &ctx->smix;
	int i;

	for (i = 0; i < smix->smp; i++) {
		xmp_smix_release_sample(opaque, i);
	}

	free(smix->xxs);
	free(smix->xxi);
	smix->xxs = NULL;
	smix->xxi = NULL;
}
