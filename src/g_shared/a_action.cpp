#include "actor.h"
#include "thingdef/thingdef.h"
#include "p_conversation.h"
#include "p_lnspec.h"
#include "a_action.h"
#include "m_random.h"
#include "s_sound.h"
#include "d_player.h"
#include "p_local.h"
#include "p_terrain.h"
#include "p_enemy.h"
#include "statnums.h"
#include "templates.h"

static FRandom pr_freezedeath ("FreezeDeath");
static FRandom pr_icesettics ("IceSetTics");
static FRandom pr_freeze ("FreezeDeathChunks");

/***************************** IceChunk ************************************/

class AIceChunk : public AActor
{
	DECLARE_ACTOR (AIceChunk, AActor)
};

FState AIceChunk::States[] =
{
	S_NORMAL (ICEC, 'A',   10, NULL 					, &States[1]),
	S_NORMAL (ICEC, 'B',   10, A_IceSetTics 			, &States[2]),
	S_NORMAL (ICEC, 'C',   10, A_IceSetTics 			, &States[3]),
	S_NORMAL (ICEC, 'D',   10, A_IceSetTics 			, NULL),
};

IMPLEMENT_ACTOR (AIceChunk, Any, -1, 0)
	PROP_RadiusFixed (3)
	PROP_HeightFixed (4)
	PROP_Mass(5)
	PROP_Gravity (FRACUNIT/8)
	PROP_Flags (MF_DROPOFF)
	PROP_Flags2 (MF2_CANNOTPUSH|MF2_FLOORCLIP|MF2_NOTELEPORT)

	PROP_SpawnState (0)
END_DEFAULTS

/***************************************************************************/

// A chunk of ice that is also a player -------------------------------------

class AIceChunkHead : public APlayerChunk
{
	DECLARE_ACTOR (AIceChunkHead, APlayerChunk)
};

FState AIceChunkHead::States[] =
{
	S_NORMAL (PLAY, 'A',	0, NULL						, &States[1]),
	S_NORMAL (ICEC, 'A',   10, A_IceCheckHeadDone		, &States[1])
};

IMPLEMENT_ACTOR (AIceChunkHead, Any, -1, 0)
	PROP_RadiusFixed (3)
	PROP_HeightFixed (4)
	PROP_Mass(5)
	PROP_DamageType (NAME_Ice)
	PROP_Gravity (FRACUNIT/8)
	PROP_Flags (MF_DROPOFF)
	PROP_Flags2 (MF2_CANNOTPUSH)

	PROP_SpawnState (0)
END_DEFAULTS

//----------------------------------------------------------------------------
//
// PROC A_NoBlocking
//
//----------------------------------------------------------------------------

void A_NoBlocking (AActor *actor)
{
	// [RH] Andy Baker's stealth monsters
	if (actor->flags & MF_STEALTH)
	{
		actor->alpha = OPAQUE;
		actor->visdir = 0;
	}

	actor->flags &= ~MF_SOLID;

	// If the actor has a conversation that sets an item to drop, drop that.
	if (actor->Conversation != NULL && actor->Conversation->DropType != NULL)
	{
		P_DropItem (actor, actor->Conversation->DropType, -1, 256);
		actor->Conversation = NULL;
		return;
	}

	actor->Conversation = NULL;

	// If the actor has attached metadata for items to drop, drop those.
	// Otherwise, call NoBlockingSet() and let it decide what to drop.
	if (!actor->IsKindOf (RUNTIME_CLASS (APlayerPawn)))	// [GRB]
	{
		FDropItem *di = GetDropItems(RUNTIME_TYPE(actor));

		if (di != NULL)
		{
			while (di != NULL)
			{
				if (di->Name != NAME_None)
				{
					const PClass *ti = PClass::FindClass(di->Name);
					if (ti) P_DropItem (actor, ti, di->amount, di->probability);
				}
				di = di->Next;
			}
		}
		else
		{
			actor->NoBlockingSet ();
		}
	}
}

//==========================================================================
//
// A_SetFloorClip
//
//==========================================================================

void A_SetFloorClip (AActor *actor)
{
	actor->flags2 |= MF2_FLOORCLIP;
	actor->AdjustFloorClip ();
}

//==========================================================================
//
// A_UnSetFloorClip
//
//==========================================================================

void A_UnSetFloorClip (AActor *actor)
{
	actor->flags2 &= ~MF2_FLOORCLIP;
	actor->floorclip = 0;
}

//==========================================================================
//
// A_HideThing
//
//==========================================================================

void A_HideThing (AActor *actor)
{
	actor->renderflags |= RF_INVISIBLE;
}

//==========================================================================
//
// A_UnHideThing
//
//==========================================================================

void A_UnHideThing (AActor *actor)
{
	actor->renderflags &= ~RF_INVISIBLE;
}

//============================================================================
//
// A_FreezeDeath
//
//============================================================================

void A_FreezeDeath (AActor *actor)
{
	int t = pr_freezedeath();
	actor->tics = 75+t+pr_freezedeath();
	actor->flags |= MF_SOLID|MF_SHOOTABLE|MF_NOBLOOD|MF_ICECORPSE;
	actor->flags2 |= MF2_PUSHABLE|MF2_TELESTOMP|MF2_PASSMOBJ|MF2_SLIDE;
	actor->flags3 |= MF3_CRASHED;
	actor->height = actor->GetDefault()->height;
	S_Sound (actor, CHAN_BODY, "misc/freeze", 1, ATTN_NORM);

	// [RH] Andy Baker's stealth monsters
	if (actor->flags & MF_STEALTH)
	{
		actor->alpha = OPAQUE;
		actor->visdir = 0;
	}

	if (actor->player)
	{
		actor->player->damagecount = 0;
		actor->player->poisoncount = 0;
		actor->player->bonuscount = 0;
	}
	else if (actor->flags3&MF3_ISMONSTER && actor->special)
	{ // Initiate monster death actions
		LineSpecials [actor->special] (NULL, actor, false, actor->args[0],
			actor->args[1], actor->args[2], actor->args[3], actor->args[4]);
		actor->special = 0;
	}
}

//============================================================================
//
// A_IceSetTics
//
//============================================================================

void A_IceSetTics (AActor *actor)
{
	int floor;

	actor->tics = 70+(pr_icesettics()&63);
	floor = P_GetThingFloorType (actor);
	if (Terrains[floor].DamageMOD == NAME_Fire)
	{
		actor->tics >>= 2;
	}
	else if (Terrains[floor].DamageMOD == NAME_Ice)
	{
		actor->tics <<= 1;
	}
}

//============================================================================
//
// A_IceCheckHeadDone
//
//============================================================================

void A_IceCheckHeadDone (AActor *actor)
{
	if (actor->player == NULL)
	{
		actor->Destroy ();
	}
}

//============================================================================
//
// A_FreezeDeathChunks
//
//============================================================================

void A_FreezeDeathChunks (AActor *actor)
{

	int i;
	int numChunks;
	AActor *mo;
	
	if (actor->momx || actor->momy || actor->momz)
	{
		actor->tics = 3*TICRATE;
		return;
	}
	S_Sound (actor, CHAN_BODY, "misc/icebreak", 1, ATTN_NORM);

	// [RH] In Hexen, this creates a random number of shards (range [24,56])
	// with no relation to the size of the actor shattering. I think it should
	// base the number of shards on the size of the dead thing, so bigger
	// things break up into more shards than smaller things.
	// An actor with radius 20 and height 64 creates ~40 chunks.
	numChunks = MAX<int> (4, (actor->radius>>FRACBITS)*(actor->height>>FRACBITS)/32);
	i = (pr_freeze.Random2()) % (numChunks/4);
	for (i = MAX (24, numChunks + i); i >= 0; i--)
	{
		mo = Spawn<AIceChunk> (
			actor->x + (((pr_freeze()-128)*actor->radius)>>7), 
			actor->y + (((pr_freeze()-128)*actor->radius)>>7), 
			actor->z + (pr_freeze()*actor->height/255), ALLOW_REPLACE);
		mo->SetState (mo->SpawnState + (pr_freeze()%3));
		if (mo)
		{
			mo->momz = FixedDiv(mo->z-actor->z, actor->height)<<2;
			mo->momx = pr_freeze.Random2 () << (FRACBITS-7);
			mo->momy = pr_freeze.Random2 () << (FRACBITS-7);
			A_IceSetTics (mo); // set a random tic wait
			mo->RenderStyle = actor->RenderStyle;
			mo->alpha = actor->alpha;
		}
	}
	if (actor->player)
	{ // attach the player's view to a chunk of ice
		AIceChunkHead *head = Spawn<AIceChunkHead> (actor->x, actor->y, 
													actor->z + actor->player->mo->ViewHeight, ALLOW_REPLACE);
		head->momz = FixedDiv(head->z-actor->z, actor->height)<<2;
		head->momx = pr_freeze.Random2 () << (FRACBITS-7);
		head->momy = pr_freeze.Random2 () << (FRACBITS-7);
		head->player = actor->player;
		actor->player = NULL;
		head->ObtainInventory (actor);
		head->health = actor->health;
		head->angle = actor->angle;
		head->player->mo = head;
		head->pitch = 0;
		head->RenderStyle = actor->RenderStyle;
		head->alpha = actor->alpha;
		if (head->player->camera == actor)
		{
			head->player->camera = head;
		}
	}

	// [RH] Do some stuff to make this more useful outside Hexen
	if (actor->flags4 & MF4_BOSSDEATH)
	{
		A_BossDeath (actor);
	}
	A_NoBlocking (actor);

	actor->Destroy ();
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
	void Destroy ();
	void Serialize (FArchive &arc);
	AActor *Corpse;
	DWORD Count;	// Only the first corpse pointer's count is valid.
private:
	DCorpsePointer () {}
};

IMPLEMENT_POINTY_CLASS(DCorpsePointer)
 DECLARE_POINTER(Corpse)
END_POINTERS

CUSTOM_CVAR(Int, sv_corpsequeuesize, 64, CVAR_ARCHIVE|CVAR_SERVERINFO)
{
	if (self > 0)
	{
		TThinkerIterator<DCorpsePointer> iterator (STAT_CORPSEPOINTER);
		DCorpsePointer *first = iterator.Next ();
		while (first != NULL && first->Count > (DWORD)self)
		{
			DCorpsePointer *next = iterator.Next ();
			next->Count = first->Count;
			first->Destroy ();
			first = next;
		}
	}
}


DCorpsePointer::DCorpsePointer (AActor *ptr)
: DThinker (STAT_CORPSEPOINTER), Corpse (ptr)
{
	Count = 0;

	// Thinkers are added to the end of their respective lists, so
	// the first thinker in the list is the oldest one.
	TThinkerIterator<DCorpsePointer> iterator (STAT_CORPSEPOINTER);
	DCorpsePointer *first = iterator.Next ();

	if (first != this)
	{
		if (first->Count >= (DWORD)sv_corpsequeuesize)
		{
			DCorpsePointer *next = iterator.Next ();
			next->Count = first->Count;
			first->Destroy ();
			return;
		}
	}
	++first->Count;
}

void DCorpsePointer::Destroy ()
{
	// Store the count of corpses in the first thinker in the list
	TThinkerIterator<DCorpsePointer> iterator (STAT_CORPSEPOINTER);
	DCorpsePointer *first = iterator.Next ();

	int prevCount = first->Count;

	if (first == this)
	{
		first = iterator.Next ();
	}

	if (first != NULL)
	{
		first->Count = prevCount - 1;
	}

	if (Corpse != NULL)
	{
		Corpse->Destroy ();
	}
	Super::Destroy ();
}

void DCorpsePointer::Serialize (FArchive &arc)
{
	arc << Corpse << Count;
}


// throw another corpse on the queue
void A_QueueCorpse (AActor *actor)
{
	if (sv_corpsequeuesize > 0)
		new DCorpsePointer (actor);
}

// Remove an actor from the queue (for resurrection)
void A_DeQueueCorpse (AActor *actor)
{
	TThinkerIterator<DCorpsePointer> iterator (STAT_CORPSEPOINTER);
	DCorpsePointer *corpsePtr;

	while ((corpsePtr = iterator.Next()) != NULL)
	{
		if (corpsePtr->Corpse == actor)
		{
			corpsePtr->Corpse = NULL;
			corpsePtr->Destroy ();
			return;
		}
	}
}

//============================================================================
//
// A_SetInvulnerable
//
//============================================================================

void A_SetInvulnerable (AActor *actor)
{
	actor->flags2 |= MF2_INVULNERABLE;
}

//============================================================================
//
// A_UnSetInvulnerable
//
//============================================================================

void A_UnSetInvulnerable (AActor *actor)
{
	actor->flags2 &= ~MF2_INVULNERABLE;
}

//============================================================================
//
// A_SetReflective
//
//============================================================================

void A_SetReflective (AActor *actor)
{
	actor->flags2 |= MF2_REFLECTIVE;
}

//============================================================================
//
// A_UnSetReflective
//
//============================================================================

void A_UnSetReflective (AActor *actor)
{
	actor->flags2 &= ~MF2_REFLECTIVE;
}

//============================================================================
//
// A_SetReflectiveInvulnerable
//
//============================================================================

void A_SetReflectiveInvulnerable (AActor *actor)
{
	actor->flags2 |= MF2_REFLECTIVE|MF2_INVULNERABLE;
}

//============================================================================
//
// A_UnSetReflectiveInvulnerable
//
//============================================================================

void A_UnSetReflectiveInvulnerable (AActor *actor)
{
	actor->flags2 &= ~(MF2_REFLECTIVE|MF2_INVULNERABLE);
}

//==========================================================================
//
// A_SetShootable
//
//==========================================================================

void A_SetShootable (AActor *actor)
{
	actor->flags2 &= ~MF2_NONSHOOTABLE;
	actor->flags |= MF_SHOOTABLE;
}

//==========================================================================
//
// A_UnSetShootable
//
//==========================================================================

void A_UnSetShootable (AActor *actor)
{
	actor->flags2 |= MF2_NONSHOOTABLE;
	actor->flags &= ~MF_SHOOTABLE;
}

//===========================================================================
//
// A_NoGravity
//
//===========================================================================

void A_NoGravity (AActor *actor)
{
	actor->flags |= MF_NOGRAVITY;
}

//===========================================================================
//
// A_Gravity
//
//===========================================================================

void A_Gravity (AActor *actor)
{
	actor->flags &= ~MF_NOGRAVITY;
	actor->gravity = FRACUNIT;
}

//===========================================================================
//
// A_LowGravity
//
//===========================================================================

void A_LowGravity (AActor *actor)
{
	actor->flags &= ~MF_NOGRAVITY;
	actor->gravity = FRACUNIT/8;
}

//===========================================================================
//
// FaceMovementDirection
//
//===========================================================================

void FaceMovementDirection (AActor *actor)
{
	switch (actor->movedir)
	{
	case DI_EAST:
		actor->angle = 0<<24;
		break;
	case DI_NORTHEAST:
		actor->angle = 32<<24;
		break;
	case DI_NORTH:
		actor->angle = 64<<24;
		break;
	case DI_NORTHWEST:
		actor->angle = 96<<24;
		break;
	case DI_WEST:
		actor->angle = 128<<24;
		break;
	case DI_SOUTHWEST:
		actor->angle = 160<<24;
		break;
	case DI_SOUTH:
		actor->angle = 192<<24;
		break;
	case DI_SOUTHEAST:
		actor->angle = 224<<24;
		break;
	}
}

