#include "actor.h"
#include "gi.h"
#include "m_random.h"
#include "s_sound.h"
#include "d_player.h"
#include "a_action.h"
#include "p_local.h"
#include "p_enemy.h"
#include "p_pspr.h"
#include "gstrings.h"
#include "a_hexenglobal.h"

static FRandom pr_quietusdrop ("QuietusDrop");
static FRandom pr_fswordflame ("FSwordFlame");

void A_FSwordAttack (AActor *actor);
void A_DropQuietusPieces (AActor *);
void A_FSwordFlames (AActor *);

//==========================================================================

class AFighterWeaponPiece : public AFourthWeaponPiece
{
	DECLARE_STATELESS_ACTOR (AFighterWeaponPiece, AFourthWeaponPiece)
public:
	void BeginPlay ();
protected:
	bool MatchPlayerClass (AActor *toucher);
};

IMPLEMENT_CLASS (AFighterWeaponPiece)

bool AFighterWeaponPiece::MatchPlayerClass (AActor *toucher)
{
	return !toucher->IsKindOf (PClass::FindClass(NAME_ClericPlayer)) &&
		   !toucher->IsKindOf (PClass::FindClass(NAME_MagePlayer));
}

//==========================================================================

class AFWeaponPiece1 : public AFighterWeaponPiece
{
	DECLARE_CLASS (AFWeaponPiece1, AFighterWeaponPiece)
public:
	void BeginPlay ();
};

IMPLEMENT_CLASS (AFWeaponPiece1)

void AFWeaponPiece1::BeginPlay ()
{
	Super::BeginPlay ();
	PieceValue = WPIECE1;
}

//==========================================================================

class AFWeaponPiece2 : public AFighterWeaponPiece
{
	DECLARE_CLASS (AFWeaponPiece2, AFighterWeaponPiece)
public:
	void BeginPlay ();
};

IMPLEMENT_CLASS (AFWeaponPiece2)

void AFWeaponPiece2::BeginPlay ()
{
	Super::BeginPlay ();
	PieceValue = WPIECE2;
}

//==========================================================================

class AFWeaponPiece3 : public AFighterWeaponPiece
{
	DECLARE_CLASS (AFWeaponPiece3, AFighterWeaponPiece)
public:
	void BeginPlay ();
};

IMPLEMENT_CLASS (AFWeaponPiece3)

void AFWeaponPiece3::BeginPlay ()
{
	Super::BeginPlay ();
	PieceValue = WPIECE3;
}

// Fighter Sword Missile ----------------------------------------------------

class AFSwordMissile : public AActor
{
	DECLARE_CLASS (AFSwordMissile, AActor)
public:
	int DoSpecialDamage(AActor *victim, AActor *source, int damage);
};

IMPLEMENT_CLASS (AFSwordMissile)

int AFSwordMissile::DoSpecialDamage(AActor *victim, AActor *source, int damage)
{
	if (victim->player)
	{
		damage -= damage >> 2;
	}
	return damage;
}

//============================================================================
//
// A_FSwordAttack
//
//============================================================================

void A_FSwordAttack (AActor *actor)
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
	P_SpawnPlayerMissile (actor, 0, 0, -10*FRACUNIT, RUNTIME_CLASS(AFSwordMissile), actor->angle+ANGLE_45/4);
	P_SpawnPlayerMissile (actor, 0, 0,  -5*FRACUNIT, RUNTIME_CLASS(AFSwordMissile), actor->angle+ANGLE_45/8);
	P_SpawnPlayerMissile (actor, 0, 0,   0,		   RUNTIME_CLASS(AFSwordMissile), actor->angle);
	P_SpawnPlayerMissile (actor, 0, 0,   5*FRACUNIT, RUNTIME_CLASS(AFSwordMissile), actor->angle-ANGLE_45/8);
	P_SpawnPlayerMissile (actor, 0, 0,  10*FRACUNIT, RUNTIME_CLASS(AFSwordMissile), actor->angle-ANGLE_45/4);
	S_Sound (actor, CHAN_WEAPON, "FighterSwordFire", 1, ATTN_NORM);
}

//============================================================================
//
// A_FSwordFlames
//
//============================================================================

void A_FSwordFlames (AActor *actor)
{
	int i;

	for (i = 1+(pr_fswordflame()&3); i; i--)
	{
		fixed_t x = actor->x+((pr_fswordflame()-128)<<12);
		fixed_t y = actor->y+((pr_fswordflame()-128)<<12);
		fixed_t z = actor->z+((pr_fswordflame()-128)<<11);
		Spawn ("FSwordFlame", x, y, z, ALLOW_REPLACE);
	}
}

//============================================================================
//
// A_DropQuietusPieces
//
//============================================================================

void A_DropQuietusPieces (AActor *actor)
{
	static const PClass *pieces[3] =
	{
		RUNTIME_CLASS(AFWeaponPiece1),
		RUNTIME_CLASS(AFWeaponPiece2),
		RUNTIME_CLASS(AFWeaponPiece3)
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
			j = (j == 0) ? (pr_quietusdrop() & 1) + 1 : 3-j;
		}
	}
}

void AFighterWeaponPiece::BeginPlay ()
{
	Super::BeginPlay ();
	FourthWeaponClass = PClass::FindClass ("FWeapQuietus");
}

//============================================================================
//
// A_FighterAttack
//
//============================================================================

void A_FighterAttack (AActor *actor)
{
	if (!actor->target) return;

	angle_t angle = actor->angle;

	P_SpawnMissileAngle (actor, RUNTIME_CLASS(AFSwordMissile), angle+ANG45/4, 0);
	P_SpawnMissileAngle (actor, RUNTIME_CLASS(AFSwordMissile), angle+ANG45/8, 0);
	P_SpawnMissileAngle (actor, RUNTIME_CLASS(AFSwordMissile), angle,         0);
	P_SpawnMissileAngle (actor, RUNTIME_CLASS(AFSwordMissile), angle-ANG45/8, 0);
	P_SpawnMissileAngle (actor, RUNTIME_CLASS(AFSwordMissile), angle-ANG45/4, 0);
	S_Sound (actor, CHAN_WEAPON, "FighterSwordFire", 1, ATTN_NORM);
}

