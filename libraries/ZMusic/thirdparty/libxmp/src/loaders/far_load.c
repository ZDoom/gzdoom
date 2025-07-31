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

/* Based on the Farandole Composer format specifications by Daniel Potter.
 *
 * "(...) this format is for EDITING purposes (storing EVERYTHING you're
 * working on) so it may include information not completely neccessary."
 */

#include "loader.h"
#include "../far_extras.h"

struct far_header {
	uint32 magic;		/* File magic: 'FAR\xfe' */
	uint8 name[40];		/* Song name */
	uint8 crlf[3];		/* 0x0d 0x0a 0x1A */
	uint16 headersize;	/* Remaining header size in bytes */
	uint8 version;		/* Version MSN=major, LSN=minor */
	uint8 ch_on[16];	/* Channel on/off switches */
	uint8 rsvd1[9];		/* Current editing values */
	uint8 tempo;		/* Default tempo */
	uint8 pan[16];		/* Channel pan definitions */
	uint8 rsvd2[4];		/* Grid, mode (for editor) */
	uint16 textlen;		/* Length of embedded text */
};

struct far_header2 {
	uint8 order[256];	/* Orders */
	uint8 patterns;		/* Number of stored patterns (?) */
	uint8 songlen;		/* Song length in patterns */
	uint8 restart;		/* Restart pos */
	uint16 patsize[256];	/* Size of each pattern in bytes */
};

struct far_instrument {
	uint8 name[32];		/* Instrument name */
	uint32 length;		/* Length of sample (up to 64Kb) */
	uint8 finetune;		/* Finetune (unsuported) */
	uint8 volume;		/* Volume (unsuported?) */
	uint32 loop_start;	/* Loop start */
	uint32 loopend;		/* Loop end */
	uint8 sampletype;	/* 1=16 bit sample */
	uint8 loopmode;
};

struct far_event {
	uint8 note;
	uint8 instrument;
	uint8 volume;		/* In reverse nibble order? */
	uint8 effect;
};


#define MAGIC_FAR	MAGIC4('F','A','R',0xfe)


static int far_test (HIO_HANDLE *, char *, const int);
static int far_load (struct module_data *, HIO_HANDLE *, const int);

const struct format_loader libxmp_loader_far = {
    "Farandole Composer",
    far_test,
    far_load
};

static int far_test(HIO_HANDLE *f, char *t, const int start)
{
    if (hio_read32b(f) != MAGIC_FAR)
	return -1;

    libxmp_read_title(f, t, 40);

    return 0;
}


static void far_translate_effect(struct xmp_event *event, int fx, int param, int vol)
{
	switch (fx) {
	case 0x0:		/* 0x0?  Global funct */
		switch (param) {
		case 0x1:	/* 0x01  Ramp delay on */
		case 0x2:	/* 0x02  Ramp delay off */
			/* These control volume ramping and can be ignored. */
			break;
		case 0x3:	/* 0x03  Fulfill loop */
			/* This is intended to be sustain release, but the
			 * effect is buggy and just cuts most of the time. */
			event->fxt = FX_KEYOFF;
			break;
		case 0x4:	/* 0x04  Old FAR tempo */
			event->fxt = FX_FAR_TEMPO;
			event->fxp = 0x10;
			break;
		case 0x5:	/* 0x05  New FAR tempo */
			event->fxt = FX_FAR_TEMPO;
			event->fxp = 0x20;
			break;
		}
		break;
	case 0x1:		/* 0x1?  Pitch offset up */
		event->fxt = FX_FAR_PORTA_UP;
		event->fxp = param;
		break;
	case 0x2:		/* 0x2?  Pitch offset down */
		event->fxt = FX_FAR_PORTA_DN;
		event->fxp = param;
		break;
	case 0x3:		/* 0x3?  Note-port */
		event->fxt = FX_FAR_TPORTA;
		event->fxp = param;
		break;
	case 0x4:			/* 0x4?  Retrigger */
		event->fxt = FX_FAR_RETRIG;
		event->fxp = param;
		break;
	case 0x5:			/* 0x5?  Set Vibrato depth */
		event->fxt = FX_FAR_VIBDEPTH;
		event->fxp = param;
		break;
	case 0x6:			/* 0x6?  Vibrato note */
		event->fxt = FX_FAR_VIBRATO;
		event->fxp = param;
		break;
	case 0x7:			/* 0x7?  Vol Sld Up */
		event->fxt = FX_F_VSLIDE_UP;
		event->fxp = (param << 4);
		break;
	case 0x8:			/* 0x8?  Vol Sld Dn */
		event->fxt = FX_F_VSLIDE_DN;
		event->fxp = (param << 4);
		break;
	case 0x9:			/* 0x9?  Sustained vibrato */
		event->fxt = FX_FAR_VIBRATO;
		event->fxp = 0x10 /* Vibrato sustain flag */ | param;
		break;
	case 0xa:			/* 0xa?  Slide-to-vol */
		if (vol >= 0x01 && vol <= 0x10) {
			event->fxt = FX_FAR_SLIDEVOL;
			event->fxp = ((vol - 1) << 4) | param;
			event->vol = 0;
		}
		break;
	case 0xb:			/* 0xb?  Balance */
		event->fxt = FX_SETPAN;
		event->fxp = (param << 4) | param;
		break;
	case 0xc:			/* 0xc?  Note Offset */
		event->fxt = FX_FAR_DELAY;
		event->fxp = param;
		break;
	case 0xd:			/* 0xd?  Fine tempo down */
		event->fxt = FX_FAR_F_TEMPO;
		event->fxp = param;
		break;
	case 0xe:			/* 0xe?  Fine tempo up */
		event->fxt = FX_FAR_F_TEMPO;
		event->fxp = param << 4;
		break;
	case 0xf:			/* 0xf?  Set tempo */
		event->fxt = FX_FAR_TEMPO;
		event->fxp = param;
		break;
	}
}

#define COMMENT_MAXLINES 44

static void far_read_text(char *dest, size_t textlen, HIO_HANDLE *f)
{
	/* FAR module text uses 132-char lines with no line breaks... */
	size_t end, lastchar, i;

	if (textlen > COMMENT_MAXLINES * 132)
		textlen = COMMENT_MAXLINES * 132;

	while (textlen) {
		end = MIN(textlen, 132);
		textlen -= end;
		end = hio_read(dest, 1, end, f);

		lastchar = 0;
		for (i = 0; i < end; i++) {
			/* Nulls in the text area are equivalent to spaces. */
			if (dest[i] == '\0')
				dest[i] = ' ';
			else if (dest[i] != ' ')
				lastchar = i;
		}
		dest += lastchar + 1;
		*dest++ = '\n';
	}
	*dest = '\0';
}

static int far_load(struct module_data *m, HIO_HANDLE *f, const int start)
{
    struct xmp_module *mod = &m->mod;
    struct far_module_extras *me;
    int i, j, k;
    struct xmp_event *event;
    struct far_header ffh;
    struct far_header2 ffh2;
    struct far_instrument fih;
    uint8 *patbuf = NULL;
    uint8 sample_map[8];

    LOAD_INIT();

    hio_read32b(f);			/* File magic: 'FAR\xfe' */
    hio_read(ffh.name, 40, 1, f);	/* Song name */
    hio_read(ffh.crlf, 3, 1, f);	/* 0x0d 0x0a 0x1A */
    ffh.headersize = hio_read16l(f);	/* Remaining header size in bytes */
    ffh.version = hio_read8(f);		/* Version MSN=major, LSN=minor */
    hio_read(ffh.ch_on, 16, 1, f);	/* Channel on/off switches */
    hio_seek(f, 9, SEEK_CUR);		/* Current editing values */
    ffh.tempo = hio_read8(f);		/* Default tempo */
    hio_read(ffh.pan, 16, 1, f);	/* Channel pan definitions */
    hio_read32l(f);			/* Grid, mode (for editor) */
    ffh.textlen = hio_read16l(f);	/* Length of embedded text */

    /* Sanity check */
    if (ffh.tempo >= 16) {
	return -1;
    }

    if ((m->comment = (char *)malloc(ffh.textlen + COMMENT_MAXLINES + 1)) != NULL) {
	far_read_text(m->comment, ffh.textlen, f);
    } else {
	hio_seek(f, ffh.textlen, SEEK_CUR);	/* Skip song text */
    }

    hio_read(ffh2.order, 256, 1, f);	/* Orders */
    ffh2.patterns = hio_read8(f);	/* Number of stored patterns (?) */
    ffh2.songlen = hio_read8(f);	/* Song length in patterns */
    ffh2.restart = hio_read8(f);	/* Restart pos */
    for (i = 0; i < 256; i++) {
	ffh2.patsize[i] = hio_read16l(f); /* Size of each pattern in bytes */
    }

    if (hio_error(f)) {
        return -1;
    }

    /* Skip unsupported header extension if it exists. The documentation claims
     * this field is the "remaining" header size, but it's the total size. */
    if (ffh.headersize > 869 + ffh.textlen) {
	if (hio_seek(f, ffh.headersize, SEEK_SET))
	    return -1;
    }

    mod->chn = 16;
    /*mod->pat=ffh2.patterns; (Error in specs? --claudio) */
    mod->len = ffh2.songlen;
    mod->rst = ffh2.restart;
    memcpy (mod->xxo, ffh2.order, mod->len);

    for (mod->pat = i = 0; i < 256; i++) {
	if (ffh2.patsize[i])
	    mod->pat = i + 1;
    }
    /* Make sure referenced zero-sized patterns are also counted. */
    for (i = 0; i < mod->len; i++) {
	if (mod->pat <= mod->xxo[i])
	    mod->pat = mod->xxo[i] + 1;
    }

    mod->trk = mod->chn * mod->pat;

    if (libxmp_far_new_module_extras(m) != 0)
	return -1;

    me = FAR_MODULE_EXTRAS(*m);
    me->coarse_tempo = ffh.tempo;
    me->fine_tempo = 0;
    me->tempo_mode = 1;
    m->time_factor = FAR_TIME_FACTOR;
    libxmp_far_translate_tempo(1, 0, me->coarse_tempo, &me->fine_tempo, &mod->spd, &mod->bpm);

    m->period_type = PERIOD_CSPD;
    m->c4rate = C4_NTSC_RATE;

    m->quirk |= QUIRK_VSALL | QUIRK_PBALL | QUIRK_VIBALL;

    strncpy(mod->name, (char *)ffh.name, 40);
    libxmp_set_type(m, "Farandole Composer %d.%d", MSN(ffh.version), LSN(ffh.version));

    MODULE_INFO();

    if (libxmp_init_pattern(mod) < 0)
	return -1;

    /* Read and convert patterns */
    D_(D_INFO "Comment bytes  : %d", ffh.textlen);
    D_(D_INFO "Stored patterns: %d", mod->pat);

    if ((patbuf = (uint8 *)malloc(256 * 16 * 4)) == NULL)
	return -1;

    for (i = 0; i < mod->pat; i++) {
	uint8 brk, note, ins, vol, fxb;
	uint8 *pos;
	int rows;

	if (libxmp_alloc_pattern(mod, i) < 0)
	    goto err;

	if (!ffh2.patsize[i])
	    continue;

	rows = (ffh2.patsize[i] - 2) / 64;

	/* Sanity check */
	if (rows <= 0 || rows > 256) {
	    goto err;
	}

	mod->xxp[i]->rows = rows;

	if (libxmp_alloc_tracks_in_pattern(mod, i) < 0)
	    goto err;

	brk = hio_read8(f) + 1;
	hio_read8(f);

	if (hio_read(patbuf, rows * 64, 1, f) < 1) {
	    D_(D_CRIT "read error at pat %d", i);
	    goto err;
	}

	pos = patbuf;
	for (j = 0; j < mod->xxp[i]->rows; j++) {
	    for (k = 0; k < mod->chn; k++) {
		event = &EVENT(i, k, j);

		if (k == 0 && j == brk)
		    event->f2t = FX_BREAK;

		note = *pos++;
		ins  = *pos++;
		vol  = *pos++;
		fxb  = *pos++;

		if (note)
		    event->note = note + 48;
		if (event->note || ins)
		    event->ins = ins + 1;

		if (vol >= 0x01 && vol <= 0x10)
		    event->vol = (vol - 1) * 16 + 1;

		far_translate_effect(event, MSN(fxb), LSN(fxb), vol);
	    }
	}
    }
    free(patbuf);

    /* Allocate tracks for any patterns referenced with a size of 0. These
     * use the configured pattern break position, which is 64 by default. */
    for (i = 0; i < mod->len; i++) {
	int pat = mod->xxo[i];
	if (mod->xxp[pat]->rows == 0) {
	    mod->xxp[pat]->rows = 64;
	    if (libxmp_alloc_tracks_in_pattern(mod, pat) < 0)
		return -1;
	}
    }

    mod->ins = -1;
    if (hio_read(sample_map, 1, 8, f) < 8) {
	D_(D_CRIT "read error at sample map");
	return -1;
    }
    for (i = 0; i < 64; i++) {
	if (sample_map[i / 8] & (1 << (i % 8)))
		mod->ins = i;
    }
    mod->ins++;

    mod->smp = mod->ins;

    if (libxmp_init_instrument(m) < 0)
	return -1;

    /* Read and convert instruments and samples */

    for (i = 0; i < mod->ins; i++) {
	if (!(sample_map[i / 8] & (1 << (i % 8))))
		continue;

	if (libxmp_alloc_subinstrument(mod, i, 1) < 0)
	    return -1;

	hio_read(fih.name, 32, 1, f);	/* Instrument name */
	fih.length = hio_read32l(f);	/* Length of sample (up to 64Kb) */
	fih.finetune = hio_read8(f);	/* Finetune (unsuported) */
	fih.volume = hio_read8(f);	/* Volume (unsuported?) */
	fih.loop_start = hio_read32l(f);/* Loop start */
	fih.loopend = hio_read32l(f);	/* Loop end */
	fih.sampletype = hio_read8(f);	/* 1=16 bit sample */
	fih.loopmode = hio_read8(f);

	/* Sanity check */
	if (fih.length > 0x10000 || fih.loop_start > 0x10000 ||
            fih.loopend > 0x10000) {
		return -1;
	}

	mod->xxs[i].len = fih.length;
	mod->xxs[i].lps = fih.loop_start;
	mod->xxs[i].lpe = fih.loopend;
	mod->xxs[i].flg = 0;

	if (mod->xxs[i].len > 0)
		mod->xxi[i].nsm = 1;

	if (fih.sampletype != 0) {
		mod->xxs[i].flg |= XMP_SAMPLE_16BIT;
		mod->xxs[i].len >>= 1;
		mod->xxs[i].lps >>= 1;
		mod->xxs[i].lpe >>= 1;
	}

	mod->xxs[i].flg |= fih.loopmode ? XMP_SAMPLE_LOOP : 0;
	mod->xxi[i].sub[0].vol = 0xff; /* fih.volume; */
	mod->xxi[i].sub[0].sid = i;

	libxmp_instrument_name(mod, i, fih.name, 32);

	D_(D_INFO "[%2X] %-32.32s %04x %04x %04x %c V%02x",
		i, mod->xxi[i].name, mod->xxs[i].len, mod->xxs[i].lps,
		mod->xxs[i].lpe, fih.loopmode ? 'L' : ' ', mod->xxi[i].sub[0].vol);

	if (libxmp_load_sample(m, f, 0, &mod->xxs[i], NULL) < 0)
		return -1;
    }

    /* Panning map */
    for (i = 0; i < 16; i++) {
	if (ffh.ch_on[i] == 0)
	    mod->xxc[i].flg |= XMP_CHANNEL_MUTE;
	if (ffh.pan[i] < 0x10)
	    mod->xxc[i].pan = (ffh.pan[i] << 4) | ffh.pan[i];
    }

    m->volbase = 0xf0;

    return 0;

  err:
    free(patbuf);
    return -1;
}
