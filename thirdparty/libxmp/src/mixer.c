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

#include <math.h>
#include "common.h"
#include "virtual.h"
#include "mixer.h"
#include "period.h"
#include "player.h"	/* for set_sample_end() */

#ifdef LIBXMP_PAULA_SIMULATOR
#include "paula.h"
#endif


#define DOWNMIX_SHIFT	 12
#define LIM8_HI		 127
#define LIM8_LO		-128
#define LIM16_HI	 32767
#define LIM16_LO	-32768

#define ANTICLICK_FPSHIFT	24

struct loop_data
{
#define LOOP_PROLOGUE 1
#define LOOP_EPILOGUE 2
	void *sptr;
	int start;
	int end;
	int first_loop;
	int _16bit;
	int active;
	int prologue_num;
	int epilogue_num;
	uint8 prologue[LOOP_PROLOGUE * 2 /* 16-bit */ * 2 /* stereo */];
	uint8 epilogue[LOOP_EPILOGUE * 2 /* 16-bit */ * 2 /* stereo */];
};

/* Mixers array index:
 *
 * bit 0: 0=8 bit sample, 1=16 bit sample
 * bit 1: 0=mono sample, 1=stereo sample
 * bit 2: 0=mono output, 1=stereo output
 * bit 3: 0=unfiltered, 1=filtered
 */

#define FLAG_16_BITS	0x01
#define FLAG_STEREO	0x02
#define FLAG_STEREOOUT	0x04
#define FLAG_FILTER	0x08
#define FLAG_ACTIVE	0x10
/* #define FLAG_SYNTH	0x20 */
#define FIDX_FLAGMASK	(FLAG_16_BITS | FLAG_STEREO | FLAG_STEREOOUT | FLAG_FILTER)

#define MIX_FN(x) void libxmp_mix_##x(struct mixer_voice * LIBXMP_RESTRICT, \
	int32 * LIBXMP_RESTRICT, int, int, int, int, int, int, int)

#define DECLARE_MIX_FUNCTIONS(type) \
	MIX_FN(monoout_mono_8bit_ ## type); \
	MIX_FN(monoout_mono_16bit_ ## type); \
	MIX_FN(monoout_stereo_8bit_ ## type); \
	MIX_FN(monoout_stereo_16bit_ ## type); \
	MIX_FN(stereoout_mono_8bit_ ## type); \
	MIX_FN(stereoout_mono_16bit_ ## type); \
	MIX_FN(stereoout_stereo_8bit_ ## type); \
	MIX_FN(stereoout_stereo_16bit_ ## type)

#define LIST_MIX_FUNCTIONS(type) \
	libxmp_mix_monoout_mono_8bit_ ## type, \
	libxmp_mix_monoout_mono_16bit_ ## type, \
	libxmp_mix_monoout_stereo_8bit_ ## type, \
	libxmp_mix_monoout_stereo_16bit_ ## type, \
	libxmp_mix_stereoout_mono_8bit_ ## type, \
	libxmp_mix_stereoout_mono_16bit_ ## type, \
	libxmp_mix_stereoout_stereo_8bit_ ## type, \
	libxmp_mix_stereoout_stereo_16bit_ ## type

DECLARE_MIX_FUNCTIONS(nearest);
DECLARE_MIX_FUNCTIONS(linear);
DECLARE_MIX_FUNCTIONS(spline);

#ifndef LIBXMP_CORE_DISABLE_IT
DECLARE_MIX_FUNCTIONS(linear_filter);
DECLARE_MIX_FUNCTIONS(spline_filter);
#endif

#ifdef LIBXMP_PAULA_SIMULATOR
MIX_FN(monoout_mono_a500);
MIX_FN(monoout_mono_a500_filter);
MIX_FN(stereoout_mono_a500);
MIX_FN(stereoout_mono_a500_filter);
#endif

typedef void (*MIX_FP) (struct mixer_voice* LIBXMP_RESTRICT, int32* LIBXMP_RESTRICT, int, int, int, int, int, int, int);

static const MIX_FP nearest_mixers[] = {
	LIST_MIX_FUNCTIONS(nearest),

#ifndef LIBXMP_CORE_DISABLE_IT
	LIST_MIX_FUNCTIONS(nearest)
#endif
};

static const MIX_FP linear_mixers[] = {
	LIST_MIX_FUNCTIONS(linear),

#ifndef LIBXMP_CORE_DISABLE_IT
	LIST_MIX_FUNCTIONS(linear_filter)
#endif
};

static const MIX_FP spline_mixers[] = {
	LIST_MIX_FUNCTIONS(spline),

#ifndef LIBXMP_CORE_DISABLE_IT
	LIST_MIX_FUNCTIONS(spline_filter)
#endif
};

#ifdef LIBXMP_PAULA_SIMULATOR
#define LIST_MIX_FUNCTIONS_PAULA(type) \
	libxmp_mix_monoout_mono_ ## type, NULL, NULL, NULL, \
	libxmp_mix_stereoout_mono_ ## type, NULL, NULL, NULL, \
	NULL, NULL, NULL, NULL, \
	NULL, NULL, NULL, NULL

static const MIX_FP a500_mixers[] = {
	LIST_MIX_FUNCTIONS_PAULA(a500)
};

static const MIX_FP a500led_mixers[] = {
	LIST_MIX_FUNCTIONS_PAULA(a500_filter)
};
#endif


/* Downmix 32bit samples to 8bit, signed or unsigned, mono or stereo output */
static void downmix_int_8bit(char *dest, int32 *src, int num, int amp, int offs)
{
	int smp;
	int shift = DOWNMIX_SHIFT + 8 - amp;

	for (; num--; src++, dest++) {
		smp = *src >> shift;
		if (smp > LIM8_HI) {
			*dest = LIM8_HI;
		} else if (smp < LIM8_LO) {
			*dest = LIM8_LO;
		} else {
			*dest = smp;
		}

		if (offs) *dest += offs;
	}
}


/* Downmix 32bit samples to 16bit, signed or unsigned, mono or stereo output */
static void downmix_int_16bit(int16 *dest, int32 *src, int num, int amp, int offs)
{
	int smp;
	int shift = DOWNMIX_SHIFT - amp;

	for (; num--; src++, dest++) {
		smp = *src >> shift;
		if (smp > LIM16_HI) {
			*dest = LIM16_HI;
		} else if (smp < LIM16_LO) {
			*dest = LIM16_LO;
		} else {
			*dest = smp;
		}

		if (offs) *dest += offs;
	}
}

static void anticlick(struct mixer_voice *vi)
{
	vi->flags |= ANTICLICK;
	vi->old_vl = 0;
	vi->old_vr = 0;
}

/* Ok, it's messy, but it works :-) Hipolito */
static void do_anticlick(struct context_data *ctx, int voc, int32 *buf, int count)
{
	struct player_data *p = &ctx->p;
	struct mixer_data *s = &ctx->s;
	struct mixer_voice *vi = &p->virt.voice_array[voc];
	int smp_l, smp_r;
	int discharge = s->ticksize >> ANTICLICK_SHIFT;
	int stepmul, stepval;
	uint32 stepmul_sq;

	smp_l = vi->sleft;
	smp_r = vi->sright;
	vi->sright = vi->sleft = 0;

	if (smp_l == 0 && smp_r == 0) {
		return;
	}

	if (buf == NULL) {
		buf = s->buf32;
		count = discharge;
	} else if (count > discharge) {
		count = discharge;
	}

	if (count <= 0) {
		return;
	}

	stepval = (1 << ANTICLICK_FPSHIFT) / count;
	stepmul = stepval * count;

	if (~s->format & XMP_FORMAT_MONO) {
		while ((stepmul -= stepval) > 0) {
			/* Truncate to 16-bits of precision so the product is 32-bits. */
			stepmul_sq = stepmul >> (ANTICLICK_FPSHIFT - 16);
			stepmul_sq *= stepmul_sq;
			*buf++ += (stepmul_sq * (int64)smp_l) >> 32;
			*buf++ += (stepmul_sq * (int64)smp_r) >> 32;
		}
	} else {
		while ((stepmul -= stepval) > 0) {
			stepmul_sq = stepmul >> (ANTICLICK_FPSHIFT - 16);
			stepmul_sq *= stepmul_sq;
			*buf++ += (stepmul_sq * (int64)smp_l) >> 32;
		}
	}
}

static void set_sample_end(struct context_data *ctx, int voc, int end)
{
	struct player_data *p = &ctx->p;
	struct module_data *m = &ctx->m;
	struct mixer_voice *vi = &p->virt.voice_array[voc];
	struct channel_data *xc;

	if ((uint32)voc >= p->virt.maxvoc)
		return;

	xc = &p->xc_data[vi->chn];

	if (end) {
		SET_NOTE(NOTE_SAMPLE_END);
		vi->fidx &= ~FLAG_ACTIVE;
		if (HAS_QUIRK(QUIRK_RSTCHN)) {
			libxmp_virt_resetvoice(ctx, voc, 0);
		}
	} else {
		RESET_NOTE(NOTE_SAMPLE_END);
	}
}

/* Back up sample data before and after loop and replace it for interpolation.
 * TODO: if higher order interpolation than spline is added, the copy needs to
 *       properly wrap around the loop data (modulo) for correct small loops.
 * TODO: use an overlap buffer like OpenMPT? This is easier, but a little dirty. */
static void init_sample_wraparound(struct mixer_data *s, struct loop_data *ld,
				   struct mixer_voice *vi, struct xmp_sample *xxs)
{
	int prologue_num = LOOP_PROLOGUE;
	int epilogue_num = LOOP_EPILOGUE;
	int bidir;
	int i;

	if (!vi->sptr || s->interp == XMP_INTERP_NEAREST || (~xxs->flg & XMP_SAMPLE_LOOP)) {
		ld->active = 0;
		return;
	}

	ld->sptr = vi->sptr;
	ld->start = vi->start;
	ld->end = vi->end;
	ld->first_loop = !(vi->flags & SAMPLE_LOOP);
	ld->_16bit = (xxs->flg & XMP_SAMPLE_16BIT);
	ld->active = 1;

	/* Stereo */
	if (xxs->flg & XMP_SAMPLE_STEREO) {
		ld->start <<= 1;
		ld->end <<= 1;
		prologue_num <<= 1;
		epilogue_num <<= 1;
	}
	ld->prologue_num = prologue_num;
	ld->epilogue_num = epilogue_num;

	bidir = vi->flags & VOICE_BIDIR;

	if (ld->_16bit) {
		uint16 *start = (uint16 *)ld->sptr + ld->start;
		uint16 *end = (uint16 *)ld->sptr + ld->end;

		memcpy(ld->prologue, start - prologue_num, prologue_num * 2);
		memcpy(ld->epilogue, end, epilogue_num * 2);

		if (!ld->first_loop) {
			for (i = 0; i < prologue_num; i++) {
				int j = i - prologue_num;
				start[j] = bidir ? start[-1 - j] : end[j];
			}
		}
		for (i = 0; i < epilogue_num; i++) {
			end[i] = bidir ? end[-1 - i] : start[i];
		}
	} else {
		uint8 *start = (uint8 *)ld->sptr + ld->start;
		uint8 *end = (uint8 *)ld->sptr + ld->end;

		memcpy(ld->prologue, start - prologue_num, prologue_num);
		memcpy(ld->epilogue, end, epilogue_num);

		if (!ld->first_loop) {
			for (i = 0; i < prologue_num; i++) {
				int j = i - prologue_num;
				start[j] = bidir ? start[-1 - j] : end[j];
			}
		}
		for (i = 0; i < epilogue_num; i++) {
			end[i] = bidir ? end[-1 - i] : start[i];
		}
	}
}

/* Restore old sample data from before and after loop. */
static void reset_sample_wraparound(struct loop_data *ld)
{
	int prologue_num = ld->prologue_num;
	int epilogue_num = ld->epilogue_num;

	if (!ld->active)
		return;

	if (ld->_16bit) {
		uint16 *start = (uint16 *)ld->sptr + ld->start;
		uint16 *end = (uint16 *)ld->sptr + ld->end;

		memcpy(start - prologue_num, ld->prologue, prologue_num * 2);
		memcpy(end, ld->epilogue, epilogue_num * 2);
	} else {
		uint8 *start = (uint8 *)ld->sptr + ld->start;
		uint8 *end = (uint8 *)ld->sptr + ld->end;

		memcpy(start - prologue_num, ld->prologue, prologue_num);
		memcpy(end, ld->epilogue, epilogue_num);
	}
}

static int has_active_sustain_loop(struct context_data *ctx, struct mixer_voice *vi,
				   struct xmp_sample *xxs)
{
#ifndef LIBXMP_CORE_DISABLE_IT
	struct module_data *m = &ctx->m;
	return vi->smp < m->mod.smp && (xxs->flg & XMP_SAMPLE_SLOOP) && (~vi->flags & VOICE_RELEASE);
#else
	return 0;
#endif
}

static int has_active_loop(struct context_data *ctx, struct mixer_voice *vi,
			   struct xmp_sample *xxs)
{
	return (xxs->flg & XMP_SAMPLE_LOOP) || has_active_sustain_loop(ctx, vi, xxs);
}

/* Update the voice endpoints based on current sample loop state. */
static void adjust_voice_end(struct context_data *ctx, struct mixer_voice *vi,
			     struct xmp_sample *xxs, struct extra_sample_data *xtra)
{
	vi->flags &= ~VOICE_BIDIR;

	if (xtra && has_active_sustain_loop(ctx, vi, xxs)) {
		vi->start = xtra->sus;
		vi->end = xtra->sue;
		if (xxs->flg & XMP_SAMPLE_SLOOP_BIDIR) vi->flags |= VOICE_BIDIR;

	} else if (xxs->flg & XMP_SAMPLE_LOOP) {
		vi->start = xxs->lps;
		if ((xxs->flg & XMP_SAMPLE_LOOP_FULL) && (~vi->flags & SAMPLE_LOOP)) {
			vi->end = xxs->len;
		} else {
			vi->end = xxs->lpe;
			if (xxs->flg & XMP_SAMPLE_LOOP_BIDIR) vi->flags |= VOICE_BIDIR;
		}
	} else {
		vi->start = 0;
		vi->end = xxs->len;
	}
}

static int loop_reposition(struct context_data *ctx, struct mixer_voice *vi,
			   struct xmp_sample *xxs, struct extra_sample_data *xtra)
{
	int loop_changed = !(vi->flags & SAMPLE_LOOP);

	vi->flags |= SAMPLE_LOOP;

	if(loop_changed)
		adjust_voice_end(ctx, vi, xxs, xtra);

	if (~vi->flags & VOICE_BIDIR) {
		/* Reposition for next loop */
		if (~vi->flags & VOICE_REVERSE)
			vi->pos -= vi->end - vi->start;
		else
			vi->pos += vi->end - vi->start;
	} else {
		/* Bidirectional loop: switch directions */
		vi->flags ^= VOICE_REVERSE;

		/* Wrap voice position around endpoint */
		if (vi->flags & VOICE_REVERSE) {
			/* OpenMPT Bidi-Loops.it: "In Impulse Tracker's software
			 * mixer, ping-pong loops are shortened by one sample."
			 */
			vi->pos = vi->end * 2 - ctx->s.bidir_adjust - vi->pos;
		} else {
			vi->pos = vi->start * 2 - vi->pos;
		}
	}
	/* Safety check: pos should not be excessively past the sample end.
	 * This only seems to happen with very low sample rates. */
	if (vi->pos > xxs->len + 1) {
		vi->pos = xxs->len + 1;
	}
	return loop_changed;
}

static void hotswap_sample(struct context_data *ctx, struct mixer_voice *vi,
 int voc, int smp)
{
	int vol = vi->vol;
	int pan = vi->pan;
	libxmp_mixer_setpatch(ctx, voc, smp, 0);
	vi->flags |= SAMPLE_LOOP;
	vi->vol = vol;
	vi->pan = pan;
}

static void get_current_sample(struct context_data *ctx, struct mixer_voice *vi,
 struct xmp_sample **xxs, struct extra_sample_data **xtra, int *c5spd)
{
	struct module_data *m = &ctx->m;
	struct xmp_module *mod = &m->mod;

	if (vi->smp < mod->smp) {
		*xxs = &mod->xxs[vi->smp];
		*xtra = &m->xtra[vi->smp];
		*c5spd = m->xtra[vi->smp].c5spd;
	} else {
		*xxs = &ctx->smix.xxs[vi->smp - mod->smp];
		*xtra = NULL;
		*c5spd = m->c4rate;
	}
	adjust_voice_end(ctx, vi, *xxs, *xtra);
}

/* Calculate the required number of sample frames to render a tick.
 * Returns -1 if any of the parameters are invalid. */
int libxmp_mixer_get_ticksize(int freq, double time_factor, double rrate, int bpm)
{
	double calc;
	int ticksize;

	if (freq <= 0 || bpm <= 0 || time_factor <= 0.0 || rrate <= 0.0) {
		return -1;
	}

	calc = freq * time_factor * rrate / bpm / 1000;
	if (calc > INT_MAX || calc != calc /* NaN */) {
		return -1;
	}

	ticksize = (int)calc;

	if (ticksize < (1 << ANTICLICK_SHIFT))
		ticksize = 1 << ANTICLICK_SHIFT;

	return ticksize;
}

/* Prepare the mixer for the next tick */
void libxmp_mixer_prepare(struct context_data *ctx)
{
	struct player_data *p = &ctx->p;
	struct module_data *m = &ctx->m;
	struct mixer_data *s = &ctx->s;
	int bytelen;

	s->ticksize = libxmp_mixer_get_ticksize(s->freq, m->time_factor, m->rrate, p->bpm);

	/* Protect the mixer from broken values caused by xmp_set_tempo_factor. */
	if (s->ticksize < 0 || s->ticksize > (XMP_MAX_FRAMESIZE / 2)) {
		s->ticksize = XMP_MAX_FRAMESIZE / 2;
	}

	bytelen = s->ticksize * sizeof(int32);
	if (~s->format & XMP_FORMAT_MONO) {
		bytelen *= 2;
	}
	memset(s->buf32, 0, bytelen);
}

/* Fill the output buffer calling one of the handlers. The buffer contains
 * sound for one tick (a PAL frame or 1/50s for standard vblank-timed mods)
 */
void libxmp_mixer_softmixer(struct context_data *ctx)
{
	struct player_data *p = &ctx->p;
	struct mixer_data *s = &ctx->s;
	struct module_data *m = &ctx->m;
	struct xmp_module *mod = &m->mod;
	struct extra_sample_data *xtra;
	struct xmp_sample *xxs;
	struct mixer_voice *vi;
	struct loop_data loop_data;
	double step, step_dir;
	int samples, size;
	int vol, vol_l, vol_r, voc, usmp;
	int prev_l, prev_r = 0;
	int32 *buf_pos;
	MIX_FP  mix_fn;
	const MIX_FP *mixerset;

	switch (s->interp) {
	case XMP_INTERP_NEAREST:
		mixerset = nearest_mixers;
		break;
	case XMP_INTERP_LINEAR:
		mixerset = linear_mixers;
		break;
	case XMP_INTERP_SPLINE:
		mixerset = spline_mixers;
		break;
	default:
		mixerset = linear_mixers;
	}

#ifdef LIBXMP_PAULA_SIMULATOR
	if (p->flags & XMP_FLAGS_A500) {
		if (IS_AMIGA_MOD()) {
			if (p->filter) {
				mixerset = a500led_mixers;
			} else {
				mixerset = a500_mixers;
			}
		}
	}
#endif

#ifndef LIBXMP_CORE_DISABLE_IT
	/* OpenMPT Bidi-Loops.it: "In Impulse Tracker's software
	 * mixer, ping-pong loops are shortened by one sample."
	 */
	s->bidir_adjust = IS_PLAYER_MODE_IT() ? 1 : 0;
#endif

	libxmp_mixer_prepare(ctx);

	for (voc = 0; voc < p->virt.maxvoc; voc++) {
		int c5spd, rampsize, delta_l, delta_r;

		vi = &p->virt.voice_array[voc];

		if (vi->flags & ANTICLICK) {
			if (s->interp > XMP_INTERP_NEAREST) {
				do_anticlick(ctx, voc, NULL, 0);
			}
			vi->flags &= ~ANTICLICK;
		}

		if (vi->chn < 0) {
			continue;
		}

		if (vi->period < 1) {
			libxmp_virt_resetvoice(ctx, voc, 1);
			continue;
		}

		/* Negative positions can be left over from some
		 * loop edge cases. These can be safely clamped. */
		if (vi->pos < 0.0)
			vi->pos = 0.0;

		vi->pos0 = vi->pos;

		buf_pos = s->buf32;
		vol = vi->vol;

		/* Mix volume (S3M and IT) */
		if (m->mvolbase > 0 && m->mvol != m->mvolbase) {
			vol = vol * m->mvol / m->mvolbase;
		}

		if (vi->pan == PAN_SURROUND) {
			vol_l = vol * 0x80;
			vol_r = -vol * 0x80;
		} else {
			vol_l = vol * (0x80 - vi->pan);
			vol_r = vol * (0x80 + vi->pan);
		}

		/* Sample is paused - skip channel unless a new sample is queued. */
		if (vi->flags & SAMPLE_PAUSED) {
			if ((~vi->flags & SAMPLE_QUEUED) || vi->queued.smp < 0) {
				vi->flags &= ~SAMPLE_QUEUED;
				continue;
			}
			hotswap_sample(ctx, vi, voc, vi->queued.smp);
			get_current_sample(ctx, vi, &xxs, &xtra, &c5spd);
			vi->pos = vi->start;
		} else {
			get_current_sample(ctx, vi, &xxs, &xtra, &c5spd);
		}

		step = C4_PERIOD * c5spd / s->freq / vi->period;

		/* Don't allow <=0, otherwise m5v-nwlf.it crashes
		 * Extremely high values that can cause undefined float/int
		 * conversion are also possible for c5spd modules. */
		if (step < 0.001 || step > (double)SHRT_MAX) {
			continue;
		}

		init_sample_wraparound(s, &loop_data, vi, xxs);

		rampsize = s->ticksize >> ANTICLICK_SHIFT;
		delta_l = (vol_l - vi->old_vl) / rampsize;
		delta_r = (vol_r - vi->old_vr) / rampsize;

		for (size = usmp = s->ticksize; size > 0; ) {
			int split_noloop = 0;

			if (p->xc_data[vi->chn].split) {
				split_noloop = 1;
			}

			/* How many samples we can write before the loop break
			 * or sample end... */
			if (~vi->flags & VOICE_REVERSE) {
				if (vi->pos >= vi->end) {
					samples = 0;
					if (--usmp <= 0)
						break;
				} else {
					double c = ceil(((double)vi->end - vi->pos) / step);
					/* ...inside the tick boundaries */
					if (c > size) {
						c = size;
					}
					samples = c;
				}
				step_dir = step;
			} else {
				/* Reverse */
				if (vi->pos <= vi->start) {
					samples = 0;
					if (--usmp <= 0)
						break;
				} else {
					double c = ceil((vi->pos - (double)vi->start) / step);
					if (c > size) {
						c = size;
					}
					samples = c;
				}
				step_dir = -step;
			}

			if (vi->vol) {
				int mix_size = samples;
				int mixer_id = vi->fidx & FIDX_FLAGMASK;

				if (~s->format & XMP_FORMAT_MONO) {
					mix_size *= 2;
				}

				/* For Hipolito's anticlick routine */
				if (samples > 0) {
					if (~s->format & XMP_FORMAT_MONO) {
						prev_l = buf_pos[mix_size - 2];
						prev_r = buf_pos[mix_size - 1];
					} else {
						prev_l = buf_pos[mix_size - 1];
					}
				} else {
					prev_r = prev_l = 0;
				}

#ifndef LIBXMP_CORE_DISABLE_IT
				/* See OpenMPT env-flt-max.it */
				if (vi->filter.cutoff >= 0xfe &&
				    vi->filter.resonance == 0) {
					mixer_id &= ~FLAG_FILTER;
				}
#endif

				mix_fn = mixerset[mixer_id];

				/* Call the output handler */
				if (samples > 0 && vi->sptr != NULL) {
					int rsize = 0;

					if (rampsize > samples) {
						rampsize -= samples;
					} else {
						rsize = samples - rampsize;
						rampsize = 0;
					}

					if (delta_l == 0 && delta_r == 0) {
						/* no need to ramp */
						rsize = samples;
					}

					if (mix_fn != NULL) {
						mix_fn(vi, buf_pos, samples,
							vol_l >> 8, vol_r >> 8, step_dir * (1 << SMIX_SHIFT), rsize, delta_l, delta_r);
					}

					buf_pos += mix_size;
					vi->old_vl += samples * delta_l;
					vi->old_vr += samples * delta_r;

					/* For Hipolito's anticlick routine */
					if (~s->format & XMP_FORMAT_MONO) {
						vi->sleft = buf_pos[-2] - prev_l;
						vi->sright = buf_pos[-1] - prev_r;
					} else {
						vi->sleft = buf_pos[-1] - prev_l;
					}
				}
			}

			vi->pos += step_dir * samples;
			size -= samples;

			/* One-shot samples do not loop. */
			if ((!has_active_loop(ctx, vi, xxs) || split_noloop) &&
			    !(vi->flags & SAMPLE_QUEUED)) {
				if (size > 0) {
					do_anticlick(ctx, voc, buf_pos, size);
					set_sample_end(ctx, voc, 1);
					/* Next sample should ramp. */
					vol_l = vol_r = 0;
				}
				size = 0;
				continue;
			}

			/* Loop before continuing to the next channel if the
			 * tick is complete. This is particularly important
			 * for reverse loops to avoid position clamping. */
			if (size > 0 ||
			    ((~vi->flags & VOICE_REVERSE) && vi->pos >= vi->end) ||
			     ((vi->flags & VOICE_REVERSE) && vi->pos <= vi->start)) {
				if (vi->flags & SAMPLE_QUEUED) {
					/* Protracker sample swap */
					do_anticlick(ctx, voc, buf_pos, size);
					if (vi->queued.smp < 0 ||
					    (!has_active_loop(ctx, vi, xxs) &&
					     !(mod->xxs[vi->queued.smp].flg & XMP_SAMPLE_LOOP))) {
						/* Invalid samples and one-shots that
						 * are being replaced by one-shots
						 * (OpenMPT PTStoppedSwap.mod) stop
						 * the current sample. If the current
						 * sample is looped, it needs to be paused.
						 */
						vi->flags &= ~SAMPLE_QUEUED;
						vi->flags |= SAMPLE_PAUSED;
						set_sample_end(ctx, voc, 1);
						/* Next sample should ramp. */
						vol_l = vol_r = 0;
						size = 0;
						continue;
					}
					reset_sample_wraparound(&loop_data);
					hotswap_sample(ctx, vi, voc, vi->queued.smp);
					get_current_sample(ctx, vi, &xxs, &xtra, &c5spd);
					init_sample_wraparound(s, &loop_data, vi, xxs);
					vi->pos = vi->start;
					continue;
				}
				if (loop_reposition(ctx, vi, xxs, xtra)) {
					reset_sample_wraparound(&loop_data);
					init_sample_wraparound(s, &loop_data, vi, xxs);
				}
			}
		}

		reset_sample_wraparound(&loop_data);
		vi->old_vl = vol_l;
		vi->old_vr = vol_r;
	}

	/* Render final frame */

	size = s->ticksize;
	if (~s->format & XMP_FORMAT_MONO) {
		size *= 2;
	}

	if (size > XMP_MAX_FRAMESIZE) {
		size = XMP_MAX_FRAMESIZE;
	}

	if (s->format & XMP_FORMAT_8BIT) {
		downmix_int_8bit(s->buffer, s->buf32, size, s->amplify,
				s->format & XMP_FORMAT_UNSIGNED ? 0x80 : 0);
	} else {
		downmix_int_16bit((int16 *)s->buffer, s->buf32, size, s->amplify,
				s->format & XMP_FORMAT_UNSIGNED ? 0x8000 : 0);
	}

	s->dtright = s->dtleft = 0;
}

void libxmp_mixer_voicepos(struct context_data *ctx, int voc, double pos, int ac)
{
	struct player_data *p = &ctx->p;
	struct module_data *m = &ctx->m;
	struct mixer_voice *vi = &p->virt.voice_array[voc];
	struct xmp_sample *xxs;
	struct extra_sample_data *xtra;

	/* Position changes e.g. retrigger make the new sample take effect
	 * if queued (OpenMPT InstrSwapRetrigger.mod). */
	if (vi->flags & SAMPLE_QUEUED) {
		vi->flags &= ~SAMPLE_QUEUED;
		if (vi->queued.smp < 0) {
			vi->flags |= SAMPLE_PAUSED;
		} else if (vi->smp != vi->queued.smp) {
			hotswap_sample(ctx, vi, voc, vi->queued.smp);
		}
		vi->flags |= SAMPLE_LOOP;
	}

	if (vi->smp < m->mod.smp) {
		xxs = &m->mod.xxs[vi->smp];
		xtra = &m->xtra[vi->smp];
	} else {
		xxs = &ctx->smix.xxs[vi->smp - m->mod.smp];
		xtra = NULL;
	}

	if (xxs->flg & XMP_SAMPLE_SYNTH) {
		return;
	}

	vi->pos = pos;

	adjust_voice_end(ctx, vi, xxs, xtra);

	if (vi->pos >= vi->end) {
		vi->pos = vi->end;
		/* Restart forward sample loops. */
		if ((~vi->flags & VOICE_REVERSE) && has_active_loop(ctx, vi, xxs))
			loop_reposition(ctx, vi, xxs, xtra);
	} else if ((vi->flags & VOICE_REVERSE) && vi->pos <= 0.1) {
		/* Hack: 0 maps to the end for reversed samples. */
		vi->pos = vi->end;
	}

	if (ac) {
		anticlick(vi);
	}
}

double libxmp_mixer_getvoicepos(struct context_data *ctx, int voc)
{
	struct player_data *p = &ctx->p;
	struct mixer_voice *vi = &p->virt.voice_array[voc];
	struct xmp_sample *xxs;

	xxs = libxmp_get_sample(ctx, vi->smp);

	if (xxs->flg & XMP_SAMPLE_SYNTH) {
		return 0;
	}

	return vi->pos;
}

void libxmp_mixer_setpatch(struct context_data *ctx, int voc, int smp, int ac)
{
	struct player_data *p = &ctx->p;
#ifndef LIBXMP_CORE_DISABLE_IT
	struct module_data *m = &ctx->m;
#endif
	struct mixer_data *s = &ctx->s;
	struct mixer_voice *vi = &p->virt.voice_array[voc];
	struct xmp_sample *xxs;

	xxs = libxmp_get_sample(ctx, smp);

	vi->smp = smp;
	vi->vol = 0;
	vi->pan = 0;
	vi->flags &= ~(SAMPLE_LOOP | SAMPLE_QUEUED | SAMPLE_PAUSED | VOICE_REVERSE | VOICE_BIDIR);

	vi->fidx = 0;

	if (~s->format & XMP_FORMAT_MONO) {
		vi->fidx |= FLAG_STEREOOUT;
	}

	set_sample_end(ctx, voc, 0);

	/*mixer_setvol(ctx, voc, 0);*/

	vi->sptr = xxs->data;
	vi->fidx |= FLAG_ACTIVE;

#ifndef LIBXMP_CORE_DISABLE_IT
	if (HAS_QUIRK(QUIRK_FILTER) && s->dsp & XMP_DSP_LOWPASS) {
		vi->fidx |= FLAG_FILTER;
	}
#endif

	if (xxs->flg & XMP_SAMPLE_16BIT) {
		vi->fidx |= FLAG_16_BITS;
	}
	if (xxs->flg & XMP_SAMPLE_STEREO) {
		vi->fidx |= FLAG_STEREO;
	}

	libxmp_mixer_voicepos(ctx, voc, 0, ac);
}

/**
 * Replace the current playing sample when it reaches the end of its
 * sample loop, a la Protracker 1/2. The new sample will begin playing
 * at the start of its loop if it is looped, the start of the sample if
 * it is a one-shot, and it will not play and instead pause the channel
 * if both the original and the new sample are one-shots or if the new
 * sample is empty/invalid/-1.
 */
void libxmp_mixer_queuepatch(struct context_data *ctx, int voc, int smp)
{
	struct player_data *p = &ctx->p;
	struct mixer_voice *vi = &p->virt.voice_array[voc];

	if (smp != vi->smp || (vi->flags & SAMPLE_PAUSED)) {
		vi->queued.smp = smp;
		vi->flags |= SAMPLE_QUEUED;
	}
}

void libxmp_mixer_setnote(struct context_data *ctx, int voc, int note)
{
	struct player_data *p = &ctx->p;
	struct mixer_voice *vi = &p->virt.voice_array[voc];

	/* FIXME: Workaround for crash on notes that are too high
	 *        see 6nations.it (+114 transposition on instrument 16)
	 */
	if (note > 149) {
		note = 149;
	}

	vi->note = note;
	vi->period = libxmp_note_to_period_mix(note, 0);

	anticlick(vi);
}

void libxmp_mixer_setperiod(struct context_data *ctx, int voc, double period)
{
	struct player_data *p = &ctx->p;
	struct mixer_voice *vi = &p->virt.voice_array[voc];

	vi->period = period;
}

void libxmp_mixer_setvol(struct context_data *ctx, int voc, int vol)
{
	struct player_data *p = &ctx->p;
	struct mixer_voice *vi = &p->virt.voice_array[voc];

	if (vol == 0) {
		anticlick(vi);
	}

	vi->vol = vol;
}

void libxmp_mixer_release(struct context_data *ctx, int voc, int rel)
{
	struct player_data *p = &ctx->p;
	struct mixer_voice *vi = &p->virt.voice_array[voc];

	if (rel) {
#ifndef LIBXMP_CORE_DISABLE_IT
		/* Cancel voice reverse when releasing an active sustain loop,
		 * unless the main loop is bidirectional. This is done both for
		 * bidirectional sustain loops and for forward sustain loops
		 * that have been reversed with MPT S9F Play Backward. */
		if (~vi->flags & VOICE_RELEASE) {
			struct xmp_sample *xxs = libxmp_get_sample(ctx, vi->smp);

			if (has_active_sustain_loop(ctx, vi, xxs) &&
			    (~xxs->flg & XMP_SAMPLE_LOOP_BIDIR))
				vi->flags &= ~VOICE_REVERSE;
		}
#endif
		vi->flags |= VOICE_RELEASE;
	} else {
		vi->flags &= ~VOICE_RELEASE;
	}
}

void libxmp_mixer_reverse(struct context_data *ctx, int voc, int rev)
{
	struct player_data *p = &ctx->p;
	struct mixer_voice *vi = &p->virt.voice_array[voc];

	/* Don't reverse samples that have already ended */
	if (~vi->fidx & FLAG_ACTIVE) {
		return;
	}

	if (rev) {
		vi->flags |= VOICE_REVERSE;
	} else {
		vi->flags &= ~VOICE_REVERSE;
	}
}

void libxmp_mixer_seteffect(struct context_data *ctx, int voc, int type, int val)
{
#ifndef LIBXMP_CORE_DISABLE_IT
	struct player_data *p = &ctx->p;
	struct mixer_voice *vi = &p->virt.voice_array[voc];

	switch (type) {
	case DSP_EFFECT_CUTOFF:
		vi->filter.cutoff = val;
		break;
	case DSP_EFFECT_RESONANCE:
		vi->filter.resonance = val;
		break;
	case DSP_EFFECT_FILTER_A0:
		vi->filter.a0 = val;
		break;
	case DSP_EFFECT_FILTER_B0:
		vi->filter.b0 = val;
		break;
	case DSP_EFFECT_FILTER_B1:
		vi->filter.b1 = val;
		break;
	}
#endif
}

void libxmp_mixer_setpan(struct context_data *ctx, int voc, int pan)
{
	struct player_data *p = &ctx->p;
	struct mixer_voice *vi = &p->virt.voice_array[voc];

	vi->pan = pan;
}

int libxmp_mixer_numvoices(struct context_data *ctx, int num)
{
	struct mixer_data *s = &ctx->s;

	if (num > s->numvoc || num < 0) {
		return s->numvoc;
	} else {
		return num;
	}
}

int libxmp_mixer_on(struct context_data *ctx, int rate, int format, int c4rate)
{
	struct mixer_data *s = &ctx->s;

	s->buffer = (char *) calloc(XMP_MAX_FRAMESIZE, sizeof(int16));
	if (s->buffer == NULL)
		goto err;

	s->buf32 = (int32 *) calloc(XMP_MAX_FRAMESIZE, sizeof(int32));
	if (s->buf32 == NULL)
		goto err1;

	s->freq = rate;
	s->format = format;
	s->amplify = DEFAULT_AMPLIFY;
	s->mix = DEFAULT_MIX;
	/* s->pbase = C4_PERIOD * c4rate / s->freq; */
	s->interp = XMP_INTERP_LINEAR;	/* default interpolation type */
	s->dsp = XMP_DSP_LOWPASS;	/* enable filters by default */
	/* s->numvoc = SMIX_NUMVOC; */
	s->dtright = s->dtleft = 0;
	s->bidir_adjust = 0;

	return 0;

    err1:
	free(s->buffer);
	s->buffer = NULL;
    err:
	return -1;
}

void libxmp_mixer_off(struct context_data *ctx)
{
	struct mixer_data *s = &ctx->s;

	free(s->buffer);
	free(s->buf32);
	s->buf32 = NULL;
	s->buffer = NULL;
}
