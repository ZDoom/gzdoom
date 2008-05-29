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
 * combine.c - The combining (COMB) signal type.      / / \  \
 *                                                   | <  /   \_
 * By entheh.                                        |  \/ /\   /
 *                                                    \_  /  > /
 * This takes multiple monaural signals and             | \ / /
 * combines them into a single multichannel             |  ' /
 * signal. It assumes the correct number of              \__/
 * channels is passed. An ASSERT() is in place
 * to check the number of channels when you
 * compile with -DDEBUGMODE. As an exception, if one channel is passed the
 * signals are all mixed together.
 */

#include <stdlib.h>

#include "dumb.h"



#define SIGTYPE_COMBINING DUMB_ID('C','O','M','B')



typedef struct COMBINING_SIGNAL
{
	int n_signals;
	int sig[ZERO_SIZE];
}
COMBINING_SIGNAL;



typedef struct COMBINING_SAMPINFO
{
	int n_signals;
	int downmix;
	DUH_SIGNAL_SAMPINFO *csampinfo[ZERO_SIZE];
}
COMBINING_SAMPINFO;



static void *combining_load_signal(DUH *duh, DUMBFILE *file)
{
	COMBINING_SIGNAL *signal;
	int n_signals;

	(void)duh;

	n_signals = dumbfile_getc(file);

	/* No point in combining only one signal! */
	if (dumbfile_error(file) || n_signals <= 1)
		return NULL;

	signal = malloc(sizeof(*signal) + n_signals * sizeof(*signal->sig));
	if (!signal)
		return NULL;

	signal->n_signals = n_signals;

	{
		int n;
		for (n = 0; n < signal->n_signals; n++) {
			signal->sig[n] = dumbfile_igetl(file);
			if (dumbfile_error(file)) {
				free(signal);
				return NULL;
			}
		}
	}

	return signal;
}



static void *combining_start_samples(DUH *duh, void *signal, int n_channels, long pos)
{
#define signal ((COMBINING_SIGNAL *)signal)

	COMBINING_SAMPINFO *sampinfo;

	sampinfo = malloc(sizeof(*sampinfo) + signal->n_signals * sizeof(*sampinfo->csampinfo));
	if (!sampinfo)
		return NULL;

	sampinfo->n_signals = signal->n_signals;
	if (n_channels == 1)
		sampinfo->downmix = 1;
	else if (n_channels == signal->n_signals)
		sampinfo->downmix = 0;
	else {
		TRACE("Combining signal discrepancy: %d signals, %d channels.\n", signal->n_signals, n_channels);
		free(sampinfo);
		return NULL;
	}

	{
		int worthwhile = 0;

		{
			int n;
			for (n = 0; n < signal->n_signals; n++) {
				sampinfo->csampinfo[n] = duh_signal_start_samples(duh, signal->sig[n], 1, pos);
				if (sampinfo->csampinfo[n])
					worthwhile = 1;
			}
		}

		if (!worthwhile) {
			free(sampinfo);
			return NULL;
		}
	}

	return sampinfo;

#undef signal
}



static long combining_render_samples(
	void *sampinfo,
	float volume, float delta,
	long size, sample_t **samples
)
{
#define sampinfo ((COMBINING_SAMPINFO *)sampinfo)

	long max_size;

	int n;

	if (sampinfo->downmix)
		volume /= sampinfo->n_signals;

	max_size = duh_signal_render_samples(sampinfo->csampinfo[0], volume, delta, size, samples);

	if (sampinfo->downmix) {

		long s;
		long sz;

		sample_t *sampbuf = malloc(size * sizeof(sample_t));

		if (!sampbuf)
			return 0;

		for (n = 1; n < sampinfo->n_signals; n++) {
			sz = duh_signal_render_samples(sampinfo->csampinfo[n], volume, delta, size, &sampbuf);
			if (sz > max_size) {
				for (s = max_size; s < sz; s++)
					samples[0][s] = sampbuf[s];
				sz = max_size;
				max_size = s;
			}
			for (s = 0; s < sz; s++)
				samples[0][s] += sampbuf[s];
		}

		free(sampbuf);

	} else {

		long *sz = malloc(size * sizeof(*sz));
		long s;

		if (!sz)
			return 0;

		sz[0] = max_size;

		for (n = 1; n < sampinfo->n_signals; n++) {
			sz[n] = duh_signal_render_samples(sampinfo->csampinfo[n], volume, delta, size, samples + n);
			if (sz[n] > max_size)
				max_size = sz[n];
		}

		for (n = 0; n < sampinfo->n_signals; n++)
			for (s = sz[n]; s < max_size; s++)
				samples[n][s] = 0;

		free(sz);

	}

	return max_size;

#undef sampinfo
}



static void combining_end_samples(void *sampinfo)
{
#define sampinfo ((COMBINING_SAMPINFO *)sampinfo)

	int n;

	for (n = 0; n < sampinfo->n_signals; n++)
		duh_signal_end_samples(sampinfo->csampinfo[n]);

	free(sampinfo);

#undef sampinfo
}



static void combining_unload_signal(void *signal)
{
	free(signal);
}



static DUH_SIGTYPE_DESC sigtype_combining = {
	SIGTYPE_COMBINING,
	&combining_load_signal,
	&combining_start_samples,
	NULL,
	&combining_render_samples,
	&combining_end_samples,
	&combining_unload_signal
};


void dumb_register_sigtype_combining(void)
{
	dumb_register_sigtype(&sigtype_combining);
}
