#include "actor.h"
#include "info.h"
#include "a_pickups.h"
#include "a_action.h"
#include "m_random.h"
#include "p_local.h"
#include "s_sound.h"
#include "gstrings.h"
#include "thingdef/thingdef.h"
#include "p_enemy.h"
#include "a_specialspot.h"
#include "g_level.h"
#include "a_sharedglobal.h"
#include "templates.h"
#include "r_data/r_translate.h"
#include "doomstat.h"
#include "farchive.h"

// Include all the other Heretic stuff here to reduce compile time
#include "a_chicken.cpp"
#include "a_dsparil.cpp"
#include "a_hereticartifacts.cpp"
#include "a_hereticimp.cpp"
#include "a_hereticweaps.cpp"
#include "a_ironlich.cpp"
#include "a_knight.cpp"
#include "a_wizard.cpp"


static FRandom pr_podpain ("PodPain");
static FRandom pr_makepod ("MakePod");
static FRandom pr_teleg ("TeleGlitter");
static FRandom pr_teleg2 ("TeleGlitter2");
static FRandom pr_volcano ("VolcanoSet");
static FRandom pr_blast ("VolcanoBlast");
static FRandom pr_volcimpact ("VolcBallImpact");

//----------------------------------------------------------------------------
//
// PROC A_PodPain
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_PodPain)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_CLASS(gootype, 0);

	int count;
	int chance;
	AActor *goo;

	chance = pr_podpain ();
	if (chance < 128)
	{
		return;
	}
	for (count = chance > 240 ? 2 : 1; count; count--)
	{
		goo = Spawn(gootype, self->PosPlusZ(48*FRACUNIT), ALLOW_REPLACE);
		goo->target = self;
		goo->velx = pr_podpain.Random2() << 9;
		goo->vely = pr_podpain.Random2() << 9;
		goo->velz = FRACUNIT/2 + (pr_podpain() << 9);
	}
}

//----------------------------------------------------------------------------
//
// PROC A_RemovePod
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_RemovePod)
{
	AActor *mo;

	if ( (mo = self->master))
	{
		if (mo->special1 > 0)
		{
			mo->special1--;
		}
	}
}

//----------------------------------------------------------------------------
//
// PROC A_MakePod
//
//----------------------------------------------------------------------------

#define MAX_GEN_PODS 16

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_MakePod)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_CLASS(podtype, 0);

	AActor *mo;
	fixed_t x;
	fixed_t y;

	if (self->special1 == MAX_GEN_PODS)
	{ // Too many generated pods
		return;
	}
	x = self->X();
	y = self->Y();
	mo = Spawn(podtype, x, y, ONFLOORZ, ALLOW_REPLACE);
	if (!P_CheckPosition (mo, x, y))
	{ // Didn't fit
		mo->Destroy ();
		return;
	}
	mo->SetState (mo->FindState("Grow"));
	P_ThrustMobj (mo, pr_makepod()<<24, (fixed_t)(4.5*FRACUNIT));
	S_Sound (mo, CHAN_BODY, self->AttackSound, 1, ATTN_IDLE);
	self->special1++; // Increment generated pod count
	mo->master = self; // Link the generator to the pod
	return;
}

//----------------------------------------------------------------------------
//
// PROC A_AccTeleGlitter
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_AccTeleGlitter)
{
	if (++self->health > 35)
	{
		self->velz += self->velz/2;
	}
}


//----------------------------------------------------------------------------
//
// PROC A_VolcanoSet
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_VolcanoSet)
{
	self->tics = 105 + (pr_volcano() & 127);
}

//----------------------------------------------------------------------------
//
// PROC A_VolcanoBlast
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_VolcanoBlast)
{
	int i;
	int count;
	AActor *blast;
	angle_t angle;

	count = 1 + (pr_blast() % 3);
	for (i = 0; i < count; i++)
	{
		blast = Spawn("VolcanoBlast", self->PosPlusZ(44*FRACUNIT), ALLOW_REPLACE);
		blast->target = self;
		angle = pr_blast () << 24;
		blast->angle = angle;
		angle >>= ANGLETOFINESHIFT;
		blast->velx = FixedMul (1*FRACUNIT, finecosine[angle]);
		blast->vely = FixedMul (1*FRACUNIT, finesine[angle]);
		blast->velz = (FRACUNIT*5/2) + (pr_blast() << 10);
		S_Sound (blast, CHAN_BODY, "world/volcano/shoot", 1, ATTN_NORM);
		P_CheckMissileSpawn (blast, self->radius);
	}
}

//----------------------------------------------------------------------------
//
// PROC A_VolcBallImpact
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_VolcBallImpact)
{
	unsigned int i;
	AActor *tiny;
	angle_t angle;

	if (self->Z() <= self->floorz)
	{
		self->flags |= MF_NOGRAVITY;
		self->gravity = FRACUNIT;
		self->AddZ(28*FRACUNIT);
		//self->velz = 3*FRACUNIT;
	}
	P_RadiusAttack (self, self->target, 25, 25, NAME_Fire, RADF_HURTSOURCE);
	for (i = 0; i < 4; i++)
	{
		tiny = Spawn("VolcanoTBlast", self->Pos(), ALLOW_REPLACE);
		tiny->target = self;
		angle = i*ANG90;
		tiny->angle = angle;
		angle >>= ANGLETOFINESHIFT;
		tiny->velx = FixedMul (FRACUNIT*7/10, finecosine[angle]);
		tiny->vely = FixedMul (FRACUNIT*7/10, finesine[angle]);
		tiny->velz = FRACUNIT + (pr_volcimpact() << 9);
		P_CheckMissileSpawn (tiny, self->radius);
	}
}

