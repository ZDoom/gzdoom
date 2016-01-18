/*
#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "p_local.h"
#include "s_sound.h"
#include "thingdef/thingdef.h"
*/

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

DEFINE_ACTION_FUNCTION(AActor, A_BatSpawnInit)
{
	self->special1 = 0;	// Frequency count
}

DEFINE_ACTION_FUNCTION(AActor, A_BatSpawn)
{
	AActor *mo;
	int delta;
	angle_t angle;

	// Countdown until next spawn
	if (self->special1-- > 0) return;
	self->special1 = self->args[0];		// Reset frequency count

	delta = self->args[1];
	if (delta==0) delta=1;
	angle = self->angle + (((pr_batspawn()%delta)-(delta>>1))<<24);
	mo = P_SpawnMissileAngle (self, PClass::FindClass ("Bat"), angle, 0);
	if (mo)
	{
		mo->args[0] = pr_batspawn()&63;			// floatbob index
		mo->args[4] = self->args[4];			// turn degrees
		mo->special2 = self->args[3]<<3;		// Set lifetime
		mo->target = self;
	}
}


DEFINE_ACTION_FUNCTION(AActor, A_BatMove)
{
	angle_t newangle;

	if (self->special2 < 0)
	{
		self->SetState (self->FindState(NAME_Death));
	}
	self->special2 -= 2;		// Called every 2 tics

	if (pr_batmove()<128)
	{
		newangle = self->angle + ANGLE_1*self->args[4];
	}
	else
	{
		newangle = self->angle - ANGLE_1*self->args[4];
	}

	// Adjust velocity vector to new direction
	newangle >>= ANGLETOFINESHIFT;
	self->velx = FixedMul (self->Speed, finecosine[newangle]);
	self->vely = FixedMul (self->Speed, finesine[newangle]);

	if (pr_batmove()<15)
	{
		S_Sound (self, CHAN_VOICE, "BatScream", 1, ATTN_IDLE);
	}

	// Handle Z movement
	self->SetZ(self->target->Z() + 16*finesine[self->args[0] << BOBTOFINESHIFT]);
	self->args[0] = (self->args[0]+3)&63;	
}
