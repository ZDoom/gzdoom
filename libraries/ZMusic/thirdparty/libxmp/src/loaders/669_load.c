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


static int c669_test (HIO_HANDLE *, char *, const int);
static int c669_load (struct module_data *, HIO_HANDLE *, const int);

const struct format_loader libxmp_loader_669 = {
    "Composer 669",
    c669_test,
    c669_load
};

static int c669_test(HIO_HANDLE *f, char *t, const int start)
{
    uint16 id;

    id = hio_read16b(f);
    if (id != 0x6966 && id != 0x4a4e)
	return -1;

    hio_seek(f, 110, SEEK_SET);
    if (hio_read8(f) > 64)
	return -1;
    if (hio_read8(f) > 128)
	return -1;

    hio_seek(f, 240, SEEK_SET);
    if (hio_read8(f) != 0xff)
	return -1;

    hio_seek(f, start + 2, SEEK_SET);
    libxmp_read_title(f, t, 36);

    return 0;
}


struct c669_file_header {
    uint8 marker[2];		/* 'if'=standard, 'JN'=extended */
    uint8 message[108];		/* Song message */
    uint8 nos;			/* Number of samples (0-64) */
    uint8 nop;			/* Number of patterns (0-128) */
    uint8 loop;			/* Loop order number */
    uint8 order[128];		/* Order list */
    uint8 speed[128];		/* Tempo list for patterns */
    uint8 pbrk[128];		/* Break list for patterns */
};

struct c669_instrument_header {
    uint8 name[13];		/* ASCIIZ instrument name */
    uint32 length;		/* Instrument length */
    uint32 loop_start;		/* Instrument loop start */
    uint32 loopend;		/* Instrument loop end */
};


#define NONE 0xff

/* Effects bug fixed by Miod Vallat <miodrag@multimania.com> */

static const uint8 fx[6] = {
    FX_669_PORTA_UP,
    FX_669_PORTA_DN,
    FX_669_TPORTA,
    FX_669_FINETUNE,
    FX_669_VIBRATO,
    FX_SPEED_CP
};


static int c669_load(struct module_data *m, HIO_HANDLE *f, const int start)
{
    struct xmp_module *mod = &m->mod;
    int i, j;
    struct xmp_event *event;
    struct c669_file_header sfh;
    struct c669_instrument_header sih;
    uint8 ev[3];

    LOAD_INIT();

    hio_read(sfh.marker, 2, 1, f);	/* 'if'=standard, 'JN'=extended */
    hio_read(sfh.message, 108, 1, f);	/* Song message */
    sfh.nos = hio_read8(f);		/* Number of samples (0-64) */
    sfh.nop = hio_read8(f);		/* Number of patterns (0-128) */

    /* Sanity check */
    if (sfh.nos > 64 || sfh.nop > 128)
	return -1;

    sfh.loop = hio_read8(f);		/* Loop order number */
    if (hio_read(sfh.order, 1, 128, f) != 128)	/* Order list */
	return -1;
    if (hio_read(sfh.speed, 1, 128, f) != 128)	/* Tempo list for patterns */
	return -1;
    if (hio_read(sfh.pbrk, 1, 128, f) != 128) 	/* Break list for patterns */
	return -1;

    mod->chn = 8;
    mod->ins = sfh.nos;
    mod->pat = sfh.nop;
    mod->trk = mod->chn * mod->pat;
    for (i = 0; i < 128; i++) {
	if (sfh.order[i] > sfh.nop)
	    break;
    }
    mod->len = i;
    memcpy (mod->xxo, sfh.order, mod->len);
    mod->spd = 6;
    mod->bpm = 78;
    mod->smp = mod->ins;

    m->period_type = PERIOD_CSPD;
    m->c4rate = C4_NTSC_RATE;

    libxmp_copy_adjust(mod->name, sfh.message, 36);
    libxmp_set_type(m, strncmp((char *)sfh.marker, "if", 2) ?
				"UNIS 669" : "Composer 669");

    MODULE_INFO();

    m->comment = (char *) malloc(109);
    memcpy(m->comment, sfh.message, 108);
    m->comment[108] = 0;

    /* Read and convert instruments and samples */

    if (libxmp_init_instrument(m) < 0)
	return -1;

    D_(D_INFO "Instruments: %d", mod->pat);

    for (i = 0; i < mod->ins; i++) {
	struct xmp_instrument *xxi = &mod->xxi[i];
	struct xmp_sample *xxs = &mod->xxs[i];
	struct xmp_subinstrument *sub;

	if (libxmp_alloc_subinstrument(mod, i, 1) < 0)
	    return -1;

	sub = &xxi->sub[0];

	hio_read (sih.name, 13, 1, f);		/* ASCIIZ instrument name */
	sih.length = hio_read32l(f);		/* Instrument size */
	sih.loop_start = hio_read32l(f);	/* Instrument loop start */
	sih.loopend = hio_read32l(f);		/* Instrument loop end */

	/* Sanity check */
	if (sih.length > MAX_SAMPLE_SIZE)
	    return -1;

	xxs->len = sih.length;
	xxs->lps = sih.loop_start;
	xxs->lpe = sih.loopend >= 0xfffff ? 0 : sih.loopend;
	xxs->flg = xxs->lpe ? XMP_SAMPLE_LOOP : 0;	/* 1 == Forward loop */

	sub->vol = 0x40;
	sub->pan = 0x80;
	sub->sid = i;

	if (xxs->len > 0)
		xxi->nsm = 1;

	libxmp_instrument_name(mod, i, sih.name, 13);

	D_(D_INFO "[%2X] %-14.14s %04x %04x %04x %c", i,
		xxi->name, xxs->len, xxs->lps, xxs->lpe,
		xxs->flg & XMP_SAMPLE_LOOP ? 'L' : ' ');
    }

    if (libxmp_init_pattern(mod) < 0)
	return -1;

    /* Read and convert patterns */
    D_(D_INFO "Stored patterns: %d", mod->pat);
    for (i = 0; i < mod->pat; i++) {
	int pbrk;

	if (libxmp_alloc_pattern_tracks(mod, i, 64) < 0)
	    return -1;

	event = &EVENT(i, 0, 0);
	event->f2t = FX_SPEED_CP;
	event->f2p = sfh.speed[i];

	pbrk = sfh.pbrk[i];
	if (pbrk >= 64)
	    return -1;

	event = &EVENT(i, 1, pbrk);
	event->f2t = FX_BREAK;
	event->f2p = 0;

	for (j = 0; j < 64 * 8; j++) {
	    event = &EVENT(i, j % 8, j / 8);
	    if(hio_read(ev, 1, 3, f) < 3) {
		D_(D_CRIT "read error at pat %d", i);
		return -1;
	    }

	    if ((ev[0] & 0xfe) != 0xfe) {
		event->note = 1 + 36 + (ev[0] >> 2);
		event->ins = 1 + MSN(ev[1]) + ((ev[0] & 0x03) << 4);
	    }

	    if (ev[0] != 0xff)
		event->vol = (LSN(ev[1]) << 2) + 1;

	    if (ev[2] != 0xff) {
		if (MSN(ev[2]) >= ARRAY_SIZE(fx))
		    continue;

		event->fxt = fx[MSN(ev[2])];
		event->fxp = LSN(ev[2]);

		if (event->fxt == FX_SPEED_CP) {
		    event->f2t = FX_PER_CANCEL;
		}
	    }
	}
    }

    /* Read samples */
    D_(D_INFO "Stored samples: %d", mod->smp);

    for (i = 0; i < mod->ins; i++) {
	if (mod->xxs[i].len <= 2)
	    continue;
	if (libxmp_load_sample(m, f, SAMPLE_FLAG_UNS, &mod->xxs[i], NULL) < 0)
	    return -1;
    }

    for (i = 0; i < mod->chn; i++) {
	mod->xxc[i].pan = DEFPAN((i % 2) * 0xff);
    }

    m->quirk |= QUIRK_PBALL|QUIRK_PERPAT;

    return 0;
}
