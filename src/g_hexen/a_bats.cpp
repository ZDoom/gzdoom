#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "p_local.h"
#include "s_sound.h"

static FRandom pr_batspawn ("BatSpawn");
static FRandom pr_batmove ("BatMove");

//===========================================================================
// Bat Spawner Variables
//	special1	frequency counter
//	special2	
//	args[0]		frequency of spawn (1=fastest, 10=slowest)
//	args[1]		spread angle (0..255)
//	args[2]		
//	args[3]		duration of bats (in octics)
//	args[4]		turn amount per move (in degrees)
//
// Bat Variables
//	special2	lifetime counter
//	args[4]		turn amount per move (in degrees)
//===========================================================================

void A_BatSpawnInit (AActor *actor)
{
	actor->special1 = 0;	// Frequency count
}

void A_BatSpawn (AActor *actor)
{
	AActor *mo;
	int delta;
	angle_t angle;

	// Countdown until next spawn
	if (actor->special1-- > 0) return;
	actor->special1 = actor->args[0];		// Reset frequency count

	delta = actor->args[1];
	if (delta==0) delta=1;
	angle = actor->angle + (((pr_batspawn()%delta)-(delta>>1))<<24);
	mo = P_SpawnMissileAngle (actor, PClass::FindClass ("Bat"), angle, 0);
	if (mo)
	{
		mo->args[0] = pr_batspawn()&63;			// floatbob index
		mo->args[4] = actor->args[4];			// turn degrees
		mo->special2 = actor->args[3]<<3;		// Set lifetime
		mo->target = actor;
	}
}


void A_BatMove (AActor *actor)
{
	angle_t newangle;

	if (actor->special2 < 0)
	{
		actor->SetState (actor->FindState(NAME_Death));
	}
	actor->special2 -= 2;		// Called every 2 tics

	if (pr_batmove()<128)
	{
		newangle = actor->angle + ANGLE_1*actor->args[4];
	}
	else
	{
		newangle = actor->angle - ANGLE_1*actor->args[4];
	}

	// Adjust momentum vector to new direction
	newangle >>= ANGLETOFINESHIFT;
	actor->momx = FixedMul (actor->Speed, finecosine[newangle]);
	actor->momy = FixedMul (actor->Speed, finesine[newangle]);

	if (pr_batmove()<15)
	{
		S_Sound (actor, CHAN_VOICE, "BatScream", 1, ATTN_IDLE);
	}

	// Handle Z movement
	actor->z = actor->target->z + 2*FloatBobOffsets[actor->args[0]];
	actor->args[0] = (actor->args[0]+3)&63;	
}
