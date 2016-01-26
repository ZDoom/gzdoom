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
	angle_t 	angle;
	int 		damage;
	int 		pitch;
	AActor		*linetarget;

	if (self->player != NULL)
	{
		AWeapon *weapon = self->player->ReadyWeapon;
		if (weapon != NULL && !(weapon->WeaponFlags & WIF_DEHAMMO) && ACTION_CALL_FROM_WEAPON())
		{
			if (!weapon->DepleteAmmo (weapon->bAltFire))
				return;
		}
	}

	damage = (pr_punch()%10+1)<<1;

	if (self->FindInventory<APowerStrength>())	
		damage *= 10;

	angle = self->angle;

	angle += pr_punch.Random2() << 18;
	pitch = P_AimLineAttack (self, angle, MELEERANGE, &linetarget);

	P_LineAttack (self, angle, MELEERANGE, pitch, damage, NAME_Melee, NAME_BulletPuff, LAF_ISMELEEATTACK, &linetarget);

	// turn to face target
	if (linetarget)
	{
		S_Sound (self, CHAN_WEAPON, "*fist", 1, ATTN_NORM);
		self->angle = self->AngleTo(linetarget);
	}
}

//
// A_FirePistol
//
DEFINE_ACTION_FUNCTION(AActor, A_FirePistol)
{
	bool accurate;

	if (self->player != NULL)
	{
		AWeapon *weapon = self->player->ReadyWeapon;
		if (weapon != NULL && ACTION_CALL_FROM_WEAPON())
		{
			if (!weapon->DepleteAmmo (weapon->bAltFire, true, 1))
				return;

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

	P_GunShot (self, accurate, PClass::FindClass(NAME_BulletPuff), P_BulletSlope (self));
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
	angle_t angle;
	angle_t slope;
	player_t *player;
	AActor *linetarget;
	int actualdamage;

	ACTION_PARAM_START(11);
	ACTION_PARAM_SOUND(fullsound, 0);
	ACTION_PARAM_SOUND(hitsound, 1);
	ACTION_PARAM_INT(damage, 2);
	ACTION_PARAM_CLASS(pufftype, 3);
	ACTION_PARAM_INT(Flags, 4);
	ACTION_PARAM_FIXED(Range, 5);
	ACTION_PARAM_ANGLE(Spread_XY, 6);
	ACTION_PARAM_ANGLE(Spread_Z, 7);
	ACTION_PARAM_FIXED(LifeSteal, 8);
	ACTION_PARAM_INT(lifestealmax, 9);
	ACTION_PARAM_CLASS(armorbonustype, 10);

	if (NULL == (player = self->player))
	{
		return;
	}

	if (pufftype == NULL) pufftype = PClass::FindClass(NAME_BulletPuff);
	if (damage == 0) damage = 2;

	if (!(Flags & SF_NORANDOM))
		damage *= (pr_saw()%10+1);
	
	// use meleerange + 1 so the puff doesn't skip the flash (i.e. plays all states)
	if (Range == 0) Range = MELEERANGE+1;

	angle = self->angle + (pr_saw.Random2() * (Spread_XY / 255));
	slope = P_AimLineAttack (self, angle, Range, &linetarget) + (pr_saw.Random2() * (Spread_Z / 255));

	AWeapon *weapon = self->player->ReadyWeapon;
	if ((weapon != NULL) && !(Flags & SF_NOUSEAMMO) && !(!linetarget && (Flags & SF_NOUSEAMMOMISS)) && !(weapon->WeaponFlags & WIF_DEHAMMO) && ACTION_CALL_FROM_WEAPON())
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
	}

	P_LineAttack (self, angle, Range, slope, damage, NAME_Melee, pufftype, false, &linetarget, &actualdamage);

	if (!linetarget)
	{
		if ((Flags & SF_RANDOMLIGHTMISS) && (pr_saw() > 64))
		{
			player->extralight = !player->extralight;
		}
		S_Sound (self, CHAN_WEAPON, fullsound, 1, ATTN_NORM);
		return;
	}

	if (Flags & SF_RANDOMLIGHTHIT)
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

	if (LifeSteal && !(linetarget->flags5 & MF5_DONTDRAIN))
	{
		if (Flags & SF_STEALARMOR)
		{
			if (!armorbonustype) armorbonustype = PClass::FindClass("ArmorBonus");

			if (armorbonustype->IsDescendantOf (RUNTIME_CLASS(ABasicArmorBonus)))
			{
				ABasicArmorBonus *armorbonus = static_cast<ABasicArmorBonus *>(Spawn (armorbonustype, 0,0,0, NO_REPLACE));
				armorbonus->SaveAmount *= (actualdamage * LifeSteal) >> FRACBITS;
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
			P_GiveBody (self, (actualdamage * LifeSteal) >> FRACBITS, lifestealmax);
		}
	}

	S_Sound (self, CHAN_WEAPON, hitsound, 1, ATTN_NORM);
		
	// turn to face target
	if (!(Flags & SF_NOTURN))
	{
		angle = self->AngleTo(linetarget);
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
	if (!(Flags & SF_NOPULLIN))
		self->flags |= MF_JUSTATTACKED;
}

//
// A_FireShotgun
//
DEFINE_ACTION_FUNCTION(AActor, A_FireShotgun)
{
	int i;
	player_t *player;

	if (NULL == (player = self->player))
	{
		return;
	}

	S_Sound (self, CHAN_WEAPON,  "weapons/shotgf", 1, ATTN_NORM);
	AWeapon *weapon = self->player->ReadyWeapon;
	if (weapon != NULL && ACTION_CALL_FROM_WEAPON())
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire, true, 1))
			return;
		P_SetPsprite (player, ps_flash, weapon->FindState(NAME_Flash));
	}
	player->mo->PlayAttacking2 ();

	angle_t pitch = P_BulletSlope (self);

	for (i=0 ; i<7 ; i++)
		P_GunShot (self, false, PClass::FindClass(NAME_BulletPuff), pitch);
}

//
// A_FireShotgun2
//
DEFINE_ACTION_FUNCTION(AActor, A_FireShotgun2)
{
	int 		i;
	angle_t 	angle;
	int 		damage;
	player_t *player;

	if (NULL == (player = self->player))
	{
		return;
	}

	S_Sound (self, CHAN_WEAPON, "weapons/sshotf", 1, ATTN_NORM);
	AWeapon *weapon = self->player->ReadyWeapon;
	if (weapon != NULL && ACTION_CALL_FROM_WEAPON())
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire, true, 2))
			return;
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
}

DEFINE_ACTION_FUNCTION(AActor, A_OpenShotgun2)
{
	S_Sound (self, CHAN_WEAPON, "weapons/sshoto", 1, ATTN_NORM);
}

DEFINE_ACTION_FUNCTION(AActor, A_LoadShotgun2)
{
	S_Sound (self, CHAN_WEAPON, "weapons/sshotl", 1, ATTN_NORM);
}

DEFINE_ACTION_FUNCTION(AActor, A_CloseShotgun2)
{
	S_Sound (self, CHAN_WEAPON, "weapons/sshotc", 1, ATTN_NORM);
	A_ReFire (self);
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

	const PClass * cls = weapon->GetClass();
	while (cls != RUNTIME_CLASS(AWeapon))
	{
		FActorInfo * info = cls->ActorInfo;
		if (flashstate >= info->OwnedStates && flashstate < info->OwnedStates + info->NumOwnedStates)
		{
			// The flash state belongs to this class.
			// Now let's check if the actually wanted state does also
			if (flashstate+index < info->OwnedStates + info->NumOwnedStates)
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
		cls = cls->ParentClass;
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
	player_t *player;

	if (self == NULL || NULL == (player = self->player))
	{
		return;
	}

	AWeapon *weapon = player->ReadyWeapon;
	if (weapon != NULL && ACTION_CALL_FROM_WEAPON())
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire, true, 1))
			return;
		
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

	P_GunShot (self, !player->refire, PClass::FindClass(NAME_BulletPuff), P_BulletSlope (self));
}

//
// A_FireMissile
//
DEFINE_ACTION_FUNCTION(AActor, A_FireMissile)
{
	player_t *player;

	if (NULL == (player = self->player))
	{
		return;
	}
	AWeapon *weapon = self->player->ReadyWeapon;
	if (weapon != NULL && ACTION_CALL_FROM_WEAPON())
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire, true, 1))
			return;
	}
	P_SpawnPlayerMissile (self, PClass::FindClass("Rocket"));
}

//
// A_FireSTGrenade: not exactly backported from ST, but should work the same
//
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_FireSTGrenade)
{
	player_t *player;
	ACTION_PARAM_START(1);
	ACTION_PARAM_CLASS(grenade, 0);
	if (grenade == NULL) return;

	if (NULL == (player = self->player))
	{
		return;
	}
	AWeapon *weapon = self->player->ReadyWeapon;
	if (weapon != NULL && ACTION_CALL_FROM_WEAPON())
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
	}
		
	// Temporarily raise the pitch to send the grenade slightly upwards
	fixed_t SavedPlayerPitch = self->pitch;
	self->pitch -= (1152 << FRACBITS);
	P_SpawnPlayerMissile(self, grenade);
	self->pitch = SavedPlayerPitch;
}

//
// A_FirePlasma
//
DEFINE_ACTION_FUNCTION(AActor, A_FirePlasma)
{
	player_t *player;

	if (NULL == (player = self->player))
	{
		return;
	}
	AWeapon *weapon = self->player->ReadyWeapon;
	if (weapon != NULL && ACTION_CALL_FROM_WEAPON())
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire, true, 1))
			return;

		FState *flash = weapon->FindState(NAME_Flash);
		if (flash != NULL)
		{
			P_SetSafeFlash(weapon, player, flash, (pr_fireplasma()&1));
		}
	}

	P_SpawnPlayerMissile (self, PClass::FindClass("PlasmaBall"));
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
	FireRailgun(self, 0, ACTION_CALL_FROM_WEAPON());
}

DEFINE_ACTION_FUNCTION(AActor, A_FireRailgunRight)
{
	FireRailgun(self, 10, ACTION_CALL_FROM_WEAPON());
}

DEFINE_ACTION_FUNCTION(AActor, A_FireRailgunLeft)
{
	FireRailgun(self, -10, ACTION_CALL_FROM_WEAPON());
}

DEFINE_ACTION_FUNCTION(AActor, A_RailWait)
{
	// Okay, this was stupid. Just use a NULL function instead of this.
}

//
// A_FireBFG
//

DEFINE_ACTION_FUNCTION(AActor, A_FireBFG)
{
	player_t *player;

	if (NULL == (player = self->player))
	{
		return;
	}

	AWeapon *weapon = self->player->ReadyWeapon;
	if (weapon != NULL && ACTION_CALL_FROM_WEAPON())
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire, true, deh.BFGCells))
			return;
	}

	P_SpawnPlayerMissile (self,  0, 0, 0, PClass::FindClass("BFGBall"), self->angle, NULL, NULL, !!(dmflags2 & DF2_NO_FREEAIMBFG));
}


//
// A_BFGSpray
// Spawn a BFG explosion on every monster in view
//
DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_BFGSpray)
{
	int 				i;
	int 				j;
	int 				damage;
	angle_t 			an;
	AActor				*linetarget;

	ACTION_PARAM_START(7);
	ACTION_PARAM_CLASS(spraytype, 0);
	ACTION_PARAM_INT(numrays, 1);
	ACTION_PARAM_INT(damagecnt, 2);
	ACTION_PARAM_ANGLE(angle, 3);
	ACTION_PARAM_FIXED(distance, 4);
	ACTION_PARAM_ANGLE(vrange, 5);
	ACTION_PARAM_INT(defdamage, 6);

	if (spraytype == NULL) spraytype = PClass::FindClass("BFGExtra");
	if (numrays <= 0) numrays = 40;
	if (damagecnt <= 0) damagecnt = 15;
	if (angle == 0) angle = ANG90;
	if (distance <= 0) distance = 16 * 64 * FRACUNIT;
	if (vrange == 0) vrange = ANGLE_1 * 32;

	// [RH] Don't crash if no target
	if (!self->target)
		return;

	// offset angles from its attack angle
	for (i = 0; i < numrays; i++)
	{
		an = self->angle - angle / 2 + angle / numrays*i;

		// self->target is the originator (player) of the missile
		P_AimLineAttack(self->target, an, distance, &linetarget, vrange);

		if (linetarget != NULL)
		{
			AActor *spray = Spawn(spraytype, linetarget->PosPlusZ(linetarget->height >> 2), ALLOW_REPLACE);

			int dmgFlags = 0;
			FName dmgType = NAME_BFGSplash;

			if (spray != NULL)
			{
				if (spray->flags6 & MF6_MTHRUSPECIES && self->target->GetSpecies() == linetarget->GetSpecies())
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

			int newdam = P_DamageMobj(linetarget, self->target, self->target, damage, dmgType, dmgFlags);
			P_TraceBleed(newdam > 0 ? newdam : damage, linetarget, self->target);
		}
	}
}

//
// A_BFGsound
//
DEFINE_ACTION_FUNCTION(AActor, A_BFGsound)
{
	S_Sound (self, CHAN_WEAPON, "weapons/bfgf", 1, ATTN_NORM);
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
	const PClass * plasma[] = {PClass::FindClass("PlasmaBall1"), PClass::FindClass("PlasmaBall2")};
	AActor * mo = NULL;

	player_t *player;
	bool doesautoaim = false;

	if (NULL == (player = self->player))
	{
		return;
	}

	AWeapon *weapon = self->player->ReadyWeapon;
	if (!ACTION_CALL_FROM_WEAPON()) weapon = NULL;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire, true, 1))
			return;

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
	if (doesautoaim && weapon != NULL) weapon->WeaponFlags &= ~WIF_NOAUTOAIM; // Restore autoaim setting
}
