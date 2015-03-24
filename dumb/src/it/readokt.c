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
 * readokt.c - Code to read an Oktalyzer module       / / \  \
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



static int it_okt_read_pattern(IT_PATTERN *pattern, const unsigned char *data, int length, int n_channels)
{
	int pos;
	int channel;
	int row;
	int n_rows;
	IT_ENTRY *entry;

	if (length < 2) return -1;

	n_rows = (data[0] << 8) | data[1];
	if (!n_rows) n_rows = 64;

	if (length < 2 + (n_rows * n_channels * 4)) return -1;

	pattern->n_rows = n_rows;

	/* compute number of entries */
	pattern->n_entries = n_rows; /* Account for the row end markers */
	pos = 2;
	for (row = 0; row < pattern->n_rows; row++) {
		for (channel = 0; channel < n_channels; channel++) {
			if (data[pos+0] | data[pos+2])
				pattern->n_entries++;
			pos += 4;
		}
	}

	pattern->entry = (IT_ENTRY *) malloc(pattern->n_entries * sizeof(*pattern->entry));
	if (!pattern->entry)
		return -1;

	entry = pattern->entry;
	pos = 2;
	for (row = 0; row < n_rows; row++) {
		for (channel = 0; channel < n_channels; channel++) {
			if (data[pos+0] | data[pos+2]) {
				entry->channel = channel;
				entry->mask = 0;

				if (data[pos+0] > 0 && data[pos+0] <= 36) {
					entry->mask |= IT_ENTRY_NOTE | IT_ENTRY_INSTRUMENT;

					entry->note = data[pos+0] + 35;
					entry->instrument = data[pos+1] + 1;
				}

				entry->effect = 0;
				entry->effectvalue = data[pos+3];

				switch (data[pos+2]) {
				case  2: if (data[pos+3]) entry->effect = IT_PORTAMENTO_DOWN; break; // XXX code calls this rs_portu, but it's adding to the period, which decreases the pitch
				case 13: if (data[pos+3]) entry->effect = IT_OKT_NOTE_SLIDE_DOWN; break;
				case 21: if (data[pos+3]) entry->effect = IT_OKT_NOTE_SLIDE_DOWN_ROW; break;

				case  1: if (data[pos+3]) entry->effect = IT_PORTAMENTO_UP; break;   // XXX same deal here, increasing the pitch
				case 17: if (data[pos+3]) entry->effect = IT_OKT_NOTE_SLIDE_UP; break;
				case 30: if (data[pos+3]) entry->effect = IT_OKT_NOTE_SLIDE_UP_ROW; break;

				case 10: if (data[pos+3]) entry->effect = IT_OKT_ARPEGGIO_3; break;
				case 11: if (data[pos+3]) entry->effect = IT_OKT_ARPEGGIO_4; break;
				case 12: if (data[pos+3]) entry->effect = IT_OKT_ARPEGGIO_5; break;

				case 15: entry->effect = IT_S; entry->effectvalue = EFFECT_VALUE(IT_S_SET_FILTER, data[pos+3] & 0x0F); break;

				case 25: entry->effect = IT_JUMP_TO_ORDER; break;

				case 27: entry->note = IT_NOTE_OFF; entry->mask |= IT_ENTRY_NOTE; break;

				case 28: entry->effect = IT_SET_SPEED; break;

				case 31:
					if ( data[pos+3] <= 0x40 ) entry->effect = IT_SET_CHANNEL_VOLUME;
					else if ( data[pos+3] <= 0x50 ) { entry->effect = IT_OKT_VOLUME_SLIDE_DOWN; entry->effectvalue = data[pos+3] - 0x40; }
					else if ( data[pos+3] <= 0x60 ) { entry->effect = IT_OKT_VOLUME_SLIDE_UP;   entry->effectvalue = data[pos+3] - 0x50; }
					else if ( data[pos+3] <= 0x70 ) { entry->effect = IT_OKT_VOLUME_SLIDE_DOWN; entry->effectvalue = data[pos+3] - 0x50; }
					else if ( data[pos+3] <= 0x80 ) { entry->effect = IT_OKT_VOLUME_SLIDE_UP;   entry->effectvalue = data[pos+3] - 0x60; }
					break;
				}

				if ( entry->effect ) entry->mask |= IT_ENTRY_EFFECT;

				entry++;
			}
			pos += 4;
		}
		IT_SET_END_ROW(entry);
		entry++;
	}

	return 0;
}



static void it_okt_read_sample_header(IT_SAMPLE *sample, const unsigned char * data)
{
	int loop_start, loop_length;

	memcpy(sample->name, data, 20);
	sample->name[20] = 0;

	sample->filename[0] = 0;

	sample->length = (data[20] << 24) | (data[21] << 16) | (data[22] << 8) | data[23];
	sample->global_volume = 64;
	sample->default_volume = data[29];
	loop_start = ((data[24] << 8) | data[25]) << 1;
	loop_length = ((data[26] << 8) | data[27]) << 1;
	sample->sus_loop_start = loop_start;
	sample->sus_loop_end = loop_start + loop_length;

	if (sample->length <= 0) {
		sample->flags = 0;
		return;
	}

	sample->flags = IT_SAMPLE_EXISTS;

	sample->default_pan = 0;
	sample->C5_speed = (int)( AMIGA_CLOCK / 214.0 ); //(long)(16726.0*pow(DUMB_PITCH_BASE, finetune*32));
	sample->finetune = 0;

	if (sample->sus_loop_end > sample->length)
		sample->sus_loop_end = sample->length;

	if (loop_length > 2)
		sample->flags |= IT_SAMPLE_SUS_LOOP;

	sample->vibrato_speed = 0;
	sample->vibrato_depth = 0;
	sample->vibrato_rate = 0;
	sample->vibrato_waveform = 0; // do we have to set _all_ these?
	sample->max_resampling_quality = -1;
}



static int it_okt_read_sample_data(IT_SAMPLE *sample, const char * data, int length)
{
	if (length && sample->length) {
		if (length < sample->length) {
			sample->length = length;
			if (length < sample->sus_loop_end) sample->sus_loop_end = length;
		}

		sample->data = malloc(length);

		if (!sample->data)
			return -1;

		memcpy(sample->data, data, length);
	}

	return 0;
}



typedef struct IFF_CHUNK IFF_CHUNK;
typedef struct IFF_CHUNKED IFF_CHUNKED;

struct IFF_CHUNK
{
	unsigned type;
	unsigned char * data;
	unsigned size;
};

struct IFF_CHUNKED
{
	unsigned chunk_count;
	IFF_CHUNK * chunks;
};



static IFF_CHUNKED *dumbfile_read_okt(DUMBFILE *f)
{
	IFF_CHUNKED *mod = (IFF_CHUNKED *) malloc(sizeof(*mod));
	if (!mod) return NULL;

	mod->chunk_count = 0;
	mod->chunks = 0;

	for (;;)
	{
		long bytes_read;
		IFF_CHUNK * chunk = ( IFF_CHUNK * ) realloc( mod->chunks, ( mod->chunk_count + 1 ) * sizeof( IFF_CHUNK ) );
		if ( !chunk )
		{
			if ( mod->chunks ) free( mod->chunks );
			free( mod );
			return NULL;
		}
		mod->chunks = chunk;
		chunk += mod->chunk_count;

		bytes_read = dumbfile_mgetl( f );
		if ( bytes_read < 0 ) break;

		chunk->type = bytes_read;
		chunk->size = dumbfile_mgetl( f );

		if ( dumbfile_error( f ) ) break;

		chunk->data = (unsigned char *) malloc( chunk->size );
		if ( !chunk->data )
		{
			free( mod->chunks );
			free( mod );
			return NULL;
		}

		bytes_read = dumbfile_getnc( ( char * ) chunk->data, chunk->size, f );
		if ( bytes_read < (long)chunk->size )
		{
			if ( bytes_read <= 0 ) {
				free( chunk->data );
				break;
			} else {
				chunk->size = bytes_read;
				mod->chunk_count++;
				break;
			}
		}

		mod->chunk_count++;
	}

	if ( !mod->chunk_count ) {
		if ( mod->chunks ) free(mod->chunks);
		free(mod);
		mod = NULL;
	}

	return mod;
}

void free_okt(IFF_CHUNKED * mod)
{
	unsigned i;
	if (mod)
	{
		if (mod->chunks)
		{
			for (i = 0; i < mod->chunk_count; i++)
			{
				if (mod->chunks[i].data) free(mod->chunks[i].data);
			}
			free(mod->chunks);
		}
		free(mod);
	}
}

const IFF_CHUNK * get_chunk_by_type(IFF_CHUNKED * mod, unsigned type, unsigned offset)
{
	unsigned i;
	if (mod)
	{
		if (mod->chunks)
		{
			for (i = 0; i < mod->chunk_count; i++)
			{
				if (mod->chunks[i].type == type)
				{
					if (!offset) return &mod->chunks[i];
					else offset--;
				}
			}
		}
	}
	return NULL;
}

unsigned get_chunk_count(IFF_CHUNKED *mod, unsigned type)
{
	unsigned i, count = 0;
	if (mod)
	{
		if (mod->chunks)
		{
			for (i = 0; i < mod->chunk_count; i++)
			{
				if (mod->chunks[i].type == type) count++;
			}
		}
	}
	return count;
}


static DUMB_IT_SIGDATA *it_okt_load_sigdata(DUMBFILE *f)
{
	DUMB_IT_SIGDATA *sigdata;
    int n_channels;
    int i, j, k, l;
	IFF_CHUNKED *mod;
	const IFF_CHUNK *chunk;

	char signature[8];

	if (dumbfile_getnc(signature, 8, f) < 8 ||
		memcmp(signature, "OKTASONG", 8)) {
		return NULL;
	}

	mod = dumbfile_read_okt(f);
	if (!mod)
		return NULL;

	sigdata = (DUMB_IT_SIGDATA *) malloc(sizeof(*sigdata));
	if (!sigdata) {
		free_okt(mod);
		return NULL;
	}

	sigdata->name[0] = 0;

	chunk = get_chunk_by_type(mod, DUMB_ID('S','P','E','E'), 0);
	if (!chunk || chunk->size < 2) {
		free(sigdata);
		free_okt(mod);
		return NULL;
	}

	sigdata->speed = (chunk->data[0] << 8) | chunk->data[1];

	chunk = get_chunk_by_type(mod, DUMB_ID('S','A','M','P'), 0);
	if (!chunk || chunk->size < 32) {
		free(sigdata);
		free_okt(mod);
		return NULL;
	}

	sigdata->n_samples = chunk->size / 32;

	chunk = get_chunk_by_type(mod, DUMB_ID('C','M','O','D'), 0);
	if (!chunk || chunk->size < 8) {
		free(sigdata);
		free_okt(mod);
		return NULL;
	}

	n_channels = 0;

	for (i = 0; i < 4; i++) {
		j = (chunk->data[i * 2] << 8) | chunk->data[i * 2 + 1];
		if (!j) n_channels++;
		else if (j == 1) n_channels += 2;
	}

	if (!n_channels) {
		free(sigdata);
		free_okt(mod);
		return NULL;
	}

	sigdata->n_pchannels = n_channels;

	sigdata->sample = (IT_SAMPLE *) malloc(sigdata->n_samples * sizeof(*sigdata->sample));
	if (!sigdata->sample) {
		free(sigdata);
		free_okt(mod);
		return NULL;
	}

	sigdata->song_message = NULL;
	sigdata->order = NULL;
	sigdata->instrument = NULL;
	sigdata->pattern = NULL;
	sigdata->midi = NULL;
	sigdata->checkpoint = NULL;

	sigdata->n_instruments = 0;

	for (i = 0; (unsigned)i < (unsigned)sigdata->n_samples; i++)
		sigdata->sample[i].data = NULL;

	chunk = get_chunk_by_type(mod, DUMB_ID('S','A','M','P'), 0);

	for (i = 0; (unsigned)i < (unsigned)sigdata->n_samples; i++) {
		it_okt_read_sample_header(&sigdata->sample[i], chunk->data + 32 * i);
	}

	sigdata->restart_position = 0;

	chunk = get_chunk_by_type(mod, DUMB_ID('P','L','E','N'), 0);
	if (!chunk || chunk->size < 2) {
		_dumb_it_unload_sigdata(sigdata);
		free_okt(mod);
		return NULL;
	}

	sigdata->n_orders = (chunk->data[0] << 8) | chunk->data[1];
	// what if this is > 128?

	if (sigdata->n_orders <= 0 || sigdata->n_orders > 128) {
		_dumb_it_unload_sigdata(sigdata);
		free_okt(mod);
		return NULL;
	}

	chunk = get_chunk_by_type(mod, DUMB_ID('P','A','T','T'), 0);
    if (!chunk || chunk->size < (unsigned)sigdata->n_orders) {
		_dumb_it_unload_sigdata(sigdata);
		free_okt(mod);
		return NULL;
	}

	sigdata->order = (unsigned char *) malloc(sigdata->n_orders);
	if (!sigdata->order) {
		_dumb_it_unload_sigdata(sigdata);
		free_okt(mod);
		return NULL;
	}

	memcpy(sigdata->order, chunk->data, sigdata->n_orders);

	/* Work out how many patterns there are. */
	chunk = get_chunk_by_type(mod, DUMB_ID('S','L','E','N'), 0);
	if (!chunk || chunk->size < 2) {
		_dumb_it_unload_sigdata(sigdata);
		free_okt(mod);
		return NULL;
	}

	sigdata->n_patterns = (chunk->data[0] << 8) | chunk->data[1];

	j = get_chunk_count(mod, DUMB_ID('P','B','O','D'));
	if (sigdata->n_patterns > (int)j) sigdata->n_patterns = (int)j;

	if (!sigdata->n_patterns) {
		_dumb_it_unload_sigdata(sigdata);
		free_okt(mod);
		return NULL;
	}

	sigdata->pattern = (IT_PATTERN *) malloc(sigdata->n_patterns * sizeof(*sigdata->pattern));
	if (!sigdata->pattern) {
		_dumb_it_unload_sigdata(sigdata);
		free_okt(mod);
		return NULL;
	}
	for (i = 0; (unsigned)i < (unsigned)sigdata->n_patterns; i++)
		sigdata->pattern[i].entry = NULL;

	/* Read in the patterns */
	for (i = 0; (unsigned)i < (unsigned)sigdata->n_patterns; i++) {
		chunk = get_chunk_by_type(mod, DUMB_ID('P','B','O','D'), i);
		if (it_okt_read_pattern(&sigdata->pattern[i], chunk->data, chunk->size, n_channels) != 0) {
			_dumb_it_unload_sigdata(sigdata);
			free_okt(mod);
			return NULL;
		}
	}

	/* And finally, the sample data */
	k = get_chunk_count(mod, DUMB_ID('S','B','O','D'));
	for (i = 0, j = 0; (unsigned)i < (unsigned)sigdata->n_samples && j < k; i++) {
		if (sigdata->sample[i].flags & IT_SAMPLE_EXISTS) {
			chunk = get_chunk_by_type(mod, DUMB_ID('S','B','O','D'), j);
			if (it_okt_read_sample_data(&sigdata->sample[i], (const char *)chunk->data, chunk->size)) {
				_dumb_it_unload_sigdata(sigdata);
				free_okt(mod);
				return NULL;
			}
			j++;
		}
	}
	for (; (unsigned)i < (unsigned)sigdata->n_samples; i++) {
		sigdata->sample[i].flags = 0;
	}

	chunk = get_chunk_by_type(mod, DUMB_ID('C','M','O','D'), 0);

	for (i = 0, j = 0; i < n_channels && j < 4; j++) {
		k = (chunk->data[j * 2] << 8) | chunk->data[j * 2 + 1];
		l = (j == 1 || j == 2) ? 48 : 16;
		if (k == 0) {
			sigdata->channel_pan[i++] = l;
		}
		else if (k == 1) {
			sigdata->channel_pan[i++] = l;
			sigdata->channel_pan[i++] = l;
		}
	}

	free_okt(mod);

	/* Now let's initialise the remaining variables, and we're done! */
	sigdata->flags = IT_WAS_AN_OKT | IT_WAS_AN_XM | IT_WAS_A_MOD | IT_OLD_EFFECTS | IT_COMPATIBLE_GXX | IT_STEREO;

	sigdata->global_volume = 128;
	sigdata->mixing_volume = 48;
	/* We want 50 ticks per second; 50/6 row advances per second;
	 * 50*10=500 row advances per minute; 500/4=125 beats per minute.
	 */
	sigdata->tempo = 125;
	sigdata->pan_separation = 128;

	memset(sigdata->channel_volume, 64, DUMB_IT_N_CHANNELS);
	memset(sigdata->channel_pan + n_channels, 32, DUMB_IT_N_CHANNELS - n_channels);

	_dumb_it_fix_invalid_orders(sigdata);

	return sigdata;
}



DUH *DUMBEXPORT dumb_read_okt_quick(DUMBFILE *f)
{
	sigdata_t *sigdata;

	DUH_SIGTYPE_DESC *descptr = &_dumb_sigtype_it;

    sigdata = it_okt_load_sigdata(f);

	if (!sigdata)
		return NULL;

	{
		const char *tag[1][2];
		tag[0][0] = "FORMAT";
		tag[0][1] = "Oktalyzer";
		return make_duh(-1, 1, (const char *const (*)[2])tag, 1, &descptr, &sigdata);
	}
}
