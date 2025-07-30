/* Extended Module Player
 * Copyright (C) 1996-2021 Claudio Matsuoka and Hipolito Carraro Jr
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

#define PTM_CH_MASK	0x1f
#define PTM_NI_FOLLOW	0x20
#define PTM_VOL_FOLLOWS	0x80
#define PTM_FX_FOLLOWS	0x40

struct ptm_file_header {
	uint8 name[28];		/* Song name */
	uint8 doseof;		/* 0x1a */
	uint8 vermin;		/* Minor version */
	uint8 vermaj;		/* Major type */
	uint8 rsvd1;		/* Reserved */
	uint16 ordnum;		/* Number of orders (must be even) */
	uint16 insnum;		/* Number of instruments */
	uint16 patnum;		/* Number of patterns */
	uint16 chnnum;		/* Number of channels */
	uint16 flags;		/* Flags (set to 0) */
	uint16 rsvd2;		/* Reserved */
	uint32 magic;		/* 'PTMF' */
	uint8 rsvd3[16];	/* Reserved */
	uint8 chset[32];	/* Channel settings */
	uint8 order[256];	/* Orders */
	uint16 patseg[128];
};

struct ptm_instrument_header {
	uint8 type;		/* Sample type */
	uint8 dosname[12];	/* DOS file name */
	uint8 vol;		/* Volume */
	uint16 c4spd;		/* C4 speed */
	uint16 smpseg;		/* Sample segment (not used) */
	uint32 smpofs;		/* Sample offset */
	uint32 length;		/* Length */
	uint32 loopbeg;		/* Loop begin */
	uint32 loopend;		/* Loop end */
	uint32 gusbeg;		/* GUS begin address */
	uint32 guslps;		/* GUS loop start address */
	uint32 guslpe;		/* GUS loop end address */
	uint8 gusflg;		/* GUS loop flags */
	uint8 rsvd1;		/* Reserved */
	uint8 name[28];		/* Instrument name */
	uint32 magic;		/* 'PTMS' */
};

#define MAGIC_PTMF	MAGIC4('P','T','M','F')

static int ptm_test(HIO_HANDLE *, char *, const int);
static int ptm_load(struct module_data *, HIO_HANDLE *, const int);

const struct format_loader libxmp_loader_ptm = {
	"Poly Tracker",
	ptm_test,
	ptm_load
};

static int ptm_test(HIO_HANDLE *f, char *t, const int start)
{
	hio_seek(f, start + 44, SEEK_SET);
	if (hio_read32b(f) != MAGIC_PTMF)
		return -1;

	hio_seek(f, start + 0, SEEK_SET);
	libxmp_read_title(f, t, 28);

	return 0;
}

static const int ptm_vol[] = {
	0, 5, 8, 10, 12, 14, 15, 17, 18, 20, 21, 22, 23, 25, 26,
	27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 37, 38, 39, 40,
	41, 42, 42, 43, 44, 45, 46, 46, 47, 48, 49, 49, 50, 51, 51,
	52, 53, 54, 54, 55, 56, 56, 57, 58, 58, 59, 59, 60, 61, 61,
	62, 63, 63, 64, 64
};

static int ptm_load(struct module_data *m, HIO_HANDLE *f, const int start)
{
	struct xmp_module *mod = &m->mod;
	int c, r, i, smp_ofs[256];
	struct xmp_event *event;
	struct ptm_file_header pfh;
	struct ptm_instrument_header pih;
	uint8 n, b;

	LOAD_INIT();

	/* Load and convert header */

	hio_read(pfh.name, 28, 1, f);	/* Song name */
	pfh.doseof = hio_read8(f);	/* 0x1a */
	pfh.vermin = hio_read8(f);	/* Minor version */
	pfh.vermaj = hio_read8(f);	/* Major type */
	pfh.rsvd1 = hio_read8(f);	/* Reserved */
	pfh.ordnum = hio_read16l(f);	/* Number of orders (must be even) */
	pfh.insnum = hio_read16l(f);	/* Number of instruments */
	pfh.patnum = hio_read16l(f);	/* Number of patterns */
	pfh.chnnum = hio_read16l(f);	/* Number of channels */
	pfh.flags = hio_read16l(f);	/* Flags (set to 0) */
	pfh.rsvd2 = hio_read16l(f);	/* Reserved */
	pfh.magic = hio_read32b(f);	/* 'PTMF' */

	if (pfh.magic != MAGIC_PTMF)
		return -1;

	/* Sanity check */
	if (pfh.ordnum > 256 || pfh.insnum > 255 || pfh.patnum > 128 ||
	    pfh.chnnum > 32) {
		return -1;
	}

	hio_read(pfh.rsvd3, 16, 1, f);	/* Reserved */
	hio_read(pfh.chset, 32, 1, f);	/* Channel settings */
	hio_read(pfh.order, 256, 1, f);	/* Orders */
	for (i = 0; i < 128; i++)
		pfh.patseg[i] = hio_read16l(f);

	if (hio_error(f))
		return -1;

	mod->len = pfh.ordnum;
	mod->ins = pfh.insnum;
	mod->pat = pfh.patnum;
	mod->chn = pfh.chnnum;
	mod->trk = mod->pat * mod->chn;
	mod->smp = mod->ins;
	mod->spd = 6;
	mod->bpm = 125;
	memcpy(mod->xxo, pfh.order, 256);

	m->c4rate = C4_NTSC_RATE;

	libxmp_copy_adjust(mod->name, pfh.name, 28);
	libxmp_set_type(m, "Poly Tracker PTM %d.%02x", pfh.vermaj, pfh.vermin);

	MODULE_INFO();

	if (libxmp_init_instrument(m) < 0) {
		return -1;
	}

	/* Read and convert instruments and samples */

	for (i = 0; i < mod->ins; i++) {
		struct xmp_instrument *xxi = &mod->xxi[i];
		struct xmp_sample *xxs = &mod->xxs[i];
		struct xmp_subinstrument *sub;

		pih.type = hio_read8(f);	/* Sample type */
		hio_read(pih.dosname, 12, 1, f);	/* DOS file name */
		pih.vol = hio_read8(f);		/* Volume */
		pih.c4spd = hio_read16l(f);	/* C4 speed */
		pih.smpseg = hio_read16l(f);	/* Sample segment (not used) */
		pih.smpofs = hio_read32l(f);	/* Sample offset */
		pih.length = hio_read32l(f);	/* Length */
		pih.loopbeg = hio_read32l(f);	/* Loop begin */
		pih.loopend = hio_read32l(f);	/* Loop end */
		pih.gusbeg = hio_read32l(f);	/* GUS begin address */
		pih.guslps = hio_read32l(f);	/* GUS loop start address */
		pih.guslpe = hio_read32l(f);	/* GUS loop end address */
		pih.gusflg = hio_read8(f);	/* GUS loop flags */
		pih.rsvd1 = hio_read8(f);	/* Reserved */
		hio_read(pih.name, 28, 1, f);	/* Instrument name */
		pih.magic = hio_read32b(f);	/* 'PTMS' */

		if (hio_error(f)) {
			return -1;
		}

		if ((pih.type & 3) != 1)
			continue;

		if (libxmp_alloc_subinstrument(mod, i, 1) < 0) {
			return -1;
		}

		sub = &xxi->sub[0];

		smp_ofs[i] = pih.smpofs;
		xxs->len = pih.length;
		xxs->lps = pih.loopbeg;
		xxs->lpe = pih.loopend;

		if (mod->xxs[i].len > 0) {
			mod->xxi[i].nsm = 1;
		}

		xxs->flg = 0;
		if (pih.type & 0x04) {
			xxs->flg |= XMP_SAMPLE_LOOP;
		}
		if (pih.type & 0x08) {
			xxs->flg |= XMP_SAMPLE_LOOP | XMP_SAMPLE_LOOP_BIDIR;
		}
		if (pih.type & 0x10) {
			xxs->flg |= XMP_SAMPLE_16BIT;
			xxs->len >>= 1;
			xxs->lps >>= 1;
			xxs->lpe >>= 1;
		}

		sub->vol = pih.vol;
		sub->pan = 0x80;
		sub->sid = i;
		pih.magic = 0;

		libxmp_instrument_name(mod, i, pih.name, 28);

		D_(D_INFO "[%2X] %-28.28s %05x%c%05x %05x %c V%02x %5d",
		   i, mod->xxi[i].name, mod->xxs[i].len,
		   pih.type & 0x10 ? '+' : ' ',
		   xxs->lps, xxs->lpe, xxs->flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
		   sub->vol, pih.c4spd);

		/* Convert C4SPD to relnote/finetune */
		libxmp_c2spd_to_note(pih.c4spd, &sub->xpo, &sub->fin);
	}

	if (libxmp_init_pattern(mod) < 0)
		return -1;

	/* Read patterns */
	D_(D_INFO "Stored patterns: %d", mod->pat);

	for (i = 0; i < mod->pat; i++) {

		/* channel control to prevent infinite loop in pattern reading */
		/* addresses fuzz bug reported by Lionel Debroux in 20161223 */
		char chn_ctrl[32];

		if (!pfh.patseg[i])
			continue;

		if (libxmp_alloc_pattern_tracks(mod, i, 64) < 0)
			return -1;

		hio_seek(f, start + 16L * pfh.patseg[i], SEEK_SET);
		r = 0;

		memset(chn_ctrl, 0, sizeof(chn_ctrl));

		while (r < 64) {

			b = hio_read8(f);
			if (!b) {
				r++;
				memset(chn_ctrl, 0, sizeof(chn_ctrl));
				continue;
			}

			c = b & PTM_CH_MASK;
			if (chn_ctrl[c]) {
				/* uh-oh, something wrong happened */
				return -1;
			}
			/* mark this channel as read */
			chn_ctrl[c] = 1;

			if (c >= mod->chn) {
				continue;
			}

			event = &EVENT(i, c, r);
			if (b & PTM_NI_FOLLOW) {
				n = hio_read8(f);
				switch (n) {
				case 255:
					n = 0;
					break;	/* Empty note */
				case 254:
					n = XMP_KEY_OFF;
					break;	/* Key off */
				default:
					n += 12;
				}
				event->note = n;
				event->ins = hio_read8(f);
			}
			if (b & PTM_FX_FOLLOWS) {
				event->fxt = hio_read8(f);
				event->fxp = hio_read8(f);

				if (event->fxt > 0x17)
					event->fxt = event->fxp = 0;

				switch (event->fxt) {
				case 0x0e:	/* Extended effect */
					if (MSN(event->fxp) == 0x8) {	/* Pan set */
						event->fxt = FX_SETPAN;
						event->fxp =
						    LSN(event->fxp) << 4;
					}
					break;
				case 0x10:	/* Set global volume */
					event->fxt = FX_GLOBALVOL;
					break;
				case 0x11:	/* Multi retrig */
					event->fxt = FX_MULTI_RETRIG;
					break;
				case 0x12:	/* Fine vibrato */
					event->fxt = FX_FINE_VIBRATO;
					break;
				case 0x13:	/* Note slide down */
					event->fxt = FX_NSLIDE_DN;
					break;
				case 0x14:	/* Note slide up */
					event->fxt = FX_NSLIDE_UP;
					break;
				case 0x15:	/* Note slide down + retrig */
					event->fxt = FX_NSLIDE_R_DN;
					break;
				case 0x16:	/* Note slide up + retrig */
					event->fxt = FX_NSLIDE_R_UP;
					break;
				case 0x17:	/* Reverse sample */
					event->fxt = event->fxp = 0;
					break;
				}
			}
			if (b & PTM_VOL_FOLLOWS) {
				event->vol = hio_read8(f) + 1;
			}
		}
	}

	D_(D_INFO "Stored samples: %d", mod->smp);

	for (i = 0; i < mod->smp; i++) {
		if (mod->xxi[i].nsm == 0)
			continue;

		if (mod->xxs[i].len == 0)
			continue;

		hio_seek(f, start + smp_ofs[i], SEEK_SET);
		if (libxmp_load_sample(m, f, SAMPLE_FLAG_8BDIFF, &mod->xxs[i], NULL) < 0)
			return -1;
	}

	m->vol_table = ptm_vol;

	for (i = 0; i < mod->chn; i++)
		mod->xxc[i].pan = pfh.chset[i] << 4;

	m->quirk |= QUIRKS_ST3;
	m->read_event_type = READ_EVENT_ST3;

	return 0;
}
