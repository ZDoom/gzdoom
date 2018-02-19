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

    aq.h - Audio queue.
	   Written by Masanao Izumo <mo@goice.co.jp>
*/

#ifndef ___AQ_H_
#define ___AQ_H_

#include <stdint.h>

namespace TimidityPlus
{

/* interfaces */

class Effect;
class Reverb;
struct PlayMode;

class AudioQueue
{
	struct AudioBucket
	{
		char *data;
		int len;
		AudioBucket *next;
	};

	Effect *effect;
	PlayMode *playMode;

	int buffer_size = 0;
	int32_t device_qsize = 0;
	int Bps = 0;	/* Bytes per sample frame */
	int bucket_size = 0;
	int nbuckets = 0;
	double bucket_time = 0;
	int aq_fill_buffer_flag = 0;
	int32_t aq_start_count = 0;
	int32_t aq_add_count = 0;

	int32_t play_counter = 0, play_offset_counter = 0;
	double play_start_time = 0;

	AudioBucket *base_buckets = NULL;
	AudioBucket *allocated_bucket_list = NULL;
	AudioBucket *head = NULL;
	AudioBucket *tail = NULL;

	void allocSoftQueue(void);
	int addPlayBucket(const char *buf, int n);
	void reuseAudioBucket(AudioBucket *bucket);
	AudioBucket *nextAllocatedBucket(void);
	void flushBuckets(void);
	int fillOne(void);
	int outputData(char *buff, int nbytes);

	double last_soft_buff_time, last_fill_start_time;

public:
	AudioQueue(PlayMode *pm, int buffersize, Reverb *reverb);	// play_mode, audio_buffer_size
	~AudioQueue();

	void setup(); // allocates the buffer for software queue, and estimate maxmum queue size of audio device.
	void setSoftQueue(double soft_buff_time, double fill_start_time); // makes software audio queue. If fill_start_time is positive, TiMidity doesn't start playing immidiately until the autio buffer is filled.
	int add(int32_t *samples, int32_t count); // adds new samples to software queue.  If samples is NULL, only updates internal software queue buffer.
	int32_t softFilled(void); // returns filled queue length of software buffer.
	int flush(int discard); // If discard is true, discards all audio queue and returns immediately, otherwise waits until play all out.
	int softFlush(void); // transfers all buffer to device
	void freeSoftQueue(void); // free soft_que memory

	int fillBufferFlag() // non-zero if aq->add() is in filling mode
	{
		return aq_fill_buffer_flag;
	}
};

}
#endif /* ___AQ_H_ */
