#include "actor.h"
#include "info.h"
#include "s_sound.h"
#include "m_random.h"
#include "a_pickups.h"
#include "a_doomglobal.h"
#include "d_player.h"
#include "p_pspr.h"
#include "p_local.h"
#include "gstrings.h"
#include "p_effect.h"
#include "gi.h"
#include "templates.h"
#include "thingdef.h"

static FRandom pr_punch ("Punch");
static FRandom pr_saw ("Saw");
static FRandom pr_fireshotgun2 ("FireSG2");
static FRandom pr_fireplasma ("FirePlasma");
static FRandom pr_firerail ("FireRail");
static FRandom pr_bfgspray ("BFGSpray");

// Fist ---------------------------------------------------------------------

void A_Punch (AActor *);

class AFist : public AWeapon
{
	DECLARE_ACTOR (AFist, AWeapon)
};

FState AFist::States[] =
{
#define S_PUNCH 0
	S_NORMAL (PUNG, 'A',	1, A_WeaponReady		, &States[S_PUNCH]),

#define S_PUNCHDOWN (S_PUNCH+1)
	S_NORMAL (PUNG, 'A',	1, A_Lower				, &States[S_PUNCHDOWN]),

#define S_PUNCHUP (S_PUNCHDOWN+1)
	S_NORMAL (PUNG, 'A',	1, A_Raise				, &States[S_PUNCHUP]),

#define S_PUNCH1 (S_PUNCHUP+1)
	S_NORMAL (PUNG, 'B',	4, NULL 				, &States[S_PUNCH1+1]),
	S_NORMAL (PUNG, 'C',	4, A_Punch				, &States[S_PUNCH1+2]),
	S_NORMAL (PUNG, 'D',	5, NULL 				, &States[S_PUNCH1+3]),
	S_NORMAL (PUNG, 'C',	4, NULL 				, &States[S_PUNCH1+4]),
	S_NORMAL (PUNG, 'B',	5, A_ReFire 			, &States[S_PUNCH])
};

IMPLEMENT_ACTOR (AFist, Doom, -1, 0)
	PROP_Weapon_SelectionOrder (3700)
	PROP_Weapon_Flags (WIF_WIMPY_WEAPON|WIF_BOT_MELEE)
	PROP_Weapon_UpState (S_PUNCHUP)
	PROP_Weapon_DownState (S_PUNCHDOWN)
	PROP_Weapon_ReadyState (S_PUNCH)
	PROP_Weapon_AtkState (S_PUNCH1)
	PROP_Weapon_HoldAtkState (S_PUNCH1)
	PROP_Weapon_Kickback (100)
	PROP_Obituary("$OB_MPFIST")

END_DEFAULTS

//
// A_Punch
//
void A_Punch (AActor *actor)
{
	angle_t 	angle;
	int 		damage;
	int 		pitch;

	if (actor->player != NULL)
	{
		AWeapon *weapon = actor->player->ReadyWeapon;
		if (weapon != NULL)
		{
			if (!weapon->DepleteAmmo (weapon->bAltFire))
				return;
		}
	}

	damage = (pr_punch()%10+1)<<1;

	if (actor->FindInventory<APowerStrength>())	
		damage *= 10;

	angle = actor->angle;

	angle += pr_punch.Random2() << 18;
	pitch = P_AimLineAttack (actor, angle, MELEERANGE);
	P_LineAttack (actor, angle, MELEERANGE, pitch, damage, MOD_UNKNOWN, RUNTIME_CLASS(ABulletPuff));

	// turn to face target
	if (linetarget)
	{
		S_Sound (actor, CHAN_WEAPON, "*fist", 1, ATTN_NORM);
		actor->angle = R_PointToAngle2 (actor->x,
										actor->y,
										linetarget->x,
										linetarget->y);
	}
}

// Pistol -------------------------------------------------------------------

void A_FirePistol (AActor *);

class APistol : public AWeapon
{
	DECLARE_ACTOR (APistol, AWeapon)
};

FState APistol::States[] =
{
#define S_PISTOL 0
	S_NORMAL (PISG, 'A',	1, A_WeaponReady		, &States[S_PISTOL]),

#define S_PISTOLDOWN (S_PISTOL+1)
	S_NORMAL (PISG, 'A',	1, A_Lower				, &States[S_PISTOLDOWN]),

#define S_PISTOLUP (S_PISTOLDOWN+1)
	S_NORMAL (PISG, 'A',	1, A_Raise				, &States[S_PISTOLUP]),

#define S_PISTOL1 (S_PISTOLUP+1)
	S_NORMAL (PISG, 'A',	4, NULL 				, &States[S_PISTOL1+1]),
	S_NORMAL (PISG, 'B',	6, A_FirePistol 		, &States[S_PISTOL1+2]),
	S_NORMAL (PISG, 'C',	4, NULL 				, &States[S_PISTOL1+3]),
	S_NORMAL (PISG, 'B',	5, A_ReFire 			, &States[S_PISTOL]),

#define S_PISTOLFLASH (S_PISTOL1+4)
	S_BRIGHT (PISF, 'A',	7, A_Light1 			, &AWeapon::States[S_LIGHTDONE]),
// This next state is here just in case people want to shoot plasma balls or railguns
// with the pistol using Dehacked.
	S_BRIGHT (PISF, 'A',	7, A_Light1 			, &AWeapon::States[S_LIGHTDONE])
};

IMPLEMENT_ACTOR (APistol, Doom, -1, 0)
	PROP_Weapon_SelectionOrder (1900)
	PROP_Weapon_Flags (WIF_WIMPY_WEAPON)

	PROP_Weapon_AmmoUse1 (1)
	PROP_Weapon_AmmoGive1 (20)

	PROP_Weapon_UpState (S_PISTOLUP)
	PROP_Weapon_DownState (S_PISTOLDOWN)
	PROP_Weapon_ReadyState (S_PISTOL)
	PROP_Weapon_AtkState (S_PISTOL1)
	PROP_Weapon_HoldAtkState (S_PISTOL1)
	PROP_Weapon_FlashState (S_PISTOLFLASH)
	PROP_Weapon_Kickback (100)
	PROP_Weapon_MoveCombatDist (25000000)
	PROP_Weapon_AmmoType1 ("Clip")
	PROP_Obituary("$OB_MPPISTOL")
END_DEFAULTS

//
// A_FirePistol
//
void A_FirePistol (AActor *actor)
{
	bool accurate;

	if (actor->player != NULL)
	{
		AWeapon *weapon = actor->player->ReadyWeapon;
		if (weapon != NULL)
		{
			if (!weapon->DepleteAmmo (weapon->bAltFire))
				return;

			P_SetPsprite (actor->player, ps_flash, weapon->FlashState);
		}
		actor->player->mo->PlayAttacking2 ();

		accurate = !actor->player->refire;
	}
	else
	{
		accurate = true;
	}

	S_Sound (actor, CHAN_WEAPON, "weapons/pistol", 1, ATTN_NORM);

	P_BulletSlope (actor);
	P_GunShot (actor, accurate, RUNTIME_CLASS(ABulletPuff));
}

// Chainsaw -----------------------------------------------------------------

void A_Saw (AActor *);

class AChainsaw : public AWeapon
{
	DECLARE_ACTOR (AChainsaw, AWeapon)
};

FState AChainsaw::States[] =
{
#define S_SAW 0
	S_NORMAL (SAWG, 'C',	4, A_WeaponReady		, &States[S_SAW+1]),
	S_NORMAL (SAWG, 'D',	4, A_WeaponReady		, &States[S_SAW+0]),

#define S_SAWDOWN (S_SAW+2)
	S_NORMAL (SAWG, 'C',	1, A_Lower				, &States[S_SAWDOWN]),

#define S_SAWUP (S_SAWDOWN+1)
	S_NORMAL (SAWG, 'C',	1, A_Raise				, &States[S_SAWUP]),

#define S_SAW1 (S_SAWUP+1)
	S_NORMAL (SAWG, 'A',	4, A_Saw				, &States[S_SAW1+1]),
	S_NORMAL (SAWG, 'B',	4, A_Saw				, &States[S_SAW1+2]),
	S_NORMAL (SAWG, 'B',	0, A_ReFire 			, &States[S_SAW]),

#define S_CSAW (S_SAW1+3)
	S_NORMAL (CSAW, 'A',   -1, NULL 				, NULL)
};

IMPLEMENT_ACTOR (AChainsaw, Doom, 2005, 32)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (16)
	PROP_Flags (MF_SPECIAL)
	PROP_SpawnState (S_CSAW)

	PROP_Weapon_SelectionOrder (2200)
	PROP_Weapon_Flags (WIF_BOT_MELEE)
	PROP_Weapon_UpState (S_SAWUP)
	PROP_Weapon_DownState (S_SAWDOWN)
	PROP_Weapon_ReadyState (S_SAW)
	PROP_Weapon_AtkState (S_SAW1)
	PROP_Weapon_HoldAtkState (S_SAW1)
	PROP_Weapon_UpSound ("weapons/sawup")
	PROP_Weapon_ReadySound ("weapons/sawidle")
	PROP_Obituary("$OB_MPCHAINSAW")
	PROP_Inventory_PickupMessage("$GOTCHAINSAW")
END_DEFAULTS

//
// A_Saw
//
void A_Saw (AActor *actor)
{
	angle_t 	angle;
	int 		damage;
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

	damage = 2 * (pr_saw()%10+1);
	angle = actor->angle;
	angle += pr_saw.Random2() << 18;
	
	// use meleerange + 1 so the puff doesn't skip the flash
	// [RH] What I think that really means is that they want the puff to show
	// up on walls. If the distance to P_LineAttack is <= MELEERANGE, then it
	// won't puff the wall, which is why the fist does not create puffs on
	// the walls.
	P_LineAttack (actor, angle, MELEERANGE+1,
				  P_AimLineAttack (actor, angle, MELEERANGE+1), damage,
				  MOD_UNKNOWN, RUNTIME_CLASS(ABulletPuff));

	if (!linetarget)
	{
		S_Sound (actor, CHAN_WEAPON, "weapons/sawfull", 1, ATTN_NORM);
		return;
	}
	S_Sound (actor, CHAN_WEAPON, "weapons/sawhit", 1, ATTN_NORM);
		
	// turn to face target
	angle = R_PointToAngle2 (actor->x, actor->y,
							 linetarget->x, linetarget->y);
	if (angle - actor->angle > ANG180)
	{
		if (angle - actor->angle < (angle_t)(-ANG90/20))
			actor->angle = angle + ANG90/21;
		else
			actor->angle -= ANG90/20;
	}
	else
	{
		if (angle - actor->angle > ANG90/20)
			actor->angle = angle - ANG90/21;
		else
			actor->angle += ANG90/20;
	}
	actor->flags |= MF_JUSTATTACKED;
}

// Shotgun ------------------------------------------------------------------

void A_FireShotgun (AActor *);

class AShotgun : public AWeapon
{
	DECLARE_ACTOR (AShotgun, AWeapon)
};

FState AShotgun::States[] =
{
#define S_SGUN 0
	S_NORMAL (SHTG, 'A',	1, A_WeaponReady		, &States[S_SGUN]),

#define S_SGUNDOWN (S_SGUN+1)
	S_NORMAL (SHTG, 'A',	1, A_Lower				, &States[S_SGUNDOWN]),

#define S_SGUNUP (S_SGUNDOWN+1)
	S_NORMAL (SHTG, 'A',	1, A_Raise				, &States[S_SGUNUP]),

#define S_SGUN1 (S_SGUNUP+1)
	S_NORMAL (SHTG, 'A',	3, NULL 				, &States[S_SGUN1+1]),
	S_NORMAL (SHTG, 'A',	7, A_FireShotgun		, &States[S_SGUN1+2]),
	S_NORMAL (SHTG, 'B',	5, NULL 				, &States[S_SGUN1+3]),
	S_NORMAL (SHTG, 'C',	5, NULL 				, &States[S_SGUN1+4]),
	S_NORMAL (SHTG, 'D',	4, NULL 				, &States[S_SGUN1+5]),
	S_NORMAL (SHTG, 'C',	5, NULL 				, &States[S_SGUN1+6]),
	S_NORMAL (SHTG, 'B',	5, NULL 				, &States[S_SGUN1+7]),
	S_NORMAL (SHTG, 'A',	3, NULL 				, &States[S_SGUN1+8]),
	S_NORMAL (SHTG, 'A',	7, A_ReFire 			, &States[S_SGUN]),

#define S_SGUNFLASH (S_SGUN1+9)
	S_BRIGHT (SHTF, 'A',	4, A_Light1 			, &States[S_SGUNFLASH+1]),
	S_BRIGHT (SHTF, 'B',	3, A_Light2 			, &AWeapon::States[S_LIGHTDONE]),

#define S_SHOT (S_SGUNFLASH+2)
	S_NORMAL (SHOT, 'A',   -1, NULL 				, NULL)
};

IMPLEMENT_ACTOR (AShotgun, Doom, 2001, 27)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (16)
	PROP_Flags (MF_SPECIAL)
	PROP_SpawnState (S_SHOT)

	PROP_Weapon_SelectionOrder (1300)
	PROP_Weapon_AmmoUse1 (1)
	PROP_Weapon_AmmoGive1 (8)
	PROP_Weapon_UpState (S_SGUNUP)
	PROP_Weapon_DownState (S_SGUNDOWN)
	PROP_Weapon_ReadyState (S_SGUN)
	PROP_Weapon_AtkState (S_SGUN1)
	PROP_Weapon_HoldAtkState (S_SGUN1)
	PROP_Weapon_FlashState (S_SGUNFLASH)
	PROP_Weapon_Kickback (100)
	PROP_Weapon_MoveCombatDist (24000000)
	PROP_Weapon_AmmoType1 ("Shell")
	PROP_Obituary("$OB_MPSHOTGUN")
	PROP_Inventory_PickupMessage("$GOTSHOTGUN")
END_DEFAULTS

//
// A_FireShotgun
//
void A_FireShotgun (AActor *actor)
{
	int i;
	player_t *player;

	if (NULL == (player = actor->player))
	{
		return;
	}

	S_Sound (actor, CHAN_WEAPON,  "weapons/shotgf", 1, ATTN_NORM);
	AWeapon *weapon = actor->player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
		P_SetPsprite (player, ps_flash, weapon->FlashState);
	}
	player->mo->PlayAttacking2 ();

	P_BulletSlope (actor);

	for (i=0 ; i<7 ; i++)
		P_GunShot (actor, false, RUNTIME_CLASS(ABulletPuff));
}

// Super Shotgun ------------------------------------------------------------

void A_FireShotgun2 (AActor *actor);
void A_OpenShotgun2 (AActor *actor);
void A_LoadShotgun2 (AActor *actor);
void A_CloseShotgun2 (AActor *actor);

class ASuperShotgun : public AWeapon
{
	DECLARE_ACTOR (ASuperShotgun, AWeapon)
};

FState ASuperShotgun::States[] =
{
#define S_DSGUN 0
	S_NORMAL (SHT2, 'A',	1, A_WeaponReady		, &States[S_DSGUN]),

#define S_DSGUNDOWN (S_DSGUN+1)
	S_NORMAL (SHT2, 'A',	1, A_Lower				, &States[S_DSGUNDOWN]),

#define S_DSGUNUP (S_DSGUNDOWN+1)
	S_NORMAL (SHT2, 'A',	1, A_Raise				, &States[S_DSGUNUP]),

#define S_DSGUN1 (S_DSGUNUP+1)
	S_NORMAL (SHT2, 'A',	3, NULL 				, &States[S_DSGUN1+1]),
	S_NORMAL (SHT2, 'A',	7, A_FireShotgun2		, &States[S_DSGUN1+2]),
	S_NORMAL (SHT2, 'B',	7, NULL 				, &States[S_DSGUN1+3]),
	S_NORMAL (SHT2, 'C',	7, A_CheckReload		, &States[S_DSGUN1+4]),
	S_NORMAL (SHT2, 'D',	7, A_OpenShotgun2		, &States[S_DSGUN1+5]),
	S_NORMAL (SHT2, 'E',	7, NULL 				, &States[S_DSGUN1+6]),
	S_NORMAL (SHT2, 'F',	7, A_LoadShotgun2		, &States[S_DSGUN1+7]),
	S_NORMAL (SHT2, 'G',	6, NULL 				, &States[S_DSGUN1+8]),
	S_NORMAL (SHT2, 'H',	6, A_CloseShotgun2		, &States[S_DSGUN1+9]),
	S_NORMAL (SHT2, 'A',	5, A_ReFire 			, &States[S_DSGUN]),

#define S_DSNR (S_DSGUN1+10)
	S_NORMAL (SHT2, 'B',	7, NULL 				, &States[S_DSNR+1]),
	S_NORMAL (SHT2, 'A',	3, NULL 				, &States[S_DSGUNDOWN]),

#define S_DSGUNFLASH (S_DSNR+2)
	S_BRIGHT (SHT2, 'I',	4, A_Light1 			, &States[S_DSGUNFLASH+1]),
	S_BRIGHT (SHT2, 'J',	3, A_Light2 			, &AWeapon::States[S_LIGHTDONE]),

#define S_SHOT2 (S_DSGUNFLASH+2)
	S_NORMAL (SGN2, 'A',   -1, NULL 				, NULL)
};

IMPLEMENT_ACTOR (ASuperShotgun, Doom, 82, 33)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (16)
	PROP_Flags (MF_SPECIAL)
	PROP_SpawnState (S_SHOT2)

	PROP_Weapon_SelectionOrder (400)
	PROP_Weapon_AmmoUse1 (2)
	PROP_Weapon_AmmoGive1 (8)
	PROP_Weapon_UpState (S_DSGUNUP)
	PROP_Weapon_DownState (S_DSGUNDOWN)
	PROP_Weapon_ReadyState (S_DSGUN)
	PROP_Weapon_AtkState (S_DSGUN1)
	PROP_Weapon_HoldAtkState (S_DSGUN1)
	PROP_Weapon_FlashState (S_DSGUNFLASH)
	PROP_Weapon_Kickback (100)
	PROP_Weapon_MoveCombatDist (15000000)
	PROP_Weapon_AmmoType1 ("Shell")
	PROP_Obituary("$OB_MPSSHOTGUN")
	PROP_Inventory_PickupMessage("$GOTSHOTGUN2")
END_DEFAULTS

//
// A_FireShotgun2
//
void A_FireShotgun2 (AActor *actor)
{
	int 		i;
	angle_t 	angle;
	int 		damage;
	player_t *player;

	if (NULL == (player = actor->player))
	{
		return;
	}

	S_Sound (actor, CHAN_WEAPON, "weapons/sshotf", 1, ATTN_NORM);
	AWeapon *weapon = actor->player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
		P_SetPsprite (player, ps_flash, weapon->FlashState);
	}
	player->mo->PlayAttacking2 ();


	P_BulletSlope (actor);
		
	for (i=0 ; i<20 ; i++)
	{
		damage = 5*(pr_fireshotgun2()%3+1);
		angle = actor->angle;
		angle += pr_fireshotgun2.Random2() << 19;

		// Doom adjusts the bullet slope by shifting a random number [-255,255]
		// left 5 places. At 2048 units away, this means the vertical position
		// of the shot can deviate as much as 255 units from nominal. So using
		// some simple trigonometry, that means the vertical angle of the shot
		// can deviate by as many as ~7.097 degrees or ~84676099 BAMs.

		P_LineAttack (actor,
					  angle,
					  PLAYERMISSILERANGE,
					  bulletpitch + (pr_fireshotgun2.Random2() * 332063), damage,
					  MOD_UNKNOWN, RUNTIME_CLASS(ABulletPuff));
	}
}

void A_OpenShotgun2 (AActor *actor)
{
	S_Sound (actor, CHAN_WEAPON, "weapons/sshoto", 1, ATTN_NORM);
}

void A_LoadShotgun2 (AActor *actor)
{
	S_Sound (actor, CHAN_WEAPON, "weapons/sshotl", 1, ATTN_NORM);
}

void A_CloseShotgun2 (AActor *actor)
{
	S_Sound (actor, CHAN_WEAPON, "weapons/sshotc", 1, ATTN_NORM);
	A_ReFire (actor);
}

// Chaingun -----------------------------------------------------------------

void A_FireCGun (AActor *);

class AChaingun : public AWeapon
{
	DECLARE_ACTOR (AChaingun, AWeapon)
};

FState AChaingun::States[] =
{
#define S_CHAIN 0
	S_NORMAL (CHGG, 'A',	1, A_WeaponReady		, &States[S_CHAIN]),

#define S_CHAINDOWN (S_CHAIN+1)
	S_NORMAL (CHGG, 'A',	1, A_Lower				, &States[S_CHAINDOWN]),

#define S_CHAINUP (S_CHAINDOWN+1)
	S_NORMAL (CHGG, 'A',	1, A_Raise				, &States[S_CHAINUP]),

#define S_CHAIN1 (S_CHAINUP+1)
	S_NORMAL (CHGG, 'A',	4, A_FireCGun			, &States[S_CHAIN1+1]),
	S_NORMAL (CHGG, 'B',	4, A_FireCGun			, &States[S_CHAIN1+2]),
	S_NORMAL (CHGG, 'B',	0, A_ReFire 			, &States[S_CHAIN]),

#define S_CHAINFLASH (S_CHAIN1+3)
	S_BRIGHT (CHGF, 'A',	5, A_Light1 			, &AWeapon::States[S_LIGHTDONE]),
	S_BRIGHT (CHGF, 'B',	5, A_Light2 			, &AWeapon::States[S_LIGHTDONE]),

#define S_MGUN (S_CHAINFLASH+2)
	S_NORMAL (MGUN, 'A',   -1, NULL 				, NULL)
};

IMPLEMENT_ACTOR (AChaingun, Doom, 2002, 28)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (16)
	PROP_Flags (MF_SPECIAL)
	PROP_SpawnState (S_MGUN)

	PROP_Weapon_SelectionOrder (700)
	PROP_Weapon_AmmoUse1 (1)
	PROP_Weapon_AmmoGive1 (20)
	PROP_Weapon_UpState (S_CHAINUP)
	PROP_Weapon_DownState (S_CHAINDOWN)
	PROP_Weapon_ReadyState (S_CHAIN)
	PROP_Weapon_AtkState (S_CHAIN1)
	PROP_Weapon_HoldAtkState (S_CHAIN1)
	PROP_Weapon_FlashState (S_CHAINFLASH)
	PROP_Weapon_Kickback (100)
	PROP_Weapon_MoveCombatDist (27000000)
	PROP_Weapon_AmmoType1 ("Clip")
	PROP_Obituary("$OB_MPCHAINGUN")
	PROP_Inventory_PickupMessage("$GOTCHAINGUN")
END_DEFAULTS

//
// A_FireCGun
//
void A_FireCGun (AActor *actor)
{
	player_t *player;

	if (actor == NULL || NULL == (player = actor->player))
	{
		return;
	}
	S_Sound (actor, CHAN_WEAPON, "weapons/chngun", 1, ATTN_NORM);

	AWeapon *weapon = player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
		if (weapon->FlashState != NULL)
		{
			// [RH] Fix for Sparky's messed-up Dehacked patch! Blargh!
			int theflash = clamp (int(players->psprites[ps_weapon].state - weapon->AtkState), 0, 1);

			if (weapon->FlashState[theflash].sprite.index != weapon->FlashState->sprite.index)
			{
				theflash = 0;
			}

			P_SetPsprite (player, ps_flash, weapon->FlashState + theflash);
		}

	}
	player->mo->PlayAttacking2 ();

	P_BulletSlope (actor);
	P_GunShot (actor, !player->refire, RUNTIME_CLASS(ABulletPuff));
}

// Rocket launcher ---------------------------------------------------------

void A_FireMissile (AActor *);
void A_Explode (AActor *);

class ARocketLauncher : public AWeapon
{
	DECLARE_ACTOR (ARocketLauncher, AWeapon)
};

FState ARocketLauncher::States[] =
{
#define S_MISSILE 0
	S_NORMAL (MISG, 'A',	1, A_WeaponReady		, &States[S_MISSILE]),

#define S_MISSILEDOWN (S_MISSILE+1)
	S_NORMAL (MISG, 'A',	1, A_Lower				, &States[S_MISSILEDOWN]),

#define S_MISSILEUP (S_MISSILEDOWN+1)
	S_NORMAL (MISG, 'A',	1, A_Raise				, &States[S_MISSILEUP]),

#define S_MISSILE1 (S_MISSILEUP+1)
	S_NORMAL (MISG, 'B',	8, A_GunFlash			, &States[S_MISSILE1+1]),
	S_NORMAL (MISG, 'B',   12, A_FireMissile		, &States[S_MISSILE1+2]),
	S_NORMAL (MISG, 'B',	0, A_ReFire 			, &States[S_MISSILE]),

#define S_MISSILEFLASH (S_MISSILE1+3)
	S_BRIGHT (MISF, 'A',	3, A_Light1 			, &States[S_MISSILEFLASH+1]),
	S_BRIGHT (MISF, 'B',	4, NULL 				, &States[S_MISSILEFLASH+2]),
	S_BRIGHT (MISF, 'C',	4, A_Light2 			, &States[S_MISSILEFLASH+3]),
	S_BRIGHT (MISF, 'D',	4, A_Light2 			, &AWeapon::States[S_LIGHTDONE]),

#define S_LAUN (S_MISSILEFLASH+4)
	S_NORMAL (LAUN, 'A',   -1, NULL 				, NULL)
};

IMPLEMENT_ACTOR (ARocketLauncher, Doom, 2003, 29)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (16)
	PROP_Flags (MF_SPECIAL)
	PROP_SpawnState (S_LAUN)

	PROP_Weapon_SelectionOrder (2500)
	PROP_Weapon_Flags (WIF_NOAUTOFIRE|WIF_BOT_REACTION_SKILL_THING|WIF_BOT_EXPLOSIVE)
	PROP_Weapon_AmmoUse1 (1)
	PROP_Weapon_AmmoGive1 (2)
	PROP_Weapon_UpState (S_MISSILEUP)
	PROP_Weapon_DownState (S_MISSILEDOWN)
	PROP_Weapon_ReadyState (S_MISSILE)
	PROP_Weapon_AtkState (S_MISSILE1)
	PROP_Weapon_HoldAtkState (S_MISSILE1)
	PROP_Weapon_FlashState (S_MISSILEFLASH)
	PROP_Weapon_Kickback (100)
	PROP_Weapon_MoveCombatDist (18350080)
	PROP_Weapon_AmmoType1 ("RocketAmmo")
	PROP_Weapon_ProjectileType ("Rocket")
	PROP_Inventory_PickupMessage("$GOTLAUNCHER")
END_DEFAULTS

FState ARocket::States[] =
{
#define S_ROCKET 0
	S_BRIGHT (MISL, 'A',	1, NULL 						, &States[S_ROCKET]),

#define S_EXPLODE (S_ROCKET+1)
	S_BRIGHT (MISL, 'B',	8, A_Explode					, &States[S_EXPLODE+1]),
	S_BRIGHT (MISL, 'C',	6, NULL 						, &States[S_EXPLODE+2]),
	S_BRIGHT (MISL, 'D',	4, NULL 						, NULL)
};

IMPLEMENT_ACTOR (ARocket, Doom, -1, 127)
	PROP_RadiusFixed (11)
	PROP_HeightFixed (8)
	PROP_SpeedFixed (20)
	PROP_Damage (20)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF|MF_NOGRAVITY)
	PROP_Flags2 (MF2_PCROSS|MF2_IMPACT|MF2_NOTELEPORT)
	PROP_Flags4 (MF4_RANDOMIZE)

	PROP_SpawnState (S_ROCKET)
	PROP_DeathState (S_EXPLODE)

	PROP_SeeSound ("weapons/rocklf")
	PROP_DeathSound ("weapons/rocklx")
	PROP_Obituary("$OB_MPROCKET")
END_DEFAULTS

void ARocket::BeginPlay ()
{
	Super::BeginPlay ();
	effects |= FX_ROCKET;
}

//
// A_FireMissile
//
void A_FireMissile (AActor *actor)
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
	P_SpawnPlayerMissile (actor, RUNTIME_CLASS(ARocket));
}

// Plasma rifle ------------------------------------------------------------

void A_FirePlasma (AActor *);

class APlasmaRifle : public AWeapon
{
	DECLARE_ACTOR (APlasmaRifle, AWeapon)
};

FState APlasmaRifle::States[] =
{
#define S_PLASMA 0
	S_NORMAL (PLSG, 'A',	1, A_WeaponReady		, &States[S_PLASMA]),

#define S_PLASMADOWN (S_PLASMA+1)
	S_NORMAL (PLSG, 'A',	1, A_Lower				, &States[S_PLASMADOWN]),

#define S_PLASMAUP (S_PLASMADOWN+1)
	S_NORMAL (PLSG, 'A',	1, A_Raise				, &States[S_PLASMAUP]),

#define S_PLASMA1 (S_PLASMAUP+1)
	S_NORMAL (PLSG, 'A',	3, A_FirePlasma 		, &States[S_PLASMA1+1]),
	S_NORMAL (PLSG, 'B',   20, A_ReFire 			, &States[S_PLASMA]),

#define S_PLASMAFLASH (S_PLASMA1+2)
	S_BRIGHT (PLSF, 'A',	4, A_Light1 			, &AWeapon::States[S_LIGHTDONE]),
	S_BRIGHT (PLSF, 'B',	4, A_Light1 			, &AWeapon::States[S_LIGHTDONE]),

#define S_PLAS (S_PLASMAFLASH+2)
	S_NORMAL (PLAS, 'A',   -1, NULL 				, NULL)
};

IMPLEMENT_ACTOR (APlasmaRifle, Doom, 2004, 30)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (16)
	PROP_Flags (MF_SPECIAL)
	PROP_SpawnState (S_PLAS)

	PROP_Weapon_SelectionOrder (100)
	PROP_Weapon_AmmoUse1 (1)
	PROP_Weapon_AmmoGive1 (40)
	PROP_Weapon_UpState (S_PLASMAUP)
	PROP_Weapon_DownState (S_PLASMADOWN)
	PROP_Weapon_ReadyState (S_PLASMA)
	PROP_Weapon_AtkState (S_PLASMA1)
	PROP_Weapon_HoldAtkState (S_PLASMA1)
	PROP_Weapon_FlashState (S_PLASMAFLASH)
	PROP_Weapon_Kickback (100)
	PROP_Weapon_MoveCombatDist (27000000)
	PROP_Weapon_ProjectileType ("PlasmaBall")
	PROP_Weapon_AmmoType1 ("Cell")
	PROP_Inventory_PickupMessage("$GOTPLASMA")
END_DEFAULTS

FState APlasmaBall::States[] =
{
#define S_PLASBALL 0
	S_BRIGHT (PLSS, 'A',	6, NULL 						, &States[S_PLASBALL+1]),
	S_BRIGHT (PLSS, 'B',	6, NULL 						, &States[S_PLASBALL]),

#define S_PLASEXP (S_PLASBALL+2)
	S_BRIGHT (PLSE, 'A',	4, NULL 						, &States[S_PLASEXP+1]),
	S_BRIGHT (PLSE, 'B',	4, NULL 						, &States[S_PLASEXP+2]),
	S_BRIGHT (PLSE, 'C',	4, NULL 						, &States[S_PLASEXP+3]),
	S_BRIGHT (PLSE, 'D',	4, NULL 						, &States[S_PLASEXP+4]),
	S_BRIGHT (PLSE, 'E',	4, NULL 						, NULL)
};

IMPLEMENT_ACTOR (APlasmaBall, Doom, -1, 51)
	PROP_RadiusFixed (13)
	PROP_HeightFixed (8)
	PROP_SpeedFixed (25)
	PROP_Damage (5)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF|MF_NOGRAVITY)
	PROP_Flags2 (MF2_PCROSS|MF2_IMPACT|MF2_NOTELEPORT)
	PROP_Flags3 (MF3_WARNBOT)
	PROP_Flags4 (MF4_RANDOMIZE)
	PROP_RenderStyle (STYLE_Add)
	PROP_Alpha (TRANSLUC75)

	PROP_SpawnState (S_PLASBALL)
	PROP_DeathState (S_PLASEXP)

	PROP_SeeSound ("weapons/plasmaf")
	PROP_DeathSound ("weapons/plasmax")
	PROP_Obituary("$OB_MPPLASMARIFLE")
END_DEFAULTS

//
// A_FirePlasma
//
void A_FirePlasma (AActor *actor)
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
		if (weapon->FlashState != NULL)
		{
			P_SetPsprite (player, ps_flash, weapon->FlashState + (pr_fireplasma()&1));
		}
	}

	P_SpawnPlayerMissile (actor, RUNTIME_CLASS(APlasmaBall));
}

//
// [RH] A_FireRailgun
//
static int RailOffset;

void A_FireRailgun (AActor *actor)
{
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
		if (weapon->FlashState != NULL)
		{
			P_SetPsprite (player, ps_flash, weapon->FlashState + (pr_firerail()&1));
		}
	}

	damage = deathmatch ? 100 : 150;

	P_RailAttack (actor, damage, RailOffset);
	RailOffset = 0;
}

void A_FireRailgunRight (AActor *actor)
{
	RailOffset = 10;
	A_FireRailgun (actor);
}

void A_FireRailgunLeft (AActor *actor)
{
	RailOffset = -10;
	A_FireRailgun (actor);
}

void A_RailWait (AActor *actor)
{
	// Okay, this was stupid. Just use a NULL function instead of this.
}

// BFG 9000 -----------------------------------------------------------------

void A_FireBFG (AActor *);
void A_BFGSpray (AActor *);
void A_BFGsound (AActor *);

class ABFG9000 : public AWeapon
{
	DECLARE_ACTOR (ABFG9000, AWeapon)
};

class ABFGExtra : public AActor
{
	DECLARE_ACTOR (ABFGExtra, AActor)
};

FState ABFG9000::States[] =
{
#define S_BFG 0
	S_NORMAL (BFGG, 'A',	1, A_WeaponReady		, &States[S_BFG]),

#define S_BFGDOWN (S_BFG+1)
	S_NORMAL (BFGG, 'A',	1, A_Lower				, &States[S_BFGDOWN]),

#define S_BFGUP (S_BFGDOWN+1)
	S_NORMAL (BFGG, 'A',	1, A_Raise				, &States[S_BFGUP]),

#define S_BFG1 (S_BFGUP+1)
	S_NORMAL (BFGG, 'A',   20, A_BFGsound			, &States[S_BFG1+1]),
	S_NORMAL (BFGG, 'B',   10, A_GunFlash			, &States[S_BFG1+2]),
	S_NORMAL (BFGG, 'B',   10, A_FireBFG			, &States[S_BFG1+3]),
	S_NORMAL (BFGG, 'B',   20, A_ReFire 			, &States[S_BFG]),

#define S_BFGFLASH (S_BFG1+4)
	S_BRIGHT (BFGF, 'A',   11, A_Light1 			, &States[S_BFGFLASH+1]),
	S_BRIGHT (BFGF, 'B',	6, A_Light2 			, &AWeapon::States[S_LIGHTDONE]),

#define S_BFUG (S_BFGFLASH+2)
	S_NORMAL (BFUG, 'A',   -1, NULL 				, NULL)
};

IMPLEMENT_ACTOR (ABFG9000, Doom, 2006, 31)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (20)
	PROP_Flags (MF_SPECIAL)
	PROP_SpawnState (S_BFUG)

	PROP_Weapon_Flags (WIF_NOAUTOFIRE|WIF_BOT_REACTION_SKILL_THING|WIF_BOT_BFG)
	PROP_Weapon_SelectionOrder (2800)
	PROP_Weapon_AmmoUse1 (40)
	PROP_Weapon_AmmoGive1 (40)
	PROP_Weapon_UpState (S_BFGUP)
	PROP_Weapon_DownState (S_BFGDOWN)
	PROP_Weapon_ReadyState (S_BFG)
	PROP_Weapon_AtkState (S_BFG1)
	PROP_Weapon_HoldAtkState (S_BFG1)
	PROP_Weapon_FlashState (S_BFGFLASH)
	PROP_Weapon_Kickback (100)
	PROP_Weapon_MoveCombatDist (10000000)
	PROP_Weapon_AmmoType1 ("Cell")
	PROP_Weapon_ProjectileType ("BFGBall")
	PROP_Inventory_PickupMessage("$GOTBFG9000")
END_DEFAULTS

FState ABFGBall::States[] =
{
#define S_BFGSHOT 0
	S_BRIGHT (BFS1, 'A',	4, NULL 						, &States[S_BFGSHOT+1]),
	S_BRIGHT (BFS1, 'B',	4, NULL 						, &States[S_BFGSHOT]),

#define S_BFGLAND (S_BFGSHOT+2)
	S_BRIGHT (BFE1, 'A',	8, NULL 						, &States[S_BFGLAND+1]),
	S_BRIGHT (BFE1, 'B',	8, NULL 						, &States[S_BFGLAND+2]),
	S_BRIGHT (BFE1, 'C',	8, A_BFGSpray					, &States[S_BFGLAND+3]),
	S_BRIGHT (BFE1, 'D',	8, NULL 						, &States[S_BFGLAND+4]),
	S_BRIGHT (BFE1, 'E',	8, NULL 						, &States[S_BFGLAND+5]),
	S_BRIGHT (BFE1, 'F',	8, NULL 						, NULL)
};

IMPLEMENT_ACTOR (ABFGBall, Doom, -1, 128)
	PROP_RadiusFixed (13)
	PROP_HeightFixed (8)
	PROP_SpeedFixed (25)
	PROP_Damage (100)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF|MF_NOGRAVITY)
	PROP_Flags2 (MF2_PCROSS|MF2_IMPACT|MF2_NOTELEPORT)
	PROP_Flags4 (MF4_RANDOMIZE)
	PROP_RenderStyle (STYLE_Add)
	PROP_Alpha (TRANSLUC75)

	PROP_SpawnState (S_BFGSHOT)
	PROP_DeathState (S_BFGLAND)

	PROP_DeathSound ("weapons/bfgx")
	PROP_Obituary("$OB_MPBFG_BOOM")
END_DEFAULTS

FState ABFGExtra::States[] =
{
	S_BRIGHT (BFE2, 'A',	8, NULL 				, &States[1]),
	S_BRIGHT (BFE2, 'B',	8, NULL 				, &States[2]),
	S_BRIGHT (BFE2, 'C',	8, NULL 				, &States[3]),
	S_BRIGHT (BFE2, 'D',	8, NULL 				, NULL)
};

IMPLEMENT_ACTOR (ABFGExtra, Doom, -1, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_RenderStyle (STYLE_Add)
	PROP_Alpha (TRANSLUC75)

	PROP_SpawnState (0)
END_DEFAULTS

//
// A_FireBFG
//

void A_FireBFG (AActor *actor)
{
	player_t *player;

	if (NULL == (player = actor->player))
	{
		return;
	}
	// [RH] bfg can be forced to not use freeaim
	angle_t storedpitch = actor->pitch;
	int storedaimdist = player->userinfo.aimdist;

	AWeapon *weapon = actor->player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
	}

	if (dmflags2 & DF2_NO_FREEAIMBFG)
	{
		actor->pitch = 0;
		player->userinfo.aimdist = ANGLE_1*35;
	}
	P_SpawnPlayerMissile (actor, RUNTIME_CLASS(ABFGBall));
	actor->pitch = storedpitch;
	player->userinfo.aimdist = storedaimdist;
}

//
// A_BFGSpray
// Spawn a BFG explosion on every monster in view
//
void A_BFGSpray (AActor *mo) 
{
	int 				i;
	int 				j;
	int 				damage;
	angle_t 			an;
	AActor				*thingToHit;
	const PClass		*spraytype = NULL;
	int					numrays = 40;
	int					damagecnt = 15;

	int index = CheckIndex (3, NULL);
	if (index >= 0) 
	{
		spraytype = PClass::FindClass ((ENamedName)StateParameters[index]);
		numrays = EvalExpressionI (StateParameters[index+1], mo);
		if (numrays <= 0)
			numrays = 40;
		damagecnt = EvalExpressionI (StateParameters[index+2], mo);
		if (damagecnt <= 0)
			damagecnt = 15;
	}
	if (spraytype == NULL)
	{
		spraytype = RUNTIME_CLASS(ABFGExtra);
	}

	// [RH] Don't crash if no target
	if (!mo->target)
		return;

	// offset angles from its attack angle
	for (i = 0; i < numrays; i++)
	{
		an = mo->angle - ANG90/2 + ANG90/numrays*i;

		// mo->target is the originator (player) of the missile
		P_AimLineAttack (mo->target, an, 16*64*FRACUNIT, ANGLE_1*32);

		if (!linetarget)
			continue;

		Spawn (spraytype, linetarget->x, linetarget->y,
			linetarget->z + (linetarget->height>>2), ALLOW_REPLACE);
		
		damage = 0;
		for (j = 0; j < damagecnt; ++j)
			damage += (pr_bfgspray() & 7) + 1;

		thingToHit = linetarget;
		P_DamageMobj (thingToHit, mo->target, mo->target, damage, MOD_BFG_SPLASH);
		P_TraceBleed (damage, thingToHit, mo->target);
	}
}

//
// A_BFGsound
//
void A_BFGsound (AActor *actor)
{
	S_Sound (actor, CHAN_WEAPON, "weapons/bfgf", 1, ATTN_NORM);
}

