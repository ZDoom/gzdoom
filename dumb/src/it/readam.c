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
 * readam.c - Code to read a RIFF AM module           / / \  \
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

static int it_riff_am_process_sample( IT_SAMPLE * sample, const unsigned char * data, int len, int ver )
{
	int header_length;
	int default_pan;
	int default_volume;
	int flags;
	int length;
	int length_bytes;
	int loop_start;
	int loop_end;
	int sample_rate;

	if ( ver == 0 )
	{
		if ( len < 0x38 )
			return -1;

		header_length = 0x38;

		memcpy( sample->name, data, 28 );
		sample->name[ 28 ] = 0;

		default_pan = data[ 0x1C ];
		default_volume = data[ 0x1D ];
		flags = data[ 0x1E ] | ( data[ 0x1F ] << 8 );
		length = data[ 0x20 ] | ( data[ 0x21 ] << 8 ) | ( data[ 0x22 ] << 16 ) | ( data[ 0x23 ] << 24 );
		loop_start = data[ 0x24 ] | ( data[ 0x25 ] << 8 ) | ( data[ 0x26 ] << 16 ) | ( data[ 0x27 ] << 24 );
		loop_end = data[ 0x28 ] | ( data[ 0x29 ] << 8 ) | ( data[ 0x2A ] << 16 ) | ( data[ 0x2B ] << 24 );
		sample_rate = data[ 0x2C ] | ( data[ 0x2D ] << 8 ) | ( data[ 0x2E ] << 16 ) | ( data[ 0x2F ] << 24 );
	}
	else
	{
		if (len < 4) return -1;

		header_length = data[ 0 ] | ( data[ 1 ] << 8 ) | ( data[ 2 ] << 16 ) | ( data[ 3 ] << 24 );
		if ( header_length < 0x40 )
			return -1;
		if ( header_length + 4 > len )
			return -1;

		data += 4;
		len -= 4;

		memcpy( sample->name, data, 32 );
		sample->name[ 32 ] = 0;

		default_pan = data[ 0x20 ] | ( data[ 0x21 ] << 8 );
		default_volume = data[ 0x22 ] | ( data[ 0x23 ] << 8 );
		flags = data[ 0x24 ] | ( data[ 0x25 ] << 8 ); /* | ( data[ 0x26 ] << 16 ) | ( data[ 0x27 ] << 24 );*/
		length = data[ 0x28 ] | ( data[ 0x29 ] << 8 ) | ( data[ 0x2A ] << 16 ) | ( data[ 0x2B ] << 24 );
		loop_start = data[ 0x2C ] | ( data[ 0x2D ] << 8 ) | ( data[ 0x2E ] << 16 ) | ( data[ 0x2F ] << 24 );
		loop_end = data[ 0x30 ] | ( data[ 0x31 ] << 8 ) | ( data[ 0x32 ] << 16 ) | ( data[ 0x33 ] << 24 );
		sample_rate = data[ 0x34 ] | ( data[ 0x35 ] << 8 ) | ( data[ 0x36 ] << 16 ) | ( data[ 0x37 ] << 24 );

		if ( default_pan > 0x7FFF || default_volume > 0x7FFF )
			return -1;

		default_pan = default_pan * 64 / 32767;
		default_volume = default_volume * 64 / 32767;
	}

	/*if ( data[ 0x38 ] || data[ 0x39 ] || data[ 0x3A ] || data[ 0x3B ] )
		return -1;*/

	if ( ! length ) {
		sample->flags &= ~IT_SAMPLE_EXISTS;
		return 0;
	}

	if ( flags & ~( 0x8000 | 0x80 | 0x20 | 0x10 | 0x08 | 0x04 ) )
		return -1;

	length_bytes = length << ( ( flags & 0x04 ) >> 2 );

	if ( length_bytes + header_length > len )
		return -1;

	sample->flags = 0;

	if ( flags & 0x80 ) sample->flags |= IT_SAMPLE_EXISTS;
	if ( flags & 0x04 ) sample->flags |= IT_SAMPLE_16BIT;

	sample->length = length;
	sample->loop_start = loop_start;
	sample->loop_end = loop_end;
	sample->C5_speed = sample_rate;
	sample->default_volume = default_volume;
	sample->default_pan = default_pan | ( ( flags & 0x20 ) << 2 );
	sample->filename[0] = 0;
	sample->global_volume = 64;
	sample->vibrato_speed = 0;
	sample->vibrato_depth = 0;
	sample->vibrato_rate = 0;
	sample->vibrato_waveform = IT_VIBRATO_SINE;
	sample->finetune = 0;
	sample->max_resampling_quality = -1;

	if ( flags & 0x08 )
	{
		if (((unsigned int)sample->loop_end <= (unsigned int)sample->length) &&
			((unsigned int)sample->loop_start < (unsigned int)sample->loop_end))
		{
			sample->length = sample->loop_end;
			sample->flags |= IT_SAMPLE_LOOP;
			if ( flags & 0x10 ) sample->flags |= IT_SAMPLE_PINGPONG_LOOP;
		}
	}

	length_bytes = sample->length << ( ( flags & 0x04 ) >> 2 );

	sample->data = malloc( length_bytes );
	if ( ! sample->data )
		return -1;

	memcpy( sample->data, data + header_length, length_bytes );

	return 0;
}

static int it_riff_am_process_pattern( IT_PATTERN * pattern, const unsigned char * data, int len, int ver )
{
	int nrows, row, pos;
	unsigned flags;
	IT_ENTRY * entry;

	nrows = data[0] + 1;

	pattern->n_rows = nrows;

	data += 1;
	len -= 1;

	pattern->n_entries = 0;

	row = 0;
	pos = 0;

	while ( (row < nrows) && (pos < len) ) {
		if ( ! data[ pos ] ) {
			++ row;
			++ pos;
			continue;
		}

		flags = data[ pos++ ] & 0xE0;

		if (flags) {
			++ pattern->n_entries;
			if (flags & 0x80) pos += 2;
			if (flags & 0x40) pos += 2;
			if (flags & 0x20) pos ++;
		}
	}

	if ( ! pattern->n_entries ) return 0;

	pattern->n_entries += nrows;

	pattern->entry = malloc( pattern->n_entries * sizeof( * pattern->entry ) );
	if ( ! pattern->entry ) return -1;

	entry = pattern->entry;

	row = 0;
	pos = 0;

	while ( ( row < nrows ) && ( pos < len ) )
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
		entry->channel = flags & 0x1F;
		entry->mask = 0;

		if (flags & 0xE0)
		{
			if ( flags & 0x80 )
			{
				_dumb_it_xm_convert_effect( data[ pos + 1 ], data[ pos ], entry, 0 );
				pos += 2;
			}

			if ( flags & 0x40 )
			{
				if ( data[ pos ] )
				{
					entry->mask |= IT_ENTRY_INSTRUMENT;
					entry->instrument = data[ pos ];
				}
				if ( data[ pos + 1 ] )
				{
					entry->mask |= IT_ENTRY_NOTE;
					entry->note = data[ pos + 1 ] - 1;
				}
				pos += 2;
			}

			if ( flags & 0x20 )
			{
				entry->mask |= IT_ENTRY_VOLPAN;
				if ( ver == 0 ) entry->volpan = data[ pos ];
				else entry->volpan = data[ pos ] * 64 / 127;
				++ pos;
			}

			if (entry->mask) entry++;
		}
	}

	while ( row < nrows )
	{
		IT_SET_END_ROW( entry );
		++ entry;
		++ row;
	}

	pattern->n_entries = (int)(entry - pattern->entry);
	if ( ! pattern->n_entries ) return -1;

	return 0;
}

static DUMB_IT_SIGDATA *it_riff_amff_load_sigdata( struct riff * stream )
{
	DUMB_IT_SIGDATA *sigdata;

	int n, found;
	unsigned int o;

	unsigned char * ptr;

	if ( ! stream ) goto error;

	if ( stream->type != DUMB_ID( 'A', 'M', 'F', 'F' ) ) goto error;

	sigdata = malloc( sizeof( *sigdata ) );
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
		case DUMB_ID( 'M', 'A', 'I', 'N' ):
			/* initialization data */
			if ( ( found & 1 ) || ( c->size < 0x48 ) ) goto error_sd;
			found |= 1;
			break;

		case DUMB_ID( 'O', 'R', 'D', 'R' ):
			if ( ( found & 2 ) || ( c->size < 1 ) ) goto error_sd;
			found |= 2;
			break;

		case DUMB_ID( 'P', 'A', 'T', 'T' ):
			ptr = ( unsigned char * ) c->data;
			if ( ptr[ 0 ] >= sigdata->n_patterns ) sigdata->n_patterns = ptr[ 0 ] + 1;
			o = ptr[ 1 ] | ( ptr[ 2 ] << 8 ) | ( ptr[ 3 ] << 16 ) | ( ptr[ 4 ] << 24 );
			if ( o + 5 > c->size ) goto error_sd;
			break;

		case DUMB_ID( 'I', 'N', 'S', 'T' ):
			{
				if ( c->size < 0xE1 ) goto error;
				ptr = ( unsigned char * ) c->data;
				if ( ptr[ 1 ] >= sigdata->n_samples ) sigdata->n_samples = ptr[ 1 ] + 1;
				if ( c->size >= 0x121 && ( ptr[ 0xE1 ] == 'S' && ptr[ 0xE2 ] == 'A' &&
					ptr[ 0xE3 ] == 'M' && ptr[ 0xE4 ] == 'P' ) )
				{
					unsigned size = ptr[ 0xE5 ] | ( ptr[ 0xE6 ] << 8 ) | ( ptr[ 0xE7 ] << 16 ) | ( ptr[ 0xE8 ] << 24 );
					if ( size + 0xE1 + 8 > c->size ) goto error;
				}
			}
			break;
		}
	}

	if ( found != 3 || !sigdata->n_samples || !sigdata->n_patterns ) goto error_sd;

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
		case DUMB_ID( 'M', 'A', 'I', 'N' ):
			ptr = ( unsigned char * ) c->data;
			memcpy( sigdata->name, c->data, 64 );
			sigdata->name[ 64 ] = 0;
			sigdata->flags = IT_STEREO | IT_OLD_EFFECTS | IT_COMPATIBLE_GXX | IT_WAS_AN_S3M;
			if ( ! ( ptr[ 0x40 ] & 1 ) ) sigdata->flags |= IT_LINEAR_SLIDES;
			if ( ( ptr[ 0x40 ] & ~3 ) || ! ( ptr[ 0x40 ] & 2 ) ) goto error_usd; // unknown flags
			sigdata->n_pchannels = ptr[ 0x41 ];
			sigdata->speed = ptr[ 0x42 ];
			sigdata->tempo = ptr[ 0x43 ];

			sigdata->global_volume = ptr[ 0x48 ];

			if ( (int)c->size < 0x48 + sigdata->n_pchannels ) goto error_usd;

			for ( o = 0; (int)o < sigdata->n_pchannels; ++o )
			{
				sigdata->channel_pan[ o ] = ptr[ 0x49 + o ];
				if ( ptr[ 0x49 + o ] >= 128 )
				{
					sigdata->channel_volume[ o ] = 0;
				}
			}
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
		sample->flags = 0;
		sample->name[ 0 ] = 0;
	}

	for ( n = 0; (unsigned int)n < stream->chunk_count; ++n )
	{
		struct riff_chunk * c = stream->chunks + n;
		switch ( c->type )
		{
		case DUMB_ID( 'O', 'R', 'D', 'R' ):
			ptr = ( unsigned char * ) c->data;
			sigdata->n_orders = ptr[ 0 ] + 1;
			if ( sigdata->n_orders + 1 > (int)c->size ) goto error_usd;
			sigdata->order = malloc( sigdata->n_orders );
			if ( ! sigdata->order ) goto error_usd;
			memcpy( sigdata->order, ptr + 1, sigdata->n_orders );
			break;

		case DUMB_ID( 'P', 'A', 'T', 'T' ):
			ptr = ( unsigned char * ) c->data;
			o = ptr[ 1 ] | ( ptr[ 2 ] << 8 ) | ( ptr[ 3 ] << 16 ) | ( ptr[ 4 ] << 24 );
			if ( it_riff_am_process_pattern( sigdata->pattern + ptr[ 0 ], ptr + 5, o, 0 ) ) goto error_usd;
			break;

		case DUMB_ID( 'I', 'N', 'S', 'T' ):
			{
				IT_SAMPLE * sample;
				ptr = ( unsigned char * ) c->data;
				sample = sigdata->sample + ptr[ 1 ];
				if ( c->size >= 0x121 && ( ptr[ 0xE1 ] == 'S' && ptr[ 0xE2 ] == 'A' &&
					ptr[ 0xE3 ] == 'M' && ptr[ 0xE4 ] == 'P' ) )
				{
					unsigned size = ptr[ 0xE5 ] | ( ptr[ 0xE6 ] << 8 ) | ( ptr[ 0xE7 ] << 16 ) | ( ptr[ 0xE8 ] << 24 );
					if ( it_riff_am_process_sample( sample, ptr + 0xE1 + 8, size, 0 ) ) goto error_usd;
				}
				else
				{
					memcpy( sample->name, ptr + 2, 28 );
					sample->name[ 28 ] = 0;
				}
			}
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

static DUMB_IT_SIGDATA *it_riff_am_load_sigdata( struct riff * stream )
{
	DUMB_IT_SIGDATA *sigdata;

	int n, o, p, found;

	unsigned char * ptr;

	if ( ! stream ) goto error;

	if ( stream->type != DUMB_ID( 'A', 'M', ' ', ' ' ) ) goto error;

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
		case DUMB_ID( 'I' ,'N' ,'I' ,'T' ):
			/* initialization data */
			if ( ( found & 1 ) || ( c->size < 0x48 ) ) goto error_sd;
			found |= 1;
			break;

		case DUMB_ID( 'O', 'R', 'D', 'R' ):
			if ( ( found & 2 ) || ( c->size < 1 ) ) goto error_sd;
			found |= 2;
			break;

		case DUMB_ID( 'P', 'A', 'T', 'T' ):
			ptr = ( unsigned char * ) c->data;
			if ( ptr[ 0 ] >= sigdata->n_patterns ) sigdata->n_patterns = ptr[ 0 ] + 1;
			o = ptr[ 1 ] | ( ptr[ 2 ] << 8 ) | ( ptr[ 3 ] << 16 ) | ( ptr[ 4 ] << 24 );
			if ( o + 5 > (int)c->size ) goto error_sd;
			break;

		case DUMB_ID( 'R', 'I', 'F', 'F' ):
			{
				struct riff * str = ( struct riff * ) c->data;
				switch ( str->type )
				{
				case DUMB_ID( 'A', 'I', ' ', ' ' ):
					for ( o = 0; (unsigned int)o < str->chunk_count; ++o )
					{
						struct riff_chunk * chk = str->chunks + o;
						switch( chk->type )
						{
						case DUMB_ID( 'I', 'N', 'S', 'T' ):
							{
								struct riff * temp;
								unsigned size;
								unsigned sample_found;
								ptr = ( unsigned char * ) chk->data;
								size = ptr[ 0 ] | ( ptr[ 1 ] << 8 ) | ( ptr[ 2 ] << 16 ) | ( ptr[ 3 ] << 24 );
								if ( size < 0x142 ) goto error;
								sample_found = 0;
								if ( ptr[ 5 ] >= sigdata->n_samples ) sigdata->n_samples = ptr[ 5 ] + 1;
								temp = riff_parse( ptr + 4 + size, chk->size - size - 4, 1 );
								if ( temp )
								{
									if ( temp->type == DUMB_ID( 'A', 'S', ' ', ' ' ) )
									{
										for ( p = 0; (unsigned int)p < temp->chunk_count; ++p )
										{
											if ( temp->chunks[ p ].type == DUMB_ID( 'S', 'A', 'M', 'P' ) )
											{
												if ( sample_found )
												{
													riff_free( temp );
													goto error;
												}
												sample_found = 1;
											}
										}
									}
									riff_free( temp );
								}
							}
						}
					}
				}
			}
			break;
		}
	}

	if ( found != 3 || !sigdata->n_samples || !sigdata->n_patterns ) goto error_sd;

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
		case DUMB_ID( 'I', 'N', 'I', 'T' ):
			ptr = ( unsigned char * ) c->data;
			memcpy( sigdata->name, c->data, 64 );
			sigdata->name[ 64 ] = 0;
			sigdata->flags = IT_STEREO | IT_OLD_EFFECTS | IT_COMPATIBLE_GXX | IT_WAS_AN_S3M;
			if ( ! ( ptr[ 0x40 ] & 1 ) ) sigdata->flags |= IT_LINEAR_SLIDES;
			if ( ( ptr[ 0x40 ] & ~3 ) || ! ( ptr[ 0x40 ] & 2 ) ) goto error_usd; // unknown flags
			sigdata->n_pchannels = ptr[ 0x41 ];
			sigdata->speed = ptr[ 0x42 ];
			sigdata->tempo = ptr[ 0x43 ];

			sigdata->global_volume = ptr[ 0x48 ];

			if ( (int)c->size < 0x48 + sigdata->n_pchannels ) goto error_usd;

			for ( o = 0; o < sigdata->n_pchannels; ++o )
			{
				if ( ptr[ 0x49 + o ] <= 128 )
				{
					sigdata->channel_pan[ o ] = ptr[ 0x49 + o ] / 2;
				}
				else
				{
					sigdata->channel_volume[ o ] = 0;
				}
			}
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
		sample->flags = 0;
		sample->name[ 0 ] = 0;
	}

	for ( n = 0; (unsigned int)n < stream->chunk_count; ++n )
	{
		struct riff_chunk * c = stream->chunks + n;
		switch ( c->type )
		{
		case DUMB_ID( 'O', 'R', 'D', 'R' ):
			ptr = ( unsigned char * ) c->data;
			sigdata->n_orders = ptr[ 0 ] + 1;
			if ( sigdata->n_orders + 1 > (int)c->size ) goto error_usd;
			sigdata->order = malloc( sigdata->n_orders );
			if ( ! sigdata->order ) goto error_usd;
			memcpy( sigdata->order, ptr + 1, sigdata->n_orders );
			break;

		case DUMB_ID( 'P', 'A', 'T', 'T' ):
			ptr = ( unsigned char * ) c->data;
			o = ptr[ 1 ] | ( ptr[ 2 ] << 8 ) | ( ptr[ 3 ] << 16 ) | ( ptr[ 4 ] << 24 );
			if ( it_riff_am_process_pattern( sigdata->pattern + ptr[ 0 ], ptr + 5, o, 1 ) ) goto error_usd;
			break;

		case DUMB_ID( 'R', 'I', 'F', 'F' ):
			{
				struct riff * str = ( struct riff * ) c->data;
				switch ( str->type )
				{
				case DUMB_ID('A', 'I', ' ', ' '):
					for ( o = 0; (unsigned int)o < str->chunk_count; ++o )
					{
						struct riff_chunk * chk = str->chunks + o;
						switch( chk->type )
						{
						case DUMB_ID( 'I', 'N', 'S', 'T' ):
							{
								struct riff * temp;
								unsigned size;
								unsigned sample_found;
								IT_SAMPLE * sample;
								ptr = ( unsigned char * ) chk->data;
								size = ptr[ 0 ] | ( ptr[ 1 ] << 8 ) | ( ptr[ 2 ] << 16 ) | ( ptr[ 3 ] << 24 );
								temp = riff_parse( ptr + 4 + size, chk->size - size - 4, 1 );
								sample_found = 0;
								sample = sigdata->sample + ptr[ 5 ];
								if ( temp )
								{
									if ( temp->type == DUMB_ID( 'A', 'S', ' ', ' ' ) )
									{
										for ( p = 0; (unsigned int)p < temp->chunk_count; ++p )
										{
											struct riff_chunk * c = temp->chunks + p;
											if ( c->type == DUMB_ID( 'S', 'A', 'M', 'P' ) )
											{
												if ( sample_found )
												{
													riff_free( temp );
													goto error_usd;
												}
												if ( it_riff_am_process_sample( sigdata->sample + ptr[ 5 ], ( unsigned char * ) c->data, c->size, 1 ) )
												{
													riff_free( temp );
													goto error_usd;
												}
												sample_found = 1;
											}
										}
									}
									riff_free( temp );
								}
								if ( ! sample_found )
								{
									memcpy( sample->name, ptr + 6, 32 );
									sample->name[ 32 ] = 0;
								}
							}
						}
					}
				}
			}
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


DUH *dumb_read_riff_amff( struct riff * stream )
{
	sigdata_t *sigdata;
	int32 length;

	DUH_SIGTYPE_DESC *descptr = &_dumb_sigtype_it;

	sigdata = it_riff_amff_load_sigdata( stream );

	if (!sigdata)
		return NULL;

	length = 0;/*_dumb_it_build_checkpoints(sigdata, 0);*/

	{
		const char *tag[2][2];
		tag[0][0] = "TITLE";
		tag[0][1] = ((DUMB_IT_SIGDATA *)sigdata)->name;
		tag[1][0] = "FORMAT";
		tag[1][1] = "RIFF AMFF";
		return make_duh( length, 2, ( const char * const (*) [ 2 ] ) tag, 1, & descptr, & sigdata );
	}
}

DUH *dumb_read_riff_am( struct riff * stream )
{
	sigdata_t *sigdata;

	DUH_SIGTYPE_DESC *descptr = &_dumb_sigtype_it;

	sigdata = it_riff_am_load_sigdata( stream );

	if (!sigdata)
		return NULL;

	{
		const char *tag[2][2];
		tag[0][0] = "TITLE";
		tag[0][1] = ((DUMB_IT_SIGDATA *)sigdata)->name;
		tag[1][0] = "FORMAT";
		tag[1][1] = "RIFF AM";
		return make_duh( -1, 2, ( const char * const (*) [ 2 ] ) tag, 1, & descptr, & sigdata );
	}
}
