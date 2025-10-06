/* Extended Module Player
 * Copyright (C) 1996-2025 Claudio Matsuoka and Hipolito Carraro Jr
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

#ifndef LIBXMP_CORE_DISABLE_IT

#include "loader.h"
#include "it.h"
#include "../period.h"

#define MAGIC_IMPM	MAGIC4('I','M','P','M')
#define MAGIC_IMPI	MAGIC4('I','M','P','I')
#define MAGIC_IMPS	MAGIC4('I','M','P','S')

#define TEMP_BUFFER_LEN	65536

static int it_test(HIO_HANDLE *, char *, const int);
static int it_load(struct module_data *, HIO_HANDLE *, const int);

const struct format_loader libxmp_loader_it = {
	"Impulse Tracker",
	it_test,
	it_load
};

static int it_test(HIO_HANDLE *f, char *t, const int start)
{
	if (hio_read32b(f) != MAGIC_IMPM)
		return -1;

	libxmp_read_title(f, t, 26);

	return 0;
}


#define	FX_NONE	0xff
#define FX_XTND 0xfe
#define L_CHANNELS 64

static const uint8 fx[32] = {
	/*   */ FX_NONE,
	/* A */ FX_S3M_SPEED,
	/* B */ FX_JUMP,
	/* C */ FX_IT_BREAK,
	/* D */ FX_VOLSLIDE,
	/* E */ FX_PORTA_DN,
	/* F */ FX_PORTA_UP,
	/* G */ FX_TONEPORTA,
	/* H */ FX_VIBRATO,
	/* I */ FX_TREMOR,
	/* J */ FX_S3M_ARPEGGIO,
	/* K */ FX_VIBRA_VSLIDE,
	/* L */ FX_TONE_VSLIDE,
	/* M */ FX_TRK_VOL,
	/* N */ FX_TRK_VSLIDE,
	/* O */ FX_OFFSET,
	/* P */ FX_IT_PANSLIDE,
	/* Q */ FX_MULTI_RETRIG,
	/* R */ FX_TREMOLO,
	/* S */ FX_XTND,
	/* T */ FX_IT_BPM,
	/* U */ FX_FINE_VIBRATO,
	/* V */ FX_GLOBALVOL,
	/* W */ FX_GVOL_SLIDE,
	/* X */ FX_SETPAN,
	/* Y */ FX_PANBRELLO,
	/* Z */ FX_MACRO,
	/* ? */ FX_NONE,
	/* / */ FX_MACROSMOOTH,
	/* ? */ FX_NONE,
	/* ? */ FX_NONE,
	/* ? */ FX_NONE
};

static void xlat_fx(int c, struct xmp_event *e, uint8 *last_fxp, int new_fx)
{
	uint8 h = MSN(e->fxp), l = LSN(e->fxp);

	switch (e->fxt = fx[e->fxt]) {
	case FX_XTND:		/* Extended effect */
		e->fxt = FX_EXTENDED;

		if (h == 0 && e->fxp == 0) {
			e->fxp = last_fxp[c];
			h = MSN(e->fxp);
			l = LSN(e->fxp);
		} else {
			last_fxp[c] = e->fxp;
		}

		switch (h) {
		case 0x1:	/* Glissando */
			e->fxp = 0x30 | l;
			break;
		case 0x2:	/* Finetune -- not supported */
			e->fxt = e->fxp = 0;
			break;
		case 0x3:	/* Vibrato wave */
			e->fxp = 0x40 | l;
			break;
		case 0x4:	/* Tremolo wave */
			e->fxp = 0x70 | l;
			break;
		case 0x5:	/* Panbrello wave */
			if (l <= 3) {
				e->fxt = FX_PANBRELLO_WF;
				e->fxp = l;
			} else {
				e->fxt = e->fxp = 0;
			}
			break;
		case 0x6:	/* Pattern delay */
			e->fxp = 0xe0 | l;
			break;
		case 0x7:	/* Instrument functions */
			e->fxt = FX_IT_INSTFUNC;
			e->fxp &= 0x0f;
			break;
		case 0x8:	/* Set pan position */
			e->fxt = FX_SETPAN;
			e->fxp = l << 4;
			break;
		case 0x9:
			if (l == 0 || l == 1) {
				/* 0x91 = set surround */
				e->fxt = FX_SURROUND;
				e->fxp = l;
			} else if (l == 0xe || l == 0xf) {
				/* 0x9f Play reverse (MPT) */
				e->fxt = FX_REVERSE;
				e->fxp = l - 0xe;
			}
			break;
		case 0xa:	/* High offset */
			e->fxt = FX_HIOFFSET;
			e->fxp = l;
			break;
		case 0xb:	/* Pattern loop */
			e->fxp = 0x60 | l;
			break;
		case 0xc:	/* Note cut */
		case 0xd:	/* Note delay */
			if ((e->fxp = l) == 0)
				e->fxp++;  /* SD0 and SC0 become SD1 and SC1 */
			e->fxp |= h << 4;
			break;
		case 0xe:	/* Pattern row delay */
			e->fxt = FX_IT_ROWDELAY;
			e->fxp = l;
			break;
		case 0xf:	/* Set parametered macro */
			e->fxt = FX_MACRO_SET;
			e->fxp = l;
			break;
		default:
			e->fxt = e->fxp = 0;
		}
		break;
	case FX_TREMOR:
		if (!new_fx && e->fxp != 0) {
			e->fxp = ((MSN(e->fxp) + 1) << 4) | (LSN(e->fxp) + 1);
		}
		break;
	case FX_GLOBALVOL:
		if (e->fxp > 0x80) {	/* See storlek test 16 */
			e->fxt = e->fxp = 0;
		}
		break;
	case FX_NONE:		/* No effect */
		e->fxt = e->fxp = 0;
		break;
	}
}


static void xlat_volfx(struct xmp_event *event)
{
	int b;

	b = event->vol;
	event->vol = 0;

	if (b <= 0x40) {
		event->vol = b + 1;
	} else if (b >= 65 && b <= 74) {	/* A */
		event->f2t = FX_F_VSLIDE_UP_2;
		event->f2p = b - 65;
	} else if (b >= 75 && b <= 84) {	/* B */
		event->f2t = FX_F_VSLIDE_DN_2;
		event->f2p = b - 75;
	} else if (b >= 85 && b <= 94) {	/* C */
		event->f2t = FX_VSLIDE_UP_2;
		event->f2p = b - 85;
	} else if (b >= 95 && b <= 104) {	/* D */
		event->f2t = FX_VSLIDE_DN_2;
		event->f2p = b - 95;
	} else if (b >= 105 && b <= 114) {	/* E */
		event->f2t = FX_PORTA_DN;
		event->f2p = (b - 105) << 2;
	} else if (b >= 115 && b <= 124) {	/* F */
		event->f2t = FX_PORTA_UP;
		event->f2p = (b - 115) << 2;
	} else if (b >= 128 && b <= 192) {	/* pan */
		if (b == 192) {
			event->f2p = 0xff;
		} else {
			event->f2p = (b - 128) << 2;
		}
		event->f2t = FX_SETPAN;
	} else if (b >= 193 && b <= 202) {	/* G */
		uint8 val[10] = {
			0x00, 0x01, 0x04, 0x08, 0x10,
			0x20, 0x40, 0x60, 0x80, 0xff
		};
		event->f2t = FX_TONEPORTA;
		event->f2p = val[b - 193];
	} else if (b >= 203 && b <= 212) {	/* H */
		event->f2t = FX_VIBRATO;
		event->f2p = b - 203;
	}
}


static void fix_name(uint8 *s, int l)
{
	int i;

	/* IT names can have 0 at start of data, replace with space */
	for (l--, i = 0; i < l; i++) {
		if (s[i] == 0)
			s[i] = ' ';
	}
	for (i--; i >= 0 && s[i] == ' '; i--) {
		if (s[i] == ' ')
			s[i] = 0;
	}
}


static int load_it_midi_config(struct module_data *m, HIO_HANDLE *f)
{
	int i;

	m->midi = (struct midi_macro_data *) calloc(1, sizeof(struct midi_macro_data));
	if (m->midi == NULL)
		return -1;

	/* Skip global MIDI macros */
	if (hio_seek(f, 9 * 32, SEEK_CUR) < 0)
		return -1;

	/* SFx macros */
	for (i = 0; i < 16; i++) {
		if (hio_read(m->midi->param[i].data, 1, 32, f) < 32)
			return -1;
		m->midi->param[i].data[31] = '\0';
	}
	/* Zxx macros */
	for (i = 0; i < 128; i++) {
		if (hio_read(m->midi->fixed[i].data, 1, 32, f) < 32)
			return -1;
		m->midi->fixed[i].data[31] = '\0';
	}
	return 0;
}


static int read_envelope(struct xmp_envelope *ei, struct it_envelope *env,
			  HIO_HANDLE *f)
{
	int i;
	uint8 buf[82];

	if (hio_read(buf, 1, 82, f) != 82) {
		return -1;
	}

	env->flg = buf[0];
	env->num = MIN(buf[1], 25); /* Clamp to IT max */

	env->lpb = buf[2];
	env->lpe = buf[3];
	env->slb = buf[4];
	env->sle = buf[5];

	for (i = 0; i < 25; i++) {
		env->node[i].y = buf[6 + i * 3];
		env->node[i].x = readmem16l(buf + 7 + i * 3);
	}

	ei->flg = env->flg & IT_ENV_ON ? XMP_ENVELOPE_ON : 0;

	if (env->flg & IT_ENV_LOOP) {
		ei->flg |= XMP_ENVELOPE_LOOP;
	}

	if (env->flg & IT_ENV_SLOOP) {
		ei->flg |= XMP_ENVELOPE_SUS | XMP_ENVELOPE_SLOOP;
	}

	if (env->flg & IT_ENV_CARRY) {
		ei->flg |= XMP_ENVELOPE_CARRY;
	}

	ei->npt = env->num;
	ei->sus = env->slb;
	ei->sue = env->sle;
	ei->lps = env->lpb;
	ei->lpe = env->lpe;

	if (ei->npt > 0 && ei->npt <= 25 /* XMP_MAX_ENV_POINTS */) {
		for (i = 0; i < ei->npt; i++) {
			ei->data[i * 2] = env->node[i].x;
			ei->data[i * 2 + 1] = env->node[i].y;
		}
	} else {
		ei->flg &= ~XMP_ENVELOPE_ON;
	}

	return 0;
}

static void identify_tracker(struct module_data *m, struct it_file_header *ifh,
			     int pat_before_smp, int *is_mpt_116)
{
#ifndef LIBXMP_CORE_PLAYER
	char tracker_name[40];
	int sample_mode = ~ifh->flags & IT_USE_INST;

	m->flow_mode = FLOW_MODE_IT_210;
	switch (ifh->cwt >> 8) {
	case 0x00:
		strcpy(tracker_name, "unmo3");
		break;
	case 0x01:
	case 0x02:		/* test from Schism Tracker sources */
		if (ifh->cmwt == 0x0200 && ifh->cwt == 0x0214
		    && ifh->flags == 9 && ifh->special == 0
		    && ifh->hilite_maj == 0 && ifh->hilite_min == 0
		    && ifh->insnum == 0 && ifh->patnum + 1 == ifh->ordnum
		    && ifh->gv == 128 && ifh->mv == 100 && ifh->is == 1
		    && ifh->sep == 128 && ifh->pwd == 0
		    && ifh->msglen == 0 && ifh->msgofs == 0 && ifh->rsvd == 0) {
			strcpy(tracker_name, "OpenSPC conversion");
		} else if (ifh->cmwt == 0x0200 && ifh->cwt == 0x0217) {
			strcpy(tracker_name, "ModPlug Tracker 1.16");
			/* ModPlug Tracker files aren't really IMPM 2.00 */
			ifh->cmwt = sample_mode ? 0x100 : 0x214;
			m->flow_mode = FLOW_MODE_MPT_116;
			*is_mpt_116 = 1;
		} else if (ifh->cmwt == 0x0200 && ifh->cwt == 0x0202 && pat_before_smp) {
			/* ModPlug Tracker ITs from pre-alpha 4 use tracker
			 * 0x0202 and format 0x0200. Unfortunately, ITs from
			 * Impulse Tracker may *also* use this. These MPT ITs
			 * can be detected because they write patterns before
			 * samples/instruments. */
			strcpy(tracker_name, "ModPlug Tracker 1.0 pre-alpha");
			ifh->cmwt = sample_mode ? 0x100 : 0x200;
			/* TODO: pre-alpha 4 has its own Pattern Loop behavior;
			 * the <=1.16 behavior is present in pre-alpha 6. */
			m->flow_mode = FLOW_MODE_MPT_116;
			*is_mpt_116 = 1;
		} else if (ifh->cwt == 0x0216) {
			strcpy(tracker_name, "Impulse Tracker 2.14v3");
		} else if (ifh->cwt == 0x0217) {
			strcpy(tracker_name, "Impulse Tracker 2.14v5");
		} else if (ifh->cwt == 0x0214 && !memcmp(&ifh->rsvd, "CHBI", 4)) {
			strcpy(tracker_name, "Chibi Tracker");
		} else {
			snprintf(tracker_name, 40, "Impulse Tracker %d.%02x",
				 (ifh->cwt & 0x0f00) >> 8, ifh->cwt & 0xff);

			if (ifh->cwt < 0x104) {
				m->flow_mode = FLOW_MODE_IT_100;
			} else if (ifh->cwt < 0x200) {
				m->flow_mode = FLOW_MODE_IT_104;
			} else if (ifh->cwt < 0x210) {
				m->flow_mode = FLOW_MODE_IT_200;
			}
		}
		break;
	case 0x08:
	case 0x7f:
		if (ifh->cwt == 0x0888) {
			strcpy(tracker_name, "OpenMPT 1.17");
			/* TODO: 1.17.02.49 onward implement IT 2.10+
			 * Pattern Loop when the IT compatibility flag is set
			 * (by default, it is not set). */
			m->flow_mode = FLOW_MODE_MPT_116;
			*is_mpt_116 = 1;
		} else if (ifh->cwt == 0x7fff) {
			strcpy(tracker_name, "munch.py");
		} else {
			snprintf(tracker_name, 40, "unknown (%04x)", ifh->cwt);
		}
		break;
	default:
		switch (ifh->cwt >> 12) {
		case 0x1:
			libxmp_schism_tracker_string(tracker_name, 40,
				(ifh->cwt & 0x0fff), ifh->rsvd);
			break;
		case 0x5:
			snprintf(tracker_name, 40, "OpenMPT %d.%02x",
				 (ifh->cwt & 0x0f00) >> 8, ifh->cwt & 0xff);
			if (memcmp(&ifh->rsvd, "OMPT", 4))
				strncat(tracker_name, " (compat.)", 39);
			break;
		case 0x06:
			snprintf(tracker_name, 40, "BeRoTracker %d.%02x",
				 (ifh->cwt & 0x0f00) >> 8, ifh->cwt & 0xff);
			break;
		default:
			snprintf(tracker_name, 40, "unknown (%04x)", ifh->cwt);
		}
	}

	libxmp_set_type(m, "%s IT %d.%02x", tracker_name, ifh->cmwt >> 8,
						ifh->cmwt & 0xff);
#else
	libxmp_set_type(m, "Impulse Tracker");
	m->flow_mode = FLOW_MODE_IT_210;
#endif
}

static int load_old_it_instrument(struct xmp_instrument *xxi, HIO_HANDLE *f)
{
	int inst_map[120], inst_rmap[XMP_MAX_KEYS];
	struct it_instrument1_header i1h;
	int c, k, j;
	uint8 buf[64];

	if (hio_read(buf, 1, 64, f) != 64) {
		return -1;
	}

	i1h.magic = readmem32b(buf);
	if (i1h.magic != MAGIC_IMPI) {
		D_(D_CRIT "bad instrument magic");
		return -1;
	}
	memcpy(i1h.dosname, buf + 4, 12);
	i1h.zero = buf[16];
	i1h.flags = buf[17];
	i1h.vls = buf[18];
	i1h.vle = buf[19];
	i1h.sls = buf[20];
	i1h.sle = buf[21];
	i1h.fadeout = readmem16l(buf + 24);
	i1h.nna = buf[26];
	i1h.dnc = buf[27];
	i1h.trkvers = readmem16l(buf + 28);
	i1h.nos = buf[30];

	memcpy(i1h.name, buf + 32, 26);
	fix_name(i1h.name, 26);

	if (hio_read(i1h.keys, 1, 240, f) != 240) {
		return -1;
	}
	if (hio_read(i1h.epoint, 1, 200, f) != 200) {
		return -1;
	}
	if (hio_read(i1h.enode, 1, 50, f) != 50) {
		return -1;
	}

	libxmp_copy_adjust(xxi->name, i1h.name, 25);

	xxi->rls = i1h.fadeout << 7;

	xxi->aei.flg = 0;
	if (i1h.flags & IT_ENV_ON) {
		xxi->aei.flg |= XMP_ENVELOPE_ON;
	}
	if (i1h.flags & IT_ENV_LOOP) {
		xxi->aei.flg |= XMP_ENVELOPE_LOOP;
	}
	if (i1h.flags & IT_ENV_SLOOP) {
		xxi->aei.flg |= XMP_ENVELOPE_SUS | XMP_ENVELOPE_SLOOP;
	}
	if (i1h.flags & IT_ENV_CARRY) {
		xxi->aei.flg |= XMP_ENVELOPE_SUS | XMP_ENVELOPE_CARRY;
	}
	xxi->aei.lps = i1h.vls;
	xxi->aei.lpe = i1h.vle;
	xxi->aei.sus = i1h.sls;
	xxi->aei.sue = i1h.sle;

	for (k = 0; k < 25 && i1h.enode[k * 2] != 0xff; k++) ;

	/* Sanity check */
	if (k >= 25 || i1h.enode[k * 2] != 0xff) {
		return -1;
	}

	for (xxi->aei.npt = k; k--;) {
		xxi->aei.data[k * 2] = i1h.enode[k * 2];
		xxi->aei.data[k * 2 + 1] = i1h.enode[k * 2 + 1];
	}

	/* See how many different instruments we have */
	for (j = 0; j < 120; j++)
		inst_map[j] = -1;

	for (k = j = 0; j < XMP_MAX_KEYS; j++) {
		c = j < 120 ? i1h.keys[j * 2 + 1] - 1 : -1;
		if (c < 0 || c >= 120) {
			xxi->map[j].ins = 0;
			xxi->map[j].xpo = 0;
			continue;
		}
		if (inst_map[c] == -1) {
			inst_map[c] = k;
			inst_rmap[k] = c;
			k++;
		}
		xxi->map[j].ins = inst_map[c];
		xxi->map[j].xpo = i1h.keys[j * 2] - j;
	}

	xxi->nsm = k;
	xxi->vol = 0x40;

	if (k) {
		xxi->sub = (struct xmp_subinstrument *) calloc(k, sizeof(struct xmp_subinstrument));
		if (xxi->sub == NULL) {
			return -1;
		}

		for (j = 0; j < k; j++) {
			struct xmp_subinstrument *sub = &xxi->sub[j];

			sub->sid = inst_rmap[j];
			sub->nna = i1h.nna;
			sub->dct =
			    i1h.dnc ? XMP_INST_DCT_NOTE : XMP_INST_DCT_OFF;
			sub->dca = XMP_INST_DCA_CUT;
			sub->pan = -1;
		}
	}

	D_(D_INFO "[  ] %-26.26s %d %-4.4s %4d %2d %c%c%c %3d",
	   /*i,*/ i1h.name,
	   i1h.nna,
	   i1h.dnc ? "on" : "off",
	   i1h.fadeout,
	   xxi->aei.npt,
	   xxi->aei.flg & XMP_ENVELOPE_ON ? 'V' : '-',
	   xxi->aei.flg & XMP_ENVELOPE_LOOP ? 'L' : '-',
	   xxi->aei.flg & XMP_ENVELOPE_SUS ? 'S' : '-', xxi->nsm);

	return 0;
}

static int load_new_it_instrument(struct xmp_instrument *xxi, HIO_HANDLE *f)
{
	int inst_map[120], inst_rmap[XMP_MAX_KEYS];
	struct it_instrument2_header i2h;
	struct it_envelope env;
	int dca2nna[] = { 0, 2, 3, 3 /* Northern Sky (cj-north.it) has this... */ };
	int c, k, j;
	uint8 buf[64];

	if (hio_read(buf, 1, 64, f) != 64) {
		return -1;
	}

	i2h.magic = readmem32b(buf);
	if (i2h.magic != MAGIC_IMPI) {
		D_(D_CRIT "bad instrument magic");
		return -1;
	}
	memcpy(i2h.dosname, buf + 4, 12);
	i2h.zero = buf[16];
	i2h.nna = buf[17];
	i2h.dct = buf[18];
	i2h.dca = buf[19];

	/* Sanity check */
	if (i2h.dca > 3) {
		/* Northern Sky has an instrument with DCA 3 */
		D_(D_WARN "bad instrument dca: %d", i2h.dca);
		i2h.dca = 0;
	}

	i2h.fadeout = readmem16l(buf + 20);
	i2h.pps = buf[22];
	i2h.ppc = buf[23];
	i2h.gbv = buf[24];
	i2h.dfp = buf[25];
	i2h.rv = buf[26];
	i2h.rp = buf[27];
	i2h.trkvers = readmem16l(buf + 28);
	i2h.nos = buf[30];

	memcpy(i2h.name, buf + 32, 26);
	fix_name(i2h.name, 26);

	i2h.ifc = buf[58];
	i2h.ifr = buf[59];
	i2h.mch = buf[60];
	i2h.mpr = buf[61];
	i2h.mbnk = readmem16l(buf + 62);

	if (hio_read(i2h.keys, 1, 240, f) != 240) {
		D_(D_CRIT "key map read error");
		return -1;
	}

	libxmp_copy_adjust(xxi->name, i2h.name, 25);
	xxi->rls = i2h.fadeout << 6;

	/* Envelopes */

	if (read_envelope(&xxi->aei, &env, f) < 0) {
		return -1;
	}
	if (read_envelope(&xxi->pei, &env, f) < 0) {
		return -1;
	}
	if (read_envelope(&xxi->fei, &env, f) < 0) {
		return -1;
	}

	if (xxi->pei.flg & XMP_ENVELOPE_ON) {
		for (j = 0; j < xxi->pei.npt; j++)
			xxi->pei.data[j * 2 + 1] += 32;
	}

	if (xxi->aei.flg & XMP_ENVELOPE_ON && xxi->aei.npt == 0) {
		xxi->aei.npt = 1;
	}
	if (xxi->pei.flg & XMP_ENVELOPE_ON && xxi->pei.npt == 0) {
		xxi->pei.npt = 1;
	}
	if (xxi->fei.flg & XMP_ENVELOPE_ON && xxi->fei.npt == 0) {
		xxi->fei.npt = 1;
	}

	if (env.flg & IT_ENV_FILTER) {
		xxi->fei.flg |= XMP_ENVELOPE_FLT;
		for (j = 0; j < env.num; j++) {
			xxi->fei.data[j * 2 + 1] += 32;
			xxi->fei.data[j * 2 + 1] *= 4;
		}
	} else {
		/* Pitch envelope is *50 to get fine interpolation */
		for (j = 0; j < env.num; j++)
			xxi->fei.data[j * 2 + 1] *= 50;
	}

	/* See how many different instruments we have */
	for (j = 0; j < 120; j++)
		inst_map[j] = -1;

	for (k = j = 0; j < 120; j++) {
		c = i2h.keys[j * 2 + 1] - 1;
		if (c < 0 || c >= 120) {
			xxi->map[j].ins = 0xff;	/* No sample */
			xxi->map[j].xpo = 0;
			continue;
		}
		if (inst_map[c] == -1) {
			inst_map[c] = k;
			inst_rmap[k] = c;
			k++;
		}
		xxi->map[j].ins = inst_map[c];
		xxi->map[j].xpo = i2h.keys[j * 2] - j;
	}

	xxi->nsm = k;
	xxi->vol = MIN(i2h.gbv, 128) >> 1;

	if (k) {
		xxi->sub = (struct xmp_subinstrument *) calloc(k, sizeof(struct xmp_subinstrument));
		if (xxi->sub == NULL)
			return -1;

		for (j = 0; j < k; j++) {
			struct xmp_subinstrument *sub = &xxi->sub[j];

			sub->sid = inst_rmap[j];
			sub->nna = i2h.nna;
			sub->dct = i2h.dct;
			sub->dca = dca2nna[i2h.dca];
			sub->pan = i2h.dfp & 0x80 ? -1 : i2h.dfp * 4;
			sub->ifc = i2h.ifc;
			sub->ifr = i2h.ifr;
			sub->rvv = ((int)i2h.rp << 8) | i2h.rv;
		}
	}

	D_(D_INFO "[  ] %-26.26s %d %d %d %4d %4d  %2x "
	   "%02x %c%c%c %3d %02x %02x",
	   /*i,*/ i2h.name,
	   i2h.nna, i2h.dct, i2h.dca,
	   i2h.fadeout,
	   i2h.gbv,
	   i2h.dfp & 0x80 ? 0x80 : i2h.dfp * 4,
	   i2h.rv,
	   xxi->aei.flg & XMP_ENVELOPE_ON ? 'V' : '-',
	   xxi->pei.flg & XMP_ENVELOPE_ON ? 'P' : '-',
	   env.flg & 0x01 ? env.flg & 0x80 ? 'F' : 'P' : '-',
	   xxi->nsm, i2h.ifc, i2h.ifr);

	return 0;
}

static void force_sample_length(struct xmp_sample *xxs, struct extra_sample_data *xtra, int len)
{
	xxs->len = len;

	if (xxs->lpe > xxs->len)
		xxs->lpe = xxs->len;

	if (xxs->lps >= xxs->len)
		xxs->flg &= ~XMP_SAMPLE_LOOP;

	if (xtra) {
		if (xtra->sue > xxs->len)
			xtra->sue = xxs->len;

		if(xtra->sus >= xxs->len)
			xxs->flg &= ~(XMP_SAMPLE_SLOOP | XMP_SAMPLE_SLOOP_BIDIR);
	}
}

static void *unpack_it_sample(struct xmp_sample *xxs,
	const struct it_sample_header *ish, uint8 *tmpbuf, HIO_HANDLE *f)
{
	void *decbuf;
	int bytes = xxs->len;
	int channels = 1;
	int i;

	if (ish->flags & IT_SMP_16BIT)
		bytes <<= 1;

	if (ish->flags & IT_SMP_STEREO) {
		bytes <<= 1;
		channels = 2;
	}

	decbuf = calloc(1, bytes);
	if (decbuf == NULL)
		return NULL;

	if (ish->flags & IT_SMP_16BIT) {
		int16 *pos = (int16 *)decbuf;

		for (i = 0; i < channels; i++) {
			itsex_decompress16(f, pos, xxs->len,
					   tmpbuf, TEMP_BUFFER_LEN,
					   ish->convert & IT_CVT_DIFF);
			pos += xxs->len;
		}
	} else {
		uint8 *pos = (uint8 *)decbuf;

		for(i = 0; i < channels; i++) {
			itsex_decompress8(f, pos, xxs->len,
					  tmpbuf, TEMP_BUFFER_LEN,
					  ish->convert & IT_CVT_DIFF);
			pos += xxs->len;
		}
	}
	return decbuf;
}

static int load_it_sample(struct module_data *m, int i, int start,
			  int sample_mode, uint8 *tmpbuf, HIO_HANDLE *f)
{
	struct it_sample_header ish;
	struct xmp_module *mod = &m->mod;
	struct extra_sample_data *xtra;
	struct xmp_sample *xxs;
	int j, k;
	uint8 buf[80];

	if (sample_mode) {
		mod->xxi[i].sub = (struct xmp_subinstrument *) calloc(1, sizeof(struct xmp_subinstrument));
		if (mod->xxi[i].sub == NULL) {
			return -1;
		}
	}

	if (hio_read(buf, 1, 80, f) != 80) {
		return -1;
	}

	ish.magic = readmem32b(buf);
	/* Changed to continue to allow use-brdg.it and use-funk.it to
	 * load correctly (both IT 2.04)
	 */
	if (ish.magic != MAGIC_IMPS) {
		return 0;
	}

	xxs = &mod->xxs[i];
	xtra = &m->xtra[i];

	memcpy(ish.dosname, buf + 4, 12);
	ish.zero = buf[16];
	ish.gvl = buf[17];
	ish.flags = buf[18];
	ish.vol = buf[19];

	memcpy(ish.name, buf + 20, 26);
	fix_name(ish.name, 26);

	ish.convert = buf[46];
	ish.dfp = buf[47];
	ish.length = readmem32l(buf + 48);
	ish.loopbeg = readmem32l(buf + 52);
	ish.loopend = readmem32l(buf + 56);
	ish.c5spd = readmem32l(buf + 60);
	ish.sloopbeg = readmem32l(buf + 64);
	ish.sloopend = readmem32l(buf + 68);
	ish.sample_ptr = readmem32l(buf + 72);
	ish.vis = buf[76];
	ish.vid = buf[77];
	ish.vir = buf[78];
	ish.vit = buf[79];

	if (ish.flags & IT_SMP_16BIT) {
		xxs->flg = XMP_SAMPLE_16BIT;
	}
	if (ish.flags & IT_SMP_STEREO) {
		xxs->flg |= XMP_SAMPLE_STEREO;
	}
	xxs->len = ish.length;

	xxs->lps = ish.loopbeg;
	xxs->lpe = ish.loopend;
	xxs->flg |= ish.flags & IT_SMP_LOOP ? XMP_SAMPLE_LOOP : 0;
	xxs->flg |= ish.flags & IT_SMP_BLOOP ? XMP_SAMPLE_LOOP_BIDIR : 0;
	xxs->flg |= ish.flags & IT_SMP_SLOOP ? XMP_SAMPLE_SLOOP : 0;
	xxs->flg |= ish.flags & IT_SMP_BSLOOP ? XMP_SAMPLE_SLOOP_BIDIR : 0;

	if (ish.flags & IT_SMP_SLOOP) {
		xtra->sus = ish.sloopbeg;
		xtra->sue = ish.sloopend;
	}

	if (sample_mode) {
		/* Create an instrument for each sample */
		mod->xxi[i].vol = 64;
		mod->xxi[i].sub[0].vol = ish.vol;
		mod->xxi[i].sub[0].pan = 0x80;
		mod->xxi[i].sub[0].sid = i;
		mod->xxi[i].nsm = !!(xxs->len);
		libxmp_instrument_name(mod, i, ish.name, 25);
	} else {
		libxmp_copy_adjust(xxs->name, ish.name, 25);
	}

	D_(D_INFO "\n[%2X] %-26.26s %05x%c%c %05x %05x %05x %05x "
	   "%02x%02x %02x%02x %5d ",
	   i, sample_mode ? xxs->name : mod->xxs[i].name,
	   xxs->len,
	   ish.flags & IT_SMP_16BIT ? '+' : ' ',
	   ish.flags & IT_SMP_STEREO ? 's' : ' ',
	   MIN(xxs->lps, 0xfffff), MIN(xxs->lpe, 0xfffff),
	   MIN(ish.sloopbeg, 0xfffff), MIN(ish.sloopend, 0xfffff),
	   ish.flags, ish.convert, ish.vol, ish.gvl, ish.c5spd);

	/* Convert C5SPD to relnote/finetune
	 *
	 * In IT we can have a sample associated with two or more
	 * instruments, but c5spd is a sample attribute -- so we must
	 * scan all xmp instruments to set the correct transposition
	 */

	for (j = 0; j < mod->ins; j++) {
		for (k = 0; k < mod->xxi[j].nsm; k++) {
			struct xmp_subinstrument *sub = &mod->xxi[j].sub[k];
			if (sub->sid == i) {
				sub->vol = ish.vol;
				sub->gvl = MIN(ish.gvl, 64);
				sub->vra = ish.vis;	/* sample to sub-instrument vibrato */
				sub->vde = ish.vid << 1;
				sub->vwf = ish.vit;
				sub->vsw = (0xff - ish.vir) >> 1;

				libxmp_c2spd_to_note(ish.c5spd,
					      &mod->xxi[j].sub[k].xpo,
					      &mod->xxi[j].sub[k].fin);

				/* Set sample pan (overrides subinstrument) */
				if (ish.dfp & 0x80) {
					sub->pan = (ish.dfp & 0x7f) * 4;
				} else if (sample_mode) {
					sub->pan = -1;
				}
			}
		}
	}

	if (ish.flags & IT_SMP_SAMPLE && xxs->len > 1) {
		int cvt = 0;

		/* Sanity check - some modules may have invalid sizes on
		 * unused samples so only check this if the sample flag is set. */
		if (xxs->len > MAX_SAMPLE_SIZE) {
			return -1;
		}

		if (0 != hio_seek(f, start + ish.sample_ptr, SEEK_SET))
			return -1;

		if (xxs->lpe > xxs->len || xxs->lps >= xxs->lpe)
			xxs->flg &= ~XMP_SAMPLE_LOOP;

		if (ish.convert == IT_CVT_ADPCM)
			cvt |= SAMPLE_FLAG_ADPCM;

		if (~ish.convert & IT_CVT_SIGNED)
			cvt |= SAMPLE_FLAG_UNS;

		/* compressed samples */
		if (ish.flags & IT_SMP_COMP) {
			long min_size, file_len, left;
			void *decbuf;
			int samples = xxs->len;
			int ret;

			if (ish.flags & IT_SMP_STEREO)
				samples <<= 1;

			/* Sanity check - the lower bound on IT compressed
			 * sample size (in bytes) is a little over 1/8th of the
			 * number of SAMPLES in the sample.
			 */
			file_len = hio_size(f);
			min_size = samples >> 3;
			left = file_len - (long)ish.sample_ptr;
			/* No data to read at all? Just skip it... */
			if (left <= 0)
				return 0;

			if ((file_len > 0) && (left < min_size)) {
				D_(D_WARN "sample %X failed minimum size check "
				   "(len=%d, needs >=%ld bytes, %ld available): "
				   "resizing to %ld",
				   i, xxs->len, min_size, left, left << 3);

				force_sample_length(xxs, xtra, left << 3);
			}

			decbuf = unpack_it_sample(xxs, &ish, tmpbuf, f);
			if (decbuf == NULL)
				return -1;

#ifdef WORDS_BIGENDIAN
			if (ish.flags & IT_SMP_16BIT) {
				/* decompression generates native-endian
				 * samples, but we want little-endian.
				 */
				cvt |= SAMPLE_FLAG_BIGEND;
			}
#endif

			ret = libxmp_load_sample(m, NULL, SAMPLE_FLAG_NOLOAD | cvt,
					  &mod->xxs[i], decbuf);
			if (ret < 0) {
				free(decbuf);
				return -1;
			}

			free(decbuf);
		} else {
			if (libxmp_load_sample(m, f, cvt, &mod->xxs[i], NULL) < 0)
				return -1;
		}
	}

	return 0;
}

static int load_it_pattern(struct module_data *m, int i, int new_fx,
			   uint8 *patbuf, HIO_HANDLE *f)
{
	struct xmp_module *mod = &m->mod;
	struct xmp_event *event, dummy, lastevent[L_CHANNELS];
	uint8 mask[L_CHANNELS];
	uint8 last_fxp[64];
	uint8 *pos;

	int r, c, pat_len, num_rows;
	uint8 b;

	r = 0;

	memset(last_fxp, 0, sizeof(last_fxp));
	memset(lastevent, 0, L_CHANNELS * sizeof(struct xmp_event));
	memset(&dummy, 0, sizeof(struct xmp_event));

	pat_len = hio_read16l(f) /* - 4 */ ;
	mod->xxp[i]->rows = num_rows = hio_read16l(f);

	if (libxmp_alloc_tracks_in_pattern(mod, i) < 0) {
		return -1;
	}

	memset(mask, 0, L_CHANNELS);
	hio_read16l(f);
	hio_read16l(f);

	if (hio_read(patbuf, 1, pat_len, f) < (size_t)pat_len) {
		D_(D_CRIT "read error loading pattern %d", i);
		return -1;
	}
	pos = patbuf;

	while (r < num_rows && --pat_len >= 0) {
		b = *(pos++);
		if (!b) {
			r++;
			continue;
		}
		c = (b - 1) & 63;

		if (b & 0x80) {
			if (pat_len < 1) break;
			mask[c] = *(pos++);
			pat_len--;
		}
		/*
		 * WARNING: we IGNORE events in disabled channels. Disabled
		 * channels should be muted only, but we don't know the
		 * real number of channels before loading the patterns and
		 * we don't want to set it to 64 channels.
		 */
		if (c >= mod->chn) {
			event = &dummy;
		} else {
			event = &EVENT(i, c, r);
		}

		if (mask[c] & 0x01) {
			if (pat_len < 1) break;
			b = *(pos++);

			/* From ittech.txt:
			 * Note ranges from 0->119 (C-0 -> B-9)
			 * 255 = note off, 254 = notecut
			 * Others = note fade (already programmed into IT's player
			 *                     but not available in the editor)
			 */
			switch (b) {
			case 0xff:	/* key off */
				b = XMP_KEY_OFF;
				break;
			case 0xfe:	/* cut */
				b = XMP_KEY_CUT;
				break;
			default:
				if (b > 119) {	/* fade */
					b = XMP_KEY_FADE;
				} else {
					b++;	/* note */
				}
			}
			lastevent[c].note = event->note = b;
			pat_len--;
		}
		if (mask[c] & 0x02) {
			if (pat_len < 1) break;
			b = *(pos++);
			lastevent[c].ins = event->ins = b;
			pat_len--;
		}
		if (mask[c] & 0x04) {
			if (pat_len < 1) break;
			b = *(pos++);
			lastevent[c].vol = event->vol = b;
			xlat_volfx(event);
			pat_len--;
		}
		if (mask[c] & 0x08) {
			if (pat_len < 2) break;
			b = *(pos++);
			if (b >= ARRAY_SIZE(fx)) {
				D_(D_WARN "invalid effect %#02x", b);
				pos++;

			} else {
				event->fxt = b;
				event->fxp = *(pos++);

				xlat_fx(c, event, last_fxp, new_fx);
				lastevent[c].fxt = event->fxt;
				lastevent[c].fxp = event->fxp;
			}
			pat_len -= 2;
		}
		if (mask[c] & 0x10) {
			event->note = lastevent[c].note;
		}
		if (mask[c] & 0x20) {
			event->ins = lastevent[c].ins;
		}
		if (mask[c] & 0x40) {
			event->vol = lastevent[c].vol;
			xlat_volfx(event);
		}
		if (mask[c] & 0x80) {
			event->fxt = lastevent[c].fxt;
			event->fxp = lastevent[c].fxp;
		}
	}

	return 0;
}

static int it_load(struct module_data *m, HIO_HANDLE *f, const int start)
{
	struct xmp_module *mod = &m->mod;
	int c, i, j;
	struct it_file_header ifh;
	int max_ch;
	uint32 *pp_ins;		/* Pointers to instruments */
	uint32 *pp_smp;		/* Pointers to samples */
	uint32 *pp_pat;		/* Pointers to patterns */
	uint8 *patbuf = NULL;
	uint8 *pos;
	int new_fx, sample_mode;
	int pat_before_smp = 0;
	int is_mpt_116 = 0;

	LOAD_INIT();

	/* Load and convert header */
	ifh.magic = hio_read32b(f);
	if (ifh.magic != MAGIC_IMPM) {
		return -1;
	}

	hio_read(ifh.name, 26, 1, f);
	ifh.hilite_min = hio_read8(f);
	ifh.hilite_maj = hio_read8(f);

	ifh.ordnum = hio_read16l(f);
	ifh.insnum = hio_read16l(f);
	ifh.smpnum = hio_read16l(f);
	ifh.patnum = hio_read16l(f);

	ifh.cwt = hio_read16l(f);
	ifh.cmwt = hio_read16l(f);
	ifh.flags = hio_read16l(f);
	ifh.special = hio_read16l(f);

	ifh.gv = hio_read8(f);
	ifh.mv = hio_read8(f);
	ifh.is = hio_read8(f);
	ifh.it = hio_read8(f);
	ifh.sep = hio_read8(f);
	ifh.pwd = hio_read8(f);

	/* Sanity check */
	if (ifh.gv > 0x80) {
		D_(D_CRIT "invalid gv (%u)", ifh.gv);
		goto err;
	}

	ifh.msglen = hio_read16l(f);
	ifh.msgofs = hio_read32l(f);
	ifh.rsvd = hio_read32l(f);

	hio_read(ifh.chpan, 64, 1, f);
	hio_read(ifh.chvol, 64, 1, f);

	if (hio_error(f)) {
		D_(D_CRIT "error reading IT header");
		goto err;
	}

	memcpy(mod->name, ifh.name, sizeof(ifh.name));
	/* sizeof(ifh.name) == 26, sizeof(mod->name) == 64. */
	mod->name[sizeof(ifh.name)] = '\0';
	mod->len = ifh.ordnum;
	mod->ins = ifh.insnum;
	mod->smp = ifh.smpnum;
	mod->pat = ifh.patnum;

	/* Sanity check */
	if (mod->ins > 255 || mod->smp > 255 || mod->pat > 255) {
		D_(D_CRIT "invalid ins (%u), smp (%u), or pat (%u)",
		   mod->ins, mod->smp, mod->pat);
		goto err;
	}

	if (mod->ins) {
		pp_ins = (uint32 *) calloc(4, mod->ins);
		if (pp_ins == NULL)
			goto err;
	} else {
		pp_ins = NULL;
	}

	pp_smp = (uint32 *) calloc(4, mod->smp);
	if (pp_smp == NULL)
		goto err2;

	pp_pat = (uint32 *) calloc(4, mod->pat);
	if (pp_pat == NULL)
		goto err3;

	mod->spd = ifh.is;
	mod->bpm = ifh.it;

	sample_mode = ~ifh.flags & IT_USE_INST;

	if (ifh.flags & IT_LINEAR_FREQ) {
		m->period_type = PERIOD_LINEAR;
	}

	for (i = 0; i < 64; i++) {
		struct xmp_channel *xxc = &mod->xxc[i];

		if (ifh.chpan[i] == 100) {	/* Surround -> center */
			xxc->flg |= XMP_CHANNEL_SURROUND;
		}

		if (ifh.chpan[i] & 0x80) {	/* Channel mute */
			xxc->flg |= XMP_CHANNEL_MUTE;
		}

		if (ifh.flags & IT_STEREO) {
			xxc->pan = (int)ifh.chpan[i] * 0x80 >> 5;
			if (xxc->pan > 0xff)
				xxc->pan = 0xff;
		} else {
			xxc->pan = 0x80;
		}

		xxc->vol = ifh.chvol[i];
	}

	if (mod->len <= XMP_MAX_MOD_LENGTH) {
		hio_read(mod->xxo, 1, mod->len, f);
	} else {
		hio_read(mod->xxo, 1, XMP_MAX_MOD_LENGTH, f);
		hio_seek(f, mod->len - XMP_MAX_MOD_LENGTH, SEEK_CUR);
		mod->len = XMP_MAX_MOD_LENGTH;
	}

	new_fx = ifh.flags & IT_OLD_FX ? 0 : 1;

	for (i = 0; i < mod->ins; i++)
		pp_ins[i] = hio_read32l(f);
	for (i = 0; i < mod->smp; i++)
		pp_smp[i] = hio_read32l(f);
	for (i = 0; i < mod->pat; i++)
		pp_pat[i] = hio_read32l(f);

	/* Skip edit history if it exists. */
	if (ifh.special & IT_EDIT_HISTORY) {
		int skip = hio_read16l(f) * 8;
		if (hio_error(f) || (skip && hio_seek(f, skip, SEEK_CUR) < 0))
			goto err4;
	}

	if ((ifh.flags & IT_MIDI_CONFIG) || (ifh.special & IT_SPEC_MIDICFG)) {
		if (load_it_midi_config(m, f) < 0)
			goto err4;
	}
	if (mod->smp && mod->pat && pp_pat[0] != 0 && pp_pat[0] < pp_smp[0])
		pat_before_smp = 1;

	m->c4rate = C4_NTSC_RATE;

	identify_tracker(m, &ifh, pat_before_smp, &is_mpt_116);

	MODULE_INFO();

	D_(D_INFO "Instrument/FX mode: %s/%s",
	   sample_mode ? "sample" : ifh.cmwt >= 0x200 ?
	   "new" : "old", ifh.flags & IT_OLD_FX ? "old" : "IT");

	if (sample_mode)
		mod->ins = mod->smp;

	if (libxmp_init_instrument(m) < 0)
		goto err4;

	D_(D_INFO "Instruments: %d", mod->ins);

	for (i = 0; i < mod->ins; i++) {
		/*
		 * IT files can have three different instrument types: 'New'
		 * instruments, 'old' instruments or just samples. We need a
		 * different loader for each of them.
		 */

		struct xmp_instrument *xxi = &mod->xxi[i];

		if (!sample_mode && ifh.cmwt >= 0x200) {
			/* New instrument format */
			if (hio_seek(f, start + pp_ins[i], SEEK_SET) < 0) {
				goto err4;
			}

			if (load_new_it_instrument(xxi, f) < 0) {
				goto err4;
			}

		} else if (!sample_mode) {
			/* Old instrument format */
			if (hio_seek(f, start + pp_ins[i], SEEK_SET) < 0) {
				goto err4;
			}

			if (load_old_it_instrument(xxi, f) < 0) {
				goto err4;
			}
		}
	}

	D_(D_INFO "Stored Samples: %d", mod->smp);

	/* This buffer should be able to hold any pattern or sample block.
	 * Round up to a multiple of 4--the sample decompressor relies on
	 * this to simplify its code.
	 */
	if ((patbuf = (uint8 *)malloc(TEMP_BUFFER_LEN)) == NULL) {
		D_(D_CRIT "failed to allocate temporary buffer");
		goto err4;
	}

	for (i = 0; i < mod->smp; i++) {

		if (hio_seek(f, start + pp_smp[i], SEEK_SET) < 0) {
			goto err4;
		}

		if (load_it_sample(m, i, start, sample_mode, patbuf, f) < 0) {
			goto err4;
		}
	}
	/* Reset any error status set by truncated samples. */
	hio_error(f);

	D_(D_INFO "Stored patterns: %d", mod->pat);

	/* Effects in muted channels are processed, so scan patterns first to
	 * see the real number of channels
	 */
	max_ch = 0;
	for (i = 0; i < mod->pat; i++) {
		uint8 mask[L_CHANNELS];
		int pat_len, num_rows, row;

		/* If the offset to a pattern is 0, the pattern is empty */
		if (pp_pat[i] == 0)
			continue;

		hio_seek(f, start + pp_pat[i], SEEK_SET);
		pat_len = hio_read16l(f) /* - 4 */ ;
		num_rows = hio_read16l(f);
		memset(mask, 0, L_CHANNELS);
		hio_read16l(f);
		hio_read16l(f);

		/* Sanity check:
		 * - Impulse Tracker and Schism Tracker allow up to 200 rows.
		 * - ModPlug Tracker 1.16 allows 256 rows.
		 * - OpenMPT allows 1024 rows.
		 */
		if (num_rows > 1024) {
			D_(D_WARN "skipping pattern %d (%d rows)", i, num_rows);
			pp_pat[i] = 0;
			continue;
		}

		if (hio_read(patbuf, 1, pat_len, f) < (size_t)pat_len) {
			D_(D_CRIT "error scanning pattern %d", i);
			goto err4;
		}
		pos = patbuf;

		row = 0;
		while (row < num_rows && --pat_len >= 0) {
			int b = *(pos++);
			if (b == 0) {
				row++;
				continue;
			}

			c = (b - 1) & 63;

			if (c > max_ch)
				max_ch = c;

			if (b & 0x80) {
				if (pat_len < 1) break;
				mask[c] = *(pos++);
				pat_len--;
			}

			if (mask[c] & 0x01) {
				pos++;
				pat_len--;
			}
			if (mask[c] & 0x02) {
				pos++;
				pat_len--;
			}
			if (mask[c] & 0x04) {
				pos++;
				pat_len--;
			}
			if (mask[c] & 0x08) {
				pos += 2;
				pat_len -= 2;
			}
		}
	}

	/* Set the number of channels actually used
	 */
	mod->chn = max_ch + 1;
	mod->trk = mod->pat * mod->chn;

	if (libxmp_init_pattern(mod) < 0) {
		goto err4;
	}

	/* Read patterns */
	for (i = 0; i < mod->pat; i++) {

		if (libxmp_alloc_pattern(mod, i) < 0) {
			goto err4;
		}

		/* If the offset to a pattern is 0, the pattern is empty */
		if (pp_pat[i] == 0) {
			mod->xxp[i]->rows = 64;
			for (j = 0; j < mod->chn; j++) {
				int tnum = i * mod->chn + j;
				if (libxmp_alloc_track(mod, tnum, 64) < 0)
					goto err4;
				mod->xxp[i]->index[j] = tnum;
			}
			continue;
		}

		if (hio_seek(f, start + pp_pat[i], SEEK_SET) < 0) {
			D_(D_CRIT "error seeking to %d", start + pp_pat[i]);
			goto err4;
		}

		if (load_it_pattern(m, i, new_fx, patbuf, f) < 0) {
			D_(D_CRIT "error loading pattern %d", i);
			goto err4;
		}
	}

	free(patbuf);
	free(pp_pat);
	free(pp_smp);
	free(pp_ins);

	/* Song message */
	if ((ifh.special & IT_HAS_MSG) && ifh.msglen > 0) {
		if ((m->comment = (char *)malloc(ifh.msglen)) != NULL) {
			hio_seek(f, start + ifh.msgofs, SEEK_SET);

			D_(D_INFO "Message length : %d", ifh.msglen);

			ifh.msglen = hio_read(m->comment, 1, ifh.msglen, f);
			hio_error(f); /* Clear error if any */

			for (j = 0; j + 1 < ifh.msglen; j++) {
				int b = m->comment[j];
				if (b == '\r') {
					m->comment[j] = '\n';
				} else if ((b < 32 || b > 127) && b != '\n'
					   && b != '\t') {
					m->comment[j] = '.';
				}
			}
			m->comment[j] = 0;
		}
	}

	/* Format quirks */

	m->quirk |= QUIRKS_IT | QUIRK_ARPMEM | QUIRK_INSVOL;

	if (ifh.flags & IT_LINK_GXX) {
		m->quirk |= QUIRK_PRENV;
	} else {
		m->quirk |= QUIRK_UNISLD;
	}

	if (new_fx) {
		m->quirk |= QUIRK_VIBHALF | QUIRK_VIBINV;
	} else {
		m->quirk &= ~QUIRK_VIBALL;
		m->quirk |= QUIRK_ITOLDFX;
	}

	if (sample_mode) {
		m->quirk &= ~(QUIRK_VIRTUAL | QUIRK_RSTCHN);
	}

	m->gvolbase = 0x80;
	m->gvol = ifh.gv;
	m->mvolbase = 48;
	m->mvol = ifh.mv;
	m->read_event_type = READ_EVENT_IT;

#ifndef LIBXMP_CORE_PLAYER
	if (is_mpt_116)
		libxmp_apply_mpt_preamp(m);
#endif

	return 0;

err4:
	free(patbuf);
	free(pp_pat);
err3:
	free(pp_smp);
err2:
	free(pp_ins);
err:
	return -1;
}

#endif /* LIBXMP_CORE_DISABLE_IT */
