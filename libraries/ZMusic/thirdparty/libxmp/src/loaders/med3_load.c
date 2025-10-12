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

/*
 * MED 2.00 is in Fish disk #349 and has a couple of demo modules, get it
 * from ftp://ftp.funet.fi/pub/amiga/fish/301-400/ff349
 */

#include "loader.h"
#include "med.h"

#define MAGIC_MED3	MAGIC4('M','E','D',3)


static int med3_test(HIO_HANDLE *, char *, const int);
static int med3_load (struct module_data *, HIO_HANDLE *, const int);

const struct format_loader libxmp_loader_med3 = {
	"MED 2.00 MED3",
	med3_test,
	med3_load
};

static int med3_test(HIO_HANDLE *f, char *t, const int start)
{
	if (hio_read32b(f) !=  MAGIC_MED3)
		return -1;

	libxmp_read_title(f, t, 0);

	return 0;
}


#define MASK		0x80000000

#define M0F_LINEMSK0F	0x01
#define M0F_LINEMSK1F	0x02
#define M0F_FXMSK0F	0x04
#define M0F_FXMSK1F	0x08
#define M0F_LINEMSK00	0x10
#define M0F_LINEMSK10	0x20
#define M0F_FXMSK00	0x40
#define M0F_FXMSK10	0x80


/*
 * From the MED 2.00 file loading/saving routines by Teijo Kinnunen, 1990
 */

static uint8 get_nibble(uint8 *mem, uint16 *nbnum)
{
	uint8 *mloc = mem + (*nbnum / 2), res;

	if(*nbnum & 0x1)
		res = *mloc & 0x0f;
	else
		res = *mloc >> 4;
	(*nbnum)++;

	return res;
}

static uint16 get_nibbles(uint8 *mem, uint16 *nbnum, uint8 nbs)
{
	uint16 res = 0;

	while (nbs--) {
		res <<= 4;
		res |= get_nibble(mem, nbnum);
	}

	return res;
}

static int unpack_block(struct module_data *m, uint16 bnum, uint8 *from, uint16 convsz)
{
	struct xmp_module *mod = &m->mod;
	struct xmp_event *event;
	uint32 linemsk0 = *((uint32 *)from), linemsk1 = *((uint32 *)from + 1);
	uint32 fxmsk0 = *((uint32 *)from + 2), fxmsk1 = *((uint32 *)from + 3);
	uint32 *lmptr = &linemsk0, *fxptr = &fxmsk0;
	uint16 fromn = 0, lmsk;
	uint8 *fromst = from + 16, bcnt, *tmpto;
	uint8 *patbuf, *to;
	uint32 nibs_left = convsz * 2;
	int i, j, trkn = mod->chn;

	/*from += 16;*/
	patbuf = to = (uint8 *) calloc(3, 4 * 64);
	if (to == NULL) {
		goto err;
	}

	for (i = 0; i < 64; i++) {
		if (i == 32) {
			lmptr = &linemsk1;
			fxptr = &fxmsk1;
		}

		if (*lmptr & MASK) {
			if (trkn / 4 > nibs_left) {
				goto err2;
			}
			nibs_left -= trkn / 4;

			lmsk = get_nibbles(fromst, &fromn, (uint8)(trkn / 4));
			lmsk <<= (16 - trkn);
			tmpto = to;

			for (bcnt = 0; bcnt < trkn; bcnt++) {
				if (lmsk & 0x8000) {
					if (nibs_left < 3) {
						goto err2;
					}
					nibs_left -= 3;
					*tmpto = (uint8)get_nibbles(fromst,
						&fromn,2);
					*(tmpto + 1) = (get_nibble(fromst,
							&fromn) << 4);
				}
				lmsk <<= 1;
				tmpto += 3;
			}
		}

		if (*fxptr & MASK) {
			if (trkn / 4 > nibs_left) {
				goto err2;
			}
			nibs_left -= trkn / 4;

			lmsk = get_nibbles(fromst,&fromn,(uint8)(trkn / 4));
			lmsk <<= (16 - trkn);
			tmpto = to;

			for (bcnt = 0; bcnt < trkn; bcnt++) {
				if (lmsk & 0x8000) {
					if (nibs_left < 3) {
						goto err2;
					}
					nibs_left -= 3;
					*(tmpto+1) |= get_nibble(fromst,
							&fromn);
					*(tmpto+2) = (uint8)get_nibbles(fromst,
							&fromn,2);
				}
				lmsk <<= 1;
				tmpto += 3;
			}
		}
		to += 3 * trkn;
		*lmptr <<= 1;
		*fxptr <<= 1;
	}

	for (i = 0; i < 64; i++) {
		for (j = 0; j < 4; j++) {
			event = &EVENT(bnum, j, i);

			event->note = patbuf[i * 12 + j * 3 + 0];
			if (event->note)
				event->note += 48;
			event->ins  = patbuf[i * 12 + j * 3 + 1] >> 4;
			if (event->ins)
				event->ins++;
			event->fxt  = patbuf[i * 12 + j * 3 + 1] & 0x0f;
			event->fxp  = patbuf[i * 12 + j * 3 + 2];

			switch (event->fxt) {
			case 0x00:	/* arpeggio */
			case 0x01:	/* slide up */
			case 0x02:	/* slide down */
			case 0x03:	/* portamento */
			case 0x04:	/* vibrato? */
				break;
			case 0x0c:	/* set volume (BCD) */
				event->fxp = MSN(event->fxp) * 10 +
							LSN(event->fxp);
				break;
			case 0x0d:	/* volume slides */
				event->fxt = FX_VOLSLIDE;
				break;
			case 0x0f:	/* tempo/break */
				if (event->fxp == 0) {
					event->fxt = FX_BREAK;
				} else if (event->fxp == 0xff) {
					event->fxp = event->fxt = 0;
					event->vol = 1;
				} else if (event->fxp == 0xfe) {
					event->fxp = event->fxt = 0;
				} else if (event->fxp == 0xf1) {
					/* Retrigger once on tick 3 */
					event->fxt = FX_EXTENDED;
					event->fxp = (EX_RETRIG << 4) | 3;
				} else if (event->fxp == 0xf2) {
					/* Delay until tick 3 */
					event->fxt = FX_EXTENDED;
					event->fxp = (EX_DELAY << 4) | 3;
				} else if (event->fxp == 0xf3) {
					/* Retrigger every 2 ticks (TODO: buggy) */
					event->fxt = FX_MED_RETRIG;
					event->fxp = 0x02;
				} else if (event->fxp <= 0xf0) {
					event->fxt = FX_S3M_BPM;
					event->fxp = mmd_convert_tempo(event->fxp, 0, 0);
				} else {
					event->fxt = event->fxp = 0;
				}
				break;
			default:
				event->fxp = event->fxt = 0;
			}
		}
	}

	free(patbuf);

	return 0;

     err2:
	free(patbuf);
     err:
	return -1;
}


static int med3_load(struct module_data *m, HIO_HANDLE *f, const int start)
{
	struct xmp_module *mod = &m->mod;
	int i, j;
	uint32 mask;
	int transp, sliding;
	int tempo;
	int flags;

	LOAD_INIT();

	hio_read32b(f);

	libxmp_set_type(m, "MED 2.00 MED3");

	mod->ins = mod->smp = 32;

	if (libxmp_init_instrument(m) < 0)
		return -1;

	/* read instrument names */
	for (i = 0; i < 32; i++) {
		uint8 c, buf[40];
		for (j = 0; j < 40; j++) {
			c = hio_read8(f);
			buf[j] = c;
			if (c == 0)
				break;
		}
		libxmp_instrument_name(mod, i, buf, 32);
		if (libxmp_alloc_subinstrument(mod, i, 1) < 0)
			return -1;
	}

	/* read instrument volumes */
	mask = hio_read32b(f);
	for (i = 0; i < 32; i++, mask <<= 1) {
		mod->xxi[i].sub[0].vol = mask & MASK ? hio_read8(f) : 0;
		mod->xxi[i].sub[0].pan = 0x80;
		mod->xxi[i].sub[0].fin = 0;
		mod->xxi[i].sub[0].sid = i;
	}

	/* read instrument loops */
	mask = hio_read32b(f);
	for (i = 0; i < 32; i++, mask <<= 1) {
		mod->xxs[i].lps = mask & MASK ? hio_read16b(f) : 0;
	}

	/* read instrument loop length */
	mask = hio_read32b(f);
	for (i = 0; i < 32; i++, mask <<= 1) {
		uint32 lsiz = mask & MASK ? hio_read16b(f) : 0;
		mod->xxs[i].len = mod->xxs[i].lps + lsiz;
		mod->xxs[i].lpe = mod->xxs[i].lps + lsiz;
		mod->xxs[i].flg = lsiz > 1 ? XMP_SAMPLE_LOOP : 0;
	}

	mod->chn = 4;
	mod->pat = hio_read16b(f);
	mod->trk = mod->chn * mod->pat;

	mod->len = hio_read16b(f);

	/* Sanity check */
	if (mod->len > 256 || mod->pat > 256)
		return -1;

	hio_read(mod->xxo, 1, mod->len, f);
	tempo = hio_read16b(f);
	transp = hio_read8s(f);
	flags = hio_read8(f);		/* flags */
	sliding = hio_read16b(f);	/* sliding */
	hio_read32b(f);			/* jumping mask */
	hio_seek(f, 16, SEEK_CUR);	/* rgb */

	mod->spd = 6;
	mod->bpm = mmd_convert_tempo(tempo, 0, 0);
	m->time_factor = MED_TIME_FACTOR;

	/* read midi channels */
	mask = hio_read32b(f);
	for (i = 0; i < 32; i++, mask <<= 1) {
		if (mask & MASK)
			hio_read8(f);
	}

	/* read midi programs */
	mask = hio_read32b(f);
	for (i = 0; i < 32; i++, mask <<= 1) {
		if (mask & MASK)
			hio_read8(f);
	}

	MODULE_INFO();

	D_(D_INFO "Sliding: %d", sliding);
	D_(D_INFO "Play transpose: %d", transp);

	m->quirk |= QUIRK_RTONCE; /* FF1 */
	if (sliding == 6)
		m->quirk |= QUIRK_VSALL | QUIRK_PBALL;

	for (i = 0; i < 32; i++)
		mod->xxi[i].sub[0].xpo = transp;

	if (libxmp_init_pattern(mod) < 0)
		return -1;

	/* Load and convert patterns */
	D_(D_INFO "Stored patterns: %d", mod->pat);

	for (i = 0; i < mod->pat; i++) {
		uint32 *conv;
		uint8 b;
		/*uint8 tracks;*/
		uint16 convsz;

		if (libxmp_alloc_pattern_tracks(mod, i, 64) < 0)
			return -1;

		/* TODO: not clear if this should be respected. Later MED
		 * formats are capable of having different track counts. */
		/*tracks =*/ hio_read8(f);

		b = hio_read8(f);
		convsz = hio_read16b(f);
		conv = (uint32 *) calloc(1, convsz + 16);
		if (conv == NULL)
			return -1;

                if (b & M0F_LINEMSK00)
			*conv = 0L;
                else if (b & M0F_LINEMSK0F)
			*conv = 0xffffffff;
                else
			*conv = hio_read32b(f);

                if (b & M0F_LINEMSK10)
			*(conv + 1) = 0L;
                else if (b & M0F_LINEMSK1F)
			*(conv + 1) = 0xffffffff;
                else
			*(conv + 1) = hio_read32b(f);

                if (b & M0F_FXMSK00)
			*(conv + 2) = 0L;
                else if (b & M0F_FXMSK0F)
			*(conv + 2) = 0xffffffff;
                else
			*(conv + 2) = hio_read32b(f);

                if (b & M0F_FXMSK10)
			*(conv + 3) = 0L;
                else if (b & M0F_FXMSK1F)
			*(conv + 3) = 0xffffffff;
                else
			*(conv + 3) = hio_read32b(f);

		if (hio_read(conv + 4, 1, convsz, f) != convsz) {
			free(conv);
			return -1;
		}

                if (unpack_block(m, i, (uint8 *)conv, convsz) < 0) {
			free(conv);
			return -1;
		}

		free(conv);
	}

	/* Load samples */

	D_(D_INFO "Instruments: %d", mod->ins);

	mask = hio_read32b(f);
	for (i = 0; i < 32; i++, mask <<= 1) {
		if (~mask & MASK)
			continue;

		if (~flags & FLAG_INSTRSATT) {
			/* Song file */
			if (med_load_external_instrument(f, m, i)) {
				D_(D_CRIT "error loading instrument %d", i);
				return -1;
			}
			continue;
		}

		/* Module file */
		mod->xxi[i].nsm = 1;
		mod->xxs[i].len = hio_read32b(f);

		if (mod->xxs[i].len == 0)
			mod->xxi[i].nsm = 0;

		if (hio_read16b(f))		/* type */
			continue;

		D_(D_INFO "[%2X] %-32.32s %04x %04x %04x %c V%02x ",
			i, mod->xxi[i].name, mod->xxs[i].len, mod->xxs[i].lps,
			mod->xxs[i].lpe,
			mod->xxs[i].flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
			mod->xxi[i].sub[0].vol);

		if (libxmp_load_sample(m, f, 0, &mod->xxs[i], NULL) < 0)
			return -1;
	}

	return 0;
}
