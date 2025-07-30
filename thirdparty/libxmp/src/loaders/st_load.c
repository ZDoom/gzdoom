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

/* Ultimate Soundtracker support based on the module format description
 * written by Michael Schwendt <sidplay@geocities.com>
 */

#include "loader.h"
#include "mod.h"
#include "../period.h"

static int st_test(HIO_HANDLE *, char *, const int);
static int st_load(struct module_data *, HIO_HANDLE *, const int);

const struct format_loader libxmp_loader_st = {
	"Soundtracker",
	st_test,
	st_load
};

/* musanx.mod contains 22 period and instrument errors */
#define ST_MAX_PATTERN_ERRORS 22
/* Allow some degree of sample truncation for ST modules.
 * The worst known module currently is u2.mod with 7% truncation. */
#define ST_TRUNCATION_LIMIT   93

static const int period[] = {
	856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453,
	428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240, 226,
	214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113,
	/* Off-by-one period values found in blueberry.mod, snd.mod,
	 * quite a lot.mod, sweet dreams.mod, and bar----fringdus.mod */
	763, 679, 641, 571, 539, 509, 429, 340, 321, 300, 286, 270,
	227, 191, 162,
	-1
};

static int st_expected_size(int smp_size, int pat)
{
	return 600 + smp_size + 1024 * pat;
}

static int st_test(HIO_HANDLE *f, char *t, const int start)
{
	int i, j, k;
	int pat, pat_short, ins, smp_size;
	struct st_header mh;
	uint8 pat_buf[1024];
	uint8 *mod_event;
	int pattern_errors;
	int test_flags = TEST_NAME_IGNORE_AFTER_CR;
	long size;

	size = hio_size(f);

	if (size < 600) {
		return -1;
	}

	smp_size = 0;

	hio_seek(f, start, SEEK_SET);
	hio_read(mh.name, 1, 20, f);
	/* The Super Ski 2 modules have unusual "SONG\x13\x88" names. */
	if (mh.name[5] == 0x88) {
		mh.name[5] = 'X';
		if (mh.name[4] == 0x13)
			mh.name[4] = 'X';
	}
	if (libxmp_test_name(mh.name, 20, 0) < 0) {
		D_(D_CRIT "bad module name; not ST");
		return -1;
	}

	for (i = 0; i < 15; i++) {
		hio_read(mh.ins[i].name, 1, 22, f);
		mh.ins[i].size = hio_read16b(f);
		mh.ins[i].finetune = hio_read8(f);
		mh.ins[i].volume = hio_read8(f);
		mh.ins[i].loop_start = hio_read16b(f);
		mh.ins[i].loop_size = hio_read16b(f);
	}
	mh.len = hio_read8(f);
	mh.restart = hio_read8(f);
	hio_read(mh.order, 1, 128, f);

	for (pat = pat_short = i = 0; i < 128; i++) {
		if (mh.order[i] > 0x7f)
			return -1;
		if (mh.order[i] > pat) {
			pat = mh.order[i];
			if (i < mh.len)
				pat_short = pat;
		}
	}
	pat++;
	pat_short++;

	if (pat > 0x7f || mh.len == 0 || mh.len > 0x80)
		return -1;

	for (i = 0; i < 15; i++) {
		smp_size += 2 * mh.ins[i].size;

		/* pennylane.mod and heymusic-sssexremix.mod have unusual
		 * values after the \0. */
		if (i == 0 &&
		    (!memcmp(mh.ins[i].name, "funbass\0\r", 9) ||
		     !memcmp(mh.ins[i].name, "st-69:baseline\0R\0\0\xA5", 17))) {
			D_(D_INFO "ignoring junk name values after \\0");
			test_flags |= TEST_NAME_IGNORE_AFTER_0;
		}

		/* Crepequs.mod has random values in first byte */
		mh.ins[i].name[0] = 'X';

		if (libxmp_test_name(mh.ins[i].name, 22, test_flags) < 0) {
			D_(D_CRIT "bad instrument name %d; not ST", i);
			return -1;
		}

		if (mh.ins[i].volume > 0x40)
			return -1;

		if (mh.ins[i].finetune > 0x0f)
			return -1;

		if (mh.ins[i].size > 0x8000)
			return -1;

		/* This test is always false, disable it
		 *
		 * if ((mh.ins[i].loop_start >> 1) > 0x8000)
		 *    return -1;
		 */

		if (mh.ins[i].loop_size > 0x8000)
			return -1;

		/* This test fails in atmosfer.mod, disable it
		 *
		 * if (mh.ins[i].loop_size > 1 && mh.ins[i].loop_size > mh.ins[i].size)
		 *    return -1;
		 */

		/* Bad rip of fin-nv1.mod has this unused instrument. */
		if (mh.ins[i].size == 0 &&
		    mh.ins[i].loop_start == 4462 &&
		    mh.ins[i].loop_size == 2078) {
			D_(D_INFO "ignoring bad instrument for fin-nv1.mod");
			continue;
		}

		if ((mh.ins[i].loop_start >> 1) > mh.ins[i].size)
			return -1;

		if (mh.ins[i].size
		    && (mh.ins[i].loop_start >> 1) == mh.ins[i].size)
			return -1;

		if (mh.ins[i].size == 0 && mh.ins[i].loop_start > 0)
			return -1;
	}

	if (smp_size < 8) {
		return -1;
	}

	/* If the file size is correct when counting only patterns prior to the
	 * module length, use the shorter count. This quirk is found in some
	 * ST modules, most of them authored by Jean Baudlot. See razor-1911.mod,
	 * the Operation Wolf soundtrack, or the Bad Dudes soundtrack.
	 */
	if (size < st_expected_size(smp_size, pat) &&
	    size == st_expected_size(smp_size, pat_short)) {
		D_(D_INFO "ST pattern list probably quirked, ignoring patterns past len");
		pat = pat_short;
	}

	pattern_errors = 0;
	for (ins = i = 0; i < pat; i++) {
		if (hio_read(pat_buf, 1, 1024, f) < 1024) {
			D_(D_CRIT "read error at pattern %d; not ST", i);
			return -1;
		}
		mod_event = pat_buf;
		for (j = 0; j < (64 * 4); j++, mod_event += 4) {
			int p, s;

			s = (mod_event[0] & 0xf0) | MSN(mod_event[2]);

			if (s > 15) {	/* sample number > 15 */
				D_(D_INFO "%d/%d/%d: invalid sample number: %d", i, j / 4, j % 4, s);
				if ((++pattern_errors) > ST_MAX_PATTERN_ERRORS)
					goto bad_pattern_data;
			}

			if (s > ins) {	/* find highest used sample */
				ins = s;
			}

			p = 256 * LSN(mod_event[0]) + mod_event[1];

			if (p == 0) {
				continue;
			}

			for (k = 0; period[k] >= 0; k++) {
				if (p == period[k])
					break;
			}
			if (period[k] < 0) {
				D_(D_INFO "%d/%d/%d: invalid period: %d", i, j / 4, j % 4, p);
				if ((++pattern_errors) > ST_MAX_PATTERN_ERRORS)
					goto bad_pattern_data;
			}
		}
	}

	/* Check if file was cut before any unused samples */
	if (size < st_expected_size(smp_size, pat)) {
		int ss, limit;
		for (ss = i = 0; i < 15 && i < ins; i++) {
			ss += 2 * mh.ins[i].size;
		}

		limit = st_expected_size(ss, pat) * ST_TRUNCATION_LIMIT / 100;
		if (size < limit) {
			D_(D_CRIT "expected size %d, minimum allowed size %d, real size %ld, diff %ld",
			 st_expected_size(smp_size, pat), limit, size, size - limit);
			return -1;
		}
	}

	hio_seek(f, start, SEEK_SET);
	libxmp_read_title(f, t, 20);

	return 0;

bad_pattern_data:
	D_(D_CRIT "too many pattern errors; not ST: %d", pattern_errors);
	return -1;
}

static int st_load(struct module_data *m, HIO_HANDLE *f, const int start)
{
	struct xmp_module *mod = &m->mod;
	int i, j;
	struct xmp_event *event;
	struct st_header mh;
	uint8 pat_buf[1024];
	uint8 *mod_event;
	int ust = 1;
	/* int lps_mult = m->fetch & XMP_CTL_FIXLOOP ? 1 : 2; */
	const char *modtype;
	int fxused;
	int smp_size, pat_short;
	long size;

	LOAD_INIT();

	mod->chn = 4;
	mod->ins = 15;
	mod->smp = mod->ins;

	smp_size = 0;

	hio_read(mh.name, 1, 20, f);
	for (i = 0; i < 15; i++) {
		hio_read(mh.ins[i].name, 1, 22, f);
		mh.ins[i].size = hio_read16b(f);
		mh.ins[i].finetune = hio_read8(f);
		mh.ins[i].volume = hio_read8(f);
		mh.ins[i].loop_start = hio_read16b(f);
		mh.ins[i].loop_size = hio_read16b(f);
		smp_size += 2 * mh.ins[i].size;
	}
	mh.len = hio_read8(f);
	mh.restart = hio_read8(f);
	hio_read(mh.order, 1, 128, f);

	mod->len = mh.len;
	mod->rst = mh.restart;

	/* UST: The byte at module offset 471 is BPM, not the song restart
	 *      The default for UST modules is 0x78 = 120 BPM = 48 Hz.
	 */
	if (mod->rst < 0x40)	/* should be 0x20 */
		ust = 0;

	memcpy(mod->xxo, mh.order, 128);

	for (pat_short = i = 0; i < 128; i++) {
		if (mod->xxo[i] > mod->pat) {
			mod->pat = mod->xxo[i];
			if (i < mh.len)
				pat_short = mod->pat;
		}
	}
	mod->pat++;
	pat_short++;

	/* If the file size is correct when counting only patterns prior to the
	 * module length, use the shorter count. See test function for info.
	 */
	size = hio_size(f);
	if (size < st_expected_size(smp_size, mod->pat) &&
	    size == st_expected_size(smp_size, pat_short)) {
		mod->pat = pat_short;
	}

	for (i = 0; i < mod->ins; i++) {
		/* UST: Volume word does not contain a "Finetuning" value in its
		 * high-byte.
		 */
		if (mh.ins[i].finetune)
			ust = 0;

		/* if (mh.ins[i].size == 0 && mh.ins[i].loop_size == 1)
		   nt = 1; */

		/* UST: Maximum sample length is 9999 bytes decimal, but 1387
		 * words hexadecimal. Longest samples on original sample disk
		 * ST-01 were 9900 bytes.
		 */
		if (mh.ins[i].size > 0x1387 || mh.ins[i].loop_start > 9999
		    || mh.ins[i].loop_size > 0x1387) {
			ust = 0;
		}
	}

	if (libxmp_init_instrument(m) < 0) {
		return -1;
	}

	for (i = 0; i < mod->ins; i++) {
		struct xmp_instrument *xxi = &mod->xxi[i];
		struct xmp_sample *xxs = &mod->xxs[i];
		struct xmp_subinstrument *sub;

		if (libxmp_alloc_subinstrument(mod, i, 1) < 0)
			return -1;

		sub = &xxi->sub[0];

		xxs->len = 2 * mh.ins[i].size - mh.ins[i].loop_start;
		xxs->lps = 0;
		xxs->lpe = xxs->lps + 2 * mh.ins[i].loop_size;
		xxs->flg = mh.ins[i].loop_size > 1 ? XMP_SAMPLE_LOOP : 0;
		sub->fin = (int8) (mh.ins[i].finetune << 4);
		sub->vol = mh.ins[i].volume;
		sub->pan = 0x80;
		sub->sid = i;
		strncpy((char *)xxi->name, (char *)mh.ins[i].name, 22);

		if (xxs->len > 0) {
			xxi->nsm = 1;
		}
	}

	mod->trk = mod->chn * mod->pat;

	strncpy(mod->name, (char *)mh.name, 20);

	if (libxmp_init_pattern(mod) < 0) {
		return -1;
	}

	/* Load and convert patterns */
	/* Also scan patterns for tracker detection */
	fxused = 0;

	D_(D_INFO "Stored patterns: %d", mod->pat);

	for (i = 0; i < mod->pat; i++) {
		if (libxmp_alloc_pattern_tracks(mod, i, 64) < 0)
			return -1;

		if (hio_read(pat_buf, 1, 1024, f) < 1024)
			return -1;

		mod_event = pat_buf;
		for (j = 0; j < (64 * 4); j++, mod_event += 4) {
			event = &EVENT(i, j % 4, j / 4);
			libxmp_decode_protracker_event(event, mod_event);

			if (event->fxt)
				fxused |= 1 << event->fxt;
			else if (event->fxp)
				fxused |= 1;

			/* UST: Only effects 1 (arpeggio) and 2 (pitchbend) are
			 * available.
			 */
			if (event->fxt && event->fxt != 1 && event->fxt != 2)
				ust = 0;

			/* Karsten Obarski's sleepwalk uses arpeggio 30 and 40 */
			if (event->fxt == 1) {	/* unlikely arpeggio */
				if (event->fxp == 0x00)
					ust = 0;
				/*if ((ev.fxp & 0x0f) == 0 || (ev.fxp & 0xf0) == 0)
				   ust = 0; */
			}

			if (event->fxt == 2) {  /* bend up and down at same time? */
				if ((event->fxp & 0x0f) != 0 && (event->fxp & 0xf0) != 0)
					ust = 0;
			}
		}
	}

	if (fxused & ~0x0006) {
		ust = 0;
	}

	if (ust) {
		modtype = "Ultimate Soundtracker";
	} else if ((fxused & ~0xd007) == 0) {
		modtype = "Soundtracker IX";	/* or MasterSoundtracker? */
	} else if ((fxused & ~0xf807) == 0) {
		modtype = "D.O.C Soundtracker 2.0";
	} else {
		modtype = "unknown tracker 15 instrument";
	}

	snprintf(mod->type, XMP_NAME_SIZE, "%s", modtype);

	MODULE_INFO();

	for (i = 0; i < mod->ins; i++) {
		D_(D_INFO "[%2X] %-22.22s %04x %04x %04x %c V%02x %+d",
		   i, mod->xxi[i].name, mod->xxs[i].len, mod->xxs[i].lps,
		   mod->xxs[i].lpe, mh.ins[i].loop_size > 1 ? 'L' : ' ',
		   mod->xxi[i].sub[0].vol, mod->xxi[i].sub[0].fin >> 4);
	}

	m->quirk |= QUIRK_NOBPM;
	m->period_type = PERIOD_MODRNG;

	/* Perform the necessary conversions for Ultimate Soundtracker */
	if (ust) {
		/* Fix restart & bpm */
		mod->bpm = mod->rst;
		mod->rst = 0;

		/* Fix sample loops */
		for (i = 0; i < mod->ins; i++) {
			/* FIXME */
		}

		/* Fix effects (arpeggio and pitchbending) */
		for (i = 0; i < mod->pat; i++) {
			for (j = 0; j < (64 * 4); j++) {
				event = &EVENT(i, j % 4, j / 4);
				if (event->fxt == 1)
					event->fxt = 0;
				else if (event->fxt == 2
					 && (event->fxp & 0xf0) == 0)
					event->fxt = 1;
				else if (event->fxt == 2
					 && (event->fxp & 0x0f) == 0)
					event->fxp >>= 4;
			}
		}
	} else {
		if (mod->rst >= mod->len)
			mod->rst = 0;
	}

	/* Load samples */

	D_(D_INFO "Stored samples: %d", mod->smp);

	for (i = 0; i < mod->ins; i++) {
		if (!mod->xxs[i].len)
			continue;

		/* Skip transient part of sample.
		 *
		 * Dennis Lindroos <denafcm@gmail.com> reports: One main thing is
		 * sample-looping which on all trackers up to Noisetracker 1 (i
		 * think it was Mahoney who actually changed the loopstart to be
		 * in WORDS) never play looped samples from the beginning, i.e.
		 * only plays the looped part. This can be heard in old modules
		 * especially with "analogstring", "strings2" or "strings3"
		 * samples because these have "slow attack" that is not part of
		 * the loop and thus they sound "sharper"..
		 */
		hio_seek(f, mh.ins[i].loop_start, SEEK_CUR);

		if (libxmp_load_sample(m, f, 0, &mod->xxs[i], NULL) < 0) {
			return -1;
		}
	}

	return 0;
}
