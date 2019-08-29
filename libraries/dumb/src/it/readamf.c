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
 * readamf.c - Code to read a DSMI AMF module from    / / \  \
 *             an open file.                         | <  /   \_
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



static void it_amf_process_track( IT_ENTRY *entry_table, unsigned char *track, int rows, int channels )
{
	int last_instrument = 0;
	int tracksize = track[ 0 ] + ( track[ 1 ] << 8 ) + ( track[ 2 ] << 16 );
	track += 3;
	while ( tracksize-- ) {
		unsigned int row = track[ 0 ];
		unsigned int command = track[ 1 ];
		unsigned int argument = track[ 2 ];
		IT_ENTRY * entry = entry_table + row * channels;
		if ( row >= ( unsigned int ) rows ) break;
		if ( command < 0x7F ) {
			entry->mask |= IT_ENTRY_NOTE | IT_ENTRY_INSTRUMENT | IT_ENTRY_VOLPAN;
			entry->note = command;
			if ( ! entry->instrument ) entry->instrument = last_instrument;
			entry->volpan = argument;
		}
		else if ( command == 0x7F ) {
			signed char row_delta = ( signed char ) argument;
			int row_source = ( int ) row + ( int ) row_delta;
			if ( row_source >= 0 && row_source < ( int ) rows ) {
				*entry = entry_table[ row_source * channels ];
			}
		}
		else if ( command == 0x80 ) {
			entry->mask |= IT_ENTRY_INSTRUMENT;
			last_instrument = argument + 1;
			entry->instrument = last_instrument;
		}
		else if ( command == 0x83 ) {
			entry->mask |= IT_ENTRY_VOLPAN;
			entry->volpan = argument;
		}
		else {
			unsigned int effect = command & 0x7F;
			unsigned int effectvalue = argument;
			switch (effect) {
				case 0x01: effect = IT_SET_SPEED; break;

				case 0x02: effect = IT_VOLUME_SLIDE;
				case 0x0A: if ( effect == 0x0A ) effect = IT_VOLSLIDE_TONEPORTA;
				case 0x0B: if ( effect == 0x0B ) effect = IT_VOLSLIDE_VIBRATO;
					if ( effectvalue & 0x80 ) effectvalue = ( -( signed char ) effectvalue ) & 0x0F;
					else effectvalue = ( effectvalue & 0x0F ) << 4;
					break;

				case 0x04:
					if ( effectvalue & 0x80 ) {
						effect = IT_PORTAMENTO_UP;
						effectvalue = ( -( signed char ) effectvalue ) & 0x7F;
					}
					else {
						effect = IT_PORTAMENTO_DOWN;
					}
					break;

				case 0x06: effect = IT_TONE_PORTAMENTO; break;

				case 0x07: effect = IT_TREMOR; break;

				case 0x08: effect = IT_ARPEGGIO; break;

				case 0x09: effect = IT_VIBRATO; break;

				case 0x0C: effect = IT_BREAK_TO_ROW; break;

				case 0x0D: effect = IT_JUMP_TO_ORDER; break;

				case 0x0F: effect = IT_RETRIGGER_NOTE; break;

				case 0x10: effect = IT_SET_SAMPLE_OFFSET; break;

				case 0x11:
					if ( effectvalue ) {
						effect = IT_VOLUME_SLIDE;
						if ( effectvalue & 0x80 )
							effectvalue = 0xF0 | ( ( -( signed char ) effectvalue ) & 0x0F );
						else
							effectvalue = 0x0F | ( ( effectvalue & 0x0F ) << 4 );
					}
					else
						effect = 0;
					break;

				case 0x12:
				case 0x16:
					if ( effectvalue ) {
						int mask = ( effect == 0x16 ) ? 0xE0 : 0xF0;
						effect = ( effectvalue & 0x80 ) ? IT_PORTAMENTO_UP : IT_PORTAMENTO_DOWN;
						if ( effectvalue & 0x80 )
							effectvalue = mask | ( ( -( signed char ) effectvalue ) & 0x0F );
						else
							effectvalue = mask | ( effectvalue & 0x0F );
                    }
					else
						effect = 0;
					break;

				case 0x13:
					effect = IT_S;
					effectvalue = EFFECT_VALUE( IT_S_NOTE_DELAY, effectvalue & 0x0F );
					break;

				case 0x14:
					effect = IT_S;
					effectvalue = EFFECT_VALUE( IT_S_DELAYED_NOTE_CUT, effectvalue & 0x0F );
					break;

				case 0x15: effect = IT_SET_SONG_TEMPO; break;

				case 0x17:
					effectvalue = ( effectvalue + 64 ) & 0x7F;
					if ( entry->mask & IT_ENTRY_EFFECT ) {
						if ( !( entry->mask & IT_ENTRY_VOLPAN ) ) {
							entry->mask |= IT_ENTRY_VOLPAN;
							entry->volpan = ( effectvalue / 2 ) + 128;
						}
						effect = 0;
					}
					else {
						effect = IT_SET_PANNING;
					}
					break;

				default: effect = effectvalue = 0;
			}
			if ( effect ) {
				entry->mask |= IT_ENTRY_EFFECT;
				entry->effect = effect;
				entry->effectvalue = effectvalue;
			}
		}
		track += 3;
	}
}

static int it_amf_process_pattern( IT_PATTERN *pattern, IT_ENTRY *entry_table, int rows, int channels )
{
	int i, j;
	int n_entries = rows;
	IT_ENTRY * entry;

	pattern->n_rows = rows;

	for ( i = 0, j = channels * rows; i < j; i++ ) {
		if ( entry_table[ i ].mask ) {
			n_entries++;
		}
	}

	pattern->n_entries = n_entries;

	pattern->entry = entry = malloc( n_entries * sizeof( IT_ENTRY ) );
	if ( !entry ) {
		return -1;
	}

	for ( i = 0; i < rows; i++ ) {
		for ( j = 0; j < channels; j++ ) {
			if ( entry_table[ i * channels + j ].mask ) {
				*entry = entry_table[ i * channels + j ];
				entry->channel = j;
				entry++;
			}
		}
		IT_SET_END_ROW( entry );
		entry++;
	}

	return 0;
}

static int it_amf_read_sample_header( IT_SAMPLE *sample, DUMBFILE *f, int * offset, int ver )
{
	int exists;

	exists = dumbfile_getc( f );

    dumbfile_getnc( (char *) sample->name, 32, f );
	sample->name[32] = 0;

    dumbfile_getnc( (char *) sample->filename, 13, f );
	sample->filename[13] = 0;

	*offset = dumbfile_igetl( f );
	sample->length = dumbfile_igetl( f );
	sample->C5_speed = dumbfile_igetw( f );
	sample->default_volume = dumbfile_getc( f );
	sample->global_volume = 64;
	if ( sample->default_volume > 64 ) sample->default_volume = 64;

	if ( ver >= 11 ) {
		sample->loop_start = dumbfile_igetl( f );
		sample->loop_end = dumbfile_igetl( f );
	} else {
		sample->loop_start = dumbfile_igetw( f );
		sample->loop_end = sample->length;
	}

	if ( sample->length <= 0 ) {
		sample->flags = 0;
		return 0;
	}

	sample->flags = exists == 1 ? IT_SAMPLE_EXISTS : 0;

	sample->default_pan = 0;
	sample->finetune = 0;

	if ( sample->loop_end > sample->loop_start + 2 && sample->loop_end <= sample->length )
		sample->flags |= IT_SAMPLE_LOOP;

	sample->vibrato_speed = 0;
	sample->vibrato_depth = 0;
	sample->vibrato_rate = 0;
	sample->vibrato_waveform = 0; // do we have to set _all_ these?
	sample->max_resampling_quality = -1;

	return dumbfile_error(f);
}



static int it_amf_read_sample_data( IT_SAMPLE *sample, DUMBFILE *f )
{
	int i, read_length = 0;

	sample->data = malloc( sample->length );

	if ( !sample->data )
		return -1;

	if ( sample->length )
		read_length = dumbfile_getnc( sample->data, sample->length, f );

	for ( i = 0; i < read_length; i++ ) {
		( ( char * ) sample->data )[ i ] ^= 0x80;
	}

	for ( i = read_length; i < sample->length; i++ ) {
		( ( char * ) sample->data )[ i ] = 0;
	}

	return 0; /* Sometimes the last sample is truncated :( */
}

static DUMB_IT_SIGDATA *it_amf_load_sigdata(DUMBFILE *f, int * version)
{
	DUMB_IT_SIGDATA *sigdata;
	int i, j, ver, ntracks, realntracks, nchannels;

	int maxsampleseekpos = 0;
	int sampleseekpos[256];

	unsigned short *orderstotracks;
	unsigned short *trackmap;
	unsigned int tracksize[256];

	unsigned char **track;

	static const char sig[] = "AMF";

	char signature [3];

	if ( dumbfile_getnc( signature, 3, f ) != 3 ||
		memcmp( signature, sig, 3 ) ) {
		return NULL;
	}

	*version = ver = dumbfile_getc( f );
	if ( ver < 10 || ver > 14) {
		return NULL;
	}

	sigdata = malloc(sizeof(*sigdata));
	if (!sigdata) {
		return NULL;
	}

    dumbfile_getnc( (char *) sigdata->name, 32, f );
	sigdata->name[ 32 ] = 0;
	sigdata->n_samples = dumbfile_getc( f );
	sigdata->n_orders = dumbfile_getc( f );
	ntracks = dumbfile_igetw( f );
	nchannels = dumbfile_getc( f );

	if ( dumbfile_error( f ) ||
		sigdata->n_samples < 1 || sigdata->n_samples > 255 ||
		sigdata->n_orders < 1 || sigdata->n_orders > 255 ||
		! ntracks ||
		nchannels < 1 || nchannels > 32 ) {
		free( sigdata );
		return NULL;
	}
    
    sigdata->n_pchannels = nchannels;

	memset( sigdata->channel_volume, 64, DUMB_IT_N_CHANNELS );

	if ( ver >= 11 ) {
		int nchannels = ( ver >= 13 ) ? 32 : 16;
		for ( i = 0; i < nchannels; i++ ) {
			signed char panpos = dumbfile_getc( f );
			int pan = ( panpos + 64 ) / 2;
			if ( pan < 0 ) pan = 0;
			else if ( pan > 64 ) pan = IT_SURROUND;
			sigdata->channel_pan[ i ] = pan;
		}
	}
	else {
		int sep = 32 * dumb_it_default_panning_separation / 100;
		for ( i = 0; i < 16; i++ ) {
			sigdata->channel_pan[ i ] = ( dumbfile_getc( f ) & 1 ) ? 32 - sep : 32 + sep;
		}
	}

	sigdata->tempo = 125;
	sigdata->speed = 6;
	if ( ver >= 13 ) {
		i = dumbfile_getc( f );
		if ( i >= 32 ) sigdata->tempo = i;
		i = dumbfile_getc( f );
		if ( i <= 32 ) sigdata->speed = i;
	}

	sigdata->order = malloc( sigdata->n_orders );
	if ( !sigdata->order ) {
		free( sigdata );
		return NULL;
	}

	orderstotracks = malloc( sigdata->n_orders * nchannels * sizeof( unsigned short ) );
	if ( !orderstotracks ) {
		free( sigdata->order );
		free( sigdata );
		return NULL;
	}

	for ( i = 0; i < sigdata->n_orders; i++ ) {
		sigdata->order[ i ] = i;
		tracksize[ i ] = 64;
		if ( ver >= 14 ) {
			tracksize[ i ] = dumbfile_igetw( f );
		}
		for ( j = 0; j < nchannels; j++ ) {
			orderstotracks[ i * nchannels + j ] = dumbfile_igetw( f );
		}
	}

	if ( dumbfile_error( f ) ) {
		free( orderstotracks );
		free( sigdata->order );
		free( sigdata );
		return NULL;
	}

	sigdata->sample = malloc( sigdata->n_samples * sizeof( *sigdata->sample ) );
	if ( !sigdata->sample ) {
		free( orderstotracks );
		free( sigdata->order );
		free( sigdata );
		return NULL;
	}

	sigdata->restart_position = 0;

	sigdata->song_message = NULL;
	sigdata->instrument = NULL;
	sigdata->pattern = NULL;
	sigdata->midi = NULL;
	sigdata->checkpoint = NULL;

	sigdata->n_instruments = 0;

	for ( i = 0; i < sigdata->n_samples; ++i )
		sigdata->sample[i].data = NULL;

	for ( i = 0; i < sigdata->n_samples; ++i ) {
		int offset;
		if ( it_amf_read_sample_header( &sigdata->sample[i], f, &offset, ver ) ) {
			goto error_ott;
		}
		sampleseekpos[ i ] = offset;
		if ( offset > maxsampleseekpos ) maxsampleseekpos = offset;
	}

	sigdata->n_patterns = sigdata->n_orders;

	sigdata->pattern = malloc( sigdata->n_patterns * sizeof( *sigdata->pattern ) );
	if ( !sigdata->pattern ) {
		goto error_ott;
	}
	for (i = 0; i < sigdata->n_patterns; ++i)
		sigdata->pattern[i].entry = NULL;

	trackmap = malloc( ntracks * sizeof( unsigned short ) );
	if ( !trackmap ) {
		goto error_ott;
	}

	if ( dumbfile_getnc( ( char * ) trackmap, ntracks * sizeof( unsigned short ), f ) != (long)(ntracks * sizeof( unsigned short )) ) {
		goto error_tm;
	}

	realntracks = 0;

	for ( i = 0; i < ntracks; i++ ) {
		if ( trackmap[ i ] > realntracks ) realntracks = trackmap[ i ];
	}

	track = calloc( realntracks, sizeof( unsigned char * ) );
	if ( !track ) {
		goto error_tm;
	}

	for ( i = 0; i < realntracks; i++ ) {
		int tracksize = dumbfile_igetw( f );
		tracksize += dumbfile_getc( f ) << 16;
		track[ i ] = malloc( tracksize * 3 + 3 );
		if ( !track[ i ] ) {
			goto error_all;
		}
		track[ i ][ 0 ] = tracksize & 255;
		track[ i ][ 1 ] = ( tracksize >> 8 ) & 255;
		track[ i ][ 2 ] = ( tracksize >> 16 ) & 255;
        if ( dumbfile_getnc( (char *) track[ i ] + 3, tracksize * 3, f ) != tracksize * 3 ) {
			goto error_all;
		}
	}

	for ( i = 1; i <= maxsampleseekpos; i++ ) {
		for ( j = 0; j < sigdata->n_samples; j++ ) {
			if ( sampleseekpos[ j ] == i ) {
				if ( it_amf_read_sample_data( &sigdata->sample[ j ], f ) ) {
					goto error_all;
				}
				break;
			}
		}
	}

	/* Process tracks into patterns */
	for ( i = 0; i < sigdata->n_patterns; i++ ) {
		IT_ENTRY * entry_table = calloc( tracksize[ i ] * nchannels, sizeof( IT_ENTRY ) );
		if ( !entry_table ) {
			goto error_all;
		}
		for ( j = 0; j < nchannels; j++ ) {
			int ntrack = orderstotracks[ i * nchannels + j ];
			if ( ntrack && ntrack <= ntracks ) {
				int realtrack = trackmap[ ntrack - 1 ];
				if ( realtrack ) {
					realtrack--;
					if ( realtrack < realntracks && track[ realtrack ] ) {
						it_amf_process_track( entry_table + j, track[ realtrack ], tracksize[ i ], nchannels );
					}
				}
			}
		}
		if ( it_amf_process_pattern( &sigdata->pattern[ i ], entry_table, tracksize[ i ], nchannels ) ) {
			free( entry_table );
			goto error_all;
		}
		free( entry_table );
	}

	/* Now let's initialise the remaining variables, and we're done! */
	sigdata->flags = IT_OLD_EFFECTS | IT_COMPATIBLE_GXX | IT_STEREO | IT_WAS_AN_S3M;

	sigdata->global_volume = 128;
	sigdata->mixing_volume = 48;
	sigdata->pan_separation = 128;

	_dumb_it_fix_invalid_orders(sigdata);

	for ( i = 0; i < realntracks; i++ ) {
		if ( track[ i ] ) {
			free( track[ i ] );
		}
	}
	free( track );
	free( trackmap );
	free( orderstotracks );

	return sigdata;

error_all:
	for ( i = 0; i < realntracks; i++ ) {
		if ( track[ i ] ) {
			free( track[ i ] );
		}
	}
	free( track );
error_tm:
	free( trackmap );
error_ott:
	free( orderstotracks );
	_dumb_it_unload_sigdata( sigdata );
	return NULL;
}



DUH *DUMBEXPORT dumb_read_amf_quick(DUMBFILE *f)
{
	sigdata_t *sigdata;

	DUH_SIGTYPE_DESC *descptr = &_dumb_sigtype_it;

	int version;

	sigdata = it_amf_load_sigdata(f, &version);

	if (!sigdata)
		return NULL;

	{
		const char *tag[2][2];
		char ver_string[14];
		tag[0][0] = "TITLE";
        tag[0][1] = (const char *)(((DUMB_IT_SIGDATA *)sigdata)->name);
		tag[1][0] = "FORMAT";
		memcpy( ver_string, "DSMI AMF v", 10 );
		ver_string[10] = '0' + version / 10;
		ver_string[11] = '.';
		ver_string[12] = '0' + version % 10;
		ver_string[13] = 0;
		tag[1][1] = ver_string;
		return make_duh(-1, 2, (const char *const (*)[2])tag, 1, &descptr, &sigdata);
	}
}
