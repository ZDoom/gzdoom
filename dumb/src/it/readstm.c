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
 * readstm.c - Code to read a ScreamTracker 2         / / \  \
 *             module from an open file.             | <  /   \_
 *                                                   |  \/ /\   /
 * By Chris Moeller.                                  \_  /  > /
 *                                                      | \ / /
 *                                                      |  ' /
 *                                                       \__/
 */

// IT_STEREO... :o
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "dumb.h"
#include "internal/it.h"

#ifdef _MSC_VER
	#define strnicmp _strnicmp
#else
	#if defined(unix) || defined(__unix__) || defined(__unix)
		#include <strings.h>
	#endif
	#define strnicmp strncasecmp
#endif

static int it_stm_read_sample_header( IT_SAMPLE *sample, DUMBFILE *f, unsigned short *offset )
{
    dumbfile_getnc( (char *) sample->filename, 12, f );
	sample->filename[12] = 0;

	memcpy( sample->name, sample->filename, 13 );

	dumbfile_skip( f, 2 );

	*offset = dumbfile_igetw( f );

	sample->length = dumbfile_igetw( f );
	sample->loop_start = dumbfile_igetw( f );
	sample->loop_end = dumbfile_igetw( f );

	sample->default_volume = dumbfile_getc( f );

	dumbfile_skip( f, 1 );

	sample->C5_speed = dumbfile_igetw( f ) << 3;

	dumbfile_skip( f, 6 );

	if ( sample->length < 4 || !sample->default_volume ) {
		/* Looks like no-existy. */
		sample->flags &= ~IT_SAMPLE_EXISTS;
		sample->length = 0;
		*offset = 0;
		return dumbfile_error( f );
	}

	sample->flags = IT_SAMPLE_EXISTS;
	sample->global_volume = 64;
	sample->default_pan = 0; // 0 = don't use, or 160 = centre?

	if ( ( sample->loop_start < sample->length ) &&
		( sample->loop_end > sample->loop_start ) &&
		( sample->loop_end != 0xFFFF ) ) {
		sample->flags |= IT_SAMPLE_LOOP;
		if ( sample->loop_end > sample->length ) sample->loop_end = sample->length;
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

static int it_stm_read_sample_data( IT_SAMPLE *sample, DUMBFILE * f )
{
	if ( ! sample->length ) return 0;

	sample->data = malloc( sample->length );
	if (!sample->data)
		return -1;

    dumbfile_getnc( sample->data, sample->length, f );

    return dumbfile_error( f );
}

static int it_stm_read_pattern( IT_PATTERN *pattern, DUMBFILE *f, unsigned char *buffer )
{
	int pos;
	int channel;
	int row;
	IT_ENTRY *entry;

	pattern->n_rows = 64;

    if ( dumbfile_getnc( (char *) buffer, 64 * 4 * 4, f ) != 64 * 4 * 4 )
		return -1;

	pattern->n_entries = 64;
	pos = 0;
	for ( row = 0; row < 64; ++row ) {
		for ( channel = 0; channel < 4; ++channel ) {
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
		for ( channel = 0; channel < 4; ++channel ) {
			if ( buffer[ pos + 0 ] | buffer[ pos + 1 ] | buffer[ pos + 2 ] | buffer[ pos + 3 ] ) {
				unsigned note;
				note = buffer[ pos + 0 ];
				entry->channel = channel;
				entry->mask = 0;
				entry->instrument = buffer[ pos + 1 ] >> 3;
				entry->volpan = ( buffer[ pos + 1 ] & 0x07 ) + ( buffer[ pos + 2 ] >> 1 );
				entry->effect = buffer[ pos + 2 ] & 0x0F;
				entry->effectvalue = buffer[ pos + 3 ];
				if ( entry->instrument && entry->instrument < 32 )
					entry->mask |= IT_ENTRY_INSTRUMENT;
				if ( note < 251 ) {
					entry->mask |= IT_ENTRY_NOTE;
					entry->note = ( note >> 4 ) * 12 + ( note & 0x0F );
				}
				if ( entry->volpan <= 64 )
					entry->mask |= IT_ENTRY_VOLPAN;
				entry->mask |= IT_ENTRY_EFFECT;
				switch ( entry->effect ) {
                    case IT_SET_SPEED:
                    /* taken care of in the renderer */
                        break;

					case IT_BREAK_TO_ROW:
						entry->effectvalue -= (entry->effectvalue >> 4) * 6;
						break;

					case IT_JUMP_TO_ORDER:
					case IT_VOLUME_SLIDE:
					case IT_PORTAMENTO_DOWN:
					case IT_PORTAMENTO_UP:
					case IT_TONE_PORTAMENTO:
					case IT_VIBRATO:
					case IT_TREMOR:
					case IT_ARPEGGIO:
					case IT_VOLSLIDE_VIBRATO:
					case IT_VOLSLIDE_TONEPORTA:
						break;

					default:
						entry->mask &= ~IT_ENTRY_EFFECT;
						break;
				}
				if ( entry->mask ) ++entry;
			}
			pos += 4;
		}
		IT_SET_END_ROW(entry);
		++entry;
	}

	pattern->n_entries = (int)(entry - pattern->entry);

	return 0;
}



static DUMB_IT_SIGDATA *it_stm_load_sigdata(DUMBFILE *f, int * version)
{
	DUMB_IT_SIGDATA *sigdata;

	char tracker_name[ 8 ];

	unsigned short sample_offset[ 31 ];

    int n;

	sigdata = malloc(sizeof(*sigdata));
	if (!sigdata) return NULL;

	/* Skip song name. */
    dumbfile_getnc((char *)sigdata->name, 20, f);
	sigdata->name[20] = 0;

	dumbfile_getnc(tracker_name, 8, f);
	n = dumbfile_getc(f);
	if ( n != 0x02 && n != 0x1A && n != 0x1B )
	{
		free( sigdata );
		return NULL;
	}
	if ( dumbfile_getc(f) != 2 ) /* only support modules */
	{
		free( sigdata );
		return NULL;
	}
	if ( strnicmp( tracker_name, "!Scream!", 8 ) &&
		strnicmp( tracker_name, "BMOD2STM", 8 ) &&
		strnicmp( tracker_name, "WUZAMOD!", 8 ) )
	{
		free( sigdata );
		return NULL;
	}

	*version = dumbfile_mgetw(f);

	sigdata->song_message = NULL;
	sigdata->order = NULL;
	sigdata->instrument = NULL;
	sigdata->sample = NULL;
	sigdata->pattern = NULL;
	sigdata->midi = NULL;
	sigdata->checkpoint = NULL;

	sigdata->n_instruments = 0;
	sigdata->n_samples = 31;
	sigdata->n_pchannels = 4;

    sigdata->tempo = 125;
    sigdata->mixing_volume = 48;
	sigdata->pan_separation = 128;

	/** WARNING: which ones? */
    sigdata->flags = IT_OLD_EFFECTS | IT_COMPATIBLE_GXX | IT_WAS_AN_S3M | IT_WAS_AN_STM | IT_STEREO;

    n = dumbfile_getc(f);
    if ( n < 32 ) n = 32;
    sigdata->speed = n;
    sigdata->n_patterns = dumbfile_getc(f);
	sigdata->global_volume = dumbfile_getc(f) << 1;
	if ( sigdata->global_volume > 128 ) sigdata->global_volume = 128;
	
	dumbfile_skip(f, 13);

	if ( dumbfile_error(f) || sigdata->n_patterns < 1 || sigdata->n_patterns > 99 ) {
		_dumb_it_unload_sigdata(sigdata);
		return NULL;
	}

	sigdata->sample = malloc(sigdata->n_samples * sizeof(*sigdata->sample));
	if (!sigdata->sample) {
		_dumb_it_unload_sigdata(sigdata);
		return NULL;
	}
	for (n = 0; n < sigdata->n_samples; n++)
		sigdata->sample[n].data = NULL;

	if (sigdata->n_patterns) {
		sigdata->pattern = malloc(sigdata->n_patterns * sizeof(*sigdata->pattern));
		if (!sigdata->pattern) {
			_dumb_it_unload_sigdata(sigdata);
			return NULL;
		}
		for (n = 0; n < sigdata->n_patterns; n++)
			sigdata->pattern[n].entry = NULL;
	}

	memset( sigdata->channel_volume, 64, 4 );
	n = 32 * dumb_it_default_panning_separation / 100;
	sigdata->channel_pan[ 0 ] = 32 + n;
	sigdata->channel_pan[ 1 ] = 32 - n;
	sigdata->channel_pan[ 2 ] = 32 + n;
	sigdata->channel_pan[ 3 ] = 32 - n;

	for ( n = 0; n < sigdata->n_samples; ++n ) {
		if ( it_stm_read_sample_header( &sigdata->sample[ n ], f, &sample_offset[ n ] ) ) {
			_dumb_it_unload_sigdata( sigdata );
			return NULL;
		}
	}

	sigdata->order = malloc( 128 );
	if ( !sigdata->order ) {
		_dumb_it_unload_sigdata( sigdata );
		return NULL;
	}

	/* Orders, byte each, length = sigdata->n_orders (should be even) */
    dumbfile_getnc( (char *) sigdata->order, *version >= 0x200 ? 128 : 64, f );
	if (*version < 0x200) memset( sigdata->order + 64, 0xFF, 64 );
	sigdata->restart_position = 0;

	for ( n = 127; n >= 0; --n ) {
		if ( sigdata->order[ n ] < sigdata->n_patterns ) break;
	}
	if ( n < 0 ) {
		_dumb_it_unload_sigdata( sigdata );
		return NULL;
	}
	sigdata->n_orders = n + 1;

	for ( n = 0; n < 128; ++n ) {
		if ( sigdata->order[ n ] >= 99 ) sigdata->order[ n ] = 0xFF;
	}

	if ( sigdata->n_patterns ) {
		unsigned char * buffer = malloc( 64 * 4 * 4 );
		if ( ! buffer ) {
			_dumb_it_unload_sigdata( sigdata );
			return NULL;
		}
		for ( n = 0; n < sigdata->n_patterns; ++n ) {
			if ( it_stm_read_pattern( &sigdata->pattern[ n ], f, buffer ) ) {
				free( buffer );
				_dumb_it_unload_sigdata( sigdata );
				return NULL;
			}
		}
		free( buffer );
	}

    for ( n = 0; n < sigdata->n_samples; ++n ) {
        if ( sample_offset[ n ] )
        {
            if ( dumbfile_seek( f, sample_offset[ n ] * 16, DFS_SEEK_SET ) ||
                 it_stm_read_sample_data( &sigdata->sample[ n ], f ) ) {
                _dumb_it_unload_sigdata( sigdata );
                return NULL;
            }
        }
        else
        {
            sigdata->sample[ n ].flags = 0;
            sigdata->sample[ n ].length = 0;
        }
    }

	_dumb_it_fix_invalid_orders(sigdata);

	return sigdata;
}

DUH *DUMBEXPORT dumb_read_stm_quick(DUMBFILE *f)
{
	sigdata_t *sigdata;
	int ver;

	DUH_SIGTYPE_DESC *descptr = &_dumb_sigtype_it;

	sigdata = it_stm_load_sigdata(f , &ver);

	if (!sigdata)
		return NULL;

	{
		char version[16];
		const char *tag[2][2];
		tag[0][0] = "TITLE";
        tag[0][1] = (const char *)(((DUMB_IT_SIGDATA *)sigdata)->name);
		tag[1][0] = "FORMAT";
		version[0] = 'S';
		version[1] = 'T';
		version[2] = 'M';
		version[3] = ' ';
		version[4] = 'v';
		version[5] = '0' + ((ver >> 8) & 15);
		version[6] = '.';
		if ((ver & 255) > 99)
		{
			version[7] = '0' + ((ver & 255) / 100 );
			version[8] = '0' + (((ver & 255) / 10) % 10);
			version[9] = '0' + ((ver & 255) % 10);
			version[10] = 0;
		}
		else
		{
			version[7] = '0' + ((ver & 255) / 10);
			version[8] = '0' + ((ver & 255) % 10);
			version[9] = 0;
		}
		tag[1][1] = (const char *) &version;
		return make_duh(-1, 2, (const char *const (*)[2])tag, 1, &descptr, &sigdata);
	}
}
