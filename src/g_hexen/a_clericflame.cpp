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

const fixed_t FLAMESPEED	= fixed_t(0.45*FRACUNIT);
const fixed_t CFLAMERANGE	= 12*64*FRACUNIT;
const fixed_t FLAMEROTSPEED	= 2*FRACUNIT;

static FRandom pr_missile ("CFlameMissile");

void A_CFlameAttack (AActor *);
void A_CFlameRotate (AActor *);
void A_CFlamePuff (AActor *);
void A_CFlameMissile (AActor *);

// The Cleric's Flame Strike ------------------------------------------------

class ACWeapFlame : public AClericWeapon
{
	DECLARE_ACTOR (ACWeapFlame, AClericWeapon)
};

FState ACWeapFlame::States[] =
{
#define S_CFLAME1 0
	S_BRIGHT (WCFM, 'A',	4, NULL					    , &States[S_CFLAME1+1]),
	S_BRIGHT (WCFM, 'B',	4, NULL					    , &States[S_CFLAME1+2]),
	S_BRIGHT (WCFM, 'C',	4, NULL					    , &States[S_CFLAME1+3]),
	S_BRIGHT (WCFM, 'D',	4, NULL					    , &States[S_CFLAME1+4]),
	S_BRIGHT (WCFM, 'E',	4, NULL					    , &States[S_CFLAME1+5]),
	S_BRIGHT (WCFM, 'F',	4, NULL					    , &States[S_CFLAME1+6]),
	S_BRIGHT (WCFM, 'G',	4, NULL					    , &States[S_CFLAME1+7]),
	S_BRIGHT (WCFM, 'H',	4, NULL					    , &States[S_CFLAME1]),

#define S_CFLAMEREADY (S_CFLAME1+8)
	S_NORMAL (CFLM, 'A',	1, A_WeaponReady		    , &States[S_CFLAMEREADY+1]),
	S_NORMAL (CFLM, 'A',	1, A_WeaponReady		    , &States[S_CFLAMEREADY+2]),
	S_NORMAL (CFLM, 'A',	1, A_WeaponReady		    , &States[S_CFLAMEREADY+3]),
	S_NORMAL (CFLM, 'A',	1, A_WeaponReady		    , &States[S_CFLAMEREADY+4]),
	S_NORMAL (CFLM, 'B',	1, A_WeaponReady		    , &States[S_CFLAMEREADY+5]),
	S_NORMAL (CFLM, 'B',	1, A_WeaponReady		    , &States[S_CFLAMEREADY+6]),
	S_NORMAL (CFLM, 'B',	1, A_WeaponReady		    , &States[S_CFLAMEREADY+7]),
	S_NORMAL (CFLM, 'B',	1, A_WeaponReady		    , &States[S_CFLAMEREADY+8]),
	S_NORMAL (CFLM, 'C',	1, A_WeaponReady		    , &States[S_CFLAMEREADY+9]),
	S_NORMAL (CFLM, 'C',	1, A_WeaponReady		    , &States[S_CFLAMEREADY+10]),
	S_NORMAL (CFLM, 'C',	1, A_WeaponReady		    , &States[S_CFLAMEREADY+11]),
	S_NORMAL (CFLM, 'C',	1, A_WeaponReady		    , &States[S_CFLAMEREADY]),

#define S_CFLAMEDOWN (S_CFLAMEREADY+12)
	S_NORMAL (CFLM, 'A',	1, A_Lower				    , &States[S_CFLAMEDOWN]),

#define S_CFLAMEUP (S_CFLAMEDOWN+1)
	S_NORMAL (CFLM, 'A',	1, A_Raise				    , &States[S_CFLAMEUP]),

#define S_CFLAMEATK (S_CFLAMEUP+1)
	S_NORMAL2 (CFLM, 'A',	2, NULL					    , &States[S_CFLAMEATK+1], 0, 40),
	S_NORMAL2 (CFLM, 'D',	2, NULL					    , &States[S_CFLAMEATK+2], 0, 50),
	S_NORMAL2 (CFLM, 'D',	2, NULL					    , &States[S_CFLAMEATK+3], 0, 36),
	S_BRIGHT (CFLM, 'E',	4, NULL					    , &States[S_CFLAMEATK+4]),
	S_BRIGHT (CFLM, 'F',	4, A_CFlameAttack		    , &States[S_CFLAMEATK+5]),
	S_BRIGHT (CFLM, 'E',	4, NULL					    , &States[S_CFLAMEATK+6]),
	S_NORMAL2 (CFLM, 'G',	2, NULL					    , &States[S_CFLAMEATK+7], 0, 40),
	S_NORMAL (CFLM, 'G',	2, NULL					    , &States[S_CFLAMEREADY]),
};

IMPLEMENT_ACTOR (ACWeapFlame, Hexen, 8009, 0)
	PROP_Flags (MF_SPECIAL|MF_NOGRAVITY)
	PROP_SpawnState (S_CFLAME1)

	PROP_Weapon_SelectionOrder (1000)
	PROP_Weapon_AmmoUse1 (4)
	PROP_Weapon_AmmoGive1 (25)
	PROP_Weapon_UpState (S_CFLAMEUP)
	PROP_Weapon_DownState (S_CFLAMEDOWN)
	PROP_Weapon_ReadyState (S_CFLAMEREADY)
	PROP_Weapon_AtkState (S_CFLAMEATK)
	PROP_Weapon_Kickback (150)
	PROP_Weapon_YAdjust (10)
	PROP_Weapon_MoveCombatDist (27000000)
	PROP_Weapon_AmmoType1 ("Mana2")
	PROP_Weapon_ProjectileType ("CFlameMissile")
	PROP_Inventory_PickupMessage("$TXT_WEAPON_C3")
END_DEFAULTS

// Floor Flame --------------------------------------------------------------

class ACFlameFloor : public AActor
{
	DECLARE_ACTOR (ACFlameFloor, AActor)
};

FState ACFlameFloor::States[] =
{
	S_BRIGHT (CFFX, 'N',	5, NULL					    , &States[1]),
	S_BRIGHT (CFFX, 'O',	4, NULL					    , &States[2]),
	S_BRIGHT (CFFX, 'P',	3, NULL					    , NULL),
};

IMPLEMENT_ACTOR (ACFlameFloor, Hexen, -1, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_RenderStyle (STYLE_Add)
	PROP_SpawnState (0)
END_DEFAULTS

// Flame Puff ---------------------------------------------------------------

class AFlamePuff : public AActor
{
	DECLARE_ACTOR (AFlamePuff, AActor)
};

FState AFlamePuff::States[] =
{
	S_BRIGHT (CFFX, 'A',	3, NULL					    , &States[1]),
	S_BRIGHT (CFFX, 'B',	3, NULL					    , &States[2]),
	S_BRIGHT (CFFX, 'C',	3, NULL					    , &States[3]),
	S_BRIGHT (CFFX, 'D',	4, NULL					    , &States[4]),
	S_BRIGHT (CFFX, 'E',	3, NULL					    , &States[5]),
	S_BRIGHT (CFFX, 'F',	4, NULL					    , &States[6]),
	S_BRIGHT (CFFX, 'G',	3, NULL					    , &States[7]),
	S_BRIGHT (CFFX, 'H',	4, NULL					    , &States[8]),
	S_BRIGHT (CFFX, 'I',	3, NULL					    , &States[9]),
	S_BRIGHT (CFFX, 'J',	4, NULL					    , &States[10]),
	S_BRIGHT (CFFX, 'K',	3, NULL					    , &States[11]),
	S_BRIGHT (CFFX, 'L',	4, NULL					    , &States[12]),
	S_BRIGHT (CFFX, 'M',	3, NULL					    , NULL),
};

IMPLEMENT_ACTOR (AFlamePuff, Hexen, -1, 0)
	PROP_RadiusFixed (1)
	PROP_HeightFixed (1)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (0)

	PROP_SeeSound ("ClericFlameExplode")
	PROP_AttackSound ("ClericFlameExplode")
END_DEFAULTS

// Flame Puff 2 -------------------------------------------------------------

class AFlamePuff2 : public AActor
{
	DECLARE_ACTOR (AFlamePuff2, AActor)
};

FState AFlamePuff2::States[] =
{
	S_BRIGHT (CFFX, 'A',	3, NULL					    , &States[1]),
	S_BRIGHT (CFFX, 'B',	3, NULL					    , &States[2]),
	S_BRIGHT (CFFX, 'C',	3, NULL					    , &States[3]),
	S_BRIGHT (CFFX, 'D',	4, NULL					    , &States[4]),
	S_BRIGHT (CFFX, 'E',	3, NULL					    , &States[5]),
	S_BRIGHT (CFFX, 'F',	4, NULL					    , &States[6]),
	S_BRIGHT (CFFX, 'G',	3, NULL					    , &States[7]),
	S_BRIGHT (CFFX, 'H',	4, NULL					    , &States[8]),
	S_BRIGHT (CFFX, 'I',	3, NULL					    , &States[9]),
	S_BRIGHT (CFFX, 'C',	3, NULL					    , &States[10]),
	S_BRIGHT (CFFX, 'D',	4, NULL					    , &States[11]),
	S_BRIGHT (CFFX, 'E',	3, NULL					    , &States[12]),
	S_BRIGHT (CFFX, 'F',	4, NULL					    , &States[13]),
	S_BRIGHT (CFFX, 'G',	3, NULL					    , &States[14]),
	S_BRIGHT (CFFX, 'H',	4, NULL					    , &States[15]),
	S_BRIGHT (CFFX, 'I',	3, NULL					    , &States[16]),
	S_BRIGHT (CFFX, 'J',	4, NULL					    , &States[17]),
	S_BRIGHT (CFFX, 'K',	3, NULL					    , &States[18]),
	S_BRIGHT (CFFX, 'L',	4, NULL					    , &States[19]),
	S_BRIGHT (CFFX, 'M',	3, NULL					    , NULL),
};

IMPLEMENT_ACTOR (AFlamePuff2, Hexen, -1, 0)
	PROP_RadiusFixed (1)
	PROP_HeightFixed (1)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (0)

	PROP_SeeSound ("ClericFlameExplode")
	PROP_AttackSound ("ClericFlameExplode")
END_DEFAULTS

// Circle Flame -------------------------------------------------------------

class ACircleFlame : public AActor
{
	DECLARE_ACTOR (ACircleFlame, AActor)
public:
	void GetExplodeParms (int &damage, int &dist, bool &hurtSource);
};

FState ACircleFlame::States[] =
{
#define S_CIRCLE_FLAME1 0
	S_BRIGHT (CFCF, 'A',	4, NULL					    , &States[S_CIRCLE_FLAME1+1]),
	S_BRIGHT (CFCF, 'B',	2, A_CFlameRotate		    , &States[S_CIRCLE_FLAME1+2]),
	S_BRIGHT (CFCF, 'C',	2, NULL					    , &States[S_CIRCLE_FLAME1+3]),
	S_BRIGHT (CFCF, 'D',	1, NULL					    , &States[S_CIRCLE_FLAME1+4]),
	S_BRIGHT (CFCF, 'E',	2, NULL					    , &States[S_CIRCLE_FLAME1+5]),
	S_BRIGHT (CFCF, 'F',	2, A_CFlameRotate		    , &States[S_CIRCLE_FLAME1+6]),
	S_BRIGHT (CFCF, 'G',	1, NULL					    , &States[S_CIRCLE_FLAME1+7]),
	S_BRIGHT (CFCF, 'H',	2, NULL					    , &States[S_CIRCLE_FLAME1+8]),
	S_BRIGHT (CFCF, 'I',	2, NULL					    , &States[S_CIRCLE_FLAME1+9]),
	S_BRIGHT (CFCF, 'J',	1, A_CFlameRotate		    , &States[S_CIRCLE_FLAME1+10]),
	S_BRIGHT (CFCF, 'K',	2, NULL					    , &States[S_CIRCLE_FLAME1+11]),
	S_BRIGHT (CFCF, 'L',	3, NULL					    , &States[S_CIRCLE_FLAME1+12]),
	S_BRIGHT (CFCF, 'M',	3, NULL					    , &States[S_CIRCLE_FLAME1+13]),
	S_BRIGHT (CFCF, 'N',	2, A_CFlameRotate		    , &States[S_CIRCLE_FLAME1+14]),
	S_BRIGHT (CFCF, 'O',	3, NULL					    , &States[S_CIRCLE_FLAME1+15]),
	S_BRIGHT (CFCF, 'P',	2, NULL					    , NULL),

#define S_CIRCLE_FLAME_X1 (S_CIRCLE_FLAME1+16)
	S_BRIGHT (CFCF, 'Q',	3, NULL					    , &States[S_CIRCLE_FLAME_X1+1]),
	S_BRIGHT (CFCF, 'R',	3, NULL					    , &States[S_CIRCLE_FLAME_X1+2]),
	S_BRIGHT (CFCF, 'S',	3, A_Explode			    , &States[S_CIRCLE_FLAME_X1+3]),
	S_BRIGHT (CFCF, 'T',	3, NULL					    , &States[S_CIRCLE_FLAME_X1+4]),
	S_BRIGHT (CFCF, 'U',	3, NULL					    , &States[S_CIRCLE_FLAME_X1+5]),
	S_BRIGHT (CFCF, 'V',	3, NULL					    , &States[S_CIRCLE_FLAME_X1+6]),
	S_BRIGHT (CFCF, 'W',	3, NULL					    , &States[S_CIRCLE_FLAME_X1+7]),
	S_BRIGHT (CFCF, 'X',	3, NULL					    , &States[S_CIRCLE_FLAME_X1+8]),
	S_BRIGHT (CFCF, 'Y',	3, NULL					    , &States[S_CIRCLE_FLAME_X1+9]),
	S_BRIGHT (CFCF, 'Z',	3, NULL					    , NULL),
};

IMPLEMENT_ACTOR (ACircleFlame, Hexen, -1, 0)
	PROP_RadiusFixed (6)
	PROP_Damage (2)
	PROP_DamageType (NAME_Fire)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT)
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (S_CIRCLE_FLAME1)
	PROP_DeathState (S_CIRCLE_FLAME_X1)

	PROP_DeathSound ("ClericFlameCircle")
END_DEFAULTS

void ACircleFlame::GetExplodeParms (int &damage, int &dist, bool &hurtSource)
{
	damage = 20;
	hurtSource = false;
}

// Flame Missile ------------------------------------------------------------

class ACFlameMissile : public AActor
{
	DECLARE_ACTOR (ACFlameMissile, AActor)
public:
	void BeginPlay ();
	void Tick ();
};

FState ACFlameMissile::States[] =
{
#define S_CFLAME_MISSILE1 0
	S_BRIGHT (CFFX, 'A',	4, NULL					    , &States[S_CFLAME_MISSILE1+1]),
	S_NORMAL (CFFX, 'A',	1, A_CFlamePuff			    , &AFlamePuff::States[0]),

#define S_CFLAME_MISSILE_X (S_CFLAME_MISSILE1+2)
	S_BRIGHT (CFFX, 'A',	1, A_CFlameMissile		    , &AFlamePuff::States[0]),
};

IMPLEMENT_ACTOR (ACFlameMissile, Hexen, -1, 0)
	PROP_SpeedFixed (200)
	PROP_RadiusFixed (14)
	PROP_HeightFixed (8)
	PROP_Damage (8)
	PROP_DamageType (NAME_Fire)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_IMPACT|MF2_PCROSS)
	PROP_RenderFlags (RF_INVISIBLE)
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (S_CFLAME_MISSILE1)
	PROP_DeathState (S_CFLAME_MISSILE_X)
END_DEFAULTS

void ACFlameMissile::BeginPlay ()
{
	special1 = 2;
}

void ACFlameMissile::Tick ()
{
	int i;
	fixed_t xfrac;
	fixed_t yfrac;
	fixed_t zfrac;
	fixed_t newz;
	bool changexy;
	AActor *mo;

	PrevX = x;
	PrevY = y;
	PrevZ = z;

	// Handle movement
	if (momx || momy ||	(z != floorz) || momz)
	{
		xfrac = momx>>3;
		yfrac = momy>>3;
		zfrac = momz>>3;
		changexy = xfrac || yfrac;
		for (i = 0; i < 8; i++)
		{
			if (changexy)
			{
				if (!P_TryMove (this, x+xfrac, y+yfrac, true))
				{ // Blocked move
					P_ExplodeMissile (this, BlockingLine, BlockingMobj);
					return;
				}
			}
			z += zfrac;
			if (z <= floorz)
			{ // Hit the floor
				z = floorz;
				P_HitFloor (this);
				P_ExplodeMissile (this, NULL, NULL);
				return;
			}
			if (z+height > ceilingz)
			{ // Hit the ceiling
				z = ceilingz-height;
				P_ExplodeMissile (this, NULL, NULL);
				return;
			}
			if (changexy)
			{
				if (!--special1)
				{
					special1 = 4;
					newz = z-12*FRACUNIT;
					if (newz < floorz)
					{
						newz = floorz;
					}
					mo = Spawn<ACFlameFloor> (x, y, newz, ALLOW_REPLACE);
					if (mo)
					{
						mo->angle = angle;
					}
				}
			}
		}
	}
	// Advance the state
	if (tics != -1)
	{
		tics--;
		while (!tics)
		{
			if (!SetState (state->GetNextState ()))
			{ // mobj was removed
				return;
			}
		}
	}
}

//============================================================================
//
// A_CFlameAttack
//
//============================================================================

void A_CFlameAttack (AActor *actor)
{
	player_t *player;

	if (NULL == (player = actor->player))
	{
		return;
	}
	AWeapon *weapon = actor->player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
	}
	P_SpawnPlayerMissile (actor, RUNTIME_CLASS(ACFlameMissile));
	S_Sound (actor, CHAN_WEAPON, "ClericFlameFire", 1, ATTN_NORM);
}

//============================================================================
//
// A_CFlamePuff
//
//============================================================================

void A_CFlamePuff (AActor *actor)
{
	A_UnHideThing (actor);
	actor->momx = 0;
	actor->momy = 0;
	actor->momz = 0;
	S_Sound (actor, CHAN_BODY, "ClericFlameExplode", 1, ATTN_NORM);
}

//============================================================================
//
// A_CFlameMissile
//
//============================================================================

void A_CFlameMissile (AActor *actor)
{
	int i;
	int an, an90;
	fixed_t dist;
	AActor *mo;
	
	A_UnHideThing (actor);
	S_Sound (actor, CHAN_BODY, "ClericFlameExplode", 1, ATTN_NORM);
	AActor *BlockingMobj = actor->BlockingMobj;
	if (BlockingMobj && BlockingMobj->flags&MF_SHOOTABLE)
	{ // Hit something, so spawn the flame circle around the thing
		dist = BlockingMobj->radius+18*FRACUNIT;
		for (i = 0; i < 4; i++)
		{
			an = (i*ANG45)>>ANGLETOFINESHIFT;
			an90 = (i*ANG45+ANG90)>>ANGLETOFINESHIFT;
			mo = Spawn<ACircleFlame> (BlockingMobj->x+FixedMul(dist, finecosine[an]),
				BlockingMobj->y+FixedMul(dist, finesine[an]), 
				BlockingMobj->z+5*FRACUNIT, ALLOW_REPLACE);
			if (mo)
			{
				mo->angle = an<<ANGLETOFINESHIFT;
				mo->target = actor->target;
				mo->momx = mo->special1 = FixedMul(FLAMESPEED, finecosine[an]);
				mo->momy = mo->special2 = FixedMul(FLAMESPEED, finesine[an]);
				mo->tics -= pr_missile()&3;
			}
			mo = Spawn<ACircleFlame> (BlockingMobj->x-FixedMul(dist, finecosine[an]),
				BlockingMobj->y-FixedMul(dist, finesine[an]), 
				BlockingMobj->z+5*FRACUNIT, ALLOW_REPLACE);
			if(mo)
			{
				mo->angle = ANG180+(an<<ANGLETOFINESHIFT);
				mo->target = actor->target;
				mo->momx = mo->special1 = FixedMul(-FLAMESPEED, finecosine[an]);
				mo->momy = mo->special2 = FixedMul(-FLAMESPEED, finesine[an]);
				mo->tics -= pr_missile()&3;
			}
		}
		actor->SetState (&AFlamePuff2::States[0]);
	}
}

//============================================================================
//
// A_CFlameRotate
//
//============================================================================

void A_CFlameRotate (AActor *actor)
{
	int an;

	an = (actor->angle+ANG90)>>ANGLETOFINESHIFT;
	actor->momx = actor->special1+FixedMul(FLAMEROTSPEED, finecosine[an]);
	actor->momy = actor->special2+FixedMul(FLAMEROTSPEED, finesine[an]);
	actor->angle += ANG90/15;
}
