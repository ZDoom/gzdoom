#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "p_enemy.h"
#include "p_local.h"
#include "a_sharedglobal.h"
#include "s_sound.h"

static FRandom pr_thrustraise ("ThrustRaise");

// Dirt clump (spawned by spike) --------------------------------------------

class ADirtClump : public AActor
{
	DECLARE_ACTOR (ADirtClump, AActor)
};

FState ADirtClump::States[] =
{
	S_NORMAL (TSPK, 'C',   20, NULL, &States[0])
};

IMPLEMENT_ACTOR (ADirtClump, Hexen, -1, 0)
	PROP_Flags (MF_NOBLOCKMAP)
	PROP_Flags2 (MF2_NOTELEPORT)
	PROP_SpawnState (0)
END_DEFAULTS

// Spike (thrust floor) -----------------------------------------------------

void A_ThrustInitUp (AActor *);
void A_ThrustInitDn (AActor *);
void A_ThrustRaise (AActor *);
void A_ThrustLower (AActor *);
void A_ThrustBlock (AActor *);
void A_ThrustImpale (AActor *);

AActor *tsthing;

bool PIT_ThrustStompThing (AActor *thing)
{
	fixed_t blockdist;

	if (!(thing->flags & MF_SHOOTABLE) )
		return true;

	blockdist = thing->radius + tsthing->radius;
	if ( abs(thing->x - tsthing->x) >= blockdist || 
		  abs(thing->y - tsthing->y) >= blockdist ||
			(thing->z > tsthing->z+tsthing->height) )
		return true;	// didn't hit it

	if (thing == tsthing)
		return true;	// don't clip against self

	P_DamageMobj (thing, tsthing, tsthing, 10001, NAME_Crush);
	P_TraceBleed (10001, thing);
	tsthing->args[1] = 1;	// Mark thrust thing as bloody

	return true;
}

void P_ThrustSpike (AActor *actor)
{
	static TArray<AActor *> spikebt;

	int xl,xh,yl,yh,bx,by;
	int x0,x2,y0,y2;

	tsthing = actor;

	x0 = actor->x - actor->radius;
	x2 = actor->x + actor->radius;
	y0 = actor->y - actor->radius;
	y2 = actor->y + actor->radius;

	xl = (x0 - bmaporgx - MAXRADIUS)>>MAPBLOCKSHIFT;
	xh = (x2 - bmaporgx + MAXRADIUS)>>MAPBLOCKSHIFT;
	yl = (y0 - bmaporgy - MAXRADIUS)>>MAPBLOCKSHIFT;
	yh = (y2 - bmaporgy + MAXRADIUS)>>MAPBLOCKSHIFT;

	spikebt.Clear();

	// stomp on any things contacted
	for (bx = xl; bx <= xh; bx++)
		for (by = yl; by <= yh; by++)
			P_BlockThingsIterator (bx, by, PIT_ThrustStompThing, spikebt);
}

// AThrustFloor is just a container for all the spike states.
// All the real spikes subclass it.

class AThrustFloor : public AActor
{
	DECLARE_ACTOR (AThrustFloor, AActor)
	HAS_OBJECT_POINTERS
public:
	void Serialize (FArchive &arc);

	fixed_t GetSinkSpeed () { return 6*FRACUNIT; }
	fixed_t GetRaiseSpeed () { return special2*FRACUNIT; }

	void Activate (AActor *activator);
	void Deactivate (AActor *activator);

	ADirtClump *DirtClump;
};

IMPLEMENT_POINTY_CLASS (AThrustFloor)
 DECLARE_POINTER (DirtClump)
END_POINTERS

void AThrustFloor::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << DirtClump;
}

FState AThrustFloor::States[] =
{
#define S_THRUSTRAISING 0
	S_NORMAL (TSPK, 'A',	2, A_ThrustRaise			, &States[S_THRUSTRAISING]),

#define S_BTHRUSTRAISING (S_THRUSTRAISING+1)
	S_NORMAL (TSPK, 'B',	2, A_ThrustRaise			, &States[S_BTHRUSTRAISING]),

#define S_THRUSTIMPALE (S_BTHRUSTRAISING+1)
	S_NORMAL (TSPK, 'A',	2, A_ThrustImpale			, &States[S_THRUSTRAISING]),

#define S_BTHRUSTIMPALE (S_THRUSTIMPALE+1)
	S_NORMAL (TSPK, 'B',	2, A_ThrustImpale			, &States[S_BTHRUSTRAISING]),

#define S_THRUSTBLOCK (S_BTHRUSTIMPALE+1)
	S_NORMAL (TSPK, 'A',   10, NULL 					, &States[S_THRUSTBLOCK]),

#define S_BTHRUSTBLOCK (S_THRUSTBLOCK+1)
	S_NORMAL (TSPK, 'B',   10, NULL 					, &States[S_BTHRUSTBLOCK]),

#define S_THRUSTLOWER (S_BTHRUSTBLOCK+1)
	S_NORMAL (TSPK, 'A',	2, A_ThrustLower			, &States[S_THRUSTLOWER]),

#define S_BTHRUSTLOWER (S_THRUSTLOWER+1)
	S_NORMAL (TSPK, 'B',	2, A_ThrustLower			, &States[S_BTHRUSTLOWER]),

#define S_THRUSTSTAY (S_BTHRUSTLOWER+1)
	S_NORMAL (TSPK, 'A',   -1, NULL 					, &States[S_THRUSTSTAY]),

#define S_BTHRUSTSTAY (S_THRUSTSTAY+1)
	S_NORMAL (TSPK, 'B',   -1, NULL 					, &States[S_BTHRUSTSTAY]),

#define S_THRUSTINIT2 (S_BTHRUSTSTAY+1)
	S_NORMAL (TSPK, 'A',	3, NULL 					, &States[S_THRUSTINIT2+1]),
	S_NORMAL (TSPK, 'A',	4, A_ThrustInitUp			, &States[S_THRUSTBLOCK]),

#define S_BTHRUSTINIT2 (S_THRUSTINIT2+2)
	S_NORMAL (TSPK, 'B',	3, NULL 					, &States[S_BTHRUSTINIT2+1]),
	S_NORMAL (TSPK, 'B',	4, A_ThrustInitUp			, &States[S_BTHRUSTBLOCK]),

#define S_THRUSTINIT1 (S_BTHRUSTINIT2+2)
	S_NORMAL (TSPK, 'A',	3, NULL 					, &States[S_THRUSTINIT1+1]),
	S_NORMAL (TSPK, 'A',	4, A_ThrustInitDn			, &States[S_THRUSTSTAY]),

#define S_BTHRUSTINIT1 (S_THRUSTINIT1+2)
	S_NORMAL (TSPK, 'B',	3, NULL 					, &States[S_BTHRUSTINIT1+1]),
	S_NORMAL (TSPK, 'B',	4, A_ThrustInitDn			, &States[S_BTHRUSTSTAY]),

#define S_THRUSTRAISE (S_BTHRUSTINIT1+2)
	S_NORMAL (TSPK, 'A',	8, A_ThrustRaise			, &States[S_THRUSTRAISE+1]),
	S_NORMAL (TSPK, 'A',	6, A_ThrustRaise			, &States[S_THRUSTRAISE+2]),
	S_NORMAL (TSPK, 'A',	4, A_ThrustRaise			, &States[S_THRUSTRAISE+3]),
	S_NORMAL (TSPK, 'A',	3, A_ThrustBlock			, &States[S_THRUSTIMPALE]),

#define S_BTHRUSTRAISE (S_THRUSTRAISE+4)
	S_NORMAL (TSPK, 'B',	8, A_ThrustRaise			, &States[S_BTHRUSTRAISE+1]),
	S_NORMAL (TSPK, 'B',	6, A_ThrustRaise			, &States[S_BTHRUSTRAISE+2]),
	S_NORMAL (TSPK, 'B',	4, A_ThrustRaise			, &States[S_BTHRUSTRAISE+3]),
	S_NORMAL (TSPK, 'B',	3, A_ThrustBlock			, &States[S_BTHRUSTIMPALE]),

};

BEGIN_DEFAULTS (AThrustFloor, Hexen, -1, 0)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (128)
END_DEFAULTS

void AThrustFloor::Activate (AActor *activator)
{
	if (args[0] == 0)
	{
		S_Sound (this, CHAN_BODY, "ThrustSpikeLower", 1, ATTN_NORM);
		renderflags &= ~RF_INVISIBLE;
		if (args[1])
			SetState (&States[S_BTHRUSTRAISE]);
		else
			SetState (&States[S_THRUSTRAISE]);
	}
}

void AThrustFloor::Deactivate (AActor *activator)
{
	if (args[0] == 1)
	{
		S_Sound (this, CHAN_BODY, "ThrustSpikeRaise", 1, ATTN_NORM);
		if (args[1])
			SetState (&States[S_BTHRUSTLOWER]);
		else
			SetState (&States[S_THRUSTLOWER]);
	}
}

// Spike up -----------------------------------------------------------------

class AThrustFloorUp : public AThrustFloor
{
	DECLARE_STATELESS_ACTOR (AThrustFloorUp, AThrustFloor)
};

IMPLEMENT_STATELESS_ACTOR (AThrustFloorUp, Hexen, 10091, 104)
	PROP_Flags (MF_SOLID)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_FLOORCLIP)
	PROP_SpawnState (S_THRUSTINIT2)
END_DEFAULTS

// Spike down ---------------------------------------------------------------

class AThrustFloorDown : public AThrustFloor
{
	DECLARE_STATELESS_ACTOR (AThrustFloorDown, AThrustFloor)
};

IMPLEMENT_STATELESS_ACTOR (AThrustFloorDown, Hexen, 10090, 105)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_FLOORCLIP)
	PROP_RenderFlags (RF_INVISIBLE)
	PROP_SpawnState (S_THRUSTINIT1)
END_DEFAULTS

//===========================================================================
//
// Thrust floor stuff
//
// Thrust Spike Variables
//		DirtClump	pointer to dirt clump actor
//		special2	speed of raise
//		args[0]		0 = lowered,  1 = raised
//		args[1]		0 = normal,   1 = bloody
//===========================================================================

void A_ThrustInitUp (AActor *actor)
{
	actor->special2 = 5;	// Raise speed
	actor->args[0] = 1;		// Mark as up
	actor->floorclip = 0;
	actor->flags = MF_SOLID;
	actor->flags2 = MF2_NOTELEPORT|MF2_FLOORCLIP;
	actor->special1 = 0L;
}

void A_ThrustInitDn (AActor *actor)
{
	actor->special2 = 5;	// Raise speed
	actor->args[0] = 0;		// Mark as down
	actor->floorclip = actor->GetDefault()->height;
	actor->flags = 0;
	actor->flags2 = MF2_NOTELEPORT|MF2_FLOORCLIP;
	actor->renderflags = RF_INVISIBLE;
	static_cast<AThrustFloor *>(actor)->DirtClump =
		Spawn<ADirtClump> (actor->x, actor->y, actor->z, ALLOW_REPLACE);
}


void A_ThrustRaise (AActor *self)
{
	AThrustFloor *actor = static_cast<AThrustFloor *>(self);

	if (A_RaiseMobj (actor))
	{	// Reached it's target height
		actor->args[0] = 1;
		if (actor->args[1])
			actor->SetStateNF (&AThrustFloor::States[S_BTHRUSTINIT2]);
		else
			actor->SetStateNF (&AThrustFloor::States[S_THRUSTINIT2]);
	}

	// Lose the dirt clump
	if ((actor->floorclip < actor->height) && actor->DirtClump)
	{
		actor->DirtClump->Destroy ();
		actor->DirtClump = NULL;
	}

	// Spawn some dirt
	if (pr_thrustraise()<40)
		P_SpawnDirt (actor, actor->radius);
	actor->special2++;							// Increase raise speed
}

void A_ThrustLower (AActor *actor)
{
	if (A_SinkMobj (actor))
	{
		actor->args[0] = 0;
		if (actor->args[1])
			actor->SetStateNF (&AThrustFloor::States[S_BTHRUSTINIT1]);
		else
			actor->SetStateNF (&AThrustFloor::States[S_THRUSTINIT1]);
	}
}

void A_ThrustBlock (AActor *actor)
{
	actor->flags |= MF_SOLID;
}

void A_ThrustImpale (AActor *actor)
{
	// Impale all shootables in radius
	P_ThrustSpike (actor);
}

