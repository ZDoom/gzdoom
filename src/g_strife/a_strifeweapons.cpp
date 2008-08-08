#include "a_pickups.h"
#include "p_local.h"
#include "m_random.h"
#include "a_strifeglobal.h"
#include "s_sound.h"
#include "p_enemy.h"
#include "templates.h"
#include "thingdef/thingdef.h"

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

// Punch Dagger -------------------------------------------------------------

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
	P_LineAttack (actor, angle, 80*FRACUNIT, pitch, damage, NAME_Melee, "StrifeSpark", true);

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

// Poison Bolt --------------------------------------------------------------

class APoisonBolt : public AActor
{
	DECLARE_CLASS (APoisonBolt, AActor)
public:
	int DoSpecialDamage (AActor *target, int damage);
};

IMPLEMENT_CLASS (APoisonBolt)

int APoisonBolt::DoSpecialDamage (AActor *target, int damage)
{
	if (target->flags & MF_NOBLOOD)
	{
		return -1;
	}
	if (target->health < 1000000)
	{
		if (!(target->flags2 & MF2_BOSS))			
			return target->health + 10;
		else
			return 50;
	}
	return 1;
}

// Strife's Crossbow --------------------------------------------------------

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
		P_SetPsprite (self->player, ps_flash, self->player->ReadyWeapon->FindState(NAME_Flash));
	}
}

//============================================================================
//
// A_FireElectric
//
//============================================================================

void A_FireArrow (AActor *self)
{
	angle_t savedangle;

	int index=CheckIndex(1);
	if (index<0) return;

	ENamedName MissileName=(ENamedName)StateParameters[index];

	if (self->player == NULL)
		return;

	AWeapon *weapon = self->player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
	}

	const PClass * ti=PClass::FindClass(MissileName);
	if (ti) 
	{
		savedangle = self->angle;
		self->angle += pr_electric.Random2 () << (18 - self->player->accuracy * 5 / 100);
		self->player->mo->PlayAttacking2 ();
		P_SpawnPlayerMissile (self, ti);
		self->angle = savedangle;
		S_Sound (self, CHAN_WEAPON, "weapons/xbowshoot", 1, ATTN_NORM);
	}
}

// Assault Gun --------------------------------------------------------------

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

	P_LineAttack (mo, angle, PLAYERMISSILERANGE, pitch, damage, NAME_None, NAME_StrifePuff);
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

// Mini-Missile Launcher ----------------------------------------------------

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
	P_SpawnPlayerMissile (self, PClass::FindClass("MiniMissile"));
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
	P_SpawnPuff (self, PClass::FindClass("MiniMissilePuff"), self->x, self->y, self->z, self->angle - ANGLE_180, 2, PF_HITTHING);
	trail = Spawn("RocketTrail", self->x - self->momx, self->y - self->momy, self->z, ALLOW_REPLACE);
	if (trail != NULL)
	{
		trail->momz = FRACUNIT;
	}
}

// Flame Thrower ------------------------------------------------------------

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
	self = P_SpawnPlayerMissile (self, PClass::FindClass("FlameMissile"));
	if (self != NULL)
	{
		self->momz += 5*FRACUNIT;
	}
}

// Mauler -------------------------------------------------------------------

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
		P_LineAttack (self, angle, PLAYERMISSILERANGE, pitch, damage, NAME_Disintegrate, NAME_MaulerPuff);
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
	P_SpawnPlayerMissile (self, PClass::FindClass("MaulerTorpedo"));
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

AActor *P_SpawnSubMissile (AActor *source, const PClass *type, AActor *target);

void A_MaulerTorpedoWave (AActor *self)
{
	AActor *wavedef = GetDefaultByName("MaulerTorpedoWave");
	fixed_t savedz;
	self->angle += ANGLE_180;

	// If the torpedo hit the ceiling, it should still spawn the wave
	savedz = self->z;
	if (wavedef && self->ceilingz - self->z < wavedef->height)
	{
		self->z = self->ceilingz - wavedef->height;
	}

	for (int i = 0; i < 80; ++i)
	{
		self->angle += ANGLE_45/10;
		P_SpawnSubMissile (self, PClass::FindClass("MaulerTorpedoWave"), self->target);
	}
	self->z = savedz;
}

AActor *P_SpawnSubMissile (AActor *source, const PClass *type, AActor *target)
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

	if (other->flags4 & MF4_SPECTRAL)
	{
		if (source->flags & MF_MISSILE && source->flags4 & MF4_SPECTRAL)
		{
			other->health = source->health;
		}
		else if (target->player != NULL)
		{
			other->health = -1;
		}
		else
		{
			other->health = -2;
		}
	}

	if (P_CheckMissileSpawn (other))
	{
		angle_t pitch = P_AimLineAttack (source, source->angle, 1024*FRACUNIT);
		other->momz = FixedMul (-finesine[pitch>>ANGLETOFINESHIFT], other->Speed);
		return other;
	}
	return NULL;
}

class APhosphorousFire : public AActor
{
	DECLARE_CLASS (APhosphorousFire, AActor)
public:
	int DoSpecialDamage (AActor *target, int damage);
};

IMPLEMENT_CLASS (APhosphorousFire)

int APhosphorousFire::DoSpecialDamage (AActor *target, int damage)
{
	if (target->flags & MF_NOBLOOD)
	{
		return damage / 2;
	}
	return Super::DoSpecialDamage (target, damage);
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

//============================================================================
//
// A_FireGrenade
//
//============================================================================

void A_FireGrenade (AActor *self)
{
	const PClass *grenadetype;
	player_t *player = self->player;
	AActor *grenade;
	angle_t an;
	fixed_t tworadii;
	AWeapon *weapon;

	int index=CheckIndex(3);
	if (index<0) return;

	ENamedName MissileName=(ENamedName)StateParameters[index];
	angle_t Angle=angle_t(EvalExpressionF (StateParameters[index+1], self) * ANGLE_1);

	if (player == NULL)
		return;

	if ((weapon = player->ReadyWeapon) == NULL)
		return;

	grenadetype = PClass::FindClass(MissileName);

	if (!weapon->DepleteAmmo (weapon->bAltFire))
		return;

	// Make it flash
	FState *jumpto = P_GetState(weapon, NULL, StateParameters[index + 2]);

	P_SetPsprite (player, ps_flash, jumpto);

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

	an = self->angle + Angle;
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
		spot = Spawn("SpectralLightningSpot", linetarget->x, linetarget->y, ONFLOORZ, ALLOW_REPLACE);
		if (spot != NULL)
		{
			spot->tracer = linetarget;
		}
	}
	else
	{
		spot = Spawn("SpectralLightningSpot", actor->x, actor->y, actor->z, ALLOW_REPLACE);
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
	player_t *player = actor->player;

	if (player == NULL || player->ReadyWeapon == NULL)
		return;

	P_DamageMobj (actor, actor, NULL, 2*4, 0, DMG_NO_ARMOR);
	S_Sound (actor, CHAN_WEAPON, "weapons/sigilcharge", 1, ATTN_NORM);

	P_SpawnPlayerMissile (actor, PClass::FindClass("SpectralLightningH1"));
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
		spot = P_SpawnSubMissile (actor, PClass::FindClass("SpectralLightningBall1"), actor);
		if (spot != NULL)
		{
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
		spot = P_SpawnPlayerMissile (actor, 0,0,0, PClass::FindClass("SpectralLightningBigV1"), actor->angle, &linetarget);
		if (spot != NULL)
		{
			spot->tracer = linetarget;
		}
	}
	else
	{
		spot = P_SpawnPlayerMissile (actor, PClass::FindClass("SpectralLightningBigV1"));
		if (spot != NULL)
		{
			spot->momx += FixedMul (spot->Speed, finecosine[actor->angle >> ANGLETOFINESHIFT]);
			spot->momy += FixedMul (spot->Speed, finesine[actor->angle >> ANGLETOFINESHIFT]);
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
	player_t *player = actor->player;

	if (player == NULL || player->ReadyWeapon == NULL)
		return;

	P_DamageMobj (actor, actor, NULL, 5*4, 0, DMG_NO_ARMOR);
	S_Sound (actor, CHAN_WEAPON, "weapons/sigilcharge", 1, ATTN_NORM);

	P_SpawnPlayerMissile (actor, PClass::FindClass("SpectralLightningBigBall1"));
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
