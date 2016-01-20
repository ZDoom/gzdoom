/*
** a_specialspot.cpp
** Handling for special spot actors
** like BrainTargets, MaceSpawners etc.
**
**---------------------------------------------------------------------------
** Copyright 2008 Christoph Oelckers
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
#include "a_specialspot.h"
#include "m_random.h"
#include "p_local.h"
#include "statnums.h"
#include "i_system.h"
#include "thingdef/thingdef.h"
#include "doomstat.h"
#include "farchive.h"

static FRandom pr_spot ("SpecialSpot");
static FRandom pr_spawnmace ("SpawnMace");


IMPLEMENT_CLASS(DSpotState)
IMPLEMENT_CLASS (ASpecialSpot)
TObjPtr<DSpotState> DSpotState::SpotState;

//----------------------------------------------------------------------------
//
// Spot list
//
//----------------------------------------------------------------------------

struct FSpotList
{
	const PClass *Type;
	TArray<ASpecialSpot*> Spots;
	unsigned Index;
	int SkipCount;
	int numcalls;

	FSpotList()
	{
	}

	FSpotList(const PClass *type)
	{
		Type = type;
		Index = 0;
		SkipCount = 0;
		numcalls = 0;
	}

	//----------------------------------------------------------------------------
	//
	// 
	//
	//----------------------------------------------------------------------------

	void Serialize(FArchive &arc)
	{
		arc << Type << Spots << Index << SkipCount << numcalls;
	}

	//----------------------------------------------------------------------------
	//
	// 
	//
	//----------------------------------------------------------------------------

	bool Add(ASpecialSpot *newspot)
	{
		for(unsigned i = 0; i < Spots.Size(); i++)
		{
			if (Spots[i] == newspot) return false;
		}
		Spots.Push(newspot);
		return true;
	}

	//----------------------------------------------------------------------------
	//
	// 
	//
	//----------------------------------------------------------------------------

	bool Remove(ASpecialSpot *spot)
	{
		for(unsigned i = 0; i < Spots.Size(); i++)
		{
			if (Spots[i] == spot) 
			{
				Spots.Delete(i);
				if (Index > i) Index--;
				return true;
			}
		}
		return false;
	}

	//----------------------------------------------------------------------------
	//
	// 
	//
	//----------------------------------------------------------------------------

	ASpecialSpot *GetNextInList(int skipcounter)
	{
		if (Spots.Size() > 0 && ++SkipCount > skipcounter)
		{
			SkipCount = 0;

			ASpecialSpot *spot = Spots[Index];
			if (++Index >= Spots.Size()) Index = 0;
			numcalls++;
			return spot;
		}
		return NULL;
	}

	//----------------------------------------------------------------------------
	//
	// 
	//
	//----------------------------------------------------------------------------

	ASpecialSpot *GetSpotWithMinMaxDistance(fixed_t x, fixed_t y, fixed_t mindist, fixed_t maxdist)
	{
		if (Spots.Size() == 0) return NULL;
		int i = pr_spot() % Spots.Size();
		int initial = i;

		fixed_t distance;

		while (true)
		{
			distance = Spots[i]->AproxDistance(x, y);

			if ((distance >= mindist) && ((maxdist == 0) || (distance <= maxdist))) break;

			i = (i+1) % Spots.Size();
			if (i == initial) return NULL;
		}
		numcalls++;
		return Spots[i];
	}

	//----------------------------------------------------------------------------
	//
	// 
	//
	//----------------------------------------------------------------------------

	ASpecialSpot *GetRandomSpot(bool onlyfirst)
	{
		if (Spots.Size() && !numcalls)
		{
			int i = pr_spot() % Spots.Size();
			numcalls++;
			return Spots[i];
		}
		else return NULL;
	}
};

//----------------------------------------------------------------------------
//
// 
//
//----------------------------------------------------------------------------

DSpotState::DSpotState ()
: DThinker (STAT_INFO)
{
	if (SpotState)
	{
		I_Error ("Only one SpotState is allowed to exist at a time.\nCheck your code.");
	}
	else
	{
		SpotState = this;
	}
}

//----------------------------------------------------------------------------
//
// 
//
//----------------------------------------------------------------------------

void DSpotState::Destroy ()
{
	for(unsigned i = 0; i < SpotLists.Size(); i++)
	{
		delete SpotLists[i];
	}
	SpotLists.Clear();
	SpotLists.ShrinkToFit();

	SpotState = NULL;
	Super::Destroy();
}

//----------------------------------------------------------------------------
//
// 
//
//----------------------------------------------------------------------------

void DSpotState::Tick () 
{
}

//----------------------------------------------------------------------------
//
// 
//
//----------------------------------------------------------------------------

DSpotState *DSpotState::GetSpotState(bool create)
{
	if (SpotState == NULL && create) SpotState = new DSpotState;
	return SpotState;
}

//----------------------------------------------------------------------------
//
// 
//
//----------------------------------------------------------------------------

FSpotList *DSpotState::FindSpotList(const PClass *type)
{
	for(unsigned i = 0; i < SpotLists.Size(); i++)
	{
		if (SpotLists[i]->Type == type) return SpotLists[i];
	}
	return SpotLists[SpotLists.Push(new FSpotList(type))];
}

//----------------------------------------------------------------------------
//
// 
//
//----------------------------------------------------------------------------

bool DSpotState::AddSpot(ASpecialSpot *spot)
{
	FSpotList *list = FindSpotList(RUNTIME_TYPE(spot));
	if (list != NULL) return list->Add(spot);
	return false;
}

//----------------------------------------------------------------------------
//
// 
//
//----------------------------------------------------------------------------

bool DSpotState::RemoveSpot(ASpecialSpot *spot)
{
	FSpotList *list = FindSpotList(RUNTIME_TYPE(spot));
	if (list != NULL) return list->Remove(spot);
	return false;
}

//----------------------------------------------------------------------------
//
// 
//
//----------------------------------------------------------------------------

void DSpotState::Serialize(FArchive &arc)
{
	Super::Serialize(arc);
	if (arc.IsStoring())
	{
		arc.WriteCount(SpotLists.Size());
		for(unsigned i = 0; i < SpotLists.Size(); i++)
		{
			SpotLists[i]->Serialize(arc);
		}
	}
	else
	{
		unsigned c = arc.ReadCount();
		SpotLists.Resize(c);
		for(unsigned i = 0; i < SpotLists.Size(); i++)
		{
			SpotLists[i] = new FSpotList;
			SpotLists[i]->Serialize(arc);
		}
	}
}

//----------------------------------------------------------------------------
//
// 
//
//----------------------------------------------------------------------------

ASpecialSpot *DSpotState::GetNextInList(const PClass *type, int skipcounter)
{
	FSpotList *list = FindSpotList(type);
	if (list != NULL) return list->GetNextInList(skipcounter);
	return NULL;
}

//----------------------------------------------------------------------------
//
// 
//
//----------------------------------------------------------------------------

ASpecialSpot *DSpotState::GetSpotWithMinMaxDistance(const PClass *type, fixed_t x, fixed_t y, fixed_t mindist, fixed_t maxdist)
{
	FSpotList *list = FindSpotList(type);
	if (list != NULL) return list->GetSpotWithMinMaxDistance(x, y, mindist, maxdist);
	return NULL;
}

//----------------------------------------------------------------------------
//
// 
//
//----------------------------------------------------------------------------

ASpecialSpot *DSpotState::GetRandomSpot(const PClass *type, bool onlyonce)
{
	FSpotList *list = FindSpotList(type);
	if (list != NULL) return list->GetRandomSpot(onlyonce);
	return NULL;
}


//----------------------------------------------------------------------------
//
// 
//
//----------------------------------------------------------------------------

void ASpecialSpot::BeginPlay()
{
	DSpotState *state = DSpotState::GetSpotState();
	if (state != NULL) state->AddSpot(this);
}

//----------------------------------------------------------------------------
//
// 
//
//----------------------------------------------------------------------------

void ASpecialSpot::Destroy()
{
	DSpotState *state = DSpotState::GetSpotState(false);
	if (state != NULL) state->RemoveSpot(this);
	Super::Destroy();
}

// Mace spawn spot ----------------------------------------------------------


// Every mace spawn spot will execute this action. The first one
// will build a list of all mace spots in the level and spawn a
// mace. The rest of the spots will do nothing.

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_SpawnSingleItem)
{
	AActor *spot = NULL;
	DSpotState *state = DSpotState::GetSpotState();

	if (state != NULL) spot = state->GetRandomSpot(RUNTIME_TYPE(self), true);
	if (spot == NULL) return;

	ACTION_PARAM_START(4);
	ACTION_PARAM_CLASS(cls, 0);
	ACTION_PARAM_INT(fail_sp, 1);
	ACTION_PARAM_INT(fail_co, 2);
	ACTION_PARAM_INT(fail_dm, 3);

	if (!multiplayer && pr_spawnmace() < fail_sp)
	{ // Sometimes doesn't show up if not in deathmatch
		return;
	}

	if (multiplayer && !deathmatch && pr_spawnmace() < fail_co)
	{
		return;
	}

	if (deathmatch && pr_spawnmace() < fail_dm)
	{
		return;
	}

	if (cls == NULL)
	{
		return;
	}

	AActor *spawned = Spawn(cls, self->Pos(), ALLOW_REPLACE);

	if (spawned)
	{
		spawned->SetOrigin (spot->Pos(), false);
		spawned->SetZ(spawned->floorz);
		// We want this to respawn.
		if (!(self->flags & MF_DROPPED)) 
		{
			spawned->flags &= ~MF_DROPPED;
		}
		if (spawned->IsKindOf(RUNTIME_CLASS(AInventory)))
		{
			static_cast<AInventory*>(spawned)->SpawnPointClass = RUNTIME_TYPE(self);
		}
	}
}

