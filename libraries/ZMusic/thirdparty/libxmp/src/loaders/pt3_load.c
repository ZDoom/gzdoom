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
#include "mod.h"
#include "iff.h"

#define MAGIC_FORM	MAGIC4('F','O','R','M')
#define MAGIC_MODL	MAGIC4('M','O','D','L')
#define MAGIC_VERS	MAGIC4('V','E','R','S')
#define MAGIC_INFO	MAGIC4('I','N','F','O')

static int pt3_test(HIO_HANDLE *, char *, const int);
static int pt3_load(struct module_data *, HIO_HANDLE *, const int);
static int ptdt_load(struct module_data *, HIO_HANDLE *, const int);

const struct format_loader libxmp_loader_pt3 = {
	"Protracker 3",
	pt3_test,
	pt3_load
};

static int pt3_test(HIO_HANDLE *f, char *t, const int start)
{
	if (hio_read32b(f) != MAGIC_FORM)
		return -1;

	hio_read32b(f);	/* skip size */

	if (hio_read32b(f) != MAGIC_MODL)
		return -1;

	if (hio_read32b(f) != MAGIC_VERS)
		return -1;

	hio_read32b(f);	/* skip size */

	hio_seek(f, 10, SEEK_CUR);

	if (hio_read32b(f) == MAGIC_INFO) {
		hio_read32b(f);	/* skip size */
		libxmp_read_title(f, t, 32);
	} else {
		libxmp_read_title(f, t, 0);
	}

	return 0;
}

#define PT3_FLAG_CIA	0x0001	/* VBlank if not set */
#define PT3_FLAG_FILTER	0x0002	/* Filter status */
#define PT3_FLAG_SONG	0x0004	/* Modules have this bit unset */
#define PT3_FLAG_IRQ	0x0008	/* Soft IRQ */
#define PT3_FLAG_VARPAT	0x0010	/* Variable pattern length */
#define PT3_FLAG_8VOICE	0x0020	/* 4 voices if not set */
#define PT3_FLAG_16BIT	0x0040	/* 8 bit samples if not set */
#define PT3_FLAG_RAWPAT	0x0080	/* Packed patterns if not set */

struct local_data {
	int has_ptdt;
};

static int get_info(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	struct local_data *data = (struct local_data *)parm;
	/* int flags; */
	/* int day, month, year, hour, min, sec;
	int dhour, dmin, dsec; */

	/* Sanity check */
	if(data->has_ptdt) {
		return -1;
	}

	hio_read(mod->name, 1, 32, f);
	mod->ins = hio_read16b(f);
	mod->len = hio_read16b(f);
	mod->pat = hio_read16b(f);
	mod->gvl = hio_read16b(f);
	mod->bpm = hio_read16b(f);	/* Not clamped by Protracker 3.6 */
	/*flags =*/ hio_read16b(f);
	/*day   =*/ hio_read16b(f);
	/*month =*/ hio_read16b(f);
	/*year  =*/ hio_read16b(f);
	/*hour  =*/ hio_read16b(f);
	/*min   =*/ hio_read16b(f);
	/*sec   =*/ hio_read16b(f);
	/*dhour =*/ hio_read16b(f);
	/*dmin  =*/ hio_read16b(f);
	/*dsec  =*/ hio_read16b(f);

	/* Sanity check */
	if (mod->ins > 255 || mod->len > 256 || mod->pat > 255) {
		return -1;
	}

	MODULE_INFO();

	/*D_(D_INFO "Creation date: %02d/%02d/%02d %02d:%02d:%02d",
		       day, month, year, hour, min, sec);
	D_(D_INFO "Playing time: %02d:%02d:%02d", dhour, dmin, dsec);*/

	return 0;
}

static int get_cmnt(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	D_(D_INFO "Comment size: %d", size);

	return 0;
}

static int get_ptdt(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	struct local_data *data = (struct local_data *)parm;

	/* Sanity check */
	if(data->has_ptdt) {
		return -1;
	}
	data->has_ptdt = 1;

	ptdt_load(m, f, 0);

	return 0;
}

static int pt3_load(struct module_data *m, HIO_HANDLE *f, const int start)
{
	iff_handle handle;
	struct local_data data;
	char buf[20];
	int ret;

	LOAD_INIT();

	memset(&data, 0, sizeof(struct local_data));

	hio_read32b(f);		/* FORM */
	hio_read32b(f);		/* size */
	hio_read32b(f);		/* MODL */
	hio_read32b(f);		/* VERS */
	hio_read32b(f);		/* VERS size */

	if (hio_read(buf, 1, 10, f) < 10)
		return -1;
	libxmp_set_type(m, "%-6.6s IFFMODL", buf + 4);

	handle = libxmp_iff_new();
	if (handle == NULL)
		return -1;

	/* IFF chunk IDs */
	ret = libxmp_iff_register(handle, "INFO", get_info);
	ret |= libxmp_iff_register(handle, "CMNT", get_cmnt);
	ret |= libxmp_iff_register(handle, "PTDT", get_ptdt);

	if (ret != 0)
		return -1;

	libxmp_iff_set_quirk(handle, IFF_FULL_CHUNK_SIZE);

	/* Load IFF chunks */
	if (libxmp_iff_load(handle, m, f, &data) < 0) {
		libxmp_iff_release(handle);
		return -1;
	}

	libxmp_iff_release(handle);

	/* Sanity check */
	if (m->mod.smp <= 0) {
		return -1;
	}

	return 0;
}

static int ptdt_load(struct module_data *m, HIO_HANDLE *f, const int start)
{
	struct xmp_module *mod = &m->mod;
	int i, j;
	struct xmp_event *event;
	struct mod_header mh;
	uint8 mod_event[4];

	hio_read(mh.name, 20, 1, f);
	for (i = 0; i < 31; i++) {
		hio_read(mh.ins[i].name, 22, 1, f);
		mh.ins[i].size = hio_read16b(f);
		mh.ins[i].finetune = hio_read8(f);
		mh.ins[i].volume = hio_read8(f);
		mh.ins[i].loop_start = hio_read16b(f);
		mh.ins[i].loop_size = hio_read16b(f);
	}
	mh.len = hio_read8(f);
	mh.restart = hio_read8(f);
	if (hio_read(mh.order, 128, 1, f) < 1) {
		D_(D_CRIT "read error at order list");
		return -1;
	}
	hio_read(mh.magic, 4, 1, f);

	mod->ins = 31;
	mod->smp = mod->ins;
	mod->chn = 4;
	mod->len = mh.len;
	mod->rst = mh.restart;
	memcpy(mod->xxo, mh.order, 128);

	for (i = 0; i < 128; i++) {
		if (mod->xxo[i] > mod->pat)
			mod->pat = mod->xxo[i];
	}

	mod->pat++;
	mod->trk = mod->chn * mod->pat;

	if (libxmp_init_instrument(m) < 0)
		return -1;

	for (i = 0; i < mod->ins; i++) {
		if (libxmp_alloc_subinstrument(mod, i, 1) < 0)
			return -1;

		mod->xxs[i].len = 2 * mh.ins[i].size;
		mod->xxs[i].lps = 2 * mh.ins[i].loop_start;
		mod->xxs[i].lpe = mod->xxs[i].lps + 2 * mh.ins[i].loop_size;
		mod->xxs[i].flg = mh.ins[i].loop_size > 1 ? XMP_SAMPLE_LOOP : 0;

		if (mod->xxs[i].len > 0)
			mod->xxi[i].nsm = 1;

		mod->xxi[i].sub[0].fin = (int8)(mh.ins[i].finetune << 4);
		mod->xxi[i].sub[0].vol = mh.ins[i].volume;
		mod->xxi[i].sub[0].pan = 0x80;
		mod->xxi[i].sub[0].sid = i;
		mod->xxi[i].rls = 0xfff;

		libxmp_instrument_name(mod, i, mh.ins[i].name, 22);

		D_(D_INFO "[%2X] %-22.22s %04x %04x %04x %c V%02x %+d",
				i, mod->xxi[i].name,
				mod->xxs[i].len, mod->xxs[i].lps,
				mod->xxs[i].lpe,
				mh.ins[i].loop_size > 1 ? 'L' : ' ',
				mod->xxi[i].sub[0].vol,
				mod->xxi[i].sub[0].fin >> 4);
	}

	if (libxmp_init_pattern(mod) < 0)
		return -1;

	/* Load and convert patterns */
	D_(D_INFO "Stored patterns: %d", mod->pat);

	for (i = 0; i < mod->pat; i++) {
		if (libxmp_alloc_pattern_tracks(mod, i, 64) < 0)
			return -1;

		for (j = 0; j < (64 * 4); j++) {
			event = &EVENT(i, j % 4, j / 4);
			if (hio_read(mod_event, 1, 4, f) < 4) {
				D_(D_CRIT "read error at pat %d", i);
				return -1;
			}
			libxmp_decode_protracker_event(event, mod_event);
		}
	}

	m->period_type = PERIOD_MODRNG;

	/* Load samples */
	D_(D_INFO "Stored samples: %d", mod->smp);

	for (i = 0; i < mod->smp; i++) {
		if (!mod->xxs[i].len)
			continue;

		if (libxmp_load_sample(m, f, 0, &mod->xxs[i], NULL) < 0)
			return -1;
	}

	return 0;
}
