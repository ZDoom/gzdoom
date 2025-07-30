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

/* Based on the format description by FreeJack of The Elven Nation
 *
 * The loader recognizes four subformats:
 * - MAS_UTrack_V001: Ultra Tracker version < 1.4
 * - MAS_UTrack_V002: Ultra Tracker version 1.4
 * - MAS_UTrack_V003: Ultra Tracker version 1.5
 * - MAS_UTrack_V004: Ultra Tracker version 1.6
 */

#include "loader.h"
#include "../period.h"


static int ult_test (HIO_HANDLE *, char *, const int);
static int ult_load (struct module_data *, HIO_HANDLE *, const int);

const struct format_loader libxmp_loader_ult = {
    "Ultra Tracker",
    ult_test,
    ult_load
};

static int ult_test(HIO_HANDLE *f, char *t, const int start)
{
    char buf[15];

    if (hio_read(buf, 1, 15, f) < 15)
	return -1;

    if (memcmp(buf, "MAS_UTrack_V00", 14))
	return -1;

    if (buf[14] < '1' || buf[14] > '4')
	return -1;

    libxmp_read_title(f, t, 32);

    return 0;
}


struct ult_header {
    uint8 magic[15];		/* 'MAS_UTrack_V00x' */
    uint8 name[32];		/* Song name */
    uint8 msgsize;		/* ver < 1.4: zero */
};

struct ult_header2 {
    uint8 order[256];		/* Orders */
    uint8 channels;		/* Number of channels - 1 */
    uint8 patterns;		/* Number of patterns - 1 */
};

struct ult_instrument {
    uint8 name[32];		/* Instrument name */
    uint8 dosname[12];		/* DOS file name */
    uint32 loop_start;		/* Loop start */
    uint32 loopend;		/* Loop end */
    uint32 sizestart;		/* Sample size is sizeend - sizestart */
    uint32 sizeend;
    uint8 volume;		/* Volume (log; ver >= 1.4 linear) */
    uint8 bidiloop;		/* Sample loop flags */
    int16 finetune;		/* Finetune */
    uint16 c2spd;		/* C2 frequency */
};

struct ult_event {
    /* uint8 note; */
    uint8 ins;
    uint8 fxt;			/* MSN = fxt, LSN = f2t */
    uint8 f2p;			/* Secondary comes first -- little endian! */
    uint8 fxp;
};


static int ult_load(struct module_data *m, HIO_HANDLE *f, const int start)
{
    struct xmp_module *mod = &m->mod;
    int i, j, k, ver, cnt;
    struct xmp_event *event;
    struct ult_header ufh;
    struct ult_header2 ufh2;
    struct ult_instrument uih;
    struct ult_event ue;
    const char *verstr[4] = { "< 1.4", "1.4", "1.5", "1.6" };

    uint8 x8;

    LOAD_INIT();

    hio_read(ufh.magic, 15, 1, f);
    hio_read(ufh.name, 32, 1, f);
    ufh.msgsize = hio_read8(f);

    ver = ufh.magic[14] - '0';

    strncpy(mod->name, (char *)ufh.name, 32);
    mod->name[32] = '\0';
    libxmp_set_type(m, "Ultra Tracker %s ULT V%03d", verstr[ver - 1], ver);

    m->c4rate = C4_NTSC_RATE;

    MODULE_INFO();

    hio_seek(f, ufh.msgsize * 32, SEEK_CUR);

    mod->ins = mod->smp = hio_read8(f);
    /* mod->flg |= XXM_FLG_LINEAR; */

    /* Read and convert instruments */

    if (libxmp_init_instrument(m) < 0)
	return -1;

    D_(D_INFO "Instruments: %d", mod->ins);

    for (i = 0; i < mod->ins; i++) {
	if (libxmp_alloc_subinstrument(mod, i, 1) < 0)
	    return -1;

	hio_read(uih.name, 32, 1, f);
	hio_read(uih.dosname, 12, 1, f);
	uih.loop_start = hio_read32l(f);
	uih.loopend = hio_read32l(f);
	uih.sizestart = hio_read32l(f);
	uih.sizeend = hio_read32l(f);
	uih.volume = hio_read8(f);
	uih.bidiloop = hio_read8(f);
	uih.c2spd = (ver >= 4) ? hio_read16l(f) : 0; /* Incorrect in ult_form.txt */
	uih.finetune = hio_read16l(f);
	if (hio_error(f)) {
	    D_(D_CRIT "read error at instrument %d", i);
	    return -1;
	}

	/* Sanity check:
	 * "[SizeStart] seems to tell UT how to load the sample into the GUS's
	 * onboard memory." The maximum supported GUS RAM is 16 MB (PnP).
	 * Samples also can't cross 256k boundaries. In practice it seems like
	 * nothing ever goes over 1 MB, the maximum on most GUS cards.
	 */
	if (uih.sizestart > uih.sizeend || uih.sizeend > (16 << 20) ||
	    uih.sizeend - uih.sizestart > (256 << 10)) {
	    D_(D_CRIT "invalid sample %d sizestart/sizeend", i);
	    return -1;
	}

	mod->xxs[i].len = uih.sizeend - uih.sizestart;
	mod->xxs[i].lps = uih.loop_start;
	mod->xxs[i].lpe = uih.loopend;

	if (mod->xxs[i].len > 0)
	    mod->xxi[i].nsm = 1;

	/* BiDi Loop : (Bidirectional Loop)
	 *
	 * UT takes advantage of the Gus's ability to loop a sample in
	 * several different ways. By setting the Bidi Loop, the sample can
	 * be played forward or backwards, looped or not looped. The Bidi
	 * variable also tracks the sample resolution (8 or 16 bit).
	 *
	 * The following table shows the possible values of the Bidi Loop.
	 * Bidi = 0  : No looping, forward playback,  8bit sample
	 * Bidi = 4  : No Looping, forward playback, 16bit sample
	 * Bidi = 8  : Loop Sample, forward playback, 8bit sample
	 * Bidi = 12 : Loop Sample, forward playback, 16bit sample
	 * Bidi = 24 : Loop Sample, reverse playback 8bit sample
	 * Bidi = 28 : Loop Sample, reverse playback, 16bit sample
	 */

	/* Claudio's note: I'm ignoring reverse playback for samples */

	switch (uih.bidiloop) {
	case 20:		/* Type 20 is in seasons.ult */
	case 4:
	    mod->xxs[i].flg = XMP_SAMPLE_16BIT;
	    break;
	case 8:
	    mod->xxs[i].flg = XMP_SAMPLE_LOOP;
	    break;
	case 12:
	    mod->xxs[i].flg = XMP_SAMPLE_16BIT | XMP_SAMPLE_LOOP;
	    break;
	case 24:
	    mod->xxs[i].flg = XMP_SAMPLE_LOOP | XMP_SAMPLE_LOOP_REVERSE;
	    break;
	case 28:
	    mod->xxs[i].flg = XMP_SAMPLE_16BIT | XMP_SAMPLE_LOOP | XMP_SAMPLE_LOOP_REVERSE;
	    break;
	}

/* TODO: Add logarithmic volume support */
	mod->xxi[i].sub[0].vol = uih.volume;
	mod->xxi[i].sub[0].pan = 0x80;
	mod->xxi[i].sub[0].sid = i;

	libxmp_instrument_name(mod, i, uih.name, 24);

	D_(D_INFO "[%2X] %-32.32s %05x%c%05x %05x %c V%02x F%04x %5d",
		i, mod->xxi[i].name, mod->xxs[i].len,
		mod->xxs[i].flg & XMP_SAMPLE_16BIT ? '+' : ' ',
		mod->xxs[i].lps, mod->xxs[i].lpe,
		mod->xxs[i].flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
		mod->xxi[i].sub[0].vol, uih.finetune, uih.c2spd);

	if (ver > 3)
	    libxmp_c2spd_to_note(uih.c2spd, &mod->xxi[i].sub[0].xpo, &mod->xxi[i].sub[0].fin);
    }

    hio_read(ufh2.order, 256, 1, f);
    ufh2.channels = hio_read8(f);
    ufh2.patterns = hio_read8(f);

    if (hio_error(f)) {
	return -1;
    }

    for (i = 0; i < 256; i++) {
	if (ufh2.order[i] == 0xff)
	    break;
	mod->xxo[i] = ufh2.order[i];
    }
    mod->len = i;
    mod->chn = ufh2.channels + 1;
    mod->pat = ufh2.patterns + 1;
    mod->spd = 6;
    mod->bpm = 125;
    mod->trk = mod->chn * mod->pat;

    /* Sanity check */
    if (mod->chn > XMP_MAX_CHANNELS) {
	return -1;
    }

    for (i = 0; i < mod->chn; i++) {
	if (ver >= 3) {
	    x8 = hio_read8(f);
	    mod->xxc[i].pan = 255 * x8 / 15;
	} else {
	    mod->xxc[i].pan = DEFPAN((((i + 1) / 2) % 2) * 0xff); /* ??? */
	}
    }

    if (libxmp_init_pattern(mod) < 0)
	return -1;

    /* Read and convert patterns */

    D_(D_INFO "Stored patterns: %d", mod->pat);

    /* Events are stored by channel */
    for (i = 0; i < mod->pat; i++) {
	if (libxmp_alloc_pattern_tracks(mod, i, 64) < 0)
	    return -1;
    }

    for (i = 0; i < mod->chn; i++) {
	for (j = 0; j < 64 * mod->pat; ) {
	    cnt = 1;
	    x8 = hio_read8(f);		/* Read note or repeat code (0xfc) */
	    if (x8 == 0xfc) {
		cnt = hio_read8(f);		/* Read repeat count */
		x8 = hio_read8(f);		/* Read note */
	    }
	    if (hio_read(&ue, 1, 4, f) < 4) {	/* Read rest of the event */
		D_(D_CRIT "read error at channel %d pos %d", i, j);
		return -1;
	    }

	    if (cnt == 0)
		cnt++;

	    if (j + cnt > 64 * mod->pat) {
		D_(D_WARN "invalid track data packing");
		return -1;
	    }

	    for (k = 0; k < cnt; k++, j++) {
		event = &EVENT (j >> 6, i , j & 0x3f);
		memset(event, 0, sizeof (struct xmp_event));
		if (x8)
		    event->note = x8 + 36;
		event->ins = ue.ins;
		event->fxt = MSN (ue.fxt);
		event->f2t = LSN (ue.fxt);
		event->fxp = ue.fxp;
		event->f2p = ue.f2p;

		switch (event->fxt) {
		case 0x03:		/* Tone portamento */
		    event->fxt = FX_ULT_TPORTA;
		    break;
		case 0x05:		/* 'Special' effect */
		case 0x06:		/* Reserved */
		    event->fxt = event->fxp = 0;
		    break;
		case 0x0b:		/* Pan */
		    event->fxt = FX_SETPAN;
		    event->fxp <<= 4;
		    break;
		case 0x09:		/* Sample offset */
/* TODO: fine sample offset */
		    event->fxp <<= 2;
		    break;
		}

		switch (event->f2t) {
		case 0x03:		/* Tone portamento */
		    event->f2t = FX_ULT_TPORTA;
		    break;
		case 0x05:		/* 'Special' effect */
		case 0x06:		/* Reserved */
		    event->f2t = event->f2p = 0;
		    break;
		case 0x0b:		/* Pan */
		    event->f2t = FX_SETPAN;
		    event->f2p <<= 4;
		    break;
		case 0x09:		/* Sample offset */
/* TODO: fine sample offset */
		    event->f2p <<= 2;
		    break;
		}

	    }
	}
    }

    D_(D_INFO "Stored samples: %d", mod->smp);

    for (i = 0; i < mod->ins; i++) {
	if (!mod->xxs[i].len)
	    continue;
	if (libxmp_load_sample(m, f, 0, &mod->xxs[i], NULL) < 0)
	    return -1;
    }

    m->volbase = 0x100;

    return 0;
}

