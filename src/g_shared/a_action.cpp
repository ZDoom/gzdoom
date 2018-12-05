//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1994-1996 Raven Software
// Copyright 1999-2016 Randy Heit
// Copyright 2002-2016 Christoph Oelckers
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------


#include "actor.h"
#include "p_conversation.h"
#include "p_lnspec.h"
#include "d_player.h"
#include "p_local.h"
#include "p_terrain.h"
#include "p_enemy.h"
#include "serializer.h"
#include "vm.h"
#include "actorinlines.h"

//----------------------------------------------------------------------------
//
// PROC A_NoBlocking
//
//----------------------------------------------------------------------------

void A_Unblock(AActor *self, bool drop)
{
	// [RH] Andy Baker's stealth monsters
	if (self->flags & MF_STEALTH)
	{
		self->Alpha = 1.;
		self->visdir = 0;
	}

	self->flags &= ~MF_SOLID;

	// If the actor has a conversation that sets an item to drop, drop that.
	if (self->Conversation != NULL && self->Conversation->DropType != NULL)
	{
		P_DropItem (self, self->Conversation->DropType, -1, 256);
		self->Conversation = NULL;
		return;
	}

	self->Conversation = NULL;

	// If the actor has attached metadata for items to drop, drop those.
	if (drop && !self->IsKindOf (RUNTIME_CLASS (APlayerPawn)))	// [GRB]
	{
		auto di = self->GetDropItems();

		if (di != NULL)
		{
			while (di != NULL)
			{
				if (di->Name != NAME_None)
				{
					PClassActor *ti = PClass::FindActor(di->Name);
					if (ti != NULL)
					{
						P_DropItem (self, ti, di->Amount, di->Probability);
					}
				}
				di = di->Next;
			}
		}
	}
}

//----------------------------------------------------------------------------
//
// CorpseQueue Routines (used by Hexen)
//
//----------------------------------------------------------------------------

// Corpse queue for monsters - this should be saved out

class DCorpsePointer : public DThinker
{
	DECLARE_CLASS (DCorpsePointer, DThinker)
	HAS_OBJECT_POINTERS
public:
	DCorpsePointer (AActor *ptr);
	void Queue();
	void OnDestroy() override;
	void Serialize(FSerializer &arc);
	TObjPtr<AActor*> Corpse;
	uint32_t Count;	// Only the first corpse pointer's count is valid.
private:
	DCorpsePointer () {}
};

IMPLEMENT_CLASS(DCorpsePointer, false, true)

IMPLEMENT_POINTERS_START(DCorpsePointer)
	IMPLEMENT_POINTER(Corpse)
IMPLEMENT_POINTERS_END

CUSTOM_CVAR(Int, sv_corpsequeuesize, 64, CVAR_ARCHIVE|CVAR_SERVERINFO)
{
	if (self > 0)
	{
		TThinkerIterator<DCorpsePointer> iterator (STAT_CORPSEPOINTER);
		DCorpsePointer *first = iterator.Next ();
		while (first != NULL && first->Count > (uint32_t)self)
		{
			DCorpsePointer *next = iterator.Next ();
			first->Destroy ();
			first = next;
		}
	}
}


DCorpsePointer::DCorpsePointer(AActor *ptr)
	: DThinker(STAT_CORPSEPOINTER), Corpse(ptr)
{
	Count = 0;
}

void DCorpsePointer::Queue()
{
	// Thinkers are added to the end of their respective lists, so
	// the first thinker in the list is the oldest one.
	TThinkerIterator<DCorpsePointer> iterator (STAT_CORPSEPOINTER);
	DCorpsePointer *first = iterator.Next ();

	if (first != nullptr)
	{
		if (first != this)
		{
			if (first->Count >= (uint32_t)sv_corpsequeuesize)
			{
				DCorpsePointer *next = iterator.Next();
				first->Destroy();
				first = next;
			}
		}
		++first->Count;
	}
}

void DCorpsePointer::OnDestroy ()
{
	// Store the count of corpses in the first thinker in the list
	TThinkerIterator<DCorpsePointer> iterator (STAT_CORPSEPOINTER);
	DCorpsePointer *first = iterator.Next ();

	// During a serialization unwind the thinker list won't be available.
	if (first != nullptr)
	{
		int prevCount = first->Count;

		if (first == this)
		{
			first = iterator.Next();
		}

		if (first != NULL)
		{
			first->Count = prevCount - 1;
		}

	}
	if (Corpse != NULL)
	{
		Corpse->Destroy();
	}
	Super::OnDestroy();
}

void DCorpsePointer::Serialize(FSerializer &arc)
{
	Super::Serialize(arc);
	arc("corpse", Corpse)
		("count", Count);
}


// throw another corpse on the queue
DEFINE_ACTION_FUNCTION(AActor, A_QueueCorpse)
{
	PARAM_SELF_PROLOGUE(AActor);

	if (sv_corpsequeuesize > 0)
	{
		auto p = Create<DCorpsePointer> (self);
		p->Queue();
	}
	return 0;
}

// Remove an self from the queue (for resurrection)
DEFINE_ACTION_FUNCTION(AActor, A_DeQueueCorpse)
{
	PARAM_SELF_PROLOGUE(AActor);

	TThinkerIterator<DCorpsePointer> iterator (STAT_CORPSEPOINTER);
	DCorpsePointer *corpsePtr;

	while ((corpsePtr = iterator.Next()) != NULL)
	{
		if (corpsePtr->Corpse == self)
		{
			corpsePtr->Corpse = NULL;
			corpsePtr->Destroy ();
			return 0;
		}
	}
	return 0;
}

