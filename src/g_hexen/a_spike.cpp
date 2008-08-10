#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "p_enemy.h"
#include "p_local.h"
#include "a_sharedglobal.h"
#include "s_sound.h"
#include "m_bbox.h"

static FRandom pr_thrustraise ("ThrustRaise");

// Spike (thrust floor) -----------------------------------------------------

void A_ThrustInitUp (AActor *);
void A_ThrustInitDn (AActor *);
void A_ThrustRaise (AActor *);
void A_ThrustLower (AActor *);
void A_ThrustImpale (AActor *);

// AThrustFloor is just a container for all the spike states.
// All the real spikes subclass it.

class AThrustFloor : public AActor
{
	DECLARE_CLASS (AThrustFloor, AActor)
public:
	void Serialize (FArchive &arc);

	void Activate (AActor *activator);
	void Deactivate (AActor *activator);

	TObjPtr<AActor> DirtClump;
};

IMPLEMENT_CLASS (AThrustFloor)

void AThrustFloor::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << DirtClump;
}

void AThrustFloor::Activate (AActor *activator)
{
	if (args[0] == 0)
	{
		S_Sound (this, CHAN_BODY, "ThrustSpikeLower", 1, ATTN_NORM);
		renderflags &= ~RF_INVISIBLE;
		if (args[1])
			SetState (FindState ("BloodThrustRaise"));
		else
			SetState (FindState ("ThrustRaise"));
	}
}

void AThrustFloor::Deactivate (AActor *activator)
{
	if (args[0] == 1)
	{
		S_Sound (this, CHAN_BODY, "ThrustSpikeRaise", 1, ATTN_NORM);
		if (args[1])
			SetState (FindState ("BloodThrustLower"));
		else
			SetState (FindState ("ThrustLower"));
	}
}

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
		Spawn("DirtClump", actor->x, actor->y, actor->z, ALLOW_REPLACE);
}


void A_ThrustRaise (AActor *self)
{
	AThrustFloor *actor = static_cast<AThrustFloor *>(self);

	if (A_RaiseMobj (actor, self->special2*FRACUNIT))
	{	// Reached it's target height
		actor->args[0] = 1;
		if (actor->args[1])
			actor->SetStateNF (actor->FindState ("BloodThrustInit2"));
		else
			actor->SetStateNF (actor->FindState ("ThrustInit2"));
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
	if (A_SinkMobj (actor, 6*FRACUNIT))
	{
		actor->args[0] = 0;
		if (actor->args[1])
			actor->SetStateNF (actor->FindState ("BloodThrustInit1"));
		else
			actor->SetStateNF (actor->FindState ("ThrustInit1"));
	}
}

void A_ThrustImpale (AActor *actor)
{
	AActor *thing;
	FBlockThingsIterator it(FBoundingBox(actor->x, actor->y, actor->radius));
	while ((thing = it.Next()))
	{
		if (!thing->intersects(actor))
		{
			continue;
		}

		if (!(thing->flags & MF_SHOOTABLE) )
			continue;

		if (thing == actor)
			continue;	// don't clip against self

		P_DamageMobj (thing, actor, actor, 10001, NAME_Crush);
		P_TraceBleed (10001, thing);
		actor->args[1] = 1;	// Mark thrust thing as bloody
	}
}

