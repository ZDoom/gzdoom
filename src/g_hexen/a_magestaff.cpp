#include "actor.h"
#include "info.h"
#include "p_local.h"
#include "m_random.h"
#include "s_sound.h"
#include "p_enemy.h"
#include "a_hexenglobal.h"
#include "gstrings.h"

static FRandom pr_mstafftrack ("MStaffTrack");
static FRandom pr_bloodscourgedrop ("BloodScourgeDrop");

void A_MStaffTrack (AActor *);
void A_DropBloodscourgePieces (AActor *);
void A_MStaffAttack (AActor *actor);
void A_MStaffPalette (AActor *actor);

static AActor *FrontBlockCheck (AActor *mo, int index);
static divline_t BlockCheckLine;

//==========================================================================

class AMageWeaponPiece : public AFourthWeaponPiece
{
	DECLARE_CLASS (AMageWeaponPiece, AFourthWeaponPiece)
public:
	void BeginPlay ();
protected:
	bool MatchPlayerClass (AActor *toucher);
};

IMPLEMENT_CLASS (AMageWeaponPiece)

bool AMageWeaponPiece::MatchPlayerClass (AActor *toucher)
{
	return !toucher->IsKindOf (PClass::FindClass(NAME_FighterPlayer)) &&
		   !toucher->IsKindOf (PClass::FindClass(NAME_ClericPlayer));
}

//==========================================================================

class AMWeaponPiece1 : public AMageWeaponPiece
{
	DECLARE_CLASS (AMWeaponPiece1, AMageWeaponPiece)
public:
	void BeginPlay ();
};

IMPLEMENT_CLASS (AMWeaponPiece1)

void AMWeaponPiece1::BeginPlay ()
{
	Super::BeginPlay ();
	PieceValue = WPIECE1<<6;
}

//==========================================================================

class AMWeaponPiece2 : public AMageWeaponPiece
{
	DECLARE_CLASS (AMWeaponPiece2, AMageWeaponPiece)
public:
	void BeginPlay ();
};

IMPLEMENT_CLASS (AMWeaponPiece2)

void AMWeaponPiece2::BeginPlay ()
{
	Super::BeginPlay ();
	PieceValue = WPIECE2<<6;
}

//==========================================================================

class AMWeaponPiece3 : public AMageWeaponPiece
{
	DECLARE_CLASS (AMWeaponPiece3, AMageWeaponPiece)
public:
	void BeginPlay ();
};

IMPLEMENT_CLASS (AMWeaponPiece3)

void AMWeaponPiece3::BeginPlay ()
{
	Super::BeginPlay ();
	PieceValue = WPIECE3<<6;
}

// The Mages's Staff (Bloodscourge) -----------------------------------------

class AMWeapBloodscourge : public AMageWeapon
{
	DECLARE_CLASS (AMWeapBloodscourge, AMageWeapon)
public:
	void Serialize (FArchive &arc)
	{
		Super::Serialize (arc);
		arc << MStaffCount;
	}
	PalEntry GetBlend ()
	{
		return PalEntry (MStaffCount * 128 / 3, 151, 110, 0);
	}
	BYTE MStaffCount;
};

IMPLEMENT_CLASS (AMWeapBloodscourge)

// Mage Staff FX2 (Bloodscourge) --------------------------------------------

void A_BeAdditive (AActor *self)
{
	self->RenderStyle = STYLE_Add;
}

class AMageStaffFX2 : public AActor
{
	DECLARE_CLASS(AMageStaffFX2, AActor)
public:
	void GetExplodeParms (int &damage, int &dist, bool &hurtSource);
	int SpecialMissileHit (AActor *victim);
	bool IsOkayToAttack (AActor *link);
	bool SpecialBlastHandling (AActor *source, fixed_t strength);
};

IMPLEMENT_CLASS (AMageStaffFX2)

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
		P_DamageMobj (victim, this, target, 10, NAME_Fire);
		return 1;	// Keep going
	}
	return -1;
}

bool AMageStaffFX2::IsOkayToAttack (AActor *link)
{
	if (((link->flags3&MF3_ISMONSTER) || link->player)
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
			angle_t angle = R_PointToAngle2 (master->x, master->y,
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
// MStaffSpawn
//
//============================================================================

void MStaffSpawn (AActor *pmo, angle_t angle)
{
	AActor *mo;
	AActor *linetarget;

	mo = P_SpawnPlayerMissile (pmo, 0, 0, 8*FRACUNIT,
		RUNTIME_CLASS(AMageStaffFX2), angle, &linetarget);
	if (mo)
	{
		mo->target = pmo;
		mo->tracer = linetarget;
	}
}

//============================================================================
//
// A_MStaffAttack
//
//============================================================================

void A_MStaffAttack (AActor *actor)
{
	angle_t angle;
	player_t *player;
	AActor *linetarget;

	if (NULL == (player = actor->player))
	{
		return;
	}

	AMWeapBloodscourge *weapon = static_cast<AMWeapBloodscourge *> (actor->player->ReadyWeapon);
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
	}
	angle = actor->angle;
	
	// [RH] Let's try and actually track what the player aimed at
	P_AimLineAttack (actor, angle, PLAYERMISSILERANGE, &linetarget, ANGLE_1*32);
	if (linetarget == NULL)
	{
		BlockCheckLine.x = actor->x;
		BlockCheckLine.y = actor->y;
		BlockCheckLine.dx = -finesine[angle >> ANGLETOFINESHIFT];
		BlockCheckLine.dy = -finecosine[angle >> ANGLETOFINESHIFT];
		linetarget = P_BlockmapSearch (actor, 10, FrontBlockCheck);
	}
	MStaffSpawn (actor, angle);
	MStaffSpawn (actor, angle-ANGLE_1*5);
	MStaffSpawn (actor, angle+ANGLE_1*5);
	S_Sound (actor, CHAN_WEAPON, "MageStaffFire", 1, ATTN_NORM);
	weapon->MStaffCount = 3;
}

//============================================================================
//
// A_MStaffPalette
//
//============================================================================

void A_MStaffPalette (AActor *actor)
{
	if (actor->player != NULL)
	{
		AMWeapBloodscourge *weapon = static_cast<AMWeapBloodscourge *> (actor->player->ReadyWeapon);
		if (weapon != NULL && weapon->MStaffCount != 0)
		{
			weapon->MStaffCount--;
		}
	}
}

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

//============================================================================
//
// A_DropBloodscourgePieces
//
//============================================================================

void A_DropBloodscourgePieces (AActor *actor)
{
	static const PClass *pieces[3] =
	{
		RUNTIME_CLASS(AMWeaponPiece1),
		RUNTIME_CLASS(AMWeaponPiece2),
		RUNTIME_CLASS(AMWeaponPiece3)
	};

	for (int i = 0, j = 0, fineang = 0; i < 3; ++i)
	{
		AActor *piece = Spawn (pieces[j], actor->x, actor->y, actor->z, ALLOW_REPLACE);
		if (piece != NULL)
		{
			piece->momx = actor->momx + finecosine[fineang];
			piece->momy = actor->momy + finesine[fineang];
			piece->momz = actor->momz;
			piece->flags |= MF_DROPPED;
			fineang += FINEANGLES/3;
			j = (j == 0) ? (pr_bloodscourgedrop() & 1) + 1 : 3-j;
		}
	}
}

void AMageWeaponPiece::BeginPlay ()
{
	Super::BeginPlay ();
	FourthWeaponClass = RUNTIME_CLASS(AMWeapBloodscourge);
}

//============================================================================
//
// FrontBlockCheck
//
// [RH] Like RoughBlockCheck, but it won't return anything behind a line.
//
//============================================================================

static AActor *FrontBlockCheck (AActor *mo, int index)
{
	FBlockNode *link;

	for (link = blocklinks[index]; link != NULL; link = link->NextActor)
	{
		if (link->Me != mo)
		{
			if (P_PointOnDivlineSide (link->Me->x, link->Me->y, &BlockCheckLine) == 0 &&
				mo->IsOkayToAttack (link->Me))
			{
				return link->Me;
			}
		}
	}
	return NULL;
}

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
		mo->tracer = P_BlockmapSearch (mo, 10, FrontBlockCheck);
	}
}

//============================================================================
//
// A_MStaffAttack2 - for use by mage class boss
//
//============================================================================

void A_MageAttack (AActor *actor)
{
	if (!actor->target) return;

	angle_t angle;
	angle = actor->angle;
	MStaffSpawn2 (actor, angle);
	MStaffSpawn2 (actor, angle-ANGLE_1*5);
	MStaffSpawn2 (actor, angle+ANGLE_1*5);
	S_Sound (actor, CHAN_WEAPON, "MageStaffFire", 1, ATTN_NORM);
}

