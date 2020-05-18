#pragma once
#include <stdint.h>
#include "SFMT.h"

struct SFMTObj
{
	void Init(uint32_t seed1, uint32_t seed2)
	{
		uint32_t seeds[2] = { seed1, seed2 };
		InitByArray(seeds, 2);
	}
	void GenRandAll();
	void GenRandArray(w128_t *array, int size);
	void PeriodCertification();
	int GetMinArraySize32();
	int GetMinArraySize64();
	unsigned int GenRand32();
	uint64_t GenRand64();
	void FillArray32(uint32_t *array, int size);
	void FillArray64(uint64_t *array, int size);
	void InitGenRand(uint32_t seed);
	void InitByArray(uint32_t *init_key, int key_length);

protected:	

	/** index counter to the 32-bit internal state array */
	int idx;

	/** the 128-bit internal state array */
	union
	{
		w128_t w128[SFMT::N];
		unsigned int u[SFMT::N32];
		uint64_t u64[SFMT::N64];
	} sfmt;
	
	/** a flag: it is 0 if and only if the internal state is not yet
	 * initialized. */
#ifndef NDEBUG
	bool initialized = false;
#endif
	
};