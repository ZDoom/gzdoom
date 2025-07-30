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
#include "mod.h"
#include "../period.h"

static int flt_test(HIO_HANDLE *, char *, const int);
static int flt_load(struct module_data *, HIO_HANDLE *, const int);

const struct format_loader libxmp_loader_flt = {
	"Startrekker",
	flt_test,
	flt_load
};

static int flt_test(HIO_HANDLE * f, char *t, const int start)
{
	char buf[4];

	hio_seek(f, start + 1080, SEEK_SET);
	if (hio_read(buf, 1, 4, f) < 4)
		return -1;

	/* Also RASP? */
	if (memcmp(buf, "FLT", 3) && memcmp(buf, "EXO", 3))
		return -1;

	if (buf[3] != '4' && buf[3] != '8' && buf[3] != 'M')
		return -1;

	hio_seek(f, start + 0, SEEK_SET);
	libxmp_read_title(f, t, 20);

	return 0;
}

/* Waveforms from the Startrekker 1.2 AM synth replayer code */

static const int8 am_waveform[3][32] = {
	{    0,   25,   49,   71,   90,  106,  117,  125,	/* Sine */
	   127,  125,  117,  106,   90,   71,   49,   25,
	     0,  -25,  -49,  -71,  -90, -106, -117, -125,
	  -127, -125, -117, -106,  -90,  -71,  -49,  -25
	},

	{ -128, -120, -112, -104,  -96,  -88,  -80,  -72,	/* Ramp */
	   -64,  -56,  -48,  -40,  -32,  -24,  -16,   -8,
	     0,    8,   16,   24,   32,   40,   48,   56,
	    64,   72,   80,   88,   96,  104,  112,  120
	},

	{ -128, -128, -128, -128, -128, -128, -128, -128,	/* Square */
	  -128, -128, -128, -128, -128, -128, -128, -128,
	   127,  127,  127,  127,  127,  127,  127,  127,
	   127,  127,  127,  127,  127,  127,  127,  127
	}
};

struct am_instrument {
	int16 l0;		/* start amplitude */
	int16 a1l;		/* attack level */
	int16 a1s;		/* attack speed */
	int16 a2l;		/* secondary attack level */
	int16 a2s;		/* secondary attack speed */
	int16 sl;		/* sustain level */
	int16 ds;		/* decay speed */
	int16 st;		/* sustain time */
	int16 rs;		/* release speed */
	int16 wf;		/* waveform */
	int16 p_fall;		/* ? */
	int16 v_amp;		/* vibrato amplitude */
	int16 v_spd;		/* vibrato speed */
	int16 fq;		/* base frequency */
};

static int is_am_instrument(HIO_HANDLE *nt, int i)
{
	char buf[2];
	int16 wf;

	hio_seek(nt, 144 + i * 120, SEEK_SET);
	hio_read(buf, 1, 2, nt);
	if (memcmp(buf, "AM", 2))
		return 0;
	hio_seek(nt, 24, SEEK_CUR);
	wf = hio_read16b(nt);
	if (hio_error(nt) || wf < 0 || wf > 3)
		return 0;

	return 1;
}

static int read_am_instrument(struct module_data *m, HIO_HANDLE *nt, int i)
{
	struct xmp_module *mod = &m->mod;
	struct xmp_instrument *xxi = &mod->xxi[i];
	struct xmp_sample *xxs = &mod->xxs[i];
	struct xmp_envelope *vol_env = &xxi->aei;
	struct xmp_envelope *freq_env = &xxi->fei;
	struct am_instrument am;
	char *wave;
	int a, b;
	int8 am_noise[1024];

	hio_seek(nt, 144 + i * 120 + 2 + 4, SEEK_SET);
	am.l0  = hio_read16b(nt);
	am.a1l = hio_read16b(nt);
	am.a1s = hio_read16b(nt);
	am.a2l = hio_read16b(nt);
	am.a2s = hio_read16b(nt);
	am.sl  = hio_read16b(nt);
	am.ds  = hio_read16b(nt);
	am.st  = hio_read16b(nt);
	hio_read16b(nt);
	am.rs  = hio_read16b(nt);
	am.wf  = hio_read16b(nt);
	am.p_fall = -(int16) hio_read16b(nt);
	am.v_amp = hio_read16b(nt);
	am.v_spd = hio_read16b(nt);
	am.fq  = hio_read16b(nt);

	if (hio_error(nt)) {
		return -1;
	}

#if 0
	printf
	    ("L0=%d A1L=%d A1S=%d A2L=%d A2S=%d SL=%d DS=%d ST=%d RS=%d WF=%d\n",
	     am.l0, am.a1l, am.a1s, am.a2l, am.a2s, am.sl, am.ds, am.st, am.rs,
	     am.wf);
#endif

	if (am.wf < 3) {
		xxs->len = 32;
		xxs->lps = 0;
		xxs->lpe = 32;
		wave = (char *)&am_waveform[am.wf][0];
	} else {
		int j;

		xxs->len = 1024;
		xxs->lps = 0;
		xxs->lpe = 1024;

		for (j = 0; j < 1024; j++)
			am_noise[j] = rand() % 256;

		wave = (char *)&am_noise[0];
	}

	xxs->flg = XMP_SAMPLE_LOOP;
	xxi->sub[0].vol = 0x40;	/* prelude.mod has 0 in instrument */
	xxi->nsm = 1;
	xxi->sub[0].xpo = -12 * am.fq;
	xxi->sub[0].vwf = 0;
	xxi->sub[0].vde = am.v_amp << 2;
	xxi->sub[0].vra = am.v_spd;

	/*
	 * AM synth envelope parameters based on the Startrekker 1.2 docs
	 *
	 * L0    Start amplitude for the envelope
	 * A1L   Attack level
	 * A1S   The speed that the amplitude changes to the attack level, $1
	 *       is slow and $40 is fast.
	 * A2L   Secondary attack level, for those who likes envelopes...
	 * A2S   Secondary attack speed.
	 * DS    The speed that the amplitude decays down to the:
	 * SL    Sustain level. There is remains for the time set by the
	 * ST    Sustain time.
	 * RS    Release speed. The speed that the amplitude falls from ST to 0.
	 */
	if (am.a1s == 0)
		am.a1s = 1;
	if (am.a2s == 0)
		am.a2s = 1;
	if (am.ds == 0)
		am.ds = 1;
	if (am.rs == 0)
		am.rs = 1;

	vol_env->npt = 6;
	vol_env->flg = XMP_ENVELOPE_ON;

	vol_env->data[0] = 0;
	vol_env->data[1] = am.l0 / 4;

	/*
	 * Startrekker increments/decrements the envelope by the stage speed
	 * until it reaches the next stage level.
	 *
	 *         ^
	 *         |
	 *     100 +.........o
	 *         |        /:
	 *     A2L +.......o :        x = 256 * (A2L - A1L) / (256 - A1L)
	 *         |      /: :
	 *         |     / : :
	 *     A1L +....o..:.:
	 *         |    :  : :
	 *         |    :x : :
	 *         +----+--+-+----->
	 *              |    |
	 *              |256/|
	 *               A2S
	 */

	if (am.a1l > am.l0) {
		a = am.a1l - am.l0;
		b = 256 - am.l0;
	} else {
		a = am.l0 - am.a1l;
		b = am.l0;
	}
	if (b == 0)
		b = 1;

	vol_env->data[2] = vol_env->data[0] + (256 * a) / (am.a1s * b);
	vol_env->data[3] = am.a1l / 4;

	if (am.a2l > am.a1l) {
		a = am.a2l - am.a1l;
		b = 256 - am.a1l;
	} else {
		a = am.a1l - am.a2l;
		b = am.a1l;
	}
	if (b == 0)
		b = 1;

	vol_env->data[4] = vol_env->data[2] + (256 * a) / (am.a2s * b);
	vol_env->data[5] = am.a2l / 4;

	if (am.sl > am.a2l) {
		a = am.sl - am.a2l;
		b = 256 - am.a2l;
	} else {
		a = am.a2l - am.sl;
		b = am.a2l;
	}
	if (b == 0)
		b = 1;

	vol_env->data[6] = vol_env->data[4] + (256 * a) / (am.ds * b);
	vol_env->data[7] = am.sl / 4;
	vol_env->data[8] = vol_env->data[6] + am.st;
	vol_env->data[9] = am.sl / 4;
	vol_env->data[10] = vol_env->data[8] + (256 / am.rs);
	vol_env->data[11] = 0;

	/*
	 * Implement P.FALL using pitch envelope
	 */

	if (am.p_fall) {
		freq_env->npt = 2;
		freq_env->flg = XMP_ENVELOPE_ON;
		freq_env->data[0] = 0;
		freq_env->data[1] = 0;
		freq_env->data[2] = 1024 / abs(am.p_fall);
		freq_env->data[3] = 10 * (am.p_fall < 0 ? -256 : 256);
	}

	if (libxmp_load_sample(m, NULL, SAMPLE_FLAG_NOLOAD, xxs, wave))
		return -1;

	return 0;
}

static int flt_load(struct module_data *m, HIO_HANDLE * f, const int start)
{
	struct xmp_module *mod = &m->mod;
	int i, j;
	struct xmp_event *event;
	struct mod_header mh;
	uint8 mod_event[4];
	const char *tracker;
	char filename[1024];
	char buf[16];
	HIO_HANDLE *nt;
	int am_synth;

	LOAD_INIT();

	/* See if we have the synth parameters file */
	am_synth = 0;
	snprintf(filename, 1024, "%s%s.NT", m->dirname, m->basename);
	if ((nt = hio_open(filename, "rb")) == NULL) {
		snprintf(filename, 1024, "%s%s.nt", m->dirname, m->basename);
		if ((nt = hio_open(filename, "rb")) == NULL) {
			snprintf(filename, 1024, "%s%s.AS", m->dirname,
				 m->basename);
			if ((nt = hio_open(filename, "rb")) == NULL) {
				snprintf(filename, 1024, "%s%s.as", m->dirname,
					 m->basename);
				nt = hio_open(filename, "rb");
			}
		}
	}

	tracker = "Startrekker";

	if (nt) {
		if (hio_read(buf, 1, 16, nt) != 16) {
			goto err;
		}
		if (memcmp(buf, "ST1.2 ModuleINFO", 16) == 0) {
			am_synth = 1;
			tracker = "Startrekker 1.2";
		} else if (memcmp(buf, "ST1.3 ModuleINFO", 16) == 0) {
			am_synth = 1;
			tracker = "Startrekker 1.3";
		} else if (memcmp(buf, "AudioSculpture10", 16) == 0) {
			am_synth = 1;
			tracker = "AudioSculpture 1.0";
		}
	}

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
	hio_read(mh.order, 128, 1, f);
	hio_read(mh.magic, 4, 1, f);

	if (mh.magic[3] == '4') {
		mod->chn = 4;
	} else {
		mod->chn = 8;
	}

	mod->ins = 31;
	mod->smp = mod->ins;
	mod->len = mh.len;
	mod->rst = mh.restart;
	memcpy(mod->xxo, mh.order, 128);

	for (i = 0; i < 128; i++) {
		if (mod->chn > 4)
			mod->xxo[i] >>= 1;
		if (mod->xxo[i] > mod->pat)
			mod->pat = mod->xxo[i];
	}

	mod->pat++;

	mod->trk = mod->chn * mod->pat;

	strncpy(mod->name, (char *)mh.name, 20);
	libxmp_set_type(m, "%s %4.4s", tracker, mh.magic);
	MODULE_INFO();

	if (libxmp_init_instrument(m) < 0)
		goto err;

	for (i = 0; i < mod->ins; i++) {
		struct xmp_instrument *xxi = &mod->xxi[i];
		struct xmp_sample *xxs = &mod->xxs[i];
		struct xmp_subinstrument *sub;

		if (libxmp_alloc_subinstrument(mod, i, 1) < 0)
			goto err;

		sub = &xxi->sub[0];

		xxs->len = 2 * mh.ins[i].size;
		xxs->lps = 2 * mh.ins[i].loop_start;
		xxs->lpe = xxs->lps + 2 * mh.ins[i].loop_size;
		xxs->flg = mh.ins[i].loop_size > 1 ? XMP_SAMPLE_LOOP : 0;
		sub->fin = (int8) (mh.ins[i].finetune << 4);
		sub->vol = mh.ins[i].volume;
		sub->pan = 0x80;
		sub->sid = i;
		xxi->rls = 0xfff;

		if (xxs->len > 0)
			xxi->nsm = 1;

		libxmp_instrument_name(mod, i, mh.ins[i].name, 22);
	}

	if (libxmp_init_pattern(mod) < 0)
		goto err;

	/* Load and convert patterns */
	D_(D_INFO "Stored patterns: %d", mod->pat);

	/* "The format you are looking for is FLT8, and the ONLY two
	 *  differences are: It says FLT8 instead of FLT4 or M.K., AND, the
	 *  patterns are PAIRED. I thought this was the easiest 8 track
	 *  format possible, since it can be loaded in a normal 4 channel
	 *  tracker if you should want to rip sounds or patterns. So, in a
	 *  8 track FLT8 module, patterns 00 and 01 is "really" pattern 00.
	 *  Patterns 02 and 03 together is "really" pattern 01. Thats it.
	 *  Oh well, I didnt have the time to implement all effect commands
	 *  either, so some FLT8 modules would play back badly (I think
	 *  especially the portamento command uses a different "scale" than
	 *  the normal portamento command, that would be hard to patch).
	 */
	for (i = 0; i < mod->pat; i++) {
		if (libxmp_alloc_pattern_tracks(mod, i, 64) < 0)
			goto err;

		for (j = 0; j < (64 * 4); j++) {
			event = &EVENT(i, j % 4, j / 4);
			if (hio_read(mod_event, 1, 4, f) < 4) {
				D_(D_CRIT "read error at pat %d", i);
				goto err;
			}
			libxmp_decode_noisetracker_event(event, mod_event);
		}
		if (mod->chn > 4) {
			for (j = 0; j < (64 * 4); j++) {
				event = &EVENT(i, (j % 4) + 4, j / 4);
				if (hio_read(mod_event, 1, 4, f) < 4) {
					D_(D_CRIT "read error at pat %d", i);
					goto err;
				}
				libxmp_decode_noisetracker_event(event, mod_event);

				/* no macros */
				if (event->fxt == 0x0e)
					event->fxt = event->fxp = 0;
			}
		}
	}

	/* no such limit for synth instruments
	 * mod->flg |= XXM_FLG_MODRNG;
	 */

	/* Load samples */

	D_(D_INFO "Stored samples: %d", mod->smp);

	for (i = 0; i < mod->smp; i++) {
		if (mod->xxs[i].len == 0) {
			if (am_synth && is_am_instrument(nt, i)) {
				if (read_am_instrument(m, nt, i) < 0) {
					D_(D_CRIT "Missing nt file");
					goto err;
				}
			}
			continue;
		}
		if (libxmp_load_sample(m, f, SAMPLE_FLAG_FULLREP, &mod->xxs[i], NULL) <
		    0) {
			goto err;
		}
	}

	if (nt) {
		hio_close(nt);
	}

	return 0;

      err:
	if (nt) {
		hio_close(nt);
	}

	return -1;
}
