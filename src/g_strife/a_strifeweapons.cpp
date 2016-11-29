/*
#include "a_pickups.h"
#include "p_local.h"
#include "m_random.h"
#include "a_strifeglobal.h"
#include "s_sound.h"
#include "p_enemy.h"
#include "templates.h"
#include "vm.h"
#include "doomstat.h"
*/

// Note: Strife missiles do 1-4 times their damage amount.
// Doom missiles do 1-8 times their damage amount, so to
// make the strife missiles do proper damage without
// hacking more stuff in the executable, be sure to give
// all Strife missiles the MF4_STRIFEDAMAGE flag.

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

// Assault Gun --------------------------------------------------------------

// Mini-Missile Launcher ----------------------------------------------------

// Flame Thrower ------------------------------------------------------------

//============================================================================
//
// A_FlameDie
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_FlameDie)
{
	PARAM_SELF_PROLOGUE(AActor);

	self->flags |= MF_NOGRAVITY;
	self->Vel.Z = pr_flamedie() & 3;
	return 0;
}

//============================================================================
//
// A_FireFlamer
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_FireFlamer)
{
	PARAM_ACTION_PROLOGUE(AActor);

	player_t *player = self->player;

	if (player != NULL)
	{
		AWeapon *weapon = self->player->ReadyWeapon;
		if (weapon != NULL)
		{
			if (!weapon->DepleteAmmo (weapon->bAltFire))
				return 0;
		}
		player->mo->PlayAttacking2 ();
	}

	self->Angles.Yaw += pr_flamethrower.Random2() * (5.625/256.);
	self = P_SpawnPlayerMissile (self, PClass::FindActor("FlameMissile"));
	if (self != NULL)
	{
		self->Vel.Z += 5;
	}
	return 0;
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

DEFINE_ACTION_FUNCTION(AActor, A_FireMauler1)
{
	PARAM_ACTION_PROLOGUE(AActor);

	if (self->player != NULL)
	{
		AWeapon *weapon = self->player->ReadyWeapon;
		if (weapon != NULL)
		{
			if (!weapon->DepleteAmmo (weapon->bAltFire))
				return 0;
		}
		// Strife apparently didn't show the player shooting. Let's fix that.
		self->player->mo->PlayAttacking2 ();
	}

	S_Sound (self, CHAN_WEAPON, "weapons/mauler1", 1, ATTN_NORM);


	DAngle bpitch = P_BulletSlope (self);

	for (int i = 0; i < 20; ++i)
	{
		int damage = 5 * (pr_mauler1() % 3 + 1);
		DAngle angle = self->Angles.Yaw + pr_mauler1.Random2() * (11.25 / 256);
		DAngle pitch = bpitch + pr_mauler1.Random2() * (7.097 / 256);
		
		// Strife used a range of 2112 units for the mauler to signal that
		// it should use a different puff. ZDoom's default range is longer
		// than this, so let's not handicap it by being too faithful to the
		// original.
		P_LineAttack (self, angle, PLAYERMISSILERANGE, pitch, damage, NAME_Hitscan, NAME_MaulerPuff);
	}
	return 0;
}

//============================================================================
//
// A_FireMauler2Pre
//
// Makes some noise and moves the psprite.
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_FireMauler2Pre)
{
	PARAM_ACTION_PROLOGUE(AActor);

	S_Sound (self, CHAN_WEAPON, "weapons/mauler2charge", 1, ATTN_NORM);

	if (self->player != nullptr)
	{
		self->player->GetPSprite(PSP_WEAPON)->x += pr_mauler2.Random2() / 64.;
		self->player->GetPSprite(PSP_WEAPON)->y += pr_mauler2.Random2() / 64.;
	}
	return 0;
}

//============================================================================
//
// A_FireMauler2Pre
//
// Fires the torpedo.
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_FireMauler2)
{
	PARAM_ACTION_PROLOGUE(AActor);

	if (self->player != NULL)
	{
		AWeapon *weapon = self->player->ReadyWeapon;
		if (weapon != NULL)
		{
			if (!weapon->DepleteAmmo (weapon->bAltFire))
				return 0;
		}
		self->player->mo->PlayAttacking2 ();
	}
	P_SpawnPlayerMissile (self, PClass::FindActor("MaulerTorpedo"));
	P_DamageMobj (self, self, NULL, 20, self->DamageType);
	self->Thrust(self->Angles.Yaw+180., 7.8125);
	return 0;
}

//============================================================================
//
// A_MaulerTorpedoWave
//
// Launches lots of balls when the torpedo hits something.
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_MaulerTorpedoWave)
{
	PARAM_SELF_PROLOGUE(AActor);

	AActor *wavedef = GetDefaultByName("MaulerTorpedoWave");
	double savedz;
	self->Angles.Yaw += 180.;

	// If the torpedo hit the ceiling, it should still spawn the wave
	savedz = self->Z();
	if (wavedef && self->ceilingz < self->Z() + wavedef->Height)
	{
		self->SetZ(self->ceilingz - wavedef->Height);
	}

	for (int i = 0; i < 80; ++i)
	{
		self->Angles.Yaw += 4.5;
		P_SpawnSubMissile (self, PClass::FindActor("MaulerTorpedoWave"), self->target);
	}
	self->SetZ(savedz);
	return 0;
}

class APhosphorousFire : public AActor
{
	DECLARE_CLASS (APhosphorousFire, AActor)
public:
	int DoSpecialDamage (AActor *target, int damage, FName damagetype);
};

IMPLEMENT_CLASS(APhosphorousFire, false, false)

int APhosphorousFire::DoSpecialDamage (AActor *target, int damage, FName damagetype)
{
	if (target->flags & MF_NOBLOOD)
	{
		return damage / 2;
	}
	return Super::DoSpecialDamage (target, damage, damagetype);
}

DEFINE_ACTION_FUNCTION(AActor, A_BurnArea)
{
	PARAM_SELF_PROLOGUE(AActor);

	P_RadiusAttack (self, self->target, 128, 128, self->DamageType, RADF_HURTSOURCE);
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_Burnination)
{
	PARAM_SELF_PROLOGUE(AActor);

	self->Vel.Z -= 8;
	self->Vel.X += (pr_phburn.Random2 (3));
	self->Vel.Y += (pr_phburn.Random2 (3));
	S_Sound (self, CHAN_VOICE, "world/largefire", 1, ATTN_NORM);

	// Only the main fire spawns more.
	if (!(self->flags & MF_DROPPED))
	{
		// Original x and y offsets seemed to be like this:
		//		x + (((pr_phburn() + 12) & 31) << F.RACBITS);
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

		DVector2 pos = self->Vec2Offset((double)xofs, (double)yofs);
		sector_t * sector = P_PointInSector(pos);

		// The sector's floor is too high so spawn the flame elsewhere.
		if (sector->floorplane.ZatPoint(pos) > self->Z() + self->MaxStepHeight)
		{
			pos = self->Pos();
		}

		AActor *drop = Spawn<APhosphorousFire> (DVector3(pos, self->Z() + 4.), ALLOW_REPLACE);
		if (drop != NULL)
		{
			drop->Vel.X = self->Vel.X + pr_phburn.Random2 (7);
			drop->Vel.Y = self->Vel.Y + pr_phburn.Random2 (7);
			drop->Vel.Z = self->Vel.Z - 1;
			drop->reactiontime = (pr_phburn() & 3) + 2;
			drop->flags |= MF_DROPPED;
		}
	}
	return 0;
}

//============================================================================
//
// A_FireGrenade
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_FireGrenade)
{
	PARAM_ACTION_PROLOGUE(AActor);
	PARAM_CLASS(grenadetype, AActor);
	PARAM_ANGLE(angleofs);
	PARAM_STATE(flash)

	player_t *player = self->player;
	AActor *grenade;
	DAngle an;
	AWeapon *weapon;

	if (player == nullptr || grenadetype == nullptr)
		return 0;

	if ((weapon = player->ReadyWeapon) == nullptr)
		return 0;

	if (!weapon->DepleteAmmo (weapon->bAltFire))
		return 0;

	P_SetPsprite (player, PSP_FLASH, flash, true);

	if (grenadetype != nullptr)
	{
		self->AddZ(32);
		grenade = P_SpawnSubMissile (self, grenadetype, self);
		self->AddZ(-32);
		if (grenade == nullptr)
			return 0;

		if (grenade->SeeSound != 0)
		{
			S_Sound (grenade, CHAN_VOICE, grenade->SeeSound, 1, ATTN_NORM);
		}

		grenade->Vel.Z = (-self->Angles.Pitch.TanClamped()) * grenade->Speed + 8;

		DVector2 offset = self->Angles.Yaw.ToVector(self->radius + grenade->radius);
		DAngle an = self->Angles.Yaw + angleofs;
		offset += an.ToVector(15);
		grenade->SetOrigin(grenade->Vec3Offset(offset.X, offset.Y, 0.), false);
	}
	return 0;
}

