#include "m_random.h"
#include "p_local.h"

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

void A_FogSpawn (AActor *actor)
{
	static const PClass *fogs[3] =
	{
		PClass::FindClass ("FogPatchSmall"),
		PClass::FindClass ("FogPatchMedium"),
		PClass::FindClass ("FogPatchLarge")
	};

	AActor *mo=NULL;
	angle_t delta;

	if (actor->special1-- > 0) return;

	actor->special1 = actor->args[2];		// Reset frequency count

	mo = Spawn (fogs[pr_fogspawn()%3], actor->x, actor->y, actor->z, ALLOW_REPLACE);

	if (mo)
	{
		delta = actor->args[1];
		if (delta==0) delta=1;
		mo->angle = actor->angle + (((pr_fogspawn()%delta)-(delta>>1))<<24);
		mo->target = actor;
		if (actor->args[0] < 1) actor->args[0] = 1;
		mo->args[0] = (pr_fogspawn() % (actor->args[0]))+1;	// Random speed
		mo->args[3] = actor->args[3];						// Set lifetime
		mo->args[4] = 1;									// Set to moving
		mo->special2 = pr_fogspawn()&63;
	}
}

//==========================================================================
//
// A_FogMove
//
//==========================================================================

void A_FogMove (AActor *actor)
{
	int speed = actor->args[0]<<FRACBITS;
	angle_t angle;
	int weaveindex;

	if (!(actor->args[4])) return;

	if (actor->args[3]-- <= 0)
	{
		actor->SetStateNF (actor->FindState(NAME_Death));
		return;
	}

	if ((actor->args[3] % 4) == 0)
	{
		weaveindex = actor->special2;
		actor->z += FloatBobOffsets[weaveindex]>>1;
		actor->special2 = (weaveindex+1)&63;
	}

	angle = actor->angle>>ANGLETOFINESHIFT;
	actor->momx = FixedMul(speed, finecosine[angle]);
	actor->momy = FixedMul(speed, finesine[angle]);
}

