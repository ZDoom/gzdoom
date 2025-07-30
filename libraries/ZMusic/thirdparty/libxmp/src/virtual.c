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
#include "virtual.h"
#include "mixer.h"

#ifdef LIBXMP_PAULA_SIMULATOR
#include "paula.h"
#endif

#define	FREE	-1

/* For virt_pastnote() */
void libxmp_player_set_release(struct context_data *, int);
void libxmp_player_set_fadeout(struct context_data *, int);


/* Get parent channel */
int libxmp_virt_getroot(struct context_data *ctx, int chn)
{
	struct player_data *p = &ctx->p;
	struct mixer_voice *vi;
	int voc;

	voc = p->virt.virt_channel[chn].map;
	if (voc < 0) {
		return -1;
	}

	vi = &p->virt.voice_array[voc];

	return vi->root;
}

void libxmp_virt_resetvoice(struct context_data *ctx, int voc, int mute)
{
	struct player_data *p = &ctx->p;
	struct mixer_voice *vi = &p->virt.voice_array[voc];
#ifdef LIBXMP_PAULA_SIMULATOR
	struct paula_state *paula;
#endif

	if ((uint32)voc >= p->virt.maxvoc) {
		return;
	}

	if (mute) {
		libxmp_mixer_setvol(ctx, voc, 0);
	}

	p->virt.virt_used--;
	p->virt.virt_channel[vi->root].count--;
	p->virt.virt_channel[vi->chn].map = FREE;
#ifdef LIBXMP_PAULA_SIMULATOR
	paula = vi->paula;
#endif
	memset(vi, 0, sizeof(struct mixer_voice));
#ifdef LIBXMP_PAULA_SIMULATOR
	vi->paula = paula;
#endif
	vi->chn = vi->root = FREE;
}

/* virt_on (number of tracks) */
int libxmp_virt_on(struct context_data *ctx, int num)
{
	struct player_data *p = &ctx->p;
	struct module_data *m = &ctx->m;
	int i;

	p->virt.num_tracks = num;
	num = libxmp_mixer_numvoices(ctx, -1);

	p->virt.virt_channels = p->virt.num_tracks;

	if (HAS_QUIRK(QUIRK_VIRTUAL)) {
		p->virt.virt_channels += num;
	} else if (num > p->virt.virt_channels) {
		num = p->virt.virt_channels;
	}

	p->virt.maxvoc = libxmp_mixer_numvoices(ctx, num);

	p->virt.voice_array = (struct mixer_voice *) calloc(p->virt.maxvoc,
						sizeof(struct mixer_voice));
	if (p->virt.voice_array == NULL)
		goto err;

	for (i = 0; i < p->virt.maxvoc; i++) {
		p->virt.voice_array[i].chn = FREE;
		p->virt.voice_array[i].root = FREE;
	}

#ifdef LIBXMP_PAULA_SIMULATOR
	/* Initialize Paula simulator */
	if (IS_AMIGA_MOD()) {
		for (i = 0; i < p->virt.maxvoc; i++) {
			p->virt.voice_array[i].paula = (struct paula_state *) calloc(1, sizeof(struct paula_state));
			if (p->virt.voice_array[i].paula == NULL) {
				goto err2;
			}
			libxmp_paula_init(ctx, p->virt.voice_array[i].paula);
		}
	}
#endif

	p->virt.virt_channel = (struct virt_channel *) malloc(p->virt.virt_channels *
							sizeof(struct virt_channel));
	if (p->virt.virt_channel == NULL)
		goto err2;

	for (i = 0; i < p->virt.virt_channels; i++) {
		p->virt.virt_channel[i].map = FREE;
		p->virt.virt_channel[i].count = 0;
	}

	p->virt.virt_used = 0;

	return 0;

      err2:
#ifdef LIBXMP_PAULA_SIMULATOR
	if (IS_AMIGA_MOD()) {
		for (i = 0; i < p->virt.maxvoc; i++) {
			free(p->virt.voice_array[i].paula);
		}
	}
#endif
	free(p->virt.voice_array);
	p->virt.voice_array = NULL;
      err:
	return -1;
}

void libxmp_virt_off(struct context_data *ctx)
{
	struct player_data *p = &ctx->p;
#ifdef LIBXMP_PAULA_SIMULATOR
	struct module_data *m = &ctx->m;
	int i;
#endif

#ifdef LIBXMP_PAULA_SIMULATOR
	/* Free Paula simulator state */
	if (IS_AMIGA_MOD()) {
		for (i = 0; i < p->virt.maxvoc; i++) {
			free(p->virt.voice_array[i].paula);
		}
	}
#endif

	p->virt.virt_used = p->virt.maxvoc = 0;
	p->virt.virt_channels = 0;
	p->virt.num_tracks = 0;

	free(p->virt.voice_array);
	free(p->virt.virt_channel);
	p->virt.voice_array = NULL;
	p->virt.virt_channel = NULL;
}

void libxmp_virt_reset(struct context_data *ctx)
{
	struct player_data *p = &ctx->p;
	int i;

	if (p->virt.virt_channels < 1) {
		return;
	}

	/* CID 129203 (#1 of 1): Useless call (USELESS_CALL)
	 * Call is only useful for its return value, which is ignored.
	 *
	 * libxmp_mixer_numvoices(ctx, p->virt.maxvoc);
	 */

	for (i = 0; i < p->virt.maxvoc; i++) {
		struct mixer_voice *vi = &p->virt.voice_array[i];
#ifdef LIBXMP_PAULA_SIMULATOR
		struct paula_state *paula = vi->paula;
#endif
		memset(vi, 0, sizeof(struct mixer_voice));
#ifdef LIBXMP_PAULA_SIMULATOR
		vi->paula = paula;
#endif
		vi->chn = FREE;
		vi->root = FREE;
	}

	for (i = 0; i < p->virt.virt_channels; i++) {
		p->virt.virt_channel[i].map = FREE;
		p->virt.virt_channel[i].count = 0;
	}

	p->virt.virt_used = 0;
}

static int free_voice(struct context_data *ctx)
{
	struct player_data *p = &ctx->p;
	int i, num, vol;

	/* Find background voice with lowest volume*/
	num = FREE;
	vol = INT_MAX;
	for (i = 0; i < p->virt.maxvoc; i++) {
		struct mixer_voice *vi = &p->virt.voice_array[i];

		if (vi->chn >= p->virt.num_tracks && vi->vol < vol) {
			num = i;
			vol = vi->vol;
		}
	}

	/* Free voice */
	if (num >= 0) {
		p->virt.virt_channel[p->virt.voice_array[num].chn].map = FREE;
		p->virt.virt_channel[p->virt.voice_array[num].root].count--;
		p->virt.virt_used--;
	}

	return num;
}

static int alloc_voice(struct context_data *ctx, int chn)
{
	struct player_data *p = &ctx->p;
	int i;

	/* Find free voice */
	for (i = 0; i < p->virt.maxvoc; i++) {
		if (p->virt.voice_array[i].chn == FREE)
			break;
	}

	/* not found */
	if (i == p->virt.maxvoc) {
		i = free_voice(ctx);
	}

	if (i >= 0) {
		p->virt.virt_channel[chn].count++;
		p->virt.virt_used++;

		p->virt.voice_array[i].chn = chn;
		p->virt.voice_array[i].root = chn;
		p->virt.virt_channel[chn].map = i;
	}

	return i;
}

static int map_virt_channel(struct player_data *p, int chn)
{
	int voc;

	if ((uint32)chn >= p->virt.virt_channels)
		return -1;

	voc = p->virt.virt_channel[chn].map;

	if ((uint32)voc >= p->virt.maxvoc)
		return -1;

	return voc;
}

int libxmp_virt_mapchannel(struct context_data *ctx, int chn)
{
	return map_virt_channel(&ctx->p, chn);
}

void libxmp_virt_resetchannel(struct context_data *ctx, int chn)
{
	struct player_data *p = &ctx->p;
	struct mixer_voice *vi;
#ifdef LIBXMP_PAULA_SIMULATOR
	struct paula_state *paula;
#endif
	int voc;

	if ((voc = map_virt_channel(p, chn)) < 0)
		return;

	libxmp_mixer_setvol(ctx, voc, 0);

	p->virt.virt_used--;
	p->virt.virt_channel[p->virt.voice_array[voc].root].count--;
	p->virt.virt_channel[chn].map = FREE;

	vi = &p->virt.voice_array[voc];
#ifdef LIBXMP_PAULA_SIMULATOR
	paula = vi->paula;
#endif
	memset(vi, 0, sizeof(struct mixer_voice));
#ifdef LIBXMP_PAULA_SIMULATOR
	vi->paula = paula;
#endif
	vi->chn = vi->root = FREE;
}

void libxmp_virt_setvol(struct context_data *ctx, int chn, int vol)
{
	struct player_data *p = &ctx->p;
	int voc, root;

	if ((voc = map_virt_channel(p, chn)) < 0) {
		return;
	}

	root = p->virt.voice_array[voc].root;
	if (root < XMP_MAX_CHANNELS && p->channel_mute[root]) {
		vol = 0;
	}

	libxmp_mixer_setvol(ctx, voc, vol);

	if (vol == 0 && chn >= p->virt.num_tracks) {
		libxmp_virt_resetvoice(ctx, voc, 1);
	}
}

void libxmp_virt_release(struct context_data *ctx, int chn, int rel)
{
	struct player_data *p = &ctx->p;
	int voc;

	if ((voc = map_virt_channel(p, chn)) < 0) {
		return;
	}

	libxmp_mixer_release(ctx, voc, rel);
}

void libxmp_virt_reverse(struct context_data *ctx, int chn, int rev)
{
	struct player_data *p = &ctx->p;
	int voc;

	if ((voc = map_virt_channel(p, chn)) < 0) {
		return;
	}

	libxmp_mixer_reverse(ctx, voc, rev);
}

void libxmp_virt_setpan(struct context_data *ctx, int chn, int pan)
{
	struct player_data *p = &ctx->p;
	int voc;

	if ((voc = map_virt_channel(p, chn)) < 0) {
		return;
	}

	libxmp_mixer_setpan(ctx, voc, pan);
}

void libxmp_virt_seteffect(struct context_data *ctx, int chn, int type, int val)
{
	struct player_data *p = &ctx->p;
	int voc;

	if ((voc = map_virt_channel(p, chn)) < 0) {
		return;
	}

	libxmp_mixer_seteffect(ctx, voc, type, val);
}

double libxmp_virt_getvoicepos(struct context_data *ctx, int chn)
{
	struct player_data *p = &ctx->p;
	int voc;

	if ((voc = map_virt_channel(p, chn)) < 0) {
		return -1;
	}

	return libxmp_mixer_getvoicepos(ctx, voc);
}

#ifndef LIBXMP_CORE_PLAYER

void libxmp_virt_setsmp(struct context_data *ctx, int chn, int smp)
{
	struct player_data *p = &ctx->p;
	struct mixer_voice *vi;
	double pos;
	int voc;

	if ((voc = map_virt_channel(p, chn)) < 0) {
		return;
	}

	vi = &p->virt.voice_array[voc];
	if (vi->smp == smp) {
		return;
	}

	pos = libxmp_mixer_getvoicepos(ctx, voc);
	libxmp_mixer_setpatch(ctx, voc, smp, 0);
	libxmp_mixer_voicepos(ctx, voc, pos, 0);	/* Restore old position */
}

#endif

#ifndef LIBXMP_CORE_DISABLE_IT

void libxmp_virt_setnna(struct context_data *ctx, int chn, int nna)
{
	struct player_data *p = &ctx->p;
	struct module_data *m = &ctx->m;
	int voc;

	if (!HAS_QUIRK(QUIRK_VIRTUAL)) {
		return;
	}

	if ((voc = map_virt_channel(p, chn)) < 0) {
		return;
	}

	p->virt.voice_array[voc].act = nna;
}

static void check_dct(struct context_data *ctx, int i, int chn, int ins,
			int smp, int key, int nna, int dct, int dca)
{
	struct player_data *p = &ctx->p;
	struct mixer_voice *vi = &p->virt.voice_array[i];
	int voc;

	voc = p->virt.virt_channel[chn].map;

	if (vi->root == chn && vi->ins == ins) {

		if (nna == XMP_INST_NNA_CUT) {
			libxmp_virt_resetvoice(ctx, i, 1);
			return;
		}

		vi->act = nna;

		if ((dct == XMP_INST_DCT_INST) ||
		    (dct == XMP_INST_DCT_SMP && vi->smp == smp) ||
		    (dct == XMP_INST_DCT_NOTE && vi->key == key)) {

			if (nna == XMP_INST_NNA_OFF && dca == XMP_INST_DCA_FADE) {
				vi->act = VIRT_ACTION_OFF;
			} else if (dca) {
				if (i != voc || vi->act) {
					vi->act = dca;
				}
			} else {
				libxmp_virt_resetvoice(ctx, i, 1);
			}
		}
	}
}

#endif

/* For note slides */
void libxmp_virt_setnote(struct context_data *ctx, int chn, int note)
{
	struct player_data *p = &ctx->p;
	int voc;

	if ((voc = map_virt_channel(p, chn)) < 0) {
		return;
	}

	libxmp_mixer_setnote(ctx, voc, note);
}

int libxmp_virt_setpatch(struct context_data *ctx, int chn, int ins, int smp,
			 int note, int key, int nna, int dct, int dca)
{
	struct player_data *p = &ctx->p;
	int voc, vfree;

	if ((uint32)chn >= p->virt.virt_channels) {
		return -1;
	}

	if (ins < 0) {
		smp = -1;
	}

#ifndef LIBXMP_CORE_DISABLE_IT
	if (dct) {
		int i;

		for (i = 0; i < p->virt.maxvoc; i++) {
			check_dct(ctx, i, chn, ins, smp, key, nna, dct, dca);
		}
	}
#endif

	voc = p->virt.virt_channel[chn].map;

	if (voc > FREE) {
		if (p->virt.voice_array[voc].act) {
			vfree = alloc_voice(ctx, chn);

			if (vfree < 0) {
				return -1;
			}

			for (chn = p->virt.num_tracks; chn < p->virt.virt_channels &&
			     p->virt.virt_channel[chn++].map > FREE;) ;

			p->virt.voice_array[voc].chn = --chn;
			p->virt.virt_channel[chn].map = voc;
			voc = vfree;
		}
	} else {
		voc = alloc_voice(ctx, chn);
		if (voc < 0) {
			return -1;
		}
	}

	if (smp < 0) {
		libxmp_virt_resetvoice(ctx, voc, 1);
		return chn;	/* was -1 */
	}

	libxmp_mixer_setpatch(ctx, voc, smp, 1);
	libxmp_mixer_setnote(ctx, voc, note);
	p->virt.voice_array[voc].ins = ins;
	p->virt.voice_array[voc].act = nna;
	p->virt.voice_array[voc].key = key;

	return chn;
}

void libxmp_virt_setperiod(struct context_data *ctx, int chn, double period)
{
	struct player_data *p = &ctx->p;
	int voc;

	if ((voc = map_virt_channel(p, chn)) < 0) {
		return;
	}

	libxmp_mixer_setperiod(ctx, voc, period);
}

void libxmp_virt_voicepos(struct context_data *ctx, int chn, double pos)
{
	struct player_data *p = &ctx->p;
	int voc;

	if ((voc = map_virt_channel(p, chn)) < 0) {
		return;
	}

	libxmp_mixer_voicepos(ctx, voc, pos, 1);
}

#ifndef LIBXMP_CORE_DISABLE_IT

void libxmp_virt_pastnote(struct context_data *ctx, int chn, int act)
{
	struct player_data *p = &ctx->p;
	int c, voc;

	for (c = p->virt.num_tracks; c < p->virt.virt_channels; c++) {
		if ((voc = map_virt_channel(p, c)) < 0)
			continue;

		if (p->virt.voice_array[voc].root == chn) {
			switch (act) {
			case VIRT_ACTION_CUT:
				libxmp_virt_resetvoice(ctx, voc, 1);
				break;
			case VIRT_ACTION_OFF:
				libxmp_player_set_release(ctx, c);
				break;
			case VIRT_ACTION_FADE:
				libxmp_player_set_fadeout(ctx, c);
				break;
			}
		}
	}
}

#endif

int libxmp_virt_cstat(struct context_data *ctx, int chn)
{
	struct player_data *p = &ctx->p;
	int voc;

	if ((voc = map_virt_channel(p, chn)) < 0) {
		return VIRT_INVALID;
	}

	if (chn < p->virt.num_tracks) {
		return VIRT_ACTIVE;
	}

	return p->virt.voice_array[voc].act;
}
