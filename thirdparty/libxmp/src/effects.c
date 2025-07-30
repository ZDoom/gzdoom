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

#include "common.h"
#include "player.h"
#include "effects.h"
#include "period.h"
#include "virtual.h"
#include "mixer.h"
#ifndef LIBXMP_CORE_PLAYER
#include "extras.h"
#endif

#define NOT_IMPLEMENTED
#define HAS_QUIRK(x) (m->quirk & (x))

#define SET_LFO_NOTZERO(lfo, depth, rate) do { \
	if ((depth) != 0) libxmp_lfo_set_depth(lfo, depth); \
	if ((rate) != 0)  libxmp_lfo_set_rate(lfo, rate); \
} while (0)

#define EFFECT_MEMORY__(p, m) do { \
	if ((p) == 0) { (p) = (m); } else { (m) = (p); } \
} while (0)

/* ST3 effect memory is not a bug, but it's a weird implementation and it's
 * unlikely to be supported in anything other than ST3 (or OpenMPT).
 */
#define EFFECT_MEMORY(p, m) do { \
	if (HAS_QUIRK(QUIRK_ST3BUGS)) { \
		EFFECT_MEMORY__((p), xc->vol.memory); \
	} else { \
		EFFECT_MEMORY__((p), (m)); \
	} \
} while (0)

#define EFFECT_MEMORY_SETONLY(p, m) do { \
	EFFECT_MEMORY__((p), (m)); \
	if (HAS_QUIRK(QUIRK_ST3BUGS)) { \
		if ((p) != 0) { xc->vol.memory = (p); } \
	} \
} while (0)

#define EFFECT_MEMORY_S3M(p) do { \
	if (HAS_QUIRK(QUIRK_ST3BUGS)) { \
		EFFECT_MEMORY__((p), xc->vol.memory); \
	} \
} while (0)


static void do_toneporta(struct context_data *ctx,
                                struct channel_data *xc, int note)
{
	struct module_data *m = &ctx->m;
	struct xmp_instrument *instrument = &m->mod.xxi[xc->ins];
	struct xmp_subinstrument *sub;
	int mapped_xpo = 0;
	int mapped = 0;

	if (IS_VALID_NOTE(xc->key)) {
		mapped = instrument->map[xc->key].ins;
	}

	if (mapped >= instrument->nsm) {
		mapped = 0;
	}

	sub = &instrument->sub[mapped];

	if (IS_VALID_NOTE(note - 1) && (uint32)xc->ins < m->mod.ins) {
		note--;
		if (IS_VALID_NOTE(xc->key_porta)) {
			mapped_xpo = instrument->map[xc->key_porta].xpo;
		}
		xc->porta.target = libxmp_note_to_period(ctx, note + sub->xpo +
			mapped_xpo, xc->finetune, xc->per_adj);
	}
	xc->porta.dir = xc->period < xc->porta.target ? 1 : -1;
}

void libxmp_process_fx(struct context_data *ctx, struct channel_data *xc, int chn,
		struct xmp_event *e, int fnum)
{
	struct player_data *p = &ctx->p;
	struct module_data *m = &ctx->m;
	struct xmp_module *mod = &m->mod;
	struct flow_control *f = &p->flow;
	uint8 note, fxp, fxt;
	int h, l;

	/* key_porta is IT only */
	if (m->read_event_type != READ_EVENT_IT) {
		xc->key_porta = xc->key;
	}

	note = e->note;
	if (fnum == 0) {
		fxt = e->fxt;
		fxp = e->fxp;
	} else {
		fxt = e->f2t;
		fxp = e->f2p;
	}

	switch (fxt) {
	case FX_ARPEGGIO:
	      fx_arpeggio:
		if (!HAS_QUIRK(QUIRK_ARPMEM) || fxp != 0) {
			xc->arpeggio.val[0] = 0;
			xc->arpeggio.val[1] = MSN(fxp);
			xc->arpeggio.val[2] = LSN(fxp);
			xc->arpeggio.size = 3;
		}
		break;
	case FX_S3M_ARPEGGIO:
		EFFECT_MEMORY(fxp, xc->arpeggio.memory);
		goto fx_arpeggio;

#ifndef LIBXMP_CORE_PLAYER
	case FX_OKT_ARP3:
		if (fxp != 0) {
			xc->arpeggio.val[0] = -MSN(fxp);
			xc->arpeggio.val[1] = 0;
			xc->arpeggio.val[2] = LSN(fxp);
			xc->arpeggio.size = 3;
		}
		break;
	case FX_OKT_ARP4:
		if (fxp != 0) {
			xc->arpeggio.val[0] = 0;
			xc->arpeggio.val[1] = LSN(fxp);
			xc->arpeggio.val[2] = 0;
			xc->arpeggio.val[3] = -MSN(fxp);
			xc->arpeggio.size = 4;
		}
		break;
	case FX_OKT_ARP5:
		if (fxp != 0) {
			xc->arpeggio.val[0] = LSN(fxp);
			xc->arpeggio.val[1] = LSN(fxp);
			xc->arpeggio.val[2] = 0;
			xc->arpeggio.size = 3;
		}
		break;
#endif
	case FX_PORTA_UP:	/* Portamento up */
		EFFECT_MEMORY(fxp, xc->freq.memory);

		if (HAS_QUIRK(QUIRK_FINEFX)
		    && (fnum == 0 || !HAS_QUIRK(QUIRK_ITVPOR))) {
			switch (MSN(fxp)) {
			case 0xf:
				fxp &= 0x0f;
				goto fx_f_porta_up;
			case 0xe:
				fxp &= 0x0f;
				fxp |= 0x10;
				goto fx_xf_porta;
			}
		}

		SET(PITCHBEND);

		if (fxp != 0) {
			xc->freq.slide = -fxp;
			if (HAS_QUIRK(QUIRK_UNISLD))
				xc->porta.memory = fxp;
		} else if (xc->freq.slide > 0) {
			xc->freq.slide *= -1;
		}
		break;
	case FX_PORTA_DN:	/* Portamento down */
		EFFECT_MEMORY(fxp, xc->freq.memory);

		if (HAS_QUIRK(QUIRK_FINEFX)
		    && (fnum == 0 || !HAS_QUIRK(QUIRK_ITVPOR))) {
			switch (MSN(fxp)) {
			case 0xf:
				fxp &= 0x0f;
				goto fx_f_porta_dn;
			case 0xe:
				fxp &= 0x0f;
				fxp |= 0x20;
				goto fx_xf_porta;
			}
		}

		SET(PITCHBEND);

		if (fxp != 0) {
			xc->freq.slide = fxp;
			if (HAS_QUIRK(QUIRK_UNISLD))
				xc->porta.memory = fxp;
		} else if (xc->freq.slide < 0) {
			xc->freq.slide *= -1;
		}
		break;
	case FX_TONEPORTA:	/* Tone portamento */
		EFFECT_MEMORY_SETONLY(fxp, xc->porta.memory);

		if (fxp != 0) {
			if (HAS_QUIRK(QUIRK_UNISLD)) /* IT compatible Gxx off */
				xc->freq.memory = fxp;
			xc->porta.slide = fxp;
		}

		if (HAS_QUIRK(QUIRK_IGSTPOR)) {
			if (note == 0 && xc->porta.dir == 0)
				break;
		}

		if (!IS_VALID_INSTRUMENT(xc->ins))
			break;

		do_toneporta(ctx, xc, note);

		SET(TONEPORTA);
		break;

	case FX_VIBRATO:	/* Vibrato */
		EFFECT_MEMORY_SETONLY(fxp, xc->vibrato.memory);

		SET(VIBRATO);
		SET_LFO_NOTZERO(&xc->vibrato.lfo, LSN(fxp) << 2, MSN(fxp));
		break;
	case FX_FINE_VIBRATO:	/* Fine vibrato */
		EFFECT_MEMORY_SETONLY(fxp, xc->vibrato.memory);

		SET(VIBRATO);
		SET_LFO_NOTZERO(&xc->vibrato.lfo, LSN(fxp), MSN(fxp));
		break;

	case FX_TONE_VSLIDE:	/* Toneporta + vol slide */
		if (!IS_VALID_INSTRUMENT(xc->ins))
			break;
		do_toneporta(ctx, xc, note);
		SET(TONEPORTA);
		goto fx_volslide;
	case FX_VIBRA_VSLIDE:	/* Vibrato + vol slide */
		SET(VIBRATO);
		goto fx_volslide;

	case FX_TREMOLO:	/* Tremolo */
		EFFECT_MEMORY(fxp, xc->tremolo.memory);
		SET(TREMOLO);
		SET_LFO_NOTZERO(&xc->tremolo.lfo, LSN(fxp), MSN(fxp));
		break;

	case FX_SETPAN:		/* Set pan */
		if (HAS_QUIRK(QUIRK_PROTRACK)) {
			break;
		}
	    fx_setpan:
		/* From OpenMPT PanOff.xm:
		 * "Another chapter of weird FT2 bugs: Note-Off + Note Delay
		 *  + Volume Column Panning = Panning effect is ignored."
		 */
		if (!HAS_QUIRK(QUIRK_FT2BUGS)		/* If not FT2 */
		    || fnum == 0			/* or not vol column */
		    || e->note != XMP_KEY_OFF		/* or not keyoff */
		    || e->fxt != FX_EXTENDED		/* or not delay */
		    || MSN(e->fxp) != EX_DELAY) {
			xc->pan.val = fxp;
			xc->pan.surround = 0;
		}
		xc->rpv = 0;	/* storlek_20: set pan overrides random pan */
		xc->pan.surround = 0;
		break;
	case FX_OFFSET:		/* Set sample offset */
		EFFECT_MEMORY(fxp, xc->offset.memory);
		SET(OFFSET);
		if (note) {
			xc->offset.val &= xc->offset.val & ~0xffff;
			xc->offset.val |= fxp << 8;
			xc->offset.val2 = fxp << 8;
		}
		if (e->ins) {
			xc->offset.val2 = fxp << 8;
		}
		break;
	case FX_VOLSLIDE:	/* Volume slide */
	      fx_volslide:
		/* S3M file volume slide note:
		 * DFy  Fine volume down by y (...) If y is 0, the command will
		 *      be treated as a volume slide up with a value of f (15).
		 *      If a DFF command is specified, the volume will be slid
		 *      up.
		 */
		if (HAS_QUIRK(QUIRK_FINEFX)) {
			h = MSN(fxp);
			l = LSN(fxp);
			if (l == 0xf && h != 0) {
				xc->vol.memory = fxp;
				fxp >>= 4;
				goto fx_f_vslide_up;
			} else if (h == 0xf && l != 0) {
				xc->vol.memory = fxp;
				fxp &= 0x0f;
				goto fx_f_vslide_dn;
			}
		}

		/* recover memory */
		if (fxp == 0x00) {
			if ((fxp = xc->vol.memory) != 0)
				goto fx_volslide;
		}

		SET(VOL_SLIDE);

		/* Skaven's 2nd reality (S3M) has volslide parameter D7 => pri
		 * down. Other trackers only compute volumes if the other
		 * parameter is 0, Fall from sky.xm has 2C => do nothing.
		 * Also don't assign xc->vol.memory if fxp is 0, see Guild
		 * of Sounds.xm
		 */
		if (fxp) {
			xc->vol.memory = fxp;
			h = MSN(fxp);
			l = LSN(fxp);
			if (fxp) {
				if (HAS_QUIRK(QUIRK_VOLPDN)) {
					xc->vol.slide = l ? -l : h;
				} else {
					xc->vol.slide = h ? h : -l;
				}
			}
		}

		/* Mirko reports that a S3M with D0F effects created with ST321
		 * should process volume slides in all frames like ST300. I
		 * suspect ST3/IT could be handling D0F effects like this.
		 */
		if (HAS_QUIRK(QUIRK_FINEFX)) {
			if (MSN(xc->vol.memory) == 0xf
			    || LSN(xc->vol.memory) == 0xf) {
				SET(FINE_VOLS);
				xc->vol.fslide = xc->vol.slide;
			}
		}
		break;
	case FX_VOLSLIDE_2:	/* Secondary volume slide */
		SET(VOL_SLIDE_2);
		if (fxp) {
			h = MSN(fxp);
			l = LSN(fxp);
			xc->vol.slide2 = h ? h : -l;
		}
		break;
	case FX_JUMP:		/* Order jump */
		p->flow.pbreak = 1;
		p->flow.jump = fxp;
		/* effect B resets effect D in lower channels */
		p->flow.jumpline = 0;
		break;
	case FX_VOLSET:		/* Volume set */
		SET(NEW_VOL);
		xc->volume = fxp;
		if (xc->split) {
			p->xc_data[xc->pair].volume = xc->volume;
		}
		break;
	case FX_BREAK:		/* Pattern break */
		p->flow.pbreak = 1;
		p->flow.jumpline = 10 * MSN(fxp) + LSN(fxp);
		break;
	case FX_EXTENDED:	/* Extended effect */
		EFFECT_MEMORY_S3M(fxp);
		fxt = fxp >> 4;
		fxp &= 0x0f;
		switch (fxt) {
		case EX_FILTER:		/* Amiga led filter */
			if (IS_AMIGA_MOD()) {
				p->filter = !(fxp & 1);
			}
			break;
		case EX_F_PORTA_UP:	/* Fine portamento up */
			EFFECT_MEMORY(fxp, xc->fine_porta.up_memory);
			goto fx_f_porta_up;
		case EX_F_PORTA_DN:	/* Fine portamento down */
			EFFECT_MEMORY(fxp, xc->fine_porta.down_memory);
			goto fx_f_porta_dn;
		case EX_GLISS:		/* Glissando toggle */
			if (fxp) {
				SET_NOTE(NOTE_GLISSANDO);
			} else {
				RESET_NOTE(NOTE_GLISSANDO);
			}
			break;
		case EX_VIBRATO_WF:	/* Set vibrato waveform */
			fxp &= 3;
			libxmp_lfo_set_waveform(&xc->vibrato.lfo, fxp);
			break;
		case EX_FINETUNE:	/* Set finetune */
			if (!HAS_QUIRK(QUIRK_FT2BUGS) || note > 0) {
				xc->finetune = (int8)(fxp << 4);
			}
			break;
		case EX_PATTERN_LOOP:	/* Loop pattern */
			if (fxp == 0) {
				/* mark start of loop */
				f->loop[chn].start = p->row;
				if (HAS_QUIRK(QUIRK_FT2BUGS))
				  p->flow.jumpline = p->row;
			} else {
				/* end of loop */
				if (f->loop[chn].count) {
					if (--f->loop[chn].count) {
						/* **** H:FIXME **** */
						f->loop_chn = ++chn;
					} else {
						if (HAS_QUIRK(QUIRK_S3MLOOP))
							f->loop[chn].start =
							    p->row + 1;
					}
				} else {
					f->loop[chn].count = fxp;
					f->loop_chn = ++chn;
				}
			}
			break;
		case EX_TREMOLO_WF:	/* Set tremolo waveform */
			libxmp_lfo_set_waveform(&xc->tremolo.lfo, fxp & 3);
			break;
		case EX_SETPAN:
			fxp <<= 4;
			goto fx_setpan;
		case EX_RETRIG:		/* Retrig note */
			SET(RETRIG);
			xc->retrig.val = fxp;
			xc->retrig.count = LSN(xc->retrig.val) + 1;
			xc->retrig.type = 0;
			xc->retrig.limit = 0;
			break;
		case EX_F_VSLIDE_UP:	/* Fine volume slide up */
			EFFECT_MEMORY(fxp, xc->fine_vol.up_memory);
			goto fx_f_vslide_up;
		case EX_F_VSLIDE_DN:	/* Fine volume slide down */
			EFFECT_MEMORY(fxp, xc->fine_vol.down_memory);
			goto fx_f_vslide_dn;
		case EX_CUT:		/* Cut note */
			SET(RETRIG);
			SET_NOTE(NOTE_CUT);	/* for IT cut-carry */
			xc->retrig.val = fxp + 1;
			xc->retrig.count = xc->retrig.val;
			xc->retrig.type = 0x10;
			break;
		case EX_DELAY:		/* Note delay */
			/* computed at frame loop */
			break;
		case EX_PATT_DELAY:	/* Pattern delay */
			goto fx_patt_delay;
		case EX_INVLOOP:	/* Invert loop / funk repeat */
			xc->invloop.speed = fxp;
			break;
		}
		break;
	case FX_SPEED:		/* Set speed */
		if (HAS_QUIRK(QUIRK_NOBPM) || p->flags & XMP_FLAGS_VBLANK) {
			goto fx_s3m_speed;
		}

		/* speedup.xm needs BPM = 20 */
		if (fxp < 0x20) {
			goto fx_s3m_speed;
		}
		goto fx_s3m_bpm;

	case FX_FINETUNE:
		xc->finetune = (int16) (fxp - 0x80);
		break;

	case FX_F_VSLIDE_UP:	/* Fine volume slide up */
		EFFECT_MEMORY(fxp, xc->fine_vol.up_memory);
	    fx_f_vslide_up:
		SET(FINE_VOLS);
		xc->vol.fslide = fxp;
		break;
	case FX_F_VSLIDE_DN:	/* Fine volume slide down */
		EFFECT_MEMORY(fxp, xc->fine_vol.up_memory);
	    fx_f_vslide_dn:
		SET(FINE_VOLS);
		xc->vol.fslide = -fxp;
		break;

	case FX_F_PORTA_UP:	/* Fine portamento up */
	    fx_f_porta_up:
		if (fxp) {
			SET(FINE_BEND);
			xc->freq.fslide = -fxp;
		}
		break;
	case FX_F_PORTA_DN:	/* Fine portamento down */
	    fx_f_porta_dn:
		if (fxp) {
			SET(FINE_BEND);
			xc->freq.fslide = fxp;
		}
		break;
	case FX_PATT_DELAY:
	    fx_patt_delay:
		if (m->read_event_type != READ_EVENT_ST3 || !p->flow.delay) {
			p->flow.delay = fxp;
		}
		break;

	case FX_S3M_SPEED:	/* Set S3M speed */
		EFFECT_MEMORY_S3M(fxp);
	    fx_s3m_speed:
		if (fxp) {
			p->speed = fxp;
#ifndef LIBXMP_CORE_PLAYER
			p->st26_speed = 0;
#endif
		}
		break;
	case FX_S3M_BPM:	/* Set S3M BPM */
	    fx_s3m_bpm: {
		/* Lower time factor in MED allows lower BPM values */
		int min_bpm = (int)(0.5 + m->time_factor * XMP_MIN_BPM / 10);
		if (fxp < min_bpm)
			fxp = min_bpm;
		p->bpm = fxp;
		p->frame_time = m->time_factor * m->rrate / p->bpm;
		break;
	}

#ifndef LIBXMP_CORE_DISABLE_IT
	case FX_IT_BPM:		/* Set IT BPM */
		if (MSN(fxp) == 0) {
			SET(TEMPO_SLIDE);
			if (LSN(fxp))	/* T0x - Tempo slide down by x */
				xc->tempo.slide = -LSN(fxp);
			/* T00 - Repeat previous slide */
		} else if (MSN(fxp) == 1) {	/* T1x - Tempo slide up by x */
			SET(TEMPO_SLIDE);
			xc->tempo.slide = LSN(fxp);
		} else {
			if (fxp < XMP_MIN_BPM)
				fxp = XMP_MIN_BPM;
			p->bpm = fxp;
		}
		p->frame_time = m->time_factor * m->rrate / p->bpm;
		break;
	case FX_IT_ROWDELAY:
		if (!f->rowdelay_set) {
			f->rowdelay = fxp;
			f->rowdelay_set = ROWDELAY_ON | ROWDELAY_FIRST_FRAME;
		}
		break;

	/* From the OpenMPT VolColMemory.it test case:
	 * "Volume column commands a, b, c and d (volume slide) share one
	 *  effect memory, but it should not be shared with Dxy in the effect
	 *  column.
	 */
	case FX_VSLIDE_UP_2:	/* Fine volume slide up */
		EFFECT_MEMORY(fxp, xc->vol.memory2);
		SET(VOL_SLIDE_2);
		xc->vol.slide2 = fxp;
		break;
	case FX_VSLIDE_DN_2:	/* Fine volume slide down */
		EFFECT_MEMORY(fxp, xc->vol.memory2);
		SET(VOL_SLIDE_2);
		xc->vol.slide2 = -fxp;
		break;
	case FX_F_VSLIDE_UP_2:	/* Fine volume slide up */
		EFFECT_MEMORY(fxp, xc->vol.memory2);
		SET(FINE_VOLS_2);
		xc->vol.fslide2 = fxp;
		break;
	case FX_F_VSLIDE_DN_2:	/* Fine volume slide down */
		EFFECT_MEMORY(fxp, xc->vol.memory2);
		SET(FINE_VOLS_2);
		xc->vol.fslide2 = -fxp;
		break;
	case FX_IT_BREAK:	/* Pattern break with hex parameter */
		if (!f->loop_chn)
		{
			p->flow.pbreak = 1;
			p->flow.jumpline = fxp;
		}
		break;

#endif

	case FX_GLOBALVOL:	/* Set global volume */
		if (fxp > m->gvolbase) {
			p->gvol = m->gvolbase;
		} else {
			p->gvol = fxp;
		}
		break;
	case FX_GVOL_SLIDE:	/* Global volume slide */
            fx_gvolslide:
		if (fxp) {
			SET(GVOL_SLIDE);
			xc->gvol.memory = fxp;
			h = MSN(fxp);
			l = LSN(fxp);

			if (HAS_QUIRK(QUIRK_FINEFX)) {
				if (l == 0xf && h != 0) {
					xc->gvol.slide = 0;
					xc->gvol.fslide = h;
				} else if (h == 0xf && l != 0) {
					xc->gvol.slide = 0;
					xc->gvol.fslide = -l;
				} else {
					xc->gvol.slide = h ? h : -l;
					xc->gvol.fslide = 0;
				}
			} else {
				xc->gvol.slide = h ? h : -l;
				xc->gvol.fslide = 0;
			}
		} else {
			if ((fxp = xc->gvol.memory) != 0) {
				goto fx_gvolslide;
			}
		}
		break;
	case FX_KEYOFF:		/* Key off */
		xc->keyoff = fxp + 1;
		break;
	case FX_ENVPOS:		/* Set envelope position */
		/* From OpenMPT SetEnvPos.xm:
		 * "When using the Lxx effect, Fasttracker 2 only sets the
		 *  panning envelope position if the volume envelope’s sustain
		 *  flag is set.
		 */
		if (HAS_QUIRK(QUIRK_FT2BUGS)) {
			struct xmp_instrument *instrument;
			instrument = libxmp_get_instrument(ctx, xc->ins);
			if (instrument != NULL) {
				if (instrument->aei.flg & XMP_ENVELOPE_SUS) {
					xc->p_idx = fxp;
				}
			}
		} else {
			xc->p_idx = fxp;
		}
		xc->v_idx = fxp;
		xc->f_idx = fxp;
		break;
	case FX_PANSLIDE:	/* Pan slide (XM) */
		EFFECT_MEMORY(fxp, xc->pan.memory);
		SET(PAN_SLIDE);
		xc->pan.slide = LSN(fxp) - MSN(fxp);
		break;
	case FX_PANSL_NOMEM:	/* Pan slide (XM volume column) */
		SET(PAN_SLIDE);
		xc->pan.slide = LSN(fxp) - MSN(fxp);
		break;

#ifndef LIBXMP_CORE_DISABLE_IT
	case FX_IT_PANSLIDE:	/* Pan slide w/ fine pan (IT) */
		SET(PAN_SLIDE);
		if (fxp) {
			if (MSN(fxp) == 0xf) {
				xc->pan.slide = 0;
				xc->pan.fslide = LSN(fxp);
			} else if (LSN(fxp) == 0xf) {
				xc->pan.slide = 0;
				xc->pan.fslide = -MSN(fxp);
			} else {
				SET(PAN_SLIDE);
				xc->pan.slide = LSN(fxp) - MSN(fxp);
				xc->pan.fslide = 0;
			}
		}
		break;
#endif

	case FX_MULTI_RETRIG:	/* Multi retrig */
		EFFECT_MEMORY_S3M(fxp);
		if (fxp) {
			xc->retrig.val = fxp;
			xc->retrig.type = MSN(xc->retrig.val);
		}
		if (note) {
			xc->retrig.count = LSN(xc->retrig.val) + 1;
		}
		xc->retrig.limit = 0;
		SET(RETRIG);
		break;
	case FX_TREMOR:			/* Tremor */
		EFFECT_MEMORY(fxp, xc->tremor.memory);
		xc->tremor.up = MSN(fxp);
		xc->tremor.down = LSN(fxp);
		if (IS_PLAYER_MODE_FT2()) {
			xc->tremor.count |= 0x80;
		} else {
			if (xc->tremor.up == 0) {
				xc->tremor.up++;
			}
			if (xc->tremor.down == 0) {
				xc->tremor.down++;
			}
		}
		SET(TREMOR);
		break;
	case FX_XF_PORTA:	/* Extra fine portamento */
	      fx_xf_porta:
		SET(FINE_BEND);
		switch (MSN(fxp)) {
		case 1:
			xc->freq.fslide = -0.25 * LSN(fxp);
			break;
		case 2:
			xc->freq.fslide = 0.25 * LSN(fxp);
			break;
		}
		break;
	case FX_SURROUND:
		xc->pan.surround = fxp;
		break;
	case FX_REVERSE:	/* Play forward/backward */
		libxmp_virt_reverse(ctx, chn, fxp);
		break;

#ifndef LIBXMP_CORE_DISABLE_IT
	case FX_TRK_VOL:	/* Track volume setting */
		if (fxp <= m->volbase) {
			xc->mastervol = fxp;
		}
		break;
	case FX_TRK_VSLIDE:	/* Track volume slide */
		if (fxp == 0) {
			if ((fxp = xc->trackvol.memory) == 0)
				break;
		}

		if (HAS_QUIRK(QUIRK_FINEFX)) {
			h = MSN(fxp);
			l = LSN(fxp);
			if (h == 0xf && l != 0) {
				xc->trackvol.memory = fxp;
				fxp &= 0x0f;
				goto fx_trk_fvslide;
			} else if (l == 0xf && h != 0) {
				xc->trackvol.memory = fxp;
				fxp &= 0xf0;
				goto fx_trk_fvslide;
			}
		}

		SET(TRK_VSLIDE);
		if (fxp) {
			h = MSN(fxp);
			l = LSN(fxp);

			xc->trackvol.memory = fxp;
			if (HAS_QUIRK(QUIRK_VOLPDN)) {
				xc->trackvol.slide = l ? -l : h;
			} else {
				xc->trackvol.slide = h ? h : -l;
			}
		}

		break;
	case FX_TRK_FVSLIDE:	/* Track fine volume slide */
	      fx_trk_fvslide:
		SET(TRK_FVSLIDE);
		if (fxp) {
			xc->trackvol.fslide = MSN(fxp) - LSN(fxp);
		}
		break;

	case FX_IT_INSTFUNC:
		switch (fxp) {
		case 0:	/* Past note cut */
			libxmp_virt_pastnote(ctx, chn, VIRT_ACTION_CUT);
			break;
		case 1:	/* Past note off */
			libxmp_virt_pastnote(ctx, chn, VIRT_ACTION_OFF);
			break;
		case 2:	/* Past note fade */
			libxmp_virt_pastnote(ctx, chn, VIRT_ACTION_FADE);
			break;
		case 3:	/* Set NNA to note cut */
			libxmp_virt_setnna(ctx, chn, XMP_INST_NNA_CUT);
			break;
		case 4:	/* Set NNA to continue */
			libxmp_virt_setnna(ctx, chn, XMP_INST_NNA_CONT);
			break;
		case 5:	/* Set NNA to note off */
			libxmp_virt_setnna(ctx, chn, XMP_INST_NNA_OFF);
			break;
		case 6:	/* Set NNA to note fade */
			libxmp_virt_setnna(ctx, chn, XMP_INST_NNA_FADE);
			break;
		case 7:	/* Turn off volume envelope */
			SET_PER(VENV_PAUSE);
			break;
		case 8:	/* Turn on volume envelope */
			RESET_PER(VENV_PAUSE);
			break;
		case 9:	/* Turn off pan envelope */
			SET_PER(PENV_PAUSE);
			break;
		case 0xa:	/* Turn on pan envelope */
			RESET_PER(PENV_PAUSE);
			break;
		case 0xb:	/* Turn off pitch envelope */
			SET_PER(FENV_PAUSE);
			break;
		case 0xc:	/* Turn on pitch envelope */
			RESET_PER(FENV_PAUSE);
			break;
		}
		break;
	case FX_FLT_CUTOFF:
		xc->filter.cutoff = fxp;
		break;
	case FX_FLT_RESN:
		xc->filter.resonance = fxp;
		break;
	case FX_MACRO_SET:
		xc->macro.active = LSN(fxp);
		break;
	case FX_MACRO:
		SET(MIDI_MACRO);
		xc->macro.val = fxp;
		xc->macro.slide = 0;
		break;
	case FX_MACROSMOOTH:
		if (ctx->p.speed && xc->macro.val < 0x80) {
			SET(MIDI_MACRO);
			xc->macro.target = fxp;
			xc->macro.slide = ((float)fxp - xc->macro.val) / ctx->p.speed;
		}
		break;
	case FX_PANBRELLO:	/* Panbrello */
		SET(PANBRELLO);
		SET_LFO_NOTZERO(&xc->panbrello.lfo, LSN(fxp) << 4, MSN(fxp));
		break;
	case FX_PANBRELLO_WF:	/* Panbrello waveform */
		libxmp_lfo_set_waveform(&xc->panbrello.lfo, fxp & 3);
		break;
	case FX_HIOFFSET:	/* High offset */
		xc->offset.val &= 0xffff;
		xc->offset.val |= fxp << 16;
		break;
#endif

#ifndef LIBXMP_CORE_PLAYER

	/* SFX effects */
	case FX_VOL_ADD:
		if (!IS_VALID_INSTRUMENT(xc->ins)) {
			break;
		}
		SET(NEW_VOL);
		xc->volume = m->mod.xxi[xc->ins].sub[0].vol + fxp;
		if (xc->volume > m->volbase) {
			xc->volume = m->volbase;
		}
		break;
	case FX_VOL_SUB:
		if (!IS_VALID_INSTRUMENT(xc->ins)) {
			break;
		}
		SET(NEW_VOL);
		xc->volume = m->mod.xxi[xc->ins].sub[0].vol - fxp;
		if (xc->volume < 0) {
			xc->volume =0;
		}
		break;
	case FX_PITCH_ADD:
		SET_PER(TONEPORTA);
		xc->porta.target = libxmp_note_to_period(ctx, note - 1, xc->finetune, 0)
			+ fxp;
		xc->porta.slide = 2;
		xc->porta.dir = 1;
		break;
	case FX_PITCH_SUB:
		SET_PER(TONEPORTA);
		xc->porta.target = libxmp_note_to_period(ctx, note - 1, xc->finetune, 0)
			- fxp;
		xc->porta.slide = 2;
		xc->porta.dir = -1;
		break;

	/* Saga Musix says:
	 *
	 * "When both nibbles of an Fxx command are set, SoundTracker 2.6
	 * applies the both values alternatingly, first the high nibble,
	 * then the low nibble on the next row, then the high nibble again...
	 * If only the high nibble is set, it should act like if only the low
	 * nibble is set (i.e. F30 is the same as F03).
	 */
	case FX_ICE_SPEED:
		if (fxp) {
			if (LSN(fxp)) {
				p->st26_speed = (MSN(fxp) << 8) | LSN(fxp);
			} else {
				p->st26_speed = MSN(fxp);
			}
		}
		break;

	case FX_VOLSLIDE_UP:	/* Vol slide with uint8 arg */
		if (HAS_QUIRK(QUIRK_FINEFX)) {
			h = MSN(fxp);
			l = LSN(fxp);
			if (h == 0xf && l != 0) {
				fxp &= 0x0f;
				goto fx_f_vslide_up;
			}
		}

		if (fxp)
			xc->vol.slide = fxp;
		SET(VOL_SLIDE);
		break;
	case FX_VOLSLIDE_DN:	/* Vol slide with uint8 arg */
		if (HAS_QUIRK(QUIRK_FINEFX)) {
			h = MSN(fxp);
			l = LSN(fxp);
			if (h == 0xf && l != 0) {
				fxp &= 0x0f;
				goto fx_f_vslide_dn;
			}
		}

		if (fxp)
			xc->vol.slide = -fxp;
		SET(VOL_SLIDE);
		break;
	case FX_F_VSLIDE:	/* Fine volume slide */
		SET(FINE_VOLS);
		if (fxp) {
			h = MSN(fxp);
			l = LSN(fxp);
			xc->vol.fslide = h ? h : -l;
		}
		break;
	case FX_NSLIDE_DN:
	case FX_NSLIDE_UP:
	case FX_NSLIDE_R_DN:
	case FX_NSLIDE_R_UP:
		if (fxp != 0) {
			if (fxt == FX_NSLIDE_R_DN || fxt == FX_NSLIDE_R_UP) {
				xc->retrig.val = MSN(fxp);
				xc->retrig.count = MSN(fxp) + 1;
				xc->retrig.type = 0;
				xc->retrig.limit = 0;
			}

			if (fxt == FX_NSLIDE_UP || fxt == FX_NSLIDE_R_UP)
				xc->noteslide.slide = LSN(fxp);
			else
				xc->noteslide.slide = -LSN(fxp);

			xc->noteslide.count = xc->noteslide.speed = MSN(fxp);
		}
		if (fxt == FX_NSLIDE_R_DN || fxt == FX_NSLIDE_R_UP)
			SET(RETRIG);
		SET(NOTE_SLIDE);
		break;
	case FX_NSLIDE2_DN:
		SET(NOTE_SLIDE);
		xc->noteslide.slide = -fxp;
		xc->noteslide.count = xc->noteslide.speed = 1;
		break;
	case FX_NSLIDE2_UP:
		SET(NOTE_SLIDE);
		xc->noteslide.slide = fxp;
		xc->noteslide.count = xc->noteslide.speed = 1;
		break;
	case FX_F_NSLIDE_DN:
		SET(FINE_NSLIDE);
		xc->noteslide.fslide = -fxp;
		break;
	case FX_F_NSLIDE_UP:
		SET(FINE_NSLIDE);
		xc->noteslide.fslide = fxp;
		break;

	case FX_PER_VIBRATO:	/* Persistent vibrato */
		if (LSN(fxp) != 0) {
			SET_PER(VIBRATO);
		} else {
			RESET_PER(VIBRATO);
		}
		SET_LFO_NOTZERO(&xc->vibrato.lfo, LSN(fxp) << 2, MSN(fxp));
		break;
	case FX_PER_PORTA_UP:	/* Persistent portamento up */
		SET_PER(PITCHBEND);
		xc->freq.slide = -fxp;
		if ((xc->freq.memory = fxp) == 0)
			RESET_PER(PITCHBEND);
		break;
	case FX_PER_PORTA_DN:	/* Persistent portamento down */
		SET_PER(PITCHBEND);
		xc->freq.slide = fxp;
		if ((xc->freq.memory = fxp) == 0)
			RESET_PER(PITCHBEND);
		break;
	case FX_PER_TPORTA:	/* Persistent tone portamento */
		if (!IS_VALID_INSTRUMENT(xc->ins))
			break;
		SET_PER(TONEPORTA);
		do_toneporta(ctx, xc, note);
		xc->porta.slide = fxp;
		if (fxp == 0)
			RESET_PER(TONEPORTA);
		break;
	case FX_PER_VSLD_UP:	/* Persistent volslide up */
		SET_PER(VOL_SLIDE);
		xc->vol.slide = fxp;
		if (fxp == 0)
			RESET_PER(VOL_SLIDE);
		break;
	case FX_PER_VSLD_DN:	/* Persistent volslide down */
		SET_PER(VOL_SLIDE);
		xc->vol.slide = -fxp;
		if (fxp == 0)
			RESET_PER(VOL_SLIDE);
		break;
	case FX_VIBRATO2:	/* Deep vibrato (2x) */
		SET(VIBRATO);
		SET_LFO_NOTZERO(&xc->vibrato.lfo, LSN(fxp) << 3, MSN(fxp));
		break;
	case FX_SPEED_CP:	/* Set speed and ... */
		if (fxp) {
			p->speed = fxp;
			p->st26_speed = 0;
		}
		/* fall through */
	case FX_PER_CANCEL:	/* Cancel persistent effects */
		xc->per_flags = 0;
		break;

	/* 669 effects */

	case FX_669_PORTA_UP:	/* 669 portamento up */
		SET_PER(PITCHBEND);
		xc->freq.slide = 80 * fxp;
		if ((xc->freq.memory = fxp) == 0)
			RESET_PER(PITCHBEND);
		break;
	case FX_669_PORTA_DN:	/* 669 portamento down */
		SET_PER(PITCHBEND);
		xc->freq.slide = -80 * fxp;
		if ((xc->freq.memory = fxp) == 0)
			RESET_PER(PITCHBEND);
		break;
	case FX_669_TPORTA:	/* 669 tone portamento */
		if (!IS_VALID_INSTRUMENT(xc->ins))
			break;
		SET_PER(TONEPORTA);
		do_toneporta(ctx, xc, note);
		xc->porta.slide = 40 * fxp;
		if (fxp == 0)
			RESET_PER(TONEPORTA);
		break;
	case FX_669_FINETUNE:	/* 669 finetune */
		xc->finetune = 80 * (int8)fxp;
		break;
	case FX_669_VIBRATO:	/* 669 vibrato */
		if (LSN(fxp) != 0) {
			libxmp_lfo_set_waveform(&xc->vibrato.lfo, 669);
			SET_PER(VIBRATO);
		} else {
			RESET_PER(VIBRATO);
		}
		SET_LFO_NOTZERO(&xc->vibrato.lfo, 669, 1);
		break;

	/* ULT effects */

	case FX_ULT_TPORTA:	/* ULT tone portamento */
		/* Like normal persistent tone portamento, except:
		 *
		 * 1) Despite the documentation claiming 300 cancels tone
		 * portamento, it actually reuses the last parameter.
		 *
		 * 2) A 3xx without a note will reuse the last target note.
		 */
		if (!IS_VALID_INSTRUMENT(xc->ins))
			break;
		SET_PER(TONEPORTA);
		EFFECT_MEMORY(fxp, xc->porta.memory);
		EFFECT_MEMORY(note, xc->porta.note_memory);
		do_toneporta(ctx, xc, note);
		xc->porta.slide = fxp;
		if (fxp == 0)
			RESET_PER(TONEPORTA);
		break;

	/* Archimedes (!Tracker, Digital Symphony, et al.) effects */

	case FX_LINE_JUMP:	/* !Tracker and Digital Symphony "Line Jump" */
		/* Jump to a line within the current order. In Digital Symphony
		 * this can be combined with position jump (like pattern break)
		 * and overrides the pattern break line in lower channels. */
		if (p->flow.pbreak == 0) {
			p->flow.pbreak = 1;
			p->flow.jump = p->ord;
		}
		p->flow.jumpline = fxp;
		p->flow.jump_in_pat = p->ord;
		break;
#endif

	default:
#ifndef LIBXMP_CORE_PLAYER
		libxmp_extras_process_fx(ctx, xc, chn, note, fxt, fxp, fnum);
#endif
		break;
	}
}
