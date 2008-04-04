#include "templates.h"
#include "actor.h"
#include "info.h"
#include "s_sound.h"
#include "m_random.h"
#include "a_pickups.h"
#include "a_hereticglobal.h"
#include "d_player.h"
#include "p_pspr.h"
#include "p_local.h"
#include "gstrings.h"
#include "p_effect.h"
#include "gstrings.h"
#include "p_enemy.h"
#include "gi.h"
#include "r_translate.h"

static FRandom pr_sap ("StaffAtkPL1");
static FRandom pr_sap2 ("StaffAtkPL2");
static FRandom pr_fgw ("FireWandPL1");
static FRandom pr_fgw2 ("FireWandPL2");
static FRandom pr_boltspark ("BoltSpark");
static FRandom pr_spawnmace ("SpawnMace");
static FRandom pr_macerespawn ("MaceRespawn");
static FRandom pr_maceatk ("FireMacePL1");
static FRandom pr_gatk ("GauntletAttack");
static FRandom pr_bfx1 ("BlasterFX1");
static FRandom pr_ripd ("RipperD");
static FRandom pr_fb1 ("FireBlasterPL1");
static FRandom pr_bfx1t ("BlasterFX1Tick");
static FRandom pr_hrfx2 ("HornRodFX2");
static FRandom pr_rp ("RainPillar");
static FRandom pr_fsr1 ("FireSkullRodPL1");
static FRandom pr_storm ("SkullRodStorm");
static FRandom pr_impact ("RainImpact");
static FRandom pr_pfx1 ("PhoenixFX1");
static FRandom pr_pfx2 ("PhoenixFX2");
static FRandom pr_fp2 ("FirePhoenixPL2");

#define FLAME_THROWER_TICS (10*TICRATE)


#define USE_GWND_AMMO_1 1
#define USE_GWND_AMMO_2 1
#define USE_CBOW_AMMO_1 1
#define USE_CBOW_AMMO_2 1
#define USE_BLSR_AMMO_1 1
#define USE_BLSR_AMMO_2 5
#define USE_SKRD_AMMO_1 1
#define USE_SKRD_AMMO_2 5
#define USE_PHRD_AMMO_1 1
#define USE_PHRD_AMMO_2 1
#define USE_MACE_AMMO_1 1
#define USE_MACE_AMMO_2 5

extern bool P_AutoUseChaosDevice (player_t *player);

// Base Heretic weapon class ------------------------------------------------

IMPLEMENT_STATELESS_ACTOR (AHereticWeapon, Heretic, -1, 0)
	PROP_Weapon_Kickback (150)
END_DEFAULTS

// --- Staff ----------------------------------------------------------------

void A_StaffAttackPL1 (AActor *);
void A_StaffAttackPL2 (AActor *);

// Staff --------------------------------------------------------------------

class AStaff : public AHereticWeapon
{
	DECLARE_ACTOR (AStaff, AHereticWeapon)
};

class AStaffPowered : public AStaff
{
	DECLARE_STATELESS_ACTOR (AStaffPowered, AStaff)
};

FState AStaff::States[] =
{
#define S_STAFFREADY 0
	S_NORMAL (STFF, 'A',	1, A_WeaponReady				, &States[S_STAFFREADY]),

#define S_STAFFDOWN (S_STAFFREADY+1)
	S_NORMAL (STFF, 'A',	1, A_Lower						, &States[S_STAFFDOWN]),

#define S_STAFFUP (S_STAFFDOWN+1)
	S_NORMAL (STFF, 'A',	1, A_Raise						, &States[S_STAFFUP]),

#define S_STAFFREADY2 (S_STAFFUP+1)
	S_NORMAL (STFF, 'D',	4, A_WeaponReady				, &States[S_STAFFREADY2+1]),
	S_NORMAL (STFF, 'E',	4, A_WeaponReady				, &States[S_STAFFREADY2+2]),
	S_NORMAL (STFF, 'F',	4, A_WeaponReady				, &States[S_STAFFREADY2+0]),

#define S_STAFFDOWN2 (S_STAFFREADY2+3)
	S_NORMAL (STFF, 'D',	1, A_Lower						, &States[S_STAFFDOWN2]),

#define S_STAFFUP2 (S_STAFFDOWN2+1)
	S_NORMAL (STFF, 'D',	1, A_Raise						, &States[S_STAFFUP2]),

#define S_STAFFATK1 (S_STAFFUP2+1)
	S_NORMAL (STFF, 'B',	6, NULL 						, &States[S_STAFFATK1+1]),
	S_NORMAL (STFF, 'C',	8, A_StaffAttackPL1 			, &States[S_STAFFATK1+2]),
	S_NORMAL (STFF, 'B',	8, A_ReFire 					, &States[S_STAFFREADY]),

#define S_STAFFATK2 (S_STAFFATK1+3)
	S_NORMAL (STFF, 'G',	6, NULL 						, &States[S_STAFFATK2+1]),
	S_NORMAL (STFF, 'H',	8, A_StaffAttackPL2 			, &States[S_STAFFATK2+2]),
	S_NORMAL (STFF, 'G',	8, A_ReFire 					, &States[S_STAFFREADY2+0])
};

IMPLEMENT_ACTOR (AStaff, Heretic, -1, 0)
	PROP_Weapon_SelectionOrder (3800)
	PROP_Flags2Set(MF2_THRUGHOST)
	PROP_Weapon_Flags (WIF_WIMPY_WEAPON|WIF_BOT_MELEE)
	PROP_Weapon_UpState (S_STAFFUP)
	PROP_Weapon_DownState (S_STAFFDOWN)
	PROP_Weapon_ReadyState (S_STAFFREADY)
	PROP_Weapon_AtkState (S_STAFFATK1)
	PROP_Weapon_SisterType ("StaffPowered")
END_DEFAULTS

IMPLEMENT_STATELESS_ACTOR (AStaffPowered, Heretic, -1, 0)
	PROP_Weapon_Flags (WIF_WIMPY_WEAPON|WIF_READYSNDHALF|WIF_POWERED_UP|WIF_BOT_MELEE|WIF_STAFF2_KICKBACK)
	PROP_Weapon_UpState (S_STAFFUP2)
	PROP_Weapon_DownState (S_STAFFDOWN2)
	PROP_Weapon_ReadyState (S_STAFFREADY2)
	PROP_Weapon_AtkState (S_STAFFATK2)
	PROP_Weapon_ReadySound ("weapons/staffcrackle")
	PROP_Weapon_SisterType ("Staff")
END_DEFAULTS

// Staff puff ---------------------------------------------------------------

FState AStaffPuff::States[] =
{
	S_BRIGHT (PUF3, 'A',	4, NULL 						, &States[1]),
	S_NORMAL (PUF3, 'B',	4, NULL 						, &States[2]),
	S_NORMAL (PUF3, 'C',	4, NULL 						, &States[3]),
	S_NORMAL (PUF3, 'D',	4, NULL 						, NULL)
};

IMPLEMENT_ACTOR (AStaffPuff, Heretic, -1, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_Flags3 (MF3_PUFFONACTORS)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (HR_SHADOW)
	PROP_SpawnState (0)
	PROP_AttackSound ("weapons/staffhit")
END_DEFAULTS

void AStaffPuff::BeginPlay ()
{
	Super::BeginPlay ();
	momz = FRACUNIT;
}

// Staff puff 2 -------------------------------------------------------------

class AStaffPuff2 : public AStaffPuff
{
	DECLARE_ACTOR (AStaffPuff2, AStaffPuff)
public:
	void BeginPlay ();
};

FState AStaffPuff2::States[] =
{
	S_BRIGHT (PUF4, 'A',	4, NULL 						, &States[1]),
	S_BRIGHT (PUF4, 'B',	4, NULL 						, &States[2]),
	S_BRIGHT (PUF4, 'C',	4, NULL 						, &States[3]),
	S_BRIGHT (PUF4, 'D',	4, NULL 						, &States[4]),
	S_BRIGHT (PUF4, 'E',	4, NULL 						, &States[5]),
	S_BRIGHT (PUF4, 'F',	4, NULL 						, NULL)
};

IMPLEMENT_ACTOR (AStaffPuff2, Heretic, -1, 0)
	PROP_SpawnState (0)
	PROP_RenderStyle (STYLE_Add)
	PROP_Alpha (OPAQUE)
	PROP_AttackSound ("weapons/staffpowerhit")
END_DEFAULTS

void AStaffPuff2::BeginPlay ()
{
	Super::BeginPlay ();
	momz = 0;
}

//----------------------------------------------------------------------------
//
// PROC A_StaffAttackPL1
//
//----------------------------------------------------------------------------

void A_StaffAttackPL1 (AActor *actor)
{
	angle_t angle;
	int damage;
	int slope;
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
	damage = 5+(pr_sap()&15);
	angle = player->mo->angle;
	angle += pr_sap.Random2() << 18;
	slope = P_AimLineAttack (player->mo, angle, MELEERANGE);
	P_LineAttack (player->mo, angle, MELEERANGE, slope, damage, NAME_Melee, RUNTIME_CLASS(AStaffPuff), true);
	if (linetarget)
	{
		//S_StartSound(player->mo, sfx_stfhit);
		// turn to face target
		player->mo->angle = R_PointToAngle2 (player->mo->x,
			player->mo->y, linetarget->x, linetarget->y);
	}
}

//----------------------------------------------------------------------------
//
// PROC A_StaffAttackPL2
//
//----------------------------------------------------------------------------

void A_StaffAttackPL2 (AActor *actor)
{
	angle_t angle;
	int damage;
	int slope;
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
	// P_inter.c:P_DamageMobj() handles target momentums
	damage = 18+(pr_sap2()&63);
	angle = player->mo->angle;
	angle += pr_sap2.Random2() << 18;
	slope = P_AimLineAttack (player->mo, angle, MELEERANGE);
	P_LineAttack (player->mo, angle, MELEERANGE, slope, damage, NAME_Melee, RUNTIME_CLASS(AStaffPuff2), true);
	if (linetarget)
	{
		//S_StartSound(player->mo, sfx_stfpow);
		// turn to face target
		player->mo->angle = R_PointToAngle2 (player->mo->x,
			player->mo->y, linetarget->x, linetarget->y);
	}
}

// --- Gold wand ------------------------------------------------------------

void A_FireGoldWandPL1 (AActor *);
void A_FireGoldWandPL2 (AActor *);

// Gold wand ----------------------------------------------------------------

class AGoldWand : public AHereticWeapon
{
	DECLARE_ACTOR (AGoldWand, AHereticWeapon)
};

class AGoldWandPowered : public AGoldWand
{
	DECLARE_STATELESS_ACTOR (AGoldWandPowered, AGoldWand)
};

FState AGoldWand::States[] =
{
#define S_GOLDWANDREADY 0
	S_NORMAL (GWND, 'A',	1, A_WeaponReady				, &States[S_GOLDWANDREADY]),

#define S_GOLDWANDDOWN (S_GOLDWANDREADY+1)
	S_NORMAL (GWND, 'A',	1, A_Lower						, &States[S_GOLDWANDDOWN]),

#define S_GOLDWANDUP (S_GOLDWANDDOWN+1)
	S_NORMAL (GWND, 'A',	1, A_Raise						, &States[S_GOLDWANDUP]),

#define S_GOLDWANDATK1 (S_GOLDWANDUP+1)
	S_NORMAL (GWND, 'B',	3, NULL 						, &States[S_GOLDWANDATK1+1]),
	S_NORMAL (GWND, 'C',	5, A_FireGoldWandPL1			, &States[S_GOLDWANDATK1+2]),
	S_NORMAL (GWND, 'D',	3, NULL 						, &States[S_GOLDWANDATK1+3]),
	S_NORMAL (GWND, 'D',	0, A_ReFire 					, &States[S_GOLDWANDREADY]),

#define S_GOLDWANDATK2 (S_GOLDWANDATK1+4)
	S_NORMAL (GWND, 'B',	3, NULL 						, &States[S_GOLDWANDATK2+1]),
	S_NORMAL (GWND, 'C',	4, A_FireGoldWandPL2			, &States[S_GOLDWANDATK2+2]),
	S_NORMAL (GWND, 'D',	3, NULL 						, &States[S_GOLDWANDATK2+3]),
	S_NORMAL (GWND, 'D',	0, A_ReFire 					, &States[S_GOLDWANDREADY])
};

IMPLEMENT_ACTOR (AGoldWand, Heretic, -1, 0)
	PROP_Flags5 (MF5_BLOODSPLATTER)
	PROP_Weapon_SelectionOrder (2000)
	PROP_Weapon_AmmoUse1 (USE_GWND_AMMO_1)
	PROP_Weapon_AmmoGive1 (25)
	PROP_Weapon_UpState (S_GOLDWANDUP)
	PROP_Weapon_DownState (S_GOLDWANDDOWN)
	PROP_Weapon_ReadyState (S_GOLDWANDREADY)
	PROP_Weapon_AtkState (S_GOLDWANDATK1)
	PROP_Weapon_YAdjust (5)
	PROP_Weapon_MoveCombatDist (25000000)
	PROP_Weapon_AmmoType1 ("GoldWandAmmo")
	PROP_Weapon_SisterType ("GoldWandPowered")
END_DEFAULTS

IMPLEMENT_STATELESS_ACTOR (AGoldWandPowered, Heretic, -1, 0)
	PROP_Weapon_Flags (WIF_POWERED_UP)
	PROP_Weapon_AmmoUse1 (USE_GWND_AMMO_2)
	PROP_Weapon_AmmoGive1 (0)
	PROP_Weapon_AtkState (S_GOLDWANDATK2)
	PROP_Weapon_SisterType ("GoldWand")
END_DEFAULTS

// Gold wand FX1 ------------------------------------------------------------

class AGoldWandFX1 : public AActor
{
	DECLARE_ACTOR (AGoldWandFX1, AActor)
};

FState AGoldWandFX1::States[] =
{
#define S_GWANDFX1 0
	S_BRIGHT (FX01, 'A',	6, NULL 						, &States[S_GWANDFX1+1]),
	S_BRIGHT (FX01, 'B',	6, NULL 						, &States[S_GWANDFX1+0]),

#define S_GWANDFXI1 (S_GWANDFX1+2)
	S_BRIGHT (FX01, 'E',	3, NULL 						, &States[S_GWANDFXI1+1]),
	S_BRIGHT (FX01, 'F',	3, NULL 						, &States[S_GWANDFXI1+2]),
	S_BRIGHT (FX01, 'G',	3, NULL 						, &States[S_GWANDFXI1+3]),
	S_BRIGHT (FX01, 'H',	3, NULL 						, NULL)
};

IMPLEMENT_ACTOR (AGoldWandFX1, Heretic, -1, 151)
	PROP_RadiusFixed (10)
	PROP_HeightFixed (6)
	PROP_SpeedFixed (22)
	PROP_Damage (2)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF|MF_NOGRAVITY)
	PROP_Flags2 (MF2_NOTELEPORT)
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (S_GWANDFX1)
	PROP_DeathState (S_GWANDFXI1)

	PROP_DeathSound ("weapons/wandhit")
END_DEFAULTS

// Gold wand FX2 ------------------------------------------------------------

class AGoldWandFX2 : public AGoldWandFX1
{
	DECLARE_ACTOR (AGoldWandFX2, AGoldWandFX1)
};

FState AGoldWandFX2::States[] =
{
	S_BRIGHT (FX01, 'C',	6, NULL 						, &States[1]),
	S_BRIGHT (FX01, 'D',	6, NULL 						, &States[0])
};

IMPLEMENT_ACTOR (AGoldWandFX2, Heretic, -1, 152)
	PROP_SpeedFixed (18)
	PROP_Damage (1)

	PROP_SpawnState (0)

	PROP_DeathSound ("")
END_DEFAULTS

// Gold wand puff 1 ---------------------------------------------------------

class AGoldWandPuff1 : public AActor
{
	DECLARE_ACTOR (AGoldWandPuff1, AActor)
};

FState AGoldWandPuff1::States[] =
{
	S_BRIGHT (PUF2, 'A',	3, NULL 						, &States[1]),
	S_BRIGHT (PUF2, 'B',	3, NULL 						, &States[2]),
	S_BRIGHT (PUF2, 'C',	3, NULL 						, &States[3]),
	S_BRIGHT (PUF2, 'D',	3, NULL 						, &States[4]),
	S_BRIGHT (PUF2, 'E',	3, NULL 						, NULL)
};

IMPLEMENT_ACTOR (AGoldWandPuff1, Heretic, -1, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_Flags3 (MF3_PUFFONACTORS)
	PROP_SpawnState (0)
	PROP_RenderStyle (STYLE_Add)
END_DEFAULTS

// Gold wand puff 2 ---------------------------------------------------------

class AGoldWandPuff2 : public AGoldWandFX1
{
	DECLARE_STATELESS_ACTOR (AGoldWandPuff2, AGoldWandFX1)
};

IMPLEMENT_STATELESS_ACTOR (AGoldWandPuff2, Heretic, -1, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_SpawnState (S_GWANDFXI1)
END_DEFAULTS

//----------------------------------------------------------------------------
//
// PROC A_FireGoldWandPL1
//
//----------------------------------------------------------------------------

void A_FireGoldWandPL1 (AActor *actor)
{
	AActor *mo;
	angle_t angle;
	int damage;
	player_t *player;

	if (NULL == (player = actor->player))
	{
		return;
	}

	mo = player->mo;
	AWeapon *weapon = actor->player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
	}
	P_BulletSlope(mo);
	damage = 7+(pr_fgw()&7);
	angle = mo->angle;
	if (player->refire)
	{
		angle += pr_fgw.Random2() << 18;
	}
	P_LineAttack (mo, angle, PLAYERMISSILERANGE, bulletpitch, damage, NAME_None, RUNTIME_CLASS(AGoldWandPuff1));
	S_Sound (player->mo, CHAN_WEAPON, "weapons/wandhit", 1, ATTN_NORM);
}

//----------------------------------------------------------------------------
//
// PROC A_FireGoldWandPL2
//
//----------------------------------------------------------------------------

void A_FireGoldWandPL2 (AActor *actor)
{
	int i;
	AActor *mo;
	angle_t angle;
	int damage;
	fixed_t momz;
	player_t *player;

	if (NULL == (player = actor->player))
	{
		return;
	}

	mo = player->mo;
	AWeapon *weapon = actor->player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
	}
	P_BulletSlope (mo);
	momz = FixedMul (GetDefault<AGoldWandFX2>()->Speed,
		finetangent[FINEANGLES/4-((signed)bulletpitch>>ANGLETOFINESHIFT)]);
	P_SpawnMissileAngle (mo, RUNTIME_CLASS(AGoldWandFX2), mo->angle-(ANG45/8), momz);
	P_SpawnMissileAngle (mo, RUNTIME_CLASS(AGoldWandFX2), mo->angle+(ANG45/8), momz);
	angle = mo->angle-(ANG45/8);
	for(i = 0; i < 5; i++)
	{
		damage = 1+(pr_fgw2()&7);
		P_LineAttack (mo, angle, PLAYERMISSILERANGE, bulletpitch, damage, NAME_None, RUNTIME_CLASS(AGoldWandPuff2));
		angle += ((ANG45/8)*2)/4;
	}
	S_Sound (player->mo, CHAN_WEAPON, "weapons/wandhit", 1, ATTN_NORM);
}

// --- Crossbow -------------------------------------------------------------

void A_FireCrossbowPL1 (AActor *);
void A_FireCrossbowPL2 (AActor *);
void A_BoltSpark (AActor *);

// Crossbow -----------------------------------------------------------------

class ACrossbow : public AHereticWeapon
{
	DECLARE_ACTOR (ACrossbow, AHereticWeapon)
};

class ACrossbowPowered : public ACrossbow
{
	DECLARE_STATELESS_ACTOR (ACrossbowPowered, ACrossbow)
};

FState ACrossbow::States[] =
{
#define S_WBOW 0
	S_NORMAL (WBOW, 'A',   -1, NULL 						, NULL),

#define S_CRBOW (S_WBOW+1)
	S_NORMAL (CRBW, 'A',	1, A_WeaponReady				, &States[S_CRBOW+1]),
	S_NORMAL (CRBW, 'A',	1, A_WeaponReady				, &States[S_CRBOW+2]),
	S_NORMAL (CRBW, 'A',	1, A_WeaponReady				, &States[S_CRBOW+3]),
	S_NORMAL (CRBW, 'A',	1, A_WeaponReady				, &States[S_CRBOW+4]),
	S_NORMAL (CRBW, 'A',	1, A_WeaponReady				, &States[S_CRBOW+5]),
	S_NORMAL (CRBW, 'A',	1, A_WeaponReady				, &States[S_CRBOW+6]),
	S_NORMAL (CRBW, 'B',	1, A_WeaponReady				, &States[S_CRBOW+7]),
	S_NORMAL (CRBW, 'B',	1, A_WeaponReady				, &States[S_CRBOW+8]),
	S_NORMAL (CRBW, 'B',	1, A_WeaponReady				, &States[S_CRBOW+9]),
	S_NORMAL (CRBW, 'B',	1, A_WeaponReady				, &States[S_CRBOW+10]),
	S_NORMAL (CRBW, 'B',	1, A_WeaponReady				, &States[S_CRBOW+11]),
	S_NORMAL (CRBW, 'B',	1, A_WeaponReady				, &States[S_CRBOW+12]),
	S_NORMAL (CRBW, 'C',	1, A_WeaponReady				, &States[S_CRBOW+13]),
	S_NORMAL (CRBW, 'C',	1, A_WeaponReady				, &States[S_CRBOW+14]),
	S_NORMAL (CRBW, 'C',	1, A_WeaponReady				, &States[S_CRBOW+15]),
	S_NORMAL (CRBW, 'C',	1, A_WeaponReady				, &States[S_CRBOW+16]),
	S_NORMAL (CRBW, 'C',	1, A_WeaponReady				, &States[S_CRBOW+17]),
	S_NORMAL (CRBW, 'C',	1, A_WeaponReady				, &States[S_CRBOW+0]),

#define S_CRBOWDOWN (S_CRBOW+18)
	S_NORMAL (CRBW, 'A',	1, A_Lower						, &States[S_CRBOWDOWN]),

#define S_CRBOWUP (S_CRBOWDOWN+1)
	S_NORMAL (CRBW, 'A',	1, A_Raise						, &States[S_CRBOWUP]),

#define S_CRBOWATK1 (S_CRBOWUP+1)
	S_NORMAL (CRBW, 'D',	6, A_FireCrossbowPL1			, &States[S_CRBOWATK1+1]),
	S_NORMAL (CRBW, 'E',	3, NULL 						, &States[S_CRBOWATK1+2]),
	S_NORMAL (CRBW, 'F',	3, NULL 						, &States[S_CRBOWATK1+3]),
	S_NORMAL (CRBW, 'G',	3, NULL 						, &States[S_CRBOWATK1+4]),
	S_NORMAL (CRBW, 'H',	3, NULL 						, &States[S_CRBOWATK1+5]),
	S_NORMAL (CRBW, 'A',	4, NULL 						, &States[S_CRBOWATK1+6]),
	S_NORMAL (CRBW, 'B',	4, NULL 						, &States[S_CRBOWATK1+7]),
	S_NORMAL (CRBW, 'C',	5, A_ReFire 					, &States[S_CRBOW+0]),

#define S_CRBOWATK2 (S_CRBOWATK1+8)
	S_NORMAL (CRBW, 'D',	5, A_FireCrossbowPL2			, &States[S_CRBOWATK2+1]),
	S_NORMAL (CRBW, 'E',	3, NULL 						, &States[S_CRBOWATK2+2]),
	S_NORMAL (CRBW, 'F',	2, NULL 						, &States[S_CRBOWATK2+3]),
	S_NORMAL (CRBW, 'G',	3, NULL 						, &States[S_CRBOWATK2+4]),
	S_NORMAL (CRBW, 'H',	2, NULL 						, &States[S_CRBOWATK2+5]),
	S_NORMAL (CRBW, 'A',	3, NULL 						, &States[S_CRBOWATK2+6]),
	S_NORMAL (CRBW, 'B',	3, NULL 						, &States[S_CRBOWATK2+7]),
	S_NORMAL (CRBW, 'C',	4, A_ReFire 					, &States[S_CRBOW+0])
};

IMPLEMENT_ACTOR (ACrossbow, Heretic, 2001, 27)
	PROP_Flags (MF_SPECIAL)
	PROP_SpawnState (S_WBOW)

	PROP_Weapon_SelectionOrder (800)
	PROP_Weapon_AmmoUse1 (USE_CBOW_AMMO_1)
	PROP_Weapon_AmmoGive1 (10)
	PROP_Weapon_UpState (S_CRBOWUP)
	PROP_Weapon_DownState (S_CRBOWDOWN)
	PROP_Weapon_ReadyState (S_CRBOW)
	PROP_Weapon_AtkState (S_CRBOWATK1)
	PROP_Weapon_YAdjust (15)
	PROP_Weapon_MoveCombatDist (24000000)
	PROP_Weapon_AmmoType1 ("CrossbowAmmo")
	PROP_Weapon_SisterType ("CrossbowPowered")
	PROP_Weapon_ProjectileType ("CrossbowFX1")
	PROP_Inventory_PickupMessage("$TXT_WPNCROSSBOW")
END_DEFAULTS

IMPLEMENT_STATELESS_ACTOR (ACrossbowPowered, Heretic, -1, 0)
	PROP_Weapon_Flags (WIF_POWERED_UP)
	PROP_Weapon_AmmoUse1 (USE_CBOW_AMMO_2)
	PROP_Weapon_AmmoGive1 (0)
	PROP_Weapon_AtkState (S_CRBOWATK2)
	PROP_Weapon_SisterType ("Crossbow")
	PROP_Weapon_ProjectileType ("CrossbowFX2")
END_DEFAULTS

// Crossbow FX1 -------------------------------------------------------------

class ACrossbowFX1 : public AActor
{
	DECLARE_ACTOR (ACrossbowFX1, AActor)
};

FState ACrossbowFX1::States[] =
{
#define S_CRBOWFX1 0
	S_BRIGHT (FX03, 'B',	1, NULL 						, &States[S_CRBOWFX1]),

#define S_CRBOWFXI1 (S_CRBOWFX1+1)
	S_BRIGHT (FX03, 'H',	8, NULL 						, &States[S_CRBOWFXI1+1]),
	S_BRIGHT (FX03, 'I',	8, NULL 						, &States[S_CRBOWFXI1+2]),
	S_BRIGHT (FX03, 'J',	8, NULL 						, NULL)
};

IMPLEMENT_ACTOR (ACrossbowFX1, Heretic, -1, 147)
	PROP_RadiusFixed (11)
	PROP_HeightFixed (8)
	PROP_SpeedFixed (30)
	PROP_Damage (10)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF|MF_NOGRAVITY)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_PCROSS|MF2_IMPACT)
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (S_CRBOWFX1)
	PROP_DeathState (S_CRBOWFXI1)

	PROP_SeeSound ("weapons/bowshoot")
	PROP_DeathSound ("weapons/bowhit")
END_DEFAULTS

// Crossbow FX2 -------------------------------------------------------------

class ACrossbowFX2 : public ACrossbowFX1
{
	DECLARE_ACTOR (ACrossbowFX2, ACrossbowFX1)
};

FState ACrossbowFX2::States[] =
{
#define S_CRBOWFX2 0
	S_BRIGHT (FX03, 'B',	1, A_BoltSpark					, &States[S_CRBOWFX2])
};

IMPLEMENT_ACTOR (ACrossbowFX2, Heretic, -1, 148)
	PROP_SpeedFixed (32)
	PROP_Damage (6)
	PROP_RenderStyle (STYLE_Add)
	PROP_SpawnState (S_CRBOWFX2)
END_DEFAULTS

// Crossbow FX3 -------------------------------------------------------------

class ACrossbowFX3 : public ACrossbowFX1
{
	DECLARE_ACTOR (ACrossbowFX3, ACrossbowFX1)
};

FState ACrossbowFX3::States[] =
{
#define S_CRBOWFX3 0
	S_BRIGHT (FX03, 'A',	1, NULL 						, &States[S_CRBOWFX3]),

#define S_CRBOWFXI3 (S_CRBOWFX3+1)
	S_BRIGHT (FX03, 'C',	8, NULL 						, &States[S_CRBOWFXI3+1]),
	S_BRIGHT (FX03, 'D',	8, NULL 						, &States[S_CRBOWFXI3+2]),
	S_BRIGHT (FX03, 'E',	8, NULL 						, NULL)
};

IMPLEMENT_ACTOR (ACrossbowFX3, Heretic, -1, 149)
	PROP_SpeedFixed (20)
	PROP_Damage (2)
	PROP_FlagsClear (MF_NOBLOCKMAP)
	PROP_Flags2 (MF2_WINDTHRUST|MF2_THRUGHOST|MF2_NOTELEPORT|MF2_PCROSS|MF2_IMPACT)
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (S_CRBOWFX3)
	PROP_DeathState (S_CRBOWFXI3)

	PROP_SeeSound ("")
END_DEFAULTS

// Crossbow FX4 -------------------------------------------------------------

class ACrossbowFX4 : public AActor
{
	DECLARE_ACTOR (ACrossbowFX4, AActor)
};

FState ACrossbowFX4::States[] =
{
#define S_CRBOWFX4 0
	S_BRIGHT (FX03, 'F',	8, NULL 						, &States[S_CRBOWFX4+1]),
	S_BRIGHT (FX03, 'G',	8, NULL 						, NULL)
};

IMPLEMENT_ACTOR (ACrossbowFX4, Heretic, -1, 0)
	PROP_Flags (MF_NOBLOCKMAP)
	PROP_Gravity (FRACUNIT/8)
	PROP_RenderStyle (STYLE_Add)
	PROP_SpawnState (S_CRBOWFX4)
END_DEFAULTS

//----------------------------------------------------------------------------
//
// PROC A_FireCrossbowPL1
//
//----------------------------------------------------------------------------

void A_FireCrossbowPL1 (AActor *actor)
{
	AActor *pmo;
	player_t *player;

	if (NULL == (player = actor->player))
	{
		return;
	}

	pmo = player->mo;
	AWeapon *weapon = actor->player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
	}
	P_SpawnPlayerMissile (pmo, RUNTIME_CLASS(ACrossbowFX1));
	P_SpawnPlayerMissile (pmo, RUNTIME_CLASS(ACrossbowFX3), pmo->angle-(ANG45/10));
	P_SpawnPlayerMissile (pmo, RUNTIME_CLASS(ACrossbowFX3), pmo->angle+(ANG45/10));
}

//----------------------------------------------------------------------------
//
// PROC A_FireCrossbowPL2
//
//----------------------------------------------------------------------------

void A_FireCrossbowPL2(AActor *actor)
{
	AActor *pmo;
	player_t *player;

	if (NULL == (player = actor->player))
	{
		return;
	}

	pmo = player->mo;
	AWeapon *weapon = actor->player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
	}
	P_SpawnPlayerMissile (pmo, RUNTIME_CLASS(ACrossbowFX2));
	P_SpawnPlayerMissile (pmo, RUNTIME_CLASS(ACrossbowFX2), pmo->angle-(ANG45/10));
	P_SpawnPlayerMissile (pmo, RUNTIME_CLASS(ACrossbowFX2), pmo->angle+(ANG45/10));
	P_SpawnPlayerMissile (pmo, RUNTIME_CLASS(ACrossbowFX3), pmo->angle-(ANG45/5));
	P_SpawnPlayerMissile (pmo, RUNTIME_CLASS(ACrossbowFX3), pmo->angle+(ANG45/5));
}

//----------------------------------------------------------------------------
//
// PROC A_BoltSpark
//
//----------------------------------------------------------------------------

void A_BoltSpark (AActor *bolt)
{
	AActor *spark;

	if (pr_boltspark() > 50)
	{
		spark = Spawn<ACrossbowFX4> (bolt->x, bolt->y, bolt->z, ALLOW_REPLACE);
		spark->x += pr_boltspark.Random2() << 10;
		spark->y += pr_boltspark.Random2() << 10;
	}
}

// --- Mace -----------------------------------------------------------------

#define MAGIC_JUNK 1234

void A_FireMacePL1B (AActor *);
void A_FireMacePL1 (AActor *);
void A_MacePL1Check (AActor *);
void A_MaceBallImpact (AActor *);
void A_MaceBallImpact2 (AActor *);
void A_FireMacePL2 (AActor *);
void A_DeathBallImpact (AActor *);

// The mace itself ----------------------------------------------------------

class AMace : public AHereticWeapon
{
	DECLARE_ACTOR (AMace, AHereticWeapon)
	HAS_OBJECT_POINTERS
public:
	void Serialize (FArchive &arc);
protected:
	bool DoRespawn ();
	int NumMaceSpots;
	TObjPtr<AActor> FirstSpot;
private:

	friend void A_SpawnMace (AActor *self);
};

class AMacePowered : public AMace
{
	DECLARE_STATELESS_ACTOR (AMacePowered, AMace)
};

IMPLEMENT_POINTY_CLASS (AMace)
 DECLARE_POINTER (FirstSpot)
END_POINTERS

void AMace::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << NumMaceSpots << FirstSpot;
}

FState AMace::States[] =
{
#define S_WMCE 0
	S_NORMAL (WMCE, 'A',   -1, NULL 				, NULL),

#define S_MACEREADY (S_WMCE+1)
	S_NORMAL (MACE, 'A',	1, A_WeaponReady		, &States[S_MACEREADY]),

#define S_MACEDOWN (S_MACEREADY+1)
	S_NORMAL (MACE, 'A',	1, A_Lower				, &States[S_MACEDOWN]),

#define S_MACEUP (S_MACEDOWN+1)
	S_NORMAL (MACE, 'A',	1, A_Raise				, &States[S_MACEUP]),

#define S_MACEATK1 (S_MACEUP+1)
	S_NORMAL (MACE, 'B',	4, NULL 				, &States[S_MACEATK1+1]),
	S_NORMAL (MACE, 'C',	3, A_FireMacePL1		, &States[S_MACEATK1+2]),
	S_NORMAL (MACE, 'D',	3, A_FireMacePL1		, &States[S_MACEATK1+3]),
	S_NORMAL (MACE, 'E',	3, A_FireMacePL1		, &States[S_MACEATK1+4]),
	S_NORMAL (MACE, 'F',	3, A_FireMacePL1		, &States[S_MACEATK1+5]),
	S_NORMAL (MACE, 'C',	4, A_ReFire 			, &States[S_MACEATK1+6]),
	S_NORMAL (MACE, 'D',	4, NULL 				, &States[S_MACEATK1+7]),
	S_NORMAL (MACE, 'E',	4, NULL 				, &States[S_MACEATK1+8]),
	S_NORMAL (MACE, 'F',	4, NULL 				, &States[S_MACEATK1+9]),
	S_NORMAL (MACE, 'B',	4, NULL 				, &States[S_MACEREADY]),

#define S_MACEATK2 (S_MACEATK1+10)
	S_NORMAL (MACE, 'B',	4, NULL 				, &States[S_MACEATK2+1]),
	S_NORMAL (MACE, 'D',	4, A_FireMacePL2		, &States[S_MACEATK2+2]),
	S_NORMAL (MACE, 'B',	4, NULL 				, &States[S_MACEATK2+3]),
	S_NORMAL (MACE, 'A',	8, A_ReFire 			, &States[S_MACEREADY])
};

BEGIN_DEFAULTS (AMace, Heretic, -1, 0)
	PROP_Flags (MF_SPECIAL)
	PROP_SpawnState (0)

	PROP_Weapon_SelectionOrder (1400)
	PROP_Weapon_Flags (WIF_BOT_REACTION_SKILL_THING|WIF_BOT_EXPLOSIVE)
	PROP_Weapon_AmmoUse1 (USE_MACE_AMMO_1)
	PROP_Weapon_AmmoGive1 (50)
	PROP_Weapon_UpState (S_MACEUP)
	PROP_Weapon_DownState (S_MACEDOWN)
	PROP_Weapon_ReadyState (S_MACEREADY)
	PROP_Weapon_AtkState (S_MACEATK1)
	PROP_Weapon_HoldAtkState (S_MACEATK1+1)
	PROP_Weapon_YAdjust (15)
	PROP_Weapon_MoveCombatDist (27000000)
	PROP_Weapon_AmmoType1 ("MaceAmmo")
	PROP_Weapon_SisterType ("MacePowered")
	PROP_Weapon_ProjectileType ("MaceFX2")
	PROP_Inventory_PickupMessage("$TXT_WPNMACE")
END_DEFAULTS

IMPLEMENT_STATELESS_ACTOR (AMacePowered, Heretic, -1, 0)
	PROP_Weapon_Flags (WIF_POWERED_UP|WIF_BOT_REACTION_SKILL_THING|WIF_BOT_EXPLOSIVE)
	PROP_Weapon_AmmoUse1 (USE_MACE_AMMO_2)
	PROP_Weapon_AmmoGive1 (0)
	PROP_Weapon_AtkState (S_MACEATK2)
	PROP_Weapon_HoldAtkState (S_MACEATK2)
	PROP_Weapon_SisterType ("Mace")
	PROP_Weapon_ProjectileType ("MaceFX4")
END_DEFAULTS

// Mace FX1 -----------------------------------------------------------------

class AMaceFX1 : public AActor
{
	DECLARE_ACTOR (AMaceFX1, AActor)
};

FState AMaceFX1::States[] =
{
#define S_MACEFX1 0
	S_NORMAL (FX02, 'A',	4, A_MacePL1Check		, &States[S_MACEFX1+1]),
	S_NORMAL (FX02, 'B',	4, A_MacePL1Check		, &States[S_MACEFX1+0]),

#define S_MACEFXI1 (S_MACEFX1+2)
	S_BRIGHT (FX02, 'F',	4, A_MaceBallImpact 	, &States[S_MACEFXI1+1]),
	S_BRIGHT (FX02, 'G',	4, NULL 				, &States[S_MACEFXI1+2]),
	S_BRIGHT (FX02, 'H',	4, NULL 				, &States[S_MACEFXI1+3]),
	S_BRIGHT (FX02, 'I',	4, NULL 				, &States[S_MACEFXI1+4]),
	S_BRIGHT (FX02, 'J',	4, NULL 				, NULL)
};

IMPLEMENT_ACTOR (AMaceFX1, Heretic, -1, 154)
	PROP_RadiusFixed (8)
	PROP_HeightFixed (6)
	PROP_SpeedFixed (20)
	PROP_Damage (2)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF|MF_NOGRAVITY)
	PROP_Flags2 (MF2_HERETICBOUNCE|MF2_THRUGHOST|MF2_NOTELEPORT|MF2_PCROSS|MF2_IMPACT)
	PROP_Flags3 (MF3_WARNBOT)

	PROP_SpawnState (S_MACEFX1)
	PROP_DeathState (S_MACEFXI1)

	PROP_SeeSound ("weapons/maceshoot")
END_DEFAULTS

// Mace FX2 -----------------------------------------------------------------

class AMaceFX2 : public AActor
{
	DECLARE_ACTOR (AMaceFX2, AActor)
};

FState AMaceFX2::States[] =
{
#define S_MACEFX2 0
	S_NORMAL (FX02, 'C',	4, NULL 				, &States[S_MACEFX2+1]),
	S_NORMAL (FX02, 'D',	4, NULL 				, &States[S_MACEFX2+0]),

#define S_MACEFXI2 (S_MACEFX2+2)
	S_BRIGHT (FX02, 'F',	4, A_MaceBallImpact2	, &AMaceFX1::States[S_MACEFXI1+1])
};

IMPLEMENT_ACTOR (AMaceFX2, Heretic, -1, 156)
	PROP_RadiusFixed (8)
	PROP_HeightFixed (6)
	PROP_SpeedFixed (10)
	PROP_Damage (6)
	PROP_Gravity (FRACUNIT/8)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF)
	PROP_Flags2 (MF2_HERETICBOUNCE|MF2_THRUGHOST|MF2_NOTELEPORT|MF2_PCROSS|MF2_IMPACT)

	PROP_SpawnState (S_MACEFX2)
	PROP_DeathState (S_MACEFXI2)
END_DEFAULTS

// Mace FX3 -----------------------------------------------------------------

class AMaceFX3 : public AMaceFX1
{
	DECLARE_ACTOR (AMaceFX3, AMaceFX1)
};

FState AMaceFX3::States[] =
{
#define S_MACEFX3 0
	S_NORMAL (FX02, 'A',	4, NULL 				, &States[S_MACEFX3+1]),
	S_NORMAL (FX02, 'B',	4, NULL 				, &States[S_MACEFX3+0])
};

IMPLEMENT_ACTOR (AMaceFX3, Heretic, -1, 155)
	PROP_SpeedFixed (7)
	PROP_Damage (4)
	PROP_Gravity (FRACUNIT/8)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF)
	PROP_Flags2 (MF2_HERETICBOUNCE|MF2_THRUGHOST|MF2_NOTELEPORT|MF2_PCROSS|MF2_IMPACT)

	PROP_SpawnState (S_MACEFX3)
END_DEFAULTS

// Mace FX4 -----------------------------------------------------------------

class AMaceFX4 : public AActor
{
	DECLARE_ACTOR (AMaceFX4, AActor)
public:
	int DoSpecialDamage (AActor *target, int damage);
};

FState AMaceFX4::States[] =
{
#define S_MACEFX4 0
	S_NORMAL (FX02, 'E',   99, NULL 				, &States[S_MACEFX4+0]),

#define S_MACEFXI4 (S_MACEFX4+1)
	S_BRIGHT (FX02, 'C',	4, A_DeathBallImpact	, &AMaceFX1::States[S_MACEFXI1+1])
};

IMPLEMENT_ACTOR (AMaceFX4, Heretic, -1, 153)
	PROP_RadiusFixed (8)
	PROP_HeightFixed (6)
	PROP_SpeedFixed (7)
	PROP_Damage (18)
	PROP_Gravity (FRACUNIT/8)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF)
	PROP_Flags2 (MF2_HERETICBOUNCE|MF2_THRUGHOST|MF2_TELESTOMP|MF2_PCROSS|MF2_IMPACT)

	PROP_SpawnState (S_MACEFX4)
	PROP_DeathState (S_MACEFXI4)
END_DEFAULTS

int AMaceFX4::DoSpecialDamage (AActor *target, int damage)
{
	if ((target->flags2 & MF2_BOSS) || (target->flags3 & MF3_DONTSQUASH) || target->IsTeammate (this->target))
	{ // Don't allow cheap boss kills and don't instagib teammates
		return damage;
	}
	else if (target->player)
	{ // Player specific checks
		if (target->player->mo->flags2 & MF2_INVULNERABLE)
		{ // Can't hurt invulnerable players
			return -1;
		}
		if (P_AutoUseChaosDevice (target->player))
		{ // Player was saved using chaos device
			return -1;
		}
	}
	return 1000000; // Something's gonna die
}

// Mace spawn spot ----------------------------------------------------------

void A_SpawnMace (AActor *);

class AMaceSpawner : public AActor
{
	DECLARE_ACTOR (AMaceSpawner, AActor)

	// Uses target to point to the next mace spawner in the list
};

FState AMaceSpawner::States[] =
{
	S_NORMAL (TNT1, 'A', 1, NULL, &States[1]),
	S_NORMAL (TNT1, 'A', -1, A_SpawnMace, NULL)
};

IMPLEMENT_ACTOR (AMaceSpawner, Heretic, 2002, 0)
	PROP_Flags (MF_NOSECTOR|MF_NOBLOCKMAP)
	PROP_SpawnState (0)
END_DEFAULTS

// Every mace spawn spot will execute this action. The first one
// will build a list of all mace spots in the level and spawn a
// mace. The rest of the spots will do nothing.

void A_SpawnMace (AActor *self)
{
	if (self->target != NULL)
	{ // Another spot already did it
		return;
	}

	TThinkerIterator<AMaceSpawner> iterator;
	AActor *spot;
	AMaceSpawner *firstSpot;
	AMace *mace;
	int numspots = 0;

	spot = firstSpot = iterator.Next ();
	while (spot != NULL)
	{
		numspots++;
		spot->target = iterator.Next ();
		if (spot->target == NULL)
		{
			spot->target = firstSpot;
			spot = NULL;
		}
		else
		{
			spot = spot->target;
		}
	}
	if (numspots == 0)
	{
		return;
	}
	if (!deathmatch && pr_spawnmace() < 64)
	{ // Sometimes doesn't show up if not in deathmatch
		return;
	}
	mace = Spawn<AMace> (self->x, self->y, self->z, ALLOW_REPLACE);
	if (mace)
	{
		mace->FirstSpot = firstSpot;
		mace->NumMaceSpots = numspots;
		mace->DoRespawn ();
		// We want this mace to respawn.
		mace->flags &= ~MF_DROPPED;
	}
}

// AMace::DoRespawn
// Moves the mace to a different spot when it respawns

bool AMace::DoRespawn ()
{
	if (NumMaceSpots > 0)
	{
		int spotnum = pr_macerespawn () % NumMaceSpots;
		AActor *spot = FirstSpot;

		while (spotnum > 0)
		{
			spot = spot->target;
			spotnum--;
		}

		SetOrigin (spot->x, spot->y, spot->z);
		z = floorz;
	}
	return true;
}

//----------------------------------------------------------------------------
//
// PROC A_FireMacePL1B
//
//----------------------------------------------------------------------------

void A_FireMacePL1B (AActor *actor)
{
	AActor *pmo;
	AActor *ball;
	angle_t angle;
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
	pmo = player->mo;
	ball = Spawn<AMaceFX2> (pmo->x, pmo->y, pmo->z + 28*FRACUNIT 
		- pmo->floorclip, ALLOW_REPLACE);
	ball->momz = 2*FRACUNIT+/*((player->lookdir)<<(FRACBITS-5))*/
		finetangent[FINEANGLES/4-(pmo->pitch>>ANGLETOFINESHIFT)];
	angle = pmo->angle;
	ball->target = pmo;
	ball->angle = angle;
	ball->z += 2*finetangent[FINEANGLES/4-(pmo->pitch>>ANGLETOFINESHIFT)];
	angle >>= ANGLETOFINESHIFT;
	ball->momx = (pmo->momx>>1)+FixedMul(ball->Speed, finecosine[angle]);
	ball->momy = (pmo->momy>>1)+FixedMul(ball->Speed, finesine[angle]);
	S_Sound (ball, CHAN_BODY, "weapons/maceshoot", 1, ATTN_NORM);
	P_CheckMissileSpawn (ball);
}

//----------------------------------------------------------------------------
//
// PROC A_FireMacePL1
//
//----------------------------------------------------------------------------

void A_FireMacePL1 (AActor *actor)
{
	AActor *ball;
	player_t *player;

	if (NULL == (player = actor->player))
	{
		return;
	}

	if (pr_maceatk() < 28)
	{
		A_FireMacePL1B (actor);
		return;
	}
	AWeapon *weapon = actor->player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
	}
	player->psprites[ps_weapon].sx = ((pr_maceatk()&3)-2)*FRACUNIT;
	player->psprites[ps_weapon].sy = WEAPONTOP+(pr_maceatk()&3)*FRACUNIT;
	ball = P_SpawnPlayerMissile (player->mo, RUNTIME_CLASS(AMaceFX1),
		player->mo->angle+(((pr_maceatk()&7)-4)<<24));
	if (ball)
	{
		ball->special1 = 16; // tics till dropoff
	}
}

//----------------------------------------------------------------------------
//
// PROC A_MacePL1Check
//
//----------------------------------------------------------------------------

void A_MacePL1Check (AActor *ball)
{
	if (ball->special1 == 0)
	{
		return;
	}
	ball->special1 -= 4;
	if (ball->special1 > 0)
	{
		return;
	}
	ball->special1 = 0;
	ball->flags &= ~MF_NOGRAVITY;
	ball->gravity = FRACUNIT/8;
	// [RH] Avoid some precision loss by scaling the momentum directly
#if 0
	angle_t angle = ball->angle>>ANGLETOFINESHIFT;
	ball->momx = FixedMul(7*FRACUNIT, finecosine[angle]);
	ball->momy = FixedMul(7*FRACUNIT, finesine[angle]);
#else
	float momscale = sqrtf ((float)ball->momx * (float)ball->momx +
							(float)ball->momy * (float)ball->momy);
	momscale = 458752.f / momscale;
	ball->momx = (int)(ball->momx * momscale);
	ball->momy = (int)(ball->momy * momscale);
#endif
	ball->momz -= ball->momz>>1;
}

//----------------------------------------------------------------------------
//
// PROC A_MaceBallImpact
//
//----------------------------------------------------------------------------

void A_MaceBallImpact (AActor *ball)
{
	if ((ball->health != MAGIC_JUNK) && (ball->flags & MF_INBOUNCE))
	{ // Bounce
		ball->health = MAGIC_JUNK;
		ball->momz = (ball->momz * 192) >> 8;
		ball->flags2 &= ~MF2_BOUNCETYPE;
		ball->SetState (ball->SpawnState);
		S_Sound (ball, CHAN_BODY, "weapons/macebounce", 1, ATTN_NORM);
	}
	else
	{ // Explode
		ball->momx = ball->momy = ball->momz = 0;
		ball->flags |= MF_NOGRAVITY;
		ball->gravity = FRACUNIT;
		S_Sound (ball, CHAN_BODY, "weapons/macehit", 1, ATTN_NORM);
	}
}

//----------------------------------------------------------------------------
//
// PROC A_MaceBallImpact2
//
//----------------------------------------------------------------------------

void A_MaceBallImpact2 (AActor *ball)
{
	AActor *tiny;
	angle_t angle;

	if (ball->flags & MF_INBOUNCE)
	{
		fixed_t floordist = ball->z - ball->floorz;
		fixed_t ceildist = ball->ceilingz - ball->z;
		fixed_t vel;

		if (floordist <= ceildist)
		{
			vel = MulScale32 (ball->momz, ball->Sector->floorplane.c);
		}
		else
		{
			vel = MulScale32 (ball->momz, ball->Sector->ceilingplane.c);
		}
		if (vel < 2)
		{
			goto boom;
		}

		// Bounce
		ball->momz = (ball->momz * 192) >> 8;
		ball->SetState (ball->SpawnState);

		tiny = Spawn<AMaceFX3> (ball->x, ball->y, ball->z, ALLOW_REPLACE);
		angle = ball->angle+ANG90;
		tiny->target = ball->target;
		tiny->angle = angle;
		angle >>= ANGLETOFINESHIFT;
		tiny->momx = (ball->momx>>1)+FixedMul(ball->momz-FRACUNIT,
			finecosine[angle]);
		tiny->momy = (ball->momy>>1)+FixedMul(ball->momz-FRACUNIT,
			finesine[angle]);
		tiny->momz = ball->momz;
		P_CheckMissileSpawn (tiny);

		tiny = Spawn<AMaceFX3> (ball->x, ball->y, ball->z, ALLOW_REPLACE);
		angle = ball->angle-ANG90;
		tiny->target = ball->target;
		tiny->angle = angle;
		angle >>= ANGLETOFINESHIFT;
		tiny->momx = (ball->momx>>1)+FixedMul(ball->momz-FRACUNIT,
			finecosine[angle]);
		tiny->momy = (ball->momy>>1)+FixedMul(ball->momz-FRACUNIT,
			finesine[angle]);
		tiny->momz = ball->momz;
		P_CheckMissileSpawn (tiny);
	}
	else
	{ // Explode
boom:
		ball->momx = ball->momy = ball->momz = 0;
		ball->flags |= MF_NOGRAVITY;
		ball->flags2 &= ~MF2_BOUNCETYPE;
		ball->gravity = FRACUNIT;
	}
}

//----------------------------------------------------------------------------
//
// PROC A_FireMacePL2
//
//----------------------------------------------------------------------------

void A_FireMacePL2 (AActor *actor)
{
	AActor *mo;
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
	mo = P_SpawnPlayerMissile (player->mo, RUNTIME_CLASS(AMaceFX4));
	if (mo)
	{
		mo->momx += player->mo->momx;
		mo->momy += player->mo->momy;
		mo->momz = 2*FRACUNIT+
			clamp<fixed_t>(finetangent[FINEANGLES/4-(player->mo->pitch>>ANGLETOFINESHIFT)], -5*FRACUNIT, 5*FRACUNIT);
		if (linetarget)
		{
			mo->tracer = linetarget;
		}
	}
	S_Sound (player->mo, CHAN_WEAPON, "weapons/maceshoot", 1, ATTN_NORM);
}

//----------------------------------------------------------------------------
//
// PROC A_DeathBallImpact
//
//----------------------------------------------------------------------------

void A_DeathBallImpact (AActor *ball)
{
	int i;
	AActor *target;
	angle_t angle = 0;
	bool newAngle;

	if ((ball->z <= ball->floorz) && P_HitFloor (ball))
	{ // Landed in some sort of liquid
		ball->Destroy ();
		return;
	}
	if (ball->flags & MF_INBOUNCE)
	{
		fixed_t floordist = ball->z - ball->floorz;
		fixed_t ceildist = ball->ceilingz - ball->z;
		fixed_t vel;

		if (floordist <= ceildist)
		{
			vel = MulScale32 (ball->momz, ball->Sector->floorplane.c);
		}
		else
		{
			vel = MulScale32 (ball->momz, ball->Sector->ceilingplane.c);
		}
		if (vel < 2)
		{
			goto boom;
		}

		// Bounce
		newAngle = false;
		target = ball->tracer;
		if (target)
		{
			if (!(target->flags&MF_SHOOTABLE))
			{ // Target died
				ball->tracer = NULL;
			}
			else
			{ // Seek
				angle = R_PointToAngle2(ball->x, ball->y,
					target->x, target->y);
				newAngle = true;
			}
		}
		else
		{ // Find new target
			angle = 0;
			for (i = 0; i < 16; i++)
			{
				P_AimLineAttack (ball, angle, 10*64*FRACUNIT);
				if (linetarget && ball->target != linetarget)
				{
					ball->tracer = linetarget;
					angle = R_PointToAngle2 (ball->x, ball->y,
						linetarget->x, linetarget->y);
					newAngle = true;
					break;
				}
				angle += ANGLE_45/2;
			}
		}
		if (newAngle)
		{
			ball->angle = angle;
			angle >>= ANGLETOFINESHIFT;
			ball->momx = FixedMul (ball->Speed, finecosine[angle]);
			ball->momy = FixedMul (ball->Speed, finesine[angle]);
		}
		ball->SetState (ball->SpawnState);
		S_Sound (ball, CHAN_BODY, "weapons/macestop", 1, ATTN_NORM);
	}
	else
	{ // Explode
boom:
		ball->momx = ball->momy = ball->momz = 0;
		ball->flags |= MF_NOGRAVITY;
		ball->gravity = FRACUNIT;
		S_Sound (ball, CHAN_BODY, "weapons/maceexplode", 1, ATTN_NORM);
	}
}

// --- Gauntlets ------------------------------------------------------------

void A_GauntletAttack (AActor *);
void A_GauntletSound (AActor *);

// Gauntlets ----------------------------------------------------------------

class AGauntlets : public AHereticWeapon
{
	DECLARE_ACTOR (AGauntlets, AHereticWeapon)
};

class AGauntletsPowered : public AGauntlets
{
	DECLARE_STATELESS_ACTOR (AGauntletsPowered, AGauntlets)
};

FState AGauntlets::States[] =
{
#define S_WGNT 0
	S_NORMAL (WGNT, 'A',   -1, NULL 					, NULL),

#define S_GAUNTLETREADY (S_WGNT+1)
	S_NORMAL (GAUN, 'A',	1, A_WeaponReady			, &States[S_GAUNTLETREADY]),

#define S_GAUNTLETDOWN (S_GAUNTLETREADY+1)
	S_NORMAL (GAUN, 'A',	1, A_Lower					, &States[S_GAUNTLETDOWN]),

#define S_GAUNTLETUP (S_GAUNTLETDOWN+1)
	S_NORMAL (GAUN, 'A',	1, A_Raise					, &States[S_GAUNTLETUP]),

#define S_GAUNTLETREADY2 (S_GAUNTLETUP+1)
	S_NORMAL (GAUN, 'G',	4, A_WeaponReady			, &States[S_GAUNTLETREADY2+1]),
	S_NORMAL (GAUN, 'H',	4, A_WeaponReady			, &States[S_GAUNTLETREADY2+2]),
	S_NORMAL (GAUN, 'I',	4, A_WeaponReady			, &States[S_GAUNTLETREADY2+0]),

#define S_GAUNTLETDOWN2 (S_GAUNTLETREADY2+3)
	S_NORMAL (GAUN, 'G',	1, A_Lower					, &States[S_GAUNTLETDOWN2]),

#define S_GAUNTLETUP2 (S_GAUNTLETDOWN2+1)
	S_NORMAL (GAUN, 'G',	1, A_Raise					, &States[S_GAUNTLETUP2]),

#define S_GAUNTLETATK1 (S_GAUNTLETUP2+1)
	S_NORMAL (GAUN, 'B',	4, A_GauntletSound			, &States[S_GAUNTLETATK1+1]),
	S_NORMAL (GAUN, 'C',	4, NULL 					, &States[S_GAUNTLETATK1+2]),
	S_BRIGHT (GAUN, 'D',	4, A_GauntletAttack 		, &States[S_GAUNTLETATK1+3]),
	S_BRIGHT (GAUN, 'E',	4, A_GauntletAttack 		, &States[S_GAUNTLETATK1+4]),
	S_BRIGHT (GAUN, 'F',	4, A_GauntletAttack 		, &States[S_GAUNTLETATK1+5]),
	S_NORMAL (GAUN, 'C',	4, A_ReFire 				, &States[S_GAUNTLETATK1+6]),
	S_NORMAL (GAUN, 'B',	4, A_Light0 				, &States[S_GAUNTLETREADY]),

#define S_GAUNTLETATK2 (S_GAUNTLETATK1+7)
	S_NORMAL (GAUN, 'J',	4, A_GauntletSound			, &States[S_GAUNTLETATK2+1]),
	S_NORMAL (GAUN, 'K',	4, NULL 					, &States[S_GAUNTLETATK2+2]),
	S_BRIGHT (GAUN, 'L',	4, A_GauntletAttack 		, &States[S_GAUNTLETATK2+3]),
	S_BRIGHT (GAUN, 'M',	4, A_GauntletAttack 		, &States[S_GAUNTLETATK2+4]),
	S_BRIGHT (GAUN, 'N',	4, A_GauntletAttack 		, &States[S_GAUNTLETATK2+5]),
	S_NORMAL (GAUN, 'K',	4, A_ReFire 				, &States[S_GAUNTLETATK2+6]),
	S_NORMAL (GAUN, 'J',	4, A_Light0 				, &States[S_GAUNTLETREADY2+0])
};

IMPLEMENT_ACTOR (AGauntlets, Heretic, 2005, 32)
	PROP_Flags (MF_SPECIAL)
	PROP_Flags5 (MF5_BLOODSPLATTER)
	PROP_SpawnState (S_WGNT)

	PROP_Weapon_SelectionOrder (2300)
	PROP_Weapon_Flags (WIF_WIMPY_WEAPON|WIF_BOT_MELEE)
	PROP_Weapon_UpState (S_GAUNTLETUP)
	PROP_Weapon_DownState (S_GAUNTLETDOWN)
	PROP_Weapon_ReadyState (S_GAUNTLETREADY)
	PROP_Weapon_AtkState (S_GAUNTLETATK1)
	PROP_Weapon_HoldAtkState (S_GAUNTLETATK1+2)
	PROP_Weapon_Kickback (0)
	PROP_Weapon_YAdjust (15)
	PROP_Weapon_UpSound ("weapons/gauntletsactivate")
	PROP_Weapon_SisterType ("GauntletsPowered")
	PROP_Inventory_PickupMessage("$TXT_WPNGAUNTLETS")
END_DEFAULTS

IMPLEMENT_STATELESS_ACTOR (AGauntletsPowered, Heretic, -1, 0)
	PROP_Weapon_Flags (WIF_WIMPY_WEAPON|WIF_POWERED_UP|WIF_BOT_MELEE)
	PROP_Weapon_UpState (S_GAUNTLETUP2)
	PROP_Weapon_DownState (S_GAUNTLETDOWN2)
	PROP_Weapon_ReadyState (S_GAUNTLETREADY2)
	PROP_Weapon_AtkState (S_GAUNTLETATK2)
	PROP_Weapon_HoldAtkState (S_GAUNTLETATK2+2)
	PROP_Weapon_SisterType ("Gauntlets")
END_DEFAULTS

void A_GauntletSound (AActor *actor)
{
	// Play the sound for the initial gauntlet attack
	S_Sound (actor, CHAN_WEAPON, "weapons/gauntletsuse", 1, ATTN_NORM);
}

// Gauntlet puff 1 ----------------------------------------------------------

class AGauntletPuff1 : public AActor
{
	DECLARE_ACTOR (AGauntletPuff1, AActor)
public:
	void BeginPlay ();
};

FState AGauntletPuff1::States[] =
{
#define S_GAUNTLETPUFF1 0
	S_BRIGHT (PUF1, 'A',	4, NULL 					, &States[S_GAUNTLETPUFF1+1]),
	S_BRIGHT (PUF1, 'B',	4, NULL 					, &States[S_GAUNTLETPUFF1+2]),
	S_BRIGHT (PUF1, 'C',	4, NULL 					, &States[S_GAUNTLETPUFF1+3]),
	S_BRIGHT (PUF1, 'D',	4, NULL 					, NULL)
};

IMPLEMENT_ACTOR (AGauntletPuff1, Heretic, -1, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_Flags3 (MF3_PUFFONACTORS)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (HR_SHADOW)
	PROP_SpawnState (S_GAUNTLETPUFF1)
END_DEFAULTS

void AGauntletPuff1::BeginPlay ()
{
	Super::BeginPlay ();
	momz = FRACUNIT * 8 / 10;
}

// Gauntlett puff 2 ---------------------------------------------------------

class AGauntletPuff2 : public AGauntletPuff1
{
	DECLARE_ACTOR (AGauntletPuff2, AGauntletPuff1)
};

FState AGauntletPuff2::States[] =
{
#define S_GAUNTLETPUFF2 0
	S_BRIGHT (PUF1, 'E',	4, NULL 					, &States[S_GAUNTLETPUFF2+1]),
	S_BRIGHT (PUF1, 'F',	4, NULL 					, &States[S_GAUNTLETPUFF2+2]),
	S_BRIGHT (PUF1, 'G',	4, NULL 					, &States[S_GAUNTLETPUFF2+3]),
	S_BRIGHT (PUF1, 'H',	4, NULL 					, NULL)
};

IMPLEMENT_ACTOR (AGauntletPuff2, Heretic, -1, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (HR_SHADOW)

	PROP_SpawnState (S_GAUNTLETPUFF2)
END_DEFAULTS

//---------------------------------------------------------------------------
//
// PROC A_GauntletAttack
//
//---------------------------------------------------------------------------

void A_GauntletAttack (AActor *actor)
{
	angle_t angle;
	int damage;
	int slope;
	int randVal;
	fixed_t dist;
	player_t *player;
	const PClass *pufftype;
	AInventory *power;

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
	player->psprites[ps_weapon].sx = ((pr_gatk()&3)-2) * FRACUNIT;
	player->psprites[ps_weapon].sy = WEAPONTOP + (pr_gatk()&3) * FRACUNIT;
	angle = player->mo->angle;
	power = player->mo->FindInventory (RUNTIME_CLASS(APowerWeaponLevel2));
	if (power)
	{
		damage = pr_gatk.HitDice (2);
		dist = 4*MELEERANGE;
		angle += pr_gatk.Random2() << 17;
		pufftype = RUNTIME_CLASS(AGauntletPuff2);
	}
	else
	{
		damage = pr_gatk.HitDice (2);
		dist = MELEERANGE+1;
		angle += pr_gatk.Random2() << 18;
		pufftype = RUNTIME_CLASS(AGauntletPuff1);
	}
	slope = P_AimLineAttack (player->mo, angle, dist);
	P_LineAttack (player->mo, angle, dist, slope, damage, NAME_Melee, pufftype);
	if (!linetarget)
	{
		if (pr_gatk() > 64)
		{
			player->extralight = !player->extralight;
		}
		S_Sound (player->mo, CHAN_AUTO, "weapons/gauntletson", 1, ATTN_NORM);
		return;
	}
	randVal = pr_gatk();
	if (randVal < 64)
	{
		player->extralight = 0;
	}
	else if (randVal < 160)
	{
		player->extralight = 1;
	}
	else
	{
		player->extralight = 2;
	}
	if (power)
	{
		P_GiveBody (player->mo, damage>>1);
		S_Sound (player->mo, CHAN_AUTO, "weapons/gauntletspowhit", 1, ATTN_NORM);
	}
	else
	{
		S_Sound (player->mo, CHAN_AUTO, "weapons/gauntletshit", 1, ATTN_NORM);
	}
	// turn to face target
	angle = R_PointToAngle2 (player->mo->x, player->mo->y,
		linetarget->x, linetarget->y);
	if (angle-player->mo->angle > ANG180)
	{
		if ((int)(angle-player->mo->angle) < -ANG90/20)
			player->mo->angle = angle+ANG90/21;
		else
			player->mo->angle -= ANG90/20;
	}
	else
	{
		if (angle-player->mo->angle > ANG90/20)
			player->mo->angle = angle-ANG90/21;
		else
			player->mo->angle += ANG90/20;
	}
	player->mo->flags |= MF_JUSTATTACKED;
}

// --- Blaster (aka Claw) ---------------------------------------------------

void A_FireBlasterPL1 (AActor *);
void A_FireBlasterPL2 (AActor *);
void A_SpawnRippers (AActor *);

// Blaster ------------------------------------------------------------------

class ABlaster : public AHereticWeapon
{
	DECLARE_ACTOR (ABlaster, AHereticWeapon)
};

class ABlasterPowered : public ABlaster
{
	DECLARE_STATELESS_ACTOR (ABlasterPowered, ABlaster)
};

FState ABlaster::States[] =
{
#define S_BLSR 0
	S_NORMAL (WBLS, 'A',   -1, NULL 						, NULL),

#define S_BLASTERREADY (S_BLSR+1)
	S_NORMAL (BLSR, 'A',	1, A_WeaponReady				, &States[S_BLASTERREADY]),

#define S_BLASTERDOWN (S_BLASTERREADY+1)
	S_NORMAL (BLSR, 'A',	1, A_Lower						, &States[S_BLASTERDOWN]),

#define S_BLASTERUP (S_BLASTERDOWN+1)
	S_NORMAL (BLSR, 'A',	1, A_Raise						, &States[S_BLASTERUP]),

#define S_BLASTERATK1 (S_BLASTERUP+1)
	S_NORMAL (BLSR, 'B',	3, NULL 						, &States[S_BLASTERATK1+1]),
	S_NORMAL (BLSR, 'C',	3, NULL 						, &States[S_BLASTERATK1+2]),
	S_NORMAL (BLSR, 'D',	2, A_FireBlasterPL1 			, &States[S_BLASTERATK1+3]),
	S_NORMAL (BLSR, 'C',	2, NULL 						, &States[S_BLASTERATK1+4]),
	S_NORMAL (BLSR, 'B',	2, NULL 						, &States[S_BLASTERATK1+5]),
	S_NORMAL (BLSR, 'A',	0, A_ReFire 					, &States[S_BLASTERREADY]),

#define S_BLASTERATK2 (S_BLASTERATK1+6)
	S_NORMAL (BLSR, 'B',	0, NULL 						, &States[S_BLASTERATK2+1]),
	S_NORMAL (BLSR, 'C',	0, NULL 						, &States[S_BLASTERATK2+2]),
	S_NORMAL (BLSR, 'D',	3, A_FireBlasterPL2 			, &States[S_BLASTERATK2+3]),
	S_NORMAL (BLSR, 'C',	4, NULL 						, &States[S_BLASTERATK2+4]),
	S_NORMAL (BLSR, 'B',	4, NULL 						, &States[S_BLASTERATK2+5]),
	S_NORMAL (BLSR, 'A',	0, A_ReFire 					, &States[S_BLASTERREADY])
};

IMPLEMENT_ACTOR (ABlaster, Heretic, 53, 28)
	PROP_Flags (MF_SPECIAL)
	PROP_Flags5 (MF5_BLOODSPLATTER)
	PROP_SpawnState (S_BLSR)

	PROP_Weapon_SelectionOrder (500)
	PROP_Weapon_AmmoUse1 (USE_BLSR_AMMO_1)
	PROP_Weapon_AmmoGive1 (30)
	PROP_Weapon_UpState (S_BLASTERUP)
	PROP_Weapon_DownState (S_BLASTERDOWN)
	PROP_Weapon_ReadyState (S_BLASTERREADY)
	PROP_Weapon_AtkState (S_BLASTERATK1)
	PROP_Weapon_HoldAtkState (S_BLASTERATK1+2)
	PROP_Weapon_YAdjust (15)
	PROP_Weapon_MoveCombatDist (27000000)
	PROP_Weapon_AmmoType1 ("BlasterAmmo")
	PROP_Weapon_SisterType ("BlasterPowered")
	PROP_Inventory_PickupMessage("$TXT_WPNBLASTER")
END_DEFAULTS

IMPLEMENT_STATELESS_ACTOR (ABlasterPowered, Heretic, -1, 0)
	PROP_Weapon_Flags (WIF_POWERED_UP)
	PROP_Weapon_AmmoUse1 (USE_BLSR_AMMO_2)
	PROP_Weapon_AmmoGive1 (0)
	PROP_Weapon_AtkState (S_BLASTERATK2)
	PROP_Weapon_HoldAtkState (S_BLASTERATK2+2)
	PROP_Weapon_SisterType ("Blaster")
	PROP_Weapon_ProjectileType ("BlasterFX1")
END_DEFAULTS

// Blaster FX 1 -------------------------------------------------------------

class ABlasterFX1 : public AActor
{
	DECLARE_ACTOR (ABlasterFX1, AActor)
public:
	void Tick ();
	int DoSpecialDamage (AActor *target, int damage);
};

FState ABlasterFX1::States[] =
{
#define S_BLASTERFX1 0
	S_NORMAL (ACLO, 'E',  200, NULL 					, &States[S_BLASTERFX1+0]),

#define S_BLASTERFXI1 (S_BLASTERFX1+1)
	S_BRIGHT (FX18, 'A',	3, A_SpawnRippers			, &States[S_BLASTERFXI1+1]),
	S_BRIGHT (FX18, 'B',	3, NULL 					, &States[S_BLASTERFXI1+2]),
	S_BRIGHT (FX18, 'C',	4, NULL 					, &States[S_BLASTERFXI1+3]),
	S_BRIGHT (FX18, 'D',	4, NULL 					, &States[S_BLASTERFXI1+4]),
	S_BRIGHT (FX18, 'E',	4, NULL 					, &States[S_BLASTERFXI1+5]),
	S_BRIGHT (FX18, 'F',	4, NULL 					, &States[S_BLASTERFXI1+6]),
	S_BRIGHT (FX18, 'G',	4, NULL 					, NULL)
};

IMPLEMENT_ACTOR (ABlasterFX1, Heretic, -1, 0)
	PROP_RadiusFixed (12)
	PROP_HeightFixed (8)
	PROP_SpeedFixed (184)
	PROP_Damage (2)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF|MF_NOGRAVITY)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_PCROSS|MF2_IMPACT)

	PROP_SpawnState (S_BLASTERFX1)
	PROP_DeathState (S_BLASTERFXI1)

	PROP_DeathSound ("weapons/blasterhit")
END_DEFAULTS

int ABlasterFX1::DoSpecialDamage (AActor *target, int damage)
{
	if (target->IsKindOf (PClass::FindClass ("Ironlich")))
	{ // Less damage to Ironlich bosses
		damage = pr_bfx1() & 1;
		if (!damage)
		{
			return -1;
		}
	}
	return damage;
}

// Blaster smoke ------------------------------------------------------------

class ABlasterSmoke : public AActor
{
	DECLARE_ACTOR (ABlasterSmoke, AActor)
};

FState ABlasterSmoke::States[] =
{
#define S_BLASTERSMOKE 0
	S_NORMAL (FX18, 'H',	4, NULL 					, &States[S_BLASTERSMOKE+1]),
	S_NORMAL (FX18, 'I',	4, NULL 					, &States[S_BLASTERSMOKE+2]),
	S_NORMAL (FX18, 'J',	4, NULL 					, &States[S_BLASTERSMOKE+3]),
	S_NORMAL (FX18, 'K',	4, NULL 					, &States[S_BLASTERSMOKE+4]),
	S_NORMAL (FX18, 'L',	4, NULL 					, NULL)
};

IMPLEMENT_ACTOR (ABlasterSmoke, Heretic, -1, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_CANNOTPUSH)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (HR_SHADOW)

	PROP_SpawnState (S_BLASTERSMOKE)
END_DEFAULTS

// Ripper -------------------------------------------------------------------

class ARipper : public AActor
{
	DECLARE_ACTOR (ARipper, AActor)
public:
	int DoSpecialDamage (AActor *target, int damage);
};

FState ARipper::States[] =
{
#define S_RIPPER 0
	S_NORMAL (FX18, 'M',	4, NULL 					, &States[S_RIPPER+1]),
	S_NORMAL (FX18, 'N',	5, NULL 					, &States[S_RIPPER+0]),

#define S_RIPPERX (S_RIPPER+2)
	S_BRIGHT (FX18, 'O',	4, NULL 					, &States[S_RIPPERX+1]),
	S_BRIGHT (FX18, 'P',	4, NULL 					, &States[S_RIPPERX+2]),
	S_BRIGHT (FX18, 'Q',	4, NULL 					, &States[S_RIPPERX+3]),
	S_BRIGHT (FX18, 'R',	4, NULL 					, &States[S_RIPPERX+4]),
	S_BRIGHT (FX18, 'S',	4, NULL 					, NULL)
};

IMPLEMENT_ACTOR (ARipper, Heretic, -1, 157)
	PROP_RadiusFixed (8)
	PROP_HeightFixed (6)
	PROP_SpeedFixed (14)
	PROP_Damage (1)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF|MF_NOGRAVITY)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_RIP|MF2_PCROSS|MF2_IMPACT)
	PROP_Flags3 (MF3_WARNBOT)

	PROP_SpawnState (S_RIPPER)
	PROP_DeathState (S_RIPPERX)

	PROP_DeathSound ("weapons/blasterpowhit")
END_DEFAULTS

int ARipper::DoSpecialDamage (AActor *target, int damage)
{
	if (target->IsKindOf (PClass::FindClass ("Ironlich")))
	{ // Less damage to Ironlich bosses
		damage = pr_ripd() & 1;
		if (!damage)
		{
			return -1;
		}
	}
	return damage;
}

// Blaster Puff -------------------------------------------------------------

class ABlasterPuff : public AActor
{
	DECLARE_ACTOR (ABlasterPuff, AActor)
};

FState ABlasterPuff::States[] =
{
#define S_BLASTERPUFF1 0
	S_BRIGHT (FX17, 'A',	4, NULL 					, &States[S_BLASTERPUFF1+1]),
	S_BRIGHT (FX17, 'B',	4, NULL 					, &States[S_BLASTERPUFF1+2]),
	S_BRIGHT (FX17, 'C',	4, NULL 					, &States[S_BLASTERPUFF1+3]),
	S_BRIGHT (FX17, 'D',	4, NULL 					, &States[S_BLASTERPUFF1+4]),
	S_BRIGHT (FX17, 'E',	4, NULL 					, NULL),

#define S_BLASTERPUFF2 (S_BLASTERPUFF1+5)
	S_BRIGHT (FX17, 'F',	3, NULL 					, &States[S_BLASTERPUFF2+1]),
	S_BRIGHT (FX17, 'G',	3, NULL 					, &States[S_BLASTERPUFF2+2]),
	S_BRIGHT (FX17, 'H',	4, NULL 					, &States[S_BLASTERPUFF2+3]),
	S_BRIGHT (FX17, 'I',	4, NULL 					, &States[S_BLASTERPUFF2+4]),
	S_BRIGHT (FX17, 'J',	4, NULL 					, &States[S_BLASTERPUFF2+5]),
	S_BRIGHT (FX17, 'K',	4, NULL 					, &States[S_BLASTERPUFF2+6]),
	S_BRIGHT (FX17, 'L',	4, NULL 					, NULL)
};

IMPLEMENT_ACTOR (ABlasterPuff, Heretic, -1, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_Flags3 (MF3_PUFFONACTORS)
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (S_BLASTERPUFF2)
	PROP_CrashState (S_BLASTERPUFF1)
END_DEFAULTS

//----------------------------------------------------------------------------
//
// PROC A_FireBlasterPL1
//
//----------------------------------------------------------------------------

void A_FireBlasterPL1 (AActor *actor)
{
	angle_t angle;
	int damage;
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
	P_BulletSlope(actor);
	damage = pr_fb1.HitDice (4);
	angle = actor->angle;
	if (player->refire)
	{
		angle += pr_fb1.Random2() << 18;
	}
	P_LineAttack (actor, angle, PLAYERMISSILERANGE, bulletpitch, damage, NAME_None, RUNTIME_CLASS(ABlasterPuff));
	S_Sound (actor, CHAN_WEAPON, "weapons/blastershoot", 1, ATTN_NORM);
}

//----------------------------------------------------------------------------
//
// PROC A_FireBlasterPL2
//
//----------------------------------------------------------------------------

void A_FireBlasterPL2 (AActor *actor)
{
	AActor *mo;
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
	mo = P_SpawnPlayerMissile (actor, RUNTIME_CLASS(ABlasterFX1));
	S_Sound (mo, CHAN_WEAPON, "weapons/blastershoot", 1, ATTN_NORM);
}

//----------------------------------------------------------------------------
//
// PROC A_SpawnRippers
//
//----------------------------------------------------------------------------

void A_SpawnRippers (AActor *actor)
{
	int i;
	angle_t angle;
	AActor *ripper;

	for(i = 0; i < 8; i++)
	{
		ripper = Spawn<ARipper> (actor->x, actor->y, actor->z, ALLOW_REPLACE);
		angle = i*ANG45;
		ripper->target = actor->target;
		ripper->angle = angle;
		angle >>= ANGLETOFINESHIFT;
		ripper->momx = FixedMul (ripper->Speed, finecosine[angle]);
		ripper->momy = FixedMul (ripper->Speed, finesine[angle]);
		P_CheckMissileSpawn (ripper);
	}
}

//----------------------------------------------------------------------------
//
// PROC P_BlasterMobjThinker
//
// Thinker for the ultra-fast blaster PL2 ripper-spawning missile.
//
//----------------------------------------------------------------------------

void ABlasterFX1::Tick ()
{
	int i;
	fixed_t xfrac;
	fixed_t yfrac;
	fixed_t zfrac;
	int changexy;

	PrevX = x;
	PrevY = y;
	PrevZ = z;

	// Handle movement
	if (momx || momy || (z != floorz) || momz)
	{
		xfrac = momx>>3;
		yfrac = momy>>3;
		zfrac = momz>>3;
		changexy = xfrac | yfrac;
		for (i = 0; i < 8; i++)
		{
			if (changexy)
			{
				if (!P_TryMove (this, x + xfrac, y + yfrac, true))
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
			if (z + height > ceilingz)
			{ // Hit the ceiling
				z = ceilingz - height;
				P_ExplodeMissile (this, NULL, NULL);
				return;
			}
			if (changexy && (pr_bfx1t() < 64))
			{
				Spawn<ABlasterSmoke> (x, y, MAX<fixed_t> (z - 8 * FRACUNIT, floorz), ALLOW_REPLACE);
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

// --- Skull rod ------------------------------------------------------------

void A_FireSkullRodPL1 (AActor *);
void A_FireSkullRodPL2 (AActor *);
void A_SkullRodPL2Seek (AActor *);
void A_AddPlayerRain (AActor *);
void A_HideInCeiling (AActor *);
void A_SkullRodStorm (AActor *);
void A_RainImpact (AActor *);

// Skull (Horn) Rod ---------------------------------------------------------

class ASkullRod : public AHereticWeapon
{
	DECLARE_ACTOR (ASkullRod, AHereticWeapon)
};

class ASkullRodPowered : public ASkullRod
{
	DECLARE_STATELESS_ACTOR (ASkullRodPowered, ASkullRod)
};

FState ASkullRod::States[] =
{
#define S_WSKL 0
	S_NORMAL (WSKL, 'A',   -1, NULL 						, NULL),

#define S_HORNRODREADY (S_WSKL+1)
	S_NORMAL (HROD, 'A',	1, A_WeaponReady				, &States[S_HORNRODREADY]),

#define S_HORNRODDOWN (S_HORNRODREADY+1)
	S_NORMAL (HROD, 'A',	1, A_Lower						, &States[S_HORNRODDOWN]),

#define S_HORNRODUP (S_HORNRODDOWN+1)
	S_NORMAL (HROD, 'A',	1, A_Raise						, &States[S_HORNRODUP]),

#define S_HORNRODATK1 (S_HORNRODUP+1)
	S_NORMAL (HROD, 'A',	4, A_FireSkullRodPL1			, &States[S_HORNRODATK1+1]),
	S_NORMAL (HROD, 'B',	4, A_FireSkullRodPL1			, &States[S_HORNRODATK1+2]),
	S_NORMAL (HROD, 'B',	0, A_ReFire 					, &States[S_HORNRODREADY]),

#define S_HORNRODATK2 (S_HORNRODATK1+3)
	S_NORMAL (HROD, 'C',	2, NULL 						, &States[S_HORNRODATK2+1]),
	S_NORMAL (HROD, 'D',	3, NULL 						, &States[S_HORNRODATK2+2]),
	S_NORMAL (HROD, 'E',	2, NULL 						, &States[S_HORNRODATK2+3]),
	S_NORMAL (HROD, 'F',	3, NULL 						, &States[S_HORNRODATK2+4]),
	S_NORMAL (HROD, 'G',	4, A_FireSkullRodPL2			, &States[S_HORNRODATK2+5]),
	S_NORMAL (HROD, 'F',	2, NULL 						, &States[S_HORNRODATK2+6]),
	S_NORMAL (HROD, 'E',	3, NULL 						, &States[S_HORNRODATK2+7]),
	S_NORMAL (HROD, 'D',	2, NULL 						, &States[S_HORNRODATK2+8]),
	S_NORMAL (HROD, 'C',	2, A_ReFire 					, &States[S_HORNRODREADY])
};

IMPLEMENT_ACTOR (ASkullRod, Heretic, 2004, 30)
	PROP_Flags (MF_SPECIAL)
	PROP_SpawnState (S_WSKL)

	PROP_Weapon_SelectionOrder (200)
	PROP_Weapon_AmmoUse1 (USE_SKRD_AMMO_1)
	PROP_Weapon_AmmoGive1 (50)
	PROP_Weapon_UpState (S_HORNRODUP)
	PROP_Weapon_DownState (S_HORNRODDOWN)
	PROP_Weapon_ReadyState (S_HORNRODREADY)
	PROP_Weapon_AtkState (S_HORNRODATK1)
	PROP_Weapon_YAdjust (15)
	PROP_Weapon_MoveCombatDist (27000000)
	PROP_Weapon_AmmoType1 ("SkullRodAmmo")
	PROP_Weapon_SisterType ("SkullRodPowered")
	PROP_Weapon_ProjectileType ("HornRodFX1")
	PROP_Inventory_PickupMessage("$TXT_WPNSKULLROD")
END_DEFAULTS

IMPLEMENT_STATELESS_ACTOR (ASkullRodPowered, Heretic, -1, 0)
	PROP_Weapon_Flags (WIF_POWERED_UP)
	PROP_Weapon_AmmoUse1 (USE_SKRD_AMMO_2)
	PROP_Weapon_AmmoGive1 (0)
	PROP_Weapon_AtkState (S_HORNRODATK2)
	PROP_Weapon_SisterType ("SkullRod")
	PROP_Weapon_ProjectileType ("HornRodFX2")
END_DEFAULTS

// Horn Rod FX 1 ------------------------------------------------------------

class AHornRodFX1 : public AActor
{
	DECLARE_ACTOR (AHornRodFX1, AActor)
};

FState AHornRodFX1::States[] =
{
#define S_HRODFX1 0
	S_BRIGHT (FX00, 'A',	6, NULL 					, &States[S_HRODFX1+1]),
	S_BRIGHT (FX00, 'B',	6, NULL 					, &States[S_HRODFX1+0]),

#define S_HRODFXI1 (S_HRODFX1+2)
	S_BRIGHT (FX00, 'H',	5, NULL 					, &States[S_HRODFXI1+1]),
	S_BRIGHT (FX00, 'I',	5, NULL 					, &States[S_HRODFXI1+2]),
	S_BRIGHT (FX00, 'J',	4, NULL 					, &States[S_HRODFXI1+3]),
	S_BRIGHT (FX00, 'K',	4, NULL 					, &States[S_HRODFXI1+4]),
	S_BRIGHT (FX00, 'L',	3, NULL 					, &States[S_HRODFXI1+5]),
	S_BRIGHT (FX00, 'M',	3, NULL 					, NULL)
};

IMPLEMENT_ACTOR (AHornRodFX1, Heretic, -1, 160)
	PROP_RadiusFixed (12)
	PROP_HeightFixed (8)
	PROP_SpeedFixed (22)
	PROP_Damage (3)
	PROP_Flags (MF_MISSILE|MF_DROPOFF|MF_NOGRAVITY)
	PROP_Flags2 (MF2_WINDTHRUST|MF2_NOTELEPORT|MF2_PCROSS|MF2_IMPACT)
	PROP_Flags3 (MF3_WARNBOT)
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (S_HRODFX1)
	PROP_DeathState (S_HRODFXI1)

	PROP_SeeSound ("weapons/hornrodshoot")
	PROP_DeathSound ("weapons/hornrodhit")
END_DEFAULTS

// Horn Rod FX 2 ------------------------------------------------------------

class AHornRodFX2 : public AActor
{
	DECLARE_ACTOR (AHornRodFX2, AActor)
public:
	int DoSpecialDamage (AActor *target, int damage);
};

FState AHornRodFX2::States[] =
{
#define S_HRODFX2 0
	S_BRIGHT (FX00, 'C',	3, NULL 					, &States[S_HRODFX2+1]),
	S_BRIGHT (FX00, 'D',	3, A_SkullRodPL2Seek		, &States[S_HRODFX2+2]),
	S_BRIGHT (FX00, 'E',	3, NULL 					, &States[S_HRODFX2+3]),
	S_BRIGHT (FX00, 'F',	3, A_SkullRodPL2Seek		, &States[S_HRODFX2+0]),

#define S_HRODFXI2 (S_HRODFX2+4)
	S_BRIGHT (FX00, 'H',	5, A_AddPlayerRain			, &States[S_HRODFXI2+1]),
	S_BRIGHT (FX00, 'I',	5, NULL 					, &States[S_HRODFXI2+2]),
	S_BRIGHT (FX00, 'J',	4, NULL 					, &States[S_HRODFXI2+3]),
	S_BRIGHT (FX00, 'K',	3, NULL 					, &States[S_HRODFXI2+4]),
	S_BRIGHT (FX00, 'L',	3, NULL 					, &States[S_HRODFXI2+5]),
	S_BRIGHT (FX00, 'M',	3, NULL 					, &States[S_HRODFXI2+6]),
	S_NORMAL (FX00, 'G',	1, A_HideInCeiling			, &States[S_HRODFXI2+7]),
	S_NORMAL (FX00, 'G',	1, A_SkullRodStorm			, &States[S_HRODFXI2+7])
};

IMPLEMENT_ACTOR (AHornRodFX2, Heretic, -1, 0)
	PROP_RadiusFixed (12)
	PROP_HeightFixed (8)
	PROP_SpeedFixed (22)
	PROP_Damage (10)
	PROP_SpawnHealth (4*35)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF|MF_NOGRAVITY)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_PCROSS|MF2_IMPACT)
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (S_HRODFX2)
	PROP_DeathState (S_HRODFXI2)

	PROP_SeeSound ("weapons/hornrodshoot")
	PROP_DeathSound ("weapons/hornrodpowhit")
END_DEFAULTS

int AHornRodFX2::DoSpecialDamage (AActor *target, int damage)
{
	if (target->IsKindOf (RUNTIME_CLASS (ASorcerer2)) && pr_hrfx2() < 96)
	{ // D'Sparil teleports away
		P_DSparilTeleport (target);
		return -1;
	}
	return damage;
}

// Rain pillar 1 ------------------------------------------------------------

class ARainPillar : public AActor
{
	DECLARE_ACTOR (ARainPillar, AActor)
public:
	int DoSpecialDamage (AActor *target, int damage);
};

FState ARainPillar::States[] =
{
#define S_RAINPLR 0
	S_BRIGHT (FX22, 'A',   -1, NULL 					, NULL),

#define S_RAINPLRX (S_RAINPLR+1)
	S_BRIGHT (FX22, 'B',	4, A_RainImpact 			, &States[S_RAINPLRX+1]),
	S_BRIGHT (FX22, 'C',	4, NULL 					, &States[S_RAINPLRX+2]),
	S_BRIGHT (FX22, 'D',	4, NULL 					, &States[S_RAINPLRX+3]),
	S_BRIGHT (FX22, 'E',	4, NULL 					, &States[S_RAINPLRX+4]),
	S_BRIGHT (FX22, 'F',	4, NULL 					, NULL),

#define S_RAINAIRXPLR (S_RAINPLRX+5)
	S_BRIGHT (FX22, 'G',	4, NULL 					, &States[S_RAINAIRXPLR+1]),
	S_BRIGHT (FX22, 'H',	4, NULL 					, &States[S_RAINAIRXPLR+2]),
	S_BRIGHT (FX22, 'I',	4, NULL 					, NULL),
};

IMPLEMENT_ACTOR (ARainPillar, Heretic, -1, 0)
	PROP_RadiusFixed (5)
	PROP_HeightFixed (12)
	PROP_SpeedFixed (12)
	PROP_Damage (5)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF|MF_NOGRAVITY)
	PROP_Flags2 (MF2_NOTELEPORT)
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (S_RAINPLR)
	PROP_DeathState (S_RAINPLRX)
END_DEFAULTS

int ARainPillar::DoSpecialDamage (AActor *target, int damage)
{
	if (target->flags2 & MF2_BOSS)
	{ // Decrease damage for bosses
		damage = (pr_rp() & 7) + 1;
	}
	return damage;
}

// Rain tracker "inventory" item --------------------------------------------

class ARainTracker : public AInventory
{
	DECLARE_STATELESS_ACTOR (ARainTracker, AInventory)
public:
	void Serialize (FArchive &arc);
	AActor *Rain1, *Rain2;
};

IMPLEMENT_STATELESS_ACTOR (ARainTracker, Any, -1, 0)
	PROP_Inventory_FlagsSet (IF_UNDROPPABLE)
END_DEFAULTS

void ARainTracker::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << Rain1 << Rain2;
}

//----------------------------------------------------------------------------
//
// PROC A_FireSkullRodPL1
//
//----------------------------------------------------------------------------

void A_FireSkullRodPL1 (AActor *actor)
{
	AActor *mo;
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
	mo = P_SpawnPlayerMissile (player->mo, RUNTIME_CLASS(AHornRodFX1));
	// Randomize the first frame
	if (mo && pr_fsr1() > 128)
	{
		mo->SetState (mo->state->GetNextState());
	}
}

//----------------------------------------------------------------------------
//
// PROC A_FireSkullRodPL2
//
// The special2 field holds the player number that shot the rain missile.
// The special1 field holds the id of the rain sound.
//
//----------------------------------------------------------------------------

void A_FireSkullRodPL2 (AActor *actor)
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
	P_SpawnPlayerMissile (player->mo, RUNTIME_CLASS(AHornRodFX2));
	// Use MissileActor instead of the return value from
	// P_SpawnPlayerMissile because we need to give info to the mobj
	// even if it exploded immediately.
	if (MissileActor != NULL)
	{
		MissileActor->special2 = (int)(player - players);
		if (linetarget)
		{
			MissileActor->tracer = linetarget;
		}
		S_Sound (MissileActor, CHAN_WEAPON, "weapons/hornrodpowshoot", 1, ATTN_NORM);
	}
}

//----------------------------------------------------------------------------
//
// PROC A_SkullRodPL2Seek
//
//----------------------------------------------------------------------------

void A_SkullRodPL2Seek (AActor *actor)
{
	P_SeekerMissile (actor, ANGLE_1*10, ANGLE_1*30);
}

//----------------------------------------------------------------------------
//
// PROC A_AddPlayerRain
//
//----------------------------------------------------------------------------

void A_AddPlayerRain (AActor *actor)
{
	ARainTracker *tracker;

	if (actor->target == NULL || actor->target->health <= 0)
	{ // Shooter is dead or nonexistant
		return;
	}

	tracker = actor->target->FindInventory<ARainTracker> ();

	// They player is only allowed two rainstorms at a time. Shooting more
	// than that will cause the oldest one to terminate.
	if (tracker != NULL)
	{
		if (tracker->Rain1 && tracker->Rain2)
		{ // Terminate an active rain
			if (tracker->Rain1->health < tracker->Rain2->health)
			{
				if (tracker->Rain1->health > 16)
				{
					tracker->Rain1->health = 16;
				}
				tracker->Rain1 = NULL;
			}
			else
			{
				if (tracker->Rain2->health > 16)
				{
					tracker->Rain2->health = 16;
				}
				tracker->Rain2 = NULL;
			}
		}
	}
	else
	{
		tracker = static_cast<ARainTracker *> (actor->target->GiveInventoryType (RUNTIME_CLASS(ARainTracker)));
	}
	// Add rain mobj to list
	if (tracker->Rain1)
	{
		tracker->Rain2 = actor;
	}
	else
	{
		tracker->Rain1 = actor;
	}
	actor->special1 = S_FindSound ("misc/rain");
}

//----------------------------------------------------------------------------
//
// PROC A_SkullRodStorm
//
//----------------------------------------------------------------------------

void A_SkullRodStorm (AActor *actor)
{
	fixed_t x;
	fixed_t y;
	AActor *mo;
	ARainTracker *tracker;

	if (actor->health-- == 0)
	{
		S_StopSound (actor, CHAN_BODY);
		if (actor->target == NULL)
		{ // Player left the game
			actor->Destroy ();
			return;
		}
		tracker = actor->target->FindInventory<ARainTracker> ();
		if (tracker != NULL)
		{
			if (tracker->Rain1 == actor)
			{
				tracker->Rain1 = NULL;
			}
			else if (tracker->Rain2 == actor)
			{
				tracker->Rain2 = NULL;
			}
		}
		actor->Destroy ();
		return;
	}
	if (pr_storm() < 25)
	{ // Fudge rain frequency
		return;
	}
	x = actor->x + ((pr_storm()&127) - 64) * FRACUNIT;
	y = actor->y + ((pr_storm()&127) - 64) * FRACUNIT;
	mo = Spawn<ARainPillar> (x, y, ONCEILINGZ, ALLOW_REPLACE);
	mo->Translation = multiplayer ?
		TRANSLATION(TRANSLATION_PlayersExtra,actor->special2) : 0;
	mo->target = actor->target;
	mo->momx = 1; // Force collision detection
	mo->momz = -mo->Speed;
	mo->special2 = actor->special2; // Transfer player number
	P_CheckMissileSpawn (mo);
	if (actor->special1 != -1 && !S_IsActorPlayingSomething (actor, CHAN_BODY, -1))
	{
		S_SoundID (actor, CHAN_BODY|CHAN_LOOP, actor->special1, 1, ATTN_NORM);
	}
}

//----------------------------------------------------------------------------
//
// PROC A_RainImpact
//
//----------------------------------------------------------------------------

void A_RainImpact (AActor *actor)
{
	if (actor->z > actor->floorz)
	{
		actor->SetState (&ARainPillar::States[S_RAINAIRXPLR]);
	}
	else if (pr_impact() < 40)
	{
		P_HitFloor (actor);
	}
}

//----------------------------------------------------------------------------
//
// PROC A_HideInCeiling
//
//----------------------------------------------------------------------------

void A_HideInCeiling (AActor *actor)
{
	actor->z = actor->ceilingz + 4*FRACUNIT;
}

// --- Phoenix Rod ----------------------------------------------------------

void A_FirePhoenixPL1 (AActor *);
void A_InitPhoenixPL2 (AActor *);
void A_FirePhoenixPL2 (AActor *);
void A_ShutdownPhoenixPL2 (AActor *);
void A_PhoenixPuff (AActor *);
void A_FlameEnd (AActor *);
void A_FloatPuff (AActor *);

// Phoenix Rod --------------------------------------------------------------

class APhoenixRod : public AHereticWeapon
{
	DECLARE_ACTOR (APhoenixRod, AHereticWeapon)
public:
	void Serialize (FArchive &arc)
	{
		Super::Serialize (arc);
		arc << FlameCount;
	}
	int FlameCount;		// for flamethrower duration
};

class APhoenixRodPowered : public APhoenixRod
{
	DECLARE_STATELESS_ACTOR (APhoenixRodPowered, APhoenixRod)
public:
	void EndPowerup ();
};

FState APhoenixRod::States[] =
{
#define S_WPHX 0
	S_NORMAL (WPHX, 'A',   -1, NULL 						, NULL),

#define S_PHOENIXREADY (S_WPHX+1)
	S_NORMAL (PHNX, 'A',	1, A_WeaponReady				, &States[S_PHOENIXREADY]),

#define S_PHOENIXDOWN (S_PHOENIXREADY+1)
	S_NORMAL (PHNX, 'A',	1, A_Lower						, &States[S_PHOENIXDOWN]),

#define S_PHOENIXUP (S_PHOENIXDOWN+1)
	S_NORMAL (PHNX, 'A',	1, A_Raise						, &States[S_PHOENIXUP]),

#define S_PHOENIXATK1 (S_PHOENIXUP+1)
	S_NORMAL (PHNX, 'B',	5, NULL 						, &States[S_PHOENIXATK1+1]),
	S_NORMAL (PHNX, 'C',	7, A_FirePhoenixPL1 			, &States[S_PHOENIXATK1+2]),
	S_NORMAL (PHNX, 'D',	4, NULL 						, &States[S_PHOENIXATK1+3]),
	S_NORMAL (PHNX, 'B',	4, NULL 						, &States[S_PHOENIXATK1+4]),
	S_NORMAL (PHNX, 'B',	0, A_ReFire 					, &States[S_PHOENIXREADY]),

#define S_PHOENIXATK2 (S_PHOENIXATK1+5)
	S_NORMAL (PHNX, 'B',	3, A_InitPhoenixPL2 			, &States[S_PHOENIXATK2+1]),
	S_BRIGHT (PHNX, 'C',	1, A_FirePhoenixPL2 			, &States[S_PHOENIXATK2+2]),
	S_NORMAL (PHNX, 'B',	4, A_ReFire 					, &States[S_PHOENIXATK2+3]),
	S_NORMAL (PHNX, 'B',	4, A_ShutdownPhoenixPL2 		, &States[S_PHOENIXREADY])
};

IMPLEMENT_ACTOR (APhoenixRod, Heretic, 2003, 29)
	PROP_Flags (MF_SPECIAL)
	PROP_SpawnState (S_WPHX)

	PROP_Weapon_Flags (WIF_NOAUTOFIRE|WIF_BOT_REACTION_SKILL_THING)
	PROP_Weapon_SelectionOrder (2600)
	PROP_Weapon_AmmoUse1 (USE_PHRD_AMMO_1)
	PROP_Weapon_AmmoGive1 (2)
	PROP_Weapon_UpState (S_PHOENIXUP)
	PROP_Weapon_DownState (S_PHOENIXDOWN)
	PROP_Weapon_ReadyState (S_PHOENIXREADY)
	PROP_Weapon_AtkState (S_PHOENIXATK1)
	PROP_Weapon_YAdjust (15)
	PROP_Weapon_MoveCombatDist (18350080)
	PROP_Weapon_AmmoType1 ("PhoenixRodAmmo")
	PROP_Weapon_SisterType ("PhoenixRodPowered")
	PROP_Weapon_ProjectileType ("PhoenixFX1")
	PROP_Inventory_PickupMessage("$TXT_WPNPHOENIXROD")
END_DEFAULTS

IMPLEMENT_STATELESS_ACTOR (APhoenixRodPowered, Heretic, -1, 0)
	PROP_Weapon_Flags (WIF_NOAUTOFIRE|WIF_POWERED_UP|WIF_BOT_MELEE)
	PROP_Weapon_AmmoUse1 (USE_PHRD_AMMO_2)
	PROP_Weapon_AmmoGive1 (0)
	PROP_Weapon_AtkState (S_PHOENIXATK2)
	PROP_Weapon_HoldAtkState (S_PHOENIXATK2+1)
	PROP_Weapon_MoveCombatDist (0)
	PROP_Weapon_SisterType ("PhoenixRod")
	PROP_Weapon_ProjectileType ("PhoenixFX2")
END_DEFAULTS

void APhoenixRodPowered::EndPowerup ()
{
	P_SetPsprite (Owner->player, ps_weapon, &APhoenixRod::States[S_PHOENIXREADY]);
	DepleteAmmo (bAltFire);
	Owner->player->refire = 0;
	S_StopSound (Owner, CHAN_WEAPON);
	Owner->player->ReadyWeapon = SisterWeapon;
}

// Phoenix FX 1 -------------------------------------------------------------

FState APhoenixFX1::States[] =
{
#define S_PHOENIXFX1 0
	S_BRIGHT (FX04, 'A',	4, A_PhoenixPuff			, &States[S_PHOENIXFX1+0]),

#define S_PHOENIXFXI1 (S_PHOENIXFX1+1)
	S_BRIGHT (FX08, 'A',	6, A_Explode				, &States[S_PHOENIXFXI1+1]),
	S_BRIGHT (FX08, 'B',	5, NULL 					, &States[S_PHOENIXFXI1+2]),
	S_BRIGHT (FX08, 'C',	5, NULL 					, &States[S_PHOENIXFXI1+3]),
	S_BRIGHT (FX08, 'D',	4, NULL 					, &States[S_PHOENIXFXI1+4]),
	S_BRIGHT (FX08, 'E',	4, NULL 					, &States[S_PHOENIXFXI1+5]),
	S_BRIGHT (FX08, 'F',	4, NULL 					, &States[S_PHOENIXFXI1+6]),
	S_BRIGHT (FX08, 'G',	4, NULL 					, &States[S_PHOENIXFXI1+7]),
	S_BRIGHT (FX08, 'H',	4, NULL 					, NULL)
};

IMPLEMENT_ACTOR (APhoenixFX1, Heretic, -1, 163)
	PROP_RadiusFixed (11)
	PROP_HeightFixed (8)
	PROP_SpeedFixed (20)
	PROP_Damage (20)
	PROP_DamageType (NAME_Fire)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF|MF_NOGRAVITY)
	PROP_Flags2 (MF2_THRUGHOST|MF2_NOTELEPORT|MF2_PCROSS|MF2_IMPACT)
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (S_PHOENIXFX1)
	PROP_DeathState (S_PHOENIXFXI1)

	PROP_SeeSound ("weapons/phoenixshoot")
	PROP_DeathSound ("weapons/phoenixhit")
END_DEFAULTS

int APhoenixFX1::DoSpecialDamage (AActor *target, int damage)
{
	if (target->IsKindOf (RUNTIME_CLASS (ASorcerer2)) && pr_pfx1() < 96)
	{ // D'Sparil teleports away
		P_DSparilTeleport (target);
		return -1;
	}
	return damage;
}

// Phoenix puff -------------------------------------------------------------

FState APhoenixPuff::States[] =
{
	S_NORMAL (FX04, 'B',	4, NULL 					, &States[1]),
	S_NORMAL (FX04, 'C',	4, NULL 					, &States[2]),
	S_NORMAL (FX04, 'D',	4, NULL 					, &States[3]),
	S_NORMAL (FX04, 'E',	4, NULL 					, &States[4]),
	S_NORMAL (FX04, 'F',	4, NULL 					, NULL),
};

IMPLEMENT_ACTOR (APhoenixPuff, Heretic, -1, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_CANNOTPUSH)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (HR_SHADOW)

	PROP_SpawnState (0)
END_DEFAULTS

// Phoenix FX 2 -------------------------------------------------------------

class APhoenixFX2 : public AActor
{
	DECLARE_ACTOR (APhoenixFX2, AActor)
public:
	int DoSpecialDamage (AActor *target, int damage);
};

FState APhoenixFX2::States[] =
{
#define S_PHOENIXFX2 0
	S_BRIGHT (FX09, 'A',	2, NULL 					, &States[S_PHOENIXFX2+1]),
	S_BRIGHT (FX09, 'B',	2, NULL 					, &States[S_PHOENIXFX2+2]),
	S_BRIGHT (FX09, 'A',	2, NULL 					, &States[S_PHOENIXFX2+3]),
	S_BRIGHT (FX09, 'B',	2, NULL 					, &States[S_PHOENIXFX2+4]),
	S_BRIGHT (FX09, 'A',	2, NULL 					, &States[S_PHOENIXFX2+5]),
	S_BRIGHT (FX09, 'B',	2, A_FlameEnd				, &States[S_PHOENIXFX2+6]),
	S_BRIGHT (FX09, 'C',	2, NULL 					, &States[S_PHOENIXFX2+7]),
	S_BRIGHT (FX09, 'D',	2, NULL 					, &States[S_PHOENIXFX2+8]),
	S_BRIGHT (FX09, 'E',	2, NULL 					, &States[S_PHOENIXFX2+9]),
	S_BRIGHT (FX09, 'F',	2, NULL 					, NULL),

#define S_PHOENIXFXI2 (S_PHOENIXFX2+10)
	S_BRIGHT (FX09, 'G',	3, NULL 					, &States[S_PHOENIXFXI2+1]),
	S_BRIGHT (FX09, 'H',	3, A_FloatPuff				, &States[S_PHOENIXFXI2+2]),
	S_BRIGHT (FX09, 'I',	4, NULL 					, &States[S_PHOENIXFXI2+3]),
	S_BRIGHT (FX09, 'J',	5, NULL 					, &States[S_PHOENIXFXI2+4]),
	S_BRIGHT (FX09, 'K',	5, NULL 					, NULL)
};

IMPLEMENT_ACTOR (APhoenixFX2, Heretic, -1, 0)
	PROP_RadiusFixed (6)
	PROP_HeightFixed (8)
	PROP_SpeedFixed (10)
	PROP_Damage (2)
	PROP_DamageType (NAME_Fire)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF|MF_NOGRAVITY)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_PCROSS|MF2_IMPACT)
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (S_PHOENIXFX2)
	PROP_DeathState (S_PHOENIXFXI2)
END_DEFAULTS

int APhoenixFX2::DoSpecialDamage (AActor *target, int damage)
{
	if (target->player && pr_pfx2 () < 128)
	{ // Freeze player for a bit
		target->reactiontime += 4;
	}
	return damage;
}

//----------------------------------------------------------------------------
//
// PROC A_FirePhoenixPL1
//
//----------------------------------------------------------------------------

void A_FirePhoenixPL1 (AActor *actor)
{
	angle_t angle;
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
	P_SpawnPlayerMissile (player->mo, RUNTIME_CLASS(APhoenixFX1));
	angle = actor->angle + ANG180;
	angle >>= ANGLETOFINESHIFT;
	actor->momx += FixedMul (4*FRACUNIT, finecosine[angle]);
	actor->momy += FixedMul (4*FRACUNIT, finesine[angle]);
}

//----------------------------------------------------------------------------
//
// PROC A_PhoenixPuff
//
//----------------------------------------------------------------------------

void A_PhoenixPuff (AActor *actor)
{
	AActor *puff;
	angle_t angle;

	//[RH] Heretic never sets the target for seeking
	//P_SeekerMissile (actor, ANGLE_1*5, ANGLE_1*10);
	puff = Spawn<APhoenixPuff> (actor->x, actor->y, actor->z, ALLOW_REPLACE);
	angle = actor->angle + ANG90;
	angle >>= ANGLETOFINESHIFT;
	puff->momx = FixedMul (FRACUNIT*13/10, finecosine[angle]);
	puff->momy = FixedMul (FRACUNIT*13/10, finesine[angle]);
	puff->momz = 0;
	puff = Spawn<APhoenixPuff> (actor->x, actor->y, actor->z, ALLOW_REPLACE);
	angle = actor->angle - ANG90;
	angle >>= ANGLETOFINESHIFT;
	puff->momx = FixedMul (FRACUNIT*13/10, finecosine[angle]);
	puff->momy = FixedMul (FRACUNIT*13/10, finesine[angle]);
	puff->momz = 0;
}

//----------------------------------------------------------------------------
//
// PROC A_InitPhoenixPL2
//
//----------------------------------------------------------------------------

void A_InitPhoenixPL2 (AActor *actor)
{
	if (actor->player != NULL)
	{
		APhoenixRod *flamethrower = static_cast<APhoenixRod *> (actor->player->ReadyWeapon);
		if (flamethrower != NULL)
		{
			flamethrower->FlameCount = FLAME_THROWER_TICS;
		}
	}
}

//----------------------------------------------------------------------------
//
// PROC A_FirePhoenixPL2
//
// Flame thrower effect.
//
//----------------------------------------------------------------------------

void A_FirePhoenixPL2 (AActor *actor)
{
	AActor *mo;
	AActor *pmo;
	angle_t angle;
	fixed_t x, y, z;
	fixed_t slope;
	int soundid;
	player_t *player;
	APhoenixRod *flamethrower;

	if (NULL == (player = actor->player))
	{
		return;
	}

	soundid = S_FindSound ("weapons/phoenixpowshoot");

	flamethrower = static_cast<APhoenixRod *> (player->ReadyWeapon);
	if (flamethrower == NULL || --flamethrower->FlameCount == 0)
	{ // Out of flame
		P_SetPsprite (player, ps_weapon, &APhoenixRod::States[S_PHOENIXATK2+3]);
		player->refire = 0;
		S_StopSound (player->mo, CHAN_WEAPON);
		return;
	}
	pmo = player->mo;
	angle = pmo->angle;
	x = pmo->x + (pr_fp2.Random2() << 9);
	y = pmo->y + (pr_fp2.Random2() << 9);
	z = pmo->z + 26*FRACUNIT + finetangent[FINEANGLES/4-(pmo->pitch>>ANGLETOFINESHIFT)];
	z -= pmo->floorclip;
	slope = finetangent[FINEANGLES/4-(pmo->pitch>>ANGLETOFINESHIFT)] + (FRACUNIT/10);
	mo = Spawn<APhoenixFX2> (x, y, z, ALLOW_REPLACE);
	mo->target = pmo;
	mo->angle = angle;
	mo->momx = pmo->momx + FixedMul (mo->Speed, finecosine[angle>>ANGLETOFINESHIFT]);
	mo->momy = pmo->momy + FixedMul (mo->Speed, finesine[angle>>ANGLETOFINESHIFT]);
	mo->momz = FixedMul (mo->Speed, slope);
	if (!player->refire || !S_IsActorPlayingSomething (pmo, CHAN_WEAPON, -1))
	{
		S_SoundID (pmo, CHAN_WEAPON|CHAN_LOOP, soundid, 1, ATTN_NORM);
	}	
	P_CheckMissileSpawn (mo);
}

//----------------------------------------------------------------------------
//
// PROC A_ShutdownPhoenixPL2
//
//----------------------------------------------------------------------------

void A_ShutdownPhoenixPL2 (AActor *actor)
{
	player_t *player;

	if (NULL == (player = actor->player))
	{
		return;
	}
	S_StopSound (actor, CHAN_WEAPON);
	AWeapon *weapon = actor->player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
	}
}

//----------------------------------------------------------------------------
//
// PROC A_FlameEnd
//
//----------------------------------------------------------------------------

void A_FlameEnd (AActor *actor)
{
	actor->momz += FRACUNIT*3/2;
}

//----------------------------------------------------------------------------
//
// PROC A_FloatPuff
//
//----------------------------------------------------------------------------

void A_FloatPuff (AActor *puff)
{
	puff->momz += FRACUNIT*18/10;
}

