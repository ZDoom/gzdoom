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
#include "extras.h"
#include "med_extras.h"
#include "hmn_extras.h"
#include "far_extras.h"

/*
 * Module extras
 */

void libxmp_release_module_extras(struct context_data *ctx)
{
	struct module_data *m = &ctx->m;

	if (HAS_MED_MODULE_EXTRAS(*m))
		libxmp_med_release_module_extras(m);
	else if (HAS_HMN_MODULE_EXTRAS(*m))
		libxmp_hmn_release_module_extras(m);
	else if (HAS_FAR_MODULE_EXTRAS(*m))
		libxmp_far_release_module_extras(m);
}

/*
 * Channel extras
 */

int libxmp_new_channel_extras(struct context_data *ctx, struct channel_data *xc)
{
	struct module_data *m = &ctx->m;

	if (HAS_MED_MODULE_EXTRAS(*m)) {
		if (libxmp_med_new_channel_extras(xc) < 0)
			return -1;
	} else if (HAS_HMN_MODULE_EXTRAS(*m)) {
		if (libxmp_hmn_new_channel_extras(xc) < 0)
			return -1;
	} else if (HAS_FAR_MODULE_EXTRAS(*m)) {
		if (libxmp_far_new_channel_extras(xc) < 0)
			return -1;
	}

	return 0;
}

void libxmp_release_channel_extras(struct context_data *ctx, struct channel_data *xc)
{
	struct module_data *m = &ctx->m;

	if (HAS_MED_CHANNEL_EXTRAS(*m))
		libxmp_med_release_channel_extras(xc);
	else if (HAS_HMN_CHANNEL_EXTRAS(*m))
		libxmp_hmn_release_channel_extras(xc);
	else if (HAS_FAR_CHANNEL_EXTRAS(*m))
		libxmp_far_release_channel_extras(xc);
}

void libxmp_reset_channel_extras(struct context_data *ctx, struct channel_data *xc)
{
	struct module_data *m = &ctx->m;

	if (HAS_MED_CHANNEL_EXTRAS(*m))
		libxmp_med_reset_channel_extras(xc);
	else if (HAS_HMN_CHANNEL_EXTRAS(*m))
		libxmp_hmn_reset_channel_extras(xc);
	else if (HAS_FAR_CHANNEL_EXTRAS(*m))
		libxmp_far_reset_channel_extras(xc);
}

/*
 * Player extras
 */

void libxmp_play_extras(struct context_data *ctx, struct channel_data *xc, int chn)
{
	struct module_data *m = &ctx->m;

	if (HAS_FAR_CHANNEL_EXTRAS(*xc))
		libxmp_far_play_extras(ctx, xc, chn);

	if (xc->ins >= m->mod.ins)	/* SFX instruments have no extras */
		return;

	if (HAS_MED_INSTRUMENT_EXTRAS(m->mod.xxi[xc->ins]))
		libxmp_med_play_extras(ctx, xc, chn);
	else if (HAS_HMN_INSTRUMENT_EXTRAS(m->mod.xxi[xc->ins]))
		libxmp_hmn_play_extras(ctx, xc, chn);
}

int libxmp_extras_get_volume(struct context_data *ctx, struct channel_data *xc)
{
	struct module_data *m = &ctx->m;
	int vol;

	if (xc->ins >= m->mod.ins)
		vol = xc->volume;
	else if (HAS_MED_INSTRUMENT_EXTRAS(m->mod.xxi[xc->ins]))
		vol = MED_CHANNEL_EXTRAS(*xc)->volume * xc->volume / 64;
	else if (HAS_HMN_INSTRUMENT_EXTRAS(m->mod.xxi[xc->ins]))
		vol = HMN_CHANNEL_EXTRAS(*xc)->volume * xc->volume / 64;
	else
		vol = xc->volume;

	return vol;
}

int libxmp_extras_get_period(struct context_data *ctx, struct channel_data *xc)
{
	int period;

	if (HAS_MED_CHANNEL_EXTRAS(*xc))
		period = libxmp_med_change_period(ctx, xc);
	else period = 0;

	return period;
}

int libxmp_extras_get_linear_bend(struct context_data *ctx, struct channel_data *xc)
{
	int linear_bend;

	if (HAS_MED_CHANNEL_EXTRAS(*xc))
		linear_bend = libxmp_med_linear_bend(ctx, xc);
	else if (HAS_HMN_CHANNEL_EXTRAS(*xc))
		linear_bend = libxmp_hmn_linear_bend(ctx, xc);
	else
		linear_bend = 0;

	return linear_bend;
}

void libxmp_extras_process_fx(struct context_data *ctx, struct channel_data *xc,
			int chn, uint8 note, uint8 fxt, uint8 fxp, int fnum)
{
	if (HAS_MED_CHANNEL_EXTRAS(*xc))
		libxmp_med_extras_process_fx(ctx, xc, chn, note, fxt, fxp, fnum);
	else if (HAS_HMN_CHANNEL_EXTRAS(*xc))
		libxmp_hmn_extras_process_fx(ctx, xc, chn, note, fxt, fxp, fnum);
	else if (HAS_FAR_CHANNEL_EXTRAS(*xc))
		libxmp_far_extras_process_fx(ctx, xc, chn, note, fxt, fxp, fnum);
}
