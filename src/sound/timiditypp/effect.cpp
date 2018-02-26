/*
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999-2004 Masanao Izumo <iz@onicos.co.jp>
    Copyright (C) 1995 Tuukka Toivonen <tt@cgs.fi>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

    effect.c - To apply sound effects.
    Mainly written by Masanao Izumo <iz@onicos.co.jp>

    Interfaces:
    void init_effect(void);
    do_effect(int32_t* buf, int32_t count);
*/

#include <string.h>
#include <stdlib.h>

#include "effect.h"
#include "instrum.h"
#include "playmidi.h"
#include "reverb.h"


namespace TimidityPlus
{



#define SIDE_CONTI_SEC	10
#define CHANGE_SEC		2.0

void Effect::init_effect()
{
	effect_left_right_delay(NULL, 0);
	reverb->init_for_effect();
}

/*
	* Left & Right Delay Effect
	*/
void Effect::effect_left_right_delay(int32_t *buff, int32_t count)
{
	int32_t save[AUDIO_BUFFER_SIZE * 2];
	int32_t pi, i, j, k, v, backoff;
	int b;
	int32_t *p;

	if (buff == NULL)
	{
		memset(prev, 0, sizeof(prev));
		return;
	}
	if (effect_lr_mode == 0 || effect_lr_mode == 1 || effect_lr_mode == 2)
		b = effect_lr_mode;
	else
		return;
	count *= 2;
	backoff = 2 * (int)(playback_rate * effect_lr_delay_msec / 1000.0);
	if (backoff == 0)
		return;
	if (backoff > count)
		backoff = count;
	if (count < AUDIO_BUFFER_SIZE * 2)
	{
		memset(buff + count, 0, 4 * (AUDIO_BUFFER_SIZE * 2 - count));
		count = AUDIO_BUFFER_SIZE * 2;
	}
	memcpy(save, buff, 4 * count);
	pi = count - backoff;
	if (b == 2)
	{
		if (turn_counter == 0)
		{
			turn_counter = SIDE_CONTI_SEC * playback_rate;
			/* status: 0 -> 2 -> 3 -> 1 -> 4 -> 5 -> 0 -> ...
				* status left   right
				* 0      -      +		(right)
				* 1      +      -		(left)
				* 2     -> +    +		(right -> center)
				* 3      +     -> -	(center -> left)
				* 4     -> -    -		(left -> center)
				* 5      -     -> +	(center -> right)
				*/
			status = 0;
			tc = 0;
		}
		p = prev;
		for (i = 0; i < count; i += 2, pi += 2)
		{
			if (i < backoff)
				p = prev;
			else if (p == prev)
			{
				pi = 0;
				p = save;
			}
			if (status < 2)
				buff[i + status] = p[pi + status];
			else if (status < 4)
			{
				j = (status & 1);
				v = (int32_t)(rate0 * buff[i + j] + rate1 * p[pi + j]);
				buff[i + j] = v;
				rate0 += dr, rate1 -= dr;
			}
			else
			{
				j = (status & 1);
				k = !j;
				v = (int32_t)(rate0 * buff[i + j] + rate1 * p[pi + j]);
				buff[i + j] = v;
				buff[i + k] = p[pi + k];
				rate0 += dr, rate1 -= dr;
			}
			tc++;
			if (tc == turn_counter)
			{
				tc = 0;
				switch (status)
				{
				case 0:
					status = 2;
					turn_counter = (CHANGE_SEC / 2.0) * playback_rate;
					rate0 = 0.0;
					rate1 = 1.0;
					dr = 1.0 / turn_counter;
					break;
				case 2:
					status = 3;
					rate0 = 1.0;
					rate1 = 0.0;
					dr = -1.0 / turn_counter;
					break;
				case 3:
					status = 1;
					turn_counter = SIDE_CONTI_SEC * playback_rate;
					break;
				case 1:
					status = 4;
					turn_counter = (CHANGE_SEC / 2.0) * playback_rate;
					rate0 = 1.0;
					rate1 = 0.0;
					dr = -1.0 / turn_counter;
					break;
				case 4:
					status = 5;
					turn_counter = (CHANGE_SEC / 2.0) * playback_rate;
					rate0 = 0.0;
					rate1 = 1.0;
					dr = 1.0 / turn_counter;
					break;
				case 5:
					status = 0;
					turn_counter = SIDE_CONTI_SEC * playback_rate;
					break;
				}
			}
		}
	}
	else
	{
		for (i = 0; i < backoff; i += 2, pi += 2)
			buff[b + i] = prev[b + pi];
		for (pi = 0; i < count; i += 2, pi += 2)
			buff[b + i] = save[b + pi];
	}
	memcpy(prev + count - backoff, save + count - backoff, 4 * backoff);
}

void Effect::do_effect(int32_t *buf, int32_t count)
{
	int32_t nsamples = count * 2;
	int reverb_level = (timidity_reverb < 0)
		? -timidity_reverb & 0x7f : DEFAULT_REVERB_SEND_LEVEL;

	/* for static reverb / chorus level */
	if (timidity_reverb == 2 || timidity_reverb == 4
		|| (timidity_reverb < 0 && !(timidity_reverb & 0x80))
		|| timidity_chorus < 0)
	{
		reverb->set_dry_signal(buf, nsamples);
		/* chorus sounds horrible
			* if applied globally on top of channel chorus
			*/
		if (timidity_reverb == 2 || timidity_reverb == 4
			|| (timidity_reverb < 0 && !(timidity_reverb & 0x80)))
			reverb->set_ch_reverb(buf, nsamples, reverb_level);
		reverb->mix_dry_signal(buf, nsamples);
		/* chorus sounds horrible
			* if applied globally on top of channel chorus
			*/
		if (timidity_reverb == 2 || timidity_reverb == 4
			|| (timidity_reverb < 0 && !(timidity_reverb & 0x80)))
			reverb->do_ch_reverb(buf, nsamples);
	}
	/* L/R Delay */
	effect_left_right_delay(buf, count);
}

uint32_t Effect::frand(void)
{
	return rng.GenRand32();
}

int32_t Effect::my_mod(int32_t x, int32_t n)
{
	if (x >= n)
		x -= n;
	return x;
}

}