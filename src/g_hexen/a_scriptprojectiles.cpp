// Projectiles intended to be spawned through ACS

#include "actor.h"
#include "info.h"
#include "p_local.h"

// Fire Ball ----------------------------------------------------------------

class AFireBall : public AActor
{
	DECLARE_ACTOR (AFireBall, AActor)
};

FState AFireBall::States[] =
{
#define S_FIREBALL1_1 0
	S_BRIGHT (FBL1, 'A',	4, NULL					    , &States[S_FIREBALL1_1+1]),
	S_BRIGHT (FBL1, 'B',	4, NULL					    , &States[S_FIREBALL1_1]),

#define S_FIREBALL1_X1 (S_FIREBALL1_1+2)
	S_BRIGHT (XPL1, 'A',	4, NULL					    , &States[S_FIREBALL1_X1+1]),
	S_BRIGHT (XPL1, 'B',	4, NULL					    , &States[S_FIREBALL1_X1+2]),
	S_BRIGHT (XPL1, 'C',	4, NULL					    , &States[S_FIREBALL1_X1+3]),
	S_BRIGHT (XPL1, 'D',	4, NULL					    , &States[S_FIREBALL1_X1+4]),
	S_BRIGHT (XPL1, 'E',	4, NULL					    , &States[S_FIREBALL1_X1+5]),
	S_BRIGHT (XPL1, 'F',	4, NULL					    , NULL),
};

IMPLEMENT_ACTOR (AFireBall, Hexen, -1, 10)
	PROP_SpeedFixed (2)
	PROP_RadiusFixed (8)
	PROP_HeightFixed (8)
	PROP_Damage (4)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_FIREDAMAGE)

	PROP_SpawnState (S_FIREBALL1_1)
	PROP_DeathState (S_FIREBALL1_X1)

	PROP_DeathSound ("Fireball")
END_DEFAULTS

// Arrow --------------------------------------------------------------------

class AArrow : public AActor
{
	DECLARE_ACTOR (AArrow, AActor)
};

FState AArrow::States[] =
{
#define S_ARROW_1 0
	S_NORMAL (ARRW, 'A',   -1, NULL					    , NULL),

#define S_ARROW_X1 (S_ARROW_1+1)
	S_NORMAL (ARRW, 'A',	1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (AArrow, Hexen, -1, 50)
	PROP_SpeedFixed (6)
	PROP_RadiusFixed (8)
	PROP_HeightFixed (4)
	PROP_Damage (4)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT)

	PROP_SpawnState (S_ARROW_1)
	PROP_DeathState (S_ARROW_X1)
END_DEFAULTS

// Dart ---------------------------------------------------------------------

class ADart : public AActor
{
	DECLARE_ACTOR (ADart, AActor)
};

FState ADart::States[] =
{
#define S_DART_1 0
	S_NORMAL (DART, 'A',   -1, NULL					    , NULL),

#define S_DART_X1 (S_DART_1+1)
	S_NORMAL (DART, 'A',	1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (ADart, Hexen, -1, 51)
	PROP_SpeedFixed (6)
	PROP_RadiusFixed (8)
	PROP_HeightFixed (4)
	PROP_Damage (2)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT)

	PROP_SpawnState (S_DART_1)
	PROP_DeathState (S_DART_X1)
END_DEFAULTS

// Poison Dart --------------------------------------------------------------

class APoisonDart : public ADart
{
	DECLARE_STATELESS_ACTOR (APoisonDart, ADart)
public:
	int DoSpecialDamage (AActor *victim, int damage);
};

IMPLEMENT_STATELESS_ACTOR (APoisonDart, Hexen, -1, 52)
END_DEFAULTS

int APoisonDart::DoSpecialDamage (AActor *victim, int damage)
{
	if (target->player)
	{
		P_PoisonPlayer (target->player, this, 20);
		damage >>= 1;
	}
	return damage;
}

// Ripper Ball --------------------------------------------------------------

class ARipperBall : public AActor
{
	DECLARE_ACTOR (ARipperBall, AActor)
};

FState ARipperBall::States[] =
{
#define S_RIPPERBALL_1 0
	S_NORMAL (RIPP, 'A',	3, NULL					    , &States[S_RIPPERBALL_1+1]),
	S_NORMAL (RIPP, 'B',	3, NULL					    , &States[S_RIPPERBALL_1+2]),
	S_NORMAL (RIPP, 'C',	3, NULL					    , &States[S_RIPPERBALL_1]),

#define S_RIPPERBALL_X1 (S_RIPPERBALL_1+3)
	S_BRIGHT (CFCF, 'Q',	4, NULL					    , &States[S_RIPPERBALL_X1+1]),
	S_BRIGHT (CFCF, 'R',	3, NULL					    , &States[S_RIPPERBALL_X1+2]),
	S_BRIGHT (CFCF, 'S',	4, NULL					    , &States[S_RIPPERBALL_X1+3]),
	S_BRIGHT (CFCF, 'T',	3, NULL					    , &States[S_RIPPERBALL_X1+4]),
	S_BRIGHT (CFCF, 'U',	4, NULL					    , &States[S_RIPPERBALL_X1+5]),
	S_BRIGHT (CFCF, 'V',	3, NULL					    , &States[S_RIPPERBALL_X1+6]),
	S_BRIGHT (CFCF, 'W',	4, NULL					    , &States[S_RIPPERBALL_X1+7]),
	S_BRIGHT (CFCF, 'X',	3, NULL					    , &States[S_RIPPERBALL_X1+8]),
	S_BRIGHT (CFCF, 'Y',	4, NULL					    , &States[S_RIPPERBALL_X1+9]),
	S_BRIGHT (CFCF, 'Z',	3, NULL					    , NULL),
};

IMPLEMENT_ACTOR (ARipperBall, Hexen, -1, 53)
	PROP_SpeedFixed (6)
	PROP_RadiusFixed (8)
	PROP_Damage (2)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_RIP)

	PROP_SpawnState (S_RIPPERBALL_1)
	PROP_DeathState (S_RIPPERBALL_X1)
END_DEFAULTS

// Projectile Blade ---------------------------------------------------------

class AProjectileBlade : public AActor
{
	DECLARE_ACTOR (AProjectileBlade, AActor)
};

FState AProjectileBlade::States[] =
{
#define S_PRJ_BLADE1 0
	S_NORMAL (BLAD, 'A',   -1, NULL					    , NULL),

#define S_PRJ_BLADE_X1 (S_PRJ_BLADE1+1)
	S_NORMAL (BLAD, 'A',	1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (AProjectileBlade, Hexen, -1, 64)
	PROP_SpeedFixed (6)
	PROP_RadiusFixed (6)
	PROP_HeightFixed (6)
	PROP_Damage (3)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT)

	PROP_SpawnState (S_PRJ_BLADE1)
	PROP_DeathState (S_PRJ_BLADE_X1)
END_DEFAULTS
