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
#include "iff.h"
#include "../period.h"

/* Galaxy Music System 5.0 module file loader
 *
 * Based on the format description by Dr.Eggman
 * (http://www.jazz2online.com/J2Ov2/articles/view.php?articleID=288)
 * and Jazz Jackrabbit modules by Alexander Brandon from Lori Central
 * (http://www.loricentral.com/jj2music.html)
 */

static int gal5_test(HIO_HANDLE *, char *, const int);
static int gal5_load(struct module_data *, HIO_HANDLE *, const int);

const struct format_loader libxmp_loader_gal5 = {
	"Galaxy Music System 5.0 (J2B)",
	gal5_test,
	gal5_load
};


struct local_data {
    uint8 chn_pan[64];
};

static int gal5_test(HIO_HANDLE *f, char *t, const int start)
{
        if (hio_read32b(f) != MAGIC4('R', 'I', 'F', 'F'))
		return -1;

	hio_read32b(f);

	if (hio_read32b(f) != MAGIC4('A', 'M', ' ', ' '))
		return -1;

	if (hio_read32b(f) != MAGIC4('I', 'N', 'I', 'T'))
		return -1;

	hio_read32b(f);		/* skip size */
	libxmp_read_title(f, t, 64);

	return 0;
}

static int get_init(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	struct local_data *data = (struct local_data *)parm;
	char buf[64];
	int flags;

	if (hio_read(buf, 1, 64, f) < 64)
		return -1;
	strncpy(mod->name, buf, 63);	/* ensure string terminator */
	mod->name[63] = '\0';
	libxmp_set_type(m, "Galaxy Music System 5.0");
	flags = hio_read8(f);	/* bit 0: Amiga period */
	if (~flags & 0x01)
		m->period_type = PERIOD_LINEAR;
	mod->chn = hio_read8(f);
	mod->spd = hio_read8(f);
	mod->bpm = hio_read8(f);
	hio_read16l(f);		/* unknown - 0x01c5 */
	hio_read16l(f);		/* unknown - 0xff00 */
	hio_read8(f);		/* unknown - 0x80 */

	if (hio_read(data->chn_pan, 1, 64, f) != 64) {
		D_(D_CRIT "error reading INIT");
		return -1;
	}

	/* Sanity check */
	if (mod->chn > XMP_MAX_CHANNELS)
		return -1;

	return 0;
}

static int get_ordr(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	int i;

	mod->len = hio_read8(f) + 1;
	/* Don't follow Dr.Eggman's specs here */

	for (i = 0; i < mod->len; i++)
		mod->xxo[i] = hio_read8(f);

	return 0;
}

static int get_patt_cnt(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	int i;

	i = hio_read8(f) + 1;	/* pattern number */

	if (i > mod->pat)
		mod->pat = i;

	return 0;
}

static int get_inst_cnt(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	int i;

	hio_read32b(f);		/* 42 01 00 00 */
	hio_read8(f);		/* 00 */
	i = hio_read8(f) + 1;	/* instrument number */

	/* Sanity check */
	if (i > MAX_INSTRUMENTS)
		return -1;

	if (i > mod->ins)
		mod->ins = i;

	return 0;
}

static int get_patt(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	struct xmp_event *event, dummy;
	int i, len, chan;
	int rows, r;
	uint8 flag;

	i = hio_read8(f);	/* pattern number */
	len = hio_read32l(f);

	rows = hio_read8(f) + 1;

	/* Sanity check - don't allow duplicate patterns. */
	if (len < 0 || mod->xxp[i] != NULL)
		return -1;

	if (libxmp_alloc_pattern_tracks(mod, i, rows) < 0)
		return -1;

	for (r = 0; r < rows; ) {
		if ((flag = hio_read8(f)) == 0) {
			r++;
			continue;
		}
		if (hio_error(f)) {
			return -1;
		}

		chan = flag & 0x1f;

		event = chan < mod->chn ? &EVENT(i, chan, r) : &dummy;

		if (flag & 0x80) {
			uint8 fxp = hio_read8(f);
			uint8 fxt = hio_read8(f);

			switch (fxt) {
			case 0x14:		/* speed */
				fxt = FX_S3M_SPEED;
				break;
			default:
				if (fxt > 0x0f) {
					D_(D_CRIT "p%d r%d c%d unknown effect %02x %02x", i, r, chan, fxt, fxp);
					fxt = fxp = 0;
				}
			}

			event->fxt = fxt;
			event->fxp = fxp;
		}

		if (flag & 0x40) {
			event->ins = hio_read8(f);
			event->note = hio_read8(f);

			if (event->note == 128) {
				event->note = XMP_KEY_OFF;
			}
		}

		if (flag & 0x20) {
			event->vol = 1 + hio_read8(f) / 2;
		}
	}

	return 0;
}

static int get_inst(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	int i, srate, finetune, flags;
	int has_unsigned_sample;

	hio_read32b(f);		/* 42 01 00 00 */
	hio_read8(f);		/* 00 */
	i = hio_read8(f);		/* instrument number */

	/* Sanity check - don't allow duplicate instruments. */
	if (mod->xxi[i].nsm != 0)
		return -1;

	hio_read(mod->xxi[i].name, 1, 28, f);
	hio_seek(f, 290, SEEK_CUR);	/* Sample/note map, envelopes */
	mod->xxi[i].nsm = hio_read16l(f);

	D_(D_INFO "[%2X] %-28.28s  %2d ", i, mod->xxi[i].name, mod->xxi[i].nsm);

	if (mod->xxi[i].nsm == 0)
		return 0;

	if (libxmp_alloc_subinstrument(mod, i, mod->xxi[i].nsm) < 0)
		return -1;

	/* FIXME: Currently reading only the first sample */

	hio_read32b(f);	/* RIFF */
	hio_read32b(f);	/* size */
	hio_read32b(f);	/* AS   */
	hio_read32b(f);	/* SAMP */
	hio_read32b(f);	/* size */
	hio_read32b(f);	/* unknown - usually 0x40000000 */

	hio_read(mod->xxs[i].name, 1, 28, f);

	hio_read32b(f);	/* unknown - 0x0000 */
	hio_read8(f);	/* unknown - 0x00 */

	mod->xxi[i].sub[0].sid = i;
	mod->xxi[i].vol = hio_read8(f);
	mod->xxi[i].sub[0].pan = 0x80;
	mod->xxi[i].sub[0].vol = (hio_read16l(f) + 1) / 512;
	flags = hio_read16l(f);
	hio_read16l(f);			/* unknown - 0x0080 */
	mod->xxs[i].len = hio_read32l(f);
	mod->xxs[i].lps = hio_read32l(f);
	mod->xxs[i].lpe = hio_read32l(f);

	mod->xxs[i].flg = 0;
	has_unsigned_sample = 0;
	if (flags & 0x04)
		mod->xxs[i].flg |= XMP_SAMPLE_16BIT;
	if (flags & 0x08)
		mod->xxs[i].flg |= XMP_SAMPLE_LOOP;
	if (flags & 0x10)
		mod->xxs[i].flg |= XMP_SAMPLE_LOOP | XMP_SAMPLE_LOOP_BIDIR;
	if (~flags & 0x80)
		has_unsigned_sample = 1;

	srate = hio_read32l(f);
	finetune = 0;
	libxmp_c2spd_to_note(srate, &mod->xxi[i].sub[0].xpo, &mod->xxi[i].sub[0].fin);
	mod->xxi[i].sub[0].fin += finetune;

	hio_read32l(f);			/* 0x00000000 */
	hio_read32l(f);			/* unknown */

	D_(D_INFO "  %x: %05x%c%05x %05x %c V%02x %04x %5d",
		0, mod->xxs[i].len,
		mod->xxs[i].flg & XMP_SAMPLE_16BIT ? '+' : ' ',
		mod->xxs[i].lps,
		mod->xxs[i].lpe,
		mod->xxs[i].flg & XMP_SAMPLE_LOOP_BIDIR ? 'B' :
			mod->xxs[i].flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
		mod->xxi[i].sub[0].vol, flags, srate);

	if (mod->xxs[i].len > 1) {
		if (libxmp_load_sample(m, f, has_unsigned_sample ?
				SAMPLE_FLAG_UNS : 0, &mod->xxs[i], NULL) < 0)
			return -1;
	}

	return 0;
}

static int gal5_load(struct module_data *m, HIO_HANDLE *f, const int start)
{
	struct xmp_module *mod = &m->mod;
	iff_handle handle;
	int i, ret, offset;
	struct local_data data;

	LOAD_INIT();

	hio_read32b(f);	/* Skip RIFF */
	hio_read32b(f);	/* Skip size */
	hio_read32b(f);	/* Skip AM   */

	offset = hio_tell(f);

	mod->smp = mod->ins = 0;

	handle = libxmp_iff_new();
	if (handle == NULL)
		return -1;

	m->c4rate = C4_NTSC_RATE;

	/* IFF chunk IDs */
	ret = libxmp_iff_register(handle, "INIT", get_init);		/* Galaxy 5.0 */
	ret |= libxmp_iff_register(handle, "ORDR", get_ordr);
	ret |= libxmp_iff_register(handle, "PATT", get_patt_cnt);
	ret |= libxmp_iff_register(handle, "INST", get_inst_cnt);

	if (ret != 0)
		return -1;

	libxmp_iff_set_quirk(handle, IFF_LITTLE_ENDIAN);
	libxmp_iff_set_quirk(handle, IFF_SKIP_EMBEDDED);
	libxmp_iff_set_quirk(handle, IFF_CHUNK_ALIGN2);

	/* Load IFF chunks */
	if (libxmp_iff_load(handle, m, f, &data) < 0) {
		libxmp_iff_release(handle);
		return -1;
	}

	libxmp_iff_release(handle);

	mod->trk = mod->pat * mod->chn;
	mod->smp = mod->ins;

	MODULE_INFO();

	if (libxmp_init_instrument(m) < 0)
		return -1;

	if (libxmp_init_pattern(mod) < 0)
		return -1;

	D_(D_INFO "Stored patterns: %d", mod->pat);
	D_(D_INFO "Stored samples: %d ", mod->smp);

	hio_seek(f, start + offset, SEEK_SET);

	handle = libxmp_iff_new();
	if (handle == NULL)
		return -1;

	/* IFF chunk IDs */
	ret = libxmp_iff_register(handle, "PATT", get_patt);
	ret |= libxmp_iff_register(handle, "INST", get_inst);

	if (ret != 0)
		return -1;

	libxmp_iff_set_quirk(handle, IFF_LITTLE_ENDIAN);
	libxmp_iff_set_quirk(handle, IFF_SKIP_EMBEDDED);
	libxmp_iff_set_quirk(handle, IFF_CHUNK_ALIGN2);

	/* Load IFF chunks */
	if (libxmp_iff_load(handle, m, f, &data) < 0) {
		libxmp_iff_release(handle);
		return -1;
	}

	libxmp_iff_release(handle);

	/* Alloc missing patterns */
	for (i = 0; i < mod->pat; i++) {
		if (mod->xxp[i] == NULL) {
			if (libxmp_alloc_pattern_tracks(mod, i, 64) < 0) {
				return -1;
			}
		}
	}

	for (i = 0; i < mod->chn; i++) {
		mod->xxc[i].pan = data.chn_pan[i] * 2;
	}

	m->quirk |= QUIRKS_FT2;
	m->read_event_type = READ_EVENT_FT2;

	return 0;
}
