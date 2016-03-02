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
 * readptm.c - Code to read a Poly Tracker v2.03      / / \  \
 *             module from an open file.             | <  /   \_
 *                                                   |  \/ /\   /
 * By Chris Moeller. Based on reads3m.c               \_  /  > /
 * by entheh.                                           | \ / /
 *                                                      |  ' /
 *                                                       \__/
 */

// IT_STEREO... :o
#include <stdlib.h>
#include <string.h>

#include "dumb.h"
#include "internal/it.h"



static int it_ptm_read_sample_header(IT_SAMPLE *sample, int32 *offset, DUMBFILE *f)
{
	int flags;

	flags = dumbfile_getc(f);

    dumbfile_getnc((char *)sample->filename, 12, f);
	sample->filename[12] = 0;

	sample->default_volume = dumbfile_getc(f);

	sample->C5_speed = dumbfile_igetw(f) << 1;

	dumbfile_skip(f, 2); /* segment */

	*offset = dumbfile_igetl(f);

	sample->length = dumbfile_igetl(f);
	sample->loop_start = dumbfile_igetl(f);
	sample->loop_end = dumbfile_igetl(f);

	/* GUSBegin, GUSLStart, GUSLEnd, GUSLoop, reserverd */
	dumbfile_skip(f, 4+4+4+1+1);

    dumbfile_getnc((char *)sample->name, 28, f);
	sample->name[28] = 0;

	/*
	if (dumbfile_mgetl(f) != DUMB_ID('P','T','M','S'))
		return -1;
	*/

	/* BLAH! Shit likes to have broken or missing sample IDs */
	dumbfile_skip(f, 4);

	if ((flags & 3) == 0) {
		/* Looks like no sample */
		sample->flags &= ~IT_SAMPLE_EXISTS;
		return dumbfile_error(f);
	}

	sample->global_volume = 64;

	sample->flags = IT_SAMPLE_EXISTS;
	if (flags & 4) sample->flags |= IT_SAMPLE_LOOP;
	if (flags & 8) sample->flags |= IT_SAMPLE_PINGPONG_LOOP;

	if (flags & 16) {
		sample->flags |= IT_SAMPLE_16BIT;

		sample->length >>= 1;
		sample->loop_start >>= 1;
		sample->loop_end >>= 1;
	}

	if (sample->loop_end) sample->loop_end--;

	sample->default_pan = 0; // 0 = don't use, or 160 = centre?

	if (sample->length <= 0)
		sample->flags &= ~IT_SAMPLE_EXISTS;
	else if (sample->flags & IT_SAMPLE_LOOP) {
		if ((unsigned int)sample->loop_end > (unsigned int)sample->length)
			sample->flags &= ~IT_SAMPLE_LOOP;
		else if ((unsigned int)sample->loop_start >= (unsigned int)sample->loop_end)
			sample->flags &= ~IT_SAMPLE_LOOP;
		else
			sample->length = sample->loop_end;
	}


	//Do we need to set all these?
	sample->vibrato_speed = 0;
	sample->vibrato_depth = 0;
	sample->vibrato_rate = 0;
	sample->vibrato_waveform = IT_VIBRATO_SINE;
	sample->finetune = 0;
	sample->max_resampling_quality = -1;

	return dumbfile_error(f);
}


static int it_ptm_read_byte(DUMBFILE *f)
{
	int meh = dumbfile_getc(f);
	if (meh < 0) return 0;
	return meh;
}

static int it_ptm_read_sample_data(IT_SAMPLE *sample, int last, DUMBFILE *f)
{
	int32 n;
	int s;

	sample->data = malloc(sample->length * (sample->flags & IT_SAMPLE_16BIT ? 2 : 1));
	if (!sample->data)
		return -1;

	s = 0;

	if (sample->flags & IT_SAMPLE_16BIT) {
		unsigned char a, b;
		for (n = 0; n < sample->length; n++) {
			a = s += (signed char) it_ptm_read_byte(f);
			b = s += (signed char) it_ptm_read_byte(f);
			((short *)sample->data)[n] = a | (b << 8);
		}
	} else {
		for (n = 0; n < sample->length; n++) {
			s += (signed char) it_ptm_read_byte(f);
			((signed char *)sample->data)[n] = s;
		}
	}

	if (dumbfile_error(f) && !last)
		return -1;

	return 0;
}



static int it_ptm_read_pattern(IT_PATTERN *pattern, DUMBFILE *f, unsigned char *buffer, int length)
{
	int buflen = 0;
	int bufpos = 0;
	int effect, effectvalue;

	IT_ENTRY *entry;

	unsigned char channel;

	if (!length)
		return -1;

	pattern->n_rows = 0;
	pattern->n_entries = 0;

	/* Read in the pattern data, little by little, and work out how many
	 * entries we need room for. Sorry, but this is just so funny...
	 */
	for (;;) {
		unsigned char b = buffer[buflen++] = dumbfile_getc(f);

#if 1
		static const unsigned char used[8] = {0, 2, 2, 4, 1, 3, 3, 5};
		channel = b & 31;
		b >>= 5;
		pattern->n_entries++;
		if (b) {
			if (buflen + used[b] >= 65536) return -1;
            dumbfile_getnc((char *)buffer + buflen, used[b], f);
			buflen += used[b];
		} else {
			/* End of row */
			if (++pattern->n_rows == 64) break;
			if (buflen >= 65536) return -1;
		}
#else
		if (b == 0) {
			/* End of row */
			pattern->n_entries++;
			if (++pattern->n_rows == 64) break;
			if (buflen >= 65536) return -1;
		} else {
			static const unsigned char used[8] = {0, 2, 2, 4, 1, 3, 3, 5};
			channel = b & 31;
			b >>= 5;
			if (b) {
				pattern->n_entries++;
				if (buflen + used[b] >= 65536) return -1;
				dumbfile_getnc(buffer + buflen, used[b], f);
				buflen += used[b];
			}
		}
#endif

		/* We have ensured that buflen < 65536 at this point, so it is safe
		 * to iterate and read at least one more byte without checking.
		 * However, now would be a good time to check for errors reading from
		 * the file.
		 */

		if (dumbfile_error(f))
			return -1;

		/* Great. We ran out of data, but there should be data for more rows.
		 * Fill the rest with null data...
		 */
		if (buflen >= length && pattern->n_rows < 64)
		{
			while (pattern->n_rows < 64)
			{
				if (buflen >= 65536) return -1;
				buffer[buflen++] = 0;
				pattern->n_entries++;
				pattern->n_rows++;
			}
			break;
		}
	}

	pattern->entry = malloc(pattern->n_entries * sizeof(*pattern->entry));

	if (!pattern->entry)
		return -1;

	entry = pattern->entry;

	while (bufpos < buflen) {
		unsigned char b = buffer[bufpos++];

		if (b == 0)
		{
			/* End of row */
			IT_SET_END_ROW(entry);
			entry++;
			continue;
		}

		channel = b & 31;

		if (b & 224) {
			entry->mask = 0;
			entry->channel = channel;

			if (b & 32) {
				unsigned char n = buffer[bufpos++];
				if (n == 254 || (n >= 1 && n <= 120)) {
					if (n == 254)
						entry->note = IT_NOTE_CUT;
					else
						entry->note = n - 1;
					entry->mask |= IT_ENTRY_NOTE;
				}

				entry->instrument = buffer[bufpos++];
				if (entry->instrument)
					entry->mask |= IT_ENTRY_INSTRUMENT;
			}

			if (b & 64) {
				effect = buffer[bufpos++];
				effectvalue = buffer[bufpos++];
				_dumb_it_ptm_convert_effect(effect, effectvalue, entry);
			}

			if (b & 128) {
				entry->volpan = buffer[bufpos++];
				if (entry->volpan <= 64)
					entry->mask |= IT_ENTRY_VOLPAN;
			}

			entry++;
		}
	}

	ASSERT(entry == pattern->entry + pattern->n_entries);

	return 0;
}



/** WARNING: this is duplicated in itread.c - also bad practice to use the same struct name unless they are unified in a header */
/* Currently we assume the sample data are stored after the sample headers in
 * module files. This assumption may be unjustified; let me know if you have
 * trouble.
 */

#define PTM_COMPONENT_INSTRUMENT 1
#define PTM_COMPONENT_PATTERN    2
#define PTM_COMPONENT_SAMPLE     3

typedef struct PTM_COMPONENT
{
	unsigned char type;
	unsigned char n;
	int32 offset;
}
PTM_COMPONENT;



static int CDECL ptm_component_compare(const void *e1, const void *e2)
{
	return ((const PTM_COMPONENT *)e1)->offset -
	       ((const PTM_COMPONENT *)e2)->offset;
}



static DUMB_IT_SIGDATA *it_ptm_load_sigdata(DUMBFILE *f)
{
	DUMB_IT_SIGDATA *sigdata;

	PTM_COMPONENT *component;
	int n_components = 0;

	int n;

	unsigned char *buffer;

	sigdata = malloc(sizeof(*sigdata));
	if (!sigdata) return NULL;

	/* Skip song name. */
    dumbfile_getnc((char *)sigdata->name, 28, f);
	sigdata->name[28] = 0;

	if (dumbfile_getc(f) != 0x1A || dumbfile_igetw(f) != 0x203) {
		free(sigdata);
		return NULL;
	}

	dumbfile_skip(f, 1);

	sigdata->song_message = NULL;
	sigdata->order = NULL;
	sigdata->instrument = NULL;
	sigdata->sample = NULL;
	sigdata->pattern = NULL;
	sigdata->midi = NULL;
	sigdata->checkpoint = NULL;

	sigdata->n_orders = dumbfile_igetw(f);
	sigdata->n_instruments = 0;
	sigdata->n_samples = dumbfile_igetw(f);
	sigdata->n_patterns = dumbfile_igetw(f);

	if (dumbfile_error(f) || sigdata->n_orders <= 0 || sigdata->n_samples > 255 || sigdata->n_patterns > 128) {
		_dumb_it_unload_sigdata(sigdata);
		return NULL;
	}

	sigdata->n_pchannels = dumbfile_igetw(f);

	if (dumbfile_igetw(f) != 0) {
		_dumb_it_unload_sigdata(sigdata);
		return NULL;
	}

	dumbfile_skip(f, 2);

	if (dumbfile_mgetl(f) != DUMB_ID('P','T','M','F')) {
		_dumb_it_unload_sigdata(sigdata);
		return NULL;
	}

	dumbfile_skip(f, 16);

	sigdata->order = malloc(sigdata->n_orders);
	if (!sigdata->order) {
		_dumb_it_unload_sigdata(sigdata);
		return NULL;
	}

	if (sigdata->n_samples) {
		sigdata->sample = malloc(sigdata->n_samples * sizeof(*sigdata->sample));
		if (!sigdata->sample) {
			_dumb_it_unload_sigdata(sigdata);
			return NULL;
		}
		for (n = 0; n < sigdata->n_samples; n++)
			sigdata->sample[n].data = NULL;
	}

	if (sigdata->n_patterns) {
		sigdata->pattern = malloc(sigdata->n_patterns * sizeof(*sigdata->pattern));
		if (!sigdata->pattern) {
			_dumb_it_unload_sigdata(sigdata);
			return NULL;
		}
		for (n = 0; n < sigdata->n_patterns; n++)
			sigdata->pattern[n].entry = NULL;
	}

	/** WARNING: which ones? */
	sigdata->flags = IT_STEREO | IT_OLD_EFFECTS | IT_COMPATIBLE_GXX | IT_WAS_A_PTM;

	sigdata->global_volume = 128;
	sigdata->speed = 6;
	sigdata->tempo = 125;
	sigdata->mixing_volume = 48;

	/* Panning positions for 32 channels */
	{
		int i;
		for (i = 0; i < 32; i++) {
			int c = dumbfile_getc(f);
			if (c <= 15) {
				sigdata->channel_volume[i] = 64;
				sigdata->channel_pan[i] = c;
			} else {
				/** WARNING: this could be improved if we support channel muting... */
				sigdata->channel_volume[i] = 0;
				sigdata->channel_pan[i] = 7;
			}
		}
	}

	/* Orders, byte each, length = sigdata->n_orders (should be even) */
    dumbfile_getnc((char *)sigdata->order, sigdata->n_orders, f);
	sigdata->restart_position = 0;

	component = malloc(768*sizeof(*component));
	if (!component) {
		_dumb_it_unload_sigdata(sigdata);
		return NULL;
	}

    if (dumbfile_seek(f, 352, DFS_SEEK_SET)) {
		free(component);
		_dumb_it_unload_sigdata(sigdata);
		return NULL;
	}

	for (n = 0; n < sigdata->n_patterns; n++) {
		component[n_components].type = PTM_COMPONENT_PATTERN;
		component[n_components].n = n;
		component[n_components].offset = dumbfile_igetw(f) << 4;
		n_components++;
	}

    if (dumbfile_seek(f, 608, DFS_SEEK_SET)) {
		free(component);
		_dumb_it_unload_sigdata(sigdata);
		return NULL;
	}

	for (n = 0; n < sigdata->n_samples; n++) {
		if (it_ptm_read_sample_header(&sigdata->sample[n], &component[n_components].offset, f)) {
			free(component);
			_dumb_it_unload_sigdata(sigdata);
			return NULL;
		}
		if (!(sigdata->sample[n].flags & IT_SAMPLE_EXISTS)) continue;
		component[n_components].type = PTM_COMPONENT_SAMPLE;
		component[n_components].n = n;
		n_components++;
	}

	qsort(component, n_components, sizeof(PTM_COMPONENT), &ptm_component_compare);

	{
		int i;
		for (i = 0; i < 32; i++) {
			sigdata->channel_pan[i] -= (sigdata->channel_pan[i] & 8) >> 3;
			sigdata->channel_pan[i] = ((int)sigdata->channel_pan[i] << 5) / 7;
			if (sigdata->channel_pan[i] > 64) sigdata->channel_pan[i] = 64;
		}
	}

	sigdata->pan_separation = 128;

	if (dumbfile_error(f)) {
		free(component);
		_dumb_it_unload_sigdata(sigdata);
		return NULL;
	}

	buffer = malloc(65536);
	if (!buffer) {
		free(component);
		_dumb_it_unload_sigdata(sigdata);
		return NULL;
	}

	for (n = 0; n < n_components; n++) {
        if (dumbfile_seek(f, component[n].offset, DFS_SEEK_SET)) {
			free(buffer);
			free(component);
			_dumb_it_unload_sigdata(sigdata);
			return NULL;
		}

		switch (component[n].type) {

			case PTM_COMPONENT_PATTERN:
				if (it_ptm_read_pattern(&sigdata->pattern[component[n].n], f, buffer, (n + 1 < n_components) ? (component[n+1].offset - component[n].offset) : 0)) {
					free(buffer);
					free(component);
					_dumb_it_unload_sigdata(sigdata);
					return NULL;
				}
				break;

			case PTM_COMPONENT_SAMPLE:
				if (it_ptm_read_sample_data(&sigdata->sample[component[n].n], (n + 1 == n_components), f)) {
					free(buffer);
					free(component);
					_dumb_it_unload_sigdata(sigdata);
					return NULL;
				}
		}
	}

	free(buffer);
	free(component);

	_dumb_it_fix_invalid_orders(sigdata);

	return sigdata;
}

DUH *DUMBEXPORT dumb_read_ptm_quick(DUMBFILE *f)
{
	sigdata_t *sigdata;

	DUH_SIGTYPE_DESC *descptr = &_dumb_sigtype_it;

	sigdata = it_ptm_load_sigdata(f);

	if (!sigdata)
		return NULL;

	{
		const char *tag[2][2];
		tag[0][0] = "TITLE";
        tag[0][1] = (const char *)(((DUMB_IT_SIGDATA *)sigdata)->name);
		tag[1][0] = "FORMAT";
		tag[1][1] = "PTM";
		return make_duh(-1, 2, (const char *const (*)[2])tag, 1, &descptr, &sigdata);
	}
}
