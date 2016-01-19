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
//		special2	Internal:  Index into floatbob table
//
//==========================================================================

//==========================================================================
//
// A_FogSpawn
//
//==========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_FogSpawn)
{
	static const char *fogs[3] =
	{
		"FogPatchSmall",
		"FogPatchMedium",
		"FogPatchLarge"
	};

	AActor *mo=NULL;
	angle_t delta;

	if (self->special1-- > 0) return;

	self->special1 = self->args[2];		// Reset frequency count

	mo = Spawn (fogs[pr_fogspawn()%3], self->Pos(), ALLOW_REPLACE);

	if (mo)
	{
		delta = self->args[1];
		if (delta==0) delta=1;
		mo->angle = self->angle + (((pr_fogspawn()%delta)-(delta>>1))<<24);
		mo->target = self;
		if (self->args[0] < 1) self->args[0] = 1;
		mo->args[0] = (pr_fogspawn() % (self->args[0]))+1;	// Random speed
		mo->args[3] = self->args[3];						// Set lifetime
		mo->args[4] = 1;									// Set to moving
		mo->special2 = pr_fogspawn()&63;
	}
}

//==========================================================================
//
// A_FogMove
//
//==========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_FogMove)
{
	int speed = self->args[0]<<FRACBITS;
	angle_t angle;
	int weaveindex;

	if (!(self->args[4])) return;

	if (self->args[3]-- <= 0)
	{
		self->SetState (self->FindState(NAME_Death), true);
		return;
	}

	if ((self->args[3] % 4) == 0)
	{
		weaveindex = self->special2;
		self->AddZ(finesine[weaveindex << BOBTOFINESHIFT] * 4);
		self->special2 = (weaveindex + 1) & 63;
	}

	angle = self->angle>>ANGLETOFINESHIFT;
	self->velx = FixedMul(speed, finecosine[angle]);
	self->vely = FixedMul(speed, finesine[angle]);
}

