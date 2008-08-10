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
#include "thingdef/thingdef.h"

static FRandom pr_punch ("Punch");
static FRandom pr_saw ("Saw");
static FRandom pr_fireshotgun2 ("FireSG2");
static FRandom pr_fireplasma ("FirePlasma");
static FRandom pr_firerail ("FireRail");
static FRandom pr_bfgspray ("BFGSpray");

//
// A_Punch
//
void A_Punch (AActor *actor)
{
	angle_t 	angle;
	int 		damage;
	int 		pitch;
	AActor		*linetarget;

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
	pitch = P_AimLineAttack (actor, angle, MELEERANGE, &linetarget);
	P_LineAttack (actor, angle, MELEERANGE, pitch, damage, NAME_Melee, NAME_BulletPuff, true);

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

			P_SetPsprite (actor->player, ps_flash, weapon->FindState(NAME_Flash));
		}
		actor->player->mo->PlayAttacking2 ();

		accurate = !actor->player->refire;
	}
	else
	{
		accurate = true;
	}

	S_Sound (actor, CHAN_WEAPON, "weapons/pistol", 1, ATTN_NORM);

	P_GunShot (actor, accurate, PClass::FindClass(NAME_BulletPuff), P_BulletSlope (actor));
}

//
// A_Saw
//
void A_Saw (AActor *actor)
{
	angle_t 	angle;
	int 		damage=0;
	player_t *player;
	AActor *linetarget;
	
	FSoundID fullsound;
	FSoundID hitsound;
	const PClass * pufftype = NULL;

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

	int index = CheckIndex (4, NULL);
	if (index >= 0) 
	{
		fullsound = FSoundID(StateParameters[index]);
		hitsound = FSoundID(StateParameters[index+1]);
		damage = EvalExpressionI (StateParameters[index+2], actor);
		pufftype = PClass::FindClass ((ENamedName)StateParameters[index+3]);
	}
	else
	{
		fullsound = "weapons/sawfull";
		hitsound = "weapons/sawhit";
	}
	if (pufftype == NULL) pufftype = PClass::FindClass(NAME_BulletPuff);
	if (damage == 0) damage = 2;
	
	damage *= (pr_saw()%10+1);
	angle = actor->angle;
	angle += pr_saw.Random2() << 18;
	
	// use meleerange + 1 so the puff doesn't skip the flash (i.e. plays all states)
	P_LineAttack (actor, angle, MELEERANGE+1,
				  P_AimLineAttack (actor, angle, MELEERANGE+1, &linetarget), damage,
				  GetDefaultByType(pufftype)->DamageType, pufftype);

	if (!linetarget)
	{
		S_Sound (actor, CHAN_WEAPON, fullsound, 1, ATTN_NORM);
		return;
	}
	S_Sound (actor, CHAN_WEAPON, hitsound, 1, ATTN_NORM);
		
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
		P_SetPsprite (player, ps_flash, weapon->FindState(NAME_Flash));
	}
	player->mo->PlayAttacking2 ();

	angle_t pitch = P_BulletSlope (actor);

	for (i=0 ; i<7 ; i++)
		P_GunShot (actor, false, PClass::FindClass(NAME_BulletPuff), pitch);
}

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
		P_SetPsprite (player, ps_flash, weapon->FindState(NAME_Flash));
	}
	player->mo->PlayAttacking2 ();


	angle_t pitch = P_BulletSlope (actor);
		
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
					  pitch + (pr_fireshotgun2.Random2() * 332063), damage,
					  NAME_None, NAME_BulletPuff);
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


//------------------------------------------------------------------------------------
//
// Setting a random flash like some of Doom's weapons can easily crash when the
// definition is overridden incorrectly so let's check that the state actually exists.
// Be aware though that this will not catch all DEHACKED related problems. But it will
// find all DECORATE related ones.
//
//------------------------------------------------------------------------------------

void P_SetSafeFlash(AWeapon * weapon, player_t * player, FState * flashstate, int index)
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
	// The only way to check this would be to go through all Dehacked modifiable actors and
	// find the correct one.
	// For now let's assume that it will work.
	P_SetPsprite (player, ps_flash, flashstate + index);
}

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

	AWeapon *weapon = player->ReadyWeapon;
	if (weapon != NULL)
	{
		if (!weapon->DepleteAmmo (weapon->bAltFire))
			return;
		
		S_Sound (actor, CHAN_WEAPON, "weapons/chngun", 1, ATTN_NORM);

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

	P_GunShot (actor, !player->refire, PClass::FindClass(NAME_BulletPuff), P_BulletSlope (actor));
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
	P_SpawnPlayerMissile (actor, PClass::FindClass("Rocket"));
}

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

		FState *flash = weapon->FindState(NAME_Flash);
		if (flash != NULL)
		{
			P_SetSafeFlash(weapon, player, flash, (pr_fireplasma()&1));
		}
	}

	P_SpawnPlayerMissile (actor, PClass::FindClass("PlasmaBall"));
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

		FState *flash = weapon->FindState(NAME_Flash);
		if (flash != NULL)
		{
			P_SetSafeFlash(weapon, player, flash, (pr_firerail()&1));
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
	P_SpawnPlayerMissile (actor, PClass::FindClass("BFGBall"));
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
	AActor				*linetarget;

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
		spraytype = PClass::FindClass("BFGExtra");
	}

	// [RH] Don't crash if no target
	if (!mo->target)
		return;

	// offset angles from its attack angle
	for (i = 0; i < numrays; i++)
	{
		an = mo->angle - ANG90/2 + ANG90/numrays*i;

		// mo->target is the originator (player) of the missile
		P_AimLineAttack (mo->target, an, 16*64*FRACUNIT, &linetarget, ANGLE_1*32);

		if (!linetarget)
			continue;

		AActor *spray = Spawn (spraytype, linetarget->x, linetarget->y,
			linetarget->z + (linetarget->height>>2), ALLOW_REPLACE);

		if (spray && (spray->flags5 & MF5_PUFFGETSOWNER))
			spray->target = mo->target;

		
		damage = 0;
		for (j = 0; j < damagecnt; ++j)
			damage += (pr_bfgspray() & 7) + 1;

		thingToHit = linetarget;
		P_DamageMobj (thingToHit, mo->target, mo->target, damage, NAME_BFGSplash);
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

