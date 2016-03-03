/*
#include "a_pickups.h"
#include "p_local.h"
#include "m_random.h"
#include "a_strifeglobal.h"
#include "s_sound.h"
#include "p_enemy.h"
#include "templates.h"
#include "thingdef/thingdef.h"
#include "doomstat.h"
*/

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
	FState *painstate = emitter->FindState(NAME_Pain, NAME_Dagger);
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

DEFINE_ACTION_FUNCTION(AActor, A_JabDagger)
{
	PARAM_ACTION_PROLOGUE;

	angle_t 	angle;
	int 		damage;
	int 		pitch;
	int			power;
	FTranslatedLineTarget t;

	power = MIN(10, self->player->mo->stamina / 10);
	damage = (pr_jabdagger() % (power + 8)) * (power + 2);

	if (self->FindInventory<APowerStrength>())
	{
		damage *= 10;
	}

	angle = self->angle + (pr_jabdagger.Random2() << 18);
	pitch = P_AimLineAttack (self, angle, 80*FRACUNIT);
	P_LineAttack (self, angle, 80*FRACUNIT, pitch, damage, NAME_Melee, "StrifeSpark", true, &t);

	// turn to face target
	if (t.linetarget)
	{
		S_Sound (self, CHAN_WEAPON,
			t.linetarget->flags & MF_NOBLOOD ? "misc/metalhit" : "misc/meathit",
			1, ATTN_NORM);
		self->angle = t.angleFromSource;
		self->flags |= MF_JUSTATTACKED;
		P_DaggerAlert (self, t.linetarget);
	}
	else
	{
		S_Sound (self, CHAN_WEAPON, "misc/swish", 1, ATTN_NORM);
	}
	return 0;
}

//============================================================================
//
// A_AlertMonsters
//
//============================================================================

enum
{
	AMF_TARGETEMITTER = 1,
	AMF_TARGETNONPLAYER = 2,
	AMF_EMITFROMTARGET = 4,
};

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_AlertMonsters)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_FIXED_OPT(maxdist) { maxdist = 0; }
	PARAM_INT_OPT(Flags) { Flags = 0; }

	AActor * target = NULL;
	AActor * emitter = self;

	if (self->player != NULL || (Flags & AMF_TARGETEMITTER))
	{
		target = self;
	}
	else if (self->target != NULL && (Flags & AMF_TARGETNONPLAYER))
	{
		target = self->target;
	}
	else if (self->target != NULL && self->target->player != NULL)
	{
		target = self->target;
	}

	if (Flags & AMF_EMITFROMTARGET) emitter = target;

	if (target != NULL && emitter != NULL)
	{
		P_NoiseAlert(target, emitter, false, maxdist);
	}
	return 0;
}

// Poison Bolt --------------------------------------------------------------

class APoisonBolt : public AActor
{
	DECLARE_CLASS (APoisonBolt, AActor)
public:
	int DoSpecialDamage (AActor *target, int damage, FName damagetype);
};

IMPLEMENT_CLASS (APoisonBolt)

int APoisonBolt::DoSpecialDamage (AActor *target, int damage, FName damagetype)
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

DEFINE_ACTION_FUNCTION(AActor, A_ClearFlash)
{
	PARAM_ACTION_PROLOGUE;

	player_t *player = self->player;

	if (player == NULL)
		return 0;

	P_SetPsprite (player, ps_flash, NULL);
	return 0;
}

//============================================================================
//
// A_ShowElectricFlash
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_ShowElectricFlash)
{
	PARAM_ACTION_PROLOGUE;

	if (self->player != NULL)
	{
		P_SetPsprite (self->player, ps_flash, self->player->ReadyWeapon->FindState(NAME_Flash));
	}
	return 0;
}

//============================================================================
//
// A_FireElectric
//
//============================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_FireArrow)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_CLASS(ti, AActor);

	angle_t savedangle;

	if (self->player == NULL)
		return 0;

	AWeapon *weapon = self->player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return 0;
	}

	if (ti) 
	{
		savedangle = self->angle;
		self->angle += pr_electric.Random2 () << (18 - self->player->mo->accuracy * 5 / 100);
		self->player->mo->PlayAttacking2 ();
		P_SpawnPlayerMissile (self, ti);
		self->angle = savedangle;
		S_Sound (self, CHAN_WEAPON, "weapons/xbowshoot", 1, ATTN_NORM);
	}
	return 0;
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
		angle += pr_sgunshot.Random2() << (20 - mo->player->mo->accuracy * 5 / 100);
	}

	P_LineAttack (mo, angle, PLAYERMISSILERANGE, pitch, damage, NAME_Hitscan, NAME_StrifePuff);
}

//============================================================================
//
// A_FireAssaultGun
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_FireAssaultGun)
{
	PARAM_ACTION_PROLOGUE;

	bool accurate;

	S_Sound (self, CHAN_WEAPON, "weapons/assaultgun", 1, ATTN_NORM);

	if (self->player != NULL)
	{
		AWeapon *weapon = self->player->ReadyWeapon;
		if (weapon != NULL)
		{
			if (!weapon->DepleteAmmo (weapon->bAltFire))
				return 0;
		}
		self->player->mo->PlayAttacking2 ();
		accurate = !self->player->refire;
	}
	else
	{
		accurate = true;
	}

	P_StrifeGunShot (self, accurate, P_BulletSlope (self));
	return 0;
}

// Mini-Missile Launcher ----------------------------------------------------

//============================================================================
//
// A_FireMiniMissile
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_FireMiniMissile)
{
	PARAM_ACTION_PROLOGUE;

	player_t *player = self->player;
	angle_t savedangle;

	if (self->player == NULL)
		return 0;

	AWeapon *weapon = self->player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return 0;
	}

	savedangle = self->angle;
	self->angle += pr_minimissile.Random2() << (19 - player->mo->accuracy * 5 / 100);
	player->mo->PlayAttacking2 ();
	P_SpawnPlayerMissile (self, PClass::FindActor("MiniMissile"));
	self->angle = savedangle;
	return 0;
}

//============================================================================
//
// A_RocketInFlight
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_RocketInFlight)
{
	PARAM_ACTION_PROLOGUE;

	AActor *trail;

	S_Sound (self, CHAN_VOICE, "misc/missileinflight", 1, ATTN_NORM);
	P_SpawnPuff (self, PClass::FindActor("MiniMissilePuff"), self->Pos(), self->angle - ANGLE_180, 2, PF_HITTHING);
	trail = Spawn("RocketTrail", self->Vec3Offset(-self->velx, -self->vely, 0), ALLOW_REPLACE);
	if (trail != NULL)
	{
		trail->velz = FRACUNIT;
	}
	return 0;
}

// Flame Thrower ------------------------------------------------------------

//============================================================================
//
// A_FlameDie
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_FlameDie)
{
	PARAM_ACTION_PROLOGUE;

	self->flags |= MF_NOGRAVITY;
	self->velz = (pr_flamedie() & 3) << FRACBITS;
	return 0;
}

//============================================================================
//
// A_FireFlamer
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_FireFlamer)
{
	PARAM_ACTION_PROLOGUE;

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

	self->angle += pr_flamethrower.Random2() << 18;
	self = P_SpawnPlayerMissile (self, PClass::FindActor("FlameMissile"));
	if (self != NULL)
	{
		self->velz += 5*FRACUNIT;
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
	PARAM_ACTION_PROLOGUE;

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
	PARAM_ACTION_PROLOGUE;

	S_Sound (self, CHAN_WEAPON, "weapons/mauler2charge", 1, ATTN_NORM);

	if (self->player != NULL)
	{
		self->player->psprites[ps_weapon].sx += pr_mauler2.Random2() << 10;
		self->player->psprites[ps_weapon].sy += pr_mauler2.Random2() << 10;
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
	PARAM_ACTION_PROLOGUE;

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
	P_ThrustMobj (self, self->angle + ANGLE_180, 0x7D000);
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
	PARAM_ACTION_PROLOGUE;

	AActor *wavedef = GetDefaultByName("MaulerTorpedoWave");
	fixed_t savedz;
	self->angle += ANGLE_180;

	// If the torpedo hit the ceiling, it should still spawn the wave
	savedz = self->Z();
	if (wavedef && self->ceilingz - self->Z() < wavedef->height)
	{
		self->SetZ(self->ceilingz - wavedef->height);
	}

	for (int i = 0; i < 80; ++i)
	{
		self->angle += ANGLE_45/10;
		P_SpawnSubMissile (self, PClass::FindActor("MaulerTorpedoWave"), self->target);
	}
	self->SetZ(savedz);
	return 0;
}

AActor *P_SpawnSubMissile (AActor *source, PClassActor *type, AActor *target)
{
	AActor *other = Spawn (type, source->Pos(), ALLOW_REPLACE);

	if (other == NULL)
	{
		return NULL;
	}

	other->target = target;
	other->angle = source->angle;

	other->velx = FixedMul (other->Speed, finecosine[source->angle >> ANGLETOFINESHIFT]);
	other->vely = FixedMul (other->Speed, finesine[source->angle >> ANGLETOFINESHIFT]);

	if (other->flags4 & MF4_SPECTRAL)
	{
		if (source->flags & MF_MISSILE && source->flags4 & MF4_SPECTRAL)
		{
			other->FriendPlayer = source->FriendPlayer;
		}
		else
		{
			other->SetFriendPlayer(target->player);
		}
	}

	if (P_CheckMissileSpawn (other, source->radius))
	{
		angle_t pitch = P_AimLineAttack (source, source->angle, 1024*FRACUNIT);
		other->velz = FixedMul (-finesine[pitch>>ANGLETOFINESHIFT], other->Speed);
		return other;
	}
	return NULL;
}

class APhosphorousFire : public AActor
{
	DECLARE_CLASS (APhosphorousFire, AActor)
public:
	int DoSpecialDamage (AActor *target, int damage, FName damagetype);
};

IMPLEMENT_CLASS (APhosphorousFire)

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
	PARAM_ACTION_PROLOGUE;

	P_RadiusAttack (self, self->target, 128, 128, self->DamageType, RADF_HURTSOURCE);
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_Burnination)
{
	PARAM_ACTION_PROLOGUE;

	self->velz -= 8*FRACUNIT;
	self->velx += (pr_phburn.Random2 (3)) << FRACBITS;
	self->vely += (pr_phburn.Random2 (3)) << FRACBITS;
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

		fixedvec2 pos = self->Vec2Offset(xofs << FRACBITS, yofs << FRACBITS);
		sector_t * sector = P_PointInSector(pos.x, pos.y);

		// The sector's floor is too high so spawn the flame elsewhere.
		if (sector->floorplane.ZatPoint(pos.x, pos.y) > self->Z() + self->MaxStepHeight)
		{
			pos.x = self->X();
			pos.y = self->Y();
		}

		AActor *drop = Spawn<APhosphorousFire> (
			pos.x, pos.y,
			self->Z() + 4*FRACUNIT, ALLOW_REPLACE);
		if (drop != NULL)
		{
			drop->velx = self->velx + ((pr_phburn.Random2 (7)) << FRACBITS);
			drop->vely = self->vely + ((pr_phburn.Random2 (7)) << FRACBITS);
			drop->velz = self->velz - FRACUNIT;
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

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_FireGrenade)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_CLASS(grenadetype, AActor);
	PARAM_ANGLE(angleofs);
	PARAM_STATE(flash)

	player_t *player = self->player;
	AActor *grenade;
	angle_t an;
	fixed_t tworadii;
	AWeapon *weapon;

	if (player == NULL || grenadetype == NULL)
		return 0;

	if ((weapon = player->ReadyWeapon) == NULL)
		return 0;

	if (!weapon->DepleteAmmo (weapon->bAltFire))
		return 0;

	P_SetPsprite (player, ps_flash, flash);

	if (grenadetype != NULL)
	{
		self->AddZ(32*FRACUNIT);
		grenade = P_SpawnSubMissile (self, grenadetype, self);
		self->AddZ(-32*FRACUNIT);
		if (grenade == NULL)
			return 0;

		if (grenade->SeeSound != 0)
		{
			S_Sound (grenade, CHAN_VOICE, grenade->SeeSound, 1, ATTN_NORM);
		}

		grenade->velz = FixedMul (finetangent[FINEANGLES/4-(self->pitch>>ANGLETOFINESHIFT)], grenade->Speed) + 8*FRACUNIT;

		fixedvec2 offset;

		an = self->angle >> ANGLETOFINESHIFT;
		tworadii = self->radius + grenade->radius;
		offset.x = FixedMul (finecosine[an], tworadii);
		offset.y = FixedMul (finesine[an], tworadii);

		an = self->angle + angleofs;
		an >>= ANGLETOFINESHIFT;
		offset.x += FixedMul (finecosine[an], 15*FRACUNIT);
		offset.y += FixedMul (finesine[an], 15*FRACUNIT);

		fixedvec2 newpos = grenade->Vec2Offset(offset.x, offset.y);
		grenade->SetOrigin(newpos.x, newpos.y, grenade->Z(), false);
	}
	return 0;
}

// The Almighty Sigil! ------------------------------------------------------

IMPLEMENT_CLASS(ASigil)

//============================================================================
//
// ASigil :: Serialize
//
//============================================================================

void ASigil::BeginPlay()
{
	NumPieces = health;
}

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

DEFINE_ACTION_FUNCTION(AActor, A_SelectPiece)
{
	PARAM_ACTION_PROLOGUE;

	int pieces = MIN (static_cast<ASigil*>(self)->NumPieces, 5);

	if (pieces > 1)
	{
		self->SetState (self->FindState("Spawn")+pieces);
	}
	return 0;
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

DEFINE_ACTION_FUNCTION(AActor, A_SelectSigilView)
{
	PARAM_ACTION_PROLOGUE;

	int pieces;

	if (self->player == NULL)
	{
		return 0;
	}
	pieces = static_cast<ASigil*>(self->player->ReadyWeapon)->NumPieces;
	P_SetPsprite (self->player, ps_weapon,
		self->player->psprites[ps_weapon].state + pieces);
	return 0;
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

DEFINE_ACTION_FUNCTION(AActor, A_SelectSigilDown)
{
	PARAM_ACTION_PROLOGUE;

	int pieces;

	if (self->player == NULL)
	{
		return 0;
	}
	pieces = static_cast<ASigil*>(self->player->ReadyWeapon)->DownPieces;
	static_cast<ASigil*>(self->player->ReadyWeapon)->DownPieces = 0;
	if (pieces == 0)
	{
		pieces = static_cast<ASigil*>(self->player->ReadyWeapon)->NumPieces;
	}
	P_SetPsprite (self->player, ps_weapon,
		self->player->psprites[ps_weapon].state + pieces);
	return 0;
}

//============================================================================
//
// A_SelectSigilAttack
//
// Same as A_SelectSigilView, but used just before attacking.
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SelectSigilAttack)
{
	PARAM_ACTION_PROLOGUE;

	int pieces;

	if (self->player == NULL)
	{
		return 0;
	}
	pieces = static_cast<ASigil*>(self->player->ReadyWeapon)->NumPieces;
	P_SetPsprite (self->player, ps_weapon,
		self->player->psprites[ps_weapon].state + 4*pieces - 3);
	return 0;
}

//============================================================================
//
// A_SigilCharge
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SigilCharge)
{
	PARAM_ACTION_PROLOGUE;

	S_Sound (self, CHAN_WEAPON, "weapons/sigilcharge", 1, ATTN_NORM);
	if (self->player != NULL)
	{
		self->player->extralight = 2;
	}
	return 0;
}

//============================================================================
//
// A_LightInverse
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_LightInverse)
{
	PARAM_ACTION_PROLOGUE;

	if (self->player != NULL)
	{
		self->player->extralight = INT_MIN;
	}
	return 0;
}

//============================================================================
//
// A_FireSigil1
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_FireSigil1)
{
	PARAM_ACTION_PROLOGUE;

	AActor *spot;
	player_t *player = self->player;
	FTranslatedLineTarget t;

	if (player == NULL || player->ReadyWeapon == NULL)
		return 0;

	P_DamageMobj (self, self, NULL, 1*4, 0, DMG_NO_ARMOR);
	S_Sound (self, CHAN_WEAPON, "weapons/sigilcharge", 1, ATTN_NORM);

	P_BulletSlope (self, &t, ALF_PORTALRESTRICT);
	if (t.linetarget != NULL)
	{
		spot = Spawn("SpectralLightningSpot", t.linetarget->X(), t.linetarget->Y(), t.linetarget->floorz, ALLOW_REPLACE);
		if (spot != NULL)
		{
			spot->tracer = t.linetarget;
		}
	}
	else
	{
		spot = Spawn("SpectralLightningSpot", self->Pos(), ALLOW_REPLACE);
		if (spot != NULL)
		{
			spot->velx += 28 * finecosine[self->angle >> ANGLETOFINESHIFT];
			spot->vely += 28 * finesine[self->angle >> ANGLETOFINESHIFT];
		}
	}
	if (spot != NULL)
	{
		spot->SetFriendPlayer(player);
		spot->target = self;
	}
	return 0;
}

//============================================================================
//
// A_FireSigil2
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_FireSigil2)
{
	PARAM_ACTION_PROLOGUE;

	player_t *player = self->player;

	if (player == NULL || player->ReadyWeapon == NULL)
		return 0;

	P_DamageMobj (self, self, NULL, 2*4, 0, DMG_NO_ARMOR);
	S_Sound (self, CHAN_WEAPON, "weapons/sigilcharge", 1, ATTN_NORM);

	P_SpawnPlayerMissile (self, PClass::FindActor("SpectralLightningH1"));
	return 0;
}

//============================================================================
//
// A_FireSigil3
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_FireSigil3)
{
	PARAM_ACTION_PROLOGUE;

	AActor *spot;
	player_t *player = self->player;
	int i;

	if (player == NULL || player->ReadyWeapon == NULL)
		return 0;

	P_DamageMobj (self, self, NULL, 3*4, 0, DMG_NO_ARMOR);
	S_Sound (self, CHAN_WEAPON, "weapons/sigilcharge", 1, ATTN_NORM);

	self->angle -= ANGLE_90;
	for (i = 0; i < 20; ++i)
	{
		self->angle += ANGLE_180/20;
		spot = P_SpawnSubMissile (self, PClass::FindActor("SpectralLightningBall1"), self);
		if (spot != NULL)
		{
			spot->SetZ(self->Z() + 32*FRACUNIT);
		}
	}
	self->angle -= (ANGLE_180/20)*10;
	return 0;
}

//============================================================================
//
// A_FireSigil4
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_FireSigil4)
{
	PARAM_ACTION_PROLOGUE;

	AActor *spot;
	player_t *player = self->player;
	FTranslatedLineTarget t;

	if (player == NULL || player->ReadyWeapon == NULL)
		return 0;

	P_DamageMobj (self, self, NULL, 4*4, 0, DMG_NO_ARMOR);
	S_Sound (self, CHAN_WEAPON, "weapons/sigilcharge", 1, ATTN_NORM);

	P_BulletSlope (self, &t, ALF_PORTALRESTRICT);
	if (t.linetarget != NULL)
	{
		spot = P_SpawnPlayerMissile (self, 0,0,0, PClass::FindActor("SpectralLightningBigV1"), self->angle, &t, NULL, false, false, ALF_PORTALRESTRICT);
		if (spot != NULL)
		{
			spot->tracer = t.linetarget;
		}
	}
	else
	{
		spot = P_SpawnPlayerMissile (self, PClass::FindActor("SpectralLightningBigV1"));
		if (spot != NULL)
		{
			spot->velx += FixedMul (spot->Speed, finecosine[self->angle >> ANGLETOFINESHIFT]);
			spot->vely += FixedMul (spot->Speed, finesine[self->angle >> ANGLETOFINESHIFT]);
		}
	}
	return 0;
}

//============================================================================
//
// A_FireSigil5
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_FireSigil5)
{
	PARAM_ACTION_PROLOGUE;

	player_t *player = self->player;

	if (player == NULL || player->ReadyWeapon == NULL)
		return 0;

	P_DamageMobj (self, self, NULL, 5*4, 0, DMG_NO_ARMOR);
	S_Sound (self, CHAN_WEAPON, "weapons/sigilcharge", 1, ATTN_NORM);

	P_SpawnPlayerMissile (self, PClass::FindActor("SpectralLightningBigBall1"));
	return 0;
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
		sigil = static_cast<ASigil*>(Spawn("Sigil1", 0,0,0, NO_REPLACE));
		if (!sigil->CallTryPickup (receiver))
		{
			sigil->Destroy ();
		}
		return 0;
	}
	else if (sigil->NumPieces < 5)
	{
		++sigil->NumPieces;
		static const char* sigils[5] =
		{
			"Sigil1", "Sigil2", "Sigil3", "Sigil4", "Sigil5"
		};
		sigil->Icon = ((AInventory*)GetDefaultByName (sigils[MAX(0,sigil->NumPieces-1)]))->Icon;
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
