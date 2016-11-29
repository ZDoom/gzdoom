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

