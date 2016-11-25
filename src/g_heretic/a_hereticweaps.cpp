/*
#include "templates.h"
#include "actor.h"
#include "info.h"
#include "s_sound.h"
#include "m_random.h"
#include "a_pickups.h"
#include "d_player.h"
#include "p_pspr.h"
#include "p_local.h"
#include "gstrings.h"
#include "gi.h"
#include "r_data/r_translate.h"
#include "vm.h"
#include "doomstat.h"
*/

static FRandom pr_boltspark ("BoltSpark");
static FRandom pr_macerespawn ("MaceRespawn");
static FRandom pr_maceatk ("FireMacePL1");
static FRandom pr_bfx1 ("BlasterFX1");
static FRandom pr_ripd ("RipperD");
static FRandom pr_fb1 ("FireBlasterPL1");
static FRandom pr_bfx1t ("BlasterFX1Tick");
static FRandom pr_rp ("RainPillar");
static FRandom pr_fsr1 ("FireSkullRodPL1");
static FRandom pr_storm ("SkullRodStorm");
static FRandom pr_impact ("RainImpact");
static FRandom pr_pfx1 ("PhoenixFX1");
static FRandom pr_pfx2 ("PhoenixFX2");
static FRandom pr_fp2 ("FirePhoenixPL2");

#define FLAME_THROWER_TICS (10*TICRATE)

void P_DSparilTeleport (AActor *actor);

#define USE_BLSR_AMMO_1 1
#define USE_BLSR_AMMO_2 5
#define USE_SKRD_AMMO_1 1
#define USE_SKRD_AMMO_2 5
#define USE_PHRD_AMMO_1 1
#define USE_PHRD_AMMO_2 1
#define USE_MACE_AMMO_1 1
#define USE_MACE_AMMO_2 5

extern bool P_AutoUseChaosDevice (player_t *player);


// --- Phoenix Rod ----------------------------------------------------------

class APhoenixRod : public AWeapon
{
	DECLARE_CLASS (APhoenixRod, AWeapon)
public:
	
	void Serialize(FSerializer &arc)
	{
		Super::Serialize (arc);
		arc("flamecount", FlameCount);
	}
	int FlameCount;		// for flamethrower duration
};

class APhoenixRodPowered : public APhoenixRod
{
	DECLARE_CLASS (APhoenixRodPowered, APhoenixRod)
public:
	void EndPowerup ();
};

IMPLEMENT_CLASS(APhoenixRod, false, false)
IMPLEMENT_CLASS(APhoenixRodPowered, false, false)

void APhoenixRodPowered::EndPowerup ()
{
	DepleteAmmo (bAltFire);
	Owner->player->refire = 0;
	S_StopSound (Owner, CHAN_WEAPON);
	Owner->player->ReadyWeapon = SisterWeapon;
	P_SetPsprite(Owner->player, PSP_WEAPON, SisterWeapon->GetReadyState());
}

// Phoenix FX 2 -------------------------------------------------------------

class APhoenixFX2 : public AActor
{
	DECLARE_CLASS (APhoenixFX2, AActor)
public:
	int DoSpecialDamage (AActor *target, int damage, FName damagetype);
};

IMPLEMENT_CLASS(APhoenixFX2, false, false)

int APhoenixFX2::DoSpecialDamage (AActor *target, int damage, FName damagetype)
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

DEFINE_ACTION_FUNCTION(AActor, A_FirePhoenixPL1)
{
	PARAM_ACTION_PROLOGUE(AActor);

	player_t *player;

	if (NULL == (player = self->player))
	{
		return 0;
	}

	AWeapon *weapon = self->player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return 0;
	}
	P_SpawnPlayerMissile (self, PClass::FindActor("PhoenixFX1"));
	self->Thrust(self->Angles.Yaw + 180, 4);
	return 0;
}

//----------------------------------------------------------------------------
//
// PROC A_PhoenixPuff
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_PhoenixPuff)
{
	PARAM_SELF_PROLOGUE(AActor);

	AActor *puff;
	DAngle ang;

	//[RH] Heretic never sets the target for seeking
	//P_SeekerMissile (self, 5, 10);
	puff = Spawn("PhoenixPuff", self->Pos(), ALLOW_REPLACE);
	ang = self->Angles.Yaw + 90;
	puff->Vel = DVector3(ang.ToVector(1.3), 0);

	puff = Spawn("PhoenixPuff", self->Pos(), ALLOW_REPLACE);
	ang = self->Angles.Yaw - 90;
	puff->Vel = DVector3(ang.ToVector(1.3), 0);
	return 0;
}

//----------------------------------------------------------------------------
//
// PROC A_InitPhoenixPL2
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_InitPhoenixPL2)
{
	PARAM_ACTION_PROLOGUE(AActor);

	if (self->player != NULL)
	{
		APhoenixRod *flamethrower = static_cast<APhoenixRod *> (self->player->ReadyWeapon);
		if (flamethrower != NULL)
		{
			flamethrower->FlameCount = FLAME_THROWER_TICS;
		}
	}
	return 0;
}

//----------------------------------------------------------------------------
//
// PROC A_FirePhoenixPL2
//
// Flame thrower effect.
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_FirePhoenixPL2)
{
	PARAM_ACTION_PROLOGUE(AActor);

	AActor *mo;

	double slope;
	FSoundID soundid;
	player_t *player;
	APhoenixRod *flamethrower;

	if (nullptr == (player = self->player))
	{
		return 0;
	}

	soundid = "weapons/phoenixpowshoot";

	flamethrower = static_cast<APhoenixRod *> (player->ReadyWeapon);
	if (flamethrower == nullptr || --flamethrower->FlameCount == 0)
	{ // Out of flame
		P_SetPsprite(player, PSP_WEAPON, flamethrower->FindState("Powerdown"));
		player->refire = 0;
		S_StopSound (self, CHAN_WEAPON);
		return 0;
	}

	slope = -self->Angles.Pitch.TanClamped();
	double xo = pr_fp2.Random2() / 128.;
	double yo = pr_fp2.Random2() / 128.;
	DVector3 pos = self->Vec3Offset(xo, yo, 26 + slope - self->Floorclip);

	slope += 0.1;
	mo = Spawn("PhoenixFX2", pos, ALLOW_REPLACE);
	mo->target = self;
	mo->Angles.Yaw = self->Angles.Yaw;
	mo->VelFromAngle();
	mo->Vel += self->Vel.XY();
	mo->Vel.Z = mo->Speed * slope;
	if (!player->refire || !S_IsActorPlayingSomething (self, CHAN_WEAPON, -1))
	{
		S_Sound (self, CHAN_WEAPON|CHAN_LOOP, soundid, 1, ATTN_NORM);
	}	
	P_CheckMissileSpawn (mo, self->radius);
	return 0;
}

//----------------------------------------------------------------------------
//
// PROC A_ShutdownPhoenixPL2
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_ShutdownPhoenixPL2)
{
	PARAM_ACTION_PROLOGUE(AActor);

	player_t *player;

	if (NULL == (player = self->player))
	{
		return 0;
	}
	S_StopSound (self, CHAN_WEAPON);
	AWeapon *weapon = player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return 0;
	}
	return 0;
}

//----------------------------------------------------------------------------
//
// PROC A_FlameEnd
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_FlameEnd)
{
	PARAM_SELF_PROLOGUE(AActor);

	self->Vel.Z += 1.5;
	return 0;
}

//----------------------------------------------------------------------------
//
// PROC A_FloatPuff
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_FloatPuff)
{
	PARAM_SELF_PROLOGUE(AActor);

	self->Vel.Z += 1.8;
	return 0;
}

