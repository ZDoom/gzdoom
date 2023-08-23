/*
** dobjgc.cpp
** The garbage collector. Based largely on Lua's.
**
**---------------------------------------------------------------------------
** Copyright 2008-2022 Marisa Heit
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

#include "c_dispatch.h"
#include "menu.h"
#include "stats.h"
#include "printf.h"
#include "cmdlib.h"

// MACROS ------------------------------------------------------------------

/*
@@ DEFAULT_GCPAUSE defines the default pause between garbage-collector cycles
@* as a percentage.
** CHANGE it if you want the GC to run faster or slower (higher values
** mean larger pauses which mean slower collection.) You can also change
** this value dynamically.
*/
#define DEFAULT_GCPAUSE		150	// 150% (wait for memory to increase by half before next GC)

/*
@@ DEFAULT_GCMUL defines the default speed of garbage collection relative to
@* memory allocation as a percentage.
** CHANGE it if you want to change the granularity of the garbage
** collection. (Higher values mean coarser collections. 0 represents
** infinity, where each step performs a full collection.) You can also
** change this value dynamically.
*/
#ifndef _DEBUG
#define DEFAULT_GCMUL		600 // GC runs gcmul% the speed of memory allocation
#else
// Higher in debug builds to account for the extra time spent freeing objects
#define DEFAULT_GCMUL		800
#endif

// Minimum step size
#define GCMINSTEPSIZE		(sizeof(DObject) * 16)

// Sweeps traverse objects in chunks of this size
#define GCSWEEPGRANULARITY	40

// Cost of deleting an object
#ifndef _DEBUG
#define GCDELETECOST		75
#else
// Freeing memory is much more costly in debug builds
#define GCDELETECOST		230
#endif

// Cost of destroying an object
#define GCDESTROYCOST		15

// TYPES -------------------------------------------------------------------

class FAveragizer
{
	// Number of allocations to track
	static inline constexpr unsigned HistorySize = 512;

	size_t History[HistorySize];
	size_t TotalAmount;
	int TotalCount;
	unsigned NewestPos;

public:
	FAveragizer();
	void AddAlloc(size_t alloc);
	size_t GetAverage();
};

struct FStepStats
{
	cycle_t Clock[GC::GCS_COUNT];
	size_t BytesCovered[GC::GCS_COUNT];
	int Count[GC::GCS_COUNT];

	void Format(FString &out);
	void Reset();
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static size_t CalcStepSize();

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

namespace GC
{
size_t AllocBytes;
size_t RunningAllocBytes;
size_t RunningDeallocBytes;
size_t Threshold;
size_t Estimate;
DObject *Gray;
DObject *Root;
DObject *SoftRoots;
DObject **SweepPos;
DObject *ToDestroy;
uint32_t CurrentWhite = OF_White0 | OF_Fixed;
EGCState State = GCS_Pause;
int Pause = DEFAULT_GCPAUSE;
int StepMul = DEFAULT_GCMUL;
FStepStats StepStats;
FStepStats PrevStepStats;
bool FinalGC;
bool HadToDestroy;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static FAveragizer AllocHistory;// Tracks allocation rate over time
static cycle_t GCTime;			// Track time spent in GC

// CODE --------------------------------------------------------------------

//==========================================================================
//
// CheckGC
// 
// Check if it's time to collect, and do a collection step if it is.
// Also does some bookkeeping. Should be called fairly consistantly.
//
//==========================================================================

void CheckGC()
{
	AllocHistory.AddAlloc(RunningAllocBytes);
	RunningAllocBytes = 0;
	if (State > GCS_Pause || AllocBytes >= Threshold)
	{
		Step();
	}
}

//==========================================================================
//
// SetThreshold
//
// Sets the new threshold after a collection is finished.
//
//==========================================================================

void SetThreshold()
{
	Threshold = (std::min(Estimate, AllocBytes) / 100) * Pause;
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
	return !(obj->ObjectFlags & OF_EuthanizeMe) ? obj->PropagateMark() :
		obj->GetClass()->Size;
}

//==========================================================================
//
// SweepObjects
//
// Runs a limited sweep on the object list, returning the number of bytes
// swept.
//
//==========================================================================

static size_t SweepObjects(size_t count)
{
	DObject *curr;
	int deadmask = OtherWhite();
	size_t swept = 0;

	while ((curr = *SweepPos) != nullptr && count-- > 0)
	{
		swept += curr->GetClass()->Size;
		if ((curr->ObjectFlags ^ OF_WhiteBits) & deadmask)	// not dead?
		{
			assert(!curr->IsDead() || (curr->ObjectFlags & OF_Fixed));
			curr->MakeWhite();	// make it white (for next cycle)
			SweepPos = &curr->ObjNext;
		}
		else
		{
			assert(curr->IsDead());
			if (!(curr->ObjectFlags & OF_EuthanizeMe))
			{	// The object must be destroyed before it can be deleted.
				curr->GCNext = ToDestroy;
				ToDestroy = curr;
				SweepPos = &curr->ObjNext;
			}
			else
			{	// must erase 'curr'
				*SweepPos = curr->ObjNext;
				curr->ObjectFlags |= OF_Cleanup;
				delete curr;
				swept += GCDELETECOST;
			}
		}
	}
	return swept;
}

//==========================================================================
//
// DestroyObjects
//
// Destroys up to count objects on a list linked on GCNext, returning the
// size of objects destroyed, for updating the estimate.
//
//==========================================================================

static size_t DestroyObjects(size_t count)
{
	DObject *curr;
	size_t bytes_destroyed = 0;

	while ((curr = ToDestroy) != nullptr && count-- > 0)
	{
		assert(!(curr->ObjectFlags & OF_EuthanizeMe));
		bytes_destroyed += curr->GetClass()->Size + GCDESTROYCOST;
		ToDestroy = curr->GCNext;
		curr->GCNext = nullptr;
		curr->Destroy();
	}
	return bytes_destroyed;
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

	//assert(lobj == nullptr || !(lobj->ObjectFlags & OF_Released));
	if (lobj != nullptr && !(lobj->ObjectFlags & OF_Released))
	{
		if (lobj->ObjectFlags & OF_EuthanizeMe)
		{
			*obj = (DObject *)NULL;
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
// MarkArray
//
// Mark an array of objects gray.
//
//==========================================================================

void MarkArray(DObject **obj, size_t count)
{
	for (size_t i = 0; i < count; ++i)
	{
		Mark(obj[i]);
	}
}

//==========================================================================
//
// CalcStepSize
//
// Decide how big a step should be, based on the current allocation rate.
//
//==========================================================================

static size_t CalcStepSize()
{
	size_t avg = AllocHistory.GetAverage();
	return std::max<size_t>(GCMINSTEPSIZE, avg * StepMul / 100);
}

//==========================================================================
//
// MarkRoot
//
// Mark the root set of objects.
//
//==========================================================================

TArray<GCMarkerFunc> markers;
void AddMarkerFunc(GCMarkerFunc func)
{
	if (markers.Find(func) == markers.Size())
		markers.Push(func);
}

static void MarkRoot()
{
	PrevStepStats = StepStats;
	StepStats.Reset();

	Gray = nullptr;

	for (auto func : markers) func();

	// Mark soft roots.
	if (SoftRoots != nullptr)
	{
		DObject **probe = &SoftRoots->ObjNext;
		while (*probe != nullptr)
		{
			DObject *soft = *probe;
			probe = &soft->ObjNext;
			if ((soft->ObjectFlags & (OF_Rooted | OF_EuthanizeMe)) == OF_Rooted)
			{
				Mark(soft);
			}
		}
	}
	// Time to propagate the marks.
	State = GCS_Propagate;
}

//==========================================================================
//
// Atomic
//
// If there were any propagations that needed to be done atomicly, they
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
// SweepDone
// 
// Sets up the Destroy phase, if there are any dead objects that haven't
// been destroyed yet, or skips to the Done state.
//
//==========================================================================

static void SweepDone()
{
	HadToDestroy = ToDestroy != nullptr;
	State = HadToDestroy ? GCS_Destroy : GCS_Done;
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
		if (Gray != nullptr)
		{
			return PropagateMark();
		}
		else
		{ // no more gray objects
			Atomic();	// finish mark phase
			return 0;
		}

	case GCS_Sweep: {
		RunningDeallocBytes = 0;
		size_t swept = SweepObjects(GCSWEEPGRANULARITY);
		Estimate -= RunningDeallocBytes;
		if (*SweepPos == nullptr)
		{ // Nothing more to sweep?
			SweepDone();
		}
		return swept;
	  }

	case GCS_Destroy: {
		size_t destroy_size;
		destroy_size = DestroyObjects(GCSWEEPGRANULARITY);
		Estimate -= destroy_size;
		if (ToDestroy == nullptr)
		{ // Nothing more to destroy?
			State = GCS_Done;
		}
		return destroy_size;
	  }

	case GCS_Done:
		State = GCS_Pause;		// end collection
		SetThreshold();
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
// Performs enough single steps to cover <StepSize> bytes of memory.
// Some of those bytes might be "fake" to account for the cost of freeing
// or destroying object.
//
//==========================================================================

void Step()
{
	GCTime.ResetAndClock();

	auto enter_state = State;
	StepStats.Count[enter_state]++;
	StepStats.Clock[enter_state].Clock();

	size_t did = 0;
	size_t lim = CalcStepSize();

	do
	{
		size_t done = SingleStep();
		did += done;
		if (done < lim)
		{
			lim -= done;
		}
		else
		{
			lim = 0;
		}
		if (State != enter_state)
		{
			// Finish stats on old state
			StepStats.Clock[enter_state].Unclock();
			StepStats.BytesCovered[enter_state] += did;

			// Start stats on new state
			did = 0;
			enter_state = State;
			StepStats.Clock[enter_state].Clock();
			StepStats.Count[enter_state]++;
		}
	} while (lim && State != GCS_Pause);

	StepStats.Clock[enter_state].Unclock();
	StepStats.BytesCovered[enter_state] += did;
	GCTime.Unclock();
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
	bool ContinueCheck = true;
	while (ContinueCheck)
	{
		ContinueCheck = false;
		if (State <= GCS_Propagate)
		{
			// Reset sweep mark to sweep all elements (returning them to white)
			SweepPos = &Root;
			// Reset other collector lists
			Gray = nullptr;
			State = GCS_Sweep;
		}
		// Finish any pending GC stages
		while (State != GCS_Pause)
		{
			SingleStep();
		}
		// Loop until everything that can be destroyed and freed is
		do
		{
			MarkRoot();
			while (State != GCS_Pause)
			{
				SingleStep();
			}
			ContinueCheck |= HadToDestroy;
		} while (HadToDestroy);
	}
}

//==========================================================================
//
// Barrier
//
// Implements a write barrier to maintain the invariant that a black node
// never points to a white node by making the node pointed at gray.
//
//==========================================================================

void Barrier(DObject *pointing, DObject *pointed)
{
	assert(pointing == nullptr || (pointing->IsBlack() && !pointing->IsDead()));
	assert(pointed->IsWhite() && !pointed->IsDead());
	assert(State != GCS_Destroy && State != GCS_Pause);
	assert(!(pointed->ObjectFlags & OF_Released));	// if a released object gets here, something must be wrong.
	if (pointed->ObjectFlags & OF_Released) return;	// don't do anything with non-GC'd objects.
	// The invariant only needs to be maintained in the propagate state.
	if (State == GCS_Propagate)
	{
		pointed->White2Gray();
		pointed->GCNext = Gray;
		Gray = pointed;
	}
	// In other states, we can mark the pointing object white so this
	// barrier won't be triggered again, saving a few cycles in the future.
	else if (pointing != nullptr)
	{
		pointing->MakeWhite();
	}
}

void DelSoftRootHead()
{
	if (SoftRoots != nullptr)
	{
		// Don't let the destructor print a warning message
		SoftRoots->ObjectFlags |= OF_YesReallyDelete;
		delete SoftRoots;
	}
	SoftRoots = nullptr;
}

//==========================================================================
//
// AddSoftRoot
//
// Marks an object as a soft root. A soft root behaves exactly like a root
// in MarkRoot, except it can be added at run-time.
//
//==========================================================================

void AddSoftRoot(DObject *obj)
{
	DObject **probe;

	// Are there any soft roots yet?
	if (SoftRoots == nullptr)
	{
		// Create a new object to root the soft roots off of, and stick
		// it at the end of the object list, so we know that anything
		// before it is not a soft root.
		SoftRoots = Create<DObject>();
		SoftRoots->ObjectFlags |= OF_Fixed;
		probe = &Root;
		while (*probe != nullptr)
		{
			probe = &(*probe)->ObjNext;
		}
		Root = SoftRoots->ObjNext;
		SoftRoots->ObjNext = nullptr;
		*probe = SoftRoots;
	}
	// Mark this object as rooted and move it after the SoftRoots marker.
	probe = &Root;
	while (*probe != nullptr && *probe != obj)
	{
		probe = &(*probe)->ObjNext;
	}
	*probe = (*probe)->ObjNext;
	obj->ObjNext = SoftRoots->ObjNext;
	SoftRoots->ObjNext = obj;
	obj->ObjectFlags |= OF_Rooted;
	WriteBarrier(obj);
}

//==========================================================================
//
// DelSoftRoot
//
// Unroots an object so that it must be reachable or it will get collected.
//
//==========================================================================

void DelSoftRoot(DObject *obj)
{
	DObject **probe;

	if (obj == nullptr || !(obj->ObjectFlags & OF_Rooted))
	{ // Not rooted, so nothing to do.
		return;
	}
	obj->ObjectFlags &= ~OF_Rooted;
	// Move object out of the soft roots part of the list.
	probe = &SoftRoots;
	while (*probe != nullptr && *probe != obj)
	{
		probe = &(*probe)->ObjNext;
	}
	if (*probe == obj)
	{
		*probe = obj->ObjNext;
		obj->ObjNext = Root;
		Root = obj;
	}
}

}

//==========================================================================
//
// FAveragizer - Constructor
//
//==========================================================================

FAveragizer::FAveragizer()
{
	NewestPos = 0;
	TotalAmount = 0;
	TotalCount = 0;
	memset(History, 0, sizeof(History));
}

//==========================================================================
//
// FAveragizer :: AddAlloc
//
//==========================================================================

void FAveragizer::AddAlloc(size_t alloc)
{
	NewestPos = (NewestPos + 1) & (HistorySize - 1);
	if (TotalCount < (int)HistorySize)
	{
		TotalCount++;
	}
	else
	{
		TotalAmount -= History[NewestPos];
	}
	History[NewestPos] = alloc;
	TotalAmount += alloc;
}

//==========================================================================
//
// FAveragizer :: GetAverage
//
//==========================================================================

size_t FAveragizer::GetAverage()
{
	return TotalCount != 0 ? TotalAmount / TotalCount : 0;
}

//==========================================================================
//
// STAT gc
//
// Provides information about the current garbage collector state.
//
//==========================================================================

ADD_STAT(gc)
{
	static const char *StateStrings[] = {
		"  Pause  ",
		"Propagate",
		"  Sweep  ",
		" Destroy ",
		"  Done   "
	};
	FString out;
	double time = GC::State != GC::GCS_Pause ? GC::GCTime.TimeMS() : 0;

	GC::PrevStepStats.Format(out);
	out << "\n";
	GC::StepStats.Format(out);
	out.AppendFormat("\n%.2fms [%s] Rate:%3zuK (%3zuK)  Alloc:%6zuK  Est:%6zuK  Thresh:%6zuK",
		time,
		StateStrings[GC::State],
		(GC::AllocHistory.GetAverage() + 1023) >> 10,
		(GC::CalcStepSize() + 1023) >> 10,
		(GC::AllocBytes + 1023) >> 10,
		(GC::Estimate + 1023) >> 10,
		(GC::Threshold + 1023) >> 10);
	return out;
}

//==========================================================================
//
// FStepStats :: Reset
//
//==========================================================================

void FStepStats::Reset()
{
	for (unsigned i = 0; i < countof(Count); ++i)
	{
		Count[i] = 0;
		BytesCovered[i] = 0;
		Clock[i].Reset();
	}
}

//==========================================================================
//
// FStepStats :: Format
//
// Appends its stats to the given FString.
//
//==========================================================================

void FStepStats::Format(FString &out)
{
	// Because everything in the default green is hard to distinguish,
	// each stage has its own color.
	for (int i = GC::GCS_Propagate; i < GC::GCS_Done; ++i)
	{
		int count = Count[i];
		double time = Clock[i].TimeMS();
		out.AppendFormat(TEXTCOLOR_ESCAPESTR "%c[%c%6zuK %4d*%.2fms]",
			"-NKB"[i],	/* Color codes */
			"-PSD"[i],	/* Stage prefixes: (P)ropagate, (S)weep, (D)estroy */
			(BytesCovered[i] + 1023) >> 10, count, count != 0 ? time / count : time);
	}
	out << TEXTCOLOR_GREEN;
}

//==========================================================================
//
// CCMD gc
//
// Controls various aspects of the collector.
//
//==========================================================================

CCMD(gc)
{
	if (argv.argc() == 1)
	{
		Printf ("Usage: gc stop|now|full|count|pause [size]|stepmul [size]\n");
		return;
	}
	if (stricmp(argv[1], "stop") == 0)
	{
		GC::Threshold = ~(size_t)0 - 2;
	}
	else if (stricmp(argv[1], "now") == 0)
	{
		GC::Threshold = GC::AllocBytes;
	}
	else if (stricmp(argv[1], "full") == 0)
	{
		GC::FullGC();
	}
	else if (stricmp(argv[1], "count") == 0)
	{
		int cnt = 0;
		for (DObject *obj = GC::Root; obj; obj = obj->ObjNext, cnt++);
		Printf("%d active objects counted\n", cnt);
	}
	else if (stricmp(argv[1], "pause") == 0)
	{
		if (argv.argc() == 2)
		{
			Printf ("Current GC pause is %d\n", GC::Pause);
		}
		else
		{
			GC::Pause = max(1,atoi(argv[2]));
		}
	}
	else if (stricmp(argv[1], "stepmul") == 0)
	{
		if (argv.argc() == 2)
		{
			Printf ("Current GC stepmul is %d\n", GC::StepMul);
		}
		else
		{
			GC::StepMul = max(100, atoi(argv[2]));
		}
	}
}

