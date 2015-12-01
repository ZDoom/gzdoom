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
 * sequence.c - The sequence (SEQU) signal type.      / / \  \
 *                                                   | <  /   \_
 * By entheh.                                        |  \/ /\   /
 *                                                    \_  /  > /
 *                                                      | \ / /
 *                                                      |  ' /
 *                                                       \__/
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "dumb.h"



#define SIGTYPE_SEQUENCE DUMB_ID('S','E','Q','U')



/* We have 256 intervals per semitone, 12 * 256 per octave
   2 ** (1 / (12 * 256)) = 1.000225659305069791926712241547647863626

   pow(DUMB_PITCH_BASE, x) = 1.5
   x = log2(1.5) / log2(DUMB_PITCH_BASE)
   x = log2(1.5) * 12 * 256
   x = 1797.004802
   cf.
   x = 7 * 256 = 1792

   so, for the perfect fifth temperament, use an interval of 1797.
*/



/* Sequencing format
 * -----------------
 *
 * NOTE: A LOT OF THIS IS NOW REDUNDANT. PLEASE REFER TO duhspecs.txt.
 *
 * When a signal is initiated, it claims a reference number. If any other
 * currently playing signal has the same number, that signal becomes
 * anonymous and inaccessible; that is, if multiple signals were initiated
 * with the same reference, the reference belongs to the most recent.
 *
 * Signals can be stopped, or have their pitch, volume or parameters changed,
 * using the reference number. A signal may stop prematurely if it runs out
 * of data, in which case the reference number becomes void, and operations
 * on it will be ignored. Such a situation will not flag any kind of warning,
 * since it may be the result of inaccuracies when resampling.
 *
 * The sequence consists of a series of commands. All commands begin with a
 * long int, which is the time to wait after the last command (or the
 * beginning of the sequence) before executing this command. 65536 represents
 * one second. A time of -1 (or in fact any negative time) terminates the
 * sequence, but any currently playing signals will continue until they run
 * out. Make sure no non-terminating signals are playing when the sequence
 * ends!
 *
 * The time, if nonnegative, is followed by one byte indicating the type of
 * command. This byte can have the following values:
 *
 * SEQUENCE_START_SIGNAL
 *   unsigned char ref; - Reference. Need more than 256? Use two sequences,
 *                        and get your brain seen to.
 *   int sig; - The index of the signal to initiate.
 *   long pos; - The position at which to start. 65536 represents one second.
 *   unsigned short volume; - Volume. 65535 represents the maximum volume, so
 *                            you will want to go lower than this if you are
 *                            playing more than one signal at once.
 *   signed short pitch; - Pitch. 0 represents a frequency of 65536 Hz. Scale
 *                         is logarithmic. Add 256 to increase pitch by one
 *                         semitone in the even temperament, or add 12*256 to
 *                         increase pitch by one octave in any temperament
 *                         (i.e. double the frequency).
 *
 * SEQUENCE_SET_VOLUME
 *   unsigned char ref;
 *   unsigned short volume;
 *
 * SEQUENCE_SET_PITCH
 *   unsigned char ref;
 *   signed short pitch;
 *
 * SEQUENCE_SET_PARAMETER
 *   unsigned char ref;
 *   unsigned char id;
 *   long value;
 *   - see the description of the set_parameter function pointer for
 *     information on 'id' and 'value'.
 *
 * SEQUENCE_STOP_SIGNAL
 *   unsigned char ref;
 */



#define SEQUENCE_START_SIGNAL  0
#define SEQUENCE_SET_VOLUME    1
#define SEQUENCE_SET_PITCH     2
#define SEQUENCE_SET_PARAMETER 3
#define SEQUENCE_STOP_SIGNAL   4



typedef struct SEQUENCE_PLAYING
{
	struct SEQUENCE_PLAYING *next;

	DUH_SIGNAL_SAMPINFO *sampinfo;

	int ref;

	int pitch;
	int volume;
}
SEQUENCE_PLAYING;



typedef struct SEQUENCE_SAMPINFO
{
	DUH *duh;

	int n_channels;

	unsigned char *signal;

	long time_left;
	int sub_time_left;

	SEQUENCE_PLAYING *playing;
}
SEQUENCE_SAMPINFO;



#define sequence_c(signal) ((int)*((*(signal))++))


static int sequence_w(unsigned char **signal)
{
	int v = (*signal)[0] | ((*signal)[1] << 8);
	*signal += 2;
	return v;
}


static long sequence_l(unsigned char **signal)
{
	long v = (*signal)[0] | ((*signal)[1] << 8) | ((*signal)[2] << 16) | ((*signal)[3] << 24);
	*signal += 4;
	return v;
}


static long sequence_cl(unsigned char **signal)
{
	long v = sequence_c(signal);
	if (v & 0x80) {
		v &= 0x7F;
		v |= sequence_c(signal) << 7;
		if (v & 0x4000) {
			v &= 0x3FFF;
			v |= sequence_c(signal) << 14;
			if (v & 0x200000) {
				v &= 0x1FFFFF;
				v |= sequence_c(signal) << 21;
				if (v & 0x10000000) {
					v &= 0x0FFFFFFF;
					v |= sequence_c(signal) << 28;
				}
			}
		}
	}
	return v;
}



static void *sequence_load_signal(DUH *duh, DUMBFILE *file)
{
	long size;
	unsigned char *signal;

	(void)duh;

	size = dumbfile_igetl(file);
	if (dumbfile_error(file) || size <= 0)
		return NULL;

	signal = malloc(size);
	if (!signal)
		return NULL;

	if (dumbfile_getnc((char *)signal, size, file) < size) {
		free(signal);
		return NULL;
	}

	return signal;
}



static long render(
	SEQUENCE_SAMPINFO *sampinfo,
	float volume, float delta,
	long pos, long size, sample_t **samples
)
{
	sample_t **splptr;

	SEQUENCE_PLAYING **playing_p = &sampinfo->playing;

	long max_size = 0;
	long part_size;

	int n;
	long i;

	for (n = 0; n < sampinfo->n_channels; n++)
		memset(samples[n] + pos, 0, size * sizeof(sample_t));

	splptr = malloc(sampinfo->n_channels * sizeof(*splptr));
	if (!splptr)
		return 0;

	splptr[0] = malloc(sampinfo->n_channels * size * sizeof(sample_t));
	if (!splptr[0]) {
		free(splptr);
		return 0;
	}

	for (n = 1; n < sampinfo->n_channels; n++)
		splptr[n] = splptr[n - 1] + size;

	while (*playing_p) {
		SEQUENCE_PLAYING *playing = *playing_p;

		part_size = duh_signal_render_samples(
			playing->sampinfo,
			volume * (float)playing->volume * (1.0f / 65535.0f),
			(float)(pow(DUMB_PITCH_BASE, playing->pitch) * delta),
			size, splptr
		);

		for (n = 0; n < sampinfo->n_channels; n++)
			for (i = 0; i < part_size; i++)
				samples[n][pos+i] += splptr[n][i];

		if (part_size > max_size)
			max_size = part_size;

		if (part_size < size) {
			*playing_p = playing->next;
			duh_signal_end_samples(playing->sampinfo);
			free(playing);
		} else
			playing_p = &playing->next;
	}

	free(splptr[0]);
	free(splptr);

	return max_size;
}



/* 'offset' is added to the position at which the signal should start. It is
 * currently assumed to be positive, and is currently only used when seeking
 * forwards in the sequence.
 */
static void sequence_command(SEQUENCE_SAMPINFO *sampinfo, long offset)
{
	int command = sequence_c(&sampinfo->signal);

	if (command == SEQUENCE_START_SIGNAL) {

		int ref = sequence_c(&sampinfo->signal);
		int sig = sequence_cl(&sampinfo->signal);
		long pos = sequence_cl(&sampinfo->signal);
		int volume = sequence_w(&sampinfo->signal);
		int pitch = (int)(signed short)sequence_w(&sampinfo->signal);

		SEQUENCE_PLAYING *playing = sampinfo->playing;

		while (playing) {
			if (playing->ref == ref)
				playing->ref = -1;
			playing = playing->next;
		}

		playing = malloc(sizeof(SEQUENCE_PLAYING));

		if (playing) {
			playing->sampinfo = duh_signal_start_samples(sampinfo->duh, sig, sampinfo->n_channels, pos + offset);

			if (playing->sampinfo) {
				playing->ref = ref;
				playing->pitch = pitch;
				playing->volume = volume;

				playing->next = sampinfo->playing;
				sampinfo->playing = playing;
			} else
				free(playing);
		}

	} else if (command == SEQUENCE_SET_VOLUME) {

		int ref = sequence_c(&sampinfo->signal);
		int volume = sequence_w(&sampinfo->signal);

		SEQUENCE_PLAYING *playing = sampinfo->playing;

		while (playing) {
			if (playing->ref == ref) {
				playing->volume = volume;
				break;
			}
			playing = playing->next;
		}

	} else if (command == SEQUENCE_SET_PITCH) {

		int ref = sequence_c(&sampinfo->signal);
		int pitch = (int)(signed short)sequence_w(&sampinfo->signal);

		SEQUENCE_PLAYING *playing = sampinfo->playing;

		while (playing) {
			if (playing->ref == ref) {
				playing->pitch = pitch;
				break;
			}
			playing = playing->next;
		}

	} else if (command == SEQUENCE_SET_PARAMETER) {

		int ref = sequence_c(&sampinfo->signal);
		unsigned char id = sequence_c(&sampinfo->signal);
		long value = sequence_l(&sampinfo->signal);

		SEQUENCE_PLAYING *playing = sampinfo->playing;

		while (playing) {
			if (playing->ref == ref) {
				duh_signal_set_parameter(playing->sampinfo, id, value);
				break;
			}
			playing = playing->next;
		}

	} else if (command == SEQUENCE_STOP_SIGNAL) {

		int ref = sequence_c(&sampinfo->signal);

		SEQUENCE_PLAYING **playing_p = &sampinfo->playing;

		while (*playing_p) {
			SEQUENCE_PLAYING *playing = *playing_p;

			if (playing->ref == ref) {
				duh_signal_end_samples(playing->sampinfo);
				*playing_p = playing->next;
				free(playing);
				break;
			}

			playing_p = &playing->next;
		}

	} else {

		TRACE("Error in sequence: unknown command %d.\n", command);
		sampinfo->signal = NULL;

	}
}



static void *sequence_start_samples(DUH *duh, void *signal, int n_channels, long pos)
{
	SEQUENCE_SAMPINFO *sampinfo;
	long time = sequence_cl((unsigned char **)&signal);

	if (time < 0)
		return NULL;

	sampinfo = malloc(sizeof(SEQUENCE_SAMPINFO));
	if (!sampinfo)
		return NULL;

	sampinfo->duh = duh;
	sampinfo->n_channels = n_channels;
	sampinfo->signal = signal;
	sampinfo->playing = NULL;

	/* Seek to 'pos'. */
	while (time < pos) {
		pos -= time;

		sequence_command(sampinfo, pos);

		time = sequence_cl(&sampinfo->signal);

		if (time < 0) {
			sampinfo->signal = NULL;
			return sampinfo;
		}
	}

	sampinfo->time_left = time - pos;
	sampinfo->sub_time_left = 0;

	return sampinfo;
}



static long sequence_render_samples(
	void *sampinfo,
	float volume, float delta,
	long size, sample_t **samples
)
{

#define sampinfo ((SEQUENCE_SAMPINFO *)sampinfo)

	long pos = 0;

	int dt = (int)(delta * 65536.0f + 0.5f);

	long todo;
	LONG_LONG t;

	while (sampinfo->signal) {
		todo = (long)((((LONG_LONG)sampinfo->time_left << 16) | sampinfo->sub_time_left) / dt);

		if (todo >= size)
			break;

		if (todo) {
			render(sampinfo, volume, delta, pos, todo, samples);

			pos += todo;
			size -= todo;

		todo = (long)((((LONG_LONG)sampinfo->time_left << 16) | sampinfo->sub_time_left) / dt);
			t = sampinfo->sub_time_left - (LONG_LONG)todo * dt;
			sampinfo->sub_time_left = (long)t & 65535;
			sampinfo->time_left += (long)(t >> 16);
		}

		sequence_command(sampinfo, 0);

		todo = sequence_cl(&sampinfo->signal);

		if (todo >= 0)
			sampinfo->time_left += todo;
		else
			sampinfo->signal = NULL;
	}

	if (sampinfo->signal) {
		render(sampinfo, volume, delta, pos, size, samples);

		pos += size;

		t = sampinfo->sub_time_left - (LONG_LONG)size * dt;
		sampinfo->sub_time_left = (long)t & 65535;
		sampinfo->time_left += (long)(t >> 16);
	} else
		pos += render(sampinfo, volume, delta, pos, size, samples);

	return pos;


/** WARNING - remove this... */
#if 0
	float size_unified = size * delta;

	int n;

	sample_t **samples2 = malloc(sampinfo->n_channels * sizeof(*samples2));
	memcpy(samples2, samples, sampinfo->n_channels * sizeof(*samples2));

	while (sampinfo->signal && sampinfo->time < size_unified) {

		{
			long sz = (long)(sampinfo->time / delta);

			if (sz)
				render(sampinfo, volume, delta, sz, samples2);

			for (n = 0; n < sampinfo->n_channels; n++)
				samples2[n] += sz;

			size -= sz;
			pos += sz;

			sampinfo->time -= sz * delta;
		}

		sequence_command(sampinfo, 0);

		{
			long time = sequence_cl(&sampinfo->signal);

			if (time >= 0)
				sampinfo->time += (float)time;
			else
				sampinfo->signal = NULL;
		}

		size_unified = size * delta;
	}

	if (sampinfo->signal) {
		render(sampinfo, volume, delta, size, samples2);
		sampinfo->time -= size_unified;
		pos += size;
	} else
		pos += render(sampinfo, volume, delta, size, samples2);

	free(samples2);

	return pos;
#endif

#undef sampinfo

}



static void sequence_end_samples(void *sampinfo)
{
	SEQUENCE_PLAYING *playing = ((SEQUENCE_SAMPINFO *)sampinfo)->playing;

	while (playing) {
		SEQUENCE_PLAYING *next = playing->next;

		duh_signal_end_samples(playing->sampinfo);
		free(playing);

		playing = next;
	}

	free(sampinfo);
}



static void sequence_unload_signal(void *signal)
{
	free(signal);
}



static DUH_SIGTYPE_DESC sigtype_sequence = {
	SIGTYPE_SEQUENCE,
	&sequence_load_signal,
	&sequence_start_samples,
	NULL,
	&sequence_render_samples,
	&sequence_end_samples,
	&sequence_unload_signal
};



void dumb_register_sigtype_sequence(void)
{
	dumb_register_sigtype(&sigtype_sequence);
}
