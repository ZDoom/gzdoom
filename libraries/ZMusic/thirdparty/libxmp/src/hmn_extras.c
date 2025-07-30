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

#include "common.h"
#include "player.h"
#include "virtual.h"
#include "effects.h"
#include "hmn_extras.h"

static uint8 megaarp[16][16] = {
	{  0,  3,  7, 12, 15, 12,  7,  3,  0,  3,  7, 12, 15, 12,  7,  3 },
	{  0,  4,  7, 12, 16, 12,  7,  4,  0,  4,  7, 12, 16, 12,  7,  4 },
	{  0,  3,  8, 12, 15, 12,  8,  3,  0,  3,  8, 12, 15, 12,  8,  3 },
	{  0,  4,  8, 12, 16, 12,  8,  4,  0,  4,  8, 12, 16, 12,  8,  4 },
	{  0,  5,  8, 12, 17, 12,  8,  5,  0,  5,  8, 12, 17, 12,  8,  5 },
	{  0,  5,  9, 12, 17, 12,  9,  5,  0,  5,  9, 12, 17, 12,  9,  5 },
	{ 12,  0,  7,  0,  3,  0,  7,  0, 12,  0,  7,  0,  3,  0,  7,  0 },
	{ 12,  0,  7,  0,  4,  0,  7,  0, 12,  0,  7,  0,  4,  0,  7,  0 },

	{  0,  3,  7,  3,  7, 12,  7, 12, 15, 12,  7, 12,  7,  3,  7,  3 },
	{  0,  4,  7,  4,  7, 12,  7, 12, 16, 12,  7, 12,  7,  4,  7,  4 },
	{ 31, 27, 24, 19, 15, 12,  7,  3,  0,  3,  7, 12, 15, 19, 24, 27 },
	{ 31, 28, 24, 19, 16, 12,  7,  4,  0,  4,  7, 12, 16, 19, 24, 28 },
	{  0, 12,  0, 12,  0, 12,  0, 12,  0, 12,  0, 12,  0, 12,  0, 12 },
	{  0, 12, 24, 12,  0, 12, 24, 12,  0, 12, 24, 12,  0, 12, 24, 12 },
	{  0,  3,  0,  3,  0,  3,  0,  3,  0,  3,  0,  3,  0,  3,  0,  3 },
	{  0,  4,  0,  4,  0,  4,  0,  4,  0,  4,  0,  4,  0,  4,  0,  4 }
};


int libxmp_hmn_linear_bend(struct context_data *ctx, struct channel_data *xc)
{
	return 0;
}

void libxmp_hmn_play_extras(struct context_data *ctx, struct channel_data *xc, int chn)
{
	struct player_data *p = &ctx->p;
	struct module_data *m = &ctx->m;
	struct hmn_channel_extras *ce = (struct hmn_channel_extras *)xc->extra;
	struct xmp_instrument *xxi;
	int pos, waveform, volume;

	if (p->frame == 0 && TEST(NEW_NOTE|NEW_INS)) {
		ce->datapos = 0;
	}

	xxi = &m->mod.xxi[xc->ins];
	pos = ce->datapos;
	waveform = HMN_INSTRUMENT_EXTRAS(m->mod.xxi[xc->ins])->data[pos];
	volume = HMN_INSTRUMENT_EXTRAS(m->mod.xxi[xc->ins])->progvolume[pos] & 0x7f;

	if (waveform < xxi->nsm && xxi->sub[waveform].sid != xc->smp) {
		xc->smp = xxi->sub[waveform].sid;
		libxmp_virt_setsmp(ctx, chn, xc->smp);
	}

	pos++;
	if (pos > HMN_INSTRUMENT_EXTRAS(m->mod.xxi[xc->ins])->dataloopend)
		pos = HMN_INSTRUMENT_EXTRAS(m->mod.xxi[xc->ins])->dataloopstart;

	ce->datapos = pos;
	ce->volume = volume;
}

int libxmp_hmn_new_instrument_extras(struct xmp_instrument *xxi)
{
	xxi->extra = calloc(1, sizeof(struct hmn_instrument_extras));
	if (xxi->extra == NULL)
		return -1;
	HMN_INSTRUMENT_EXTRAS((*xxi))->magic = HMN_EXTRAS_MAGIC;
	return 0;
}

int libxmp_hmn_new_channel_extras(struct channel_data *xc)
{
	xc->extra = calloc(1, sizeof(struct hmn_channel_extras));
	if (xc->extra == NULL)
		return -1;
	HMN_CHANNEL_EXTRAS((*xc))->magic = HMN_EXTRAS_MAGIC;
	return 0;
}

void libxmp_hmn_reset_channel_extras(struct channel_data *xc)
{
	memset((char *)xc->extra + 4, 0, sizeof(struct hmn_channel_extras) - 4);
}

void libxmp_hmn_release_channel_extras(struct channel_data *xc)
{
	free(xc->extra);
	xc->extra = NULL;
}

int libxmp_hmn_new_module_extras(struct module_data *m)
{
	m->extra = calloc(1, sizeof(struct hmn_module_extras));
	if (m->extra == NULL)
		return -1;
	HMN_MODULE_EXTRAS((*m))->magic = HMN_EXTRAS_MAGIC;
	return 0;
}

void libxmp_hmn_release_module_extras(struct module_data *m)
{
	free(m->extra);
	m->extra = NULL;
}

void libxmp_hmn_extras_process_fx(struct context_data *ctx, struct channel_data *xc,
			   int chn, uint8 note, uint8 fxt, uint8 fxp, int fnum)
{
	switch (fxt) {
	case FX_MEGAARP:
		/* Not sure if this is correct... */
		fxp = LSN(fxp);

		memcpy(xc->arpeggio.val, megaarp[fxp], 16);
		xc->arpeggio.size = 16;
		break;
	}
}
