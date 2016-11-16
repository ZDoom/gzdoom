/*
#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "p_enemy.h"
#include "p_local.h"
#include "a_sharedglobal.h"
#include "s_sound.h"
#include "m_bbox.h"
#include "vm.h"
*/

static FRandom pr_thrustraise ("ThrustRaise");

// Spike (thrust floor) -----------------------------------------------------

// AThrustFloor is just a container for all the spike states.
// All the real spikes subclass it.

class AThrustFloor : public AActor
{
	DECLARE_CLASS (AThrustFloor, AActor)
public:
	
	void Activate (AActor *activator);
	void Deactivate (AActor *activator);
};

IMPLEMENT_CLASS(AThrustFloor, false, false, false, false)

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

DEFINE_ACTION_FUNCTION(AActor, A_ThrustImpale)
{
	PARAM_SELF_PROLOGUE(AActor);

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

