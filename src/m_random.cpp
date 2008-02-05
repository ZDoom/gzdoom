// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id: m_random.c,v 1.6 1998/05/03 23:13:18 killough Exp $
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
//
// DESCRIPTION:
//      Random number LUT.
//
// 1/19/98 killough: Rewrote random number generator for better randomness,
// while at the same time maintaining demo sync and backward compatibility.
//
// 2/16/98 killough: Made each RNG local to each control-equivalent block,
// to reduce the chances of demo sync problems.
//
// [RH] Changed to use different class instances for different RNGs. Be
// sure to compile with _DEBUG if you want to catch bad RNG names.
//
//-----------------------------------------------------------------------------

#include <assert.h>

#include "doomstat.h"
#include "m_random.h"
#include "farchive.h"
#include "b_bot.h"
#include "m_png.h"
#include "m_crc32.h"
#include "i_system.h"
#include "c_dispatch.h"
#include "files.h"

#define RAND_ID MAKE_ID('r','a','N','d')

FRandom M_Random;

inline int UpdateSeed (DWORD &seed)
{
	DWORD oldseed = seed;
	seed = oldseed * 1664525ul + 221297ul;
	return (int)oldseed;
}

DWORD rngseed = 1993;   // killough 3/26/98: The seed

FRandom *FRandom::RNGList;

FRandom::FRandom ()
: Seed (0), Next (NULL), NameCRC (0)
{
#ifdef _DEBUG
	Name = NULL;
#endif
	Next = RNGList;
	RNGList = this;
}

FRandom::FRandom (const char *name)
: Seed (0)
{
	NameCRC = CalcCRC32 ((const BYTE *)name, (unsigned int)strlen (name));
#ifdef _DEBUG
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

#ifdef _DEBUG
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
}

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

int FRandom::operator() ()
{
	return (UpdateSeed (Seed) >> 20) & 255;
}

int FRandom::operator() (int mod)
{
	if (mod <= 256)
	{ // The mod is small enough, so a byte is enough to get a good number.
		return (*this)() % mod;
	}
	else
	{ // For mods > 256, construct a 32-bit int and modulo that.
		int num = (*this)();
		num = (num << 8) | (*this)();
		num = (num << 8) | (*this)();
		num = (num << 8) | (*this)();
		return (num&0x7fffffff) % mod;
	}
}

int FRandom::Random2 ()
{
	int t = (*this)();
	int u = (*this)();
	return t - u;
}

int FRandom::Random2 (int mask)
{
	int t = (*this)() & mask;
	int u = (*this)() & mask;
	return t - u;
}

int FRandom::HitDice (int count)
{
	return (1 + ((UpdateSeed (Seed) >> 20) & 7)) * count;
}

// Initialize all the seeds
//
// This initialization method is critical to maintaining demo sync.
// Each seed is initialized according to its class. killough
//

void FRandom::StaticClearRandom ()
{
	const DWORD seed = rngseed*2+1;	// add 3/26/98: add rngseed
	FRandom *rng = FRandom::RNGList;

	// go through each RNG and set each starting seed differently
	while (rng != NULL)
	{
		// [RH] Use the RNG's name's CRC to modify the original seed.
		// This way, new RNGs can be added later, and it doesn't matter
		// which order they get initialized in.
		rng->Seed = seed * rng->NameCRC;
		rng = rng->Next;
	}
}

// This function produces a DWORD that can be used to check the consistancy
// of network games between different machines. Only a select few RNGs are
// used for the sum, because not all RNGs are important to network sync.

extern FRandom pr_spawnmobj;
extern FRandom pr_acs;
extern FRandom pr_chase;
extern FRandom pr_lost;
extern FRandom pr_slam;

DWORD FRandom::StaticSumSeeds ()
{
	return pr_spawnmobj.Seed + pr_acs.Seed + pr_chase.Seed + pr_lost.Seed + pr_slam.Seed;
}

void FRandom::StaticWriteRNGState (FILE *file)
{
	FRandom *rng;
	const DWORD seed = rngseed*2+1;
	FPNGChunkArchive arc (file, RAND_ID);

	arc << rngseed;

	// Only write those RNGs that have been used
	for (rng = FRandom::RNGList; rng != NULL; rng = rng->Next)
	{
		if (rng->NameCRC != 0 && rng->Seed != seed + rng->NameCRC)
		{
			arc << rng->NameCRC << rng->Seed;
		}
	}
}

void FRandom::StaticReadRNGState (PNGHandle *png)
{
	FRandom *rng;

	size_t len = M_FindPNGChunk (png, RAND_ID);

	if (len != 0)
	{
		const int rngcount = (int)((len-4) / 8);
		int i;
		DWORD crc;

		FPNGChunkArchive arc (png->File->GetFile(), RAND_ID, len);

		arc << rngseed;
		FRandom::StaticClearRandom ();

		for (i = rngcount; i; --i)
		{
			arc << crc;
			for (rng = FRandom::RNGList; rng != NULL; rng = rng->Next)
			{
				if (rng->NameCRC == crc)
				{
					arc << rng->Seed;
					break;
				}
			}
		}
		png->File->ResetFilePtr();
	}
}

// This function attempts to find an RNG with the given name.
// If it can't it will create a new one. Duplicate CRCs will
// be ignored and if it happens map to the same RNG.
// This is for use by DECORATE.
extern FRandom pr_exrandom;

static TDeletingArray<FRandom *> NewRNGs;

FRandom *FRandom::StaticFindRNG (const char *name)
{
	DWORD NameCRC = CalcCRC32 ((const BYTE *)name, (unsigned int)strlen (name));

	// Use the default RNG if this one happens to have a CRC of 0.
	if (NameCRC==0) return &pr_exrandom;

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

#ifdef _DEBUG
void FRandom::StaticPrintSeeds ()
{
	FRandom *rng = RNGList;

	while (rng != NULL)
	{
		Printf ("%s: %08x\n", rng->Name, rng->Seed);
		rng = rng->Next;
	}
}

CCMD (showrngs)
{
	FRandom::StaticPrintSeeds ();
}
#endif

