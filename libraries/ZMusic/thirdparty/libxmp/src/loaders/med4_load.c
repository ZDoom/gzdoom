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

/*
 * MED 2.13 is in Fish disk #424 and has a couple of demo modules, get it
 * from ftp://ftp.funet.fi/pub/amiga/fish/401-500/ff424. Alex Van Starrex's
 * HappySong MED4 is in ff401. MED 3.00 is in ff476.
 */

#include "med.h"
#include "loader.h"
#include "../med_extras.h"

#define MAGIC_MED4	MAGIC4('M','E','D',4)
#undef MED4_DEBUG

static int med4_test(HIO_HANDLE *, char *, const int);
static int med4_load (struct module_data *, HIO_HANDLE *, const int);

const struct format_loader libxmp_loader_med4 = {
	"MED 2.10 MED4",
	med4_test,
	med4_load
};

static int med4_test(HIO_HANDLE *f, char *t, const int start)
{
	if (hio_read32b(f) != MAGIC_MED4)
		return -1;

	libxmp_read_title(f, t, 0);

	return 0;
}

static void fix_effect(struct xmp_event *event, int hexvol)
{
	switch (event->fxt) {
	case 0x00:	/* arpeggio */
	case 0x01:	/* slide up */
	case 0x02:	/* slide down */
	case 0x03:	/* portamento */
	case 0x04:	/* vibrato? */
		break;
	case 0x0c:	/* set volume (BCD) */
		if (!hexvol) {
			event->fxp = MSN(event->fxp) * 10 + LSN(event->fxp);
		}
		break;
	case 0x0d:	/* volume slides */
		event->fxt = FX_VOLSLIDE;
		break;
	case 0x0f:	/* tempo/break */
		if (event->fxp == 0)
			event->fxt = FX_BREAK;
		if (event->fxp == 0xff) {
			event->fxp = event->fxt = 0;
			event->vol = 1;
		} else if (event->fxp == 0xf1) {
			event->fxt = FX_EXTENDED;
			event->fxp = (EX_RETRIG << 4) | 3;
		} else if (event->fxp == 0xf2) {
			event->fxt = FX_EXTENDED;
			event->fxp = (EX_CUT << 4) | 3;
		} else if (event->fxp == 0xf3) {
			event->fxt = FX_EXTENDED;
			event->fxp = (EX_DELAY << 4) | 3;
		} else if (event->fxp > 0xf0) {
			event->fxp = event->fxt = 0;
		} else if (event->fxp > 10) {
			event->fxt = FX_S3M_BPM;
			event->fxp = 125 * event->fxp / 33;
		}
		break;
	default:
		event->fxp = event->fxt = 0;
	}
}

struct stream {
	HIO_HANDLE* f;
	int has_nibble;
	uint8 value;
};

static inline void stream_init(HIO_HANDLE* f, struct stream* s)
{
	s->f = f;
	s->has_nibble = s->value = 0;
}

static inline unsigned stream_read4(struct stream* s)
{
	s->has_nibble = !s->has_nibble;
	if (!s->has_nibble) {
		return s->value & 0x0f;
	} else {
		s->value = hio_read8(s->f);
		return s->value >> 4;
	}
}

static inline unsigned stream_read8(struct stream* s)
{
	unsigned a = stream_read4(s);
	unsigned b = stream_read4(s);
	return (a << 4) | b;
}

static inline unsigned stream_read12(struct stream* s)
{
	unsigned a = stream_read4(s);
	unsigned b = stream_read4(s);
	unsigned c = stream_read4(s);
	return (a << 8) | (b << 4) | c;
}

static inline uint16 stream_read16(struct stream* s)
{
	unsigned a = stream_read4(s);
	unsigned b = stream_read4(s);
	unsigned c = stream_read4(s);
	unsigned d = stream_read4(s);
	return (a << 12) | (b << 8) | (c << 4) | d;
}

static inline uint16 stream_read_aligned16(struct stream* s, int bits)
{
	if (bits <= 4) {
		return stream_read4(s) << 12;
	}
	if (bits <= 8) {
		return stream_read8(s) << 8;
	}
	if (bits <= 12) {
		return stream_read12(s) << 4;
	}
	return stream_read16(s);
}

struct temp_inst {
	char name[32];
	int loop_start;
	int loop_end;
	int volume;
	int transpose;
};

static int med4_load(struct module_data *m, HIO_HANDLE *f, const int start)
{
	struct xmp_module *mod = &m->mod;
	int i, j, k, y;
	uint8 m0;
	uint64 mask;
	int transp, masksz;
	int32 pos;
	int vermaj, vermin;
	uint8 trkvol[16], buf[1024];
	struct xmp_event *event;
	int flags, hexvol = 0;
	int num_ins, num_smp;
	int smp_idx;
	int tempo;
	struct temp_inst temp_inst[64];

	LOAD_INIT();

	hio_read32b(f);		/* Skip magic */

	vermaj = 2;
	vermin = 10;

	/*
	 * Check if we have a MEDV chunk at the end of the file
	 */
	if ((pos = hio_tell(f)) < 0) {
		return -1;
	}

	hio_seek(f, 0, SEEK_END);
	if (hio_tell(f) > 2000) {
		hio_seek(f, -1024, SEEK_CUR);
		hio_read(buf, 1, 1024, f);
		for (i = 0; i < 1013; i++) {
			if (!memcmp(buf + i, "MEDV\000\000\000\004", 8)) {
				vermaj = *(buf + i + 10);
				vermin = *(buf + i + 11);
				break;
			}
		}
	}
	hio_seek(f, start + pos, SEEK_SET);

	snprintf(mod->type, XMP_NAME_SIZE, "MED %d.%02d MED4", vermaj, vermin);

	m0 = hio_read8(f);

	mask = masksz = 0;
	for (i = 0; m0 != 0 && i < 8; i++, m0 <<= 1) {
		if (m0 & 0x80) {
			mask <<= 8;
			mask |= hio_read8(f);
			masksz++;
		}
	}

	/* CID 128662 (#1 of 1): Bad bit shift operation (BAD_SHIFT)
	 * large_shift: left shifting by more than 63 bits has undefined
	 * behavior.
	 */
	if (masksz > 0) {
		mask <<= 8 * (sizeof(mask) - masksz);
	}
	/*printf("m0=%x mask=%x\n", m0, mask);*/

	/* read instrument names in temporary space */

	num_ins = 0;
	memset(temp_inst, 0, sizeof(temp_inst));
	for (i = 0; mask != 0 && i < 64; i++, mask <<= 1) {
		uint8 c, size;
		uint16 loop_len = 0;

		if ((int64)mask > 0)
			continue;

		num_ins = i + 1;

		/* read flags */
		c = hio_read8(f);

		/* read instrument name */
		size = hio_read8(f);
		for (j = 0; j < size; j++)
			buf[j] = hio_read8(f);
		buf[j] = 0;
#ifdef MED4_DEBUG
		printf("%02x %02x %2d [%s]\n", i, c, size, buf);
#endif

		temp_inst[i].volume = 0x40;

		if ((c & 0x01) == 0)
			temp_inst[i].loop_start = hio_read16b(f) << 1;
		if ((c & 0x02) == 0)
			loop_len = hio_read16b(f) << 1;
		if ((c & 0x04) == 0)	/* ? Tanko2 (MED 3.00 demo) */
			hio_read8(f);
		if ((c & 0x08) == 0)	/* Tim Newsham's "span" */
			hio_read8(f);
		if ((c & 0x30) == 0)
			temp_inst[i].volume = hio_read8(f);
		if ((c & 0x40) == 0)
			temp_inst[i].transpose = hio_read8s(f);

		temp_inst[i].loop_end = temp_inst[i].loop_start + loop_len;

		libxmp_copy_adjust(temp_inst[i].name, buf, 32);
	}

	mod->pat = hio_read16b(f);
	mod->len = hio_read16b(f);

	if (hio_error(f)) {
		return -1;
	}

#ifdef MED4_DEBUG
	printf("pat=%x len=%x\n", mod->pat, mod->len);
#endif
	if (mod->pat > 256 || mod->len > XMP_MAX_MOD_LENGTH)
		return -1;
	hio_read(mod->xxo, 1, mod->len, f);

	/* From MED V3.00 docs:
	 *
	 * The left proportional gadget controls the primary tempo. It canbe
	 * 1 - 240. The bigger the number, the faster the speed. Note that
	 * tempos 1 - 10 are Tracker-compatible (but obsolete, because
	 * secondary tempo can be used now).
	 */
	tempo = hio_read16b(f);
	if (tempo <= 10) {
		mod->spd = tempo;
		mod->bpm = 125;
	} else {
		mod->bpm = 125 * tempo / 33;
	}
	transp = hio_read8s(f);
	flags = hio_read8s(f);
	mod->spd = hio_read16b(f);

	if (~flags & 0x20)	/* sliding */
		m->quirk |= QUIRK_VSALL | QUIRK_PBALL;

	if (flags & 0x10)	/* dec/hex volumes */
		hexvol = 1;

	/* This is just a guess... */
	if (vermaj == 2)	/* Happy.med has tempo 5 but loads as 6 */
		mod->spd = flags & 0x20 ? 5 : 6;

	hio_seek(f, 20, SEEK_CUR);

	hio_read(trkvol, 1, 16, f);
	hio_read8(f);		/* master vol */

	MODULE_INFO();

	D_(D_INFO "Play transpose: %d", transp);

	for (i = 0; i < 64; i++)
		temp_inst[i].transpose += transp;

	/* Scan patterns to determine number of channels */
	mod->chn = 0;
	if ((pos = hio_tell(f)) < 0) {
		return -1;
	}

	for (i = 0; i < mod->pat; i++) {
		int size, plen, chn;

		size = hio_read8(f);	/* pattern control block */
		chn = hio_read8(f);
		if (chn > mod->chn)
			mod->chn = chn;
		hio_read8(f);		/* skip number of rows */
		plen = hio_read16b(f);

		hio_seek(f, size + plen - 4, SEEK_CUR);
	}

	/* Sanity check */
	if (mod->chn > 16) {
		return -1;
	}

	mod->trk = mod->chn * mod->pat;

	if (libxmp_init_pattern(mod) < 0)
		return -1;

	hio_seek(f, pos, SEEK_SET);

	/* Load and convert patterns */
	D_(D_INFO "Stored patterns: %d", mod->pat);

	for (i = 0; i < mod->pat; i++) {
		int size, plen, rows;
		uint8 ctl[4], chn;
		unsigned chmsk;
		uint32 linemask[8], fxmask[8], x;
		int num_masks;
		struct stream stream;

#ifdef MED4_DEBUG
		printf("\n===== PATTERN %d =====\n", i);
		printf("offset = %lx\n", hio_tell(f));
#endif

		size = hio_read8(f);	/* pattern control block */
		if ((pos = hio_tell(f)) < 0) {
			return -1;
		}
		chn = hio_read8(f);
		if (chn > mod->chn) {
			return -1;
		}
		rows = (int)hio_read8(f) + 1;
		plen = hio_read16b(f);
#ifdef MED4_DEBUG
		printf("size = %02x\n", size);
		printf("chn  = %01x\n", chn);
		printf("rows = %01x\n", rows);
		printf("plen = %04x\n", plen);
#endif
		/* read control byte */
		for (j = 0; j < 4; j++) {
			if (rows > j * 64)
				ctl[j] = hio_read8(f);
			else
				break;
#ifdef MED4_DEBUG
			printf("ctl[%d] = %02x\n", j, ctl[j]);

#endif
		}

		if (libxmp_alloc_pattern_tracks(mod, i, rows) < 0)
			return -1;

		/* initialize masks */
		for (y = 0; y < 8; y++) {
			linemask[y] = 0;
			fxmask[y] = 0;
		}

		/* read masks */
		num_masks = 0;
		for (y = 0; y < 8; y++) {
			if (rows > y * 32) {
				int c = ctl[y / 2];
				int s = 4 * (y % 2);
				linemask[y] = c & (0x80 >> s) ? ~0 :
					      c & (0x40 >> s) ? 0 : hio_read32b(f);
				fxmask[y]   = c & (0x20 >> s) ? ~0 :
					      c & (0x10 >> s) ? 0 : hio_read32b(f);
				num_masks++;
#ifdef MED4_DEBUG
				printf("linemask[%d] = %08x\n", y, linemask[y]);
				printf("fxmask[%d]   = %08x\n", y, fxmask[y]);
#endif
			} else {
				break;
			}
		}

		hio_seek(f, pos + size, SEEK_SET);
		stream_init(f, &stream);

		for (y = 0; y < num_masks; y++) {

		for (j = 0; j < 32; j++) {
			int line = y * 32 + j;

			if (line >= rows)
				break;

			if (linemask[y] & 0x80000000) {
				chmsk = stream_read_aligned16(&stream, chn);
				for (k = 0; k < chn; k++, chmsk <<= 1) {
					event = &EVENT(i, k, line);

					if (chmsk & 0x8000) {
						x = stream_read12(&stream);
						event->note = x >> 4;
						if (event->note)
							event->note += 48;
						event->ins  = x & 0x0f;
					}
				}
			}

			if (fxmask[y] & 0x80000000) {
				chmsk = stream_read_aligned16(&stream, chn);
				for (k = 0; k < chn; k++, chmsk <<= 1) {
					event = &EVENT(i, k, line);

					if (chmsk & 0x8000) {
						x = stream_read12(&stream);
						event->fxt = x >> 8;
						event->fxp = x & 0xff;
						fix_effect(event, hexvol);
					}
				}
			}

#ifdef MED4_DEBUG
			printf("%03d ", line);
			for (k = 0; k < 4; k++) {
				event = &EVENT(i, k, line);
				if (event->note)
					printf("%03d", event->note);
				else
					printf("---");
				printf(" %1x%1x%02x ",
					event->ins, event->fxt, event->fxp);
			}
			printf("\n");
#endif

			linemask[y] <<= 1;
			fxmask[y] <<= 1;
		}

		}
		hio_seek(f, pos + size + plen, SEEK_SET);
	}

	mod->ins = num_ins;

	if (libxmp_med_new_module_extras(m) != 0)
		return -1;

	/*
	 * Load samples
	 */
	mask = hio_read32b(f);
	if (mask == MAGIC4('M','E','D','V')) {
		mod->smp = 0;

		if (libxmp_init_instrument(m) < 0)
			return -1;
		hio_seek(f, -4, SEEK_CUR);
		goto parse_iff;
	}
	mask <<= 32;
	mask |= hio_read32b(f);

	mask <<= 1;	/* no instrument #0 */

	/* obtain number of samples */
	if ((pos = hio_tell(f)) < 0) {
		return -1;
	}
	num_smp = 0;
	{
		int _len, _type;
		uint64 _mask = mask;
		for (i = 0; _mask != 0 && i < 64; i++, _mask <<= 1) {
			if ((int64)_mask > 0)
				continue;

			_len = hio_read32b(f);
			_type = (int16)hio_read16b(f);

			if (_type == 0 || _type == -2) {
				num_smp++;
			} else if (_type == -1) {
				if (_len < 22) {
					D_(D_CRIT "invalid synth %d length", i);
					return -1;
				}
				hio_seek(f, 20, SEEK_CUR);
				num_smp += hio_read16b(f);
				_len -= 22;
			}

			if (_len < 0) {
				D_(D_CRIT "invalid sample %d length", i);
				return -1;
			}
			hio_seek(f, _len, SEEK_CUR);
		}
	}
	hio_seek(f, pos, SEEK_SET);

	mod->smp = num_smp;

	if (libxmp_init_instrument(m) < 0) {
		return -1;
	}

	D_(D_INFO "Instruments: %d", mod->ins);

	smp_idx = 0;
	for (i = 0; mask != 0 && i < num_ins; i++, mask <<= 1) {
		int length, type;
		struct SynthInstr synth;
		struct xmp_instrument *xxi;
		struct xmp_subinstrument *sub;
		struct xmp_sample *xxs;
		struct med_instrument_extras *ie;

		if ((int64)mask > 0) {
			continue;
		}

		xxi = &mod->xxi[i];

		length = hio_read32b(f);
		type = (int16)hio_read16b(f);	/* instrument type */

		strncpy((char *)xxi->name, temp_inst[i].name, 32);
		xxi->name[31] = '\0';

		D_(D_INFO "\n[%2X] %-32.32s %d", i, xxi->name, type);

		/* This is very similar to MMD1 synth/hybrid instruments,
		 * but just different enough to be reimplemented here.
		 */

		if (type == -2) {			/* Hybrid */
			pos = hio_tell(f);
			if (pos < 0) {
				return -1;
			}

			hio_read32b(f);	/* ? - MSH 00 */
			hio_read16b(f);	/* ? - ffff */
			hio_read16b(f);	/* ? - 0000 */
			hio_read16b(f);	/* ? - 0000 */
			synth.rep = hio_read16b(f);		/* ? */
			synth.replen = hio_read16b(f);	/* ? */
			synth.voltbllen = hio_read16b(f);
			synth.wftbllen = hio_read16b(f);
			synth.volspeed = hio_read8(f);
			synth.wfspeed = hio_read8(f);
			synth.wforms = hio_read16b(f);

			/* Sanity check */
			if (synth.voltbllen > 128 ||
			    synth.wftbllen > 128 ||
			    synth.wforms > 256) {
				return -1;
			}

			hio_read(synth.voltbl, 1, synth.voltbllen, f);
			hio_read(synth.wftbl, 1, synth.wftbllen, f);
			if (hio_error(f))
				return -1;

			hio_seek(f, pos + hio_read32b(f), SEEK_SET);
			length = hio_read32b(f);
			type = hio_read16b(f);

			if (libxmp_med_new_instrument_extras(xxi) != 0)
				return -1;

			xxi->nsm = 1;
			if (libxmp_alloc_subinstrument(mod, i, 1) < 0)
				return -1;

			sub = &xxi->sub[0];

			ie = MED_INSTRUMENT_EXTRAS(*xxi);
			ie->vts = synth.volspeed;
			ie->wts = synth.wfspeed;
			ie->vtlen = synth.voltbllen;
			ie->wtlen = synth.wftbllen;

			sub->pan = 0x80;
			sub->vol = temp_inst[i].volume;
			sub->xpo = temp_inst[i].transpose;
			sub->sid = smp_idx;
			sub->fin = 0 /*exp_smp.finetune*/;

			xxs = &mod->xxs[smp_idx];

			xxs->len = length;
			xxs->lps = temp_inst[i].loop_start;
			xxs->lpe = temp_inst[i].loop_end;
			xxs->flg = temp_inst[i].loop_end > 2 ?
						XMP_SAMPLE_LOOP : 0;

			D_(D_INFO "  %05x %05x %05x %02x %+03d",
					xxs->len, xxs->lps, xxs->lpe,
					sub->vol, sub->xpo /*, sub->fin >> 4*/);

			if (libxmp_load_sample(m, f, 0, xxs, NULL) < 0)
				return -1;

			smp_idx++;

			if (mmd_alloc_tables(m, i, &synth) != 0)
				return -1;

			continue;
		}

		if (type == -1) {		/* Synthetic */
			pos = hio_tell(f);
			if (pos < 0) {
				return -1;
			}

			hio_read32b(f);	/* ? - MSH 00 */
			hio_read16b(f);	/* ? - ffff */
			hio_read16b(f);	/* ? - 0000 */
			hio_read16b(f);	/* ? - 0000 */
			synth.rep = hio_read16b(f);		/* ? */
			synth.replen = hio_read16b(f);	/* ? */
			synth.voltbllen = hio_read16b(f);
			synth.wftbllen = hio_read16b(f);
			synth.volspeed = hio_read8(f);
			synth.wfspeed = hio_read8(f);
			synth.wforms = hio_read16b(f);

			if (synth.wforms == 0xffff)
				continue;

			/* Sanity check */
			if (synth.voltbllen > 128 ||
			    synth.wftbllen > 128 ||
			    synth.wforms > 64) {
				return -1;
			}

			hio_read(synth.voltbl, 1, synth.voltbllen, f);
			hio_read(synth.wftbl, 1, synth.wftbllen, f);

			for (j = 0; j < synth.wforms; j++)
				synth.wf[j] = hio_read32b(f);

			if (hio_error(f))
				return -1;

			D_(D_INFO "  VS:%02x WS:%02x WF:%02x %02x %+03d",
					synth.volspeed, synth.wfspeed,
					synth.wforms & 0xff,
					temp_inst[i].volume,
					temp_inst[i].transpose /*,
					exp_smp.finetune*/);

			if (libxmp_med_new_instrument_extras(&mod->xxi[i]) != 0)
				return -1;

			mod->xxi[i].nsm = synth.wforms;
			if (libxmp_alloc_subinstrument(mod, i, synth.wforms) < 0)
				return -1;

			ie = MED_INSTRUMENT_EXTRAS(*xxi);
			ie->vts = synth.volspeed;
			ie->wts = synth.wfspeed;
			ie->vtlen = synth.voltbllen;
			ie->wtlen = synth.wftbllen;

			for (j = 0; j < synth.wforms; j++) {
				/* Sanity check */
				if (smp_idx >= num_smp) {
					return -1;
				}

				sub = &xxi->sub[j];

				sub->pan = 0x80;
				sub->vol = 64;
				sub->xpo = -24;
				sub->sid = smp_idx;
				sub->fin = 0 /*exp_smp.finetune*/;

				hio_seek(f, pos + synth.wf[j], SEEK_SET);

				xxs = &mod->xxs[smp_idx];

				xxs->len = hio_read16b(f) * 2;
				xxs->lps = 0;
				xxs->lpe = xxs->len;
				xxs->flg = XMP_SAMPLE_LOOP;

				if (libxmp_load_sample(m, f, 0, xxs, NULL) < 0) {
					return -1;
				}

				smp_idx++;
			}

			if (mmd_alloc_tables(m, i, &synth) != 0)
				return -1;

			hio_seek(f, pos + length, SEEK_SET);
			continue;
		}

		if (type != 0) {
			hio_seek(f, length, SEEK_CUR);
			continue;
		}

		/* instr type is sample */
		xxi->nsm = 1;
		if (libxmp_alloc_subinstrument(mod, i, 1) < 0)
			return -1;

		sub = &xxi->sub[0];

		sub->vol = temp_inst[i].volume;
		sub->pan = 0x80;
		sub->xpo = temp_inst[i].transpose;
		sub->sid = smp_idx;

		/* Sanity check */
		if (smp_idx >= mod->smp)
			return -1;

		xxs = &mod->xxs[smp_idx];

		xxs->len = length;
		xxs->lps = temp_inst[i].loop_start;
		xxs->lpe = temp_inst[i].loop_end;
		xxs->flg = temp_inst[i].loop_end > 2 ? XMP_SAMPLE_LOOP : 0;

		D_(D_INFO "  %04x %04x %04x %c V%02x %+03d",
				xxs->len, mod->xxs[smp_idx].lps, xxs->lpe,
				xxs->flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
				sub->vol, sub->xpo);

		if (libxmp_load_sample(m, f, 0, xxs, NULL) < 0)
			return -1;

		/* Limit range to 3 octave (see MED.El toro) */
		for (j = 0; j < 9; j++) {
			for (k = 0; k < 12; k++) {
				int xpo = 0;

				if (j < 4)
					xpo = 12 * (4 - j);
				else if (j > 6)
					xpo = -12 * (j - 6);

				xxi->map[12 * j + k].xpo = xpo;
			}
		}

		smp_idx++;
	}

	/* Not sure what this was supposed to be, but it isn't present in
	 * Synth-a-sysmic.med or any other MED4 module on ModLand. */
	/*hio_read16b(f);*/	/* unknown */

	/* IFF-like section */
parse_iff:
	while (!hio_eof(f)) {
		int32 id, size, s2, ver;

		if ((id = hio_read32b(f)) <= 0)
			break;

		if ((size = hio_read32b(f)) <= 0)
			break;

		switch (id) {
		case MAGIC4('M','E','D','V'):
			ver = hio_read32b(f);
			size -= 4;
			vermaj = (ver & 0xff00) >> 8;
			vermin = (ver & 0xff);
			D_(D_INFO "MED Version: %d.%0d", vermaj, vermin);
			break;
		case MAGIC4('A','N','N','O'):
			/* annotation */
			s2 = size < 1023 ? size : 1023;
			if ((m->comment = (char *) malloc(s2 + 1)) != NULL) {
				int read_len = hio_read(m->comment, 1, s2, f);
				m->comment[read_len] = '\0';

				D_(D_INFO "Annotation: %s", m->comment);
				size -= s2;
			}
			break;
		case MAGIC4('H','L','D','C'):
			/* hold & decay */
			break;
		}

		hio_seek(f, size, SEEK_CUR);
	}

	m->read_event_type = READ_EVENT_MED;

	return 0;
}
