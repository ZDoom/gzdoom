/* Extended Module Player
 * Copyright (C) 1996-2023 Claudio Matsuoka and Hipolito Carraro Jr
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
 * Fri, 26 Jun 1998 17:45:59 +1000  Andrew Leahy <alf@cit.nepean.uws.edu.au>
 * Finally got it working on the DEC Alpha running DEC UNIX! In the pattern
 * reading loop I found I was getting "0" for (p-patbuf) and "0" for
 * xph.datasize, the next if statement (where it tries to read the patbuf)
 * would then cause a seg_fault.
 *
 * Sun Sep 27 12:07:12 EST 1998  Claudio Matsuoka <claudio@pos.inf.ufpr.br>
 * Extended Module 1.02 stores data in a different order, we must handle
 * this accordingly. MAX_SAMP used as a workaround to check the number of
 * samples recognized by the player.
 */

#include "loader.h"
#include "xm.h"
#ifndef LIBXMP_CORE_PLAYER
#include "vorbis.h"
#endif

static int xm_test(HIO_HANDLE *, char *, const int);
static int xm_load(struct module_data *, HIO_HANDLE *, const int);

const struct format_loader libxmp_loader_xm = {
	"Fast Tracker II",
	xm_test,
	xm_load
};

static int xm_test(HIO_HANDLE *f, char *t, const int start)
{
	char buf[20];

	if (hio_read(buf, 1, 17, f) < 17)	/* ID text */
		return -1;

	if (memcmp(buf, "Extended Module: ", 17))
		return -1;

	libxmp_read_title(f, t, 20);

	return 0;
}

static int load_xm_pattern(struct module_data *m, int num, int version,
			   uint8 *patbuf, HIO_HANDLE *f)
{
	const int headsize = version > 0x0102 ? 9 : 8;
	struct xmp_module *mod = &m->mod;
	struct xm_pattern_header xph;
	struct xmp_event *event;
	uint8 *pat, b;
	int j, k, r;
	int size, size_read;

	xph.length = hio_read32l(f);
	xph.packing = hio_read8(f);
	xph.rows = version > 0x0102 ? hio_read16l(f) : hio_read8(f) + 1;

	/* Sanity check */
	if (xph.rows > 256) {
		goto err;
	}

	xph.datasize = hio_read16l(f);
	hio_seek(f, xph.length - headsize, SEEK_CUR);
	if (hio_error(f)) {
		goto err;
	}

	r = xph.rows;
	if (r == 0) {
		r = 0x100;
	}

	if (libxmp_alloc_pattern_tracks(mod, num, r) < 0) {
		goto err;
	}

	if (xph.datasize == 0) {
		return 0;
	}

	size = xph.datasize;
	pat = patbuf;

	size_read = hio_read(patbuf, 1, size, f);
	if (size_read < size) {
		memset(patbuf + size_read, 0, size - size_read);
	}

	for (j = 0; j < r; j++) {
		for (k = 0; k < mod->chn; k++) {
			/*
			if ((pat - patbuf) >= xph.datasize)
				break;
			*/

			event = &EVENT(num, k, j);

			if (--size < 0) {
				goto err;
			}

			if ((b = *pat++) & XM_EVENT_PACKING) {
				if (b & XM_EVENT_NOTE_FOLLOWS) {
					if (--size < 0)
						goto err;
					event->note = *pat++;
				}
				if (b & XM_EVENT_INSTRUMENT_FOLLOWS) {
					if (--size < 0)
						goto err;
					event->ins = *pat++;
				}
				if (b & XM_EVENT_VOLUME_FOLLOWS) {
					if (--size < 0)
						goto err;
					event->vol = *pat++;
				}
				if (b & XM_EVENT_FXTYPE_FOLLOWS) {
					if (--size < 0)
						goto err;
					event->fxt = *pat++;
				}
				if (b & XM_EVENT_FXPARM_FOLLOWS) {
					if (--size < 0)
						goto err;
					event->fxp = *pat++;
				}
			} else {
				size -= 4;
				if (size < 0)
					goto err;
				event->note = b;
				event->ins = *pat++;
				event->vol = *pat++;
				event->fxt = *pat++;
				event->fxp = *pat++;
			}

			/* Sanity check */
			switch (event->fxt) {
			case 18:
			case 19:
			case 22:
			case 23:
			case 24:
			case 26:
			case 28:
			case 30:
			case 31:
			case 32:
				event->fxt = 0;
			}
			if (event->fxt > 34) {
				event->fxt = 0;
			}

			if (event->note == 0x61) {
				/* See OpenMPT keyoff+instr.xm test case */
				if (event->fxt == 0x0e && MSN(event->fxp) == 0x0d) {
					event->note = XMP_KEY_OFF;
				} else {
					event->note =
					event->ins ? XMP_KEY_FADE : XMP_KEY_OFF;
				}
			} else if (event->note > 0) {
				event->note += 12;
			}

			if (event->fxt == 0x0e) {
				if (MSN(event->fxp) == EX_FINETUNE) {
					unsigned char val = (LSN(event->fxp) - 8) & 0xf;
					event->fxp = (EX_FINETUNE << 4) | val;
				}
				switch (event->fxp) {
				case 0x43:
				case 0x73:
					event->fxp--;
					break;
				}
			}
			if (event->fxt == FX_XF_PORTA && MSN(event->fxp) == 0x09) {
				/* Translate MPT hacks */
				switch (LSN(event->fxp)) {
				case 0x0:	/* Surround off */
				case 0x1:	/* Surround on */
					event->fxt = FX_SURROUND;
					event->fxp = LSN(event->fxp);
					break;
				case 0xe:	/* Play forward */
				case 0xf:	/* Play reverse */
					event->fxt = FX_REVERSE;
					event->fxp = LSN(event->fxp) - 0xe;
				}
			}

			if (!event->vol) {
				continue;
			}

			/* Volume set */
			if ((event->vol >= 0x10) && (event->vol <= 0x50)) {
				event->vol -= 0x0f;
				continue;
			}

			/* Volume column effects */
			switch (event->vol >> 4) {
			case 0x06:	/* Volume slide down */
				event->f2t = FX_VOLSLIDE_2;
				event->f2p = event->vol - 0x60;
				break;
			case 0x07:	/* Volume slide up */
				event->f2t = FX_VOLSLIDE_2;
				event->f2p = (event->vol - 0x70) << 4;
				break;
			case 0x08:	/* Fine volume slide down */
				event->f2t = FX_EXTENDED;
				event->f2p =
				(EX_F_VSLIDE_DN << 4) | (event->vol - 0x80);
				break;
			case 0x09:	/* Fine volume slide up */
				event->f2t = FX_EXTENDED;
				event->f2p =
				(EX_F_VSLIDE_UP << 4) | (event->vol - 0x90);
				break;
			case 0x0a:	/* Set vibrato speed */
				event->f2t = FX_VIBRATO;
				event->f2p = (event->vol - 0xa0) << 4;
				break;
			case 0x0b:	/* Vibrato */
				event->f2t = FX_VIBRATO;
				event->f2p = event->vol - 0xb0;
				break;
			case 0x0c:	/* Set panning */
				event->f2t = FX_SETPAN;
				event->f2p = (event->vol - 0xc0) << 4;
				break;
			case 0x0d:	/* Pan slide left */
				event->f2t = FX_PANSL_NOMEM;
				event->f2p = (event->vol - 0xd0) << 4;
				break;
			case 0x0e:	/* Pan slide right */
				event->f2t = FX_PANSL_NOMEM;
				event->f2p = event->vol - 0xe0;
				break;
			case 0x0f:	/* Tone portamento */
				event->f2t = FX_TONEPORTA;
				event->f2p = (event->vol - 0xf0) << 4;

				/* From OpenMPT TonePortamentoMemory.xm:
				* "Another nice bug (...) is the combination of both
				*  portamento commands (Mx and 3xx) in the same cell:
				*  The 3xx parameter is ignored completely, and the Mx
				*  parameter is doubled. (M2 3FF is the same as M4 000)
				*/
				if (event->fxt == FX_TONEPORTA
				|| event->fxt == FX_TONE_VSLIDE) {
					if (event->fxt == FX_TONEPORTA) {
						event->fxt = 0;
					} else {
						event->fxt = FX_VOLSLIDE;
					}
					event->fxp = 0;

					if (event->f2p < 0x80) {
						event->f2p <<= 1;
					} else {
						event->f2p = 0xff;
					}
				}

				/* From OpenMPT porta-offset.xm:
				* "If there is a portamento command next to an offset
				*  command, the offset command is ignored completely. In
				*  particular, the offset parameter is not memorized."
				*/
				if (event->fxt == FX_OFFSET
				&& event->f2t == FX_TONEPORTA) {
					event->fxt = event->fxp = 0;
				}
				break;
			}
			event->vol = 0;
		}
	}

	return 0;

err:
	return -1;
}

static int load_patterns(struct module_data *m, int version, HIO_HANDLE *f)
{
	struct xmp_module *mod = &m->mod;
	uint8 *patbuf;
	int i, j;

	mod->pat++;
	if (libxmp_init_pattern(mod) < 0) {
		return -1;
	}

	D_(D_INFO "Stored patterns: %d", mod->pat - 1);

	if ((patbuf = (uint8 *) calloc(1, 65536)) == NULL) {
		return -1;
	}

	for (i = 0; i < mod->pat - 1; i++) {
		if (load_xm_pattern(m, i, version, patbuf, f) < 0) {
			goto err;
		}
	}

	/* Alloc one extra pattern */
	{
		int t = i * mod->chn;

		if (libxmp_alloc_pattern(mod, i) < 0) {
			goto err;
		}

		mod->xxp[i]->rows = 64;

		if (libxmp_alloc_track(mod, t, 64) < 0) {
			goto err;
		}

		for (j = 0; j < mod->chn; j++) {
			mod->xxp[i]->index[j] = t;
		}
	}

	free(patbuf);
	return 0;

err:
	free(patbuf);
	return -1;
}

/* Packed structures size */
#define XM_INST_HEADER_SIZE 29
#define XM_INST_SIZE 212

/* grass.near.the.house.xm defines 23 samples in instrument 1. FT2 docs
 * specify at most 16. See https://github.com/libxmp/libxmp/issues/168
 * for more details. */
#define XM_MAX_SAMPLES_PER_INST 32

#ifndef LIBXMP_CORE_PLAYER
#define MAGIC_OGGS	0x4f676753

static int is_ogg_sample(HIO_HANDLE *f, struct xmp_sample *xxs)
{
	/* uint32 size; */
	uint32 id;

	/* Sample must be at least 4 bytes long to be an OGG sample.
	 * Bonnie's Bookstore music.oxm contains zero length samples
	 * followed immediately by OGG samples. */
	if (xxs->len < 4)
		return 0;

	/* size = */ hio_read32l(f);
	id = hio_read32b(f);
	if (hio_error(f) != 0 || hio_seek(f, -8, SEEK_CUR) < 0)
		return 0;

	if (id != MAGIC_OGGS) {		/* copy input data if not Ogg file */
		return 0;
	}

	return 1;
}

static int oggdec(struct module_data *m, HIO_HANDLE *f, struct xmp_sample *xxs, int len)
{
	int i, n, ch, rate, ret, flags = 0;
	uint8 *data;
	int16 *pcm16 = NULL;

	if ((data = (uint8 *)calloc(1, len)) == NULL)
		return -1;

	hio_read32b(f);
	if (hio_error(f) != 0 || hio_read(data, 1, len - 4, f) != len - 4) {
		free(data);
		return -1;
	}

	n = stb_vorbis_decode_memory(data, len, &ch, &rate, &pcm16);
	free(data);

	if (n <= 0) {
		free(pcm16);
		return -1;
	}

	xxs->len = n;

	if ((xxs->flg & XMP_SAMPLE_16BIT) == 0) {
		uint8 *pcm = (uint8 *)pcm16;

		for (i = 0; i < n; i++) {
			pcm[i] = pcm16[i] >> 8;
		}
		pcm = (uint8 *)realloc(pcm16, n);
		if (pcm == NULL) {
			free(pcm16);
			return -1;
		}
		pcm16 = (int16 *)pcm;
	}

	flags |= SAMPLE_FLAG_NOLOAD;
#ifdef WORDS_BIGENDIAN
	flags |= SAMPLE_FLAG_BIGEND;
#endif

	ret = libxmp_load_sample(m, NULL, flags, xxs, pcm16);
	free(pcm16);

	return ret;
}
#endif

static int load_instruments(struct module_data *m, int version, HIO_HANDLE *f)
{
	struct xmp_module *mod = &m->mod;
	struct xm_instrument_header xih;
	struct xm_instrument xi;
	struct xm_sample_header xsh[XM_MAX_SAMPLES_PER_INST];
	int sample_num = 0;
	long total_sample_size;
	int i, j;
	uint8 buf[208];

	D_(D_INFO "Instruments: %d", mod->ins);

	/* ESTIMATED value! We don't know the actual value at this point */
	mod->smp = MAX_SAMPLES;

	if (libxmp_init_instrument(m) < 0) {
		return -1;
	}

	for (i = 0; i < mod->ins; i++) {
		long instr_pos = hio_tell(f);
		struct xmp_instrument *xxi = &mod->xxi[i];

		/* Modules converted with MOD2XM 1.0 always say we have 31
		 * instruments, but file may end abruptly before that. Also covers
		 * XMLiTE stripped modules and truncated files. This test will not
		 * work if file has trailing garbage.
		 *
		 * Note: loading 4 bytes past the instrument header to get the
		 * sample header size (if it exists). This is NOT considered to
		 * be part of the instrument header.
		 */
		if (hio_read(buf, XM_INST_HEADER_SIZE + 4, 1, f) != 1) {
			D_(D_WARN "short read in instrument header data");
			break;
		}

		xih.size = readmem32l(buf);		/* Instrument size */
		memcpy(xih.name, buf + 4, 22);		/* Instrument name */
		xih.type = buf[26];			/* Instrument type (always 0) */
		xih.samples = readmem16l(buf + 27);	/* Number of samples */
		xih.sh_size = readmem32l(buf + 29);	/* Sample header size */

		/* Sanity check */
		if ((int)xih.size < XM_INST_HEADER_SIZE) {
			D_(D_CRIT "instrument %d: instrument header size:%d", i + 1, xih.size);
			return -1;
		}

		if (xih.samples > XM_MAX_SAMPLES_PER_INST || (xih.samples > 0 && xih.sh_size > 0x100)) {
			D_(D_CRIT "instrument %d: samples:%d sample header size:%d", i + 1, xih.samples, xih.sh_size);
			return -1;
		}

		libxmp_instrument_name(mod, i, xih.name, 22);

		xxi->nsm = xih.samples;

		D_(D_INFO "instrument:%2X (%s) samples:%2d", i, xxi->name, xxi->nsm);

		if (xxi->nsm == 0) {
			/* Sample size should be in struct xm_instrument according to
			 * the official format description, but FT2 actually puts it in
			 * struct xm_instrument header. There's a tracker or converter
			 * that follow the specs, so we must handle both cases (see
			 * "Braintomb" by Jazztiz/ART).
			 */

			/* Umm, Cyke O'Path <cyker@heatwave.co.uk> sent me a couple of
			 * mods ("Breath of the Wind" and "Broken Dimension") that
			 * reserve the instrument data space after the instrument header
			 * even if the number of instruments is set to 0. In these modules
			 * the instrument header size is marked as 263. The following
			 * generalization should take care of both cases.
			 */

			if (hio_seek(f, (int)xih.size - (XM_INST_HEADER_SIZE + 4), SEEK_CUR) < 0) {
				return -1;
			}

			continue;
		}

		if (libxmp_alloc_subinstrument(mod, i, xxi->nsm) < 0) {
			return -1;
		}

		/* for BoobieSqueezer (see http://boobie.rotfl.at/)
		 * It works pretty much the same way as Impulse Tracker's sample
		 * only mode, where it will strip off the instrument data.
		 */
		if (xih.size < XM_INST_HEADER_SIZE + XM_INST_SIZE) {
			memset(&xi, 0, sizeof(struct xm_instrument));
			hio_seek(f, xih.size - (XM_INST_HEADER_SIZE + 4), SEEK_CUR);
		} else {
			uint8 *b = buf;

			if (hio_read(buf, 208, 1, f) != 1) {
				D_(D_CRIT "short read in instrument data");
				return -1;
			}

			memcpy(xi.sample, b, 96);		/* Sample map */
			b += 96;

			for (j = 0; j < 24; j++) {
				xi.v_env[j] = readmem16l(b);	/* Points for volume envelope */
				b += 2;
			}

			for (j = 0; j < 24; j++) {
				xi.p_env[j] = readmem16l(b);	/* Points for pan envelope */
				b += 2;
			}

			xi.v_pts = *b++;		/* Number of volume points */
			xi.p_pts = *b++;		/* Number of pan points */
			xi.v_sus = *b++;		/* Volume sustain point */
			xi.v_start = *b++;		/* Volume loop start point */
			xi.v_end = *b++;		/* Volume loop end point */
			xi.p_sus = *b++;		/* Pan sustain point */
			xi.p_start = *b++;		/* Pan loop start point */
			xi.p_end = *b++;		/* Pan loop end point */
			xi.v_type = *b++;		/* Bit 0:On 1:Sustain 2:Loop */
			xi.p_type = *b++;		/* Bit 0:On 1:Sustain 2:Loop */
			xi.y_wave = *b++;		/* Vibrato waveform */
			xi.y_sweep = *b++;		/* Vibrato sweep */
			xi.y_depth = *b++;		/* Vibrato depth */
			xi.y_rate = *b++;		/* Vibrato rate */
			xi.v_fade = readmem16l(b);	/* Volume fadeout */

			/* Skip reserved space */
			if (hio_seek(f, (int)xih.size - (XM_INST_HEADER_SIZE + XM_INST_SIZE), SEEK_CUR) < 0) {
				return -1;
			}

			/* Envelope */
			xxi->rls = xi.v_fade << 1;
			xxi->aei.npt = xi.v_pts;
			xxi->aei.sus = xi.v_sus;
			xxi->aei.lps = xi.v_start;
			xxi->aei.lpe = xi.v_end;
			xxi->aei.flg = xi.v_type;
			xxi->pei.npt = xi.p_pts;
			xxi->pei.sus = xi.p_sus;
			xxi->pei.lps = xi.p_start;
			xxi->pei.lpe = xi.p_end;
			xxi->pei.flg = xi.p_type;

			if (xxi->aei.npt <= 0 || xxi->aei.npt > 12 /*XMP_MAX_ENV_POINTS */ ) {
				xxi->aei.flg &= ~XMP_ENVELOPE_ON;
			} else {
				memcpy(xxi->aei.data, xi.v_env, xxi->aei.npt * 4);
			}

			if (xxi->pei.npt <= 0 || xxi->pei.npt > 12 /*XMP_MAX_ENV_POINTS */ ) {
				xxi->pei.flg &= ~XMP_ENVELOPE_ON;
			} else {
				memcpy(xxi->pei.data, xi.p_env, xxi->pei.npt * 4);
			}

			for (j = 12; j < 108; j++) {
				xxi->map[j].ins = xi.sample[j - 12];
				if (xxi->map[j].ins >= xxi->nsm)
					xxi->map[j].ins = 0xff;
			}
		}

		/* Read subinstrument and sample parameters */

		for (j = 0; j < xxi->nsm; j++, sample_num++) {
			struct xmp_subinstrument *sub = &xxi->sub[j];
			struct xmp_sample *xxs;
			uint8 *b = buf;

			D_(D_INFO "  sample index:%d sample id:%d", j, sample_num);

			if (sample_num >= mod->smp) {
				if (libxmp_realloc_samples(m, mod->smp * 3 / 2) < 0)
					return -1;
			}
			xxs = &mod->xxs[sample_num];

			if (hio_read(buf, 40, 1, f) != 1) {
				D_(D_CRIT "short read in sample data");
				return -1;
			}

			xsh[j].length = readmem32l(b);	/* Sample length */
			b += 4;

			/* Sanity check */
			if (xsh[j].length > MAX_SAMPLE_SIZE) {
				D_(D_CRIT "sanity check: %d: bad sample size", j);
				return -1;
			}

			xsh[j].loop_start = readmem32l(b);	/* Sample loop start */
			b += 4;
			xsh[j].loop_length = readmem32l(b);	/* Sample loop length */
			b += 4;
			xsh[j].volume = *b++;	/* Volume */
			xsh[j].finetune = *b++;	/* Finetune (-128..+127) */
			xsh[j].type = *b++;	/* Flags */
			xsh[j].pan = *b++;	/* Panning (0-255) */
			xsh[j].relnote = *(int8 *) b++;	/* Relative note number */
			xsh[j].reserved = *b++;
			memcpy(xsh[j].name, b, 22);

			sub->vol = xsh[j].volume;
			sub->pan = xsh[j].pan;
			sub->xpo = xsh[j].relnote;
			sub->fin = xsh[j].finetune;
			sub->vwf = xi.y_wave;
			sub->vde = xi.y_depth << 2;
			sub->vra = xi.y_rate;
			sub->vsw = xi.y_sweep;
			sub->sid = sample_num;

			libxmp_copy_adjust(xxs->name, xsh[j].name, 22);

			xxs->len = xsh[j].length;
			xxs->lps = xsh[j].loop_start;
			xxs->lpe = xsh[j].loop_start + xsh[j].loop_length;

			xxs->flg = 0;
			if (xsh[j].type & XM_SAMPLE_16BIT) {
				xxs->flg |= XMP_SAMPLE_16BIT;
				xxs->len >>= 1;
				xxs->lps >>= 1;
				xxs->lpe >>= 1;
			}
			if (xsh[j].type & XM_SAMPLE_STEREO) {
				/* xxs->flg |= XMP_SAMPLE_STEREO; */
				xxs->len >>= 1;
				xxs->lps >>= 1;
				xxs->lpe >>= 1;
			}

			xxs->flg |= xsh[j].type & XM_LOOP_FORWARD ? XMP_SAMPLE_LOOP : 0;
			xxs->flg |= xsh[j].type & XM_LOOP_PINGPONG ? XMP_SAMPLE_LOOP | XMP_SAMPLE_LOOP_BIDIR : 0;

			D_(D_INFO "  size:%06x loop start:%06x loop end:%06x %c V%02x F%+04d P%02x R%+03d %s",
			   mod->xxs[sub->sid].len,
			   mod->xxs[sub->sid].lps,
			   mod->xxs[sub->sid].lpe,
			   mod->xxs[sub->sid].flg & XMP_SAMPLE_LOOP_BIDIR ? 'B' :
			   mod->xxs[sub->sid].flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
			   sub->vol, sub->fin, sub->pan, sub->xpo,
			   mod->xxs[sub->sid].flg & XMP_SAMPLE_16BIT ? " (16 bit)" : "");
		}

		/* Read actual sample data */

		total_sample_size = 0;
		for (j = 0; j < xxi->nsm; j++) {
			struct xmp_subinstrument *sub = &xxi->sub[j];
			struct xmp_sample *xxs = &mod->xxs[sub->sid];
			int flags;

			flags = SAMPLE_FLAG_DIFF;
#ifndef LIBXMP_CORE_PLAYER
			if (xsh[j].reserved == 0xad) {
				flags = SAMPLE_FLAG_ADPCM;
			}
#endif

			if (version > 0x0103) {
			        D_(D_INFO "  read sample: index:%d sample id:%d", j, sub->sid);

#ifndef LIBXMP_CORE_PLAYER
				if (is_ogg_sample(f, xxs)) {
					if (oggdec(m, f, xxs, xsh[j].length) < 0) {
						return -1;
					}

					D_(D_INFO "  sample is vorbis");
					total_sample_size += xsh[j].length;
					continue;
				}
#endif

				if (libxmp_load_sample(m, f, flags, xxs, NULL) < 0) {
					return -1;
				}
				if (flags & SAMPLE_FLAG_ADPCM) {
			                D_(D_INFO "  sample is adpcm");
					total_sample_size += 16 + ((xsh[j].length + 1) >> 1);
				} else {
					total_sample_size += xsh[j].length;
				}

				/* TODO: implement stereo samples.
				 * For now, just skip the right channel. */
				if (xsh[j].type & XM_SAMPLE_STEREO) {
					hio_seek(f, xsh[j].length >> 1, SEEK_CUR);
				}
			}
		}

		/* Reposition correctly in case of 16-bit sample having odd in-file length.
		 * See "Lead Lined for '99", reported by Dennis Mulleneers.
		 */
		if (hio_seek(f, instr_pos + xih.size + 40 * xih.samples + total_sample_size, SEEK_SET) < 0) {
			return -1;
		}
	}

	/* Final sample number adjustment */
	if (libxmp_realloc_samples(m, sample_num) < 0) {
		return -1;
	}

	return 0;
}

static int xm_load(struct module_data *m, HIO_HANDLE * f, const int start)
{
	struct xmp_module *mod = &m->mod;
	int i, j;
	struct xm_file_header xfh;
	char tracker_name[21];
#ifndef LIBXMP_CORE_PLAYER
	int claims_ft2 = 0;
	int is_mpt_116 = 0;
#endif
	int len;
	uint8 buf[80];

	LOAD_INIT();

	if (hio_read(buf, 80, 1, f) != 1) {
		D_(D_CRIT "error reading header");
		return -1;
	}

	memcpy(xfh.id, buf, 17);		/* ID text */
	memcpy(xfh.name, buf + 17, 20);		/* Module name */
	/* */					/* skip 0x1a */
	memcpy(xfh.tracker, buf + 38, 20);	/* Tracker name */
	xfh.version = readmem16l(buf + 58);	/* Version number, minor-major */
	xfh.headersz = readmem32l(buf + 60);	/* Header size */
	xfh.songlen = readmem16l(buf + 64);	/* Song length */
	xfh.restart = readmem16l(buf + 66);	/* Restart position */
	xfh.channels = readmem16l(buf + 68);	/* Number of channels */
	xfh.patterns = readmem16l(buf + 70);	/* Number of patterns */
	xfh.instruments = readmem16l(buf + 72);	/* Number of instruments */
	xfh.flags = readmem16l(buf + 74);	/* 0=Amiga freq table, 1=Linear */
	xfh.tempo = readmem16l(buf + 76);	/* Default tempo */
	xfh.bpm = readmem16l(buf + 78);		/* Default BPM */

	/* Sanity checks */
	if (xfh.songlen > 256) {
		D_(D_CRIT "bad song length: %d", xfh.songlen);
		return -1;
	}
	if (xfh.patterns > 256) {
		D_(D_CRIT "bad pattern count: %d", xfh.patterns);
		return -1;
	}
	if (xfh.instruments > 255) {
		D_(D_CRIT "bad instrument count: %d", xfh.instruments);
		return -1;
	}

	if (xfh.restart > 255) {
		D_(D_CRIT "bad restart position: %d", xfh.restart);
		return -1;
	}
	if (xfh.channels > XMP_MAX_CHANNELS) {
		D_(D_CRIT "bad channel count: %d", xfh.channels);
		return -1;
	}

	/* FT2 and MPT allow up to 255 BPM. OpenMPT allows up to 1000 BPM. */
	if (xfh.tempo >= 32 || xfh.bpm < 32 || xfh.bpm > 1000) {
		if (memcmp("MED2XM", xfh.tracker, 6)) {
			D_(D_CRIT "bad tempo or BPM: %d %d", xfh.tempo, xfh.bpm);
			return -1;
		}
	}

	/* Honor header size -- needed by BoobieSqueezer XMs */
	len = xfh.headersz - 0x14;
	if (len < 0 || len > 256) {
		D_(D_CRIT "bad XM header length: %d", len);
		return -1;
	}

	memset(xfh.order, 0, sizeof(xfh.order));
	if (hio_read(xfh.order, len, 1, f) != 1) {	/* Pattern order table */
		D_(D_CRIT "error reading orders");
		return -1;
	}

	strncpy(mod->name, (char *)xfh.name, 20);

	mod->len = xfh.songlen;
	mod->chn = xfh.channels;
	mod->pat = xfh.patterns;
	mod->ins = xfh.instruments;
	mod->rst = xfh.restart;
	mod->spd = xfh.tempo;
	mod->bpm = xfh.bpm;
	mod->trk = mod->chn * mod->pat + 1;

	m->c4rate = C4_NTSC_RATE;
	m->period_type = xfh.flags & XM_LINEAR_PERIOD_MODE ?  PERIOD_LINEAR : PERIOD_AMIGA;

	memcpy(mod->xxo, xfh.order, mod->len);
	/*tracker_name[20] = 0;*/
	snprintf(tracker_name, 21, "%-20.20s", xfh.tracker);
	for (i = 20; i >= 0; i--) {
		if (tracker_name[i] == 0x20)
			tracker_name[i] = 0;
		if (tracker_name[i])
			break;
	}

	/* OpenMPT accurately emulates weird FT2 bugs */
	if (!strncmp(tracker_name, "FastTracker v2.00", 17)) {
		m->quirk |= QUIRK_FT2BUGS;
#ifndef LIBXMP_CORE_PLAYER
		claims_ft2 = 1;
#endif
	} else if (!strncmp(tracker_name, "OpenMPT ", 8)) {
		m->quirk |= QUIRK_FT2BUGS;
	}
#ifndef LIBXMP_CORE_PLAYER
	if (xfh.headersz == 0x0113) {
		strcpy(tracker_name, "unknown tracker");
		m->quirk &= ~QUIRK_FT2BUGS;
	} else if (*tracker_name == 0) {
		strcpy(tracker_name, "Digitrakker");	/* best guess */
		m->quirk &= ~QUIRK_FT2BUGS;
	}

	/* See MMD1 loader for explanation */
	if (!strncmp(tracker_name, "MED2XM by J.Pynnone", 19)) {
		if (mod->bpm <= 10) {
			mod->bpm = 125 * (0x35 - mod->bpm * 2) / 33;
		}
		m->quirk &= ~QUIRK_FT2BUGS;
	}

	if (!strncmp(tracker_name, "FastTracker v 2.00", 18)) {
		strcpy(tracker_name, "old ModPlug Tracker");
		m->quirk &= ~QUIRK_FT2BUGS;
		is_mpt_116 = 1;
	}

	libxmp_set_type(m, "%s XM %d.%02d", tracker_name, xfh.version >> 8, xfh.version & 0xff);
#else
	libxmp_set_type(m, tracker_name);
#endif

	MODULE_INFO();

	/* Honor header size */
	if (hio_seek(f, start + xfh.headersz + 60, SEEK_SET) < 0) {
		return -1;
	}

	/* XM 1.02/1.03 has a different patterns and instruments order */
	if (xfh.version <= 0x0103) {
		if (load_instruments(m, xfh.version, f) < 0) {
			return -1;
		}
		if (load_patterns(m, xfh.version, f) < 0) {
			return -1;
		}
	} else {
		if (load_patterns(m, xfh.version, f) < 0) {
			return -1;
		}
		if (load_instruments(m, xfh.version, f) < 0) {
			return -1;
		}
	}

	D_(D_INFO "Stored samples: %d", mod->smp);

	/* XM 1.02 stores all samples after the patterns */
	if (xfh.version <= 0x0103) {
		for (i = 0; i < mod->ins; i++) {
			for (j = 0; j < mod->xxi[i].nsm; j++) {
				int sid = mod->xxi[i].sub[j].sid;
				if (libxmp_load_sample(m, f, SAMPLE_FLAG_DIFF, &mod->xxs[sid], NULL) < 0) {
					return -1;
				}
			}
		}
	}

#ifndef LIBXMP_CORE_PLAYER
	/* Load MPT properties from the end of the file. */
	while (1) {
		uint32 ext = hio_read32b(f);
		uint32 sz = hio_read32l(f);
		int known = 0;

		if (hio_error(f) || sz > 0x7fffffff /* INT32_MAX */)
			break;

		switch (ext) {
		case MAGIC4('t','e','x','t'):		/* Song comment */
			known = 1;
			if (m->comment != NULL)
				break;

			if ((m->comment = (char *)malloc(sz + 1)) == NULL)
				break;

			sz = hio_read(m->comment, 1, sz, f);
			m->comment[sz] = '\0';

			for (i = 0; i < (int)sz; i++) {
				int b = m->comment[i];
				if (b == '\r') {
					m->comment[i] = '\n';
				} else if ((b < 32 || b > 127) && b != '\n'
					   && b != '\t') {
					m->comment[i] = '.';
				}
			}
			break;

		case MAGIC4('M','I','D','I'):		/* MIDI config */
		case MAGIC4('P','N','A','M'):		/* Pattern names */
		case MAGIC4('C','N','A','M'):		/* Channel names */
		case MAGIC4('C','H','F','X'):		/* Channel plugins */
		case MAGIC4('X','T','P','M'):		/* Inst. extensions */
			known = 1;
			/* fall-through */

		default:
			/* Plugin definition */
			if ((ext & MAGIC4('F','X', 0, 0)) == MAGIC4('F','X', 0, 0))
				known = 1;

			if (sz) hio_seek(f, sz, SEEK_CUR);
			break;
		}

		if(known && claims_ft2)
			is_mpt_116 = 1;

		if (ext == MAGIC4('X','T','P','M'))
			break;
	}

	if (is_mpt_116) {
		libxmp_set_type(m, "ModPlug Tracker 1.16 XM %d.%02d",
				xfh.version >> 8, xfh.version & 0xff);

		m->mvolbase = 48;
		m->mvol = 48;
		libxmp_apply_mpt_preamp(m);
	}
#endif

	for (i = 0; i < mod->chn; i++) {
		mod->xxc[i].pan = 0x80;
	}

	m->quirk |= QUIRKS_FT2;
	m->read_event_type = READ_EVENT_FT2;

	return 0;
}
