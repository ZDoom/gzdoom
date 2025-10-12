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

#include "loader.h"
#include "iff.h"
#include "../period.h"

/* TODO: Digital Home Studio features (SV19 extensions, IENV, 2.06 format, etc).
 * TODO: Digital Home Studio DTM 2.1 test; 2.03, 2.04, 1.9, 2.1 effects tests.
 * TODO: libxmp needs a new pan law for modules from 2.04 until 1.9.
 * TODO: libxmp does not support track or pattern names.
 * TODO: libxmp does not support SV19 fractional BPM.
 * TODO: libxmp does not support horrible DTM stereo hacks (see below).
 * TODO: libxmp can't tell 2.04 and pre-VERS commercial modules apart.
 * TODO: were external samples ever implemented?
 */

#define MAGIC_D_T_	MAGIC4('D','.','T','.')

/* Values to compare against version_derived for compatibility.
 * These don't directly correspond to real values in the format,
 * they were just picked so the "commercial" formats with VERS would
 * have higher values than those determined by the pattern format.
 * Note that commercial versions 1.0 through 1.9 come after 2.04, and
 * versions prior to 2.015 did not support the DTM format. VERS and
 * SV19 are not emitted up through at least version DT 1.901, so most
 * of the commercial versions will have their modules IDed as 2.04.
 * Later versions (1.914 and 1.917 confirmed) are aware of VERS and SV19.
 *
 * Note that there's a later Digital Tracker release series for BeOS
 * using a completely new format that this loader doesn't support.
 */
#define DTM_V2015	2015		/* Digital Tracker 2.015 and 2.02 */
#define DTM_V203	2030		/* Digital Tracker 2.03 */
#define DTM_V204	2040		/* Digital Tracker 2.04 thru 1.9 */
#define DTM_V19		(19 << 8)	/* Digital Tracker 1.9 (later vers) */
#define DTM_V21		(21 << 8)	/* Digital Home Studio */

/* Pattern format versions, which don't correspond directly to file
 * format versions. Versions 2.015 thru 2.03 have all zero bytes here and
 * store patterns using the Protracker format; 2.04 through 1.9 use ASCII
 * "2.04" and store note numbers instead of periods; Digital Home Studio
 * uses an ASCII "2.06" and stores completely unpacked pattern fields.
 */
#define FORMAT_MOD	0
#define FORMAT_V204	MAGIC4('2','.','0','4')
#define FORMAT_V206	MAGIC4('2','.','0','6')

/* "Mono" mode has always been panoramic stereo, but this wasn't clearly
 * communicated in the documentation or UI until Digital Home Studio. */
#define DTM_OLD_STEREO		0x00
#define DTM_PANORAMIC_STEREO	0xff


static int dt_test(HIO_HANDLE *, char *, const int);
static int dt_load (struct module_data *, HIO_HANDLE *, const int);

const struct format_loader libxmp_loader_dt = {
	"Digital Tracker",
	dt_test,
	dt_load
};

static int dt_test(HIO_HANDLE *f, char *t, const int start)
{
	uint32 size;

	if (hio_read32b(f) != MAGIC_D_T_)
		return -1;

	size = hio_read32b(f);		/* chunk size */
	hio_read16b(f);			/* type */
	hio_read16b(f);			/* stereo mode; global depth (pre-2.04) */
	hio_read16b(f);			/* reserved */
	hio_read16b(f);			/* tempo */
	hio_read16b(f);			/* bpm */
	hio_read32b(f);			/* global sample rate (pre-2.04) */

	CLAMP(size, 14, XMP_NAME_SIZE + 14);
	size -= 14;
	libxmp_read_title(f, t, size);

	return 0;
}


struct local_data {
	int vers_flag;
	int patt_flag;
	int sv19_flag;
	int pflag;
	int sflag;
	int stereo; /* 0=old stereo, ff=panoramic stereo (>=2.04) */
	int depth; /* global sample depth used by pre-2.04 modules */
	int c2spd; /* global sample rate used by pre-2.04 modules */
	int version;
	int version_derived;
	int format;
	int realpat;
	int last_pat;
	int insnum;
	int chn;

	uint8 *patbuf;
	size_t patbuf_alloc;
};


static void dtm_translate_effect(struct xmp_event *event,
				 const struct local_data *data)
{
	switch (event->fxt) {
	case 0x1:			/* portamento up */
	case 0x2:			/* portamento down */
	case 0x3:			/* tone portamento */
	case 0x4:			/* vibrato */
	case 0x5:			/* tone portamento + volslide */
	case 0x6:			/* vibrato + volslide */
	case 0x7:			/* tremolo */
	case 0x9:			/* offset */
	case 0xa:			/* volslide */
	case 0xc:			/* set volume */
		/* Protracker compatible */
		break;

	case 0x0:			/* arpeggio */
		/* DT beta through 2.04: does nothing. */
		if (data->version_derived <= DTM_V203) {
			event->fxp = 0;
		}
		break;

	case 0x8:			/* set panning */
		/* DT 2.04+: only supported in panoramic stereo mode. */
		if (data->version_derived >= DTM_V204 &&
		    data->stereo == DTM_PANORAMIC_STEREO) {
			/* DT 1.9 and up have 800 as full right and 8FF as
			 * full left. 2.04 through 1.1 use the high nibble to
			 * control the channel's left mix value (0:low, F:high)
			 * and the low nibble control the channel's right mix
			 * level. 0 is not completely silent. The default
			 * setting is 15 (full) for each channel.
			 *
			 * For the older behavior, see:
			 *   Bitmaps/no happy end !!!!!!.dtm
			 *   Tyan/fruchtix 1997.dtm
			 */
			if (data->version_derived >= DTM_V19) {
				event->fxp ^= 0xff;
			} else {
				/* TODO: solve DT's old pan law into libxmp's.
				 * would be nice to have pan law support instead.
				 *
				 * L = 0x80 * (left + 1) = vol * (0x80 - pan)
				 * R = 0x80 * (right + 1) = vol * (0x80 + pan)
				 **/
				int left = MSN(event->fxp);
				int right = LSN(event->fxp);
				int pan = 0x80 * (right - left) / (left + right + 2);
				int vol;
				if (right > left) {
					vol = 0x80 * ((right + 1) << 2) / (0x80 + pan);
				} else if (left > right) {
					vol = 0x80 * ((left + 1) << 2) / (0x80 - pan);
				} else {
					vol = (left + 1) << 2;
				}
				event->fxt = FX_SETPAN;
				event->fxp = pan + 0x80;
				event->f2t = FX_TRK_VOL;
				event->f2p = vol;
			}
		} else {
			event->fxt = event->fxp = 0;
		}
		break;

	case 0xb:			/* position jump */
		if (data->version_derived <= DTM_V203 && data->chn > 4) {
			/* DT 2.04 and under: in 6 and 8 channel modules,
			 * the break position is somewhere in the middle of
			 * a pattern unless followed by a Dxx. This can also
			 * do things like jump to position "255".
			 * FIXME: try to more closely approximate this? */
			event->f2t = FX_BREAK;
			event->f2p = 0x32;

		} else if (data->version_derived == DTM_V204) {
			/* DT commercial 1.0 through 1.2: effect is off-by-one;
			 * B01 will jump to 0, B02 will jump to 1, etc.
			 * This works as expected from 1.901 onward.
			 * TODO: does anything rely on the broken version?
			 */
			if (event->fxp > 0)
				event->fxp--;
		}
		break;

	case 0xd:			/* pattern break */
		/* DT beta through commercial 1.2: parameter is ignored.
		 * This works as expected from 1.901 onward.
		 * TODO: does anything rely on the broken version?
		 */
		if (data->version_derived <= DTM_V203) {
			event->fxp = 0;
		}
		break;

	case 0xf:			/* set speed/tempo */
		/* DT beta: all parameters set speed, 0 acts like 1
		 * DT 1.01 thru 2.04: 0 and 20h act like speed 1 (no clamp)
		 * DT commercial 1.0: 0 and 20h act like speed 1 (clamped to 66)
		 * DT commercial 1.1: 0 and 20h act like speed 1 (no clamp)
		 * DT commercial 1.2: 0 is ignored, 20h is a BPM (clamped to 66)
		 * DT commercial 1.901: 0 is ignored, 20h is a BPM (no clamp)
		 * The 66 BPM clamp in some versions seems to be to avoid DSP
		 * bugs with slow BPMs. Whatever issue was fixed by 1.901.
		 * TODO: what does anything actually rely on, if at all?
		 */
		if (data->version_derived <= DTM_V203) {
			if (event->fxp == 0 || event->fxp == 0x20) {
				event->fxp = 1;
			}
		}
		break;

	case 0xe:
		switch(MSN(event->fxp)) {
		case 0x1:		/* fine portamento up */
		case 0x2:		/* fine portamento down */
		case 0x3:		/* glissando control */
		case 0x6:		/* pattern loop */
		case 0xa:		/* fine volslide up */
		case 0xb:		/* fine volslide down */
		case 0xc:		/* note cut */
		case 0xd:		/* note delay */
		case 0xe:		/* pattern delay */
			/* Protracker compatible */
			break;

		case 0x4:		/* vibrato waveform */
			/* DT 2.04 and up: sine rampup(period) square square */
			if ((event->fxp & 3) == 3) {
				event->fxp -= 1;
			}
			/* Continuous variants are missing from all versions. */
			event->fxp &= 3;
			break;

		case 0x5:		/* set finetune */
			/* DT 2.04: only works if a note is present */
			if (event->note == 0) {
				event->fxt = event->fxp = 0;
			}
			break;

		case 0x8:		/* set panning */
			/* Missing from all versions except commercial 1.1,
			 * where it seems to be a global volume effect? */
			/* fall-through */

		default:
			event->fxt = event->fxp = 0;
			break;
		}
		break;

	default:
		event->fxt = event->fxp = 0;
		break;
	}
}

static int dtm_event_size(int format)
{
	switch (format) {
	case FORMAT_MOD:
	case FORMAT_V204:
		return 4;
	case FORMAT_V206:
		return 6;
	}
	return -1;
}

static void dtm_translate_event(struct xmp_event *event, const uint8 *in,
				const struct local_data *data)
{
	switch (data->format) {
	case FORMAT_MOD:
		libxmp_decode_protracker_event(event, in);
		break;

	case FORMAT_V204:
		if (in[0] > 0 && in[0] <= 0x7c) {
			event->note = 12 * (in[0] >> 4) + (in[0] & 0x0f) + 12;
		}
		event->vol = (in[1] & 0xfc) >> 2;
		event->ins = ((in[1] & 0x03) << 4) + (in[2] >> 4);
		event->fxt = in[2] & 0xf;
		event->fxp = in[3];
		break;

	case FORMAT_V206:
		/* FIXME Digital Home Studio */
		break;
	}

	dtm_translate_effect(event, data);
}


static int get_d_t_(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	struct local_data *data = (struct local_data *)parm;
	int name_len = size - 14;
	int b;

	if (size < 14 || size > 142) {
		D_(D_CRIT "invalid D.T. chunk length %d", size);
		return -1;
	}

	hio_read16b(f);			/* type */
	data->stereo = hio_read8(f);	/* 0:old stereo, ff:panoramic stereo */
	data->depth = hio_read8(f);	/* global sample bit depth (pre-2.04) */
	hio_read16b(f);			/* reserved */
	mod->spd = hio_read16b(f);
	if ((b = hio_read16b(f)) > 0)	/* RAMBO.DTM has bpm 0 */
		mod->bpm = b;		/* Not clamped by Digital Tracker. */
	data->c2spd = hio_read32b(f);	/* global sample rate (pre-2.04) */

	if (data->stereo != DTM_OLD_STEREO && data->stereo != DTM_PANORAMIC_STEREO) {
		D_(D_WARN "unknown stereo mode: %d, report this", data->stereo);
		data->stereo = DTM_OLD_STEREO;
	}

	/* Global sample depth is applied to all samples pre-2.04.
	 * Later Digital Tracker versions incorrectly ignore this field when
	 * importing pre-2.04 modules.
	 *
	 * DT 2.015 and 2.02 will save a value of 0 in this field.
	 */
	if (data->depth != 0 && data->depth != 8 && data->depth != 16) {
		D_(D_WARN "unknown global sample depth %d", data->depth);
		data->depth = 8;
	}

	/* Only known used values for global sample rate are 8400 and 24585,
	 * but 12292 and 49170 are also supported and referenced in the UI.
	 * 2.04 will replace the sample rates with whatever was in this field.
	 * The only values that do not crash here besides 8400 are Atari Falcon
	 * hardware frequencies:
	 *
	 * f = floor((25175040 >> 8) / div) = floor(98340 / div)
	 */
	switch (data->c2spd) {
	case 49170:
	case 24585:
	case 12292:
	case 8400:
		break;
	default:
		D_(D_WARN "unknown global sample rate %d, report this", data->c2spd);
		data->c2spd = 8400;
	}

	CLAMP(name_len, 0, XMP_NAME_SIZE);
	hio_read(mod->name, name_len, 1, f);
	libxmp_set_type(m, "Digital Tracker DTM");

	MODULE_INFO();

	return 0;
}

static int get_s_q_(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	int i, maxpat;

	/* Sanity check */
	if (mod->pat != 0) {
		return -1;
	}

	mod->len = hio_read16b(f);
	mod->rst = hio_read16b(f);

	/* Sanity check */
	if (mod->len > 256 || mod->rst > 255) {
		return -1;
	}

	hio_read32b(f);	/* reserved */

	for (maxpat = i = 0; i < 128; i++) {
		mod->xxo[i] = hio_read8(f);
		if (mod->xxo[i] > maxpat)
			maxpat = mod->xxo[i];
	}
	mod->pat = maxpat + 1;

	return 0;
}

static int get_vers(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	struct local_data *data = (struct local_data *)parm;

	if (data->vers_flag || size < 4) {
		return 0;
	}
	data->vers_flag = 1;

	data->version = hio_read32b(f);
	D_(D_INFO "DTM version    : %d", data->version);
	return 0;
}

static int get_sv19(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	struct local_data *data = (struct local_data *)parm;
	uint8 buf[32 * 2];
	uint32 bpm_frac;
	int i;

	if (!data->vers_flag || data->version < 19 || size < 86) {
		/* ignore in the extreme off chance a module with this exists. */
		return 0;
	}
	data->sv19_flag = 1;

	hio_read16b(f);			/* ticks per beat, by default 24 */
	bpm_frac = hio_read32b(f);	/* initial BPM (fractional portion) */

	/* Round up to nearest for now. */
	if (bpm_frac >= 0x80000000u) {
		mod->bpm++;
	}

	/* Initial pan table. 0=left 90=center 180=right
	 * Do not load in old stereo mode for now. */
	if (hio_read(buf, 1, 64, f) < 64) {
		return -1;
	}
	for (i = 0; i < 32; i++) {
		int val = readmem16b(buf + 2 * i);
		if (val <= 180 && data->stereo == DTM_PANORAMIC_STEREO)
			mod->xxc[i].pan = val * 0x100 / 180;
	}

	/* Instrument type table.
	 * The file format claims 32 bytes but it's only 16.
	 * It's not clear how to influence this table, if possible. */

	return 0;
}

static int get_patt(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	struct local_data *data = (struct local_data *)parm;
	uint8 ver[4];

	/* Sanity check */
	if (data->patt_flag || data->pflag || size < 8) {
		return -1;
	}
	data->patt_flag = 1;

	data->chn = mod->chn = hio_read16b(f);
	data->realpat = hio_read16b(f);
	if (hio_read(ver, 1, 4, f) < 4) {
		return -1;
	}
	data->format = readmem32b(ver);
	mod->trk = mod->chn * mod->pat;

	D_(D_INFO "PATT channels  : %d", mod->chn);
	D_(D_INFO "PATT patterns  : %d", data->realpat);
	if (data->format != FORMAT_MOD) {
		D_(D_INFO "PATT format    : %c%c%c%c",
		   ver[0], ver[1], ver[2], ver[3]);
		data->version_derived =
		 (data->format == FORMAT_V206) ? DTM_V21 : DTM_V204;
	} else {
		D_(D_INFO "PATT format    : Protracker");

		/* DTM 2.015/2.02 have depth=0 and 31 instruments instead of 63.
		 * There are also modules with depth!=0 but 31 instruments,
		 * and it's not clear what the origin of those is. */
		if (data->depth != 0)
			data->version_derived = DTM_V203;
		else
			data->version_derived = DTM_V2015;
	}

	if (data->vers_flag && data->version) {
		data->version_derived = data->version << 8;
		m->flow_mode = FLOW_MODE_DTM_19;
	} else {
		/* 6/8 channel mode's position jump bugs also affect its
		 * interactions with pattern loop. (Very weakly) attempt
		 * to support the bugs. */
		m->flow_mode = data->version_derived <= DTM_V204 && mod->chn > 4 ?
				FLOW_MODE_DTM_2015_6CH : FLOW_MODE_DTM_2015;
	}

	/* Sanity check */
	if (mod->chn > XMP_MAX_CHANNELS) {
		return -1;
	}

	if (dtm_event_size(data->format) < 0) {
		D_(D_CRIT "unknown pattern format");
		return -1;
	}

	return 0;
}

static int get_inst(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	struct local_data *data = (struct local_data *)parm;
	int i, c2spd;
	uint8 buf[50];

	/* Sanity check */
	if (mod->ins != 0) {
		return -1;
	}

	mod->ins = mod->smp = hio_read16b(f);

	/* Sanity check */
	if (mod->ins > MAX_INSTRUMENTS) {
		return -1;
	}

	D_(D_INFO "Instruments    : %d ", mod->ins);

	if (libxmp_init_instrument(m) < 0)
		return -1;

	for (i = 0; i < mod->ins; i++) {
		int len, repstart, replen;
		int fine, stereo, midinote;

		if (libxmp_alloc_subinstrument(mod, i, 1) < 0)
			return -1;

		if (hio_read(buf, 1, 50, f) < 50)
			return -1;

		/*hio_read32b(f);*/	/* reserved */
		len = readmem32b(buf + 4);
		if (len < 0) {
			len = 0;
		}
		mod->xxs[i].len = len;
		mod->xxi[i].nsm = (mod->xxs[i].len > 0);
		fine = (int8)(buf[8] << 4);		/* finetune */
		mod->xxi[i].sub[0].vol = buf[9];
		mod->xxi[i].sub[0].pan = 0x80;
		repstart = readmem32b(buf + 10);
		replen = readmem32b(buf + 14);

		if (repstart >= len || repstart < 0 || replen < 0) {
			repstart = 0;
			replen = 0;
		}
		if (replen > len - repstart) {
			replen = len - repstart;
		}
		mod->xxs[i].lps = repstart;
		mod->xxs[i].lpe = repstart + replen;

		libxmp_instrument_name(mod, i, buf + 18, 22);

		/* DT pre-2.04: global sample depth is used for playback; the
		 * sample depth field seems to be for the resampler only.
		 * See Lot/5th-2.dtm, which relies on this.
		 */
		if (data->version_derived <= DTM_V203) {
			buf[41] = data->depth ? data->depth : 8;
		}

		stereo = buf[40];	/* stereo */
		if (buf[41] > 8) {	/* resolution (8, 16, or rarely 0) */
			mod->xxs[i].flg |= XMP_SAMPLE_16BIT;
			mod->xxs[i].len >>= 1;
			mod->xxs[i].lps >>= 1;
			mod->xxs[i].lpe >>= 1;
		}

		/* 2.04 through 1.1: panoramic mode forces stereo samples to be
		 * interpreted as mono. This bug was fixed in 1.901 or another
		 * commercial version, so this can't really be checked, and
		 * there's no reason to do this anyway. */
		/*
		if (data->stereo == DTM_PANORAMIC_STEREO &&
		    data->version_derived == DTM_V204) {
			stereo = 0;
		}
		*/

		if (stereo) {
			/* TODO: in old stereo mode, stereo samples do something
			 * unusual: the left channel is sent to the first
			 * channel of the "pair" it is played in, and the
			 * right channel is sent to the second channel of
			 * the "pair" (a "pair" is channels 1&2, 3&4, etc.).
			 * If the stereo sample is played in the first channel,
			 * it overrides whatever is played by the second, but
			 * if it is played in the second channel, the first
			 * channel can override it (or more often is silent).
			 * Note that for 1&2, 5&6, etc, the left sample will
			 * pan right and the right sample will be pan left.
			 *
			 * The worst part of this is that each channel of the
			 * stereo sample is subject to the pitch and volume OF
			 * THE CHANNEL IT IS PLAYING IN. This probably needs a
			 * custom read_event and some mixer hacks.
			 */
			mod->xxs[i].flg |= XMP_SAMPLE_STEREO;
			mod->xxs[i].len >>= 1;
			mod->xxs[i].lps >>= 1;
			mod->xxs[i].lpe >>= 1;
		}
		if (mod->xxs[i].lpe -  mod->xxs[i].lps > 1) {
			mod->xxs[i].flg |= XMP_SAMPLE_LOOP;
		}

		midinote = readmem16b(buf + 42);	/* midi note */
		/*hio_read16b(f);*/			/* unknown (0x0000) */
		c2spd = readmem32b(buf + 46);		/* frequency */

		/* DT pre-2.04: the sample rate is used for resampling only(?)
		 * and the global sample rate is used for playback instead.
		 * This field was removed from 2.04. */
		if (data->version_derived <= DTM_V203) {
			c2spd = data->c2spd;
		}
		libxmp_c2spd_to_note(c2spd, &mod->xxi[i].sub[0].xpo, &mod->xxi[i].sub[0].fin);

		/* DT 2.03 through commercial 1.2: midi note resamples
		 * the sample and changes the rate in some cases, but does
		 * nothing during normal playback.
		 * DT 1.901+: changing the midi note acts as a transpose
		 * during playback and does not modify the sample at all.
		 * Values under C-2 are clamped, over B-7 are ignored.
		 */
		if (data->version_derived >= DTM_V19 && midinote < 95) {
			mod->xxi[i].sub[0].xpo += 48 - MAX(24, midinote);
		}

		/* It's strange that we have both c2spd and finetune */
		mod->xxi[i].sub[0].fin += fine;

		mod->xxi[i].sub[0].sid = i;

		D_(D_INFO "[%2X] %-22.22s %05x%c%05x %05x %c%c %2db V%02x F%+03d %5d",
			i, mod->xxi[i].name,
			mod->xxs[i].len,
			mod->xxs[i].flg & XMP_SAMPLE_16BIT ? '+' : ' ',
			repstart,
			replen,
			mod->xxs[i].flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
			stereo ? 'S' : ' ',
			buf[41],
			mod->xxi[i].sub[0].vol,
			fine,
			c2spd);
	}

	return 0;
}

static int get_dapt(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	struct local_data *data = (struct local_data *)parm;
	int pat, i, j, k;
	struct xmp_event *event;
	uint8 *pos;
	size_t total_size;
	int event_size;
	int rows;

	if (!data->patt_flag) {
		return -1;
	}

	if (!data->pflag) {
		D_(D_INFO "Stored patterns: %d", mod->pat);
		data->pflag = 1;
		data->last_pat = 0;

		if (libxmp_init_pattern(mod) < 0)
			return -1;
	}

	hio_read32b(f);	/* 0xffffffff */
	pat = hio_read16b(f);
	rows = hio_read16b(f);

	/* Sanity check */
	/* DT 2.04, all known versions of 1.9x: maximum rows is 96.
	 * These can load/resave larger patterns made with a hex editor,
	 * but with a buggy UI. It isn't clear how stable these are. */
	/* DHS: maximum configurable by interface is 4*99 = 396.
	 * TODO: it uses a different format for this entire chunk. */
	if (pat < 0 || pat >= mod->pat || rows < 0 ||
	    (data->version_derived <= DTM_V19 && rows > 96) ||
	    (data->version_derived > DTM_V19 && rows > 396)) {
		return -1;
	}
	if (pat < data->last_pat) {
		return -1;
	}

	for (i = data->last_pat; i <= pat; i++) {
		if (libxmp_alloc_pattern_tracks(mod, i, rows) < 0)
			return -1;
	}
	data->last_pat = pat + 1;

	/* (Re)allocate the pattern buffer. */
	event_size = dtm_event_size(data->format);
	total_size = event_size * rows * mod->chn;
	if (data->patbuf_alloc < total_size) {
		uint8 *tmp = (uint8 *) realloc(data->patbuf, total_size);
		if (tmp == NULL) {
			return -1;
		}
		data->patbuf = tmp;
		data->patbuf_alloc = total_size;
	}
	if (hio_read(data->patbuf, 1, total_size, f) < total_size) {
		D_(D_CRIT "read error at pattern %d", pat);
		return -1;
	}

	pos = data->patbuf;
	for (j = 0; j < rows; j++) {
		for (k = 0; k < mod->chn; k++) {
			event = &EVENT(pat, k, j);
			dtm_translate_event(event, pos, data);
			pos += event_size;
		}
	}

	return 0;
}

static int get_dait(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	struct xmp_module *mod = &m->mod;
	struct local_data *data = (struct local_data *)parm;

	if (!data->sflag) {
		D_(D_INFO "Stored samples : %d ", mod->smp);
		data->sflag = 1;
		data->insnum = 0;
	}

	if (size > 2) {
		int ret;

		/* Sanity check */
		if (data->insnum >= mod->ins) {
			return -1;
		}

		ret = libxmp_load_sample(m, f,
			SAMPLE_FLAG_BIGEND | SAMPLE_FLAG_INTERLEAVED,
			&mod->xxs[mod->xxi[data->insnum].sub[0].sid], NULL);

		if (ret < 0)
			return -1;
	}

	data->insnum++;

	return 0;
}

static int get_text(struct module_data *m, int size, HIO_HANDLE *f, void *parm)
{
	int skip_byte;
	uint32 len;

	if (size < 12 || (size & 1) || m->comment != NULL) {
		D_(D_INFO "Ignoring TEXT chunk of length %d", size);
		return 0;
	}
	/*type	= */ hio_read16b(f);
	len	= hio_read32b(f);
	/*tab	= */ hio_read16b(f);
	/*res	= */ hio_read16b(f);

	/* "=$FFFF <=> length is odd" */
	if (hio_read16b(f) == 0xffff) {
		skip_byte = 1;
	} else {
		skip_byte = 0;
	}

	if (len != (uint32)(size - 12 - skip_byte)) {
		D_(D_INFO "Bad TEXT length (chunk %d, len %u)", size, (unsigned)len);
		return 0;
	}
	if (len == 0) {
		D_(D_INFO "TEXT is empty, skipping");
		return 0;
	}
	if ((long)len > hio_size(f) - hio_tell(f)) {
		D_(D_INFO "TEXT chunk wouldn't fit within file, skipping");
		return 0;
	}

	if ((m->comment = (char * ) malloc(len + 1)) != NULL) {
		if (skip_byte) {
			hio_read8(f);
		}
		len = hio_read(m->comment, 1, len, f);
		m->comment[len] = '\0';
	}
	return 0;
}

static int dt_load(struct module_data *m, HIO_HANDLE *f, const int start)
{
	iff_handle handle;
	struct local_data data;
	struct xmp_module *mod = &m->mod;
	int ret, i;

	LOAD_INIT();

	memset(&data, 0, sizeof (struct local_data));

	handle = libxmp_iff_new();
	if (handle == NULL)
		return -1;

	m->c4rate = C4_NTSC_RATE;

	/* IFF chunk IDs */
	ret = libxmp_iff_register(handle, "D.T.", get_d_t_);
	ret |= libxmp_iff_register(handle, "S.Q.", get_s_q_);
	ret |= libxmp_iff_register(handle, "VERS", get_vers);
	ret |= libxmp_iff_register(handle, "PATT", get_patt);
	ret |= libxmp_iff_register(handle, "INST", get_inst);
	ret |= libxmp_iff_register(handle, "SV19", get_sv19);
	ret |= libxmp_iff_register(handle, "DAPT", get_dapt);
	ret |= libxmp_iff_register(handle, "DAIT", get_dait);
	ret |= libxmp_iff_register(handle, "TEXT", get_text);

	if (ret != 0)
		return -1;

	/* Load IFF chunks */
	ret = libxmp_iff_load(handle, m, f , &data);
	libxmp_iff_release(handle);
	free(data.patbuf);
	if (ret < 0)
		return -1;

	/* alloc remaining patterns */
	if (mod->xxp != NULL) {
		for (i = data.last_pat; i < mod->pat; i++) {
			if (libxmp_alloc_pattern_tracks(mod, i, 64) < 0) {
				return -1;
			}
		}
	}

	/* Correct the module type now that the version fields are known. */
	if (data.version >= 20) {
		libxmp_set_type(m, "Digital Home Studio DTM %d.%d",
				data.version / 10, data.version % 10);
	} else if (data.version) {
		libxmp_set_type(m, "Digital Tracker %d.%d DTM",
				data.version / 10, data.version % 10);
	} else if (data.format == FORMAT_V204) {
		libxmp_set_type(m, "Digital Tracker 2.04 DTM");
	} else if (data.depth != 0) {
		libxmp_set_type(m, "Digital Tracker 2.03 DTM");
	} else {
		libxmp_set_type(m, "Digital Tracker 2.015 DTM");
	}

	if (data.version_derived >= DTM_V204 &&
	    data.stereo == DTM_PANORAMIC_STEREO) {
		/* Panoramic stereo mode: all channels default to center.
		 * In 1.9xx, the SV19 chunk is used to specify initial values,
		 * so this should be skipped if it was loaded.
		 */
		if (!data.sv19_flag) {
			for (i = 0; i < mod->chn; i++) {
				mod->xxc[i].pan = 0x80;
			}
		}
	}

	return 0;
}

