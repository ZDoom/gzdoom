/* Extended Module Player
 * Copyright (C) 1996-2022 Claudio Matsuoka and Hipolito Carraro Jr
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
	uint8 x;

	for (i = 0; i < l; i++) {
		x = p[i];
		p[i] = vdic_table[x >> 1];
		if (x & 0x01)
			p[i] *= -1;
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
static void convert_delta(uint8 *p, int l, int r)
{
	uint16 *w = (uint16 *)p;
	uint16 absval = 0;

	if (r) {
		for (; l--;) {
			absval = *w + absval;
			*w++ = absval;
		}
	} else {
		for (; l--;) {
			absval = *p + absval;
			*p++ = (uint8) absval;
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
			*p += (char)0x80;	/* cast needed by MSVC++ */
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

#if 0
/* Downmix stereo samples to mono */
static void convert_stereo_to_mono(uint8 *p, int l, int r)
{
	int16 *b = (int16 *)p;
	int i;

	if (r) {
		l /= 2;
		for (i = 0; i < l; i++)
			b[i] = (b[i * 2] + b[i * 2 + 1]) / 2;
	} else {
		for (i = 0; i < l; i++)
			p[i] = (p[i * 2] + p[i * 2 + 1]) / 2;
	}
}
#endif


int libxmp_load_sample(struct module_data *m, HIO_HANDLE *f, int flags, struct xmp_sample *xxs, const void *buffer)
{
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

	/* If this sample starts at or after EOF, skip it entirely.
	 */
	if (~flags & SAMPLE_FLAG_NOLOAD) {
		long file_pos, file_len;
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
		if (file_pos + xxs->len > file_len && (~flags & SAMPLE_FLAG_ADPCM)) {
			D_(D_WARN "sample would extend %ld bytes past EOF; truncating to %ld",
				file_pos + xxs->len - file_len, file_len - file_pos);
			xxs->len = file_len - file_pos;
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

	/* Patches with samples
	 * Allocate extra sample for interpolation.
	 */
	bytelen = xxs->len;
	extralen = 4;

	/* Disable birectional loop flag if sample is not looped
	 */
	if (xxs->flg & XMP_SAMPLE_LOOP_BIDIR) {
		if (~xxs->flg & XMP_SAMPLE_LOOP)
			xxs->flg &= ~XMP_SAMPLE_LOOP_BIDIR;
	}
	if (xxs->flg & XMP_SAMPLE_SLOOP_BIDIR) {
		if (~xxs->flg & XMP_SAMPLE_SLOOP)
			xxs->flg &= ~XMP_SAMPLE_SLOOP_BIDIR;
	}

	if (xxs->flg & XMP_SAMPLE_16BIT) {
		bytelen *= 2;
		extralen *= 2;
	}

	/* add guard bytes before the buffer for higher order interpolation */
	xxs->data = (unsigned char *) malloc(bytelen + extralen + 4);
	if (xxs->data == NULL) {
		goto err;
	}

	*(uint32 *)xxs->data = 0;
	xxs->data += 4;

	if (flags & SAMPLE_FLAG_NOLOAD) {
		memcpy(xxs->data, buffer, bytelen);
	} else
#ifndef LIBXMP_CORE_PLAYER
	if (flags & SAMPLE_FLAG_ADPCM) {
		int x2 = (bytelen + 1) >> 1;
		char table[16];

		if (hio_read(table, 1, 16, f) != 16) {
			goto err2;
		}
		if (hio_read(xxs->data + x2, 1, x2, f) != x2) {
			goto err2;
		}
		adpcm4_decoder((uint8 *)xxs->data + x2,
			       (uint8 *)xxs->data, table, bytelen);
	} else
#endif
	{
		int x = hio_read(xxs->data, 1, bytelen, f);
		if (x != bytelen) {
			D_(D_WARN "short read (%d) in sample load", x - bytelen);
			memset(xxs->data + x, 0, bytelen - x);
		}
	}

#ifndef LIBXMP_CORE_PLAYER
	if (flags & SAMPLE_FLAG_7BIT) {
		convert_7bit_to_8bit(xxs->data, xxs->len);
	}
#endif

	/* Fix endianism if needed */
	if (xxs->flg & XMP_SAMPLE_16BIT) {
#ifdef WORDS_BIGENDIAN
		if (~flags & SAMPLE_FLAG_BIGEND)
			convert_endian(xxs->data, xxs->len);
#else
		if (flags & SAMPLE_FLAG_BIGEND)
			convert_endian(xxs->data, xxs->len);
#endif
	}

	/* Convert delta samples */
	if (flags & SAMPLE_FLAG_DIFF) {
		convert_delta(xxs->data, xxs->len, xxs->flg & XMP_SAMPLE_16BIT);
	} else if (flags & SAMPLE_FLAG_8BDIFF) {
		int len = xxs->len;
		if (xxs->flg & XMP_SAMPLE_16BIT) {
			len *= 2;
		}
		convert_delta(xxs->data, len, 0);
	}

	/* Convert samples to signed */
	if (flags & SAMPLE_FLAG_UNS) {
		convert_signal(xxs->data, xxs->len,
				xxs->flg & XMP_SAMPLE_16BIT);
	}

#if 0
	/* Downmix stereo samples */
	if (flags & SAMPLE_FLAG_STEREO) {
		convert_stereo_to_mono(xxs->data, xxs->len,
					xxs->flg & XMP_SAMPLE_16BIT);
		xxs->len /= 2;
	}
#endif

#ifndef LIBXMP_CORE_PLAYER
	if (flags & SAMPLE_FLAG_VIDC) {
		convert_vidc_to_linear(xxs->data, xxs->len);
	}
#endif

	/* Check for full loop samples */
	if (flags & SAMPLE_FLAG_FULLREP) {
	    if (xxs->lps == 0 && xxs->len > xxs->lpe)
		xxs->flg |= XMP_SAMPLE_LOOP_FULL;
	}

	/* Add extra samples at end */
	if (xxs->flg & XMP_SAMPLE_16BIT) {
		for (i = 0; i < 8; i++) {
			xxs->data[bytelen + i] = xxs->data[bytelen - 2 + i];
		}
	} else {
		for (i = 0; i < 4; i++) {
			xxs->data[bytelen + i] = xxs->data[bytelen - 1 + i];
		}
	}

	/* Add extra samples at start */
	if (xxs->flg & XMP_SAMPLE_16BIT) {
		xxs->data[-2] = xxs->data[0];
		xxs->data[-1] = xxs->data[1];
	} else {
		xxs->data[-1] = xxs->data[0];
	}

	return 0;

#ifndef LIBXMP_CORE_PLAYER
    err2:
	libxmp_free_sample(xxs);
#endif
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
