#include "actor.h"
#include "info.h"
#include "s_sound.h"
#include "m_random.h"
#include "a_pickups.h"
#include "a_doomglobal.h"
#include "d_player.h"
#include "p_pspr.h"
#include "p_local.h"
#include "p_inter.h"
#include "gstrings.h"
#include "p_effect.h"
#include "gi.h"

static FRandom pr_punch ("Punch");
static FRandom pr_saw ("Saw");
static FRandom pr_fireshotgun2 ("FireSG2");
static FRandom pr_fireplasma ("FirePlasma");
static FRandom pr_firerail ("FireRail");
static FRandom pr_bfgspray ("BFGSpray");

/* ammo ********************************************************************/

// a big item has five clip loads.
int clipammo[NUMAMMO] =
{
	10,		// bullets
	4,		// shells
	20,		// cells
	1		// rockets
};

// Clip --------------------------------------------------------------------

class AClip : public AAmmo
{
	DECLARE_ACTOR (AClip, AAmmo)
public:
	virtual bool TryPickup (AActor *toucher)
	{
		if (flags & MF_DROPPED)
			return P_GiveAmmo (toucher->player, am_clip, clipammo[am_clip]/2);
		else
			return P_GiveAmmo (toucher->player, am_clip, clipammo[am_clip]);
	}
	virtual ammotype_t GetAmmoType () const
	{
		return am_clip;
	}
protected:
	virtual const char *PickupMessage ()
	{
		return GStrings(GOTCLIP);
	}
};

FState AClip::States[] =
{
	S_NORMAL (CLIP, 'A',   -1, NULL 				, NULL)
};

IMPLEMENT_ACTOR (AClip, Doom, 2007, 11)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (16)
	PROP_Flags (MF_SPECIAL)

	PROP_SpawnState (0)
END_DEFAULTS

AT_GAME_SET (Clip)
{
	AmmoPics[am_clip] = "CLIPA0";
}

// Clip box ----------------------------------------------------------------

class AClipBox : public AAmmo
{
	DECLARE_ACTOR (AClipBox, AAmmo)
public:
	virtual bool TryPickup (AActor *toucher)
	{
		return P_GiveAmmo (toucher->player, am_clip, clipammo[am_clip]*5);
	}
	virtual ammotype_t GetAmmoType () const
	{
		return am_clip;
	}
protected:
	virtual const char *PickupMessage ()
	{
		return GStrings(GOTCLIPBOX);
	}
};

FState AClipBox::States[] =
{
	S_NORMAL (AMMO, 'A',   -1, NULL 				, NULL)
};

IMPLEMENT_ACTOR (AClipBox, Doom, 2048, 139)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (16)
	PROP_Flags (MF_SPECIAL)

	PROP_SpawnState (0)
END_DEFAULTS

// Rocket ------------------------------------------------------------------

class ARocketAmmo : public AAmmo
{
	DECLARE_ACTOR (ARocketAmmo, AAmmo)
public:
	virtual bool TryPickup (AActor *toucher)
	{
		return P_GiveAmmo (toucher->player, am_misl, clipammo[am_misl]);
	}
	virtual ammotype_t GetAmmoType () const
	{
		return am_misl;
	}
protected:
	virtual const char *PickupMessage ()
	{
		return GStrings(GOTROCKET);
	}
};

FState ARocketAmmo::States[] =
{
	S_NORMAL (ROCK, 'A',   -1, NULL 				, NULL)
};

IMPLEMENT_ACTOR (ARocketAmmo, Doom, 2010, 140)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (26)
	PROP_Flags (MF_SPECIAL)

	PROP_SpawnState (0)
END_DEFAULTS

AT_GAME_SET (RocketAmmo)
{
	AmmoPics[am_misl] = "ROCKA0";
}

// Rocket box --------------------------------------------------------------

class ARocketBox : public AAmmo
{
	DECLARE_ACTOR (ARocketBox, AAmmo)
public:
	virtual bool TryPickup (AActor *toucher)
	{
		return P_GiveAmmo (toucher->player, am_misl, clipammo[am_misl]*5);
	}
	virtual ammotype_t GetAmmoType () const
	{
		return am_misl;
	}
protected:
	virtual const char *PickupMessage ()
	{
		return GStrings(GOTROCKBOX);
	}
};

FState ARocketBox::States[] =
{
	S_NORMAL (BROK, 'A',   -1, NULL 				, NULL)
};

IMPLEMENT_ACTOR (ARocketBox, Doom, 2046, 141)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (16)
	PROP_Flags (MF_SPECIAL)

	PROP_SpawnState (0)
END_DEFAULTS

// Cell --------------------------------------------------------------------

class ACell : public AAmmo
{
	DECLARE_ACTOR (ACell, AAmmo)
public:
	virtual bool TryPickup (AActor *toucher)
	{
		return P_GiveAmmo (toucher->player, am_cell, clipammo[am_cell]);
	}
	virtual ammotype_t GetAmmoType () const
	{
		return am_cell;
	}
protected:
	virtual const char *PickupMessage ()
	{
		return GStrings(GOTCELL);
	}
};

FState ACell::States[] =
{
	S_NORMAL (CELL, 'A',   -1, NULL 				, NULL)
};

IMPLEMENT_ACTOR (ACell, Doom, 2047, 75)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (10)
	PROP_Flags (MF_SPECIAL)

	PROP_SpawnState (0)
END_DEFAULTS

AT_GAME_SET (Cell)
{
	AmmoPics[am_cell] = "CELLA0";
}

// Cell pack ---------------------------------------------------------------

class ACellPack : public AAmmo
{
	DECLARE_ACTOR (ACellPack, AAmmo)
public:
	virtual bool TryPickup (AActor *toucher)
	{
		return P_GiveAmmo (toucher->player, am_cell, clipammo[am_cell]*5);
	}
	virtual ammotype_t GetAmmoType () const
	{
		return am_cell;
	}
protected:
	virtual const char *PickupMessage ()
	{
		return GStrings(GOTCELLBOX);
	}
};

FState ACellPack::States[] =
{
	S_NORMAL (CELP, 'A',   -1, NULL 				, NULL)
};

IMPLEMENT_ACTOR (ACellPack, Doom, 17, 142)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (18)
	PROP_Flags (MF_SPECIAL)

	PROP_SpawnState (0)
END_DEFAULTS

// Shells ------------------------------------------------------------------

class AShell : public AAmmo
{
	DECLARE_ACTOR (AShell, AAmmo)
public:
	virtual bool TryPickup (AActor *toucher)
	{
		return P_GiveAmmo (toucher->player, am_shell, clipammo[am_shell]);
	}
	virtual ammotype_t GetAmmoType () const
	{
		return am_shell;
	}
protected:
	virtual const char *PickupMessage ()
	{
		return GStrings(GOTSHELLS);
	}
};

FState AShell::States[] =
{
	S_NORMAL (SHEL, 'A',   -1, NULL 				, NULL)
};

IMPLEMENT_ACTOR (AShell, Doom, 2008, 12)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (8)
	PROP_Flags (MF_SPECIAL)

	PROP_SpawnState (0)
END_DEFAULTS

AT_GAME_SET (Shell)
{
	AmmoPics[am_shell] = "SHELA0";
}

// Shell box ---------------------------------------------------------------

class AShellBox : public AAmmo
{
	DECLARE_ACTOR (AShellBox, AAmmo)
public:
	virtual bool TryPickup (AActor *toucher)
	{
		return P_GiveAmmo (toucher->player, am_shell, clipammo[am_shell]*5);
	}
	virtual ammotype_t GetAmmoType () const
	{
		return am_shell;
	}
protected:
	virtual const char *PickupMessage ()
	{
		return GStrings(GOTSHELLBOX);
	}
};

FState AShellBox::States[] =
{
	S_NORMAL (SBOX, 'A',   -1, NULL 				, NULL)
};

IMPLEMENT_ACTOR (AShellBox, Doom, 2049, 143)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (10)
	PROP_Flags (MF_SPECIAL)

	PROP_SpawnState (0)
END_DEFAULTS

/* the weapons that use the ammo above *************************************/

// Fist ---------------------------------------------------------------------

void A_Punch (player_t *, pspdef_t *);

class AFist : public AWeapon
{
	DECLARE_ACTOR (AFist, AWeapon)
public:
	weapontype_t OldStyleID () const;

	static FWeaponInfo WeaponInfo;
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

FWeaponInfo AFist::WeaponInfo =
{
	0,
	am_noammo,
	am_noammo,
	0,
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
	RUNTIME_CLASS(AFist),
	-1
};

IMPLEMENT_ACTOR (AFist, Doom, -1, 0)
END_DEFAULTS

WEAPON1 (wp_fist, AFist)

weapontype_t AFist::OldStyleID () const
{
	return wp_fist;
}

//
// A_Punch
//
void A_Punch (player_t *player, pspdef_t *psp)
{
	angle_t 	angle;
	int 		damage;
	int 		pitch;

	player->UseAmmo ();

	damage = (pr_punch()%10+1)<<1;

	if (player->powers[pw_strength])	
		damage *= 10;

	angle = player->mo->angle;

	angle += pr_punch.Random2() << 18;
	pitch = P_AimLineAttack (player->mo, angle, MELEERANGE);
	PuffType = RUNTIME_CLASS(ABulletPuff);
	P_LineAttack (player->mo, angle, MELEERANGE, pitch, damage);

	// turn to face target
	if (linetarget)
	{
		S_Sound (player->mo, CHAN_WEAPON, "*fist", 1, ATTN_NORM);
		player->mo->angle = R_PointToAngle2 (player->mo->x,
											 player->mo->y,
											 linetarget->x,
											 linetarget->y);
	}
}

// Pistol -------------------------------------------------------------------

void A_FirePistol (player_t *, pspdef_t *);

class APistol : public AWeapon
{
	DECLARE_ACTOR (APistol, AWeapon)
public:
	weapontype_t OldStyleID () const;

	static FWeaponInfo WeaponInfo;
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

FWeaponInfo APistol::WeaponInfo =
{
	0,
	am_clip,
	am_clip,
	1,
	20,
	&States[S_PISTOLUP],
	&States[S_PISTOLDOWN],
	&States[S_PISTOL],
	&States[S_PISTOL1],
	&States[S_PISTOL1],
	&States[S_PISTOLFLASH],
	RUNTIME_CLASS(AClip),
	100,
	0,
	NULL,
	NULL,
	RUNTIME_CLASS(APistol),
	-1
};

IMPLEMENT_ACTOR (APistol, Doom, -1, 0)
END_DEFAULTS

WEAPON1 (wp_pistol, APistol)

weapontype_t APistol::OldStyleID () const
{
	return wp_pistol;
}

//
// A_FirePistol
//
void A_FirePistol (player_t *player, pspdef_t *psp)
{
	S_Sound (player->mo, CHAN_WEAPON, "weapons/pistol", 1, ATTN_NORM);

	player->mo->PlayAttacking2 ();
	player->UseAmmo ();

	P_SetPsprite (player, ps_flash,
		wpnlev1info[player->readyweapon]->flashstate);

	PuffType = RUNTIME_CLASS(ABulletPuff);
	P_BulletSlope (player->mo);
	P_GunShot (player->mo, !player->refire);
}

// Chainsaw -----------------------------------------------------------------

void A_Saw (player_t *, pspdef_t *);

class AChainsaw : public AWeapon
{
	DECLARE_ACTOR (AChainsaw, AWeapon)
protected:
	const char *PickupMessage ();
public:
	weapontype_t OldStyleID () const;

	static FWeaponInfo WeaponInfo;
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

FWeaponInfo AChainsaw::WeaponInfo =
{
	0,
	am_noammo,
	am_noammo,
	0,
	0,
	&States[S_SAWUP],
	&States[S_SAWDOWN],
	&States[S_SAW],
	&States[S_SAW1],
	&States[S_SAW1],
	NULL,
	RUNTIME_CLASS(AChainsaw),
	0,
	0,
	"weapons/sawup",
	"weapons/sawidle",
	RUNTIME_CLASS(AChainsaw),
	-1
};

IMPLEMENT_ACTOR (AChainsaw, Doom, 2005, 32)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (16)
	PROP_Flags (MF_SPECIAL)

	PROP_SpawnState (S_CSAW)
END_DEFAULTS

WEAPON1 (wp_chainsaw, AChainsaw)

weapontype_t AChainsaw::OldStyleID () const
{
	return wp_chainsaw;
}

const char *AChainsaw::PickupMessage ()
{
	return GStrings(GOTCHAINSAW);
}

//
// A_Saw
//
void A_Saw (player_t *player, pspdef_t *psp)
{
	angle_t 	angle;
	int 		damage;

	player->UseAmmo ();

	damage = 2 * (pr_saw()%10+1);
	angle = player->mo->angle;
	angle += pr_saw.Random2() << 18;
	
	// use meleerange + 1 so the puff doesn't skip the flash
	// [RH] What I think that really means is that they want the puff to show
	// up on walls. If the distance to P_LineAttack is <= MELEERANGE, then it
	// won't puff the wall, which is why the fist does not create puffs on
	// the walls.
	PuffType = RUNTIME_CLASS(ABulletPuff);
	P_LineAttack (player->mo, angle, MELEERANGE+1,
				  P_AimLineAttack (player->mo, angle, MELEERANGE+1), damage);

	if (!linetarget)
	{
		S_Sound (player->mo, CHAN_WEAPON, "weapons/sawfull", 1, ATTN_NORM);
		return;
	}
	S_Sound (player->mo, CHAN_WEAPON, "weapons/sawhit", 1, ATTN_NORM);
		
	// turn to face target
	angle = R_PointToAngle2 (player->mo->x, player->mo->y,
							 linetarget->x, linetarget->y);
	if (angle - player->mo->angle > ANG180)
	{
		if (angle - player->mo->angle < (angle_t)(-ANG90/20))
			player->mo->angle = angle + ANG90/21;
		else
			player->mo->angle -= ANG90/20;
	}
	else
	{
		if (angle - player->mo->angle > ANG90/20)
			player->mo->angle = angle - ANG90/21;
		else
			player->mo->angle += ANG90/20;
	}
	player->mo->flags |= MF_JUSTATTACKED;
}

// Shotgun ------------------------------------------------------------------

void A_FireShotgun (player_t *, pspdef_t *);

class AShotgun : public AWeapon
{
	DECLARE_ACTOR (AShotgun, AWeapon)
protected:
	const char *PickupMessage ();
public:
	weapontype_t OldStyleID () const;

	static FWeaponInfo WeaponInfo;
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

FWeaponInfo AShotgun::WeaponInfo =
{
	0,
	am_shell,
	am_shell,
	1,
	8,
	&States[S_SGUNUP],
	&States[S_SGUNDOWN],
	&States[S_SGUN],
	&States[S_SGUN1],
	&States[S_SGUN1],
	&States[S_SGUNFLASH],
	RUNTIME_CLASS(AShotgun),
	100,
	0,
	NULL,
	NULL,
	RUNTIME_CLASS(AShotgun),
	-1
};

IMPLEMENT_ACTOR (AShotgun, Doom, 2001, 27)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (16)
	PROP_Flags (MF_SPECIAL)

	PROP_SpawnState (S_SHOT)
END_DEFAULTS

WEAPON1 (wp_shotgun, AShotgun)

weapontype_t AShotgun::OldStyleID () const
{
	return wp_shotgun;
}

const char *AShotgun::PickupMessage ()
{
	return GStrings(GOTSHOTGUN);
}

//
// A_FireShotgun
//
void A_FireShotgun (player_t *player, pspdef_t *psp)
{
	int i;

	S_Sound (player->mo, CHAN_WEAPON,  "weapons/shotgf", 1, ATTN_NORM);
	player->mo->PlayAttacking2 ();
	player->UseAmmo ();

	P_SetPsprite (player, ps_flash, wpnlev1info[player->readyweapon]->flashstate);

	P_BulletSlope (player->mo);
	PuffType = RUNTIME_CLASS(ABulletPuff);

	for (i=0 ; i<7 ; i++)
		P_GunShot (player->mo, false);
}

// Super Shotgun ------------------------------------------------------------

void A_FireShotgun2 (player_t *, pspdef_t *);
void A_CheckReload (player_t *, pspdef_t *);
void A_OpenShotgun2 (player_t *, pspdef_t *);
void A_LoadShotgun2 (player_t *, pspdef_t *);
void A_CloseShotgun2 (player_t *, pspdef_t *);

class ASuperShotgun : public AWeapon
{
	DECLARE_ACTOR (ASuperShotgun, AWeapon)
protected:
	const char *PickupMessage ();
public:
	weapontype_t OldStyleID () const;

	static FWeaponInfo WeaponInfo;
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

FWeaponInfo ASuperShotgun::WeaponInfo =
{
	0,
	am_shell,
	am_shell,
	2,
	8,
	&States[S_DSGUNUP],
	&States[S_DSGUNDOWN],
	&States[S_DSGUN],
	&States[S_DSGUN1],
	&States[S_DSGUN1],
	&States[S_DSGUNFLASH],
	RUNTIME_CLASS(ASuperShotgun),
	100,
	0,
	NULL,
	NULL,
	RUNTIME_CLASS(ASuperShotgun),
	-1
};

IMPLEMENT_ACTOR (ASuperShotgun, Doom, 82, 33)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (16)
	PROP_Flags (MF_SPECIAL)

	PROP_SpawnState (S_SHOT2)
END_DEFAULTS

WEAPON1 (wp_supershotgun, ASuperShotgun)

weapontype_t ASuperShotgun::OldStyleID () const
{
	return wp_supershotgun;
}

const char *ASuperShotgun::PickupMessage ()
{
	return GStrings(GOTSHOTGUN2);
}

//
// A_FireShotgun2
//
void A_FireShotgun2 (player_t *player, pspdef_t *psp)
{
	int 		i;
	angle_t 	angle;
	int 		damage;
				
		
	S_Sound (player->mo, CHAN_WEAPON, "weapons/sshotf", 1, ATTN_NORM);
	player->mo->PlayAttacking2 ();
	player->UseAmmo ();

	P_SetPsprite (player, ps_flash,
		wpnlev1info[player->readyweapon]->flashstate);

	P_BulletSlope (player->mo);
		
	PuffType = RUNTIME_CLASS(ABulletPuff);
	for (i=0 ; i<20 ; i++)
	{
		damage = 5*(pr_fireshotgun2()%3+1);
		angle = player->mo->angle;
		angle += pr_fireshotgun2.Random2() << 19;

		// Doom adjusts the bullet slope by shifting a random number [-255,255]
		// left 5 places. At 2048 units away, this means the vertical position
		// of the shot can deviate as much as 255 units from nominal. So using
		// some simple trigonometry, that means the vertical angle of the shot
		// can deviate by as many as ~7.097 degrees or ~84676099 BAMs.

		P_LineAttack (player->mo,
					  angle,
					  MISSILERANGE,
					  bulletpitch + (pr_fireshotgun2.Random2() * 332063), damage);
	}
}

void A_CheckReload (player_t *player, pspdef_t *psp)
{
	P_CheckAmmo (player);
}

void A_OpenShotgun2 (player_t *player, pspdef_t *psp)
{
	S_Sound (player->mo, CHAN_WEAPON, "weapons/sshoto", 1, ATTN_NORM);
}

void A_LoadShotgun2 (player_t *player, pspdef_t *psp)
{
	S_Sound (player->mo, CHAN_WEAPON, "weapons/sshotl", 1, ATTN_NORM);
}

void A_CloseShotgun2 (player_t *player, pspdef_t *psp)
{
	S_Sound (player->mo, CHAN_WEAPON, "weapons/sshotc", 1, ATTN_NORM);
	A_ReFire(player,psp);
}

// Chaingun -----------------------------------------------------------------

void A_FireCGun (player_t *, pspdef_t *);

class AChaingun : public AWeapon
{
	DECLARE_ACTOR (AChaingun, AWeapon)
protected:
	const char *PickupMessage ();
public:
	weapontype_t OldStyleID () const;

	static FWeaponInfo WeaponInfo;
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

FWeaponInfo AChaingun::WeaponInfo =
{
	0,
	am_clip,
	am_clip,
	1,
	20,
	&States[S_CHAINUP],
	&States[S_CHAINDOWN],
	&States[S_CHAIN],
	&States[S_CHAIN1],
	&States[S_CHAIN1],
	&States[S_CHAINFLASH],
	RUNTIME_CLASS(AChaingun),
	100,
	0,
	NULL,
	NULL,
	RUNTIME_CLASS(AChaingun),
	-1
};

IMPLEMENT_ACTOR (AChaingun, Doom, 2002, 28)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (16)
	PROP_Flags (MF_SPECIAL)

	PROP_SpawnState (S_MGUN)
END_DEFAULTS

WEAPON1 (wp_chaingun, AChaingun)

weapontype_t AChaingun::OldStyleID () const
{
	return wp_chaingun;
}

const char *AChaingun::PickupMessage ()
{
	return GStrings(GOTCHAINGUN);
}

//
// A_FireCGun
//
void A_FireCGun (player_t *player, pspdef_t *psp)
{
	S_Sound (player->mo, CHAN_WEAPON, "weapons/chngun", 1, ATTN_NORM);
	FWeaponInfo *wpninfo = wpnlev1info[player->readyweapon];

	if (wpninfo->ammo < NUMAMMO && !player->ammo[wpninfo->ammo])
		return;
				
	player->mo->PlayAttacking2 ();
	player->UseAmmo ();

	if (wpninfo->flashstate != NULL)
	{
		P_SetPsprite (player, ps_flash,
			wpninfo->flashstate + (psp->state - wpninfo->atkstate));
	}

	P_BulletSlope (player->mo);
	PuffType = RUNTIME_CLASS (ABulletPuff);		
	P_GunShot (player->mo, !player->refire);
}

// Rocket launcher ---------------------------------------------------------

void A_FireMissile (player_t *, pspdef_t *);
void A_Explode (AActor *);

class ARocketLauncher : public AWeapon
{
	DECLARE_ACTOR (ARocketLauncher, AWeapon)
protected:
	const char *PickupMessage ();
public:
	weapontype_t OldStyleID () const;

	static FWeaponInfo WeaponInfo;
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

FWeaponInfo ARocketLauncher::WeaponInfo =
{
	WIF_NOAUTOFIRE,
	am_misl,
	am_misl,
	1,
	2,
	&States[S_MISSILEUP],
	&States[S_MISSILEDOWN],
	&States[S_MISSILE],
	&States[S_MISSILE1],
	&States[S_MISSILE1],
	&States[S_MISSILEFLASH],
	RUNTIME_CLASS(ARocketLauncher),
	100,
	0,
	NULL,
	NULL,
	RUNTIME_CLASS(ARocketLauncher),
	-1
};

IMPLEMENT_ACTOR (ARocketLauncher, Doom, 2003, 29)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (16)
	PROP_Flags (MF_SPECIAL)

	PROP_SpawnState (S_LAUN)
END_DEFAULTS

WEAPON1 (wp_missile, ARocketLauncher)

weapontype_t ARocketLauncher::OldStyleID () const
{
	return wp_missile;
}

const char *ARocketLauncher::PickupMessage ()
{
	return GStrings(GOTLAUNCHER);
}

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

	PROP_SpawnState (S_ROCKET)
	PROP_DeathState (S_EXPLODE)

	PROP_SeeSound ("weapons/rocklf")
	PROP_DeathSound ("weapons/rocklx")
END_DEFAULTS

void ARocket::BeginPlay ()
{
	Super::BeginPlay ();
	effects |= FX_ROCKET;
}

//
// A_FireMissile
//
void A_FireMissile (player_t *player, pspdef_t *psp)
{
	player->UseAmmo ();
	P_SpawnPlayerMissile (player->mo, RUNTIME_CLASS(ARocket));
}

// Plasma rifle ------------------------------------------------------------

void A_FirePlasma (player_t *, pspdef_t *);

class APlasmaRifle : public AWeapon
{
	DECLARE_ACTOR (APlasmaRifle, AWeapon)
protected:
	const char *PickupMessage ();
public:
	weapontype_t OldStyleID () const;

	static FWeaponInfo WeaponInfo;
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

FWeaponInfo APlasmaRifle::WeaponInfo =
{
	0,
	am_cell,
	am_cell,
	1,
	40,
	&States[S_PLASMAUP],
	&States[S_PLASMADOWN],
	&States[S_PLASMA],
	&States[S_PLASMA1],
	&States[S_PLASMA1],
	&States[S_PLASMAFLASH],
	RUNTIME_CLASS(APlasmaRifle),
	100,
	0,
	NULL,
	NULL,
	RUNTIME_CLASS(APlasmaRifle),
	-1
};

IMPLEMENT_ACTOR (APlasmaRifle, Doom, 2004, 30)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (16)
	PROP_Flags (MF_SPECIAL)

	PROP_SpawnState (S_PLAS)
END_DEFAULTS

WEAPON1 (wp_plasma, APlasmaRifle)

weapontype_t APlasmaRifle::OldStyleID () const
{
	return wp_plasma;
}

const char *APlasmaRifle::PickupMessage ()
{
	return GStrings(GOTPLASMA);
}

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
	PROP_RenderStyle (STYLE_Add)
	PROP_Alpha (TRANSLUC75)

	PROP_SpawnState (S_PLASBALL)
	PROP_DeathState (S_PLASEXP)

	PROP_SeeSound ("weapons/plasmaf")
	PROP_DeathSound ("weapons/plasmax")
END_DEFAULTS

//
// A_FirePlasma
//
void A_FirePlasma (player_t *player, pspdef_t *psp)
{
	player->UseAmmo ();

	if (wpnlev1info[player->readyweapon]->flashstate)
	{
		P_SetPsprite (player, ps_flash,
			wpnlev1info[player->readyweapon]->flashstate
			+ (pr_fireplasma()&1));
	}

	P_SpawnPlayerMissile (player->mo, RUNTIME_CLASS(APlasmaBall));
}

//
// [RH] A_FireRailgun
//
static int RailOffset;

void A_FireRailgun (player_t *player, pspdef_t *psp)
{
	int damage;

	player->UseAmmo ();

	if (wpnlev1info[player->readyweapon]->flashstate)
	{
		P_SetPsprite (player, ps_flash,
			wpnlev1info[player->readyweapon]->flashstate
			+ (pr_firerail()&1));
	}

	damage = deathmatch ? 100 : 150;

	P_RailAttack (player->mo, damage, RailOffset);
	RailOffset = 0;
}

void A_FireRailgunRight (player_t *player, pspdef_t *psp)
{
	RailOffset = 10;
	A_FireRailgun (player, psp);
}

void A_FireRailgunLeft (player_t *player, pspdef_t *psp)
{
	RailOffset = -10;
	A_FireRailgun (player, psp);
}

void A_RailWait (player_t *player, pspdef_t *psp)
{
	// Okay, this was stupid. Just use a NULL function instead of this.
}

// BFG 9000 -----------------------------------------------------------------

void A_FireBFG (player_t *, pspdef_t *);
void A_BFGSpray (AActor *);
void A_BFGsound (player_t *, pspdef_t *);

class ABFG9000 : public AWeapon
{
	DECLARE_ACTOR (ABFG9000, AWeapon)
protected:
	const char *PickupMessage ();
public:
	weapontype_t OldStyleID () const;

	static FWeaponInfo WeaponInfo;
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

FWeaponInfo ABFG9000::WeaponInfo =
{
	WIF_NOAUTOFIRE,
	am_cell,
	am_cell,
	40,
	40,
	&States[S_BFGUP],
	&States[S_BFGDOWN],
	&States[S_BFG],
	&States[S_BFG1],
	&States[S_BFG1],
	&States[S_BFGFLASH],
	RUNTIME_CLASS(ABFG9000),
	100,
	0,
	NULL,
	NULL,
	RUNTIME_CLASS(ABFG9000),
	-1
};

IMPLEMENT_ACTOR (ABFG9000, Doom, 2006, 31)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (20)
	PROP_Flags (MF_SPECIAL)

	PROP_SpawnState (S_BFUG)
END_DEFAULTS

WEAPON1 (wp_bfg, ABFG9000)

weapontype_t ABFG9000::OldStyleID () const
{
	return wp_bfg;
}

const char *ABFG9000::PickupMessage ()
{
	return GStrings(GOTBFG9000);
}

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
	PROP_RenderStyle (STYLE_Add)
	PROP_Alpha (TRANSLUC75)

	PROP_SpawnState (S_BFGSHOT)
	PROP_DeathState (S_BFGLAND)

	PROP_DeathSound ("weapons/bfgx")
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

void A_FireBFG (player_t *player, pspdef_t *psp)
{
	// [RH] bfg can be forced to not use freeaim
	angle_t storedpitch = player->mo->pitch;
	int storedaimdist = player->userinfo.aimdist;

	player->UseAmmo ();

	if (dmflags2 & DF2_NO_FREEAIMBFG)
	{
		player->mo->pitch = 0;
		player->userinfo.aimdist = ANGLE_1*35;
	}
	P_SpawnPlayerMissile (player->mo, RUNTIME_CLASS(ABFGBall));
	player->mo->pitch = storedpitch;
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

	// [RH] Don't crash if no target
	if (!mo->target)
		return;

	// offset angles from its attack angle
	for (i = 0; i < 40; i++)
	{
		an = mo->angle - ANG90/2 + ANG90/40*i;

		// mo->target is the originator (player) of the missile
		P_AimLineAttack (mo->target, an, 16*64*FRACUNIT, ANGLE_1*32);

		if (!linetarget)
			continue;

		Spawn<ABFGExtra> (linetarget->x, linetarget->y,
			linetarget->z + (linetarget->height>>2));
		
		damage = 0;
		for (j = 0; j < 15; ++j)
			damage += (pr_bfgspray() & 7) + 1;

		P_DamageMobj (linetarget, mo->target, mo->target, damage, MOD_BFG_SPLASH);
		P_TraceBleed (damage, linetarget, mo->target);
	}
}

//
// A_BFGsound
//
void A_BFGsound (player_t *player, pspdef_t *psp)
{
	S_Sound (player->mo, CHAN_WEAPON, "weapons/bfgf", 1, ATTN_NORM);
}

