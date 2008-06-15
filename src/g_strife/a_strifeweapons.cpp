#include "a_pickups.h"
#include "p_local.h"
#include "m_random.h"
#include "a_strifeglobal.h"
#include "s_sound.h"
#include "p_enemy.h"
#include "templates.h"

// Note: Strife missiles do 1-4 times their damage amount.
// Doom missiles do 1-8 times their damage amount, so to
// make the strife missiles do proper damage without
// hacking more stuff in the executable, be sure to give
// all Strife missiles the MF4_STRIFEDAMAGE flag.

static FRandom pr_jabdagger ("JabDagger");
static FRandom pr_electric ("FireElectric");
static FRandom pr_sgunshot ("StrifeGunShot");
static FRandom pr_minimissile ("MiniMissile");
static FRandom pr_flamethrower ("FlameThrower");
static FRandom pr_flamedie ("FlameDie");
static FRandom pr_mauler1 ("Mauler1");
static FRandom pr_mauler2 ("Mauler2");
static FRandom pr_phburn ("PhBurn");

void A_LoopActiveSound (AActor *);
void A_Countdown (AActor *);

IMPLEMENT_STATELESS_ACTOR (AStrifeWeapon, Strife, -1, 0)
	PROP_Weapon_Kickback (100)
END_DEFAULTS

// Same as the bullet puff for Doom -----------------------------------------

FState AStrifePuff::States[] =
{
	// When you don't hit an actor
	S_BRIGHT (PUFY, 'A', 4, NULL, &States[1]),
	S_NORMAL (PUFY, 'B', 4, NULL, &States[2]),
	S_NORMAL (PUFY, 'C', 4, NULL, &States[3]),
	S_NORMAL (PUFY, 'D', 4, NULL, NULL),

	// When you do hit an actor
	S_NORMAL (POW3, 'A', 3, NULL, &States[5]),
	S_NORMAL (POW3, 'B', 3, NULL, &States[6]),
	S_NORMAL (POW3, 'C', 3, NULL, &States[7]),
	S_NORMAL (POW3, 'D', 3, NULL, &States[8]),
	S_NORMAL (POW3, 'E', 3, NULL, &States[9]),
	S_NORMAL (POW3, 'F', 3, NULL, &States[10]),
	S_NORMAL (POW3, 'G', 3, NULL, &States[11]),
	S_NORMAL (POW3, 'H', 3, NULL, NULL),
};

IMPLEMENT_ACTOR (AStrifePuff, Strife, -1, 0)
	PROP_SpawnState (4)
	PROP_CrashState (0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_Flags4 (MF4_ALLOWPARTICLES)
	PROP_Alpha (TRANSLUC25)
	PROP_RenderStyle (STYLE_Translucent)
END_DEFAULTS

// A spark when you hit something that doesn't bleed ------------------------
// Only used by the dagger.

class AStrifeSpark : public AActor
{
	DECLARE_ACTOR (AStrifeSpark, AActor)
};

FState AStrifeSpark::States[] =
{
	// When you hit an actor
	S_NORMAL (POW3, 'A', 3, NULL, &States[1]),
	S_NORMAL (POW3, 'B', 3, NULL, &States[2]),
	S_NORMAL (POW3, 'C', 3, NULL, &States[3]),
	S_NORMAL (POW3, 'D', 3, NULL, &States[4]),
	S_NORMAL (POW3, 'E', 3, NULL, &States[5]),
	S_NORMAL (POW3, 'F', 3, NULL, &States[6]),
	S_NORMAL (POW3, 'G', 3, NULL, &States[7]),
	S_NORMAL (POW3, 'H', 3, NULL, NULL),

	// When you hit something else
	S_NORMAL (POW2, 'A', 4, NULL, &States[9]),
	S_NORMAL (POW2, 'B', 4, NULL, &States[10]),
	S_NORMAL (POW2, 'C', 4, NULL, &States[11]),
	S_NORMAL (POW2, 'D', 4, NULL, NULL)
};

IMPLEMENT_ACTOR (AStrifeSpark, Strife, -1, 0)
	PROP_SpawnState (0)
	PROP_CrashState (8)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_Flags4 (MF4_ALLOWPARTICLES)
	PROP_RenderStyle (STYLE_Add)
	PROP_Alpha (TRANSLUC25)
END_DEFAULTS

// Punch Dagger -------------------------------------------------------------

void A_JabDagger (AActor *);

class APunchDagger : public AStrifeWeapon
{
	DECLARE_ACTOR (APunchDagger, AStrifeWeapon)
};

FState APunchDagger::States[] =
{
#define S_PUNCH 0
	S_NORMAL (PNCH, 'A',	1, A_WeaponReady		, &States[S_PUNCH]),

#define S_PUNCHDOWN (S_PUNCH+1)
	S_NORMAL (PNCH, 'A',	1, A_Lower				, &States[S_PUNCHDOWN]),

#define S_PUNCHUP (S_PUNCHDOWN+1)
	S_NORMAL (PNCH, 'A',	1, A_Raise				, &States[S_PUNCHUP]),

#define S_PUNCH1 (S_PUNCHUP+1)
	S_NORMAL (PNCH, 'B',	4, NULL 				, &States[S_PUNCH1+1]),
	S_NORMAL (PNCH, 'C',	4, A_JabDagger			, &States[S_PUNCH1+2]),
	S_NORMAL (PNCH, 'D',	5, NULL 				, &States[S_PUNCH1+3]),
	S_NORMAL (PNCH, 'C',	4, NULL 				, &States[S_PUNCH1+4]),
	S_NORMAL (PNCH, 'B',	5, A_ReFire 			, &States[S_PUNCH])
};

IMPLEMENT_ACTOR (APunchDagger, Strife, -1, 0)
	PROP_Weapon_SelectionOrder (3900)
	PROP_Weapon_Flags (WIF_NOALERT)
	PROP_Weapon_UpState (S_PUNCHUP)
	PROP_Weapon_DownState (S_PUNCHDOWN)
	PROP_Weapon_ReadyState (S_PUNCH)
	PROP_Weapon_AtkState (S_PUNCH1)
END_DEFAULTS

//============================================================================
//
// P_DaggerAlert
//
//============================================================================

void P_DaggerAlert (AActor *target, AActor *emitter)
{
	AActor *looker;
	sector_t *sec = emitter->Sector;

	if (emitter->LastHeard != NULL)
		return;
	if (emitter->health <= 0)
		return;
	if (!(emitter->flags3 & MF3_ISMONSTER))
		return;
	if (emitter->flags4 & MF4_INCOMBAT)
		return;
	emitter->flags4 |= MF4_INCOMBAT;

	emitter->target = target;
	FState * painstate = emitter->FindState(NAME_Pain);
	if (painstate != NULL)
	{
		emitter->SetState (painstate);
	}

	for (looker = sec->thinglist; looker != NULL; looker = looker->snext)
	{
		if (looker == emitter || looker == target)
			continue;

		if (looker->health <= 0)
			continue;

		if (!(looker->flags4 & MF4_SEESDAGGERS))
			continue;

		if (!(looker->flags4 & MF4_INCOMBAT))
		{
			if (!P_CheckSight (looker, target) && !P_CheckSight (looker, emitter))
				continue;

			looker->target = target;
			if (looker->SeeSound)
			{
				S_Sound (looker, CHAN_VOICE, looker->SeeSound, 1, ATTN_NORM);
			}
			looker->SetState (looker->SeeState);
			looker->flags4 |= MF4_INCOMBAT;
		}
	}
}

//============================================================================
//
// A_JabDagger
//
//============================================================================

void A_JabDagger (AActor *actor)
{
	angle_t 	angle;
	int 		damage;
	int 		pitch;
	int			power;
	AActor *linetarget;

	power = MIN(10, actor->player->stamina / 10);
	damage = (pr_jabdagger() % (power + 8)) * (power + 2);

	if (actor->FindInventory<APowerStrength>())
	{
		damage *= 10;
	}

	angle = actor->angle + (pr_jabdagger.Random2() << 18);
	pitch = P_AimLineAttack (actor, angle, 80*FRACUNIT, &linetarget);
	P_LineAttack (actor, angle, 80*FRACUNIT, pitch, damage, NAME_Melee, RUNTIME_CLASS(AStrifeSpark), true);

	// turn to face target
	if (linetarget)
	{
		S_Sound (actor, CHAN_WEAPON,
			linetarget->flags & MF_NOBLOOD ? "misc/metalhit" : "misc/meathit",
			1, ATTN_NORM);
		actor->angle = R_PointToAngle2 (actor->x,
										actor->y,
										linetarget->x,
										linetarget->y);
		actor->flags |= MF_JUSTATTACKED;
		P_DaggerAlert (actor, linetarget);
	}
	else
	{
		S_Sound (actor, CHAN_WEAPON, "misc/swish", 1, ATTN_NORM);
	}
}

// The base for Strife projectiles that die with ZAP1 -----------------------

void A_AlertMonsters (AActor *);

class AStrifeZap1 : public AActor
{
	DECLARE_ACTOR (AStrifeZap1, AActor)
};

FState AStrifeZap1::States[] =
{
	S_NORMAL (ZAP1, 'A',  3, A_AlertMonsters,	&States[1]),
	S_NORMAL (ZAP1, 'B',  3, NULL,				&States[2]),
	S_NORMAL (ZAP1, 'C',  3, NULL,				&States[3]),
	S_NORMAL (ZAP1, 'D',  3, NULL,				&States[4]),
	S_NORMAL (ZAP1, 'E',  3, NULL,				&States[5]),
	S_NORMAL (ZAP1, 'F',  3, NULL,				&States[6]),
	S_NORMAL (ZAP1, 'E',  3, NULL,				&States[7]),
	S_NORMAL (ZAP1, 'D',  3, NULL,				&States[8]),
	S_NORMAL (ZAP1, 'C',  3, NULL,				&States[9]),
	S_NORMAL (ZAP1, 'B',  3, NULL,				&States[10]),
	S_NORMAL (ZAP1, 'A',  3, NULL,				NULL),
};

IMPLEMENT_ACTOR (AStrifeZap1, Strife, -1, 0)
	PROP_DeathState (0)

	// The spawn state is here just to allow it to be spawned on its own.
	// Strife doesn't contain any actual mobjinfo that will spawn this;
	// it is always the death state for something else.
	PROP_SpawnState (0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF)
END_DEFAULTS

//============================================================================
//
// A_AlertMonsters
//
//============================================================================

void A_AlertMonsters (AActor *self)
{
	if (self->player != NULL)
	{
		P_NoiseAlert(self, self);
	}
	else if (self->target != NULL && self->target->player != NULL)
	{
		P_NoiseAlert (self->target, self);
	}
}

// Electric Bolt ------------------------------------------------------------

class AElectricBolt : public AStrifeZap1
{
	DECLARE_ACTOR (AElectricBolt, AStrifeZap1)
};

FState AElectricBolt::States[] =
{
	S_NORMAL (AROW, 'A', 10, A_LoopActiveSound, &States[0]),
};

IMPLEMENT_ACTOR (AElectricBolt, Strife, -1, 0)
	PROP_SpawnState (0)
	PROP_SpeedFixed (30)
	PROP_RadiusFixed (10)
	PROP_HeightFixed (10)
	PROP_Damage (10)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_PCROSS|MF2_IMPACT)
	PROP_Flags4 (MF4_STRIFEDAMAGE)
	PROP_MaxStepHeight (4)
	PROP_StrifeType (102)
	PROP_SeeSound ("misc/swish")
	PROP_ActiveSound ("misc/swish")
	PROP_DeathSound ("weapons/xbowhit")
END_DEFAULTS

// Poison Bolt --------------------------------------------------------------

class APoisonBolt : public AActor
{
	DECLARE_ACTOR (APoisonBolt, AActor)
public:
	int DoSpecialDamage (AActor *target, int damage);
};

FState APoisonBolt::States[] =
{
	S_NORMAL (ARWP, 'A', 10, A_LoopActiveSound, &States[0]),

	S_NORMAL (AROW, 'A',  1, NULL,				NULL)
};

IMPLEMENT_ACTOR (APoisonBolt, Strife, -1, 0)
	PROP_SpawnState (0)
	PROP_DeathState (1)
	PROP_SpeedFixed (30)
	PROP_RadiusFixed (10)
	PROP_HeightFixed (10)
	PROP_DamageLong (500)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_PCROSS|MF2_IMPACT)
	PROP_Flags4 (MF4_STRIFEDAMAGE)
	PROP_MaxStepHeight (4)
	PROP_StrifeType (103)
	PROP_SeeSound ("misc/swish")
	PROP_ActiveSound ("misc/swish")
END_DEFAULTS

int APoisonBolt::DoSpecialDamage (AActor *target, int damage)
{
	if (target->flags & MF_NOBLOOD)
	{
		return -1;
	}
	if (target->health < 1000000)
	{
		return target->health + 10;
	}
	return 1;
}

// Strife's Crossbow --------------------------------------------------------

void A_XBowReFire (AActor *);
void A_ClearFlash (AActor *);
void A_ShowElectricFlash (AActor *);
void A_FireElectric (AActor *);
void A_FirePoison (AActor *);

class AStrifeCrossbow : public AStrifeWeapon
{
	DECLARE_ACTOR (AStrifeCrossbow, AStrifeWeapon)
};

FState AStrifeCrossbow::States[] =
{
	S_NORMAL (CBOW, 'A',   -1, NULL,				&States[0]),

		// Electric

#define S_EXBOW 1
	S_NORMAL (XBOW, 'A',	0, A_ShowElectricFlash, &States[S_EXBOW+1]),
	S_NORMAL (XBOW, 'A',	1, A_WeaponReady,		&States[S_EXBOW+1]),

#define S_EXBOWDOWN (S_EXBOW+2)
	S_NORMAL (XBOW, 'A',	1, A_Lower,				&States[S_EXBOWDOWN]),

#define S_EXBOWUP (S_EXBOWDOWN+1)
	S_NORMAL (XBOW, 'A',	1, A_Raise,				&States[S_EXBOWUP]),

#define S_EXBOWATK (S_EXBOWUP+1)
	S_NORMAL (XBOW, 'A',	3, A_ClearFlash,		&States[S_EXBOWATK+1]),
	S_NORMAL (XBOW, 'B',	6, A_FireElectric,		&States[S_EXBOWATK+2]),
	S_NORMAL (XBOW, 'C',	4, NULL,				&States[S_EXBOWATK+3]),
	S_NORMAL (XBOW, 'D',	6, NULL,				&States[S_EXBOWATK+4]),
	S_NORMAL (XBOW, 'E',	3, NULL,				&States[S_EXBOWATK+5]),
	S_NORMAL (XBOW, 'F',	5, NULL,				&States[S_EXBOWATK+6]),
	S_NORMAL (XBOW, 'G',	5, A_XBowReFire,		&States[S_EXBOW]),

#define S_EXBOWARROWHEAD (S_EXBOWATK+7)
	S_NORMAL (XBOW, 'K',	5, NULL,				&States[S_EXBOWARROWHEAD+1]),
	S_NORMAL (XBOW, 'L',	5, NULL,				&States[S_EXBOWARROWHEAD+2]),
	S_NORMAL (XBOW, 'M',	5, NULL,				&States[S_EXBOWARROWHEAD]),

		// Poison

#define S_PXBOW (S_EXBOWARROWHEAD+3)
	S_NORMAL (XBOW, 'H',	1, A_WeaponReady,		&States[S_PXBOW]),

#define S_PXBOWDOWN (S_PXBOW+1)
	S_NORMAL (XBOW, 'H',	1, A_Lower,				&States[S_PXBOWDOWN]),

#define S_PXBOWUP (S_PXBOWDOWN+1)
	S_NORMAL (XBOW, 'H',	1, A_Raise,				&States[S_PXBOWUP]),

#define S_PXBOWATK (S_PXBOWUP+1)
	S_NORMAL (XBOW, 'H',	3, NULL,				&States[S_PXBOWATK+1]),
	S_NORMAL (XBOW, 'B',	6, A_FirePoison,		&States[S_PXBOWATK+2]),
	S_NORMAL (XBOW, 'C',	4, NULL,				&States[S_PXBOWATK+3]),
	S_NORMAL (XBOW, 'D',	6, NULL,				&States[S_PXBOWATK+4]),
	S_NORMAL (XBOW, 'E',	3, NULL,				&States[S_PXBOWATK+5]),
	S_NORMAL (XBOW, 'I',	5, NULL,				&States[S_PXBOWATK+6]),
	S_NORMAL (XBOW, 'J',	5, A_XBowReFire,		&States[S_PXBOW]),
};

// The electric version

IMPLEMENT_ACTOR (AStrifeCrossbow, Strife, 2001, 0)
	PROP_Flags (MF_SPECIAL)
	PROP_Flags2 (MF2_FLOORCLIP)
	PROP_SpawnState (0)
	PROP_StrifeType (194)
	PROP_StrifeTeaserType (188)
	PROP_StrifeTeaserType2 (192)

	PROP_Weapon_SelectionOrder (1200)
	PROP_Weapon_Flags (WIF_NOALERT)
	PROP_Weapon_AmmoUse1 (1)
	PROP_Weapon_AmmoGive1 (8)
	PROP_Weapon_UpState (S_EXBOWUP)
	PROP_Weapon_DownState (S_EXBOWDOWN)
	PROP_Weapon_ReadyState (S_EXBOW)
	PROP_Weapon_AtkState (S_EXBOWATK)
	PROP_Weapon_MoveCombatDist (24000000)
	PROP_Weapon_AmmoType1 ("ElectricBolts")
	PROP_Weapon_SisterType ("StrifeCrossbow2")
	PROP_Weapon_ProjectileType ("ElectricBolt")
	PROP_Inventory_PickupMessage("$TXT_STRIFECROSSBOW")

	PROP_Inventory_Icon ("CBOWA0")
	PROP_Tag ("crossbow")
END_DEFAULTS

// Poison Crossbow ----------------------------------------------------------

class AStrifeCrossbow2 : public AStrifeCrossbow
{
	DECLARE_STATELESS_ACTOR (AStrifeCrossbow2, AStrifeCrossbow)
};

IMPLEMENT_STATELESS_ACTOR (AStrifeCrossbow2, Strife, -1, 0)
	PROP_Weapon_SelectionOrder (2700)
	PROP_Weapon_Flags (WIF_NOALERT)
	PROP_Weapon_AmmoUse1 (1)
	PROP_Weapon_AmmoGive1 (0)
	PROP_Weapon_UpState (S_PXBOWUP)
	PROP_Weapon_DownState (S_PXBOWDOWN)
	PROP_Weapon_ReadyState (S_PXBOW)
	PROP_Weapon_AtkState (S_PXBOWATK)
	PROP_Weapon_AmmoType1 ("PoisonBolts")
	PROP_Weapon_SisterType ("StrifeCrossbow")
	PROP_Weapon_ProjectileType ("PoisonBolt")
END_DEFAULTS


//============================================================================
//
// A_ClearFlash
//
//============================================================================

void A_ClearFlash (AActor *self)
{
	player_t *player = self->player;

	if (player == NULL)
		return;

	P_SetPsprite (player, ps_flash, NULL);
}

//============================================================================
//
// A_ShowElectricFlash
//
//============================================================================

void A_ShowElectricFlash (AActor *self)
{
	if (self->player != NULL)
	{
		P_SetPsprite (self->player, ps_flash, &AStrifeCrossbow::States[S_EXBOWARROWHEAD]);
	}
}

//============================================================================
//
// A_FireElectric
//
//============================================================================

void A_FireElectric (AActor *self)
{
	angle_t savedangle;

	if (self->player == NULL)
		return;

	AWeapon *weapon = self->player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
	}

	savedangle = self->angle;
	self->angle += pr_electric.Random2 () << (18 - self->player->accuracy * 5 / 100);
	self->player->mo->PlayAttacking2 ();
	P_SpawnPlayerMissile (self, RUNTIME_CLASS(AElectricBolt));
	self->angle = savedangle;
	S_Sound (self, CHAN_WEAPON, "weapons/xbowshoot", 1, ATTN_NORM);
}

//============================================================================
//
// A_FirePoison
//
//============================================================================

void A_FirePoison (AActor *self)
{
	angle_t savedangle;

	if (self->player == NULL)
		return;

	AWeapon *weapon = self->player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
	}

	savedangle = self->angle;
	self->angle += pr_electric.Random2 () << (18 - self->player->accuracy * 5 / 100);
	self->player->mo->PlayAttacking2 ();
	P_SpawnPlayerMissile (self, RUNTIME_CLASS(APoisonBolt));
	self->angle = savedangle;
	S_Sound (self, CHAN_WEAPON, "weapons/xbowshoot", 1, ATTN_NORM);
}

//============================================================================
//
// A_XBowReFire
//
//============================================================================

void A_XBowReFire (AActor *self)
{
	player_t *player = self->player;

	if (player == NULL)
		return;

	AWeapon *weapon = player->ReadyWeapon;
	if (weapon == NULL)
		return;

	weapon->CheckAmmo (weapon->bAltFire ? AWeapon::AltFire : AWeapon::PrimaryFire, true);
	if (weapon->GetClass() == RUNTIME_CLASS(AStrifeCrossbow))
	{
		P_SetPsprite (player, ps_flash, &AStrifeCrossbow::States[S_EXBOWARROWHEAD]);
	}
}

// Assault Gun --------------------------------------------------------------

class AAssaultGun : public AStrifeWeapon
{
	DECLARE_ACTOR (AAssaultGun, AStrifeWeapon)
public:
	bool HandlePickup (AInventory *item);
};

class AAssaultGunStanding : public AAssaultGun
{
	DECLARE_ACTOR (AAssaultGunStanding, AAssaultGun)
public:
	AInventory *CreateCopy (AActor *other);
};

void A_FireAssaultGun (AActor *);

FState AAssaultGun::States[] =
{
	S_NORMAL (RIFL, 'A',   -1, NULL,				&States[0]),

#define S_ASSAULT 1
	S_NORMAL (RIFG, 'A',	1, A_WeaponReady,		&States[S_ASSAULT]),

#define S_ASSAULTDOWN (S_ASSAULT+1)
	S_NORMAL (RIFG, 'B',	1, A_Lower,				&States[S_ASSAULTDOWN]),

#define S_ASSAULTUP (S_ASSAULTDOWN+1)
	S_NORMAL (RIFG, 'A',	1, A_Raise,				&States[S_ASSAULTUP]),

#define S_ASSAULTATK (S_ASSAULTUP+1)
	S_NORMAL (RIFF, 'A',	3, A_FireAssaultGun,	&States[S_ASSAULTATK+1]),
	S_NORMAL (RIFF, 'B',	3, A_FireAssaultGun,	&States[S_ASSAULTATK+2]),
	S_NORMAL (RIFG, 'D',	3, A_FireAssaultGun,	&States[S_ASSAULTATK+3]),
	S_NORMAL (RIFG, 'C',	0, A_ReFire,			&States[S_ASSAULTATK+4]),
	S_NORMAL (RIFG, 'B',	2, NULL,				&States[S_ASSAULT])
};

IMPLEMENT_ACTOR (AAssaultGun, Strife, 2002, 0)
	PROP_Flags (MF_SPECIAL)
	PROP_Flags2 (MF2_FLOORCLIP)
	PROP_SpawnState (0)
	PROP_StrifeType (188)
	PROP_StrifeTeaserType (182)
	PROP_StrifeTeaserType2 (186)

	PROP_Weapon_SelectionOrder (600)
	PROP_Weapon_AmmoUse1 (1)
	PROP_Weapon_AmmoGive1 (20)
	PROP_Weapon_UpState (S_ASSAULTUP)
	PROP_Weapon_DownState (S_ASSAULTDOWN)
	PROP_Weapon_ReadyState (S_ASSAULT)
	PROP_Weapon_AtkState (S_ASSAULTATK)
	PROP_Weapon_MoveCombatDist (27000000)
	PROP_Weapon_AmmoType1 ("ClipOfBullets")

	PROP_Inventory_Icon ("RIFLA0")
	PROP_Tag ("assault_gun")
	PROP_Inventory_PickupMessage("$TXT_ASSAULTGUN")
END_DEFAULTS

//============================================================================
//
// AAssaultGun :: HandlePickup
//
// Picking up the standing assault gun is the same as picking up a regular
// one.
//
//============================================================================

bool AAssaultGun::HandlePickup (AInventory *item)
{
	if (item->GetClass() == RUNTIME_CLASS(AAssaultGunStanding) ||
		item->GetClass() == GetClass())
	{
		if (static_cast<AWeapon *>(item)->PickupForAmmo (this))
		{
			item->ItemFlags |= IF_PICKUPGOOD;
		}
		return true;
	}
	if (Inventory != NULL)
	{
		return Inventory->HandlePickup (item);
	}
	return false;
}

//============================================================================
//
// P_StrifeGunShot
//
//============================================================================

void P_StrifeGunShot (AActor *mo, bool accurate, angle_t pitch)
{
	angle_t angle;
	int damage;

	damage = 4*(pr_sgunshot()%3+1);
	angle = mo->angle;

	if (mo->player != NULL && !accurate)
	{
		angle += pr_sgunshot.Random2() << (20 - mo->player->accuracy * 5 / 100);
	}

	P_LineAttack (mo, angle, PLAYERMISSILERANGE, pitch, damage, NAME_None, RUNTIME_CLASS(AStrifePuff));
}

//============================================================================
//
// A_FireAssaultGun
//
//============================================================================

void A_FireAssaultGun (AActor *self)
{
	bool accurate;

	S_Sound (self, CHAN_WEAPON, "weapons/assaultgun", 1, ATTN_NORM);

	if (self->player != NULL)
	{
		AWeapon *weapon = self->player->ReadyWeapon;
		if (weapon != NULL)
		{
			if (!weapon->DepleteAmmo (weapon->bAltFire))
				return;
		}
		self->player->mo->PlayAttacking2 ();
		accurate = !self->player->refire;
	}
	else
	{
		accurate = true;
	}

	P_StrifeGunShot (self, accurate, P_BulletSlope (self));
}

// Standing variant of the assault gun --------------------------------------

FState AAssaultGunStanding::States[] =
{
	S_NORMAL (RIFL, 'B',   -1, NULL,				&States[0]),
};

IMPLEMENT_ACTOR (AAssaultGunStanding, Strife, 2006, 0)
	PROP_SpawnState (0)
	PROP_StrifeType (189)
	PROP_StrifeTeaserType (183)
	PROP_StrifeTeaserType2 (187)
	// "pulse_rifle" in the Teaser
END_DEFAULTS

//============================================================================
//
// AAssaultGunStanding :: CreateCopy
//
// This is just a different look for the standard assault gun. It is not a
// gun in its own right, so give the player an AssaultGun, not this.
//
//============================================================================

AInventory *AAssaultGunStanding::CreateCopy (AActor *other)
{
	AAssaultGun *copy = Spawn<AAssaultGun> (0,0,0, NO_REPLACE);
	copy->AmmoGive1 = AmmoGive1;
	copy->AmmoGive2 = AmmoGive2;
	GoAwayAndDie ();
	return copy;
}

// Mini-Missile Launcher ----------------------------------------------------

void A_FireMiniMissile (AActor *);

class AMiniMissileLauncher : public AStrifeWeapon
{
	DECLARE_ACTOR (AMiniMissileLauncher, AStrifeWeapon)
};

FState AMiniMissileLauncher::States[] =
{
	S_NORMAL (MMSL, 'A',   -1, NULL, NULL),

#define S_MMISSILE 1
	S_NORMAL (MMIS, 'A',	1, A_WeaponReady,		&States[S_MMISSILE]),

#define S_MMISSILEDOWN (S_MMISSILE+1)
	S_NORMAL (MMIS, 'A',	1, A_Lower,				&States[S_MMISSILEDOWN]),

#define S_MMISSILEUP (S_MMISSILEDOWN+1)
	S_NORMAL (MMIS, 'A',	1, A_Raise,				&States[S_MMISSILEUP]),

#define S_MMISSILEATK (S_MMISSILEUP+1)
	S_NORMAL (MMIS, 'A',	4, A_FireMiniMissile,	&States[S_MMISSILEATK+1]),
	S_NORMAL (MMIS, 'B',	4, A_Light1,			&States[S_MMISSILEATK+2]),
	S_BRIGHT (MMIS, 'C',	5, NULL,				&States[S_MMISSILEATK+3]),
	S_BRIGHT (MMIS, 'D',	2, A_Light2,			&States[S_MMISSILEATK+4]),
	S_BRIGHT (MMIS, 'E',	2, NULL,				&States[S_MMISSILEATK+5]),
	S_BRIGHT (MMIS, 'F',	2, A_Light0,			&States[S_MMISSILEATK+6]),
	S_NORMAL (MMIS, 'F',	0, A_ReFire,			&States[S_MMISSILE])
};

IMPLEMENT_ACTOR (AMiniMissileLauncher, Strife, 2003, 0)
	PROP_Flags (MF_SPECIAL)
	PROP_Flags2 (MF2_FLOORCLIP)
	PROP_SpawnState (0)
	PROP_StrifeType (192)
	PROP_StrifeTeaserType (186)
	PROP_StrifeTeaserType2 (190)

	PROP_Weapon_SelectionOrder (1800)
	PROP_Weapon_AmmoUse1 (1)
	PROP_Weapon_AmmoGive1 (8)
	PROP_Weapon_UpState (S_MMISSILEUP)
	PROP_Weapon_DownState (S_MMISSILEDOWN)
	PROP_Weapon_ReadyState (S_MMISSILE)
	PROP_Weapon_AtkState (S_MMISSILEATK)
	PROP_Weapon_MoveCombatDist (18350080)
	PROP_Weapon_AmmoType1 ("MiniMissiles")

	PROP_Inventory_Icon ("MMSLA0")
	PROP_Tag ("mini_missile_launcher")	// "missile_gun" in the Teaser
	PROP_Inventory_PickupMessage("$TXT_MMLAUNCHER")
END_DEFAULTS

// Rocket Trail -------------------------------------------------------------

class ARocketTrail : public AActor
{
	DECLARE_ACTOR (ARocketTrail, AActor);
};

FState ARocketTrail::States[] =
{
	S_NORMAL (PUFY, 'B', 4, NULL, &States[1]),
	S_NORMAL (PUFY, 'C', 4, NULL, &States[2]),
	S_NORMAL (PUFY, 'B', 4, NULL, &States[3]),
	S_NORMAL (PUFY, 'C', 4, NULL, &States[4]),
	S_NORMAL (PUFY, 'D', 4, NULL, NULL),
};

IMPLEMENT_ACTOR (ARocketTrail, Strife, -1, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_Alpha (TRANSLUC25)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_StrifeType (51)
	PROP_SeeSound ("misc/missileinflight")
END_DEFAULTS

// Rocket Puff --------------------------------------------------------------

class AMiniMissilePuff : public AStrifePuff
{
	DECLARE_STATELESS_ACTOR (AMiniMissilePuff, AStrifePuff)
};

IMPLEMENT_STATELESS_ACTOR (AMiniMissilePuff, Strife, -1, 0)
	PROP_SpawnState (0)
END_DEFAULTS

// Mini Missile -------------------------------------------------------------

void A_RocketInFlight (AActor *);

class AMiniMissile : public AActor
{
	DECLARE_ACTOR (AMiniMissile, AActor);
public:
	void PreExplode ()
	{
		S_StopSound (this, CHAN_VOICE);
		RenderStyle = STYLE_Add;
	}
	void GetExplodeParms (int &damage, int &dist, bool &hurtSource)
	{
		damage = dist = 64;
	}
};

FState AMiniMissile::States[] =
{
	S_BRIGHT (MICR, 'A', 6, A_RocketInFlight,	&States[0]),

	S_BRIGHT (SMIS, 'A', 5, A_ExplodeAndAlert,	&States[2]),
	S_BRIGHT (SMIS, 'B', 5, NULL,				&States[3]),
	S_BRIGHT (SMIS, 'C', 4, NULL,				&States[4]),
	S_BRIGHT (SMIS, 'D', 2, NULL,				&States[5]),
	S_BRIGHT (SMIS, 'E', 2, NULL,				&States[6]),
	S_BRIGHT (SMIS, 'F', 2, NULL,				&States[7]),
	S_BRIGHT (SMIS, 'G', 2, NULL,				NULL),
};

IMPLEMENT_ACTOR (AMiniMissile, Strife, -1, 0)
	PROP_SpawnState (0)
	PROP_DeathState (1)
	PROP_SpeedFixed (20)
	PROP_RadiusFixed (10)
	PROP_HeightFixed (14)
	PROP_Damage (10)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_PCROSS|MF2_IMPACT)
	PROP_Flags4 (MF4_STRIFEDAMAGE)
	PROP_MaxStepHeight (4)
	PROP_StrifeType (99)
	PROP_SeeSound ("weapons/minimissile")
	PROP_DeathSound ("weapons/minimissilehit")
END_DEFAULTS

//============================================================================
//
// A_FireMiniMissile
//
//============================================================================

void A_FireMiniMissile (AActor *self)
{
	player_t *player = self->player;
	angle_t savedangle;

	if (self->player == NULL)
		return;

	AWeapon *weapon = self->player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
	}

	savedangle = self->angle;
	self->angle += pr_minimissile.Random2() << (19 - player->accuracy * 5 / 100);
	player->mo->PlayAttacking2 ();
	P_SpawnPlayerMissile (self, RUNTIME_CLASS(AMiniMissile));
	self->angle = savedangle;
}

//============================================================================
//
// A_RocketInFlight
//
//============================================================================

void A_RocketInFlight (AActor *self)
{
	AActor *trail;

	S_Sound (self, CHAN_VOICE, "misc/missileinflight", 1, ATTN_NORM);
	P_SpawnPuff (RUNTIME_CLASS(AMiniMissilePuff), self->x, self->y, self->z, self->angle - ANGLE_180, 2, PF_HITTHING);
	trail = Spawn<ARocketTrail> (self->x - self->momx, self->y - self->momy, self->z, ALLOW_REPLACE);
	if (trail != NULL)
	{
		trail->momz = FRACUNIT;
	}
}

// Flame Thrower ------------------------------------------------------------

void A_FireFlamer (AActor *);


FState AFlameThrower::States[] =
{
	S_NORMAL (FLAM, 'A', -1, NULL, NULL),

#define S_FLAMER 1
	S_NORMAL (FLMT, 'A', 3, A_WeaponReady,	&States[S_FLAMER+1]),
	S_NORMAL (FLMT, 'B', 3, A_WeaponReady,	&States[S_FLAMER+0]),

#define S_FLAMERDOWN (S_FLAMER+2)
	S_NORMAL (FLMT, 'A', 1, A_Lower,		&States[S_FLAMERDOWN]),

#define S_FLAMERUP (S_FLAMERDOWN+1)
	S_NORMAL (FLMT, 'A', 1, A_Raise,		&States[S_FLAMERUP]),

#define S_FLAMERATK (S_FLAMERUP+1)
	S_NORMAL (FLMF, 'A', 2, A_FireFlamer,	&States[S_FLAMERATK+1]),
	S_NORMAL (FLMF, 'B', 3, A_ReFire,		&States[S_FLAMER])
};

IMPLEMENT_ACTOR (AFlameThrower, Strife, 2005, 0)
	PROP_Flags (MF_SPECIAL)
	PROP_Flags2 (MF2_FLOORCLIP)
	PROP_SpawnState (0)
	PROP_StrifeType (190)
	PROP_StrifeTeaserType (184)
	PROP_StrifeTeaserType2 (188)

	PROP_Weapon_SelectionOrder (2100)
	PROP_Weapon_Flags (WIF_BOT_MELEE)
	PROP_Weapon_Kickback (0)
	PROP_Weapon_AmmoUse1 (1)
	PROP_Weapon_AmmoGive1 (100)
	PROP_Weapon_UpState (S_FLAMERUP)
	PROP_Weapon_DownState (S_FLAMERDOWN)
	PROP_Weapon_ReadyState (S_FLAMER)
	PROP_Weapon_AtkState (S_FLAMERATK)
	PROP_Weapon_UpSound ("weapons/flameidle")
	PROP_Weapon_ReadySound ("weapons/flameidle")
	PROP_Weapon_AmmoType1 ("EnergyPod")
	PROP_Weapon_ProjectileType ("FlameMissile")

	PROP_Inventory_Icon ("FLAMA0")
	PROP_Tag ("flame_thrower")
	PROP_Inventory_PickupMessage("$TXT_FLAMER")
END_DEFAULTS


// Flame Thrower Projectile -------------------------------------------------

void A_FlameDie (AActor *);

FState AFlameMissile::States[] =
{
#define S_FLAME 0
	S_BRIGHT (FRBL, 'A', 3, NULL,			&States[S_FLAME+1]),
	S_BRIGHT (FRBL, 'B', 3, NULL,			&States[S_FLAME+2]),
	S_BRIGHT (FRBL, 'C', 3, A_Countdown,	&States[S_FLAME]),

#define S_FLAMEDIE (S_FLAME+3)
	S_BRIGHT (FRBL, 'D', 5, A_FlameDie,		&States[S_FLAMEDIE+1]),
	S_BRIGHT (FRBL, 'E', 5, NULL,			&States[S_FLAMEDIE+2]),
	S_BRIGHT (FRBL, 'F', 5, NULL,			&States[S_FLAMEDIE+3]),
	S_BRIGHT (FRBL, 'G', 5, NULL,			&States[S_FLAMEDIE+4]),
	S_BRIGHT (FRBL, 'H', 5, NULL,			&States[S_FLAMEDIE+5]),
	S_BRIGHT (FRBL, 'I', 5, NULL,			NULL),
};

IMPLEMENT_ACTOR (AFlameMissile, Strife, -1, 0)
	PROP_SpawnState (S_FLAME)
	PROP_DeathState (S_FLAMEDIE)
	PROP_SpeedFixed (15)
	PROP_RadiusFixed (8)
	PROP_HeightFixed (11)
	PROP_Mass (10)
	PROP_Damage (4)
	PROP_DamageType (NAME_Fire)
	PROP_ReactionTime (8)
	PROP_Flags (MF_NOBLOCKMAP|MF_DROPOFF|MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_PCROSS|MF2_IMPACT)
	PROP_Flags4 (MF4_STRIFEDAMAGE)
	PROP_MaxStepHeight (4)
	PROP_RenderStyle (STYLE_Add)
	PROP_Alpha (OPAQUE)
	PROP_SeeSound ("weapons/flamethrower")
END_DEFAULTS

//============================================================================
//
// A_FlameDie
//
//============================================================================

void A_FlameDie (AActor *self)
{
	self->flags |= MF_NOGRAVITY;
	self->momz = (pr_flamedie() & 3) << FRACBITS;
}

//============================================================================
//
// A_FireFlamer
//
//============================================================================

void A_FireFlamer (AActor *self)
{
	player_t *player = self->player;

	if (player != NULL)
	{
		AWeapon *weapon = self->player->ReadyWeapon;
		if (weapon != NULL)
		{
			if (!weapon->DepleteAmmo (weapon->bAltFire))
				return;
		}
		player->mo->PlayAttacking2 ();
	}

	self->angle += pr_flamethrower.Random2() << 18;
	self = P_SpawnPlayerMissile (self, RUNTIME_CLASS(AFlameMissile));
	if (self != NULL)
	{
		self->momz += 5*FRACUNIT;
	}
}

// Mauler -------------------------------------------------------------------

void A_FireMauler1 (AActor *);
void A_FireMauler2Pre (AActor *);
void A_FireMauler2 (AActor *);
void A_MaulerTorpedoWave (AActor *);

class AMauler : public AStrifeWeapon
{
	DECLARE_ACTOR (AMauler, AStrifeWeapon)
};

FState AMauler::States[] =
{
	S_NORMAL (TRPD, 'A', -1, NULL,				NULL),

#define S_MAULER1 1
	S_NORMAL (MAUL, 'F',  6, A_WeaponReady,		&States[S_MAULER1+1]),
	S_NORMAL (MAUL, 'G',  6, A_WeaponReady,		&States[S_MAULER1+2]),
	S_NORMAL (MAUL, 'H',  6, A_WeaponReady,		&States[S_MAULER1+3]),
	S_NORMAL (MAUL, 'A',  6, A_WeaponReady,		&States[S_MAULER1]),

#define S_MAULER1DOWN (S_MAULER1+4)
	S_NORMAL (MAUL, 'A',  1, A_Lower,			&States[S_MAULER1DOWN]),

#define S_MAULER1UP (S_MAULER1DOWN+1)
	S_NORMAL (MAUL, 'A',  1, A_Raise,			&States[S_MAULER1UP]),

#define S_MAULER1ATK (S_MAULER1UP+1)
	// Why does the firing picture have its own sprite?
	S_BRIGHT (BLSF, 'A',  5, A_FireMauler1,		&States[S_MAULER1ATK+1]),
	S_BRIGHT (MAUL, 'B',  3, A_Light1,			&States[S_MAULER1ATK+2]),
	S_NORMAL (MAUL, 'C',  2, A_Light2,			&States[S_MAULER1ATK+3]),
	S_NORMAL (MAUL, 'D',  2, NULL,				&States[S_MAULER1ATK+4]),
	S_NORMAL (MAUL, 'E',  2, NULL,				&States[S_MAULER1ATK+5]),
	S_NORMAL (MAUL, 'A',  7, A_Light0,			&States[S_MAULER1ATK+6]),
	S_NORMAL (MAUL, 'H',  7, NULL,				&States[S_MAULER1ATK+7]),
	S_NORMAL (MAUL, 'G',  7, A_CheckReload,		&States[S_MAULER1]),

#define S_MAULER2 (S_MAULER1ATK+8)
	S_NORMAL (MAUL, 'I',  7, A_WeaponReady,		&States[S_MAULER2+1]),
	S_NORMAL (MAUL, 'J',  7, A_WeaponReady,		&States[S_MAULER2+2]),
	S_NORMAL (MAUL, 'K',  7, A_WeaponReady,		&States[S_MAULER2+3]),
	S_NORMAL (MAUL, 'L',  7, A_WeaponReady,		&States[S_MAULER2]),

#define S_MAULER2DOWN (S_MAULER2+4)
	S_NORMAL (MAUL, 'I',  1, A_Lower,			&States[S_MAULER2DOWN]),

#define S_MAULER2UP (S_MAULER2DOWN+1)
	S_NORMAL (MAUL, 'I',  1, A_Raise,			&States[S_MAULER2UP]),

#define S_MAULER2ATK (S_MAULER2UP+1)
	S_NORMAL (MAUL, 'I', 20, A_FireMauler2Pre,	&States[S_MAULER2ATK+1]),
	S_NORMAL (MAUL, 'J', 10, A_Light1,			&States[S_MAULER2ATK+2]),
	S_BRIGHT (BLSF, 'A', 10, A_FireMauler2,		&States[S_MAULER2ATK+3]),
	S_BRIGHT (MAUL, 'B', 10, A_Light2,			&States[S_MAULER2ATK+4]),
	S_NORMAL (MAUL, 'C',  2, NULL,				&States[S_MAULER2ATK+5]),
	S_NORMAL (MAUL, 'D',  2, A_Light0,			&States[S_MAULER2ATK+6]),
	S_NORMAL (MAUL, 'E',  2, A_ReFire,			&States[S_MAULER2]),
};

// The scatter version

IMPLEMENT_ACTOR (AMauler, Strife, 2004, 0)
	PROP_Flags (MF_SPECIAL)
	PROP_Flags2 (MF2_FLOORCLIP)
	PROP_SpawnState (0)
	PROP_StrifeType (193)
	PROP_StrifeTeaserType (187)
	PROP_StrifeTeaserType2 (191)

	PROP_Weapon_SelectionOrder (300)
	PROP_Weapon_AmmoUse1 (20)
	PROP_Weapon_AmmoGive1 (40)
	PROP_Weapon_UpState (S_MAULER1UP)
	PROP_Weapon_DownState (S_MAULER1DOWN)
	PROP_Weapon_ReadyState (S_MAULER1)
	PROP_Weapon_AtkState (S_MAULER1ATK)
	PROP_Weapon_MoveCombatDist (15000000)
	PROP_Weapon_AmmoType1 ("EnergyPod")
	PROP_Weapon_SisterType ("Mauler2")

	PROP_Inventory_Icon ("TRPDA0")
	PROP_Tag ("mauler")		// "blaster" in the Teaser
	PROP_Inventory_PickupMessage("$TXT_MAULER")
END_DEFAULTS

// Mauler Torpedo version ---------------------------------------------------

class AMauler2 : public AMauler
{
	DECLARE_STATELESS_ACTOR (AMauler2, AMauler)
};

// The torpedo version

IMPLEMENT_STATELESS_ACTOR (AMauler2, Strife, -1, 0)
	PROP_Weapon_SelectionOrder (3300)
	PROP_Weapon_AmmoUse1 (30)
	PROP_Weapon_AmmoGive1 (0)
	PROP_Weapon_UpState (S_MAULER2UP)
	PROP_Weapon_DownState (S_MAULER2DOWN)
	PROP_Weapon_ReadyState (S_MAULER2)
	PROP_Weapon_AtkState (S_MAULER2ATK)
	PROP_Weapon_MoveCombatDist (10000000)
	PROP_Weapon_AmmoType1 ("EnergyPod")
	PROP_Weapon_SisterType ("Mauler")
	PROP_Weapon_ProjectileType ("MaulerTorpedo")
END_DEFAULTS

// Mauler "Bullet" Puff -----------------------------------------------------

class AMaulerPuff : public AActor
{
	DECLARE_ACTOR (AMaulerPuff, AActor)
};

FState AMaulerPuff::States[] =
{
	S_NORMAL (MPUF, 'A', 5, NULL, &States[1]),
	S_NORMAL (MPUF, 'B', 5, NULL, &States[2]),
	S_NORMAL (POW1, 'A', 4, NULL, &States[3]),
	S_NORMAL (POW1, 'B', 4, NULL, &States[4]),
	S_NORMAL (POW1, 'C', 4, NULL, &States[5]),
	S_NORMAL (POW1, 'D', 4, NULL, &States[6]),
	S_NORMAL (POW1, 'E', 4, NULL, NULL)
};

IMPLEMENT_ACTOR (AMaulerPuff, Strife, -1, 0)
	PROP_SpawnState (0)
	PROP_DamageType (NAME_Disintegrate)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_Flags3 (MF3_PUFFONACTORS)
	PROP_RenderStyle (STYLE_Add)
END_DEFAULTS

// The Mauler's Torpedo -----------------------------------------------------

class AMaulerTorpedo : public AActor
{
	DECLARE_ACTOR (AMaulerTorpedo, AActor)
};

FState AMaulerTorpedo::States[] =
{
	S_BRIGHT (TORP, 'A', 4, NULL,					&States[1]),
	S_BRIGHT (TORP, 'B', 4, NULL,					&States[2]),
	S_BRIGHT (TORP, 'C', 4, NULL,					&States[3]),
	S_BRIGHT (TORP, 'D', 4, NULL,					&States[0]),

	S_BRIGHT (THIT, 'A', 8, NULL,					&States[5]),
	S_BRIGHT (THIT, 'B', 8, NULL,					&States[6]),
	S_BRIGHT (THIT, 'C', 8, A_MaulerTorpedoWave,	&States[7]),
	S_BRIGHT (THIT, 'D', 8, NULL,					&States[8]),
	S_BRIGHT (THIT, 'E', 8, NULL,					NULL)
};

IMPLEMENT_ACTOR (AMaulerTorpedo, Strife, -1, 0)
	PROP_SpawnState (0)
	PROP_DeathState (4)
	PROP_SpeedFixed (20)
	PROP_RadiusFixed (13)
	PROP_HeightFixed (8)
	PROP_Damage (1)
	PROP_DamageType (NAME_Disintegrate)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_PCROSS|MF2_IMPACT)
	PROP_Flags4 (MF4_STRIFEDAMAGE)
	PROP_MaxStepHeight (4)
	PROP_RenderStyle (STYLE_Add)
	PROP_SeeSound ("weapons/mauler2fire")
	PROP_DeathSound ("weapons/mauler2hit")
END_DEFAULTS

// The mini torpedoes shot by the big torpedo --------------------------------

class AMaulerTorpedoWave : public AActor
{
	DECLARE_ACTOR (AMaulerTorpedoWave, AActor)
};

FState AMaulerTorpedoWave::States[] =
{
	S_BRIGHT (TWAV, 'A', 9, NULL, &States[1]),
	S_BRIGHT (TWAV, 'B', 9, NULL, &States[2]),
	S_BRIGHT (TWAV, 'C', 9, NULL, NULL)
};

IMPLEMENT_ACTOR (AMaulerTorpedoWave, Strife, -1, 0)
	PROP_SpawnState (0)
	PROP_DeathState (2)
	PROP_SpeedFixed (35)
	PROP_RadiusFixed (13)
	PROP_HeightFixed (13)
	PROP_Damage (10)
	PROP_DamageType (NAME_Disintegrate)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_PCROSS|MF2_IMPACT)
	PROP_Flags4 (MF4_STRIFEDAMAGE)
	PROP_MaxStepHeight (4)
	PROP_RenderStyle (STYLE_Add)
END_DEFAULTS

//============================================================================
//
// A_FireMauler1
//
// Hey! This is exactly the same as a super shotgun except for the sound
// and the bullet puffs and the disintegration death.
//
//============================================================================

void A_FireMauler1 (AActor *self)
{
	if (self->player != NULL)
	{
		AWeapon *weapon = self->player->ReadyWeapon;
		if (weapon != NULL)
		{
			if (!weapon->DepleteAmmo (weapon->bAltFire))
				return;
		}
		// Strife apparently didn't show the player shooting. Let's fix that.
		self->player->mo->PlayAttacking2 ();
	}

	S_Sound (self, CHAN_WEAPON, "weapons/mauler1", 1, ATTN_NORM);


	int bpitch = P_BulletSlope (self);

	for (int i = 0; i < 20; ++i)
	{
		int damage = 5 * (pr_mauler1() % 3 + 1);
		angle_t angle = self->angle + (pr_mauler1.Random2() << 19);
		int pitch = bpitch + (pr_mauler1.Random2() * 332063);
		
		// Strife used a range of 2112 units for the mauler to signal that
		// it should use a different puff. ZDoom's default range is longer
		// than this, so let's not handicap it by being too faithful to the
		// original.
		P_LineAttack (self, angle, PLAYERMISSILERANGE, pitch, damage, NAME_Disintegrate, RUNTIME_CLASS(AMaulerPuff));
	}
}

//============================================================================
//
// A_FireMauler2Pre
//
// Makes some noise and moves the psprite.
//
//============================================================================

void A_FireMauler2Pre (AActor *self)
{
	S_Sound (self, CHAN_WEAPON, "weapons/mauler2charge", 1, ATTN_NORM);

	if (self->player != NULL)
	{
		self->player->psprites[ps_weapon].sx += pr_mauler2.Random2() << 10;
		self->player->psprites[ps_weapon].sy += pr_mauler2.Random2() << 10;
	}
}

//============================================================================
//
// A_FireMauler2Pre
//
// Fires the torpedo.
//
//============================================================================

void A_FireMauler2 (AActor *self)
{
	if (self->player != NULL)
	{
		AWeapon *weapon = self->player->ReadyWeapon;
		if (weapon != NULL)
		{
			if (!weapon->DepleteAmmo (weapon->bAltFire))
				return;
		}
		self->player->mo->PlayAttacking2 ();
	}
	P_SpawnPlayerMissile (self, RUNTIME_CLASS(AMaulerTorpedo));
	P_DamageMobj (self, self, NULL, 20, self->DamageType);
	P_ThrustMobj (self, self->angle + ANGLE_180, 0x7D000);
}

//============================================================================
//
// A_MaulerTorpedoWave
//
// Launches lots of balls when the torpedo hits something.
//
//============================================================================

AActor *P_SpawnSubMissile (AActor *source, PClass *type, AActor *target);

void A_MaulerTorpedoWave (AActor *self)
{
	fixed_t savedz;
	self->angle += ANGLE_180;

	// If the torpedo hit the ceiling, it should still spawn the wave
	savedz = self->z;
	if (self->ceilingz - self->z < GetDefault<AMaulerTorpedoWave>()->height)
	{
		self->z = self->ceilingz - GetDefault<AMaulerTorpedoWave>()->height;
	}

	for (int i = 0; i < 80; ++i)
	{
		self->angle += ANGLE_45/10;
		P_SpawnSubMissile (self, RUNTIME_CLASS(AMaulerTorpedoWave), self->target);
	}
	self->z = savedz;
}

AActor *P_SpawnSubMissile (AActor *source, PClass *type, AActor *target)
{
	AActor *other = Spawn (type, source->x, source->y, source->z, ALLOW_REPLACE);

	if (other == NULL)
	{
		return NULL;
	}

	other->target = target;
	other->angle = source->angle;

	other->momx = FixedMul (other->Speed, finecosine[source->angle >> ANGLETOFINESHIFT]);
	other->momy = FixedMul (other->Speed, finesine[source->angle >> ANGLETOFINESHIFT]);

	if (P_CheckMissileSpawn (other))
	{
		angle_t pitch = P_AimLineAttack (source, source->angle, 1024*FRACUNIT);
		other->momz = FixedMul (-finesine[pitch>>ANGLETOFINESHIFT], other->Speed);
		return other;
	}
	return NULL;
}

// High-Explosive Grenade ---------------------------------------------------

class AHEGrenade : public AActor
{
	DECLARE_ACTOR (AHEGrenade, AActor)
public:
	void PreExplode ();
	void GetExplodeParms (int &damage, int &dist, bool &hurtSource);
};

FState AHEGrenade::States[] =
{
	S_NORMAL (GRAP, 'A', 3, A_Countdown,			&States[1]),
	S_NORMAL (GRAP, 'B', 3, A_Countdown,			&States[0]),

	S_BRIGHT (BNG4, 'A', 2, A_ExplodeAndAlert,		&States[3]),
	S_BRIGHT (BNG4, 'B', 3, NULL,					&States[4]),
	S_BRIGHT (BNG4, 'C', 3, NULL,					&States[5]),
	S_BRIGHT (BNG4, 'D', 3, NULL,					&States[6]),
	S_BRIGHT (BNG4, 'E', 3, NULL,					&States[7]),
	S_BRIGHT (BNG4, 'F', 3, NULL,					&States[8]),
	S_BRIGHT (BNG4, 'G', 3, NULL,					&States[9]),
	S_BRIGHT (BNG4, 'H', 3, NULL,					&States[10]),
	S_BRIGHT (BNG4, 'I', 3, NULL,					&States[11]),
	S_BRIGHT (BNG4, 'J', 3, NULL,					&States[12]),
	S_BRIGHT (BNG4, 'K', 3, NULL,					&States[13]),
	S_BRIGHT (BNG4, 'L', 3, NULL,					&States[14]),
	S_BRIGHT (BNG4, 'M', 3, NULL,					&States[15]),
	S_BRIGHT (BNG4, 'N', 3, NULL,					NULL)
};

IMPLEMENT_ACTOR (AHEGrenade, Strife, -1, 0)
	PROP_SpawnState (0)
	PROP_DeathState (2)
	PROP_SpeedFixed (15)
	PROP_RadiusFixed (13)
	PROP_HeightFixed (13)
	PROP_Mass (20)
	PROP_Damage (1)
	PROP_ReactionTime (30)
	PROP_Flags (MF_NOBLOCKMAP|MF_DROPOFF|MF_MISSILE)
	PROP_Flags2 (MF2_FLOORCLIP|MF2_NOTELEPORT|MF2_PCROSS|MF2_IMPACT|MF2_DOOMBOUNCE)
	PROP_Flags3 (MF3_CANBOUNCEWATER)
	PROP_Flags4 (MF4_STRIFEDAMAGE|MF4_NOBOUNCESOUND)
	PROP_Flags5 (MF5_BOUNCEONACTORS|MF5_EXPLODEONWATER)
	PROP_MaxStepHeight (4)
	PROP_StrifeType (106)
	PROP_BounceFactor((FRACUNIT*5/10))
	PROP_BounceCount(2)
	PROP_SeeSound ("weapons/hegrenadeshoot")
	PROP_DeathSound ("weapons/hegrenadebang")
END_DEFAULTS

void AHEGrenade::PreExplode ()
{
	RenderStyle = STYLE_Add;
	flags |= MF_NOGRAVITY;
}

void AHEGrenade::GetExplodeParms (int &damage, int &dist, bool &hurtSource)
{
	damage = dist = 192;
	hurtSource = true;
}

// White Phosphorous Grenade ------------------------------------------------

void A_SpawnBurn (AActor *);

class APhosphorousGrenade : public AActor
{
	DECLARE_ACTOR (APhosphorousGrenade, AActor)
};

FState APhosphorousGrenade::States[] =
{
	S_NORMAL (GRIN, 'A', 3, A_Countdown,	&States[1]),
	S_NORMAL (GRIN, 'B', 3, A_Countdown,	&States[0]),

	S_BRIGHT (BNG3, 'A', 2, A_SpawnBurn,	NULL)
};

IMPLEMENT_ACTOR (APhosphorousGrenade, Strife, -1, 0)
	PROP_SpawnState (0)
	PROP_DeathState (2)
	PROP_SpeedFixed (15)
	PROP_RadiusFixed (13)
	PROP_HeightFixed (13)
	PROP_Mass (20)
	PROP_Damage (1)
	PROP_ReactionTime (40)
	PROP_Flags (MF_NOBLOCKMAP|MF_DROPOFF|MF_MISSILE)
	PROP_Flags2 (MF2_FLOORCLIP|MF2_NOTELEPORT|MF2_PCROSS|MF2_IMPACT|MF2_DOOMBOUNCE)
	PROP_Flags3 (MF3_CANBOUNCEWATER)
	PROP_Flags4 (MF4_STRIFEDAMAGE|MF4_NOBOUNCESOUND)
	PROP_Flags5 (MF5_BOUNCEONACTORS|MF5_EXPLODEONWATER)
	PROP_MaxStepHeight (4)
	PROP_StrifeType (107)
	PROP_BounceFactor((FRACUNIT*5/10))
	PROP_BounceCount(2)
	PROP_SeeSound ("weapons/phgrenadeshoot")
	PROP_DeathSound ("weapons/phgrenadebang")
END_DEFAULTS

// Fire from the Phoshorous Grenade -----------------------------------------

void A_Burnination (AActor *self);

class APhosphorousFire : public AActor
{
	DECLARE_ACTOR (APhosphorousFire, AActor)
public:
	int DoSpecialDamage (AActor *target, int damage);
};

void A_BurnArea (AActor *);

FState APhosphorousFire::States[] =
{
#define S_BURNINATION 0
	S_BRIGHT (BNG3, 'B',	2, A_BurnArea,			&States[S_BURNINATION+1]),
	S_BRIGHT (BNG3, 'C',	2, A_Countdown,			&States[S_BURNINATION+2]),
	S_BRIGHT (FLBE, 'A',	2, A_Burnination,		&States[S_BURNINATION+3]),
	S_BRIGHT (FLBE, 'B',	2, A_Countdown,			&States[S_BURNINATION+4]),
	S_BRIGHT (FLBE, 'C',	2, A_BurnArea,			&States[S_BURNINATION+5]),
	S_BRIGHT (FLBE, 'D',	3, A_Countdown,			&States[S_BURNINATION+6]),
	S_BRIGHT (FLBE, 'E',	3, A_BurnArea,			&States[S_BURNINATION+7]),
	S_BRIGHT (FLBE, 'F',	3, A_Countdown,			&States[S_BURNINATION+8]),
	S_BRIGHT (FLBE, 'G',	3, A_Burnination,		&States[S_BURNINATION+5]),

#define S_BURNDWINDLE (S_BURNINATION+9)
	S_BRIGHT (FLBE, 'H',	2, NULL,				&States[S_BURNDWINDLE+1]),
	S_BRIGHT (FLBE, 'I',	2, A_Burnination,		&States[S_BURNDWINDLE+2]),
	S_BRIGHT (FLBE, 'J',	2, NULL,				&States[S_BURNDWINDLE+3]),
	S_BRIGHT (FLBE, 'K',	2, NULL,				NULL),
};

IMPLEMENT_ACTOR (APhosphorousFire, Strife, -1, 0)
	PROP_SpawnState (S_BURNINATION)
	PROP_DeathState (S_BURNDWINDLE)
	PROP_ReactionTime (120)
	PROP_DamageType (NAME_Fire)
	PROP_Flags (MF_NOBLOCKMAP)
	PROP_Flags2 (MF2_FLOORCLIP|MF2_NOTELEPORT|MF2_NODMGTHRUST)
	PROP_RenderStyle (STYLE_Add)
END_DEFAULTS

int APhosphorousFire::DoSpecialDamage (AActor *target, int damage)
{
	if (target->flags & MF_NOBLOOD)
	{
		return damage / 2;
	}
	return Super::DoSpecialDamage (target, damage);
}

void A_SpawnBurn (AActor *self)
{
	Spawn<APhosphorousFire> (self->x, self->y, self->z, ALLOW_REPLACE);
}

void A_BurnArea (AActor *self)
{
	P_RadiusAttack (self, self->target, 128, 128, self->DamageType, true);
}

void A_Burnination (AActor *self)
{
	self->momz -= 8*FRACUNIT;
	self->momx += (pr_phburn.Random2 (3)) << FRACBITS;
	self->momy += (pr_phburn.Random2 (3)) << FRACBITS;
	S_Sound (self, CHAN_VOICE, "world/largefire", 1, ATTN_NORM);

	// Only the main fire spawns more.
	if (!(self->flags & MF_DROPPED))
	{
		// Original x and y offsets seemed to be like this:
		//		x + (((pr_phburn() + 12) & 31) << FRACBITS);
		//
		// But that creates a lop-sided burn because it won't use negative offsets.
		int xofs, xrand = pr_phburn();
		int yofs, yrand = pr_phburn();

		// Adding 12 is pointless if you're going to mask it afterward.
		xofs = xrand & 31;
		if (xrand & 128)
		{
			xofs = -xofs;
		}

		yofs = yrand & 31;
		if (yrand & 128)
		{
			yofs = -yofs;
		}

		fixed_t x = self->x + (xofs << FRACBITS);
		fixed_t y = self->y + (yofs << FRACBITS);
		sector_t * sector = P_PointInSector(x, y);

		// The sector's floor is too high so spawn the flame elsewhere.
		if (sector->floorplane.ZatPoint(x, y) > self->z + self->MaxStepHeight)
		{
			x = self->x;
			y = self->y;
		}

		AActor *drop = Spawn<APhosphorousFire> (
			x, y,
			self->z + 4*FRACUNIT, ALLOW_REPLACE);
		if (drop != NULL)
		{
			drop->momx = self->momx + ((pr_phburn.Random2 (7)) << FRACBITS);
			drop->momy = self->momy + ((pr_phburn.Random2 (7)) << FRACBITS);
			drop->momz = self->momz - FRACUNIT;
			drop->reactiontime = (pr_phburn() & 3) + 2;
			drop->flags |= MF_DROPPED;
		}
	}
}

// High-Explosive Grenade Launcher ------------------------------------------

void A_FireGrenade (AActor *);

class AStrifeGrenadeLauncher : public AStrifeWeapon
{
	DECLARE_ACTOR (AStrifeGrenadeLauncher, AStrifeWeapon)
};

FState AStrifeGrenadeLauncher::States[] =
{
#define S_HEPICKUP 0
	S_NORMAL (GRND, 'A',   -1, NULL,					NULL),

#define S_HEGRENADE (S_HEPICKUP+1)
	S_NORMAL (GREN, 'A',	1, A_WeaponReady,			&States[S_HEGRENADE]),

#define S_HEGRENADE_DOWN (S_HEGRENADE+1)
	S_NORMAL (GREN, 'A',	1, A_Lower,					&States[S_HEGRENADE_DOWN]),

#define S_HEGRENADE_UP (S_HEGRENADE_DOWN+1)
	S_NORMAL (GREN, 'A',	1, A_Raise,					&States[S_HEGRENADE_UP]),

#define S_HEGRENADE_ATK (S_HEGRENADE_UP+1)
	S_NORMAL (GREN, 'A',	5, A_FireGrenade,			&States[S_HEGRENADE_ATK+1]),
	S_NORMAL (GREN, 'B',   10, NULL,					&States[S_HEGRENADE_ATK+2]),
	S_NORMAL (GREN, 'A',	5, A_FireGrenade,			&States[S_HEGRENADE_ATK+3]),
	S_NORMAL (GREN, 'C',   10, NULL,					&States[S_HEGRENADE_ATK+4]),
	S_NORMAL (GREN, 'A',	0, A_ReFire,				&States[S_HEGRENADE]),

#define S_HEGRENADE_FLASH (S_HEGRENADE_ATK+5)
	S_BRIGHT (GREF, 'A',	5, A_Light1,				&AWeapon::States[S_LIGHTDONE]),
	S_NORMAL (GREF, 'A',   10, A_Light0,				&AWeapon::States[S_LIGHTDONE]),
	S_BRIGHT (GREF, 'B',	5, A_Light2,				&AWeapon::States[S_LIGHTDONE]),


#define S_PHGRENADE (S_HEGRENADE_FLASH+3)
	S_NORMAL (GREN, 'D',	1, A_WeaponReady,			&States[S_PHGRENADE]),

#define S_PHGRENADE_DOWN (S_PHGRENADE+1)
	S_NORMAL (GREN, 'D',	1, A_Lower,					&States[S_PHGRENADE_DOWN]),

#define S_PHGRENADE_UP (S_PHGRENADE_DOWN+1)
	S_NORMAL (GREN, 'D',	1, A_Raise,					&States[S_PHGRENADE_UP]),

#define S_PHGRENADE_ATK (S_PHGRENADE_UP+1)
	S_NORMAL (GREN, 'D',	5, A_FireGrenade,			&States[S_PHGRENADE_ATK+1]),
	S_NORMAL (GREN, 'E',   10, NULL,					&States[S_PHGRENADE_ATK+2]),
	S_NORMAL (GREN, 'D',	5, A_FireGrenade,			&States[S_PHGRENADE_ATK+3]),
	S_NORMAL (GREN, 'F',   10, NULL,					&States[S_PHGRENADE_ATK+4]),
	S_NORMAL (GREN, 'A',	0, A_ReFire,				&States[S_PHGRENADE]),

#define S_PHGRENADE_FLASH (S_PHGRENADE_ATK+5)
	S_BRIGHT (GREF, 'C',	5, A_Light1,				&AWeapon::States[S_LIGHTDONE]),
	S_NORMAL (GREF, 'C',   10, A_Light0,				&AWeapon::States[S_LIGHTDONE]),
	S_BRIGHT (GREF, 'D',	5, A_Light2,				&AWeapon::States[S_LIGHTDONE]),
};

IMPLEMENT_ACTOR (AStrifeGrenadeLauncher, Strife, 154, 0)
	PROP_Flags (MF_SPECIAL)
	PROP_Flags2 (MF2_FLOORCLIP)
	PROP_SpawnState (S_HEPICKUP)
	PROP_StrifeType (195)
	PROP_StrifeTeaserType (189)
	PROP_StrifeTeaserType2 (193)

	PROP_Weapon_SelectionOrder (2400)
	PROP_Weapon_AmmoUse1 (1)
	PROP_Weapon_AmmoGive1 (12)
	PROP_Weapon_UpState (S_HEGRENADE_UP)
	PROP_Weapon_DownState (S_HEGRENADE_DOWN)
	PROP_Weapon_ReadyState (S_HEGRENADE)
	PROP_Weapon_AtkState (S_HEGRENADE_ATK)
	PROP_Weapon_FlashState (S_HEGRENADE_FLASH)
	PROP_Weapon_MoveCombatDist (18350080)
	PROP_Weapon_AmmoType1 ("HEGrenadeRounds")
	PROP_Weapon_SisterType ("StrifeGrenadeLauncher2")
	PROP_Weapon_ProjectileType ("HEGrenade")

	PROP_Inventory_Icon ("GRNDA0")
	PROP_Tag ("Grenade_launcher")
	PROP_Inventory_PickupMessage("$TXT_GLAUNCHER")
END_DEFAULTS

// White Phosphorous Grenade Launcher ---------------------------------------

class AStrifeGrenadeLauncher2 : public AStrifeGrenadeLauncher
{
	DECLARE_STATELESS_ACTOR (AStrifeGrenadeLauncher2, AStrifeGrenadeLauncher)
};

IMPLEMENT_STATELESS_ACTOR (AStrifeGrenadeLauncher2, Strife, -1, 0)
	PROP_Weapon_SelectionOrder (3200)
	PROP_Weapon_AmmoUse1 (1)
	PROP_Weapon_AmmoGive1 (0)
	PROP_Weapon_UpState (S_PHGRENADE_UP)
	PROP_Weapon_DownState (S_PHGRENADE_DOWN)
	PROP_Weapon_ReadyState (S_PHGRENADE)
	PROP_Weapon_AtkState (S_PHGRENADE_ATK)
	PROP_Weapon_FlashState (S_PHGRENADE_FLASH)
	PROP_Weapon_AmmoType1 ("PhosphorusGrenadeRounds")
	PROP_Weapon_SisterType ("StrifeGrenadeLauncher")
	PROP_Weapon_ProjectileType ("PhosphorousGrenade")
END_DEFAULTS

//============================================================================
//
// A_FireGrenade
//
//============================================================================

void A_FireGrenade (AActor *self)
{
	PClass *grenadetype;
	player_t *player = self->player;
	AActor *grenade;
	angle_t an;
	fixed_t tworadii;
	AWeapon *weapon;

	if (player == NULL)
		return;

	if ((weapon = player->ReadyWeapon) == NULL)
		return;

	if (weapon->GetClass() == RUNTIME_CLASS(AStrifeGrenadeLauncher))
	{
		grenadetype = RUNTIME_CLASS(AHEGrenade);
	}
	else
	{
		grenadetype = RUNTIME_CLASS(APhosphorousGrenade);
	}
	if (!weapon->DepleteAmmo (weapon->bAltFire))
		return;

	// Make it flash
	P_SetPsprite (player, ps_flash, weapon->FindState(NAME_Flash) +
		(player->psprites[ps_weapon].state - weapon->GetAtkState(false)));

	self->z += 32*FRACUNIT;
	grenade = P_SpawnSubMissile (self, grenadetype, self);
	self->z -= 32*FRACUNIT;
	if (grenade == NULL)
		return;

	if (grenade->SeeSound != 0)
	{
		S_Sound (grenade, CHAN_VOICE, grenade->SeeSound, 1, ATTN_NORM);
	}

	grenade->momz = FixedMul (finetangent[FINEANGLES/4-(self->pitch>>ANGLETOFINESHIFT)], grenade->Speed) + 8*FRACUNIT;

	an = self->angle >> ANGLETOFINESHIFT;
	tworadii = self->radius + grenade->radius;
	grenade->x += FixedMul (finecosine[an], tworadii);
	grenade->y += FixedMul (finesine[an], tworadii);

	if (weapon->GetAtkState(false) == player->psprites[ps_weapon].state)
	{
		an = self->angle - ANGLE_90;
	}
	else
	{
		an = self->angle + ANGLE_90;
	}
	an >>= ANGLETOFINESHIFT;
	grenade->x += FixedMul (finecosine[an], 15*FRACUNIT);
	grenade->y += FixedMul (finesine[an], 15*FRACUNIT);
}

// The Almighty Sigil! ------------------------------------------------------

void A_SelectPiece (AActor *);
void A_SelectSigilView (AActor *);
void A_SelectSigilDown (AActor *);
void A_SelectSigilAttack (AActor *);
void A_SigilCharge (AActor *);
void A_FireSigil1 (AActor *);
void A_FireSigil2 (AActor *);
void A_FireSigil3 (AActor *);
void A_FireSigil4 (AActor *);
void A_FireSigil5 (AActor *);
void A_LightInverse (AActor *);

FState ASigil::States[] =
{
	S_NORMAL (SIGL, 'A',  0, NULL,					&States[1]),
	S_NORMAL (SIGL, 'A', -1, A_SelectPiece,			NULL),
	S_NORMAL (SIGL, 'B', -1, NULL,					NULL),
	S_NORMAL (SIGL, 'C', -1, NULL,					NULL),
	S_NORMAL (SIGL, 'D', -1, NULL,					NULL),
	S_NORMAL (SIGL, 'E', -1, NULL,					NULL),

#define S_SIGIL 6
	S_BRIGHT (SIGH, 'A',  0, A_SelectSigilView,		&States[S_SIGIL+1]),
	S_BRIGHT (SIGH, 'A',  1, A_WeaponReady,			&States[S_SIGIL+1]),
	S_BRIGHT (SIGH, 'B',  1, A_WeaponReady,			&States[S_SIGIL+2]),
	S_BRIGHT (SIGH, 'C',  1, A_WeaponReady,			&States[S_SIGIL+3]),
	S_BRIGHT (SIGH, 'D',  1, A_WeaponReady,			&States[S_SIGIL+4]),
	S_BRIGHT (SIGH, 'E',  1, A_WeaponReady,			&States[S_SIGIL+5]),

#define S_SIGILDOWN (S_SIGIL+6)
	S_BRIGHT (SIGH, 'A',  0, A_SelectSigilDown,		&States[S_SIGILDOWN+1]),
	S_BRIGHT (SIGH, 'A',  1, A_Lower,				&States[S_SIGILDOWN+1]),
	S_BRIGHT (SIGH, 'B',  1, A_Lower,				&States[S_SIGILDOWN+2]),
	S_BRIGHT (SIGH, 'C',  1, A_Lower,				&States[S_SIGILDOWN+3]),
	S_BRIGHT (SIGH, 'D',  1, A_Lower,				&States[S_SIGILDOWN+4]),
	S_BRIGHT (SIGH, 'E',  1, A_Lower,				&States[S_SIGILDOWN+5]),

#define S_SIGILUP (S_SIGILDOWN+6)
	S_BRIGHT (SIGH, 'A',  0, A_SelectSigilView,		&States[S_SIGILUP+1]),
	S_BRIGHT (SIGH, 'A',  1, A_Raise,				&States[S_SIGILUP+1]),
	S_BRIGHT (SIGH, 'B',  1, A_Raise,				&States[S_SIGILUP+2]),
	S_BRIGHT (SIGH, 'C',  1, A_Raise,				&States[S_SIGILUP+3]),
	S_BRIGHT (SIGH, 'D',  1, A_Raise,				&States[S_SIGILUP+4]),
	S_BRIGHT (SIGH, 'E',  1, A_Raise,				&States[S_SIGILUP+5]),

#define S_SIGILATK (S_SIGILUP+6)
	S_BRIGHT (SIGH, 'A',  0, A_SelectSigilAttack,	&States[S_SIGILATK+1]),

	S_BRIGHT (SIGH, 'A', 18, A_SigilCharge,			&States[S_SIGILATK+2]),
	S_BRIGHT (SIGH, 'A',  3, A_GunFlash,			&States[S_SIGILATK+3]),
	S_NORMAL (SIGH, 'A', 10, A_FireSigil1,			&States[S_SIGILATK+4]),
	S_NORMAL (SIGH, 'A',  5, NULL,					&States[S_SIGIL]),

	S_BRIGHT (SIGH, 'B', 18, A_SigilCharge,			&States[S_SIGILATK+6]),
	S_BRIGHT (SIGH, 'B',  3, A_GunFlash,			&States[S_SIGILATK+7]),
	S_NORMAL (SIGH, 'B', 10, A_FireSigil2,			&States[S_SIGILATK+8]),
	S_NORMAL (SIGH, 'B',  5, NULL,					&States[S_SIGIL]),

	S_BRIGHT (SIGH, 'C', 18, A_SigilCharge,			&States[S_SIGILATK+10]),
	S_BRIGHT (SIGH, 'C',  3, A_GunFlash,			&States[S_SIGILATK+11]),
	S_NORMAL (SIGH, 'C', 10, A_FireSigil3,			&States[S_SIGILATK+12]),
	S_NORMAL (SIGH, 'C',  5, NULL,					&States[S_SIGIL]),

	S_BRIGHT (SIGH, 'D', 18, A_SigilCharge,			&States[S_SIGILATK+14]),
	S_BRIGHT (SIGH, 'D',  3, A_GunFlash,			&States[S_SIGILATK+15]),
	S_NORMAL (SIGH, 'D', 10, A_FireSigil4,			&States[S_SIGILATK+16]),
	S_NORMAL (SIGH, 'D',  5, NULL,					&States[S_SIGIL]),

	S_BRIGHT (SIGH, 'E', 18, A_SigilCharge,			&States[S_SIGILATK+18]),
	S_BRIGHT (SIGH, 'E',  3, A_GunFlash,			&States[S_SIGILATK+19]),
	S_NORMAL (SIGH, 'E', 10, A_FireSigil5,			&States[S_SIGILATK+20]),
	S_NORMAL (SIGH, 'E',  5, NULL,					&States[S_SIGIL]),

#define S_SIGILFLASH (S_SIGILATK+1+4*5)
	S_BRIGHT (SIGF, 'A',  4, A_Light2,				&States[S_SIGILFLASH+1]),
	S_BRIGHT (SIGF, 'B',  6, A_LightInverse,		&States[S_SIGILFLASH+2]),
	S_BRIGHT (SIGF, 'C',  4, A_Light1,				&States[S_SIGILFLASH+3]),
	S_BRIGHT (SIGF, 'C',  0, A_Light0,				NULL)
};

IMPLEMENT_ACTOR (ASigil, Strife, -1, 0)
	PROP_Weapon_SelectionOrder (4000)
	PROP_Weapon_UpState (S_SIGILUP)
	PROP_Weapon_DownState (S_SIGILDOWN)
	PROP_Weapon_ReadyState (S_SIGIL)
	PROP_Weapon_AtkState (S_SIGILATK)
	PROP_Weapon_FlashState (S_SIGILFLASH)
	PROP_Sigil_NumPieces (1)
	PROP_SpawnState (0)
	PROP_Flags (MF_SPECIAL)
	PROP_Flags2 (MF2_FLOORCLIP)
	PROP_Weapon_FlagsSet (WIF_CHEATNOTWEAPON)
	PROP_Inventory_PickupSound("weapons/sigilcharge")
	PROP_Tag ("SIGIL")
	PROP_Inventory_Icon ("I_SGL1")
	PROP_Inventory_PickupMessage("$TXT_SIGIL")
END_DEFAULTS

// Sigil 1 ------------------------------------------------------------------

class ASigil1 : public ASigil
{
	DECLARE_STATELESS_ACTOR (ASigil1, ASigil)
};

IMPLEMENT_STATELESS_ACTOR (ASigil1, Strife, 77, 0)
	PROP_Sigil_NumPieces (1)
	PROP_StrifeType (196)
	PROP_StrifeTeaserType (190)
	PROP_StrifeTeaserType2 (194)
	PROP_Tag ("SIGIL")
	PROP_Inventory_Icon ("I_SGL1")
END_DEFAULTS

// Sigil 2 ------------------------------------------------------------------

class ASigil2 : public ASigil
{
	DECLARE_STATELESS_ACTOR (ASigil2, ASigil)
};

IMPLEMENT_STATELESS_ACTOR (ASigil2, Strife, 78, 0)
	PROP_Sigil_NumPieces (2)
	PROP_StrifeType (197)
	PROP_StrifeTeaserType (191)
	PROP_StrifeTeaserType2 (195)
	PROP_Tag ("SIGIL")
	PROP_Inventory_Icon ("I_SGL2")
END_DEFAULTS

// Sigil 3 ------------------------------------------------------------------

class ASigil3 : public ASigil
{
	DECLARE_STATELESS_ACTOR (ASigil3, ASigil)
};

IMPLEMENT_STATELESS_ACTOR (ASigil3, Strife, 79, 0)
	PROP_Sigil_NumPieces (3)
	PROP_StrifeType (198)
	PROP_StrifeTeaserType (192)
	PROP_StrifeTeaserType2 (196)
	PROP_Tag ("SIGIL")
	PROP_Inventory_Icon ("I_SGL3")
END_DEFAULTS

// Sigil 4 ------------------------------------------------------------------

class ASigil4 : public ASigil
{
	DECLARE_STATELESS_ACTOR (ASigil4, ASigil)
};

IMPLEMENT_STATELESS_ACTOR (ASigil4, Strife, 80, 0)
	PROP_Sigil_NumPieces (4)
	PROP_StrifeType (199)
	PROP_StrifeTeaserType (193)
	PROP_StrifeTeaserType2 (197)
	PROP_Tag ("SIGIL")
	PROP_Inventory_Icon ("I_SGL4")
END_DEFAULTS

// Sigil 5 ------------------------------------------------------------------

class ASigil5 : public ASigil
{
	DECLARE_STATELESS_ACTOR (ASigil5, ASigil)
};

IMPLEMENT_STATELESS_ACTOR (ASigil5, Strife, 81, 0)
	PROP_Sigil_NumPieces (5)
	PROP_StrifeType (200)
	PROP_StrifeTeaserType (194)
	PROP_StrifeTeaserType2 (198)
	PROP_Tag ("SIGIL")
	PROP_Inventory_Icon ("I_SGL5")
END_DEFAULTS

//============================================================================
//
// ASigil :: Serialize
//
//============================================================================

void ASigil::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << NumPieces << DownPieces;
}

//============================================================================
//
// ASigil :: HandlePickup
//
//============================================================================

bool ASigil::HandlePickup (AInventory *item)
{
	if (item->IsKindOf (RUNTIME_CLASS(ASigil)))
	{
		int otherPieces = static_cast<ASigil*>(item)->NumPieces;
		if (otherPieces > NumPieces)
		{
			item->ItemFlags |= IF_PICKUPGOOD;
			Icon = item->Icon;
			// If the player is holding the Sigil right now, drop it and bring
			// it back with the new piece(s) in view.
			if (Owner->player != NULL && Owner->player->ReadyWeapon == this)
			{
				DownPieces = NumPieces;
				Owner->player->PendingWeapon = this;
			}
			NumPieces = otherPieces;
		}
		return true;
	}
	if (Inventory != NULL)
	{
		return Inventory->HandlePickup (item);
	}
	return false;
}

//============================================================================
//
// ASigil :: CreateCopy
//
//============================================================================

AInventory *ASigil::CreateCopy (AActor *other)
{
	ASigil *copy = Spawn<ASigil> (0,0,0, NO_REPLACE);
	copy->Amount = Amount;
	copy->MaxAmount = MaxAmount;
	copy->NumPieces = NumPieces;
	copy->Icon = Icon;
	GoAwayAndDie ();
	return copy;
}

//============================================================================
//
// A_SelectPiece
//
// Decide which sprite frame this Sigil should use as an item, based on how
// many pieces it represents.
//
//============================================================================

void A_SelectPiece (AActor *self)
{
	int pieces = MIN (static_cast<ASigil*>(self)->NumPieces, 5);

	if (pieces > 1)
	{
		self->SetState (&ASigil::States[pieces]);
	}
}

//============================================================================
//
// A_SelectSigilView
//
// Decide which first-person frame this Sigil should show, based on how many
// pieces it represents. Strife did this by selecting a flash that looked like
// the Sigil whenever you switched to it and at the end of an attack. I have
// chosen to make the weapon sprite choose the correct frame and let the flash
// be a regular flash. It means I need to use more states, but I think it's
// worth it.
//
//============================================================================

void A_SelectSigilView (AActor *self)
{
	int pieces;

	if (self->player == NULL)
	{
		return;
	}
	pieces = static_cast<ASigil*>(self->player->ReadyWeapon)->NumPieces;
	P_SetPsprite (self->player, ps_weapon,
		self->player->psprites[ps_weapon].state + pieces);
}

//============================================================================
//
// A_SelectSigilDown
//
// Same as A_SelectSigilView, except it uses DownPieces. This is so that when
// you pick up a Sigil, the old one will drop and *then* change to the new
// one.
//
//============================================================================

void A_SelectSigilDown (AActor *self)
{
	int pieces;

	if (self->player == NULL)
	{
		return;
	}
	pieces = static_cast<ASigil*>(self->player->ReadyWeapon)->DownPieces;
	static_cast<ASigil*>(self->player->ReadyWeapon)->DownPieces = 0;
	if (pieces == 0)
	{
		pieces = static_cast<ASigil*>(self->player->ReadyWeapon)->NumPieces;
	}
	P_SetPsprite (self->player, ps_weapon,
		self->player->psprites[ps_weapon].state + pieces);
}

//============================================================================
//
// A_SelectSigilAttack
//
// Same as A_SelectSigilView, but used just before attacking.
//
//============================================================================

void A_SelectSigilAttack (AActor *self)
{
	int pieces;

	if (self->player == NULL)
	{
		return;
	}
	pieces = static_cast<ASigil*>(self->player->ReadyWeapon)->NumPieces;
	P_SetPsprite (self->player, ps_weapon,
		self->player->psprites[ps_weapon].state + 4*pieces - 3);
}

//============================================================================
//
// A_SigilCharge
//
//============================================================================

void A_SigilCharge (AActor *self)
{
	S_Sound (self, CHAN_WEAPON, "weapons/sigilcharge", 1, ATTN_NORM);
	if (self->player != NULL)
	{
		self->player->extralight = 2;
	}
}

//============================================================================
//
// A_LightInverse
//
//============================================================================

void A_LightInverse (AActor *actor)
{
	if (actor->player != NULL)
	{
		actor->player->extralight = INT_MIN;
	}
}

//============================================================================
//
// A_FireSigil1
//
//============================================================================

void A_FireSigil1 (AActor *actor)
{
	AActor *spot;
	player_t *player = actor->player;
	AActor *linetarget;

	if (player == NULL || player->ReadyWeapon == NULL)
		return;

	P_DamageMobj (actor, actor, NULL, 1*4, 0, DMG_NO_ARMOR);
	S_Sound (actor, CHAN_WEAPON, "weapons/sigilcharge", 1, ATTN_NORM);

	P_BulletSlope (actor, &linetarget);
	if (linetarget != NULL)
	{
		spot = Spawn<ASpectralLightningSpot> (linetarget->x, linetarget->y, ONFLOORZ, ALLOW_REPLACE);
		if (spot != NULL)
		{
			spot->tracer = linetarget;
		}
	}
	else
	{
		spot = Spawn<ASpectralLightningSpot> (actor->x, actor->y, actor->z, ALLOW_REPLACE);
		if (spot != NULL)
		{
			spot->momx += 28 * finecosine[actor->angle >> ANGLETOFINESHIFT];
			spot->momy += 28 * finesine[actor->angle >> ANGLETOFINESHIFT];
		}
	}
	if (spot != NULL)
	{
		spot->health = -1;
		spot->target = actor;
	}
}

//============================================================================
//
// A_FireSigil2
//
//============================================================================

void A_FireSigil2 (AActor *actor)
{
	AActor *spot;
	player_t *player = actor->player;

	if (player == NULL || player->ReadyWeapon == NULL)
		return;

	P_DamageMobj (actor, actor, NULL, 2*4, 0, DMG_NO_ARMOR);
	S_Sound (actor, CHAN_WEAPON, "weapons/sigilcharge", 1, ATTN_NORM);

	spot = P_SpawnPlayerMissile (actor, RUNTIME_CLASS(ASpectralLightningH1));
	if (spot != NULL)
	{
		spot->health = -1;
	}
}

//============================================================================
//
// A_FireSigil3
//
//============================================================================

void A_FireSigil3 (AActor *actor)
{
	AActor *spot;
	player_t *player = actor->player;
	int i;

	if (player == NULL || player->ReadyWeapon == NULL)
		return;

	P_DamageMobj (actor, actor, NULL, 3*4, 0, DMG_NO_ARMOR);
	S_Sound (actor, CHAN_WEAPON, "weapons/sigilcharge", 1, ATTN_NORM);

	actor->angle -= ANGLE_90;
	for (i = 0; i < 20; ++i)
	{
		actor->angle += ANGLE_180/20;
		spot = P_SpawnSubMissile (actor, RUNTIME_CLASS(ASpectralLightningBall1), actor);
		if (spot != NULL)
		{
			spot->health = -1;
			spot->z = actor->z + 32*FRACUNIT;
		}
	}
	actor->angle -= (ANGLE_180/20)*10;
}

//============================================================================
//
// A_FireSigil4
//
//============================================================================

void A_FireSigil4 (AActor *actor)
{
	AActor *spot;
	player_t *player = actor->player;
	AActor *linetarget;

	if (player == NULL || player->ReadyWeapon == NULL)
		return;

	P_DamageMobj (actor, actor, NULL, 4*4, 0, DMG_NO_ARMOR);
	S_Sound (actor, CHAN_WEAPON, "weapons/sigilcharge", 1, ATTN_NORM);

	P_BulletSlope (actor, &linetarget);
	if (linetarget != NULL)
	{
		spot = P_SpawnPlayerMissile (actor, 0,0,0, RUNTIME_CLASS(ASpectralLightningBigV1), actor->angle, &linetarget);
		if (spot != NULL)
		{
			spot->tracer = linetarget;
			spot->health = -1;
		}
	}
	else
	{
		spot = P_SpawnPlayerMissile (actor, RUNTIME_CLASS(ASpectralLightningBigV1));
		if (spot != NULL)
		{
			spot->momx += FixedMul (spot->Speed, finecosine[actor->angle >> ANGLETOFINESHIFT]);
			spot->momy += FixedMul (spot->Speed, finesine[actor->angle >> ANGLETOFINESHIFT]);
			spot->health = -1;
		}
	}
}

//============================================================================
//
// A_FireSigil5
//
//============================================================================

void A_FireSigil5 (AActor *actor)
{
	AActor *spot;
	player_t *player = actor->player;

	if (player == NULL || player->ReadyWeapon == NULL)
		return;

	P_DamageMobj (actor, actor, NULL, 5*4, 0, DMG_NO_ARMOR);
	S_Sound (actor, CHAN_WEAPON, "weapons/sigilcharge", 1, ATTN_NORM);

	spot = P_SpawnPlayerMissile (actor, RUNTIME_CLASS(ASpectralLightningBigBall1));
	if (spot != NULL)
	{
		spot->health = -1;
	}
}

//============================================================================
//
// ASigil :: SpecialDropAction
//
// Monsters don't drop Sigil pieces. The Sigil pieces grab hold of the person
// who killed the dropper and automatically enter their inventory. That's the
// way it works if you believe Macil, anyway...
//
//============================================================================

bool ASigil::SpecialDropAction (AActor *dropper)
{
	// Give a Sigil piece to every player in the game
	for (int i = 0; i < MAXPLAYERS; ++i)
	{
		if (playeringame[i] && players[i].mo != NULL)
		{
			GiveSigilPiece (players[i].mo);
			Destroy ();
		}
	}
	return true;
}

//============================================================================
//
// ASigil :: GiveSigilPiece
//
// Gives the actor another Sigil piece, up to 5. Returns the number of Sigil
// pieces the actor previously held.
//
//============================================================================

int ASigil::GiveSigilPiece (AActor *receiver)
{
	ASigil *sigil;

	sigil = receiver->FindInventory<ASigil> ();
	if (sigil == NULL)
	{
		sigil = Spawn<ASigil1> (0,0,0, NO_REPLACE);
		if (!sigil->TryPickup (receiver))
		{
			sigil->Destroy ();
		}
		return 0;
	}
	else if (sigil->NumPieces < 5)
	{
		++sigil->NumPieces;
		static const PClass *const sigils[5] =
		{
			RUNTIME_CLASS(ASigil1),
			RUNTIME_CLASS(ASigil2),
			RUNTIME_CLASS(ASigil3),
			RUNTIME_CLASS(ASigil4),
			RUNTIME_CLASS(ASigil5)
		};
		sigil->Icon = ((AInventory*)GetDefaultByType (sigils[MAX(0,sigil->NumPieces-1)]))->Icon;
		// If the player has the Sigil out, drop it and bring it back up.
		if (sigil->Owner->player != NULL && sigil->Owner->player->ReadyWeapon == sigil)
		{
			sigil->Owner->player->PendingWeapon = sigil;
			sigil->DownPieces = sigil->NumPieces - 1;
		}
		return sigil->NumPieces - 1;
	}
	else
	{
		return 5;
	}
}
