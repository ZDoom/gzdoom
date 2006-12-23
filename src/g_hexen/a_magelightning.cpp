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
void A_FreeTargMobj (AActor *);

// The Mage's Lightning Arc of Death ----------------------------------------

class AMWeapLightning : public AMageWeapon
{
	DECLARE_ACTOR (AMWeapLightning, AMageWeapon)
};

FState AMWeapLightning::States[] =
{
#define S_MW_LIGHTNING1 0
	S_BRIGHT (WMLG, 'A',	4, NULL					    , &States[S_MW_LIGHTNING1+1]),
	S_BRIGHT (WMLG, 'B',	4, NULL					    , &States[S_MW_LIGHTNING1+2]),
	S_BRIGHT (WMLG, 'C',	4, NULL					    , &States[S_MW_LIGHTNING1+3]),
	S_BRIGHT (WMLG, 'D',	4, NULL					    , &States[S_MW_LIGHTNING1+4]),
	S_BRIGHT (WMLG, 'E',	4, NULL					    , &States[S_MW_LIGHTNING1+5]),
	S_BRIGHT (WMLG, 'F',	4, NULL					    , &States[S_MW_LIGHTNING1+6]),
	S_BRIGHT (WMLG, 'G',	4, NULL					    , &States[S_MW_LIGHTNING1+7]),
	S_BRIGHT (WMLG, 'H',	4, NULL					    , &States[S_MW_LIGHTNING1]),

#define S_MLIGHTNINGREADY (S_MW_LIGHTNING1+8)
	S_BRIGHT (MLNG, 'A',	1, A_WeaponReady		    , &States[S_MLIGHTNINGREADY+1]),
	S_BRIGHT (MLNG, 'A',	1, A_WeaponReady		    , &States[S_MLIGHTNINGREADY+2]),
	S_BRIGHT (MLNG, 'A',	1, A_WeaponReady		    , &States[S_MLIGHTNINGREADY+3]),
	S_BRIGHT (MLNG, 'A',	1, A_WeaponReady		    , &States[S_MLIGHTNINGREADY+4]),
	S_BRIGHT (MLNG, 'A',	1, A_WeaponReady		    , &States[S_MLIGHTNINGREADY+5]),
	S_BRIGHT (MLNG, 'A',	1, A_LightningReady		    , &States[S_MLIGHTNINGREADY+6]),
	S_BRIGHT (MLNG, 'B',	1, A_WeaponReady		    , &States[S_MLIGHTNINGREADY+7]),
	S_BRIGHT (MLNG, 'B',	1, A_WeaponReady		    , &States[S_MLIGHTNINGREADY+8]),
	S_BRIGHT (MLNG, 'B',	1, A_WeaponReady		    , &States[S_MLIGHTNINGREADY+9]),
	S_BRIGHT (MLNG, 'B',	1, A_WeaponReady		    , &States[S_MLIGHTNINGREADY+10]),
	S_BRIGHT (MLNG, 'B',	1, A_WeaponReady		    , &States[S_MLIGHTNINGREADY+11]),
	S_BRIGHT (MLNG, 'B',	1, A_WeaponReady		    , &States[S_MLIGHTNINGREADY+12]),
	S_BRIGHT (MLNG, 'C',	1, A_WeaponReady		    , &States[S_MLIGHTNINGREADY+13]),
	S_BRIGHT (MLNG, 'C',	1, A_WeaponReady		    , &States[S_MLIGHTNINGREADY+14]),
	S_BRIGHT (MLNG, 'C',	1, A_WeaponReady		    , &States[S_MLIGHTNINGREADY+15]),
	S_BRIGHT (MLNG, 'C',	1, A_WeaponReady		    , &States[S_MLIGHTNINGREADY+16]),
	S_BRIGHT (MLNG, 'C',	1, A_WeaponReady		    , &States[S_MLIGHTNINGREADY+17]),
	S_BRIGHT (MLNG, 'C',	1, A_LightningReady		    , &States[S_MLIGHTNINGREADY+18]),
	S_BRIGHT (MLNG, 'B',	1, A_WeaponReady		    , &States[S_MLIGHTNINGREADY+19]),
	S_BRIGHT (MLNG, 'B',	1, A_WeaponReady		    , &States[S_MLIGHTNINGREADY+20]),
	S_BRIGHT (MLNG, 'B',	1, A_WeaponReady		    , &States[S_MLIGHTNINGREADY+21]),
	S_BRIGHT (MLNG, 'B',	1, A_WeaponReady		    , &States[S_MLIGHTNINGREADY+22]),
	S_BRIGHT (MLNG, 'B',	1, A_WeaponReady		    , &States[S_MLIGHTNINGREADY+23]),
	S_BRIGHT (MLNG, 'B',	1, A_WeaponReady		    , &States[S_MLIGHTNINGREADY]),

#define S_MLIGHTNINGDOWN (S_MLIGHTNINGREADY+24)
	S_BRIGHT (MLNG, 'A',	1, A_Lower				    , &States[S_MLIGHTNINGDOWN]),

#define S_MLIGHTNINGUP (S_MLIGHTNINGDOWN+1)
	S_BRIGHT (MLNG, 'A',	1, A_Raise				    , &States[S_MLIGHTNINGUP]),

#define S_MLIGHTNINGATK (S_MLIGHTNINGUP+1)
	S_BRIGHT (MLNG, 'D',	3, NULL					    , &States[S_MLIGHTNINGATK+1]),
	S_BRIGHT (MLNG, 'E',	3, NULL					    , &States[S_MLIGHTNINGATK+2]),
	S_BRIGHT (MLNG, 'F',	4, A_MLightningAttack	    , &States[S_MLIGHTNINGATK+3]),
	S_BRIGHT (MLNG, 'G',	4, NULL					    , &States[S_MLIGHTNINGATK+4]),
	S_BRIGHT (MLNG, 'H',	3, NULL					    , &States[S_MLIGHTNINGATK+5]),
	S_BRIGHT (MLNG, 'I',	3, NULL					    , &States[S_MLIGHTNINGATK+6]),
	S_BRIGHT2 (MLNG, 'I',	6, NULL					    , &States[S_MLIGHTNINGATK+7], 0, 199),
	S_BRIGHT2 (MLNG, 'C',	2, NULL					    , &States[S_MLIGHTNINGATK+8], 0, 55),
	S_BRIGHT2 (MLNG, 'B',	2, NULL					    , &States[S_MLIGHTNINGATK+9], 0, 50),
	S_BRIGHT2 (MLNG, 'B',	2, NULL					    , &States[S_MLIGHTNINGATK+10], 0, 45),
	S_BRIGHT2 (MLNG, 'B',	2, NULL					    , &States[S_MLIGHTNINGREADY], 0, 40),
};

IMPLEMENT_ACTOR (AMWeapLightning, Hexen, 8040, 0)
	PROP_Flags (MF_SPECIAL|MF_NOGRAVITY)
	PROP_SpawnState (S_MW_LIGHTNING1)

	PROP_Weapon_SelectionOrder (1100)
	PROP_Weapon_AmmoUse1 (5)
	PROP_Weapon_AmmoGive1 (25)
	PROP_Weapon_UpState (S_MLIGHTNINGUP)
	PROP_Weapon_DownState (S_MLIGHTNINGDOWN)
	PROP_Weapon_ReadyState (S_MLIGHTNINGREADY)
	PROP_Weapon_AtkState (S_MLIGHTNINGATK)
	PROP_Weapon_Kickback (0)
	PROP_Weapon_YAdjust (20)
	PROP_Weapon_MoveCombatDist (23000000)
	PROP_Weapon_AmmoType1 ("Mana2")
	PROP_Weapon_ProjectileType ("LightningFloor")
	PROP_Inventory_PickupMessage("$TXT_WEAPON_M3")
END_DEFAULTS

// Lightning ----------------------------------------------------------------

IMPLEMENT_ABSTRACT_ACTOR (ALightning)

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
			// There needs to be a better way to do this...
			static const PClass *centaur=NULL;
			if (!centaur) centaur = PClass::FindClass("Centaur");

			if (thing->IsKindOf(centaur))
			{ // Lightning does more damage to centaurs
				P_DamageMobj(thing, this, target, 9, NAME_Electric);
			}
			else
			{
				P_DamageMobj(thing, this, target, 3, NAME_Electric);
			}
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

// Ceiling Lightning --------------------------------------------------------

class ALightningCeiling : public ALightning
{
	DECLARE_ACTOR (ALightningCeiling, ALightning)
};

FState ALightningCeiling::States[] =
{
#define S_LIGHTNING_CEILING1 0
	S_BRIGHT (MLFX, 'A',	2, A_LightningZap		    , &States[S_LIGHTNING_CEILING1+1]),
	S_BRIGHT (MLFX, 'B',	2, A_LightningClip		    , &States[S_LIGHTNING_CEILING1+2]),
	S_BRIGHT (MLFX, 'C',	2, A_LightningClip		    , &States[S_LIGHTNING_CEILING1+3]),
	S_BRIGHT (MLFX, 'D',	2, A_LightningClip		    , &States[S_LIGHTNING_CEILING1]),

#define S_LIGHTNING_C_X1 (S_LIGHTNING_CEILING1+4)
	S_BRIGHT (MLF2, 'A',	2, A_LightningRemove	    , &States[S_LIGHTNING_C_X1+1]),
	S_BRIGHT (MLF2, 'B',	3, NULL					    , &States[S_LIGHTNING_C_X1+2]),
	S_BRIGHT (MLF2, 'C',	3, NULL					    , &States[S_LIGHTNING_C_X1+3]),
	S_BRIGHT (MLF2, 'D',	3, NULL					    , &States[S_LIGHTNING_C_X1+4]),
	S_BRIGHT (MLF2, 'E',	3, NULL					    , &States[S_LIGHTNING_C_X1+5]),
	S_BRIGHT (MLF2, 'K',	3, NULL					    , &States[S_LIGHTNING_C_X1+6]),
	S_BRIGHT (MLF2, 'L',	3, NULL					    , &States[S_LIGHTNING_C_X1+7]),
	S_BRIGHT (MLF2, 'M',	3, NULL					    , &States[S_LIGHTNING_C_X1+8]),
	S_NORMAL (ACLO, 'E',   35, NULL					    , &States[S_LIGHTNING_C_X1+9]),
	S_BRIGHT (MLF2, 'N',	3, NULL					    , &States[S_LIGHTNING_C_X1+10]),
	S_BRIGHT (MLF2, 'O',	3, NULL					    , &States[S_LIGHTNING_C_X1+11]),
	S_BRIGHT (MLF2, 'P',	4, NULL					    , &States[S_LIGHTNING_C_X1+12]),
	S_BRIGHT (MLF2, 'Q',	3, NULL					    , &States[S_LIGHTNING_C_X1+13]),
	S_BRIGHT (MLF2, 'P',	3, NULL					    , &States[S_LIGHTNING_C_X1+14]),
	S_BRIGHT (MLF2, 'Q',	4, NULL					    , &States[S_LIGHTNING_C_X1+15]),
	S_BRIGHT (MLF2, 'P',	3, NULL					    , &States[S_LIGHTNING_C_X1+16]),
	S_BRIGHT (MLF2, 'O',	3, NULL					    , &States[S_LIGHTNING_C_X1+17]),
	S_BRIGHT (MLF2, 'P',	3, NULL					    , &States[S_LIGHTNING_C_X1+18]),
	S_BRIGHT (MLF2, 'P',	1, A_HideThing			    , &States[S_LIGHTNING_C_X1+19]),
	S_NORMAL (ACLO, 'E', 1050, A_FreeTargMobj		    , NULL),
};

IMPLEMENT_ACTOR (ALightningCeiling, Hexen, -1, 0)
	PROP_SpawnHealth (144)
	PROP_SpeedFixed (25)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (40)
	PROP_Damage (8)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_IMPACT|MF2_PCROSS)
	PROP_Flags3 (MF3_CEILINGHUGGER)
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (S_LIGHTNING_CEILING1)
	PROP_DeathState (S_LIGHTNING_C_X1)
END_DEFAULTS

// Floor Lightning ----------------------------------------------------------

class ALightningFloor : public ALightning
{
	DECLARE_ACTOR (ALightningFloor, ALightning)
};

FState ALightningFloor::States[] =
{
#define S_LIGHTNING_FLOOR1 0
	S_BRIGHT (MLFX, 'E',	2, A_LightningZap		    , &States[S_LIGHTNING_FLOOR1+1]),
	S_BRIGHT (MLFX, 'F',	2, A_LightningClip		    , &States[S_LIGHTNING_FLOOR1+2]),
	S_BRIGHT (MLFX, 'G',	2, A_LightningClip		    , &States[S_LIGHTNING_FLOOR1+3]),
	S_BRIGHT (MLFX, 'H',	2, A_LightningClip		    , &States[S_LIGHTNING_FLOOR1]),

#define S_LIGHTNING_F_X1 (S_LIGHTNING_FLOOR1+4)
	S_BRIGHT (MLF2, 'F',	2, A_LightningRemove	    , &States[S_LIGHTNING_F_X1+1]),
	S_BRIGHT (MLF2, 'G',	3, NULL					    , &States[S_LIGHTNING_F_X1+2]),
	S_BRIGHT (MLF2, 'H',	3, NULL					    , &States[S_LIGHTNING_F_X1+3]),
	S_BRIGHT (MLF2, 'I',	3, NULL					    , &States[S_LIGHTNING_F_X1+4]),
	S_BRIGHT (MLF2, 'J',	3, NULL					    , &States[S_LIGHTNING_F_X1+5]),
	S_BRIGHT (MLF2, 'K',	3, NULL					    , &States[S_LIGHTNING_F_X1+6]),
	S_BRIGHT (MLF2, 'L',	3, NULL					    , &States[S_LIGHTNING_F_X1+7]),
	S_BRIGHT (MLF2, 'M',	3, NULL					    , &States[S_LIGHTNING_F_X1+8]),
	S_NORMAL (ACLO, 'E',   20, NULL					    , &States[S_LIGHTNING_F_X1+9]),
	S_BRIGHT (MLF2, 'N',	3, NULL					    , &States[S_LIGHTNING_F_X1+10]),
	S_BRIGHT (MLF2, 'O',	3, NULL					    , &States[S_LIGHTNING_F_X1+11]),
	S_BRIGHT (MLF2, 'P',	4, NULL					    , &States[S_LIGHTNING_F_X1+12]),
	S_BRIGHT (MLF2, 'Q',	3, NULL					    , &States[S_LIGHTNING_F_X1+13]),
	S_BRIGHT (MLF2, 'P',	3, NULL					    , &States[S_LIGHTNING_F_X1+14]),
	S_BRIGHT (MLF2, 'Q',	4, A_LastZap			    , &States[S_LIGHTNING_F_X1+15]),
	S_BRIGHT (MLF2, 'P',	3, NULL					    , &States[S_LIGHTNING_F_X1+16]),
	S_BRIGHT (MLF2, 'O',	3, NULL					    , &States[S_LIGHTNING_F_X1+17]),
	S_BRIGHT (MLF2, 'P',	3, NULL					    , &States[S_LIGHTNING_F_X1+18]),
	S_BRIGHT (MLF2, 'P',	1, A_HideThing			    , &ALightningCeiling::States[S_LIGHTNING_C_X1+19]),
};

IMPLEMENT_ACTOR (ALightningFloor, Hexen, -1, 0)
	PROP_SpawnHealth (144)
	PROP_SpeedFixed (25)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (40)
	PROP_Damage (8)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_IMPACT|MF2_PCROSS)
	PROP_Flags3 (MF3_FLOORHUGGER)
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (S_LIGHTNING_FLOOR1)
	PROP_DeathState (S_LIGHTNING_F_X1)
END_DEFAULTS

// Lightning Zap ------------------------------------------------------------

class ALightningZap : public AActor
{
	DECLARE_ACTOR (ALightningZap, AActor)
public:
	int SpecialMissileHit (AActor *thing);
};

FState ALightningZap::States[] =
{
#define S_LIGHTNING_ZAP1 0
	S_BRIGHT (MLFX, 'I',	2, A_ZapMimic			    , &States[S_LIGHTNING_ZAP1+1]),
	S_BRIGHT (MLFX, 'J',	2, A_ZapMimic			    , &States[S_LIGHTNING_ZAP1+2]),
	S_BRIGHT (MLFX, 'K',	2, A_ZapMimic			    , &States[S_LIGHTNING_ZAP1+3]),
	S_BRIGHT (MLFX, 'L',	2, A_ZapMimic			    , &States[S_LIGHTNING_ZAP1+4]),
	S_BRIGHT (MLFX, 'M',	2, A_ZapMimic			    , &States[S_LIGHTNING_ZAP1]),

#define S_LIGHTNING_ZAP_X1 (S_LIGHTNING_ZAP1+5)
	S_BRIGHT (MLFX, 'N',	2, NULL					    , &States[S_LIGHTNING_ZAP_X1+1]),
	S_BRIGHT (MLFX, 'O',	2, NULL					    , &States[S_LIGHTNING_ZAP_X1+2]),
	S_BRIGHT (MLFX, 'P',	2, NULL					    , &States[S_LIGHTNING_ZAP_X1+3]),
	S_BRIGHT (MLFX, 'Q',	2, NULL					    , &States[S_LIGHTNING_ZAP_X1+4]),
	S_BRIGHT (MLFX, 'R',	2, NULL					    , &States[S_LIGHTNING_ZAP_X1+5]),
	S_BRIGHT (MLFX, 'S',	2, NULL					    , &States[S_LIGHTNING_ZAP_X1+6]),
	S_BRIGHT (MLFX, 'T',	2, NULL					    , &States[S_LIGHTNING_ZAP_X1+7]),
	S_BRIGHT (MLFX, 'U',	2, NULL					    , NULL),
};

IMPLEMENT_ACTOR (ALightningZap, Hexen, -1, 0)
	PROP_RadiusFixed (15)
	PROP_HeightFixed (35)
	PROP_Damage (2)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE)
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (S_LIGHTNING_ZAP1)
	PROP_DeathState (S_LIGHTNING_ZAP_X1+7)
END_DEFAULTS

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

	fmo = P_SpawnPlayerMissile (actor, RUNTIME_CLASS(ALightningFloor));
	cmo = P_SpawnPlayerMissile (actor, RUNTIME_CLASS(ALightningCeiling));
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
		mo->SetState (&ALightningZap::States[S_LIGHTNING_ZAP_X1]);
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
