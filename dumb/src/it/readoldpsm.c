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
 * readpsm.c - Code to read an old Protracker         / / \  \
 *             Studio module from an open file.      | <  /   \_
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

static int CDECL psm_sample_compare(const void *e1, const void *e2)
{
	const unsigned char * pa = e1;
	const unsigned char * pb = e2;
	int a = pa[37] | (pa[38] << 8) | (pa[39] << 16) | (pa[40] << 24);
	int b = pb[37] | (pb[38] << 8) | (pb[39] << 16) | (pb[40] << 24);
	return a - b;
}

static int it_old_psm_read_samples(IT_SAMPLE ** sample, DUMBFILE * f, int * num)
{
    int n, o, count = *num, true_num, snum, offset, flags, finetune, delta;

    unsigned char * buffer;
	const unsigned char * sdata;
    int32 sample_bytes;

	buffer = malloc(count * 64);
	if (!buffer) goto error;

    if (dumbfile_getnc((char *)buffer, count * 64, f) < count * 64) goto error_fb;

	true_num = 0;

	for (n = 0; n < count; n++) {
		snum = buffer[(n * 64) + 45] | (buffer[(n * 64) + 46] << 8);
		if ((snum < 1) || (snum > 255)) goto error_fb;
		if (true_num < snum) true_num = snum;
	}

	if (true_num > count) {
		IT_SAMPLE * meh = realloc(*sample, true_num * sizeof(*meh));
		if (!meh) goto error_fb;
		for (n = count; n < true_num; n++) {
			meh[n].data = NULL;
		}
		*sample = meh;
		*num = true_num;
	}

	qsort(buffer, count, 64, &psm_sample_compare);

	for (n = 0; n < true_num; n++) {
		(*sample)[n].flags = 0;
	}

	for (n = 0; n < count; n++) {
		IT_SAMPLE * s;
		snum = buffer[(n * 64) + 45] | (buffer[(n * 64) + 46] << 8);
		s = &((*sample)[snum - 1]);
		memcpy(s->filename, buffer + (n * 64), 13);
		s->filename[13] = 0;
		memcpy(s->name, buffer + (n * 64) + 13, 24);
		s->name[24] = 0;
		offset = buffer[(n * 64) + 37] | (buffer[(n * 64) + 38] << 8) |
				 (buffer[(n * 64) + 39] << 16) | (buffer[(n * 64) + 40] << 24);
		flags = buffer[(n * 64) + 47];
		s->length = buffer[(n * 64) + 48] | (buffer[(n * 64) + 49] << 8) |
					(buffer[(n * 64) + 50] << 16) | (buffer[(n * 64) + 51] << 24);
		s->loop_start = buffer[(n * 64) + 52] | (buffer[(n * 64) + 53] << 8) |
						(buffer[(n * 64) + 54] << 16) | (buffer[(n * 64) + 55] << 24);
		s->loop_end = buffer[(n * 64) + 56] | (buffer[(n * 64) + 57] << 8) |
					  (buffer[(n * 64) + 58] << 16) | (buffer[(n * 64) + 59] << 24);

		if (s->length <= 0) continue;

		finetune = buffer[(n * 64) + 60];
		s->default_volume = buffer[(n * 64) + 61];
		s->C5_speed = buffer[(n * 64) + 62] | (buffer[(n * 64) + 63] << 8);
		if (finetune & 15) {
			finetune &= 15;
			if (finetune >= 8) finetune -= 16;
			//s->C5_speed = (long)((double)s->C5_speed * pow(DUMB_PITCH_BASE, finetune*32));
			s->finetune = finetune * 32;
		}
		else s->finetune = 0;

		s->flags |= IT_SAMPLE_EXISTS;
		if (flags & 0x41) {
			s->flags &= ~IT_SAMPLE_EXISTS;
			continue;
		}
		if (flags & 0x20) s->flags |= IT_SAMPLE_PINGPONG_LOOP;
		if (flags & 4) s->flags |= IT_SAMPLE_16BIT;

		if (flags & 0x80) {
			s->flags |= IT_SAMPLE_LOOP;
			if ((unsigned int)s->loop_end > (unsigned int)s->length)
				s->loop_end = s->length;
			else if ((unsigned int)s->loop_start >= (unsigned int)s->loop_end)
				s->flags &= ~IT_SAMPLE_LOOP;
			else
				s->length = s->loop_end;
		}

		s->global_volume = 64;

		s->vibrato_speed = 0;
		s->vibrato_depth = 0;
		s->vibrato_rate = 0;
		s->vibrato_waveform = IT_VIBRATO_SINE;
		s->max_resampling_quality = -1;

        sample_bytes = s->length * ((flags & 4) ? 2 : 1);
        s->data = malloc(sample_bytes);
		if (!s->data) goto error_fb;

        if (dumbfile_seek(f, offset, DFS_SEEK_SET) || dumbfile_getnc(s->data, sample_bytes, f) < sample_bytes) goto error_fb;
        sdata = ( const unsigned char * ) s->data;

		if (flags & 0x10) {
			if (flags & 8) {
				if (flags & 4) {
					for (o = 0; o < s->length; o++)
						((short *)s->data)[o] = (sdata[o * 2] | (sdata[(o * 2) + 1] << 8)) ^ 0x8000;
				} else {
					for (o = 0; o < s->length; o++)
						((signed char *)s->data)[o] = sdata[o] ^ 0x80;
				}
			} else {
				if (flags & 4) {
					for (o = 0; o < s->length; o++)
						((short *)s->data)[o] = sdata[o * 2] | (sdata[(o * 2) + 1] << 8);
				} else {
					memcpy(s->data, sdata, s->length);
				}
			}
		} else {
			delta = 0;
			if (flags & 8) {
				/* unsigned delta? mehhh, does anything even use this? */
				if (flags & 4) {
					for (o = 0; o < s->length; o++) {
						delta += (short)(sdata[o * 2] | (sdata[(o * 2) + 1] << 8));
						((short *)s->data)[o] = delta ^ 0x8000;
					}
				} else {
					for (o = 0; o < s->length; o++) {
						delta += (signed char)sdata[o];
						((signed char *)s->data)[o] = delta ^ 0x80;
					}
				}
			} else {
				if (flags & 4) {
					for (o = 0; o < s->length; o++) {
						delta += (short)(sdata[o * 2] | (sdata[(o * 2) + 1] << 8));
						((short *)s->data)[o] = delta;
					}
				} else {
					for (o = 0; o < s->length; o++) {
						delta += (signed char)sdata[o];
						((signed char *)s->data)[o] = delta;
					}
				}
			}
		}
	}

	free(buffer);

	return 0;

error_fb:
	free(buffer);
error:
	return -1;
}

static int it_old_psm_read_patterns(IT_PATTERN * pattern, DUMBFILE * f, int num, int size, int pchans)
{
	int n, offset, psize, rows, chans, row, flags, channel;

	unsigned char * buffer, * ptr, * end;

	IT_ENTRY * entry;

	buffer = malloc(size);
	if (!buffer) goto error;

    if (dumbfile_getnc((char *)buffer, size, f) < size) goto error_fb;

	offset = 0;

	for (n = 0; n < num; n++) {
		IT_PATTERN * p = &pattern[n];

		if (offset >= size) goto error_fb;

		ptr = buffer + offset;
		psize = ptr[0] | (ptr[1] << 8);
		rows = ptr[2];
		chans = ptr[3];

		if (!rows || !chans) {
			p->n_rows = 1;
			p->n_entries = 0;
			continue;
		}

		psize = (psize + 15) & ~15;

		end = ptr + psize;
		ptr += 4;

		p->n_rows = rows;
		p->n_entries = rows;
		row = 0;

		while ((row < rows) && (ptr < end)) {
			flags = *ptr++;
			if (!flags) {
				row++;
				continue;
			}
			if (flags & 0xE0) {
				p->n_entries++;
				if (flags & 0x80) ptr += 2;
				if (flags & 0x40) ptr++;
				if (flags & 0x20) {
					ptr++;
					if (*ptr == 40) ptr += 3;
					else ptr++;
				}
			}
		}

		entry = malloc(p->n_entries * sizeof(*p->entry));
		if (!entry) goto error_fb;

		p->entry = entry;

		ptr = buffer + offset + 4;
		row = 0;

		while ((row < rows) && (ptr < end)) {
			flags = *ptr++;
			if (!flags) {
				IT_SET_END_ROW(entry);
				entry++;
				row++;
				continue;
			}
			if (flags & 0xE0) {
				entry->mask = 0;
				entry->channel = channel = flags & 0x1F;
				if (channel >= chans)
				{
					//channel = 0;
					//goto error_fb;
				}
				if (flags & 0x80) {
					if ((*ptr < 60) && (channel < pchans)) {
						entry->mask |= IT_ENTRY_NOTE;
						entry->note = *ptr + 35;
					}
					ptr++;
					if (*ptr) {
						entry->mask |= IT_ENTRY_INSTRUMENT;
						entry->instrument = *ptr;
					}
					ptr++;
				}
				if (flags & 0x40) {
					if (*ptr <= 64) {
						entry->mask |= IT_ENTRY_VOLPAN;
						entry->volpan = *ptr;
					}
					ptr++;
				}
				if (flags & 0x20) {
					entry->mask |= IT_ENTRY_EFFECT;

					switch (*ptr) {
						case 1:
							entry->effect = IT_XM_FINE_VOLSLIDE_UP;
							entry->effectvalue = ptr[1];
							break;

						case 2:
							entry->effect = IT_VOLUME_SLIDE;
							entry->effectvalue = (ptr[1] << 4) & 0xF0;
							break;

						case 3:
							entry->effect = IT_XM_FINE_VOLSLIDE_DOWN;
							entry->effectvalue = ptr[1];
							break;

						case 4:
							entry->effect = IT_VOLUME_SLIDE;
							entry->effectvalue = ptr[1] & 0xF;
							break;

						case 10:
							entry->effect = IT_PORTAMENTO_UP;
							entry->effectvalue = EFFECT_VALUE(0xF, ptr[1]);
							break;

						case 11:
							entry->effect = IT_PORTAMENTO_UP;
							entry->effectvalue = ptr[1];
							break;

						case 12:
							entry->effect = IT_PORTAMENTO_DOWN;
							entry->effectvalue = EFFECT_VALUE(ptr[1], 0xF);
							break;

						case 13:
							entry->effect = IT_PORTAMENTO_DOWN;
							entry->effectvalue = ptr[1];
							break;

						case 14:
							entry->effect = IT_TONE_PORTAMENTO;
							entry->effectvalue = ptr[1];
							break;

						case 15:
							entry->effect = IT_S;
							entry->effectvalue = EFFECT_VALUE(IT_S_SET_GLISSANDO_CONTROL, ptr[1] & 15);
							break;

						case 16:
							entry->effect = IT_VOLSLIDE_TONEPORTA;
							entry->effectvalue = ptr[1] << 4;
							break;

						case 17:
							entry->effect = IT_VOLSLIDE_TONEPORTA;
							entry->effectvalue = ptr[1] & 0xF;
							break;

						case 20:
							entry->effect = IT_VIBRATO;
							entry->effectvalue = ptr[1];
							break;

						case 21:
							entry->effect = IT_S;
							entry->effectvalue = EFFECT_VALUE(IT_S_SET_VIBRATO_WAVEFORM, ptr[1] & 11);
							break;

						case 22:
							entry->effect = IT_VOLSLIDE_VIBRATO;
							entry->effectvalue = ptr[1] << 4;
							break;

						case 23:
							entry->effect = IT_VOLSLIDE_VIBRATO;
							entry->effectvalue = ptr[1] & 0xF;
							break;

						case 30:
							entry->effect = IT_TREMOLO;
							entry->effectvalue = ptr[1];
							break;

						case 31:
							entry->effect = IT_S;
							entry->effectvalue = EFFECT_VALUE(IT_S_SET_TREMOLO_WAVEFORM, ptr[1] & 11);
							break;

						case 40:
							entry->effect = IT_SET_SAMPLE_OFFSET;
							entry->effectvalue = ptr[2];
							ptr += 2;
							break;

						case 41:
							entry->effect = IT_XM_RETRIGGER_NOTE;
							entry->effectvalue = ptr[1];
							break;

						case 42:
							entry->effect = IT_S;
							entry->effectvalue = EFFECT_VALUE(IT_S_DELAYED_NOTE_CUT, ptr[1] & 0xF);
							break;

						case 43:
							entry->effect = IT_S;
							entry->effectvalue = EFFECT_VALUE(IT_S_NOTE_DELAY, ptr[1] & 0xF);
							break;

						case 50:
							entry->effect = IT_JUMP_TO_ORDER;
							entry->effectvalue = ptr[1];
							break;

						case 51:
							entry->effect = IT_BREAK_TO_ROW;
							entry->effectvalue = ptr[1];
							break;

						case 52:
							entry->effect = IT_S;
							entry->effectvalue = EFFECT_VALUE(IT_S_PATTERN_LOOP, ptr[1] & 0xF);
							break;

						case 53:
							entry->effect = IT_S;
							entry->effectvalue = EFFECT_VALUE(IT_S_PATTERN_DELAY, ptr[1] & 0xF);
							break;

						case 60:
							entry->effect = IT_SET_SPEED;
							entry->effectvalue = ptr[1];
							break;

						case 61:
							entry->effect = IT_SET_SONG_TEMPO;
							entry->effectvalue = ptr[1];
							break;

						case 70:
							entry->effect = IT_ARPEGGIO;
							entry->effectvalue = ptr[1];
							break;

						case 71:
							entry->effect = IT_S;
							entry->effectvalue = EFFECT_VALUE(IT_S_FINETUNE, ptr[1] & 0xF);
							break;

						case 72:
							/* "balance" ... panning? */
							entry->effect = IT_SET_PANNING;
							entry->effectvalue = ((ptr[1] - ((ptr[1] & 8) >> 3)) << 5) / 7;
							break;

						default:
							entry->mask &= ~IT_ENTRY_EFFECT;
					}

					ptr += 2;
				}
				if (entry->mask) entry++;
			}
		}

		p->n_entries = (int)(entry - p->entry);
		offset += psize;
	}

	free(buffer);

	return 0;

error_fb:
	free(buffer);
error:
	return -1;
}

#define PSM_COMPONENT_ORDERS            0
#define PSM_COMPONENT_PANPOS            1
#define PSM_COMPONENT_PATTERNS          2
#define PSM_COMPONENT_SAMPLE_HEADERS    3
#define PSM_COMPONENT_COMMENTS          4

typedef struct PSM_COMPONENT
{
	unsigned char type;
	int32 offset;
}
PSM_COMPONENT;

static int CDECL psm_component_compare(const void *e1, const void *e2)
{
	return ((const PSM_COMPONENT *)e1)->offset -
	       ((const PSM_COMPONENT *)e2)->offset;
}

static DUMB_IT_SIGDATA *it_old_psm_load_sigdata(DUMBFILE *f)
{
	DUMB_IT_SIGDATA *sigdata;

	PSM_COMPONENT *component;
	int n_components = 0;

	int n, flags, version, pver, n_orders, n_channels, total_pattern_size;

	if (dumbfile_mgetl(f) != DUMB_ID('P','S','M',254)) goto error;

	sigdata = malloc(sizeof(*sigdata));
	if (!sigdata) goto error;

    if (dumbfile_getnc((char *)sigdata->name, 60, f) < 60 ||
		sigdata->name[59] != 0x1A) goto error_sd;
	sigdata->name[59] = 0;

	flags = dumbfile_getc(f);
	version = dumbfile_getc(f);
	pver = dumbfile_getc(f);
	sigdata->speed = dumbfile_getc(f);
	sigdata->tempo = dumbfile_getc(f);
	sigdata->mixing_volume = dumbfile_getc(f);
	sigdata->n_orders = dumbfile_igetw(f);
	n_orders = dumbfile_igetw(f);
	sigdata->n_patterns = dumbfile_igetw(f);
	sigdata->n_samples = dumbfile_igetw(f);
	sigdata->n_pchannels = dumbfile_igetw(f);
	n_channels = dumbfile_igetw(f);

	if (dumbfile_error(f) ||
		(flags & 1) ||
		(version != 1 && version != 0x10) ||
		(pver) ||
		(sigdata->n_orders <= 0) ||
		(sigdata->n_orders > 255) ||
		(n_orders > 255) ||
		(n_orders < sigdata->n_orders) ||
		(sigdata->n_patterns > 255) ||
		(sigdata->n_samples > 255) ||
		(sigdata->n_pchannels > DUMB_IT_N_CHANNELS) ||
		(sigdata->n_pchannels > n_channels) ||
		(n_channels > DUMB_IT_N_CHANNELS))
		goto error_sd;

	sigdata->flags = IT_STEREO | IT_OLD_EFFECTS | IT_COMPATIBLE_GXX;

	sigdata->global_volume = 128;
	sigdata->pan_separation = 128;

	sigdata->song_message = NULL;
	sigdata->order = NULL;
	sigdata->instrument = NULL;
	sigdata->sample = NULL;
	sigdata->pattern = NULL;
	sigdata->midi = NULL;
	sigdata->checkpoint = NULL;

	sigdata->n_instruments = 0;

	sigdata->restart_position = 0;

	sigdata->order = malloc(sigdata->n_orders);
	if (!sigdata->order) goto error_usd;

	if (sigdata->n_samples) {
		sigdata->sample = malloc(sigdata->n_samples * sizeof(*sigdata->sample));
		if (!sigdata->sample) goto error_usd;
		for (n = 0; n < sigdata->n_samples; n++)
			sigdata->sample[n].data = NULL;
	}

	if (sigdata->n_patterns) {
		sigdata->pattern = malloc(sigdata->n_patterns * sizeof(*sigdata->pattern));
		if (!sigdata->pattern) goto error_usd;
		for (n = 0; n < sigdata->n_patterns; n++)
			sigdata->pattern[n].entry = NULL;
	}

	component = malloc(5 * sizeof(*component));
	if (!component) goto error_usd;

	for (n = 0; n < 5; n++) {
		component[n_components].offset = dumbfile_igetl(f);
		if (component[n_components].offset) {
			component[n_components].type = n;
			n_components++;
		}
	}

	if (!n_components) goto error_fc;

	total_pattern_size = dumbfile_igetl(f);
	if (!total_pattern_size) goto error_fc;

	qsort(component, n_components, sizeof(PSM_COMPONENT), &psm_component_compare);

	memset(sigdata->channel_volume, 64, DUMB_IT_N_CHANNELS);

	for (n = 0; n < DUMB_IT_N_CHANNELS; n += 4) {
		int sep = 32 * dumb_it_default_panning_separation / 100;
		sigdata->channel_pan[n  ] = 32 - sep;
		sigdata->channel_pan[n+1] = 32 + sep;
		sigdata->channel_pan[n+2] = 32 + sep;
		sigdata->channel_pan[n+3] = 32 - sep;
	}

	for (n = 0; n < n_components; n++)
	{
		int o;

        if ( dumbfile_seek(f, component[n].offset, DFS_SEEK_SET) ) goto error_fc;

		switch (component[n].type) {

			case PSM_COMPONENT_ORDERS:
                if (dumbfile_getnc((char *)sigdata->order, sigdata->n_orders, f) < sigdata->n_orders) goto error_fc;
				if (n_orders > sigdata->n_orders)
					if (dumbfile_skip(f, n_orders - sigdata->n_orders))
                        goto error_fc;
                if (dumbfile_igetw(f)) goto error_fc;
				break;

			case PSM_COMPONENT_PANPOS:
                if (dumbfile_getnc((char *)sigdata->channel_pan, sigdata->n_pchannels, f) < sigdata->n_pchannels) goto error_fc;
				for (o = 0; o < sigdata->n_pchannels; o++) {
					sigdata->channel_pan[o] -= (sigdata->channel_pan[o] & 8) >> 3;
					sigdata->channel_pan[o] = ((int)sigdata->channel_pan[o] << 5) / 7;
				}
				break;

			case PSM_COMPONENT_PATTERNS:
                if (it_old_psm_read_patterns(sigdata->pattern, f, sigdata->n_patterns, total_pattern_size, sigdata->n_pchannels)) goto error_fc;
				break;

			case PSM_COMPONENT_SAMPLE_HEADERS:
                if (it_old_psm_read_samples(&sigdata->sample, f, &sigdata->n_samples)) goto error_fc;
				break;

			case PSM_COMPONENT_COMMENTS:
				if (dumbfile_mgetl(f) == DUMB_ID('T','E','X','T')) {
					o = dumbfile_igetw(f);
					if (o > 0) {
						sigdata->song_message = malloc(o + 1);
                        if (dumbfile_getnc((char *)sigdata->song_message, o, f) < o) goto error_fc;
						sigdata->song_message[o] = 0;
					}
				}
				break;
		}
	}

	_dumb_it_fix_invalid_orders(sigdata);

	free(component);

	return sigdata;

error_fc:
	free(component);
error_usd:
	_dumb_it_unload_sigdata(sigdata);
	return NULL;
error_sd:
	free(sigdata);
error:
	return NULL;
}

DUH *DUMBEXPORT dumb_read_old_psm_quick(DUMBFILE *f)
{
	sigdata_t *sigdata;

	DUH_SIGTYPE_DESC *descptr = &_dumb_sigtype_it;

	sigdata = it_old_psm_load_sigdata(f);

	if (!sigdata)
		return NULL;

	{
		const char *tag[2][2];
		tag[0][0] = "TITLE";
        tag[0][1] = (const char *)(((DUMB_IT_SIGDATA *)sigdata)->name);
		tag[1][0] = "FORMAT";
		tag[1][1] = "PSM (old)";
		return make_duh(-1, 2, (const char *const (*)[2])tag, 1, &descptr, &sigdata);
	}
}
