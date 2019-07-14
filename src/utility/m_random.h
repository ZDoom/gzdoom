/*
** m_random.h
** Random number generators
**
**---------------------------------------------------------------------------
** Copyright 2002-2009 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#ifndef __M_RANDOM__
#define __M_RANDOM__

#include <stdio.h>
#include "basictypes.h"
#include "sfmt/SFMT.h"

class FSerializer;

class FRandom
{
public:
	FRandom ();
	FRandom (const char *name);
	~FRandom ();

	// Returns a random number in the range [0,255]
	int operator()()
	{
		return GenRand32() & 255;
	}

	// Returns a random number in the range [0,mod)
	int operator() (int mod)
	{
		return (0 == mod)
			? 0
			: (GenRand32() % mod);
	}

	// Returns rand# - rand#
	int Random2()
	{
		return Random2(255);
	}

// Returns (rand# & mask) - (rand# & mask)
	int Random2(int mask)
	{
		int t = GenRand32() & mask & 255;
		return t - (GenRand32() & mask & 255);
	}

	// HITDICE macro used in Heretic and Hexen
	int HitDice(int count)
	{
		return (1 + (GenRand32() & 7)) * count;
	}

	int Random()				// synonym for ()
	{
		return operator()();
	}

	void Init(uint32_t seed);

	// SFMT interface
	unsigned int GenRand32();
	uint64_t GenRand64();
	void FillArray32(uint32_t *array, int size);
	void FillArray64(uint64_t *array, int size);
	void InitGenRand(uint32_t seed);
	void InitByArray(uint32_t *init_key, int key_length);
	int GetMinArraySize32();
	int GetMinArraySize64();

	/* These real versions are due to Isaku Wada */
	/** generates a random number on [0,1]-real-interval */
	static inline double ToReal1(uint32_t v)
	{
		return v * (1.0/4294967295.0); 
		/* divided by 2^32-1 */ 
	}

	/** generates a random number on [0,1]-real-interval */
	inline double GenRand_Real1()
	{
		return ToReal1(GenRand32());
	}

	/** generates a random number on [0,1)-real-interval */
	static inline double ToReal2(uint32_t v)
	{
		return v * (1.0/4294967296.0); 
		/* divided by 2^32 */
	}

	/** generates a random number on [0,1)-real-interval */
	inline double GenRand_Real2()
	{
		return ToReal2(GenRand32());
	}

	/** generates a random number on (0,1)-real-interval */
	static inline double ToReal3(uint32_t v)
	{
		return (((double)v) + 0.5)*(1.0/4294967296.0); 
		/* divided by 2^32 */
	}

	/** generates a random number on (0,1)-real-interval */
	inline double GenRand_Real3(void)
	{
		return ToReal3(GenRand32());
	}
	/** These real versions are due to Isaku Wada */

	/** generates a random number on [0,1) with 53-bit resolution*/
	static inline double ToRes53(uint64_t v) 
	{ 
		return v * (1.0/18446744073709551616.0L);
	}

	/** generates a random number on [0,1) with 53-bit resolution from two
	 * 32 bit integers */
	static inline double ToRes53Mix(uint32_t x, uint32_t y) 
	{ 
		return ToRes53(x | ((uint64_t)y << 32));
	}

	/** generates a random number on [0,1) with 53-bit resolution
	 */
	inline double GenRand_Res53(void) 
	{ 
		return ToRes53(GenRand64());
	} 

	/** generates a random number on [0,1) with 53-bit resolution
		using 32bit integer.
	 */
	inline double GenRand_Res53_Mix() 
	{ 
		uint32_t x, y;

		x = GenRand32();
		y = GenRand32();
		return ToRes53Mix(x, y);
	}

	// Static interface
	static void StaticClearRandom ();
	static uint32_t StaticSumSeeds ();
	static void StaticReadRNGState (FSerializer &arc);
	static void StaticWriteRNGState (FSerializer &file);
	static FRandom *StaticFindRNG(const char *name);

#ifndef NDEBUG
	static void StaticPrintSeeds ();
#endif

private:
#ifndef NDEBUG
	const char *Name;
#endif
	FRandom *Next;
	uint32_t NameCRC;

	static FRandom *RNGList;

	/*-------------------------------------------
	  SFMT internal state, index counter and flag 
	  -------------------------------------------*/

	void GenRandAll();
	void GenRandArray(w128_t *array, int size);
	void PeriodCertification();

	/** the 128-bit internal state array */
	union
	{
		w128_t w128[SFMT::N];
		unsigned int u[SFMT::N32];
		uint64_t u64[SFMT::N64];
	} sfmt;
	/** index counter to the 32-bit internal state array */
	int idx;
	/** a flag: it is 0 if and only if the internal state is not yet
	 * initialized. */
#ifndef NDEBUG
	bool initialized;
#endif
};

extern uint32_t rngseed;			// The starting seed (not part of state)

extern uint32_t staticrngseed;		// Static rngseed that can be set by the user
extern bool use_staticrng;


// M_Random can be used for numbers that do not affect gameplay
extern FRandom M_Random;

#endif
