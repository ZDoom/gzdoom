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

/* Liquid Tracker module loader based on the format description written
 * by Nir Oren. Tested with Shell.liq sent by Adi Sapir.
 *
 * TODO:
 * - Vibrato, tremolo intensities wrong?
 * - Gxx allows values >64 :(
 * - MCx sets volume to 0
 * - MDx doesn't delay volume?
 * - MEx repeated tick 0s don't act like tick 0
 * - Nxx has some bizarre behavior that prevents it from working between
 *   patterns sometimes and sometimes causes notes to drop out?
 * - Pxx shares effect memory with Lxy, possibly other related awful things.
 * - LIQ portamento is 4x more fine than S3M-compatibility portamento?
 *   (i.e. LIQ-mode UF1 seems equivalent to S3M-mode UE1). LT's S3M loader
 *   renames notes down by two octaves which is possibly implicated here.
 */

#include "loader.h"
#include "../period.h"


struct liq_header {
	uint8 magic[14];	/* "Liquid Module:" */
	uint8 name[30];		/* ASCIIZ module name */
	uint8 author[20];	/* Author name */
	uint8 _0x1a;		/* 0x1a */
	uint8 tracker[20];	/* Tracker name */
	uint16 version;		/* Format version */
	uint16 speed;		/* Initial speed */
	uint16 bpm;		/* Initial bpm */
	uint16 low;		/* Lowest note (Amiga Period*4) */
	uint16 high;		/* Uppest note (Amiga Period*4) */
	uint16 chn;		/* Number of channels */
/*#define LIQ_FLAG_CUT_ON_LIMIT		0x01 */
#define LIQ_FLAG_SCREAM_TRACKER_COMPAT	0x02
	uint32 flags;		/* Module flags */
	uint16 pat;		/* Number of patterns saved */
	uint16 ins;		/* Number of instruments */
	uint16 len;		/* Module length */
	uint16 hdrsz;		/* Header size */
};

struct liq_instrument {
#if 0
	uint8 magic[4];		/* 'L', 'D', 'S', 'S' */
#endif
	uint16 version;		/* LDSS header version */
	uint8 name[30];		/* Instrument name */
	uint8 editor[20];	/* Generator name */
	uint8 author[20];	/* Author name */
	uint8 hw_id;		/* Hardware used to record the sample */
	uint32 length;		/* Sample length */
	uint32 loopstart;	/* Sample loop start */
	uint32 loopend;		/* Sample loop end */
	uint32 c2spd;		/* C2SPD */
	uint8 vol;		/* Volume */
#define LIQ_SAMPLE_16BIT	0x01
#define LIQ_SAMPLE_STEREO	0x02
#define LIQ_SAMPLE_SIGNED	0x04
	uint8 flags;		/* Flags */
	uint8 pan;		/* Pan */
	uint8 midi_ins;		/* General MIDI instrument */
	uint8 gvl;		/* Global volume */
	uint8 chord;		/* Chord type */
	uint16 hdrsz;		/* LDSS header size */
	uint16 comp;		/* Compression algorithm */
	uint32 crc;		/* CRC */
	uint8 midi_ch;		/* MIDI channel */
	uint8 loop_type;	/* -1 or 0: normal, 1: ping pong*/
	uint8 rsvd[10];		/* Reserved */
	uint8 filename[25];	/* DOS file name */
};

struct liq_pattern {
#if 0
	uint8 magic[4];		/* 'L', 'P', 0, 0 */
#endif
	uint8 name[30];		/* ASCIIZ pattern name */
	uint16 rows;		/* Number of rows */
	uint32 size;		/* Size of packed pattern */
	uint32 reserved;	/* Reserved */
};


static int liq_test (HIO_HANDLE *, char *, const int);
static int liq_load (struct module_data *, HIO_HANDLE *, const int);

const struct format_loader libxmp_loader_liq = {
    "Liquid Tracker",
    liq_test,
    liq_load
};

static int liq_test(HIO_HANDLE *f, char *t, const int start)
{
    char buf[15];

    if (hio_read(buf, 1, 14, f) < 14)
	return -1;

    if (memcmp(buf, "Liquid Module:", 14))
	return -1;

    libxmp_read_title(f, t, 30);

    return 0;
}


#define NONE 0xff


static const uint8 fx[25] = {
	FX_ARPEGGIO,
	FX_S3M_BPM,
	FX_BREAK,
	FX_PORTA_DN,
	NONE,
	FX_FINE_VIBRATO,
	FX_GLOBALVOL,
	NONE,
	NONE,
	FX_JUMP,
	NONE,
	FX_VOLSLIDE,
	FX_EXTENDED,
	FX_TONEPORTA,
	FX_OFFSET,
	FX_SETPAN,
	NONE,
	FX_RETRIG,
	FX_S3M_SPEED,
	FX_TREMOLO,
	FX_PORTA_UP,
	FX_VIBRATO,
	NONE,
	FX_TONE_VSLIDE,
	FX_VIBRA_VSLIDE
};


static void liq_translate_effect(struct xmp_event *e)
{
    uint8 h = MSN (e->fxp), l = LSN (e->fxp);

    if (e->fxt >= ARRAY_SIZE(fx)) {
	D_(D_WARN "invalid effect %#02x", e->fxt);
	e->fxt = e->fxp = 0;
	return;
    }

    switch (e->fxt = fx[e->fxt]) {
    case FX_GLOBALVOL:			/* Global volume (decimal) */
	l = (e->fxp >> 4) * 10 + (e->fxp & 0x0f);
	e->fxp = l;
	break;
    case FX_EXTENDED:			/* Extended effects */
	switch (h) {
	case 0x3:			/* Glissando */
	    e->fxp = l | (EX_GLISS << 4);
	    break;
	case 0x4:			/* Vibrato wave */
	    if ((l & 3) == 3)
		l--;
	    e->fxp = l | (EX_VIBRATO_WF << 4);
	    break;
	case 0x5:			/* Finetune */
	    e->fxp = l | (EX_FINETUNE << 4);
	    break;
	case 0x6:			/* Pattern loop */
	    e->fxp = l | (EX_PATTERN_LOOP << 4);
	    break;
	case 0x7:			/* Tremolo wave */
	    if ((l & 3) == 3)
		l--;
	    e->fxp = l | (EX_TREMOLO_WF << 4);
	    break;
	case 0xc:			/* Cut */
	    e->fxp = l | (EX_CUT << 4);
	    break;
	case 0xd:			/* Delay */
	    e->fxp = l | (EX_DELAY << 4);
	    break;
	case 0xe:			/* Pattern delay */
	    e->fxp = l | (EX_PATT_DELAY << 4);
	    break;
	default:			/* Ignore */
	    e->fxt = e->fxp = 0;
	    break;
	}
	break;
    case FX_SETPAN:			/* Pan control */
	l = (e->fxp >> 4) * 10 + (e->fxp & 0x0f);
	if (l == 70) {
	    /* TODO: if the effective value is 70, reset ALL channels to
	    * default pan positions. */
	    e->fxt = e->fxp = 0;
	} else if (l == 66) {
	    e->fxt = FX_SURROUND;
	    e->fxp = 1;
	} else if (l <= 64) {
	    e->fxp = l * 0xff / 64;
	} else {
	    e->fxt = e->fxp = 0;
	}
	break;
    case NONE:				/* No effect */
	e->fxt = e->fxp = 0;
	break;
    }
}

static int liq_translate_note(uint8 note, struct xmp_event *event)
{
    if (note == 0xfe) {
	event->note = XMP_KEY_OFF;
    } else {
	/* 1.00+ format documents claim a 9 octave range, but Liquid Tracker
	 * <=1.50 only allows the use of the first 7. 0.00 should be within
	 * the NO range of 5 octaves. Either way, they convert the same for
	 * now, so just ignore any note that libxmp can't handle. */
	if (note > 107) {
	    D_(D_CRIT "invalid note %d", note);
	    return -1;
	}
	if (note + 36 >= XMP_MAX_KEYS) {
	    D_(D_CRIT "note %d outside of libxmp range, ignoring", note);
	    return 0;
	}
	event->note = note + 1 + 36;
    }
    return 0;
}

static int decode_event(uint8 x1, struct xmp_event *event, HIO_HANDLE *f)
{
    uint8 x2 = 0;

    memset (event, 0, sizeof (struct xmp_event));

    if (x1 & 0x01) {
	x2 = hio_read8(f);
	if (liq_translate_note(x2, event) < 0)
	    return -1;
    }

    if (x1 & 0x02)
	event->ins = hio_read8(f) + 1;

    if (x1 & 0x04)
	event->vol = hio_read8(f) + 1;

    if (x1 & 0x08)
	event->fxt = hio_read8(f) - 'A';
    else
	event->fxt = NONE;

    if (x1 & 0x10)
	event->fxp = hio_read8(f);
    else
	event->fxp = 0xff;

    D_(D_INFO "  event: %02x %02x %02x %02x %02x",
	event->note, event->ins, event->vol, event->fxt, event->fxp);

    /* Sanity check */
    if (event->ins > 100 || event->vol > 65)
	return -1;

    liq_translate_effect(event);
    return 0;
}

static int liq_load(struct module_data *m, HIO_HANDLE *f, const int start)
{
    struct xmp_module *mod = &m->mod;
    int i;
    struct xmp_event *event = NULL;
    struct liq_header lh;
    struct liq_instrument li;
    struct liq_pattern lp;
    uint8 x1, x2;
    uint32 pmag;
    char tracker_name[21];
    int num_channels_stored;
    int num_orders_stored;

    LOAD_INIT();

    hio_read(lh.magic, 14, 1, f);
    hio_read(lh.name, 30, 1, f);
    hio_read(lh.author, 20, 1, f);
    hio_read8(f);
    hio_read(lh.tracker, 20, 1, f);

    lh.version = hio_read16l(f);
    lh.speed = hio_read16l(f);
    lh.bpm = hio_read16l(f);
    lh.low = hio_read16l(f);
    lh.high = hio_read16l(f);
    lh.chn = hio_read16l(f);
    lh.flags = hio_read32l(f);
    lh.pat = hio_read16l(f);
    lh.ins = hio_read16l(f);
    lh.len = hio_read16l(f);
    lh.hdrsz = hio_read16l(f);

    /* Sanity check */
    if (lh.chn > XMP_MAX_CHANNELS || lh.pat > 256 || lh.ins > 256) {
	return -1;
    }

    if ((lh.version >> 8) == 0) {
	lh.hdrsz = lh.len;
	lh.len = 0;
	/* Skip 3 of 5 undocumented bytes (already read 2). */
	hio_seek(f, 3, SEEK_CUR);
	num_channels_stored = 64;
	num_orders_stored = 256;
	if (lh.chn > 64)
	    return -1;
    } else {
	num_channels_stored = lh.chn;
	num_orders_stored = lh.len;
    }

    if (lh.len > 256) {
	return -1;
    }

    mod->spd = lh.speed;
    mod->bpm = MIN(lh.bpm, 255);
    mod->chn = lh.chn;
    mod->pat = lh.pat;
    mod->ins = mod->smp = lh.ins;
    mod->len = lh.len;
    mod->trk = mod->chn * mod->pat;

    m->quirk |= QUIRK_INSVOL;

    strncpy(mod->name, (char *)lh.name, 30);
    strncpy(tracker_name, (char *)lh.tracker, 20);
    /* strncpy(m->author, (char *)lh.author, 20); */
    tracker_name[20] = 0;
    for (i = 20; i >= 0; i--) {
	if (tracker_name[i] == 0x20)
	   tracker_name[i] = 0;
	if (tracker_name[i])
	   break;
    }
    snprintf(mod->type, XMP_NAME_SIZE, "%s LIQ %d.%02d",
		tracker_name, lh.version >> 8, lh.version & 0x00ff);

    for (i = 0; i < mod->chn; i++) {
	uint8 pan = hio_read8(f);

	if (pan < 64) {
	    pan <<= 2;
	} else if (pan == 64) {
	    pan = 0xff;
	} else if (pan == 66) {
	    pan = 0x80;
	    mod->xxc[i].flg |= XMP_CHANNEL_SURROUND;
	} else {
	    /* Sanity check */
	    D_(D_CRIT "invalid channel %d panning value %d", i, pan);
	    return -1;
	}

	mod->xxc[i].pan = pan;
    }
    if (i < num_channels_stored) {
	hio_seek(f, num_channels_stored - i, SEEK_CUR);
    }

    for (i = 0; i < mod->chn; i++) {
	mod->xxc[i].vol = hio_read8(f);
    }
    if (i < num_channels_stored) {
	hio_seek(f, num_channels_stored - i, SEEK_CUR);
    }

    hio_read(mod->xxo, 1, num_orders_stored, f);

    /* Skip 1.01 echo pools */
    hio_seek(f, start + lh.hdrsz, SEEK_SET);

    /* Version 0.00 doesn't store length. */
    if (lh.version < 0x100) {
	for (i = 0; i < 256; i++) {
	    if (mod->xxo[i] == 0xff)
		break;
	}
	mod->len = i;
    }

    m->c4rate = C4_NTSC_RATE;

    MODULE_INFO();

    if (libxmp_init_pattern(mod) < 0)
	return -1;

    /* Read and convert patterns */

    D_(D_INFO "Stored patterns: %d", mod->pat);

    x1 = x2 = 0;
    for (i = 0; i < mod->pat; i++) {
	int row, channel, count;

	if (libxmp_alloc_pattern(mod, i) < 0)
	    return -1;

	pmag = hio_read32b(f);
	/* In spite of the documentation's claims, a magic of !!!! doesn't
	 * do anything special here. Liquid Tracker expects a full pattern
	 * specification regardless of the magic and no modules use !!!!. */
	if (pmag != MAGIC4('L','P',0,0)) {
	    D_(D_CRIT "invalid pattern %d magic %08x", i, pmag);
	    return -1;
	}

	hio_read(lp.name, 30, 1, f);
	lp.rows = hio_read16l(f);
	lp.size = hio_read32l(f);
	lp.reserved = hio_read32l(f);

	/* Sanity check */
	if (lp.rows > 256) {
	    return -1;
	}

	D_(D_INFO "rows: %d  size: %d\n", lp.rows, lp.size);

	mod->xxp[i]->rows = lp.rows;
	libxmp_alloc_tracks_in_pattern(mod, i);

	row = 0;
	channel = 0;
	count = hio_tell(f);

/*
 * Packed pattern data is stored full Track after full Track from the left to
 * the right (all Intervals in Track and then going Track right). You should
 * expect 0C0h on any pattern end, and then your Unpacked Patterndata Pointer
 * should be equal to the value in offset [24h]; if it's not, you should exit
 * with an error.
 */

read_event:
	/* Sanity check */
	if (i >= mod->pat || channel >= mod->chn || row >= mod->xxp[i]->rows)
	    return -1;

	event = &EVENT(i, channel, row);

	if (x2) {
	    if (decode_event(x1, event, f) < 0)
		return -1;
	    x2--;
	    goto next_row;
	}

	x1 = hio_read8(f);

test_event:
	/* Sanity check */
	if (i >= mod->pat || channel >= mod->chn || row >= mod->xxp[i]->rows)
		return -1;

	event = &EVENT(i, channel, row);
	D_(D_INFO "* count=%ld chan=%d row=%d event=%02x",
				hio_tell(f) - count, channel, row, x1);

	switch (x1) {
	case 0xc0:			/* end of pattern */
	    D_(D_WARN "- end of pattern");
	    if (hio_tell(f) - count != lp.size)
		return -1;
	    goto next_pattern;
	case 0xe1:			/* skip channels */
	    x1 = hio_read8(f);
	    channel += x1;
	    D_(D_INFO "  [skip %d channels]", x1);
	    /* fall thru */
	case 0xa0:			/* next channel */
	    D_(D_INFO "  [next channel]");
	    channel++;
	    if (channel >= mod->chn) {
		D_(D_CRIT "uh-oh! bad channel number!");
		channel--;
	    }
	    row = -1;
	    goto next_row;
	case 0xe0:			/* skip rows */
	    x1 = hio_read8(f);
	    D_(D_INFO "  [skip %d rows]", x1);
	    row += x1;
	    /* fall thru */
	case 0x80:			/* next row */
	    D_(D_INFO "  [next row]");
	    goto next_row;
	}

	if (x1 > 0xc0 && x1 < 0xe0) {	/* packed data */
	    D_(D_INFO "  [packed data]");
	    if (decode_event(x1, event, f) < 0)
		return -1;
	    goto next_row;
	}

	if (x1 > 0xa0 && x1 < 0xc0) {	/* packed data repeat */
	    x2 = hio_read8(f);
	    D_(D_INFO "  [packed data - repeat %d times]", x2);
	    if (decode_event(x1, event, f) < 0)
		return -1;
	    goto next_row;
	}

	if (x1 > 0x80 && x1 < 0xa0) {	/* packed data repeat, keep note */
	    x2 = hio_read8(f);
	    D_(D_INFO "  [packed data - repeat %d times, keep note]", x2);
	    if (decode_event(x1, event, f) < 0)
		return -1;
	    while (x2) {
	        row++;

		/* Sanity check */
		if (row >= lp.rows)
		    return -1;

		memcpy(&EVENT(i, channel, row), event, sizeof (struct xmp_event));
		x2--;
	    }
	    goto next_row;
	}

	/* unpacked data */
	D_ (D_INFO "  [unpacked data]");
	if (liq_translate_note(x1, event) < 0)
	    return -1;

	x1 = hio_read8(f);
	if (x1 > 100) {
	    row++;
	    goto test_event;
	}
	if (x1 != 0xff)
	    event->ins = x1 + 1;

	x1 = hio_read8(f);
	if (x1 != 0xff)
	    event->vol = x1 + 1;

	x1 = hio_read8(f);
	if (x1 != 0xff)
	    event->fxt = x1 - 'A';

	x1 = hio_read8(f);
	event->fxp = x1;

	D_(D_INFO "  event: %02x %02x %02x %02x %02x\n",
	    event->note, event->ins, event->vol, event->fxt, event->fxp);

	/* Sanity check */
	if (event->ins > 100 || event->vol > 65)
	    return -1;

	liq_translate_effect(event);

next_row:
	row++;
	if (row >= mod->xxp[i]->rows) {
	    row = 0;
	    x2 = 0;
	    channel++;
	}

	/* Sanity check */
	if (channel >= mod->chn) {
	    channel = 0;
	}

	goto read_event;

next_pattern:
	;
    }

    /* Read and convert instruments */

    if (libxmp_init_instrument(m) < 0)
	return -1;

    D_(D_INFO "Instruments: %d", mod->ins);

    for (i = 0; i < mod->ins; i++) {
	struct xmp_instrument *xxi = &mod->xxi[i];
	struct xmp_subinstrument *sub;
	struct xmp_sample *xxs = &mod->xxs[i];
	uint32 ldss_magic;

	if (libxmp_alloc_subinstrument(mod, i, 1) < 0)
	    return -1;

	sub = &xxi->sub[0];

	ldss_magic = hio_read32b(f);
	if (ldss_magic == MAGIC4('?','?','?','?'))
	    continue;
	if (ldss_magic != MAGIC4('L','D','S','S')) {
	    D_(D_CRIT "invalid instrument %d magic %08x", i, ldss_magic);
	    return -1;
	}

	li.version = hio_read16l(f);
	hio_read(li.name, 30, 1, f);
	hio_read(li.editor, 20, 1, f);
	hio_read(li.author, 20, 1, f);
	li.hw_id = hio_read8(f);

	li.length = hio_read32l(f);
	li.loopstart = hio_read32l(f);
	li.loopend = hio_read32l(f);
	li.c2spd = hio_read32l(f);

	li.vol = hio_read8(f);
	li.flags = hio_read8(f);
	li.pan = hio_read8(f);
	li.midi_ins = hio_read8(f);
	li.gvl = hio_read8(f);
	li.chord = hio_read8(f);

	li.hdrsz = hio_read16l(f);
	li.comp = hio_read16l(f);
	li.crc = hio_read32l(f);

	li.midi_ch = hio_read8(f);
	li.loop_type = hio_read8(f);
	hio_read(li.rsvd, 1, 10, f);
	hio_read(li.filename, 25, 1, f);

	/* Sanity check */
	if (hio_error(f)) {
	    return -1;
	}

	xxi->nsm = !!(li.length);
	xxi->vol = 0x40;

	xxs->len = li.length;
	xxs->lps = li.loopstart;
	xxs->lpe = li.loopend;

	/* Note: LIQ_SAMPLE_SIGNED is ignored by Liquid Tracker 1.50;
	 * all samples are interpreted as signed. */
	if (li.flags & LIQ_SAMPLE_16BIT) {
	    xxs->flg |= XMP_SAMPLE_16BIT;
	    xxs->len >>= 1;
	    xxs->lps >>= 1;
	    xxs->lpe >>= 1;
	}
	/* Storage is the same as S3M i.e. left then right. The shareware
	 * version mixes stereo samples to mono, as stereo samples were
	 * listed as a registered feature (yet to be verified). */
	if (li.flags & LIQ_SAMPLE_STEREO) {
	    xxs->flg |= XMP_SAMPLE_STEREO;
	    xxs->len >>= 1;
	    xxs->lps >>= 1;
	    xxs->lpe >>= 1;
	}

	if (li.loopend > 0) {
	    xxs->flg |= XMP_SAMPLE_LOOP;
	    if (li.loop_type == 1) {
		xxs->flg |= XMP_SAMPLE_LOOP_BIDIR;
	    }
	}

	/* Global volume was added(?) in LDSS 1.01 and, like the channel
	 * volume, has a range of 0-64 with 32=100%. */
	if (li.version < 0x101) {
	    li.gvl = 0x20;
	}

	sub->vol = li.vol;
	sub->gvl = li.gvl;
	sub->pan = li.pan;
	sub->sid = i;

	libxmp_instrument_name(mod, i, li.name, 30);

	D_(D_INFO "[%2X] %-30.30s %05x%c%05x %05x %c%c%c %02x %02x %2d.%02d %5d",
		i, mod->xxi[i].name, mod->xxs[i].len,
		xxs->flg & XMP_SAMPLE_16BIT ? '+' : ' ', xxs->lps, xxs->lpe,
		xxs->flg & XMP_SAMPLE_STEREO ? 's' : ' ',
		xxs->flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
		xxs->flg & XMP_SAMPLE_LOOP_BIDIR ? 'B' : ' ', sub->vol, sub->gvl,
		li.version >> 8, li.version & 0xff, li.c2spd);

	libxmp_c2spd_to_note(li.c2spd, &sub->xpo, &sub->fin);
	hio_seek(f, li.hdrsz - 0x90, SEEK_CUR);

	if (xxs->len == 0)
	    continue;

	if (libxmp_load_sample(m, f, 0, xxs, NULL) < 0)
	    return -1;
    }

    m->quirk |= QUIRK_FINEFX | QUIRK_RTONCE;
    m->flow_mode = (lh.flags & LIQ_FLAG_SCREAM_TRACKER_COMPAT) ?
		   FLOW_MODE_LIQUID_COMPAT : FLOW_MODE_LIQUID;
    m->read_event_type = READ_EVENT_ST3;

    /* Channel volume and instrument global volume are both "normally" 32 and
     * can be increased to 64, effectively allowing per-channel and
     * per-instrument gain of 2x each. Simulate this by keeping the original
     * volumes while increasing mix volume by 2x each.
     *
     * The global volume effect also (unintentionally?) has gain functionality
     * in the sense that values >64 are not ignored. This isn't supported yet.
     */
    m->mvol = 48 * 2 * 2;
    m->mvolbase = 48;

    return 0;
}

