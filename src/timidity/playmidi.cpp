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

void Renderer::reset_voices()
{
	for (int i = 0; i < MAX_VOICES; i++)
	{
		voice[i].status = VOICE_FREE;
	}
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
	}
	reset_voices();
}

void Renderer::select_sample(int v, Instrument *ip, int vel)
{
	double f, cdiff, diff;
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
	for (i = 0; i < s; i++)
	{
		diff = fabs(sp->root_freq - f);
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

void Renderer::recompute_freq(int v)
{
	Channel *ch = &channel[voice[v].channel];
	int 
		sign = (voice[v].sample_increment < 0), /* for bidirectional loops */
		pb = ch->pitchbend;
	double a;

	if (voice[v].sample->sample_rate == 0)
	{
		return;
	}

	if (voice[v].vibrato_control_ratio != 0)
	{
		/* This instrument has vibrato. Invalidate any precomputed
		   sample_increments. */
		memset(voice[v].vibrato_sample_increment, 0, sizeof(voice[v].vibrato_sample_increment));
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
	int chan = v->channel;
	int panning = v->panning;
	double vol = channel[chan].volume / 16383.f;
	double expr = channel[chan].expression / 16383.f;
	double vel = v->velocity / 127.f;
	double tempamp = calc_vol(vol * expr * vel) * v->sample->volume;

	if (panning >= 0x1DBB && panning < 0x2244)
	{
		v->panned = PANNED_CENTER;
		v->left_amp = (float)(tempamp * 0.70710678118654752440084436210485);	// * sqrt(0.5)
	}
	else if (panning == 0)
	{
		v->panned = PANNED_LEFT;
		v->left_amp = (float)tempamp;
	}
	else if (panning == 0x3FFF)
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

void Renderer::kill_key_group(int i)
{
	int j = voices; 

	if (voice[i].sample->key_group == 0)
	{
		return;
	}
	while (j--)
	{
		if (voice[j].status != VOICE_ON && voice[j].status != VOICE_SUSTAINED) continue;
		if (i == j) continue;
		if (voice[i].channel != voice[j].channel) continue;
		if (voice[j].sample->key_group != voice[i].sample->key_group) continue;
		kill_note(j);
	}
}

float Renderer::calculate_scaled_frequency(Sample *sp, int note)
{
	double scalednote = (note - sp->scale_note) * sp->scale_factor / 1024.0 + sp->scale_note;
	return (float)note_to_freq(scalednote);
}

void Renderer::start_note(int chan, int note, int vel, int i)
{
	Instrument *ip;
	int bank = channel[chan].bank;
	int prog = channel[chan].program;

	if (ISDRUMCHANNEL(chan))
	{
		if (NULL == drumset[bank] || NULL == (ip = drumset[bank]->instrument[note]))
		{
			if (!(ip = drumset[0]->instrument[note]))
				return; /* No instrument? Then we can't play. */
		}
		if (ip->type == INST_GUS && ip->samples != 1)
		{
			cmsg(CMSG_WARNING, VERB_VERBOSE, 
				"Strange: percussion instrument with %d samples!", ip->samples);
		}
	}
	else
	{
		if (channel[chan].program == SPECIAL_PROGRAM)
		{
			ip = default_instrument;
		}
		else if (NULL == tonebank[bank] || NULL == (ip = tonebank[bank]->instrument[prog]))
		{
			if (NULL == (ip = tonebank[0]->instrument[prog]))
				return; /* No instrument? Then we can't play. */
		}
	}
	if (ip->sample->scale_factor != 1024)
	{
		voice[i].orig_frequency = calculate_scaled_frequency(ip->sample, note & 0x7F);
	}
	else
	{
		voice[i].orig_frequency = note_to_freq(note & 0x7F);
	}
	select_sample(i, ip, vel);

	voice[i].status = VOICE_ON;
	voice[i].channel = chan;
	voice[i].note = note;
	voice[i].velocity = vel;
	voice[i].sample_offset = 0;
	voice[i].sample_increment = 0; /* make sure it isn't negative */

	voice[i].tremolo_phase = 0;
	voice[i].tremolo_phase_increment = voice[i].sample->tremolo_phase_increment;
	voice[i].tremolo_sweep = voice[i].sample->tremolo_sweep_increment;
	voice[i].tremolo_sweep_position = 0;

	voice[i].vibrato_sweep = voice[i].sample->vibrato_sweep_increment;
	voice[i].vibrato_sweep_position = 0;
	voice[i].vibrato_control_ratio = voice[i].sample->vibrato_control_ratio;
	voice[i].vibrato_control_counter = voice[i].vibrato_phase = 0;

	kill_key_group(i);

	memset(voice[i].vibrato_sample_increment, 0, sizeof(voice[i].vibrato_sample_increment));

	if (channel[chan].panning != NO_PANNING)
	{
		voice[i].panning = channel[chan].panning;
	}
	else
	{
		voice[i].panning = voice[i].sample->panning;
	}

	recompute_freq(i);
	recompute_amp(&voice[i]);
	if (voice[i].sample->modes & PATCH_NO_SRELEASE)
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
}

void Renderer::kill_note(int i)
{
	voice[i].status = VOICE_DIE;
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
		else if (voice[i].channel == chan && ((voice[i].note == note && !voice[i].sample->self_nonexclusive) || channel[chan].mono))
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
			if (voice[i].status != VOICE_ON && voice[i].status != VOICE_DIE)
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
		/* This can still cause a click, but if we had a free voice to
		   spare for ramping down this note, we wouldn't need to kill it
		   in the first place... Still, this needs to be fixed. Perhaps
		   we could use a reserve of voices to play dying notes only. */

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
	if (voice[i].sample->modes & PATCH_NO_SRELEASE)
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
		   hits the end of its data (ofs >= data_length). */
		voice[i].status = VOICE_OFF;
	}
}

void Renderer::note_off(int chan, int note, int vel)
{
	int i = voices;
	while (i--)
	{
		if (voice[i].status == VOICE_ON &&
			voice[i].channel == chan &&
			voice[i].note == note)
		{
			if (channel[chan].sustain)
			{
				voice[i].status = VOICE_SUSTAINED;
			}
			else
			{
				finish_note(i);
			}
			if (!voice[i].sample->self_nonexclusive)
			{
				return;
			}
		}
	}
}

/* Process the All Notes Off event */
void Renderer::all_notes_off(int chan)
{
	int i = voices;
	while (i--)
	{
		if (voice[i].status == VOICE_ON && voice[i].channel == chan)
		{
			if (channel[chan].sustain) 
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
void Renderer::all_sounds_off(int chan)
{
	int i = voices;
	while (i--)
	{
		if (voice[i].channel == chan && 
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
			if (!(voice[i].sample->self_nonexclusive))
			{
				return;
			}
		}
	}
}

void Renderer::adjust_panning(int chan)
{
	int i = voices;
	while (i--)
	{
		if ((voice[i].channel == chan) &&
			(voice[i].status == VOICE_ON || voice[i].status == VOICE_SUSTAINED))
		{
			voice[i].panning = channel[chan].panning;
			recompute_amp(&voice[i]);
			apply_envelope_to_amp(&voice[i]);
		}
	}
}

void Renderer::drop_sustain(int chan)
{
	int i = voices;
	while (i--)
	{
		if (voice[i].status == VOICE_SUSTAINED && voice[i].channel == chan)
		{
			finish_note(i);
		}
	}
}

void Renderer::adjust_pitchbend(int chan)
{
	int i = voices;
	while (i--)
	{
		if (voice[i].status != VOICE_FREE && voice[i].channel == chan)
		{
			recompute_freq(i);
		}
	}
}

void Renderer::adjust_volume(int chan)
{
	int i = voices;
	while (i--)
	{
		if (voice[i].channel == chan &&
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
		note_off(chan, parm1, parm2);
		break;

	case ME_KEYPRESSURE:
		adjust_pressure(chan, parm1, parm2);
		break;

	case ME_CONTROLCHANGE:
		HandleController(chan, parm1, parm2);
		break;

	case ME_PROGRAM:
		if (ISDRUMCHANNEL(chan))
		{
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
		channel[chan].bank = val;
		break;

	case CTRL_BANK_SELECT+32:
		if (val == 0)
		{
			channel[chan].bank = 0;
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
		adjust_panning(chan);
		break;

	case CTRL_PAN+32:
		channel[chan].panning = (channel[chan].panning & 0x3F80) | (val);
		adjust_panning(chan);
		break;

	case CTRL_SUSTAIN:
		channel[chan].sustain = val;
		if (val == 0)
		{
			drop_sustain(chan);
		}
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

void Renderer::DataEntryCoarseNRPN(int chan, int nrpn, int val)
{
}

void Renderer::DataEntryFineNRPN(int chan, int nrpn, int val)
{
}

void Renderer::HandleLongMessage(const BYTE *data, int len)
{
	// SysEx handling goes here.
}

void Renderer::Reset()
{
	lost_notes = cut_notes = 0;
	reset_midi();
}

}
