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
 * readasy.c - Code to read an ASYLUM Music Format    / / \  \
 *             module from an open file.             | <  /   \_
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



static int it_asy_read_pattern( IT_PATTERN *pattern, DUMBFILE *f, unsigned char *buffer )
{
	int pos;
	int channel;
	int row;
	IT_ENTRY *entry;

	pattern->n_rows = 64;

    if ( dumbfile_getnc( (char *) buffer, 64 * 8 * 4, f ) != 64 * 8 * 4 )
		return -1;

	/* compute number of entries */
	pattern->n_entries = 64; /* Account for the row end markers */
	pos = 0;
	for ( row = 0; row < 64; ++row ) {
		for ( channel = 0; channel < 8; ++channel ) {
			if ( buffer[ pos + 0 ] | buffer[ pos + 1 ] | buffer[ pos + 2 ] | buffer[ pos + 3 ] )
				++pattern->n_entries;
			pos += 4;
		}
	}

	pattern->entry = malloc( pattern->n_entries * sizeof( *pattern->entry ) );
	if ( !pattern->entry )
		return -1;

	entry = pattern->entry;
	pos = 0;
	for ( row = 0; row < 64; ++row ) {
		for ( channel = 0; channel < 8; ++channel ) {
			if ( buffer[ pos + 0 ] | buffer[ pos + 1 ] | buffer[ pos + 2 ] | buffer[ pos + 3 ] ) {
				entry->channel = channel;
				entry->mask = 0;

				if ( buffer[ pos + 0 ] && buffer[ pos + 0 ] < 96 ) {
					entry->note = buffer[ pos + 0 ];
					entry->mask |= IT_ENTRY_NOTE;
				}

				if ( buffer[ pos + 1 ] && buffer[ pos + 1 ] <= 64 ) {
					entry->instrument = buffer[ pos + 1 ];
					entry->mask |= IT_ENTRY_INSTRUMENT;
				}

				_dumb_it_xm_convert_effect( buffer[ pos + 2 ], buffer[ pos + 3 ], entry, 1 );
                
                // fixup
                switch ( entry->effect ) {
                    case IT_SET_PANNING:
                        entry->effectvalue <<= 1;
                        break;
                }

				if ( entry->mask ) ++entry;
			}
			pos += 4;
		}
		IT_SET_END_ROW( entry );
		++entry;
	}

	pattern->n_entries = (int)(entry - pattern->entry);

	return 0;
}



static int it_asy_read_sample_header( IT_SAMPLE *sample, DUMBFILE *f )
{
	int finetune, key_offset;

/**
     21       22   Chars     Sample 1 name.  If the name is not a full
                             22 chars in length, it will be null
                             terminated.

If
the sample name begins with a '#' character (ASCII $23 (35)) then this is
assumed not to be an instrument name, and is probably a message.
*/
    dumbfile_getnc( (char *) sample->name, 22, f );
	sample->name[22] = 0;

	sample->filename[0] = 0;

/** Each  finetune step changes  the note 1/8th  of  a  semitone. */
	finetune = ( signed char ) ( dumbfile_getc( f ) << 4 ) >> 4; /* signed nibble */
	sample->default_volume = dumbfile_getc( f ); // Should we be setting global_volume to this instead?
	sample->global_volume = 64;
	if ( sample->default_volume > 64 ) sample->default_volume = 64;
	key_offset = ( signed char ) dumbfile_getc( f ); /* base key offset */
	sample->length = dumbfile_igetl( f );
	sample->loop_start = dumbfile_igetl( f );
	sample->loop_end = sample->loop_start + dumbfile_igetl( f );

	if ( sample->length <= 0 ) {
		sample->flags = 0;
		return 0;
	}

	sample->flags = IT_SAMPLE_EXISTS;

	sample->default_pan = 0;
	sample->C5_speed = (int)( AMIGA_CLOCK / 214.0 * pow( DUMB_SEMITONE_BASE, key_offset ) );//( long )( 16726.0 * pow( DUMB_PITCH_BASE, finetune * 32 ) );
	sample->finetune = finetune * 32;
	// the above line might be wrong

	if ( ( sample->loop_end - sample->loop_start > 2 ) && ( sample->loop_end <= sample->length ) )
		sample->flags |= IT_SAMPLE_LOOP;

	sample->vibrato_speed = 0;
	sample->vibrato_depth = 0;
	sample->vibrato_rate = 0;
	sample->vibrato_waveform = 0; // do we have to set _all_ these?
	sample->max_resampling_quality = -1;

	return dumbfile_error(f);
}



static int it_asy_read_sample_data( IT_SAMPLE *sample, DUMBFILE *f )
{
	int32 truncated_size;

	/* let's get rid of the sample data coming after the end of the loop */
	if ( ( sample->flags & IT_SAMPLE_LOOP ) && sample->loop_end < sample->length ) {
		truncated_size = sample->length - sample->loop_end;
		sample->length = sample->loop_end;
	} else {
		truncated_size = 0;
	}

	sample->data = malloc( sample->length );

	if ( !sample->data )
		return -1;

	if ( sample->length )
		dumbfile_getnc( sample->data, sample->length, f );

	dumbfile_skip( f, truncated_size );

	return dumbfile_error( f );
}



static DUMB_IT_SIGDATA *it_asy_load_sigdata(DUMBFILE *f)
{
	DUMB_IT_SIGDATA *sigdata;
	int i;

	static const char sig_part[] = "ASYLUM Music Format";
	static const char sig_rest[] = " V1.0"; /* whee, string space optimization with format type below */

	char signature [32];

	if ( dumbfile_getnc( signature, 32, f ) != 32 ||
		memcmp( signature, sig_part, 19 ) ||
		memcmp( signature + 19, sig_rest, 5 ) ) {
		return NULL;
	}

	sigdata = malloc(sizeof(*sigdata));
	if (!sigdata) {
		return NULL;
	}

	sigdata->speed = dumbfile_getc( f ); /* XXX seems to fit the files I have */
	sigdata->tempo = dumbfile_getc( f ); /* ditto */
	sigdata->n_samples = dumbfile_getc( f ); /* ditto */
	sigdata->n_patterns = dumbfile_getc( f );
	sigdata->n_orders = dumbfile_getc( f );
	sigdata->restart_position = dumbfile_getc( f );

	if ( dumbfile_error( f ) || !sigdata->n_samples || sigdata->n_samples > 64 || !sigdata->n_patterns ||
		!sigdata->n_orders ) {
		free( sigdata );
		return NULL;
	}

	if ( sigdata->restart_position > sigdata->n_orders ) /* XXX */
		sigdata->restart_position = 0;

	sigdata->order = malloc( sigdata->n_orders );
	if ( !sigdata->order ) {
		free( sigdata );
		return NULL;
	}

    if ( dumbfile_getnc( (char *) sigdata->order, sigdata->n_orders, f ) != sigdata->n_orders ||
		dumbfile_skip( f, 256 - sigdata->n_orders ) ) {
		free( sigdata->order );
		free( sigdata );
		return NULL;
	}

	sigdata->sample = malloc( sigdata->n_samples * sizeof( *sigdata->sample ) );
	if ( !sigdata->sample ) {
		free( sigdata->order );
		free( sigdata );
		return NULL;
	}

	sigdata->song_message = NULL;
	sigdata->instrument = NULL;
	sigdata->pattern = NULL;
	sigdata->midi = NULL;
	sigdata->checkpoint = NULL;

	sigdata->n_instruments = 0;

	for ( i = 0; i < sigdata->n_samples; ++i )
		sigdata->sample[i].data = NULL;

	for ( i = 0; i < sigdata->n_samples; ++i ) {
		if ( it_asy_read_sample_header( &sigdata->sample[i], f ) ) {
			_dumb_it_unload_sigdata( sigdata );
			return NULL;
		}
	}

	if ( dumbfile_skip( f, 37 * ( 64 - sigdata->n_samples ) ) ) {
		_dumb_it_unload_sigdata( sigdata );
		return NULL;
	}

	sigdata->pattern = malloc( sigdata->n_patterns * sizeof( *sigdata->pattern ) );
	if ( !sigdata->pattern ) {
		_dumb_it_unload_sigdata( sigdata );
		return NULL;
	}
	for (i = 0; i < sigdata->n_patterns; ++i)
		sigdata->pattern[i].entry = NULL;

	/* Read in the patterns */
	{
		unsigned char *buffer = malloc( 64 * 8 * 4 ); /* 64 rows * 8 channels * 4 bytes */
		if ( !buffer ) {
			_dumb_it_unload_sigdata( sigdata );
			return NULL;
		}
		for ( i = 0; i < sigdata->n_patterns; ++i ) {
			if ( it_asy_read_pattern( &sigdata->pattern[i], f, buffer ) != 0 ) {
				free( buffer );
				_dumb_it_unload_sigdata( sigdata );
				return NULL;
			}
		}
		free( buffer );
	}

	/* And finally, the sample data */
	for ( i = 0; i < sigdata->n_samples; ++i ) {
		if ( it_asy_read_sample_data( &sigdata->sample[i], f ) ) {
			_dumb_it_unload_sigdata( sigdata );
			return NULL;
		}
	}

	/* Now let's initialise the remaining variables, and we're done! */
	sigdata->flags = IT_WAS_AN_XM | IT_WAS_A_MOD | IT_OLD_EFFECTS | IT_COMPATIBLE_GXX | IT_STEREO;

	sigdata->global_volume = 128;
	sigdata->mixing_volume = 48;
	sigdata->pan_separation = 128;

	sigdata->n_pchannels = 8;

	sigdata->name[0] = 0;

	memset(sigdata->channel_volume, 64, DUMB_IT_N_CHANNELS);

	for (i = 0; i < DUMB_IT_N_CHANNELS; i += 4) {
		int sep = 32 * dumb_it_default_panning_separation / 100;
		sigdata->channel_pan[i+0] = 32 - sep;
		sigdata->channel_pan[i+1] = 32 + sep;
		sigdata->channel_pan[i+2] = 32 + sep;
		sigdata->channel_pan[i+3] = 32 - sep;
	}

	_dumb_it_fix_invalid_orders(sigdata);

	return sigdata;
}



DUH *DUMBEXPORT dumb_read_asy_quick(DUMBFILE *f)
{
	sigdata_t *sigdata;

	DUH_SIGTYPE_DESC *descptr = &_dumb_sigtype_it;

	sigdata = it_asy_load_sigdata(f);

	if (!sigdata)
		return NULL;

	{
		const char *tag[2][2];
		tag[0][0] = "TITLE";
        tag[0][1] = (const char *)(((DUMB_IT_SIGDATA *)sigdata)->name);
		tag[1][0] = "FORMAT";
		tag[1][1] = "ASYLUM Music Format";
		return make_duh(-1, 2, (const char *const (*)[2])tag, 1, &descptr, &sigdata);
	}
}
