/*
#include "actor.h"
#include "gi.h"
#include "m_random.h"
#include "s_sound.h"
#include "d_player.h"
#include "a_action.h"
#include "p_local.h"
#include "a_action.h"
#include "p_pspr.h"
#include "gstrings.h"
#include "a_hexenglobal.h"
#include "thingdef/thingdef.h"
#include "g_level.h"
*/

#define ZAGSPEED	FRACUNIT

static FRandom pr_lightningready ("LightningReady");
static FRandom pr_lightningclip ("LightningClip");
static FRandom pr_zap ("LightningZap");
static FRandom pr_zapf ("LightningZapF");
static FRandom pr_hit ("LightningHit");

DECLARE_ACTION(A_LightningClip)
DECLARE_ACTION(A_LightningZap)

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
			thing->velx += velx>>4;
			thing->vely += vely>>4;
		}
		if ((!thing->player && !(thing->flags2&MF2_BOSS))
			|| !(level.time&1))
		{
			P_DamageMobj(thing, this, target, 3, NAME_Electric);
			if (!(S_IsActorPlayingSomething (this, CHAN_WEAPON, -1)))
			{
				S_Sound (this, CHAN_WEAPON, this->AttackSound, 1, ATTN_NORM);
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

DEFINE_ACTION_FUNCTION(AActor, A_LightningReady)
{
	DoReadyWeapon(self);
	if (pr_lightningready() < 160)
	{
		S_Sound (self, CHAN_WEAPON, "MageLightningReady", 1, ATTN_NORM);
	}
}

//============================================================================
//
// A_LightningClip
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_LightningClip)
{
	AActor *cMo;
	AActor *target = NULL;
	int zigZag;

	if (self->flags3 & MF3_FLOORHUGGER)
	{
		if (self->lastenemy == NULL)
		{
			return;
		}
		self->SetZ(self->floorz);
		target = self->lastenemy->tracer;
	}
	else if (self->flags3 & MF3_CEILINGHUGGER)
	{
		self->SetZ(self->ceilingz-self->height);
		target = self->tracer;
	}
	if (self->flags3 & MF3_FLOORHUGGER)
	{ // floor lightning zig-zags, and forces the ceiling lightning to mimic
		cMo = self->lastenemy;
		zigZag = pr_lightningclip();
		if((zigZag > 128 && self->special1 < 2) || self->special1 < -2)
		{
			P_ThrustMobj(self, self->angle+ANG90, ZAGSPEED);
			if(cMo)
			{
				P_ThrustMobj(cMo, self->angle+ANG90, ZAGSPEED);
			}
			self->special1++;
		}
		else
		{
			P_ThrustMobj(self, self->angle-ANG90, ZAGSPEED);
			if(cMo)
			{
				P_ThrustMobj(cMo, cMo->angle-ANG90, ZAGSPEED);
			}
			self->special1--;
		}
	}
	if(target)
	{
		if(target->health <= 0)
		{
			P_ExplodeMissile(self, NULL, NULL);
		}
		else
		{
			self->angle = self->AngleTo(target);
			self->velx = 0;
			self->vely = 0;
			P_ThrustMobj (self, self->angle, self->Speed>>1);
		}
	}
}


//============================================================================
//
// A_LightningZap
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_LightningZap)
{

	const PClass *lightning=PClass::FindClass((ENamedName) self->GetClass()->Meta.GetMetaInt (ACMETA_MissileName, NAME_LightningZap));
	AActor *mo;
	fixed_t deltaZ;

	CALL_ACTION(A_LightningClip, self);

	self->health -= 8;
	if (self->health <= 0)
	{
		self->SetState (self->FindState(NAME_Death));
		return;
	}
	if (self->flags3 & MF3_FLOORHUGGER)
	{
		deltaZ = 10*FRACUNIT;
	}
	else
	{
		deltaZ = -10*FRACUNIT;
	}			
	fixed_t xo = ((pr_zap() - 128)*self->radius / 256);
	fixed_t yo = ((pr_zap() - 128)*self->radius / 256);

	mo = Spawn(lightning, self->Vec3Offset(xo, yo, deltaZ), ALLOW_REPLACE);
	if (mo)
	{
		mo->lastenemy = self;
		mo->velx = self->velx;
		mo->vely = self->vely;
		mo->target = self->target;
		if (self->flags3 & MF3_FLOORHUGGER)
		{
			mo->velz = 20*FRACUNIT;
		}
		else 
		{
			mo->velz = -20*FRACUNIT;
		}
	}
	if ((self->flags3 & MF3_FLOORHUGGER) && pr_zapf() < 160)
	{
		S_Sound (self, CHAN_BODY, self->ActiveSound, 1, ATTN_NORM);
	}
}

//============================================================================
//
// A_MLightningAttack
//
//============================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_MLightningAttack)
{
    ACTION_PARAM_START(2);
    ACTION_PARAM_CLASS(floor, 0);
    ACTION_PARAM_CLASS(ceiling, 1);

	AActor *fmo, *cmo;

	fmo = P_SpawnPlayerMissile (self, floor);
	cmo = P_SpawnPlayerMissile (self, ceiling);
	if (fmo)
	{
		fmo->special1 = 0;
		fmo->lastenemy = cmo;
		CALL_ACTION(A_LightningZap, fmo);	
	}
	if (cmo)
	{
		cmo->tracer = NULL;
		cmo->lastenemy = fmo;
		CALL_ACTION(A_LightningZap, cmo);	
	}
	S_Sound (self, CHAN_BODY, "MageLightningFire", 1, ATTN_NORM);

	if (self->player != NULL)
	{
		AWeapon *weapon = self->player->ReadyWeapon;
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

DEFINE_ACTION_FUNCTION(AActor, A_ZapMimic)
{
	AActor *mo;

	mo = self->lastenemy;
	if (mo)
	{
		if (mo->state >= mo->FindState(NAME_Death))
		{
			P_ExplodeMissile (self, NULL, NULL);
		}
		else
		{
			self->velx = mo->velx;
			self->vely = mo->vely;
		}
	}
}

//============================================================================
//
// A_LastZap
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_LastZap)
{
	const PClass *lightning=PClass::FindClass((ENamedName) self->GetClass()->Meta.GetMetaInt (ACMETA_MissileName, NAME_LightningZap));

	AActor *mo;

	mo = Spawn(lightning, self->Pos(), ALLOW_REPLACE);
	if (mo)
	{
		mo->SetState (mo->FindState (NAME_Death));
		mo->velz = 40*FRACUNIT;
		mo->Damage = 0;
	}
}

//============================================================================
//
// A_LightningRemove
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_LightningRemove)
{
	AActor *mo;

	mo = self->lastenemy;
	if (mo)
	{
		mo->lastenemy = NULL;
		P_ExplodeMissile (mo, NULL, NULL);
	}
}
