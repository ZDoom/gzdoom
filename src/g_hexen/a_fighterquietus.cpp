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

IMPLEMENT_STATELESS_ACTOR (AFighterWeaponPiece, Hexen, -1, 0)
	PROP_Inventory_PickupMessage("$TXT_QUIETUS_PIECE")
END_DEFAULTS

bool AFighterWeaponPiece::MatchPlayerClass (AActor *toucher)
{
	return !toucher->IsKindOf (PClass::FindClass(NAME_ClericPlayer)) &&
		   !toucher->IsKindOf (PClass::FindClass(NAME_MagePlayer));
}

//==========================================================================

class AFWeaponPiece1 : public AFighterWeaponPiece
{
	DECLARE_ACTOR (AFWeaponPiece1, AFighterWeaponPiece)
public:
	void BeginPlay ();
};

FState AFWeaponPiece1::States[] =
{
	S_BRIGHT (WFR1, 'A',   -1, NULL					    , NULL)
};

IMPLEMENT_ACTOR (AFWeaponPiece1, Hexen, 12, 29)
	PROP_Flags (MF_SPECIAL)
	PROP_Flags2 (MF2_FLOATBOB)
	PROP_SpawnState (0)
END_DEFAULTS

void AFWeaponPiece1::BeginPlay ()
{
	Super::BeginPlay ();
	PieceValue = WPIECE1;
}

//==========================================================================

class AFWeaponPiece2 : public AFighterWeaponPiece
{
	DECLARE_ACTOR (AFWeaponPiece2, AFighterWeaponPiece)
public:
	void BeginPlay ();
};

FState AFWeaponPiece2::States[] =
{
	S_BRIGHT (WFR2, 'A',   -1, NULL					    , NULL)
};

IMPLEMENT_ACTOR (AFWeaponPiece2, Hexen, 13, 30)
	PROP_Flags (MF_SPECIAL)
	PROP_Flags2 (MF2_FLOATBOB)
	PROP_SpawnState (0)
END_DEFAULTS

void AFWeaponPiece2::BeginPlay ()
{
	Super::BeginPlay ();
	PieceValue = WPIECE2;
}

//==========================================================================

class AFWeaponPiece3 : public AFighterWeaponPiece
{
	DECLARE_ACTOR (AFWeaponPiece3, AFighterWeaponPiece)
public:
	void BeginPlay ();
};

FState AFWeaponPiece3::States[] =
{
	S_BRIGHT (WFR3, 'A',   -1, NULL					    , NULL)
};

IMPLEMENT_ACTOR (AFWeaponPiece3, Hexen, 16, 31)
	PROP_Flags (MF_SPECIAL)
	PROP_Flags2 (MF2_FLOATBOB)
	PROP_SpawnState (0)
END_DEFAULTS

void AFWeaponPiece3::BeginPlay ()
{
	Super::BeginPlay ();
	PieceValue = WPIECE3;
}

// An actor that spawns the three pieces of the fighter's fourth weapon -----

// This gets spawned if weapon drop is on so that other players can pick up
// this player's weapon.

class AQuietusDrop : public AActor
{
	DECLARE_ACTOR (AQuietusDrop, AActor)
};

FState AQuietusDrop::States[] =
{
	S_NORMAL (TNT1, 'A', 1, NULL, &States[1]),
	S_NORMAL (TNT1, 'A', 1, A_DropQuietusPieces, NULL)
};

IMPLEMENT_ACTOR (AQuietusDrop, Hexen, -1, 0)
	PROP_SpawnState (0)
END_DEFAULTS

// The Fighter's Sword (Quietus) --------------------------------------------

class AFWeapQuietus : public AFighterWeapon
{
	DECLARE_ACTOR (AFWeapQuietus, AFighterWeapon)
};

FState AFWeapQuietus::States[] =
{
	// Dummy state, because the fourth weapon does not appear in a level directly.
	S_NORMAL (TNT1, 'A',   -1, NULL					    , NULL),

#define S_FSWORDREADY (1)
	S_BRIGHT (FSRD, 'A',	1, A_WeaponReady		    , &States[S_FSWORDREADY+1]),	//  S_FSWORDREADY
	S_BRIGHT (FSRD, 'A',	1, A_WeaponReady		    , &States[S_FSWORDREADY+2]),	//  S_FSWORDREADY1
	S_BRIGHT (FSRD, 'A',	1, A_WeaponReady		    , &States[S_FSWORDREADY+3]),	//  S_FSWORDREADY2
	S_BRIGHT (FSRD, 'A',	1, A_WeaponReady		    , &States[S_FSWORDREADY+4]),	//  S_FSWORDREADY3
	S_BRIGHT (FSRD, 'B',	1, A_WeaponReady		    , &States[S_FSWORDREADY+5]),	//  S_FSWORDREADY4
	S_BRIGHT (FSRD, 'B',	1, A_WeaponReady		    , &States[S_FSWORDREADY+6]),	//  S_FSWORDREADY5
	S_BRIGHT (FSRD, 'B',	1, A_WeaponReady		    , &States[S_FSWORDREADY+7]),	//  S_FSWORDREADY6
	S_BRIGHT (FSRD, 'B',	1, A_WeaponReady		    , &States[S_FSWORDREADY+8]),	//  S_FSWORDREADY7
	S_BRIGHT (FSRD, 'C',	1, A_WeaponReady		    , &States[S_FSWORDREADY+9]),	//  S_FSWORDREADY8
	S_BRIGHT (FSRD, 'C',	1, A_WeaponReady		    , &States[S_FSWORDREADY+10]),	//  S_FSWORDREADY9
	S_BRIGHT (FSRD, 'C',	1, A_WeaponReady		    , &States[S_FSWORDREADY+11]),	//  S_FSWORDREADY10
	S_BRIGHT (FSRD, 'C',	1, A_WeaponReady		    , &States[S_FSWORDREADY]),	//  S_FSWORDREADY11

#define S_FSWORDDOWN (S_FSWORDREADY+12)
	S_BRIGHT (FSRD, 'A',	1, A_Lower				    , &States[S_FSWORDDOWN]),	//  S_FSWORDDOWN

#define S_FSWORDUP (S_FSWORDDOWN+1)
	S_BRIGHT (FSRD, 'A',	1, A_Raise				    , &States[S_FSWORDUP]),	//  S_FSWORDUP

#define S_FSWORDATK (S_FSWORDUP+1)
	S_BRIGHT2 (FSRD, 'D',	3, NULL					    , &States[S_FSWORDATK+1], 5, 36),	//  S_FSWORDATK_1
	S_BRIGHT2 (FSRD, 'E',	3, NULL					    , &States[S_FSWORDATK+2], 5, 36),	//  S_FSWORDATK_2
	S_BRIGHT2 (FSRD, 'F',	2, NULL					    , &States[S_FSWORDATK+3], 5, 36),	//  S_FSWORDATK_3
	S_BRIGHT2 (FSRD, 'G',	3, A_FSwordAttack		    , &States[S_FSWORDATK+4], 5, 36),	//  S_FSWORDATK_4
	S_BRIGHT2 (FSRD, 'H',	2, NULL					    , &States[S_FSWORDATK+5], 5, 36),	//  S_FSWORDATK_5
	S_BRIGHT2 (FSRD, 'I',	2, NULL					    , &States[S_FSWORDATK+6], 5, 36),	//  S_FSWORDATK_6
	S_BRIGHT2 (FSRD, 'I',  10, NULL					    , &States[S_FSWORDATK+7], 5, 150),	//  S_FSWORDATK_7
	S_BRIGHT2 (FSRD, 'A',	1, NULL					    , &States[S_FSWORDATK+8], 5, 60),	//  S_FSWORDATK_8
	S_BRIGHT2 (FSRD, 'B',	1, NULL					    , &States[S_FSWORDATK+9], 5, 55),	//  S_FSWORDATK_9
	S_BRIGHT2 (FSRD, 'C',	1, NULL					    , &States[S_FSWORDATK+10], 5, 50),	//  S_FSWORDATK_10
	S_BRIGHT2 (FSRD, 'A',	1, NULL					    , &States[S_FSWORDATK+11], 5, 45),	//  S_FSWORDATK_11
	S_BRIGHT2 (FSRD, 'B',	1, NULL					    , &States[S_FSWORDREADY], 5, 40),	//  S_FSWORDATK_12
};

IMPLEMENT_ACTOR (AFWeapQuietus, Hexen, -1, 0)
	PROP_Flags (MF_SPECIAL)
	PROP_SpawnState (0)

	PROP_Weapon_SelectionOrder (2900)
	PROP_Weapon_Flags (WIF_PRIMARY_USES_BOTH)
	PROP_Weapon_AmmoUse1 (14)
	PROP_Weapon_AmmoUse2 (14)
	PROP_Weapon_AmmoGive1 (0)
	PROP_Weapon_AmmoGive2 (0)
	PROP_Weapon_UpState (S_FSWORDUP)
	PROP_Weapon_DownState (S_FSWORDDOWN)
	PROP_Weapon_ReadyState (S_FSWORDREADY)
	PROP_Weapon_AtkState (S_FSWORDATK)
	PROP_Weapon_Kickback (150)
	PROP_Weapon_YAdjust (10)
	PROP_Weapon_MoveCombatDist (20000000)
	PROP_Weapon_AmmoType1 ("Mana1")
	PROP_Weapon_AmmoType2 ("Mana2")
	PROP_Weapon_ProjectileType ("FSwordMissile")
	PROP_Inventory_PickupMessage("$TXT_WEAPON_F4")
END_DEFAULTS

// Fighter Sword Missile ----------------------------------------------------

class AFSwordMissile : public AActor
{
	DECLARE_ACTOR (AFSwordMissile, AActor)
public:
	void GetExplodeParms (int &damage, int &dist, bool &hurtSource);
	int DoSpecialDamage(AActor *victim, AActor *source, int damage);
};

FState AFSwordMissile::States[] =
{
#define S_FSWORD_MISSILE1 0
	S_BRIGHT (FSFX, 'A',	3, NULL					    , &States[S_FSWORD_MISSILE1+1]),
	S_BRIGHT (FSFX, 'B',	3, NULL					    , &States[S_FSWORD_MISSILE1+2]),
	S_BRIGHT (FSFX, 'C',	3, NULL					    , &States[S_FSWORD_MISSILE1]),

#define S_FSWORD_MISSILE_X1 (S_FSWORD_MISSILE1+3)
	S_BRIGHT (FSFX, 'D',	4, NULL					    , &States[S_FSWORD_MISSILE_X1+1]),
	S_BRIGHT (FSFX, 'E',	3, A_FSwordFlames		    , &States[S_FSWORD_MISSILE_X1+2]),
	S_BRIGHT (FSFX, 'F',	4, A_Explode			    , &States[S_FSWORD_MISSILE_X1+3]),
	S_BRIGHT (FSFX, 'G',	3, NULL					    , &States[S_FSWORD_MISSILE_X1+4]),
	S_BRIGHT (FSFX, 'H',	4, NULL					    , &States[S_FSWORD_MISSILE_X1+5]),
	S_BRIGHT (FSFX, 'I',	3, NULL					    , &States[S_FSWORD_MISSILE_X1+6]),
	S_BRIGHT (FSFX, 'J',	4, NULL					    , &States[S_FSWORD_MISSILE_X1+7]),
	S_BRIGHT (FSFX, 'K',	3, NULL					    , &States[S_FSWORD_MISSILE_X1+8]),
	S_BRIGHT (FSFX, 'L',	3, NULL					    , &States[S_FSWORD_MISSILE_X1+9]),
	S_BRIGHT (FSFX, 'M',	3, NULL					    , NULL),
};

IMPLEMENT_ACTOR (AFSwordMissile, Hexen, -1, 0)
	PROP_SpeedFixed (30)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (8)
	PROP_Damage (8)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_IMPACT|MF2_PCROSS)
	PROP_Flags4 (MF4_EXTREMEDEATH)
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (S_FSWORD_MISSILE1)
	PROP_DeathState (S_FSWORD_MISSILE_X1)

	PROP_DeathSound ("FighterSwordExplode")
END_DEFAULTS

void AFSwordMissile::GetExplodeParms (int &damage, int &dist, bool &hurtSource)
{
	damage = 64;
	hurtSource = false;
}

int AFSwordMissile::DoSpecialDamage(AActor *victim, AActor *source, int damage)
{
	if (victim->player)
	{
		damage -= damage >> 2;
	}
	return damage;
}


// Fighter Sword Flame ------------------------------------------------------

class AFSwordFlame : public AActor
{
	DECLARE_ACTOR (AFSwordFlame, AActor)
};

FState AFSwordFlame::States[] =
{
	S_BRIGHT (FSFX, 'N',	3, NULL					    , &States[1]),
	S_BRIGHT (FSFX, 'O',	3, NULL					    , &States[2]),
	S_BRIGHT (FSFX, 'P',	3, NULL					    , &States[3]),
	S_BRIGHT (FSFX, 'Q',	3, NULL					    , &States[4]),
	S_BRIGHT (FSFX, 'R',	3, NULL					    , &States[5]),
	S_BRIGHT (FSFX, 'S',	3, NULL					    , &States[6]),
	S_BRIGHT (FSFX, 'T',	3, NULL					    , &States[7]),
	S_BRIGHT (FSFX, 'U',	3, NULL					    , &States[8]),
	S_BRIGHT (FSFX, 'V',	3, NULL					    , &States[9]),
	S_BRIGHT (FSFX, 'W',	3, NULL					    , NULL),
};

IMPLEMENT_ACTOR (AFSwordFlame, Hexen, -1, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (HX_SHADOW)
	PROP_SpawnState (0)
END_DEFAULTS


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
		Spawn<AFSwordFlame> (x, y, z, ALLOW_REPLACE);
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
	FourthWeaponClass = RUNTIME_CLASS(AFWeapQuietus);
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

