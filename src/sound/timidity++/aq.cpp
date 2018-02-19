/*
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999-2002 Masanao Izumo <mo@goice.co.jp>
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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    aq.c - Audio queue.
	      Written by Masanao Izumo <mo@goice.co.jp>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "timidity.h"
#include "common.h"
#include "aq.h"
#include "instrum.h"
#include "playmidi.h"
#include "effect.h"


namespace TimidityPlus
{


#define TEST_SPARE_RATE 0.9
#define MAX_BUCKET_TIME 0.2
#define MAX_FILLED_TIME 2.0


AudioQueue::AudioQueue(PlayMode *pm, int bufsize, Reverb *reverb)
{
	playMode = pm;
	buffer_size = bufsize;
	effect = new Effect(reverb);
}

AudioQueue::~AudioQueue()
{
	freeSoftQueue();
	delete effect;
}

void AudioQueue::setup(void)
{
	int ch = 2;	// stereo only

	/* Initialize Bps, bucket_size, device_qsize, and bucket_time */

	if (playMode->encoding & PE_24BIT)
		Bps = 3 * ch;
	else
		Bps = 2 * ch;

	bucket_size = buffer_size * Bps;
	bucket_time = (double)bucket_size / Bps / playMode->rate;

	device_qsize = 0;
	freeSoftQueue();
	nbuckets = 0;

	effect->init_effect();
	aq_add_count = 0;
}

void AudioQueue::setSoftQueue(double soft_buff_time, double fill_start_time)
{
	int nb;

	/* for re-initialize */
	if (soft_buff_time < 0)
		soft_buff_time = last_soft_buff_time;
	if (fill_start_time < 0)
		fill_start_time = last_fill_start_time;

	nb = (int)(soft_buff_time / bucket_time);
	if (nb == 0)
		aq_start_count = 0;
	else
		aq_start_count = (int32_t)(fill_start_time * playMode->rate);
	aq_fill_buffer_flag = (aq_start_count > 0);

	if (nbuckets != nb)
	{
		nbuckets = nb;
		allocSoftQueue();
	}

	last_soft_buff_time = soft_buff_time;
	last_fill_start_time = fill_start_time;
}

/* Send audio data to playMode->output_data() */
int AudioQueue::outputData(char *buff, int nbytes)
{
	int i;

	play_counter += nbytes / Bps;

	while (nbytes > 0)
	{
		i = nbytes;
		if (i > bucket_size)
			i = bucket_size;
		if (playMode->output_data(buff, i) == -1)
			return -1;
		nbytes -= i;
		buff += i;
	}

	return 0;
}

int AudioQueue::add(int32_t *samples, int32_t count)
{
	int32_t nbytes, i;
	char *buff;

	if (!count)
	{
		return 0;
	}

	aq_add_count += count;
	effect->do_effect(samples, count);
	nbytes = general_output_convert(samples, count);
	buff = (char *)samples;

	if (device_qsize == 0)
		return playMode->output_data(buff, nbytes);

	aq_fill_buffer_flag = (aq_add_count <= aq_start_count);

	while ((i = addPlayBucket(buff, nbytes)) < nbytes)
	{
		buff += i;
		nbytes -= i;
		if (head && head->len == bucket_size)
		{
			if (fillOne() == -1)
				return -1;
		}
		aq_fill_buffer_flag = 0;
	}
	return 0;
}

/* alloc_soft_queue() (re-)initializes audio buckets. */
void AudioQueue::allocSoftQueue(void)
{
	int i;
	char *base;

	freeSoftQueue();

	base_buckets = new AudioBucket[nbuckets];
	base = new char[nbuckets * bucket_size];
	for (i = 0; i < nbuckets; i++)
		base_buckets[i].data = base + i * bucket_size;
	flushBuckets();
}

void AudioQueue::freeSoftQueue(void)
{
	if (base_buckets)
	{
		delete[] base_buckets[0].data;
		delete[] base_buckets;
		base_buckets = NULL;
	}
}

/* aq_fill_one() transfers one audio bucket to device. */
int AudioQueue::fillOne(void)
{
	AudioBucket *tmp;

	if (head == NULL)
		return 0;
	if (outputData(head->data, bucket_size) == -1)
		return -1;
	tmp = head;
	head = head->next;
	reuseAudioBucket(tmp);
	return 0;
}

int32_t AudioQueue::softFilled(void)
{
	int32_t bytes;
	AudioBucket *cur;

	bytes = 0;
	for (cur = head; cur != NULL; cur = cur->next)
		bytes += cur->len;
	return bytes / Bps;
}

int AudioQueue::softFlush(void)
{
	while (head)
	{
		if (head->len < bucket_size)
		{
			/* Add silence code */
			memset(head->data + head->len, 0, bucket_size - head->len);
			head->len = bucket_size;
		}
		if (fillOne() == -1)
			return RC_ERROR;
	}
	return RC_OK;
}

int AudioQueue::flush(int discard)
{
	aq_add_count = 0;
	effect->init_effect();

	if (discard)
	{
		if (playMode->acntl(PM_REQ_DISCARD, NULL) != -1)
		{
			flushBuckets();
			return RC_OK;
		}
	}

	playMode->acntl(PM_REQ_FLUSH, NULL);
	flushBuckets();
	return RC_OK;
}

/* add_play_bucket() attempts to add buf to audio bucket.
	* It returns actually added bytes.
	*/
int AudioQueue::addPlayBucket(const char *buf, int n)
{
	int total;

	if (n == 0)
		return 0;

	if (!nbuckets) {
		playMode->output_data((char *)buf, n);
		return n;
	}

	if (head == NULL)
		head = tail = nextAllocatedBucket();

	total = 0;
	while (n > 0)
	{
		int i;

		if (tail->len == bucket_size)
		{
			AudioBucket *b;
			if ((b = nextAllocatedBucket()) == NULL)
				break;
			if (head == NULL)
				head = tail = b;
			else
				tail = tail->next = b;
		}

		i = bucket_size - tail->len;
		if (i > n)
			i = n;
		memcpy(tail->data + tail->len, buf + total, i);
		total += i;
		n -= i;
		tail->len += i;
	}

	return total;
}

/* Flush and clear audio bucket */
void AudioQueue::flushBuckets(void)
{
	int i;

	allocated_bucket_list = NULL;
	for (i = 0; i < nbuckets; i++)
		reuseAudioBucket(&base_buckets[i]);
	head = tail = NULL;
	aq_fill_buffer_flag = (aq_start_count > 0);
	play_counter = play_offset_counter = 0;
}

/* next_allocated_bucket() gets free bucket.  If all buckets is used, it
	* returns NULL.
	*/
AudioQueue::AudioBucket *AudioQueue::nextAllocatedBucket(void)
{
	AudioBucket *b;

	if (allocated_bucket_list == NULL)
		return NULL;
	b = allocated_bucket_list;
	allocated_bucket_list = allocated_bucket_list->next;
	b->len = 0;
	b->next = NULL;
	return b;
}

/* Reuse specified bucket */
void AudioQueue::reuseAudioBucket(AudioBucket *bucket)
{
	bucket->next = allocated_bucket_list;
	allocated_bucket_list = bucket;
}
}