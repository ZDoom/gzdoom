/*
**
**
**---------------------------------------------------------------------------
** Copyright 2011 FDARI
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

#include "actorptrselect.h"
#include "actor.h"
#include "d_player.h"
#include "p_local.h"
#include "g_levellocals.h"

//==========================================================================
//
// Standard pointer acquisition functions
//
// Possible effective results at run-time
//   assigntovariable = NULL (or a RETURN statement is issued)
//   P_BulletSlope(pointer_owner, &temporary), assigntovariable = temporary
//   assigntovariable = pointer_owner->target or ...->master or ...->tracer
//
//==========================================================================


/*
	COPY_AAPTR

	Result overview in order of priority:

	1. Caller is player and a player specific selector is specified: Player specific selector is used.
	2. Caller is non-null and a general actor selector is specified: General actor selector is used.
	3. A static actor selector is specified: Static actor selector is used.
	4. The origin actor is used.

	Only one selector of each type can be used.
*/

AActor *COPY_AAPTREX(FLevelLocals *Level, AActor *origin, int selector)
{
	if (selector == AAPTR_DEFAULT) return origin;

	FTranslatedLineTarget t;

	auto AAPTR_RESOLVE_PLAYERNUM = [=](int playernum) -> AActor*
	{
		return (Level->PlayerInGame(playernum) ? Level->Players[playernum]->mo : nullptr);
	};

	if (origin)
	{
		if (origin->player)
		{
			switch (selector & AAPTR_PLAYER_SELECTORS)
			{
			case AAPTR_PLAYER_GETTARGET:
				P_BulletSlope(origin, &t, ALF_PORTALRESTRICT);
				return t.linetarget;

			case AAPTR_PLAYER_GETCONVERSATION:
				return origin->player->ConversationNPC;
			}
		}

		switch (selector & AAPTR_GENERAL_SELECTORS)
		{
		case AAPTR_TARGET: return origin->target;
		case AAPTR_MASTER: return origin->master;
		case AAPTR_TRACER: return origin->tracer;
		case AAPTR_FRIENDPLAYER:
			return origin->FriendPlayer ? AAPTR_RESOLVE_PLAYERNUM(origin->FriendPlayer - 1) : NULL;

		case AAPTR_GET_LINETARGET:
			P_BulletSlope(origin, &t, ALF_PORTALRESTRICT);
			return t.linetarget;
		}
	}
	
	switch (selector & AAPTR_STATIC_SELECTORS)
	{
		case AAPTR_PLAYER1: return AAPTR_RESOLVE_PLAYERNUM(0);
		case AAPTR_PLAYER2: return AAPTR_RESOLVE_PLAYERNUM(1);
		case AAPTR_PLAYER3: return AAPTR_RESOLVE_PLAYERNUM(2);
		case AAPTR_PLAYER4: return AAPTR_RESOLVE_PLAYERNUM(3);
		case AAPTR_PLAYER5: return AAPTR_RESOLVE_PLAYERNUM(4);
		case AAPTR_PLAYER6: return AAPTR_RESOLVE_PLAYERNUM(5);
		case AAPTR_PLAYER7: return AAPTR_RESOLVE_PLAYERNUM(6);
		case AAPTR_PLAYER8: return AAPTR_RESOLVE_PLAYERNUM(7);
		case AAPTR_NULL: return NULL;
	}

	return origin;
}

AActor *COPY_AAPTR(AActor *origin, int selector)
{
	if (origin == nullptr) return nullptr;
	return COPY_AAPTREX(origin->Level, origin, selector);
}

// [FDARI] Exported logic for guarding against loops in Target (for missiles) and Master (for all) chains.
// It is called from multiple locations.
// The code may be in need of optimisation.


//==========================================================================
//
// Checks whether this actor is a missile
// Unfortunately this was buggy in older versions of the code and many
// released DECORATE monsters rely on this bug so it can only be fixed
// with an optional flag
//
//==========================================================================

void VerifyTargetChain(AActor *self, bool preciseMissileCheck)
{
	if (!self || !self->isMissile(preciseMissileCheck)) return;

	AActor *origin = self;
	AActor *next = origin->target;

	// origin: the most recent actor that has been verified as appearing only once
	// next: the next actor to be verified; will be "origin" in the next iteration

	while (next && next->isMissile(preciseMissileCheck)) // we only care when there are missiles involved
	{
		AActor *compare = self;
		// every new actor must prove not to be the first actor in the chain, or any subsequent actor
		// any actor up to and including "origin" has only appeared once
		for (;;)
 		{
			if (compare == next)
			{
				// if any of the actors from self to (inclusive) origin match the next actor,
				// self has reached/created a loop
				self->target = nullptr;
				return;
			}
			if (compare == origin) break; // when "compare" = origin, we know that the next actor is, and should be "next"
			compare = compare->target;
		}

		origin = next;
		next = next->target;
	}
}

void VerifyMasterChain(AActor *self)
{
	// See VerifyTargetChain for detailed comments.

	if (!self) return;
	AActor *origin = self;
	AActor *next = origin->master;
	while (next) // We always care (See "VerifyTargetChain")
	{
		AActor *compare = self;
		for (;;)
		{
			if (compare == next)
			{
				self->master = nullptr;
				return;
			}
			if (compare == origin) break;
			compare = compare->master;
		}

		origin = next;
		next = next->master;
	}
}

//==========================================================================
//
// Checks whether this actor is a missile
// Unfortunately this was buggy in older versions of the code and many
// released DECORATE monsters rely on this bug so it can only be fixed
// with an optional flag
//
//==========================================================================

void ASSIGN_AAPTR(AActor *toActor, int toSlot, AActor *ptr, int flags) 
{
	switch (toSlot)
	{
		case AAPTR_TARGET: 
			toActor->target = ptr; 
			if (!(PTROP_UNSAFETARGET & (flags))) VerifyTargetChain(toActor); 
			break;

		case AAPTR_MASTER: 
			toActor->master = ptr; 
			if (!(PTROP_UNSAFEMASTER & (flags))) VerifyMasterChain(toActor); 
			break;

		case AAPTR_TRACER: 
			toActor->tracer = ptr; 
			break;
	}
}
