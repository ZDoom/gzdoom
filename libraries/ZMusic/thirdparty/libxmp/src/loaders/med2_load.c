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

/*
 * MED 1.12 is in Fish disk #255
 */

#include "loader.h"
#include "../period.h"

#define MAGIC_MED2	MAGIC4('M','E','D',2)

static int med2_test(HIO_HANDLE *, char *, const int);
static int med2_load (struct module_data *, HIO_HANDLE *, const int);

const struct format_loader libxmp_loader_med2 = {
	"MED 1.12 MED2",
	med2_test,
	med2_load
};


static int med2_test(HIO_HANDLE *f, char *t, const int start)
{
	if (hio_read32b(f) !=  MAGIC_MED2)
		return -1;

	libxmp_read_title(f, t, 0);

	return 0;
}

static int med2_load(struct module_data *m, HIO_HANDLE *f, const int start)
{
	struct xmp_module *mod = &m->mod;
	int i, j, k;
	int sliding;
	struct xmp_event *event;
	uint8 buf[40];

	LOAD_INIT();

	if (hio_read32b(f) != MAGIC_MED2)
		return -1;

	libxmp_set_type(m, "MED 1.12 MED2");

	mod->ins = mod->smp = 32;

	if (libxmp_init_instrument(m) < 0)
		return -1;

	/* read instrument names */
	hio_read(buf, 1, 40, f);	/* skip 0 */
	for (i = 0; i < 31; i++) {
		if (hio_read(buf, 1, 40, f) != 40)
			return -1;

		libxmp_instrument_name(mod, i, buf, 40);
		if (libxmp_alloc_subinstrument(mod, i, 1) < 0)
			return -1;
	}

	/* read instrument volumes */
	hio_read8(f);		/* skip 0 */
	for (i = 0; i < 31; i++) {
		mod->xxi[i].sub[0].vol = hio_read8(f);
		mod->xxi[i].sub[0].pan = 0x80;
		mod->xxi[i].sub[0].fin = 0;
		mod->xxi[i].sub[0].sid = i;
	}

	/* read instrument loops */
	hio_read16b(f);		/* skip 0 */
	for (i = 0; i < 31; i++) {
		mod->xxs[i].lps = hio_read16b(f);
	}

	/* read instrument loop length */
	hio_read16b(f);		/* skip 0 */
	for (i = 0; i < 31; i++) {
		uint32 lsiz = hio_read16b(f);
		mod->xxs[i].lpe = mod->xxs[i].lps + lsiz;
		mod->xxs[i].flg = lsiz > 1 ? XMP_SAMPLE_LOOP : 0;
	}

	mod->chn = 4;
	mod->pat = hio_read16b(f);
	mod->trk = mod->chn * mod->pat;

	if (hio_read(mod->xxo, 1, 100, f) != 100)
		return -1;

	mod->len = hio_read16b(f);

	/* Sanity check */
	if (mod->pat > 256 || mod->len > 100)
		return -1;

	k = hio_read16b(f);
	if (k < 1) {
		return -1;
	}

	mod->spd = 6;
	mod->bpm = k;
	m->time_factor = MED_TIME_FACTOR;

	hio_read16b(f);			/* flags */
	sliding = hio_read16b(f);	/* sliding */
	hio_read32b(f);			/* jumping mask */
	hio_seek(f, 16, SEEK_CUR);	/* rgb */

	MODULE_INFO();

	D_(D_INFO "Sliding: %d", sliding);

	if (sliding == 6)
		m->quirk |= QUIRK_VSALL | QUIRK_PBALL;

	if (libxmp_init_pattern(mod) < 0)
		return -1;

	/* Load and convert patterns */
	D_(D_INFO "Stored patterns: %d", mod->pat);

	for (i = 0; i < mod->pat; i++) {
		if (libxmp_alloc_pattern_tracks(mod, i, 64) < 0)
			return -1;

		hio_read32b(f);

		for (j = 0; j < 64; j++) {
			for (k = 0; k < 4; k++) {
				uint8 x;
				event = &EVENT(i, k, j);
				event->note = libxmp_period_to_note(hio_read16b(f));
				x = hio_read8(f);
				event->ins = x >> 4;
				event->fxt = x & 0x0f;
				event->fxp = hio_read8(f);

				switch (event->fxt) {
				case 0x00:		/* arpeggio */
				case 0x01:		/* slide up */
				case 0x02:		/* slide down */
				case 0x03:		/* portamento */
				case 0x04:		/* vibrato? */
				case 0x0c:		/* volume */
					break;		/* ...like protracker */
				case 0x0d:		/* volslide */
				case 0x0e:		/* volslide */
					event->fxt = FX_VOLSLIDE;
					break;
				case 0x0f:
					event->fxt = FX_S3M_BPM;
					break;
				}
			}
		}
	}

	/* Load samples */

	D_(D_INFO "Instruments    : %d ", mod->ins);

	for (i = 0; i < 31; i++) {
		char path[XMP_MAXPATH];
		char ins_path[256];
		char ins_name[32];
		char name[256];
		HIO_HANDLE *s = NULL;
		int found = 0;

		if (libxmp_copy_name_for_fopen(ins_name, mod->xxi[i].name, 32) != 0)
			continue;

		libxmp_get_instrument_path(m, ins_path, 256);
		if (libxmp_check_filename_case(ins_path, ins_name, name, 256)) {
			snprintf(path, XMP_MAXPATH, "%s/%s", ins_path, name);
			found = 1;
		}

		/* Try the module dir if the instrument path didn't work. */
		if (!found && m->dirname != NULL &&
		    libxmp_check_filename_case(m->dirname, ins_name, name, 256)) {
			snprintf(path, XMP_MAXPATH, "%s%s", m->dirname, name);
			found = 1;
		}

		if (found) {
			if ((s = hio_open(path,"rb")) != NULL) {
				mod->xxs[i].len = hio_size(s);
			}
		}

		if (mod->xxs[i].len > 0) {
			mod->xxi[i].nsm = 1;
		}

		if (!strlen(mod->xxi[i].name) && !mod->xxs[i].len) {
			if (s != NULL) {
				hio_close(s);
			}
			continue;
		}

		D_(D_INFO "[%2X] %-32.32s %04x %04x %04x %c V%02x",
			i, mod->xxi[i].name, mod->xxs[i].len, mod->xxs[i].lps,
			mod->xxs[i].lpe,
			mod->xxs[i].flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
			mod->xxi[i].sub[0].vol);

		if (s != NULL) {
			int ret = libxmp_load_sample(m, s, 0, &mod->xxs[i], NULL);
			hio_close(s);
			if (ret < 0) {
				return -1;
			}
		}
	}

	return 0;
}
