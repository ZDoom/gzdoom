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

static int it_riff_am_process_sample( IT_SAMPLE * sample, DUMBFILE * f, int len, int ver )
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

    int32 start = dumbfile_pos( f );

	if ( ver == 0 )
    {
		if ( len < 0x38 )
			return -1;

		header_length = 0x38;

        dumbfile_getnc( (char *) sample->name, 28, f );
		sample->name[ 28 ] = 0;

        default_pan = dumbfile_getc( f );
        default_volume = dumbfile_getc( f );
        flags = dumbfile_igetw( f );
        length = dumbfile_igetl( f );
        loop_start = dumbfile_igetl( f );
        loop_end = dumbfile_igetl( f );
        sample_rate = dumbfile_igetl( f );
	}
	else
	{
		if (len < 4) return -1;

        header_length = dumbfile_igetl( f );
		if ( header_length < 0x40 )
			return -1;
		if ( header_length + 4 > len )
			return -1;

        start += 4;
		len -= 4;

        dumbfile_getnc( (char *) sample->name, 32, f );

        default_pan = dumbfile_igetw( f );
        default_volume = dumbfile_igetw( f );
        flags = dumbfile_igetw( f );
        dumbfile_skip( f, 2 );
        length = dumbfile_igetl( f );
        loop_start = dumbfile_igetl( f );
        loop_end = dumbfile_igetl( f );
        sample_rate = dumbfile_igetl( f );

		if ( default_pan > 0x7FFF || default_volume > 0x7FFF )
			return -1;

		default_pan = default_pan * 64 / 32767;
		default_volume = default_volume * 64 / 32767;
	}

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

    if ( dumbfile_seek( f, start + header_length, DFS_SEEK_SET ) )
        return -1;

    dumbfile_getnc( sample->data, length_bytes, f );

	return 0;
}

static int it_riff_am_process_pattern( IT_PATTERN * pattern, DUMBFILE * f, int len, int ver )
{
    int nrows, row;
    long start, end;
	unsigned flags;
    int p, q, r;
	IT_ENTRY * entry;

    nrows = dumbfile_getc( f ) + 1;

	pattern->n_rows = nrows;

	len -= 1;

	pattern->n_entries = 0;

	row = 0;

    start = dumbfile_pos( f );
    end = start + len;

    while ( (row < nrows) && !dumbfile_error( f ) && (dumbfile_pos( f ) < end) ) {
        p = dumbfile_getc( f );
        if ( ! p ) {
			++ row;
			continue;
		}

        flags = p & 0xE0;

        if (flags) {
			++ pattern->n_entries;
            if (flags & 0x80) dumbfile_skip( f, 2 );
            if (flags & 0x40) dumbfile_skip( f, 2 );
            if (flags & 0x20) dumbfile_skip( f, 1 );
		}
	}

	if ( ! pattern->n_entries ) return 0;

	pattern->n_entries += nrows;

	pattern->entry = malloc( pattern->n_entries * sizeof( * pattern->entry ) );
	if ( ! pattern->entry ) return -1;

	entry = pattern->entry;

	row = 0;

    dumbfile_seek( f, start, DFS_SEEK_SET );

    while ( ( row < nrows ) && !dumbfile_error( f ) && ( dumbfile_pos( f ) < end ) )
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
		entry->channel = flags & 0x1F;
		entry->mask = 0;

		if (flags & 0xE0)
		{
			if ( flags & 0x80 )
			{
                q = dumbfile_getc( f );
                r = dumbfile_getc( f );
                _dumb_it_xm_convert_effect( r, q, entry, 0 );
			}

			if ( flags & 0x40 )
			{
                q = dumbfile_getc( f );
                r = dumbfile_getc( f );
                if ( q )
				{
					entry->mask |= IT_ENTRY_INSTRUMENT;
                    entry->instrument = q;
				}
                if ( r )
				{
					entry->mask |= IT_ENTRY_NOTE;
                    entry->note = r - 1;
				}
			}

			if ( flags & 0x20 )
			{
                q = dumbfile_getc( f );
				entry->mask |= IT_ENTRY_VOLPAN;
                if ( ver == 0 ) entry->volpan = q;
                else entry->volpan = q * 64 / 127;
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

static DUMB_IT_SIGDATA *it_riff_amff_load_sigdata( DUMBFILE * f, struct riff * stream )
{
	DUMB_IT_SIGDATA *sigdata;

    int n, o, p, found;

	if ( ! stream ) goto error;

	if ( stream->type != DUMB_ID( 'A', 'M', 'F', 'F' ) ) goto error;

	sigdata = malloc( sizeof( *sigdata ) );
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
            if ( dumbfile_seek( f, c->offset, DFS_SEEK_SET ) ) goto error_sd;
            o = dumbfile_getc( f );
            if ( o >= sigdata->n_patterns ) sigdata->n_patterns = o + 1;
            o = dumbfile_igetl( f );
            if ( (unsigned)o + 5 > c->size ) goto error_sd;
			break;

		case DUMB_ID( 'I', 'N', 'S', 'T' ):
			{
				if ( c->size < 0xE1 ) goto error_sd;
                if ( dumbfile_seek( f, c->offset + 1, DFS_SEEK_SET ) ) goto error_sd;
                o = dumbfile_getc( f );
                if ( o >= sigdata->n_samples ) sigdata->n_samples = o + 1;
                if ( c->size >= 0x121 )
                {
                    if ( dumbfile_seek( f, c->offset + 0xE1, DFS_SEEK_SET ) ) goto error_sd;
                    if ( dumbfile_mgetl( f ) == DUMB_ID('S','A','M','P') )
                    {
                        unsigned size = dumbfile_igetl( f );
                        if ( size + 0xE1 + 8 > c->size ) goto error_sd;
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
		case DUMB_ID( 'M', 'A', 'I', 'N' ):
            if ( dumbfile_seek( f, c->offset, DFS_SEEK_SET ) ) goto error_usd;
            dumbfile_getnc( (char *) sigdata->name, 64, f );
			sigdata->name[ 64 ] = 0;
			sigdata->flags = IT_STEREO | IT_OLD_EFFECTS | IT_COMPATIBLE_GXX | IT_WAS_AN_S3M;
            o = dumbfile_getc( f );
            if ( ! ( o & 1 ) ) sigdata->flags |= IT_LINEAR_SLIDES;
            if ( ( o & ~3 ) || ! ( o & 2 ) ) goto error_usd; // unknown flags
            sigdata->n_pchannels = dumbfile_getc( f );
            sigdata->speed = dumbfile_getc( f );
            sigdata->tempo = dumbfile_getc( f );

            dumbfile_skip( f, 4 );

            sigdata->global_volume = dumbfile_getc( f );

            if ( c->size < 0x48 + (unsigned)sigdata->n_pchannels ) goto error_usd;

			for ( o = 0; o < sigdata->n_pchannels; ++o )
			{
                p = dumbfile_getc( f );
                sigdata->channel_pan[ o ] = p;
                if ( p >= 128 )
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

    for ( n = 0; (unsigned)n < stream->chunk_count; ++n )
	{
		struct riff_chunk * c = stream->chunks + n;
		switch ( c->type )
		{
		case DUMB_ID( 'O', 'R', 'D', 'R' ):
            if ( dumbfile_seek( f, c->offset, DFS_SEEK_SET ) ) goto error_usd;
            sigdata->n_orders = dumbfile_getc( f ) + 1;
            if ( (unsigned)sigdata->n_orders + 1 > c->size ) goto error_usd;
			sigdata->order = malloc( sigdata->n_orders );
			if ( ! sigdata->order ) goto error_usd;
            dumbfile_getnc( (char *) sigdata->order, sigdata->n_orders, f );
			break;

		case DUMB_ID( 'P', 'A', 'T', 'T' ):
            if ( dumbfile_seek( f, c->offset, DFS_SEEK_SET ) ) goto error_usd;
            o = dumbfile_getc( f );
            p = dumbfile_igetl( f );
            if ( it_riff_am_process_pattern( sigdata->pattern + o, f, p, 0 ) ) goto error_usd;
			break;

		case DUMB_ID( 'I', 'N', 'S', 'T' ):
			{
				IT_SAMPLE * sample;
                if ( dumbfile_seek( f, c->offset + 1, DFS_SEEK_SET ) ) goto error_usd;
                sample = sigdata->sample + dumbfile_getc( f );
                if ( c->size >= 0x121 )
                {
                    if ( dumbfile_seek( f, c->offset + 0xE1, DFS_SEEK_SET ) ) goto error_usd;
                    if ( dumbfile_mgetl( f ) == DUMB_ID('S','A','M','P') )
                    {
                        unsigned size = dumbfile_igetl( f );
                        if ( it_riff_am_process_sample( sample, f, size, 0 ) ) goto error_usd;
                        break;
                    }
				}
                dumbfile_seek( f, c->offset + 2, DFS_SEEK_SET );
                dumbfile_getnc( (char *) sample->name, 28, f );
                sample->name[ 28 ] = 0;
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

static DUMB_IT_SIGDATA *it_riff_am_load_sigdata( DUMBFILE * f, struct riff * stream )
{
	DUMB_IT_SIGDATA *sigdata;

	int n, o, p, found;

    if ( ! f || ! stream ) goto error;

	if ( stream->type != DUMB_ID( 'A', 'M', ' ', ' ' ) ) goto error;

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
            if ( dumbfile_seek( f, c->offset, DFS_SEEK_SET ) ) goto error_sd;
            o = dumbfile_getc( f );
            if ( o >= sigdata->n_patterns ) sigdata->n_patterns = o + 1;
            o = dumbfile_igetl( f );
            if ( (unsigned)o + 5 > c->size ) goto error_sd;
			break;

		case DUMB_ID( 'R', 'I', 'F', 'F' ):
			{
                struct riff * str = c->nested;
				switch ( str->type )
				{
				case DUMB_ID( 'A', 'I', ' ', ' ' ):
                    for ( o = 0; (unsigned)o < str->chunk_count; ++o )
					{
						struct riff_chunk * chk = str->chunks + o;
						switch( chk->type )
						{
						case DUMB_ID( 'I', 'N', 'S', 'T' ):
							{
								struct riff * temp;
								unsigned size;
								unsigned sample_found;
                                if ( dumbfile_seek( f, chk->offset, DFS_SEEK_SET ) ) goto error_sd;
                                size = dumbfile_igetl( f );
								if ( size < 0x142 ) goto error_sd;
								sample_found = 0;
                                dumbfile_skip( f, 1 );
                                p = dumbfile_getc( f );
                                if ( p >= sigdata->n_samples ) sigdata->n_samples = p + 1;
                                temp = riff_parse( f, chk->offset + 4 + size, chk->size - size - 4, 1 );
								if ( temp )
								{
									if ( temp->type == DUMB_ID( 'A', 'S', ' ', ' ' ) )
									{
                                        for ( p = 0; (unsigned)p < temp->chunk_count; ++p )
										{
											if ( temp->chunks[ p ].type == DUMB_ID( 'S', 'A', 'M', 'P' ) )
											{
												if ( sample_found )
												{
													riff_free( temp );
                                                    goto error_sd;
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
		case DUMB_ID( 'I', 'N', 'I', 'T' ):
            if ( dumbfile_seek( f, c->offset, DFS_SEEK_SET ) ) goto error_usd;
            dumbfile_getnc( (char *) sigdata->name, 64, f );
			sigdata->name[ 64 ] = 0;
			sigdata->flags = IT_STEREO | IT_OLD_EFFECTS | IT_COMPATIBLE_GXX | IT_WAS_AN_S3M;
            o = dumbfile_getc( f );
            if ( ! ( o & 1 ) ) sigdata->flags |= IT_LINEAR_SLIDES;
            if ( ( o & ~3 ) || ! ( o & 2 ) ) goto error_usd; // unknown flags
            sigdata->n_pchannels = dumbfile_getc( f );
            sigdata->speed = dumbfile_getc( f );
            sigdata->tempo = dumbfile_getc( f );

            dumbfile_skip( f, 4 );

            sigdata->global_volume = dumbfile_getc( f );

            if ( c->size < 0x48 + (unsigned)sigdata->n_pchannels ) goto error_usd;

			for ( o = 0; o < sigdata->n_pchannels; ++o )
			{
                p = dumbfile_getc( f );
                if ( p <= 128 )
				{
                    sigdata->channel_pan[ o ] = p / 2;
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

    for ( n = 0; (unsigned)n < stream->chunk_count; ++n )
	{
		struct riff_chunk * c = stream->chunks + n;
		switch ( c->type )
		{
		case DUMB_ID( 'O', 'R', 'D', 'R' ):
            if ( dumbfile_seek( f, c->offset, DFS_SEEK_SET ) ) goto error_usd;
            sigdata->n_orders = dumbfile_getc( f ) + 1;
            if ( (unsigned)sigdata->n_orders + 1 > c->size ) goto error_usd;
			sigdata->order = malloc( sigdata->n_orders );
			if ( ! sigdata->order ) goto error_usd;
            dumbfile_getnc( (char *) sigdata->order, sigdata->n_orders, f );
			break;

		case DUMB_ID( 'P', 'A', 'T', 'T' ):
            if ( dumbfile_seek( f, c->offset, DFS_SEEK_SET ) ) goto error_usd;
            o = dumbfile_getc( f );
            p = dumbfile_igetl( f );
            if ( it_riff_am_process_pattern( sigdata->pattern + o, f, p, 1 ) ) goto error_usd;
			break;

		case DUMB_ID( 'R', 'I', 'F', 'F' ):
			{
                struct riff * str = c->nested;
				switch ( str->type )
				{
				case DUMB_ID('A', 'I', ' ', ' '):
                    for ( o = 0; (unsigned)o < str->chunk_count; ++o )
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
                                if ( dumbfile_seek( f, chk->offset, DFS_SEEK_SET ) ) goto error_usd;
                                size = dumbfile_igetl( f );
                                dumbfile_skip( f, 1 );
                                p = dumbfile_getc( f );
                                temp = riff_parse( f, chk->offset + 4 + size, chk->size - size - 4, 1 );
								sample_found = 0;
                                sample = sigdata->sample + p;
								if ( temp )
								{
									if ( temp->type == DUMB_ID( 'A', 'S', ' ', ' ' ) )
									{
                                        for ( p = 0; (unsigned)p < temp->chunk_count; ++p )
										{
											struct riff_chunk * c = temp->chunks + p;
											if ( c->type == DUMB_ID( 'S', 'A', 'M', 'P' ) )
											{
												if ( sample_found )
												{
													riff_free( temp );
													goto error_usd;
												}
                                                {
                                                    riff_free( temp );
                                                    goto error_usd;
                                                }
                                                if ( it_riff_am_process_sample( sample, f, c->size, 1 ) )
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
                                    dumbfile_seek( f, chk->offset + 6, DFS_SEEK_SET );
                                    dumbfile_getnc( (char *) sample->name, 32, f );
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

DUH *dumb_read_riff_amff( DUMBFILE * f, struct riff * stream )
{
	sigdata_t *sigdata;
	long length;

	DUH_SIGTYPE_DESC *descptr = &_dumb_sigtype_it;

    sigdata = it_riff_amff_load_sigdata( f, stream );

	if (!sigdata)
		return NULL;

	length = 0;/*_dumb_it_build_checkpoints(sigdata, 0);*/

	{
		const char *tag[2][2];
		tag[0][0] = "TITLE";
        tag[0][1] = (const char *)(((DUMB_IT_SIGDATA *)sigdata)->name);
		tag[1][0] = "FORMAT";
		tag[1][1] = "RIFF AMFF";
		return make_duh( length, 2, ( const char * const (*) [ 2 ] ) tag, 1, & descptr, & sigdata );
	}
}

DUH *dumb_read_riff_am( DUMBFILE * f, struct riff * stream )
{
	sigdata_t *sigdata;

	DUH_SIGTYPE_DESC *descptr = &_dumb_sigtype_it;

    sigdata = it_riff_am_load_sigdata( f, stream );

	if (!sigdata)
		return NULL;

	{
		const char *tag[2][2];
		tag[0][0] = "TITLE";
        tag[0][1] = (const char *)(((DUMB_IT_SIGDATA *)sigdata)->name);
		tag[1][0] = "FORMAT";
		tag[1][1] = "RIFF AM";
		return make_duh( -1, 2, ( const char * const (*) [ 2 ] ) tag, 1, & descptr, & sigdata );
	}
}
