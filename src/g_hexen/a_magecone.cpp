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

const int SHARDSPAWN_LEFT	= 1;
const int SHARDSPAWN_RIGHT	= 2;
const int SHARDSPAWN_UP		= 4;
const int SHARDSPAWN_DOWN	= 8;

static FRandom pr_cone ("FireConePL1");

void A_FireConePL1 (AActor *actor);
void A_ShedShard (AActor *);

// The Mage's Frost Cone ----------------------------------------------------

class AMWeapFrost : public AMageWeapon
{
	DECLARE_ACTOR (AMWeapFrost, AMageWeapon)
};

FState AMWeapFrost::States[] =
{
#define S_COS1 0
	S_BRIGHT (WMCS, 'A',	8, NULL					    , &States[S_COS1+1]),
	S_BRIGHT (WMCS, 'B',	8, NULL					    , &States[S_COS1+2]),
	S_BRIGHT (WMCS, 'C',	8, NULL					    , &States[S_COS1]),

#define S_CONEREADY (S_COS1+3)
	S_NORMAL (CONE, 'A',	1, A_WeaponReady		    , &States[S_CONEREADY]),

#define S_CONEDOWN (S_CONEREADY+1)
	S_NORMAL (CONE, 'A',	1, A_Lower				    , &States[S_CONEDOWN]),

#define S_CONEUP (S_CONEDOWN+1)
	S_NORMAL (CONE, 'A',	1, A_Raise				    , &States[S_CONEUP]),

#define S_CONEATK (S_CONEUP+1)
	S_NORMAL (CONE, 'B',	3, NULL					    , &States[S_CONEATK+1]),
	S_NORMAL (CONE, 'C',	4, NULL					    , &States[S_CONEATK+2]),
	S_NORMAL (CONE, 'D',	3, NULL					    , &States[S_CONEATK+3]),
	S_NORMAL (CONE, 'E',	5, NULL					    , &States[S_CONEATK+4]),
	S_NORMAL (CONE, 'F',	3, A_FireConePL1		    , &States[S_CONEATK+5]),
	S_NORMAL (CONE, 'G',	3, NULL					    , &States[S_CONEATK+6]),
	S_NORMAL (CONE, 'A',	9, NULL					    , &States[S_CONEATK+7]),
	S_NORMAL (CONE, 'A',   10, A_ReFire				    , &States[S_CONEREADY]),
};

IMPLEMENT_ACTOR (AMWeapFrost, Hexen, 53, 36)
	PROP_Flags (MF_SPECIAL)
	PROP_Flags5 (MF5_BLOODSPLATTER)
	PROP_SpawnState (S_COS1)

	PROP_Weapon_SelectionOrder (1700)
	PROP_Weapon_AmmoUse1 (3)
	PROP_Weapon_AmmoGive1 (25)
	PROP_Weapon_UpState (S_CONEUP)
	PROP_Weapon_DownState (S_CONEDOWN)
	PROP_Weapon_ReadyState (S_CONEREADY)
	PROP_Weapon_AtkState (S_CONEATK)
	PROP_Weapon_HoldAtkState (S_CONEATK+2)
	PROP_Weapon_Kickback (150)
	PROP_Weapon_YAdjust (20)
	PROP_Weapon_MoveCombatDist (19000000)
	PROP_Weapon_AmmoType1 ("Mana1")
	PROP_Weapon_ProjectileType ("FrostMissile")
	PROP_Inventory_PickupMessage("$TXT_WEAPON_M2")
END_DEFAULTS

// Frost Missile ------------------------------------------------------------

class AFrostMissile : public AActor
{
	DECLARE_ACTOR (AFrostMissile, AActor)
public:
	int DoSpecialDamage (AActor *victim, int damage);
};

FState AFrostMissile::States[] =
{
	S_BRIGHT (SHRD, 'A',	2, NULL					    , &States[1]),
	S_BRIGHT (SHRD, 'A',	3, A_ShedShard			    , &States[2]),
	S_BRIGHT (SHRD, 'B',	3, NULL					    , &States[3]),
	S_BRIGHT (SHRD, 'C',	3, NULL					    , &States[0]),

#define S_SHARDFXE1_1 (4)
	S_BRIGHT (SHEX, 'A',	5, NULL					    , &States[S_SHARDFXE1_1+1]),
	S_BRIGHT (SHEX, 'B',	5, NULL					    , &States[S_SHARDFXE1_1+2]),
	S_BRIGHT (SHEX, 'C',	5, NULL					    , &States[S_SHARDFXE1_1+3]),
	S_BRIGHT (SHEX, 'D',	5, NULL					    , &States[S_SHARDFXE1_1+4]),
	S_BRIGHT (SHEX, 'E',	5, NULL					    , NULL),
};

IMPLEMENT_ACTOR (AFrostMissile, Hexen, -1, 0)
	PROP_SpeedFixed (25)
	PROP_RadiusFixed (13)
	PROP_HeightFixed (8)
	PROP_Damage (1)
	PROP_DamageType (NAME_Ice)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_IMPACT|MF2_PCROSS)

	PROP_SpawnState (0)
	PROP_DeathState (S_SHARDFXE1_1)

	PROP_DeathSound ("MageShardsExplode")
END_DEFAULTS

int AFrostMissile::DoSpecialDamage (AActor *victim, int damage)
{
	if (special2 > 0)
	{
		damage <<= special2;
	}
	return damage;
}

// Ice Shard ----------------------------------------------------------------

class AIceShard : public AFrostMissile
{
	DECLARE_ACTOR (AIceShard, AFrostMissile)
};

FState AIceShard::States[] =
{
	S_BRIGHT (SHRD, 'A',	3, NULL					    , &States[1]),
	S_BRIGHT (SHRD, 'B',	3, NULL					    , &States[2]),
	S_BRIGHT (SHRD, 'C',	3, NULL					    , &States[0]),
};

IMPLEMENT_ACTOR (AIceShard, Hexen, -1, 65)
	PROP_DamageType (NAME_Ice)
	PROP_Flags2 (MF2_NOTELEPORT)
	PROP_SpawnState (0)
END_DEFAULTS

//============================================================================
//
// A_FireConePL1
//
//============================================================================

void A_FireConePL1 (AActor *actor)
{
	angle_t angle;
	int damage;
	int slope;
	int i;
	AActor *mo;
	bool conedone=false;
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
	S_Sound (actor, CHAN_WEAPON, "MageShardsFire", 1, ATTN_NORM);

	damage = 90+(pr_cone()&15);
	for (i = 0; i < 16; i++)
	{
		angle = actor->angle+i*(ANG45/16);
		slope = P_AimLineAttack (actor, angle, MELEERANGE);
		if (linetarget)
		{
			P_DamageMobj (linetarget, actor, actor, damage, NAME_Ice);
			conedone = true;
			break;
		}
	}

	// didn't find any creatures, so fire projectiles
	if (!conedone)
	{
		mo = P_SpawnPlayerMissile (actor, RUNTIME_CLASS(AFrostMissile));
		if (mo)
		{
			mo->special1 = SHARDSPAWN_LEFT|SHARDSPAWN_DOWN|SHARDSPAWN_UP
				|SHARDSPAWN_RIGHT;
			mo->special2 = 3; // Set sperm count (levels of reproductivity)
			mo->target = actor;
			mo->args[0] = 3;		// Mark Initial shard as super damage
		}
	}
}

//============================================================================
//
// A_ShedShard
//
//============================================================================

void A_ShedShard (AActor *actor)
{
	AActor *mo;
	int spawndir = actor->special1;
	int spermcount = actor->special2;

	if (spermcount <= 0) return;				// No sperm left
	actor->special2 = 0;
	spermcount--;

	// every so many calls, spawn a new missile in its set directions
	if (spawndir & SHARDSPAWN_LEFT)
	{
		mo = P_SpawnMissileAngleZSpeed (actor, actor->z, RUNTIME_CLASS(AFrostMissile), actor->angle+(ANG45/9),
											 0, (20+2*spermcount)<<FRACBITS, actor->target);
		if (mo)
		{
			mo->special1 = SHARDSPAWN_LEFT;
			mo->special2 = spermcount;
			mo->momz = actor->momz;
			mo->args[0] = (spermcount==3)?2:0;
		}
	}
	if (spawndir & SHARDSPAWN_RIGHT)
	{
		mo = P_SpawnMissileAngleZSpeed (actor, actor->z, RUNTIME_CLASS(AFrostMissile), actor->angle-(ANG45/9),
											 0, (20+2*spermcount)<<FRACBITS, actor->target);
		if (mo)
		{
			mo->special1 = SHARDSPAWN_RIGHT;
			mo->special2 = spermcount;
			mo->momz = actor->momz;
			mo->args[0] = (spermcount==3)?2:0;
		}
	}
	if (spawndir & SHARDSPAWN_UP)
	{
		mo = P_SpawnMissileAngleZSpeed (actor, actor->z+8*FRACUNIT, RUNTIME_CLASS(AFrostMissile), actor->angle, 
											 0, (15+2*spermcount)<<FRACBITS, actor->target);
		if (mo)
		{
			mo->momz = actor->momz;
			if (spermcount & 1)			// Every other reproduction
				mo->special1 = SHARDSPAWN_UP | SHARDSPAWN_LEFT | SHARDSPAWN_RIGHT;
			else
				mo->special1 = SHARDSPAWN_UP;
			mo->special2 = spermcount;
			mo->args[0] = (spermcount==3)?2:0;
		}
	}
	if (spawndir & SHARDSPAWN_DOWN)
	{
		mo = P_SpawnMissileAngleZSpeed (actor, actor->z-4*FRACUNIT, RUNTIME_CLASS(AFrostMissile), actor->angle, 
											 0, (15+2*spermcount)<<FRACBITS, actor->target);
		if (mo)
		{
			mo->momz = actor->momz;
			if (spermcount & 1)			// Every other reproduction
				mo->special1 = SHARDSPAWN_DOWN | SHARDSPAWN_LEFT | SHARDSPAWN_RIGHT;
			else
				mo->special1 = SHARDSPAWN_DOWN;
			mo->special2 = spermcount;
			mo->target = actor->target;
			mo->args[0] = (spermcount==3)?2:0;
		}
	}
}
