#include "actorptrselect.h"
#include "actor.h"
#include "d_player.h"
#include "p_pspr.h"

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

#define AAPTR_RESOLVE_PLAYERNUM(playernum) (playeringame[playernum] ? players[playernum].mo : NULL)

AActor *COPY_AAPTR(AActor *origin, int selector)
{
	if (selector == AAPTR_DEFAULT) return origin;

	if (origin)
	{
		if (origin->player)
		{
			switch (selector & AAPTR_PLAYER_SELECTORS)
			{
			case AAPTR_PLAYER_GETTARGET:
				{
					AActor *gettarget = NULL;
					P_BulletSlope(origin, &gettarget);
					return gettarget;
				}
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
			{
				AActor *gettarget = NULL;
				P_BulletSlope(origin, &gettarget);
				return gettarget;
			}
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
				self->target = NULL;
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
				self->master = NULL;
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