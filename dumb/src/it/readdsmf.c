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
 * readam.c - Code to read a RIFF DSMF module         / / \  \
 *             from a parsed RIFF structure.         | <  /   \_
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
#include "internal/riff.h"

DUH *dumb_read_riff_dsmf( struct riff * stream );

static int it_riff_dsmf_process_sample( IT_SAMPLE * sample, const unsigned char * data, int len )
{
	int flags;

	memcpy( sample->filename, data, 13 );
	sample->filename[ 13 ] = 0;

	flags = data[ 13 ] | ( data[ 14 ] << 8 );
	sample->default_volume = data[ 15 ];
	sample->length = data[ 16 ] | ( data[ 17 ] << 8 ) | ( data[ 18 ] << 16 ) | ( data[ 19 ] << 24 );
	sample->loop_start = data[ 20 ] | ( data[ 21 ] << 8 ) | ( data[ 22 ] << 16 ) | ( data[ 23 ] << 24 );
	sample->loop_end = data[ 24 ] | ( data[ 25 ] << 8 ) | ( data[ 26 ] << 16 ) | ( data[ 27 ] << 24 );
	sample->C5_speed = ( data[ 32 ] | ( data[ 33 ] << 8 ) ) * 2;
	memcpy( sample->name, data + 36, 28 );
	sample->name[ 28 ] = 0;

	/*if ( data[ 0x38 ] || data[ 0x39 ] || data[ 0x3A ] || data[ 0x3B ] )
		return -1;*/

	if ( ! sample->length ) {
		sample->flags &= ~IT_SAMPLE_EXISTS;
		return 0;
	}

	/*if ( flags & ~( 2 | 1 ) )
		return -1;*/

	if ( sample->length + 64 > len )
		return -1;

	sample->flags = IT_SAMPLE_EXISTS;

	sample->default_pan = 0;
	sample->global_volume = 64;
	sample->vibrato_speed = 0;
	sample->vibrato_depth = 0;
	sample->vibrato_rate = 0;
	sample->vibrato_waveform = IT_VIBRATO_SINE;
	sample->finetune = 0;
	sample->max_resampling_quality = -1;

	if ( flags & 1 )
	{
		if (((unsigned int)sample->loop_end <= (unsigned int)sample->length) &&
			((unsigned int)sample->loop_start < (unsigned int)sample->loop_end))
		{
			sample->length = sample->loop_end;
			sample->flags |= IT_SAMPLE_LOOP;
			if ( flags & 0x10 ) sample->flags |= IT_SAMPLE_PINGPONG_LOOP;
		}
	}

	sample->data = malloc( sample->length );
	if ( ! sample->data )
		return -1;

	memcpy( sample->data, data + 64, sample->length );

	if ( ! ( flags & 2 ) )
	{
		for ( flags = 0; flags < sample->length; ++flags )
			( ( signed char * ) sample->data ) [ flags ] ^= 0x80;
	}

	return 0;
}

static int it_riff_dsmf_process_pattern( IT_PATTERN * pattern, const unsigned char * data, int len )
{
	int length, row, pos;
	unsigned flags;
	IT_ENTRY * entry;

	length = data[ 0 ] | ( data[ 1 ] << 8 );
	if ( length > len ) return -1;

	data += 2;
	len = length - 2;

	pattern->n_rows = 64;
	pattern->n_entries = 64;

	row = 0;
	pos = 0;

	while ( (row < 64) && (pos < len) ) {
		if ( ! data[ pos ] ) {
			++ row;
			++ pos;
			continue;
		}

		flags = data[ pos++ ] & 0xF0;

		if (flags) {
			++ pattern->n_entries;
			if (flags & 0x80) pos ++;
			if (flags & 0x40) pos ++;
			if (flags & 0x20) pos ++;
			if (flags & 0x10) pos += 2;
		}
	}

	if ( pattern->n_entries == 64 ) return 0;

	pattern->entry = malloc( pattern->n_entries * sizeof( * pattern->entry ) );
	if ( ! pattern->entry ) return -1;

	entry = pattern->entry;

	row = 0;
	pos = 0;

	while ( ( row < 64 ) && ( pos < len ) )
	{
		if ( ! data[ pos ] )
		{
			IT_SET_END_ROW( entry );
			++ entry;
			++ row;
			++ pos;
			continue;
		}

		flags = data[ pos++ ];
		entry->channel = flags & 0x0F;
		entry->mask = 0;

		if ( flags & 0xF0 )
		{
			if ( flags & 0x80 )
			{
				if ( data[ pos ] )
				{
					entry->mask |= IT_ENTRY_NOTE;
					entry->note = data[ pos ] - 1;
				}
				++ pos;
			}

			if ( flags & 0x40 )
			{
				if ( data[ pos ] )
				{
					entry->mask |= IT_ENTRY_INSTRUMENT;
					entry->instrument = data[ pos ];
				}
				++ pos;
			}

			if ( flags & 0x20 )
			{
				entry->mask |= IT_ENTRY_VOLPAN;
				entry->volpan = data[ pos ];
				++ pos;
			}

			if ( flags & 0x10 )
			{
				_dumb_it_xm_convert_effect( data[ pos ], data[ pos + 1 ], entry, 0 );
				pos += 2;
			}

			if (entry->mask) entry++;
		}
	}

	while ( row < 64 )
	{
		IT_SET_END_ROW( entry );
		++ entry;
		++ row;
	}

	pattern->n_entries = (int)(entry - pattern->entry);
	if ( ! pattern->n_entries ) return -1;

	return 0;
}

static DUMB_IT_SIGDATA *it_riff_dsmf_load_sigdata( struct riff * stream )
{
	DUMB_IT_SIGDATA *sigdata;

	int n, o, found;

	unsigned char * ptr;

	if ( ! stream ) goto error;

	if ( stream->type != DUMB_ID( 'D', 'S', 'M', 'F' ) ) goto error;

	sigdata = malloc(sizeof(*sigdata));
	if ( ! sigdata ) goto error;

	sigdata->n_patterns = 0;
	sigdata->n_samples = 0;
	sigdata->name[0] = 0;

	found = 0;

	for ( n = 0; (unsigned int)n < stream->chunk_count; ++n )
	{
		struct riff_chunk * c = stream->chunks + n;
		switch( c->type )
		{
		case DUMB_ID( 'S' ,'O' ,'N' ,'G' ):
			/* initialization data */
			if ( ( found ) || ( c->size < 192 ) ) goto error_sd;
			found = 1;
			break;

		case DUMB_ID( 'P', 'A', 'T', 'T' ):
			++ sigdata->n_patterns;
			break;

		case DUMB_ID( 'I', 'N', 'S', 'T' ):
			++ sigdata->n_samples;
			break;
		}
	}

	if ( !found || !sigdata->n_samples || !sigdata->n_patterns ) goto error_sd;

	if ( sigdata->n_samples > 255 || sigdata->n_patterns > 255 ) goto error_sd;

	sigdata->song_message = NULL;
	sigdata->order = NULL;
	sigdata->instrument = NULL;
	sigdata->sample = NULL;
	sigdata->pattern = NULL;
	sigdata->midi = NULL;
	sigdata->checkpoint = NULL;

	sigdata->mixing_volume = 48;
	sigdata->pan_separation = 128;

	sigdata->n_instruments = 0;
	sigdata->n_orders = 0;
	sigdata->restart_position = 0;

	memset(sigdata->channel_volume, 64, DUMB_IT_N_CHANNELS);

	for (n = 0; n < DUMB_IT_N_CHANNELS; n += 4) {
		sigdata->channel_pan[n  ] = 16;
		sigdata->channel_pan[n+1] = 48;
		sigdata->channel_pan[n+2] = 48;
		sigdata->channel_pan[n+3] = 16;
	}

	for ( n = 0; (unsigned int)n < stream->chunk_count; ++n )
	{
		struct riff_chunk * c = stream->chunks + n;
		switch ( c->type )
		{
		case DUMB_ID( 'S', 'O', 'N', 'G' ):
			ptr = ( unsigned char * ) c->data;
			memcpy( sigdata->name, c->data, 28 );
			sigdata->name[ 28 ] = 0;
			sigdata->flags = IT_WAS_AN_XM | IT_WAS_A_MOD | IT_STEREO | IT_OLD_EFFECTS | IT_COMPATIBLE_GXX;
			sigdata->n_orders = ptr[ 36 ] | ( ptr[ 37 ] << 8 );
			//sigdata->n_samples = ptr[ 38 ] | ( ptr[ 39 ] << 8 ); // whatever
			//sigdata->n_patterns = ptr[ 40 ] | ( ptr[ 41 ] << 8 );
			sigdata->n_pchannels = ptr[ 42 ] | ( ptr[ 43 ] << 8 );
			sigdata->global_volume = ptr[ 44 ];
			sigdata->mixing_volume = ptr[ 45 ];
			sigdata->speed = ptr[ 46 ];
			sigdata->tempo = ptr[ 47 ];

			for ( o = 0; o < 16; ++o )
			{
				sigdata->channel_pan[ o ] = ptr[ 48 + o ] / 2;
			}

			sigdata->order = malloc( 128 );
			if ( ! sigdata->order ) goto error_usd;
			memcpy( sigdata->order, ptr + 64, 128 );

			break;
		}
	}

	sigdata->pattern = malloc( sigdata->n_patterns * sizeof( *sigdata->pattern ) );
	if ( ! sigdata->pattern ) goto error_usd;
	for ( n = 0; n < sigdata->n_patterns; ++n )
		sigdata->pattern[ n ].entry = NULL;

	sigdata->sample = malloc( sigdata->n_samples * sizeof( *sigdata->sample ) );
	if ( ! sigdata->sample ) goto error_usd;
	for ( n = 0; n < sigdata->n_samples; ++n )
	{
		IT_SAMPLE * sample = sigdata->sample + n;
		sample->data = NULL;
	}

	sigdata->n_samples = 0;
	sigdata->n_patterns = 0;

	for ( n = 0; (unsigned int)n < stream->chunk_count; ++n )
	{
		struct riff_chunk * c = stream->chunks + n;
		switch ( c->type )
		{
		case DUMB_ID( 'P', 'A', 'T', 'T' ):
			if ( it_riff_dsmf_process_pattern( sigdata->pattern + sigdata->n_patterns, ( unsigned char * ) c->data, c->size ) ) goto error_usd;
			++ sigdata->n_patterns;
			break;

		case DUMB_ID( 'I', 'N', 'S', 'T' ):
			if ( it_riff_dsmf_process_sample( sigdata->sample + sigdata->n_samples, ( unsigned char * ) c->data, c->size ) ) goto error_usd;
			++ sigdata->n_samples;
			break;
		}
	}

	_dumb_it_fix_invalid_orders( sigdata );

	return sigdata;

error_usd:
	_dumb_it_unload_sigdata( sigdata );
	goto error;
error_sd:
	free( sigdata );
error:
	return NULL;
}

DUH *dumb_read_riff_dsmf( struct riff * stream )
{
	sigdata_t *sigdata;

	DUH_SIGTYPE_DESC *descptr = &_dumb_sigtype_it;

	sigdata = it_riff_dsmf_load_sigdata( stream );

	if (!sigdata)
		return NULL;

	{
		const char *tag[2][2];
		tag[0][0] = "TITLE";
		tag[0][1] = ((DUMB_IT_SIGDATA *)sigdata)->name;
		tag[1][0] = "FORMAT";
		tag[1][1] = "RIFF DSMF";
		return make_duh( -1, 2, ( const char * const (*) [ 2 ] ) tag, 1, & descptr, & sigdata );
	}
}
