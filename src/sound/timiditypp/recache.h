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
*/

#ifndef ___RECACHE_H_
#define ___RECACHE_H_

#include <stdint.h>

namespace TimidityPlus
{


struct cache_hash
{
    /* cache key */
    int note;
    Sample *sp;

    int32_t cnt;			/* counter */
    double r;			/* size/refcnt */
    Sample *resampled;
    struct cache_hash *next;
};

class Player;

class Recache
{
	Player *player;
	enum
	{
		HASH_TABLE_SIZE = 251,
		MIXLEN = 256,

		MIN_LOOPSTART = MIXLEN,
		MIN_LOOPLEN = 1024,
		MAX_EXPANDLEN = (1024 * 32),

		CACHE_RESAMPLING_OK = 0,
		CACHE_RESAMPLING_NOTOK = 1,
		SORT_THRESHOLD = 20
	};

	struct CNote 
	{
		int32_t on[128];
		struct cache_hash *cache[128];
	};

	CNote channel_note_table[MAX_CHANNELS];
	sample_t *cache_data;
	splen_t cache_data_len;
	struct cache_hash *cache_hash_table[HASH_TABLE_SIZE];
	MBlockList hash_entry_pool;


	void free_cache_data(void);
	double sample_resamp_info(Sample *, int, splen_t *, splen_t *, splen_t *);
	void qsort_cache_array(struct cache_hash **, int32_t, int32_t);
	void insort_cache_array(struct cache_hash **, int32_t);
	int cache_resampling(struct cache_hash *);
	void loop_connect(sample_t *, int32_t, int32_t);

public:

	Recache(Player *p)
	{
        memset(this, 0, sizeof(*this));
		player = p;
		resamp_cache_reset();
	}

	~Recache()
	{
		free_cache_data();
	}

	void resamp_cache_reset(void);
	void resamp_cache_refer_on(Voice *vp, int32_t sample_start);
	void resamp_cache_refer_off(int ch, int note, int32_t sample_end);
	void resamp_cache_refer_alloff(int ch, int32_t sample_end);
	void resamp_cache_create(void);
	struct cache_hash *resamp_cache_fetch(Sample *sp, int note);

};

const int32_t allocate_cache_size = DEFAULT_CACHE_DATA_SIZE;

}
#endif /* ___RECACHE_H_ */
