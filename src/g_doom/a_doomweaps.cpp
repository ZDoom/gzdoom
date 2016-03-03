/*
#include "actor.h"
#include "info.h"
#include "s_sound.h"
#include "m_random.h"
#include "a_pickups.h"
#include "d_player.h"
#include "p_pspr.h"
#include "p_local.h"
#include "gstrings.h"
#include "p_effect.h"
#include "gi.h"
#include "templates.h"
#include "thingdef/thingdef.h"
#include "doomstat.h"
*/

static FRandom pr_punch ("Punch");
static FRandom pr_saw ("Saw");
static FRandom pr_fireshotgun2 ("FireSG2");
static FRandom pr_fireplasma ("FirePlasma");
static FRandom pr_firerail ("FireRail");
static FRandom pr_bfgspray ("BFGSpray");
static FRandom pr_oldbfg ("OldBFG");

//
// A_Punch
//
DEFINE_ACTION_FUNCTION(AActor, A_Punch)
{
	PARAM_ACTION_PROLOGUE;

	angle_t 	angle;
	int 		damage;
	int 		pitch;
	FTranslatedLineTarget t;

	if (self->player != NULL)
	{
		AWeapon *weapon = self->player->ReadyWeapon;
		if (weapon != NULL && !(weapon->WeaponFlags & WIF_DEHAMMO) && ACTION_CALL_FROM_WEAPON())
		{
			if (!weapon->DepleteAmmo (weapon->bAltFire))
				return 0;
		}
	}

	damage = (pr_punch()%10+1)<<1;

	if (self->FindInventory<APowerStrength>())	
		damage *= 10;

	angle = self->angle;

	angle += pr_punch.Random2() << 18;
	pitch = P_AimLineAttack (self, angle, MELEERANGE);

	P_LineAttack (self, angle, MELEERANGE, pitch, damage, NAME_Melee, NAME_BulletPuff, LAF_ISMELEEATTACK, &t);

	// turn to face target
	if (t.linetarget)
	{
		S_Sound (self, CHAN_WEAPON, "*fist", 1, ATTN_NORM);
		self->angle = t.angleFromSource;
	}
	return 0;
}

//
// A_FirePistol
//
DEFINE_ACTION_FUNCTION(AActor, A_FirePistol)
{
	PARAM_ACTION_PROLOGUE;

	bool accurate;

	if (self->player != NULL)
	{
		AWeapon *weapon = self->player->ReadyWeapon;
		if (weapon != NULL && ACTION_CALL_FROM_WEAPON())
		{
			if (!weapon->DepleteAmmo (weapon->bAltFire, true, 1))
				return 0;

			P_SetPsprite (self->player, ps_flash, weapon->FindState(NAME_Flash));
		}
		self->player->mo->PlayAttacking2 ();

		accurate = !self->player->refire;
	}
	else
	{
		accurate = true;
	}

	S_Sound (self, CHAN_WEAPON, "weapons/pistol", 1, ATTN_NORM);

	P_GunShot (self, accurate, PClass::FindActor(NAME_BulletPuff), P_BulletSlope (self));
	return 0;
}

//
// A_Saw
//
enum SAW_Flags
{
	SF_NORANDOM = 1,
	SF_RANDOMLIGHTMISS = 2,
	SF_RANDOMLIGHTHIT = 4,
	SF_NOUSEAMMOMISS = 8,
	SF_NOUSEAMMO = 16,
	SF_NOPULLIN = 32,
	SF_NOTURN = 64,
	SF_STEALARMOR = 128,
};

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_Saw)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_SOUND_OPT	(fullsound)			{ fullsound = "weapons/sawfull"; }
	PARAM_SOUND_OPT	(hitsound)			{ hitsound = "weapons/sawhit"; }
	PARAM_INT_OPT	(damage)			{ damage = 2; }
	PARAM_CLASS_OPT	(pufftype, AActor)	{ pufftype = NULL; }
	PARAM_INT_OPT	(flags)				{ flags = 0; }
	PARAM_FIXED_OPT	(range)				{ range = 0; }
	PARAM_ANGLE_OPT(spread_xy)			{ spread_xy = 33554432; /*angle_t(2.8125 * (ANGLE_90 / 90.0));*/ } // The floating point expression does not get optimized away.
	PARAM_ANGLE_OPT	(spread_z)			{ spread_z = 0; }
	PARAM_FIXED_OPT	(lifesteal)			{ lifesteal = 0; }
	PARAM_INT_OPT	(lifestealmax)		{ lifestealmax = 0; }
	PARAM_CLASS_OPT	(armorbonustype, ABasicArmorBonus)	{ armorbonustype = NULL; }

	angle_t angle;
	angle_t slope;
	player_t *player;
	FTranslatedLineTarget t;
	int actualdamage;

	if (NULL == (player = self->player))
	{
		return 0;
	}

	if (pufftype == NULL)
	{
		pufftype = PClass::FindActor(NAME_BulletPuff);
	}
	if (damage == 0)
	{
		damage = 2;
	}
	if (!(flags & SF_NORANDOM))
	{
		damage *= (pr_saw()%10+1);
	}
	if (range == 0)
	{ // use meleerange + 1 so the puff doesn't skip the flash (i.e. plays all states)
		range = MELEERANGE+1;
	}

	angle = self->angle + (pr_saw.Random2() * (spread_xy / 255));
	slope = P_AimLineAttack (self, angle, range, &t) + (pr_saw.Random2() * (spread_z / 255));

	AWeapon *weapon = self->player->ReadyWeapon;
	if ((weapon != NULL) && !(flags & SF_NOUSEAMMO) && !(!t.linetarget && (flags & SF_NOUSEAMMOMISS)) && !(weapon->WeaponFlags & WIF_DEHAMMO) && ACTION_CALL_FROM_WEAPON())
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return 0;
	}

	P_LineAttack (self, angle, range, slope, damage, NAME_Melee, pufftype, false, &t, &actualdamage);

	if (!t.linetarget)
	{
		if ((flags & SF_RANDOMLIGHTMISS) && (pr_saw() > 64))
		{
			player->extralight = !player->extralight;
		}
		S_Sound (self, CHAN_WEAPON, fullsound, 1, ATTN_NORM);
		return 0;
	}

	if (flags & SF_RANDOMLIGHTHIT)
	{
		int randVal = pr_saw();
		if (randVal < 64)
		{
			player->extralight = 0;
		}
		else if (randVal < 160)
		{
			player->extralight = 1;
		}
		else
		{
			player->extralight = 2;
		}
	}

	if (lifesteal && !(t.linetarget->flags5 & MF5_DONTDRAIN))
	{
		if (flags & SF_STEALARMOR)
		{
			if (armorbonustype == NULL)
			{
				armorbonustype = dyn_cast<ABasicArmorBonus::MetaClass>(PClass::FindClass("ArmorBonus"));
			}
			if (armorbonustype != NULL)
			{
				assert(armorbonustype->IsDescendantOf (RUNTIME_CLASS(ABasicArmorBonus)));
				ABasicArmorBonus *armorbonus = static_cast<ABasicArmorBonus *>(Spawn(armorbonustype, 0,0,0, NO_REPLACE));
				armorbonus->SaveAmount *= (actualdamage * lifesteal) >> FRACBITS;
				armorbonus->MaxSaveAmount = lifestealmax <= 0 ? armorbonus->MaxSaveAmount : lifestealmax;
				armorbonus->flags |= MF_DROPPED;
				armorbonus->ClearCounters();

				if (!armorbonus->CallTryPickup (self))
				{
					armorbonus->Destroy ();
				}
			}
		}

		else
		{
			P_GiveBody (self, (actualdamage * lifesteal) >> FRACBITS, lifestealmax);
		}
	}

	S_Sound (self, CHAN_WEAPON, hitsound, 1, ATTN_NORM);
		
	// turn to face target
	if (!(flags & SF_NOTURN))
	{
		angle = t.angleFromSource;
		if (angle - self->angle > ANG180)
		{
			if (angle - self->angle < (angle_t)(-ANG90 / 20))
				self->angle = angle + ANG90 / 21;
			else
				self->angle -= ANG90 / 20;
		}
		else
		{
			if (angle - self->angle > ANG90 / 20)
				self->angle = angle - ANG90 / 21;
			else
				self->angle += ANG90 / 20;
		}
	}
	if (!(flags & SF_NOPULLIN))
		self->flags |= MF_JUSTATTACKED;
	return 0;
}

//
// A_FireShotgun
//
DEFINE_ACTION_FUNCTION(AActor, A_FireShotgun)
{
	PARAM_ACTION_PROLOGUE;

	int i;
	player_t *player;

	if (NULL == (player = self->player))
	{
		return 0;
	}

	S_Sound (self, CHAN_WEAPON,  "weapons/shotgf", 1, ATTN_NORM);
	AWeapon *weapon = self->player->ReadyWeapon;
	if (weapon != NULL && ACTION_CALL_FROM_WEAPON())
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire, true, 1))
			return 0;
		P_SetPsprite (player, ps_flash, weapon->FindState(NAME_Flash));
	}
	player->mo->PlayAttacking2 ();

	angle_t pitch = P_BulletSlope (self);

	for (i = 0; i < 7; i++)
	{
		P_GunShot (self, false, PClass::FindActor(NAME_BulletPuff), pitch);
	}
	return 0;
}

//
// A_FireShotgun2
//
DEFINE_ACTION_FUNCTION(AActor, A_FireShotgun2)
{
	PARAM_ACTION_PROLOGUE;

	int 		i;
	angle_t 	angle;
	int 		damage;
	player_t *player;

	if (NULL == (player = self->player))
	{
		return 0;
	}

	S_Sound (self, CHAN_WEAPON, "weapons/sshotf", 1, ATTN_NORM);
	AWeapon *weapon = self->player->ReadyWeapon;
	if (weapon != NULL && ACTION_CALL_FROM_WEAPON())
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire, true, 2))
			return 0;
		P_SetPsprite (player, ps_flash, weapon->FindState(NAME_Flash));
	}
	player->mo->PlayAttacking2 ();


	angle_t pitch = P_BulletSlope (self);
		
	for (i=0 ; i<20 ; i++)
	{
		damage = 5*(pr_fireshotgun2()%3+1);
		angle = self->angle;
		angle += pr_fireshotgun2.Random2() << 19;

		// Doom adjusts the bullet slope by shifting a random number [-255,255]
		// left 5 places. At 2048 units away, this means the vertical position
		// of the shot can deviate as much as 255 units from nominal. So using
		// some simple trigonometry, that means the vertical angle of the shot
		// can deviate by as many as ~7.097 degrees or ~84676099 BAMs.

		P_LineAttack (self,
					  angle,
					  PLAYERMISSILERANGE,
					  pitch + (pr_fireshotgun2.Random2() * 332063), damage,
					  NAME_Hitscan, NAME_BulletPuff);
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_OpenShotgun2)
{
	PARAM_ACTION_PROLOGUE;
	S_Sound (self, CHAN_WEAPON, "weapons/sshoto", 1, ATTN_NORM);
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_LoadShotgun2)
{
	PARAM_ACTION_PROLOGUE;
	S_Sound (self, CHAN_WEAPON, "weapons/sshotl", 1, ATTN_NORM);
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_CloseShotgun2)
{
	PARAM_ACTION_PROLOGUE;
	S_Sound (self, CHAN_WEAPON, "weapons/sshotc", 1, ATTN_NORM);
	A_ReFire (self);
	return 0;
}


//------------------------------------------------------------------------------------
//
// Setting a random flash like some of Doom's weapons can easily crash when the
// definition is overridden incorrectly so let's check that the state actually exists.
// Be aware though that this will not catch all DEHACKED related problems. But it will
// find all DECORATE related ones.
//
//------------------------------------------------------------------------------------

void P_SetSafeFlash(AWeapon *weapon, player_t *player, FState *flashstate, int index)
{

	PClassActor *cls = weapon->GetClass();
	while (cls != RUNTIME_CLASS(AWeapon))
	{
		if (flashstate >= cls->OwnedStates && flashstate < cls->OwnedStates + cls->NumOwnedStates)
		{
			// The flash state belongs to this class.
			// Now let's check if the actually wanted state does also
			if (flashstate + index < cls->OwnedStates + cls->NumOwnedStates)
			{
				// we're ok so set the state
				P_SetPsprite (player, ps_flash, flashstate + index);
				return;
			}
			else
			{
				// oh, no! The state is beyond the end of the state table so use the original flash state.
				P_SetPsprite (player, ps_flash, flashstate);
				return;
			}
		}
		// try again with parent class
		cls = static_cast<PClassActor *>(cls->ParentClass);
	}
	// if we get here the state doesn't seem to belong to any class in the inheritance chain
	// This can happen with Dehacked if the flash states are remapped. 
	// The only way to check this would be to go through all Dehacked modifiable actors, convert
	// their states into a single flat array and find the correct one.
	// Rather than that, just check to make sure it belongs to something.
	if (FState::StaticFindStateOwner(flashstate + index) == NULL)
	{ // Invalid state. With no index offset, it should at least be valid.
		index = 0;
	}
	P_SetPsprite (player, ps_flash, flashstate + index);
}

//
// A_FireCGun
//
DEFINE_ACTION_FUNCTION(AActor, A_FireCGun)
{
	PARAM_ACTION_PROLOGUE;

	player_t *player;

	if (self == NULL || NULL == (player = self->player))
	{
		return 0;
	}

	AWeapon *weapon = player->ReadyWeapon;
	if (weapon != NULL && ACTION_CALL_FROM_WEAPON())
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire, true, 1))
			return 0;
		
		S_Sound (self, CHAN_WEAPON, "weapons/chngun", 1, ATTN_NORM);

		FState *flash = weapon->FindState(NAME_Flash);
		if (flash != NULL)
		{
			// [RH] Fix for Sparky's messed-up Dehacked patch! Blargh!
			FState * atk = weapon->FindState(NAME_Fire);

			int theflash = clamp (int(player->psprites[ps_weapon].state - atk), 0, 1);

			if (flash[theflash].sprite != flash->sprite)
			{
				theflash = 0;
			}

			P_SetSafeFlash (weapon, player, flash, theflash);
		}

	}
	player->mo->PlayAttacking2 ();

	P_GunShot (self, !player->refire, PClass::FindActor(NAME_BulletPuff), P_BulletSlope (self));
	return 0;
}

//
// A_FireMissile
//
DEFINE_ACTION_FUNCTION(AActor, A_FireMissile)
{
	PARAM_ACTION_PROLOGUE;

	player_t *player;

	if (NULL == (player = self->player))
	{
		return 0;
	}
	AWeapon *weapon = self->player->ReadyWeapon;
	if (weapon != NULL && ACTION_CALL_FROM_WEAPON())
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire, true, 1))
			return 0;
	}
	P_SpawnPlayerMissile (self, PClass::FindActor("Rocket"));
	return 0;
}

//
// A_FireSTGrenade: not exactly backported from ST, but should work the same
//
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_FireSTGrenade)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_CLASS_OPT(grenade, AActor)	{ grenade = PClass::FindActor("Grenade"); }

	player_t *player;

	if (grenade == NULL)
		return 0;

	if (NULL == (player = self->player))
	{
		return 0;
	}
	AWeapon *weapon = self->player->ReadyWeapon;
	if (weapon != NULL && ACTION_CALL_FROM_WEAPON())
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return 0;
	}
		
	// Temporarily raise the pitch to send the grenade slightly upwards
	fixed_t SavedPlayerPitch = self->pitch;
	self->pitch -= (1152 << FRACBITS);
	P_SpawnPlayerMissile(self, grenade);
	self->pitch = SavedPlayerPitch;
	return 0;
}

//
// A_FirePlasma
//
DEFINE_ACTION_FUNCTION(AActor, A_FirePlasma)
{
	PARAM_ACTION_PROLOGUE;

	player_t *player;

	if (NULL == (player = self->player))
	{
		return 0;
	}
	AWeapon *weapon = self->player->ReadyWeapon;
	if (weapon != NULL && ACTION_CALL_FROM_WEAPON())
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire, true, 1))
			return 0;

		FState *flash = weapon->FindState(NAME_Flash);
		if (flash != NULL)
		{
			P_SetSafeFlash(weapon, player, flash, (pr_fireplasma()&1));
		}
	}

	P_SpawnPlayerMissile (self, PClass::FindActor("PlasmaBall"));
	return 0;
}

//
// [RH] A_FireRailgun
//
static void FireRailgun(AActor *self, int offset_xy, bool fromweapon)
{
	int damage;
	player_t *player;

	if (NULL == (player = self->player))
	{
		return;
	}

	AWeapon *weapon = self->player->ReadyWeapon;
	if (weapon != NULL && fromweapon)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire, true, 1))
			return;

		FState *flash = weapon->FindState(NAME_Flash);
		if (flash != NULL)
		{
			P_SetSafeFlash(weapon, player, flash, (pr_firerail()&1));
		}
	}

	damage = deathmatch ? 100 : 150;

	P_RailAttack (self, damage, offset_xy);
}


DEFINE_ACTION_FUNCTION(AActor, A_FireRailgun)
{
	PARAM_ACTION_PROLOGUE;
	FireRailgun(self, 0, ACTION_CALL_FROM_WEAPON());
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_FireRailgunRight)
{
	PARAM_ACTION_PROLOGUE;
	FireRailgun(self, 10, ACTION_CALL_FROM_WEAPON());
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_FireRailgunLeft)
{
	PARAM_ACTION_PROLOGUE;
	FireRailgun(self, -10, ACTION_CALL_FROM_WEAPON());
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_RailWait)
{
	// Okay, this was stupid. Just use a NULL function instead of this.
	return 0;
}

//
// A_FireBFG
//

DEFINE_ACTION_FUNCTION(AActor, A_FireBFG)
{
	PARAM_ACTION_PROLOGUE;

	player_t *player;

	if (NULL == (player = self->player))
	{
		return 0;
	}

	AWeapon *weapon = self->player->ReadyWeapon;
	if (weapon != NULL && ACTION_CALL_FROM_WEAPON())
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire, true, deh.BFGCells))
			return 0;
	}

	P_SpawnPlayerMissile (self,  0, 0, 0, PClass::FindActor("BFGBall"), self->angle, NULL, NULL, !!(dmflags2 & DF2_NO_FREEAIMBFG));
	return 0;
}


//
// A_BFGSpray
// Spawn a BFG explosion on every monster in view
//
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_BFGSpray)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_CLASS_OPT	(spraytype, AActor)		{ spraytype = NULL; }
	PARAM_INT_OPT	(numrays)				{ numrays = 40; }
	PARAM_INT_OPT	(damagecnt)				{ damagecnt = 15; }
	PARAM_ANGLE_OPT	(angle)					{ angle = ANGLE_90; }
	PARAM_FIXED_OPT	(distance)				{ distance = 16*64*FRACUNIT; }
	PARAM_ANGLE_OPT	(vrange)				{ vrange = 32*ANGLE_1; }
	PARAM_INT_OPT	(defdamage)				{ defdamage = 0; }

	int 				i;
	int 				j;
	int 				damage;
	angle_t 			an;
	FTranslatedLineTarget t;

	if (spraytype == NULL) spraytype = PClass::FindActor("BFGExtra");
	if (numrays <= 0) numrays = 40;
	if (damagecnt <= 0) damagecnt = 15;
	if (angle == 0) angle = ANG90;
	if (distance <= 0) distance = 16 * 64 * FRACUNIT;
	if (vrange == 0) vrange = ANGLE_1 * 32;

	// [RH] Don't crash if no target
	if (!self->target)
		return 0;

	// offset angles from its attack angle
	for (i = 0; i < numrays; i++)
	{
		an = self->angle - angle / 2 + angle / numrays*i;

		// self->target is the originator (player) of the missile
		P_AimLineAttack(self->target, an, distance, &t, vrange);

		if (t.linetarget != NULL)
		{
			AActor *spray = Spawn(spraytype, t.linetarget->PosPlusZ(t.linetarget->height >> 2), ALLOW_REPLACE);

			int dmgFlags = 0;
			FName dmgType = NAME_BFGSplash;

			if (spray != NULL)
			{
				if (spray->flags6 & MF6_MTHRUSPECIES && self->target->GetSpecies() == t.linetarget->GetSpecies())
				{
					spray->Destroy(); // [MC] Remove it because technically, the spray isn't trying to "hit" them.
					continue;
				}
				if (spray->flags5 & MF5_PUFFGETSOWNER) spray->target = self->target;
				if (spray->flags3 & MF3_FOILINVUL) dmgFlags |= DMG_FOILINVUL;
				if (spray->flags7 & MF7_FOILBUDDHA) dmgFlags |= DMG_FOILBUDDHA;
				dmgType = spray->DamageType;
			}

			if (defdamage == 0)
			{
				damage = 0;
				for (j = 0; j < damagecnt; ++j)
					damage += (pr_bfgspray() & 7) + 1;
			}
			else
			{
				// if this is used, damagecnt will be ignored
				damage = defdamage;
			}

			int newdam = P_DamageMobj(t.linetarget, self->target, self->target, damage, dmgType, dmgFlags|DMG_USEANGLE, t.angleFromSource);
			P_TraceBleed(newdam > 0 ? newdam : damage, &t, self);
		}
	}
	return 0;
}

//
// A_BFGsound
//
DEFINE_ACTION_FUNCTION(AActor, A_BFGsound)
{
	PARAM_ACTION_PROLOGUE;
	S_Sound (self, CHAN_WEAPON, "weapons/bfgf", 1, ATTN_NORM);
	return 0;
}

//
// A_FireOldBFG
//
// This function emulates Doom's Pre-Beta BFG
// By Lee Killough 6/6/98, 7/11/98, 7/19/98, 8/20/98
//
// This code may not be used in other mods without appropriate credit given.
// Code leeches will be telefragged.

DEFINE_ACTION_FUNCTION(AActor, A_FireOldBFG)
{
	PARAM_ACTION_PROLOGUE;
	PClassActor *plasma[] = { PClass::FindActor("PlasmaBall1"), PClass::FindActor("PlasmaBall2") };
	AActor * mo = NULL;

	player_t *player;
	bool doesautoaim = false;

	if (NULL == (player = self->player))
	{
		return 0;
	}

	AWeapon *weapon = self->player->ReadyWeapon;
	if (!ACTION_CALL_FROM_WEAPON()) weapon = NULL;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire, true, 1))
			return 0;

		doesautoaim = !(weapon->WeaponFlags & WIF_NOAUTOAIM);
		weapon->WeaponFlags |= WIF_NOAUTOAIM; // No autoaiming that gun
	}
	self->player->extralight = 2;

	// Save values temporarily
	angle_t SavedPlayerAngle = self->angle;
	fixed_t SavedPlayerPitch = self->pitch;
	for (int i = 0; i < 2; i++) // Spawn two plasma balls in sequence
    {
		self->angle += ((pr_oldbfg()&127) - 64) * (ANG90/768);
		self->pitch += ((pr_oldbfg()&127) - 64) * (ANG90/640);
		mo = P_SpawnPlayerMissile (self, plasma[i]);
		// Restore saved values
		self->angle = SavedPlayerAngle;
		self->pitch = SavedPlayerPitch;
    }
	if (doesautoaim && weapon != NULL)
	{ // Restore autoaim setting
		weapon->WeaponFlags &= ~WIF_NOAUTOAIM;
	}
	return 0;
}
