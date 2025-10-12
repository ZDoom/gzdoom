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

/* Loader for Imago Orpheus modules based on the format description
 * written by Lutz Roeder.
 */

#include "loader.h"
#include "../period.h"


#define IMF_EOR		0x00
#define IMF_CH_MASK	0x1f
#define IMF_NI_FOLLOW	0x20
#define IMF_FX_FOLLOWS	0x80
#define IMF_F2_FOLLOWS	0x40

/* Sample flags */
#define IMF_SAMPLE_LOOP		0x01
#define IMF_SAMPLE_BIDI		0x02
#define IMF_SAMPLE_16BIT	0x04
#define IMF_SAMPLE_DEFPAN	0x08

struct imf_channel {
	char name[12];		/* Channelname (ASCIIZ-String, max 11 chars) */
	uint8 status;		/* Channel status */
	uint8 pan;		/* Pan positions */
	uint8 chorus;		/* Default chorus */
	uint8 reverb;		/* Default reverb */
};

struct imf_header {
	char name[32];		/* Songname (ASCIIZ-String, max. 31 chars) */
	uint16 len;		/* Number of orders saved */
	uint16 pat;		/* Number of patterns saved */
	uint16 ins;		/* Number of instruments saved */
	uint16 flg;		/* Module flags */
	uint8 unused1[8];
	uint8 tpo;		/* Default tempo (1..255) */
	uint8 bpm;		/* Default beats per minute (BPM) (32..255) */
	uint8 vol;		/* Default mastervolume (0..64) */
	uint8 amp;		/* Amplification factor (4..127) */
	uint8 unused2[8];
	uint32 magic;		/* 'IM10' */
	struct imf_channel chn[32];	/* Channel settings */
	uint8 pos[256];		/* Order list */
};

struct imf_env {
	uint8 npt;		/* Number of envelope points */
	uint8 sus;		/* Envelope sustain point */
	uint8 lps;		/* Envelope loop start point */
	uint8 lpe;		/* Envelope loop end point */
	uint8 flg;		/* Envelope flags */
	uint8 unused[3];
};

struct imf_instrument {
	char name[32];		/* Inst. name (ASCIIZ-String, max. 31 chars) */
	uint8 map[120];		/* Multisample settings */
	uint8 unused[8];
	uint16 vol_env[32];	/* Volume envelope settings */
	uint16 pan_env[32];	/* Pan envelope settings */
	uint16 pitch_env[32];	/* Pitch envelope settings */
	struct imf_env env[3];
	uint16 fadeout;		/* Fadeout rate (0...0FFFH) */
	uint16 nsm;		/* Number of samples in instrument */
	uint32 magic;		/* 'II10' */
};

struct imf_sample {
	char name[13];		/* Sample filename (12345678.ABC) */
	uint8 unused1[3];
	uint32 len;		/* Length */
	uint32 lps;		/* Loop start */
	uint32 lpe;		/* Loop end */
	uint32 rate;		/* Samplerate */
	uint8 vol;		/* Default volume (0..64) */
	uint8 pan;		/* Default pan (00h = Left / 80h = Middle) */
	uint8 unused2[14];
	uint8 flg;		/* Sample flags */
	uint8 unused3[5];
	uint16 ems;		/* Reserved for internal usage */
	uint32 dram;		/* Reserved for internal usage */
	uint32 magic;		/* 'IS10' */
};


#define MAGIC_IM10	MAGIC4('I','M','1','0')
#define MAGIC_II10	MAGIC4('I','I','1','0')
#define MAGIC_IS10	MAGIC4('I','S','1','0')
#define MAGIC_IW10	MAGIC4('I','W','1','0') /* leaving all behind.imf */

static int imf_test (HIO_HANDLE *, char *, const int);
static int imf_load (struct module_data *, HIO_HANDLE *, const int);

const struct format_loader libxmp_loader_imf = {
    "Imago Orpheus v1.0",
    imf_test,
    imf_load
};

static int imf_test(HIO_HANDLE *f, char *t, const int start)
{
    hio_seek(f, start + 60, SEEK_SET);
    if (hio_read32b(f) != MAGIC_IM10)
	return -1;

    hio_seek(f, start, SEEK_SET);
    libxmp_read_title(f, t, 32);

    return 0;
}

#define NONE 0xff
#define FX_IMF_FPORTA_UP 0xfe
#define FX_IMF_FPORTA_DN 0xfd


/* Effect conversion table */
static const uint8 fx[36] = {
	NONE,
	FX_S3M_SPEED,
	FX_S3M_BPM,
	FX_TONEPORTA,
	FX_TONE_VSLIDE,
	FX_VIBRATO,
	FX_VIBRA_VSLIDE,
	FX_FINE_VIBRATO,
	FX_TREMOLO,
	FX_S3M_ARPEGGIO,
	FX_SETPAN,
	FX_PANSLIDE,
	FX_VOLSET,
	FX_VOLSLIDE,
	FX_F_VSLIDE,
	FX_FINETUNE,
	FX_NSLIDE_UP,
	FX_NSLIDE_DN,
	FX_PORTA_UP,
	FX_PORTA_DN,
	FX_IMF_FPORTA_UP,
	FX_IMF_FPORTA_DN,
	FX_FLT_CUTOFF,
	FX_FLT_RESN,
	FX_OFFSET,
	NONE /* fine offset */,
	FX_KEYOFF,
	FX_MULTI_RETRIG,
	FX_TREMOR,
	FX_JUMP,
	FX_BREAK,
	FX_GLOBALVOL,
	FX_GVOL_SLIDE,
	FX_EXTENDED,
	FX_CHORUS,
	FX_REVERB
};


/* Effect translation */
static void xlat_fx (int c, uint8 *fxt, uint8 *fxp)
{
    uint8 h = MSN (*fxp), l = LSN (*fxp);

    if (*fxt >= ARRAY_SIZE(fx)) {
	D_(D_WARN "invalid effect %#02x", *fxt);
	*fxt = *fxp = 0;
	return;
    }

    switch (*fxt = fx[*fxt]) {
    case FX_IMF_FPORTA_UP:
	*fxt = FX_PORTA_UP;
	if (*fxp < 0x30)
	    *fxp = LSN (*fxp >> 2) | 0xe0;
	else
	    *fxp = LSN (*fxp >> 4) | 0xf0;
	break;
    case FX_IMF_FPORTA_DN:
	*fxt = FX_PORTA_DN;
	if (*fxp < 0x30)
	    *fxp = LSN (*fxp >> 2) | 0xe0;
	else
	    *fxp = LSN (*fxp >> 4) | 0xf0;
	break;
    case FX_EXTENDED:			/* Extended effects */
	switch (h) {
	case 0x1:			/* Set filter */
	case 0x2:			/* Undefined */
	case 0x4:			/* Undefined */
	case 0x6:			/* Undefined */
	case 0x7:			/* Undefined */
	case 0x9:			/* Undefined */
	case 0xe:			/* Ignore envelope */
	case 0xf:			/* Invert loop */
	    *fxp = *fxt = 0;
	    break;
	case 0x3:			/* Glissando */
	    *fxp = l | (EX_GLISS << 4);
	    break;
	case 0x5:			/* Vibrato waveform */
	    *fxp = l | (EX_VIBRATO_WF << 4);
	    break;
	case 0x8:			/* Tremolo waveform */
	    *fxp = l | (EX_TREMOLO_WF << 4);
	    break;
	case 0xa:			/* Pattern loop */
	    *fxp = l | (EX_PATTERN_LOOP << 4);
	    break;
	case 0xb:			/* Pattern delay */
	    *fxp = l | (EX_PATT_DELAY << 4);
	    break;
	case 0xc:
	    if (l == 0)
		*fxt = *fxp = 0;
	}
	break;
    case NONE:				/* No effect */
	*fxt = *fxp = 0;
	break;
    }
}


static int imf_load(struct module_data *m, HIO_HANDLE *f, const int start)
{
    struct xmp_module *mod = &m->mod;
    int c, r, i, j;
    struct xmp_event *event = 0, dummy;
    struct imf_header ih;
    struct imf_instrument ii;
    struct imf_sample is;
    int pat_len, smp_num;
    uint8 n, b;

    LOAD_INIT();

    /* Load and convert header */
    hio_read(ih.name, 32, 1, f);
    ih.len = hio_read16l(f);
    ih.pat = hio_read16l(f);
    ih.ins = hio_read16l(f);
    ih.flg = hio_read16l(f);
    hio_read(ih.unused1, 8, 1, f);
    ih.tpo = hio_read8(f);
    ih.bpm = hio_read8(f);
    ih.vol = hio_read8(f);
    ih.amp = hio_read8(f);
    hio_read(ih.unused2, 8, 1, f);
    ih.magic = hio_read32b(f);

    /* Sanity check */
    if (ih.len > 256 || ih.pat > 256 || ih.ins > 255) {
	return -1;
    }

    for (i = 0; i < 32; i++) {
	hio_read(ih.chn[i].name, 12, 1, f);
	ih.chn[i].chorus = hio_read8(f);
	ih.chn[i].reverb = hio_read8(f);
	ih.chn[i].pan = hio_read8(f);
	ih.chn[i].status = hio_read8(f);
    }

    if (hio_read(ih.pos, 256, 1, f) < 1) {
	D_(D_CRIT "read error at order list");
	return -1;
    }

    if (ih.magic != MAGIC_IM10) {
	return -1;
    }

    libxmp_copy_adjust(mod->name, (uint8 *)ih.name, 32);

    mod->len = ih.len;
    mod->ins = ih.ins;
    mod->smp = 1024;
    mod->pat = ih.pat;

    if (ih.flg & 0x01)
	m->period_type = PERIOD_LINEAR;

    mod->spd = ih.tpo;
    mod->bpm = ih.bpm;

    libxmp_set_type(m, "Imago Orpheus 1.0 IMF");

    MODULE_INFO();

    mod->chn = 0;
    for (i = 0; i < 32; i++) {
	/* 0=enabled; 1=muted, but still processed; 2=disabled.*/
	if (ih.chn[i].status >= 2)
	    continue;

	mod->chn = i + 1;
	mod->xxc[i].pan = ih.chn[i].pan;
#if 0
	/* FIXME */
	mod->xxc[i].cho = ih.chn[i].chorus;
	mod->xxc[i].rvb = ih.chn[i].reverb;
	mod->xxc[i].flg |= XMP_CHANNEL_FX;
#endif
    }

    mod->trk = mod->pat * mod->chn;

    memcpy(mod->xxo, ih.pos, mod->len);
    for (i = 0; i < mod->len; i++) {
	if (mod->xxo[i] == 0xff)
	    mod->xxo[i]--;
    }

    m->c4rate = C4_NTSC_RATE;

    if (libxmp_init_pattern(mod) < 0)
	return -1;

    /* Read patterns */

    D_(D_INFO "Stored patterns: %d", mod->pat);

    for (i = 0; i < mod->pat; i++) {
        int rows;

	pat_len = hio_read16l(f) - 4;

	rows = hio_read16l(f);

	/* Sanity check */
	if (rows > 256) {
	    return -1;
	}

	if (libxmp_alloc_pattern_tracks(mod, i, rows) < 0)
	    return -1;

	r = 0;

	while (--pat_len >= 0) {
	    b = hio_read8(f);

	    if (b == IMF_EOR) {
		r++;
		continue;
	    }

	    /* Sanity check */
	    if (r >= rows) {
		return -1;
	    }

	    c = b & IMF_CH_MASK;
	    event = c >= mod->chn ? &dummy : &EVENT(i, c, r);

	    if (b & IMF_NI_FOLLOW) {
		n = hio_read8(f);
		switch (n) {
		case 255:
		case 160:	/* ?! */
		    n = XMP_KEY_OFF;
		    break;	/* Key off */
		default:
		    n = 13 + 12 * MSN (n) + LSN (n);
		}

		event->note = n;
		event->ins = hio_read8(f);
		pat_len -= 2;
	    }
	    if (b & IMF_FX_FOLLOWS) {
		event->fxt = hio_read8(f);
		event->fxp = hio_read8(f);
		xlat_fx(c, &event->fxt, &event->fxp);
		pat_len -= 2;
	    }
	    if (b & IMF_F2_FOLLOWS) {
		event->f2t = hio_read8(f);
		event->f2p = hio_read8(f);
		xlat_fx(c, &event->f2t, &event->f2p);
		pat_len -= 2;
	    }
	}
    }

    if (libxmp_init_instrument(m) < 0)
	return -1;

    /* Read and convert instruments and samples */

    D_(D_INFO "Instruments: %d", mod->ins);

    for (smp_num = i = 0; i < mod->ins; i++) {
	struct xmp_instrument *xxi = &mod->xxi[i];

	hio_read(ii.name, 32, 1, f);
	ii.name[31] = 0;
	hio_read(ii.map, 120, 1, f);
	hio_read(ii.unused, 8, 1, f);
	for (j = 0; j < 32; j++)
		ii.vol_env[j] = hio_read16l(f);
	for (j = 0; j < 32; j++)
		ii.pan_env[j] = hio_read16l(f);
	for (j = 0; j < 32; j++)
		ii.pitch_env[j] = hio_read16l(f);
	for (j = 0; j < 3; j++) {
	    ii.env[j].npt = hio_read8(f);
	    ii.env[j].sus = hio_read8(f);
	    ii.env[j].lps = hio_read8(f);
	    ii.env[j].lpe = hio_read8(f);
	    ii.env[j].flg = hio_read8(f);
	    hio_read(ii.env[j].unused, 3, 1, f);
	}
	ii.fadeout = hio_read16l(f);
	ii.nsm = hio_read16l(f);
	ii.magic = hio_read32b(f);

	/* Sanity check */
	if (ii.nsm > 255)
	    return -1;

	/* Imago Orpheus may emit blank instruments with a signature
	 * of four nuls. Found in "leaving all behind.imf" by Karsten Koch. */
	if (ii.magic != MAGIC_II10 && ii.magic != 0) {
	    D_(D_CRIT "unknown instrument %d magic %08x @ %ld", i,
	       ii.magic, hio_tell(f));
	    return -2;
	}

	xxi->nsm = ii.nsm;

	if (xxi->nsm > 0) {
	    if (libxmp_alloc_subinstrument(mod, i, xxi->nsm) < 0)
		return -1;
	}

	strncpy((char *)xxi->name, ii.name, 31);
	xxi->name[31] = '\0';

	for (j = 0; j < 108; j++) {
		xxi->map[j + 12].ins = ii.map[j];
	}

	D_(D_INFO "[%2X] %-31.31s %2d %4x %c", i, ii.name, ii.nsm,
		ii.fadeout, ii.env[0].flg & 0x01 ? 'V' : '-');

	xxi->aei.npt = ii.env[0].npt;
	xxi->aei.sus = ii.env[0].sus;
	xxi->aei.lps = ii.env[0].lps;
	xxi->aei.lpe = ii.env[0].lpe;
	xxi->aei.flg = ii.env[0].flg & 0x01 ? XMP_ENVELOPE_ON : 0;
	xxi->aei.flg |= ii.env[0].flg & 0x02 ? XMP_ENVELOPE_SUS : 0;
	xxi->aei.flg |= ii.env[0].flg & 0x04 ?  XMP_ENVELOPE_LOOP : 0;

	/* Sanity check */
	if (xxi->aei.npt > 16) {
	    return -1;
	}

	for (j = 0; j < xxi->aei.npt; j++) {
	    xxi->aei.data[j * 2] = ii.vol_env[j * 2];
	    xxi->aei.data[j * 2 + 1] = ii.vol_env[j * 2 + 1];
	}

	for (j = 0; j < ii.nsm; j++, smp_num++) {
	    struct xmp_subinstrument *sub = &xxi->sub[j];
	    struct xmp_sample *xxs = &mod->xxs[smp_num];
	    int sid;

	    hio_read(is.name, 13, 1, f);
	    hio_read(is.unused1, 3, 1, f);
	    is.len = hio_read32l(f);
	    is.lps = hio_read32l(f);
	    is.lpe = hio_read32l(f);
	    is.rate = hio_read32l(f);
	    is.vol = hio_read8(f);
	    is.pan = hio_read8(f);
	    hio_read(is.unused2, 14, 1, f);
	    is.flg = hio_read8(f);
	    hio_read(is.unused3, 5, 1, f);
	    is.ems = hio_read16l(f);
	    is.dram = hio_read32l(f);
	    is.magic = hio_read32b(f);

	    if (is.magic != MAGIC_IS10 && is.magic != MAGIC_IW10) {
		D_(D_CRIT "unknown sample %d:%d magic %08x @ %ld", i, j,
		   is.magic, hio_tell(f));
		return -1;
	    }

	    /* Sanity check */
	    if (is.len > 0x100000 || is.lps > 0x100000 || is.lpe > 0x100000)
		return -1;

	    sub->sid = smp_num;
	    sub->vol = is.vol;
	    sub->pan = (is.flg & IMF_SAMPLE_DEFPAN) ? is.pan : -1;
	    xxs->len = is.len;
	    xxs->lps = is.lps;
	    xxs->lpe = is.lpe;
	    xxs->flg = 0;

	    if (is.flg & IMF_SAMPLE_LOOP) {
		xxs->flg |= XMP_SAMPLE_LOOP;
	    }
	    if (is.flg & IMF_SAMPLE_BIDI) {
		xxs->flg |= XMP_SAMPLE_LOOP_BIDIR;
	    }
	    if (is.flg & IMF_SAMPLE_16BIT) {
	        xxs->flg |= XMP_SAMPLE_16BIT;
	        xxs->len >>= 1;
	        xxs->lps >>= 1;
	        xxs->lpe >>= 1;
	    }

	    D_(D_INFO "  %02x: %05x %05x %05x %5d %c%c%c%c %2s",
		    j, is.len, is.lps, is.lpe, is.rate,
		    (is.flg & IMF_SAMPLE_LOOP)   ? 'L' : '.',
		    (is.flg & IMF_SAMPLE_BIDI)   ? 'B' : '.',
		    (is.flg & IMF_SAMPLE_16BIT)  ? '+' : '.',
		    (is.flg & IMF_SAMPLE_DEFPAN) ? 'P' : '.',
		    (is.magic == MAGIC_IS10) ? "IS" : "IW");

	    libxmp_c2spd_to_note(is.rate, &sub->xpo, &sub->fin);

	    if (xxs->len <= 0)
		continue;

	    sid = sub->sid;
	    if (libxmp_load_sample(m, f, 0, &mod->xxs[sid], NULL) < 0)
		return -1;
	}
    }

    mod->smp = smp_num;
    mod->xxs = (struct xmp_sample *) realloc(mod->xxs, sizeof(struct xmp_sample) * mod->smp);
    if (mod->xxs == NULL) {
        return -1;
    }
    m->xtra = (struct extra_sample_data *) realloc(m->xtra, sizeof(struct extra_sample_data) * mod->smp);
    if (m->xtra == NULL) {
        return -1;
    }

    m->c4rate = C4_NTSC_RATE;
    m->quirk |= QUIRK_FILTER | QUIRKS_ST3 | QUIRK_ARPMEM;
    m->flow_mode = FLOW_MODE_ORPHEUS;
    m->read_event_type = READ_EVENT_ST3;

    m->gvol = ih.vol;
    m->mvol = ih.amp;
    m->mvolbase = 48;
    CLAMP(m->mvol, 4, 127);

    return 0;
}
