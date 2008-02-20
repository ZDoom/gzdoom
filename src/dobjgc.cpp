/*
** dobjgc.cpp
** The garbage collector. Based largely on Lua's.
**
**---------------------------------------------------------------------------
** Copyright 2008 Randy Heit
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
/******************************************************************************
* Copyright (C) 1994-2008 Lua.org, PUC-Rio.  All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be
* included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
******************************************************************************/

// HEADER FILES ------------------------------------------------------------

#include "dobject.h"
#include "b_bot.h"
#include "p_local.h"
#include "g_game.h"
#include "r_data.h"
#include "a_sharedglobal.h"
#include "sbar.h"
#include "stats.h"
#include "c_dispatch.h"

// MACROS ------------------------------------------------------------------

/*
@@ DEFAULT_GCPAUSE defines the default pause between garbage-collector cycles
@* as a percentage.
** CHANGE it if you want the GC to run faster or slower (higher values
** mean larger pauses which mean slower collection.) You can also change
** this value dynamically.
*/
#define DEFAULT_GCPAUSE		200	// 200% (wait for memory to double before next GC)

/*
@@ DEFAULT_GCMUL defines the default speed of garbage collection relative to
@* memory allocation as a percentage.
** CHANGE it if you want to change the granularity of the garbage
** collection. (Higher values mean coarser collections. 0 represents
** infinity, where each step performs a full collection.) You can also
** change this value dynamically.
*/
#define DEFAULT_GCMUL		200 // GC runs 'twice the speed' of memory allocation


#define GCSTEPSIZE		1024u
#define GCSWEEPMAX		40
#define GCSWEEPCOST		10
#define GCFINALIZECOST	100

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

namespace GC
{
size_t AllocBytes;
size_t Threshold;
size_t Estimate;
DObject *Gray;
DObject *Root;
DObject **SweepPos;
DWORD CurrentWhite = OF_White0;
EGCState State = GCS_Pause;
int Pause = DEFAULT_GCPAUSE;
int StepMul = DEFAULT_GCMUL;
int StepCount;
ptrdiff_t Dept;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
//
// SetThreshold
//
// Sets the new threshold after a collection is finished.
//
//==========================================================================

void SetThreshold()
{
	Threshold = (Estimate / 100) * Pause;
}

//==========================================================================
//
// PropagateMark
//
// Marks the top-most gray object black and marks all objects it points to
// gray.
//
//==========================================================================

size_t PropagateMark()
{
	DObject *obj = Gray;
	assert(obj->IsGray());
	obj->Gray2Black();
	Gray = obj->GCNext;
	return obj->PropagateMark();
}

//==========================================================================
//
// PropagateAll
//
// Empties the gray list by propagating every single object in it.
//
//==========================================================================

static size_t PropagateAll()
{
	size_t m = 0;
	while (Gray != NULL)
	{
		m += PropagateMark();
	}
	return m;
}

//==========================================================================
//
// SweepList
//
// Runs a limited sweep on a list, returning the location where to resume
// the sweep at next time.
//
//==========================================================================

static DObject **SweepList(DObject **p, size_t count)
{
	DObject *curr;
	int deadmask = OtherWhite();

	while ((curr = *p) != NULL && count-- > 0)
	{
		if ((curr->ObjectFlags ^ OF_WhiteBits) & deadmask)	// not dead?
		{
			assert(!curr->IsDead() || (curr->ObjectFlags & OF_Fixed));
			curr->MakeWhite();	// make it white (for next cycle)
			p = &curr->ObjNext;
		}
		else	// must erase 'curr'
		{
			assert(curr->IsDead());
			*p = curr->ObjNext;
			if (curr == Root)
			{
				Root = curr->ObjNext;
			}
			if (!(curr->ObjectFlags & OF_EuthanizeMe))
			{ // The object must be destroyed before it can be finalized.
				curr->Destroy();
			}
			curr->ObjectFlags |= OF_Cleanup;
			delete curr;
		}
	}
	return p;
}

//==========================================================================
//
// Mark
//
// Mark a single object gray.
//
//==========================================================================

void Mark(DObject **obj)
{
	DObject *lobj = *obj;
	// AActor::LastLook does double-duty as a pointer and a player index! Eww.
	if (lobj != NULL && (size_t)lobj >= MAXPLAYERS)
	{
		if (lobj->ObjectFlags & OF_EuthanizeMe)
		{
			*obj = NULL;
		}
		else if (lobj->IsWhite())
		{
			lobj->White2Gray();
			lobj->GCNext = Gray;
			Gray = lobj;
		}
	}
}

//==========================================================================
//
// MarkRoot
//
// Mark the root set of objects.
//
//==========================================================================

static void MarkRoot()
{
	int i;

	Gray = NULL;
	Mark(Args);
	Mark(screen);
	Mark(StatusBar);
	DThinker::MarkRoots();
	for (i = 0; i < BODYQUESIZE; ++i)
	{
		Mark(bodyque[i]);
	}
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i])
			players[i].PropagateMark();
	}
	if (sectors != NULL)
	{
		for (i = 0; i < numsectors; ++i)
		{
			Mark(sectors[i].SoundTarget);
			Mark(sectors[i].CeilingSkyBox);
			Mark(sectors[i].FloorSkyBox);
			Mark(sectors[i].SecActTarget);
			Mark(sectors[i].floordata);
			Mark(sectors[i].ceilingdata);
			Mark(sectors[i].lightingdata);
		}
	}
	{ // Silly bots
		DObject *foo = &bglobal;
		Mark(foo);
	}
	State = GCS_Propagate;
	StepCount = 0;
}

//==========================================================================
//
// Atomic
//
// If their were any propagations that needed to be done atomicly, they
// would go here. It also sets things up for the sweep state.
//
//==========================================================================

static void Atomic()
{
	// Flip current white
	CurrentWhite = OtherWhite();
	SweepPos = &Root;
	State = GCS_Sweep;
	Estimate = AllocBytes;
}

//==========================================================================
//
// SingleStep
//
// Performs one step of the collector.
//
//==========================================================================

static size_t SingleStep()
{
	switch (State)
	{
	case GCS_Pause:
		MarkRoot();		// Start a new collection
		return 0;

	case GCS_Propagate:
		if (Gray != NULL)
		{
			return PropagateMark();
		}
		else
		{ // no more gray objects
			Atomic();	// finish mark phase
			return 0;
		}

	case GCS_Sweep: {
		size_t old = AllocBytes;
		SweepPos = SweepList(SweepPos, GCSWEEPMAX);
		if (*SweepPos == NULL)
		{ // Nothing more to sweep?
			State = GCS_Finalize;
		}
		assert(old >= AllocBytes);
		Estimate -= old - AllocBytes;
		return GCSWEEPMAX * GCSWEEPCOST;
	  }

	case GCS_Finalize:
		State = GCS_Pause;		// end collection
		Dept = 0;
		return 0;

	default:
		assert(0);
		return 0;
	}
}

//==========================================================================
//
// Step
//
// Performs enough single steps to cover GCSTEPSIZE * StepMul% bytes of
// memory.
//
//==========================================================================

void Step()
{
	size_t lim = (GCSTEPSIZE/100) * StepMul;
	size_t olim;
	if (lim == 0)
	{
		lim = (~(size_t)0) / 2;		// no limit
	}
	Dept += AllocBytes - Threshold;
	do
	{
		olim = lim;
		lim -= SingleStep();
	} while (olim > lim && State != GCS_Pause);
	if (State != GCS_Pause)
	{
		if (Dept < GCSTEPSIZE)
		{
			Threshold = AllocBytes + GCSTEPSIZE;	// - lim/StepMul
		}
		else
		{
			Dept -= GCSTEPSIZE;
			Threshold = AllocBytes;
		}
	}
	else
	{
		assert(AllocBytes >= Estimate);
		SetThreshold();
	}
	StepCount++;
}

//==========================================================================
//
// FullGC
//
// Collects everything in one fell swoop.
//
//==========================================================================

void FullGC()
{
	if (State <= GCS_Propagate)
	{
		// Reset sweep mark to sweep all elements (returning them to white)
		SweepPos = &Root;
		// Reset other collector lists
		Gray = NULL;
		State = GCS_Sweep;
	}
	// Finish any pending sweep phase
	while (State != GCS_Finalize)
	{
		SingleStep();
	}
	MarkRoot();
	while (State != GCS_Pause)
	{
		SingleStep();
	}
	SetThreshold();
}

}

ADD_STAT(mem)
{
	static const char *StateStrings[] = {
		"  Pause  ",
		"Propagate",
		"  Sweep  ",
		"Finalize " };
	FString out;
	out.Format("[%s] Alloc:%6uK  Thresh:%6uK  Est:%6uK  Steps: %d",
		StateStrings[GC::State],
		(GC::AllocBytes + 1023) >> 10,
		(GC::Threshold + 1023) >> 10,
		(GC::Estimate + 1023) >> 10,
		GC::StepCount);
	if (GC::State != GC::GCS_Pause)
	{
		out.AppendFormat("  %+d", GC::Dept);
	}
	return out;
}

//==========================================================================
//
// CCMD gcnow
//
// Initiates a collection. The collection is still done incrementally,
// rather than all at once.
//
//==========================================================================

CCMD(gcnow)
{
	GC::Threshold = GC::AllocBytes;
}
