/*  _______         ____    __         ___    ___
 * \    _  \       \    /  \  /       \   \  /   /       '   '  '
 *  |  | \  \       |  |    ||         |   \/   |         .      .
 *  |  |  |  |      |  |    ||         ||\  /|  |
 *  |  |  |  |      |  |    ||         || \/ |  |         '  '  '
 *  |  |  |  |      |  |    ||         ||    |  |         .      .
 *  |  |_/  /        \  \__//          ||    |  |
 * /_______/ynamic    \____/niversal  /__\  /____\usic   /|  .  . ibliotheque
 *                                                      /  \
 *                                                     / .  \
 * sample.c - The sample (SAMP) signal type.          / / \  \
 *                                                   | <  /   \_
 * By entheh.                                        |  \/ /\   /
 *                                                    \_  /  > /
 * This only supports monaural samples. For             | \ / /
 * multiple channels, use multiple samples and          |  ' /
 * a combining signal (COMB).                            \__/
 */

/* NOTE: filters need not be credited yet, as they will be moved elsewhere. */
/** WARNING don't forget to move these filters somewhere */

#include <stdlib.h>
#include <string.h>

#include "dumb.h"


/** WARNING move these things somewhere useful, for DUH writing - this applies to other signal types too */
#define SIGTYPE_SAMPLE DUMB_ID('S','A','M','P')



#define SAMPFLAG_16BIT    1 /* sample in file is 16 bit, rather than 8 bit */
#define SAMPFLAG_LOOP     2 /* loop indefinitely */
#define SAMPFLAG_XLOOP    4 /* loop x times; only relevant if LOOP not set */
#define SAMPFLAG_PINGPONG 8 /* loop back and forth, if LOOP or XLOOP set */


/* SAMPPARAM_N_LOOPS: add 'value' iterations to the loop. 'value' is assumed
 * to be positive.
 */
#define SAMPPARAM_N_LOOPS 0



typedef struct SAMPLE_SIGDATA
{
	long size;
	int flags;
	long loop_start;
	long loop_end;
	sample_t *samples;
}
SAMPLE_SIGDATA;



typedef struct SAMPLE_SIGRENDERER
{
	SAMPLE_SIGDATA *sigdata;
	int n_channels;
	DUMB_RESAMPLER r;
	int n_loops;
}
SAMPLE_SIGRENDERER;



static sigdata_t *sample_load_sigdata(DUH *duh, DUMBFILE *file)
{
	SAMPLE_SIGDATA *sigdata;
	long size;
	long n;
	int flags;

	(void)duh;

	size = dumbfile_igetl(file);

	if (dumbfile_error(file) || size <= 0)
		return NULL;

	flags = dumbfile_getc(file);
	if (flags < 0)
		return NULL;

	sigdata = malloc(sizeof(*sigdata));
	if (!sigdata)
		return NULL;

	sigdata->samples = malloc(size * sizeof(sample_t));
	if (!sigdata->samples) {
		free(sigdata);
		return NULL;
	}

	sigdata->size = size;
	sigdata->flags = flags;

	if (sigdata->flags & (SAMPFLAG_LOOP | SAMPFLAG_XLOOP)) {
		sigdata->loop_start = dumbfile_igetl(file);

		if (dumbfile_error(file) || (unsigned long)sigdata->loop_start >= (unsigned long)size) {
			free(sigdata->samples);
			free(sigdata);
			return NULL;
		}

		if (sigdata->flags & SAMPFLAG_LOOP)
			sigdata->loop_end = size;
		else {
			sigdata->loop_end = dumbfile_igetl(file);

			if (dumbfile_error(file) || sigdata->loop_end <= sigdata->loop_start || sigdata->loop_end > size) {
				free(sigdata->samples);
				free(sigdata);
				return NULL;
			}
		}
	} else {
		sigdata->loop_start = 0;
		sigdata->loop_end = size;
	}

	if (sigdata->flags & SAMPFLAG_16BIT) {
		for (n = 0; n < size; n++) {
			int m = dumbfile_igetw(file);
			if (m < 0) {
				free(sigdata->samples);
				free(sigdata);
				return NULL;
			}
			sigdata->samples[n] = (int)(signed short)m << 8;
		}
	} else {
		for (n = 0; n < size; n++) {
			int m = dumbfile_getc(file);
			if (m < 0) {
				free(sigdata->samples);
				free(sigdata);
				return NULL;
			}
			sigdata->samples[n] = (int)(signed char)m << 16;
		}
	}

	return sigdata;
}



static void sample_pickup(DUMB_RESAMPLER *r, void *data)
{
	SAMPLE_SIGRENDERER *sigrenderer = data;

	if (!(sigrenderer->sigdata->flags & (SAMPFLAG_LOOP | SAMPFLAG_XLOOP))) {
		r->dir = 0;
		return;
	}

	if (!(sigrenderer->sigdata->flags & SAMPFLAG_LOOP) && sigrenderer->n_loops == 0) {
		r->dir = 0;
		return;
	}

	if (sigrenderer->sigdata->flags & SAMPFLAG_PINGPONG) {
		if (r->dir < 0) {
			r->pos = (r->start << 1) - 1 - r->pos;
			r->subpos ^= 65535;
			r->dir = 1;
		} else {
			r->pos = (r->end << 1) - 1 - r->pos;
			r->subpos ^= 65535;
			r->dir = -1;
		}
	} else
		r->pos -= r->end - r->start;

	if (!(sigrenderer->sigdata->flags & SAMPFLAG_LOOP)) {
		if (sigrenderer->n_loops > 0) {
			sigrenderer->n_loops--;
			if (sigrenderer->n_loops == 0) {
				r->start = 0;
				r->end = sigrenderer->sigdata->size;
			}
		}
	}
}



static sigrenderer_t *sample_start_sigrenderer(DUH *duh, sigdata_t *data, int n_channels, long pos)
{
	SAMPLE_SIGDATA *sigdata = data;
	SAMPLE_SIGRENDERER *sigrenderer;

	(void)duh;

	sigrenderer = malloc(sizeof(*sigrenderer));
	if (!sigrenderer) return NULL;

	sigrenderer->sigdata = data;
	sigrenderer->n_channels = n_channels;
	dumb_reset_resampler(&sigrenderer->r, sigdata->samples, pos, 0, sigdata->size);
	sigrenderer->r.pickup = &sample_pickup;
	sigrenderer->r.pickup_data = sigrenderer;
	sigrenderer->n_loops = 0;

	return sigrenderer;
}



#if 0
/* The name says it all ;-) */
static void sample_cheap_low_pass_filter(sample *src, long size, float max_freq) {

	long i;
	float fact = max_freq / 44100.0f;

	for (i = 0; i < size-1; i++) {
		float d = src[i+1] - src[i];
		if (d > fact)
			src[i+1] += fact - d;
		else if (d < -fact)
			src[i+1] += -d - fact;
	}

	return;
}



/* Dithering with noise shaping filter. Set shape = 0 for no shaping. */
static void sample_dither_filter(float *src, long size, float shape) {
	float r1 = 0, r2 = 0;
	float s1 = 0, s2 = 0; /* Feedback buffer */
	float o = 0.5f / 255;
	float tmp;
	int i;

	for (i = 0; i < size; i++) {
		r2 = r1;
		r1 = rand() / (float)RAND_MAX;

		tmp = src[i] + shape * (s1 + s1 - s2);
		src[i] = tmp + o * (r1 - r2);
		src[i] = MID(-1.0f, src[i], 1.0f);

		s2 = s1;
		s1 = tmp - src[i];
	}
	return;
}
#endif



static void sample_sigrenderer_set_sigparam(sigrenderer_t *data, unsigned char id, long value)
{
	SAMPLE_SIGRENDERER *sigrenderer = data;

	if (id == SAMPPARAM_N_LOOPS) {
		if ((sigrenderer->sigdata->flags & (SAMPFLAG_LOOP | SAMPFLAG_XLOOP)) == SAMPFLAG_XLOOP) {
			sigrenderer->n_loops += value;
			sigrenderer->r.start = sigrenderer->n_loops ? sigrenderer->sigdata->loop_start : 0;
			sigrenderer->r.end = sigrenderer->n_loops ? sigrenderer->sigdata->loop_end : sigrenderer->sigdata->size;
		}
	}
}



static long sample_sigrenderer_get_samples(
	sigrenderer_t *data,
	float volume, float delta,
	long size, sample_t **samples
)
{
	SAMPLE_SIGRENDERER *sigrenderer = data;

	DUMB_RESAMPLER initial_r = sigrenderer->r;

	long s = dumb_resample(&sigrenderer->r, samples[0], size, volume, delta);

	int n;
	for (n = 1; n < sigrenderer->n_channels; n++) {
		sigrenderer->r = initial_r;
		dumb_resample(&sigrenderer->r, samples[n], size, volume, delta);
	}

	return s;
}



static void sample_sigrenderer_get_current_sample(sigrenderer_t *data, float volume, sample_t *samples)
{
	SAMPLE_SIGRENDERER *sigrenderer = data;
	int n;
	for (n = 0; n < sigrenderer->n_channels; n++)
		samples[n] = dumb_resample_get_current_sample(&sigrenderer->r, volume);
}



static void sample_end_sigrenderer(sigrenderer_t *sigrenderer)
{
	free(sigrenderer);
}



static void sample_unload_sigdata(sigdata_t *data)
{
	SAMPLE_SIGDATA *sigdata = data;
	free(sigdata->samples);
	free(data);
}



static DUH_SIGTYPE_DESC sigtype_sample = {
	SIGTYPE_SAMPLE,
	&sample_load_sigdata,
	&sample_start_sigrenderer,
	&sample_sigrenderer_set_sigparam,
	&sample_sigrenderer_get_samples,
	&sample_sigrenderer_get_current_sample,
	&sample_end_sigrenderer,
	&sample_unload_sigdata
};



void dumb_register_sigtype_sample(void)
{
	dumb_register_sigtype(&sigtype_sample);
}
