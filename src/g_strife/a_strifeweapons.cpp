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
// hacking more stuff in the executable, their damages
// need to be halved compared to what is in the Strife exe.

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

class AStrifeWeapon : public AWeapon
{
	DECLARE_STATELESS_ACTOR (AStrifeWeapon, AWeapon)
};
IMPLEMENT_ABSTRACT_ACTOR (AStrifeWeapon)

// A spark when you hit something that doesn't bleed ------------------------
// Only used by the dagger, I think.

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
	S_NORMAL (POW2, 'D', 4, NULL, NULL),
};

IMPLEMENT_ACTOR (AStrifeSpark, Strife, -1, 0)
	PROP_SpawnState (0)
	PROP_CrashState (8)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_RenderStyle (STYLE_Add)
	PROP_Alpha (HX_ALTSHADOW)
END_DEFAULTS

// Same as the bullet puff for Doom -----------------------------------------

FState AStrifePuff::States[] =
{
	S_BRIGHT (PUFY, 'A', 4, NULL, &States[1]),
	S_NORMAL (PUFY, 'B', 4, NULL, &States[2]),
	S_NORMAL (PUFY, 'C', 4, NULL, &States[3]),
	S_NORMAL (PUFY, 'D', 4, NULL, NULL)
};

IMPLEMENT_ACTOR (AStrifePuff, Strife, -1, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_Alpha (HX_SHADOW)
	PROP_RenderStyle (STYLE_Translucent)
END_DEFAULTS

// Punch Dagger -------------------------------------------------------------

void A_JabDagger (AActor *, pspdef_t *);

class APunchDagger : public AStrifeWeapon
{
	DECLARE_ACTOR (APunchDagger, AStrifeWeapon)
public:
	weapontype_t OldStyleID () const;

	static FWeaponInfo WeaponInfo;
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

FWeaponInfo APunchDagger::WeaponInfo =
{
	WIF_NOALERT,
	am_noammo,
	am_noammo,
	1,
	0,
	&States[S_PUNCHUP],
	&States[S_PUNCHDOWN],
	&States[S_PUNCH],
	&States[S_PUNCH1],
	&States[S_PUNCH1],
	NULL,
	NULL,
	100,
	0,
	NULL,
	NULL,
	RUNTIME_CLASS(APunchDagger),
	-1
};

IMPLEMENT_ACTOR (APunchDagger, Strife, -1, 0)
END_DEFAULTS

WEAPON1 (wp_dagger, APunchDagger)

weapontype_t APunchDagger::OldStyleID () const
{
	return wp_dagger;
}

//============================================================================
//
// A_JabDagger
//
//============================================================================

void A_JabDagger (AActor *actor, pspdef_t *psp)
{
	angle_t 	angle;
	int 		damage;
	int 		pitch;
	fixed_t		somestat;

	// somestat is presumably in the range 0-100, as a fixed-point number
	// somestat = (int @ player+0x131);
	somestat = FRACUNIT;

	// Is this right? You have a chance of doing 0 damage!
	// Right or wrong, this is what it looks like it's doing.
	somestat = (somestat >> FRACBITS) / 10;
	damage = (pr_jabdagger() & (somestat + 7)) * (somestat + 2);

	if (actor->player->powers[pw_strength])
	{
		damage *= 10;
	}

	angle = actor->angle + (pr_jabdagger.Random2() << 18);
	pitch = P_AimLineAttack (actor, angle, 80*FRACUNIT);
	PuffType = RUNTIME_CLASS(AStrifeSpark);
	P_LineAttack (actor, angle, 80*FRACUNIT, pitch, damage);

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
		// Sys1eae0 (actor->player, linetarget);
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
	if (self->target != NULL && self->target->player != NULL)
	{
		P_NoiseAlert (self, self->target);
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
	PROP_SeeSound ("misc/swish")
	PROP_ActiveSound ("misc/swish")
	PROP_DeathSound ("weapons/xbowhit")
END_DEFAULTS

// Poison Bolt --------------------------------------------------------------

class APoisonBolt : public AActor
{
	DECLARE_ACTOR (APoisonBolt, AActor)
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
	PROP_SeeSound ("misc/swish")
	PROP_ActiveSound ("misc/swish")
END_DEFAULTS

// Strife's Crossbow --------------------------------------------------------

void A_XBowReFire (AActor *);
void A_ClearFlash (AActor *);
void A_ShowElectricFlash (AActor *);
void A_FireElectric (AActor *);
void A_FirePoison (AActor *);

class AStrifeCrossbow : public AStrifeWeapon
{
	DECLARE_ACTOR (AStrifeCrossbow, AStrifeWeapon)
public:
	weapontype_t OldStyleID() const;
	static FWeaponInfo WeaponInfo;
	bool TryPickup (AActor *toucher);
protected:
	const char *PickupMessage ()
	{
		return "You picked up the crossbow";
	}
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
FWeaponInfo AStrifeCrossbow::WeaponInfo =
{
	WIF_NOALERT,
	am_electricbolt,
	am_electricbolt,
	1,
	8,
	&States[S_EXBOWUP],
	&States[S_EXBOWDOWN],
	&States[S_EXBOW],
	&States[S_EXBOWATK],
	&States[S_EXBOWATK],
	NULL,
	RUNTIME_CLASS(AStrifeCrossbow),
	150,
	0,
	NULL,
	NULL,
	RUNTIME_CLASS(AStrifeCrossbow),
	-1
};

WEAPON1 (wp_electricxbow, AStrifeCrossbow)

IMPLEMENT_ACTOR (AStrifeCrossbow, Strife, 2001, 0)
	PROP_Flags (MF_SPECIAL)
	PROP_SpawnState (0)
	PROP_Tag ("crossbow")
END_DEFAULTS

weapontype_t AStrifeCrossbow::OldStyleID() const
{
	return wp_electricxbow;
}

bool AStrifeCrossbow::TryPickup (AActor *toucher)
{
	if (Super::TryPickup (toucher))
	{
		// You get the electric and poison crossbows at the same time
		toucher->player->weaponowned[wp_poisonxbow] = true;
		return true;
	}
	return false;
}

// Poison Crossbow ----------------------------------------------------------

class AStrifeCrossbow2 : public AStrifeCrossbow
{
	DECLARE_STATELESS_ACTOR (AStrifeCrossbow2, AStrifeCrossbow)
public:
	static FWeaponInfo WeaponInfo;
};

// The poison version
FWeaponInfo AStrifeCrossbow2::WeaponInfo =
{
	WIF_NOALERT,
	am_poisonbolt,
	am_poisonbolt,
	1,
	0,
	&States[S_PXBOWUP],
	&States[S_PXBOWDOWN],
	&States[S_PXBOW],
	&States[S_PXBOWATK],
	&States[S_PXBOWATK],
	NULL,
	RUNTIME_CLASS(AStrifeCrossbow),
	150,
	0,
	NULL,
	NULL,
	RUNTIME_CLASS(AStrifeCrossbow2),
	-1
};

WEAPON1 (wp_poisonxbow, AStrifeCrossbow2)

IMPLEMENT_ABSTRACT_ACTOR (AStrifeCrossbow2)


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
	fixed_t somestat;
	angle_t savedangle;

	//somestat = (int @ player+0x1df)
	somestat = FRACUNIT;

	somestat = (somestat >> FRACBITS) * 5 / 100;
	savedangle = self->angle;
	self->angle += pr_electric.Random2 () << (18 - somestat);
	self->player->UseAmmo ();
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
	fixed_t somestat;
	angle_t savedangle;

	//somestat = (int @ player+0x1df)
	somestat = FRACUNIT;

	somestat = (somestat >> FRACBITS) * 5 / 100;
	savedangle = self->angle;
	self->angle += pr_electric.Random2 () << (18 - somestat);
	self->player->UseAmmo ();
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

	P_CheckAmmo (player);
	if (player->readyweapon == wp_electricxbow)
	{
		P_SetPsprite (player, ps_flash, &AStrifeCrossbow::States[S_EXBOWARROWHEAD]);
	}
}

// Assault Gun --------------------------------------------------------------

class AAssaultGun : public AStrifeWeapon
{
	DECLARE_ACTOR (AAssaultGun, AStrifeWeapon)
public:
	weapontype_t OldStyleID() const;
	static FWeaponInfo WeaponInfo;
protected:
	const char *PickupMessage ()
	{
		return "You picked up the assault gun";
	}
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

FWeaponInfo AAssaultGun::WeaponInfo =
{
	0,
	am_clip,
	am_clip,
	1,
	20,
	&States[S_ASSAULTUP],
	&States[S_ASSAULTDOWN],
	&States[S_ASSAULT],
	&States[S_ASSAULTATK],
	&States[S_ASSAULTATK],
	NULL,
	RUNTIME_CLASS(AAssaultGun),
	150,
	0,
	NULL,
	NULL,
	RUNTIME_CLASS(AAssaultGun),
	-1
};

WEAPON1 (wp_assaultgun, AAssaultGun)

IMPLEMENT_ACTOR (AAssaultGun, Strife, 2002, 0)
	PROP_Flags (MF_SPECIAL)
	PROP_SpawnState (0)
	PROP_Tag ("assault_gun")
END_DEFAULTS

weapontype_t AAssaultGun::OldStyleID() const
{
	return wp_assaultgun;
}

//============================================================================
//
// P_StrifeGunShot
//
//============================================================================

void P_StrifeGunShot (AActor *mo, bool accurate)
{
	fixed_t somestat;
	angle_t angle;
	int damage;

	PuffType = RUNTIME_CLASS(AStrifePuff);
	damage = 4*(pr_sgunshot()%3+1);
	angle = mo->angle;

	if (mo->player != NULL && !accurate)
	{
		//somestat = (int @ player+0x1df)
		somestat = FRACUNIT;

		angle += pr_sgunshot.Random2() << (20 - (somestat >> FRACBITS) * 5 / 100);
	}

	P_LineAttack (mo, angle, PLAYERMISSILERANGE, bulletpitch, damage);
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
		self->player->mo->PlayAttacking2 ();
		if (!self->player->UseAmmo (true))
		{
			return;
		}
		accurate = !self->player->refire;
	}
	else
	{
		accurate = true;
	}

	P_BulletSlope (self);
	P_StrifeGunShot (self, accurate);
}

// Standing variant of the assault gun --------------------------------------

class AAssaultGunStanding : public AAssaultGun
{
	DECLARE_ACTOR (AAssaultGunStanding, AAssaultGun)
};

FState AAssaultGunStanding::States[] =
{
	S_NORMAL (RIFL, 'B',   -1, NULL,				&States[0]),
};

IMPLEMENT_ACTOR (AAssaultGunStanding, Strife, 2006, 0)
	PROP_SpawnState (0)
END_DEFAULTS

// Mini-Missile Launcher ----------------------------------------------------

void A_FireMiniMissile (AActor *);

class AMiniMissileLauncher : public AStrifeWeapon
{
	DECLARE_ACTOR (AMiniMissileLauncher, AStrifeWeapon)
public:
	weapontype_t OldStyleID() const;
	static FWeaponInfo WeaponInfo;
protected:
	const char *PickupMessage ()
	{
		return "You picked up the mini missile launcher";
	}
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

FWeaponInfo AMiniMissileLauncher::WeaponInfo =
{
	0,
	am_misl,
	am_misl,
	1,
	8,
	&States[S_MMISSILEUP],
	&States[S_MMISSILEDOWN],
	&States[S_MMISSILE],
	&States[S_MMISSILEATK],
	&States[S_MMISSILEATK],
	NULL,
	RUNTIME_CLASS(AMiniMissileLauncher),
	150,
	0,
	NULL,
	NULL,
	RUNTIME_CLASS(AMiniMissileLauncher),
	-1
};

WEAPON1 (wp_minimissile, AMiniMissileLauncher)

IMPLEMENT_ACTOR (AMiniMissileLauncher, Strife, 2003, 0)
	PROP_Flags (MF_SPECIAL)
	PROP_SpawnState (0)
	PROP_Tag ("mini_missile_launcher")
END_DEFAULTS

weapontype_t AMiniMissileLauncher::OldStyleID() const
{
	return wp_minimissile;
}

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
	PROP_Alpha (HX_SHADOW)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_SeeSound ("misc/missileinflight")
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

	S_BRIGHT (SMIS, 'A', 5, A_Explode,			&States[2]),
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
	fixed_t somestat;
	angle_t savedangle;

	//somestat = (int @ player+0x1df);
	somestat = FRACUNIT;

	savedangle = self->angle;
	self->angle += pr_minimissile.Random2() << (19 - (somestat >> FRACBITS) * 5 / 100);
	player->mo->PlayAttacking2 ();
	player->UseAmmo ();
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
	PuffType = RUNTIME_CLASS(AStrifePuff);
	P_SpawnPuff (self->x, self->y, self->z, self->angle - ANGLE_180, 2);
	trail = Spawn<ARocketTrail> (self->x - self->momx, self->y - self->momy, self->z);
	if (trail != NULL)
	{
		trail->momz = FRACUNIT;
	}
}

// Flame Thrower ------------------------------------------------------------

void A_FireFlamer (AActor *);

class AFlameThrower : public AStrifeWeapon
{
	DECLARE_ACTOR (AFlameThrower, AStrifeWeapon)
public:
	weapontype_t OldStyleID() const;
	static FWeaponInfo WeaponInfo;
protected:
	const char *PickupMessage ()
	{
		return "You picked up the flame thrower";
	}
};

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

FWeaponInfo AFlameThrower::WeaponInfo =
{
	0,
	am_cell,
	am_cell,
	1,
	40,
	&States[S_FLAMERUP],
	&States[S_FLAMERDOWN],
	&States[S_FLAMER],
	&States[S_FLAMERATK],
	&States[S_FLAMERATK],
	NULL,
	RUNTIME_CLASS(AFlameThrower),
	150,
	0,
	"weapons/flameidle",
	"weapons/flameidle",
	RUNTIME_CLASS(AFlameThrower),
	-1
};

WEAPON1 (wp_flamethrower, AFlameThrower)

IMPLEMENT_ACTOR (AFlameThrower, Strife, 2005, 0)
	PROP_Flags (MF_SPECIAL)
	PROP_SpawnState (0)
	PROP_Tag ("flame_thrower")
END_DEFAULTS

weapontype_t AFlameThrower::OldStyleID () const
{
	return wp_flamethrower;
}

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
	PROP_ReactionTime (8)
	PROP_Flags (MF_NOBLOCKMAP|MF_DROPOFF|MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_PCROSS|MF2_IMPACT|MF2_FIREDAMAGE)
	PROP_Flags4 (MF4_STRIFEDAMAGE)
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
		player->mo->PlayAttacking2 ();
		player->UseAmmo ();
	}

	self->angle += pr_flamethrower.Random2() << 18;
	self = P_SpawnPlayerMissile (self, RUNTIME_CLASS(AFlameMissile));
	if (self != NULL)
	{
		self->momz += 5*FRACUNIT;
	}
}

// Mauler -------------------------------------------------------------------

void A_FireMauler1 (AActor *, pspdef_t *);
void A_FireMauler2Pre (AActor *, pspdef_t *);
void A_FireMauler2 (AActor *, pspdef_t *);
void A_MaulerTorpedoWave (AActor *);

class AMauler : public AStrifeWeapon
{
	DECLARE_ACTOR (AMauler, AStrifeWeapon)
public:
	weapontype_t OldStyleID() const;
	static FWeaponInfo WeaponInfo;
	bool TryPickup (AActor *toucher);
protected:
	const char *PickupMessage ()
	{
		return "You picked up the mauler";
	}
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
	S_NORMAL (MAUL, 'J', 20, A_FireMauler2Pre,	&States[S_MAULER2ATK+1]),
	S_BRIGHT (BLSF, 'A', 10, A_Light1,			&States[S_MAULER2ATK+2]),
	S_BRIGHT (MAUL, 'B', 10, A_FireMauler2,		&States[S_MAULER2ATK+3]),
	S_NORMAL (MAUL, 'C',  2, NULL,				&States[S_MAULER2ATK+4]),
	S_NORMAL (MAUL, 'D',  2, A_Light0,			&States[S_MAULER2ATK+5]),
	S_NORMAL (MAUL, 'E',  2, A_ReFire,			&States[S_MAULER2]),
};

// The scatter version
FWeaponInfo AMauler::WeaponInfo =
{
	0,
	am_cell,
	am_cell,
	20,
	40,
	&States[S_MAULER1UP],
	&States[S_MAULER1DOWN],
	&States[S_MAULER1],
	&States[S_MAULER1ATK],
	&States[S_MAULER1ATK],
	NULL,
	RUNTIME_CLASS(AMauler),
	150,
	0,
	NULL,
	NULL,
	RUNTIME_CLASS(AMauler),
	-1
};

WEAPON1 (wp_maulerscatter, AMauler)

IMPLEMENT_ACTOR (AMauler, Strife, 2004, 0)
	PROP_Flags (MF_SPECIAL)
	PROP_SpawnState (0)
	PROP_Tag ("mauler")
END_DEFAULTS

weapontype_t AMauler::OldStyleID() const
{
	return wp_maulerscatter;
}

bool AMauler::TryPickup (AActor *toucher)
{
	if (Super::TryPickup (toucher))
	{
		// You get the scatter and torpedo version at the same time
		toucher->player->weaponowned[wp_maulertorpedo] = true;
		return true;
	}
	return false;
}

// Mauler Torpedo version ---------------------------------------------------

class AMauler2 : public AMauler
{
	DECLARE_STATELESS_ACTOR (AMauler2, AMauler)
public:
	static FWeaponInfo WeaponInfo;
};

// The torpedo version
FWeaponInfo AMauler2::WeaponInfo =
{
	0,
	am_cell,
	am_cell,
	30,
	0,
	&States[S_MAULER2UP],
	&States[S_MAULER2DOWN],
	&States[S_MAULER2],
	&States[S_MAULER2ATK],
	&States[S_MAULER2ATK],
	NULL,
	RUNTIME_CLASS(AMauler),
	150,
	0,
	NULL,
	NULL,
	RUNTIME_CLASS(AMauler2),
	-1
};

WEAPON1 (wp_maulertorpedo, AMauler2)

IMPLEMENT_ABSTRACT_ACTOR (AMauler2)

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
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
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
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_PCROSS|MF2_IMPACT)
	PROP_Flags4 (MF4_STRIFEDAMAGE)
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
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_PCROSS|MF2_IMPACT)
	PROP_Flags4 (MF4_STRIFEDAMAGE)
	PROP_RenderStyle (STYLE_Add)
END_DEFAULTS

//============================================================================
//
// A_FireMauler1
//
// Hey! This is exactly the same as a super shotgun except for the sound
// and the bullet puffs and the disintegration death.
//
// Note: Disintegration death not done yet.
//
//============================================================================

void A_FireMauler1 (AActor *self, pspdef_t *)
{
	if (self->player != NULL)
	{
		self->player->UseAmmo ();
	}

	S_Sound (self, CHAN_WEAPON, "weapons/mauler1", 1, ATTN_NORM);

	// Strife apparently didn't show the player shooting. Let's fix that.
	self->player->mo->PlayAttacking2 ();

	P_BulletSlope (self);
	PuffType = RUNTIME_CLASS(AMaulerPuff);

	for (int i = 0; i < 20; ++i)
	{
		int damage = 5 * (pr_mauler1() % 3 + 1);
		angle_t angle = self->angle + (pr_mauler1.Random2() << 19);
		int pitch = bulletpitch + (pr_mauler1.Random2() * 332063);
		
		// Strife used a range of 2112 units for the mauler, apparently as
		// a signal to use slightly different behavior, such as the puff.
		// ZDoom's default range is longer than this, so let's not handicap
		// it by being too faithful to the original.
		P_LineAttack (self, angle, PLAYERMISSILERANGE, pitch, damage);
	}
}

//============================================================================
//
// A_FireMauler2Pre
//
// Makes some noise and moves the psprite.
//
//============================================================================

void A_FireMauler2Pre (AActor *self, pspdef_t *psp)
{
	S_Sound (self, CHAN_WEAPON, "weapons/mauler2charge", 1, ATTN_NORM);

	if (self->player != NULL && psp != NULL)
	{
		psp->sx += pr_mauler2.Random2() << 10;
		psp->sy += pr_mauler2.Random2() << 10;
	}
}

//============================================================================
//
// A_FireMauler2Pre
//
// Fires the torpedo.
//
//============================================================================

void A_FireMauler2 (AActor *self, pspdef_t *)
{
	if (self->player != NULL)
	{
		self->player->mo->PlayAttacking2 ();
		self->player->UseAmmo ();
	}
	P_SpawnPlayerMissile (self, RUNTIME_CLASS(AMaulerTorpedo));
	P_DamageMobj (self, self, NULL, 20);
	P_ThrustMobj (self, self->angle + ANGLE_180, 0x7D000);
}

//============================================================================
//
// A_MaulerTorpedoWave
//
// Launches lots of balls when the torpedo hits something.
//
//============================================================================

AActor *P_SpawnSubMissile (AActor *source, TypeInfo *type);

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
		AActor *wave = P_SpawnSubMissile (self, RUNTIME_CLASS(AMaulerTorpedoWave));
		if (wave != NULL)
		{
			wave->target = self->target;
		}
	}
	self->z = savedz;
}

AActor *P_SpawnSubMissile (AActor *source, TypeInfo *type)
{
	AActor *other = Spawn (type, source->x, source->y, source->z);

	if (other == NULL)
	{
		return NULL;
	}

	other->target = source;
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
	S_NORMAL (GRAP, 'A', 3, A_Countdown,	&States[1]),
	S_NORMAL (GRAP, 'B', 3, A_Countdown,	&States[0]),

	S_BRIGHT (BNG4, 'A', 2, A_Explode,		&States[3]),
	S_BRIGHT (BNG4, 'B', 3, NULL,			&States[4]),
	S_BRIGHT (BNG4, 'C', 3, NULL,			&States[5]),
	S_BRIGHT (BNG4, 'D', 3, NULL,			&States[6]),
	S_BRIGHT (BNG4, 'E', 3, NULL,			&States[7]),
	S_BRIGHT (BNG4, 'F', 3, NULL,			&States[8]),
	S_BRIGHT (BNG4, 'G', 3, NULL,			&States[9]),
	S_BRIGHT (BNG4, 'H', 3, NULL,			&States[10]),
	S_BRIGHT (BNG4, 'I', 3, NULL,			&States[11]),
	S_BRIGHT (BNG4, 'J', 3, NULL,			&States[12]),
	S_BRIGHT (BNG4, 'K', 3, NULL,			&States[13]),
	S_BRIGHT (BNG4, 'L', 3, NULL,			&States[14]),
	S_BRIGHT (BNG4, 'M', 3, NULL,			&States[15]),
	S_BRIGHT (BNG4, 'N', 3, NULL,			NULL)
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
	PROP_Flags2 (MF2_NOTELEPORT|MF2_PCROSS|MF2_IMPACT|MF2_DOOMBOUNCE)
	PROP_Flags4 (MF4_STRIFEDAMAGE)
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
	PROP_Flags2 (MF2_NOTELEPORT|MF2_PCROSS|MF2_IMPACT|MF2_DOOMBOUNCE)
	PROP_Flags4 (MF4_STRIFEDAMAGE)
	PROP_SeeSound ("weapons/phgrenadeshoot")
	PROP_DeathSound ("weapons/phgrenadebang")
END_DEFAULTS

// Fire from the Phoshorous Grenade -----------------------------------------

void A_Burnination (AActor *self);

class APhosphorousFire : public AActor
{
	DECLARE_ACTOR (APhosphorousFire, AActor)
};

FState APhosphorousFire::States[] =
{
#define S_BURNINATION 0
	S_BRIGHT (BNG3, 'B',	2, A_Explode,			&States[S_BURNINATION+1]),
	S_BRIGHT (BNG3, 'C',	2, A_Countdown,			&States[S_BURNINATION+2]),
	S_BRIGHT (FLBE, 'A',	2, A_Burnination,		&States[S_BURNINATION+3]),
	S_BRIGHT (FLBE, 'B',	2, A_Countdown,			&States[S_BURNINATION+4]),
	S_BRIGHT (FLBE, 'C',	2, A_Explode,			&States[S_BURNINATION+5]),
	S_BRIGHT (FLBE, 'D',	3, A_Countdown,			&States[S_BURNINATION+6]),
	S_BRIGHT (FLBE, 'E',	3, A_Explode,			&States[S_BURNINATION+7]),
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
	PROP_Flags (MF_NOBLOCKMAP)
	PROP_Flags2 (MF2_FIREDAMAGE)
	PROP_RenderStyle (STYLE_Add)
END_DEFAULTS

void A_SpawnBurn (AActor *self)
{
	Spawn<APhosphorousFire> (self->x, self->y, self->z);
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
		int xofs = pr_phburn();
		int yofs = pr_phburn();

		xofs = (xofs - 128);
		if (xofs < 0)
		{
			xofs = clamp (xofs, -31, -12);
		}
		else
		{
			xofs = clamp (xofs, 12, 31);
		}

		yofs = (yofs - 128);
		if (yofs < 0)
		{
			yofs = clamp (yofs, -31, -12);
		}
		else
		{
			yofs = clamp (yofs, 12, 31);
		}

		AActor *drop = Spawn<APhosphorousFire> (
			self->x + (xofs << FRACBITS),
			self->y + (yofs << FRACBITS),
			self->z + 4*FRACUNIT);
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

void A_FireGrenade (AActor *, pspdef_t *);

class AStrifeGrenadeLauncher : public AStrifeWeapon
{
	DECLARE_ACTOR (AStrifeGrenadeLauncher, AStrifeWeapon)
public:
	weapontype_t OldStyleID() const;
	static FWeaponInfo WeaponInfo;
	bool TryPickup (AActor *toucher);
protected:
	const char *PickupMessage ()
	{
		return "You picked up the Grenade launcher";
	}
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

FWeaponInfo AStrifeGrenadeLauncher::WeaponInfo =
{
	0,
	am_hegrenade,
	am_hegrenade,
	1,
	8,
	&States[S_HEGRENADE_UP],
	&States[S_HEGRENADE_DOWN],
	&States[S_HEGRENADE],
	&States[S_HEGRENADE_ATK],
	&States[S_HEGRENADE_ATK],
	&States[S_HEGRENADE_FLASH],
	RUNTIME_CLASS(AStrifeGrenadeLauncher),
	150,
	0,
	NULL,
	NULL,
	RUNTIME_CLASS(AStrifeGrenadeLauncher),
	-1
};

WEAPON1 (wp_hegrenadelauncher, AStrifeGrenadeLauncher)

IMPLEMENT_ACTOR (AStrifeGrenadeLauncher, Strife, 154, 0)
	PROP_Flags (MF_SPECIAL)
	PROP_SpawnState (S_HEPICKUP)
	PROP_Tag ("Grenade_launcher")
END_DEFAULTS

weapontype_t AStrifeGrenadeLauncher::OldStyleID () const
{
	return wp_hegrenadelauncher;
}

bool AStrifeGrenadeLauncher::TryPickup (AActor *toucher)
{
	if (Super::TryPickup (toucher))
	{
		// You get the high-explosive and white phosphorous grenade
		// launchers at the same time.
		toucher->player->weaponowned[wp_phgrenadelauncher] = true;
		return true;
	}
	return false;
}

// White Phosphorous Grenade Launcher ---------------------------------------

class AStrifeGrenadeLauncher2 : public AStrifeGrenadeLauncher
{
	DECLARE_STATELESS_ACTOR (AStrifeGrenadeLauncher2, AStrifeGrenadeLauncher)
public:
	static FWeaponInfo WeaponInfo;
};

FWeaponInfo AStrifeGrenadeLauncher2::WeaponInfo =
{
	0,
	am_phgrenade,
	am_phgrenade,
	1,
	0,
	&States[S_PHGRENADE_UP],
	&States[S_PHGRENADE_DOWN],
	&States[S_PHGRENADE],
	&States[S_PHGRENADE_ATK],
	&States[S_PHGRENADE_ATK],
	&States[S_PHGRENADE_FLASH],
	RUNTIME_CLASS(AStrifeGrenadeLauncher),
	150,
	0,
	NULL,
	NULL,
	RUNTIME_CLASS(AStrifeGrenadeLauncher2),
	-1
};

WEAPON1 (wp_phgrenadelauncher, AStrifeGrenadeLauncher2)

IMPLEMENT_ABSTRACT_ACTOR (AStrifeGrenadeLauncher2)

//============================================================================
//
// A_FireGrenade
//
//============================================================================

void A_FireGrenade (AActor *self, pspdef_t *psp)
{
	TypeInfo *grenadetype;
	player_t *player = self->player;
	AActor *grenade;
	angle_t an;
	fixed_t tworadii;
	FWeaponInfo *weapon;

	if (player == NULL)
		return;

	if (player->powers[pw_weaponlevel2])
	{
		weapon = wpnlev2info[player->readyweapon];
	}
	else
	{
		weapon = wpnlev1info[player->readyweapon];
	}

	if (player->readyweapon == wp_hegrenadelauncher)
	{
		grenadetype = RUNTIME_CLASS(AHEGrenade);
	}
	else
	{
		grenadetype = RUNTIME_CLASS(APhosphorousGrenade);
	}
	if (!player->UseAmmo ())
		return;

	// Make it flash
	P_SetPsprite (player, ps_flash, weapon->flashstate + (psp->state - weapon->atkstate));

	self->z += 32*FRACUNIT;
	grenade = P_SpawnSubMissile (self, grenadetype);
	self->z -= 32*FRACUNIT;
	if (grenade == NULL)
		return;

	if (grenade->SeeSound != 0)
	{
		S_SoundID (grenade, CHAN_VOICE, grenade->SeeSound, 1, ATTN_NORM);
	}

	grenade->momz = FixedMul (finetangent[FINEANGLES/4-(self->pitch>>ANGLETOFINESHIFT)], grenade->Speed) + 8*FRACUNIT;

	an = self->angle >> ANGLETOFINESHIFT;
	tworadii = self->radius + grenade->radius;
	grenade->x += FixedMul (finecosine[an], tworadii);
	grenade->y += FixedMul (finesine[an], tworadii);

	if (weapon->atkstate == psp->state)
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
