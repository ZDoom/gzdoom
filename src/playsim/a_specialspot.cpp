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
#include "p_local.h"
#include "doomstat.h"
#include "serializer_doom.h"
#include "a_pickups.h"
#include "vm.h"

static FRandom pr_spot ("SpecialSpot");

IMPLEMENT_CLASS(DSpotState, false, false)

//----------------------------------------------------------------------------
//
// Spot list
//
//----------------------------------------------------------------------------

struct FSpotList
{
	PClassActor *Type;
	TArray<AActor*> Spots;
	unsigned Index;
	int SkipCount;
	int numcalls;

	FSpotList()
	{
	}

	FSpotList(PClassActor *type)
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

	bool Add(AActor *newspot)
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

	bool Remove(AActor *spot)
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

	AActor *GetNextInList(int skipcounter)
	{
		if (Spots.Size() > 0 && ++SkipCount > skipcounter)
		{
			SkipCount = 0;

			AActor *spot = Spots[Index];
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

	AActor *GetSpotWithMinMaxDistance(double x, double y, double mindist, double maxdist)
	{
		if (Spots.Size() == 0) return NULL;
		int i = pr_spot() % Spots.Size();
		int initial = i;

		double distance;

		while (true)
		{
			distance = Spots[i]->Distance2D(x, y);

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

	AActor *GetRandomSpot(bool onlyfirst)
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

FSerializer &Serialize(FSerializer &arc, const char *key, FSpotList &list, FSpotList *def)
{
	if (arc.BeginObject(key))
	{
		arc("type", list.Type)
			("spots", list.Spots)
			("index", list.Index)
			("skipcount", list.SkipCount)
			("numcalls", list.numcalls)
			.EndObject();
	}
	return arc;
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

DSpotState::DSpotState ()
{
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

void DSpotState::OnDestroy ()
{
	SpotLists.Reset();
	Super::OnDestroy();
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

FSpotList *DSpotState::FindSpotList(PClassActor *type)
{
	if (type == nullptr) return nullptr;
	for(unsigned i = 0; i < SpotLists.Size(); i++)
	{
		if (SpotLists[i].Type == type) return &SpotLists[i];
	}
	return &SpotLists[SpotLists.Push(FSpotList(type))];
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

bool DSpotState::AddSpot(AActor *spot)
{
	FSpotList *list = FindSpotList(spot->GetClass());
	if (list != NULL) return list->Add(spot);
	return false;
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

bool DSpotState::RemoveSpot(AActor *spot)
{
	FSpotList *list = FindSpotList(spot->GetClass());
	if (list != NULL) return list->Remove(spot);
	return false;
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

void DSpotState::Serialize(FSerializer &arc)
{
	Super::Serialize(arc);
	arc("spots", SpotLists);
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

AActor *DSpotState::GetNextInList(PClassActor *type, int skipcounter)
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

AActor *DSpotState::GetSpotWithMinMaxDistance(PClassActor *type, double x, double y, double mindist, double maxdist)
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

AActor *DSpotState::GetRandomSpot(PClassActor *type, bool onlyonce)
{
	FSpotList *list = FindSpotList(type);
	if (list != NULL) return list->GetRandomSpot(onlyonce);
	return NULL;
}

