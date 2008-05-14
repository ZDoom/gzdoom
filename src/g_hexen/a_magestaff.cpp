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
	DECLARE_STATELESS_ACTOR (AMageWeaponPiece, AFourthWeaponPiece)
public:
	void BeginPlay ();
protected:
	bool MatchPlayerClass (AActor *toucher);
};

IMPLEMENT_STATELESS_ACTOR (AMageWeaponPiece, Hexen, -1, 0)
	PROP_Inventory_PickupMessage("$TXT_BLOODSCOURGE_PIECE")
END_DEFAULTS

bool AMageWeaponPiece::MatchPlayerClass (AActor *toucher)
{
	return !toucher->IsKindOf (PClass::FindClass(NAME_FighterPlayer)) &&
		   !toucher->IsKindOf (PClass::FindClass(NAME_ClericPlayer));
}

//==========================================================================

class AMWeaponPiece1 : public AMageWeaponPiece
{
	DECLARE_ACTOR (AMWeaponPiece1, AMageWeaponPiece)
public:
	void BeginPlay ();
};

FState AMWeaponPiece1::States[] =
{
	S_BRIGHT (WMS1, 'A',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (AMWeaponPiece1, Hexen, 21, 37)
	PROP_Flags (MF_SPECIAL)
	PROP_Flags2 (MF2_FLOATBOB)
	PROP_SpawnState (0)
END_DEFAULTS

void AMWeaponPiece1::BeginPlay ()
{
	Super::BeginPlay ();
	PieceValue = WPIECE1<<6;
}

//==========================================================================

class AMWeaponPiece2 : public AMageWeaponPiece
{
	DECLARE_ACTOR (AMWeaponPiece2, AMageWeaponPiece)
public:
	void BeginPlay ();
};

FState AMWeaponPiece2::States[] =
{
	S_BRIGHT (WMS2, 'A',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (AMWeaponPiece2, Hexen, 22, 38)
	PROP_Flags (MF_SPECIAL)
	PROP_Flags2 (MF2_FLOATBOB)
	PROP_SpawnState (0)
END_DEFAULTS

void AMWeaponPiece2::BeginPlay ()
{
	Super::BeginPlay ();
	PieceValue = WPIECE2<<6;
}

//==========================================================================

class AMWeaponPiece3 : public AMageWeaponPiece
{
	DECLARE_ACTOR (AMWeaponPiece3, AMageWeaponPiece)
public:
	void BeginPlay ();
};

FState AMWeaponPiece3::States[] =
{
	S_BRIGHT (WMS3, 'A',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (AMWeaponPiece3, Hexen, 23, 39)
	PROP_Flags (MF_SPECIAL)
	PROP_Flags2 (MF2_FLOATBOB)
	PROP_SpawnState (0)
END_DEFAULTS

void AMWeaponPiece3::BeginPlay ()
{
	Super::BeginPlay ();
	PieceValue = WPIECE3<<6;
}

// An actor that spawns the three pieces of the mage's fourth weapon --------

// This gets spawned if weapon drop is on so that other players can pick up
// this player's weapon.

class ABloodscourgeDrop : public AActor
{
	DECLARE_ACTOR (ABloodscourgeDrop, AActor)
};

FState ABloodscourgeDrop::States[] =
{
	S_NORMAL (TNT1, 'A', 1, NULL, &States[1]),
	S_NORMAL (TNT1, 'A', 1, A_DropBloodscourgePieces, NULL)
};

IMPLEMENT_ACTOR (ABloodscourgeDrop, Hexen, -1, 0)
	PROP_SpawnState (0)
END_DEFAULTS

// The Mages's Staff (Bloodscourge) -----------------------------------------

class AMWeapBloodscourge : public AMageWeapon
{
	DECLARE_ACTOR (AMWeapBloodscourge, AMageWeapon)
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

FState AMWeapBloodscourge::States[] =
{
	// Dummy state, because the fourth weapon does not appear in a level directly.
	S_NORMAL (TNT1, 'A',   -1, NULL					    , NULL),

#define S_MSTAFFREADY 1
	S_NORMAL (MSTF, 'A',	1, A_WeaponReady		    , &States[S_MSTAFFREADY+1]),
	S_NORMAL (MSTF, 'A',	1, A_WeaponReady		    , &States[S_MSTAFFREADY+2]),
	S_NORMAL (MSTF, 'A',	1, A_WeaponReady		    , &States[S_MSTAFFREADY+3]),
	S_NORMAL (MSTF, 'A',	1, A_WeaponReady		    , &States[S_MSTAFFREADY+4]),
	S_NORMAL (MSTF, 'A',	1, A_WeaponReady		    , &States[S_MSTAFFREADY+5]),
	S_NORMAL (MSTF, 'A',	1, A_WeaponReady		    , &States[S_MSTAFFREADY+6]),
	S_NORMAL (MSTF, 'B',	1, A_WeaponReady		    , &States[S_MSTAFFREADY+7]),
	S_NORMAL (MSTF, 'B',	1, A_WeaponReady		    , &States[S_MSTAFFREADY+8]),
	S_NORMAL (MSTF, 'B',	1, A_WeaponReady		    , &States[S_MSTAFFREADY+9]),
	S_NORMAL (MSTF, 'B',	1, A_WeaponReady		    , &States[S_MSTAFFREADY+10]),
	S_NORMAL (MSTF, 'B',	1, A_WeaponReady		    , &States[S_MSTAFFREADY+11]),
	S_NORMAL (MSTF, 'B',	1, A_WeaponReady		    , &States[S_MSTAFFREADY+12]),
	S_NORMAL (MSTF, 'C',	1, A_WeaponReady		    , &States[S_MSTAFFREADY+13]),
	S_NORMAL (MSTF, 'C',	1, A_WeaponReady		    , &States[S_MSTAFFREADY+14]),
	S_NORMAL (MSTF, 'C',	1, A_WeaponReady		    , &States[S_MSTAFFREADY+15]),
	S_NORMAL (MSTF, 'C',	1, A_WeaponReady		    , &States[S_MSTAFFREADY+16]),
	S_NORMAL (MSTF, 'C',	1, A_WeaponReady		    , &States[S_MSTAFFREADY+17]),
	S_NORMAL (MSTF, 'C',	1, A_WeaponReady		    , &States[S_MSTAFFREADY+18]),
	S_NORMAL (MSTF, 'D',	1, A_WeaponReady		    , &States[S_MSTAFFREADY+19]),
	S_NORMAL (MSTF, 'D',	1, A_WeaponReady		    , &States[S_MSTAFFREADY+20]),
	S_NORMAL (MSTF, 'D',	1, A_WeaponReady		    , &States[S_MSTAFFREADY+21]),
	S_NORMAL (MSTF, 'D',	1, A_WeaponReady		    , &States[S_MSTAFFREADY+22]),
	S_NORMAL (MSTF, 'D',	1, A_WeaponReady		    , &States[S_MSTAFFREADY+23]),
	S_NORMAL (MSTF, 'D',	1, A_WeaponReady		    , &States[S_MSTAFFREADY+24]),
	S_NORMAL (MSTF, 'E',	1, A_WeaponReady		    , &States[S_MSTAFFREADY+25]),
	S_NORMAL (MSTF, 'E',	1, A_WeaponReady		    , &States[S_MSTAFFREADY+26]),
	S_NORMAL (MSTF, 'E',	1, A_WeaponReady		    , &States[S_MSTAFFREADY+27]),
	S_NORMAL (MSTF, 'E',	1, A_WeaponReady		    , &States[S_MSTAFFREADY+28]),
	S_NORMAL (MSTF, 'E',	1, A_WeaponReady		    , &States[S_MSTAFFREADY+29]),
	S_NORMAL (MSTF, 'E',	1, A_WeaponReady		    , &States[S_MSTAFFREADY+30]),
	S_NORMAL (MSTF, 'F',	1, A_WeaponReady		    , &States[S_MSTAFFREADY+31]),
	S_NORMAL (MSTF, 'F',	1, A_WeaponReady		    , &States[S_MSTAFFREADY+32]),
	S_NORMAL (MSTF, 'F',	1, A_WeaponReady		    , &States[S_MSTAFFREADY+33]),
	S_NORMAL (MSTF, 'F',	1, A_WeaponReady		    , &States[S_MSTAFFREADY+34]),
	S_NORMAL (MSTF, 'F',	1, A_WeaponReady		    , &States[S_MSTAFFREADY]),

#define S_MSTAFFDOWN (S_MSTAFFREADY+35)
	S_NORMAL (MSTF, 'A',	1, A_Lower				    , &States[S_MSTAFFDOWN]),

#define S_MSTAFFUP (S_MSTAFFDOWN+1)
	S_NORMAL (MSTF, 'A',	1, A_Raise				    , &States[S_MSTAFFUP]),

#define S_MSTAFFATK (S_MSTAFFUP+1)
	S_NORMAL2 (MSTF, 'G',	4, NULL					    , &States[S_MSTAFFATK+1], 0, 40),
	S_BRIGHT2 (MSTF, 'H',	4, A_MStaffAttack		    , &States[S_MSTAFFATK+2], 0, 48),
	S_BRIGHT2 (MSTF, 'H',	2, A_MStaffPalette		    , &States[S_MSTAFFATK+3], 0, 48),
	S_NORMAL2 (MSTF, 'I',	2, A_MStaffPalette		    , &States[S_MSTAFFATK+4], 0, 48),
	S_NORMAL2 (MSTF, 'I',	2, A_MStaffPalette		    , &States[S_MSTAFFATK+5], 0, 48),
	S_NORMAL2 (MSTF, 'I',	1, NULL					    , &States[S_MSTAFFATK+6], 0, 40),
	S_NORMAL2 (MSTF, 'J',	5, NULL					    , &States[S_MSTAFFREADY], 0, 36)
};

IMPLEMENT_ACTOR (AMWeapBloodscourge, Hexen, -1, 0)
	PROP_Flags (MF_SPECIAL)
	PROP_SpawnState (0)

	PROP_Weapon_SelectionOrder (3100)
	PROP_Weapon_Flags (WIF_PRIMARY_USES_BOTH)
	PROP_Weapon_AmmoUse1 (15)
	PROP_Weapon_AmmoUse2 (15)
	PROP_Weapon_UpState (S_MSTAFFUP)
	PROP_Weapon_DownState (S_MSTAFFDOWN)
	PROP_Weapon_ReadyState (S_MSTAFFREADY)
	PROP_Weapon_AtkState (S_MSTAFFATK)
	PROP_Weapon_Kickback (150)
	PROP_Weapon_YAdjust (20)
	PROP_Weapon_MoveCombatDist (20000000)
	PROP_Weapon_AmmoType1 ("Mana1")
	PROP_Weapon_AmmoType2 ("Mana2")
	PROP_Weapon_ProjectileType ("MageStaffFX2")
	PROP_Inventory_PickupMessage("$TXT_WEAPON_M4")
END_DEFAULTS

// Mage Staff FX2 (Bloodscourge) --------------------------------------------

void A_BeAdditive (AActor *self)
{
	self->RenderStyle = STYLE_Add;
}

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
	S_BRIGHT (MSP2, 'E',	4, A_BeAdditive			    , &States[S_MSTAFF_FX2_X1+1]),
	S_BRIGHT (MSP2, 'F',	5, A_Explode			    , &States[S_MSTAFF_FX2_X1+2]),
	S_BRIGHT (MSP2, 'G',	5, NULL					    , &States[S_MSTAFF_FX2_X1+3]),
	S_BRIGHT (MSP2, 'H',	5, NULL					    , &States[S_MSTAFF_FX2_X1+4]),
	S_BRIGHT (MSP2, 'I',	4, NULL					    , NULL),
};

IMPLEMENT_ACTOR (AMageStaffFX2, Hexen, -1, 0)
	PROP_SpeedFixed (17)
	PROP_HeightFixed (8)
	PROP_Damage (4)
	PROP_DamageType (NAME_Fire)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_IMPACT|MF2_PCROSS|MF2_SEEKERMISSILE)
	PROP_Flags4 (MF4_EXTREMEDEATH)

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
