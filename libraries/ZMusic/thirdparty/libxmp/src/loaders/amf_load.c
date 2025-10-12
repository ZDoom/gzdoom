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

/* AMF loader written based on the format specs by Miodrag Vallat with
 * fixes by Andre Timmermans.
 *
 * The AMF format is the internal format used by DSMI, the DOS Sound and Music
 * Interface, which is the engine of DMP. As DMP was able to play more and more
 * module formats, the format evolved to support more features. There were 5
 * official formats, numbered from 10 (AMF 1.0) to 14 (AMF 1.4).
 */

#include "loader.h"
#include "../period.h"


static int amf_test(HIO_HANDLE *, char *, const int);
static int amf_load (struct module_data *, HIO_HANDLE *, const int);

const struct format_loader libxmp_loader_amf = {
	"DSMI Advanced Module Format",
	amf_test,
	amf_load
};

static int amf_test(HIO_HANDLE * f, char *t, const int start)
{
	char buf[4];
	int ver;

	if (hio_read(buf, 1, 3, f) < 3)
		return -1;

	if (buf[0] != 'A' || buf[1] != 'M' || buf[2] != 'F')
		return -1;

	ver = hio_read8(f);
	if ((ver != 0x01 && ver < 0x08) || ver > 0x0e)
		return -1;

	libxmp_read_title(f, t, 32);

	return 0;
}


static int amf_load(struct module_data *m, HIO_HANDLE *f, const int start)
{
	struct xmp_module *mod = &m->mod;
	int i, j;
	struct xmp_event *event;
	uint8 buf[1024];
	int *trkmap, newtrk;
	int no_loopend = 0;
	int ver;

	LOAD_INIT();

	hio_read(buf, 1, 3, f);
	ver = hio_read8(f);

	if (hio_read(buf, 1, 32, f) != 32)
		return -1;

	memcpy(mod->name, buf, 32);
	mod->name[32] = '\0';
	libxmp_set_type(m, "DSMI %d.%d AMF", ver / 10, ver % 10);

	mod->ins = hio_read8(f);
	mod->len = hio_read8(f);
	mod->trk = hio_read16l(f);
	mod->chn = 4;

	if (ver >= 0x09) {
		mod->chn = hio_read8(f);
	}

	/* Sanity check */
	if (mod->ins == 0 || mod->len == 0 || mod->trk == 0
		|| mod->chn == 0 || mod->chn > XMP_MAX_CHANNELS) {
		return -1;
	}

	mod->smp = mod->ins;
	mod->pat = mod->len;

	if (ver == 0x09 || ver == 0x0a)
		hio_read(buf, 1, 16, f);	/* channel remap table */

	if (ver >= 0x0b) {
		int pan_len = ver >= 0x0c ? 32 : 16;

		if (hio_read(buf, 1, pan_len, f) != pan_len)	/* panning table */
			return -1;

		for (i = 0; i < pan_len; i++) {
			mod->xxc[i].pan = 0x80 + 2 * (int8)buf[i];
		}
	}

	if (ver >= 0x0d) {
		mod->bpm = hio_read8(f);
		mod->spd = hio_read8(f);
	}

	m->c4rate = C4_NTSC_RATE;

	MODULE_INFO();


	/* Orders */

	/*
	 * Andre Timmermans <andre.timmermans@atos.net> says:
	 *
	 * Order table: track numbers in this table are not explained,
	 * but as you noticed you have to perform -1 to obtain the index
	 * in the track table. For value 0, found in some files, I think
	 * it means an empty track.
	 *
	 * 2021 note: this is misleading. Do not subtract 1 from the logical
	 * track values found in the order table; load the mapping table to
	 * index 1 instead.
	 */

	for (i = 0; i < mod->len; i++)
		mod->xxo[i] = i;

	D_(D_INFO "Stored patterns: %d", mod->pat);

	mod->xxp = (struct xmp_pattern **) calloc(mod->pat, sizeof(struct xmp_pattern *));
	if (mod->xxp == NULL)
		return -1;

	for (i = 0; i < mod->pat; i++) {
		if (libxmp_alloc_pattern(mod, i) < 0)
			return -1;

		mod->xxp[i]->rows = ver >= 0x0e ? hio_read16l(f) : 64;

		if (mod->xxp[i]->rows > 256)
			return -1;

		for (j = 0; j < mod->chn; j++) {
			uint16 t = hio_read16l(f);
			mod->xxp[i]->index[j] = t;
		}
	}

	/* Instruments */

	if (libxmp_init_instrument(m) < 0)
		return -1;

	/* Probe for 2-byte loop start 1.0 format
	 * in facing_n.amf and sweetdrm.amf have only the sample
	 * loop start specified in 2 bytes
	 *
	 * These modules are an early variant of the AMF 1.0 format. Since
	 * normal AMF 1.0 files have 32-bit lengths/loop start/loop end,
	 * this is possibly caused by these fields having been expanded for
	 * the 1.0 format, but M2AMF 1.3 writing instrument structs with
	 * the old length (which would explain the missing 6 bytes).
	 */
	if (ver == 0x0a) {
		uint8 b;
		uint32 len, val;
		long pos = hio_tell(f);
		if (pos < 0) {
			return -1;
		}
		for (i = 0; i < mod->ins; i++) {
			b = hio_read8(f);
			if (b != 0 && b != 1) {
				no_loopend = 1;
				break;
			}
			hio_seek(f, 32 + 13, SEEK_CUR);
			if (hio_read32l(f) > (uint32)mod->ins) { /* check index */
				no_loopend = 1;
				break;
			}
			len = hio_read32l(f);
			if (len > 0x100000) {		/* check len */
				no_loopend = 1;
				break;
			}
			if (hio_read16l(f) == 0x0000) {	/* check c2spd */
				no_loopend = 1;
				break;
			}
			if (hio_read8(f) > 0x40) {	/* check volume */
				no_loopend = 1;
				break;
			}
			val = hio_read32l(f);		/* check loop start */
			if (val > len) {
				no_loopend = 1;
				break;
			}
			val = hio_read32l(f);		/* check loop end */
			if (val > len) {
				no_loopend = 1;
				break;
			}
		}
		hio_seek(f, pos, SEEK_SET);
	}

	if (no_loopend) {
		D_(D_INFO "Detected AMF 1.0 truncated instruments.");
	}

	for (i = 0; i < mod->ins; i++) {
		int c2spd;

		if (libxmp_alloc_subinstrument(mod, i, 1) < 0)
			return -1;

		hio_read8(f);

		hio_read(buf, 1, 32, f);
		libxmp_instrument_name(mod, i, buf, 32);

		hio_read(buf, 1, 13, f);	/* sample name */
		hio_read32l(f);			/* sample index */

		mod->xxi[i].nsm = 1;
		mod->xxi[i].sub[0].sid = i;
		mod->xxi[i].sub[0].pan = 0x80;

		if (ver >= 0x0a) {
			mod->xxs[i].len = hio_read32l(f);
		} else {
			mod->xxs[i].len = hio_read16l(f);
		}
		c2spd = hio_read16l(f);
		libxmp_c2spd_to_note(c2spd, &mod->xxi[i].sub[0].xpo, &mod->xxi[i].sub[0].fin);
		mod->xxi[i].sub[0].vol = hio_read8(f);

		/*
		 * Andre Timmermans <andre.timmermans@atos.net> says:
		 *
		 * [Miodrag Vallat's] doc tells that in version 1.0 only
		 * sample loop start is present (2 bytes) but the files I
		 * have tells both start and end are present (2*4 bytes).
		 * Maybe it should be read as version < 1.0.
		 *
		 * CM: confirmed with Maelcum's "The tribal zone"
		 */

		if (no_loopend != 0) {
			mod->xxs[i].lps = hio_read16l(f);
			mod->xxs[i].lpe = mod->xxs[i].len;
		} else if (ver >= 0x0a) {
			mod->xxs[i].lps = hio_read32l(f);
			mod->xxs[i].lpe = hio_read32l(f);
		} else {
			/* Non-looping samples are stored with lpe=-1, not 0. */
			mod->xxs[i].lps = hio_read16l(f);
			mod->xxs[i].lpe = hio_read16l(f);

			if (mod->xxs[i].lpe == 0xffff)
				mod->xxs[i].lpe = 0;
		}

		if (no_loopend != 0) {
			mod->xxs[i].flg = mod->xxs[i].lps > 0 ? XMP_SAMPLE_LOOP : 0;
		} else {
			mod->xxs[i].flg = mod->xxs[i].lpe > mod->xxs[i].lps ?
							XMP_SAMPLE_LOOP : 0;
		}

		D_(D_INFO "[%2X] %-32.32s %05x %05x %05x %c V%02x %5d",
			i, mod->xxi[i].name, mod->xxs[i].len, mod->xxs[i].lps,
			mod->xxs[i].lpe, mod->xxs[i].flg & XMP_SAMPLE_LOOP ?
			'L' : ' ', mod->xxi[i].sub[0].vol, c2spd);
	}

	if (hio_error(f))
		return -1;


	/* Tracks */

	/* Index 0 is a blank track that isn't stored in the file. To keep
	 * things simple, load the mapping table to index 1 so the table
	 * index is the same as the logical track value. Older versions
	 * attempted to remap it to index 0 and subtract 1 from the index,
	 * breaking modules that directly reference the empty track in the
	 * order table (see "cosmos st.amf").
	 */
	trkmap = (int *) calloc(mod->trk + 1, sizeof(int));
	if (trkmap == NULL)
		return -1;
	newtrk = 0;

	for (i = 1; i <= mod->trk; i++) {		/* read track table */
		uint16 t;
		t = hio_read16l(f);
		trkmap[i] = t;
		if (t > newtrk) newtrk = t;
	}

	for (i = 0; i < mod->pat; i++) {		/* read track table */
		for (j = 0; j < mod->chn; j++) {
			uint16 k = mod->xxp[i]->index[j];

			/* Use empty track if an invalid track is requested
			 * (such as in Lasse Makkonen "faster and louder")
			 */
			if (k > mod->trk)
				k = 0;
			mod->xxp[i]->index[j] = trkmap[k];
		}
	}

	mod->trk = newtrk + 1;		/* + empty track */
	free(trkmap);

	if (hio_error(f))
		return -1;

	D_(D_INFO "Stored tracks: %d", mod->trk - 1);

	mod->xxt = (struct xmp_track **) calloc(mod->trk, sizeof(struct xmp_track *));
	if (mod->xxt == NULL)
		return -1;

	/* Alloc track 0 as empty track */
	if (libxmp_alloc_track(mod, 0, 64) < 0)
		return -1;

	/* Alloc rest of the tracks */
	for (i = 1; i < mod->trk; i++) {
		uint8 t1, t2, t3;
		int size;

		if (libxmp_alloc_track(mod, i, 64) < 0)	/* FIXME! */
			return -1;

		/* Previous versions loaded this as a 24-bit value, but it's
		 * just a word. The purpose of the third byte is unknown, and
		 * DSMI just ignores it.
		 */
		size = hio_read16l(f);
		hio_read8(f);

		if (hio_error(f))
			return -1;

		/* Version 0.1 AMFs do not count the end-of-track marker in
		 * the event count, so add 1. This hasn't been verified yet. */
		if (ver == 0x01 && size != 0)
			size++;

		for (j = 0; j < size; j++) {
			t1 = hio_read8(f);			/* row */
			t2 = hio_read8(f);			/* type */
			t3 = hio_read8(f);			/* parameter */

			if (t1 == 0xff && t2 == 0xff && t3 == 0xff)
				break;

			/* If an event is encountered past the end of the
			 * track, treat it the same as the track end. This is
			 * encountered in "Avoid.amf".
			 */
			if (t1 >= mod->xxt[i]->rows) {
				if (hio_seek(f, (size - j - 1) * 3, SEEK_CUR))
					return -1;

				break;
			}

			event = &mod->xxt[i]->event[t1];

			if (t2 < 0x7f) {		/* note */
				if (t2 > 0)
					event->note = t2 + 1;
				/* A volume value of 0xff indicates that
				 * the old volume should be reused. Prior
				 * libxmp versions also forgot to add 1 here.
				 */
				event->vol = (t3 != 0xff) ? (t3 + 1) : 0;
			} else if (t2 == 0x7f) {	/* note retrigger */

				/* AMF.TXT claims that this duplicates the
				 * previous event, which is a lie. M2AMF emits
				 * this for MODs when an instrument change
				 * occurs with no note, indicating it should
				 * retrigger (like in PT 2.3). Ignore this.
				 *
				 * See: "aladdin - aladd.pc.amf", "eye.amf".
				 */
			} else if (t2 == 0x80) {	/* instrument */
				event->ins = t3 + 1;
			} else  {			/* effects */
				uint8 fxp, fxt;

				fxp = fxt = 0;

				switch (t2) {
				case 0x81:
					fxt = FX_S3M_SPEED;
					fxp = t3;
					break;
				case 0x82:
					if ((int8)t3 > 0) {
						fxt = FX_VOLSLIDE;
						fxp = t3 << 4;
					} else {
						fxt = FX_VOLSLIDE;
						fxp = -(int8)t3 & 0x0f;
					}
					break;
				case 0x83:
					/* See volume notes above. Previous
					 * releases forgot to add 1 here. */
					event->vol = (t3 + 1);
					break;
				case 0x84:
					/* AT: Not explained for 0x84, pitch
					 * slide, value 0x00 corresponds to
					 * S3M E00 and 0x80 stands for S3M F00
					 * (I checked with M2AMF)
					 */
					if ((int8)t3 >= 0) {
						fxt = FX_PORTA_DN;
						fxp = t3;
					} else if (t3 == 0x80) {
						fxt = FX_PORTA_UP;
						fxp = 0;
					} else {
						fxt = FX_PORTA_UP;
						fxp = -(int8)t3;
					}
					break;
				case 0x85:
					/* porta abs -- unknown */
					break;
				case 0x86:
					fxt = FX_TONEPORTA;
					fxp = t3;
					break;

				/* AT: M2AMF maps both tremolo and tremor to
				 * 0x87. Since tremor is only found in certain
				 * formats, maybe it would be better to
				 * consider it is a tremolo.
				 */
				case 0x87:
					fxt = FX_TREMOLO;
					fxp = t3;
					break;
				case 0x88:
					fxt = FX_ARPEGGIO;
					fxp = t3;
					break;
				case 0x89:
					fxt = FX_VIBRATO;
					fxp = t3;
					break;
				case 0x8a:
					if ((int8)t3 > 0) {
						fxt = FX_TONE_VSLIDE;
						fxp = t3 << 4;
					} else {
						fxt = FX_TONE_VSLIDE;
						fxp = -(int8)t3 & 0x0f;
					}
					break;
				case 0x8b:
					if ((int8)t3 > 0) {
						fxt = FX_VIBRA_VSLIDE;
						fxp = t3 << 4;
					} else {
						fxt = FX_VIBRA_VSLIDE;
						fxp = -(int8)t3 & 0x0f;
					}
					break;
				case 0x8c:
					fxt = FX_BREAK;
					fxp = t3;
					break;
				case 0x8d:
					fxt = FX_JUMP;
					fxp = t3;
					break;
				case 0x8e:
					/* sync -- unknown */
					break;
				case 0x8f:
					fxt = FX_EXTENDED;
					fxp = (EX_RETRIG << 4) | (t3 & 0x0f);
					break;
				case 0x90:
					fxt = FX_OFFSET;
					fxp = t3;
					break;
				case 0x91:
					if ((int8)t3 > 0) {
						fxt = FX_EXTENDED;
						fxp = (EX_F_VSLIDE_UP << 4) |
							(t3 & 0x0f);
					} else {
						fxt = FX_EXTENDED;
						fxp = (EX_F_VSLIDE_DN << 4) |
							(t3 & 0x0f);
					}
					break;
				case 0x92:
					if ((int8)t3 > 0) {
						fxt = FX_PORTA_DN;
						fxp = 0xf0 | (fxp & 0x0f);
					} else {
						fxt = FX_PORTA_UP;
						fxp = 0xf0 | (fxp & 0x0f);
					}
					break;
				case 0x93:
					fxt = FX_EXTENDED;
					fxp = (EX_DELAY << 4) | (t3 & 0x0f);
					break;
				case 0x94:
					fxt = FX_EXTENDED;
					fxp = (EX_CUT << 4) | (t3 & 0x0f);
					break;
				case 0x95:
					fxt = FX_SPEED;
					if (t3 < 0x21)
						t3 = 0x21;
					fxp = t3;
					break;
				case 0x96:
					if ((int8)t3 > 0) {
						fxt = FX_PORTA_DN;
						fxp = 0xe0 | (fxp & 0x0f);
					} else {
						fxt = FX_PORTA_UP;
						fxp = 0xe0 | (fxp & 0x0f);
					}
					break;
				case 0x97:
					/* Same as S3M pan, but param is offset by -0x40. */
					if (t3 == 0x64) { /* 0xA4 - 0x40 */
						fxt = FX_SURROUND;
						fxp = 1;
					} else if (t3 >= 0xC0 || t3 <= 0x40) {
						int pan = ((int8)t3 << 1) + 0x80;
						fxt = FX_SETPAN;
						fxp = MIN(0xff, pan);
					}
					break;
				}

				event->fxt = fxt;
				event->fxp = fxp;
			}
		}
	}


	/* Samples */

	D_(D_INFO "Stored samples: %d", mod->smp);

	for (i = 0; i < mod->ins; i++) {
		if (libxmp_load_sample(m, f, SAMPLE_FLAG_UNS, &mod->xxs[i], NULL) < 0)
			return -1;
	}

	m->quirk |= QUIRK_FINEFX;

	return 0;
}
