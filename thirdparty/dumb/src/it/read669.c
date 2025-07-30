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
 * read669.c - Code to read a 669 Composer module     / / \  \
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



static int it_669_read_pattern(IT_PATTERN *pattern, DUMBFILE *f, int tempo, int breakpoint, unsigned char *buffer, int * used_channels)
{
	int pos;
	int channel;
	int row;
	IT_ENTRY *entry;

	pattern->n_rows = 64;

    if (dumbfile_getnc((char *)buffer, 64 * 3 * 8, f) < 64 * 3 * 8)
		return -1;

	/* compute number of entries */
	pattern->n_entries = 64 + 1; /* Account for the row end markers, speed command */
	if (breakpoint < 63) pattern->n_entries++; /* and break to row 0 */

	pos = 0;
	for (row = 0; row < 64; row++) {
		for (channel = 0; channel < 8; channel++) {
			if (buffer[pos+0] != 0xFF || buffer[pos+2] != 0xFF)
				pattern->n_entries++;
			pos += 3;
		}
	}

	pattern->entry = malloc(pattern->n_entries * sizeof(*pattern->entry));
	if (!pattern->entry)
		return -1;

	if (breakpoint == 63) breakpoint++;

	entry = pattern->entry;

	entry->channel = 8;
	entry->mask = IT_ENTRY_EFFECT;
	entry->effect = IT_SET_SPEED;
	entry->effectvalue = tempo;
	entry++;

	pos = 0;
	for (row = 0; row < 64; row++) {

		if (row == breakpoint) {
			entry->channel = 8;
			entry->mask = IT_ENTRY_EFFECT;
			entry->effect = IT_BREAK_TO_ROW;
			entry->effectvalue = 0;
			entry++;
		}

		for (channel = 0; channel < 8; channel++) {
			if (buffer[pos+0] != 0xFF || buffer[pos+2] != 0xFF) {
				entry->channel = channel;
				entry->mask = 0;

				if (buffer[pos+0] < 0xFE) {
					entry->mask |= IT_ENTRY_NOTE | IT_ENTRY_INSTRUMENT;
					entry->note = (buffer[pos+0] >> 2) + 36;
					entry->instrument = (((buffer[pos+0] << 4) | (buffer[pos+1] >> 4)) & 0x3F) + 1;
				}
				if (buffer[pos+0] <= 0xFE) {
					entry->mask |= IT_ENTRY_VOLPAN;
					entry->volpan = ((buffer[pos+1] & 15) << 6) / 15;
					if (*used_channels < channel + 1) *used_channels = channel + 1;
				}
				if (buffer[pos+2] != 0xFF) {
					entry->mask |= IT_ENTRY_EFFECT;
					entry->effectvalue = buffer[pos+2] & 15;
					switch (buffer[pos+2] >> 4) {
						case 0:
							entry->effect = IT_PORTAMENTO_UP;
							break;
						case 1:
							entry->effect = IT_PORTAMENTO_DOWN;
							break;
						case 2:
							entry->effect = IT_TONE_PORTAMENTO;
							break;
						case 3:
							entry->effect = IT_S;
							entry->effectvalue += IT_S_FINETUNE * 16 + 8;
							break;
						case 4:
							entry->effect = IT_VIBRATO;
							// XXX speed unknown
							entry->effectvalue |= 0x10;
							break;
						case 5:
							if (entry->effectvalue) {
								entry->effect = IT_SET_SPEED;
							} else {
								entry->mask &= ~IT_ENTRY_EFFECT;
							}
							break;
#if 0
						/* dunno about this, really... */
						case 6:
							if (entry->effectvalue == 0) {
								entry->effect = IT_PANNING_SLIDE;
								entry->effectvalue = 0xFE;
							} else if (entry->effectvalue == 1) {
								entry->effect = IT_PANNING_SLIDE;
								entry->effectvalue = 0xEF;
							} else {
								entry->mask &= ~IT_ENTRY_EFFECT;
							}
							break;
#endif
						default:
							entry->mask &= ~IT_ENTRY_EFFECT;
							break;
					}
					if (*used_channels < channel + 1) *used_channels = channel + 1;
				}

				entry++;
			}
			pos += 3;
		}
		IT_SET_END_ROW(entry);
		entry++;
	}

	return 0;
}



static int it_669_read_sample_header(IT_SAMPLE *sample, DUMBFILE *f)
{
    dumbfile_getnc((char *)sample->name, 13, f);
	sample->name[13] = 0;

	sample->filename[0] = 0;

	sample->length = dumbfile_igetl(f);
	sample->loop_start = dumbfile_igetl(f);
	sample->loop_end = dumbfile_igetl(f);

	if (dumbfile_error(f))
		return -1;

	if (sample->length <= 0) {
		sample->flags = 0;
		return 0;
	}

	sample->flags = IT_SAMPLE_EXISTS;

	sample->global_volume = 64;
	sample->default_volume = 64;

	sample->default_pan = 0;
	sample->C5_speed = 8363;
	// the above line might be wrong

	if ((sample->loop_end > sample->length) && !(sample->loop_start))
		sample->loop_end = 0;

	if (sample->loop_end > sample->length)
		sample->loop_end = sample->length;

	if (sample->loop_end - sample->loop_start > 2)
		sample->flags |= IT_SAMPLE_LOOP;

	sample->vibrato_speed = 0;
	sample->vibrato_depth = 0;
	sample->vibrato_rate = 0;
	sample->vibrato_waveform = 0; // do we have to set _all_ these?
	sample->finetune = 0;
	sample->max_resampling_quality = -1;

	return 0;
}



static int it_669_read_sample_data(IT_SAMPLE *sample, DUMBFILE *f)
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

	if (sample->length)
	{
		i = dumbfile_getnc(sample->data, sample->length, f);
		
		if (i < sample->length) {
			//return -1;
			// ficking truncated files
			if (i <= 0) {
				sample->flags = 0;
				return 0;
			}
			sample->length = i;
			if (sample->loop_end > i) sample->loop_end = i;
		} else {
			/* skip truncated data */
			dumbfile_skip(f, truncated_size);
			// Should we be truncating it?
			if (dumbfile_error(f))
				return -1;
		}

		for (i = 0; i < sample->length; i++)
			((signed char *)sample->data)[i] ^= 0x80;
	}

	return 0;
}


static DUMB_IT_SIGDATA *it_669_load_sigdata(DUMBFILE *f, int * ext)
{
	DUMB_IT_SIGDATA *sigdata;
	int n_channels;
	int i;
	unsigned char tempolist[128];
	unsigned char breaklist[128];

	i = dumbfile_igetw(f);
	if (i != 0x6669 && i != 0x4E4A) return NULL;

	*ext = (i == 0x4E4A);

	sigdata = malloc(sizeof(*sigdata));
	if (!sigdata) {
		return NULL;
	}

    if (dumbfile_getnc((char *)sigdata->name, 36, f) < 36) {
		free(sigdata);
		return NULL;
	}
	sigdata->name[36] = 0;

	sigdata->order = NULL;
	sigdata->instrument = NULL;
	sigdata->pattern = NULL;
	sigdata->midi = NULL;
	sigdata->checkpoint = NULL;
	sigdata->sample = NULL;

	sigdata->n_instruments = 0;

	sigdata->song_message = malloc(72 + 2 + 1);
	if (!sigdata->song_message) {
		free(sigdata);
		return NULL;
	}
    if (dumbfile_getnc((char *)sigdata->song_message, 36, f) < 36) {
		_dumb_it_unload_sigdata(sigdata);
		return NULL;
	}
	sigdata->song_message[36] = 13;
	sigdata->song_message[36 + 1] = 10;
    if (dumbfile_getnc((char *)sigdata->song_message + 38, 36, f) < 36) {
		_dumb_it_unload_sigdata(sigdata);
		return NULL;
	}
	sigdata->song_message[38 + 36] = 0;

	sigdata->n_samples = dumbfile_getc(f);
	sigdata->n_patterns = dumbfile_getc(f);
	sigdata->restart_position = dumbfile_getc(f);

	if ((sigdata->n_samples) > 64 || (sigdata->n_patterns > 128)) {
		_dumb_it_unload_sigdata(sigdata);
		return NULL;
	}

	sigdata->order = malloc(128); /* We may need to scan the extra ones! */
	if (!sigdata->order) {
		_dumb_it_unload_sigdata(sigdata);
		return NULL;
	}
    if (dumbfile_getnc((char *)sigdata->order, 128, f) < 128) {
		_dumb_it_unload_sigdata(sigdata);
		return NULL;
	}

	for (i = 0; i < 128; i++) {
		if (sigdata->order[i] == 255) break;
		if (sigdata->order[i] >= sigdata->n_patterns) {
			_dumb_it_unload_sigdata(sigdata);
			return NULL;
		}
	}
	if (!i) {
		_dumb_it_unload_sigdata(sigdata);
		return NULL;
	}
	sigdata->n_orders = i;

    if (dumbfile_getnc((char *)tempolist, 128, f) < 128) {
		_dumb_it_unload_sigdata(sigdata);
		return NULL;
	}

    if (dumbfile_getnc((char *)breaklist, 128, f) < 128) {
		_dumb_it_unload_sigdata(sigdata);
		return NULL;
	}

	sigdata->sample = malloc(sigdata->n_samples * sizeof(*sigdata->sample));
	if (!sigdata->sample) {
		_dumb_it_unload_sigdata(sigdata);
		return NULL;
	}

	for (i = 0; i < sigdata->n_samples; i++)
		sigdata->sample[i].data = NULL;

	for (i = 0; i < sigdata->n_samples; i++) {
		if (it_669_read_sample_header(&sigdata->sample[i], f)) {
			_dumb_it_unload_sigdata(sigdata);
			return NULL;
		}
	}

	/* May as well try to save a tiny bit of memory. */
	if (sigdata->n_orders < 128) {
		unsigned char *order = realloc(sigdata->order, sigdata->n_orders);
		if (order) sigdata->order = order;
	}

	sigdata->pattern = malloc(sigdata->n_patterns * sizeof(*sigdata->pattern));
	if (!sigdata->pattern) {
		_dumb_it_unload_sigdata(sigdata);
		return NULL;
	}
	for (i = 0; i < sigdata->n_patterns; i++)
		sigdata->pattern[i].entry = NULL;

	n_channels = 0;

	/* Read in the patterns */
	{
		unsigned char *buffer = malloc(64 * 3 * 8); /* 64 rows * 3 bytes * 8 channels */
		if (!buffer) {
			_dumb_it_unload_sigdata(sigdata);
			return NULL;
		}
		for (i = 0; i < sigdata->n_patterns; i++) {
			if (it_669_read_pattern(&sigdata->pattern[i], f, tempolist[i], breaklist[i], buffer, &n_channels) != 0) {
				free(buffer);
				_dumb_it_unload_sigdata(sigdata);
				return NULL;
			}
		}
		free(buffer);
	}

	sigdata->n_pchannels = n_channels;

	/* And finally, the sample data */
	for (i = 0; i < sigdata->n_samples; i++) {
		if (it_669_read_sample_data(&sigdata->sample[i], f)) {
			_dumb_it_unload_sigdata(sigdata);
			return NULL;
		}
	}

	/* Now let's initialise the remaining variables, and we're done! */
	sigdata->flags = IT_OLD_EFFECTS | IT_LINEAR_SLIDES | IT_STEREO | IT_WAS_A_669;

	sigdata->global_volume = 128;
	sigdata->mixing_volume = 48;
	sigdata->speed = 4;
	sigdata->tempo = 78;
	sigdata->pan_separation = 128;

	memset(sigdata->channel_volume, 64, DUMB_IT_N_CHANNELS);

	for (i = 0; i < DUMB_IT_N_CHANNELS; i += 2) {
		int sep = 32 * dumb_it_default_panning_separation / 100;
		sigdata->channel_pan[i+0] = 32 + sep;
		sigdata->channel_pan[i+1] = 32 - sep;
	}

	_dumb_it_fix_invalid_orders(sigdata);

	return sigdata;
}



DUH *DUMBEXPORT dumb_read_669_quick(DUMBFILE *f)
{
	sigdata_t *sigdata;
	int ext;

	DUH_SIGTYPE_DESC *descptr = &_dumb_sigtype_it;

	sigdata = it_669_load_sigdata(f, &ext);

	if (!sigdata)
		return NULL;

	{
		const char *tag[2][2];
		tag[0][0] = "TITLE";
        tag[0][1] = (const char *)(((DUMB_IT_SIGDATA *)sigdata)->name);
		tag[1][0] = "FORMAT";
		tag[1][1] = ext ? "669 Extended" : "669";
		return make_duh(-1, 2, (const char *const (*)[2])tag, 1, &descptr, &sigdata);
	}
}
