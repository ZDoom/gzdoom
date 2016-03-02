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
 * readmod.c - Code to read a good old-fashioned      / / \  \
 *             Amiga module from an open file.       | <  /   \_
 *                                                   |  \/ /\   /
 * By entheh.                                         \_  /  > /
 *                                                      | \ / /
 *                                                      |  ' /
 *                                                       \__/
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "dumb.h"
#include "internal/it.h"



static int it_mod_read_pattern(IT_PATTERN *pattern, DUMBFILE *f, int n_channels, unsigned char *buffer)
{
	int pos;
	int channel;
	int row;
	IT_ENTRY *entry;

	pattern->n_rows = 64;

	if (n_channels == 0) {
		/* Read the first four channels, leaving gaps for the rest. */
		for (pos = 0; pos < 64*8*4; pos += 8*4)
            dumbfile_getnc((char *)buffer + pos, 4*4, f);
		/* Read the other channels into the gaps we left. */
		for (pos = 4*4; pos < 64*8*4; pos += 8*4)
            dumbfile_getnc((char *)buffer + pos, 4*4, f);

		n_channels = 8;
	} else
        dumbfile_getnc((char *)buffer, 64 * n_channels * 4, f);

	if (dumbfile_error(f))
		return -1;

	/* compute number of entries */
	pattern->n_entries = 64; /* Account for the row end markers */
	pos = 0;
	for (row = 0; row < 64; row++) {
		for (channel = 0; channel < n_channels; channel++) {
			if (buffer[pos+0] | buffer[pos+1] | buffer[pos+2] | buffer[pos+3])
				pattern->n_entries++;
			pos += 4;
		}
	}

	pattern->entry = malloc(pattern->n_entries * sizeof(*pattern->entry));
	if (!pattern->entry)
		return -1;

	entry = pattern->entry;
	pos = 0;
	for (row = 0; row < 64; row++) {
		for (channel = 0; channel < n_channels; channel++) {
			if (buffer[pos+0] | buffer[pos+1] | buffer[pos+2] | buffer[pos+3]) {
				unsigned char sample = (buffer[pos+0] & 0xF0) | (buffer[pos+2] >> 4);
				int period = ((int)(buffer[pos+0] & 0x0F) << 8) | buffer[pos+1];

				entry->channel = channel;
				entry->mask = 0;

				if (period) {
					int note;
					entry->mask |= IT_ENTRY_NOTE;

					/* frequency = (AMIGA_DIVISOR / 8) / (period * 2)
					 * C-1: period = 214 -> frequency = 16726
					 * so, set C5_speed to 16726
					 * and period = 214 should translate to C5 aka 60
					 * halve the period, go up an octive
					 *
					 * period = 214 / pow(DUMB_SEMITONE_BASE, note - 60)
					 * pow(DUMB_SEMITONE_BASE, note - 60) = 214 / period
					 * note - 60 = log(214/period) / log(DUMB_SEMITONE_BASE)
					 */
					note = (int)floor(log(214.0/period) / log(DUMB_SEMITONE_BASE) + 60.5);
					entry->note = MID(0, note, 119);
					// or should we preserve the period?
					//entry->note = buffer[pos+0] & 0x0F; /* High nibble */
					//entry->volpan = buffer[pos+1]; /* Low byte */
					// and what about finetune?
				}

				if (sample) {
					entry->mask |= IT_ENTRY_INSTRUMENT;
					entry->instrument = sample;
				}

				_dumb_it_xm_convert_effect(buffer[pos+2] & 0x0F, buffer[pos+3], entry, 1);

				entry++;
			}
			pos += 4;
		}
		IT_SET_END_ROW(entry);
		entry++;
	}

	return 0;
}



static int it_mod_read_sample_header(IT_SAMPLE *sample, DUMBFILE *f, int stk)
{
	int finetune, loop_start, loop_length;

/**
     21       22   Chars     Sample 1 name.  If the name is not a full
                             22 chars in length, it will be null
                             terminated.

If
the sample name begins with a '#' character (ASCII $23 (35)) then this is
assumed not to be an instrument name, and is probably a message.
*/
    dumbfile_getnc((char *)sample->name, 22, f);
	sample->name[22] = 0;

	sample->filename[0] = 0;

	sample->length = dumbfile_mgetw(f) << 1;
	finetune = (signed char)(dumbfile_getc(f) << 4) >> 4; /* signed nibble */
/** Each  finetune step changes  the note 1/8th  of  a  semitone. */
	sample->global_volume = 64;
	sample->default_volume = dumbfile_getc(f); // Should we be setting global_volume to this instead?
	loop_start = dumbfile_mgetw(f);
	if ( !stk ) loop_start <<= 1;
	loop_length = dumbfile_mgetw(f) << 1;
	if ( loop_length > 2 && loop_start + loop_length > sample->length && loop_start / 2 + loop_length <= sample->length )
		loop_start /= 2;
	sample->loop_start = loop_start;
	sample->loop_end = loop_start + loop_length;
/**
Once this sample has been played completely from beginning
to end, if the  repeat length (next field)  is greater than two  bytes it
will loop back to this position in the sample and continue playing.  Once
it has played for  the repeat length,  it continues to  loop back to  the
repeat start offset.  This means the sample continues playing until it is
told to stop.
*/

	if (sample->length <= 0) {
		sample->flags = 0;
		return 0;
	}

	sample->flags = IT_SAMPLE_EXISTS;

	sample->default_pan = 0;
	sample->C5_speed = (int)( AMIGA_CLOCK / 214.0 ); //(long)(16726.0*pow(DUMB_PITCH_BASE, finetune*32));
	sample->finetune = finetune * 32;
	// the above line might be wrong

	if (sample->loop_end > sample->length)
		sample->loop_end = sample->length;

	if (sample->loop_end - sample->loop_start > 2)
		sample->flags |= IT_SAMPLE_LOOP;

	sample->vibrato_speed = 0;
	sample->vibrato_depth = 0;
	sample->vibrato_rate = 0;
	sample->vibrato_waveform = 0; // do we have to set _all_ these?
	sample->max_resampling_quality = -1;

	return dumbfile_error(f);
}



static int it_mod_read_sample_data(IT_SAMPLE *sample, DUMBFILE *f, uint32 fft)
{
	int32 i;
	int32 truncated_size;

	/* let's get rid of the sample data coming after the end of the loop */
	if ((sample->flags & IT_SAMPLE_LOOP) && sample->loop_end < sample->length) {
		truncated_size = sample->length - sample->loop_end;
		sample->length = sample->loop_end;
	} else {
		truncated_size = 0;
	}

	if (sample->length) {
		sample->data = malloc(sample->length);

		if (!sample->data)
			return -1;

		/* Sample data are stored in "8-bit two's compliment format" (sic). */
		/*
		for (i = 0; i < sample->length; i++)
			((signed char *)sample->left)[i] = dumbfile_getc(f);
		*/
		/* F U Olivier Lapicque */
		if (sample->length >= 5)
		{
			i = dumbfile_getnc(sample->data, 5, f);
			if (i == 5)
			{
				if (!memcmp(sample->data, "ADPCM", 5))
				{
					if (_dumb_it_read_sample_data_adpcm4(sample, f) < 0)
						return -1;

					return 0;
				}
				else
				{
					i += dumbfile_getnc(((char *)sample->data) + 5, sample->length - 5, f);
				}
			}
		}
		else
		{
			i = dumbfile_getnc(sample->data, sample->length, f);
		}
		if (i < sample->length)
		{
			if (i <= 0)
			{
				sample->flags = 0;
				return 0;
			}
			sample->length = i;
			if (sample->loop_end > i) sample->loop_end = i;
			// holy crap!
			if (sample->loop_start > i) sample->flags &= ~IT_SAMPLE_LOOP;
		}
		else
		{
			/* skip truncated data */
			int feh = dumbfile_error(f);

			if (truncated_size) dumbfile_skip(f, truncated_size);
			// Should we be truncating it?

			if (feh)
				return -1;
		}

		if (fft == DUMB_ID('M',0,0,0) || fft == DUMB_ID('8',0,0,0)) {
			int delta = 0;
			for (i = 0; i < sample->length; i++) {
				delta += ((signed char *)sample->data)[i];
				((signed char *)sample->data)[i] = delta;
			}
		}
	}

	return 0;
}




#define MOD_FFT_OFFSET (20 + 31*(22+2+1+1+2+2) + 1 + 1 + 128)

static DUMB_IT_SIGDATA *it_mod_load_sigdata(DUMBFILE *f, int restrict_)
{
	DUMB_IT_SIGDATA *sigdata;
	int n_channels;
	int i;
	uint32 fft;

    if ( dumbfile_seek(f, MOD_FFT_OFFSET, DFS_SEEK_SET) )
        return NULL;

    fft = dumbfile_mgetl(f);
    if (dumbfile_error(f))
        return NULL;

    if ( dumbfile_seek(f, 0, DFS_SEEK_SET) )
        return NULL;

	sigdata = malloc(sizeof(*sigdata));
	if (!sigdata) {
		return NULL;
	}

	/**
      1       20   Chars     Title of the song.  If the title is not a
                             full 20 chars in length, it will be null-
                             terminated.
	*/
    if (dumbfile_getnc((char *)sigdata->name, 20, f) < 20) {
		free(sigdata);
        return NULL;
	}
	sigdata->name[20] = 0;

	sigdata->n_samples = 31;

	switch (fft) {
		case DUMB_ID('M','.','K','.'):
		case DUMB_ID('M','!','K','!'):
		case DUMB_ID('M','&','K','!'):
		case DUMB_ID('N','.','T','.'):
		case DUMB_ID('N','S','M','S'):
		case DUMB_ID('F','L','T','4'):
		case DUMB_ID('M',0,0,0):
		case DUMB_ID('8',0,0,0):
			n_channels = 4;
			break;
		case DUMB_ID('F','L','T','8'):
			n_channels = 0;
			/* 0 indicates a special case; two four-channel patterns must be
			 * combined into one eight-channel pattern. Pattern indexes must
			 * be halved. Why oh why do they obfuscate so?
			 */
			/*for (i = 0; i < 128; i++)
				sigdata->order[i] >>= 1;*/
			break;
		case DUMB_ID('C','D','8','1'):
		case DUMB_ID('O','C','T','A'):
		case DUMB_ID('O','K','T','A'):
			n_channels = 8;
			break;
		case DUMB_ID('1','6','C','N'):
			n_channels = 16;
			break;
		case DUMB_ID('3','2','C','N'):
			n_channels = 32;
			break;
		default:
			/* If we get an illegal tag, assume 4 channels 15 samples. */
			if ((fft & 0x0000FFFFL) == DUMB_ID(0,0,'C','H')) {
				if (fft >= '1' << 24 && fft < '4' << 24) {
					n_channels = ((fft & 0x00FF0000L) >> 16) - '0';
					if ((unsigned int)n_channels >= 10) {
						/* Rightmost character wasn't a digit. */
						n_channels = 4;
						sigdata->n_samples = 15;
					} else {
						n_channels += (((fft & 0xFF000000L) >> 24) - '0') * 10;
						/* MODs should really only go up to 32 channels, but we're lenient. */
						if ((unsigned int)(n_channels - 1) >= DUMB_IT_N_CHANNELS - 1) {
							/* No channels or too many? Can't be right... */
							n_channels = 4;
							sigdata->n_samples = 15;
						}
					}
				} else {
					n_channels = 4;
					sigdata->n_samples = 15;
				}
			} else if ((fft & 0x00FFFFFFL) == DUMB_ID(0,'C','H','N')) {
				n_channels = (fft >> 24) - '0';
				if ((unsigned int)(n_channels - 1) >= 9) {
					/* Character was '0' or it wasn't a digit */
					n_channels = 4;
					sigdata->n_samples = 15;
				}
			} else if ((fft & 0xFFFFFF00L) == DUMB_ID('T','D','Z',0)) {
				n_channels = (fft & 0x000000FFL) - '0';
				if ((unsigned int)(n_channels - 1) >= 9) {
					/* We've been very lenient, given that it should have
					 * been 1, 2 or 3, but this MOD has been very naughty and
					 * must be punished.
					 */
					n_channels = 4;
					sigdata->n_samples = 15;
				}
			} else {
				n_channels = 4;
				sigdata->n_samples = 15;
			}
	}

	// moo
	if ( ( restrict_ & 1 ) && sigdata->n_samples == 15 )
	{
		free(sigdata);
        return NULL;
	}

	sigdata->n_pchannels = n_channels ? n_channels : 8; /* special case for 0, see above */

	sigdata->sample = malloc(sigdata->n_samples * sizeof(*sigdata->sample));
	if (!sigdata->sample) {
		free(sigdata);
        return NULL;
	}

	sigdata->song_message = NULL;
	sigdata->order = NULL;
	sigdata->instrument = NULL;
	sigdata->pattern = NULL;
	sigdata->midi = NULL;
	sigdata->checkpoint = NULL;

	sigdata->n_instruments = 0;

	for (i = 0; i < sigdata->n_samples; i++)
		sigdata->sample[i].data = NULL;

	for (i = 0; i < sigdata->n_samples; i++) {
		if (it_mod_read_sample_header(&sigdata->sample[i], f, sigdata->n_samples == 15)) {
			_dumb_it_unload_sigdata(sigdata);
            return NULL;
		}
	}

	sigdata->n_orders = dumbfile_getc(f);
	sigdata->restart_position = dumbfile_getc(f);
	// what if this is >= 127? what about with Fast Tracker II?

/*	if (sigdata->n_orders <= 0 || sigdata->n_orders > 128) { // is this right?
		_dumb_it_unload_sigdata(sigdata);
        return NULL;
	}*/

	//if (sigdata->restart_position >= sigdata->n_orders)
		//sigdata->restart_position = 0;

	sigdata->order = malloc(128); /* We may need to scan the extra ones! */
	if (!sigdata->order) {
		_dumb_it_unload_sigdata(sigdata);
        return NULL;
	}
    if (dumbfile_getnc((char *)sigdata->order, 128, f) < 128) {
		_dumb_it_unload_sigdata(sigdata);
        return NULL;
	}

	if (sigdata->n_orders <= 0 || sigdata->n_orders > 128) { // is this right?
		sigdata->n_orders = 128;
		//while (sigdata->n_orders > 1 && !sigdata->order[sigdata->n_orders - 1]) sigdata->n_orders--;
	}

	if ( ! n_channels )
		for (i = 0; i < 128; i++)
			sigdata->order[i] >>= 1;

	/* "The old NST format contains only 15 samples (instead of 31). Further
	 * it doesn't contain a file format tag (id). So Pattern data offset is
	 * at 20+15*30+1+1+128."
	 * - Then I shall assume the File Format Tag never exists if there are
	 * only 15 samples. I hope this isn't a faulty assumption...
	 */
	if (sigdata->n_samples == 31)
		dumbfile_skip(f, 4);

	sigdata->n_patterns = -1;

    if ( ( restrict_ & 2 ) )
	{
        unsigned char buffer[5];
        long sample_number;
        long total_sample_size;
        long offset = dumbfile_pos(f);
        long remain = dumbfile_get_size(f) - offset;
        if ( dumbfile_error( f ) ||
             dumbfile_seek( f, 0, SEEK_END ) ) {
            _dumb_it_unload_sigdata(sigdata);
            return NULL;
        }
        sample_number = sigdata->n_samples - 1;
        total_sample_size = 0;
        while (dumbfile_pos(f) > offset && sample_number >= 0) {
            if (sigdata->sample[sample_number].flags & IT_SAMPLE_EXISTS) {
                if ( dumbfile_seek(f, -((sigdata->sample[sample_number].length + 1) / 2 + 5 + 16), DFS_SEEK_CUR) ||
                     dumbfile_getnc((char *)buffer, 5, f) < 5 ) {
                    _dumb_it_unload_sigdata(sigdata);
                    return NULL;
                }
                if ( !memcmp( buffer, "ADPCM", 5 ) ) { /* BAH */
                    total_sample_size += (sigdata->sample[sample_number].length + 1) / 2 + 5 + 16;
                    if ( dumbfile_seek(f, -5, DFS_SEEK_CUR) ) {
                        _dumb_it_unload_sigdata(sigdata);
                        return NULL;
                    }
                } else {
                    total_sample_size += sigdata->sample[sample_number].length;
                    if ( dumbfile_seek(f, -(sigdata->sample[sample_number].length - ((sigdata->sample[sample_number].length + 1) / 2 + 5 + 16) + 5), DFS_SEEK_CUR) ) {
                        _dumb_it_unload_sigdata(sigdata);
                        return NULL;
                    }
                }
            }
            --sample_number;
        }

		if (remain > total_sample_size) {
			sigdata->n_patterns = ( remain - total_sample_size + 4 ) / ( 256 * sigdata->n_pchannels );
			if (fft == DUMB_ID('M',0,0,0) || fft == DUMB_ID('8',0,0,0)) {
				remain -= sigdata->n_patterns * 256 * sigdata->n_pchannels;
				if (dumbfile_skip(f, remain - total_sample_size)) {
					_dumb_it_unload_sigdata(sigdata);
					return NULL;
				}
			}
		}
	}
	else
    {
        for (i = 0; i < 128; i++)
        {
            if (sigdata->order[i] > sigdata->n_patterns)
                sigdata->n_patterns = sigdata->order[i];
        }
		sigdata->n_patterns++;
	}

	if ( sigdata->n_patterns <= 0 ) {
		_dumb_it_unload_sigdata(sigdata);
		return NULL;
	}

	/* May as well try to save a tiny bit of memory. */
	if (sigdata->n_orders < 128) {
		unsigned char *order = realloc(sigdata->order, sigdata->n_orders);
		if (order) sigdata->order = order;
	}

	sigdata->pattern = malloc(sigdata->n_patterns * sizeof(*sigdata->pattern));
	if (!sigdata->pattern) {
		_dumb_it_unload_sigdata(sigdata);
		return NULL;
	}
	for (i = 0; i < sigdata->n_patterns; i++)
		sigdata->pattern[i].entry = NULL;

	/* Read in the patterns */
	{
		unsigned char *buffer = malloc(256 * sigdata->n_pchannels); /* 64 rows * 4 bytes */
		if (!buffer) {
			_dumb_it_unload_sigdata(sigdata);
			return NULL;
		}
		for (i = 0; i < sigdata->n_patterns; i++) {
			if (it_mod_read_pattern(&sigdata->pattern[i], f, n_channels, buffer) != 0) {
				free(buffer);
				_dumb_it_unload_sigdata(sigdata);
				return NULL;
			}
		}
		free(buffer);
	}

	/* And finally, the sample data */
	for (i = 0; i < sigdata->n_samples; i++) {
		if (it_mod_read_sample_data(&sigdata->sample[i], f, fft)) {
			_dumb_it_unload_sigdata(sigdata);
			return NULL;
		}
	}

	/* w00t! */
	/*if ( n_channels == 4 &&
		( sigdata->n_samples == 15 ||
		( ( fft & 240 ) != DUMB_ID( 0, 0, 'C', 0 ) &&
		( fft & 240 ) != DUMB_ID( 0, 0, 'H', 0 ) &&
		( fft & 240 ) != 0 ) ) ) {
		for ( i = 0; i < sigdata->n_samples; ++i ) {
			IT_SAMPLE * sample = &sigdata->sample [i];
			if ( sample && ( sample->flags & IT_SAMPLE_EXISTS ) ) {
				int n, o;
				o = sample->length;
				if ( o > 4 ) o = 4;
				for ( n = 0; n < o; ++n )
					( ( char * ) sample->data ) [n] = 0;
			}
		}
	}*/

	/* Now let's initialise the remaining variables, and we're done! */
	sigdata->flags = IT_WAS_AN_XM | IT_WAS_A_MOD | IT_OLD_EFFECTS | IT_COMPATIBLE_GXX | IT_STEREO;

	sigdata->global_volume = 128;
	sigdata->mixing_volume = 48;
	/* We want 50 ticks per second; 50/6 row advances per second;
	 * 50*10=500 row advances per minute; 500/4=125 beats per minute.
	 */
	sigdata->speed = 6;
	sigdata->tempo = 125;
	sigdata->pan_separation = 128;

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



DUH *DUMBEXPORT dumb_read_mod_quick(DUMBFILE *f, int restrict_)
{
	sigdata_t *sigdata;

	DUH_SIGTYPE_DESC *descptr = &_dumb_sigtype_it;

	sigdata = it_mod_load_sigdata(f, restrict_);

	if (!sigdata)
		return NULL;

	{
		const char *tag[2][2];
		tag[0][0] = "TITLE";
        tag[0][1] = (const char *)(((DUMB_IT_SIGDATA *)sigdata)->name);
		tag[1][0] = "FORMAT";
		tag[1][1] = "MOD";
		return make_duh(-1, 2, (const char *const (*)[2])tag, 1, &descptr, &sigdata);
	}
}
