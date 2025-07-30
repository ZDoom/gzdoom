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

/*
 * Tue, 30 Jun 1998 20:23:11 +0200
 * Reported by John v/d Kamp <blade_@dds.nl>:
 * I have this song from Purple Motion called wcharts.s3m, the global
 * volume was set to 0, creating a devide by 0 error in xmp. There should
 * be an extra test if it's 0 or not.
 *
 * Claudio's fix: global volume ignored
 */

/*
 * Sat, 29 Aug 1998 18:50:43 -0500 (CDT)
 * Reported by Joel Jordan <scriber@usa.net>:
 * S3M files support tempos outside the ranges defined by xmp (that is,
 * the MOD/XM tempo ranges).  S3M's can have tempos from 0 to 255 and speeds
 * from 0 to 255 as well, since these are handled with separate effects
 * unlike the MOD format.  This becomes an issue in such songs as Skaven's
 * "Catch that Goblin", which uses speeds above 0x1f.
 *
 * Claudio's fix: FX_S3M_SPEED added. S3M supports speeds from 0 to 255 and
 * tempos from 32 to 255 (S3M speed == xmp tempo, S3M tempo == xmp BPM).
 */

/* Wed, 21 Oct 1998 15:03:44 -0500  Geoff Reedy <vader21@imsa.edu>
 * It appears that xmp has issues loading/playing a specific instrument
 * used in LUCCA.S3M.
 * (Fixed by Hipolito in xmp-2.0.0dev34)
 */

/*
 * From http://code.pui.ch/2007/02/18/turn-demoscene-modules-into-mp3s/
 * The only flaw I noticed [in xmp] is a problem with portamento in Purple
 * Motion's second reality soundtrack (1:06-1:17)
 *
 * Claudio's note: that's a dissonant beating between channels 6 and 7
 * starting at pos12, caused by pitchbending effect F25.
 */

#include "loader.h"
#include "s3m.h"
#include "../period.h"

#define MAGIC_SCRM	MAGIC4('S','C','R','M')
#define MAGIC_SCRI	MAGIC4('S','C','R','I')
#define MAGIC_SCRS	MAGIC4('S','C','R','S')

static int s3m_test(HIO_HANDLE *, char *, const int);
static int s3m_load(struct module_data *, HIO_HANDLE *, const int);

const struct format_loader libxmp_loader_s3m = {
	"Scream Tracker 3",
	s3m_test,
	s3m_load
};

static int s3m_test(HIO_HANDLE *f, char *t, const int start)
{
	hio_seek(f, start + 44, SEEK_SET);
	if (hio_read32b(f) != MAGIC_SCRM)
		return -1;

	hio_seek(f, start + 29, SEEK_SET);
	if (hio_read8(f) != 0x10)
		return -1;

	hio_seek(f, start + 0, SEEK_SET);
	libxmp_read_title(f, t, 28);

	return 0;
}

#define NONE		0xff
#define FX_S3M_EXTENDED	0xfe

/* Effect conversion table */
static const uint8 fx[27] = {
	NONE,
	FX_S3M_SPEED,		/* Axx  Set speed to xx (the default is 06) */
	FX_JUMP,		/* Bxx  Jump to order xx (hexadecimal) */
	FX_BREAK,		/* Cxx  Break pattern to row xx (decimal) */
	FX_VOLSLIDE,		/* Dxy  Volume slide down by y/up by x */
	FX_PORTA_DN,		/* Exx  Slide down by xx */
	FX_PORTA_UP,		/* Fxx  Slide up by xx */
	FX_TONEPORTA,		/* Gxx  Tone portamento with speed xx */
	FX_VIBRATO,		/* Hxy  Vibrato with speed x and depth y */
	FX_TREMOR,		/* Ixy  Tremor with ontime x and offtime y */
	FX_S3M_ARPEGGIO,	/* Jxy  Arpeggio with halfnote additions */
	FX_VIBRA_VSLIDE,	/* Kxy  Dual command: H00 and Dxy */
	FX_TONE_VSLIDE,		/* Lxy  Dual command: G00 and Dxy */
	NONE,
	NONE,
	FX_OFFSET,		/* Oxy  Set sample offset */
	NONE,
	FX_MULTI_RETRIG,	/* Qxy  Retrig (+volumeslide) note */
	FX_TREMOLO,		/* Rxy  Tremolo with speed x and depth y */
	FX_S3M_EXTENDED,	/* Sxx  (misc effects) */
	FX_S3M_BPM,		/* Txx  Tempo = xx (hex) */
	FX_FINE_VIBRATO,	/* Uxx  Fine vibrato */
	FX_GLOBALVOL,		/* Vxx  Set global volume */
	NONE,
	FX_SETPAN,		/* Xxx  Set pan */
	NONE,
	NONE
};

/* Effect translation */
static void xlat_fx(int c, struct xmp_event *e)
{
	uint8 h = MSN(e->fxp), l = LSN(e->fxp);

	if (e->fxt >= ARRAY_SIZE(fx)) {
		D_(D_WARN "invalid effect %02x", e->fxt);
		e->fxt = e->fxp = 0;
		return;
	}

	switch (e->fxt = fx[e->fxt]) {
	case FX_S3M_BPM:
		if (e->fxp < 0x20) {
			e->fxp = e->fxt = 0;
		}
		break;
	case FX_S3M_EXTENDED:	/* Extended effects */
		e->fxt = FX_EXTENDED;
		switch (h) {
		case 0x1:	/* Glissando */
			e->fxp = LSN(e->fxp) | (EX_GLISS << 4);
			break;
		case 0x2:	/* Finetune */
			e->fxp =
			    ((LSN(e->fxp) - 8) & 0x0f) | (EX_FINETUNE << 4);
			break;
		case 0x3:	/* Vibrato wave */
			e->fxp = LSN(e->fxp) | (EX_VIBRATO_WF << 4);
			break;
		case 0x4:	/* Tremolo wave */
			e->fxp = LSN(e->fxp) | (EX_TREMOLO_WF << 4);
			break;
		case 0x5:
		case 0x6:
		case 0x7:
		case 0x9:
		case 0xa:	/* Ignore */
			e->fxt = e->fxp = 0;
			break;
		case 0x8:	/* Set pan */
			e->fxt = FX_SETPAN;
			e->fxp = l << 4;
			break;
		case 0xb:	/* Pattern loop */
			e->fxp = LSN(e->fxp) | (EX_PATTERN_LOOP << 4);
			break;
		case 0xc:
			if (!l)
				e->fxt = e->fxp = 0;
		}
		break;
	case FX_SETPAN:
		/* Saga Musix says: "The X effect in S3M files is not
		 * exclusive to IT and clones. You will find tons of S3Ms made
		 * with ST3 itself using this effect (and relying on an
		 * external player being used). X in S3M also behaves
		 * differently than in IT, which your code does not seem to
		 * handle: X00 - X80 is left... right, XA4 is surround (like
		 * S91 in IT), other values are not supposed to do anything.
		 */
		if (e->fxp == 0xa4) {
			// surround
			e->fxt = FX_SURROUND;
			e->fxp = 1;
		} else {
			int pan = ((int)e->fxp) << 1;
			if (pan > 0xff) {
				pan = 0xff;
			}
			e->fxp = pan;
		}
		break;
	case NONE:		/* No effect */
		e->fxt = e->fxp = 0;
		break;
	}
}

static int s3m_load(struct module_data *m, HIO_HANDLE * f, const int start)
{
	struct xmp_module *mod = &m->mod;
	int c, r, i;
	struct xmp_event *event = 0, dummy;
	struct s3m_file_header sfh;
	struct s3m_instrument_header sih;
#ifndef LIBXMP_CORE_PLAYER
	struct s3m_adlib_header sah;
	char tracker_name[40];
#endif
	int pat_len;
	uint8 n, b;
	uint16 *pp_ins;			/* Parapointers to instruments */
	uint16 *pp_pat;			/* Parapointers to patterns */
	int stereo;
	int ret;
	uint8 buf[96]

	LOAD_INIT();

	if (hio_read(buf, 1, 96, f) != 96) {
		goto err;
	}

	memcpy(sfh.name, buf, 28);		/* Song name */
	sfh.type = buf[30];			/* File type */
	sfh.ordnum = readmem16l(buf + 32);	/* Number of orders (must be even) */
	sfh.insnum = readmem16l(buf + 34);	/* Number of instruments */
	sfh.patnum = readmem16l(buf + 36);	/* Number of patterns */
	sfh.flags = readmem16l(buf + 38);	/* Flags */
	sfh.version = readmem16l(buf + 40);	/* Tracker ID and version */
	sfh.ffi = readmem16l(buf + 42);		/* File format information */

	/* Sanity check */
	if (sfh.ffi != 1 && sfh.ffi != 2) {
		goto err;
	}
	if (sfh.ordnum > 255 || sfh.insnum > 255 || sfh.patnum > 255) {
		goto err;
	}

	sfh.magic = readmem32b(buf + 44);	/* 'SCRM' */
	sfh.gv = buf[48];			/* Global volume */
	sfh.is = buf[49];			/* Initial speed */
	sfh.it = buf[50];			/* Initial tempo */
	sfh.mv = buf[51];			/* Master volume */
	sfh.uc = buf[52];			/* Ultra click removal */
	sfh.dp = buf[53];			/* Default pan positions if 0xfc */
	memcpy(sfh.rsvd2, buf + 54, 8);		/* Reserved */
	sfh.special = readmem16l(buf + 62);	/* Ptr to special custom data */
	memcpy(sfh.chset, buf + 64, 32);	/* Channel settings */

	if (sfh.magic != MAGIC_SCRM) {
		goto err;
	}

	libxmp_copy_adjust(mod->name, sfh.name, 28);

	pp_ins = (uint16 *) calloc(sfh.insnum, sizeof(uint16));
	if (pp_ins == NULL) {
		goto err;
	}

	pp_pat = (uint16 *) calloc(sfh.patnum, sizeof(uint16));
	if (pp_pat == NULL) {
		goto err2;
	}

	if (sfh.flags & S3M_AMIGA_RANGE) {
		m->period_type = PERIOD_MODRNG;
	}
	if (sfh.flags & S3M_ST300_VOLS) {
		m->quirk |= QUIRK_VSALL;
	}
	/* m->volbase = 4096 / sfh.gv; */
	mod->spd = sfh.is;
	mod->bpm = sfh.it;
	mod->chn = 0;

	/* Mix volume and stereo flag conversion (reported by Saga Musix).
	 * 1) Old format uses mix volume 0-7, and the stereo flag is 0x10.
	 * 2) Newer ST3s unconditionally convert MV 0x02 and 0x12 to 0x20.
	 */
	m->mvolbase = 48;

	if (sfh.ffi == 1) {
		m->mvol = ((sfh.mv & 0xf) + 1) * 0x10;
		stereo = sfh.mv & 0x10;
		CLAMP(m->mvol, 0x10, 0x7f);

	} else if (sfh.mv == 0x02 || sfh.mv == 0x12) {
		m->mvol = 0x20;
		stereo = sfh.mv & 0x10;

	} else {
		m->mvol = sfh.mv & S3M_MV_VOLUME;
		stereo = sfh.mv & S3M_MV_STEREO;

		if (m->mvol == 0) {
			m->mvol = 48;		/* Default is 48 */
		} else if (m->mvol < 16) {
			m->mvol = 16;		/* Minimum is 16 */
		}
	}

	/* "Note that in stereo, the mastermul is internally multiplied by
	 * 11/8 inside the player since there is generally more room in the
	 * output stream." Do the inverse to affect fewer modules. */
	if (!stereo) {
		m->mvol = m->mvol * 8 / 11;
	}

	for (i = 0; i < 32; i++) {
		int x;
		if (sfh.chset[i] == S3M_CH_OFF)
			continue;

		mod->chn = i + 1;

		x = sfh.chset[i] & S3M_CH_NUMBER;
		if (stereo && x < S3M_CH_ADLIB) {
			mod->xxc[i].pan = x < S3M_CH_RIGHT ? 0x30 : 0xc0;
		} else {
			mod->xxc[i].pan = 0x80;
		}
	}

	if (sfh.ordnum <= XMP_MAX_MOD_LENGTH) {
		mod->len = sfh.ordnum;
		if (hio_read(mod->xxo, 1, mod->len, f) != mod->len) {
			goto err3;
		}
	} else {
		mod->len = XMP_MAX_MOD_LENGTH;
		if (hio_read(mod->xxo, 1, mod->len, f) != mod->len) {
			goto err3;
		}
		if (hio_seek(f, sfh.ordnum - XMP_MAX_MOD_LENGTH, SEEK_CUR) < 0) {
			goto err3;
		}
	}

	/* Don't trust sfh.patnum */
	mod->pat = -1;
	for (i = 0; i < mod->len; ++i) {
		if (mod->xxo[i] < 0xfe && mod->xxo[i] > mod->pat) {
			mod->pat = mod->xxo[i];
		}
	}
	mod->pat++;
	if (mod->pat > sfh.patnum) {
		mod->pat = sfh.patnum;
	}
	if (mod->pat == 0) {
		goto err3;
	}

	mod->trk = mod->pat * mod->chn;
	/* Load and convert header */
	mod->ins = sfh.insnum;
	mod->smp = mod->ins;

	for (i = 0; i < sfh.insnum; i++) {
		pp_ins[i] = hio_read16l(f);
	}

	for (i = 0; i < sfh.patnum; i++) {
		pp_pat[i] = hio_read16l(f);
	}

	/* Default pan positions */

	for (i = 0, sfh.dp -= 0xfc; !sfh.dp /* && n */  && (i < 32); i++) {
		uint8 x = hio_read8(f);
		if (x & S3M_PAN_SET) {
			mod->xxc[i].pan = (x << 4) & 0xff;
		}
	}

	m->c4rate = C4_NTSC_RATE;

	if (sfh.version == 0x1300) {
		m->quirk |= QUIRK_VSALL;
	}

#ifndef LIBXMP_CORE_PLAYER
	switch (sfh.version >> 12) {
	case 1:
		snprintf(tracker_name, 40, "Scream Tracker %d.%02x",
			 (sfh.version & 0x0f00) >> 8, sfh.version & 0xff);
		m->quirk |= QUIRK_ST3BUGS;
		break;
	case 2:
		snprintf(tracker_name, 40, "Imago Orpheus %d.%02x",
			 (sfh.version & 0x0f00) >> 8, sfh.version & 0xff);
		break;
	case 3:
		if (sfh.version == 0x3216) {
			strcpy(tracker_name, "Impulse Tracker 2.14v3");
		} else if (sfh.version == 0x3217) {
			strcpy(tracker_name, "Impulse Tracker 2.14v5");
		} else {
			snprintf(tracker_name, 40, "Impulse Tracker %d.%02x",
				 (sfh.version & 0x0f00) >> 8,
				 sfh.version & 0xff);
		}
		break;
	case 5:
		if (sfh.version == 0x5447) {
			strcpy(tracker_name, "Graoumf Tracker");
		} else if (sfh.rsvd2[0] || sfh.rsvd2[1]) {
			snprintf(tracker_name, 40, "OpenMPT %d.%02x.%02x.%02x",
				 (sfh.version & 0x0f00) >> 8, sfh.version & 0xff, sfh.rsvd2[1], sfh.rsvd2[0]);
		} else {
			snprintf(tracker_name, 40, "OpenMPT %d.%02x",
				 (sfh.version & 0x0f00) >> 8, sfh.version & 0xff);
		}
		m->quirk |= QUIRK_ST3BUGS;
		break;
	case 4:
		if (sfh.version != 0x4100) {
			libxmp_schism_tracker_string(tracker_name, 40,
				(sfh.version & 0x0fff), sfh.rsvd2[0] | (sfh.rsvd2[1] << 8));
			break;
		}
		/* fall through */
	case 6:
		snprintf(tracker_name, 40, "BeRoTracker %d.%02x",
			 (sfh.version & 0x0f00) >> 8, sfh.version & 0xff);
		break;
	default:
		snprintf(tracker_name, 40, "unknown (%04x)", sfh.version);
	}

	libxmp_set_type(m, "%s S3M", tracker_name);
#else
	libxmp_set_type(m, "Scream Tracker 3");
	m->quirk |= QUIRK_ST3BUGS;
#endif

	MODULE_INFO();

	if (libxmp_init_pattern(mod) < 0)
		goto err3;

	/* Read patterns */

	D_(D_INFO "Stored patterns: %d", mod->pat);

	for (i = 0; i < mod->pat; i++) {
		if (libxmp_alloc_pattern_tracks(mod, i, 64) < 0)
			goto err3;

		if (pp_pat[i] == 0)
			continue;

		hio_seek(f, start + pp_pat[i] * 16, SEEK_SET);
		r = 0;
		pat_len = hio_read16l(f) - 2;

		while (pat_len >= 0 && r < mod->xxp[i]->rows) {
			b = hio_read8(f);

			if (hio_error(f)) {
				goto err3;
			}

			if (b == S3M_EOR) {
				r++;
				continue;
			}

			c = b & S3M_CH_MASK;
			event = c >= mod->chn ? &dummy : &EVENT(i, c, r);

			if (b & S3M_NI_FOLLOW) {
				switch (n = hio_read8(f)) {
				case 255:
					n = 0;
					break;	/* Empty note */
				case 254:
					n = XMP_KEY_OFF;
					break;	/* Key off */
				default:
					n = 13 + 12 * MSN(n) + LSN(n);
				}
				event->note = n;
				event->ins = hio_read8(f);
				pat_len -= 2;
			}

			if (b & S3M_VOL_FOLLOWS) {
				event->vol = hio_read8(f) + 1;
				pat_len--;
			}

			if (b & S3M_FX_FOLLOWS) {
				event->fxt = hio_read8(f);
				event->fxp = hio_read8(f);
				xlat_fx(c, event);

				pat_len -= 2;
			}
		}
	}

	D_(D_INFO "Stereo enabled: %s", stereo ? "yes" : "no");
	D_(D_INFO "Pan settings: %s", sfh.dp ? "no" : "yes");

	if (libxmp_init_instrument(m) < 0)
		goto err3;

	/* Read and convert instruments and samples */

	D_(D_INFO "Instruments: %d", mod->ins);

	for (i = 0; i < mod->ins; i++) {
		struct xmp_instrument *xxi = &mod->xxi[i];
		struct xmp_sample *xxs = &mod->xxs[i];
		struct xmp_subinstrument *sub;
		int load_sample_flags;
		uint32 sample_segment;

		xxi->sub = (struct xmp_subinstrument *) calloc(1, sizeof(struct xmp_subinstrument));
		if (xxi->sub == NULL) {
			goto err3;
		}

		sub = &xxi->sub[0];

		hio_seek(f, start + pp_ins[i] * 16, SEEK_SET);
		sub->pan = 0x80;
		sub->sid = i;

		if (hio_read(buf, 1, 80, f) != 80) {
			goto err3;
		}

		if (buf[0] >= 2) {
#ifndef LIBXMP_CORE_PLAYER
			/* OPL2 FM instrument */

			memcpy(sah.dosname, buf + 1, 12);	/* DOS file name */
			memcpy(sah.reg, buf + 16, 12);		/* Adlib registers */
			sah.vol = buf[28];
			sah.dsk = buf[29];
			sah.c2spd = readmem16l(buf + 32);	/* C4 speed */
			memcpy(sah.name, buf + 48, 28);		/* Instrument name */
			sah.magic = readmem32b(buf + 76);		/* 'SCRI' */

			if (sah.magic != MAGIC_SCRI) {
				D_(D_CRIT "error: FM instrument magic");
				goto err3;
			}
			sah.magic = 0;

			libxmp_instrument_name(mod, i, sah.name, 28);

			xxi->nsm = 1;
			sub->vol = sah.vol;
			libxmp_c2spd_to_note(sah.c2spd, &sub->xpo, &sub->fin);
			sub->xpo += 12;
			ret = libxmp_load_sample(m, f, SAMPLE_FLAG_ADLIB, xxs, (char *)sah.reg);
			if (ret < 0)
				goto err3;

			D_(D_INFO "[%2X] %-28.28s", i, xxi->name);

			continue;
#else
			goto err3;
#endif
		}

		memcpy(sih.dosname, buf + 1, 12);	/* DOS file name */
		sih.memseg_hi = buf[13];		/* High byte of sample pointer */
		sih.memseg = readmem16l(buf + 14);	/* Pointer to sample data */
		sih.length = readmem32l(buf + 16);	/* Length */

#if 0
		/* ST3 limit */
		if ((sfh.version >> 12) == 1 && sih.length > 64000)
			sih.length = 64000;
#endif

		if (sih.length > MAX_SAMPLE_SIZE) {
			goto err3;
		}

		sih.loopbeg = readmem32l(buf + 20);	/* Loop begin */
		sih.loopend = readmem32l(buf + 24);	/* Loop end */
		sih.vol = buf[28];			/* Volume */
		sih.pack = buf[30];			/* Packing type */
		sih.flags = buf[31];			/* Loop/stereo/16bit flags */
		sih.c2spd = readmem16l(buf + 32);	/* C4 speed */
		memcpy(sih.name, buf + 48, 28);		/* Instrument name */
		sih.magic = readmem32b(buf + 76);	/* 'SCRS' */

		if (buf[0] == 1 && sih.magic != MAGIC_SCRS) {
			D_(D_CRIT "error: instrument magic");
			goto err3;
		}

		xxs->len = sih.length;
		xxi->nsm = sih.length > 0 ? 1 : 0;
		xxs->lps = sih.loopbeg;
		xxs->lpe = sih.loopend;

		xxs->flg = sih.flags & 1 ? XMP_SAMPLE_LOOP : 0;

		if (sih.flags & 4) {
			xxs->flg |= XMP_SAMPLE_16BIT;
		}

		load_sample_flags = (sfh.ffi == 1) ? 0 : SAMPLE_FLAG_UNS;
		if (sih.pack == 4) {
			load_sample_flags = SAMPLE_FLAG_ADPCM;
		}

		sub->vol = sih.vol;
		sih.magic = 0;

		libxmp_instrument_name(mod, i, sih.name, 28);

		D_(D_INFO "[%2X] %-28.28s %04x%c%04x %04x %c V%02x %5d",
		   i, mod->xxi[i].name, mod->xxs[i].len,
		   xxs->flg & XMP_SAMPLE_16BIT ? '+' : ' ',
		   xxs->lps, mod->xxs[i].lpe,
		   xxs->flg & XMP_SAMPLE_LOOP ? 'L' : ' ', sub->vol, sih.c2spd);

		libxmp_c2spd_to_note(sih.c2spd, &sub->xpo, &sub->fin);

		sample_segment = sih.memseg + ((uint32)sih.memseg_hi << 16);

		if (hio_seek(f, start + 16L * sample_segment, SEEK_SET) < 0) {
			goto err3;
		}

		ret = libxmp_load_sample(m, f, load_sample_flags, xxs, NULL);
		if (ret < 0) {
			goto err3;
		}
	}

	free(pp_pat);
	free(pp_ins);

	m->quirk |= QUIRKS_ST3 | QUIRK_ARPMEM;
	m->read_event_type = READ_EVENT_ST3;

	return 0;

err3:
	free(pp_pat);
err2:
	free(pp_ins);
err:
	return -1;
}
