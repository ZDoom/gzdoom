
//**************************************************************************
//**
//** p_pspr.c : Heretic 2 : Raven Software, Corp.
//**
//** $RCSfile: p_pspr.c,v $
//** $Revision: 1.105 $
//** $Date: 96/01/06 03:23:35 $
//** $Author: bgokey $
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>

#include "doomdef.h"
#include "d_event.h"
#include "c_cvars.h"
#include "m_random.h"
#include "p_local.h"
#include "s_sound.h"
#include "doomstat.h"
#include "gi.h"
#include "p_pspr.h"
#include "p_effect.h"
#include "a_doomglobal.h"
#include "templates.h"
#include "thingdef/thingdef.h"

// MACROS ------------------------------------------------------------------

#define LOWERSPEED				FRACUNIT*6
#define RAISESPEED				FRACUNIT*6

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// [SO] 1=Weapons states are all 1 tick
//		2=states with a function 1 tick, others 0 ticks.
CVAR(Int, sv_fastweapons, false, CVAR_SERVERINFO);

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static FRandom pr_wpnreadysnd ("WpnReadySnd");
static FRandom pr_gunshot ("GunShot");

// CODE --------------------------------------------------------------------

//---------------------------------------------------------------------------
//
// PROC P_SetPsprite
//
//---------------------------------------------------------------------------

void P_SetPsprite (player_t *player, int position, FState *state)
{
	pspdef_t *psp;

	if (position == ps_weapon)
	{
		// A_WeaponReady will re-set this as needed
		player->cheats &= ~CF_WEAPONREADY;
	}

	psp = &player->psprites[position];
	do
	{
		if (state == NULL)
		{ // Object removed itself.
			psp->state = NULL;
			break;
		}
		psp->state = state;

		if (sv_fastweapons >= 2 && position == ps_weapon)
			psp->tics = (state->Action == NULL) ? 0 : 1;
		else if (sv_fastweapons)
			psp->tics = 1;		// great for producing decals :)
		else
			psp->tics = state->GetTics(); // could be 0

		if (state->GetMisc1())
		{ // Set coordinates.
			psp->sx = state->GetMisc1()<<FRACBITS;
		}
		if (state->GetMisc2())
		{
			psp->sy = state->GetMisc2()<<FRACBITS;
		}

		if (state->GetAction())
		{ 
			// The parameterized action functions need access to the current state and
			// if a function is supposed to work with both actors and weapons
			// there is no real means to get to it reliably so I store it in a global variable here.
			// Yes, I know this is truly awful but it is the only method I can think of 
			// that does not involve changing stuff throughout the code. 
			// Of course this should be rewritten ASAP.
			CallingState = state;
			// Call action routine.
			if (player->mo != NULL)
			{
				state->GetAction() (player->mo);
			}
			if (!psp->state)
			{
				break;
			}
		}
		state = psp->state->GetNextState();
	} while (!psp->tics); // An initial state of 0 could cycle through.
}

//---------------------------------------------------------------------------
//
// PROC P_SetPspriteNF
//
// Identical to P_SetPsprite, without calling the action function
//---------------------------------------------------------------------------

void P_SetPspriteNF (player_t *player, int position, FState *state)
{
	pspdef_t *psp;

	psp = &player->psprites[position];
	do
	{
		if (state == NULL)
		{ // Object removed itself.
			psp->state = NULL;
			break;
		}
		psp->state = state;
		psp->tics = state->GetTics(); // could be 0

		if (state->GetMisc1())
		{ // Set coordinates.
			psp->sx = state->GetMisc1()<<FRACBITS;
		}
		if (state->GetMisc2())
		{
			psp->sy = state->GetMisc2()<<FRACBITS;
		}
		state = psp->state->GetNextState();
	} while (!psp->tics); // An initial state of 0 could cycle through.
}

//---------------------------------------------------------------------------
//
// PROC P_BringUpWeapon
//
// Starts bringing the pending weapon up from the bottom of the screen.
// This is only called to start the rising, not throughout it.
//
//---------------------------------------------------------------------------

void P_BringUpWeapon (player_t *player)
{
	FState *newstate;
	AWeapon *weapon;

	if (player->PendingWeapon == WP_NOCHANGE)
	{
		player->psprites[ps_weapon].sy = WEAPONTOP;
		P_SetPsprite (player, ps_weapon, player->ReadyWeapon->GetReadyState());
		return;
	}

	weapon = player->PendingWeapon;

	// If the player has a tome of power, use this weapon's powered up
	// version, if one is available.
	if (weapon != NULL &&
		weapon->SisterWeapon &&
		weapon->SisterWeapon->WeaponFlags & WIF_POWERED_UP &&
		player->mo->FindInventory (RUNTIME_CLASS(APowerWeaponLevel2)))
	{
		weapon = weapon->SisterWeapon;
	}

	if (weapon != NULL)
	{
		if (weapon->UpSound)
		{
			S_Sound (player->mo, CHAN_WEAPON, weapon->UpSound, 1, ATTN_NORM);
		}
		newstate = weapon->GetUpState ();
	}
	else
	{
		newstate = NULL;
	}
	player->PendingWeapon = WP_NOCHANGE;
	player->ReadyWeapon = weapon;
	player->psprites[ps_weapon].sy = player->cheats & CF_INSTANTWEAPSWITCH
		? WEAPONTOP : WEAPONBOTTOM;
	P_SetPsprite (player, ps_weapon, newstate);
}


//---------------------------------------------------------------------------
//
// PROC P_FireWeapon
//
//---------------------------------------------------------------------------

void P_FireWeapon (player_t *player)
{
	AWeapon *weapon;

	// [SO] 9/2/02: People were able to do an awful lot of damage
	// when they were observers...
	if (!player->isbot && bot_observer)
	{
		return;
	}

	weapon = player->ReadyWeapon;
	if (weapon == NULL || !weapon->CheckAmmo (AWeapon::PrimaryFire, true))
	{
		return;
	}

	player->mo->PlayAttacking ();
	weapon->bAltFire = false;
	P_SetPsprite (player, ps_weapon, weapon->GetAtkState(!!player->refire));
	if (!(weapon->WeaponFlags & WIF_NOALERT))
	{
		P_NoiseAlert (player->mo, player->mo, false);
	}
}

//---------------------------------------------------------------------------
//
// PROC P_FireWeaponAlt
//
//---------------------------------------------------------------------------

void P_FireWeaponAlt (player_t *player)
{
	AWeapon *weapon;

	// [SO] 9/2/02: People were able to do an awful lot of damage
	// when they were observers...
	if (!player->isbot && bot_observer)
	{
		return;
	}

	weapon = player->ReadyWeapon;
	if (weapon == NULL || weapon->FindState(NAME_AltFire) == NULL || !weapon->CheckAmmo (AWeapon::AltFire, true))
	{
		return;
	}

	player->mo->PlayAttacking ();
	weapon->bAltFire = true;


	P_SetPsprite (player, ps_weapon, weapon->GetAltAtkState(!!player->refire));
	if (!(weapon->WeaponFlags & WIF_NOALERT))
	{
		P_NoiseAlert (player->mo, player->mo, false);
	}
}

//---------------------------------------------------------------------------
//
// PROC P_DropWeapon
//
// The player died, so put the weapon away.
//
//---------------------------------------------------------------------------

void P_DropWeapon (player_t *player)
{
	if (player->ReadyWeapon != NULL)
	{
		P_SetPsprite (player, ps_weapon, player->ReadyWeapon->GetDownState());
	}
}

//============================================================================
//
// P_BobWeapon
//
// [RH] Moved this out of A_WeaponReady so that the weapon can bob every
// tic and not just when A_WeaponReady is called. Not all weapons execute
// A_WeaponReady every tic, and it looks bad if they don't bob smoothly.
//
//============================================================================

void P_BobWeapon (player_t *player, pspdef_t *psp, fixed_t *x, fixed_t *y)
{
	static fixed_t curbob;

	AWeapon *weapon;
	fixed_t bobtarget;

	weapon = player->ReadyWeapon;

	if (weapon == NULL || weapon->WeaponFlags & WIF_DONTBOB)
	{
		*x = *y = 0;
		return;
	}

	// Bob the weapon based on movement speed.
	int angle = (128*35/TICRATE*level.time)&FINEMASK;

	// [RH] Smooth transitions between bobbing and not-bobbing frames.
	// This also fixes the bug where you can "stick" a weapon off-center by
	// shooting it when it's at the peak of its swing.
	bobtarget = (player->cheats & CF_WEAPONREADY) ? player->bob : 0;
	if (curbob != bobtarget)
	{
		if (abs (bobtarget - curbob) <= 1*FRACUNIT)
		{
			curbob = bobtarget;
		}
		else
		{
			fixed_t zoom = MAX<fixed_t> (1*FRACUNIT, abs (curbob - bobtarget) / 40);
			if (curbob > bobtarget)
			{
				curbob -= zoom;
			}
			else
			{
				curbob += zoom;
			}
		}
	}

	if (curbob != 0)
	{
		*x = FixedMul(player->bob, finecosine[angle]);
		*y = FixedMul(player->bob, finesine[angle & (FINEANGLES/2-1)]);
	}
	else
	{
		*x = 0;
		*y = 0;
	}
}

//---------------------------------------------------------------------------
//
// PROC A_WeaponReady
//
// The player can fire the weapon or change to another weapon at this time.
//
//---------------------------------------------------------------------------

void A_WeaponReady(AActor *actor)
{
	player_t *player = actor->player;
	AWeapon *weapon;

	if (NULL == player)
	{
		return;
	}

	weapon = player->ReadyWeapon;

	if (NULL == weapon)
	{
		return;
	}

	// Change player from attack state
	if (actor->InStateSequence(actor->state, actor->MissileState) ||
		actor->InStateSequence(actor->state, actor->MeleeState))
	{
		static_cast<APlayerPawn *>(actor)->PlayIdle ();
	}

	// Play ready sound, if any.
	if (weapon->ReadySound && player->psprites[ps_weapon].state == weapon->FindState(NAME_Ready))
	{
		if (!(weapon->WeaponFlags & WIF_READYSNDHALF) || pr_wpnreadysnd() < 128)
		{
			S_Sound (actor, CHAN_WEAPON, weapon->ReadySound, 1, ATTN_NORM);
		}
	}

	// Prepare for bobbing and firing action.
	player->cheats |= CF_WEAPONREADY;
	player->psprites[ps_weapon].sx = 0;
	player->psprites[ps_weapon].sy = WEAPONTOP;
}

//---------------------------------------------------------------------------
//
// PROC P_CheckWeaponFire
//
// The player can fire the weapon or change to another weapon at this time.
// [RH] This was in A_WeaponReady before, but that only works well when the
// weapon's ready frames have a one tic delay.
//
//---------------------------------------------------------------------------

void P_CheckWeaponFire (player_t *player)
{
	AWeapon *weapon = player->ReadyWeapon;

	if (weapon == NULL)
		return;

	// Put the weapon away if the player has a pending weapon or has died.
	if ((player->morphTics == 0 && player->PendingWeapon != WP_NOCHANGE) || player->health <= 0)
	{
		P_SetPsprite (player, ps_weapon, weapon->GetDownState());
		return;
	}

	// Check for fire. Some weapons do not auto fire.
	if (player->cmd.ucmd.buttons & BT_ATTACK)
	{
		if (!player->attackdown || !(weapon->WeaponFlags & WIF_NOAUTOFIRE))
		{
			player->attackdown = true;
			P_FireWeapon (player);
			return;
		}
	}
	else if (player->cmd.ucmd.buttons & BT_ALTATTACK)
	{
		if (!player->attackdown || !(weapon->WeaponFlags & WIF_NOAUTOFIRE))
		{
			player->attackdown = true;
			P_FireWeaponAlt (player);
			return;
		}
	}
	else
	{
		player->attackdown = false;
	}
}

//---------------------------------------------------------------------------
//
// PROC A_ReFire
//
// The player can re-fire the weapon without lowering it entirely.
//
//---------------------------------------------------------------------------

void A_ReFire (AActor *actor)
{
	player_t *player = actor->player;

	if (NULL == player)
	{
		return;
	}
	if ((player->cmd.ucmd.buttons&BT_ATTACK)
		&& !player->ReadyWeapon->bAltFire
		&& player->PendingWeapon == WP_NOCHANGE && player->health)
	{
		player->refire++;
		P_FireWeapon (player);
	}
	else if ((player->cmd.ucmd.buttons&BT_ALTATTACK)
		&& player->ReadyWeapon->bAltFire
		&& player->PendingWeapon == WP_NOCHANGE && player->health)
	{
		player->refire++;
		P_FireWeaponAlt (player);
	}
	else
	{
		player->refire = 0;
		player->ReadyWeapon->CheckAmmo (player->ReadyWeapon->bAltFire
			? AWeapon::AltFire : AWeapon::PrimaryFire, true);
	}
}

//---------------------------------------------------------------------------
//
// PROC A_CheckReload
//
// Present in Doom, but unused. Also present in Strife, and actually used.
// This and what I call A_XBowReFire are actually the same thing in Strife,
// not two separate functions as I have them here.
//
//---------------------------------------------------------------------------

void A_CheckReload (AActor *actor)
{
	if (actor->player != NULL)
	{
		actor->player->ReadyWeapon->CheckAmmo (
			actor->player->ReadyWeapon->bAltFire ? AWeapon::AltFire
			: AWeapon::PrimaryFire, true);
	}
}

//---------------------------------------------------------------------------
//
// PROC A_Lower
//
//---------------------------------------------------------------------------

void A_Lower (AActor *actor)
{
	player_t *player = actor->player;
	pspdef_t *psp;

	if (NULL == player)
	{
		return;
	}
	psp = &player->psprites[ps_weapon];
	if (player->morphTics || player->cheats & CF_INSTANTWEAPSWITCH)
	{
		psp->sy = WEAPONBOTTOM;
	}
	else
	{
		psp->sy += LOWERSPEED;
	}
	if (psp->sy < WEAPONBOTTOM)
	{ // Not lowered all the way yet
		return;
	}
	if (player->playerstate == PST_DEAD)
	{ // Player is dead, so don't bring up a pending weapon
		psp->sy = WEAPONBOTTOM;
		return;
	}
	if (player->health <= 0)
	{ // Player is dead, so keep the weapon off screen
		P_SetPsprite (player,  ps_weapon, NULL);
		return;
	}
/*	if (player->PendingWeapon != WP_NOCHANGE)
	{ // [RH] Make sure we're actually changing weapons.
		player->ReadyWeapon = player->PendingWeapon;
	} */
	// [RH] Clear the flash state. Only needed for Strife.
	P_SetPsprite (player, ps_flash, NULL);
	P_BringUpWeapon (player);
}

//---------------------------------------------------------------------------
//
// PROC A_Raise
//
//---------------------------------------------------------------------------

void A_Raise (AActor *actor)
{
	if (actor == NULL)
	{
		return;
	}
	player_t *player = actor->player;
	pspdef_t *psp;

	if (NULL == player)
	{
		return;
	}
	if (player->PendingWeapon != WP_NOCHANGE)
	{
		P_SetPsprite (player, ps_weapon, player->ReadyWeapon->GetDownState());
		return;
	}
	psp = &player->psprites[ps_weapon];
	psp->sy -= RAISESPEED;
	if (psp->sy > WEAPONTOP)
	{ // Not raised all the way yet
		return;
	}
	psp->sy = WEAPONTOP;
	if (player->ReadyWeapon != NULL)
	{
		P_SetPsprite (player, ps_weapon, player->ReadyWeapon->GetReadyState());
	}
	else
	{
		player->psprites[ps_weapon].state = NULL;
	}
}




//
// A_GunFlash
//
void A_GunFlash (AActor *actor)
{
	player_t *player = actor->player;

	if (NULL == player)
	{
		return;
	}
	player->mo->PlayAttacking2 ();

	FState * flash=NULL;
	if (player->ReadyWeapon->bAltFire) flash = player->ReadyWeapon->FindState(NAME_AltFlash);
	if (flash == NULL) flash = player->ReadyWeapon->FindState(NAME_Flash);
	P_SetPsprite (player, ps_flash, flash);
}



//
// WEAPON ATTACKS
//

//
// P_BulletSlope
// Sets a slope so a near miss is at aproximately
// the height of the intended target
//

angle_t P_BulletSlope (AActor *mo, AActor **pLineTarget)
{
	static const int angdiff[3] = { -1<<26, 1<<26, 0 };
	int i;
	angle_t an;
	angle_t pitch;
	AActor *linetarget;

	// see which target is to be aimed at
	i = 2;
	do
	{
		an = mo->angle + angdiff[i];
		pitch = P_AimLineAttack (mo, an, 16*64*FRACUNIT, &linetarget);

		if (mo->player != NULL &&
			level.IsFreelookAllowed() &&
			mo->player->userinfo.aimdist <= ANGLE_1/2)
		{
			break;
		}
	} while (linetarget == NULL && --i >= 0);
	if (pLineTarget != NULL)
	{
		*pLineTarget = linetarget;
	}
	return pitch;
}


//
// P_GunShot
//
void P_GunShot (AActor *mo, bool accurate, const PClass *pufftype, angle_t pitch)
{
	angle_t 	angle;
	int 		damage;
		
	damage = 5*(pr_gunshot()%3+1);
	angle = mo->angle;

	if (!accurate)
	{
		angle += pr_gunshot.Random2 () << 18;
	}

	P_LineAttack (mo, angle, PLAYERMISSILERANGE, pitch, damage, NAME_None, pufftype);
}

void A_Light0 (AActor *actor)
{
	if (actor->player != NULL)
	{
		actor->player->extralight = 0;
	}
}

void A_Light1 (AActor *actor)
{
	if (actor->player != NULL)
	{
		actor->player->extralight = 1;
	}
}

void A_Light2 (AActor *actor)
{
	if (actor->player != NULL)
	{
		actor->player->extralight = 2;
	}
}

void A_Light (AActor *actor)
{
	int index=CheckIndex(1, &CallingState);

	if (actor->player != NULL && index > 0)
	{
		actor->player->extralight = clamp<int>(EvalExpressionI (StateParameters[index], actor), 0, 20);
	}
}

//------------------------------------------------------------------------
//
// PROC P_SetupPsprites
//
// Called at start of level for each player
//
//------------------------------------------------------------------------

void P_SetupPsprites(player_t *player)
{
	int i;

	// Remove all psprites
	for (i = 0; i < NUMPSPRITES; i++)
	{
		player->psprites[i].state = NULL;
	}
	// Spawn the ready weapon
	player->PendingWeapon = player->ReadyWeapon;
	P_BringUpWeapon (player);
}

//------------------------------------------------------------------------
//
// PROC P_MovePsprites
//
// Called every tic by player thinking routine
//
//------------------------------------------------------------------------

void P_MovePsprites (player_t *player)
{
	int i;
	pspdef_t *psp;
	FState *state;

	// [RH] If you don't have a weapon, then the psprites should be NULL.
	if (player->ReadyWeapon == NULL && (player->health > 0 || player->mo->DamageType != NAME_Fire))
	{
		P_SetPsprite (player, ps_weapon, NULL);
		P_SetPsprite (player, ps_flash, NULL);
		if (player->PendingWeapon != WP_NOCHANGE)
		{
			P_BringUpWeapon (player);
		}
	}
	else
	{
		psp = &player->psprites[0];
		for (i = 0; i < NUMPSPRITES; i++, psp++)
		{
			if ((state = psp->state) != NULL) // a null state means not active
			{
				// drop tic count and possibly change state
				if (psp->tics != -1)	// a -1 tic count never changes
				{
					psp->tics--;
					if(!psp->tics)
					{
						P_SetPsprite (player, i, psp->state->GetNextState());
					}
				}
			}
		}
		player->psprites[ps_flash].sx = player->psprites[ps_weapon].sx;
		player->psprites[ps_flash].sy = player->psprites[ps_weapon].sy;
		if (player->cheats & CF_WEAPONREADY)
		{
			P_CheckWeaponFire (player);
		}
	}
}

FArchive &operator<< (FArchive &arc, pspdef_t &def)
{
	return arc << def.state << def.tics << def.sx << def.sy;
}
