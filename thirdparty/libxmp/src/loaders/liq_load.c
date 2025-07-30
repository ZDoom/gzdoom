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

/* Liquid Tracker module loader based on the format description written
 * by Nir Oren. Tested with Shell.liq sent by Adi Sapir.
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
	uint8 flags;		/* Flags */
	uint8 pan;		/* Pan */
	uint8 midi_ins;		/* General MIDI instrument */
	uint8 gvl;		/* Global volume */
	uint8 chord;		/* Chord type */
	uint16 hdrsz;		/* LDSS header size */
	uint16 comp;		/* Compression algorithm */
	uint32 crc;		/* CRC */
	uint8 midi_ch;		/* MIDI channel */
	uint8 rsvd[11];		/* Reserved */
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
	NONE,
	NONE,
	NONE,
	FX_JUMP,
	NONE,
	FX_VOLSLIDE,
	FX_EXTENDED,
	FX_TONEPORTA,
	FX_OFFSET,
	NONE,			/* FIXME: Pan */
	NONE,
	NONE, /*FX_MULTI_RETRIG,*/
	FX_S3M_SPEED,
	FX_TREMOLO,
	FX_PORTA_UP,
	FX_VIBRATO,
	NONE,
	FX_TONE_VSLIDE,
	FX_VIBRA_VSLIDE
};


/* Effect translation */
static void xlat_fx(int c, struct xmp_event *e)
{
    uint8 h = MSN (e->fxp), l = LSN (e->fxp);

    if (e->fxt >= ARRAY_SIZE(fx)) {
	D_(D_WARN "invalid effect %#02x", e->fxt);
	e->fxt = e->fxp = 0;
	return;
    }

    switch (e->fxt = fx[e->fxt]) {
    case FX_EXTENDED:			/* Extended effects */
	switch (h) {
	case 0x3:			/* Glissando */
	    e->fxp = l | (EX_GLISS << 4);
	    break;
	case 0x4:			/* Vibrato wave */
	    if (l == 3)
		l++;
	    e->fxp = l | (EX_VIBRATO_WF << 4);
	    break;
	case 0x5:			/* Finetune */
	    e->fxp = l | (EX_FINETUNE << 4);
	    break;
	case 0x6:			/* Pattern loop */
	    e->fxp = l | (EX_PATTERN_LOOP << 4);
	    break;
	case 0x7:			/* Tremolo wave */
	    if (l == 3)
		l++;
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
    case NONE:				/* No effect */
	e->fxt = e->fxp = 0;
	break;
    }
}


static int decode_event(uint8 x1, struct xmp_event *event, HIO_HANDLE *f)
{
    uint8 x2;

    memset (event, 0, sizeof (struct xmp_event));

    if (x1 & 0x01) {
	x2 = hio_read8(f);
	if (x2 == 0xfe)
	    event->note = XMP_KEY_OFF;
	else
	    event->note = x2 + 1 + 36;
    }

    if (x1 & 0x02)
	event->ins = hio_read8(f) + 1;

    if (x1 & 0x04)
	event->vol = hio_read8(f);

    if (x1 & 0x08)
	event->fxt = hio_read8(f) - 'A';

    if (x1 & 0x10)
	event->fxp = hio_read8(f);

    D_(D_INFO "  event: %02x %02x %02x %02x %02x",
	event->note, event->ins, event->vol, event->fxt, event->fxp);

    /* Sanity check */
    if (event->note > 107 && event->note != XMP_KEY_OFF)
	return -1;

    if (event->ins > 100 || event->vol > 64 || event->fxt > 26)
	return -1;

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
	hio_seek(f, -2, SEEK_CUR);
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

    if (lh.version > 0) {
	for (i = 0; i < mod->chn; i++) {
	    uint8 pan = hio_read8(f);

            if (pan >= 64) {
	        if (pan == 64) {
                    pan = 63;
                } else if (pan == 66) {
		    pan = 31;
                    mod->xxc[i].flg |= XMP_CHANNEL_SURROUND;
                } else {
                    /* Sanity check */
                    return -1;
                }
            }

	    mod->xxc[i].pan = pan << 2;
	}

	for (i = 0; i < mod->chn; i++)
	    mod->xxc[i].vol = hio_read8(f);

	hio_read(mod->xxo, 1, mod->len, f);

	/* Skip 1.01 echo pools */
	hio_seek(f, lh.hdrsz - (0x6d + mod->chn * 2 + mod->len), SEEK_CUR);
    } else {
	hio_seek(f, start + 0xf0, SEEK_SET);
	hio_read (mod->xxo, 1, 256, f);
	hio_seek(f, start + lh.hdrsz, SEEK_SET);

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
	if (pmag == 0x21212121)		/* !!!! */
	    continue;
	if (pmag != 0x4c500000)		/* LP\0\0 */
	    return -1;

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
	    xlat_fx (channel, event);
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
	    xlat_fx (channel, event);
	    goto next_row;
	}

	if (x1 > 0xa0 && x1 < 0xc0) {	/* packed data repeat */
	    x2 = hio_read8(f);
	    D_(D_INFO "  [packed data - repeat %d times]", x2);
	    if (decode_event(x1, event, f) < 0)
		return -1;
	    xlat_fx (channel, event);
	    goto next_row;
	}

	if (x1 > 0x80 && x1 < 0xa0) {	/* packed data repeat, keep note */
	    x2 = hio_read8(f);
	    D_(D_INFO "  [packed data - repeat %d times, keep note]", x2);
	    if (decode_event(x1, event, f) < 0)
		return -1;
	    xlat_fx (channel, event);
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
	if (x1 < 0xfe)
	    event->note = 1 + 36 + x1;
	else if (x1 == 0xfe)
	    event->note = XMP_KEY_OFF;

	x1 = hio_read8(f);
	if (x1 > 100) {
	    row++;
	    goto test_event;
	}
	if (x1 != 0xff)
	    event->ins = x1 + 1;

	x1 = hio_read8(f);
	if (x1 != 0xff)
	    event->vol = x1;

	x1 = hio_read8(f);
	if (x1 != 0xff)
	    event->fxt = x1 - 'A';

	x1 = hio_read8(f);
	event->fxp = x1;

	/* Sanity check */
	if (event->fxt > 24) {
		return -1;
	}

	xlat_fx(channel, event);

	D_(D_INFO "  event: %02x %02x %02x %02x %02x\n",
	    event->note, event->ins, event->vol, event->fxt, event->fxp);

	/* Sanity check */
	if (event->note > 119 && event->note != XMP_KEY_OFF)
		return -1;

	if (event->ins > 100 || event->vol > 65)
		return -1;

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
	unsigned char b[4];

	if (libxmp_alloc_subinstrument(mod, i, 1) < 0)
	    return -1;

	sub = &xxi->sub[0];

	if (hio_read(b, 1, 4, f) < 4)
	    return -1;

	if (b[0] == '?' && b[1] == '?' && b[2] == '?' && b[3] == '?')
	    continue;
	if (b[0] != 'L' || b[1] != 'D' || b[2] != 'S' || b[3] != 'S')
	    return -1;

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
	hio_read(li.rsvd, 11, 1, f);
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

	if (li.flags & 0x01) {
	    xxs->flg = XMP_SAMPLE_16BIT;
	    xxs->len >>= 1;
	    xxs->lps >>= 1;
	    xxs->lpe >>= 1;
	}

	if (li.loopend > 0)
	    xxs->flg = XMP_SAMPLE_LOOP;

	/* FIXME: LDSS 1.0 have global vol == 0 ? */
	/* if (li.gvl == 0) */
	li.gvl = 0x40;

	sub->vol = li.vol;
	sub->gvl = li.gvl;
	sub->pan = li.pan;
	sub->sid = i;

	libxmp_instrument_name(mod, i, li.name, 31);

	D_(D_INFO "[%2X] %-30.30s %05x%c%05x %05x %c %02x %02x %2d.%02d %5d",
		i, mod->xxi[i].name, mod->xxs[i].len,
		xxs->flg & XMP_SAMPLE_16BIT ? '+' : ' ', xxs->lps, xxs->lpe,
		xxs[i].flg & XMP_SAMPLE_LOOP ? 'L' : ' ', sub->vol, sub->gvl,
		li.version >> 8, li.version & 0xff, li.c2spd);

	libxmp_c2spd_to_note(li.c2spd, &sub->xpo, &sub->fin);
	hio_seek(f, li.hdrsz - 0x90, SEEK_CUR);

	if (xxs->len == 0)
	    continue;

	if (libxmp_load_sample(m, f, 0, xxs, NULL) < 0)
	    return -1;
    }

    m->quirk |= QUIRKS_ST3;
    m->read_event_type = READ_EVENT_ST3;

    return 0;
}

