#include "actor.h"
#include "info.h"
#include "p_local.h"
#include "m_random.h"
#include "s_sound.h"
#include "p_enemy.h"
#include "a_hexenglobal.h"

static FRandom pr_mstafftrack ("MStaffTrack");

void A_MStaffTrack (AActor *);

// Mage Staff FX2 (Bloodscourge) --------------------------------------------

class AMageStaffFX2 : public AActor
{
	DECLARE_ACTOR (AMageStaffFX2, AActor)
public:
	void GetExplodeParms (int &damage, int &dist, bool &hurtSource);
	int SpecialMissileHit (AActor *victim);
	bool IsOkayToAttack (AActor *link);
	bool SpecialBlastHandling (AActor *source, fixed_t strength);
};

FState AMageStaffFX2::States[] =
{
#define S_MSTAFF_FX2_1 0
	S_BRIGHT (MSP2, 'A',	2, A_MStaffTrack		    , &States[S_MSTAFF_FX2_1+1]),
	S_BRIGHT (MSP2, 'B',	2, A_MStaffTrack		    , &States[S_MSTAFF_FX2_1+2]),
	S_BRIGHT (MSP2, 'C',	2, A_MStaffTrack		    , &States[S_MSTAFF_FX2_1+3]),
	S_BRIGHT (MSP2, 'D',	2, A_MStaffTrack		    , &States[S_MSTAFF_FX2_1]),

#define S_MSTAFF_FX2_X1 (S_MSTAFF_FX2_1+4)
	S_BRIGHT (MSP2, 'E',	4, NULL					    , &States[S_MSTAFF_FX2_X1+1]),
	S_BRIGHT (MSP2, 'F',	5, A_Explode			    , &States[S_MSTAFF_FX2_X1+2]),
	S_BRIGHT (MSP2, 'G',	5, NULL					    , &States[S_MSTAFF_FX2_X1+3]),
	S_BRIGHT (MSP2, 'H',	5, NULL					    , &States[S_MSTAFF_FX2_X1+4]),
	S_BRIGHT (MSP2, 'I',	4, NULL					    , NULL),
};

IMPLEMENT_ACTOR (AMageStaffFX2, Hexen, -1, 0)
	PROP_SpeedFixed (17)
	PROP_HeightFixed (8)
	PROP_Damage (4)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_FIREDAMAGE|MF2_IMPACT|MF2_PCROSS|MF2_SEEKERMISSILE)

	PROP_SpawnState (S_MSTAFF_FX2_1)
	PROP_DeathState (S_MSTAFF_FX2_X1)

	PROP_DeathSound ("MageStaffExplode")
END_DEFAULTS

void AMageStaffFX2::GetExplodeParms (int &damage, int &dist, bool &hurtSource)
{
	damage = 80;
	dist = 192;
	hurtSource = false;
}

int AMageStaffFX2::SpecialMissileHit (AActor *victim)
{
	if (victim != target &&
		!victim->player &&
		!(victim->flags2 & MF2_BOSS))
	{
		P_DamageMobj (victim, this, target, 10);
	}
	return -1;
}

bool AMageStaffFX2::IsOkayToAttack (AActor *link)
{
	if ((link->flags&MF_COUNTKILL ||
		(link->player))
		&& !(link->flags2&MF2_DORMANT))
	{
		if (!(link->flags&MF_SHOOTABLE))
		{
			return false;
		}
		if (multiplayer && !deathmatch && link->player && target->player)
		{
			return false;
		}
		if (link == target)
		{
			return false;
		}
		else if (P_CheckSight (this, link))
		{
			AActor *master = target;
			angle = R_PointToAngle2 (master->x, master->y,
							link->x, link->y) - master->angle;
			angle >>= 24;
			if (angle>226 || angle<30)
			{
				return true;
			}
		}
	}
	return false;
}

bool AMageStaffFX2::SpecialBlastHandling (AActor *source, fixed_t strength)
{
	// Reflect to originator
	tracer = target;	
	target = source;
	return true;
}

//============================================================================

//============================================================================
//
// MStaffSpawn2 - for use by mage class boss
//
//============================================================================

void MStaffSpawn2 (AActor *actor, angle_t angle)
{
	AActor *mo;

	mo = P_SpawnMissileAngleZ (actor, actor->z+40*FRACUNIT,
		RUNTIME_CLASS(AMageStaffFX2), angle, 0);
	if (mo)
	{
		mo->target = actor;
		mo->tracer = P_RoughMonsterSearch (mo, 10);
	}
}

//============================================================================
//
// A_MStaffAttack2 - for use by mage class boss
//
//============================================================================

void A_MStaffAttack2 (AActor *actor)
{
	angle_t angle;
	angle = actor->angle;
	MStaffSpawn2 (actor, angle);
	MStaffSpawn2 (actor, angle-ANGLE_1*5);
	MStaffSpawn2 (actor, angle+ANGLE_1*5);
	S_Sound (actor, CHAN_WEAPON, "MageStaffFire", 1, ATTN_NORM);
}

//============================================================================
//
// MStaffSpawn
//
//============================================================================
#if 0
void MStaffSpawn (AActor *pmo, angle_t angle)
{
	AActor *mo;

	// spawn at z+40, not z+32
	mo = P_SPMAngle (pmo, MT_MSTAFF_FX2, angle);
	if (mo)
	{
		mo->target = pmo;
		mo->special1 = (int)P_RoughMonsterSearch(mo, 10);
	}
}
#endif

//============================================================================
//
// A_MStaffTrack
//
//============================================================================

void A_MStaffTrack (AActor *actor)
{
	if ((actor->tracer == 0) && (pr_mstafftrack()<50))
	{
		actor->tracer = P_RoughMonsterSearch (actor, 10);
	}
	P_SeekerMissile (actor, ANGLE_1*2, ANGLE_1*10);
}
