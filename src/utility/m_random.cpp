/*
** m_random.cpp
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
** This file employs the techniques for improving demo sync and backward
** compatibility that Lee Killough introduced with BOOM. However, none of
** the actual code he wrote is left. In contrast to BOOM, each RNG source
** in ZDoom is implemented as a separate class instance that provides an
** interface to the high-quality Mersenne Twister. See
** <http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/SFMT/index.html>.
**
** As Killough's description from m_random.h is still mostly relevant,
** here it is:
**   killough 2/16/98:
**
**   Make every random number generator local to each control-equivalent block.
**   Critical for demo sync. The random number generators are made local to
**   reduce the chances of sync problems. In Doom, if a single random number
**   generator call was off, it would mess up all random number generators.
**   This reduces the chances of it happening by making each RNG local to a
**   control flow block.
**
**   Notes to developers: if you want to reduce your demo sync hassles, follow
**   this rule: for each call to P_Random you add, add a new class to the enum
**   type below for each block of code which calls P_Random. If two calls to
**   P_Random are not in "control-equivalent blocks", i.e. there are any cases
**   where one is executed, and the other is not, put them in separate classes.
*/

// HEADER FILES ------------------------------------------------------------

#include <assert.h>

#include "doomstat.h"
#include "m_random.h"
#include "serializer.h"
#include "m_crc32.h"
#include "c_dispatch.h"

// MACROS ------------------------------------------------------------------

#define RAND_ID MAKE_ID('r','a','N','d')

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern FRandom pr_spawnmobj;
extern FRandom pr_acs;
extern FRandom pr_chase;
extern FRandom pr_exrandom;
extern FRandom pr_damagemobj;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

FRandom M_Random;

// Global seed. This is modified predictably to initialize every RNG.
uint32_t rngseed;

// Static RNG marker. This is only used when the RNG is set for each new game.
uint32_t staticrngseed;
bool use_staticrng;

// Allows checking or staticly setting the global seed.
CCMD(rngseed)
{
	if (argv.argc() == 1)
	{
		Printf("Usage: rngseed get|set|clear\n");
		return;
	}
	if (stricmp(argv[1], "get") == 0)
	{
		Printf("rngseed is %d\n", rngseed);
	}
	else if (stricmp(argv[1], "set") == 0)
	{
		if (argv.argc() == 2)
		{
			Printf("You need to specify a value to set\n");
		}
		else
		{
			staticrngseed = atoi(argv[2]);
			use_staticrng = true;
			Printf("Static rngseed %d will be set for next game\n", staticrngseed);
		}
	}
	else if (stricmp(argv[1], "clear") == 0)
	{
		use_staticrng = false;
		Printf("Static rngseed cleared\n");
	}
}

// PRIVATE DATA DEFINITIONS ------------------------------------------------

FRandom *FRandom::RNGList;
static TDeletingArray<FRandom *> NewRNGs;

// CODE --------------------------------------------------------------------

//==========================================================================
//
// FRandom - Nameless constructor
//
// Constructing an RNG in this way means it won't be stored in savegames.
//
//==========================================================================

FRandom::FRandom ()
: NameCRC (0)
{
#ifndef NDEBUG
	Name = NULL;
	initialized = false;
#endif
	Next = RNGList;
	RNGList = this;
	Init(0);
}

//==========================================================================
//
// FRandom - Named constructor
//
// This is the standard way to construct RNGs.
//
//==========================================================================

FRandom::FRandom (const char *name)
{
	NameCRC = CalcCRC32 ((const uint8_t *)name, (unsigned int)strlen (name));
#ifndef NDEBUG
	initialized = false;
	Name = name;
	// A CRC of 0 is reserved for nameless RNGs that don't get stored
	// in savegames. The chance is very low that you would get a CRC of 0,
	// but it's still possible.
	assert (NameCRC != 0);
#endif

	// Insert the RNG in the list, sorted by CRC
	FRandom **prev = &RNGList, *probe = RNGList;

	while (probe != NULL && probe->NameCRC < NameCRC)
	{
		prev = &probe->Next;
		probe = probe->Next;
	}

#ifndef NDEBUG
	if (probe != NULL)
	{
		// Because RNGs are identified by their CRCs in save games,
		// no two RNGs can have names that hash to the same CRC.
		// Obviously, this means every RNG must have a unique name.
		assert (probe->NameCRC != NameCRC);
	}
#endif

	Next = probe;
	*prev = this;
	Init(0);
}

//==========================================================================
//
// FRandom - Destructor
//
//==========================================================================

FRandom::~FRandom ()
{
	FRandom *rng, **prev;

	FRandom *last = NULL;

	prev = &RNGList;
	rng = RNGList;

	while (rng != NULL && rng != this)
	{
		last = rng;
		rng = rng->Next;
	}

	if (rng != NULL)
	{
		*prev = rng->Next;
	}
}

//==========================================================================
//
// FRandom :: StaticClearRandom
//
// Initialize every RNGs. RNGs are seeded based on the global seed and their
// name, so each different RNG can have a different starting value despite
// being derived from a common global seed.
//
//==========================================================================

void FRandom::StaticClearRandom ()
{
	// go through each RNG and set each starting seed differently
	for (FRandom *rng = FRandom::RNGList; rng != NULL; rng = rng->Next)
	{
		rng->Init(rngseed);
	}
}

//==========================================================================
//
// FRandom :: Init
//
// Initialize a single RNG with a given seed.
//
//==========================================================================

void FRandom::Init(uint32_t seed)
{
	// [RH] Use the RNG's name's CRC to modify the original seed.
	// This way, new RNGs can be added later, and it doesn't matter
	// which order they get initialized in.
	uint32_t seeds[2] = { NameCRC, seed };
	InitByArray(seeds, 2);
}

//==========================================================================
//
// FRandom :: StaticSumSeeds
//
// This function produces a uint32_t that can be used to check the consistancy
// of network games between different machines. Only a select few RNGs are
// used for the sum, because not all RNGs are important to network sync.
//
//==========================================================================

uint32_t FRandom::StaticSumSeeds ()
{
	return
		pr_spawnmobj.sfmt.u[0] + pr_spawnmobj.idx +
		pr_acs.sfmt.u[0] + pr_acs.idx +
		pr_chase.sfmt.u[0] + pr_chase.idx +
		pr_damagemobj.sfmt.u[0] + pr_damagemobj.idx;
}

//==========================================================================
//
// FRandom :: StaticWriteRNGState
//
// Stores the state of every RNG into a savegame.
//
//==========================================================================

void FRandom::StaticWriteRNGState (FSerializer &arc)
{
	FRandom *rng;

	arc("rngseed", rngseed);

	if (arc.BeginArray("rngs"))
	{
		for (rng = FRandom::RNGList; rng != NULL; rng = rng->Next)
		{
			// Only write those RNGs that have names
			if (rng->NameCRC != 0)
			{
				if (arc.BeginObject(nullptr))
				{
					arc("crc", rng->NameCRC)
						("index", rng->idx)
						.Array("u", rng->sfmt.u, SFMT::N32)
						.EndObject();
				}
			}
		}
		arc.EndArray();
	}
}

//==========================================================================
//
// FRandom :: StaticReadRNGState
//
// Restores the state of every RNG from a savegame. RNGs that were added
// since the savegame was created are cleared to their initial value.
//
//==========================================================================

void FRandom::StaticReadRNGState(FSerializer &arc)
{
	FRandom *rng;

	arc("rngseed", rngseed);

	// Call StaticClearRandom in order to ensure that SFMT is initialized
	FRandom::StaticClearRandom ();

	if (arc.BeginArray("rngs"))
	{
		int count = arc.ArraySize();

		for (int i = 0; i < count; i++)
		{
			if (arc.BeginObject(nullptr))
			{
				uint32_t crc;
				arc("crc", crc);

				for (rng = FRandom::RNGList; rng != NULL; rng = rng->Next)
				{
					if (rng->NameCRC == crc)
					{
						arc("index", rng->idx)
							.Array("u", rng->sfmt.u, SFMT::N32);
						break;
					}
				}
				arc.EndObject();
			}
		}
		arc.EndArray();
	}
}

//==========================================================================
//
// FRandom :: StaticFindRNG
//
// This function attempts to find an RNG with the given name.
// If it can't it will create a new one. Duplicate CRCs will
// be ignored and if it happens map to the same RNG.
// This is for use by DECORATE.
//
//==========================================================================

FRandom *FRandom::StaticFindRNG (const char *name)
{
	uint32_t NameCRC = CalcCRC32 ((const uint8_t *)name, (unsigned int)strlen (name));

	// Use the default RNG if this one happens to have a CRC of 0.
	if (NameCRC == 0) return &pr_exrandom;

	// Find the RNG in the list, sorted by CRC
	FRandom **prev = &RNGList, *probe = RNGList;

	while (probe != NULL && probe->NameCRC < NameCRC)
	{
		prev = &probe->Next;
		probe = probe->Next;
	}
	// Found one so return it.
	if (probe == NULL || probe->NameCRC != NameCRC)
	{
		// A matching RNG doesn't exist yet so create it.
		probe = new FRandom(name);

		// Store the new RNG for destruction when ZDoom quits.
		NewRNGs.Push(probe);
	}
	return probe;
}

//==========================================================================
//
// FRandom :: StaticPrintSeeds
//
// Prints a snapshot of the current RNG states. This is probably wrong.
//
//==========================================================================

#ifndef NDEBUG
void FRandom::StaticPrintSeeds ()
{
	FRandom *rng = RNGList;

	while (rng != NULL)
	{
		int idx = rng->idx < SFMT::N32 ? rng->idx : 0;
		Printf ("%s: %08x .. %d\n", rng->Name, rng->sfmt.u[idx], idx);
		rng = rng->Next;
	}
}

CCMD (showrngs)
{
	FRandom::StaticPrintSeeds ();
}
#endif

