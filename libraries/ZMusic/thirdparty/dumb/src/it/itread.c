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
 * itread.c - Code to read an Impulse Tracker         / / \  \
 *            module from an open file.              | <  /   \_
 *                                                   |  \/ /\   /
 * Based on the loader from an IT player by Bob.      \_  /  > /
 * Adapted for DUMB by entheh.                          | \ / /
 *                                                      |  ' /
 *                                                       \__/
 */

#include <stdlib.h>
#include <string.h>//might not be necessary later; required for memset

#include "dumb.h"
#include "internal/it.h"

#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif


#define INVESTIGATE_OLD_INSTRUMENTS



typedef unsigned char byte;
typedef unsigned short word;
typedef uint32 dword;

typedef struct readblock_crap readblock_crap;

struct readblock_crap {
	unsigned char *sourcebuf;
	unsigned char *sourcepos;
	unsigned char *sourceend;
	int rembits;
};


static int readblock(DUMBFILE *f, readblock_crap * crap)
{
	int32 size;
	int c;

	size = dumbfile_igetw(f);
	if (size < 0)
		return size;

	crap->sourcebuf = malloc(size);
	if (!crap->sourcebuf)
		return -1;

	c = dumbfile_getnc((char *)crap->sourcebuf, size, f);
	if (c < size) {
		free(crap->sourcebuf);
		crap->sourcebuf = NULL;
		return -1;
	}

	crap->sourcepos = crap->sourcebuf;
	crap->sourceend = crap->sourcebuf + size;
	crap->rembits = 8;
	return 0;
}



static void freeblock(readblock_crap * crap)
{
	free(crap->sourcebuf);
	crap->sourcebuf = NULL;
}



static int readbits(int bitwidth, readblock_crap * crap)
{
	int val = 0;
	int b = 0;

	if (crap->sourcepos >= crap->sourceend) return val;

	while (bitwidth > crap->rembits) {
		val |= *crap->sourcepos++ << b;
		if (crap->sourcepos >= crap->sourceend) return val;
		b += crap->rembits;
		bitwidth -= crap->rembits;
		crap->rembits = 8;
	}

	val |= (*crap->sourcepos & ((1 << bitwidth) - 1)) << b;
	*crap->sourcepos >>= bitwidth;
	crap->rembits -= bitwidth;

	return val;
}



/** WARNING - do we even need to pass `right`? */
/** WARNING - why bother memsetting at all? The whole array is written... */
// if we do memset, dumb_silence() would be neater...
static int decompress8(DUMBFILE *f, signed char *data, int len, int it215, int stereo)
{
	int blocklen, blockpos;
	byte bitwidth;
	word val;
	char d1, d2;
	readblock_crap crap;

	memset(&crap, 0, sizeof(crap));

	for (blocklen = 0, blockpos = 0; blocklen < len; blocklen++, blockpos += 1 + stereo)
		data[ blockpos ] = 0;

	while (len > 0) {
		//Read a block of compressed data:
		if (readblock(f, &crap))
			return -1;
		//Set up a few variables
		blocklen = (len < 0x8000) ? len : 0x8000; //Max block length is 0x8000 bytes
		blockpos = 0;
		bitwidth = 9;
		d1 = d2 = 0;
		//Start the decompression:
		while (blockpos < blocklen) {
			//Read a value:
			val = (word)readbits(bitwidth, &crap);
			//Check for bit width change:

			if (bitwidth < 7) { //Method 1:
				if (val == (1 << (bitwidth - 1))) {
					val = (word)readbits(3, &crap) + 1;
					bitwidth = (val < bitwidth) ? val : val + 1;
					continue;
				}
			}
			else if (bitwidth < 9) { //Method 2
				byte border = (0xFF >> (9 - bitwidth)) - 4;

				if (val > border && val <= (border + 8)) {
					val -= border;
					bitwidth = (val < bitwidth) ? val : val + 1;
					continue;
				}
			}
			else if (bitwidth == 9) { //Method 3
				if (val & 0x100) {
					bitwidth = (val + 1) & 0xFF;
					continue;
				}
			}
			else { //Illegal width, abort ?
				freeblock(&crap);
				return -1;
			}

			//Expand the value to signed byte:
			{
				char v; //The sample value:
				if (bitwidth < 8) {
					byte shift = 8 - bitwidth;
					v = (val << shift);
					v >>= shift;
				}
				else
					v = (char)val;

				//And integrate the sample value
				//(It always has to end with integration doesn't it ? ;-)
				d1 += v;
				d2 += d1;
			}

			//Store !
			/* Version 2.15 was an unofficial version with hacked compression
			 * code. Yay, better compression :D
			 */
			*data++ = it215 ? d2 : d1;
			data += stereo;
			len--;
			blockpos++;
		}
		freeblock(&crap);
	}
	return 0;
}



static int decompress16(DUMBFILE *f, short *data, int len, int it215, int stereo)
{
	int blocklen, blockpos;
	byte bitwidth;
	int32 val;
	short d1, d2;
	readblock_crap crap;

	memset(&crap, 0, sizeof(crap));

	for ( blocklen = 0, blockpos = 0; blocklen < len; blocklen++, blockpos += 1 + stereo )
		data[ blockpos ] = 0;

	while (len > 0) {
		//Read a block of compressed data:
		if (readblock(f, &crap))
			return -1;
		//Set up a few variables
		blocklen = (len < 0x4000) ? len : 0x4000; // Max block length is 0x4000 bytes
		blockpos = 0;
		bitwidth = 17;
		d1 = d2 = 0;
		//Start the decompression:
		while (blockpos < blocklen) {
			val = readbits(bitwidth, &crap);
			//Check for bit width change:

			if (bitwidth < 7) { //Method 1:
				if (val == (1 << (bitwidth - 1))) {
					val = readbits(4, &crap) + 1;
					bitwidth = (byte)((val < bitwidth) ? val : val + 1);
					continue;
				}
			}
			else if (bitwidth < 17) { //Method 2
				word border = (0xFFFF >> (17 - bitwidth)) - 8;

				if (val > border && val <= (border + 16)) {
					val -= border;
					bitwidth = (byte)(val < bitwidth ? val : val + 1);
					continue;
				}
			}
			else if (bitwidth == 17) { //Method 3
				if (val & 0x10000) {
					bitwidth = (byte)((val + 1) & 0xFF);
					continue;
				}
			}
			else { //Illegal width, abort ?
				freeblock(&crap);
				return -1;
			}

			//Expand the value to signed byte:
			{
				short v; //The sample value:
				if (bitwidth < 16) {
					byte shift = 16 - bitwidth;
					v = (short)(val << shift);
					v >>= shift;
				}
				else
					v = (short)val;

				//And integrate the sample value
				//(It always has to end with integration doesn't it ? ;-)
				d1 += v;
				d2 += d1;
			}

			//Store !
			/* Version 2.15 was an unofficial version with hacked compression
			 * code. Yay, better compression :D
			 */
			*data++ = it215 ? d2 : d1;
			data += stereo;
			len--;
			blockpos++;
		}
		freeblock(&crap);
	}
	return 0;
}



static int it_read_envelope(IT_ENVELOPE *envelope, DUMBFILE *f)
{
	int n;

	envelope->flags = dumbfile_getc(f);
	envelope->n_nodes = dumbfile_getc(f);
	if(envelope->n_nodes > 25) {
		TRACE("IT error: wrong number of envelope nodes (%d)\n", envelope->n_nodes);
		envelope->n_nodes = 0;
		return -1;
	}
	envelope->loop_start = dumbfile_getc(f);
	envelope->loop_end = dumbfile_getc(f);
	envelope->sus_loop_start = dumbfile_getc(f);
	envelope->sus_loop_end = dumbfile_getc(f);
	for (n = 0; n < envelope->n_nodes; n++) {
		envelope->node_y[n] = dumbfile_getc(f);
		envelope->node_t[n] = dumbfile_igetw(f);
	}
	dumbfile_skip(f, 75 - envelope->n_nodes * 3 + 1);

	if (envelope->n_nodes <= 0)
		envelope->flags &= ~IT_ENVELOPE_ON;
	else {
		if (envelope->loop_end >= envelope->n_nodes || envelope->loop_start > envelope->loop_end) envelope->flags &= ~IT_ENVELOPE_LOOP_ON;
		if (envelope->sus_loop_end >= envelope->n_nodes || envelope->sus_loop_start > envelope->sus_loop_end) envelope->flags &= ~IT_ENVELOPE_SUSTAIN_LOOP;
	}

	return dumbfile_error(f);
}



static int it_read_old_instrument(IT_INSTRUMENT *instrument, DUMBFILE *f)
{
	int n;

	/*if (dumbfile_mgetl(f) != IT_INSTRUMENT_SIGNATURE)
		return -1;*/
	// XXX
	dumbfile_skip(f, 4);

    dumbfile_getnc((char *)instrument->filename, 13, f);
	instrument->filename[13] = 0;

	instrument->volume_envelope.flags = dumbfile_getc(f);
	instrument->volume_envelope.loop_start = dumbfile_getc(f);
	instrument->volume_envelope.loop_end = dumbfile_getc(f);
	instrument->volume_envelope.sus_loop_start = dumbfile_getc(f);
	instrument->volume_envelope.sus_loop_end = dumbfile_getc(f);

	/* Skip two unused bytes. */
	dumbfile_skip(f, 2);

	/* In the old instrument format, fadeout ranges from 0 to 64, and is
	 * subtracted at intervals from a value starting at 512. In the new
	 * format, all these values are doubled. Therefore we double when loading
	 * from the old instrument format - that way we don't have to think about
	 * it later.
	 */
	instrument->fadeout = dumbfile_igetw(f) << 1;
	instrument->new_note_action = dumbfile_getc(f);
	instrument->dup_check_type = dumbfile_getc(f);
	instrument->dup_check_action = DCA_NOTE_CUT; // This might be wrong!
	/** WARNING - what is the duplicate check action for old-style instruments? */

	/* Skip Tracker Version and Number of Samples. These are only used in
	 * separate instrument files. Also skip unused byte.
	 */
	dumbfile_skip(f, 4);

    dumbfile_getnc((char *)instrument->name, 26, f);
	instrument->name[26] = 0;

	/* Skip unused bytes following the Instrument Name. */
	dumbfile_skip(f, 6);

	instrument->pp_separation = 0;
	instrument->pp_centre = 60;
	instrument->global_volume = 128;
	/** WARNING - should global_volume be 64 or something? */
	instrument->default_pan = 32;
	/** WARNING - should default_pan be 128, meaning don`t use? */
	instrument->random_volume = 0;
	instrument->random_pan = 0;

	for (n = 0; n < 120; n++) {
		instrument->map_note[n] = dumbfile_getc(f);
		instrument->map_sample[n] = dumbfile_getc(f);
	}

	/* Skip "Volume envelope (200 bytes)". */
	// - need to know better what this is for though.
	dumbfile_skip(f, 200);

#ifdef INVESTIGATE_OLD_INSTRUMENTS
	fprintf(stderr, "Inst %02d Env:", n);
#endif

	for (n = 0; n < 25; n++)
	{
		instrument->volume_envelope.node_t[n] = dumbfile_getc(f);
		instrument->volume_envelope.node_y[n] = dumbfile_getc(f);

#ifdef INVESTIGATE_OLD_INSTRUMENTS
		fprintf(stderr, " %d,%d",
				instrument->volume_envelope.node_t[n],
				instrument->volume_envelope.node_y[n]);
#endif

		// This loop is unfinished, as we can probably escape from it before
		// the end if we want to. Hence the otherwise useless dumbfile_skip()
		// call below.
	}
	dumbfile_skip(f, 50 - (n << 1));
	instrument->volume_envelope.n_nodes = n;

#ifdef INVESTIGATE_OLD_INSTRUMENTS
	fprintf(stderr, "\n");
#endif

	if (dumbfile_error(f))
		return -1;

	{
		IT_ENVELOPE *envelope = &instrument->volume_envelope;
		if (envelope->n_nodes <= 0)
			envelope->flags &= ~IT_ENVELOPE_ON;
		else {
			if (envelope->loop_end >= envelope->n_nodes || envelope->loop_start > envelope->loop_end) envelope->flags &= ~IT_ENVELOPE_LOOP_ON;
			if (envelope->sus_loop_end >= envelope->n_nodes || envelope->sus_loop_start > envelope->sus_loop_end) envelope->flags &= ~IT_ENVELOPE_SUSTAIN_LOOP;
		}
	}

	instrument->filter_cutoff = 127;
	instrument->filter_resonance = 0;

	instrument->pan_envelope.flags = 0;
	instrument->pitch_envelope.flags = 0;

	return 0;
}



static int it_read_instrument(IT_INSTRUMENT *instrument, DUMBFILE *f, int maxlen)
{
	int n, len = 0;

	/*if (dumbfile_mgetl(f) != IT_INSTRUMENT_SIGNATURE)
		return -1;*/
	// XXX

	if (maxlen) len = dumbfile_pos(f);

	dumbfile_skip(f, 4);

    dumbfile_getnc((char *)instrument->filename, 13, f);
	instrument->filename[13] = 0;

	instrument->new_note_action = dumbfile_getc(f);
	instrument->dup_check_type = dumbfile_getc(f);
	instrument->dup_check_action = dumbfile_getc(f);
	instrument->fadeout = dumbfile_igetw(f);
	instrument->pp_separation = dumbfile_getc(f);
	instrument->pp_centre = dumbfile_getc(f);
	instrument->global_volume = dumbfile_getc(f);
	instrument->default_pan = dumbfile_getc(f);
	instrument->random_volume = dumbfile_getc(f);
	instrument->random_pan = dumbfile_getc(f);

	/* Skip Tracker Version and Number of Samples. These are only used in
	 * separate instrument files. Also skip unused byte.
	 */
	dumbfile_skip(f, 4);

    dumbfile_getnc((char *)instrument->name, 26, f);
	instrument->name[26] = 0;

	instrument->filter_cutoff = dumbfile_getc(f);
	instrument->filter_resonance = dumbfile_getc(f);

	/* Skip MIDI Channel, Program and Bank. */
	//dumbfile_skip(f, 4);
	/*instrument->output = dumbfile_getc(f);
	if ( instrument->output > 16 ) {
		instrument->output -= 128;
	} else {
		instrument->output = 0;
	}
	dumbfile_skip(f, 3);*/
	dumbfile_skip(f, 4);

	for (n = 0; n < 120; n++) {
		instrument->map_note[n] = dumbfile_getc(f);
		instrument->map_sample[n] = dumbfile_getc(f);
	}

	if (dumbfile_error(f))
		return -1;

	if (it_read_envelope(&instrument->volume_envelope, f)) return -1;
	if (it_read_envelope(&instrument->pan_envelope, f)) return -1;
	if (it_read_envelope(&instrument->pitch_envelope, f)) return -1;

	if (maxlen) {
		len = dumbfile_pos(f) - len;
		if ( maxlen - len < 124 ) return 0;
	}

	if ( dumbfile_mgetl(f) == IT_MPTX_SIGNATURE ) {
		for ( n = 0; n < 120; n++ ) {
			instrument->map_sample[ n ] += dumbfile_getc( f ) << 8;
		}

		if (dumbfile_error(f))
			return -1;
	}

	/*if ( dumbfile_mgetl(f) == IT_INSM_SIGNATURE ) {
		int32 end = dumbfile_igetl(f);
		end += dumbfile_pos(f);
		while ( dumbfile_pos(f) < end ) {
			int chunkid = dumbfile_igetl(f);
			switch ( chunkid ) {
				case DUMB_ID('P','L','U','G'):
					instrument->output = dumbfile_getc(f);
					break;
				default:
					chunkid = chunkid / 0x100 + dumbfile_getc(f) * 0x1000000;
					break;
			}
		}

		if (dumbfile_error(f))
			return -1;
	}*/

	return 0;
}



static int it_read_sample_header(IT_SAMPLE *sample, unsigned char *convert, int32 *offset, DUMBFILE *f)
{
	/* XXX
	if (dumbfile_mgetl(f) != IT_SAMPLE_SIGNATURE)
		return -1;*/
	int hax = 0;
	int32 s = dumbfile_mgetl(f);
	if (s != IT_SAMPLE_SIGNATURE) {
		if ( s == ( IT_SAMPLE_SIGNATURE >> 16 ) ) {
			s <<= 16;
			s |= dumbfile_mgetw(f);
			if ( s != IT_SAMPLE_SIGNATURE )
				return -1;
			hax = 1;
		}
	}

    dumbfile_getnc((char *)sample->filename, 13, f);
	sample->filename[13] = 0;

	sample->global_volume = dumbfile_getc(f);
	sample->flags = dumbfile_getc(f);
	sample->default_volume = dumbfile_getc(f);

    dumbfile_getnc((char *)sample->name, 26, f);
	sample->name[26] = 0;

	*convert = dumbfile_getc(f);
	sample->default_pan = dumbfile_getc(f);
	sample->length = dumbfile_igetl(f);
	sample->loop_start = dumbfile_igetl(f);
	sample->loop_end = dumbfile_igetl(f);
	sample->C5_speed = dumbfile_igetl(f);
	sample->sus_loop_start = dumbfile_igetl(f);
	sample->sus_loop_end = dumbfile_igetl(f);

#ifdef STEREO_SAMPLES_COUNT_AS_TWO
	if (sample->flags & IT_SAMPLE_STEREO) {
		sample->length >>= 1;
		sample->loop_start >>= 1;
		sample->loop_end >>= 1;
		sample->C5_speed >>= 1;
		sample->sus_loop_start >>= 1;
		sample->sus_loop_end >>= 1;
	}
#endif

	if (sample->flags & IT_SAMPLE_EXISTS) {
		if (sample->length <= 0)
			sample->flags &= ~IT_SAMPLE_EXISTS;
		else {
			if ((unsigned int)sample->loop_end > (unsigned int)sample->length)
				sample->flags &= ~IT_SAMPLE_LOOP;
			else if ((unsigned int)sample->loop_start >= (unsigned int)sample->loop_end)
				sample->flags &= ~IT_SAMPLE_LOOP;

			if ((unsigned int)sample->sus_loop_end > (unsigned int)sample->length)
				sample->flags &= ~IT_SAMPLE_SUS_LOOP;
			else if ((unsigned int)sample->sus_loop_start >= (unsigned int)sample->sus_loop_end)
				sample->flags &= ~IT_SAMPLE_SUS_LOOP;

			/* We may be able to truncate the sample to save memory. */
			if (sample->flags & IT_SAMPLE_LOOP &&
				*convert != 0xFF) { /* not truncating compressed samples, for now... */
				if ((sample->flags & IT_SAMPLE_SUS_LOOP) && sample->sus_loop_end >= sample->loop_end)
					sample->length = sample->sus_loop_end;
				else
					sample->length = sample->loop_end;
			}
		}
	}

	*offset = dumbfile_igetl(f);

	sample->vibrato_speed = dumbfile_getc(f);
	sample->vibrato_depth = dumbfile_getc(f);
	if ( ! hax ) {
		sample->vibrato_rate = dumbfile_getc(f);
		sample->vibrato_waveform = dumbfile_getc(f);
	} else {
		sample->vibrato_rate = 0;
		sample->vibrato_waveform = 0;
	}
	sample->finetune = 0;
	sample->max_resampling_quality = -1;

	return dumbfile_error(f);
}

int32 _dumb_it_read_sample_data_adpcm4(IT_SAMPLE *sample, DUMBFILE *f)
{
	int32 n, len, delta;
	signed char * ptr, * end;
	signed char compression_table[16];
    if (dumbfile_getnc((char *)compression_table, 16, f) != 16)
        return -1;
	ptr = (signed char *) sample->data;
	delta = 0;

	end = ptr + sample->length;
	len = (sample->length + 1) / 2;
	for (n = 0; n < len; n++) {
		int b = dumbfile_getc(f);
		if (b < 0) return -1;
		delta += compression_table[b & 0x0F];
		*ptr++ = (signed char)delta;
		if (ptr >= end) break;
		delta += compression_table[b >> 4];
		*ptr++ = (signed char)delta;
	}

	return 0;
}


static int32 it_read_sample_data(IT_SAMPLE *sample, unsigned char convert, DUMBFILE *f)
{
	int32 n;

	int32 datasize = sample->length;
	if (sample->flags & IT_SAMPLE_STEREO) datasize <<= 1;

	sample->data = malloc(datasize * (sample->flags & IT_SAMPLE_16BIT ? 2 : 1));
	if (!sample->data)
		return -1;

	if (!(sample->flags & IT_SAMPLE_16BIT) && (convert == 0xFF)) {
		if (_dumb_it_read_sample_data_adpcm4(sample, f) < 0)
			return -1;
	} else if (sample->flags & 8) {
		/* If the sample is packed, then we must unpack it. */

		/* Behavior as defined by greasemonkey's munch.py and observed by XMPlay and OpenMPT */

		if (sample->flags & IT_SAMPLE_STEREO) {
			if (sample->flags & IT_SAMPLE_16BIT) {
				decompress16(f, (short *) sample->data, datasize >> 1, convert & 4, 1);
				decompress16(f, (short *) sample->data + 1, datasize >> 1, convert & 4, 1);
			} else {
				decompress8(f, (signed char *) sample->data, datasize >> 1, convert & 4, 1);
				decompress8(f, (signed char *) sample->data + 1, datasize >> 1, convert & 4, 1);
			}
		} else {
			if (sample->flags & IT_SAMPLE_16BIT)
				decompress16(f, (short *) sample->data, datasize, convert & 4, 0);
			else
				decompress8(f, (signed char *) sample->data, datasize, convert & 4, 0);
		}
 	} else if (sample->flags & IT_SAMPLE_16BIT) {
		if (sample->flags & IT_SAMPLE_STEREO) {
			if (convert & 2) {
				for (n = 0; n < datasize; n += 2)
					((short *)sample->data)[n] = dumbfile_mgetw(f);
				for (n = 1; n < datasize; n += 2)
					((short *)sample->data)[n] = dumbfile_mgetw(f);
			} else {
				for (n = 0; n < datasize; n += 2)
					((short *)sample->data)[n] = dumbfile_igetw(f);
				for (n = 1; n < datasize; n += 2)
					((short *)sample->data)[n] = dumbfile_igetw(f);
			}
		} else {
 			if (convert & 2)
				for (n = 0; n < datasize; n++)
					((short *)sample->data)[n] = dumbfile_mgetw(f);
			else
				for (n = 0; n < datasize; n++)
					((short *)sample->data)[n] = dumbfile_igetw(f);
		}
 	} else {
		if (sample->flags & IT_SAMPLE_STEREO) {
			for (n = 0; n < datasize; n += 2)
				((signed char *)sample->data)[n] = dumbfile_getc(f);
			for (n = 1; n < datasize; n += 2)
				((signed char *)sample->data)[n] = dumbfile_getc(f);
		} else
			for (n = 0; n < datasize; n++)
				((signed char *)sample->data)[n] = dumbfile_getc(f);
	}

	if (dumbfile_error(f))
		return -1;

	if (!(convert & 1)) {
		/* Convert to signed. */
		if (sample->flags & IT_SAMPLE_16BIT)
			for (n = 0; n < datasize; n++)
				((short *)sample->data)[n] ^= 0x8000;
		else
			for (n = 0; n < datasize; n++)
				((signed char *)sample->data)[n] ^= 0x80;
	}

	/* NOT SUPPORTED:
	 *
	 * convert &  4 - Samples stored as delta values
	 * convert & 16 - Samples stored as TX-Wave 12-bit values
	 * convert & 32 - Left/Right/All Stereo prompt
	 */

	return 0;
}



//#define DETECT_DUPLICATE_CHANNELS
#ifdef DETECT_DUPLICATE_CHANNELS
#include <stdio.h>
#endif
static int it_read_pattern(IT_PATTERN *pattern, DUMBFILE *f, unsigned char *buffer)
{
	unsigned char cmask[DUMB_IT_N_CHANNELS];
	unsigned char cnote[DUMB_IT_N_CHANNELS];
	unsigned char cinstrument[DUMB_IT_N_CHANNELS];
	unsigned char cvolpan[DUMB_IT_N_CHANNELS];
	unsigned char ceffect[DUMB_IT_N_CHANNELS];
	unsigned char ceffectvalue[DUMB_IT_N_CHANNELS];
#ifdef DETECT_DUPLICATE_CHANNELS
	IT_ENTRY *dupentry[DUMB_IT_N_CHANNELS];
#endif

	int n_entries = 0;
	int buflen;
	int bufpos = 0;

	IT_ENTRY *entry;

	unsigned char channel;
	unsigned char mask;

	memset(cmask, 0, sizeof(cmask));
	memset(cnote, 0, sizeof(cnote));
	memset(cinstrument, 0, sizeof(cinstrument));
	memset(cvolpan, 0, sizeof(cvolpan));
	memset(ceffect, 0, sizeof(ceffect));
	memset(ceffectvalue, 0, sizeof(ceffectvalue));
#ifdef DETECT_DUPLICATE_CHANNELS
	{
		int i;
		for (i = 0; i < DUMB_IT_N_CHANNELS; i++) dupentry[i] = NULL;
	}
#endif

	buflen = dumbfile_igetw(f);
	pattern->n_rows = dumbfile_igetw(f);

	/* Skip four unused bytes. */
	dumbfile_skip(f, 4);

	if (dumbfile_error(f))
		return -1;

	/* Read in the pattern data. */
    dumbfile_getnc((char *)buffer, buflen, f);

	if (dumbfile_error(f))
		return -1;

	/* Scan the pattern data, and work out how many entries we need room for. */
	while (bufpos < buflen) {
		unsigned char b = buffer[bufpos++];

		if (b == 0) {
			/* End of row */
			n_entries++;
			continue;
		}

		channel = (b - 1) & 63;

		if (b & 128)
			cmask[channel] = mask = buffer[bufpos++];
		else
			mask = cmask[channel];

		{
			static const unsigned char used[16] = {0, 1, 1, 2, 1, 2, 2, 3, 2, 3, 3, 4, 3, 4, 4, 5};
			n_entries += (mask != 0);
			bufpos += used[mask & 15];
		}
	}

	pattern->n_entries = n_entries;

	pattern->entry = malloc(n_entries * sizeof(*pattern->entry));

	if (!pattern->entry)
		return -1;

	bufpos = 0;
	memset(cmask, 0, sizeof(cmask));

	entry = pattern->entry;

	while (bufpos < buflen) {
		unsigned char b = buffer[bufpos++];

		if (b == 0) {
			/* End of row */
			IT_SET_END_ROW(entry);
			entry++;
#ifdef DETECT_DUPLICATE_CHANNELS
			{
				int i;
				for (i = 0; i < DUMB_IT_N_CHANNELS; i++) dupentry[i] = NULL;
			}
#endif
			continue;
		}

		channel = (b - 1) & 63;

		if (b & 128)
			cmask[channel] = mask = buffer[bufpos++];
		else
			mask = cmask[channel];

		if (mask) {
			entry->mask = (mask & 15) | (mask >> 4);
			entry->channel = channel;

			if (mask & IT_ENTRY_NOTE)
				cnote[channel] = entry->note = buffer[bufpos++];
			else if (mask & (IT_ENTRY_NOTE << 4))
				entry->note = cnote[channel];

			if (mask & IT_ENTRY_INSTRUMENT)
				cinstrument[channel] = entry->instrument = buffer[bufpos++];
			else if (mask & (IT_ENTRY_INSTRUMENT << 4))
				entry->instrument = cinstrument[channel];

			if (mask & IT_ENTRY_VOLPAN)
				cvolpan[channel] = entry->volpan = buffer[bufpos++];
			else if (mask & (IT_ENTRY_VOLPAN << 4))
				entry->volpan = cvolpan[channel];

			if (mask & IT_ENTRY_EFFECT) {
				ceffect[channel] = entry->effect = buffer[bufpos++];
				ceffectvalue[channel] = entry->effectvalue = buffer[bufpos++];
			} else {
				entry->effect = ceffect[channel];
				entry->effectvalue = ceffectvalue[channel];
			}

#ifdef DETECT_DUPLICATE_CHANNELS
			if (dupentry[channel]) {
				FILE *f = fopen("dupentry.txt", "a");
				if (!f) abort();
				fprintf(f, "Two events on channel %d:", channel);
				fprintf(f, "  Event #1:");
				if (dupentry[channel]->mask & IT_ENTRY_NOTE      ) fprintf(f, " %03d", dupentry[channel]->note      ); else fprintf(f, " ...");
				if (dupentry[channel]->mask & IT_ENTRY_INSTRUMENT) fprintf(f, " %03d", dupentry[channel]->instrument); else fprintf(f, " ...");
				if (dupentry[channel]->mask & IT_ENTRY_VOLPAN    ) fprintf(f, " %03d", dupentry[channel]->volpan    ); else fprintf(f, " ...");
				if (dupentry[channel]->mask & IT_ENTRY_EFFECT) fprintf(f, " %c%02X\n", 'A' - 1 + dupentry[channel]->effect, dupentry[channel]->effectvalue); else fprintf(f, " ...\n");
				fprintf(f, "  Event #2:");
				if (entry->mask & IT_ENTRY_NOTE      ) fprintf(f, " %03d", entry->note      ); else fprintf(f, " ...");
				if (entry->mask & IT_ENTRY_INSTRUMENT) fprintf(f, " %03d", entry->instrument); else fprintf(f, " ...");
				if (entry->mask & IT_ENTRY_VOLPAN    ) fprintf(f, " %03d", entry->volpan    ); else fprintf(f, " ...");
				if (entry->mask & IT_ENTRY_EFFECT) fprintf(f, " %c%02X\n", 'A' - 1 + entry->effect, entry->effectvalue); else fprintf(f, " ...\n");
				fclose(f);
			}
			dupentry[channel] = entry;
#endif

			entry++;
		}
	}

	ASSERT(entry == pattern->entry + n_entries);

	return 0;
}



/* Currently we assume the sample data are stored after the sample headers in
 * module files. This assumption may be unjustified; let me know if you have
 * trouble.
 */

#define IT_COMPONENT_SONG_MESSAGE 1
#define IT_COMPONENT_INSTRUMENT   2
#define IT_COMPONENT_PATTERN      3
#define IT_COMPONENT_SAMPLE       4

typedef struct IT_COMPONENT
{
	unsigned char type;
	unsigned short n;
	int32 offset;
	short sampfirst; /* component[sampfirst] = first sample data after this */
	short sampnext; /* sampnext is used to create linked lists of sample data */
}
IT_COMPONENT;



static int CDECL it_component_compare(const void *e1, const void *e2)
{
	return ((const IT_COMPONENT *)e1)->offset -
	       ((const IT_COMPONENT *)e2)->offset;
}



static sigdata_t *it_load_sigdata(DUMBFILE *f)
{
	DUMB_IT_SIGDATA *sigdata;

	int cwt, cmwt;
	int special;
	int message_length, message_offset;

	IT_COMPONENT *component;
	int n_components = 0;

	unsigned char sample_convert[4096];

	int n;

	unsigned char *buffer;

	if (dumbfile_mgetl(f) != IT_SIGNATURE)
    {
		return NULL;
    }

	sigdata = malloc(sizeof(*sigdata));

	if (!sigdata)
    {
		return NULL;
    }

	sigdata->song_message = NULL;
	sigdata->order = NULL;
	sigdata->instrument = NULL;
	sigdata->sample = NULL;
	sigdata->pattern = NULL;
	sigdata->midi = NULL;
	sigdata->checkpoint = NULL;

    dumbfile_getnc((char *)sigdata->name, 26, f);
	sigdata->name[26] = 0;

	/* Skip pattern row highlight info. */
	dumbfile_skip(f, 2);

	sigdata->n_orders = dumbfile_igetw(f);
	sigdata->n_instruments = dumbfile_igetw(f);
	sigdata->n_samples = dumbfile_igetw(f);
	sigdata->n_patterns = dumbfile_igetw(f);

	cwt = dumbfile_igetw(f);
	cmwt = dumbfile_igetw(f);

	sigdata->flags = dumbfile_igetw(f);
	special = dumbfile_igetw(f);

	sigdata->global_volume = dumbfile_getc(f);
	sigdata->mixing_volume = dumbfile_getc(f);
	sigdata->speed = dumbfile_getc(f);
	if (sigdata->speed == 0) sigdata->speed = 6; // Should we? What about tempo?
	sigdata->tempo = dumbfile_getc(f);
	sigdata->pan_separation = dumbfile_getc(f); /** WARNING: use this */

	/* Skip Pitch Wheel Depth */
	dumbfile_skip(f, 1);

	message_length = dumbfile_igetw(f);
	message_offset = dumbfile_igetl(f);

	/* Skip Reserved. */
	dumbfile_skip(f, 4);

    dumbfile_getnc((char *)sigdata->channel_pan, DUMB_IT_N_CHANNELS, f);
    dumbfile_getnc((char *)sigdata->channel_volume, DUMB_IT_N_CHANNELS, f);

	// XXX sample count
	if (dumbfile_error(f) || sigdata->n_orders <= 0 || sigdata->n_instruments > 256 || sigdata->n_samples > 4000 || sigdata->n_patterns > 256) {
		_dumb_it_unload_sigdata(sigdata);
		return NULL;
	}

	sigdata->order = malloc(sigdata->n_orders);
	if (!sigdata->order) {
		_dumb_it_unload_sigdata(sigdata);
		return NULL;
	}

	if (sigdata->n_instruments) {
		sigdata->instrument = malloc(sigdata->n_instruments * sizeof(*sigdata->instrument));
		if (!sigdata->instrument) {
			_dumb_it_unload_sigdata(sigdata);
			return NULL;
		}
	}

	if (sigdata->n_samples) {
		sigdata->sample = malloc(sigdata->n_samples * sizeof(*sigdata->sample));
		if (!sigdata->sample) {
			_dumb_it_unload_sigdata(sigdata);
			return NULL;
		}
		for (n = 0; n < sigdata->n_samples; n++)
			sigdata->sample[n].data = NULL;
	}

	if (sigdata->n_patterns) {
		sigdata->pattern = malloc(sigdata->n_patterns * sizeof(*sigdata->pattern));
		if (!sigdata->pattern) {
			_dumb_it_unload_sigdata(sigdata);
			return NULL;
		}
		for (n = 0; n < sigdata->n_patterns; n++)
			sigdata->pattern[n].entry = NULL;
	}

    dumbfile_getnc((char *)sigdata->order, sigdata->n_orders, f);
	sigdata->restart_position = 0;

	component = malloc(769 * sizeof(*component));
	if (!component) {
		_dumb_it_unload_sigdata(sigdata);
		return NULL;
	}

	if (special & 1) {
		component[n_components].type = IT_COMPONENT_SONG_MESSAGE;
		component[n_components].offset = message_offset;
		component[n_components].sampfirst = -1;
		n_components++;
	}

	for (n = 0; n < sigdata->n_instruments; n++) {
		component[n_components].type = IT_COMPONENT_INSTRUMENT;
		component[n_components].n = n;
		component[n_components].offset = dumbfile_igetl(f);
		component[n_components].sampfirst = -1;
		n_components++;
	}

	for (n = 0; n < sigdata->n_samples; n++) {
		component[n_components].type = IT_COMPONENT_SAMPLE;
		component[n_components].n = n;
		component[n_components].offset = dumbfile_igetl(f);
		component[n_components].sampfirst = -1;
		n_components++;
	}

	for (n = 0; n < sigdata->n_patterns; n++) {
		int32 offset = dumbfile_igetl(f);
		if (offset) {
			component[n_components].type = IT_COMPONENT_PATTERN;
			component[n_components].n = n;
			component[n_components].offset = offset;
			component[n_components].sampfirst = -1;
			n_components++;
		} else {
			/* Empty 64-row pattern */
			sigdata->pattern[n].n_rows = 64;
			sigdata->pattern[n].n_entries = 0;
		}
	}

	if (dumbfile_error(f)) {
		free(component);
		_dumb_it_unload_sigdata(sigdata);
		return NULL;
	}

	/*
	if (!(sigdata->flags & 128) != !(special & 8)) {
		fprintf(stderr, "Flags   Bit 7 (\"Request embedded MIDI configuration\"): %s\n", sigdata->flags & 128 ? "=SET=" : "clear");
		fprintf(stderr, "Special Bit 3     (\"MIDI configuration embedded\")    : %s\n", special        &   8 ? "=SET=" : "clear");
		fprintf(stderr, "entheh would like to investigate this IT file.\n");
		fprintf(stderr, "Please contact him! entheh@users.sf.net\n");
	}
	*/

	if (special & 8) {
		/* MIDI configuration is embedded. */
		unsigned char mididata[32];
		int i;
		sigdata->midi = malloc(sizeof(*sigdata->midi));
		if (!sigdata->midi) {
			free(component);
			_dumb_it_unload_sigdata(sigdata);
			return NULL;
			// Should we be happy with this outcome in some situations?
		}
		// What are we skipping?
		i = dumbfile_igetw(f);
		if (dumbfile_error(f) || dumbfile_skip(f, 8*i)) {
			free(component);
			_dumb_it_unload_sigdata(sigdata);
			return NULL;
		}
		/* Read embedded MIDI configuration */
		// What are the first 9 commands for?
		if (dumbfile_skip(f, 32*9)) {
			free(component);
			_dumb_it_unload_sigdata(sigdata);
			return NULL;
		}
		for (i = 0; i < 16; i++) {
			unsigned char len = 0;
			int j, leftdigit = -1;
            if (dumbfile_getnc((char *)mididata, 32, f) < 32) {
				free(component);
				_dumb_it_unload_sigdata(sigdata);
				return NULL;
			}
			sigdata->midi->SFmacroz[i] = 0;
			for (j = 0; j < 32; j++) {
				if (leftdigit >= 0) {
					if (mididata[j] == 0) {
						sigdata->midi->SFmacro[i][len++] = leftdigit;
						break;
					} else if (mididata[j] == ' ')
						sigdata->midi->SFmacro[i][len++] = leftdigit;
					else if (mididata[j] >= '0' && mididata[j] <= '9')
						sigdata->midi->SFmacro[i][len++] = (leftdigit << 4) | (mididata[j] - '0');
					else if (mididata[j] >= 'A' && mididata[j] <= 'F')
						sigdata->midi->SFmacro[i][len++] = (leftdigit << 4) | (mididata[j] - 'A' + 0xA);
					leftdigit = -1;
				} else if (mididata[j] == 0)
					break;
				else if (mididata[j] == 'z')
					sigdata->midi->SFmacroz[i] |= 1 << len++;
				else if (mididata[j] >= '0' && mididata[j] <= '9')
					leftdigit = mididata[j] - '0';
				else if (mididata[j] >= 'A' && mididata[j] <= 'F')
					leftdigit = mididata[j] - 'A' + 0xA;
			}
			sigdata->midi->SFmacrolen[i] = len;
		}
		for (i = 0; i < 128; i++) {
			unsigned char len = 0;
			int j, leftdigit = -1;
            dumbfile_getnc((char *)mididata, 32, f);
			for (j = 0; j < 32; j++) {
				if (leftdigit >= 0) {
					if (mididata[j] == 0) {
						sigdata->midi->Zmacro[i][len++] = leftdigit;
						break;
					} else if (mididata[j] == ' ')
						sigdata->midi->Zmacro[i][len++] = leftdigit;
					else if (mididata[j] >= '0' && mididata[j] <= '9')
						sigdata->midi->Zmacro[i][len++] = (leftdigit << 4) | (mididata[j] - '0');
					else if (mididata[j] >= 'A' && mididata[j] <= 'F')
						sigdata->midi->Zmacro[i][len++] = (leftdigit << 4) | (mididata[j] - 'A' + 0xA);
					leftdigit = -1;
				} else if (mididata[j] == 0)
					break;
				else if (mididata[j] >= '0' && mididata[j] <= '9')
					leftdigit = mididata[j] - '0';
				else if (mididata[j] >= 'A' && mididata[j] <= 'F')
					leftdigit = mididata[j] - 'A' + 0xA;
			}
			sigdata->midi->Zmacrolen[i] = len;
		}
	}

	sigdata->flags &= IT_REAL_FLAGS;

    qsort(component, n_components, sizeof(IT_COMPONENT), &it_component_compare);

	buffer = malloc(65536);
	if (!buffer) {
		free(component);
		_dumb_it_unload_sigdata(sigdata);
		return NULL;
	}

	for (n = 0; n < n_components; n++) {
		int32 offset;
		int m;

		/* XXX */
		if ( component[n].offset == 0 ) {
			switch (component[n].type) {
				case IT_COMPONENT_INSTRUMENT:
					memset( &sigdata->instrument[component[n].n], 0, sizeof(IT_INSTRUMENT) );
					break;
				case IT_COMPONENT_SAMPLE:
					memset( &sigdata->sample[component[n].n], 0, sizeof(IT_SAMPLE) );
					break;
				case IT_COMPONENT_PATTERN:
					{
						IT_PATTERN * p = &sigdata->pattern[component[n].n];
						p->entry = 0;
						p->n_rows = 64;
						p->n_entries = 0;
					}
					break;
			}
			continue;
		}

        if (dumbfile_seek(f, component[n].offset, DFS_SEEK_SET)) {
			free(buffer);
			free(component);
			_dumb_it_unload_sigdata(sigdata);
			return NULL;
		}

		switch (component[n].type) {

			case IT_COMPONENT_SONG_MESSAGE:
				if ( n < n_components ) {
					message_length = min( message_length, component[n+1].offset - component[n].offset );
				}
				sigdata->song_message = malloc(message_length + 1);
				if (sigdata->song_message) {
                    if (dumbfile_getnc((char *)sigdata->song_message, message_length, f) < message_length) {
						free(buffer);
						free(component);
						_dumb_it_unload_sigdata(sigdata);
						return NULL;
					}
					sigdata->song_message[message_length] = 0;
				}
				break;

			case IT_COMPONENT_INSTRUMENT:
				if (cmwt < 0x200)
					m = it_read_old_instrument(&sigdata->instrument[component[n].n], f);
				else
					m = it_read_instrument(&sigdata->instrument[component[n].n], f, (n + 1 < n_components) ? (component[n+1].offset - component[n].offset) : 0);

				if (m) {
					free(buffer);
					free(component);
					_dumb_it_unload_sigdata(sigdata);
					return NULL;
				}
				break;

			case IT_COMPONENT_PATTERN:
				if (it_read_pattern(&sigdata->pattern[component[n].n], f, buffer)) {
					free(buffer);
					free(component);
					_dumb_it_unload_sigdata(sigdata);
					return NULL;
				}
				break;

			case IT_COMPONENT_SAMPLE:
				if (it_read_sample_header(&sigdata->sample[component[n].n], &sample_convert[component[n].n], &offset, f)) {
					free(buffer);
					free(component);
					_dumb_it_unload_sigdata(sigdata);
					return NULL;
				}

				if (sigdata->sample[component[n].n].flags & IT_SAMPLE_EXISTS) {
					short *sample;

					for (m = n + 1; m < n_components; m++)
						if (component[m].offset > offset)
							break;
					m--;

					sample = &component[m].sampfirst;

					while (*sample >= 0 && component[*sample].offset <= offset)
						sample = &component[*sample].sampnext;

					component[n].sampnext = *sample;
					*sample = n;

					component[n].offset = offset;
				}
		}

		m = component[n].sampfirst;

		while (m >= 0) {
            if (dumbfile_seek(f, component[m].offset, DFS_SEEK_SET)) {
				free(buffer);
				free(component);
				_dumb_it_unload_sigdata(sigdata);
				return NULL;
			}

			if (it_read_sample_data(&sigdata->sample[component[m].n], sample_convert[component[m].n], f)) {
				free(buffer);
				free(component);
				_dumb_it_unload_sigdata(sigdata);
				return NULL;
			}

			m = component[m].sampnext;
		}
    }

    for ( n = 0; n < 10; n++ )
    {
        if ( dumbfile_getc( f ) == 'X' )
        {
            if ( dumbfile_getc( f ) == 'T' )
            {
                if ( dumbfile_getc( f ) == 'P' )
                {
                    if ( dumbfile_getc( f ) == 'M' )
                    {
                        break;
                    }
                }
            }
        }
    }

    if ( !dumbfile_error( f ) && n < 10 )
    {
        unsigned int mptx_id = dumbfile_igetl( f );
        while ( !dumbfile_error( f ) && mptx_id != DUMB_ID('M','P','T','S') )
        {
            unsigned int size = dumbfile_igetw( f );
            switch (mptx_id)
            {
            /* TODO: Add instrument extension readers */

            default:
                dumbfile_skip(f, size * sigdata->n_instruments);
                break;
            }

            mptx_id = dumbfile_igetl( f );
        }

        mptx_id = dumbfile_igetl( f );
        while ( !dumbfile_error(f) && dumbfile_pos(f) < dumbfile_get_size(f) )
        {
            unsigned int size = dumbfile_igetw( f );
            switch (mptx_id)
            {
            /* TODO: Add more song extension readers */

            case DUMB_ID('D','T','.','.'):
                if ( size == 2 )
                    sigdata->tempo = dumbfile_igetw( f );
                else if ( size == 4 )
                    sigdata->tempo = dumbfile_igetl( f );
                break;

            default:
                dumbfile_skip(f, size);
                break;
            }
            mptx_id = dumbfile_igetl( f );
        }
    }

    free(buffer);
	free(component);

	_dumb_it_fix_invalid_orders(sigdata);

	return sigdata;
}



DUH *DUMBEXPORT dumb_read_it_quick(DUMBFILE *f)
{
	sigdata_t *sigdata;

	DUH_SIGTYPE_DESC *descptr = &_dumb_sigtype_it;

	sigdata = it_load_sigdata(f);

	if (!sigdata)
		return NULL;

	{
		const char *tag[2][2];
		tag[0][0] = "TITLE";
        tag[0][1] = (const char *)(((DUMB_IT_SIGDATA *)sigdata)->name);
		tag[1][0] = "FORMAT";
		tag[1][1] = "IT";
		return make_duh(-1, 2, (const char *const (*)[2])tag, 1, &descptr, &sigdata);
	}
}
