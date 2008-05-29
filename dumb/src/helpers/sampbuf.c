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
 * sampbuf.c - Helper for allocating sample           / / \  \
 *             buffers.                              | <  /   \_
 *                                                   |  \/ /\   /
 * By entheh.                                         \_  /  > /
 *                                                      | \ / /
 *                                                      |  ' /
 *                                                       \__/
 */

#include <stdlib.h>
#include "dumb.h"



/* DEPRECATED */
sample_t **create_sample_buffer(int n_channels, int32 length)
{
	int i;
	sample_t **samples = malloc(n_channels * sizeof(*samples));
	if (!samples) return NULL;
	samples[0] = malloc(n_channels * length * sizeof(*samples[0]));
	if (!samples[0]) {
		free(samples);
		return NULL;
	}
	for (i = 1; i < n_channels; i++) samples[i] = samples[i-1] + length;
	return samples;
}



sample_t **DUMBEXPORT allocate_sample_buffer(int n_channels, int32 length)
{
	int i;
	sample_t **samples = malloc(((n_channels + 1) >> 1) * sizeof(*samples));
	if (!samples) return NULL;
	samples[0] = malloc(n_channels * length * sizeof(*samples[0]));
	if (!samples[0]) {
		free(samples);
		return NULL;
	}
	for (i = 1; i < (n_channels + 1) >> 1; i++) samples[i] = samples[i-1] + length*2;
	return samples;
}



void DUMBEXPORT destroy_sample_buffer(sample_t **samples)
{
	if (samples) {
		free(samples[0]);
		free(samples);
	}
}
