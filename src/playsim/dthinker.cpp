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
#include "serializer_doom.h"
#include "d_player.h"
#include "vm.h"
#include "c_dispatch.h"
#include "v_text.h"
#include "g_levellocals.h"
#include "a_dynlight.h"
#include "v_video.h"
#include "g_cvars.h"
#include "d_main.h"

#include "p_visualthinker.h"

static int ThinkCount;
static cycle_t ThinkCycles;
extern cycle_t BotSupportCycles;
extern cycle_t ActionCycles;
extern int BotWTG;

IMPLEMENT_CLASS(DThinker, false, false)

struct ProfileInfo
{
	int numcalls = 0;
	cycle_t timer;

	ProfileInfo()
	{
		timer.Reset();
	}
};

static TMap<FName, ProfileInfo> Profiles;
static unsigned int profilethinkers, profilelimit;
DThinker *NextToThink;

//==========================================================================
//
//
//
//==========================================================================

void FThinkerCollection::Link(DThinker *thinker, int statnum)
{
	FThinkerList *list;
	if ((thinker->ObjectFlags & OF_JustSpawned) && statnum >= STAT_FIRST_THINKING)
	{
		list = &FreshThinkers[statnum];
	}
	else
	{
		if (statnum != STAT_TRAVELLING) thinker->ObjectFlags &= ~OF_JustSpawned;
		list = &Thinkers[statnum];
	}
	list->AddTail(thinker);
}

//==========================================================================
//
//
//
//==========================================================================

void FThinkerCollection::RunThinkers(FLevelLocals *Level)
{
	int i, count;

	ThinkCount = 0;
	ThinkCycles.Reset();
	BotSupportCycles.Reset();
	ActionCycles.Reset();
	BotWTG = 0;

	ThinkCycles.Clock();

	bool dolights;
	if ((gl_lights && vid_rendermode == 4) || (r_dynlights && vid_rendermode != 4))
	{
		dolights = true;// Level->lights || (Level->flags3 & LEVEL3_LIGHTCREATED);
	}
	else
	{
		dolights = false;
	}
	Level->flags3 &= ~LEVEL3_LIGHTCREATED;


	auto recreateLights = [=]() {
		auto it = Level->GetThinkerIterator<AActor>();

		// Set dynamic lights at the end of the tick, so that this catches all changes being made through the last frame.
		while (auto ac = it.Next())
		{
			if (ac->flags8 & MF8_RECREATELIGHTS)
			{
				ac->flags8 &= ~MF8_RECREATELIGHTS;
				if (dolights) ac->SetDynamicLights();
			}
			// This was merged from P_RunEffects to eliminate the costly duplicate ThinkerIterator loop.
			if ((ac->effects || ac->fountaincolor) && ac->ShouldRenderLocally() && !Level->isFrozen())
			{
				P_RunEffect(ac, ac->effects);
			}
		}
	};

	if (!profilethinkers)
	{
		// Tick every thinker left from last time
		for (i = STAT_FIRST_THINKING; i <= MAX_STATNUM; ++i)
		{
			Thinkers[i].TickThinkers(nullptr);
		}

		// Keep ticking the fresh thinkers until there are no new ones.
		do
		{
			count = 0;
			for (i = STAT_FIRST_THINKING; i <= MAX_STATNUM; ++i)
			{
				count += FreshThinkers[i].TickThinkers(&Thinkers[i]);
			}
		} while (count != 0);

		recreateLights();
		if (dolights)
		{
			for (auto light = Level->lights; light;)
			{
				auto next = light->next;
				light->Tick();
				light = next;
			}
		}
	}
	else
	{
		Profiles.Clear();
		// Tick every thinker left from last time
		for (i = STAT_FIRST_THINKING; i <= MAX_STATNUM; ++i)
		{
			Thinkers[i].ProfileThinkers(nullptr);
		}

		// Keep ticking the fresh thinkers until there are no new ones.
		do
		{
			count = 0;
			for (i = STAT_FIRST_THINKING; i <= MAX_STATNUM; ++i)
			{
				count += FreshThinkers[i].ProfileThinkers(&Thinkers[i]);
			}
		} while (count != 0);

		recreateLights();
		if (dolights)
		{
			// Also profile the internal dynamic lights, even though they are not implemented as thinkers.
			auto &prof = Profiles[NAME_InternalDynamicLight];
			prof.timer.Clock();
			for (auto light = Level->lights; light;)
			{
				prof.numcalls++;
				auto next = light->next;
				light->Tick();
				light = next;
			}
			prof.timer.Unclock();
		}


		struct SortedProfileInfo
		{
			const char* className;
			int numcalls;
			double time;
		};

		TArray<SortedProfileInfo> sorted;
		sorted.Grow(Profiles.CountUsed());

		auto it = TMap<FName, ProfileInfo>::Iterator(Profiles);
		TMap<FName, ProfileInfo>::Pair *pair;
		while (it.NextPair(pair))
		{
			sorted.Push({ pair->Key.GetChars(), pair->Value.numcalls, pair->Value.timer.TimeMS() });
		}

		std::sort(sorted.begin(), sorted.end(), [](const SortedProfileInfo& left, const SortedProfileInfo& right)
		{
			switch (profilethinkers)
			{
			case 1: // by name, from A to Z
				return stricmp(left.className, right.className) < 0;
			case 2: // by name, from Z to A
				return stricmp(right.className, left.className) < 0;
			case 3: // number of calls, ascending
				return left.numcalls < right.numcalls;
			case 4: // number of calls, descending
				return right.numcalls < left.numcalls;
			case 5: // average time, ascending
				return left.time / left.numcalls < right.time / right.numcalls;
			case 6: // average time, descending
				return right.time / right.numcalls < left.time / left.numcalls;
			case 7: // total time, ascending
				return left.time < right.time;
			default: // total time, descending
				return right.time < left.time;
			}
		});

		Printf(TEXTCOLOR_YELLOW "Total, ms   Averg, ms   Calls   Actor class\n");
		Printf(TEXTCOLOR_YELLOW "----------  ----------  ------  --------------------\n");

		const unsigned count = min(profilelimit > 0 ? profilelimit : UINT_MAX, sorted.Size());

		for (unsigned i = 0; i < count; ++i)
		{
			const SortedProfileInfo& info = sorted[i];
			Printf("%s%10.6f  %s%10.6f  %s%6d  %s%s\n",
				profilethinkers >= 7 ? TEXTCOLOR_YELLOW : TEXTCOLOR_WHITE, info.time,
				profilethinkers == 5 || profilethinkers == 6 ? TEXTCOLOR_YELLOW : TEXTCOLOR_WHITE, info.time / info.numcalls,
				profilethinkers == 3 || profilethinkers == 4 ? TEXTCOLOR_YELLOW : TEXTCOLOR_WHITE, info.numcalls,
				profilethinkers == 1 || profilethinkers == 2 ? TEXTCOLOR_YELLOW : TEXTCOLOR_WHITE, info.className);
		}

		profilethinkers = 0;
	}

	ThinkCycles.Unclock();
}

//==========================================================================
//
// This version doesn't modify the level since that's already been done by
// the networked ticking. This also runs while the player is predicting
// to make sure it keeps ticking regardless of network game status.
//
//==========================================================================

void FThinkerCollection::RunClientsideThinkers(FLevelLocals* Level)
{
	int i, count;

	bool dolights;
	if ((gl_lights && vid_rendermode == 4) || (r_dynlights && vid_rendermode != 4))
	{
		dolights = true;// Level->lights || (Level->flags3 & LEVEL3_LIGHTCREATED);
	}
	else
	{
		dolights = false;
	}

	auto recreateLights = [=]() {
		auto it = Level->GetClientsideThinkerIterator<AActor>();

		// Set dynamic lights at the end of the tick, so that this catches all changes being made through the last frame.
		while (auto ac = it.Next())
		{
			if (ac->flags8 & MF8_RECREATELIGHTS)
			{
				ac->flags8 &= ~MF8_RECREATELIGHTS;
				if (dolights) ac->SetDynamicLights();
			}
			// This was merged from P_RunEffects to eliminate the costly duplicate ThinkerIterator loop.
			if ((ac->effects || ac->fountaincolor) && ac->ShouldRenderLocally() && !Level->isFrozen())
			{
				P_RunEffect(ac, ac->effects);
			}
		}
	};

	// Tick every thinker left from last time
	for (i = STAT_FIRST_THINKING; i <= MAX_STATNUM; ++i)
	{
		Thinkers[i].TickThinkers(nullptr);
	}

	// Keep ticking the fresh thinkers until there are no new ones.
	do
	{
		count = 0;
		for (i = STAT_FIRST_THINKING; i <= MAX_STATNUM; ++i)
		{
			count += FreshThinkers[i].TickThinkers(&Thinkers[i]);
		}
	} while (count != 0);

	recreateLights();
}

//==========================================================================
//
// Destroy every thinker
//
//==========================================================================

void FThinkerCollection::DestroyAllThinkers(bool fullgc)
{
	int i;
	bool error = false;

	for (i = 0; i <= MAX_STATNUM; i++)
	{
		if (i != STAT_TRAVELLING && i != STAT_STATIC)
		{
			error |= Thinkers[i].DoDestroyThinkers();
			error |= FreshThinkers[i].DoDestroyThinkers();
		}
	}
	error |= Thinkers[MAX_STATNUM + 1].DoDestroyThinkers();
	if (fullgc) GC::FullGC();
	if (error)
	{
		ClearGlobalVMStack();
		if (fullgc) I_Error("DestroyAllThinkers failed");
		else I_FatalError("DestroyAllThinkers failed");
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FThinkerCollection::SerializeThinkers(FSerializer &arc, bool hubLoad)
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
			Thinkers[i].SaveList(arc);
			FreshThinkers[i].SaveList(arc);
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
									thinker->CallPostSerialize();
								}
								else
								{
									Thinkers[i].AddTail(thinker);
									thinker->CallPostSerialize();
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

void FThinkerList::AddTail(DThinker *thinker)
{
	assert(thinker->PrevThinker == nullptr && thinker->NextThinker == nullptr);
	assert(!(thinker->ObjectFlags & OF_EuthanizeMe));
	if (Sentinel == nullptr)
	{
		// This cannot use CreateThinker because it must not be added to the list automatically.
		Sentinel = (DThinker*)RUNTIME_CLASS(DThinker)->CreateNew();
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

DThinker *FThinkerCollection::FirstThinker(int statnum)
{
	DThinker *node;

	if ((unsigned)statnum > MAX_STATNUM)
	{
		statnum = MAX_STATNUM;
	}
	node = Thinkers[statnum].GetHead();
	if (node == nullptr)
	{
		node = FreshThinkers[statnum].GetHead();
		if (node == nullptr)
		{
			return nullptr;
		}
	}
	return node;
}

//==========================================================================
//
// Mark the first thinker of each list
//
//==========================================================================

void FThinkerCollection::MarkRoots()
{
	for (int i = 0; i <= MAX_STATNUM; ++i)
	{
		GC::Mark(Thinkers[i].Sentinel);
		GC::Mark(FreshThinkers[i].Sentinel);
	}
	GC::Mark(Thinkers[MAX_STATNUM + 1].Sentinel);
}

//==========================================================================
//
//
//
//==========================================================================

DThinker *FThinkerList::GetHead() const
{
	if (Sentinel == nullptr || Sentinel->NextThinker == Sentinel)
	{
		return nullptr;
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
	if (Sentinel == nullptr || Sentinel->PrevThinker == Sentinel)
	{
		return nullptr;
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
	return Sentinel == nullptr || Sentinel->NextThinker == nullptr;
}

//==========================================================================
//
//
//
//==========================================================================

void FThinkerList::DestroyThinkers()
{
	if (DoDestroyThinkers())
	{
		I_Error("DestroyThinkers failed");
	}
}

//==========================================================================
//
//
//
//==========================================================================

bool FThinkerList::DoDestroyThinkers()
{
	bool error = false;
	if (Sentinel != nullptr)
	{
		// Taking down the linked list live is far too dangerous in case something goes wrong. So first copy all elements into an array, take down the list and then destroy them.

		TArray<DThinker *> toDelete;
		DThinker *node = Sentinel->NextThinker;
		while (node != Sentinel)
		{
			assert(node != nullptr);
			auto next = node->NextThinker;
			toDelete.Push(node);
			node->NextThinker = node->PrevThinker = nullptr;	// clear the links
			node = next;
		}
		Sentinel->NextThinker = Sentinel->PrevThinker = nullptr;
		Sentinel->Destroy();
		Sentinel = nullptr;
		for (auto node : toDelete)
		{
			// We must intercept all exceptions so that we can continue deleting the list.
			try
			{
				node->Destroy();
			}
			catch (CVMAbortException &exception)
			{
				Printf("VM exception in DestroyThinkers:\n");
				exception.MaybePrintMessage();
				Printf(PRINT_NONOTIFY | PRINT_BOLD, "%s", exception.stacktrace.GetChars());
				// forcibly delete this. Cleanup may be incomplete, though.
				node->ObjectFlags |= OF_YesReallyDelete;
				delete node;
				error = true;
			}
			catch (CRecoverableError &exception)
			{
				Printf(PRINT_NONOTIFY | PRINT_BOLD, "Error in DestroyThinkers: %s\n", exception.GetMessage());
				// forcibly delete this. Cleanup may be incomplete, though.
				node->ObjectFlags |= OF_YesReallyDelete;
				delete node;
				error = true;
			}
		}
	}
	return error;
}

//==========================================================================
//
//
//
//==========================================================================

void FThinkerList::SaveList(FSerializer &arc)
{
	auto node = GetHead();
	if (node != nullptr)
	{
		while (!(node->ObjectFlags & OF_Sentinel))
		{
			assert(node->NextThinker != nullptr && !(node->NextThinker->ObjectFlags & OF_EuthanizeMe));
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

int FThinkerList::TickThinkers(FThinkerList *dest)
{
	int count = 0;
	DThinker *node = GetHead();

	if (node == nullptr)
	{
		return 0;
	}

	while (node != Sentinel)
	{
		++count;
		NextToThink = node->NextThinker;
		if (node->ObjectFlags & OF_JustSpawned)
		{
			// Leave OF_JustSpawn set until after Tick() so the ticker can check it.
			if (dest != nullptr)
			{ // Move thinker from this list to the destination list
				node->Remove();
				dest->AddTail(node);
			}
			node->CallPostBeginPlay();
		}
		else if (dest != nullptr)
		{
			I_Error("There is a thinker in the fresh list that has already ticked.\n");
		}

		if (!(node->ObjectFlags & OF_EuthanizeMe))
		{ // Only tick thinkers not scheduled for destruction
			ThinkCount++;
			node->CallTick();
			node->ObjectFlags &= ~OF_JustSpawned;
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
int FThinkerList::ProfileThinkers(FThinkerList *dest)
{
	int count = 0;
	DThinker *node = GetHead();

	if (node == nullptr)
	{
		return 0;
	}

	while (node != Sentinel)
	{
		++count;
		NextToThink = node->NextThinker;
		if (node->ObjectFlags & OF_JustSpawned)
		{
			// Leave OF_JustSpawn set until after Tick() so the ticker can check it.
			if (dest != nullptr)
			{ // Move thinker from this list to the destination list
				node->Remove();
				dest->AddTail(node);
			}
			node->CallPostBeginPlay();
		}
		else if (dest != nullptr)
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

DThinker::~DThinker ()
{
	assert(NextThinker == nullptr && PrevThinker == nullptr);
}

void DThinker::OnDestroy ()
{
	assert((NextThinker != nullptr && PrevThinker != nullptr) ||
		   (NextThinker == nullptr && PrevThinker == nullptr));
	if (NextThinker != nullptr)
	{
		Remove();
	}
	Super::OnDestroy();
}

void DThinker::Serialize(FSerializer &arc)
{
	Super::Serialize(arc);
	arc("level", Level);
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
	if (prev == nullptr && next == nullptr) return;	// This was already removed earlier.

	assert((ObjectFlags & OF_Sentinel) || (prev != this && next != this));
	assert(prev->NextThinker == this);
	assert(next->PrevThinker == this);
	prev->NextThinker = next;
	next->PrevThinker = prev;
	GC::WriteBarrier(prev, next);
	GC::WriteBarrier(next, prev);
	NextThinker = nullptr;
	PrevThinker = nullptr;
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

void DThinker::CallPostSerialize()
{
	PostSerialize();
	IFOVERRIDENVIRTUALPTRNAME(this, NAME_Thinker, OnLoad)
	{
		VMValue params[] = { this };
		VMCall(func, params, 1, nullptr, 0);
	}
}

//==========================================================================
//
// 
//
//==========================================================================

DThinker *FLevelLocals::FirstThinker (int statnum)
{
	return Thinkers.FirstThinker(statnum);
}

DThinker* FLevelLocals::FirstClientsideThinker(int statnum)
{
	return ClientsideThinkers.FirstThinker(statnum);
}

//==========================================================================
//
//
//
//==========================================================================

void DThinker::ChangeStatNum (int statnum)
{
	if ((unsigned)statnum > MAX_STATNUM)
	{
		statnum = MAX_STATNUM;
	}
	Remove();
	if (IsClientside())
		Level->ClientsideThinkers.Link(this, statnum);
	else
		Level->Thinkers.Link(this, statnum);
}

static void ChangeStatNum(DThinker *thinker, int statnum)
{
	thinker->ChangeStatNum(statnum);
}

DEFINE_ACTION_FUNCTION_NATIVE(DThinker, ChangeStatNum, ChangeStatNum)
{
	PARAM_SELF_PROLOGUE(DThinker);
	PARAM_INT(stat);

	ChangeStatNum(self, stat);
	return 0;
}

//==========================================================================
//
//
//
//==========================================================================

CCMD(profilethinkers)
{
	const int argc = argv.argc();

	if (argc == 2 || argc == 3)
	{
		const char *str = argv[1];
		bool ascend = true;

		if (*str == '+')
		{
			++str;
		}
		else if (*str == '-')
		{
			ascend = false;
			++str;
		}

		int mode = 0;

		switch (*str)
		{
		case 't': mode = ascend ? 7 : 8; break;
		case 'a': mode = ascend ? 5 : 6; break;
		case '#': mode = ascend ? 3 : 4; break;
		case 'c': mode = ascend ? 1 : 2; break;
		default: mode = atoi(str); break;
		}

		profilethinkers = mode;
		profilelimit = argc == 3 ? atoi(argv[2]) : 0;
	}
	else
	{
		Printf(
			"Usage: profilethinkers [+|-][t|a|#|c] [limit]\n"
			"       profilethinkers [1..8] [limit]\n\n"
			"Sorting modes:\n"
			TEXTCOLOR_YELLOW "c +c 1  " TEXTCOLOR_NORMAL "actor class, ascending\n"
			TEXTCOLOR_YELLOW "  -c 2  " TEXTCOLOR_NORMAL "actor class, descending\n"
			TEXTCOLOR_YELLOW "# +# 3  " TEXTCOLOR_NORMAL "number of calls, ascending\n"
			TEXTCOLOR_YELLOW "  -# 4  " TEXTCOLOR_NORMAL "number of calls, descending\n"
			TEXTCOLOR_YELLOW "a +a 5  " TEXTCOLOR_NORMAL "average time, ascending\n"
			TEXTCOLOR_YELLOW "  -a 6  " TEXTCOLOR_NORMAL "average time, descending\n"
			TEXTCOLOR_YELLOW "t +t 7  " TEXTCOLOR_NORMAL "total time, ascending\n"
			TEXTCOLOR_YELLOW "  -t 8  " TEXTCOLOR_NORMAL "total time, descending\n");
	}
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

FThinkerIterator::FThinkerIterator (FLevelLocals *l, const PClass *type, int statnum, bool clientside) : Level(l)
{
	m_ThinkerPool = clientside ? &Level->ClientsideThinkers : &Level->Thinkers;
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
	Reinit();
}

//==========================================================================
//
//
//
//==========================================================================

FThinkerIterator::FThinkerIterator (FLevelLocals *l, const PClass *type, int statnum, DThinker *prev, bool clientside) : Level(l)
{
	m_ThinkerPool = clientside ? &Level->ClientsideThinkers : &Level->Thinkers;
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
	if (prev == nullptr || (prev->NextThinker->ObjectFlags & OF_Sentinel))
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
	m_CurrThinker = m_ThinkerPool->Thinkers[m_Stat].GetHead();
	m_SearchingFresh = false;
}

//==========================================================================
//
//
//
//==========================================================================

DThinker *FThinkerIterator::Next (bool exact)
{
	if (m_ParentType == nullptr)
	{
		return nullptr;
	}
	do
	{
		do
		{
			if (m_CurrThinker != nullptr)
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
				m_CurrThinker = m_ThinkerPool->FreshThinkers[m_Stat].GetHead();
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
		m_CurrThinker = m_ThinkerPool->Thinkers[m_Stat].GetHead();
		m_SearchingFresh = false;
	} while (m_SearchStats && m_Stat != STAT_FIRST_THINKING);
	return nullptr;
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
