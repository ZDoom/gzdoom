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

#include "loader.h"
#include "../period.h"

/* Nir Oren's Liquid Tracker old "NO" format. I have only one NO module,
 * Moti Radomski's "Time after time" from ftp.modland.com.
 *
 * Another NO "Waste of Time" is bundled with The Liquid Tracker 0.80b, and
 * a converter called MOD2LIQ is bundled with The Liquid Player 1.00.
 */


static int no_test (HIO_HANDLE *, char *, const int);
static int no_load (struct module_data *, HIO_HANDLE *, const int);

const struct format_loader libxmp_loader_no = {
	"Liquid Tracker NO",
	no_test,
	no_load
};

static int no_test(HIO_HANDLE *f, char *t, const int start)
{
	uint8 buf[33];
	int nsize, pat, chn;
	int i;

	hio_seek(f, start, SEEK_CUR);

	if (hio_read32b(f) != 0x4e4f0000)		/* NO 0x00 0x00 */
		return -1;

	if (hio_read(buf, 1, 33, f) < 33)
		return -1;

	nsize = buf[0];
	if (nsize > 29)
		return -1;

	/* test title */
	for (i = 0; i < nsize; i++) {
		if (buf[i + 1] == '\0')
			return -1;
	}

	/* test number of patterns */
	pat = buf[30];
	if (pat == 0)
		return -1;

	/* test number of channels */
	chn = buf[32];
	if (chn <= 0 || chn > 16)
		return -1;

	hio_seek(f, start + 5, SEEK_SET);

	libxmp_read_title(f, t, nsize);

	return 0;
}


/* Note that 0.80b or slightly earlier started translating these in the
 * editor to the new Liquid Module effects. It's not clear if the saving
 * format was modified at this point, but 0.80b and 0.82b are not aware
 * of the Liquid Module format.
 *
 * TODO: the changelog alleges an Fxx (05h) global volume effect was added
 * in 0.67b, which would be present in releases 0.68b and 0.69b only.
 */
static const uint8 fx[15] = {
	FX_SPEED,
	FX_VIBRATO,
	FX_BREAK,
	FX_PORTA_DN,
	FX_PORTA_UP,
	0,
	FX_ARPEGGIO,
	FX_SETPAN,
	0, /* special */
	FX_JUMP,
	FX_TREMOLO,
	FX_VOLSLIDE,
	0, /* special */
	FX_TONEPORTA,
	FX_OFFSET
};

/* These are all FX_EXTENDED but in a custom order. */
static const uint8 fx_misc2[16] = {
	EX_F_PORTA_UP,
	EX_F_PORTA_DN,
	EX_F_VSLIDE_UP,
	EX_F_VSLIDE_DN,
	EX_VIBRATO_WF,
	EX_TREMOLO_WF,
	EX_RETRIG,
	EX_CUT,
	EX_DELAY,
	0,
	0,
	EX_PATTERN_LOOP,
	EX_PATT_DELAY,
	0,
	0,
	0
};

static void no_translate_effect(struct xmp_event *event, int fxt, int fxp)
{
	int value;
	switch (fxt) {
	case 0x0:				/* Axx Set Speed/BPM */
	case 0x1:				/* Bxy Vibrato */
	case 0x2:				/* Cxx Cut (break) */
	case 0x3:				/* Dxx Porta Down */
	case 0x4:				/* Exx Porta Up */
	case 0x6:				/* Gxx Arpeggio */
	case 0x9:				/* Jxx Jump Position */
	case 0xa:				/* Kxy Tremolo */
	case 0xb:				/* Lxy Volslide (fine 0.80b+ only) */
	case 0xd:				/* Nxx Note Portamento */
	case 0xe:				/* Oxx Sample Offset */
		event->fxt = fx[fxt];
		event->fxp = fxp;
		break;

	case 0x7:				/* Hxx Pan Control */
		/* Value is decimal, effective values >64 are ignored */
		value = MSN(fxp) * 10 + LSN(fxp);
		if (value == 70) {
			/* TODO: reset panning H70 (H6A also works)
			 * this resets ALL channels to default pan. */
		} else if (value <= 64) {
			event->fxt = FX_SETPAN;
			event->fxp = value * 0xff / 64;
		}
		break;

	case 0x8:				/* Ixy Misc 1 */
		switch (MSN(fxp)) {
		case 0x0:			/* I0y Vibrato + volslide up */
			event->fxt = FX_VIBRA_VSLIDE;
			event->fxp = LSN(fxp) << 4;
			break;
		case 0x1:			/* I1y Vibrato + volslide down */
			event->fxt = FX_VIBRA_VSLIDE;
			event->fxp = LSN(fxp);
			break;
		case 0x2:			/* I2y Noteporta + volslide up */
			event->fxt = FX_TONE_VSLIDE;
			event->fxp = LSN(fxp) << 4;
			break;
		case 0x3:			/* I3y Noteporta + volslide down */
			event->fxt = FX_TONE_VSLIDE;
			event->fxp = LSN(fxp);
			break;
		/* TODO: if these were ever implemented they were after 0.64b
		 * and before 0.80b only, i.e. versions not available to test. */
		case 0x4:			/* I4y Tremolo + volslide up */
			event->fxt = FX_TREMOLO;
			event->fxp = 0;
			event->f2t = FX_VOLSLIDE;
			event->f2p = LSN(fxp) << 4;
			break;
		case 0x5:			/* I5y Tremolo + volslide down */
			event->fxt = FX_TREMOLO;
			event->fxp = 0;
			event->f2t = FX_VOLSLIDE;
			event->f2p = LSN(fxp);
			break;
		}
		break;

	case 0xc:				/* Mxy Misc 2 */
		value = MSN(fxp);
		fxp = LSN(fxp);
		switch (value) {
		case 0x4:			/* M4x Vibrato Waveform */
		case 0x5:			/* M5x Tremolo Waveform */
			if ((fxp & 3) == 3) {
				fxp--;
			}
			/* fall-through */

		case 0x0:			/* M0y Fine Porta Up */
		case 0x1:			/* M1y Fine Porta Down */
		case 0x2:			/* M2y Fine Volslide Up */
		case 0x3:			/* M3y Fine Volslide Down */
		case 0x6:			/* M6y Note Retrigger */
		case 0x7:			/* M7y Note Cut */
		case 0x8:			/* M8y Note Delay */
		case 0xb:			/* MBy Pattern Loop */
		case 0xc:			/* MCy Pattern Delay */
			event->fxt = FX_EXTENDED;
			event->fxp = (fx_misc2[value] << 4) | fxp;
			break;
		}
		break;
	}
}

static int no_load(struct module_data *m, HIO_HANDLE *f, const int start)
{
	struct xmp_module *mod = &m->mod;
	struct xmp_event *event;
	uint8 buf[46];
	int i, j, k;
	int nsize;

	LOAD_INIT();

	hio_read32b(f);			/* NO 0x00 0x00 */

	libxmp_set_type(m, "Liquid Tracker");

	nsize = hio_read8(f);
	if (hio_read(mod->name, 1, 29, f) < 29)
		return -1;
	mod->name[nsize] = '\0';

	mod->pat = hio_read8(f);
	hio_read8(f);
	mod->chn = hio_read8(f);
	mod->trk = mod->pat * mod->chn;
	hio_read8(f);
	hio_read16l(f);
	hio_read16l(f);
	hio_read8(f);
	mod->ins = mod->smp = 63;

	for (i = 0; i < 256; i++) {
		uint8 x = hio_read8(f);
		if (x == 0xff)
			break;
		mod->xxo[i] = x;
	}
	hio_seek(f, 255 - i, SEEK_CUR);
	mod->len = i;

	m->c4rate = C4_NTSC_RATE;

	MODULE_INFO();

	if (libxmp_init_instrument(m) < 0)
		return -1;

	/* Read instrument names */
	for (i = 0; i < mod->ins; i++) {
		int c2spd;

		if (libxmp_alloc_subinstrument(mod, i, 1) < 0)
			return -1;

		if (hio_read(buf, 1, 46, f) < 46)
			return -1;

		nsize = MIN(buf[0], 30);
		libxmp_instrument_name(mod, i, buf + 1, nsize);

		mod->xxi[i].sub[0].vol = buf[31];
		c2spd = readmem16l(buf + 32);
		mod->xxs[i].len = readmem32l(buf + 34);
		mod->xxs[i].lps = readmem32l(buf + 38);
		mod->xxs[i].lpe = readmem32l(buf + 42);

		if (mod->xxs[i].len > 0)
			mod->xxi[i].nsm = 1;

		mod->xxs[i].flg = mod->xxs[i].lpe > 0 ? XMP_SAMPLE_LOOP : 0;
		mod->xxi[i].sub[0].fin = 0;
		mod->xxi[i].sub[0].pan = 0x80;
		mod->xxi[i].sub[0].sid = i;

		D_(D_INFO "[%2X] %-22.22s  %04x %04x %04x %c V%02x %5d",
				i, mod->xxi[i].name,
				mod->xxs[i].len, mod->xxs[i].lps, mod->xxs[i].lpe,
				mod->xxs[i].flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
				mod->xxi[i].sub[0].vol, c2spd);

		libxmp_c2spd_to_note(c2spd, &mod->xxi[i].sub[0].xpo, &mod->xxi[i].sub[0].fin);
	}

	if (libxmp_init_pattern(mod) < 0)
		return -1;

	/* Read and convert patterns */
	D_(D_INFO "Stored patterns: %d ", mod->pat);

	for (i = 0; i < mod->pat; i++) {
		if (libxmp_alloc_pattern_tracks(mod, i, 64) < 0)
			return -1;

		for (j = 0; j < mod->xxp[i]->rows; j++) {
			for (k = 0; k < mod->chn; k++) {
				uint32 x, note, ins, vol, fxt, fxp;

				event = &EVENT (i, k, j);

				x = hio_read32l(f);
				note = x & 0x0000003f;
				ins = (x & 0x00001fc0) >> 6;
				vol = (x & 0x000fe000) >> 13;
				fxt = (x & 0x00f00000) >> 20;
				fxp = (x & 0xff000000) >> 24;

				if (note != 0x3f)
					event->note = 37 + note;
				if (ins != 0x7f)
					event->ins = 1 + ins;
				if (vol != 0x7f)
					event->vol = 1 + vol;
				if (fxt != 0x0f) {
					no_translate_effect(event, fxt, fxp);
				}
			}
		}
	}

	/* Read samples */
	D_(D_INFO "Stored samples: %d", mod->smp);

	for (i = 0; i < mod->ins; i++) {
		if (mod->xxs[i].len == 0)
			continue;
		if (libxmp_load_sample(m, f, SAMPLE_FLAG_UNS, &mod->xxs[i], NULL) < 0)
			return -1;
	}

	m->quirk |= QUIRK_FINEFX | QUIRK_RTONCE;
	m->flow_mode = FLOW_MODE_LIQUID;
	m->read_event_type = READ_EVENT_ST3;

	for (i = 0; i < mod->chn; i++) {
		mod->xxc[i].pan = DEFPAN((i & 1) ? 0xff : 0x00);
	}

	return 0;
}
