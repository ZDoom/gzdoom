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

	playmidi.c -- random stuff in need of rearrangement

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "timidity.h"

namespace Timidity
{

static bool opt_stereo_surround = false;
bool adjust_panning_immediately = false;

/*	$Id: tables.c 1404 2004-08-21 12:27:02Z slouken $   Greg Lee */
const BYTE xmap[XMAPMAX][5] =
{
	{ SFXBANK, 0, 0, 120, 0 },
	{ SFXBANK, 0, 1, 120, 1 },
	{ SFXBANK, 0, 2, 120, 2 },
	{ SFXBANK, 0, 3, 120, 3 },
	{ SFXBANK, 0, 4, 120, 4 },
	{ SFXBANK, 0, 5, 120, 5 },
	{ SFXBANK, 0, 16, 120, 16 },
	{ SFXBANK, 0, 32, 120, 32 },
	{ SFXBANK, 0, 33, 120, 33 },
	{ SFXBANK, 0, 34, 120, 34 },
	{ SFXBANK, 0, 35, 120, 35 },
	{ SFXBANK, 0, 36, 120, 36 },
	{ SFXBANK, 0, 48, 120, 48 },
	{ SFXBANK, 0, 49, 120, 49 },
	{ SFXBANK, 0, 50, 120, 50 },
	{ SFXBANK, 0, 51, 120, 51 },
	{ SFXBANK, 0, 52, 120, 52 },
	{ SFXBANK, 0, 54, 120, 54 },
	{ SFXBANK, 0, 55, 120, 55 },
	{ SFXBANK, 0, 64, 120, 64 },
	{ SFXBANK, 0, 65, 120, 65 },
	{ SFXBANK, 0, 66, 120, 66 },
	{ SFXBANK, 0, 67, 120, 67 },
	{ SFXBANK, 0, 68, 120, 68 },
	{ SFXBANK, 0, 69, 120, 69 },
	{ SFXBANK, 0, 70, 120, 70 },
	{ SFXBANK, 0, 80, 120, 80 },
	{ SFXBANK, 0, 81, 120, 81 },
	{ SFXBANK, 0, 82, 120, 82 },
	{ SFXBANK, 0, 83, 120, 83 },
	{ SFXBANK, 0, 84, 120, 84 },
	{ SFXBANK, 0, 85, 120, 85 },
	{ SFXBANK, 0, 86, 120, 86 },
	{ SFXBANK, 0, 87, 120, 87 },
	{ SFXBANK, 0, 88, 120, 88 },
	{ SFXBANK, 0, 89, 120, 89 },
	{ SFXBANK, 0, 90, 120, 90 },
	{ SFXBANK, 0, 96, 120, 96 },
	{ SFXBANK, 0, 97, 120, 97 },
	{ SFXBANK, 0, 98, 120, 98 },
	{ SFXBANK, 0, 99, 120, 99 },
	{ SFXBANK, 0, 100, 120, 100 },
	{ SFXBANK, 0, 101, 120, 101 },
	{ SFXBANK, 0, 112, 120, 112 },
	{ SFXBANK, 0, 113, 120, 113 },
	{ SFXBANK, 0, 114, 120, 114 },
	{ SFXBANK, 0, 115, 120, 115 },
	{ SFXDRUM1, 0, 36, 121, 36 },
	{ SFXDRUM1, 0, 37, 121, 37 },
	{ SFXDRUM1, 0, 38, 121, 38 },
	{ SFXDRUM1, 0, 39, 121, 39 },
	{ SFXDRUM1, 0, 40, 121, 40 },
	{ SFXDRUM1, 0, 41, 121, 41 },
	{ SFXDRUM1, 0, 52, 121, 52 },
	{ SFXDRUM1, 0, 68, 121, 68 },
	{ SFXDRUM1, 0, 69, 121, 69 },
	{ SFXDRUM1, 0, 70, 121, 70 },
	{ SFXDRUM1, 0, 71, 121, 71 },
	{ SFXDRUM1, 0, 72, 121, 72 },
	{ SFXDRUM1, 0, 84, 121, 84 },
	{ SFXDRUM1, 0, 85, 121, 85 },
	{ SFXDRUM1, 0, 86, 121, 86 },
	{ SFXDRUM1, 0, 87, 121, 87 },
	{ SFXDRUM1, 0, 88, 121, 88 },
	{ SFXDRUM1, 0, 90, 121, 90 },
	{ SFXDRUM1, 0, 91, 121, 91 },
	{ SFXDRUM1, 1, 36, 122, 36 },
	{ SFXDRUM1, 1, 37, 122, 37 },
	{ SFXDRUM1, 1, 38, 122, 38 },
	{ SFXDRUM1, 1, 39, 122, 39 },
	{ SFXDRUM1, 1, 40, 122, 40 },
	{ SFXDRUM1, 1, 41, 122, 41 },
	{ SFXDRUM1, 1, 42, 122, 42 },
	{ SFXDRUM1, 1, 52, 122, 52 },
	{ SFXDRUM1, 1, 53, 122, 53 },
	{ SFXDRUM1, 1, 54, 122, 54 },
	{ SFXDRUM1, 1, 55, 122, 55 },
	{ SFXDRUM1, 1, 56, 122, 56 },
	{ SFXDRUM1, 1, 57, 122, 57 },
	{ SFXDRUM1, 1, 58, 122, 58 },
	{ SFXDRUM1, 1, 59, 122, 59 },
	{ SFXDRUM1, 1, 60, 122, 60 },
	{ SFXDRUM1, 1, 61, 122, 61 },
	{ SFXDRUM1, 1, 62, 122, 62 },
	{ SFXDRUM1, 1, 68, 122, 68 },
	{ SFXDRUM1, 1, 69, 122, 69 },
	{ SFXDRUM1, 1, 70, 122, 70 },
	{ SFXDRUM1, 1, 71, 122, 71 },
	{ SFXDRUM1, 1, 72, 122, 72 },
	{ SFXDRUM1, 1, 73, 122, 73 },
	{ SFXDRUM1, 1, 84, 122, 84 },
	{ SFXDRUM1, 1, 85, 122, 85 },
	{ SFXDRUM1, 1, 86, 122, 86 },
	{ SFXDRUM1, 1, 87, 122, 87 },
	{ XGDRUM, 0, 25, 40, 38 },
	{ XGDRUM, 0, 26, 40, 40 },
	{ XGDRUM, 0, 27, 40, 39 },
	{ XGDRUM, 0, 28, 40, 30 },
	{ XGDRUM, 0, 29, 0, 25 },
	{ XGDRUM, 0, 30, 0, 85 },
	{ XGDRUM, 0, 31, 0, 38 },
	{ XGDRUM, 0, 32, 0, 37 },
	{ XGDRUM, 0, 33, 0, 36 },
	{ XGDRUM, 0, 34, 0, 38 },
	{ XGDRUM, 0, 62, 0, 101 },
	{ XGDRUM, 0, 63, 0, 102 },
	{ XGDRUM, 0, 64, 0, 103 },
	{ XGDRUM, 8, 25, 40, 38 },
	{ XGDRUM, 8, 26, 40, 40 },
	{ XGDRUM, 8, 27, 40, 39 },
	{ XGDRUM, 8, 28, 40, 40 },
	{ XGDRUM, 8, 29, 8, 25 },
	{ XGDRUM, 8, 30, 8, 85 },
	{ XGDRUM, 8, 31, 8, 38 },
	{ XGDRUM, 8, 32, 8, 37 },
	{ XGDRUM, 8, 33, 8, 36 },
	{ XGDRUM, 8, 34, 8, 38 },
	{ XGDRUM, 8, 62, 8, 101 },
	{ XGDRUM, 8, 63, 8, 102 },
	{ XGDRUM, 8, 64, 8, 103 },
	{ XGDRUM, 16, 25, 40, 38 },
	{ XGDRUM, 16, 26, 40, 40 },
	{ XGDRUM, 16, 27, 40, 39 },
	{ XGDRUM, 16, 28, 40, 40 },
	{ XGDRUM, 16, 29, 16, 25 },
	{ XGDRUM, 16, 30, 16, 85 },
	{ XGDRUM, 16, 31, 16, 38 },
	{ XGDRUM, 16, 32, 16, 37 },
	{ XGDRUM, 16, 33, 16, 36 },
	{ XGDRUM, 16, 34, 16, 38 },
	{ XGDRUM, 16, 62, 16, 101 },
	{ XGDRUM, 16, 63, 16, 102 },
	{ XGDRUM, 16, 64, 16, 103 },
	{ XGDRUM, 24, 25, 40, 38 },
	{ XGDRUM, 24, 26, 40, 40 },
	{ XGDRUM, 24, 27, 40, 39 },
	{ XGDRUM, 24, 28, 24, 100 },
	{ XGDRUM, 24, 29, 24, 25 },
	{ XGDRUM, 24, 30, 24, 15 },
	{ XGDRUM, 24, 31, 24, 38 },
	{ XGDRUM, 24, 32, 24, 37 },
	{ XGDRUM, 24, 33, 24, 36 },
	{ XGDRUM, 24, 34, 24, 38 },
	{ XGDRUM, 24, 62, 24, 101 },
	{ XGDRUM, 24, 63, 24, 102 },
	{ XGDRUM, 24, 64, 24, 103 },
	{ XGDRUM, 24, 78, 0, 17 },
	{ XGDRUM, 24, 79, 0, 18 },
	{ XGDRUM, 25, 25, 40, 38 },
	{ XGDRUM, 25, 26, 40, 40 },
	{ XGDRUM, 25, 27, 40, 39 },
	{ XGDRUM, 25, 28, 25, 100 },
	{ XGDRUM, 25, 29, 25, 25 },
	{ XGDRUM, 25, 30, 25, 15 },
	{ XGDRUM, 25, 31, 25, 38 },
	{ XGDRUM, 25, 32, 25, 37 },
	{ XGDRUM, 25, 33, 25, 36 },
	{ XGDRUM, 25, 34, 25, 38 },
	{ XGDRUM, 25, 78, 0, 17 },
	{ XGDRUM, 25, 79, 0, 18 },
	{ XGDRUM, 32, 25, 40, 38 },
	{ XGDRUM, 32, 26, 40, 40 },
	{ XGDRUM, 32, 27, 40, 39 },
	{ XGDRUM, 32, 28, 40, 40 },
	{ XGDRUM, 32, 29, 32, 25 },
	{ XGDRUM, 32, 30, 32, 85 },
	{ XGDRUM, 32, 31, 32, 38 },
	{ XGDRUM, 32, 32, 32, 37 },
	{ XGDRUM, 32, 33, 32, 36 },
	{ XGDRUM, 32, 34, 32, 38 },
	{ XGDRUM, 32, 62, 32, 101 },
	{ XGDRUM, 32, 63, 32, 102 },
	{ XGDRUM, 32, 64, 32, 103 },
	{ XGDRUM, 40, 25, 40, 38 },
	{ XGDRUM, 40, 26, 40, 40 },
	{ XGDRUM, 40, 27, 40, 39 },
	{ XGDRUM, 40, 28, 40, 40 },
	{ XGDRUM, 40, 29, 40, 25 },
	{ XGDRUM, 40, 30, 40, 85 },
	{ XGDRUM, 40, 31, 40, 39 },
	{ XGDRUM, 40, 32, 40, 37 },
	{ XGDRUM, 40, 33, 40, 36 },
	{ XGDRUM, 40, 34, 40, 38 },
	{ XGDRUM, 40, 38, 40, 39 },
	{ XGDRUM, 40, 39, 0, 39 },
	{ XGDRUM, 40, 40, 40, 38 },
	{ XGDRUM, 40, 42, 0, 42 },
	{ XGDRUM, 40, 46, 0, 46 },
	{ XGDRUM, 40, 62, 40, 101 },
	{ XGDRUM, 40, 63, 40, 102 },
	{ XGDRUM, 40, 64, 40, 103 },
	{ XGDRUM, 40, 87, 40, 87 }
};

void Renderer::reset_voices()
{
	for (int i = 0; i < MAX_VOICES; i++)
		voice[i].status = VOICE_FREE;
}

/* Process the Reset All Controllers event */
void Renderer::reset_controllers(int c)
{
	channel[c].volume = (100 << 7) | 100;
	channel[c].expression = 0x3fff;
	channel[c].sustain = 0;
	channel[c].pitchbend = 0x2000;
	channel[c].pitchfactor = 0; /* to be computed */
	channel[c].mono = 0;
	channel[c].rpn = RPN_RESET;
	channel[c].nrpn = RPN_RESET;

	channel[c].reverberation = 0;
	channel[c].chorusdepth = 0;
}

void Renderer::reset_midi()
{
	for (int i = 0; i < MAXCHAN; i++)
	{
		reset_controllers(i);
		/* The rest of these are unaffected by the Reset All Controllers event */
		channel[i].program = default_program;
		channel[i].panning = NO_PANNING;
		channel[i].pitchsens = 200;
		channel[i].bank = 0; /* tone bank or drum set */
		channel[i].harmoniccontent = 64;
		channel[i].releasetime = 64;
		channel[i].attacktime = 64;
		channel[i].brightness = 64;
		channel[i].sfx = 0;
	}
	reset_voices();
}

void Renderer::select_sample(int v, Instrument *ip, int vel)
{
	float f, cdiff, diff, midfreq;
	int s, i;
	Sample *sp, *closest;

	s = ip->samples;
	sp = ip->sample;

	if (s == 1)
	{
		voice[v].sample = sp;
		return;
	}

	f = voice[v].orig_frequency;
	for (i = 0; i < s; i++)
	{
		if (sp->low_vel <= vel && sp->high_vel >= vel &&
			sp->low_freq <= f && sp->high_freq >= f)
		{
			voice[v].sample = sp;
			return;
		}
		sp++;
	}

	/* 
	No suitable sample found! We'll select the sample whose root
	frequency is closest to the one we want. (Actually we should
	probably convert the low, high, and root frequencies to MIDI note
	values and compare those.) */

	cdiff = 1e10;
	closest = sp = ip->sample;
	midfreq = float(sp->low_freq + sp->high_freq) / 2;
	for (i = 0; i < s; i++)
	{
		diff = sp->root_freq - f;
		/*  But the root freq. can perfectly well lie outside the keyrange
		*  frequencies, so let's try:
		*/
		/* diff = midfreq - f; */
		if (diff < 0) diff = -diff;
		if (diff < cdiff)
		{
			cdiff = diff;
			closest = sp;
		}
		sp++;
	}
	voice[v].sample = closest;
	return;
}



void Renderer::select_stereo_samples(int v, InstrumentLayer *lp, int vel)
{
	Instrument *ip;
	InstrumentLayer *nlp, *bestvel;
	int diffvel, midvel, mindiff;

	/* select closest velocity */
	bestvel = lp;
	mindiff = 500;
	for (nlp = lp; nlp; nlp = nlp->next)
	{
		midvel = (nlp->hi + nlp->lo)/2;
		if (!midvel)
		{
			diffvel = 127;
		}
		else if (voice[v].velocity < nlp->lo || voice[v].velocity > nlp->hi)
		{
			diffvel = 200;
		}
		else
		{
			diffvel = voice[v].velocity - midvel;
		}
		if (diffvel < 0)
		{
			diffvel = -diffvel;
		}
		if (diffvel < mindiff)
		{
			mindiff = diffvel;
			bestvel = nlp;
		}
	}
	ip = bestvel->instrument;

	if (ip->right_sample)
	{
		ip->sample = ip->right_sample;
		ip->samples = ip->right_samples;
		select_sample(v, ip, vel);
		voice[v].right_sample = voice[v].sample;
	}
	else
	{
		voice[v].right_sample = NULL;
	}
	ip->sample = ip->left_sample;
	ip->samples = ip->left_samples;
	select_sample(v, ip, vel);
}


void Renderer::recompute_freq(int v)
{
	Channel *ch = &channel[voice[v].channel];
	int 
		sign = (voice[v].sample_increment < 0), /* for bidirectional loops */
		pb = ch->pitchbend;
	double a;

	if (!voice[v].sample->sample_rate)
		return;

	if (voice[v].vibrato_control_ratio)
	{
		/* This instrument has vibrato. Invalidate any precomputed
		sample_increments. */

		int i = VIBRATO_SAMPLE_INCREMENTS;
		while (i--)
			voice[v].vibrato_sample_increment[i]=0;
	}

	if (pb == 0x2000 || pb < 0 || pb > 0x3FFF)
	{
		voice[v].frequency = voice[v].orig_frequency;
	}
	else
	{
		pb -= 0x2000;
		if (ch->pitchfactor == 0)
		{
			/* Damn. Somebody bent the pitch. */
			ch->pitchfactor = pow(2.f, ((abs(pb) * ch->pitchsens) / (8191.f * 1200.f)));
		}
		if (pb < 0)
		{
			voice[v].frequency = voice[v].orig_frequency / ch->pitchfactor;
		}
		else
		{
			voice[v].frequency = voice[v].orig_frequency * ch->pitchfactor;
		}
	}

	a = FSCALE(((double)(voice[v].sample->sample_rate) * voice[v].frequency) /
		((double)(voice[v].sample->root_freq) * rate),
		FRACTION_BITS);

	if (sign) 
		a = -a; /* need to preserve the loop direction */

	voice[v].sample_increment = (int)(a);
}

void Renderer::recompute_amp(Voice *v)
{
	double tempamp;
	int chan = v->channel;
	int panning = v->panning;
	double vol = channel[chan].volume / 16383.f;
	double expr = calc_vol(channel[chan].expression / 16383.f);
	double vel = calc_vol(v->velocity / 127.f);

	if (channel[chan].kit)
	{
		int note = v->sample->note_to_use;
		if (note > 0 && drumvolume[chan][note] >= 0) { vol = drumvolume[chan][note] / 127.f; }
		if (note > 0 && drumpanpot[chan][note] >= 0) { panning = drumvolume[chan][note]; panning |= panning << 7; }
	}

	vol = calc_vol(vol);
	tempamp = vel * vol * expr * v->sample->volume;

	if (panning > (60 << 7) && panning < (68 << 7))
	{
		v->panned = PANNED_CENTER;
		v->left_amp = (float)(tempamp * 0.70710678118654752440084436210485);	// * sqrt(0.5)
	}
	else if (panning < (5 << 7))
	{
		v->panned = PANNED_LEFT;
		v->left_amp = (float)tempamp;
	}
	else if (panning > (123 << 7))
	{
		v->panned = PANNED_RIGHT;
		v->left_amp = (float)tempamp; /* left_amp will be used */
	}
	else
	{
		double pan = panning / 16384.0;
		v->panned = PANNED_MYSTERY;
		v->left_amp = (float)(tempamp * sqrt(1.0 - pan));
		v->right_amp = (float)(tempamp * sqrt(pan));
	}
}


#define NOT_CLONE 0
#define STEREO_CLONE 1
#define REVERB_CLONE 2
#define CHORUS_CLONE 3


/* just a variant of note_on() */
int Renderer::vc_alloc(int j)
{
	int i = voices; 

	while (i--)
	{
		if (i != j && (voice[i].status == VOICE_FREE))
		{
			return i;
		}
	}
	return -1;
}

void Renderer::kill_others(int i)
{
	int j = voices; 

	if (!voice[i].sample->exclusiveClass) return;

	while (j--)
	{
		if (voice[j].status == VOICE_FREE ||
			voice[j].status == VOICE_OFF ||
			voice[j].status == VOICE_DIE) continue;
		if (i == j) continue;
		if (voice[i].channel != voice[j].channel) continue;
		if (voice[j].sample->note_to_use)
		{
			if (voice[j].sample->exclusiveClass != voice[i].sample->exclusiveClass) continue;
			kill_note(j);
		}
	}
}


void Renderer::clone_voice(Instrument *ip, int v, int note, int vel, int clone_type, int variationbank)
{
	int w, played_note, chorus = 0, reverb = 0, milli;
	int chan = voice[v].channel;

	// [RH] Don't do the other clones.
	if (clone_type != STEREO_CLONE)
	{
		return;
	}

	if (clone_type == STEREO_CLONE)
	{
		if (!voice[v].right_sample && variationbank != 3) return;
		if (variationbank == 6) return;
	}

	if (channel[chan].kit)
	{
		reverb = drumreverberation[chan][voice[v].note];
		chorus = drumchorusdepth[chan][voice[v].note];
	}
	else
	{
		reverb = channel[chan].reverberation;
		chorus = channel[chan].chorusdepth;
	}

	if (clone_type == REVERB_CLONE) chorus = 0;
	else if (clone_type == CHORUS_CLONE) reverb = 0;
	else if (clone_type == STEREO_CLONE) reverb = chorus = 0;

	if (reverb > 127) reverb = 127;
	if (chorus > 127) chorus = 127;

	if (clone_type == CHORUS_CLONE)
	{
		if (variationbank == 32) chorus = 30;
		else if (variationbank == 33) chorus = 60;
		else if (variationbank == 34) chorus = 90;
	}

	chorus /= 2;  /* This is an ad hoc adjustment. */

	if (!reverb && !chorus && clone_type != STEREO_CLONE) return;

	if ( (w = vc_alloc(v)) < 0 ) return;

	voice[w] = voice[v];
	if (clone_type == STEREO_CLONE) voice[v].clone_voice = w;
	voice[w].clone_voice = v;
	voice[w].clone_type = clone_type;
	voice[w].velocity = vel;

	milli = int(rate / 1000);

	if (clone_type == STEREO_CLONE)
	{
		int left, right, leftpan, rightpan;
		int panrequest = voice[v].panning;
		voice[w].sample = voice[v].right_sample;
		if (variationbank == 3)
		{
			voice[v].panning = 0;
			voice[w].panning = 127;
		}
		else
		{
			if (voice[v].sample->panning > voice[w].sample->panning)
			{
				left = w;
				right = v;
			}
			else
			{
				left = v;
				right = w;
			}
#define INSTRUMENT_SEPARATION 12
			leftpan = panrequest - INSTRUMENT_SEPARATION / 2;
			rightpan = leftpan + INSTRUMENT_SEPARATION;
			if (leftpan < 0)
			{
				leftpan = 0;
				rightpan = leftpan + INSTRUMENT_SEPARATION;
			}
			if (rightpan > 127)
			{
				rightpan = 127;
				leftpan = rightpan - INSTRUMENT_SEPARATION;
			}
			voice[left].panning = leftpan;
			voice[right].panning = rightpan;
			voice[right].echo_delay = 20 * milli;
		}
	}

	voice[w].volume = voice[w].sample->volume;

	if (reverb)
	{
		if (opt_stereo_surround)
		{
			if (voice[w].panning > 64)
				voice[w].panning = 127;
			else
				voice[w].panning = 0;
		}
		else
		{
			if (voice[v].panning < 64)
				voice[w].panning = 64 + reverb/2;
			else
				voice[w].panning = 64 - reverb/2;
		}

		/* try 98->99 for melodic instruments ? (bit much for percussion) */
		/* voice[w].volume *= calc_vol(((127 - reverb) / 8 + 98) / 127.f); */
		voice[w].volume = float(voice[w].volume * calc_vol((911 - reverb) / (8 * 127.f)));

		voice[w].echo_delay += reverb * milli;
		voice[w].envelope_rate[DECAY] *= 2;
		voice[w].envelope_rate[RELEASE] /= 2;

		if (XG_System_reverb_type >= 0)
		{
			int subtype = XG_System_reverb_type & 0x07;
			int rtype = XG_System_reverb_type >> 3;
			switch (rtype)
			{
			case 0: /* no effect */
				break;
			case 1: /* hall */
				if (subtype) voice[w].echo_delay += 100 * milli;
				break;
			case 2: /* room */
				voice[w].echo_delay /= 2;
				break;
			case 3: /* stage */
				voice[w].velocity = voice[v].velocity;
				break;
			case 4: /* plate */
				voice[w].panning = voice[v].panning;
				break;
			case 16: /* white room */
				voice[w].echo_delay = 0;
				break;
			case 17: /* tunnel */
				voice[w].echo_delay *= 2;
				voice[w].velocity /= 2;
				break;
			case 18: /* canyon */
				voice[w].echo_delay *= 2;
				break;
			case 19: /* basement */
				voice[w].velocity /= 2;
				break;
			default:
				break;
			}
		}
	}
	played_note = voice[w].sample->note_to_use;
	if (!played_note)
	{
		played_note = note & 0x7f;
		if (variationbank == 35) played_note += 12;
		else if (variationbank == 36) played_note -= 12;
		else if (variationbank == 37) played_note += 7;
		else if (variationbank == 36) played_note -= 7;
	}
#if 0
	played_note = ( (played_note - voice[w].sample->freq_center) * voice[w].sample->freq_scale ) / 1024 +
		voice[w].sample->freq_center;
#endif
	voice[w].note = played_note;
	voice[w].orig_frequency = note_to_freq(played_note);

	if (chorus)
	{
		if (opt_stereo_surround)
		{
			if (voice[v].panning < 64)
				voice[w].panning = voice[v].panning + 32;
			else
				voice[w].panning = voice[v].panning - 32;
		}

		if (!voice[w].vibrato_control_ratio)
		{
			voice[w].vibrato_control_ratio = 100;
			voice[w].vibrato_depth = 6;
			voice[w].vibrato_sweep = 74;
		}
		voice[w].volume *= 0.40f;
		voice[v].volume = voice[w].volume;
		recompute_amp(&voice[v]);
		apply_envelope_to_amp(&voice[v]);

		voice[w].vibrato_sweep = chorus/2;
		voice[w].vibrato_depth /= 2;
		if (!voice[w].vibrato_depth) voice[w].vibrato_depth = 2;
		voice[w].vibrato_control_ratio /= 2;
		voice[w].echo_delay += 30 * milli;

		if (XG_System_chorus_type >= 0)
		{
			int subtype = XG_System_chorus_type & 0x07;
			int chtype = 0x0f & (XG_System_chorus_type >> 3);
			float chorus_factor;

			switch (chtype)
			{
			case 0: /* no effect */
				break;
			case 1: /* chorus */
				chorus /= 3;
				chorus_factor = pow(2.f, chorus / (256.f * 12.f));
				if(channel[ voice[w].channel ].pitchbend + chorus < 0x2000)
				{
					voice[w].orig_frequency = voice[w].orig_frequency * chorus_factor;
				}
				else
				{
					voice[w].orig_frequency = voice[w].orig_frequency / chorus_factor;
				}
				if (subtype)
				{
					voice[w].vibrato_depth *= 2;
				}
				break;
			case 2: /* celeste */
				voice[w].orig_frequency += (voice[w].orig_frequency/128) * chorus;
				break;
			case 3: /* flanger */
				voice[w].vibrato_control_ratio = 10;
				voice[w].vibrato_depth = 100;
				voice[w].vibrato_sweep = 8;
				voice[w].echo_delay += 200 * milli;
				break;
			case 4: /* symphonic : cf Children of the Night /128 bad, /1024 ok */
				voice[w].orig_frequency += (voice[w].orig_frequency/512) * chorus;
				voice[v].orig_frequency -= (voice[v].orig_frequency/512) * chorus;
				recompute_freq(v);
				break;
			case 8: /* phaser */
				break;
			default:
				break;
			}
		}
		else
		{
			float chorus_factor;
			chorus /= 3;
			chorus_factor = pow(2.f, chorus / (256.f * 12.f));
			if (channel[ voice[w].channel ].pitchbend + chorus < 0x2000)
			{
				voice[w].orig_frequency = voice[w].orig_frequency * chorus_factor;
			}
			else
			{
				voice[w].orig_frequency = voice[w].orig_frequency / chorus_factor;
			}
		}
	}
#if 0
	voice[w].loop_start = voice[w].sample->loop_start;
	voice[w].loop_end = voice[w].sample->loop_end;
#endif
	voice[w].echo_delay_count = voice[w].echo_delay;
	if (reverb) voice[w].echo_delay *= 2;

	recompute_freq(w);
	recompute_amp(&voice[w]);
	if (voice[w].sample->modes & MODES_ENVELOPE)
	{
		/* Ramp up from 0 */
		voice[w].envelope_stage = ATTACK;
		voice[w].modulation_stage = ATTACK;
		voice[w].envelope_volume = 0;
		voice[w].modulation_volume = 0;
		voice[w].control_counter = 0;
		voice[w].modulation_counter = 0;
		recompute_envelope(&voice[w]);
		/*recompute_modulation(w);*/
	}
	else
	{
		voice[w].envelope_increment = 0;
		voice[w].modulation_increment = 0;
	}
	apply_envelope_to_amp(&voice[w]);
}


void Renderer::xremap(int *banknumpt, int *this_notept, int this_kit)
{
	int i, newmap;
	int banknum = *banknumpt;
	int this_note = *this_notept;
	int newbank, newnote;

	if (!this_kit)
	{
		if (banknum == SFXBANK && tonebank[SFXBANK])
		{
			return;
		}
		if (banknum == SFXBANK && tonebank[120])
		{
			*banknumpt = 120;
		}
		return;
	}
	if (this_kit != 127 && this_kit != 126)
	{
		return;
	}
	for (i = 0; i < XMAPMAX; i++)
	{
		newmap = xmap[i][0];
		if (!newmap) return;
		if (this_kit == 127 && newmap != XGDRUM) continue;
		if (this_kit == 126 && newmap != SFXDRUM1) continue;
		if (xmap[i][1] != banknum) continue;
		if (xmap[i][3] != this_note) continue;
		newbank = xmap[i][2];
		newnote = xmap[i][4];
		if (newbank == banknum && newnote == this_note) return;
		if (!drumset[newbank]) return;
		if (!drumset[newbank]->tone[newnote].layer) return;
		if (drumset[newbank]->tone[newnote].layer == MAGIC_LOAD_INSTRUMENT) return;
		*banknumpt = newbank;
		*this_notept = newnote;
		return;
	}
}


void Renderer::start_note(int ch, int this_note, int this_velocity, int i)
{
	InstrumentLayer *lp;
	Instrument *ip;
	int j, banknum;
	int played_note, drumpan = NO_PANNING;
	int rt;
	int attacktime, releasetime, decaytime, variationbank;
	int brightness = channel[ch].brightness;
	int harmoniccontent = channel[ch].harmoniccontent;
	int drumsflag = channel[ch].kit;
	int this_prog = channel[ch].program;

	if (channel[ch].sfx)
	{
		banknum = channel[ch].sfx;
	}
	else
	{
		banknum = channel[ch].bank;
	}

	voice[i].velocity = this_velocity;

	if (XG_System_On) xremap(&banknum, &this_note, drumsflag);
	/*   if (current_config_pc42b) pcmap(&banknum, &this_note, &this_prog, &drumsflag); */

	if (drumsflag)
	{
		if (NULL == drumset[banknum] || NULL == (lp = drumset[banknum]->tone[this_note].layer))
		{
			if (!(lp = drumset[0]->tone[this_note].layer))
				return; /* No instrument? Then we can't play. */
		}
		ip = lp->instrument;
		if (ip->type == INST_GUS && ip->samples != 1)
		{
			cmsg(CMSG_WARNING, VERB_VERBOSE, 
				"Strange: percussion instrument with %d samples!", ip->samples);
		}

		if (ip->sample->note_to_use) /* Do we have a fixed pitch? */
		{
			voice[i].orig_frequency = note_to_freq(ip->sample->note_to_use);
			drumpan = drumpanpot[ch][(int)ip->sample->note_to_use];
			drumpan |= drumpan << 7;
		}
		else
			voice[i].orig_frequency = note_to_freq(this_note & 0x7F);
	}
	else
	{
		if (channel[ch].program == SPECIAL_PROGRAM)
		{
			lp = default_instrument;
		}
		else if (NULL == tonebank[channel[ch].bank] || NULL == (lp = tonebank[channel[ch].bank]->tone[channel[ch].program].layer))
		{
			if (NULL == (lp = tonebank[0]->tone[this_prog].layer))
				return; /* No instrument? Then we can't play. */
		}
		ip = lp->instrument;
		if (ip->sample->note_to_use) /* Fixed-pitch instrument? */
			voice[i].orig_frequency = note_to_freq(ip->sample->note_to_use);
		else
			voice[i].orig_frequency = note_to_freq(this_note & 0x7F);
	}

	select_stereo_samples(i, lp, this_velocity);

	played_note = voice[i].sample->note_to_use;

	if (!played_note || !drumsflag) played_note = this_note & 0x7f;
#if 0
	played_note = ( (played_note - voice[i].sample->freq_center) * voice[i].sample->freq_scale ) / 1024 +
		voice[i].sample->freq_center;
#endif
	voice[i].status = VOICE_ON;
	voice[i].channel = ch;
	voice[i].note = played_note;
	voice[i].velocity = this_velocity;
	voice[i].sample_offset = 0;
	voice[i].sample_increment = 0; /* make sure it isn't negative */

	voice[i].tremolo_phase = 0;
	voice[i].tremolo_phase_increment = voice[i].sample->tremolo_phase_increment;
	voice[i].tremolo_sweep = voice[i].sample->tremolo_sweep_increment;
	voice[i].tremolo_sweep_position = 0;

	voice[i].vibrato_sweep = voice[i].sample->vibrato_sweep_increment;
	voice[i].vibrato_sweep_position = 0;
	voice[i].vibrato_depth = voice[i].sample->vibrato_depth;
	voice[i].vibrato_control_ratio = voice[i].sample->vibrato_control_ratio;
	voice[i].vibrato_control_counter = voice[i].vibrato_phase=0;
//	voice[i].vibrato_delay = voice[i].sample->vibrato_delay;

	kill_others(i);

	for (j = 0; j < VIBRATO_SAMPLE_INCREMENTS; j++)
		voice[i].vibrato_sample_increment[j] = 0;

	attacktime = channel[ch].attacktime;
	releasetime = channel[ch].releasetime;
	decaytime = 64;
	variationbank = channel[ch].variationbank;

	switch (variationbank)
	{
	case  8:
		attacktime = 64+32;
		break;
	case 12:
		decaytime = 64-32;
		break;
	case 16:
		brightness = 64+16;
		break;
	case 17:
		brightness = 64+32;
		break;
	case 18:
		brightness = 64-16;
		break;
	case 19:
		brightness = 64-32;
		break;
	case 20:
		harmoniccontent = 64+16;
		break;
#if 0
	case 24:
		voice[i].modEnvToFilterFc=2.0;
		voice[i].sample->cutoff_freq = 800;
		break;
	case 25:
		voice[i].modEnvToFilterFc=-2.0;
		voice[i].sample->cutoff_freq = 800;
		break;
	case 27:
		voice[i].modLfoToFilterFc=2.0;
		voice[i].lfo_phase_increment=109;
		voice[i].lfo_sweep=122;
		voice[i].sample->cutoff_freq = 800;
		break;
	case 28:
		voice[i].modLfoToFilterFc=-2.0;
		voice[i].lfo_phase_increment=109;
		voice[i].lfo_sweep=122;
		voice[i].sample->cutoff_freq = 800;
		break;
#endif
	default:
		break;
	}

	for (j = ATTACK; j < MAXPOINT; j++)
	{
		voice[i].envelope_rate[j] = voice[i].sample->envelope_rate[j];
		voice[i].envelope_offset[j] = voice[i].sample->envelope_offset[j];
	}

	voice[i].echo_delay = voice[i].envelope_rate[DELAY];
	voice[i].echo_delay_count = voice[i].echo_delay;

	if (attacktime != 64)
	{
		rt = voice[i].envelope_rate[ATTACK];
		rt = rt + ( (64-attacktime)*rt ) / 100;
		if (rt > 1000)
		{
			voice[i].envelope_rate[ATTACK] = rt;
		}
	}
	if (releasetime != 64)
	{
		rt = voice[i].envelope_rate[RELEASE];
		rt = rt + ( (64-releasetime)*rt ) / 100;
		if (rt > 1000)
		{
			voice[i].envelope_rate[RELEASE] = rt;
		}
	}
	if (decaytime != 64)
	{
		rt = voice[i].envelope_rate[DECAY];
		rt = rt + ( (64-decaytime)*rt ) / 100;
		if (rt > 1000)
		{
			voice[i].envelope_rate[DECAY] = rt;
		}
	}

	if (channel[ch].panning != NO_PANNING)
	{
		voice[i].panning = channel[ch].panning;
	}
	else
	{
		voice[i].panning = voice[i].sample->panning;
	}
	if (drumpan != NO_PANNING)
	{
		voice[i].panning = drumpan;
	}

	if (variationbank == 1)
	{
		int pan = voice[i].panning;
		int disturb = 0;
		/* If they're close up (no reverb) and you are behind the pianist,
		* high notes come from the right, so we'll spread piano etc. notes
		* out horizontally according to their pitches.
		*/
		if (this_prog < 21)
		{
			int n = voice[i].velocity - 32;
			if (n < 0) n = 0;
			if (n > 64) n = 64;
			pan = pan/2 + n;
		}
		/* For other types of instruments, the music sounds more alive if
		* notes come from slightly different directions.  However, instruments
		* do drift around in a sometimes disconcerting way, so the following
		* might not be such a good idea.
		*/
		else
		{
			disturb = (voice[i].velocity/32 % 8) + (voice[i].note % 8); /* /16? */
		}

		if (pan < 64)
		{
			pan += disturb;
		}
		else
		{
			pan -= disturb;
		}
		pan = pan < 0 ? 0 : pan > 127 ? 127 : pan;
		voice[i].panning = pan | (pan << 7);
	}

	recompute_freq(i);
	recompute_amp(&voice[i]);
	if (voice[i].sample->modes & MODES_ENVELOPE)
	{
		/* Ramp up from 0 */
		voice[i].envelope_stage = ATTACK;
		voice[i].envelope_volume = 0;
		voice[i].control_counter = 0;
		recompute_envelope(&voice[i]);
	}
	else
	{
		voice[i].envelope_increment = 0;
	}
	apply_envelope_to_amp(&voice[i]);

	voice[i].clone_voice = -1;
	voice[i].clone_type = NOT_CLONE;

	clone_voice(ip, i, this_note, this_velocity, STEREO_CLONE, variationbank);
	clone_voice(ip, i, this_note, this_velocity, CHORUS_CLONE, variationbank);
	clone_voice(ip, i, this_note, this_velocity, REVERB_CLONE, variationbank);
}

void Renderer::kill_note(int i)
{
	voice[i].status = VOICE_DIE;
	if (voice[i].clone_voice >= 0)
	{
		voice[ voice[i].clone_voice ].status = VOICE_DIE;
	}
}


/* Only one instance of a note can be playing on a single channel. */
void Renderer::note_on(int chan, int note, int vel)
{
	int i = voices, lowest = -1; 
	float lv = 1e10, v;

	while (i--)
	{
		if (voice[i].status == VOICE_FREE)
		{
			lowest = i; /* Can't get a lower volume than silence */
		}
		else if (voice[i].channel == chan && (voice[i].note == note || channel[chan].mono))
		{
			kill_note(i);
		}
	}

	if (lowest != -1)
	{
		/* Found a free voice. */
		start_note(chan, note, vel, lowest);
		return;
	}

	/* Look for the decaying note with the lowest volume */
	if (lowest == -1)
	{
		i = voices;
		while (i--)
		{
			if ( (voice[i].status != VOICE_ON &&
				  voice[i].status != VOICE_DIE &&
				  voice[i].status != VOICE_FREE) &&
				(!voice[i].clone_type))
			{
				v = voice[i].left_mix;
				if ((voice[i].panned == PANNED_MYSTERY) && (voice[i].right_mix > v))
				{
					v = voice[i].right_mix;
				}
				if (v < lv)
				{
					lv = v;
					lowest = i;
				}
			}
		}
	}

	if (lowest != -1)
	{
		int cl = voice[lowest].clone_voice;

		/* This can still cause a click, but if we had a free voice to
		spare for ramping down this note, we wouldn't need to kill it
		in the first place... Still, this needs to be fixed. Perhaps
		we could use a reserve of voices to play dying notes only. */

		if (cl >= 0)
		{
			if (voice[cl].clone_type == STEREO_CLONE ||
				(!voice[cl].clone_type && voice[lowest].clone_type == STEREO_CLONE))
			{
				voice[cl].status = VOICE_FREE;
			}
			else if (voice[cl].clone_voice == lowest)
			{
				voice[cl].clone_voice = -1;
			}
		}

		cut_notes++;
		voice[lowest].status = VOICE_FREE;
		start_note(chan, note, vel, lowest);
	}
	else
	{
		lost_notes++;
	}
}

void Renderer::finish_note(int i)
{
	if (voice[i].sample->modes & MODES_ENVELOPE)
	{
		/* We need to get the envelope out of Sustain stage */
		voice[i].envelope_stage = RELEASE;
		voice[i].status = VOICE_OFF;
		recompute_envelope(&voice[i]);
		apply_envelope_to_amp(&voice[i]);
	}
	else
	{
		/* Set status to OFF so resample_voice() will let this voice out
		of its loop, if any. In any case, this voice dies when it
		hits the end of its data (ofs>=data_length). */
		voice[i].status = VOICE_OFF;
	}

	int v;
	if ( (v = voice[i].clone_voice) >= 0)
	{
		voice[i].clone_voice = -1;
		finish_note(v);
	}
}

void Renderer::note_off(int chan, int note, int vel)
{
	int i = voices, v;
	while (i--)
	{
		if (voice[i].status == VOICE_ON &&
			voice[i].channel == chan &&
			voice[i].note == note)
		{
			if (channel[chan].sustain)
			{
				voice[i].status = VOICE_SUSTAINED;

				if ( (v = voice[i].clone_voice) >= 0)
				{
					if (voice[v].status == VOICE_ON)
						voice[v].status = VOICE_SUSTAINED;
				}
			}
			else
			{
				finish_note(i);
			}
			return;
		}
	}
}

/* Process the All Notes Off event */
void Renderer::all_notes_off(int c)
{
	int i = voices;
	cmsg(CMSG_INFO, VERB_DEBUG, "All notes off on channel %d", c);
	while (i--)
	{
		if (voice[i].status == VOICE_ON && voice[i].channel == c)
		{
			if (channel[c].sustain) 
			{
				voice[i].status = VOICE_SUSTAINED;
			}
			else
			{
				finish_note(i);
			}
		}
	}
}

/* Process the All Sounds Off event */
void Renderer::all_sounds_off(int c)
{
	int i = voices;
	while (i--)
	{
		if (voice[i].channel == c && 
			voice[i].status != VOICE_FREE &&
			voice[i].status != VOICE_DIE)
		{
			kill_note(i);
		}
	}
}

void Renderer::adjust_pressure(int chan, int note, int amount)
{
	int i = voices;
	while (i--)
	{
		if (voice[i].status == VOICE_ON &&
			voice[i].channel == chan &&
			voice[i].note == note)
		{
			voice[i].velocity = amount;
			recompute_amp(&voice[i]);
			apply_envelope_to_amp(&voice[i]);
			return;
		}
	}
}

void Renderer::adjust_panning(int c)
{
	int i = voices;
	while (i--)
	{
		if ((voice[i].channel == c) &&
			(voice[i].status == VOICE_ON || voice[i].status == VOICE_SUSTAINED))
		{
			if (voice[i].clone_type != NOT_CLONE) continue;
			voice[i].panning = channel[c].panning;
			recompute_amp(&voice[i]);
			apply_envelope_to_amp(&voice[i]);
		}
	}
}

void Renderer::drop_sustain(int c)
{
	int i = voices;
	while (i--)
	{
		if (voice[i].status == VOICE_SUSTAINED && voice[i].channel == c)
			finish_note(i);
	}
}

void Renderer::adjust_pitchbend(int c)
{
	int i = voices;
	while (i--)
	{
		if (voice[i].status != VOICE_FREE && voice[i].channel == c)
		{
			recompute_freq(i);
		}
	}
}

void Renderer::adjust_volume(int c)
{
	int i = voices;
	while (i--)
	{
		if (voice[i].channel == c &&
			(voice[i].status == VOICE_ON || voice[i].status == VOICE_SUSTAINED))
		{
			recompute_amp(&voice[i]);
			apply_envelope_to_amp(&voice[i]);
		}
	}
}

void Renderer::HandleEvent(int status, int parm1, int parm2)
{
	int command = status & 0xF0;
	int chan	= status & 0x0F;

	switch (command)
	{
	case ME_NOTEON:
		parm1 += channel[chan].transpose;
		if (parm2 == 0)
		{
			note_off(chan, parm1, 0);
		}
		else
		{
			note_on(chan, parm1, parm2);
		}
		break;

	case ME_NOTEOFF:
		parm1 += channel[chan].transpose;
		note_off(chan, parm1, parm2);
		break;

	case ME_KEYPRESSURE:
		adjust_pressure(chan, parm1, parm2);
		break;

	case ME_CONTROLCHANGE:
		HandleController(chan, parm1, parm2);
		break;

	case ME_PROGRAM:
		/* if (ISDRUMCHANNEL(chan)) { */
		if (channel[chan].kit) {
			/* Change drum set */
			channel[chan].bank = parm1;
		}
		else
		{
			channel[chan].program = parm1;
		}
		break;

	case ME_CHANNELPRESSURE:
		/* Unimplemented */
		break;

	case ME_PITCHWHEEL:
		channel[chan].pitchbend = parm1 | (parm2 << 7);
		channel[chan].pitchfactor = 0;
		/* Adjust for notes already playing */
		adjust_pitchbend(chan);
		break;
	}
}

void Renderer::HandleController(int chan, int ctrl, int val)
{
	switch (ctrl)
	{
    /* These should be the SCC-1 tone bank switch
       commands. I don't know why there are two, or
       why the latter only allows switching to bank 0.
       Also, some MIDI files use 0 as some sort of
       continuous controller. This will cause lots of
       warnings about undefined tone banks. */
	case CTRL_BANK_SELECT:
		if (XG_System_On)
		{
			if (val == SFX_BANKTYPE)
			{
				channel[chan].sfx = SFXBANK;
				channel[chan].kit = 0;
			}
			else
			{
				channel[chan].sfx = 0;
				channel[chan].kit = val;
			}
		}
		else
		{
			channel[chan].bank = val;
		}
		break;

	case CTRL_BANK_SELECT+32:
		if (XG_System_On)
		{
			channel[chan].bank = val;
		}
		break;

	case CTRL_VOLUME:
		channel[chan].volume = (channel[chan].volume & 0x007F) | (val << 7);
		adjust_volume(chan);
		break;

	case CTRL_VOLUME+32:
		channel[chan].volume = (channel[chan].volume & 0x3F80) | (val);
		adjust_volume(chan);
		break;

	case CTRL_EXPRESSION:
		channel[chan].expression = (channel[chan].expression & 0x007F) | (val << 7);
		adjust_volume(chan);
		break;

	case CTRL_EXPRESSION+32:
		channel[chan].expression = (channel[chan].expression & 0x3F80) | (val);
		adjust_volume(chan);
		break;

	case CTRL_PAN:
		channel[chan].panning = (channel[chan].panning & 0x007F) | (val << 7);
		if (adjust_panning_immediately)
		{
			adjust_panning(chan);
		}
		break;

	case CTRL_PAN+32:
		channel[chan].panning = (channel[chan].panning & 0x3F80) | (val);
		if (adjust_panning_immediately)
		{
			adjust_panning(chan);
		}
		break;

	case CTRL_SUSTAIN:
		channel[chan].sustain = val;
		if (val == 0)
		{
			drop_sustain(chan);
		}
		break;

	case CTRL_HARMONICCONTENT:
		channel[chan].harmoniccontent = val;
		break;

	case CTRL_RELEASETIME:
		channel[chan].releasetime = val;
		break;

	case CTRL_ATTACKTIME:
		channel[chan].attacktime = val;
		break;

	case CTRL_BRIGHTNESS:
		channel[chan].brightness = val;
		break;

	case CTRL_REVERBERATION:
		channel[chan].reverberation = val;
		break;

	case CTRL_CHORUSDEPTH:
		channel[chan].chorusdepth = val;
		break;

	case CTRL_NRPN_LSB:
		channel[chan].nrpn = (channel[chan].nrpn & 0x3F80) | (val);
		channel[chan].nrpn_mode = true;
		break;

	case CTRL_NRPN_MSB:
		channel[chan].nrpn = (channel[chan].nrpn & 0x007F) | (val << 7);
		channel[chan].nrpn_mode = true;
		break;

	case CTRL_RPN_LSB:
		channel[chan].rpn = (channel[chan].rpn & 0x3F80) | (val);
		channel[chan].nrpn_mode = false;
		break;

	case CTRL_RPN_MSB:
		channel[chan].rpn = (channel[chan].rpn & 0x007F) | (val << 7);
		channel[chan].nrpn_mode = false;
		break;

	case CTRL_DATA_ENTRY:
		if (channel[chan].nrpn_mode)
		{
			DataEntryCoarseNRPN(chan, channel[chan].nrpn, val);
		}
		else
		{
			DataEntryCoarseRPN(chan, channel[chan].rpn, val);
		}
		break;

	case CTRL_DATA_ENTRY+32:
		if (channel[chan].nrpn_mode)
		{
			DataEntryFineNRPN(chan, channel[chan].nrpn, val);
		}
		else
		{
			DataEntryFineRPN(chan, channel[chan].rpn, val);
		}
		break;

	case CTRL_ALL_SOUNDS_OFF:
		all_sounds_off(chan);
		break;

	case CTRL_RESET_CONTROLLERS:
		reset_controllers(chan);
		break;

	case CTRL_ALL_NOTES_OFF:
		all_notes_off(chan);
		break;
	}
}

void Renderer::DataEntryCoarseRPN(int chan, int rpn, int val)
{
	switch (rpn)
	{
	case RPN_PITCH_SENS:
		channel[chan].pitchsens = (channel[chan].pitchsens % 100) + (val * 100);
		channel[chan].pitchfactor = 0;
		break;

	// TiMidity resets the pitch sensitivity when a song attempts to write to
	// RPN_RESET. My docs tell me this is just a dummy value that is guaranteed
	// to not cause future data entry to go anywhere until a new RPN is set.
	}
}

void Renderer::DataEntryFineRPN(int chan, int rpn, int val)
{
	switch (rpn)
	{
	case RPN_PITCH_SENS:
		channel[chan].pitchsens = (channel[chan].pitchsens / 100) * 100 + val;
		channel[chan].pitchfactor = 0;
		break;
	}
}

// [RH] Q. What device are we pretending to be by responding to these NRPNs?
void Renderer::DataEntryCoarseNRPN(int chan, int nrpn, int val)
{
	switch (nrpn & 0x3F80)
	{
	case 0x0080:
		if (nrpn == NRPN_BRIGHTNESS)
		{
			channel[chan].brightness = val;
		}
		else if (nrpn == NRPN_HARMONICCONTENT)
		{
			channel[chan].harmoniccontent = val;
		}
		break;

	case NRPN_DRUMVOLUME:
		drumvolume[chan][nrpn & 0x007F] = val;
		break;

	case NRPN_DRUMPANPOT:
		if (val == 0)
		{
			val = 127 * rand() / RAND_MAX;
		}
		drumpanpot[chan][nrpn & 0x007F] = val;
		break;

	case NRPN_DRUMREVERBERATION:
		drumreverberation[chan][nrpn & 0x007F] = val;
		break;

	case NRPN_DRUMCHORUSDEPTH:
		drumchorusdepth[chan][nrpn & 0x007F] = val;
		break;
	}
}

void Renderer::DataEntryFineNRPN(int chan, int nrpn, int val)
{
	// We don't care about fine data entry for any NRPN at this time.
}

void Renderer::HandleLongMessage(const BYTE *data, int len)
{
	// SysEx handling goes here.
}

void Renderer::Reset()
{
	int i;

	lost_notes = cut_notes = 0;
	GM_System_On = GS_System_On = XG_System_On = 0;
	XG_System_reverb_type = XG_System_chorus_type = XG_System_variation_type = 0;
	memset(&drumvolume, -1, sizeof(drumvolume));
	memset(&drumchorusdepth, -1, sizeof(drumchorusdepth));
	memset(&drumreverberation, -1, sizeof(drumreverberation));
	memset(&drumpanpot, NO_PANNING, sizeof(drumpanpot));
	for (i = 0; i < MAXCHAN; ++i)
	{
		channel[i].kit = ISDRUMCHANNEL(i) ? 127 : 0;
		channel[i].brightness = 64;
		channel[i].harmoniccontent = 64;
		channel[i].variationbank = 0;
		channel[i].chorusdepth = 0;
		channel[i].reverberation = 0;
		channel[i].transpose = 0;
	}
	reset_midi();
}

}
