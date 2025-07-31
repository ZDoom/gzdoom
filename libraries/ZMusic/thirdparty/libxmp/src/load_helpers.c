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

#include <ctype.h>
#include "common.h"
#include "loaders/loader.h"


#ifndef LIBXMP_CORE_PLAYER

/*
 * Handle special "module quirks" that can't be detected automatically
 * such as Protracker 2.x compatibility, vblank timing, etc.
 */

struct module_quirk {
	uint8 md5[16];
	int flags;
	int mode;
};

const struct module_quirk mq[] = {
	/* "No Mercy" by Alf/VTL (added by Martin Willers) */
	{
		{ 0x36, 0x6e, 0xc0, 0xfa, 0x96, 0x2a, 0xeb, 0xee,
	  	  0x03, 0x4a, 0xa2, 0xdb, 0xaa, 0x49, 0xaa, 0xea },
		0, XMP_MODE_PROTRACKER
	},

	/* mod.souvenir of china */
	{
		{ 0x93, 0xf1, 0x46, 0xae, 0xb7, 0x58, 0xc3, 0x9d,
		  0x8b, 0x5f, 0xbc, 0x98, 0xbf, 0x23, 0x7a, 0x43 },
		XMP_FLAGS_FIXLOOP, XMP_MODE_AUTO
	},

#if 0
	/* "siedler ii" (added by Daniel Åkerud) */
	/* Timing fixed by vblank scan compare. CIA: 32m10s  VBlank: 12m32s */
	{
		{ 0x70, 0xaa, 0x03, 0x4d, 0xfb, 0x2f, 0x1f, 0x73,
		  0xd9, 0xfd, 0xba, 0xfe, 0x13, 0x1b, 0xb7, 0x01 },
		XMP_FLAGS_VBLANK, XMP_MODE_AUTO
	},
#endif

	/* "Klisje paa klisje" (added by Kjetil Torgrim Homme) */
	{
		{ 0xe9, 0x98, 0x01, 0x2c, 0x70, 0x0e, 0xb4, 0x3a,
		  0xf0, 0x32, 0x17, 0x11, 0x30, 0x58, 0x29, 0xb2 },
		0, XMP_MODE_NOISETRACKER
	},

#if 0
	/* -- Already covered by Noisetracker fingerprinting -- */

	/* Another version of Klisje paa klisje sent by Steve Fernandez */
	{
		{ 0x12, 0x19, 0x1c, 0x90, 0x41, 0xe3, 0xfd, 0x70,
		  0xb7, 0xe6, 0xb3, 0x94, 0x8b, 0x21, 0x07, 0x63 },
		XMP_FLAGS_VBLANK
	},
#endif

	/* "((((( nebulos )))))" sent by Tero Auvinen (AMP version) */
	{
		{ 0x51, 0x6e, 0x8d, 0xcc, 0x35, 0x7d, 0x50, 0xde,
		  0xa9, 0x85, 0xbe, 0xbf, 0x90, 0x2e, 0x42, 0xdc },
		0, XMP_MODE_NOISETRACKER
	},

	/* Purple Motion's Sundance.mod, Music Channel BBS edit */
	{
		{ 0x5d, 0x3e, 0x1e, 0x08, 0x28, 0x52, 0x12, 0xc7,
		  0x17, 0x64, 0x95, 0x75, 0x98, 0xe6, 0x95, 0xc1 },
		0, XMP_MODE_ST3
	},

	/* Asle's Ode to Protracker */
	{
		{ 0x97, 0xa3, 0x7d, 0x30, 0xd7, 0xae, 0x6d, 0x50,
		  0xc9, 0x62, 0xe9, 0xd8, 0x87, 0x1b, 0x7e, 0x8a },
		0, XMP_MODE_PROTRACKER
	},

	/* grooving3.mod */
	/* length 150778 crc32c 0xfdcf9aadU */
	{
		{ 0xdb, 0x61, 0x22, 0x44, 0x39, 0x85, 0x74, 0xe9,
		  0xfa, 0x11, 0xb8, 0xfb, 0x87, 0xe8, 0xde, 0xc5, },
		XMP_FLAGS_VBLANK, XMP_MODE_AUTO
	},
	/* mod.Rundgren */
	/* length 195078 crc32c 0x8fa827a4U */
	{
		{ 0x9a, 0xdb, 0xb2, 0x09, 0x07, 0x1c, 0x44, 0x82,
		  0xc5, 0xdf, 0x83, 0x52, 0xcc, 0x73, 0x9f, 0x20, },
		XMP_FLAGS_VBLANK, XMP_MODE_AUTO
	},
	/* dance feeling by Audiomonster */
	/* length 169734 crc32c 0x79fa2c9bU */
	{
		{ 0x31, 0x2c, 0x3d, 0xaa, 0x5f, 0x1a, 0x54, 0x44,
		  0x9d, 0xf7, 0xc4, 0x41, 0x8a, 0xc5, 0x01, 0x02, },
		XMP_FLAGS_VBLANK, XMP_MODE_AUTO
	},
	/* knights melody by Audiomonster */
	/* length 77798 crc32c 0x7bf19c5bU */
	{
		{ 0x31, 0xc3, 0x0e, 0x32, 0xfc, 0x99, 0x95, 0xd2,
		  0x97, 0x20, 0xb3, 0x77, 0x50, 0x05, 0xfe, 0xa5, },
		XMP_FLAGS_VBLANK, XMP_MODE_AUTO
	},
	/* hcomme by Bouffon */
	/* length 71346 crc32c 0x4ad49cb3U */
	{
		{ 0x6e, 0xf9, 0x78, 0xc1, 0x80, 0xae, 0x51, 0x06,
		  0x05, 0x7c, 0x6e, 0xd0, 0x26, 0x7e, 0xfe, 0x3d, },
		XMP_FLAGS_VBLANK, XMP_MODE_AUTO
	},
	/* ((((aquapool)))) by Dolphin */
	/* length 62932 crc32c 0x05b103fcU */
	{
		{ 0xff, 0x0b, 0xe0, 0x26, 0xc6, 0x31, 0xb5, 0x9b,
		  0x94, 0x83, 0x94, 0x99, 0x7e, 0x24, 0x7c, 0xdd, },
		XMP_FLAGS_VBLANK, XMP_MODE_AUTO
	},
	/* 100yarddash by Dr. Awesome */
	/* length 104666 crc32c 0xd2b0e4a6U */
	{
		{ 0x5b, 0xff, 0x2f, 0xb8, 0xef, 0x3c, 0xbe, 0x55,
		  0xa8, 0xe2, 0xa7, 0xcf, 0x5c, 0xbd, 0xdd, 0xb2, },
		XMP_FLAGS_VBLANK, XMP_MODE_AUTO
	},
	/* jazz-reggae-funk by Droid */
	/* length 115564 crc32c 0x41ff635fU */
	{
		{ 0xe5, 0x6e, 0x31, 0x2f, 0x62, 0x80, 0xc1, 0x9d,
		  0x2f, 0x24, 0x54, 0xf3, 0x89, 0x3f, 0x94, 0x6c, },
		XMP_FLAGS_VBLANK, XMP_MODE_AUTO
	},
	/* hard and heavy by Fish */
	/* length 69814 crc32c 0x1f09d3d5U */
	{
		{ 0x6b, 0xce, 0x39, 0x94, 0x75, 0x42, 0x06, 0x74,
		  0xd2, 0x83, 0xbc, 0x5e, 0x7b, 0x42, 0x1f, 0xa0, },
		XMP_FLAGS_VBLANK, XMP_MODE_AUTO
	},
	/* crazy valley by Julius and Droid */
	/* length 97496 crc32c 0xb8eec40eU */
	{
		{ 0x23, 0x77, 0x18, 0x1d, 0x21, 0x9b, 0x41, 0x8f,
		  0xc1, 0xb4, 0xf4, 0xf8, 0x22, 0xdd, 0xd8, 0xb6, },
		XMP_FLAGS_VBLANK, XMP_MODE_AUTO
	},
	/* THE ILLOGICAL ONE by Rhino */
	/* length 173432 crc32c 0xcb4e2987U */
	{
		{ 0xd8, 0xc2, 0xbb, 0xe6, 0x11, 0xd0, 0x5c, 0x02,
		  0x8e, 0x3b, 0xcb, 0x7c, 0x4a, 0x7d, 0x43, 0xa0, },
		XMP_FLAGS_VBLANK, XMP_MODE_AUTO
	},
	/* sounds of holiday by Spacebrain */
	/* length 309520 crc32c 0x28804a57U */
	{
		{ 0x36, 0x18, 0x19, 0xa4, 0x9d, 0xa2, 0xa2, 0x6f,
		  0x58, 0x60, 0xc4, 0xd9, 0x0d, 0xa2, 0x9f, 0x49, },
		XMP_FLAGS_VBLANK, XMP_MODE_AUTO
	},
	/* sunisinus by Speed-Head */
	/* length 175706 crc32c 0x2e56451bU */
	{
		{ 0x7e, 0x69, 0x44, 0xb6, 0x38, 0x0d, 0x27, 0x14,
		  0x70, 0x5d, 0x44, 0xce, 0xce, 0xdd, 0x37, 0x31, },
		XMP_FLAGS_VBLANK, XMP_MODE_AUTO
	},
	/* eat the fulcrum bop by The Assassin */
	/* length 160286 crc32c 0x583a4683U */
	{
		{ 0x11, 0xe9, 0x6f, 0x62, 0xe1, 0xc3, 0xc5, 0xcc,
		  0x3b, 0xaf, 0xea, 0x69, 0x4b, 0xce, 0x5f, 0xec, },
		XMP_FLAGS_VBLANK, XMP_MODE_AUTO
	},
	/* obvious disaster by Tip */
	/* length 221086 crc32c 0x51c6d489U */
	{
		{ 0x06, 0x8e, 0x69, 0x01, 0x49, 0x8f, 0xbd, 0x0f,
		  0xfc, 0xb7, 0x8f, 0x2a, 0x91, 0xe1, 0x8b, 0xe8, },
		XMP_FLAGS_VBLANK, XMP_MODE_AUTO
	},
	/* alien nation by Turtle */
	/* length 167548 crc32c 0xc9ec1674U */
	{
		{ 0x71, 0xdf, 0x11, 0xac, 0x5d, 0xec, 0x07, 0xf8,
		  0x10, 0x6f, 0x28, 0x8d, 0x47, 0x59, 0x54, 0x9b, },
		XMP_FLAGS_VBLANK, XMP_MODE_AUTO
	},
	/* illusions!2 by Zuhl */
	/* length 289770 crc32c 0x6bf5fbcfU */
	{
		{ 0xca, 0x37, 0x8c, 0x0e, 0x87, 0x4f, 0x1e, 0xcd,
		  0xa3, 0xe9, 0x8b, 0xdd, 0x11, 0x46, 0x8d, 0x69, },
		XMP_FLAGS_VBLANK, XMP_MODE_AUTO
	},

	{
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
		0, 0
	}
};

static void module_quirks(struct context_data *ctx)
{
	struct player_data *p = &ctx->p;
	struct module_data *m = &ctx->m;
	int i;

	for (i = 0; mq[i].flags != 0 || mq[i].mode != 0; i++) {
		if (!memcmp(m->md5, mq[i].md5, 16)) {
			p->flags |= mq[i].flags;
			p->mode = mq[i].mode;
		}
	}
}

#endif /* LIBXMP_CORE_PLAYER */

char *libxmp_adjust_string(char *s)
{
	int i;

	for (i = 0; i < strlen(s); i++) {
		if (!isprint((unsigned char)s[i]) || ((uint8) s[i] > 127))
			s[i] = ' ';
	}

	while (*s && (s[strlen(s) - 1] == ' ')) {
		s[strlen(s) - 1] = 0;
	}

	return s;
}

static void check_envelope(struct xmp_envelope *env)
{
	/* Disable envelope if invalid number of points */
	if (env->npt <= 0 || env->npt > XMP_MAX_ENV_POINTS) {
		env->flg &= ~XMP_ENVELOPE_ON;
	}

	/* Disable envelope loop if invalid loop parameters */
	if (env->lps >= env->npt || env->lpe >= env->npt) {
		env->flg &= ~XMP_ENVELOPE_LOOP;
	}

	/* Disable envelope sustain if invalid sustain */
	if (env->sus >= env->npt || env->sue >= env->npt) {
		env->flg &= ~XMP_ENVELOPE_SUS;
	}
}

static void clamp_volume_envelope(struct module_data *m, struct xmp_envelope *env)
{
	/* Clamp broken values in the volume envelope to the expected range. */
	if (env->flg & XMP_ENVELOPE_ON) {
		int i;
		for (i = 0; i < env->npt; i++) {
			int16 *data = &env->data[i * 2 + 1];
			CLAMP(*data, 0, m->volbase);
		}
	}
}

void libxmp_load_prologue(struct context_data *ctx)
{
	struct module_data *m = &ctx->m;
	int i;

	/* Reset variables */
	memset(&m->mod, 0, sizeof (struct xmp_module));
	m->rrate = PAL_RATE;
	m->c4rate = C4_PAL_RATE;
	m->volbase = 0x40;
	m->gvol = m->gvolbase = 0x40;
	m->vol_table = NULL;
	m->quirk = 0;
	m->read_event_type = READ_EVENT_MOD;
	m->period_type = PERIOD_AMIGA;
	m->compare_vblank = 0;
	m->comment = NULL;
	m->scan_cnt = NULL;
	m->midi = NULL;

	/* Set defaults */
	m->mod.pat = 0;
	m->mod.trk = 0;
	m->mod.chn = 4;
	m->mod.ins = 0;
	m->mod.smp = 0;
	m->mod.spd = 6;
	m->mod.bpm = 125;
	m->mod.len = 0;
	m->mod.rst = 0;

#ifndef LIBXMP_CORE_PLAYER
	m->extra = NULL;
#endif

	m->time_factor = DEFAULT_TIME_FACTOR;

	for (i = 0; i < 64; i++) {
		int pan = (((i + 1) / 2) % 2) * 0xff;
		m->mod.xxc[i].pan = 0x80 + (pan - 0x80) * m->defpan / 100;
		m->mod.xxc[i].vol = 0x40;
		m->mod.xxc[i].flg = 0;
	}
}

void libxmp_load_epilogue(struct context_data *ctx)
{
	struct player_data *p = &ctx->p;
	struct module_data *m = &ctx->m;
	struct xmp_module *mod = &m->mod;
	int i, j;

	mod->gvl = m->gvol;

	/* Sanity check for module parameters */
	CLAMP(mod->len, 0, XMP_MAX_MOD_LENGTH);
	CLAMP(mod->pat, 0, 257);   /* some formats have an extra pattern */
	CLAMP(mod->ins, 0, 255);
	CLAMP(mod->smp, 0, MAX_SAMPLES);
	CLAMP(mod->chn, 0, XMP_MAX_CHANNELS);

	/* Fix cases where the restart value is invalid e.g. kc_fall8.xm
	 * from http://aminet.net/mods/mvp/mvp_0002.lha (reported by
	 * Ralf Hoffmann <ralf@boomerangsworld.de>)
	 */
	if (mod->rst >= mod->len) {
		mod->rst = 0;
	}

	/* Sanity check for tempo and BPM */
	if (mod->spd <= 0 || mod->spd > 255) {
		mod->spd = 6;
	}
	CLAMP(mod->bpm, XMP_MIN_BPM, 1000);

	/* Set appropriate values for instrument volumes and subinstrument
	 * global volumes when QUIRK_INSVOL is not set, to keep volume values
	 * consistent if the user inspects struct xmp_module. We can later
	 * set volumes in the loaders and eliminate the quirk.
	 */
	for (i = 0; i < mod->ins; i++) {
		if (~m->quirk & QUIRK_INSVOL) {
			mod->xxi[i].vol = m->volbase;
		}
		for (j = 0; j < mod->xxi[i].nsm; j++) {
			if (~m->quirk & QUIRK_INSVOL) {
				mod->xxi[i].sub[j].gvl = m->volbase;
			}
		}
	}

	/* Sanity check for envelopes
	 */
	for (i = 0; i < mod->ins; i++) {
		check_envelope(&mod->xxi[i].aei);
		check_envelope(&mod->xxi[i].fei);
		check_envelope(&mod->xxi[i].pei);
		clamp_volume_envelope(m, &mod->xxi[i].aei);
	}

#ifndef LIBXMP_CORE_DISABLE_IT
	/* TODO: there's no unintrusive and clean way to get this struct into
	 * libxmp_load_sample currently, so bound these fields here for now. */
	for (i = 0; i < mod->smp; i++) {
		struct xmp_sample *xxs = &mod->xxs[i];
		struct extra_sample_data *xtra = &m->xtra[i];
		if (xtra->sus < 0) {
			xtra->sus = 0;
		}
		if (xtra->sue > xxs->len) {
			xtra->sue = xxs->len;
		}
		if (xtra->sus >= xxs->len || xtra->sus >= xtra->sue) {
			xtra->sus = xtra->sue = 0;
			xxs->flg &= ~(XMP_SAMPLE_SLOOP | XMP_SAMPLE_SLOOP_BIDIR);
		}
	}
#endif

	p->filter = 0;
	p->mode = XMP_MODE_AUTO;
	p->flags = p->player_flags;
#ifndef LIBXMP_CORE_PLAYER
	module_quirks(ctx);
#endif
	libxmp_set_player_mode(ctx);
}

int libxmp_prepare_scan(struct context_data *ctx)
{
	struct module_data *m = &ctx->m;
	struct xmp_module *mod = &m->mod;
	int i, ord;

	if (mod->xxp == NULL || mod->xxt == NULL)
		return -XMP_ERROR_LOAD;
	ord = 0;
	while (ord < mod->len && mod->xxo[ord] >= mod->pat) {
		ord++;
	}

	if (ord >= mod->len) {
		mod->len = 0;
		return 0;
	}

	m->scan_cnt = (uint8 **) calloc(mod->len, sizeof(uint8 *));
	if (m->scan_cnt == NULL)
		return -XMP_ERROR_SYSTEM;

	for (i = 0; i < mod->len; i++) {
		int pat_idx = mod->xxo[i];
		struct xmp_pattern *pat;

		/* Add pattern if referenced in orders */
		if (pat_idx < mod->pat && !mod->xxp[pat_idx]) {
			if (libxmp_alloc_pattern(mod, pat_idx) < 0) {
				return -XMP_ERROR_SYSTEM;
			}
		}

		pat = pat_idx >= mod->pat ? NULL : mod->xxp[pat_idx];
		m->scan_cnt[i] = (uint8 *) calloc(1, (pat && pat->rows)? pat->rows : 1);
		if (m->scan_cnt[i] == NULL)
			return -XMP_ERROR_SYSTEM;
	}

	return 0;
}

void libxmp_free_scan(struct context_data *ctx)
{
	struct player_data *p = &ctx->p;
	struct module_data *m = &ctx->m;
	struct xmp_module *mod = &m->mod;
	int i;

	if (m->scan_cnt) {
		for (i = 0; i < mod->len; i++)
			free(m->scan_cnt[i]);

		free(m->scan_cnt);
		m->scan_cnt = NULL;
	}

	free(p->scan);
	p->scan = NULL;
}

/* Process player personality flags */
int libxmp_set_player_mode(struct context_data *ctx)
{
	struct player_data *p = &ctx->p;
	struct module_data *m = &ctx->m;
	int q;

	switch (p->mode) {
	case XMP_MODE_AUTO:
		break;
	case XMP_MODE_MOD:
		m->c4rate = C4_PAL_RATE;
		m->quirk = 0;
		m->read_event_type = READ_EVENT_MOD;
		m->period_type = PERIOD_AMIGA;
		break;
	case XMP_MODE_NOISETRACKER:
		m->c4rate = C4_PAL_RATE;
		m->quirk = QUIRK_NOBPM;
		m->read_event_type = READ_EVENT_MOD;
		m->period_type = PERIOD_MODRNG;
		break;
	case XMP_MODE_PROTRACKER:
		m->c4rate = C4_PAL_RATE;
		m->quirk = QUIRK_PROTRACK;
		m->read_event_type = READ_EVENT_MOD;
		m->period_type = PERIOD_MODRNG;
		break;
	case XMP_MODE_S3M:
		q = m->quirk & (QUIRK_VSALL | QUIRK_ARPMEM);
		m->c4rate = C4_NTSC_RATE;
		m->quirk = QUIRKS_ST3 | q;
		m->read_event_type = READ_EVENT_ST3;
		break;
	case XMP_MODE_ST3:
		q = m->quirk & (QUIRK_VSALL | QUIRK_ARPMEM);
		m->c4rate = C4_NTSC_RATE;
		m->quirk = QUIRKS_ST3 | QUIRK_ST3BUGS | q;
		m->read_event_type = READ_EVENT_ST3;
		break;
	case XMP_MODE_ST3GUS:
		q = m->quirk & (QUIRK_VSALL | QUIRK_ARPMEM);
		m->c4rate = C4_NTSC_RATE;
		m->quirk = QUIRKS_ST3 | QUIRK_ST3BUGS | q;
		m->quirk &= ~QUIRK_RSTCHN;
		m->read_event_type = READ_EVENT_ST3;
		break;
	case XMP_MODE_XM:
		m->c4rate = C4_NTSC_RATE;
		m->quirk = QUIRKS_FT2;
		m->read_event_type = READ_EVENT_FT2;
		break;
	case XMP_MODE_FT2:
		m->c4rate = C4_NTSC_RATE;
		m->quirk = QUIRKS_FT2 | QUIRK_FT2BUGS;
		m->read_event_type = READ_EVENT_FT2;
		break;
	case XMP_MODE_IT:
		m->c4rate = C4_NTSC_RATE;
		m->quirk = QUIRKS_IT | QUIRK_VIBHALF | QUIRK_VIBINV;
		m->read_event_type = READ_EVENT_IT;
		break;
	case XMP_MODE_ITSMP:
		m->c4rate = C4_NTSC_RATE;
		m->quirk = QUIRKS_IT | QUIRK_VIBHALF | QUIRK_VIBINV;
		m->quirk &= ~(QUIRK_VIRTUAL | QUIRK_RSTCHN);
		m->read_event_type = READ_EVENT_IT;
		break;
	default:
		return -1;
	}

	if (p->mode != XMP_MODE_AUTO)
		m->compare_vblank = 0;

	return 0;
}

