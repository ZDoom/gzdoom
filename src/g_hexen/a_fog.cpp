/*
#include "m_random.h"
#include "p_local.h"
#include "thingdef/thingdef.h"
*/

static FRandom pr_fogspawn ("FogSpawn");

//==========================================================================
// Fog Variables:
//
//		args[0]		Speed (0..10) of fog
//		args[1]		Angle of spread (0..128)
//		args[2]		Frequency of spawn (1..10)
//		args[3]		Lifetime countdown
//		args[4]		Boolean: fog moving?
//		special1	Internal:  Counter for spawn frequency
//		WeaveIndexZ	Internal:  Index into floatbob table
//
//==========================================================================

//==========================================================================
//
// A_FogSpawn
//
//==========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_FogSpawn)
{
	PARAM_ACTION_PROLOGUE;

	static const char *fogs[3] =
	{
		"FogPatchSmall",
		"FogPatchMedium",
		"FogPatchLarge"
	};

	AActor *mo = NULL;
	int delta;

	if (self->special1-- > 0)
	{
		return 0;
	}
	self->special1 = self->args[2];		// Reset frequency count

	mo = Spawn (fogs[pr_fogspawn()%3], self->Pos(), ALLOW_REPLACE);

	if (mo)
	{
		delta = self->args[1];
		if (delta==0) delta=1;
		mo->Angles.Yaw = self->Angles.Yaw + (((pr_fogspawn() % delta) - (delta >> 1)) * (360 / 256.));
		mo->target = self;
		if (self->args[0] < 1) self->args[0] = 1;
		mo->args[0] = (pr_fogspawn() % (self->args[0]))+1;	// Random speed
		mo->args[3] = self->args[3];						// Set lifetime
		mo->args[4] = 1;									// Set to moving
		mo->WeaveIndexZ = pr_fogspawn()&63;
	}
	return 0;
}

//==========================================================================
//
// A_FogMove
//
//==========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_FogMove)
{
	PARAM_ACTION_PROLOGUE;

	double speed = self->args[0];
	int weaveindex;

	if (!self->args[4])
	{
		return 0;
	}

	if (self->args[3]-- <= 0)
	{
		self->SetState (self->FindState(NAME_Death), true);
		return 0;
	}

	if ((self->args[3] % 4) == 0)
	{
		weaveindex = self->WeaveIndexZ;
		self->AddZ(BobSin(weaveindex) / 2);
		self->WeaveIndexZ = (weaveindex + 1) & 63;
	}

	self->VelFromAngle(speed);
	return 0;
}

