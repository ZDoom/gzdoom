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
 * readmtm.c - Code to read a MultiTracker Module     / / \  \
 *             from an open file.                    | <  /   \_
 *                                                   |  \/ /\   /
 * By Chris Moeller.                                  \_  /  > /
 *                                                      | \ / /
 *                                                      |  ' /
 *                                                       \__/
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "dumb.h"
#include "internal/it.h"

static size_t strlen_max(const char * ptr, size_t max)
{
	const char * end, * start;
	if (ptr==0) return 0;
	start = ptr;
	end = ptr + max;
	while(*ptr && ptr < end) ptr++;
	return ptr - start;
}

static int it_mtm_assemble_pattern(IT_PATTERN *pattern, const unsigned char * track, const unsigned short * sequence, int n_rows)
{
	int n, o, note, sample;
	const unsigned char * t;
	IT_ENTRY * entry;

	pattern->n_rows = n_rows;
	pattern->n_entries = n_rows;

	for (n = 0; n < 32; n++) {
		if (sequence[n]) {
			t = &track[192 * (sequence[n] - 1)];
			for (o = 0; o < n_rows; o++) {
				if (t[0] || t[1] || t[2]) pattern->n_entries++;
				t += 3;
			}
		}
	}

	entry = malloc(pattern->n_entries * sizeof(*entry));
	if (!entry) return -1;
	pattern->entry = entry;

	for (n = 0; n < n_rows; n++) {
		for (o = 0; o < 32; o++) {
			if (sequence[o]) {
				t = &track[192 * (sequence[o] - 1) + (n * 3)];
				if (t[0] || t[1] || t[2]) {
					entry->channel = o;
					entry->mask = 0;
					note = t[0] >> 2;
					sample = ((t[0] << 4) | (t[1] >> 4)) & 0x3F;

					if (note) {
						entry->mask |= IT_ENTRY_NOTE;
						entry->note = note + 24;
					}

					if (sample) {
						entry->mask |= IT_ENTRY_INSTRUMENT;
						entry->instrument = sample;
					}

					_dumb_it_xm_convert_effect(t[1] & 0xF, t[2], entry, 1);

					if (entry->mask) entry++;
				}
			}
		}
		IT_SET_END_ROW(entry);
		entry++;
	}

	pattern->n_entries = (int)(entry - pattern->entry);

	return 0;
}

static int it_mtm_read_sample_header(IT_SAMPLE *sample, DUMBFILE *f)
{
	int finetune, flags;

    dumbfile_getnc((char *)sample->name, 22, f);
	sample->name[22] = 0;

	sample->filename[0] = 0;

	sample->length = dumbfile_igetl(f);
	sample->loop_start = dumbfile_igetl(f);
	sample->loop_end = dumbfile_igetl(f);
	finetune = (signed char)(dumbfile_getc(f) << 4) >> 4; /* signed nibble */
	sample->global_volume = 64;
	sample->default_volume = dumbfile_getc(f);

	flags = dumbfile_getc(f);

	if (sample->length <= 0) {
		sample->flags = 0;
		return 0;
	}

	sample->flags = IT_SAMPLE_EXISTS;

	if (flags & 1) {
		sample->flags |= IT_SAMPLE_16BIT;
		sample->length >>= 1;
		sample->loop_start >>= 1;
		sample->loop_end >>= 1;
	}

	sample->default_pan = 0;
	sample->C5_speed = (int)( AMIGA_CLOCK / 214.0 );//(long)(16726.0*pow(DUMB_PITCH_BASE, finetune*32));
	sample->finetune = finetune * 32;
	// the above line might be wrong

	if (sample->loop_end > sample->length)
		sample->loop_end = sample->length;

	if (sample->loop_end - sample->loop_start > 2)
		sample->flags |= IT_SAMPLE_LOOP;

	sample->vibrato_speed = 0;
	sample->vibrato_depth = 0;
	sample->vibrato_rate = 0;
	sample->vibrato_waveform = 0; // do we have to set _all_ these?
	sample->max_resampling_quality = -1;

	return dumbfile_error(f);
}

static int it_mtm_read_sample_data(IT_SAMPLE *sample, DUMBFILE *f)
{
	int32 i;
	int32 truncated_size;

	/* let's get rid of the sample data coming after the end of the loop */
	if ((sample->flags & IT_SAMPLE_LOOP) && sample->loop_end < sample->length) {
		truncated_size = sample->length - sample->loop_end;
		sample->length = sample->loop_end;
	} else {
		truncated_size = 0;
	}

	sample->data = malloc(sample->length);

	if (!sample->data)
		return -1;

	dumbfile_getnc((char *)sample->data, sample->length, f);
	dumbfile_skip(f, truncated_size);

	if (dumbfile_error(f))
		return -1;

	for (i = 0; i < sample->length; i++)
		((signed char *)sample->data)[i] ^= 0x80;

	return 0;
}

static DUMB_IT_SIGDATA *it_mtm_load_sigdata(DUMBFILE *f, int * version)
{
	DUMB_IT_SIGDATA *sigdata;

	int n, o, n_tracks, l_comment, n_rows, n_channels;

	unsigned char * track;

	unsigned short * sequence;

	char * comment;

	if (dumbfile_getc(f) != 'M' ||
		dumbfile_getc(f) != 'T' ||
		dumbfile_getc(f) != 'M') goto error;

	*version = dumbfile_getc(f);

	sigdata = malloc(sizeof(*sigdata));
	if (!sigdata) goto error;

    dumbfile_getnc((char *)sigdata->name, 20, f);
	sigdata->name[20] = 0;

	n_tracks = dumbfile_igetw(f);
	sigdata->n_patterns = dumbfile_getc(f) + 1;
	sigdata->n_orders = dumbfile_getc(f) + 1;
	l_comment = dumbfile_igetw(f);
	sigdata->n_samples = dumbfile_getc(f);
	//if (dumbfile_getc(f)) goto error_sd;
	dumbfile_getc(f);
	n_rows = dumbfile_getc(f);
	n_channels = dumbfile_getc(f);

	if (dumbfile_error(f) ||
		(n_tracks <= 0) ||
		(sigdata->n_samples <= 0) ||
		(n_rows <= 0 || n_rows > 64) ||
		(n_channels <= 0 || n_channels > 32)) goto error_sd;

	memset(sigdata->channel_volume, 64, DUMB_IT_N_CHANNELS);

    if (dumbfile_getnc((char *)sigdata->channel_pan, 32, f) < 32) goto error_sd;

	for (n = 0; n < 32; n++) {
		if (sigdata->channel_pan[n] <= 15) {
			sigdata->channel_pan[n] -= (sigdata->channel_pan[n] & 8) >> 3;
			sigdata->channel_pan[n] = (sigdata->channel_pan[n] * 32) / 7;
		} else {
			sigdata->channel_volume[n] = 0;
			sigdata->channel_pan[n] = 7;
		}
	}

	for (n = 32; n < DUMB_IT_N_CHANNELS; n += 4) {
		int sep = 32 * dumb_it_default_panning_separation / 100;
		sigdata->channel_pan[n  ] = 32 - sep;
		sigdata->channel_pan[n+1] = 32 + sep;
		sigdata->channel_pan[n+2] = 32 + sep;
		sigdata->channel_pan[n+3] = 32 - sep;
	}

	sigdata->sample = malloc(sigdata->n_samples * sizeof(*sigdata->sample));
	if (!sigdata->sample) goto error_sd;

	sigdata->flags = IT_WAS_AN_XM | IT_WAS_A_MOD | IT_STEREO | IT_OLD_EFFECTS | IT_COMPATIBLE_GXX;

	sigdata->global_volume = 128;
	sigdata->mixing_volume = 48;
	sigdata->speed = 6;
	sigdata->tempo = 125;
	sigdata->pan_separation = 128;

	sigdata->song_message = NULL;
	sigdata->order = NULL;
	sigdata->instrument = NULL;
	sigdata->pattern = NULL;
	sigdata->midi = NULL;
	sigdata->checkpoint = NULL;

	sigdata->n_instruments = 0;

	sigdata->restart_position = 0;
	sigdata->n_pchannels = n_channels;

	for (n = 0; n < sigdata->n_samples; n++)
		sigdata->sample[n].data = NULL;

	for (n = 0; n < sigdata->n_samples; n++) {
		if (it_mtm_read_sample_header(&sigdata->sample[n], f)) goto error_usd;
	}

	sigdata->order = malloc(sigdata->n_orders);
	if (!sigdata->order) goto error_usd;

    if (dumbfile_getnc((char *)sigdata->order, sigdata->n_orders, f) < sigdata->n_orders) goto error_usd;
	if (sigdata->n_orders < 128)
		if (dumbfile_skip(f, 128 - sigdata->n_orders)) goto error_usd;

	track = malloc(192 * n_tracks);
	if (!track) goto error_usd;

    if (dumbfile_getnc((char *)track, 192 * n_tracks, f) < 192 * n_tracks) goto error_ft;

	sigdata->pattern = malloc(sigdata->n_patterns * sizeof(*sigdata->pattern));
	if (!sigdata->pattern) goto error_ft;
	for (n = 0; n < sigdata->n_patterns; n++)
		sigdata->pattern[n].entry = NULL;

	sequence = malloc(sigdata->n_patterns * 32 * sizeof(*sequence));
	if (!sequence) goto error_ft;

	for (n = 0; n < sigdata->n_patterns; n++) {
		for (o = 0; o < 32; o++) {
			sequence[(n * 32) + o] = dumbfile_igetw(f);
			if (sequence[(n * 32) + o] > n_tracks)
			{
				//goto error_fs;
				// illegal track number, silence instead of rejecting the file
				sequence[(n * 32) + o] = 0;
			}
		}
	}

	for (n = 0; n < sigdata->n_patterns; n++) {
		if (it_mtm_assemble_pattern(&sigdata->pattern[n], track, &sequence[n * 32], n_rows)) goto error_fs;
	}

	if (l_comment) {
		comment = malloc(l_comment);
		if (!comment) goto error_fs;
		if (dumbfile_getnc(comment, l_comment, f) < l_comment) goto error_fc;

		/* Time for annoying "logic", yes. We want each line which has text,
		 * and each blank line in between all the valid lines.
		 */

		/* Find last actual line. */
		for (o = -1, n = 0; n < l_comment; n += 40) {
			if (comment[n]) o = n;
		}

		if (o >= 0) {

			size_t l;
			int m;
			for (l = 0, n = 0; n <= o; n += 40) {
				l += strlen_max(&comment[n], 40) + 2;
			}

			l -= 1;

			sigdata->song_message = malloc(l);
			if (!sigdata->song_message) goto error_fc;

			for (m = 0, n = 0; n <= o; n += 40) {
				int p = (int)strlen_max(&comment[n], 40);
				if (p) {
					memcpy(sigdata->song_message + m, &comment[n], p);
					m += p;
				}
				if (l - m > 1) {
					sigdata->song_message[m++] = 13;
					sigdata->song_message[m++] = 10;
				}
			}
			
			sigdata->song_message[m] = 0;
		}

		free(comment);
	}

	for (n = 0; n < sigdata->n_samples; n++) {
		if (it_mtm_read_sample_data(&sigdata->sample[n], f)) goto error_fs;
	}

	_dumb_it_fix_invalid_orders(sigdata);

	free(sequence);
	free(track);

	return sigdata;

error_fc:
	free(comment);
error_fs:
	free(sequence);
error_ft:
	free(track);
error_usd:
	_dumb_it_unload_sigdata(sigdata);
	return NULL;

error_sd:
	free(sigdata);
error:
	return NULL;
}

static char hexdigit(int in)
{
	if (in < 10) return in + '0';
	else return in + 'A' - 10;
}

DUH *DUMBEXPORT dumb_read_mtm_quick(DUMBFILE *f)
{
	sigdata_t *sigdata;
	int ver;

	DUH_SIGTYPE_DESC *descptr = &_dumb_sigtype_it;

	sigdata = it_mtm_load_sigdata(f, &ver);

	if (!sigdata)
		return NULL;

	{
		char version[16];
		const char *tag[2][2];
		tag[0][0] = "TITLE";
        tag[0][1] = (const char *)(((DUMB_IT_SIGDATA *)sigdata)->name);
		tag[1][0] = "FORMAT";
		version[0] = 'M';
		version[1] = 'T';
		version[2] = 'M';
		version[3] = ' ';
		version[4] = 'v';
		version[5] = hexdigit(ver >> 4);
		version[6] = '.';
		version[7] = hexdigit(ver & 15);
		version[8] = 0;
		tag[1][1] = (const char *) &version;
		return make_duh(-1, 2, (const char *const (*)[2])tag, 1, &descptr, &sigdata);
	}
}
