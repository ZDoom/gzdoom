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
//-----------------------------------------------------------------------------


#include "doomstat.h"
#include "m_random.h"
#include "farchive.h"
#include "b_bot.h"
//
// M_Random
// Returns a 0-255 number
//
rng_t rng;     // the random number state

DWORD rngseed = 1993;   // killough 3/26/98: The seed

int P_Random (pr_class_t pr_class)
{
	// killough 2/16/98:  We always update both sets of random number
	// generators, to ensure repeatability if the demo_compatibility
	// flag is changed while the program is running. Changing the
	// demo_compatibility flag does not change the sequences generated,
	// only which one is selected from.
	//
	// All of this RNG stuff is tricky as far as demo sync goes --
	// it's like playing with explosives :) Lee

	DWORD boom = rng.seed[pr_class];

	rng.seed[pr_class] = boom * 1664525ul + 221297ul + pr_class*2;

	return (boom >> 20) & 255;
}

// Initialize all the seeds
//
// This initialization method is critical to maintaining demo sync.
// Each seed is initialized according to its class, so if new classes
// are added they must be added to end of pr_class_t list. killough
//

void M_ClearRandom (void)
{
	int i;
	DWORD seed = rngseed*2+1;	// add 3/26/98: add rngseed
	for (i = 0; i < NUMPRCLASS; i++)	// go through each pr_class and set
		rng.seed[i] = seed *= 69069ul;	// each starting seed differently
}

void P_SerializeRNGState (FArchive &arc)
{
	int i;

	if (arc.IsStoring ())
	{
		for (i = 0; i < NUMPRCLASS; i++)
			arc << rng.seed[i];
	}
	else
	{
		for (i = 0; i < NUMPRCLASS; i++)
			arc >> rng.seed[i];
	}
}

//----------------------------------------------------------------------------
//
// $Log: m_random.c,v $
// Revision 1.6  1998/05/03  23:13:18  killough
// Fix #include
//
// Revision 1.5  1998/03/31  10:43:05  killough
// Fix (supposed) RNG problems, add new demo_insurance
//
// Revision 1.4  1998/03/28  17:56:05  killough
// Improve RNG by adding external seed
//
// Revision 1.3  1998/02/17  05:40:08  killough
// Make RNGs local to each calling block, for demo sync
//
// Revision 1.2  1998/01/26  19:23:51  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:58  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
