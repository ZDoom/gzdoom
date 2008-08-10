#include "actor.h"
#include "gi.h"
#include "m_random.h"
#include "s_sound.h"
#include "d_player.h"
#include "a_action.h"
#include "p_local.h"
#include "p_enemy.h"
#include "a_action.h"
#include "p_pspr.h"
#include "gstrings.h"
#include "a_hexenglobal.h"

#define ZAGSPEED	FRACUNIT

static FRandom pr_lightningready ("LightningReady");
static FRandom pr_lightningclip ("LightningClip");
static FRandom pr_zap ("LightningZap");
static FRandom pr_zapf ("LightningZapF");
static FRandom pr_hit ("LightningHit");

void A_LightningReady (AActor *actor);
void A_MLightningAttack (AActor *actor);

void A_LightningClip (AActor *);
void A_LightningZap (AActor *);
void A_ZapMimic (AActor *);
void A_LastZap (AActor *);
void A_LightningRemove (AActor *);

// Lightning ----------------------------------------------------------------

class ALightning : public AActor
{
	DECLARE_CLASS (ALightning, AActor)
public:
	int SpecialMissileHit (AActor *victim);
};

IMPLEMENT_CLASS(ALightning)

int ALightning::SpecialMissileHit (AActor *thing)
{
	if (thing->flags&MF_SHOOTABLE && thing != target)
	{
		if (thing->Mass != INT_MAX)
		{
			thing->momx += momx>>4;
			thing->momy += momy>>4;
		}
		if ((!thing->player && !(thing->flags2&MF2_BOSS))
			|| !(level.time&1))
		{
			P_DamageMobj(thing, this, target, 3, NAME_Electric);
			if (!(S_IsActorPlayingSomething (this, CHAN_WEAPON, -1)))
			{
				S_Sound (this, CHAN_WEAPON, "MageLightningZap", 1, ATTN_NORM);
			}
			if (thing->flags3&MF3_ISMONSTER && pr_hit() < 64)
			{
				thing->Howl ();
			}
		}
		health--;
		if (health <= 0 || thing->health <= 0)
		{
			return 0;
		}
		if (flags3 & MF3_FLOORHUGGER)
		{
			if (lastenemy && ! lastenemy->tracer)
			{
				lastenemy->tracer = thing;
			}
		}
		else if (!tracer)
		{
			tracer = thing;
		}
	}
	return 1; // lightning zaps through all sprites
}

// Lightning Zap ------------------------------------------------------------

class ALightningZap : public AActor
{
	DECLARE_CLASS (ALightningZap, AActor)
public:
	int SpecialMissileHit (AActor *thing);
};

IMPLEMENT_CLASS (ALightningZap)

int ALightningZap::SpecialMissileHit (AActor *thing)
{
	AActor *lmo;

	if (thing->flags&MF_SHOOTABLE && thing != target)
	{			
		lmo = lastenemy;
		if (lmo)
		{
			if (lmo->flags3 & MF3_FLOORHUGGER)
			{
				if (lmo->lastenemy && !lmo->lastenemy->tracer)
				{
					lmo->lastenemy->tracer = thing;
				}
			}
			else if (!lmo->tracer)
			{
				lmo->tracer = thing;
			}
			if (!(level.time&3))
			{
				lmo->health--;
			}
		}
	}
	return -1;
}

//============================================================================
//
// A_LightningReady
//
//============================================================================

void A_LightningReady (AActor *actor)
{
	A_WeaponReady (actor);
	if (pr_lightningready() < 160)
	{
		S_Sound (actor, CHAN_WEAPON, "MageLightningReady", 1, ATTN_NORM);
	}
}

//============================================================================
//
// A_LightningClip
//
//============================================================================

void A_LightningClip (AActor *actor)
{
	AActor *cMo;
	AActor *target = NULL;
	int zigZag;

	if (actor->flags3 & MF3_FLOORHUGGER)
	{
		if (actor->lastenemy == NULL)
		{
			return;
		}
		actor->z = actor->floorz;
		target = actor->lastenemy->tracer;
	}
	else if (actor->flags3 & MF3_CEILINGHUGGER)
	{
		actor->z = actor->ceilingz-actor->height;
		target = actor->tracer;
	}
	if (actor->flags3 & MF3_FLOORHUGGER)
	{ // floor lightning zig-zags, and forces the ceiling lightning to mimic
		cMo = actor->lastenemy;
		zigZag = pr_lightningclip();
		if((zigZag > 128 && actor->special1 < 2) || actor->special1 < -2)
		{
			P_ThrustMobj(actor, actor->angle+ANG90, ZAGSPEED);
			if(cMo)
			{
				P_ThrustMobj(cMo, actor->angle+ANG90, ZAGSPEED);
			}
			actor->special1++;
		}
		else
		{
			P_ThrustMobj(actor, actor->angle-ANG90, ZAGSPEED);
			if(cMo)
			{
				P_ThrustMobj(cMo, cMo->angle-ANG90, ZAGSPEED);
			}
			actor->special1--;
		}
	}
	if(target)
	{
		if(target->health <= 0)
		{
			P_ExplodeMissile(actor, NULL, NULL);
		}
		else
		{
			actor->angle = R_PointToAngle2(actor->x, actor->y, target->x, target->y);
			actor->momx = 0;
			actor->momy = 0;
			P_ThrustMobj (actor, actor->angle, actor->Speed>>1);
		}
	}
}


//============================================================================
//
// A_LightningZap
//
//============================================================================

void A_LightningZap (AActor *actor)
{
	AActor *mo;
	fixed_t deltaZ;

	A_LightningClip(actor);

	actor->health -= 8;
	if (actor->health <= 0)
	{
		actor->SetState (actor->FindState(NAME_Death));
		return;
	}
	if (actor->flags3 & MF3_FLOORHUGGER)
	{
		deltaZ = 10*FRACUNIT;
	}
	else
	{
		deltaZ = -10*FRACUNIT;
	}
	mo = Spawn<ALightningZap> (actor->x+((pr_zap()-128)*actor->radius/256), 
		actor->y+((pr_zap()-128)*actor->radius/256), 
		actor->z+deltaZ, ALLOW_REPLACE);
	if (mo)
	{
		mo->lastenemy = actor;
		mo->momx = actor->momx;
		mo->momy = actor->momy;
		mo->target = actor->target;
		if (actor->flags3 & MF3_FLOORHUGGER)
		{
			mo->momz = 20*FRACUNIT;
		}
		else 
		{
			mo->momz = -20*FRACUNIT;
		}
	}
	if ((actor->flags3 & MF3_FLOORHUGGER) && pr_zapf() < 160)
	{
		S_Sound (actor, CHAN_BODY, "MageLightningContinuous", 1, ATTN_NORM);
	}
}

//============================================================================
//
// A_MLightningAttack2
//
//============================================================================

void A_MLightningAttack2 (AActor *actor)
{
	AActor *fmo, *cmo;

	fmo = P_SpawnPlayerMissile (actor, PClass::FindClass ("LightningFloor"));
	cmo = P_SpawnPlayerMissile (actor, PClass::FindClass ("LightningCeiling"));
	if (fmo)
	{
		fmo->special1 = 0;
		fmo->lastenemy = cmo;
		A_LightningZap (fmo);	
	}
	if (cmo)
	{
		cmo->tracer = NULL;
		cmo->lastenemy = fmo;
		A_LightningZap (cmo);	
	}
	S_Sound (actor, CHAN_BODY, "MageLightningFire", 1, ATTN_NORM);
}

//============================================================================
//
// A_MLightningAttack
//
//============================================================================

void A_MLightningAttack (AActor *actor)
{
	A_MLightningAttack2(actor);
	if (actor->player != NULL)
	{
		AWeapon *weapon = actor->player->ReadyWeapon;
		if (weapon != NULL)
		{
			weapon->DepleteAmmo (weapon->bAltFire);
		}
	}
}

//============================================================================
//
// A_ZapMimic
//
//============================================================================

void A_ZapMimic (AActor *actor)
{
	AActor *mo;

	mo = actor->lastenemy;
	if (mo)
	{
		if (mo->state >= mo->FindState(NAME_Death))
		{
			P_ExplodeMissile (actor, NULL, NULL);
		}
		else
		{
			actor->momx = mo->momx;
			actor->momy = mo->momy;
		}
	}
}

//============================================================================
//
// A_LastZap
//
//============================================================================

void A_LastZap (AActor *actor)
{
	AActor *mo;

	mo = Spawn<ALightningZap> (actor->x, actor->y, actor->z, ALLOW_REPLACE);
	if (mo)
	{
		mo->SetState (mo->FindState ("Death"));
		mo->momz = 40*FRACUNIT;
		mo->Damage = 0;
	}
}

//============================================================================
//
// A_LightningRemove
//
//============================================================================

void A_LightningRemove (AActor *actor)
{
	AActor *mo;

	mo = actor->lastenemy;
	if (mo)
	{
		mo->lastenemy = NULL;
		P_ExplodeMissile (mo, NULL, NULL);
	}
}
