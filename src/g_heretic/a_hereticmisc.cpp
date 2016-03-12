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
#include "d_player.h"
#include "a_morph.h"

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
	PARAM_ACTION_PROLOGUE;
	PARAM_CLASS_OPT	(gootype, AActor)	{ gootype = PClass::FindActor("PodGoo"); }

	int count;
	int chance;
	AActor *goo;

	chance = pr_podpain ();
	if (chance < 128)
	{
		return 0;
	}
	for (count = chance > 240 ? 2 : 1; count; count--)
	{
		goo = Spawn(gootype, self->PosPlusZ(48*FRACUNIT), ALLOW_REPLACE);
		goo->target = self;
		goo->vel.x = pr_podpain.Random2() << 9;
		goo->vel.y = pr_podpain.Random2() << 9;
		goo->vel.z = FRACUNIT/2 + (pr_podpain() << 9);
	}
	return 0;
}

//----------------------------------------------------------------------------
//
// PROC A_RemovePod
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_RemovePod)
{
	PARAM_ACTION_PROLOGUE;

	AActor *mo;

	if ( (mo = self->master) )
	{
		if (mo->special1 > 0)
		{
			mo->special1--;
		}
	}
	return 0;
}

//----------------------------------------------------------------------------
//
// PROC A_MakePod
//
//----------------------------------------------------------------------------

#define MAX_GEN_PODS 16

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_MakePod)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_CLASS_OPT(podtype, AActor)	{ podtype = PClass::FindActor("Pod"); }

	AActor *mo;
	fixed_t x;
	fixed_t y;

	if (self->special1 == MAX_GEN_PODS)
	{ // Too many generated pods
		return 0;
	}
	x = self->X();
	y = self->Y();
	mo = Spawn(podtype, x, y, ONFLOORZ, ALLOW_REPLACE);
	if (!P_CheckPosition (mo, x, y))
	{ // Didn't fit
		mo->Destroy ();
		return 0;
	}
	mo->SetState (mo->FindState("Grow"));
	P_ThrustMobj (mo, pr_makepod()<<24, (fixed_t)(4.5*FRACUNIT));
	S_Sound (mo, CHAN_BODY, self->AttackSound, 1, ATTN_IDLE);
	self->special1++; // Increment generated pod count
	mo->master = self; // Link the generator to the pod
	return 0;
}

//----------------------------------------------------------------------------
//
// PROC A_AccTeleGlitter
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_AccTeleGlitter)
{
	PARAM_ACTION_PROLOGUE;

	if (++self->health > 35)
	{
		self->vel.z += self->vel.z/2;
	}
	return 0;
}


//----------------------------------------------------------------------------
//
// PROC A_VolcanoSet
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_VolcanoSet)
{
	PARAM_ACTION_PROLOGUE;

	self->tics = 105 + (pr_volcano() & 127);
	return 0;
}

//----------------------------------------------------------------------------
//
// PROC A_VolcanoBlast
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_VolcanoBlast)
{
	PARAM_ACTION_PROLOGUE;

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
		blast->vel.x = FixedMul (1*FRACUNIT, finecosine[angle]);
		blast->vel.y = FixedMul (1*FRACUNIT, finesine[angle]);
		blast->vel.z = (FRACUNIT*5/2) + (pr_blast() << 10);
		S_Sound (blast, CHAN_BODY, "world/volcano/shoot", 1, ATTN_NORM);
		P_CheckMissileSpawn (blast, self->radius);
	}
	return 0;
}

//----------------------------------------------------------------------------
//
// PROC A_VolcBallImpact
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_VolcBallImpact)
{
	PARAM_ACTION_PROLOGUE;

	unsigned int i;
	AActor *tiny;
	angle_t angle;

	if (self->Z() <= self->floorz)
	{
		self->flags |= MF_NOGRAVITY;
		self->gravity = FRACUNIT;
		self->AddZ(28*FRACUNIT);
		//self->vel.z = 3*FRACUNIT;
	}
	P_RadiusAttack (self, self->target, 25, 25, NAME_Fire, RADF_HURTSOURCE);
	for (i = 0; i < 4; i++)
	{
		tiny = Spawn("VolcanoTBlast", self->Pos(), ALLOW_REPLACE);
		tiny->target = self;
		angle = i*ANG90;
		tiny->angle = angle;
		angle >>= ANGLETOFINESHIFT;
		tiny->vel.x = FixedMul (FRACUNIT*7/10, finecosine[angle]);
		tiny->vel.y = FixedMul (FRACUNIT*7/10, finesine[angle]);
		tiny->vel.z = FRACUNIT + (pr_volcimpact() << 9);
		P_CheckMissileSpawn (tiny, self->radius);
	}
	return 0;
}

