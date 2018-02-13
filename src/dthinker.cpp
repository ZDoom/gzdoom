/*
** dthinker.cpp
** Implements the base class for almost anything in a level that might think
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
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

#include "dthinker.h"
#include "stats.h"
#include "p_local.h"
#include "statnums.h"
#include "i_system.h"
#include "doomerrors.h"
#include "serializer.h"
#include "d_player.h"
#include "vm.h"


static int ThinkCount;
static cycle_t ThinkCycles;
extern cycle_t BotSupportCycles;
extern cycle_t ActionCycles;
extern int BotWTG;

IMPLEMENT_CLASS(DThinker, false, false)

DThinker *NextToThink;

FThinkerList DThinker::Thinkers[MAX_STATNUM+2];
FThinkerList DThinker::FreshThinkers[MAX_STATNUM+1];
bool DThinker::bSerialOverride = false;

//==========================================================================
//
//
//
//==========================================================================

void FThinkerList::AddTail(DThinker *thinker)
{
	assert(thinker->PrevThinker == NULL && thinker->NextThinker == NULL);
	assert(!(thinker->ObjectFlags & OF_EuthanizeMe));
	if (Sentinel == NULL)
	{
		Sentinel = Create<DThinker>(DThinker::NO_LINK);
		Sentinel->ObjectFlags |= OF_Sentinel;
		Sentinel->NextThinker = Sentinel;
		Sentinel->PrevThinker = Sentinel;
		GC::WriteBarrier(Sentinel);
	}
	DThinker *tail = Sentinel->PrevThinker;
	assert(tail->NextThinker == Sentinel);
	thinker->PrevThinker = tail;
	thinker->NextThinker = Sentinel;
	tail->NextThinker = thinker;
	Sentinel->PrevThinker = thinker;
	GC::WriteBarrier(thinker, tail);
	GC::WriteBarrier(thinker, Sentinel);
	GC::WriteBarrier(tail, thinker);
	GC::WriteBarrier(Sentinel, thinker);
}

//==========================================================================
//
//
//
//==========================================================================

DThinker *FThinkerList::GetHead() const
{
	if (Sentinel == NULL || Sentinel->NextThinker == Sentinel)
	{
		return NULL;
	}
	assert(Sentinel->NextThinker->PrevThinker == Sentinel);
	return Sentinel->NextThinker;
}

//==========================================================================
//
//
//
//==========================================================================

DThinker *FThinkerList::GetTail() const
{
	if (Sentinel == NULL || Sentinel->PrevThinker == Sentinel)
	{
		return NULL;
	}
	return Sentinel->PrevThinker;
}

//==========================================================================
//
//
//
//==========================================================================

bool FThinkerList::IsEmpty() const
{
	return Sentinel == NULL || Sentinel->NextThinker == NULL;
}

//==========================================================================
//
//
//
//==========================================================================

void DThinker::SaveList(FSerializer &arc, DThinker *node)
{
	if (node != NULL)
	{
		while (!(node->ObjectFlags & OF_Sentinel))
		{
			assert(node->NextThinker != NULL && !(node->NextThinker->ObjectFlags & OF_EuthanizeMe));
			::Serialize<DThinker>(arc, nullptr, node, nullptr);
			node = node->NextThinker;
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

void DThinker::SerializeThinkers(FSerializer &arc, bool hubLoad)
{
	//DThinker *thinker;
	//uint8_t stat;
	//int statcount;
	int i;

	if (arc.isWriting())
	{
		arc.BeginArray("thinkers");
		for (i = 0; i <= MAX_STATNUM; i++)
		{
			arc.BeginArray(nullptr);
			SaveList(arc, Thinkers[i].GetHead());
			SaveList(arc, FreshThinkers[i].GetHead());
			arc.EndArray();
		}
		arc.EndArray();
	}
	else
	{
		if (arc.BeginArray("thinkers"))
		{
			for (i = 0; i <= MAX_STATNUM; i++)
			{

				if (arc.BeginArray(nullptr))
				{
					if (!hubLoad || i != STAT_STATIC)	// do not load static thinkers in a hub transition because they'd just duplicate the active ones.
					{
						int size = arc.ArraySize();
						for (int j = 0; j < size; j++)
						{
							DThinker *thinker = nullptr;
							arc(nullptr, thinker);
							if (thinker != nullptr)
							{
								// This may be a player stored in their ancillary list. Remove
								// them first before inserting them into the new list.
								if (thinker->NextThinker != nullptr)
								{
									thinker->Remove();
								}
								// Thinkers with the OF_JustSpawned flag set go in the FreshThinkers
								// list. Anything else goes in the regular Thinkers list.
								if (thinker->ObjectFlags & OF_EuthanizeMe)
								{
									// This thinker was destroyed during the loading process. Do
									// not link it into any list.
								}
								else if (thinker->ObjectFlags & OF_JustSpawned)
								{
									FreshThinkers[i].AddTail(thinker);
									thinker->PostSerialize();
								}
								else
								{
									Thinkers[i].AddTail(thinker);
									thinker->PostSerialize();
								}
							}
						}
					}
					arc.EndArray();
				}
			}
			arc.EndArray();
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

DThinker::DThinker (int statnum) throw()
{
	NextThinker = NULL;
	PrevThinker = NULL;
	if (bSerialOverride)
	{ // The serializer will insert us into the right list
		return;
	}

	ObjectFlags |= OF_JustSpawned;
	if ((unsigned)statnum > MAX_STATNUM)
	{
		statnum = MAX_STATNUM;
	}
	FreshThinkers[statnum].AddTail (this);
}

DThinker::DThinker(no_link_type foo) throw()
{
	foo;	// Avoid unused argument warnings.
}

DThinker::~DThinker ()
{
	assert(NextThinker == NULL && PrevThinker == NULL);
}

void DThinker::OnDestroy ()
{
	assert((NextThinker != NULL && PrevThinker != NULL) ||
		   (NextThinker == NULL && PrevThinker == NULL));
	if (NextThinker != NULL)
	{
		Remove();
	}
	Super::OnDestroy();
}

//==========================================================================
//
//
//
//==========================================================================

void DThinker::Remove()
{
	if (this == NextToThink)
	{
		NextToThink = NextThinker;
	}
	DThinker *prev = PrevThinker;
	DThinker *next = NextThinker;
	assert(prev != NULL && next != NULL);
	assert((ObjectFlags & OF_Sentinel) || (prev != this && next != this));
	assert(prev->NextThinker == this);
	assert(next->PrevThinker == this);
	prev->NextThinker = next;
	next->PrevThinker = prev;
	GC::WriteBarrier(prev, next);
	GC::WriteBarrier(next, prev);
	NextThinker = NULL;
	PrevThinker = NULL;
}

//==========================================================================
//
// 
//
//==========================================================================

void DThinker::PostBeginPlay ()
{
}

DEFINE_ACTION_FUNCTION(DThinker, PostBeginPlay)
{
	PARAM_SELF_PROLOGUE(DThinker);
	self->PostBeginPlay();
	return 0;
}

void DThinker::CallPostBeginPlay()
{
	ObjectFlags |= OF_Spawned;
	IFVIRTUAL(DThinker, PostBeginPlay)
	{
		// Without the type cast this picks the 'void *' assignment...
		VMValue params[1] = { (DObject*)this };
		VMCall(func, params, 1, nullptr, 0);
	}
	else
	{
		PostBeginPlay();
	}
}

//==========================================================================
//
//
//
//==========================================================================

void DThinker::PostSerialize()
{
}

//==========================================================================
//
// 
//
//==========================================================================

DThinker *DThinker::FirstThinker (int statnum)
{
	DThinker *node;

	if ((unsigned)statnum > MAX_STATNUM)
	{
		statnum = MAX_STATNUM;
	}
	node = Thinkers[statnum].GetHead();
	if (node == NULL)
	{
		node = FreshThinkers[statnum].GetHead();
		if (node == NULL)
		{
			return NULL;
		}
	}
	return node;
}

//==========================================================================
//
//
//
//==========================================================================

void DThinker::ChangeStatNum (int statnum)
{
	FThinkerList *list;

	// This thinker should already be in a list; verify that.
	assert(NextThinker != NULL && PrevThinker != NULL);

	if ((unsigned)statnum > MAX_STATNUM)
	{
		statnum = MAX_STATNUM;
	}
	Remove();
	if ((ObjectFlags & OF_JustSpawned) && statnum >= STAT_FIRST_THINKING)
	{
		list = &FreshThinkers[statnum];
	}
	else
	{
		list = &Thinkers[statnum];
	}
	list->AddTail(this);
}

DEFINE_ACTION_FUNCTION(DThinker, ChangeStatNum)
{
	PARAM_SELF_PROLOGUE(DThinker);
	PARAM_INT(stat);
	self->ChangeStatNum(stat);
	return 0;
}
//==========================================================================
//
// Mark the first thinker of each list
//
//==========================================================================

void DThinker::MarkRoots()
{
	for (int i = 0; i <= MAX_STATNUM; ++i)
	{
		GC::Mark(Thinkers[i].Sentinel);
		GC::Mark(FreshThinkers[i].Sentinel);
	}
	GC::Mark(Thinkers[MAX_STATNUM+1].Sentinel);
}

//==========================================================================
//
// Destroy every thinker
//
//==========================================================================

void DThinker::DestroyAllThinkers ()
{
	int i;

	for (i = 0; i <= MAX_STATNUM; i++)
	{
		if (i != STAT_TRAVELLING && i != STAT_STATIC)
		{
			DestroyThinkersInList (Thinkers[i]);
			DestroyThinkersInList (FreshThinkers[i]);
		}
	}
	DestroyThinkersInList (Thinkers[MAX_STATNUM+1]);
	GC::FullGC();
}

//==========================================================================
//
//
//
//==========================================================================

void DThinker::DestroyThinkersInList (FThinkerList &list)
{
	if (list.Sentinel != NULL)
	{
		for (DThinker *node = list.Sentinel->NextThinker; node != list.Sentinel; node = list.Sentinel->NextThinker)
		{
			assert(node != NULL);
			node->Destroy();
		}
		list.Sentinel->Destroy();
		list.Sentinel = NULL;
	}
}

//==========================================================================
//
//
//
//==========================================================================
CVAR(Bool, profilethinkers, false, 0)

struct ProfileInfo
{
	int numcalls = 0;
	cycle_t timer;

	ProfileInfo()
	{
		timer.Reset();
	}
};

TMap<FName, ProfileInfo> Profiles;


void DThinker::RunThinkers ()
{
	int i, count;

	ThinkCount = 0;
	ThinkCycles.Reset();
	BotSupportCycles.Reset();
	ActionCycles.Reset();
	BotWTG = 0;

	ThinkCycles.Clock();

	if (!profilethinkers)
	{
		// Tick every thinker left from last time
		for (i = STAT_FIRST_THINKING; i <= MAX_STATNUM; ++i)
		{
			TickThinkers(&Thinkers[i], NULL);
		}

		// Keep ticking the fresh thinkers until there are no new ones.
		do
		{
			count = 0;
			for (i = STAT_FIRST_THINKING; i <= MAX_STATNUM; ++i)
			{
				count += TickThinkers(&FreshThinkers[i], &Thinkers[i]);
			}
		} while (count != 0);
	}
	else
	{
		Profiles.Clear();
		// Tick every thinker left from last time
		for (i = STAT_FIRST_THINKING; i <= MAX_STATNUM; ++i)
		{
			ProfileThinkers(&Thinkers[i], NULL);
		}

		// Keep ticking the fresh thinkers until there are no new ones.
		do
		{
			count = 0;
			for (i = STAT_FIRST_THINKING; i <= MAX_STATNUM; ++i)
			{
				count += ProfileThinkers(&FreshThinkers[i], &Thinkers[i]);
			}
		} while (count != 0);
		auto it = TMap<FName, ProfileInfo>::Iterator(Profiles);
		TMap<FName, ProfileInfo>::Pair *pair;
		while (it.NextPair(pair))
		{
			Printf("%s, %dx, %fms\n", pair->Key.GetChars(), pair->Value.numcalls, pair->Value.timer.TimeMS());
		}
		profilethinkers = false;
	}

	ThinkCycles.Unclock();
}

//==========================================================================
//
//
//
//==========================================================================

int DThinker::TickThinkers (FThinkerList *list, FThinkerList *dest)
{
	int count = 0;
	DThinker *node = list->GetHead();

	if (node == NULL)
	{
		return 0;
	}

	while (node != list->Sentinel)
	{
		++count;
		NextToThink = node->NextThinker;
		if (node->ObjectFlags & OF_JustSpawned)
		{
			// Leave OF_JustSpawn set until after Tick() so the ticker can check it.
			if (dest != NULL)
			{ // Move thinker from this list to the destination list
				node->Remove();
				dest->AddTail(node);
			}
			node->CallPostBeginPlay();
		}
		else if (dest != NULL)
		{
			I_Error("There is a thinker in the fresh list that has already ticked.\n");
		}

		if (!(node->ObjectFlags & OF_EuthanizeMe))
		{ // Only tick thinkers not scheduled for destruction
			ThinkCount++;
			node->CallTick();
			node->ObjectFlags &= ~OF_JustSpawned;
			GC::CheckGC();
		}
		node = NextToThink;
	}
	return count;
}

//==========================================================================
//
//
//
//==========================================================================
int DThinker::ProfileThinkers(FThinkerList *list, FThinkerList *dest)
{
	int count = 0;
	DThinker *node = list->GetHead();

	if (node == NULL)
	{
		return 0;
	}

	while (node != list->Sentinel)
	{
		++count;
		NextToThink = node->NextThinker;
		if (node->ObjectFlags & OF_JustSpawned)
		{
			// Leave OF_JustSpawn set until after Tick() so the ticker can check it.
			if (dest != NULL)
			{ // Move thinker from this list to the destination list
				node->Remove();
				dest->AddTail(node);
			}
			node->CallPostBeginPlay();
		}
		else if (dest != NULL)
		{
			I_Error("There is a thinker in the fresh list that has already ticked.\n");
		}

		if (!(node->ObjectFlags & OF_EuthanizeMe))
		{ // Only tick thinkers not scheduled for destruction
			ThinkCount++;

			auto &prof = Profiles[node->GetClass()->TypeName];
			prof.numcalls++;
			prof.timer.Clock();
			node->CallTick();
			prof.timer.Unclock();
			node->ObjectFlags &= ~OF_JustSpawned;
			GC::CheckGC();
		}
		node = NextToThink;
	}
	return count;
}

//==========================================================================
//
//
//
//==========================================================================

void DThinker::Tick ()
{
}

DEFINE_ACTION_FUNCTION(DThinker, Tick)
{
	PARAM_SELF_PROLOGUE(DThinker);
	self->Tick();
	return 0;
}

void DThinker::CallTick()
{
	IFVIRTUAL(DThinker, Tick)
	{
		// Without the type cast this picks the 'void *' assignment...
		VMValue params[1] = { (DObject*)this };
		VMCall(func, params, 1, nullptr, 0);
	}
	else Tick();
}

//==========================================================================
//
//
//
//==========================================================================

size_t DThinker::PropagateMark()
{
	// Do not choke on partially initialized objects (as happens when loading a savegame fails)
	if (NextThinker != nullptr || PrevThinker != nullptr)
	{
		assert(NextThinker != nullptr && !(NextThinker->ObjectFlags & OF_EuthanizeMe));
		assert(PrevThinker != nullptr && !(PrevThinker->ObjectFlags & OF_EuthanizeMe));
	}
	GC::Mark(NextThinker);
	GC::Mark(PrevThinker);
	return Super::PropagateMark();
}

//==========================================================================
//
//
//
//==========================================================================

FThinkerIterator::FThinkerIterator (const PClass *type, int statnum)
{
	if ((unsigned)statnum > MAX_STATNUM)
	{
		m_Stat = STAT_FIRST_THINKING;
		m_SearchStats = true;
	}
	else
	{
		m_Stat = statnum;
		m_SearchStats = false;
	}
	m_ParentType = type;
	m_CurrThinker = DThinker::Thinkers[m_Stat].GetHead();
	m_SearchingFresh = false;
}

//==========================================================================
//
//
//
//==========================================================================

FThinkerIterator::FThinkerIterator (const PClass *type, int statnum, DThinker *prev)
{
	if ((unsigned)statnum > MAX_STATNUM)
	{
		m_Stat = STAT_FIRST_THINKING;
		m_SearchStats = true;
	}
	else
	{
		m_Stat = statnum;
		m_SearchStats = false;
	}
	m_ParentType = type;
	if (prev == NULL || (prev->NextThinker->ObjectFlags & OF_Sentinel))
	{
		Reinit();
	}
	else
	{
		m_CurrThinker = prev->NextThinker;
		m_SearchingFresh = false;
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FThinkerIterator::Reinit ()
{
	m_CurrThinker = DThinker::Thinkers[m_Stat].GetHead();
	m_SearchingFresh = false;
}

//==========================================================================
//
//
//
//==========================================================================

DThinker *FThinkerIterator::Next (bool exact)
{
	if (m_ParentType == NULL)
	{
		return NULL;
	}
	do
	{
		do
		{
			if (m_CurrThinker != NULL)
			{
				while (!(m_CurrThinker->ObjectFlags & OF_Sentinel))
				{
					DThinker *thinker = m_CurrThinker;
					m_CurrThinker = thinker->NextThinker;
					if (exact)
					{
						if (thinker->IsA(m_ParentType)) return thinker;
					}
					else if (thinker->IsKindOf(m_ParentType))
					{
						return thinker;
					}
					// This can actually happen when a Destroy call on 'thinker' happens to destroy 'm_CurrThinker'.
					// In that case there is no chance to recover, we have to terminate the iteration of this list.
					if (m_CurrThinker == nullptr) break;
				}
			}
			if ((m_SearchingFresh = !m_SearchingFresh))
			{
				m_CurrThinker = DThinker::FreshThinkers[m_Stat].GetHead();
			}
		} while (m_SearchingFresh);
		if (m_SearchStats)
		{
			m_Stat++;
			if (m_Stat > MAX_STATNUM)
			{
				m_Stat = STAT_FIRST_THINKING;
			}
		}
		m_CurrThinker = DThinker::Thinkers[m_Stat].GetHead();
		m_SearchingFresh = false;
	} while (m_SearchStats && m_Stat != STAT_FIRST_THINKING);
	return NULL;
}

//==========================================================================
//
// This is for scripting, which needs the iterator wrapped into an object with the needed functions exported.
// Unfortunately we cannot have templated type conversions in scripts.
//
//==========================================================================

class DThinkerIterator : public DObject, public FThinkerIterator
{
	DECLARE_ABSTRACT_CLASS(DThinkerIterator, DObject)

public:
	DThinkerIterator(PClass *cls, int statnum = MAX_STATNUM + 1)
		: FThinkerIterator(cls, statnum)
	{
	}
};

IMPLEMENT_CLASS(DThinkerIterator, true, false);
DEFINE_ACTION_FUNCTION(DThinkerIterator, Create)
{
	PARAM_PROLOGUE;
	PARAM_CLASS_DEF(type, DThinker);
	PARAM_INT_DEF(statnum);
	ACTION_RETURN_OBJECT(Create<DThinkerIterator>(type, statnum));
}

DEFINE_ACTION_FUNCTION(DThinkerIterator, Next)
{
	PARAM_SELF_PROLOGUE(DThinkerIterator);
	ACTION_RETURN_OBJECT(self->Next());
}

DEFINE_ACTION_FUNCTION(DThinkerIterator, Reinit)
{
	PARAM_SELF_PROLOGUE(DThinkerIterator);
	self->Reinit();
	return 0;
}

//==========================================================================
//
//
//
//==========================================================================

ADD_STAT (think)
{
	FString out;
	out.Format ("Think time = %04.2f ms - %d thinkers, Action = %04.2f ms", ThinkCycles.TimeMS(), ThinkCount, ActionCycles.TimeMS());
	return out;
}
