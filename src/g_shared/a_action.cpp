#include "actor.h"
#include "info.h"
#include "p_lnspec.h"
#include "a_action.h"
#include "m_random.h"
#include "s_sound.h"
#include "d_player.h"
#include "p_local.h"
#include "p_terrain.h"
#include "p_enemy.h"

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
	PROP_Flags (MF_NOBLOCKMAP|MF_DROPOFF)
	PROP_Flags2 (MF2_LOGRAV|MF2_CANNOTPUSH|MF2_FLOORCLIP)

	PROP_SpawnState (0)
END_DEFAULTS

/***************************************************************************/

// A chunk of ice that is also a player -------------------------------------

class AIceChunkHead : public APlayerPawn
{
	DECLARE_ACTOR (AIceChunkHead, APlayerPawn)
};

FState AIceChunkHead::States[] =
{
	S_NORMAL (PLAY, 'A',	0, NULL						, &States[1]),
	S_NORMAL (ICEC, 'A',   10, A_IceCheckHeadDone		, &States[1])
};

IMPLEMENT_ACTOR (AIceChunkHead, Any, -1, 0)
	PROP_RadiusFixed (3)
	PROP_HeightFixed (4)
	PROP_Flags (MF_NOBLOCKMAP|MF_DROPOFF)
	PROP_Flags2 (MF2_LOGRAV|MF2_CANNOTPUSH|MF2_ICEDAMAGE)

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
	actor->NoBlockingSet ();
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
	actor->flags |= MF_SOLID|MF_SHOOTABLE|MF_NOBLOOD;
	actor->flags2 |= MF2_PUSHABLE|MF2_TELESTOMP|MF2_PASSMOBJ|MF2_SLIDE;
	actor->height <<= 2;
	S_Sound (actor, CHAN_BODY, "FreezeDeath", 1, ATTN_NORM);

	if (actor->player)
	{
		actor->player->damagecount = 0;
		actor->player->poisoncount = 0;
		actor->player->bonuscount = 0;
	}
	else if (actor->flags&MF_COUNTKILL && actor->special)
	{ // Initiate monster death actions
		LineSpecials [actor->special] (NULL, actor, actor->args[0],
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
	if (Terrains[floor].DamageMOD == MOD_LAVA)
	{
		actor->tics >>= 2;
	}
	else if (Terrains[floor].DamageMOD == MOD_ICE)
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
	AActor *mo;
	
	if (actor->momx || actor->momy || actor->momz)
	{
		actor->tics = 3*TICRATE;
		return;
	}
	S_Sound (actor, CHAN_BODY, "FreezeShatter", 1, ATTN_NORM);

	for (i = 24 + (pr_freeze()&31); i >= 0; i--)
	{
		mo = Spawn<AIceChunk> (
			actor->x + (((pr_freeze()-128)*actor->radius)>>7), 
			actor->y + (((pr_freeze()-128)*actor->radius)>>7), 
			actor->z + (pr_freeze()*actor->height/255));
		mo->SetState (mo->SpawnState + (pr_freeze()%3));
		if (mo)
		{
			mo->momz = FixedDiv(mo->z-actor->z, actor->height)<<2;
			mo->momx = pr_freeze.Random2 () << (FRACBITS-7);
			mo->momy = pr_freeze.Random2 () << (FRACBITS-7);
			A_IceSetTics (mo); // set a random tic wait
		}
	}
	if (actor->player)
	{ // attach the player's view to a chunk of ice
		AIceChunkHead *head = Spawn<AIceChunkHead> (actor->x, actor->y, actor->z+VIEWHEIGHT);
		head->momz = FixedDiv(head->z-actor->z, actor->height)<<2;
		head->momx = pr_freeze.Random2 () << (FRACBITS-7);
		head->momy = pr_freeze.Random2 () << (FRACBITS-7);
		head->player = actor->player;
		actor->player = NULL;
		head->health = actor->health;
		head->angle = actor->angle;
		head->player->mo = head;
		head->pitch = 0;
		if (head->player->camera == actor)
		{
			head->player->camera = head;
		}
	}
	actor->RemoveFromHash ();
	actor->SetState (&AActor::States[S_FREETARGMOBJ]);
	actor->renderflags |= RF_INVISIBLE;
}

//----------------------------------------------------------------------------
//
// CorpseQueue Routines (used by Hexen)
//
//----------------------------------------------------------------------------

// Corpse queue for monsters - this should be saved out
#define CORPSEQUEUESIZE	64

class DCorpseQueue : public DThinker
{
	DECLARE_CLASS (DCorpseQueue, DThinker)
	HAS_OBJECT_POINTERS
public:
	DCorpseQueue ();
	void Serialize (FArchive &arc);
	void Enqueue (AActor *);
	void Dequeue (AActor *);
protected:
	AActor *CorpseQueue[CORPSEQUEUESIZE];
	int CorpseQueueSlot;
};

IMPLEMENT_POINTY_CLASS (DCorpseQueue)
 DECLARE_POINTER (CorpseQueue[0])
 DECLARE_POINTER (CorpseQueue[1])
 DECLARE_POINTER (CorpseQueue[2])
 DECLARE_POINTER (CorpseQueue[3])
 DECLARE_POINTER (CorpseQueue[4])
 DECLARE_POINTER (CorpseQueue[5])
 DECLARE_POINTER (CorpseQueue[6])
 DECLARE_POINTER (CorpseQueue[7])
 DECLARE_POINTER (CorpseQueue[8])
 DECLARE_POINTER (CorpseQueue[9])
 DECLARE_POINTER (CorpseQueue[10])
 DECLARE_POINTER (CorpseQueue[11])
 DECLARE_POINTER (CorpseQueue[12])
 DECLARE_POINTER (CorpseQueue[13])
 DECLARE_POINTER (CorpseQueue[14])
 DECLARE_POINTER (CorpseQueue[15])
 DECLARE_POINTER (CorpseQueue[16])
 DECLARE_POINTER (CorpseQueue[17])
 DECLARE_POINTER (CorpseQueue[18])
 DECLARE_POINTER (CorpseQueue[19])
 DECLARE_POINTER (CorpseQueue[20])
 DECLARE_POINTER (CorpseQueue[21])
 DECLARE_POINTER (CorpseQueue[22])
 DECLARE_POINTER (CorpseQueue[23])
 DECLARE_POINTER (CorpseQueue[24])
 DECLARE_POINTER (CorpseQueue[25])
 DECLARE_POINTER (CorpseQueue[26])
 DECLARE_POINTER (CorpseQueue[27])
 DECLARE_POINTER (CorpseQueue[28])
 DECLARE_POINTER (CorpseQueue[29])
 DECLARE_POINTER (CorpseQueue[30])
 DECLARE_POINTER (CorpseQueue[31])
 DECLARE_POINTER (CorpseQueue[32])
 DECLARE_POINTER (CorpseQueue[33])
 DECLARE_POINTER (CorpseQueue[34])
 DECLARE_POINTER (CorpseQueue[35])
 DECLARE_POINTER (CorpseQueue[36])
 DECLARE_POINTER (CorpseQueue[37])
 DECLARE_POINTER (CorpseQueue[38])
 DECLARE_POINTER (CorpseQueue[39])
 DECLARE_POINTER (CorpseQueue[40])
 DECLARE_POINTER (CorpseQueue[41])
 DECLARE_POINTER (CorpseQueue[42])
 DECLARE_POINTER (CorpseQueue[43])
 DECLARE_POINTER (CorpseQueue[44])
 DECLARE_POINTER (CorpseQueue[45])
 DECLARE_POINTER (CorpseQueue[46])
 DECLARE_POINTER (CorpseQueue[47])
 DECLARE_POINTER (CorpseQueue[48])
 DECLARE_POINTER (CorpseQueue[49])
 DECLARE_POINTER (CorpseQueue[50])
 DECLARE_POINTER (CorpseQueue[51])
 DECLARE_POINTER (CorpseQueue[52])
 DECLARE_POINTER (CorpseQueue[53])
 DECLARE_POINTER (CorpseQueue[54])
 DECLARE_POINTER (CorpseQueue[55])
 DECLARE_POINTER (CorpseQueue[56])
 DECLARE_POINTER (CorpseQueue[57])
 DECLARE_POINTER (CorpseQueue[58])
 DECLARE_POINTER (CorpseQueue[59])
 DECLARE_POINTER (CorpseQueue[60])
 DECLARE_POINTER (CorpseQueue[61])
 DECLARE_POINTER (CorpseQueue[62])
 DECLARE_POINTER (CorpseQueue[63])
END_POINTERS

DCorpseQueue::DCorpseQueue ()
{
	CorpseQueueSlot = 0;
	memset (CorpseQueue, 0, sizeof(CorpseQueue));
}

void DCorpseQueue::Serialize (FArchive &arc)
{
	int i;

	Super::Serialize (arc);
	for (i = 0; i < CORPSEQUEUESIZE; i++)
		arc << CorpseQueue[i];
	arc << CorpseQueueSlot;
}

// throw another corpse on the queue
void DCorpseQueue::Enqueue (AActor *actor)
{
	if (CorpseQueue[CorpseQueueSlot] != NULL)
	{ // Too many corpses - remove an old one
		CorpseQueue[CorpseQueueSlot]->Destroy ();
	}
	CorpseQueue[CorpseQueueSlot] = actor;
	CorpseQueueSlot = (CorpseQueueSlot + 1) % CORPSEQUEUESIZE;
}

// Remove a mobj from the queue (for resurrection)
void DCorpseQueue::Dequeue (AActor *actor)
{
	int slot;

	for (slot = 0; slot < CORPSEQUEUESIZE; slot++)
	{
		if (CorpseQueue[slot] == actor)
		{
			CorpseQueue[slot] = NULL;
			break;
		}
	}
}

void A_QueueCorpse (AActor *actor)
{
	DCorpseQueue *queue;
	TThinkerIterator<DCorpseQueue> iterator;

	queue = iterator.Next ();
	if (queue == NULL)
	{
		queue = new DCorpseQueue;
	}
	queue->Enqueue (actor);
}

void A_DeQueueCorpse (AActor *actor)
{
	DCorpseQueue *queue;
	TThinkerIterator<DCorpseQueue> iterator;

	queue = iterator.Next ();
	if (queue == NULL)
	{
		queue = new DCorpseQueue;
	}
	queue->Dequeue (actor);
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
	actor->flags2 &= ~MF2_LOGRAV;
}

//===========================================================================
//
// A_LowGravity
//
//===========================================================================

void A_LowGravity (AActor *actor)
{
	actor->flags &= ~MF_NOGRAVITY;
	actor->flags2 |= MF2_LOGRAV;
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

//----------------------------------------------------------------------------
//
// PROC A_CheckBurnGone
//
//----------------------------------------------------------------------------

void A_CheckBurnGone (AActor *actor)
{
	if (actor->player == NULL)
	{
		actor->Destroy ();
	}
}
