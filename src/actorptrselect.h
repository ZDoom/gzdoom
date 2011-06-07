#pragma once

#include "p_pspr.h"

//==========================================================================
//
// Standard pointer acquisition functions
//
// Use COPY_AAPTR(pointer_owner, AActor *assigntovariable, AAPTR selector)
// Use COPY_AAPTR_NOT_NULL to return from a function if the pointer is NULL
//
// Possible effective results at run-time
//   assigntovariable = NULL (or a RETURN statement is issued)
//   P_BulletSlope(pointer_owner, &temporary), assigntovariable = temporary
//   assigntovariable = pointer_owner->target or ...->master or ...->tracer
//
//==========================================================================


// Pointer selectors (enum)

enum AAPTR
{
	AAPTR_DEFAULT = 0,
	AAPTR_NULL = 0x1,
	AAPTR_TARGET = 0x2,
	AAPTR_MASTER = 0x4,
	AAPTR_TRACER = 0x8,

	AAPTR_PLAYER_GETTARGET = 0x10,
	AAPTR_PLAYER_GETCONVERSATION = 0x20,

	AAPTR_PLAYER1 = 0x40,
	AAPTR_PLAYER2 = 0x80,
	AAPTR_PLAYER3 = 0x100,
	AAPTR_PLAYER4 = 0x200,
	AAPTR_PLAYER5 = 0x400,
	AAPTR_PLAYER6 = 0x800,
	AAPTR_PLAYER7 = 0x1000,
	AAPTR_PLAYER8 = 0x2000,

	AAPTR_FRIENDPLAYER = 0x4000,

	AAPTR_PLAYER_SELECTORS =
		AAPTR_PLAYER_GETTARGET|AAPTR_PLAYER_GETCONVERSATION,

	AAPTR_GENERAL_SELECTORS =
		AAPTR_TARGET|AAPTR_MASTER|AAPTR_TRACER|AAPTR_FRIENDPLAYER,

	AAPTR_STATIC_SELECTORS =
		AAPTR_PLAYER1|AAPTR_PLAYER2|AAPTR_PLAYER3|AAPTR_PLAYER4|
		AAPTR_PLAYER5|AAPTR_PLAYER6|AAPTR_PLAYER7|AAPTR_PLAYER8|
		AAPTR_NULL

};

/*
	PROCESS_AAPTR

	Result overview in order of priority:

	1. Caller is player and a player specific selector is specified: Player specific selector is used.
	2. Caller is non-null and a general actor selector is specified: General actor selector is used.
	3. A static actor selector is specified: Static actor selector is used.
	4. The origin actor is used.

	Only one selector of each type can be used.
*/

#define AAPTR_RESOLVE_PLAYERNUM(playernum) (playeringame[playernum] ? players[playernum].mo : NULL)

static AActor *PROCESS_AAPTR(AActor *origin, int selector)
{
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

#define COPY_AAPTR_NOT_NULL(source, destination, selector) { destination = PROCESS_AAPTR(source, selector); if (!destination) return; }
#define COPY_AAPTR(source, destination, selector) { destination = PROCESS_AAPTR(source, selector); }