/* Extended Module Player
 * Copyright (C) 1996-2024 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "../common.h"
#include "loader.h"

#ifndef LIBXMP_CORE_PLAYER
/*
 * From the Audio File Formats (version 2.5)
 * Submitted-by: Guido van Rossum <guido@cwi.nl>
 * Last-modified: 27-Aug-1992
 *
 * The Acorn Archimedes uses a variation on U-LAW with the bit order
 * reversed and the sign bit in bit 0.  Being a 'minority' architecture,
 * Arc owners are quite adept at converting sound/image formats from
 * other machines, and it is unlikely that you'll ever encounter sound in
 * one of the Arc's own formats (there are several).
 */
static const int8 vdic_table[128] = {
	/*   0 */	  0,   0,   0,   0,   0,   0,   0,   0,
	/*   8 */	  0,   0,   0,   0,   0,   0,   0,   0,
	/*  16 */	  0,   0,   0,   0,   0,   0,   0,   0,
	/*  24 */	  1,   1,   1,   1,   1,   1,   1,   1,
	/*  32 */	  1,   1,   1,   1,   2,   2,   2,   2,
	/*  40 */	  2,   2,   2,   2,   3,   3,   3,   3,
	/*  48 */	  3,   3,   4,   4,   4,   4,   5,   5,
	/*  56 */	  5,   5,   6,   6,   6,   6,   7,   7,
	/*  64 */	  7,   8,   8,   9,   9,  10,  10,  11,
	/*  72 */	 11,  12,  12,  13,  13,  14,  14,  15,
	/*  80 */	 15,  16,  17,  18,  19,  20,  21,  22,
	/*  88 */	 23,  24,  25,  26,  27,  28,  29,  30,
	/*  96 */	 31,  33,  34,  36,  38,  40,  42,  44,
	/* 104 */	 46,  48,  50,  52,  54,  56,  58,  60,
	/* 112 */	 62,  65,  68,  72,  77,  80,  84,  91,
	/* 120 */	 95,  98, 103, 109, 114, 120, 126, 127
};

/* Convert 7 bit samples to 8 bit */
static void convert_7bit_to_8bit(uint8 *p, int l)
{
	for (; l--; p++) {
		*p <<= 1;
	}
}

/* Convert Archimedes VIDC samples to linear */
static void convert_vidc_to_linear(uint8 *p, int l)
{
	int i;
	int8 amp;
	uint8 x;

	for (i = 0; i < l; i++) {
		x = p[i];
		amp = vdic_table[x >> 1];
		p[i] = (uint8)((x & 0x01) ? -amp : amp);
	}
}

static void adpcm4_decoder(uint8 *inp, uint8 *outp, char *tab, int len)
{
	char delta = 0;
	uint8 b0, b1;
	int i;

	len = (len + 1) / 2;

	for (i = 0; i < len; i++) {
		b0 = *inp;
		b1 = *inp++ >> 4;
		delta += tab[b0 & 0x0f];
		*outp++ = delta;
		delta += tab[b1 & 0x0f];
		*outp++ = delta;
	}
}
#endif

/* Convert differential to absolute sample data */
static void convert_delta(uint8 *p, int frames, int is_16bit, int channels)
{
	uint16 *w = (uint16 *)p;
	uint16 absval;
	int chn, i;

	if (is_16bit) {
		for (chn = 0; chn < channels; chn++) {
			absval = 0;
			for (i = 0; i < frames; i++) {
				absval = *w + absval;
				*w++ = absval;
			}
		}
	} else {
		for (chn = 0; chn < channels; chn++) {
			absval = 0;
			for (i = 0; i < frames; i++) {
				absval = *p + absval;
				*p++ = (uint8) absval;
			}
		}
	}
}

/* Convert signed to unsigned sample data */
static void convert_signal(uint8 *p, int l, int r)
{
	uint16 *w = (uint16 *)p;

	if (r) {
		for (; l--; w++)
			*w += 0x8000;
	} else {
		for (; l--; p++)
			*p += (unsigned char)0x80;
	}
}

/* Convert little-endian 16 bit samples to big-endian */
static void convert_endian(uint8 *p, int l)
{
	uint8 b;
	int i;

	for (i = 0; i < l; i++) {
		b = p[0];
		p[0] = p[1];
		p[1] = b;
		p += 2;
	}
}

/* Convert non-interleaved stereo to interleaved stereo.
 * Due to tracker quirks this should be done after delta decoding, etc. */
static void convert_stereo_interleaved(void * LIBXMP_RESTRICT _out,
 const void *in, int frames, int is_16bit)
{
	int i;

	if (is_16bit) {
		const int16 *in_l = (const int16 *)in;
		const int16 *in_r = in_l + frames;
		int16 *out = (int16 *)_out;

		for (i = 0; i < frames; i++) {
			*(out++) = *(in_l++);
			*(out++) = *(in_r++);
		}
	} else {
		const uint8 *in_l = (const uint8 *)in;
		const uint8 *in_r = in_l + frames;
		uint8 *out = (uint8 *)_out;

		for (i = 0; i < frames; i++) {
			*(out++) = *(in_l++);
			*(out++) = *(in_r++);
		}
	}
}


int libxmp_load_sample(struct module_data *m, HIO_HANDLE *f, int flags, struct xmp_sample *xxs, const void *buffer)
{
	unsigned char *tmp = NULL;
	unsigned char *dest;
	int channels = 1;
	int framelen;
	int bytelen, extralen, i;

#ifndef LIBXMP_CORE_PLAYER
	/* Adlib FM patches */
	if (flags & SAMPLE_FLAG_ADLIB) {
		return 0;
	}
#endif

	/* Empty or invalid samples
	 */
	if (xxs->len <= 0) {
		return 0;
	}

	/* Skip sample loading
	 * FIXME: fails for ADPCM samples
	 *
	 * + Sanity check: skip huge samples (likely corrupt module)
	 */
	if (xxs->len > MAX_SAMPLE_SIZE || (m && m->smpctl & XMP_SMPCTL_SKIP)) {
		if (~flags & SAMPLE_FLAG_NOLOAD) {
			/* coverity[check_return] */
			hio_seek(f, xxs->len, SEEK_CUR);
		}
		return 0;
	}

	/* Patches with samples
	 * Allocate extra sample for interpolation.
	 */
	bytelen = xxs->len;
	framelen = 1;
	extralen = 4;

	if (xxs->flg & XMP_SAMPLE_16BIT) {
		bytelen *= 2;
		extralen *= 2;
		framelen *= 2;
	}
	if (xxs->flg & XMP_SAMPLE_STEREO) {
		bytelen *= 2;
		extralen *= 2;
		framelen *= 2;
		channels = 2;
	}

	/* If this sample starts at or after EOF, skip it entirely.
	 */
	if (~flags & SAMPLE_FLAG_NOLOAD) {
		long file_pos, file_len;
		long remaining = 0;
		long over = 0;
		if (!f) {
			return 0;
		}
		file_pos = hio_tell(f);
		file_len = hio_size(f);
		if (file_pos >= file_len) {
			D_(D_WARN "ignoring sample at EOF");
			return 0;
		}
		/* If this sample goes past EOF, truncate it. */
		remaining = file_len - file_pos;
#ifndef LIBXMP_CORE_PLAYER
		if (flags & SAMPLE_FLAG_ADPCM) {
			long bound = 16 + ((bytelen + 1) >> 1);
			if (remaining < 16) {
				D_(D_WARN "ignoring truncated ADPCM sample");
				return 0;
			}
			if (bound > remaining) {
				over = bound - remaining;
				bytelen = (remaining - 16) << 1;
			}
		} else
#endif
		if (bytelen > remaining) {
			over = bytelen - remaining;
			bytelen = remaining;
		}

		if (over) {
			D_(D_WARN "sample would extend %ld bytes past EOF; truncating to %ld",
				over, remaining);

			/* Trim extra bytes non-aligned to sample frame. */
			bytelen -= bytelen & (framelen - 1);

			xxs->len = bytelen;
			if (xxs->flg & XMP_SAMPLE_16BIT)
				xxs->len >>= 1;
			if (xxs->flg & XMP_SAMPLE_STEREO)
				xxs->len >>= 1;
		}
	}

	/* Loop parameters sanity check
	 */
	if (xxs->lps < 0) {
		xxs->lps = 0;
	}
	if (xxs->lpe > xxs->len) {
		xxs->lpe = xxs->len;
	}
	if (xxs->lps >= xxs->len || xxs->lps >= xxs->lpe) {
		xxs->lps = xxs->lpe = 0;
		xxs->flg &= ~(XMP_SAMPLE_LOOP | XMP_SAMPLE_LOOP_BIDIR);
	}

	/* Disable bidirectional loop flag if sample is not looped
	 */
	if (xxs->flg & XMP_SAMPLE_LOOP_BIDIR) {
		if (~xxs->flg & XMP_SAMPLE_LOOP)
			xxs->flg &= ~XMP_SAMPLE_LOOP_BIDIR;
	}
	if (xxs->flg & XMP_SAMPLE_SLOOP_BIDIR) {
		if (~xxs->flg & XMP_SAMPLE_SLOOP)
			xxs->flg &= ~XMP_SAMPLE_SLOOP_BIDIR;
	}

	/* add guard bytes before the buffer for higher order interpolation */
	xxs->data = (unsigned char *) malloc(bytelen + extralen + 4);
	if (xxs->data == NULL) {
		goto err;
	}

	*(uint32 *)xxs->data = 0;
	xxs->data += 4;
	dest = xxs->data;

	/* If this is a non-interleaved stereo sample, most conversions need
	 * to occur in an intermediate buffer prior to interleaving. Most
	 * formats supporting stereo samples use non-interleaved stereo.
	 */
	if ((xxs->flg & XMP_SAMPLE_STEREO) && (~flags & SAMPLE_FLAG_INTERLEAVED)) {
		tmp = (unsigned char *) malloc(bytelen);
		if (!tmp)
			goto err2;

		dest = tmp;
	}

	if (flags & SAMPLE_FLAG_NOLOAD) {
		memcpy(dest, buffer, bytelen);
	} else
#ifndef LIBXMP_CORE_PLAYER
	if (flags & SAMPLE_FLAG_ADPCM) {
		int x2 = (bytelen + 1) >> 1;
		char table[16];

		if (hio_read(table, 1, 16, f) != 16) {
			goto err2;
		}
		if (hio_read(dest + x2, 1, x2, f) != x2) {
			goto err2;
		}
		adpcm4_decoder((uint8 *)dest + x2,
			       (uint8 *)dest, table, bytelen);
	} else
#endif
	{
		int x = hio_read(dest, 1, bytelen, f);
		if (x != bytelen) {
			D_(D_WARN "short read (%d) in sample load", x - bytelen);
			memset(dest + x, 0, bytelen - x);
		}
	}

#ifndef LIBXMP_CORE_PLAYER
	if (flags & SAMPLE_FLAG_7BIT) {
		convert_7bit_to_8bit(dest, xxs->len * channels);
	}
#endif

	/* Fix endianism if needed */
	if (xxs->flg & XMP_SAMPLE_16BIT) {
#ifdef WORDS_BIGENDIAN
		if (~flags & SAMPLE_FLAG_BIGEND)
			convert_endian(dest, xxs->len * channels);
#else
		if (flags & SAMPLE_FLAG_BIGEND)
			convert_endian(dest, xxs->len * channels);
#endif
	}

	/* Convert delta samples */
	if (flags & SAMPLE_FLAG_DIFF) {
		convert_delta(dest, xxs->len, xxs->flg & XMP_SAMPLE_16BIT, channels);
	} else if (flags & SAMPLE_FLAG_8BDIFF) {
		int len = xxs->len;
		if (xxs->flg & XMP_SAMPLE_16BIT) {
			len *= 2;
		}
		convert_delta(dest, len, 0, channels);
	}

	/* Convert samples to signed */
	if (flags & SAMPLE_FLAG_UNS) {
		convert_signal(dest, xxs->len * channels,
				xxs->flg & XMP_SAMPLE_16BIT);
	}

#ifndef LIBXMP_CORE_PLAYER
	if (flags & SAMPLE_FLAG_VIDC) {
		convert_vidc_to_linear(dest, xxs->len * channels);
	}
#endif

	/* Done converting individual samples; convert to interleaved. */
	if ((xxs->flg & XMP_SAMPLE_STEREO) && (~flags & SAMPLE_FLAG_INTERLEAVED)) {
		convert_stereo_interleaved(xxs->data, dest, xxs->len,
					   xxs->flg & XMP_SAMPLE_16BIT);
	}

	/* Check for full loop samples */
	if (flags & SAMPLE_FLAG_FULLREP) {
	    if (xxs->lps == 0 && xxs->len > xxs->lpe)
		xxs->flg |= XMP_SAMPLE_LOOP_FULL;
	}

	/* Add extra samples at end */
	for (i = 0; i < extralen; i++) {
		xxs->data[bytelen + i] = xxs->data[bytelen - framelen + i];
	}

	/* Add extra samples at start */
	for (i = -1; i >= -4; i--) {
		xxs->data[i] = xxs->data[framelen + i];
	}

	free(tmp);
	return 0;

    err2:
	libxmp_free_sample(xxs);
	free(tmp);
    err:
	return -1;
}

void libxmp_free_sample(struct xmp_sample *s)
{
	if (s->data) {
		free(s->data - 4);
		s->data = NULL;		/* prevent double free in PCM load error */
	}
}
