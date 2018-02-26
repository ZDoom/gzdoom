#pragma once

#include <stdint.h>
#include "m_random.h"

#include "timidity.h"

namespace TimidityPlus
{

class Reverb;

class Effect
{
	void effect_left_right_delay(int32_t *, int32_t);
	void init_mtrand(void);
	uint32_t frand(void);
	int32_t my_mod(int32_t, int32_t);

	int turn_counter = 0, tc = 0;
	int status = 0;
	double rate0 = 0, rate1 = 0, dr = 0;
	int32_t prev[AUDIO_BUFFER_SIZE * 2] = { 0 };

	FRandom rng;
	Reverb *reverb;

public:
	Effect(Reverb *_reverb)
	{
		reverb = _reverb;
		init_effect();
	}

	void init_effect();
	void do_effect(int32_t *buf, int32_t count);

};

}