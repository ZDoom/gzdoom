/* Extended Module Player
 * Copyright (C) 1996-2025 Claudio Matsuoka and Hipolito Carraro Jr
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
#include "lfo.h"
#include "effects.h"
#include "period.h"
#include "far_extras.h"

#define FAR_GUS_CHANNELS	17
#define FAR_OLD_TEMPO_SHIFT	2 /* Power of multiplier for old tempo mode. */

/**
 * The time factor needed to directly use FAR tempos is a little unintuitive.
 *
 * Generally: FAR tries to run 32/[coarse tempo] rows per second, which
 * (usually, but not always) are subdivided into 4 "ticks". To achieve
 * this, it measures tempos in the number of ticks that should play per second
 * (see far_tempos below). Fine tempo is added or subtracted from this number.
 * To time these ticks, FAR uses the programmable interval timer (PIT) to run a
 * player interrupt.
 *
 * libxmp effectively uses a calculation of 10.0 * 0.25 / BPM to get the tick
 * duration in seconds. A base time factor of 4.0 makes this 1 / BPM, turning
 * BPM into the ticks/sec measure that FAR uses. This isn't completely
 * accurate to FAR, though.
 *
 * The x86 PIT runs at a rate of 1193182 Hz, but FAR does something strange
 * when calculating PIT divisors and uses a constant of 1197255 Hz instead.
 * This means FAR tempo is slightly slower by a factor of around:
 *
 * floor(1197255 / 32) / floor(1193182 / 32) ~= 1.003439
 *
 * This still isn't perfect, but it gets the playback rate fairly close.
 */

/* tempo[0] = 256; tempo[i] = floor(128 / i). */
static const int far_tempos[16] =
{
	256, 128, 64, 42, 32, 25, 21, 18, 16, 14, 12, 11, 10, 9, 9, 8
};

/**
 * FAR tempo has some unusual requirements that don't really match any other
 * format:
 *
 * 1) The coarse tempo is roughly equivalent to speed, but a value of 0 is
 *    supported, and FAR doesn't actually have a concept of ticks: it translates
 *    this value to tempo.
 *
 * 2) There is some very bizarre clamping behavior involving fine tempo slides
 *    that needs to be emulated.
 *
 * 3) Tempos can range from 1 to 356(!). FAR uses a fixed row subdivision size
 *    of 16, so just shift the tempo by 4 and hope libxmp doesn't change it.
 *
 * 4) There are two tempo modes, and they can be switched between arbitrarily...
 */
int libxmp_far_translate_tempo(int mode, int fine_change, int coarse,
			       int *fine, int *_speed, int *_bpm)
{
	int speed, bpm;

	if (coarse < 0 || coarse > 15 || mode < 0 || mode > 1)
		return -1;

	/* Compatibility for FAR's broken fine tempo "clamping". */
	if (fine_change < 0 && far_tempos[coarse] + *fine <= 0) {
		*fine = 0;
	} else if (fine_change > 0 && far_tempos[coarse] + *fine >= 100) {
		*fine = 100;
	}

	if (mode == 1) {
		/* "New" FAR tempo
		 * Note that negative values are possible in Farandole Composer
		 * via changing fine tempo and then slowing coarse tempo.
		 * These result in very slow final tempos due to signed to
		 * unsigned conversion. Zero should just be ignored entirely. */
		int tempo = far_tempos[coarse] + *fine;
		uint32 divisor;
		if (tempo == 0)
			return -1;

		divisor = 1197255 / tempo;

		/* Coincidentally(?), the "new" FAR tempo algorithm actually
		 * prevents the BPM from dropping too far under XMP_MIN_BPM,
		 * which is what libxmp needs anyway. */
		speed = 0;
		while (divisor > 0xffff) {
			divisor >>= 1;
			tempo <<= 1;
			speed++;
		}
		if (speed >= 2)
			speed++;
		speed += 3;
		/* Add an extra tick because the FAR replayer checks the tick
		 * remaining count before decrementing it but after handling
		 * each tick, i.e. a count of "3" executes 4 ticks. */
		speed++;
		bpm = tempo;
	} else {
		/* "Old" FAR tempo
		 * This runs into the XMP_MIN_BPM limit, but nothing uses it anyway.
		 * Old tempo mode in the original FAR replayer has 32 ticks,
		 * but ignores all except every 8th. */
		speed = 4 << FAR_OLD_TEMPO_SHIFT;
		bpm = (far_tempos[coarse] + *fine * 2) << FAR_OLD_TEMPO_SHIFT;
	}

	if (bpm < XMP_MIN_BPM)
		bpm = XMP_MIN_BPM;

	*_speed = speed;
	*_bpm = bpm;
	return 0;
}

static void libxmp_far_update_tempo(struct context_data *ctx, int fine_change)
{
	struct player_data *p = &ctx->p;
	struct module_data *m = &ctx->m;
	struct far_module_extras *me = (struct far_module_extras *)m->extra;
	int speed, bpm;

	if (libxmp_far_translate_tempo(me->tempo_mode, fine_change,
	    me->coarse_tempo, &me->fine_tempo, &speed, &bpm) == 0) {
		p->speed = speed;
		p->bpm = bpm;
	}
}

static void libxmp_far_update_vibrato(struct lfo *lfo, int rate, int depth)
{
	libxmp_lfo_set_depth(lfo, libxmp_gus_frequency_steps(depth << 1, FAR_GUS_CHANNELS));
	libxmp_lfo_set_rate(lfo, rate * 3);
}

/* Convoluted algorithm for delay times for retrigger and note offset effects. */
static int libxmp_far_retrigger_delay(struct far_module_extras *me, int param)
{
	int delay;
	if (me->coarse_tempo < 0 || me->coarse_tempo > 15 || param < 1)
		return -1;

	delay = (far_tempos[me->coarse_tempo] + me->fine_tempo) / param;

	if (me->tempo_mode) {
		/* Effects divide by 4, timer increments by 2 (round up). */
		return ((delay >> 2) + 1) >> 1;
	} else {
		/* Effects divide by 2, timer increments by 2 (round up).
		 * Old tempo mode handles every 8th tick (<< FAR_OLD_TEMPO_SHIFT).
		 * Delay values >4 result in no retrigger. */
		delay = (((delay >> 1) + 1) >> 1) << FAR_OLD_TEMPO_SHIFT;
		if (delay >= 16)
			return -1;
		if (delay < (1 << FAR_OLD_TEMPO_SHIFT))
			return (1 << FAR_OLD_TEMPO_SHIFT);
		return delay;
	}
}


void libxmp_far_play_extras(struct context_data *ctx, struct channel_data *xc, int chn)
{
	struct far_module_extras *me = FAR_MODULE_EXTRAS(ctx->m);
	struct far_channel_extras *ce = FAR_CHANNEL_EXTRAS(*xc);

	/* FAR vibrato depth is global, even though rate isn't. This might have
	 * been changed by a different channel, so make sure it's applied. */
	if (TEST(VIBRATO) || TEST_PER(VIBRATO))
		libxmp_far_update_vibrato(&xc->vibrato.lfo, ce->vib_rate, me->vib_depth);
}

int libxmp_far_new_channel_extras(struct channel_data *xc)
{
	xc->extra = calloc(1, sizeof(struct far_channel_extras));
	if (xc->extra == NULL)
		return -1;
	FAR_CHANNEL_EXTRAS(*xc)->magic = FAR_EXTRAS_MAGIC;
	return 0;
}

void libxmp_far_reset_channel_extras(struct channel_data *xc)
{
	memset((char *)xc->extra + 4, 0, sizeof(struct far_channel_extras) - 4);
}

void libxmp_far_release_channel_extras(struct channel_data *xc)
{
	free(xc->extra);
	xc->extra = NULL;
}

int libxmp_far_new_module_extras(struct module_data *m)
{
	m->extra = calloc(1, sizeof(struct far_module_extras));
	if (m->extra == NULL)
		return -1;
	FAR_MODULE_EXTRAS(*m)->magic = FAR_EXTRAS_MAGIC;
	FAR_MODULE_EXTRAS(*m)->vib_depth = 4;
	return 0;
}

void libxmp_far_release_module_extras(struct module_data *m)
{
	free(m->extra);
	m->extra = NULL;
}

void libxmp_far_extras_process_fx(struct context_data *ctx, struct channel_data *xc,
			   int chn, uint8 note, uint8 fxt, uint8 fxp, int fnum)
{
	struct xmp_module *mod = &ctx->m.mod;
	struct far_module_extras *me = FAR_MODULE_EXTRAS(ctx->m);
	struct far_channel_extras *ce = FAR_CHANNEL_EXTRAS(*xc);
	int update_tempo = 0;
	int update_vibrato = 0;
	int fine_change = 0;
	int delay, target, tempo;
	int32 diff, step;

	/* Tempo effects and vibrato are multiplexed to reduce the effects count.
	 *
	 * Misc. notes: FAR pitch offset effects can overflow/underflow GUS
	 * frequency, which isn't supported by libxmp (Haj/before.far).
	 */
	switch (fxt) {
	case FX_FAR_PORTA_UP:		/* FAR pitch offset up */
		SET(FINE_BEND);
		RESET_PER(TONEPORTA);
		xc->freq.fslide = libxmp_gus_frequency_steps(fxp << 2, FAR_GUS_CHANNELS);
		break;

	case FX_FAR_PORTA_DN:		/* FAR pitch offset down */
		SET(FINE_BEND);
		RESET_PER(TONEPORTA);
		xc->freq.fslide = -libxmp_gus_frequency_steps(fxp << 2, FAR_GUS_CHANNELS);
		break;

	/* Despite some claims, this effect scales with tempo and only
	 * corresponds to (param) rows at tempo 4. See FORMATS.DOC.
	 */
	case FX_FAR_TPORTA:		/* FAR persistent tone portamento */
		if (!IS_VALID_INSTRUMENT(xc->ins))
			break;

		tempo = far_tempos[me->coarse_tempo] + me->fine_tempo;

		SET_PER(TONEPORTA);
		if (IS_VALID_NOTE(note - 1)) {
			xc->porta.target = libxmp_note_to_period(ctx, note - 1, xc->finetune, xc->per_adj);
		}
		xc->porta.dir = xc->period < xc->porta.target ? 1 : -1;

		/* Parameter of 0 is equivalent to 1. */
		if (fxp < 1)
			fxp = 1;
		/* Tempos <=0 cause crashes and other weird behavior
		 * here in Farandole Composer, don't emulate that. */
		if (tempo < 1)
			tempo = 1;

		diff = xc->porta.target - xc->period;
		step = (diff > 0 ? diff : -diff) * 8 / (tempo * fxp);

		xc->porta.slide = (step > 0) ? step : 1;
		break;


	/* Despite some claims, this effect scales with tempo and only
	 * corresponds to (param/2) rows at tempo 4. See FORMATS.DOC.
	 */
	case FX_FAR_SLIDEVOL:		/* FAR persistent slide-to-volume */
		tempo = far_tempos[me->coarse_tempo] + me->fine_tempo;
		target = MSN(fxp) << 4;
		fxp = LSN(fxp);

		/* Parameter of 0 is equivalent to 1. */
		if (fxp < 1)
			fxp = 1;
		/* Tempos <=0 cause crashes and other weird behavior
		 * here in Farandole Composer, don't emulate that. */
		if (tempo < 1)
			tempo = 1;

		diff = target - xc->volume;
		step = diff * 16 / (tempo * fxp);
		if (step == 0)
			step = (diff > 0) ? 1 : -1;

		SET_PER(VOL_SLIDE);
		xc->vol.slide = step;
		xc->vol.target = target + 1;
		break;

	case FX_FAR_VIBDEPTH:		/* FAR set vibrato depth */
		me->vib_depth = LSN(fxp);
		update_vibrato = 1;
		break;

	case FX_FAR_VIBRATO:		/* FAR vibrato and sustained vibrato */
		if (ce->vib_sustain == 0) {
			/* With sustain, regular vibrato only sets the rate. */
			ce->vib_sustain = MSN(fxp);
			if (ce->vib_sustain == 0)
				SET(VIBRATO);
		}
		ce->vib_rate = LSN(fxp);
		update_vibrato = 1;
		break;

	/* Retrigger note param times at intervals that roughly evently
	 * divide the row. A param of 0 crashes Farandole Composer.
	 */
	case FX_FAR_RETRIG:		/* FAR retrigger */
		delay = libxmp_far_retrigger_delay(me, fxp);
		if (note && fxp > 1 && delay >= 0 && delay <= ctx->p.speed) {
			SET(RETRIG);
			xc->retrig.val = delay ? delay : 1;
			xc->retrig.count = delay + 1;
			xc->retrig.type = 0;
			xc->retrig.limit = fxp - 1;
		}
		break;

	/* A better effect name would probably be "retrigger once".
	 * The description/intent seems to be that this is a delay
	 * effect, but an initial note always plays as well. The second
	 * note always plays on the (param)th tick due to player quirks,
	 * but it's supposed to be derived similar to retrigger.
	 * A param of zero works like effect 4F (bug?).
	 */
	case FX_FAR_DELAY:		/* FAR note offset */
		if (note) {
			delay = me->tempo_mode ? fxp : fxp << FAR_OLD_TEMPO_SHIFT;
			SET(RETRIG);
			xc->retrig.val = delay ? delay : 1;
			xc->retrig.count = delay + 1;
			xc->retrig.type = 0;
			xc->retrig.limit = fxp ? 1 : 0;
		}
		break;

	case FX_FAR_TEMPO:		/* FAR coarse tempo and tempo mode */
		if (MSN(fxp)) {
			me->tempo_mode = MSN(fxp) - 1;
		} else {
			me->coarse_tempo = LSN(fxp);
		}
		update_tempo = 1;
		break;

	case FX_FAR_F_TEMPO:		/* FAR fine tempo slide up/down */
		if (MSN(fxp)) {
			me->fine_tempo += MSN(fxp);
			fine_change = MSN(fxp);
		} else if (LSN(fxp)) {
			me->fine_tempo -= LSN(fxp);
			fine_change = -LSN(fxp);
		} else {
			me->fine_tempo = 0;
		}
		update_tempo = 1;
		break;
	}

	if (update_vibrato) {
		if (ce->vib_rate != 0) {
			if (ce->vib_sustain)
				SET_PER(VIBRATO);
		} else {
			RESET_PER(VIBRATO);
			ce->vib_sustain = 0;
		}
		libxmp_far_update_vibrato(&xc->vibrato.lfo, ce->vib_rate, me->vib_depth);
	}

	if (update_tempo)
		libxmp_far_update_tempo(ctx, fine_change);
}
