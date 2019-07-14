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

EXTERN_CVAR(Int, sv_corpsequeuesize)

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
	if (drop && !self->IsKindOf(NAME_PlayerPawn))	// [GRB]
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

// throw another corpse on the queue
DEFINE_ACTION_FUNCTION(AActor, A_QueueCorpse)
{
	PARAM_SELF_PROLOGUE(AActor);

	if (sv_corpsequeuesize > 0)
	{
		auto &corpsequeue = self->Level->CorpseQueue;
		while (corpsequeue.Size() >= (unsigned)sv_corpsequeuesize)
		{
			AActor *corpse = corpsequeue[0];
			if (corpse) corpse->Destroy();
			corpsequeue.Delete(0);
		}
		corpsequeue.Push(self);
	}
	return 0;
}

// Remove an actor from the queue (for resurrection)
DEFINE_ACTION_FUNCTION(AActor, A_DeQueueCorpse)
{
	PARAM_SELF_PROLOGUE(AActor);

	auto &corpsequeue = self->Level->CorpseQueue;
	auto index = corpsequeue.FindEx([=](auto &element) { return element == self; });
	if (index < corpsequeue.Size())
	{
		corpsequeue.Delete(index);
	}
	return 0;
}

