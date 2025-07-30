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

#include "loader.h"
#include "iff.h"
#include "../period.h"

/* Galaxy Music System 4.0 module file loader
 *
 * Based on modules converted using mod2j2b.exe
 */

static int gal4_test(HIO_HANDLE *, char *, const int);
static int gal4_load(struct module_data *, HIO_HANDLE *, const int);

const struct format_loader libxmp_loader_gal4 = {
	"Galaxy Music System 4.0",
	gal4_test,
	gal4_load
};

static int gal4_test(HIO_HANDLE *f, char *t, const int start)
{
        if (hio_read32b(f) != MAGIC4('R', 'I', 'F', 'F'))
		return -1;

	hio_read32b(f);

	if (hio_read32b(f) != MAGIC4('A', 'M', 'F', 'F'))
		return -1;

	if (hio_read32b(f) != MAGIC4('M', 'A', 'I', 'N'))
		return -1;

	hio_read32b(f);		/* skip size */
	libxmp_read_title(f, t, 64);

	return 0;
}

struct local_data {
    int snum;
};

static int get_main(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	char buf[64];
	int flags;

	if (hio_read(buf, 1, 64, f) < 64)
		return -1;
	strncpy(mod->name, buf, 63);	/* ensure string terminator */
	mod->name[63] = '\0';
	libxmp_set_type(m, "Galaxy Music System 4.0");

	flags = hio_read8(f);
	if (~flags & 0x01)
		m->period_type = PERIOD_LINEAR;
	mod->chn = hio_read8(f);
	mod->spd = hio_read8(f);
	mod->bpm = hio_read8(f);
	hio_read16l(f);		/* unknown - 0x01c5 */
	hio_read16l(f);		/* unknown - 0xff00 */
	hio_read8(f);		/* unknown - 0x80 */

	/* Sanity check */
	if (mod->chn > 32) {
		return -1;
	}

	return 0;
}

static int get_ordr(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	int i;

	mod->len = hio_read8(f) + 1;
	if (hio_error(f)) {
		return -1;
	}

	for (i = 0; i < mod->len; i++) {
		mod->xxo[i] = hio_read8(f);
	}

	return 0;
}

static int get_patt_cnt(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	int i;

	i = hio_read8(f) + 1;		/* pattern number */

	if (i > mod->pat)
		mod->pat = i;

	return 0;
}

static int get_inst_cnt(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	int i;

	hio_read8(f);			/* 00 */
	i = hio_read8(f) + 1;		/* instrument number */

	/* Sanity check */
	if (i > MAX_INSTRUMENTS)
		return -1;

	if (i > mod->ins)
		mod->ins = i;

	hio_seek(f, 28, SEEK_CUR);		/* skip name */

	mod->smp += hio_read8(f);

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

	/* Sanity check */
	if (i >= mod->pat || len <= 0 || mod->xxp[i]) {
		return -1;
	}

	rows = hio_read8(f) + 1;

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

	return 9;
}

static int get_inst(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	struct local_data *data = (struct local_data *)parm;
	int i, j;
	int srate, finetune, flags;
	int val, vwf, vra, vde, vsw /*, fade*/;
	uint8 buf[30];

	hio_read8(f);		/* 00 */
	i = hio_read8(f);		/* instrument number */

	/* Sanity check */
	if (i >= mod->ins || mod->xxi[i].nsm) {
		return -1;
	}

	hio_read(mod->xxi[i].name, 1, 28, f);
	mod->xxi[i].nsm = hio_read8(f);

	for (j = 0; j < 108; j++) {
		mod->xxi[i].map[j].ins = hio_read8(f);
	}

	hio_seek(f, 11, SEEK_CUR);		/* unknown */
	vwf = hio_read8(f);			/* vibrato waveform */
	vsw = hio_read8(f);			/* vibrato sweep */
	hio_read8(f);			/* unknown */
	hio_read8(f);			/* unknown */
	vde = hio_read8(f);		/* vibrato depth */
	vra = hio_read16l(f) / 16;		/* vibrato speed */
	hio_read8(f);			/* unknown */

	val = hio_read8(f);			/* PV envelopes flags */
	if (LSN(val) & 0x01)
		mod->xxi[i].aei.flg |= XMP_ENVELOPE_ON;
	if (LSN(val) & 0x02)
		mod->xxi[i].aei.flg |= XMP_ENVELOPE_SUS;
	if (LSN(val) & 0x04)
		mod->xxi[i].aei.flg |= XMP_ENVELOPE_LOOP;
	if (MSN(val) & 0x01)
		mod->xxi[i].pei.flg |= XMP_ENVELOPE_ON;
	if (MSN(val) & 0x02)
		mod->xxi[i].pei.flg |= XMP_ENVELOPE_SUS;
	if (MSN(val) & 0x04)
		mod->xxi[i].pei.flg |= XMP_ENVELOPE_LOOP;

	val = hio_read8(f);			/* PV envelopes points */
	mod->xxi[i].aei.npt = LSN(val) + 1;
	mod->xxi[i].pei.npt = MSN(val) + 1;

	val = hio_read8(f);			/* PV envelopes sustain point */
	mod->xxi[i].aei.sus = LSN(val);
	mod->xxi[i].pei.sus = MSN(val);

	val = hio_read8(f);			/* PV envelopes loop start */
	mod->xxi[i].aei.lps = LSN(val);
	mod->xxi[i].pei.lps = MSN(val);

	hio_read8(f);			/* PV envelopes loop end */
	mod->xxi[i].aei.lpe = LSN(val);
	mod->xxi[i].pei.lpe = MSN(val);

	if (mod->xxi[i].aei.npt <= 0 || mod->xxi[i].aei.npt > MIN(10, XMP_MAX_ENV_POINTS))
		mod->xxi[i].aei.flg &= ~XMP_ENVELOPE_ON;

	if (mod->xxi[i].pei.npt <= 0 || mod->xxi[i].pei.npt > MIN(10, XMP_MAX_ENV_POINTS))
		mod->xxi[i].pei.flg &= ~XMP_ENVELOPE_ON;

	if (hio_read(buf, 1, 30, f) < 30) {	/* volume envelope points */
		D_(D_CRIT "read error at vol env %d", i);
		return -1;
	}
	for (j = 0; j < mod->xxi[i].aei.npt; j++) {
		if (j >= 10) {
			break;
		}
		mod->xxi[i].aei.data[j * 2] = readmem16l(buf + j * 3) / 16;
		mod->xxi[i].aei.data[j * 2 + 1] = buf[j * 3 + 2];
	}

	if (hio_read(buf, 1, 30, f) < 30) {	/* pan envelope points */
		D_(D_CRIT "read error at pan env %d", i);
		return -1;
	}
	for (j = 0; j < mod->xxi[i].pei.npt; j++) {
		if (j >= 10) {
			break;
		}
		mod->xxi[i].pei.data[j * 2] = readmem16l(buf + j * 3) / 16;
		mod->xxi[i].pei.data[j * 2 + 1] = buf[j * 3 + 2];
	}

	/*fade =*/ hio_read8(f);	/* fadeout - 0x80->0x02 0x310->0x0c */
	hio_read8(f);			/* unknown */

	D_(D_INFO "[%2X] %-28.28s  %2d ", i, mod->xxi[i].name, mod->xxi[i].nsm);

	if (mod->xxi[i].nsm == 0)
		return 0;

	if (libxmp_alloc_subinstrument(mod, i, mod->xxi[i].nsm) < 0)
		return -1;

	for (j = 0; j < mod->xxi[i].nsm; j++, data->snum++) {
		hio_read32b(f);	/* SAMP */
		hio_read32b(f);	/* size */

		hio_read(mod->xxs[data->snum].name, 1, 28, f);

		mod->xxi[i].sub[j].pan = hio_read8(f) * 4;
		if (mod->xxi[i].sub[j].pan == 0)	/* not sure about this */
			mod->xxi[i].sub[j].pan = 0x80;

		mod->xxi[i].sub[j].vol = hio_read8(f);
		flags = hio_read8(f);
		hio_read8(f);	/* unknown - 0x80 */

		mod->xxi[i].sub[j].vwf = vwf;
		mod->xxi[i].sub[j].vde = vde;
		mod->xxi[i].sub[j].vra = vra;
		mod->xxi[i].sub[j].vsw = vsw;
		mod->xxi[i].sub[j].sid = data->snum;

		mod->xxs[data->snum].len = hio_read32l(f);
		mod->xxs[data->snum].lps = hio_read32l(f);
		mod->xxs[data->snum].lpe = hio_read32l(f);

		mod->xxs[data->snum].flg = 0;
		if (flags & 0x04)
			mod->xxs[data->snum].flg |= XMP_SAMPLE_16BIT;
		if (flags & 0x08)
			mod->xxs[data->snum].flg |= XMP_SAMPLE_LOOP;
		if (flags & 0x10)
			mod->xxs[data->snum].flg |= XMP_SAMPLE_LOOP_BIDIR;
		/* if (flags & 0x80)
			mod->xxs[data->snum].flg |= ? */

		srate = hio_read32l(f);
		finetune = 0;
		libxmp_c2spd_to_note(srate, &mod->xxi[i].sub[j].xpo, &mod->xxi[i].sub[j].fin);
		mod->xxi[i].sub[j].fin += finetune;

		hio_read32l(f);			/* 0x00000000 */
		hio_read32l(f);			/* unknown */

		D_(D_INFO "  %X: %05x%c%05x %05x %c V%02x P%02x %5d",
			j, mod->xxs[data->snum].len,
			mod->xxs[data->snum].flg & XMP_SAMPLE_16BIT ? '+' : ' ',
			mod->xxs[data->snum].lps,
			mod->xxs[data->snum].lpe,
			mod->xxs[data->snum].flg & XMP_SAMPLE_LOOP_BIDIR ? 'B' :
			mod->xxs[data->snum].flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
			mod->xxi[i].sub[j].vol,
			mod->xxi[i].sub[j].pan,
			srate);

		if (mod->xxs[data->snum].len > 1) {
			int snum = data->snum;
			if (libxmp_load_sample(m, f, 0, &mod->xxs[snum], NULL) < 0)
				return -1;
		}
	}

	return 0;
}

static int gal4_load(struct module_data *m, HIO_HANDLE *f, const int start)
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
	ret = libxmp_iff_register(handle, "MAIN", get_main);
	ret |= libxmp_iff_register(handle, "ORDR", get_ordr);
	ret |= libxmp_iff_register(handle, "PATT", get_patt_cnt);
	ret |= libxmp_iff_register(handle, "INST", get_inst_cnt);

	if (ret != 0)
		return -1;

	libxmp_iff_set_quirk(handle, IFF_LITTLE_ENDIAN);
	libxmp_iff_set_quirk(handle, IFF_CHUNK_TRUNC4);

	/* Load IFF chunks */
	if (libxmp_iff_load(handle, m, f, &data) < 0) {
		libxmp_iff_release(handle);
		return -1;
	}

	libxmp_iff_release(handle);

	mod->trk = mod->pat * mod->chn;

	MODULE_INFO();

	if (libxmp_init_instrument(m) < 0)
		return -1;

	if (libxmp_init_pattern(mod) < 0)
		return -1;

	D_(D_INFO "Stored patterns: %d\n", mod->pat);
	D_(D_INFO "Stored samples : %d ", mod->smp);

	hio_seek(f, start + offset, SEEK_SET);
	data.snum = 0;

	handle = libxmp_iff_new();
	if (handle == NULL)
		return -1;

	/* IFF chunk IDs */
	ret = libxmp_iff_register(handle, "PATT", get_patt);
	ret |= libxmp_iff_register(handle, "INST", get_inst);

	if (ret != 0)
		return -1;

	libxmp_iff_set_quirk(handle, IFF_LITTLE_ENDIAN);
	libxmp_iff_set_quirk(handle, IFF_CHUNK_TRUNC4);

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
		mod->xxc[i].pan = 0x80;
	}

	m->quirk |= QUIRKS_FT2;
	m->read_event_type = READ_EVENT_FT2;

	return 0;
}
