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

/*
 * Common functions for MMD0/1 and MMD2/3 loaders
 * Tempo fixed by Francis Russell
 */

#include "med.h"
#include "loader.h"
#include "../med_extras.h"

#ifdef DEBUG
const char *const mmd_inst_type[] = {
	"HYB",			/* -2 */
	"SYN",			/* -1 */
	"SMP",			/*  0 */
	"I5O",			/*  1 */
	"I3O",			/*  2 */
	"I2O",			/*  3 */
	"I4O",			/*  4 */
	"I6O",			/*  5 */
	"I7O",			/*  6 */
	"EXT",			/*  7 */
};
#endif

int mmd_convert_tempo(int tempo, int bpm_on, int med_8ch)
{
	const int tempos_compat[10] = {
		195, 97, 65, 49, 39, 32, 28, 24, 22, 20
	};
	const int tempos_8ch[10] = {
		179, 164, 152, 141, 131, 123, 116, 110, 104, 99
	};

	if (tempo > 0) {
		/* From the OctaMEDv4 documentation:
		 *
		 * In 8-channel mode, you can control the playing speed more
		 * accurately (to techies: by changing the size of the mix buffer).
		 * This can be done with the left tempo gadget (values 1-10; the
		 * lower, the faster). Values 11-240 are equivalent to 10.
		 *
		 * NOTE: the tempos used for this are directly from OctaMED
		 * Soundstudio 2, but in older versions the playback speeds
		 * differed slightly between NTSC and PAL. This table seems to
		 * have been intended to be a compromise between the two.
		 */
		if (med_8ch) {
			tempo = tempo > 10 ? 10 : tempo;
			return tempos_8ch[tempo-1];
		}
		/* Tempos 1-10 in tempo mode are compatibility tempos that
		 * approximate Soundtracker speeds.
		 */
		if (tempo <= 10 && !bpm_on) {
			return tempos_compat[tempo-1];
		}
	}
	return tempo;
}

void mmd_xlat_fx(struct xmp_event *event, int bpm_on, int bpmlen, int med_8ch,
		 int hexvol)
{
	switch (event->fxt) {
	case 0x00:
		/* ARPEGGIO 00
		 * Changes the pitch six times between three different
		 * pitches during the duration of the note. It can create a
		 * chord sound or other special effect. Arpeggio works better
		 * with some instruments than others.
		 */
		break;
	case 0x01:
		/* SLIDE UP 01
		 * This slides the pitch of the current track up. It decreases
		 * the period of the note by the amount of the argument on each
		 * timing pulse. OctaMED-Pro can create slides automatically,
		 * but you may want to use this function for special effects.
		 * Note: a param of 0 does nothing and should be ignored.
		 */
		if (!event->fxp)
			event->fxt = 0;
		break;
	case 0x02:
		/* SLIDE DOWN 02
		 * The same as SLIDE UP, but it slides down.
		 * Note: a param of 0 does nothing and should be ignored.
		 */
		if (!event->fxp)
			event->fxt = 0;
		break;
	case 0x03:
		/* PORTAMENTO 03
		 * Makes precise sliding easy.
		 */
		break;
	case 0x04:
		/* VIBRATO 04
		 * The left half of the argument is the vibrato speed, the
		 * right half is the depth. If the numbers are zeros, the
		 * previous speed and depth are used.
		 */
		/* Note: this is twice as deep as the Protracker vibrato */
		event->fxt = FX_VIBRATO2;
		break;
	case 0x05:
		/* SLIDE + FADE 05
		 * ProTracker compatible. This command is the combination of
		 * commands 0300 and 0Dxx. The argument is the fade speed.
		 * The slide will continue during this command.
		 */
		/* fall-through */
	case 0x06:
		/* VIBRATO + FADE 06
		 * ProTracker compatible. Combines commands 0400 and 0Dxx.
		 * The argument is the fade speed. The vibrato will continue
		 * during this command.
		 */
		/* fall-through */
	case 0x07:
		/* TREMOLO 07
		 * ProTracker compatible.
		 * This command is a kind of "volume vibrato". The left
		 * number is the speed of the tremolo, and the right one is
		 * the depth. The depth must be quite high before the effect
		 * is audible.
		 */
		break;
	case 0x08:
		/* HOLD and DECAY 08
		 * This command must be on the same line as the note. The
		 * left half of the argument determines the decay and the
		 * right half the hold.
		 */
		event->fxt = event->fxp = 0;
		break;
	case 0x09:
		/* SECONDARY TEMPO 09
		 * This sets the secondary tempo (the number of timing
		 * pulses per note). The argument must be from 01 to 20 (hex).
		 */
		if (event->fxp >= 0x01 && event->fxp <= 0x20) {
			event->fxt = FX_SPEED;
		} else {
			event->fxt = event->fxp = 0;
		}
		break;
	case 0x0a:
		/* 0A not mentioned but it's Protracker-compatible */
		/* fall-through */
	case 0x0b:
		/* POSITION JUMP 0B
		 * The song plays up to this command and then jumps to
		 * another position in the play sequence. The song then
		 * loops from the point jumped to, to the end of the song
		 * forever. The purpose is to allow for introductions that
		 * play only once.
		 */
		/* fall-through */
	case 0x0c:
		/* SET VOLUME 0C
		 * Overrides the default volume of an instrument.
		 */
		if (!hexvol) {
			int p = event->fxp;
			event->fxp = (p >> 8) * 10 + (p & 0xff);
		}
		break;
	case 0x0d:
		/* VOLUME SLIDE 0D
		 * Smoothly slides the volume up or down. The left half of
		 * the argument increases the volume. The right decreases it.
		 */
		event->fxt = FX_VOLSLIDE;
		break;
	case 0x0e:
		/* SYNTH JUMP 0E
		 * When used with synthetic or hybrid instruments, it
		 * triggers a jump in the Waveform Sequence List. The argument
		 * is the jump destination (line no).
		 */
		event->fxt = event->fxp = 0;
		break;
	case 0x0f:
		/* MISCELLANEOUS 0F
		 * The effect depends upon the value of the argument.
		 */
		if (event->fxp == 0x00) {	/* Jump to next block */
			event->fxt = FX_BREAK;
			break;
		} else if (event->fxp <= 0xf0) {
			event->fxt = FX_S3M_BPM;
			event->fxp = mmd_convert_tempo(event->fxp, bpm_on, med_8ch);
			break;
		} else switch (event->fxp) {
		case 0xf1:	/* Play note twice */
			event->fxt = FX_EXTENDED;
			event->fxp = (EX_RETRIG << 4) | 3;
			break;
		case 0xf2:	/* Delay note */
			event->fxt = FX_EXTENDED;
			event->fxp = (EX_DELAY << 4) | 3;
			break;
		case 0xf3:	/* Play note three times */
			/* Actually just retriggers every 2 ticks, except
			 * for a bug in OctaMED <=4.00 where it will retrigger
			 * on (tick & 7) >= 2 (TODO: verify). */
			event->fxt = FX_MED_RETRIG;
			event->fxp = 0x02;
			break;
		case 0xf8:	/* Turn filter off */
		case 0xf9:	/* Turn filter on */
		case 0xfa:	/* MIDI pedal on */
		case 0xfb:	/* MIDI pedal off */
		case 0xfd:	/* Set pitch */
		case 0xfe:	/* End of song */
			event->fxt = event->fxp = 0;
			break;
		case 0xff:	/* Turn note off */
			event->fxt = event->fxp = 0;
			event->note = XMP_KEY_CUT;
			break;
		default:
			event->fxt = event->fxp = 0;
		}
		break;
	case 0x11:
		/* SLIDE PITCH UP (only once) 11
		 * Equivalent to ProTracker command E1x.
		 * Lets you control the pitch with great accuracy. This
		 * command changes only this occurrence of the note.
		 * Note: a param of 0 does nothing and should be ignored.
		 */
		event->fxt = FX_F_PORTA_UP;
		break;
	case 0x12:
		/* SLIDE DOWN (only once) 12
		 * Equivalent to ProTracker command E2x.
		 * Note: a param of 0 does nothing and should be ignored.
		 */
		event->fxt = FX_F_PORTA_DN;
		break;
	case 0x14:
		/* VIBRATO 14
		 * ProTracker compatible. This is similar to command 04
		 * except the depth is halved, to give greater accuracy.
		 */
		event->fxt = FX_VIBRATO;
		break;
	case 0x15:
		/* SET FINETUNE 15
		 * Set a finetune value for a note, overrides the default
		 * fine tune value of the instrument. Negative numbers must
		 * be entered as follows:
		 *   -1 => FF    -3 => FD    -5 => FB    -7 => F9
		 *   -2 => FE    -4 => FC    -6 => FA    -8 => F8
		 */
		event->fxt = FX_FINETUNE;
		event->fxp = (event->fxp + 8) << 4;
		break;
	case 0x16:
		/* LOOP 16
		 * Creates a loop within a block. 1600 marks the beginning
		 * of the loop.  The next occurrence of the 16 command
		 * designates the number of loops. Same as ProTracker E6x.
		 */
		event->fxt = FX_EXTENDED;
		if (event->fxp > 0x0f)
			event->fxp = 0x0f;
		event->fxp |= 0x60;
		break;
	case 0x18:
		/* STOP NOTE 18
		 * Cuts the note by zeroing the volume at the pulse specified
		 * in the argument value. This is the same as ProTracker
		 * command ECx.
		 */
		event->fxt = FX_EXTENDED;
		if (event->fxp > 0x0f)
			event->fxp = 0x0f;
		event->fxp |= 0xc0;
		break;
	case 0x19:
		/* SET SAMPLE START OFFSET
		 * Same as ProTracker command 9.
		 * When playing a sample, this command sets the starting
		 * offset (at steps of $100 = 256 bytes). Useful for speech
		 * samples.
		 */
		event->fxt = FX_OFFSET;
		break;
	case 0x1a:
		/* SLIDE VOLUME UP ONCE
		 * Only once ProTracker command EAx. Lets volume slide
		 * slowly once per line.
		 * Note: a param of 0 does nothing and should be ignored.
		 */
		event->fxt = event->fxp ? FX_F_VSLIDE_UP : 0;
		break;
	case 0x1b:
		/* SLIDE VOLUME DOWN ONCE
		 * Only once ProTracker command EBx.
		 * Note: a param of 0 does nothing and should be ignored.
		 */
		event->fxt = event->fxp ? FX_F_VSLIDE_DN : 0;
		break;
	case 0x1d:
		/* JUMP TO NEXT BLOCK 1D
		 * Jumps to the next line in the PLAY SEQUENCE LIST at the
		 * specified line. ProTracker command D. This command is
		 * like F00, except that you can specify the line number of
		 * the first line to be played. The line number must be
		 * specified in HEX.
		 */
		event->fxt = FX_BREAK;
		break;
	case 0x1e:
		/* PLAY LINE x TIMES 1E
		 * Plays only commands, notes not replayed. ProTracker
		 * pattern delay.
		 */
		event->fxt = FX_PATT_DELAY;
		break;
	case 0x1f:
		/* Command 1F: NOTE DELAY AND RETRIGGER
		 * (Protracker commands EC and ED)
		 * Gives you accurate control over note playing. You can
		 * delay the note any number of ticks, and initiate fast
		 * retrigger. Level 1 = note delay value, level 2 = retrigger
		 * value.
		 * Unlike FF1/FF3, this does nothing on a line with no note.
		 */
		if (event->fxp != 0 && event->note != 0) {
			event->fxt = FX_MED_RETRIG;
		} else {
			event->fxt = event->fxp = 0;
		}
		break;
	case 0x20:
		/* Command 20: REVERSE SAMPLE / RELATIVE SAMPLE OFFSET
		 * With command level $00, the sample is reversed. With other
		 * levels, it changes the sample offset, just like command 19,
		 * except the command level is the new offset relative to the
		 * current sample position being played.
		 * Note: 20 00 only works on the same line as a new note.
		 */
		if (event->fxp == 0 && event->note != 0) {
			event->fxt = FX_REVERSE;
			event->fxp = 1;
		} else {
			event->fxt = event->fxp = 0;
		}
		break;
	case 0x2e:
		/* Command 2E: SET TRACK PANNING
		 * Allows track panning to be changed during play. The track
		 * on which the player command appears is the track affected.
		 * The command level is in signed hex: $F0 to $10 = -16 to 16
		 * decimal.
		 */
		if (event->fxp >= 0xf0 || event->fxp <= 0x10) {
			int fxp = (signed char)event->fxp + 16;
			fxp <<= 3;
			if (fxp == 0x100)
				fxp--;
			event->fxt = FX_SETPAN;
			event->fxp = fxp;
		}
		break;
	default:
		event->fxt = event->fxp = 0;
		break;
	}
}


struct mmd_instrument_info
{
	uint32 length;
	uint32 rep;
	uint32 replen;
	int sampletrans;
	int synthtrans;
	int flg;
	int enable;
};

/* Interpret loop/flag parameters for sampled instruments (sample, hybrid, IFF).
 * This is common code to avoid replicating this mess in each loader. */
static void mmd_load_instrument_common(
			struct mmd_instrument_info *info, struct InstrHdr *instr,
			struct MMD0exp *expdata, struct InstrExt *exp_smp,
			struct MMD0sample *sample, int ver)
{
	info->enable = 1;
	info->flg = 0;
	if (ver >= 2 && expdata->s_ext_entrsz >= 8) {	/* MMD2+ instrument flags */
		uint8 instr_flags = exp_smp->instr_flags;

		if (instr_flags & SSFLG_LOOP) {
			info->flg |= XMP_SAMPLE_LOOP;
		}
		if (instr_flags & SSFLG_PINGPONG) {
			info->flg |= XMP_SAMPLE_LOOP_BIDIR;
		}
		if (instr_flags & SSFLG_DISABLED) {
			info->enable = 0;
		}
	} else {
		if (sample->replen > 1) {
			info->flg |= XMP_SAMPLE_LOOP;
		}
	}

	info->sampletrans = 36 + sample->strans;
	info->synthtrans = 12 + sample->strans;

	if (instr) {
		int sample_type = instr->type & ~(S_16|MD16|STEREO);

		if ((ver >= 3 && sample_type == 0) || sample_type == 7) {
			/* Mix mode transposes samples down two octaves.
			 * This does not apply to octave samples or synths.
			 * ExtSamples (7) are transposed regardless. */
			info->sampletrans -= 24;
		}

		info->length = instr->length;

		if (ver >= 2 && expdata->s_ext_entrsz >= 18) {	/* MMD2+ long repeat */
			info->rep = exp_smp->long_repeat;
			info->replen = exp_smp->long_replen;
		} else {
			info->rep = sample->rep << 1;
			info->replen = sample->replen << 1;
		}

		if (instr->type & S_16) {
			info->flg |= XMP_SAMPLE_16BIT;
			/* Length is (bytes / channels) but the
			 * loop is measured in sample frames. */
			info->length >>= 1;
		}

		/* STEREO means that this is a stereo sample. The sample
		* is not interleaved. The left channel comes first,
		* followed by the right channel. Important: Length
		* specifies the size of one channel only! The actual memory
		* usage for both samples is length * 2 bytes.
		*/
		if (instr->type & STEREO) {
			info->flg |= XMP_SAMPLE_STEREO;
		}
	}
}

/* Compatibility for MED Soundstudio v2 default pitch events.
 * For single-octave samples and synthetics, MED mix mode note 0x01
 * plays the note number stored in the InstrExt default pitch field.
 * Mix mode events are currently transposed up an octave and are offset
 * down by 1 for the instrument map, hence index 12.
 *
 * See med.h for more info.
 */
static void mmd_set_default_pitch_note(struct xmp_instrument *xxi,
					struct InstrExt *exp_smp, int ver)
{
	if (ver >= 3) {
		int note = MMD3_DEFAULT_NOTE;

		if (exp_smp->default_pitch)
			note = exp_smp->default_pitch - 1;

		if (note >= 0 && note < XMP_MAX_KEYS)
			xxi->map[12].xpo = note;
	}
}

int mmd_alloc_tables(struct module_data *m, int i, struct SynthInstr *synth)
{
	struct med_module_extras *me = (struct med_module_extras *)m->extra;

	me->vol_table[i] = (uint8 *) calloc(1, synth->voltbllen);
	if (me->vol_table[i] == NULL)
		goto err;
	memcpy(me->vol_table[i], synth->voltbl, synth->voltbllen);

	me->wav_table[i] = (uint8 *) calloc(1, synth->wftbllen);
	if (me->wav_table[i] == NULL)
		goto err1;
	memcpy(me->wav_table[i], synth->wftbl, synth->wftbllen);

	return 0;

    err1:
	free(me->vol_table[i]);
    err:
	return -1;
}

static int mmd_load_hybrid_instrument(HIO_HANDLE *f, struct module_data *m, int i,
			int smp_idx, struct SynthInstr *synth,
			struct MMD0exp *expdata, struct InstrExt *exp_smp,
			struct MMD0sample *sample, int ver)
{
	struct xmp_module *mod = &m->mod;
	struct xmp_instrument *xxi = &mod->xxi[i];
	struct xmp_subinstrument *sub;
	struct xmp_sample *xxs;
	struct med_instrument_extras *ie;
	struct mmd_instrument_info info;
	struct InstrHdr instr;
	int pos = hio_tell(f);
	int j;

	/* Sanity check */
	if (smp_idx >= mod->smp) {
		return -1;
	}

	synth->defaultdecay = hio_read8(f);
	hio_seek(f, 3, SEEK_CUR);
	synth->rep = hio_read16b(f);
	synth->replen = hio_read16b(f);
	synth->voltbllen = hio_read16b(f);
	synth->wftbllen = hio_read16b(f);
	synth->volspeed = hio_read8(f);
	synth->wfspeed = hio_read8(f);
	synth->wforms = hio_read16b(f);
	hio_read(synth->voltbl, 1, 128, f);
	hio_read(synth->wftbl, 1, 128, f);

	/* Sanity check */
	if (synth->voltbllen > 128 || synth->wftbllen > 128 ||
	    synth->wforms < 1 || synth->wforms > 64) {
		return -1;
	}

	for (j = 0; j < synth->wforms; j++)
		synth->wf[j] = hio_read32b(f);

	if (hio_error(f))
		return -1;

	hio_seek(f, pos - 6 + synth->wf[0], SEEK_SET);
	instr.length = hio_read32b(f);
	instr.type = hio_read16b(f);

	/* Hybrids using IFFOCT/ext samples as their sample don't seem to
	 * exist. If one is found, this should be fixed. OctaMED SS 1.03
	 * converts 16-bit samples to 8-bit when changed to hybrid. */
	if (instr.type != 0) {
		D_(D_CRIT "unsupported sample type %d for hybrid", instr.type);
		return -1;
	}

	if (libxmp_med_new_instrument_extras(xxi) != 0)
		return -1;

	xxi->nsm = synth->wforms;
	if (libxmp_alloc_subinstrument(mod, i, synth->wforms) < 0)
		return -1;

	ie = MED_INSTRUMENT_EXTRAS(*xxi);
	ie->vts = synth->volspeed;
	ie->wts = synth->wfspeed;
	ie->vtlen = synth->voltbllen;
	ie->wtlen = synth->wftbllen;

	mmd_load_instrument_common(&info, &instr, expdata, exp_smp, sample, ver);
	mmd_set_default_pitch_note(xxi, exp_smp, ver);
	sub = &xxi->sub[0];

	sub->pan = 0x80;
	sub->vol = info.enable ? sample->svol : 0;
	sub->xpo = info.sampletrans;
	sub->sid = smp_idx;
	sub->fin = exp_smp->finetune;

	xxs = &mod->xxs[smp_idx];

	xxs->len = info.length;
	xxs->lps = info.rep;
	xxs->lpe = info.rep + info.replen;
	xxs->flg = info.flg;

	if (libxmp_load_sample(m, f, 0, xxs, NULL) < 0)
		return -1;

	smp_idx++;

	for (j = 1; j < synth->wforms; j++) {
		sub = &xxi->sub[j];
		xxs = &mod->xxs[smp_idx];

		/* Sanity check */
		if (j >= xxi->nsm || smp_idx >= mod->smp)
			return -1;

		sub->pan = 0x80;
		sub->vol = info.enable ? 64 : 0;
		sub->xpo = info.synthtrans;
		sub->sid = smp_idx;
		sub->fin = exp_smp->finetune;

		hio_seek(f, pos - 6 + synth->wf[j], SEEK_SET);

		xxs->len = hio_read16b(f) * 2;
		xxs->lps = 0;
		xxs->lpe = xxs->len;
		xxs->flg = XMP_SAMPLE_LOOP;

		if (libxmp_load_sample(m, f, 0, xxs, NULL) < 0)
			return -1;

		smp_idx++;
	}
	return 0;
}

static int mmd_load_synth_instrument(HIO_HANDLE *f, struct module_data *m, int i,
			int smp_idx, struct SynthInstr *synth,
			struct MMD0exp *expdata, struct InstrExt *exp_smp,
			struct MMD0sample *sample, int ver)
{
	struct xmp_module *mod = &m->mod;
	struct xmp_instrument *xxi = &mod->xxi[i];
	struct med_instrument_extras *ie;
	struct mmd_instrument_info info;
	int pos = hio_tell(f);
	int j;

	mmd_load_instrument_common(&info, NULL, expdata, exp_smp, sample, ver);
	mmd_set_default_pitch_note(xxi, exp_smp, ver);

	synth->defaultdecay = hio_read8(f);
	hio_seek(f, 3, SEEK_CUR);
	synth->rep = hio_read16b(f);
	synth->replen = hio_read16b(f);
	synth->voltbllen = hio_read16b(f);
	synth->wftbllen = hio_read16b(f);
	synth->volspeed = hio_read8(f);
	synth->wfspeed = hio_read8(f);
	synth->wforms = hio_read16b(f);
	hio_read(synth->voltbl, 1, 128, f);
	hio_read(synth->wftbl, 1, 128, f);

	if (synth->wforms == 0xffff) {
		xxi->nsm = 0;
		return 1;
	}
	if (synth->voltbllen > 128 ||
	    synth->wftbllen > 128 ||
	    synth->wforms > 64) {
		return -1;
	}

	for (j = 0; j < synth->wforms; j++)
		synth->wf[j] = hio_read32b(f);

	if (hio_error(f))
		return -1;

	D_(D_INFO "  VS:%02x WS:%02x WF:%02x %02x %+3d %+1d",
			synth->volspeed, synth->wfspeed,
			synth->wforms & 0xff,
			sample->svol,
			sample->strans,
			exp_smp->finetune);

	if (libxmp_med_new_instrument_extras(&mod->xxi[i]) != 0)
		return -1;

	xxi->nsm = synth->wforms;
	if (libxmp_alloc_subinstrument(mod, i, synth->wforms) < 0)
		return -1;

	ie = MED_INSTRUMENT_EXTRAS(*xxi);
	ie->vts = synth->volspeed;
	ie->wts = synth->wfspeed;
	ie->vtlen = synth->voltbllen;
	ie->wtlen = synth->wftbllen;

	for (j = 0; j < synth->wforms; j++) {
		struct xmp_subinstrument *sub = &xxi->sub[j];
		struct xmp_sample *xxs = &mod->xxs[smp_idx];

		/* Sanity check */
		if (j >= xxi->nsm || smp_idx >= mod->smp)
			return -1;

		sub->pan = 0x80;
		sub->vol = info.enable ? 64 : 0;
		sub->xpo = info.synthtrans;
		sub->sid = smp_idx;
		sub->fin = exp_smp->finetune;

		hio_seek(f, pos - 6 + synth->wf[j], SEEK_SET);

		xxs->len = hio_read16b(f) * 2;
		xxs->lps = 0;
		xxs->lpe = xxs->len;
		xxs->flg = XMP_SAMPLE_LOOP;

		if (libxmp_load_sample(m, f, 0, xxs, NULL) < 0)
			return -1;

		smp_idx++;
	}

	return 0;
}

static int mmd_load_sampled_instrument(HIO_HANDLE *f, struct module_data *m, int i,
			int smp_idx, struct InstrHdr *instr,
			struct MMD0exp *expdata, struct InstrExt *exp_smp,
			struct MMD0sample *sample, int ver)
{
	struct xmp_module *mod = &m->mod;
	struct xmp_instrument *xxi = &mod->xxi[i];
	struct xmp_subinstrument *sub;
	struct xmp_sample *xxs;
	struct mmd_instrument_info info;
	int j, k;

	/* Sanity check */
	if (smp_idx >= mod->smp)
		return -1;

	/* hold & decay support */
        if (libxmp_med_new_instrument_extras(xxi) != 0)
                return -1;
	MED_INSTRUMENT_EXTRAS(*xxi)->hold = exp_smp->hold;
	xxi->rls = 0xfff - (exp_smp->decay << 4);

	xxi->nsm = 1;
	if (libxmp_alloc_subinstrument(mod, i, 1) < 0)
		return -1;

	mmd_load_instrument_common(&info, instr, expdata, exp_smp, sample, ver);
	mmd_set_default_pitch_note(xxi, exp_smp, ver);
	sub = &xxi->sub[0];

	sub->vol = info.enable ? sample->svol : 0;
	sub->pan = 0x80;
	sub->xpo = info.sampletrans;
	sub->sid = smp_idx;
	sub->fin = exp_smp->finetune << 4;

	xxs = &mod->xxs[smp_idx];

	xxs->len = info.length;
	xxs->lps = info.rep;
	xxs->lpe = info.rep + info.replen;
	xxs->flg = info.flg;

        /* Restrict sampled instruments to 3 octave range except for MMD3.
         * Checked in MMD0 with med.egypian/med.medieval from Lemmings 2
         * and MED.ParasolStars, MMD1 with med.Lemmings2
         */

	if (ver < 3) {
		/* ExtSamples have two extra octaves. */
		int octaves = (instr->type & 7) == 7 ? 5 : 3;
		for (j = 0; j < 9; j++) {
			for (k = 0; k < 12; k++) {
				int xpo = 0;

				if (j < 1)
					xpo = 12 * (1 - j);
				else if (j > octaves)
					xpo = -12 * (j - octaves);

				xxi->map[12 * j + k].xpo = xpo;
			}
		}
	}


	if (libxmp_load_sample(m, f, SAMPLE_FLAG_BIGEND, xxs, NULL) < 0) {
		return -1;
	}

	return 0;
}

static const char iffoct_insmap[6][9] = {
	/* 2 */ { 1, 1, 1, 0, 0, 0, 0, 0, 0 },
	/* 3 */ { 2, 2, 2, 2, 2, 2, 1, 1, 0 },
	/* 4 */ { 3, 3, 3, 2, 2, 2, 1, 1, 0 },
	/* 5 */ { 4, 4, 4, 3, 2, 2, 1, 1, 0 },
	/* 6 */ { 5, 5, 5, 5, 4, 3, 2, 1, 0 },
	/* 7 */ { 6, 6, 6, 6, 5, 4, 3, 2, 1 }
};

static const char iffoct_xpomap[6][9] = {
	/* 2 */ { 12, 12, 12,  0,  0,  0,  0,  0,  0 },
	/* 3 */ { 12, 12, 12, 12, 12, 12,  0,  0,-12 },
	/* 4 */ { 12, 12, 12,  0,  0,  0,-12,-12,-24 },
	/* 5 */ { 24, 24, 24, 12,  0,  0,-12,-24,-36 },
	/* 6 */ { 12, 12, 12, 12,  0,-12,-24,-36,-48 },
	/* 7 */ { 12, 12, 12, 12,  0,-12,-24,-36,-48 },
};

static int mmd_load_iffoct_instrument(HIO_HANDLE *f, struct module_data *m, int i,
			int smp_idx, struct InstrHdr *instr, int num_oct,
			struct MMD0exp *expdata, struct InstrExt *exp_smp,
			struct MMD0sample *sample, int ver)
{
	struct xmp_module *mod = &m->mod;
	struct xmp_instrument *xxi = &mod->xxi[i];
	struct xmp_subinstrument *sub;
	struct xmp_sample *xxs;
	struct mmd_instrument_info info;
	int size, j, k;

	if (num_oct < 2 || num_oct > 7)
		return -1;

	/* Sanity check */
	if (smp_idx + num_oct > mod->smp)
		return -1;

	/* Sanity check - ignore absurdly large IFFOCT instruments. */
	if ((int)instr->length < 0)
		return -1;

	/* hold & decay support */
	if (libxmp_med_new_instrument_extras(xxi) != 0)
		return -1;

	MED_INSTRUMENT_EXTRAS(*xxi)->hold = exp_smp->hold;
	xxi->rls = 0xfff - (exp_smp->decay << 4);

	xxi->nsm = num_oct;
	if (libxmp_alloc_subinstrument(mod, i, num_oct) < 0)
		return -1;

	/* base octave size */
	size = instr->length / ((1 << num_oct) - 1);
	mmd_load_instrument_common(&info, instr, expdata, exp_smp, sample, ver);

	for (j = 0; j < num_oct; j++) {
		sub = &xxi->sub[j];

		sub->vol = info.enable ? sample->svol : 0;
		sub->pan = 0x80;
		sub->xpo = info.sampletrans - 12;
		sub->sid = smp_idx;
		sub->fin = exp_smp->finetune << 4;

		xxs = &mod->xxs[smp_idx];

		xxs->len = size;
		xxs->lps = info.rep;
		xxs->lpe = info.rep + info.replen;
		xxs->flg = info.flg;

		if (libxmp_load_sample(m, f, SAMPLE_FLAG_BIGEND, xxs, NULL) < 0) {
			return -1;
		}

		smp_idx++;
		size <<= 1;
		info.rep <<= 1;
		info.replen <<= 1;
	}

	/* instrument mapping */

	for (j = 0; j < 9; j++) {
		for (k = 0; k < 12; k++) {
			xxi->map[12 * j + k].ins = iffoct_insmap[num_oct - 2][j];
			xxi->map[12 * j + k].xpo = iffoct_xpomap[num_oct - 2][j];
		}
	}

	return 0;
}

/* Number of octaves in IFFOCT samples */
const int mmd_num_oct[6] = { 5, 3, 2, 4, 6, 7 };

int mmd_load_instrument(HIO_HANDLE *f, struct module_data *m, int i, int smp_idx,
			struct MMD0exp *expdata, struct InstrExt *exp_smp,
			struct MMD0sample *sample, int ver)
{
	struct InstrHdr instr;
	struct SynthInstr synth;
	int sample_type;

	instr.length = hio_read32b(f);
	instr.type = (int16)hio_read16b(f);
	sample_type = instr.type & ~(S_16|MD16|STEREO);

	D_(D_INFO "[%2x] %-40.40s %d", i, m->mod.xxi[i].name, instr.type);

	if (instr.type == -2) {					/* Hybrid */
		int ret = mmd_load_hybrid_instrument(f, m, i, smp_idx,
			&synth, expdata, exp_smp, sample, ver);

		if (ret < 0) {
			D_(D_CRIT "error loading hybrid instrument %d", i);
			return -1;
		}

		smp_idx += synth.wforms;

		if (mmd_alloc_tables(m, i, &synth) != 0)
			return -1;

	} else if (instr.type == -1) {				/* Synthetic */
		int ret = mmd_load_synth_instrument(f, m, i, smp_idx,
			&synth, expdata, exp_smp, sample, ver);

		if (ret > 0)
			return smp_idx;

		if (ret < 0) {
			D_(D_CRIT "error loading synthetic instrument %d", i);
			return -1;
		}

		smp_idx += synth.wforms;

		if (mmd_alloc_tables(m, i, &synth) != 0)
			return -1;

	} else if (instr.type >= 1 && instr.type <= 6) {	/* IFFOCT */
		int ret;
		const int oct = mmd_num_oct[instr.type - 1];

		ret = mmd_load_iffoct_instrument(f, m, i, smp_idx,
			&instr, oct, expdata, exp_smp, sample, ver);

		if (ret < 0) {
			D_(D_CRIT "error loading IFFOCT instrument %d", i);
			return -1;
		}

		smp_idx += oct;

	} else if (sample_type == 0 || sample_type == 7) {	/* Sample */
		int ret;

		ret = mmd_load_sampled_instrument(f, m, i, smp_idx,
			&instr, expdata, exp_smp, sample, ver);

		if (ret < 0) {
			D_(D_CRIT "error loading sample %d", i);
			return -1;
		}

		smp_idx++;

	} else {
		/* Invalid instrument type */
		D_(D_CRIT "invalid instrument type: %d", instr.type);
		return -1;
	}
	return smp_idx;
}

/* Load an external instrument (pre-MMD when the internal instruments flag is
 * not set). Returns 0 on successfully loading or if the instrument can't be
 * found. Returns -1 if an instrument is found but fails to load. */
int med_load_external_instrument(HIO_HANDLE *f, struct module_data *m, int i)
{
	struct xmp_module *mod = &m->mod;
	char path[XMP_MAXPATH];
	char ins_name[32];
	HIO_HANDLE *s = NULL;

	if (libxmp_copy_name_for_fopen(ins_name, mod->xxi[i].name, 32) != 0)
		return 0;

	D_(D_INFO "[%2X] %-32.32s ---- %04x %04x %c V%02x",
		i, mod->xxi[i].name, mod->xxs[i].lps, mod->xxs[i].lpe,
		mod->xxs[i].flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
		mod->xxi[i].sub[0].vol);

	if (!libxmp_find_instrument_file(m, path, sizeof(path), ins_name))
		return 0;

	if ((s = hio_open(path, "rb")) == NULL) {
		return 0;
	}

	mod->xxs[i].len = hio_size(s);
	if (mod->xxs[i].len == 0) {
		hio_close(s);
		return 0;
	}
	mod->xxi[i].nsm = 1;

	D_(D_INFO "     %-32s %04x", "(OK)", mod->xxs[i].len);

	if (libxmp_load_sample(m, s, 0, &mod->xxs[i], NULL) < 0) {
		hio_close(s);
		return -1;
	}
	hio_close(s);
	return 0;
}


void mmd_set_bpm(struct module_data *m, int med_8ch, int deftempo,
						int bpm_on, int bpmlen)
{
	struct xmp_module *mod = &m->mod;

	mod->bpm = mmd_convert_tempo(deftempo, bpm_on, med_8ch);

	/* 8-channel mode completely overrides regular timing.
	 * See mmd_convert_tempo for more info.
	 */
	if (med_8ch) {
		m->time_factor = DEFAULT_TIME_FACTOR;
	} else if (bpm_on) {
		m->time_factor = DEFAULT_TIME_FACTOR * 4 / bpmlen;
	}
}


void mmd_info_text(HIO_HANDLE *f, struct module_data *m, int offset)
{
	int type, len;

	/* Copy song info text */
	hio_read32b(f);		/* skip next */
	hio_read16b(f);		/* skip reserved */
	type = hio_read16b(f);
	D_(D_INFO "mmdinfo type=%d", type);
	if (type == 1) {	/* 1 = ASCII */
		len = hio_read32b(f);
		D_(D_INFO "mmdinfo length=%d", len);
		if (len > 0 && len < hio_size(f)) {
			m->comment = (char *) malloc(len + 1);
			if (m->comment == NULL)
				return;
			hio_read(m->comment, 1, len, f);
			m->comment[len] = 0;
		}
	}
}

/* Determine an approximate tracker version from an MMD module since, unlike
 * MED4, they don't store any useful tracker information. If expdata is not
 * present in the module, it should be passed as NULL.
 */
int mmd_tracker_version(struct module_data *m, int mmdver, int mmdc,
	struct MMD0exp *expdata)
{
	struct xmp_module *mod = &m->mod;
	int soundstudio = 0;
	int medver = 0;
	int s_ext_entrsz = 0;
	int mmdch = '0' + mmdver;

	if (expdata) {
		D_(D_INFO "expdata.s_ext_entrsz = %d", expdata->s_ext_entrsz);
		D_(D_INFO "expdata.i_ext_entrsz = %d", expdata->i_ext_entrsz);
		s_ext_entrsz = expdata->s_ext_entrsz;
	} else {
		D_(D_INFO "expdata = NULL");
	}

	if (s_ext_entrsz > 18) {		/* s_ext_entrsz == 24 */
		medver = MED_VER_OCTAMED_SS_2;
		soundstudio = 2;
	} else if (mmdver >= 3) {
		medver = MED_VER_OCTAMED_SS_1;
		soundstudio = 1;
	} else if (s_ext_entrsz > 10) {		/* s_ext_entrsz == 18 */
		medver = MED_VER_OCTAMED_SS_1;
		soundstudio = 1;
	} else if (s_ext_entrsz > 8) {		/* s_ext_entrsz == 10 */
		medver = MED_VER_OCTAMED_502;
	} else if (s_ext_entrsz > 4) {		/* s_ext_entrsz == 8 */
		medver = MED_VER_OCTAMED_500;
	} else if (mmdver >= 2) {
		medver = MED_VER_OCTAMED_500;
	} else if (mmdver >= 1) {
		medver = MED_VER_OCTAMED_400;
	} else if (mod->chn > 4) {
		medver = MED_VER_OCTAMED_200;
	} else if (s_ext_entrsz > 2) {		/* s_ext_entrsz == 4 */
		medver = MED_VER_320;
	} else if (expdata != NULL) {		/* s_ext_entrsz == 2 */
		/* MED 3.00 and 3.10 i_ext_entrsz always 0? */
		medver = MED_VER_300;
	} else {
		medver = MED_VER_210;
	}

	if (mmdc) {
		mmdch = 'C';
	}

	if (soundstudio == 2) {
		libxmp_set_type(m, "MED Soundstudio 2.00 MMD%c", mmdch);
	} else if (soundstudio == 1) {
		libxmp_set_type(m, "OctaMED Soundstudio MMD%c", mmdch);
	} else if (medver > MED_VER_320) {
		libxmp_set_type(m, "OctaMED %d.%02x MMD%c",
				medver >> 12, medver & 0xff, mmdch);
	} else {
		libxmp_set_type(m, "MED %d.%02x MMD%c",
				medver >> 8, medver & 0xff, mmdch);
	}
	return medver;
}
