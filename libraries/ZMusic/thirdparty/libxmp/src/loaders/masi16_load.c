/* Extended Module Player
 * Copyright (C) 1996-2023 Claudio Matsuoka and Hipolito Carraro Jr
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

#define MAGIC_PSM_	MAGIC4('P','S','M',0xfe)


static int masi16_test (HIO_HANDLE *, char *, const int);
static int masi16_load (struct module_data *, HIO_HANDLE *, const int);

const struct format_loader libxmp_loader_masi16 = {
	"Epic MegaGames MASI 16",
	masi16_test,
	masi16_load
};

static int masi16_test(HIO_HANDLE *f, char *t, const int start)
{
	if (hio_read32b(f) != MAGIC_PSM_)
		return -1;

	libxmp_read_title(f, t, 60);

	return 0;
}


static void masi16_translate_effect(struct xmp_event *event, uint8 effect,
				    uint8 param, uint8 param2, uint8 param3)
{
	switch (effect) {
	case 1:					/* Fine Volume Slide Up */
		event->fxt = FX_F_VSLIDE_UP;
		event->fxp = param;
		break;
	case 2:					/* Volume Slide Up */
		event->fxt = FX_VOLSLIDE_UP;
		event->fxp = param;
		break;
	case 3:					/* Fine Volume Slide Down */
		event->fxt = FX_F_VSLIDE_DN;
		event->fxp = param;
		break;
	case 4:					/* Volume Slide Down */
		event->fxt = FX_VOLSLIDE_DN;
		event->fxp = param;
		break;

	case 10:				/* Fine Porta Up */
		event->fxt = FX_F_PORTA_UP;
		event->fxp = param;
		break;
	case 11:				/* Portamento Up */
		event->fxt = FX_PORTA_UP;
		event->fxp = param;
		break;
	case 12:				/* Fine Porta Down */
		event->fxt = FX_F_PORTA_DN;
		event->fxp = param;
		break;
	case 13:				/* Portamento Down */
		event->fxt = FX_PORTA_DN;
		event->fxp = param;
		break;
	case 14:				/* Tone Portamento */
		event->fxt = FX_TONEPORTA;
		event->fxp = param;
		break;
	case 15:				/* Glissando control */
		event->fxt = FX_EXTENDED;
		event->fxp = (EX_GLISS << 4) | (param & 0x0f);
		break;
	case 16:				/* Tone Portamento + Volslide Up */
		event->fxt = FX_TONEPORTA;
		event->fxp = 0;
		event->f2t = FX_VOLSLIDE_UP;
		event->f2p = param;
		break;
	case 17:				/* Tone Portamento + Volslide Down */
		event->fxt = FX_TONEPORTA;
		event->fxp = 0;
		event->f2t = FX_VOLSLIDE_DN;
		event->f2p = param;
		break;

	case 20:				/* Vibrato */
		event->fxt = FX_VIBRATO;
		event->fxp = param;
		break;
	case 21:				/* Vibrato waveform */
		event->fxt = FX_EXTENDED;
		event->fxp = (EX_VIBRATO_WF << 4) | (param & 0x0f);
		break;
	case 22:				/* Vibrato + Volume Slide Up */
		event->fxt = FX_VIBRATO;
		event->fxp = 0;
		event->f2t = FX_VOLSLIDE_UP;
		event->f2p = param;
		break;
	case 23:				/* Vibrato + Volume Slide Down */
		event->fxt = FX_VIBRATO;
		event->fxp = 0;
		event->f2t = FX_VOLSLIDE_DN;
		event->f2p = param;
		break;

	case 30:				/* Tremolo */
		event->fxt = FX_TREMOLO;
		event->fxp = param;
		break;
	case 31:				/* Tremolo waveform */
		event->fxt = FX_EXTENDED;
		event->fxp = (EX_TREMOLO_WF << 4) | (param & 0x0f);
		break;

	case 40:				/* Sample Offset */
		/* TODO: param and param3 are the fine and high offsets. */
		event->fxt = FX_OFFSET;
		event->fxp = param2;
		break;
	case 41:				/* Retrigger Note */
		event->fxt = FX_EXTENDED;
		event->fxp = (EX_RETRIG << 4) | (param & 0x0f);
		break;
	case 42:				/* Note Cut */
		event->fxt = FX_EXTENDED;
		event->fxp = (EX_CUT << 4) | (param & 0x0f);
		break;
	case 43:				/* Note Delay */
		event->fxt = FX_EXTENDED;
		event->fxp = (EX_DELAY << 4) | (param & 0x0f);
		break;

	case 50:				/* Position Jump */
		event->fxt = FX_JUMP;
		event->fxp = param;
		break;
	case 51:				/* Pattern Break */
		event->fxt = FX_BREAK;
		event->fxp = param;
		break;
	case 52:				/* Jump Loop */
		event->fxt = FX_EXTENDED;
		event->fxp = (EX_PATTERN_LOOP << 4) | (param & 0x0f);
		break;
	case 53:				/* Pattern Delay */
		event->fxt = FX_PATT_DELAY;
		event->fxp = param;
		break;

	case 60:				/* Set Speed */
		event->fxt = FX_S3M_SPEED;
		event->fxp = param;
		break;
	case 61:				/* Set BPM */
		event->fxt = FX_S3M_BPM;
		event->fxp = param;
		break;

	case 70:				/* Arpeggio */
		event->fxt = FX_ARPEGGIO;
		event->fxp = param;
		break;
	case 71:				/* Set Finetune */
		event->fxt = FX_FINETUNE;
		event->fxp = param;
		break;
	case 72:				/* Set Balance */
		event->fxt = FX_SETPAN;
		event->fxp = (param & 0x0f) | ((param & 0x0f) << 4);
		break;

	default:
		event->fxt = event->fxp = 0;
		break;
	}
}

static int masi16_load(struct module_data *m, HIO_HANDLE *f, const int start)
{
	struct xmp_module *mod = &m->mod;
	int c, r, i;
	struct xmp_event *event;
	uint8 buf[1024];
	uint32 p_ord, p_chn, p_pat, p_ins;
	uint32 p_smp[256];
	uint8 sample_map[256];
	int type, ver /*, mode*/;
	int stored_ins;

	LOAD_INIT();

	hio_read32b(f);

	hio_read(buf, 1, 60, f);
	memcpy(mod->name, (char *)buf, 59);
	mod->name[59] = '\0';

	type = hio_read8(f);		/* song type */
	ver = hio_read8(f);		/* song version */
	/*mode =*/ hio_read8(f);	/* pattern version */

	if (type & 0x01)		/* song mode not supported */
		return -1;

	libxmp_set_type(m, "Epic MegaGames MASI 16 PSM %d.%02d", MSN(ver), LSN(ver));

	mod->spd = hio_read8(f);
	mod->bpm = hio_read8(f);
	hio_read8(f);			/* master volume */
	hio_read16l(f);			/* song length */
	mod->len = hio_read16l(f);
	mod->pat = hio_read16l(f);
	stored_ins = hio_read16l(f);
	hio_read16l(f);			/* ignore channels to play */
	mod->chn = hio_read16l(f);	/* use channels to proceed */

	/* Sanity check */
	if (mod->len > 256 || mod->pat > 256 || stored_ins > 255 ||
	    mod->chn > XMP_MAX_CHANNELS) {
		return -1;
	}
	mod->trk = mod->pat * mod->chn;

	p_ord = hio_read32l(f);
	p_chn = hio_read32l(f);
	p_pat = hio_read32l(f);
	p_ins = hio_read32l(f);

	/* should be this way but fails with Silverball song 6 */
	//mod->flg |= ~type & 0x02 ? XXM_FLG_MODRNG : 0;

	m->c4rate = C4_NTSC_RATE;

	MODULE_INFO();

	hio_seek(f, start + p_ord, SEEK_SET);
	hio_read(mod->xxo, 1, mod->len, f);

	memset(buf, 0, mod->chn);
	hio_seek(f, start + p_chn, SEEK_SET);
	hio_read(buf, 1, 16, f);

	for (i = 0; i < mod->chn; i++) {
		if (buf[i] < 16) {
			mod->xxc[i].pan = buf[i] | (buf[i] << 4);
		}
	}

	/* Get the actual instruments count... */
	mod->ins = 0;
	for (i = 0; i < stored_ins; i++) {
		hio_seek(f, start + p_ins + 64 * i + 45, SEEK_SET);
		sample_map[i] = hio_read16l(f) - 1;
		mod->ins = MAX(mod->ins, sample_map[i] + 1);
	}
	if (mod->ins > 255 || hio_error(f))
		return -1;

	mod->smp = mod->ins;

	if (libxmp_init_instrument(m) < 0)
		return -1;

	memset(p_smp, 0, sizeof(p_smp));

	hio_seek(f, start + p_ins, SEEK_SET);
	for (i = 0; i < stored_ins; i++) {
		struct xmp_instrument *xxi;
		struct xmp_sample *xxs;
		struct xmp_subinstrument *sub;
		uint16 flags, c2spd;
		int finetune;
		int num = sample_map[i];

		if (hio_read(buf, 1, 64, f) < 64)
			return -1;

		xxi = &mod->xxi[num];
		xxs = &mod->xxs[num];

		/* Don't load duplicate instruments */
		if (xxi->sub)
			continue;

		if (libxmp_alloc_subinstrument(mod, num, 1) < 0)
			return -1;

		sub = &xxi->sub[0];

		/*hio_read(buf, 1, 13, f);*/	/* sample filename */
		/*hio_read(buf, 1, 24, f);*/	/* sample description */
		memcpy(xxi->name, buf + 13, 24);
		xxi->name[24] = '\0';
		p_smp[i] = readmem32l(buf + 37);
		/*hio_read32l(f);*/		/* memory location */
		/*hio_read16l(f);*/		/* sample number */
		flags = buf[47];		/* sample type */
		xxs->len = readmem32l(buf + 48);
		xxs->lps = readmem32l(buf + 52);
		xxs->lpe = readmem32l(buf + 56);
		finetune = buf[60];
		sub->vol = buf[61];
		c2spd = readmem16l(buf + 62);
		sub->pan = 0x80;
		sub->sid = num;
		xxs->flg = flags & 0x80 ? XMP_SAMPLE_LOOP : 0;
		xxs->flg |= flags & 0x20 ? XMP_SAMPLE_LOOP_BIDIR : 0;

		libxmp_c2spd_to_note(c2spd, &sub->xpo, &sub->fin);
		sub->fin += (int8)((finetune & 0x0f) << 4);
		sub->xpo += (finetune >> 4) - 7;

		/* The documentation claims samples shouldn't exceed 64k. The
		 * PS16 modules from Silverball and Epic Pinball confirm this.
		 * Later Protracker Studio Modules (MASI) allow up to 1MB.
		 */
		if ((uint32)xxs->len > 64 * 1024) {
			D_(D_CRIT "invalid sample %d length %d", num, xxs->len);
			return -1;
		}

		if (xxs->len > 0)
			xxi->nsm = 1;

		D_(D_INFO "[%2X] %-22.22s %04x %04x %04x %c V%02x %5d",
			num, xxi->name, xxs->len, xxs->lps,
			xxs->lpe, xxs->flg & XMP_SAMPLE_LOOP ?
			'L' : ' ', sub->vol, c2spd);
	}

	if (libxmp_init_pattern(mod) < 0)
		return -1;

	D_(D_INFO "Stored patterns: %d", mod->pat);

	hio_seek(f, start + p_pat, SEEK_SET);
	for (i = 0; i < mod->pat; i++) {
		int len;
		uint8 b, rows, chan;

		len = hio_read16l(f) - 4;
		rows = hio_read8(f);
		if (rows > 64) {
			return -1;
		}
		chan = hio_read8(f);
		if (chan > 32) {
			return -1;
		}

		if (libxmp_alloc_pattern_tracks(mod, i, rows) < 0)
			return -1;

		for (r = 0; r < rows; r++) {
			while (len > 0) {
				b = hio_read8(f);
				len--;

				if (b == 0)
					break;

				c = b & 0x0f;
				if (c >= mod->chn)
					return -1;
				event = &EVENT(i, c, r);

				if (b & 0x80) {
					event->note = hio_read8(f) + 36;
					event->ins = hio_read8(f);
					len -= 2;
				}

				if (b & 0x40) {
					event->vol = hio_read8(f) + 1;
					len--;
				}

				if (b & 0x20) {
					uint8 effect = hio_read8(f);
					uint8 param = hio_read8(f);
					uint8 param2 = 0;
					uint8 param3 = 0;

					if (effect == 40) { /* Sample Offset */
						param2 = hio_read8(f);
						param3 = hio_read8(f);
					}
					masi16_translate_effect(event, effect, param, param2, param3);
					len -= 2;
				}
			}
		}

		if (len > 0)
			hio_seek(f, len, SEEK_CUR);
	}

	/* Read samples */

	D_(D_INFO "Stored samples: %d", mod->smp);

	for (i = 0; i < stored_ins; i++) {
		struct xmp_sample *xxs = &mod->xxs[sample_map[i]];

		/* Don't load duplicate sample data */
		if (xxs->data)
			continue;

		hio_seek(f, start + p_smp[i], SEEK_SET);
		if (libxmp_load_sample(m, f, SAMPLE_FLAG_DIFF, xxs, NULL) < 0)
			return -1;
	}

	return 0;
}
