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

static int it_riff_dsmf_process_sample( IT_SAMPLE * sample, DUMBFILE * f, int len )
{
	int flags;

    dumbfile_getnc( (char *) sample->filename, 13, f );
	sample->filename[ 14 ] = 0;
	
    flags = dumbfile_igetw( f );
    sample->default_volume = dumbfile_getc( f );
    sample->length = dumbfile_igetl( f );
    sample->loop_start = dumbfile_igetl( f );
    sample->loop_end = dumbfile_igetl( f );
    dumbfile_skip( f, 32 - 28 );
    sample->C5_speed = dumbfile_igetw( f ) * 2;
    dumbfile_skip( f, 36 - 34 );
    dumbfile_getnc( (char *) sample->name, 28, f );
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

    dumbfile_getnc( sample->data, sample->length, f );

	if ( ! ( flags & 2 ) )
	{
		for ( flags = 0; flags < sample->length; ++flags )
			( ( signed char * ) sample->data ) [ flags ] ^= 0x80;
	}

	return 0;
}

static int it_riff_dsmf_process_pattern( IT_PATTERN * pattern, DUMBFILE * f, int len )
{
    int length, row;
	unsigned flags;
    long start, end;
    int p, q, r;
	IT_ENTRY * entry;

    length = dumbfile_igetw( f );
	if ( length > len ) return -1;

	len = length - 2;

	pattern->n_rows = 64;
	pattern->n_entries = 64;

	row = 0;

    start = dumbfile_pos( f );
    end = start + len;

    while ( (row < 64) && !dumbfile_error( f ) && (dumbfile_pos( f ) < end) ) {
        p = dumbfile_getc( f );
        if ( ! p ) {
			++ row;
			continue;
		}

        flags = p & 0xF0;

		if (flags) {
			++ pattern->n_entries;
            if (flags & 0x80) dumbfile_skip( f, 1 );
            if (flags & 0x40) dumbfile_skip( f, 1 );
            if (flags & 0x20) dumbfile_skip( f, 1 );
            if (flags & 0x10) dumbfile_skip( f, 2 );
		}
	}

	if ( pattern->n_entries == 64 ) return 0;

	pattern->entry = malloc( pattern->n_entries * sizeof( * pattern->entry ) );
	if ( ! pattern->entry ) return -1;

	entry = pattern->entry;

	row = 0;

    if ( dumbfile_seek( f, start, DFS_SEEK_SET ) ) return -1;

    while ( ( row < 64 ) && !dumbfile_error( f ) && ( dumbfile_pos( f ) < end ) )
	{
        p = dumbfile_getc( f );
        if ( ! p )
		{
			IT_SET_END_ROW( entry );
			++ entry;
			++ row;
			continue;
		}

        flags = p;
		entry->channel = flags & 0x0F;
		entry->mask = 0;

		if ( flags & 0xF0 )
		{
			if ( flags & 0x80 )
			{
                q = dumbfile_getc( f );
                if ( q )
				{
					entry->mask |= IT_ENTRY_NOTE;
                    entry->note = q - 1;
				}
			}

			if ( flags & 0x40 )
			{
                q = dumbfile_getc( f );
                if ( q )
				{
					entry->mask |= IT_ENTRY_INSTRUMENT;
                    entry->instrument = q;
				}
			}

			if ( flags & 0x20 )
			{
				entry->mask |= IT_ENTRY_VOLPAN;
                entry->volpan = dumbfile_getc( f );
			}

			if ( flags & 0x10 )
			{
                q = dumbfile_getc( f );
                r = dumbfile_getc( f );
                _dumb_it_xm_convert_effect( q, r, entry, 0 );
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

static DUMB_IT_SIGDATA *it_riff_dsmf_load_sigdata( DUMBFILE * f, struct riff * stream )
{
	DUMB_IT_SIGDATA *sigdata;

	int n, o, found;

	if ( ! stream ) goto error;

	if ( stream->type != DUMB_ID( 'D', 'S', 'M', 'F' ) ) goto error;

	sigdata = malloc(sizeof(*sigdata));
	if ( ! sigdata ) goto error;

	sigdata->n_patterns = 0;
	sigdata->n_samples = 0;
	sigdata->name[0] = 0;

	found = 0;

    for ( n = 0; (unsigned)n < stream->chunk_count; ++n )
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
		int sep = 32 * dumb_it_default_panning_separation / 100;
		sigdata->channel_pan[n  ] = 32 - sep;
		sigdata->channel_pan[n+1] = 32 + sep;
		sigdata->channel_pan[n+2] = 32 + sep;
		sigdata->channel_pan[n+3] = 32 - sep;
	}

    for ( n = 0; (unsigned)n < stream->chunk_count; ++n )
	{
		struct riff_chunk * c = stream->chunks + n;
		switch ( c->type )
		{
		case DUMB_ID( 'S', 'O', 'N', 'G' ):
            if ( dumbfile_seek( f, c->offset, DFS_SEEK_SET ) ) goto error_usd;
            dumbfile_getnc( (char *) sigdata->name, 28, f );
			sigdata->name[ 28 ] = 0;
			sigdata->flags = IT_STEREO | IT_OLD_EFFECTS | IT_COMPATIBLE_GXX;
            dumbfile_skip( f, 36 - 28 );
            sigdata->n_orders = dumbfile_igetw( f );
			//sigdata->n_samples = ptr[ 38 ] | ( ptr[ 39 ] << 8 ); // whatever
			//sigdata->n_patterns = ptr[ 40 ] | ( ptr[ 41 ] << 8 );
            dumbfile_skip( f, 42 - 38 );
            sigdata->n_pchannels = dumbfile_igetw( f );
            sigdata->global_volume = dumbfile_getc( f );
            sigdata->mixing_volume = dumbfile_getc( f );
            sigdata->speed = dumbfile_getc( f );
            sigdata->tempo = dumbfile_getc( f );

			for ( o = 0; o < 16; ++o )
			{
                sigdata->channel_pan[ o ] = dumbfile_getc( f ) / 2;
			}

			sigdata->order = malloc( 128 );
			if ( ! sigdata->order ) goto error_usd;
            dumbfile_getnc( (char *) sigdata->order, 128, f );

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

    for ( n = 0; (unsigned)n < stream->chunk_count; ++n )
	{
		struct riff_chunk * c = stream->chunks + n;
		switch ( c->type )
		{
		case DUMB_ID( 'P', 'A', 'T', 'T' ):
            if ( dumbfile_seek( f, c->offset, DFS_SEEK_SET ) ) goto error_usd;
            if ( it_riff_dsmf_process_pattern( sigdata->pattern + sigdata->n_patterns, f, c->size ) ) goto error_usd;
			++ sigdata->n_patterns;
			break;

		case DUMB_ID( 'I', 'N', 'S', 'T' ):
            if ( dumbfile_seek( f, c->offset, DFS_SEEK_SET ) ) goto error_usd;
            if ( it_riff_dsmf_process_sample( sigdata->sample + sigdata->n_samples, f, c->size ) ) goto error_usd;
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

DUH *dumb_read_riff_dsmf( DUMBFILE * f, struct riff * stream )
{
	sigdata_t *sigdata;

	DUH_SIGTYPE_DESC *descptr = &_dumb_sigtype_it;

    sigdata = it_riff_dsmf_load_sigdata( f, stream );

	if (!sigdata)
		return NULL;

	{
		const char *tag[2][2];
		tag[0][0] = "TITLE";
        tag[0][1] = (const char *)(((DUMB_IT_SIGDATA *)sigdata)->name);
		tag[1][0] = "FORMAT";
		tag[1][1] = "RIFF DSMF";
		return make_duh( -1, 2, ( const char * const (*) [ 2 ] ) tag, 1, & descptr, & sigdata );
	}
}
