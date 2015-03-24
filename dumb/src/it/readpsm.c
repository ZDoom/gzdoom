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
 * readpsm.c - Code to read a Protracker Studio       / / \  \
 *             module from an open file.             | <  /   \_
 *                                                   |  \/ /\   /
 * By Chris Moeller.                                  \_  /  > /
 *                                                      | \ / /
 *                                                      |  ' /
 *                                                       \__/
 */

#include <stdlib.h>
#include <string.h>

#include "dumb.h"
#include "internal/it.h"

#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifdef _MSC_VER
#define snprintf sprintf_s
#endif

#define PSMV_OLD 940730
#define PSMV_NEW 940902

typedef struct _PSMCHUNK
{
	int id;
	int len;
	unsigned char * data;
} PSMCHUNK;

typedef struct _PSMEVENT
{
	int type;
	unsigned char data[8];
} PSMEVENT;

#define PSM_EVENT_END               0
#define PSM_EVENT_PLAY_PATTERN      1
#define PSM_EVENT_JUMP_TO_LINE      4
#define PSM_EVENT_SET_SPEED         7
#define PSM_EVENT_SET_BPM           8
#define PSM_EVENT_SAMPLE_MAP_TABLE 12
#define PSM_EVENT_CHANGE_PAN       13
#define PSM_EVENT_CHANGE_VOL       14

static int it_psm_process_sample(IT_SAMPLE * sample, const unsigned char * data, int len, int id, int version) {
	int flags;
	int insno = 0;
	int length = 0;
	int loopstart = 0;
	int loopend = 0;
	int panpos;
	int defvol = 0;
	int samplerate = 0;

	if (len < 0x60) return -1;

	flags = data[0];

	if (version == PSMV_OLD) {
		memcpy(sample->name, data + 0x0D, 34);
		sample->name[34] = 0;

		insno = data[0x34] | (data[0x35] << 8);
		length = data[0x36] | (data[0x37] << 8) | (data[0x38] << 16) | (data[0x39] << 24);
		loopstart = data[0x3A] | (data[0x3B] << 8) | (data[0x3C] << 16) | (data[0x3D] << 24);
		loopend = data[0x3E] | (data[0x3F] << 8) | (data[0x40] << 16) | (data[0x41] << 24);
		panpos = data[0x43];
		defvol = data[0x44];
		samplerate = data[0x49] | (data[0x4A] << 8) | (data[0x4B] << 16) | (data[0x4C] << 24);
	} else /*if (version == PSMV_NEW)*/ {
		memcpy(sample->name, data + 0x11, 34);
		sample->name[34] = 0;

		insno = data[0x38] | (data[0x39] << 8);
		length = data[0x3A] | (data[0x3B] << 8) | (data[0x3C] << 16) | (data[0x3D] << 24);
		loopstart = data[0x3E] | (data[0x3F] << 8) | (data[0x40] << 16) | (data[0x41] << 24);
		loopend = data[0x42] | (data[0x43] << 8) | (data[0x44] << 16) | (data[0x45] << 24);
		panpos = data[0x48];
		defvol = data[0x49];
		samplerate = data[0x4E] | (data[0x4F] << 8) | (data[0x50] << 16) | (data[0x51] << 24);
	}

	if (insno != id) return -1;

	if (!length) {
		sample->flags &= ~IT_SAMPLE_EXISTS;
		return 0;
	}
	
	if ((length > len - 0x60) || ((flags & 0x7F) != 0)) return -1;

	sample->flags = IT_SAMPLE_EXISTS;
	sample->length = length;
	sample->loop_start = loopstart;
	sample->loop_end = loopend;
	sample->C5_speed = samplerate;
	sample->default_volume = defvol >> 1;
	sample->default_pan = 0;
	sample->filename[0] = 0;
	sample->global_volume = 64;
	sample->vibrato_speed = 0;
	sample->vibrato_depth = 0;
	sample->vibrato_rate = 0;
	sample->vibrato_waveform = IT_VIBRATO_SINE;
	sample->finetune = 0;
	sample->max_resampling_quality = -1;

	if (flags & 0x80) {
		if (((unsigned int)sample->loop_end <= (unsigned int)sample->length) &&
			((unsigned int)sample->loop_start < (unsigned int)sample->loop_end)) {
			sample->length = sample->loop_end;
			sample->flags |= IT_SAMPLE_LOOP;
		}
	}

	sample->data = malloc(sample->length);
	if (!sample->data)
		return -1;

	flags = 0;
	data += 0x60;

	for (insno = 0; insno < sample->length; insno++) {
		flags += (signed char)(*data++);
		((signed char *)sample->data)[insno] = flags;
	}

	return 0;
}

static int it_psm_process_pattern(IT_PATTERN * pattern, const unsigned char * data, int len, int speed, int bpm, const unsigned char * pan, const int * vol, int version) {
	int length, nrows, row, rowlen, pos;
	unsigned flags, chan;
	IT_ENTRY * entry;

	length = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
	if (len > length) len = length;

	if (version == PSMV_OLD) {
		if (len < 10) return -1;
		data += 8;
		len -= 8;
	} else /*if (version == PSMV_NEW)*/ {
		if (len < 14) return -1;
		data += 12;
		len -= 12;
	}

	nrows = data[0] | (data[1] << 8);

	if (!nrows) return 0;

	pattern->n_rows = nrows;

	data += 2;
	len -= 2;

	pattern->n_entries = 0;

	row = 0;
	pos = 2;
	rowlen = data[0] | (data[1] << 8);

	while ((row < nrows) && (pos < len)) {
		if (pos >= rowlen) {
			row++;
			rowlen += data[pos] | (data[pos+1] << 8);
			pos += 2;
			continue;
		}

		flags = data[pos++];
		chan = data[pos++];

		if (chan > 63) return -1;

		if (flags & 0xF0) {
			pattern->n_entries++;
			if (flags & 0x80) pos++;
			if (flags & 0x40) pos++;
			if (flags & 0x20) pos++;
			if (flags & 0x10) {
				switch (data[pos]) {
					case 0x29:
						pos++;
					case 0x33:
						pos++;
					default:
						pos += 2;
				}
			}
		}
	}

	if (!pattern->n_entries) return 0;

	pattern->n_entries += nrows;
	if (speed) pattern->n_entries++;
	if (bpm >= 0x20) pattern->n_entries++;

	for (pos = 0; pos < 32; pos++) {
		if (!(pan[pos*2+1] & 0xF9)) pattern->n_entries++;
		if (vol[pos] != -1) pattern->n_entries++;
	}

	pattern->entry = malloc(pattern->n_entries * sizeof(*pattern->entry));
	if (!pattern->entry) return -1;

	entry = pattern->entry;

	if (speed) {
		entry->channel = 0;
		entry->mask = IT_ENTRY_EFFECT;
		entry->effect = IT_SET_SPEED;
		entry->effectvalue = speed;
		entry++;
	}

	if (bpm >= 0x20) {
		entry->channel = 0;
		entry->mask = IT_ENTRY_EFFECT;
		entry->effect = IT_SET_SONG_TEMPO;
		entry->effectvalue = bpm;
		entry++;
	}

	for (pos = 0; pos < 32; pos++) {
		if (!(pan[pos*2+1] & 0xF9)) {
			entry->channel = pos;
			entry->mask = IT_ENTRY_EFFECT;
			switch (pan[pos*2+1]) {
			case 0:
				entry->effect = IT_SET_PANNING;
				entry->effectvalue = pan[pos*2] ^ 128;
				break;
			case 2:
				entry->effect = IT_S;
				entry->effectvalue = EFFECT_VALUE(IT_S_SET_SURROUND_SOUND,1);
				break;
			case 4:
				entry->effect = IT_SET_PANNING;
				entry->effectvalue = 128;
				break;
			}
			entry++;
		}
		if (vol[pos] != -1) {
			entry->channel = pos;
			entry->mask = IT_ENTRY_EFFECT;
			entry->effect = IT_SET_CHANNEL_VOLUME;
			entry->effectvalue = (vol[pos] + 2) >> 2;
			entry++;
		}
	}

	row = 0;
	pos = 2;
	rowlen = data[0] | (data[1] << 8);

	while ((row < nrows) && (pos < len)) {
		if (pos >= rowlen) {
			IT_SET_END_ROW(entry);
			entry++;
			row++;
			rowlen += data[pos] | (data[pos+1] << 8);
			pos += 2;
			continue;
		}

		flags = data[pos++];
		entry->channel = data[pos++];
		entry->mask = 0;

		if (flags & 0xF0) {
			if (flags & 0x80) {
				entry->mask |= IT_ENTRY_NOTE;
				if (version == PSMV_OLD) {
					if ((data[pos] < 0x80)) entry->note = (data[pos]>>4)*12+(data[pos]&0x0f)+12;
					else entry->mask &= ~IT_ENTRY_NOTE;
				} else /*if (version == PSMV_NEW)*/ {
					if ((data[pos]) && (data[pos] < 84)) entry->note = data[pos] + 35;
					else entry->mask &= ~IT_ENTRY_NOTE;
				}
				pos++;
			}

			if (flags & 0x40) {
				entry->mask |= IT_ENTRY_INSTRUMENT;
				entry->instrument = data[pos++] + 1;
			}

			if (flags & 0x20) {
				entry->mask |= IT_ENTRY_VOLPAN;
				entry->volpan = (data[pos++] + 1) >> 1;
			}

			if (flags & 0x10) {
				entry->mask |= IT_ENTRY_EFFECT;
				length = data[pos+1];
				switch (data[pos]) {
					case 1:
						entry->effect = IT_VOLUME_SLIDE;
						if (version == PSMV_OLD) entry->effectvalue = ((length&0x1e)<<3) | 0xF;
						else /*if (version == PSMV_NEW)*/ entry->effectvalue = (length<<4) | 0xF;
						break;

					case 2:
						entry->effect = IT_VOLUME_SLIDE;
						if (version == PSMV_OLD) entry->effectvalue = (length << 3) & 0xF0;
						else /*if (version == PSMV_NEW)*/ entry->effectvalue = (length << 4) & 0xF0;
						break;

					case 3:
						entry->effect = IT_VOLUME_SLIDE;
						if (version == PSMV_OLD) entry->effectvalue = (length >> 1) | 0xF0;
						else /*if (version == PSMV_NEW)*/ entry->effectvalue = length | 0xF0;
						break;

					case 4:
						entry->effect = IT_VOLUME_SLIDE;
						if (version == PSMV_OLD) entry->effectvalue = (length >> 1) & 0xF;
						else /*if (version == PSMV_NEW)*/ entry->effectvalue = length & 0xF;
						break;

					case 12:
						entry->effect = IT_PORTAMENTO_UP;
						if (version == PSMV_OLD) {
							if (length < 4) entry->effectvalue = length | 0xF0;
							else entry->effectvalue = length >> 2;
						} else /*if (version == PSMV_NEW)*/ {
							entry->effectvalue = length;
						}
						break;

					case 14:
						entry->effect = IT_PORTAMENTO_DOWN;
						if (version == PSMV_OLD) {
							if (length < 4) entry->effectvalue = length | 0xF0;
							else entry->effectvalue = length >> 2;
						} else /*if (version == PSMV_NEW)*/ {
							entry->effectvalue = length;
						}
						break;

					case 15:
						entry->effect = IT_TONE_PORTAMENTO;
						if (version == PSMV_OLD) entry->effectvalue = length >> 2;
						else /*if (version == PSMV_NEW)*/ entry->effectvalue = length;
						break;

					case 0x15:
						entry->effect = IT_VIBRATO;
						entry->effectvalue = length;
						break;

					case 0x18:
						entry->effect = IT_VOLSLIDE_VIBRATO;
						entry->effectvalue = length;
						break;

					case 0x29:
						entry->effect = IT_SET_SAMPLE_OFFSET;
						entry->effectvalue = data[pos+2];
						pos += 2;
						break;

					case 0x2A:
						entry->effect = IT_RETRIGGER_NOTE;
						entry->effectvalue = length;
						break;

					case 0x33:
#if 0
						entry->effect = IT_POSITION_JUMP;
						entry->effectvalue = data[pos+2];
#else
						entry->mask &= ~IT_ENTRY_EFFECT;
#endif
						pos++;
						break;

					case 0x34:
						entry->effect = IT_BREAK_TO_ROW;
						entry->effectvalue = length;
						break;

					case 0x3D:
						entry->effect = IT_SET_SPEED;
						entry->effectvalue = length;
						break;

					case 0x3E:
						if (length >= 0x20) {
							entry->effect = IT_SET_SONG_TEMPO;
							entry->effectvalue = length;
						} else {
							entry->mask &= ~IT_ENTRY_EFFECT;
						}
						break;

					case 0x47:
						entry->effect = IT_ARPEGGIO;
						entry->effectvalue = length;
						break;

					default:
						return -1;
				}
				pos += 2;
			}
			if (entry->mask) entry++;
		}
	}

	while (row < nrows) {
		IT_SET_END_ROW(entry);
		entry++;
		row++;
	}

	pattern->n_entries = (int)(entry - pattern->entry);
	if (!pattern->n_entries) return -1;

	return 0;
}


static void free_chunks(PSMCHUNK * chunk, int count) {
	int n;

	for (n = 0; n < count; n++) {
		if (chunk[n].data)
			free(chunk[n].data);
	}

	free(chunk);
}

static void dumb_it_optimize_orders(DUMB_IT_SIGDATA * sigdata);

static int pattcmp( const unsigned char *, const unsigned char *, size_t );

static DUMB_IT_SIGDATA *it_psm_load_sigdata(DUMBFILE *f, int * ver, int subsong)
{
	DUMB_IT_SIGDATA *sigdata;

	PSMCHUNK *chunk;
	int n_chunks = 0;

	PSMCHUNK *songchunk;
	int n_song_chunks = 0;

	PSMEVENT *event = NULL;
	int n_events = 0;

	unsigned char * ptr;

	int n, length, o;

	int found;

	int n_patterns = 0;

	int first_pattern_line = -1;
	int first_pattern;

	int speed, bpm;
	unsigned char pan[64];
	int vol[32];

	if (dumbfile_mgetl(f) != DUMB_ID('P','S','M',' ')) goto error;

	length = dumbfile_igetl(f);

	if (dumbfile_mgetl(f) != DUMB_ID('F','I','L','E')) goto error;

	chunk = calloc(768, sizeof(*chunk));

	while (length >= 8) {
		chunk[n_chunks].id = dumbfile_mgetl(f);
		n = dumbfile_igetl(f);
		length -= 8;
		if (n < 0 || n > length)
			goto error_fc;
		chunk[n_chunks].len = n;
		if (n) {
			ptr = malloc(n);
			if (!ptr) goto error_fc;
            if (dumbfile_getnc((char *)ptr, n, f) < n)
			{
				free(ptr);
				goto error_fc;
			}
			chunk[n_chunks].data = ptr;
		}
		n_chunks++;
		length -= n;
	}

	if (!n_chunks) goto error_fc;
				
	sigdata = malloc(sizeof(*sigdata));
	if (!sigdata) goto error_fc;

	sigdata->n_patterns = 0;
	sigdata->n_samples = 0;
	sigdata->name[0] = 0;

	found = 0;

	for (n = 0; n < n_chunks; n++) {
		PSMCHUNK * c = &chunk[n];
		switch(c->id) {
		case DUMB_ID('S','D','F','T'):
			/* song data format? */
			if ((found & 1) || (c->len != 8) || memcmp(c->data, "MAINSONG", 8)) goto error_sd;
			found |= 1;
			break;

		case DUMB_ID('S','O','N','G'):
			if (/*(found & 2) ||*/ (c->len < 11) /*|| memcmp(c->data, "MAINSONG", 8)*/) goto error_sd;
			found |= 2;
			break;

		case DUMB_ID('D','S','M','P'):
			sigdata->n_samples++;
			break;

		case DUMB_ID('T','I','T','L'):
			length = min((int)sizeof(sigdata->name) - 1, c->len);
			memcpy(sigdata->name, c->data, length);
			sigdata->name[length] = 0;
		}
	}

	if (found != 3 || !sigdata->n_samples) goto error_sd;

	sigdata->song_message = NULL;
	sigdata->order = NULL;
	sigdata->instrument = NULL;
	sigdata->sample = NULL;
	sigdata->pattern = NULL;
	sigdata->midi = NULL;
	sigdata->checkpoint = NULL;

	sigdata->n_instruments = 0;
	sigdata->n_orders = 0;

	for (n = 0; n < n_chunks; n++) {
		PSMCHUNK * c = &chunk[n];
		if (c->id == DUMB_ID('S','O','N','G')) {
			if (subsong == 0) break;
			subsong--;
		}
	}

	if (n == n_chunks) return NULL;
	subsong = n;

	/*for (n = 0; n < n_chunks; n++) {
		PSMCHUNK * c = &chunk[n];
		if (c->id == DUMB_ID('S','O','N','G')) {*/
	{
		PSMCHUNK * c = &chunk[subsong];
		{
			ptr = c->data;
			if (ptr[10] > 32) goto error_usd;
			sigdata->n_pchannels = ptr[10];
			length = c->len - 11;
			ptr += 11;
			songchunk = 0;
			if (length >= 8) {
				songchunk = malloc(128 * sizeof(*songchunk));
				if (!songchunk) goto error_usd;
				while (length >= 8) {
					songchunk[n_song_chunks].id = DUMB_ID(ptr[0], ptr[1], ptr[2], ptr[3]);
					n = ptr[4] | (ptr[5] << 8) | (ptr[6] << 16) | (ptr[7] << 24);
					length -= 8;
					if (n > length) goto error_sc;
					songchunk[n_song_chunks].len = n;
					songchunk[n_song_chunks].data = ptr + 8;
					n_song_chunks++;
					length -= n;
					ptr += 8 + n;
				}
			}
			/*break;*/
		}
	}

	if (!n_song_chunks) goto error_sc;

	found = 0;

	for (n = 0; n < n_song_chunks; n++) {
		PSMCHUNK * c = &songchunk[n];

		if (c->id == DUMB_ID('D','A','T','E')) {
			/* date of the library build / format spec */
			if (c->len == 6) {
				length = c->len;
				ptr = c->data;
				while (length > 0) {
					if (*ptr >= '0' && *ptr <= '9') {
						found = (found * 10) + (*ptr - '0');
					} else {
						found = 0;
						break;
					}
					ptr++;
					length--;
				}
			}
			break;
		}
	}

	/*
	if (found != 940506 &&
		found != 940509 &&
		found != 940510 &&
		found != 940530 &&
		found != 940629 &&
		found != PSMV_OLD &&
		found != 941011 &&
		found != PSMV_NEW &&
		found != 940906 &&
		found != 940903 &&
		found != 940914 &&
		found != 941213 &&
		found != 800211)   // WTF?
		goto error_sc;
	*/

	*ver = found;

	if (found == 800211 ||
		found == PSMV_NEW ||
		found == 940903 ||
		found == 940906 ||
		found == 940914 ||
		found == 941213) found = PSMV_NEW;
	else found = PSMV_OLD;

	memset(sigdata->channel_volume, 64, DUMB_IT_N_CHANNELS);

	for (n = 0; n < DUMB_IT_N_CHANNELS; n += 4) {
		int sep = 32 * dumb_it_default_panning_separation / 100;
		sigdata->channel_pan[n  ] = 32 - sep;
		sigdata->channel_pan[n+1] = 32 + sep;
		sigdata->channel_pan[n+2] = 32 + sep;
		sigdata->channel_pan[n+3] = 32 - sep;
	}

	for (n = 0; n < n_song_chunks; n++) {
		PSMCHUNK * c = &songchunk[n];

		switch (c->id) {
			case DUMB_ID('O','P','L','H'):
				if (c->len < 2) goto error_sc;
				ptr = c->data;
				o = ptr[0] | (ptr[1] << 8);
				if (!o) goto error_sc;
				event = malloc(o * sizeof(*event));
				if (!event) goto error_sc;
				length = c->len - 2;
				ptr += 2;
				while ((length > 0) && (n_events < o)) {
					event[n_events].type = *ptr;
					switch (*ptr) {
					case PSM_EVENT_END:
						ptr++;
						length--;
						break;

					case PSM_EVENT_PLAY_PATTERN:
						if (found == PSMV_OLD) {
							if (length < 5) goto error_ev;
							memcpy(event[n_events].data, ptr + 1, 4);
							ptr += 5;
							length -= 5;
						} else /*if (found == PSMV_NEW)*/ {
							if (length < 9) goto error_ev;
							memcpy(event[n_events].data, ptr + 1, 8);
							ptr += 9;
							length -= 9;
						}
						break;

					case PSM_EVENT_SET_SPEED:
					case PSM_EVENT_SET_BPM:
						if (length < 2) goto error_ev;
						event[n_events].data[0] = ptr[1];
						ptr += 2;
						length -= 2;
						break;

					case PSM_EVENT_JUMP_TO_LINE:
					case PSM_EVENT_CHANGE_VOL:
						if (length < 3) goto error_ev;
						memcpy(event[n_events].data, ptr + 1, 2);
						ptr += 3;
						length -= 3;
						break;

					case PSM_EVENT_SAMPLE_MAP_TABLE:
						if (length < 7) goto error_ev;
						memcpy(event[n_events].data, ptr + 1, 6);
						ptr += 7;
						length -= 7;
						break;

					case PSM_EVENT_CHANGE_PAN:
						if (length < 4) goto error_ev;
						memcpy(event[n_events].data, ptr + 1, 3);
						ptr += 4;
						length -= 4;
						break;

					default:
						goto error_ev;
					}
					n_events++;
				}
				break;

			case DUMB_ID('P','P','A','N'):
				length = c->len;
				if (length & 1) goto error_ev;
				ptr = c->data;
				o = 0;
				while (length > 0) {
					switch (ptr[0]) {
					case 0:
						sigdata->channel_pan[o] = ((((int)(signed char)ptr[1]) * 32) / 127) + 32;
						break;
					case 2:
						sigdata->channel_pan[o] = IT_SURROUND;
						break;
					case 4:
						sigdata->channel_pan[o] = 32;
						break;
					}
					ptr += 2;
					length -= 2;
					if (++o >= DUMB_IT_N_CHANNELS) break;
				}
				break;

			/*
			case DUMB_ID('P','A','T','T'):
			case DUMB_ID('D','S','A','M'):
			*/
		}
	}

	sigdata->flags = IT_STEREO | IT_OLD_EFFECTS | IT_COMPATIBLE_GXX;

	sigdata->global_volume = 128;
	sigdata->speed = 6;
	sigdata->tempo = 125;
	sigdata->mixing_volume = 48;
	sigdata->pan_separation = 128;

	speed = 0;
	bpm = 0;
	memset(pan, 255, sizeof(pan));
	memset(vol, 255, sizeof(vol));

	sigdata->n_patterns = n_events;
	sigdata->pattern = malloc(sigdata->n_patterns * sizeof(*sigdata->pattern));
	if (!sigdata->pattern) goto error_ev;
	for (n = 0; n < sigdata->n_patterns; n++)
		sigdata->pattern[n].entry = NULL;

	for (n = 0; n < n_events; n++) {
		PSMEVENT * e = &event[n];
		switch (e->type) {
		case PSM_EVENT_END:
			n = n_events;
			break;

		case PSM_EVENT_PLAY_PATTERN:
			for (o = 0; o < n_chunks; o++) {
				PSMCHUNK * c = &chunk[o];
				if (c->id == DUMB_ID('P','B','O','D')) {
					ptr = c->data;
					length = c->len;
					if (found == PSMV_OLD) {
						if (length < 8) goto error_ev;
						if (!pattcmp(ptr + 4, e->data, 4)) {
							if (it_psm_process_pattern(&sigdata->pattern[n_patterns], ptr, length, speed, bpm, pan, vol, found)) goto error_ev;
							if (first_pattern_line < 0) {
								first_pattern_line = n;
								first_pattern = o;
							}
							e->data[0] = n_patterns;
							e->data[1] = n_patterns >> 8;
							n_patterns++;
							break;
						}
					} else /*if (found == PSMV_NEW)*/ {
						if (length < 12) goto error_ev;
						if (!pattcmp(ptr + 4, e->data, 8)) {
							if (it_psm_process_pattern(&sigdata->pattern[n_patterns], ptr, length, speed, bpm, pan, vol, found)) goto error_ev;
							if (first_pattern_line < 0) {
								first_pattern_line = n;
								first_pattern = o;
							}
							e->data[0] = n_patterns;
							e->data[1] = n_patterns >> 8;
							n_patterns++;
							break;
						}
					}
				}
			}
			if (o == n_chunks) goto error_ev;

			speed = 0;
			bpm = 0;
			memset(pan, 255, sizeof(pan));
			memset(vol, 255, sizeof(vol));

			e->type = PSM_EVENT_END;
			break;

		case PSM_EVENT_JUMP_TO_LINE:
			o = e->data[0] | (e->data[1] << 8);
			if (o >= n_events) goto error_ev;
			if (o == 0) {
				/* whew! easy case! */
				sigdata->restart_position = 0;
				n = n_events;
			} else if (o == n) {
				/* freeze */
				n = n_events;
			} else if (o > n) {
				/* jump ahead, setting played event numbers to zero will prevent endless looping */
				n = o - 1;
			} else if (o >= first_pattern_line) {
				/* another semi-easy case */
				sigdata->restart_position = event[o].data[0] | (event[o].data[1] << 8);
				n = n_events;
			} else {
				/* crud, try to simulate rerunning all of the commands from the indicated
				 * line up to the first pattern, then dupe the first pattern again.
				 */
				/*
				PSMCHUNK * c = &chunk[first_pattern];

				for (; o < first_pattern_line; o++) {
					PSMEVENT * ev = &event[o];
					switch (ev->type) {
					case PSM_EVENT_SET_SPEED:
						speed = ev->data[0];
						break;
					case PSM_EVENT_SET_BPM:
						bpm = ev->data[0];
						break;
					case PSM_EVENT_CHANGE_PAN:
						if (ev->data[0] > 31) goto error_ev;
						pan[ev->data[0] * 2] = ev->data[1];
						pan[ev->data[0] * 2 + 1] = ev->data[2];
						break;
					case PSM_EVENT_CHANGE_VOL:
						if (ev->data[0] > 31) goto error_ev;
						vol[ev->data[0]] = ev->data[1];
						break;
					}
				}

				if (it_psm_process_pattern(&sigdata->pattern[n_patterns], c->data, c->len, speed, bpm, pan, vol, found)) goto error_ev;
				n_patterns++;
				sigdata->restart_position = 1;
				n = n_events;

				Eh, what the hell? PSM has no panning commands anyway.
				*/
				sigdata->restart_position = 0;
				n = n_events;
			}
			e->type = PSM_EVENT_END;
			break;

		case PSM_EVENT_SET_SPEED:
			speed = e->data[0];
			break;

		case PSM_EVENT_SET_BPM:
			bpm = e->data[0];
			break;

		case PSM_EVENT_CHANGE_PAN:
			o = e->data[0];
			if (o > 31) goto error_ev;
			pan[o * 2] = e->data[1];
			pan[o * 2 + 1] = e->data[2];
			break;

		case PSM_EVENT_CHANGE_VOL:
			o = e->data[0];
			if (o > 31) goto error_ev;
			vol[o] = e->data[1];
			break;

		case PSM_EVENT_SAMPLE_MAP_TABLE:
			if (e->data[0] != 0 || e->data[1] != 0xFF ||
				e->data[2] != 0 || e->data[3] != 0 ||
				e->data[4] != 1 || e->data[5] != 0)
				goto error_ev;
			break;
		}
	}

	if (n_patterns > 256) goto error_ev;

	sigdata->sample = malloc(sigdata->n_samples * sizeof(*sigdata->sample));
	if (!sigdata->sample) goto error_ev;
	for (n = 0; n < sigdata->n_samples; n++) {
		sigdata->sample[n].data = NULL;
		sigdata->sample[n].flags = 0;
	}

	o = 0;
	for (n = 0; n < n_chunks; n++) {
		PSMCHUNK * c = &chunk[n];
		if (c->id == DUMB_ID('D','S','M','P')) {
			if (it_psm_process_sample(&sigdata->sample[o], c->data, c->len, o, found)) goto error_ev;
			o++;
		}
	}

	sigdata->n_orders = n_patterns;
	sigdata->n_patterns = n_patterns;

	sigdata->order = malloc(n_patterns);

	for (n = 0; n < n_patterns; n++) {
		sigdata->order[n] = n;
	}

	free(event);
	free(songchunk);
	free_chunks(chunk, n_chunks);

	_dumb_it_fix_invalid_orders(sigdata);

	dumb_it_optimize_orders(sigdata);

	return sigdata;

error_ev:
	free(event);
error_sc:
	if (songchunk) free(songchunk);
error_usd:
	_dumb_it_unload_sigdata(sigdata);
	goto error_fc;
error_sd:
	free(sigdata);
error_fc:
	free_chunks(chunk, n_chunks);
error:
	return NULL;
}

static int CDECL it_order_compare(const void *e1, const void *e2) {
	if (*((const char *)e1) < *((const char *)e2))
		return -1;

	if (*((const char *)e1) > *((const char *)e2))
		return 1;

	return 0;
}

/*
static int it_optimize_compare(const void *e1, const void *e2) {
	if (((const IT_ENTRY *)e1)->channel < ((const IT_ENTRY *)e2)->channel)
		return -1;

	if (((const IT_ENTRY *)e1)->channel > ((const IT_ENTRY *)e2)->channel)
		return 1;

	return 0;
}
*/

static int CDECL it_entry_compare(const IT_ENTRY * e1, const IT_ENTRY * e2) {
	if (IT_IS_END_ROW(e1) && IT_IS_END_ROW(e2)) return 1;
	if (e1->channel != e2->channel) return 0;
	if (e1->mask != e2->mask) return 0;
	if ((e1->mask & IT_ENTRY_NOTE) && (e1->note != e2->note)) return 0;
	if ((e1->mask & IT_ENTRY_INSTRUMENT) && (e1->instrument != e2->instrument)) return 0;
	if ((e1->mask & IT_ENTRY_VOLPAN) && (e1->volpan != e2->volpan)) return 0;
	if ((e1->mask & IT_ENTRY_EFFECT) && ((e1->effect != e2->effect) || (e1->effectvalue != e2->effectvalue))) return 0;
	return 1;
}

/*
static void dumb_it_optimize_pattern(IT_PATTERN * pattern) {
	IT_ENTRY * entry, * end;
	IT_ENTRY * rowstart, * rowend;
	IT_ENTRY * current;

	if (!pattern->n_entries || !pattern->entry) return;

	current = entry = pattern->entry;
	end = entry + pattern->n_entries;

	while (entry < end) {
		rowstart = entry;
		while (!IT_IS_END_ROW(entry)) entry++;
		rowend = entry;
		if (rowend > rowstart + 1)
			qsort(rowstart, rowend - rowstart, sizeof(IT_ENTRY), &it_optimize_compare);
		entry = rowstart;
		while (entry < rowend) {
			if (!(entry->mask)) {}
			else if (it_entry_compare(entry, current)) {}
			else if (!(current->mask) ||
					 ((entry->channel == current->channel) &&
					 ((entry->mask | current->mask) == (entry->mask ^ current->mask)))) {
				current->mask |= entry->mask;
				if (entry->mask & IT_ENTRY_NOTE) current->note = entry->note;
				if (entry->mask & IT_ENTRY_INSTRUMENT) current->instrument = entry->instrument;
				if (entry->mask & IT_ENTRY_VOLPAN) current->volpan = entry->volpan;
				if (entry->mask & IT_ENTRY_EFFECT) {
					current->effect = entry->effect;
					current->effectvalue = entry->effectvalue;
				}
			} else {
				if (++current < entry) *current = *entry;
			}
			entry++;
		}
		if (++current < entry) *current = *entry;
		entry++;
	}

	current++;

	if (current < end) {
		IT_ENTRY * opt;
		pattern->n_entries = current - pattern->entry;
		opt = realloc(pattern->entry, pattern->n_entries * sizeof(*pattern->entry));
		if (opt) pattern->entry = opt;
	}
}
*/

static int it_pattern_compare(const IT_PATTERN * p1, const IT_PATTERN * p2) {
	IT_ENTRY * e1, * end;
	IT_ENTRY * e2;

	if (p1 == p2) return 1;
	if (p1->n_entries != p2->n_entries) return 0;
	
	e1 = p1->entry; end = e1 + p1->n_entries;
	e2 = p2->entry;

	while (e1 < end) {
		if (!it_entry_compare(e1, e2)) return 0;
		e1++; e2++;
	}

	return 1;
}

static void dumb_it_optimize_orders(DUMB_IT_SIGDATA * sigdata) {
	int n, o, p;

    /*int last_invalid = (sigdata->flags & IT_WAS_AN_XM) ? 255 : 253;*/

	unsigned char * order_list;
	int n_patterns;

	IT_PATTERN * pattern;

	if (!sigdata->n_orders || !sigdata->n_patterns) return;

	n_patterns = 0;
	order_list = malloc(sigdata->n_orders);

	if (!order_list) return;

	for (n = 0; n < sigdata->n_orders; n++) {
		if (sigdata->order[n] < sigdata->n_patterns) {
			for (o = 0; o < n_patterns; o++) {
				if (sigdata->order[n] == order_list[o]) break;
			}
			if (o == n_patterns) {
				order_list[n_patterns++] = sigdata->order[n];
			}
		}
	}

	if (!n_patterns) {
		free(order_list);
		return;
	}

	/*for (n = 0; n < n_patterns; n++) {
		dumb_it_optimize_pattern(&sigdata->pattern[order_list[n]]);
	}*/

	for (n = 0; n < n_patterns; n++) {
		for (o = n + 1; o < n_patterns; o++) {
			if ((order_list[n] != order_list[o]) &&
				it_pattern_compare(&sigdata->pattern[order_list[n]], &sigdata->pattern[order_list[o]])) {
				for (p = 0; p < sigdata->n_orders; p++) {
					if (sigdata->order[p] == order_list[o]) {
						sigdata->order[p] = order_list[n];
					}
				}
				for (p = o + 1; p < n_patterns; p++) {
					if (order_list[p] == order_list[o]) {
						order_list[p] = order_list[n];
					}
				}
				order_list[o] = order_list[n];
			}
		}
	}

	qsort(order_list, n_patterns, sizeof(*order_list), &it_order_compare);

	for (n = 0, o = 0; n < n_patterns; n++) {
		if (order_list[n] != order_list[o]) {
			if (++o < n) order_list[o] = order_list[n];
		}
	}

	n_patterns = o + 1;

	pattern = malloc(n_patterns * sizeof(*pattern));
	if (!pattern) {
		free(order_list);
		return;
	}

	for (n = 0; n < n_patterns; n++) {
		pattern[n] = sigdata->pattern[order_list[n]];
	}

	for (n = 0; n < sigdata->n_patterns; n++) {
		for (o = 0; o < n_patterns; o++) {
			if (order_list[o] == n) break;
		}
		if (o == n_patterns) {
			if (sigdata->pattern[n].entry)
				free(sigdata->pattern[n].entry);
		}
	}

	free(sigdata->pattern);
	sigdata->pattern = pattern;
	sigdata->n_patterns = n_patterns;

	for (n = 0; n < sigdata->n_orders; n++) {
		for (o = 0; o < n_patterns; o++) {
			if (sigdata->order[n] == order_list[o]) {
				sigdata->order[n] = o;
				break;
			}
		}
	}

	free(order_list);
}

int DUMBEXPORT dumb_get_psm_subsong_count(DUMBFILE *f) {
	int length, subsongs;
	int32 l;
	
	if (dumbfile_mgetl(f) != DUMB_ID('P','S','M',' ')) return 0;

	length = dumbfile_igetl(f);

	if (dumbfile_mgetl(f) != DUMB_ID('F','I','L','E')) return 0;

	subsongs = 0;

	while (length >= 8 && !dumbfile_error(f)) {
		if (dumbfile_mgetl(f) == DUMB_ID('S','O','N','G')) subsongs++;
		l = dumbfile_igetl(f);
		dumbfile_skip(f, l);
		length -= l + 8;
	}

	if (dumbfile_error(f)) return 0;

	return subsongs;
}



/* Eww */
int pattcmp( const unsigned char * a, const unsigned char * b, size_t l )
{
	size_t i, j;
	int na = 0, nb = 0, k;
	char * p;

	k = memcmp( a, b, l );
	if ( !k ) return k;

	/* damnit */

	for ( i = 0; i < l; ++i )
	{
		if ( a [i] >= '0' && a [i] <= '9' ) break;
	}

	if ( i < l )
	{
		na = strtoul( (const char *)a + i, &p, 10 );
		if ( p == (const char *)a + i ) return 1;
	}

	for ( j = 0; j < l; ++j )
	{
		if ( b [j] >= '0' && b [j] <= '9' ) break;
	}

	if ( j < l )
	{
		nb = strtoul( (const char *)b + j, &p, 10 );
		if ( p == (const char *)b + j ) return -1;
	}

	if ( i < j ) return -1;
	else if ( j > i ) return 1;

	k = memcmp( a, b, j );
	if ( k ) return k;

	return na - nb;
}



DUH *DUMBEXPORT dumb_read_psm_quick(DUMBFILE *f, int subsong)
{
	sigdata_t *sigdata;
	int ver;

	DUH_SIGTYPE_DESC *descptr = &_dumb_sigtype_it;

	sigdata = it_psm_load_sigdata(f, &ver, subsong);

	if (!sigdata)
		return NULL;

	{
		int n_tags = 2;
		char version[16];
		const char *tag[3][2];
		tag[0][0] = "TITLE";
        tag[0][1] = (const char *)(((DUMB_IT_SIGDATA *)sigdata)->name);
		tag[1][0] = "FORMAT";
		tag[1][1] = "PSM";
		if ( ver )
		{
			tag[2][0] = "FORMATVERSION";
#if NEED_ITOA
            snprintf( version, 15, "%u", ver );
            version[15] = 0;
#else
			itoa(ver, version, 10);
#endif
			tag[2][1] = (const char *) &version;
			++n_tags;
		}
		return make_duh(-1, n_tags, (const char *const (*)[2])tag, 1, &descptr, &sigdata);
	}
}
