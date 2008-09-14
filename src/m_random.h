// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id: m_random.h,v 1.9 1998/05/01 14:20:31 killough Exp $
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
// DESCRIPTION:
//
//		[RH] We now use BOOM's random number generator
//
//-----------------------------------------------------------------------------


#ifndef __M_RANDOM__
#define __M_RANDOM__

#include <stdio.h>
#include "basictypes.h"

// killough 1/19/98: rewritten to use a better random number generator
// in the new engine, although the old one is available for compatibility.

// killough 2/16/98:
//
// Make every random number generator local to each control-equivalent block.
// Critical for demo sync. Changing the order of this list breaks all previous
// versions' demos. The random number generators are made local to reduce the
// chances of sync problems. In Doom, if a single random number generator call
// was off, it would mess up all random number generators. This reduces the
// chances of it happening by making each RNG local to a control flow block.
//
// Notes to developers: if you want to reduce your demo sync hassles, follow
// this rule: for each call to P_Random you add, add a new class to the enum
// type below for each block of code which calls P_Random. If two calls to
// P_Random are not in "control-equivalent blocks", i.e. there are any cases
// where one is executed, and the other is not, put them in separate classes.
//
// Keep all current entries in this list the same, and in the order
// indicated by the #'s, because they're critical for preserving demo
// sync. Do not remove entries simply because they become unused later.
//
// [RH] Changed to use different class instances for different generators.
// This makes adding new RNGs easier, because I don't need to recompile every
// file that uses random numbers.

struct PNGHandle;

class FRandom
{
public:
	FRandom ();
	FRandom (const char *name);
	~FRandom ();

	int operator() ();			// Returns a random number in the range [0,255]
	int operator() (int mod);	// Returns a random number in the range [0,mod)
	int Random2();				// Returns rand# - rand#
	int Random2(int mask);		// Returns (rand# & mask) - (rand# & mask)
	int HitDice(int count);		// HITDICE macro used in Heretic and Hexen

	int Random()				// synonym for ()
	{
		return operator()();
	}

	DWORD GetSeed()
	{
		return Seed;
	}

	static void StaticClearRandom ();
	static DWORD StaticSumSeeds ();
	static void StaticReadRNGState (PNGHandle *png);
	static void StaticWriteRNGState (FILE *file);
	static FRandom *StaticFindRNG(const char *name);

#ifdef _DEBUG
	static void StaticPrintSeeds ();
#endif

private:
	DWORD Seed;
	FRandom *Next;
	DWORD NameCRC;

#ifdef _DEBUG
	const char *Name;
#endif

	static FRandom *RNGList;
};

extern DWORD rngseed;			// The starting seed (not part of state)

// M_Random can be used for numbers that do not affect gameplay
extern FRandom M_Random;

#endif
