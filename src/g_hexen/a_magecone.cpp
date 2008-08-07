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

// Frost Missile ------------------------------------------------------------

class AFrostMissile : public AActor
{
	DECLARE_CLASS (AFrostMissile, AActor)
public:
	int DoSpecialDamage (AActor *victim, int damage);
};

IMPLEMENT_CLASS (AFrostMissile)

int AFrostMissile::DoSpecialDamage (AActor *victim, int damage)
{
	if (special2 > 0)
	{
		damage <<= special2;
	}
	return damage;
}

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
	AActor *linetarget;

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
		slope = P_AimLineAttack (actor, angle, MELEERANGE, &linetarget);
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
