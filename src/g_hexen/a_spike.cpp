/*
#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "p_enemy.h"
#include "p_local.h"
#include "a_sharedglobal.h"
#include "s_sound.h"
#include "m_bbox.h"
#include "thingdef/thingdef.h"
*/

static FRandom pr_thrustraise ("ThrustRaise");

// Spike (thrust floor) -----------------------------------------------------

// AThrustFloor is just a container for all the spike states.
// All the real spikes subclass it.

class AThrustFloor : public AActor
{
	DECLARE_CLASS (AThrustFloor, AActor)
	HAS_OBJECT_POINTERS
public:
	void Serialize (FArchive &arc);

	void Activate (AActor *activator);
	void Deactivate (AActor *activator);

	TObjPtr<AActor> DirtClump;
};

IMPLEMENT_POINTY_CLASS (AThrustFloor)
 DECLARE_POINTER (DirtClump)
END_POINTERS

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

DEFINE_ACTION_FUNCTION(AActor, A_ThrustInitUp)
{
	PARAM_ACTION_PROLOGUE;

	self->special2 = 5;	// Raise speed
	self->args[0] = 1;		// Mark as up
	self->Floorclip = 0;
	self->flags = MF_SOLID;
	self->flags2 = MF2_NOTELEPORT|MF2_FLOORCLIP;
	self->special1 = 0L;
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_ThrustInitDn)
{
	PARAM_ACTION_PROLOGUE;

	self->special2 = 5;	// Raise speed
	self->args[0] = 0;		// Mark as down
	self->Floorclip = self->GetDefault()->Height;
	self->flags = 0;
	self->flags2 = MF2_NOTELEPORT|MF2_FLOORCLIP;
	self->renderflags = RF_INVISIBLE;
	static_cast<AThrustFloor *>(self)->DirtClump =
		Spawn("DirtClump", self->Pos(), ALLOW_REPLACE);
	return 0;
}


DEFINE_ACTION_FUNCTION(AActor, A_ThrustRaise)
{
	PARAM_ACTION_PROLOGUE;

	AThrustFloor *actor = static_cast<AThrustFloor *>(self);

	if (A_RaiseMobj (actor, self->special2))
	{	// Reached it's target height
		actor->args[0] = 1;
		if (actor->args[1])
			actor->SetState (actor->FindState ("BloodThrustInit2"), true);
		else
			actor->SetState (actor->FindState ("ThrustInit2"), true);
	}

	// Lose the dirt clump
	if ((actor->Floorclip < actor->Height) && actor->DirtClump)
	{
		actor->DirtClump->Destroy ();
		actor->DirtClump = NULL;
	}

	// Spawn some dirt
	if (pr_thrustraise()<40)
		P_SpawnDirt (actor, actor->radius);
	actor->special2++;							// Increase raise speed
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_ThrustLower)
{
	PARAM_ACTION_PROLOGUE;

	if (A_SinkMobj (self, 6))
	{
		self->args[0] = 0;
		if (self->args[1])
			self->SetState (self->FindState ("BloodThrustInit1"), true);
		else
			self->SetState (self->FindState ("ThrustInit1"), true);
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_ThrustImpale)
{
	PARAM_ACTION_PROLOGUE;

	// This doesn't need to iterate through portals.

	FPortalGroupArray check;
	FMultiBlockThingsIterator it(check, self);
	FMultiBlockThingsIterator::CheckResult cres;
	while (it.Next(&cres))
	{
		double blockdist = self->radius + cres.thing->radius;
		if (fabs(cres.thing->X() - cres.Position.X) >= blockdist || fabs(cres.thing->Y() - cres.Position.Y) >= blockdist)
			continue;

		// Q: Make this z-aware for everything? It never was before.
		if (cres.thing->Top() < self->Z() || cres.thing->Z() > self->Top())
		{
			if (self->Sector->PortalGroup != cres.thing->Sector->PortalGroup)
				continue;
		}

		if (!(cres.thing->flags & MF_SHOOTABLE) )
			continue;

		if (cres.thing == self)
			continue;	// don't clip against self

		int newdam = P_DamageMobj (cres.thing, self, self, 10001, NAME_Crush);
		P_TraceBleed (newdam > 0 ? newdam : 10001, cres.thing);
		self->args[1] = 1;	// Mark thrust thing as bloody
	}
	return 0;
}

