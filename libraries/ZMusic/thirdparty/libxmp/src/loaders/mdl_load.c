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

/* Note: envelope switching (effect 9) and sample status change (effect 8)
 * not supported.
 */

#include "loader.h"
#include "iff.h"
#include "../period.h"

#define MAGIC_DMDL	MAGIC4('D','M','D','L')


static int mdl_test (HIO_HANDLE *, char *, const int);
static int mdl_load (struct module_data *, HIO_HANDLE *, const int);

const struct format_loader libxmp_loader_mdl = {
    "Digitrakker",
    mdl_test,
    mdl_load
};

static int mdl_test(HIO_HANDLE *f, char *t, const int start)
{
    uint16 id;

    if (hio_read32b(f) != MAGIC_DMDL)
	return -1;

    hio_read8(f);		/* version */
    id = hio_read16b(f);

    if (id == 0x494e) {		/* IN */
	hio_read32b(f);
	libxmp_read_title(f, t, 32);
    } else {
	libxmp_read_title(f, t, 0);
    }

    return 0;
}


#define MDL_NOTE_FOLLOWS	0x04
#define MDL_INSTRUMENT_FOLLOWS	0x08
#define MDL_VOLUME_FOLLOWS	0x10
#define MDL_EFFECT_FOLLOWS	0x20
#define MDL_PARAMETER1_FOLLOWS	0x40
#define MDL_PARAMETER2_FOLLOWS	0x80


struct mdl_envelope {
    uint8 num;
    uint8 data[30];
    uint8 sus;
    uint8 loop;
};

struct local_data {
    int *i_index;
    int *s_index;
    int *v_index;	/* volume envelope */
    int *p_index;	/* pan envelope */
    int *f_index;	/* pitch envelope */
    int *packinfo;
    int has_in;
    int has_pa;
    int has_tr;
    int has_ii;
    int has_is;
    int has_sa;
    int v_envnum;
    int p_envnum;
    int f_envnum;
    struct mdl_envelope *v_env;
    struct mdl_envelope *p_env;
    struct mdl_envelope *f_env;
};


static void fix_env(int i, struct xmp_envelope *ei, struct mdl_envelope *env,
		    int *idx, int envnum)
{
    int j, k, lastx;

    if (idx[i] >= 0) {
	ei->flg = XMP_ENVELOPE_ON;
	ei->npt = 15;

	for (j = 0; j < envnum; j++) {
	    if (idx[i] == env[j].num) {
	        ei->flg |= env[j].sus & 0x10 ? XMP_ENVELOPE_SUS : 0;
	        ei->flg |= env[j].sus & 0x20 ? XMP_ENVELOPE_LOOP : 0;
	        ei->sus = env[j].sus & 0x0f;
	        ei->lps = env[j].loop & 0x0f;
	        ei->lpe = env[j].loop & 0xf0;

		lastx = -1;

		for (k = 0; k < ei->npt; k++) {
		    int x = env[j].data[k * 2];

		    if (x == 0)
			break;
		    ei->data[k * 2] = lastx + x;
		    ei->data[k * 2 + 1] = env[j].data[k * 2 + 1];

		    lastx = ei->data[k * 2];
		}

		ei->npt = k;
		break;
            }
        }
    }
}


/* Effects 1-6 (note effects) can only be entered in the first effect
 * column, G-L (volume-effects) only in the second column.
 */

static void xlat_fx_common(uint8 *t, uint8 *p)
{
    switch (*t) {
    case 0x07:			/* 7 - Set BPM */
	*t = FX_S3M_BPM;
	break;
    case 0x08:			/* 8 - Set pan */
    case 0x09:			/* 9 - Set envelope -- not supported */
    case 0x0a:			/* A - Not used */
	*t = *p = 0x00;
	break;
    case 0x0b:			/* B - Position jump */
    case 0x0c:			/* C - Set volume */
    case 0x0d:			/* D - Pattern break */
	/* Like protracker */
	break;
    case 0x0e:			/* E - Extended */
	switch (MSN (*p)) {
	case 0x0:		/* E0 - not used */
	case 0x3:		/* E3 - not used */
	case 0x8:		/* Set sample status -- unsupported */
	    *t = *p = 0x00;
	    break;
	case 0x1:		/* Pan slide left */
	    *t = FX_PANSLIDE;
	    *p <<= 4;
	    break;
	case 0x2:		/* Pan slide right */
	    *t = FX_PANSLIDE;
	    *p &= 0x0f;
	    break;
	}
	break;
    case 0x0f:
	*t = FX_S3M_SPEED;
	break;
    }
}

static void xlat_fx1(uint8 *t, uint8 *p)
{
    switch (*t) {
    case 0x00:			/* - - No effect */
	*p = 0;
	break;
    case 0x05:			/* 5 - Arpeggio */
	*t = FX_ARPEGGIO;
	break;
    case 0x06:			/* 6 - Not used */
	*t = *p = 0x00;
	break;
    }

    xlat_fx_common(t, p);
}


static void xlat_fx2(uint8 *t, uint8 *p)
{
    switch (*t) {
    case 0x00:			/* - - No effect */
	*p = 0;
	break;
    case 0x01:			/* G - Volume slide up */
	*t = FX_VOLSLIDE_UP;
	break;
    case 0x02:			/* H - Volume slide down */
	*t = FX_VOLSLIDE_DN;
	break;
    case 0x03:			/* I - Multi-retrig */
	*t = FX_MULTI_RETRIG;
	break;
    case 0x04:			/* J - Tremolo */
	*t = FX_TREMOLO;
	break;
    case 0x05:			/* K - Tremor */
	*t = FX_TREMOR;
	break;
    case 0x06:			/* L - Not used */
	*t = *p = 0x00;
	break;
    }

    xlat_fx_common(t, p);
}

struct bits {
	uint32 b, n;
};

static unsigned int get_bits(char i, uint8 **buf, int *len, struct bits *bits)
{
    unsigned int x;

    if (i == 0) {
	bits->b = readmem32l(*buf);
	*buf += 4; *len -= 4;
	bits->n = 32;
    }

    x = bits->b & ((1 << i) - 1);	/* get i bits */
    bits->b >>= i;
    if ((bits->n -= i) <= 24) {
	if (*len <= 0)		/* FIXME: last few bits can't be consumed */
		return x;
	bits->b |= readmem32l((*buf)++) << bits->n;
	bits->n += 8;
	(*len)--;
    }

    return x;
}

/* From the Digitrakker docs:
 *
 * The description of the sample-packmethode (1) [8bit packing]:...
 * ----------------------------------------------------------------
 *
 * The method is based on the Huffman algorithm. It's easy and very fast
 * and effective on samples. The packed sample is a bit stream:
 *
 *	     Byte 0    Byte 1    Byte 2    Byte 3
 *	Bit 76543210  fedcba98  nmlkjihg  ....rqpo
 *
 * A packed byte is stored in the following form:
 *
 *	xxxx10..0s => byte = <xxxx> + (number of <0> bits between
 *		s and 1) * 16 - 8;
 *	if s==1 then byte = byte xor 255
 *
 * If there are no <0> bits between the first bit (sign) and the <1> bit,
 * you have the following form:
 *
 *	xxx1s => byte = <xxx>; if s=1 then byte = byte xor 255
 */

static int unpack_sample8(uint8 *t, uint8 *f, int len, int l)
{
    int i, s;
    uint8 b, d;
    struct bits bits;

    D_(D_INFO "unpack sample 8bit, len=%d", len);
    get_bits(0, &f, &len, &bits);

    for (i = b = d = 0; i < l; i++) {

	/* Sanity check */
        if (len < 0)
            return -1;

	s = get_bits(1, &f, &len, &bits);
	if (get_bits(1, &f, &len, &bits)) {
	    b = get_bits(3, &f, &len, &bits);
	} else {
            b = 8;
	    while (len >= 0 && get_bits(1, &f, &len, &bits) == 0) {
		/* Sanity check */
                if (b >= 240) { return -1; }
		b += 16;
            }
	    b += get_bits(4, &f, &len, &bits);
	}

	if (s) {
	    b ^= 0xff;
	}

	d += b;
	*t++ = d;
    }

    return 0;
}

/*
 * The description of the sample-packmethode (2) [16bit packing]:...
 * ----------------------------------------------------------------
 *
 * It works as methode (1) but it only crunches every 2nd byte (the high-
 * bytes of 16 bit samples). So when you depack 16 bit samples, you have to
 * read 8 bits from the data-stream first. They present the lowbyte of the
 * sample-word. Then depack the highbyte in the descripted way (methode [1]).
 * Only the highbytes are delta-values. So take the lowbytes as they are.
 * Go on this way for the whole sample!
 */

static int unpack_sample16(uint8 *t, uint8 *f, int len, int l)
{
    int i, lo, s;
    uint8 b, d;
    struct bits bits;

    D_(D_INFO "unpack sample 16bit, len=%d", len);
    get_bits(0, &f, &len, &bits);

    for (i = lo = b = d = 0; i < l; i++) {
	/* Sanity check */
        if (len < 0)
            return -1;

	lo = get_bits(8, &f, &len, &bits);
	s = get_bits(1, &f, &len, &bits);
	if (get_bits(1, &f, &len, &bits)) {
	    b = get_bits(3, &f, &len, &bits);
	} else {
            b = 8;
	    while (len >= 0 && get_bits(1, &f, &len, &bits) == 0) {
		/* Sanity check */
                if (b >= 240) { return -1; }
		b += 16;
            }
	    b += get_bits(4, &f, &len, &bits);
	}

	if (s)
	    b ^= 0xff;
	d += b;

	*t++ = lo;
	*t++ = d;
    }

    return 0;
}


/*
 * IFF chunk handlers
 */

static int get_chunk_in(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
    struct xmp_module *mod = &m->mod;
    struct local_data *data = (struct local_data *)parm;
    int i;

    /* Sanity check */
    if (data->has_in) {
	D_(D_CRIT "duplicate IN chunk");
	return -1;
    }
    data->has_in = 1;

    hio_read(mod->name, 1, 32, f);
    mod->name[32] = '\0';
    hio_seek(f, 20, SEEK_CUR);

    mod->len = hio_read16l(f);
    mod->rst = hio_read16l(f);
    hio_read8(f);			/* gvol */
    mod->spd = hio_read8(f);
    mod->bpm = hio_read8(f);

    /* Sanity check */
    if (mod->len > 256 || mod->rst > 255) {
	return -1;
    }

    for (i = 0; i < 32; i++) {
	uint8 chinfo = hio_read8(f);
	if (chinfo & 0x80)
	    break;
	mod->xxc[i].pan = chinfo << 1;
    }
    mod->chn = i;
    hio_seek(f, 32 - i - 1, SEEK_CUR);

    if (hio_read(mod->xxo, 1, mod->len, f) != mod->len) {
	D_(D_CRIT "read error at order list");
	return -1;
    }

    MODULE_INFO();

    return 0;
}

static int get_chunk_pa(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
    struct xmp_module *mod = &m->mod;
    struct local_data *data = (struct local_data *)parm;
    int i, j, chn;
    int x;

    /* Sanity check */
    if (data->has_pa || !data->has_in) {
	D_(D_CRIT "duplicate PA chunk or missing IN chunk");
	return -1;
    }
    data->has_pa = 1;

    mod->pat = hio_read8(f);

    mod->xxp = (struct xmp_pattern **) calloc(mod->pat, sizeof(struct xmp_pattern *));
    if (mod->xxp == NULL)
        return -1;

    D_(D_INFO "Stored patterns: %d", mod->pat);

    for (i = 0; i < mod->pat; i++) {
	if (libxmp_alloc_pattern(mod, i) < 0)
	    return -1;

	chn = hio_read8(f);
	mod->xxp[i]->rows = (int)hio_read8(f) + 1;

	hio_seek(f, 16, SEEK_CUR);		/* Skip pattern name */
	for (j = 0; j < chn; j++) {
	    x = hio_read16l(f);

	    if (j < mod->chn)
		mod->xxp[i]->index[j] = x;
	}
    }

    return 0;
}

static int get_chunk_p0(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
    struct xmp_module *mod = &m->mod;
    struct local_data *data = (struct local_data *)parm;
    int i, j;
    uint16 x;

    /* Sanity check */
    if (data->has_pa || !data->has_in) {
	D_(D_CRIT "duplicate PA (0.0) chunk or missing IN chunk");
	return -1;
    }
    data->has_pa = 1;

    mod->pat = hio_read8(f);

    mod->xxp = (struct xmp_pattern **) calloc(mod->pat, sizeof(struct xmp_pattern *));
    if (mod->xxp == NULL)
        return -1;

    D_(D_INFO "Stored patterns: %d", mod->pat);

    for (i = 0; i < mod->pat; i++) {
	if (libxmp_alloc_pattern(mod, i) < 0)
	    return -1;
	mod->xxp[i]->rows = 64;

	for (j = 0; j < 32; j++) {
	    x = hio_read16l(f);

	    if (j < mod->chn)
		mod->xxp[i]->index[j] = x;
	}
    }

    return 0;
}

static int get_chunk_tr(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
    struct xmp_module *mod = &m->mod;
    struct local_data *data = (struct local_data *)parm;
    int i, j, k, row, len, max_trk;
    struct xmp_track *track;

    /* Sanity check */
    if (data->has_tr || !data->has_pa) {
	D_(D_CRIT "duplicate TR chunk or missing PA chunk");
	return -1;
    }
    data->has_tr = 1;

    mod->trk = hio_read16l(f) + 1;

    /* Sanity check */
    max_trk = 0;
    for (i = 0; i < mod->pat; i++) {
	for (j = 0; j < mod->chn; j++) {
	    if (max_trk < mod->xxp[i]->index[j])
		max_trk = mod->xxp[i]->index[j];
	}
    }
    if (max_trk >= mod->trk) {
	return -1;
    }

    mod->xxt = (struct xmp_track **) calloc(mod->trk, sizeof(struct xmp_track *));
    if (mod->xxt == NULL)
	return -1;

    D_(D_INFO "Stored tracks: %d", mod->trk);

    track = (struct xmp_track *) calloc(1, sizeof(struct xmp_track) +
					   sizeof(struct xmp_event) * 255);
    if (track == NULL)
	goto err;

    /* Empty track 0 is not stored in the file */
    if (libxmp_alloc_track(mod, 0, 256) < 0)
	goto err2;

    for (i = 1; i < mod->trk; i++) {
	/* Length of the track in bytes */
	len = hio_read16l(f);

	memset(track, 0, sizeof(struct xmp_track) +
			 sizeof(struct xmp_event) * 255);

	for (row = 0; len;) {
	    struct xmp_event *ev;

	    /* Sanity check */
	    if (row > 255) {
		goto err2;
	    }

            ev = &track->event[row];

	    j = hio_read8(f);

	    len--;
	    switch (j & 0x03) {
	    case 0:
		row += j >> 2;
		break;
	    case 1:
		/* Sanity check */
		if (row < 1 || row + (j >> 2) > 255)
		    goto err2;

		for (k = 0; k <= (j >> 2); k++)
		    memcpy(&ev[k], &ev[-1], sizeof (struct xmp_event));
		row += k - 1;
		break;
	    case 2:
		/* Sanity check */
		if ((j >> 2) == row) {
		    goto err2;
		}
		memcpy(ev, &track->event[j >> 2], sizeof (struct xmp_event));
		break;
	    case 3:
		if (j & MDL_NOTE_FOLLOWS) {
		    uint8 b = hio_read8(f);
		    len--;
		    ev->note = b == 0xff ? XMP_KEY_OFF : b + 12;
		}
		if (j & MDL_INSTRUMENT_FOLLOWS)
		    len--, ev->ins = hio_read8(f);
		if (j & MDL_VOLUME_FOLLOWS)
		    len--, ev->vol = hio_read8(f);
		if (j & MDL_EFFECT_FOLLOWS) {
		    len--, k = hio_read8(f);
		    ev->fxt = LSN(k);
		    ev->f2t = MSN(k);
		}
		if (j & MDL_PARAMETER1_FOLLOWS)
		    len--, ev->fxp = hio_read8(f);
		if (j & MDL_PARAMETER2_FOLLOWS)
		    len--, ev->f2p = hio_read8(f);
		break;
	    }

	    row++;
	}

	if (row <= 64)
	    row = 64;
	else if (row <= 128)
	    row = 128;
	else row = 256;

	if (libxmp_alloc_track(mod, i, row) < 0)
	    goto err2;

	memcpy(mod->xxt[i], track, sizeof (struct xmp_track) +
				sizeof (struct xmp_event) * (row - 1));

	mod->xxt[i]->rows = row;

	/* Translate effects */
	for (j = 0; j < row; j++) {
		struct xmp_event *ev = &mod->xxt[i]->event[j];
		xlat_fx1(&ev->fxt, &ev->fxp);
		xlat_fx2(&ev->f2t, &ev->f2p);
	}
    }

    free(track);

    return 0;

  err2:
    free(track);
  err:
    return -1;
}

static int get_chunk_ii(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
    struct xmp_module *mod = &m->mod;
    struct local_data *data = (struct local_data *)parm;
    int i, j, k;
    int map, last_map;
    uint8 buf[40];

    /* Sanity check */
    if (data->has_ii) {
	D_(D_CRIT "duplicate II chunk");
	return -1;
    }
    data->has_ii = 1;

    mod->ins = hio_read8(f);
    D_(D_INFO "Instruments: %d", mod->ins);

    mod->xxi = (struct xmp_instrument *) calloc(mod->ins, sizeof(struct xmp_instrument));
    if (mod->xxi == NULL)
	return -1;

    for (i = 0; i < mod->ins; i++) {
	struct xmp_instrument *xxi = &mod->xxi[i];

	data->i_index[i] = hio_read8(f);
	xxi->nsm = hio_read8(f);
	if (hio_read(buf, 1, 32, f) < 32) {
	    D_(D_CRIT "read error at instrument %d", i);
	    return -1;
	}
	buf[32] = 0;
	libxmp_instrument_name(mod, i, buf, 32);

	D_(D_INFO "[%2X] %-32.32s %2d", data->i_index[i], xxi->name, xxi->nsm);

	if (libxmp_alloc_subinstrument(mod, i, xxi->nsm) < 0)
	    return -1;

	for (j = 0; j < XMP_MAX_KEYS; j++)
	    xxi->map[j].ins = 0xff;

	for (last_map = j = 0; j < mod->xxi[i].nsm; j++) {
	    int x;
	    struct xmp_subinstrument *sub = &xxi->sub[j];

	    sub->sid = hio_read8(f);
	    map = hio_read8(f) + 12;
	    sub->vol = hio_read8(f);
	    for (k = last_map; k <= map; k++) {
		if (k < XMP_MAX_KEYS)
		    xxi->map[k].ins = j;
	    }
	    last_map = map + 1;

	    x = hio_read8(f);		/* Volume envelope */
	    if (j == 0)
		data->v_index[i] = x & 0x80 ? x & 0x3f : -1;
	    if (~x & 0x40)
		sub->vol = 0xff;

	    mod->xxi[i].sub[j].pan = hio_read8(f) << 1;

	    x = hio_read8(f);		/* Pan envelope */
	    if (j == 0)
		data->p_index[i] = x & 0x80 ? x & 0x3f : -1;
	    if (~x & 0x40)
		sub->pan = 0x80;

	    x = hio_read16l(f);
	    if (j == 0)
		xxi->rls = x;

	    sub->vra = hio_read8(f);	/* vibrato rate */
	    sub->vde = hio_read8(f) << 1;	/* vibrato depth */
	    sub->vsw = hio_read8(f);	/* vibrato sweep */
	    sub->vwf = hio_read8(f);	/* vibrato waveform */
	    hio_read8(f);		/* Reserved */

	    x = hio_read8(f);		/* Pitch envelope */
	    if (j == 0)
		data->f_index[i] = x & 0x80 ? x & 0x3f : -1;

	    D_(D_INFO "  %2x: V%02x S%02x v%02x p%02x f%02x",
			j, sub->vol, sub->sid, data->v_index[i],
			data->p_index[i], data->f_index[i]);
	}
    }

    return 0;
}

static int get_chunk_is(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
    struct xmp_module *mod = &m->mod;
    struct local_data *data = (struct local_data *)parm;
    int i;
    uint8 buf[64];
    uint8 x;

    /* Sanity check */
    if (data->has_is) {
	D_(D_CRIT "duplicate IS chunk");
	return -1;
    }
    data->has_is = 1;

    mod->smp = hio_read8(f);
    mod->xxs = (struct xmp_sample *) calloc(mod->smp, sizeof(struct xmp_sample));
    if (mod->xxs == NULL)
	return -1;
    m->xtra = (struct extra_sample_data *) calloc(mod->smp, sizeof(struct extra_sample_data));
    if (m->xtra == NULL)
        return -1;

    data->packinfo = (int *) calloc(mod->smp, sizeof(int));
    if (data->packinfo == NULL)
	return -1;

    D_(D_INFO "Sample infos: %d", mod->smp);

    for (i = 0; i < mod->smp; i++) {
	struct xmp_sample *xxs = &mod->xxs[i];
	int c5spd;

	data->s_index[i] = hio_read8(f);	/* Sample number */
	if (hio_read(buf, 1, 32, f) < 32) {
	    D_(D_CRIT "read error at sample %d", i);
	    return -1;
	}
	buf[32] = 0;
	libxmp_copy_adjust(xxs->name, buf, 31);

	hio_seek(f, 8, SEEK_CUR);		/* Sample filename */

	c5spd = hio_read32l(f);

	xxs->len = hio_read32l(f);
	xxs->lps = hio_read32l(f);
	xxs->lpe = hio_read32l(f);

	/* Sanity check */
	if (xxs->len < 0 || xxs->lps < 0 ||
	    xxs->lps > xxs->len || xxs->lpe > (xxs->len - xxs->lps)) {
		D_(D_CRIT "invalid sample %d - len:%d s:%d l:%d",
			i, xxs->len, xxs->lps, xxs->lpe);
		return -1;
	}

	xxs->flg = xxs->lpe > 0 ? XMP_SAMPLE_LOOP : 0;
	xxs->lpe = xxs->lps + xxs->lpe;

        m->xtra[i].c5spd = (double)c5spd;

	hio_read8(f);				/* Volume in DMDL 0.0 */
	x = hio_read8(f);
	if (x & 0x01) {
	    xxs->flg |= XMP_SAMPLE_16BIT;
	    xxs->len >>= 1;
	    xxs->lps >>= 1;
	    xxs->lpe >>= 1;
        }
	xxs->flg |= (x & 0x02) ? XMP_SAMPLE_LOOP_BIDIR : 0;
	data->packinfo[i] = (x & 0x0c) >> 2;

	D_(D_INFO "[%2X] %-32.32s %05x%c %05x %05x %c %6d %d",
			data->s_index[i], xxs->name, xxs->len,
			xxs->flg & XMP_SAMPLE_16BIT ? '+' : ' ',
			xxs->lps, xxs->lpe,
			xxs->flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
			c5spd, data->packinfo[i]);
    }

    return 0;
}

static int get_chunk_i0(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
    struct xmp_module *mod = &m->mod;
    struct local_data *data = (struct local_data *)parm;
    int i;
    uint8 buf[64];
    uint8 x;

    /* Sanity check */
    if (data->has_ii || data->has_is) {
	D_(D_CRIT "duplicate IS (0.0) chunk");
	return -1;
    }
    data->has_ii = 1;
    data->has_is = 1;

    mod->ins = mod->smp = hio_read8(f);

    D_(D_INFO "Instruments (0.0): %d", mod->ins);

    if (libxmp_init_instrument(m) < 0)
	return -1;

    data->packinfo = (int *) calloc(mod->smp, sizeof(int));
    if (data->packinfo == NULL)
	return -1;

    for (i = 0; i < mod->ins; i++) {
	struct xmp_instrument *xxi = &mod->xxi[i];
	struct xmp_subinstrument *sub;
	struct xmp_sample *xxs = &mod->xxs[i];
	int c5spd;

	xxi->nsm = 1;
	if (libxmp_alloc_subinstrument(mod, i, 1) < 0)
	    return -1;

	sub = &xxi->sub[0];
	sub->sid = data->i_index[i] = data->s_index[i] = hio_read8(f);

	if (hio_read(buf, 1, 32, f) < 32) {
	    D_(D_CRIT "read error at instrument %d", i);
	    return -1;
	}
	buf[32] = 0;
	hio_seek(f, 8, SEEK_CUR);	/* Sample filename */
	libxmp_instrument_name(mod, i, buf, 32);

	c5spd = hio_read16l(f);

	xxs->len = hio_read32l(f);
	xxs->lps = hio_read32l(f);
	xxs->lpe = hio_read32l(f);

	/* Sanity check */
	if (xxs->len < 0 || xxs->lps < 0 ||
	    xxs->lps > xxs->len || xxs->lpe > (xxs->len - xxs->lps)) {
		D_(D_CRIT "invalid sample %d - len:%d s:%d l:%d",
			i, xxs->len, xxs->lps, xxs->lpe);
		return -1;
	}

	xxs->flg = xxs->lpe > 0 ? XMP_SAMPLE_LOOP : 0;
	xxs->lpe = xxs->lps + xxs->lpe;

	sub->vol = hio_read8(f);	/* Volume */
	sub->pan = 0x80;

        m->xtra[i].c5spd = (double)c5spd;

	x = hio_read8(f);
	if (x & 0x01) {
	    xxs->flg |= XMP_SAMPLE_16BIT;
	    xxs->len >>= 1;
	    xxs->lps >>= 1;
	    xxs->lpe >>= 1;
	}
	xxs->flg |= (x & 0x02) ? XMP_SAMPLE_LOOP_BIDIR : 0;
	data->packinfo[i] = (x & 0x0c) >> 2;

	D_(D_INFO "[%2X] %-32.32s %5d V%02x %05x%c %05x %05x %d",
		data->i_index[i], xxi->name, c5spd, sub->vol,
		xxs->len, xxs->flg & XMP_SAMPLE_16BIT ? '+' : ' ',
		xxs->lps, xxs->lpe, data->packinfo[i]);
    }

    return 0;
}

static int get_chunk_sa(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
    struct xmp_module *mod = &m->mod;
    struct local_data *data = (struct local_data *)parm;
    int i, len, size_bound;
    uint8 *smpbuf = NULL, *buf;
    int smpbuf_alloc = -1;
    int left = hio_size(f) - hio_tell(f);

    /* Sanity check */
    if (data->has_sa || !data->has_is || data->packinfo == NULL) {
	D_(D_CRIT "duplicate SA chunk or missing IS chunk");
	return -1;
    }
    data->has_sa = 1;

    if (size < left)
	left = size;

    D_(D_INFO "Stored samples: %d", mod->smp);

    for (i = 0; i < mod->smp; i++) {
	struct xmp_sample *xxs = &mod->xxs[i];

        len = xxs->len;
	if (xxs->flg & XMP_SAMPLE_16BIT)
	    len <<= 1;

	/* Bound the packed sample data size before trying to allocate RAM for it... */
	switch (data->packinfo[i]) {
	case 0:
	    size_bound = len;
	    break;
	case 1:
	    /* See unpack_sample8: each byte packs to 5 bits minimum. */
	    size_bound = (len >> 3) * 5;
	    break;
	case 2:
	    /* See unpack_sample16: each upper byte packs to 5 bits minimum, lower bytes are not packed. */
	    size_bound = (len >> 4) * 13;
	    break;
	default:
	    /* Sanity check */
	    D_(D_CRIT "sample %d invalid pack %d", i, data->packinfo[i]);
	    goto err2;
	}

	/* Sanity check */
	if (left < size_bound) {
	    D_(D_CRIT "sample %d (pack=%d) requested >=%d bytes, only %d available",
		i, data->packinfo[i], size_bound, left);
	    goto err2;
	}

	if (len > smpbuf_alloc) {
	    uint8 *tmp = (uint8 *) realloc(smpbuf, len);
	    if (!tmp)
		goto err2;

	    smpbuf = tmp;
	    smpbuf_alloc = len;
	}

	switch (data->packinfo[i]) {
	case 0:
	    if (hio_read(smpbuf, 1, len, f) < len) {
		D_(D_CRIT "sample %d read error (no pack)", i);
		goto err2;
	    }
	    left -= len;
	    break;
	case 1:
	    len = hio_read32l(f);
            /* Sanity check */
            if (xxs->flg & XMP_SAMPLE_16BIT)
                goto err2;
            if (len <= 0 || len > 0x80000)  /* Max compressed sample size */
                goto err2;
	    if ((buf = (uint8 *)malloc(len + 4)) == NULL)
		goto err2;
	    if (hio_read(buf, 1, len, f) != len) {
		D_(D_CRIT "sample %d read error (8-bit)", i);
		goto err3;
	    }
	    /* The unpack function may read slightly beyond the end. */
	    buf[len] = buf[len + 1] = buf[len + 2] = buf[len + 3] = 0;
            if (unpack_sample8(smpbuf, buf, len, xxs->len) < 0) {
		D_(D_CRIT "sample %d unpack error (8-bit)", i);
		goto err3;
	    }
	    free(buf);
	    left -= len + 4;
	    break;
	case 2:
	    len = hio_read32l(f);
            /* Sanity check */
            if (~xxs->flg & XMP_SAMPLE_16BIT)
                goto err2;
            if (len <= 0 || len > MAX_SAMPLE_SIZE)
                goto err2;
	    if ((buf = (uint8 *)malloc(len + 4)) == NULL)
		goto err2;
	    if (hio_read(buf, 1, len, f) != len) {
		D_(D_CRIT "sample %d read error (16-bit)", i);
		goto err3;
	    }
	    /* The unpack function may read slightly beyond the end. */
	    buf[len] = buf[len + 1] = buf[len + 2] = buf[len + 3] = 0;
            if (unpack_sample16(smpbuf, buf, len, xxs->len) < 0) {
		D_(D_CRIT "sample %d unpack error (16-bit)", i);
		goto err3;
	    }
	    free(buf);
	    left -= len + 4;
	    break;
	}

	if (libxmp_load_sample(m, NULL, SAMPLE_FLAG_NOLOAD, xxs, (char *)smpbuf) < 0)
	    goto err2;
    }

    free(smpbuf);
    return 0;
  err3:
    free(buf);
  err2:
    free(smpbuf);
    return -1;
}

static int get_chunk_ve(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
    struct local_data *data = (struct local_data *)parm;
    int i;

    /* Sanity check */
    if (data->v_env) {
	D_(D_CRIT "duplicate VE chunk");
	return -1;
    }

    if ((data->v_envnum = hio_read8(f)) == 0)
	return 0;

    D_(D_INFO "Vol envelopes: %d", data->v_envnum);

    data->v_env = (struct mdl_envelope *) calloc(data->v_envnum, sizeof(struct mdl_envelope));
    if (data->v_env == NULL) {
	return -1;
    }

    for (i = 0; i < data->v_envnum; i++) {
	data->v_env[i].num = hio_read8(f);
	hio_read(data->v_env[i].data, 1, 30, f);
	data->v_env[i].sus = hio_read8(f);
	data->v_env[i].loop = hio_read8(f);
    }

    return 0;
}

static int get_chunk_pe(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
    struct local_data *data = (struct local_data *)parm;
    int i;

    /* Sanity check */
    if (data->p_env) {
	D_(D_CRIT "duplicate PE chunk");
	return -1;
    }

    if ((data->p_envnum = hio_read8(f)) == 0)
	return 0;

    D_(D_INFO "Pan envelopes: %d", data->p_envnum);

    data->p_env = (struct mdl_envelope *) calloc(data->p_envnum, sizeof(struct mdl_envelope));
    if (data->p_env == NULL) {
	return -1;
    }

    for (i = 0; i < data->p_envnum; i++) {
	data->p_env[i].num = hio_read8(f);
	hio_read(data->p_env[i].data, 1, 30, f);
	data->p_env[i].sus = hio_read8(f);
	data->p_env[i].loop = hio_read8(f);
    }

    return 0;
}

static int get_chunk_fe(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
    struct local_data *data = (struct local_data *)parm;
    int i;

    /* Sanity check */
    if (data->f_env) {
	D_(D_CRIT "duplicate FE chunk");
	return -1;
    }

    if ((data->f_envnum = hio_read8(f)) == 0)
	return 0;

    D_(D_INFO "Pitch envelopes: %d", data->f_envnum);

    data->f_env = (struct mdl_envelope *) calloc(data->f_envnum, sizeof(struct mdl_envelope));
    if (data->f_env == NULL) {
	return -1;
    }

    for (i = 0; i < data->f_envnum; i++) {
	data->f_env[i].num = hio_read8(f);
	hio_read(data->f_env[i].data, 1, 30, f);
	data->f_env[i].sus = hio_read8(f);
	data->f_env[i].loop = hio_read8(f);
    }

    return 0;
}


static int mdl_load(struct module_data *m, HIO_HANDLE *f, const int start)
{
    struct xmp_module *mod = &m->mod;
    iff_handle handle;
    int i, j, k, l;
    char buf[8];
    struct local_data data;
    int retval = 0;

    LOAD_INIT();

    memset(&data, 0, sizeof (struct local_data));

    /* Check magic and get version */
    hio_read32b(f);
    if (hio_read(buf, 1, 1, f) < 1)
	return -1;

    handle = libxmp_iff_new();
    if (handle == NULL)
	return -1;

    /* IFFoid chunk IDs */
    libxmp_iff_register(handle, "IN", get_chunk_in);	/* Module info */
    libxmp_iff_register(handle, "TR", get_chunk_tr);	/* Tracks */
    libxmp_iff_register(handle, "SA", get_chunk_sa);	/* Sampled data */
    libxmp_iff_register(handle, "VE", get_chunk_ve);	/* Volume envelopes */
    libxmp_iff_register(handle, "PE", get_chunk_pe);	/* Pan envelopes */
    libxmp_iff_register(handle, "FE", get_chunk_fe);	/* Pitch envelopes */

    if (MSN(*buf)) {
	libxmp_iff_register(handle, "II", get_chunk_ii);	/* Instruments */
	libxmp_iff_register(handle, "PA", get_chunk_pa);	/* Patterns */
	libxmp_iff_register(handle, "IS", get_chunk_is);	/* Sample info */
    } else {
	libxmp_iff_register(handle, "PA", get_chunk_p0);	/* Old 0.0 patterns */
	libxmp_iff_register(handle, "IS", get_chunk_i0);	/* Old 0.0 Sample info */
    }

    /* MDL uses a IFF-style file format with 16 bit IDs and little endian
     * 32 bit chunk size. There's only one chunk per data type (i.e. one
     * big chunk for all samples).
     */
    libxmp_iff_id_size(handle, 2);
    libxmp_iff_set_quirk(handle, IFF_LITTLE_ENDIAN);

    libxmp_set_type(m, "Digitrakker MDL %d.%d", MSN(*buf), LSN(*buf));

    m->volbase = 0xff;
    m->c4rate = C4_NTSC_RATE;

    data.v_envnum = data.p_envnum = data.f_envnum = 0;
    data.s_index = (int *) calloc(256, sizeof(int));
    data.i_index = (int *) calloc(256, sizeof(int));
    data.v_index = (int *) malloc(256 * sizeof(int));
    data.p_index = (int *) malloc(256 * sizeof(int));
    data.f_index = (int *) malloc(256 * sizeof(int));
    if (!data.s_index || !data.i_index || !data.v_index || !data.p_index || !data.f_index) {
	goto err;
    }

    for (i = 0; i < 256; i++) {
	data.v_index[i] = data.p_index[i] = data.f_index[i] = -1;
    }

    /* Load IFFoid chunks */
    if (libxmp_iff_load(handle, m, f, &data) < 0) {
	libxmp_iff_release(handle);
	retval = -1;
	goto err;
    }

    libxmp_iff_release(handle);

    /* Reindex instruments */
    for (i = 0; i < mod->trk; i++) {
	for (j = 0; j < mod->xxt[i]->rows; j++) {
	    struct xmp_event *e = &mod->xxt[i]->event[j];

	    for (l = 0; l < mod->ins; l++) {
		if (e->ins && e->ins == data.i_index[l]) {
		    e->ins = l + 1;
		    break;
		}
	    }
	}
    }

    /* Reindex envelopes, etc. */
    for (i = 0; i < mod->ins; i++) {
        fix_env(i, &mod->xxi[i].aei, data.v_env, data.v_index, data.v_envnum);
        fix_env(i, &mod->xxi[i].pei, data.p_env, data.p_index, data.p_envnum);
        fix_env(i, &mod->xxi[i].fei, data.f_env, data.f_index, data.f_envnum);

	for (j = 0; j < mod->xxi[i].nsm; j++) {
	    for (k = 0; k < mod->smp; k++) {
		if (mod->xxi[i].sub[j].sid == data.s_index[k]) {
		    mod->xxi[i].sub[j].sid = k;
		    /*libxmp_c2spd_to_note(data.c2spd[k],
			&mod->xxi[i].sub[j].xpo, &mod->xxi[i].sub[j].fin);*/
		    break;
		}
	    }
	}
    }

  err:
    free(data.f_index);
    free(data.p_index);
    free(data.v_index);
    free(data.i_index);
    free(data.s_index);

    free(data.v_env);
    free(data.p_env);
    free(data.f_env);

    free(data.packinfo);

    m->quirk |= QUIRKS_FT2 | QUIRK_KEYOFF;
    m->read_event_type = READ_EVENT_FT2;

    return retval;
}
