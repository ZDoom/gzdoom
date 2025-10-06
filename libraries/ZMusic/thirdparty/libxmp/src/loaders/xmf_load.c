/* Extended Module Player
 * Copyright (C) 2023-2025 Alice Rowan <petrifiedrowan@gmail.com>
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

/* Loader for Astroidea XMF, used by Imperium Galactica and some other modules.
 * This format is completely unrelated to the MIDI XMF format.
 */

#include "loader.h"

static int xmf_test(HIO_HANDLE *, char *, const int);
static int xmf_load(struct module_data *, HIO_HANDLE *, const int);

const struct format_loader libxmp_loader_xmf = {
	"Astroidea XMF",
	xmf_test,
	xmf_load
};

#define XMF_SAMPLE_ARRAY_SIZE (16 * 256)

static int xmf_test(HIO_HANDLE *f, char *t, const int start)
{
	uint8 buf[XMF_SAMPLE_ARRAY_SIZE];
	uint8 *pos;
	uint32 samples_length = 0;
	long length;
	int samples_start;
	int num_patterns;
	int num_channels;
	int num_ins;
	int xmf_type;
	int i;

	/* This value is 0x03 for all Imperium Galactica modules.
	 * The demo "Prostate 666" and all other XMFs use 0x04 instead. */
	xmf_type = hio_read8(f);
	if (xmf_type != 0x03 && xmf_type != 0x04)
		return -1;

	if (hio_read(buf, 1, XMF_SAMPLE_ARRAY_SIZE, f) < XMF_SAMPLE_ARRAY_SIZE)
		return -1;

	/* Test instruments */
	pos = buf;
	num_ins = 0;
	for (i = 0; i < 256; i++) {
		uint32 loopstart = readmem24l(pos + 0);
		uint32 loopend   = readmem24l(pos + 3);
		uint32 datastart = readmem24l(pos + 6);
		uint32 dataend   = readmem24l(pos + 9);
		uint8  flags     = pos[13];
		uint16 srate     = readmem16l(pos + 14);

		uint32 len = dataend - datastart;
		pos += 16;

		if (flags & ~(0x04 | 0x08 | 0x10)) {
			D_(D_WARN "not XMF: smp %d: unknown flags", i);
			return -1;
		}
		/* if ping-pong loop flag is enabled, normal loop flag should be enabled too */
		if ((flags & (0x08 | 0x10)) == 0x10) {
			D_(D_WARN "not XMF: smp %d: inconsistent loop flags", i);
			return -1;
		}
		/* if loop flag is enabled, the loop should have a valid end point */
		if ((flags & 0x08) && !loopend) {
			D_(D_WARN "not XMF: smp %d: inconsistent loop data", i);
			return -1;
		}
		/* a 16-bit sample should have an even number of bytes */
		if ((flags & 0x04) && (len & 1)) {
			D_(D_WARN "not XMF: smp %d: inconsistent 16-bit sample length", i);
			return -1;
		}
		/* if this slot contains a valid sample, it should have a somewhat realistic middle-c frequency */
		if (len && srate < 100) {
			D_(D_WARN "not XMF: smp %d: low sample rate", i);
			return -1;
		}

		/* Despite the data start and end values, samples are stored
		 * sequentially after the pattern data. These fields are still
		 * required to calculate the sample length. */
		if (datastart > dataend) {
			D_(D_WARN "not XMF: smp %d: data start %u > end %u",
			 i, (unsigned)datastart, (unsigned)dataend);
			return -1;
		}

		samples_length += len;

		/* All known XMFs have well-formed loops. */
		if (loopend != 0 && (loopstart >= len || loopend > len || loopstart > loopend)) {
			D_(D_WARN "not XMF: smp %d: bad loop %u %u (len: %u)",
			 i, (unsigned)loopstart, (unsigned)loopend, (unsigned)len);
			return -1;
		}

		if (len > 0)
			num_ins = i + 1;
	}
	if (num_ins > MAX_INSTRUMENTS)
		return -1;

	/* Get pattern data size. */
	if (hio_read(buf, 1, 258, f) < 258)
		return -1;

	num_channels = buf[256] + 1;
	num_patterns = buf[257] + 1;

	if (num_channels > XMP_MAX_CHANNELS)
		return -1;

	/* Test total module length */
	samples_start = 0x1103 + num_channels + num_patterns * num_channels * 64 * 6;
	length = hio_size(f);
	if (length < samples_start || (size_t)length - samples_start < samples_length) {
		D_(D_WARN "not XMF: file length %ld is shorter than required %lu",
		 length, (unsigned long)samples_start + samples_length);
		return -1;
	}

	libxmp_read_title(f, t, 0);

	return 0;
}


/* TODO: command pages would be nice, but no official modules rely on 5xy/6xy. */
static void xmf_insert_effect(struct xmp_event *event, uint8 fxt, uint8 fxp, int chn)
{
	if (chn == 0) {
		event->fxt = fxt;
		event->fxp = fxp;
	} else {
		event->f2t = fxt;
		event->f2p = fxp;
	}
}

static void xmf_translate_effect(struct xmp_event *event, uint8 effect, uint8 param, int chn)
{
	/* Most effects are Protracker compatible. Only the effects actually
	 * implemented by Imperium Galactica are handled here. */

	switch (effect) {
	case 0x00:			/* none/arpeggio */
	case 0x01:			/* portamento up */
	case 0x02:			/* portamento down */
	case 0x0f:			/* set speed + set BPM */
		if (param) {
			xmf_insert_effect(event, effect, param, chn);
		}
		break;

	case 0x03:			/* tone portamento */
	case 0x04:			/* vibrato */
	case 0x0c:			/* set volume */
	case 0x0d:			/* break */
		xmf_insert_effect(event, effect, param, chn);
		break;

	case 0x05:			/* volume slide + tone portamento */
	case 0x06:			/* volume slide + vibrato */
		if (effect == 0x05) {
			xmf_insert_effect(event, FX_TONEPORTA, 0, chn ^ 1);
		}
		if (effect == 0x06) {
			xmf_insert_effect(event, FX_VIBRATO, 0, chn ^ 1);
		}

		/* fall-through */

	case 0x0a:			/* volume slide */
		if (param & 0x0f) {
			/* down takes precedence and uses the full param. */
			xmf_insert_effect(event, FX_VOLSLIDE_DN, param << 2, chn);
		} else if (param & 0xf0) {
			xmf_insert_effect(event, FX_VOLSLIDE_UP, param >> 2, chn);
		}
		break;

	case 0x0b:			/* pattern jump (jumps to xx + 1) */
		if (param < 255) {
			xmf_insert_effect(event, FX_JUMP, param + 1, chn);
		}
		break;

	case 0x0e:			/* extended */
		switch (param >> 4) {
		case 0x01:		/* fine slide up */
		case 0x02:		/* fine slide down */
		case 0x09:		/* note retrigger (TODO: only once) */
		case 0x0c:		/* note cut */
		case 0x0d:		/* note delay */
		case 0x0e:		/* pattern delay */
			if (param & 0x0f) {
				xmf_insert_effect(event, effect, param, chn);
			}
			break;

		case 0x04:		/* vibrato waveform */
			param &= 3;
			param = param < 3 ? param : 2;
			xmf_insert_effect(event, effect, param, chn);
			break;

		case 0x06:		/* pattern loop */
			xmf_insert_effect(event, effect, param, chn);
			break;

		case 0x0a:		/* fine volume slide up */
			if (param & 0x0f) {
				xmf_insert_effect(event, FX_F_VSLIDE_UP, (param & 0x0f) << 2, chn);
			}
			break;

		case 0x0b:		/* fine volume slide down */
			if (param & 0x0f) {
				xmf_insert_effect(event, FX_F_VSLIDE_DN, (param & 0x0f) << 2, chn);
			}
			break;
		}
		break;

	case 0x10:			/* panning (4-bit, GUS driver only) */
		param &= 0x0f;
		param |= (param << 4);
		xmf_insert_effect(event, FX_SETPAN, param, chn);
		break;

	case 0x11:			/* Ultra Tracker retrigger */
		/* TODO: should support the full param range, needs testing. */
		xmf_insert_effect(event, FX_EXTENDED, (EX_RETRIG << 4) | (param & 0x0f), chn);
		break;
	}
}

static int xmf_load(struct module_data *m, HIO_HANDLE *f, const int start)
{
	struct xmp_module *mod = &m->mod;
	struct xmp_event *event;
	uint8 *buf, *pos;
	size_t pat_sz;
	int xmf_type;
	int i, j, k;

	LOAD_INIT();

	/* Imperium Galactica uses 0x03, other Astroidea tracks use 0x04 */
	xmf_type = hio_read8(f);
	if(xmf_type == 0x03)
		snprintf(mod->type, XMP_NAME_SIZE, "Imperium Galactica XMF");
	else
		snprintf(mod->type, XMP_NAME_SIZE, "Astroidea XMF");

	MODULE_INFO();

	if ((buf = (uint8 *)malloc(XMF_SAMPLE_ARRAY_SIZE)) == NULL)
		return -1;

	/* Count instruments */
	if (hio_read(buf, 1, XMF_SAMPLE_ARRAY_SIZE, f) < XMF_SAMPLE_ARRAY_SIZE)
		goto err;

	mod->ins = 0;
	pos = buf;
	for (i = 0; i < 256; i++, pos += 16) {
		if (readmem24l(pos + 9) > readmem24l(pos + 6))
			mod->ins = i;
	}
	mod->ins++;
	mod->smp = mod->ins;

	if (libxmp_init_instrument(m) < 0)
		goto err;

	/* Instruments */
	pos = buf;
	for (i = 0; i < mod->ins; i++, pos += 16) {
		struct extra_sample_data *xtra = &(m->xtra[i]);
		struct xmp_instrument *xxi = &(mod->xxi[i]);
		struct xmp_sample *xxs = &(mod->xxs[i]);
		struct xmp_subinstrument *sub;

		if (libxmp_alloc_subinstrument(mod, i, 1) < 0)
			goto err;

		sub = &(xxi->sub[0]);

		xxs->len = readmem24l(pos + 9) - readmem24l(pos + 6);
		xxs->lps = readmem24l(pos + 0);
		xxs->lpe = readmem24l(pos + 3);
		xtra->c5spd = readmem16l(pos + 14);
		sub->vol = pos[12];
		sub->sid = i;

		/* The Sound Blaster driver will only loop if both the
		 * loop start and loop end are non-zero. The Sound Blaster
		 * driver does not support 16-bit samples or bidirectional
		 * looping, and plays these as regular 8-bit looped samples.
		 *
		 * GUS: 16-bit samples are loaded as 8-bit but play as 16-bit.
		 * If the first sample is 16-bit it will partly work (due to
		 * having a GUS RAM address of 0?). Other 16-bit samples will
		 * read from silence, garbage, or other samples.
		 */
		if (pos[13] & 0x04) { /* GUS 16-bit flag */
			xxs->flg |= XMP_SAMPLE_16BIT;
			xxs->len >>= 1;
		}
		if (pos[13] & 0x08)   /* GUS loop enable */
			xxs->flg |= XMP_SAMPLE_LOOP;
		if (pos[13] & 0x10)   /* GUS reverse flag */
			xxs->flg |= XMP_SAMPLE_LOOP_BIDIR;

		if (xxs->len > 0)
			xxi->nsm = 1;

		D_(D_INFO "[%2X] %06x %06x %06x %c%c V%02x %5d", i,
		   xxs->len, xxs->lps, xxs->lpe,
		   xxs->flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
		   xxs->flg & XMP_SAMPLE_LOOP_BIDIR ? 'B' : ' ',
		   sub->vol, (int)xtra->c5spd);
	}

	/* Sequence */
	if (hio_read(mod->xxo, 1, 256, f) < 256)
		return -1;

	mod->chn = hio_read8(f) + 1;
	mod->pat = hio_read8(f) + 1;
	mod->trk = mod->chn * mod->pat;

	for (i = 0; i < 256; i++) {
		if (mod->xxo[i] == 0xff)
			break;
	}
	mod->len = i;

	/* Panning table (supported by the Gravis UltraSound driver only) */
	if (hio_read(buf, 1, mod->chn, f) < mod->chn)
		goto err;

	for (i = 0; i < mod->chn; i++) {
		mod->xxc[i].pan = 0x80 + (buf[i] - 7) * 16;
		if (mod->xxc[i].pan > 255)
			mod->xxc[i].pan = 255;
	}

	D_(D_INFO "Module length: %d", mod->len);

	pat_sz = mod->chn * 6 * 64;
	if (pat_sz > XMF_SAMPLE_ARRAY_SIZE) {
		if ((pos = (uint8 *)realloc(buf, pat_sz)) == NULL)
			goto err;
		buf = pos;
	}

	if (libxmp_init_pattern(mod) < 0)
		goto err;

	/* Patterns */
	D_(D_INFO "Stored patterns: %d", mod->pat);

	for (i = 0; i < mod->pat; i++) {
		if (libxmp_alloc_pattern_tracks(mod, i, 64) < 0)
			goto err;

		if (hio_read(buf, 1, pat_sz, f) < pat_sz)
			goto err;

		pos = buf;
		for (j = 0; j < 64; j++) {
			for (k = 0; k < mod->chn; k++) {
				event = &EVENT(i, k, j);

				if (pos[0] > 0)
					event->note = pos[0] + 36;
				event->ins = pos[1];

				xmf_translate_effect(event, pos[2], pos[5], 0);
				xmf_translate_effect(event, pos[3], pos[4], 1);
				pos += 6;
			}
		}
	}
	free(buf);

	/* Sample data */
	D_(D_INFO "Stored samples: %d", mod->smp);

	/* Despite the GUS sample start and end pointers saved in the file,
	 * these are actually just loaded sequentially. */
	for (i = 0; i < mod->ins; i++) {
		if (libxmp_load_sample(m, f, 0, &mod->xxs[i], NULL))
			return -1;
	}

	/* With the Sound Blaster driver, full volume samples have a -0dB mix.
	 * Doing this in libxmp (x4 mvolbase) clips a little bit, so use a
	 * slightly lower level (x3 mvolbase, ~192 in IT terms).
	 *
	 * This only applies to the Imperium Galactica tracks; the tracks with
	 * 0x04 use the full GUS volume range.
	 */
	m->volbase = 0xff;
	m->mvolbase = 48;
	m->mvol = (xmf_type == 0x03) ? m->mvolbase * 3 : m->mvolbase;
	return 0;

  err:
	free(buf);
	return -1;
}
