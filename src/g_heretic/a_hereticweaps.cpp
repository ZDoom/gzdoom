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


// --- Skull rod ------------------------------------------------------------


// Rain pillar 1 ------------------------------------------------------------

class ARainPillar : public AActor
{
	DECLARE_CLASS (ARainPillar, AActor)
public:
	int DoSpecialDamage (AActor *target, int damage, FName damagetype);
};

IMPLEMENT_CLASS(ARainPillar, false, false)

int ARainPillar::DoSpecialDamage (AActor *target, int damage, FName damagetype)
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
	DECLARE_CLASS (ARainTracker, AInventory)
public:
	
	void Serialize(FSerializer &arc);
	TObjPtr<AActor> Rain1, Rain2;
};

IMPLEMENT_CLASS(ARainTracker, false, false)
	
void ARainTracker::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	arc("rain1", Rain1)
		("rain2", Rain2);
}

//----------------------------------------------------------------------------
//
// PROC A_FireSkullRodPL1
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_FireSkullRodPL1)
{
	PARAM_ACTION_PROLOGUE(AActor);

	AActor *mo;
	player_t *player;

	if (NULL == (player = self->player))
	{
		return 0;
	}

	AWeapon *weapon = player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return 0;
	}
	mo = P_SpawnPlayerMissile (self, PClass::FindActor("HornRodFX1"));
	// Randomize the first frame
	if (mo && pr_fsr1() > 128)
	{
		mo->SetState (mo->state->GetNextState());
	}
	return 0;
}

//----------------------------------------------------------------------------
//
// PROC A_FireSkullRodPL2
//
// The special2 field holds the player number that shot the rain missile.
// The special1 field holds the id of the rain sound.
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_FireSkullRodPL2)
{
	PARAM_ACTION_PROLOGUE(AActor);

	player_t *player;
	AActor *MissileActor;
	FTranslatedLineTarget t;

	if (NULL == (player = self->player))
	{
		return 0;
	}
	AWeapon *weapon = player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return 0;
	}
	P_SpawnPlayerMissile (self, 0,0,0, PClass::FindActor("HornRodFX2"), self->Angles.Yaw, &t, &MissileActor);
	// Use MissileActor instead of the return value from
	// P_SpawnPlayerMissile because we need to give info to the mobj
	// even if it exploded immediately.
	if (MissileActor != NULL)
	{
		MissileActor->special2 = (int)(player - players);
		if (t.linetarget && !t.unlinked)
		{
			MissileActor->tracer = t.linetarget;
		}
		S_Sound (MissileActor, CHAN_WEAPON, "weapons/hornrodpowshoot", 1, ATTN_NORM);
	}
	return 0;
}

//----------------------------------------------------------------------------
//
// PROC A_AddPlayerRain
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_AddPlayerRain)
{
	PARAM_SELF_PROLOGUE(AActor);

	ARainTracker *tracker;

	if (self->target == NULL || self->target->health <= 0)
	{ // Shooter is dead or nonexistant
		return 0;
	}

	tracker = self->target->FindInventory<ARainTracker> ();

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
		tracker = static_cast<ARainTracker *> (self->target->GiveInventoryType (RUNTIME_CLASS(ARainTracker)));
	}
	// Add rain mobj to list
	if (tracker->Rain1)
	{
		tracker->Rain2 = self;
	}
	else
	{
		tracker->Rain1 = self;
	}
	self->special1 = S_FindSound ("misc/rain");
	return 0;
}

//----------------------------------------------------------------------------
//
// PROC A_SkullRodStorm
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_SkullRodStorm)
{
	PARAM_SELF_PROLOGUE(AActor);

	AActor *mo;
	ARainTracker *tracker;

	if (self->health-- == 0)
	{
		S_StopSound (self, CHAN_BODY);
		if (self->target == NULL)
		{ // Player left the game
			self->Destroy ();
			return 0;
		}
		tracker = self->target->FindInventory<ARainTracker> ();
		if (tracker != NULL)
		{
			if (tracker->Rain1 == self)
			{
				tracker->Rain1 = NULL;
			}
			else if (tracker->Rain2 == self)
			{
				tracker->Rain2 = NULL;
			}
		}
		self->Destroy ();
		return 0;
	}
	if (pr_storm() < 25)
	{ // Fudge rain frequency
		return 0;
	}
	double xo = ((pr_storm() & 127) - 64);
	double yo = ((pr_storm() & 127) - 64);
	DVector3 pos = self->Vec2OffsetZ(xo, yo, ONCEILINGZ);
	mo = Spawn<ARainPillar> (pos, ALLOW_REPLACE);
	// We used bouncecount to store the 3D floor index in A_HideInCeiling
	if (!mo) return 0;
	if (mo->Sector->PortalGroup != self->Sector->PortalGroup)
	{
		// spawning this through a portal will never work right so abort right away.
		mo->Destroy();
		return 0;
	}
	if (self->bouncecount >= 0 && (unsigned)self->bouncecount < self->Sector->e->XFloor.ffloors.Size())
		pos.Z = self->Sector->e->XFloor.ffloors[self->bouncecount]->bottom.plane->ZatPoint(mo);
	else
		pos.Z = self->Sector->ceilingplane.ZatPoint(mo);
	int moceiling = P_Find3DFloor(NULL, pos, false, false, pos.Z);
	if (moceiling >= 0) mo->SetZ(pos.Z - mo->Height);
	mo->Translation = multiplayer ?	TRANSLATION(TRANSLATION_RainPillar,self->special2) : 0;
	mo->target = self->target;
	mo->Vel.X = MinVel; // Force collision detection
	mo->Vel.Z = -mo->Speed;
	mo->special2 = self->special2; // Transfer player number
	P_CheckMissileSpawn (mo, self->radius);
	if (self->special1 != -1 && !S_IsActorPlayingSomething (self, CHAN_BODY, -1))
	{
		S_Sound (self, CHAN_BODY|CHAN_LOOP, self->special1, 1, ATTN_NORM);
	}
	return 0;
}

//----------------------------------------------------------------------------
//
// PROC A_RainImpact
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_RainImpact)
{
	PARAM_SELF_PROLOGUE(AActor);
	if (self->Z() > self->floorz)
	{
		self->SetState (self->FindState("NotFloor"));
	}
	else if (pr_impact() < 40)
	{
		P_HitFloor (self);
	}
	return 0;
}

//----------------------------------------------------------------------------
//
// PROC A_HideInCeiling
//
//----------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_HideInCeiling)
{
	PARAM_SELF_PROLOGUE(AActor);

	// We use bouncecount to store the 3D floor index
	double foo;
	for (int i = self->Sector->e->XFloor.ffloors.Size() - 1; i >= 0; i--)
	{
		F3DFloor * rover = self->Sector->e->XFloor.ffloors[i];
		if (!(rover->flags & FF_SOLID) || !(rover->flags & FF_EXISTS)) continue;

		if ((foo = rover->bottom.plane->ZatPoint(self)) >= self->Top())
		{
			self->SetZ(foo + 4, false);
			self->bouncecount = i;
			return 0;
		}
	}
	self->bouncecount = -1;
	self->SetZ(self->ceilingz + 4, false);
	return 0;
}

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

