/*

	TiMidity -- Experimental MIDI to WAVE converter
	Copyright (C) 1995 Tuukka Toivonen <toivonen@clinet.fi>

	This library is free software; you can redistribute it and/or
	modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation; either
	version 2.1 of the License, or (at your option) any later version.

	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public
	License along with this library; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

	instrum.c 

	Code to load and unload GUS-compatible instrument patches.

*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "timidity.h"
#include "m_swap.h"
#include "files.h"

namespace Timidity
{

extern InstrumentLayer *load_instrument_dls(Renderer *song, int drum, int bank, int instrument);

/* Some functions get aggravated if not even the standard banks are 
available. */
ToneBank standard_tonebank, standard_drumset;

/* This is only used for tracks that don't specify a program */
int default_program = DEFAULT_PROGRAM;
extern int openmode;


static void free_instrument(Instrument *ip)
{
	Sample *sp;
	int i;
	if (ip == NULL)
	{
		return;
	}
	for (i = 0, sp = &(ip->sample[0]); i < ip->samples; i++, sp++)
	{
		if (sp->data != NULL)
		{
			free(sp->data);
		}
	}
	for (i = 0, sp = &(ip->right_sample[0]); i < ip->right_samples; i++)
	{
		if (sp->data != NULL)
		{
			free(sp->data);
		}
	}
	free(ip->sample);
	if (ip->right_sample != NULL)
	{
		free(ip->right_sample);
	}
	free(ip);
}


static void free_layer(InstrumentLayer *lp)
{
	InstrumentLayer *next;

	for (; lp; lp = next)
	{
		next = lp->next;
		free_instrument(lp->instrument);
		free(lp);
	}
}

static void free_bank(int dr, int b)
{
	int i;
	ToneBank *bank = ((dr) ? drumset[b] : tonebank[b]);
	for (i = 0; i < MAXPROG; i++)
	{
		if (bank->tone[i].layer != NULL)
		{
			/* Not that this could ever happen, of course */
			if (bank->tone[i].layer != MAGIC_LOAD_INSTRUMENT)
			{
				free_layer(bank->tone[i].layer);
				bank->tone[i].layer = NULL;
			}
		}
	}
}


int convert_envelope_rate(Renderer *song, BYTE rate)
{
#if 1
	int r;

	r  = 3 - ((rate>>6) & 0x3);
	r *= 3;
	r  = (int)(rate & 0x3f) << r; /* 6.9 fixed point */

	/* 15.15 fixed point. */
	return int(((r * 44100) / song->rate) * song->control_ratio) << ((song->fast_decay) ? 10 : 9);
#else
	double frameadd = (double)(rate & 63) / (double)(1 << (3 * (rate >> 6)));
	double realadd = (frameadd * 19293 / song->rate) * (1 << 15) * song->control_ratio;
	return (int)realadd;
#endif
}

int convert_envelope_offset(BYTE offset)
{
	/* This is not too good... Can anyone tell me what these values mean?
	Are they GUS-style "exponential" volumes? And what does that mean? */

	/* 15.15 fixed point */
	return offset << (7 + 15);
}

int convert_tremolo_sweep(Renderer *song, BYTE sweep)
{
	if (!sweep)
		return 0;

	return
		int(((song->control_ratio * SWEEP_TUNING) << SWEEP_SHIFT) / (song->rate * sweep));
}

int convert_vibrato_sweep(Renderer *song, BYTE sweep, int vib_control_ratio)
{
	if (!sweep)
		return 0;

	return
		(int) (FSCALE((double) (vib_control_ratio) * SWEEP_TUNING, SWEEP_SHIFT) / (song->rate * sweep));

	/* this was overflowing with seashore.pat

	((vib_control_ratio * SWEEP_TUNING) << SWEEP_SHIFT) / (song->rate * sweep);
	*/
}

int convert_tremolo_rate(Renderer *song, BYTE rate)
{
	return
		int(((song->control_ratio * rate) << RATE_SHIFT) / (TREMOLO_RATE_TUNING * song->rate));
}

int convert_vibrato_rate(Renderer *song, BYTE rate)
{
	/* Return a suitable vibrato_control_ratio value */
	return
		int((VIBRATO_RATE_TUNING * song->rate) / (rate * 2 * VIBRATO_SAMPLE_INCREMENTS));
}

static void reverse_data(sample_t *sp, int ls, int le)
{
	sample_t s, *ep = sp + le;
	sp += ls;
	le -= ls;
	le /= 2;
	while (le--)
	{
		    s = *sp;
		*sp++ = *ep;
		*ep-- = s;
	}
}

/* 
If panning or note_to_use != -1, it will be used for all samples,
instead of the sample-specific values in the instrument file. 

For note_to_use, any value <0 or >127 will be forced to 0.

For other parameters, 1 means yes, 0 means no, other values are
undefined.

TODO: do reverse loops right */
static InstrumentLayer *load_instrument(Renderer *song, const char *name, int font_type, int percussion,
										int panning, int amp, int cfg_tuning, int note_to_use,
										int strip_loop, int strip_envelope,
										int strip_tail, int bank, int gm_num, int sf_ix)
{
	InstrumentLayer *lp, *lastlp, *headlp;
	Instrument *ip;
	FileReader *fp;
	BYTE tmp[239];
	int i,j;
	bool noluck = false;
	bool sf2flag = false;
	int right_samples = 0;
	int stereo_channels = 1, stereo_layer;
	int vlayer_list[19][4], vlayer, vlayer_count;

	if (!name) return 0;

	/* Open patch file */
	if ((fp = open_filereader(name, openmode, NULL)) == NULL)
	{
		/* Try with various extensions */
		FString tmp = name;
		tmp += ".pat";
		if ((fp = open_filereader(tmp, openmode, NULL)) == NULL)
		{
#ifdef unix			// Windows isn't case-sensitive.
			tmp.ToUpper();
			if ((fp = open_filereader(tmp, openmode, NULL)) == NULL)
#endif
			{
				noluck = true;
			}
		}
	}

	if (noluck)
	{
		cmsg(CMSG_ERROR, VERB_NORMAL, "Instrument `%s' can't be found.", name);
		return 0;
	}

	/*cmsg(CMSG_INFO, VERB_NOISY, "Loading instrument %s", current_filename);*/

	/* Read some headers and do cursory sanity checks. There are loads
	of magic offsets. This could be rewritten... */

	if ((239 != fp->Read(tmp, 239)) ||
		(memcmp(tmp, "GF1PATCH110\0ID#000002", 22) &&
		memcmp(tmp, "GF1PATCH100\0ID#000002", 22))) /* don't know what the
													differences are */
	{
		cmsg(CMSG_ERROR, VERB_NORMAL, "%s: not an instrument", name);
		delete fp;
		return 0;
	}

	/* patch layout:
	* bytes:  info:		starts at offset:
	* 12	header (see above)	0
	* 10    Gravis ID			12
	* 60	description			22
	*  1	instruments			82
	*  1	voices				83
	*  1	channels			84
	*  2	number of waveforms	85
	*  2	master volume		87
	*  4	datasize			89
	* 36   reserved, but now:	93
	*		 7 "SF2EXT\0" id	93
	*		 1 right samples	100
	*		28 reserved			101
	*  2	instrument number	129
	* 16	instrument name		131
	*  4	instrument size		147
	*  1	number of layers	151
	* 40	reserved			152
	*  1	layer duplicate		192
	*  1	layer number		193
	*  4	layer size			194
	*  1	number of samples	198
	* 40	reserved			199
	* 							239
	* THEN, for each sample, see below
	*/

	vlayer_count = 0;	// Silence, GCC

	if (!memcmp(tmp + 93, "SF2EXT", 6))
	{
		sf2flag = true;
		vlayer_count = tmp[152];
	}

	if (tmp[82] != 1 && tmp[82] != 0) /* instruments. To some patch makers, 0 means 1 */
	{
		cmsg(CMSG_ERROR, VERB_NORMAL, "Can't handle patches with %d instruments", tmp[82]);
		delete fp;
		return 0;
	}

	if (tmp[151] != 1 && tmp[151] != 0) /* layers. What's a layer? */
	{
		cmsg(CMSG_ERROR, VERB_NORMAL, "Can't handle instruments with %d layers", tmp[151]);
		delete fp;
		return 0;
	}


	if (sf2flag && vlayer_count > 0)
	{
		for (i = 0; i < 9; i++)
			for (j = 0; j < 4; j++)
				vlayer_list[i][j] = tmp[153+i*4+j];
		for (i = 9; i < 19; i++)
			for (j = 0; j < 4; j++)
				vlayer_list[i][j] = tmp[199+(i-9)*4+j];
	}
	else
	{
		for (i = 0; i < 19; i++)
			for (j = 0; j < 4; j++)
				vlayer_list[i][j] = 0;
		vlayer_list[0][0] = 0;
		vlayer_list[0][1] = 127;
		vlayer_list[0][2] = tmp[198];
		vlayer_list[0][3] = 0;
		vlayer_count = 1;
	}

	lastlp = NULL;
	headlp = NULL;	// Silence, GCC

	for (vlayer = 0; vlayer < vlayer_count; vlayer++)
	{
		lp = (InstrumentLayer *)safe_malloc(sizeof(InstrumentLayer));
		lp->lo = vlayer_list[vlayer][0];
		lp->hi = vlayer_list[vlayer][1];
		ip = (Instrument *)safe_malloc(sizeof(Instrument));
		lp->instrument = ip;
		lp->next = 0;

		if (lastlp != NULL)
		{
			lastlp->next = lp;
		}
		else
		{
			headlp = lp;
		}

		lastlp = lp;

		ip->type = sf2flag ? INST_SF2 : INST_GUS;
		ip->samples = vlayer_list[vlayer][2];
		ip->sample = (Sample *)safe_malloc(sizeof(Sample) * ip->samples);
		ip->left_samples = ip->samples;
		ip->left_sample = ip->sample;
		right_samples = vlayer_list[vlayer][3];
		ip->right_samples = right_samples;
		if (right_samples)
		{
			ip->right_sample = (Sample *)safe_malloc(sizeof(Sample) * right_samples);
			stereo_channels = 2;
		}
		else
		{
			ip->right_sample = NULL;
		}

		cmsg(CMSG_INFO, VERB_NOISY, "%s%s[%d,%d] %s(%d-%d layer %d of %d)",
			(percussion)? "   ":"", name,
			(percussion)? note_to_use : gm_num, bank,
			(right_samples)? "(2) " : "",
			lp->lo, lp->hi, vlayer+1, vlayer_count);

		for (stereo_layer = 0; stereo_layer < stereo_channels; stereo_layer++)
		{
			int sample_count;

			if (stereo_layer == 0)
			{
				sample_count = ip->left_samples;
			}
			else if (stereo_layer == 1)
			{
				sample_count = ip->right_samples;
			}
			else
			{
				sample_count = 0;
			}

			for (i = 0; i < sample_count; i++)
			{
				BYTE fractions;
				int tmplong;
				WORD tmpshort;
				WORD sample_volume;
				BYTE tmpchar;
				Sample *sp;
				BYTE sf2delay;

#define READ_CHAR(thing) \
	if (1 != fp->Read(&tmpchar,1)) goto fail; \
	thing = tmpchar;
#define READ_SHORT(thing) \
	if (2 != fp->Read(&tmpshort, 2)) goto fail; \
	thing = LittleShort(tmpshort);
#define READ_LONG(thing) \
	if (4 != fp->Read(&tmplong, 4)) goto fail; \
	thing = LittleLong(tmplong);

				/*
				*  7	 sample name
				*  1	 fractions
				*  4	 length
				*  4	 loop start
				*  4	 loop end
				*  2	 sample rate
				*  4	 low frequency
				*  4	 high frequency
				*  4	 root frequency
				*  2	 finetune
				*  1	 panning
				*  6	 envelope rates				|
				*  6	 envelope offsets			|  18 bytes
				*  3	 tremolo sweep, rate, depth	|
				*  3	 vibrato sweep, rate, depth	|
				*  1	 sample mode
				*  2	 scale frequency
				*  2	 scale factor				| from 0 to 2048 or 0 to 2
				*  2	 sample volume (??)
				* 34	 reserved
				* Now: 1 delay
				* 33	 reserved
				*/
				fp->Seek(7, SEEK_CUR);

				if (1 != fp->Read(&fractions, 1))
				{
fail:
					cmsg(CMSG_ERROR, VERB_NORMAL, "Error reading sample %d", i);
					if (stereo_layer == 1)
					{
						for (j = 0; j < i; j++)
						{
							free(ip->right_sample[j].data);
						}
						free(ip->right_sample);
						i = ip->left_samples;
					}
					for (j = 0; j < i; j++)
					{
						free(ip->left_sample[j].data);
					}
					free(ip->left_sample);
					free(ip);
					free(lp);
					delete fp;
					return 0;
				}

				if (stereo_layer == 0)
				{
					sp = &(ip->left_sample[i]);
				}
				else if (stereo_layer == 1)
				{
					sp = &(ip->right_sample[i]);
				}
				else
				{
					assert(0);
					sp = NULL;
				}

				READ_LONG(sp->data_length);
				READ_LONG(sp->loop_start);
				READ_LONG(sp->loop_end);
				READ_SHORT(sp->sample_rate);
				READ_LONG(sp->low_freq);
				READ_LONG(sp->high_freq);
				READ_LONG(sp->root_freq);
				fp->Seek(2, SEEK_CUR); /* Unused by GUS: Why have a "root frequency" and then "tuning"?? */
				sp->low_vel = 0;
				sp->high_vel = 127;

				READ_CHAR(tmp[0]);

				if (panning == -1)
					sp->panning = (tmp[0] * 8 + 4) & 0x7f;
				else
					sp->panning = (BYTE)(panning & 0x7F);
				sp->panning |= sp->panning << 7;

				sp->resonance = 0;
				sp->cutoff_freq = 0;
				sp->reverberation = 0;
				sp->chorusdepth = 0;
				sp->exclusiveClass = 0;
				sp->keyToModEnvHold = 0;
				sp->keyToModEnvDecay = 0;
				sp->keyToVolEnvHold = 0;
				sp->keyToVolEnvDecay = 0;

				if (cfg_tuning)
				{
					double tune_factor = (double)(cfg_tuning) / 1200.0;
					tune_factor = pow(2.0, tune_factor);
					sp->root_freq = (uint32)( tune_factor * (double)sp->root_freq );
				}

				/* envelope, tremolo, and vibrato */
				if (18 != fp->Read(tmp, 18)) goto fail; 

				if (!tmp[13] || !tmp[14])
				{
					sp->tremolo_sweep_increment = 0;
					sp->tremolo_phase_increment = 0;
					sp->tremolo_depth = 0;
					cmsg(CMSG_INFO, VERB_DEBUG, " * no tremolo");
				}
				else
				{
					sp->tremolo_sweep_increment = convert_tremolo_sweep(song, tmp[12]);
					sp->tremolo_phase_increment = convert_tremolo_rate(song, tmp[13]);
					sp->tremolo_depth = tmp[14];
					cmsg(CMSG_INFO, VERB_DEBUG,
						" * tremolo: sweep %d, phase %d, depth %d",
						sp->tremolo_sweep_increment, sp->tremolo_phase_increment,
						sp->tremolo_depth);
				}

				if (!tmp[16] || !tmp[17])
				{
					sp->vibrato_sweep_increment = 0;
					sp->vibrato_control_ratio = 0;
					sp->vibrato_depth = 0;
					cmsg(CMSG_INFO, VERB_DEBUG, " * no vibrato");
				}
				else
				{
					sp->vibrato_control_ratio = convert_vibrato_rate(song, tmp[16]);
					sp->vibrato_sweep_increment= convert_vibrato_sweep(song, tmp[15], sp->vibrato_control_ratio);
					sp->vibrato_depth = tmp[17];
					cmsg(CMSG_INFO, VERB_DEBUG,
						" * vibrato: sweep %d, ctl %d, depth %d",
						sp->vibrato_sweep_increment, sp->vibrato_control_ratio,
						sp->vibrato_depth);

				}

				READ_CHAR(sp->modes);
				READ_SHORT(sp->freq_center);
				READ_SHORT(sp->freq_scale);

				if (sf2flag)
				{
					READ_SHORT(sample_volume);
					READ_CHAR(sf2delay);
					READ_CHAR(sp->exclusiveClass);
					fp->Seek(32, SEEK_CUR);
				}
				else
				{
					fp->Seek(36, SEEK_CUR);
					sample_volume = 0;
					sf2delay = 0;

				}

				/* Mark this as a fixed-pitch instrument if such a deed is desired. */
				if (note_to_use != -1)
					sp->note_to_use = (BYTE)(note_to_use);
				else
					sp->note_to_use = 0;

				/* seashore.pat in the Midia patch set has no Sustain. I don't
				understand why, and fixing it by adding the Sustain flag to
				all looped patches probably breaks something else. We do it
				anyway. */

				if (sp->modes & MODES_LOOPING) 
					sp->modes |= MODES_SUSTAIN;

				/* Strip any loops and envelopes we're permitted to */
				if ((strip_loop == 1) && 
					(sp->modes & (MODES_SUSTAIN | MODES_LOOPING | MODES_PINGPONG | MODES_REVERSE)))
				{
					cmsg(CMSG_INFO, VERB_DEBUG, " - Removing loop and/or sustain");
					sp->modes &=~(MODES_SUSTAIN | MODES_LOOPING | MODES_PINGPONG | MODES_REVERSE);
				}

				if (strip_envelope == 1)
				{
					if (sp->modes & MODES_ENVELOPE)
						cmsg(CMSG_INFO, VERB_DEBUG, " - Removing envelope");
					sp->modes &= ~MODES_ENVELOPE;
				}
				else if (strip_envelope != 0)
				{
					/* Have to make a guess. */
					if (!(sp->modes & (MODES_LOOPING | MODES_PINGPONG | MODES_REVERSE)))
					{
						/* No loop? Then what's there to sustain? No envelope needed either... */
						sp->modes &= ~(MODES_SUSTAIN|MODES_ENVELOPE);
						cmsg(CMSG_INFO, VERB_DEBUG, 
							" - No loop, removing sustain and envelope");
					}
					else if (!memcmp(tmp, "??????", 6) || tmp[11] >= 100) 
					{
						/* Envelope rates all maxed out? Envelope end at a high "offset"?
						That's a weird envelope. Take it out. */
						sp->modes &= ~MODES_ENVELOPE;
						cmsg(CMSG_INFO, VERB_DEBUG, " - Weirdness, removing envelope");
					}
					else if (!(sp->modes & MODES_SUSTAIN))
					{
						/* No sustain? Then no envelope.  I don't know if this is
						justified, but patches without sustain usually don't need the
						envelope either... at least the Gravis ones. They're mostly
						drums.  I think. */
						sp->modes &= ~MODES_ENVELOPE;
						cmsg(CMSG_INFO, VERB_DEBUG, " - No sustain, removing envelope");
					}
				}

				sp->attenuation = 0;

				for (j = ATTACK; j < DELAY; j++)
				{
					sp->envelope_rate[j] = convert_envelope_rate(song, tmp[j]);
					sp->envelope_offset[j] = convert_envelope_offset(tmp[6+j]);
				}
				if (sf2flag)
				{
					if (sf2delay > 5)
					{
						sf2delay = 5;
					}
					sp->envelope_rate[DELAY] = (int)( (sf2delay * song->rate) / 1000 );
				}
				else
				{
					sp->envelope_rate[DELAY] = 0;
				}
				sp->envelope_offset[DELAY] = 0;

				for (j = ATTACK; j < DELAY; j++)
				{
					sp->modulation_rate[j] = float(sp->envelope_rate[j]);
					sp->modulation_offset[j] = float(sp->envelope_offset[j]);
				}
				sp->modulation_rate[DELAY] = sp->modulation_offset[DELAY] = 0;
				sp->modEnvToFilterFc = 0;
				sp->modEnvToPitch = 0;
				sp->lfo_sweep_increment = 0;
				sp->lfo_phase_increment = 0;
				sp->modLfoToFilterFc = 0;

				/* Then read the sample data */
				if (((sp->modes & MODES_16BIT) && sp->data_length/2 > MAX_SAMPLE_SIZE) ||
					(!(sp->modes & MODES_16BIT) && sp->data_length > MAX_SAMPLE_SIZE))
				{
					goto fail;
				}
				sp->data = (sample_t *)safe_malloc(sp->data_length + 1);

				if (sp->data_length != fp->Read(sp->data, sp->data_length))
					goto fail;

				convert_sample_data(sp, sp->data);

				/* Reverse reverse loops and pass them off as normal loops */
				if (sp->modes & MODES_REVERSE)
				{
					int t;
					/* The GUS apparently plays reverse loops by reversing the
					whole sample. We do the same because the GUS does not SUCK. */

					cmsg(CMSG_WARNING, VERB_NORMAL, "Reverse loop in %s", name);
					reverse_data((sample_t *)sp->data, 0, sp->data_length);
					sp->data[sp->data_length] = sp->data[sp->data_length - 1];

					t = sp->loop_start;
					sp->loop_start = sp->data_length - sp->loop_end;
					sp->loop_end = sp->data_length - t;

					sp->modes &= ~MODES_REVERSE;
					sp->modes |= MODES_LOOPING; /* just in case */
				}

				if (amp != -1)
				{
					sp->volume = (amp) / 100.f;
				}
				else if (sf2flag)
				{
					sp->volume = (sample_volume) / 255.f;
				}
				else
				{
#if defined(ADJUST_SAMPLE_VOLUMES)
					/* Try to determine a volume scaling factor for the sample.
					This is a very crude adjustment, but things sound more
					balanced with it. Still, this should be a runtime option. */
					int i, numsamps = sp->data_length;
					sample_t maxamp = 0, a;
					sample_t *tmp;
					for (i = numsamps, tmp = sp->data; i; --i)
					{
						a = abs(*tmp++);
						if (a > maxamp)
							maxamp = a;
					}
					sp->volume = 1 / maxamp;
					cmsg(CMSG_INFO, VERB_DEBUG, " * volume comp: %f", sp->volume);
#else
					sp->volume = 1;
#endif
				}

				/* Then fractional samples */
				sp->data_length <<= FRACTION_BITS;
				sp->loop_start <<= FRACTION_BITS;
				sp->loop_end <<= FRACTION_BITS;

				/* Adjust for fractional loop points. */
				sp->loop_start |= (fractions & 0x0F) << (FRACTION_BITS-4);
				sp->loop_end |= ((fractions>>4) & 0x0F) << (FRACTION_BITS-4);

				/* If this instrument will always be played on the same note,
				and it's not looped, we can resample it now. */
				if (sp->note_to_use && !(sp->modes & MODES_LOOPING))
					pre_resample(song, sp);

				if (strip_tail == 1)
				{
					/* Let's not really, just say we did. */
					cmsg(CMSG_INFO, VERB_DEBUG, " - Stripping tail");
					sp->data_length = sp->loop_end;
				}
			} /* end of sample loop */
		} /* end of stereo layer loop */
	} /* end of vlayer loop */


	delete fp;
	return headlp;
}

void convert_sample_data(Sample *sp, const void *data)
{
	/* convert everything to 32-bit floating point data */
	sample_t *newdata = NULL;

	switch (sp->modes & (MODES_16BIT | MODES_UNSIGNED))
	{
	case 0:
	  {					/* 8-bit, signed */
		SBYTE *cp = (SBYTE *)data;
		newdata = (sample_t *)safe_malloc((sp->data_length + 1) * sizeof(sample_t));
		for (int i = 0; i < sp->data_length; ++i)
		{
			if (cp[i] < 0)
			{
				newdata[i] = float(cp[i]) / 128.f;
			}
			else
			{
				newdata[i] = float(cp[i]) / 127.f;
			}
		}
		break;
	  }

	case MODES_UNSIGNED:
	  {					/* 8-bit, unsigned */
		BYTE *cp = (BYTE *)data;
		newdata = (sample_t *)safe_malloc((sp->data_length + 1) * sizeof(sample_t));
		for (int i = 0; i < sp->data_length; ++i)
		{
			int c = cp[i] - 128;
			if (c < 0)
			{
				newdata[i] = float(c) / 128.f;
			}
			else
			{
				newdata[i] = float(c) / 127.f;
			}
		}
		break;
	  }

	case MODES_16BIT:
	  {					/* 16-bit, signed */
		SWORD *cp = (SWORD *)data;
		/* Convert these to samples */
		sp->data_length >>= 1;
		sp->loop_start >>= 1;
		sp->loop_end >>= 1;
		newdata = (sample_t *)safe_malloc((sp->data_length + 1) * sizeof(sample_t));
		for (int i = 0; i < sp->data_length; ++i)
		{
			int c = LittleShort(cp[i]);
			if (c < 0)
			{
				newdata[i] = float(c) / 32768.f;
			}
			else
			{
				newdata[i] = float(c) / 32767.f;
			}
		}
		break;
	  }

	case MODES_16BIT | MODES_UNSIGNED:
	  {					/* 16-bit, unsigned */
		WORD *cp = (WORD *)data;
		/* Convert these to samples */
		sp->data_length >>= 1;
		sp->loop_start >>= 1;
		sp->loop_end >>= 1;
		newdata = (sample_t *)safe_malloc((sp->data_length + 1) * sizeof(sample_t));
		for (int i = 0; i < sp->data_length; ++i)
		{
			int c = LittleShort(cp[i]) - 32768;
			if (c < 0)
			{
				newdata[i] = float(c) / 32768.f;
			}
			else
			{
				newdata[i] = float(c) / 32767.f;
			}
		}
		break;
	  }
	}
	/* Duplicate the final sample for linear interpolation. */
	newdata[sp->data_length] = newdata[sp->data_length - 1];
	if (sp->data != NULL)
	{
		free(sp->data);
	}
	sp->data = newdata;
}

static int fill_bank(Renderer *song, int dr, int b)
{
	int i, errors = 0;
	ToneBank *bank = ((dr) ? drumset[b] : tonebank[b]);
	if (bank == NULL)
	{
		cmsg(CMSG_ERROR, VERB_NORMAL, 
			"Huh. Tried to load instruments in non-existent %s %d",
			(dr) ? "drumset" : "tone bank", b);
		return 0;
	}
	for (i = 0; i < MAXPROG; i++)
	{
		if (bank->tone[i].layer == MAGIC_LOAD_INSTRUMENT)
		{
			bank->tone[i].layer = load_instrument_dls(song, dr, b, i);
			if (bank->tone[i].layer != NULL)
			{
				continue;
			}
			if (bank->tone[i].name.IsEmpty())
			{
				cmsg(CMSG_WARNING, (b!=0) ? VERB_VERBOSE : VERB_NORMAL,
					"No instrument mapped to %s %d, program %d%s",
					(dr)? "drum set" : "tone bank", b, i, 
					(b!=0) ? "" : " - this instrument will not be heard");
				if (b!=0)
				{
					/* Mark the corresponding instrument in the default
					bank / drumset for loading (if it isn't already) */
					if (!dr)
					{
						if (!(standard_tonebank.tone[i].layer))
							standard_tonebank.tone[i].layer=
							MAGIC_LOAD_INSTRUMENT;
					}
					else
					{
						if (!(standard_drumset.tone[i].layer))
							standard_drumset.tone[i].layer=
							MAGIC_LOAD_INSTRUMENT;
					}
				}
				bank->tone[i].layer=0;
				errors++;
			}
			else if (!(bank->tone[i].layer=
				load_instrument(song, bank->tone[i].name, 
				bank->tone[i].font_type,
				(dr) ? 1 : 0,
				bank->tone[i].pan,
				bank->tone[i].amp,
				bank->tone[i].tuning,
				(bank->tone[i].note!=-1) ? 
				bank->tone[i].note :
			((dr) ? i : -1),
				(bank->tone[i].strip_loop!=-1) ?
				bank->tone[i].strip_loop :
			((dr) ? 1 : -1),
				(bank->tone[i].strip_envelope != -1) ? 
				bank->tone[i].strip_envelope :
			((dr) ? 1 : -1),
				bank->tone[i].strip_tail,
				b,
				((dr) ? i + 128 : i),
				bank->tone[i].sf_ix
				)))
			{
				cmsg(CMSG_ERROR, VERB_NORMAL, 
					"Couldn't load instrument %s (%s %d, program %d)",
					bank->tone[i].name.GetChars(),
					(dr)? "drum set" : "tone bank", b, i);
				errors++;
			}
		}
	}
	return errors;
}

int Renderer::load_missing_instruments()
{
	int i = MAXBANK, errors = 0;
	while (i--)
	{
		if (tonebank[i] != NULL)
			errors += fill_bank(this, 0,i);
		if (drumset[i] != NULL)
			errors += fill_bank(this, 1,i);
	}
	return errors;
}

void free_instruments()
{
	int i = 128;
	while (i--)
	{
		if (tonebank[i] != NULL)
			free_bank(0,i);
		if (drumset[i] != NULL)
			free_bank(1,i);
	}
}

int Renderer::set_default_instrument(const char *name)
{
	InstrumentLayer *lp;
	if (!(lp = load_instrument(this, name, FONT_NORMAL, 0, -1, -1, 0, -1, -1, -1, -1, 0, -1, -1)))
		return -1;
	if (default_instrument)
		free_layer(default_instrument);
	default_instrument = lp;
	default_program = SPECIAL_PROGRAM;
	return 0;
}

}
