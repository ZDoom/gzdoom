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

	mix.c

*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "timidity.h"
#include "templates.h"

namespace Timidity
{

/* Returns 1 if envelope runs out */
int recompute_envelope(Voice *v)
{
	int stage;

	stage = v->envelope_stage;

	if (stage > RELEASEC)
	{
		/* Envelope ran out. */
		v->status = VOICE_FREE;
		return 1;
	}

	if (v->sample->modes & PATCH_NO_SRELEASE)
	{
		if (v->status == VOICE_ON || v->status == VOICE_SUSTAINED)
		{
			if (stage > DECAY)
			{
				/* Freeze envelope until note turns off. Trumpets want this. */
				v->envelope_increment = 0;
				return 0;
			}
		}
	}
	v->envelope_stage = stage + 1;

	if (v->envelope_volume == v->sample->envelope_offset[stage])
	{
		return recompute_envelope(v);
	}
	v->envelope_target = v->sample->envelope_offset[stage];
	v->envelope_increment = v->sample->envelope_rate[stage];
	if (v->envelope_target < v->envelope_volume)
		v->envelope_increment = -v->envelope_increment;
	return 0;
}

void apply_envelope_to_amp(Voice *v)
{
	float env_vol = v->attenuation;
	float final_amp = v->sample->volume * FINAL_MIX_SCALE;
	if (v->tremolo_phase_increment != 0)
	{
		env_vol *= v->tremolo_volume;
	}
	if (v->sample->modes & PATCH_NO_SRELEASE)
	{
		env_vol *= v->envelope_volume / float(1 << 30);
	}
	// Note: The pan offsets are negative.
	v->left_mix = MAX(0.f, (float)calc_gf1_amp(env_vol + v->left_offset) * final_amp);
	v->right_mix = MAX(0.f, (float)calc_gf1_amp(env_vol + v->right_offset) * final_amp);
}

static int update_envelope(Voice *v)
{
	v->envelope_volume += v->envelope_increment;
	if (((v->envelope_increment < 0) && (v->envelope_volume <= v->envelope_target)) ||
		((v->envelope_increment > 0) && (v->envelope_volume >= v->envelope_target)))
	{
		v->envelope_volume = v->envelope_target;
		if (recompute_envelope(v))
		{
			return 1;
		}
	}
	return 0;
}

static void update_tremolo(Voice *v)
{
	int depth = v->sample->tremolo_depth << 7;

	if (v->tremolo_sweep != 0)
	{
		/* Update sweep position */

		v->tremolo_sweep_position += v->tremolo_sweep;
		if (v->tremolo_sweep_position >= (1 << SWEEP_SHIFT))
		{
			/* Swept to max amplitude */
			v->tremolo_sweep = 0;
		}
		else
		{
			/* Need to adjust depth */
			depth *= v->tremolo_sweep_position;
			depth >>= SWEEP_SHIFT;
		}
	}

	v->tremolo_phase += v->tremolo_phase_increment;

	v->tremolo_volume = (float)
		(1.0 - FSCALENEG((sine(v->tremolo_phase >> RATE_SHIFT) + 1.0)
		* depth * TREMOLO_AMPLITUDE_TUNING,
		17));

	/* I'm not sure about the +1.0 there -- it makes tremoloed voices'
	volumes on average the lower the higher the tremolo amplitude. */
}

/* Returns 1 if the note died */
static int update_signal(Voice *v)
{
	if (v->envelope_increment != 0 && update_envelope(v))
	{
		return 1;
	}
	if (v->tremolo_phase_increment != 0)
	{
		update_tremolo(v);
	}
	apply_envelope_to_amp(v);
	return 0;
}

static void mix_mystery_signal(SDWORD control_ratio, const sample_t *sp, float *lp, Voice *v, int count)
{
	final_volume_t 
		left = v->left_mix, 
		right = v->right_mix;
	int cc;
	sample_t s;

	if (!(cc = v->control_counter))
	{
		cc = control_ratio;
		if (update_signal(v))
			return;	/* Envelope ran out */

		left = v->left_mix;
		right = v->right_mix;
	}

	while (count)
	{
		if (cc < count)
		{
			count -= cc;
			while (cc--)
			{
				s = *sp++;
				lp[0] += left * s;
				lp[1] += right * s;
				lp += 2;
			}
			cc = control_ratio;
			if (update_signal(v))
				return;	/* Envelope ran out */
			left = v->left_mix;
			right = v->right_mix;
		}
		else
		{
			v->control_counter = cc - count;
			while (count--)
			{
				s = *sp++;
				lp[0] += left * s;
				lp[1] += right * s;
				lp += 2;
			}
			return;
		}
	}
}

static void mix_single_signal(SDWORD control_ratio, const sample_t *sp, float *lp, Voice *v, float *ampat, int count)
{
	final_volume_t amp;
	int cc;

	if (0 == (cc = v->control_counter))
	{
		cc = control_ratio;
		if (update_signal(v))
			return;		/* Envelope ran out */
	}
	amp = *ampat;

	while (count)
	{
		if (cc < count)
		{
			count -= cc;
			while (cc--)
			{
				lp[0] += *sp++ * amp;
				lp += 2;
			}
			cc = control_ratio;
			if (update_signal(v))
				return;	/* Envelope ran out */
			amp = *ampat;
		}
		else
		{
			v->control_counter = cc - count;
			while (count--)
			{
				lp[0] += *sp++ * amp;
				lp += 2;
			}
			return;
		}
	}
}

static void mix_single_left_signal(SDWORD control_ratio, const sample_t *sp, float *lp, Voice *v, int count)
{
	mix_single_signal(control_ratio, sp, lp, v, &v->left_mix, count);
}

static void mix_single_right_signal(SDWORD control_ratio, const sample_t *sp, float *lp, Voice *v, int count)
{
	mix_single_signal(control_ratio, sp, lp + 1, v, &v->right_mix, count);
}

static void mix_mono_signal(SDWORD control_ratio, const sample_t *sp, float *lp, Voice *v, int count)
{
	final_volume_t 
		left = v->left_mix;
	int cc;

	if (!(cc = v->control_counter))
	{
		cc = control_ratio;
		if (update_signal(v))
			return;	/* Envelope ran out */
		left = v->left_mix;
	}

	while (count)
	{
		if (cc < count)
		{
			count -= cc;
			while (cc--)
			{
				*lp++ += *sp++ * left;
			}
			cc = control_ratio;
			if (update_signal(v))
				return;	/* Envelope ran out */
			left = v->left_mix;
		}
		else
		{
			v->control_counter = cc - count;
			while (count--)
			{
				*lp++ += *sp++ * left;
			}
			return;
		}
	}
}

static void mix_mystery(SDWORD control_ratio, const sample_t *sp, float *lp, Voice *v, int count)
{
	final_volume_t 
		left = v->left_mix, 
		right = v->right_mix;
	sample_t s;

	while (count--)
	{
		s = *sp++;
		lp[0] += s * left;
		lp[1] += s * right;
		lp += 2;
	}
}

static void mix_single(const sample_t *sp, float *lp, final_volume_t amp, int count)
{
	while (count--)
	{
		lp[0] += *sp++ * amp;
		lp += 2;
	}
}

static void mix_single_left(const sample_t *sp, float *lp, Voice *v, int count)
{
	mix_single(sp, lp, v->left_mix, count);
}
static void mix_single_right(const sample_t *sp, float *lp, Voice *v, int count)
{
	mix_single(sp, lp + 1, v->right_mix, count);
}

static void mix_mono(const sample_t *sp, float *lp, Voice *v, int count)
{
	final_volume_t 
		left = v->left_mix;

	while (count--)
	{
		*lp++ += *sp++ * left;
	}
}

/* Ramp a note out in c samples */
static void ramp_out(const sample_t *sp, float *lp, Voice *v, int c)
{
	final_volume_t left, right, li, ri;

	sample_t s = 0; /* silly warning about uninitialized s */

	/* Fix by James Caldwell */
	if ( c == 0 ) c = 1;

	/* printf("Ramping out: left=%d, c=%d, li=%d\n", left, c, li); */

	if (v->left_offset == 0)		// All the way to the left
	{
		left = v->left_mix;
		li = -(left/c);
		if (li == 0) li = -1;

		while (c--)
		{
			left += li;
			if (left < 0)
				return;
			lp[0] += *sp++ * left;
			lp += 2;
		}
	}
	else if (v->right_offset == 0)	// All the way to the right
	{
		right = v->right_mix;
		ri = -(right/c);
		if (ri == 0) ri = -1;

		while (c--)
		{
			right += ri;
			if (right < 0)
				return;
			s = *sp++;
			lp[1] += *sp++ * right;
			lp += 2;
		}
	}
	else							// Somewhere in the middle
	{
		left = v->left_mix;
		li = -(left/c);
		if (li == 0) li = -1;
		right = v->right_mix;
		ri = -(right/c);
		if (ri == 0) ri = -1;

		right = v->right_mix;
		ri = -(right/c);
		while (c--)
		{
			left += li;
			right += ri;
			if (left < 0)
			{
				if (right < 0)
				{
					return;
				}
				left = 0;
			}
			else if (right < 0)
			{
				right = 0;
			}
			s = *sp++;
			lp[0] += s * left;
			lp[1] += s * right;
			lp += 2;
		}
	}
}


/**************** interface function ******************/

void mix_voice(Renderer *song, float *buf, Voice *v, int c)
{
	int count = c;
	sample_t *sp;
	if (c < 0)
	{
		return;
	}
	if (v->status == VOICE_DIE)
	{
		if (count >= MAX_DIE_TIME)
			count = MAX_DIE_TIME;
		sp = resample_voice(song, v, &count);
		ramp_out(sp, buf, v, count);
		v->status = VOICE_FREE;
	}
	else
	{
		sp = resample_voice(song, v, &count);
		if (count < 0)
		{
			return;
		}
		if (v->right_mix == 0)			// All the way to the left
		{
			if (v->envelope_increment != 0 || v->tremolo_phase_increment != 0)
			{
				mix_single_left_signal(song->control_ratio, sp, buf, v, count);
			}
			else
			{
				mix_single_left(sp, buf, v, count);
			}
		}
		else if (v->left_mix == 0)		// All the way to the right
		{
			if (v->envelope_increment != 0 || v->tremolo_phase_increment != 0)
			{
				mix_single_right_signal(song->control_ratio, sp, buf, v, count);
			}
			else
			{
				mix_single_right(sp, buf, v, count);
			}
		}
		else							// Somewhere in the middle
		{
			if (v->envelope_increment || v->tremolo_phase_increment)
			{
				mix_mystery_signal(song->control_ratio, sp, buf, v, count);
			}
			else
			{
				mix_mystery(song->control_ratio, sp, buf, v, count);
			}
		}
	}
}

}
