
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

// MACROS ------------------------------------------------------------------

#define LOWERSPEED				FRACUNIT*6
#define RAISESPEED				FRACUNIT*6

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

angle_t bulletpitch;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static weapontype_t DoomWeaponPrefs[] =
{
	wp_plasma, wp_supershotgun, wp_chaingun, wp_shotgun,
	wp_pistol, wp_chainsaw, wp_missile, wp_bfg, wp_fist,
	NUMWEAPONS
};

static weapontype_t HereticWeaponPrefs[] =
{
	wp_skullrod, wp_blaster, wp_crossbow, wp_mace,
	wp_goldwand, wp_gauntlets, wp_phoenixrod, wp_staff,
	NUMWEAPONS
};

static weapontype_t HexenWeaponPrefs[] =
{
	wp_fhammer, wp_faxe, wp_fsword, wp_ffist,
	wp_cfire, wp_cstaff, wp_choly, wp_cmace,
	wp_mlightning, wp_mfrost, wp_mstaff, wp_mwand,
	NUMWEAPONS
};

// CODE --------------------------------------------------------------------

//---------------------------------------------------------------------------
//
// PROC P_SetPsprite
//
//---------------------------------------------------------------------------

void P_SetPsprite (player_t *player, int position, FState *state)
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
		psp->tics = state->tics; // could be 0
		if (state->misc1)
		{ // Set coordinates.
			psp->sx = state->misc1<<FRACBITS;
		}
		if (state->misc2)
		{
			psp->sy = state->misc2<<FRACBITS;
		}
		if (state->action.acp2)
		{ // Call action routine.
			state->action.acp2 (player, psp);
			if (!psp->state)
			{
				break;
			}
		}
		state = psp->state->nextstate;
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
		psp->tics = state->tics; // could be 0
		if (state->misc1)
		{ // Set coordinates.
			psp->sx = state->misc1<<FRACBITS;
		}
		if (state->misc2)
		{
			psp->sy = state->misc2<<FRACBITS;
		}
		state = psp->state->nextstate;
	} while (!psp->tics); // An initial state of 0 could cycle through.
}

/*
=================
=
= P_CalcSwing
=
=================
*/

fixed_t swingx, swingy;

void P_CalcSwing (player_t *player)
{
	fixed_t swing;
	int angle;
		
	swing = player->bob;

	angle = (FINEANGLES/(TICRATE*2)*level.time)&FINEMASK;
	swingx = FixedMul (swing, finesine[angle]);

	angle = (FINEANGLES/(TICRATE*2)*level.time+FINEANGLES/2)&FINEMASK;
	swingy = -FixedMul (swingx, finesine[angle]);
}

//---------------------------------------------------------------------------
//
// PROC P_ActivateMorphWeapon
//
//---------------------------------------------------------------------------

void P_ActivateMorphWeapon (player_t *player)
{
	player->pendingweapon = wp_nochange;
	player->psprites[ps_weapon].sy = WEAPONTOP;
	player->readyweapon = (gameinfo.gametype == GAME_Heretic ? wp_beak : wp_snout);
	P_SetPsprite (player, ps_weapon, wpnlev1info[player->readyweapon]->readystate);
}


//---------------------------------------------------------------------------
//
// PROC P_PostMorphWeapon
//
//---------------------------------------------------------------------------

void P_PostMorphWeapon (player_t *player, weapontype_t weapon)
{
	player->pendingweapon = wp_nochange;
	player->readyweapon = weapon;
	player->psprites[ps_weapon].sy = WEAPONBOTTOM;
	P_SetPsprite (player, ps_weapon, wpnlev1info[weapon]->upstate);
}

//---------------------------------------------------------------------------
//
// PROC P_BringUpWeapon
//
// Starts bringing the pending weapon up from the bottom of the screen.
//
//---------------------------------------------------------------------------

void P_BringUpWeapon (player_t *player)
{
	FState *newstate;
	FWeaponInfo *weapon;

	if (player->pendingweapon == wp_nochange)
	{
		player->pendingweapon = player->readyweapon;
	}
	if (player->powers[pw_weaponlevel2])
	{
		weapon = wpnlev1info[player->pendingweapon];
	}
	else
	{
		weapon = wpnlev1info[player->pendingweapon];
	}

	if (weapon)
	{
		if (weapon->upsound)
		{
			S_Sound (player->mo, CHAN_WEAPON, weapon->upsound, 1, ATTN_NORM);
		}
		if (player->pendingweapon == wp_faxe && player->ammo[MANA_1])
		{
			newstate = wpnlev2info[wp_faxe]->upstate;
		}
		else
		{
			newstate = weapon->upstate;
		}
	}
	else
	{
		newstate = NULL;
	}
	player->pendingweapon = wp_nochange;
	player->psprites[ps_weapon].sy = WEAPONBOTTOM;
	P_SetPsprite (player, ps_weapon, newstate);
}

//---------------------------------------------------------------------------
//
// FUNC P_CheckAmmo
//
// Returns true if there is enough ammo to shoot.  If not, selects the
// next weapon to use.
//
//---------------------------------------------------------------------------

bool P_CheckAmmo (player_t *player)
{
	weapontype_t *prefs;
	ammotype_t ammo;
	FWeaponInfo **wpinfo;
	int count;

	if (player->powers[pw_weaponlevel2] && !*deathmatch)
	{
		wpinfo = wpnlev2info;
	}
	else
	{
		wpinfo = wpnlev1info;
	}
	ammo = wpinfo[player->readyweapon]->ammo;
	count = wpinfo[player->readyweapon]->ammouse;
	if (ammo == MANA_BOTH)
	{
		if (player->ammo[MANA_1] >= count && player->ammo[MANA_2] >= count)
		{
			return true;
		}
	}
	else if (ammo == am_noammo || player->ammo[ammo] >= count)
	{
		return true;
	}
	// out of ammo, pick a weapon to change to
	if (gameinfo.gametype == GAME_Hexen)
	{
		prefs = HexenWeaponPrefs;
	}
	else if (gameinfo.gametype == GAME_Heretic)
	{
		prefs = HereticWeaponPrefs;
	}
	else
	{
		prefs = DoomWeaponPrefs;
	}
	player->pendingweapon = wp_nochange;
	do
	{
		if (player->weaponowned[*prefs])
		{
			count = wpinfo[*prefs]->ammouse;
			ammo = wpinfo[*prefs]->ammo;
			if (ammo == MANA_BOTH)
			{
				if (player->ammo[MANA_1] >= count && player->ammo[MANA_2] >= count)
				{
					player->pendingweapon = *prefs;
				}
			}
			else if (ammo == am_noammo || player->ammo[ammo] >= count)
			{
				player->pendingweapon = *prefs;
			}
		}
		prefs++;
	} while (*prefs < NUMWEAPONS && player->pendingweapon == wp_nochange);
	if (player->pendingweapon != wp_nochange)
	{
		if (player->powers[pw_weaponlevel2])
		{
			P_SetPsprite (player, ps_weapon,
				wpnlev2info[player->readyweapon]->downstate);
		}
		else
		{
			P_SetPsprite (player, ps_weapon,
				wpnlev1info[player->readyweapon]->downstate);
		}
	}
	return false;
}


//---------------------------------------------------------------------------
//
// PROC P_FireWeapon
//
//---------------------------------------------------------------------------

void P_FireWeapon (player_t *player)
{
	FWeaponInfo **wpinfo;
	FState *attackState;

	if (!P_CheckAmmo (player))
	{
		return;
	}
	if (gameinfo.gametype == GAME_Heretic)
	{
		player->mo->PlayAttacking2 ();
	}
	else
	{
		player->mo->PlayAttacking ();
	}
	wpinfo = player->powers[pw_weaponlevel2] ? wpnlev2info : wpnlev1info;
	if (player->readyweapon == wp_faxe && player->ammo[MANA_1] > 0)
	{ // Glowing axe
		attackState = wpnlev2info[wp_faxe]->atkstate;
	}
	else
	{
		attackState = player->refire ? 
			wpinfo[player->readyweapon]->holdatkstate
			: wpinfo[player->readyweapon]->atkstate;
	}
	P_SetPsprite (player, ps_weapon, attackState);
	P_NoiseAlert (player->mo, player->mo);
	if (player->readyweapon == wp_gauntlets && !player->refire)
	{ // Play the sound for the initial gauntlet attack
		S_Sound (player->mo, CHAN_WEAPON, "weapons/gauntletsuse", 1, ATTN_NORM);
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
	if (player->powers[pw_weaponlevel2])
	{
		P_SetPsprite (player, ps_weapon,
			wpnlev2info[player->readyweapon]->downstate);
	}
	else
	{
		P_SetPsprite (player, ps_weapon,
			wpnlev1info[player->readyweapon]->downstate);
	}
}

//---------------------------------------------------------------------------
//
// PROC A_WeaponReady
//
// The player can fire the weapon or change to another weapon at this time.
//
//---------------------------------------------------------------------------

void A_WeaponReady(player_t *player, pspdef_t *psp)
{
	FWeaponInfo *weapon;
	int angle;

	if (gameinfo.gametype == GAME_Heretic && player->morphTics)
	{ // Change to the chicken beak
		P_ActivateMorphWeapon (player);
		return;
	}

	weapon = player->powers[pw_weaponlevel2] ?
		wpnlev2info[player->readyweapon] : wpnlev1info[player->readyweapon];

	// Change player from attack state
	if (player->mo->state >= player->mo->MissileState &&
		player->mo->state < player->mo->PainState)
	{
		player->mo->PlayIdle ();
	}

	// Play ready sound, if any.
	if (weapon->readysound && psp->state == weapon->readystate)
	{
		if (!(weapon->flags & WIF_READYSNDHALF) || P_Random (pr_wpnreadysnd) < 128)
		{
			S_Sound (player->mo, CHAN_WEAPON, weapon->readysound, 1, ATTN_NORM);
		}
	}
	
	// Put the weapon away if the player has a pending weapon or has
	// died.
	if (player->pendingweapon != wp_nochange || player->health <= 0)
	{
		P_SetPsprite (player, ps_weapon, weapon->downstate);
		return;
	}

	// Check for fire. Some weapons do not auto fire.
	if (player->cmd.ucmd.buttons & BT_ATTACK)
	{
		if (!(player->oldbuttons & BT_ATTACK) || !(weapon->flags & WIF_NOAUTOFIRE))
		{
			player->oldbuttons |= BT_ATTACK;
			P_FireWeapon (player);
			return;
		}
	}

	if (!(weapon->flags & WIF_DONTBOB))
	{
		// Bob the weapon based on movement speed.
		angle = (128*level.time)&FINEMASK;
		psp->sx = FRACUNIT + FixedMul(player->bob, finecosine[angle]);
		angle &= FINEANGLES/2-1;
		psp->sy = WEAPONTOP + FixedMul(player->bob, finesine[angle]);
	}
}

//---------------------------------------------------------------------------
//
// PROC A_ReFire
//
// The player can re fire the weapon without lowering it entirely.
//
//---------------------------------------------------------------------------

void A_ReFire (player_t *player, pspdef_t *psp)
{
	if ((player->cmd.ucmd.buttons&BT_ATTACK)
		&& player->pendingweapon == wp_nochange && player->health)
	{
		player->refire++;
		P_FireWeapon (player);
	}
	else
	{
		player->refire = 0;
		P_CheckAmmo (player);
	}
}

//---------------------------------------------------------------------------
//
// PROC A_Lower
//
//---------------------------------------------------------------------------

void A_Lower (player_t *player, pspdef_t *psp)
{
	if (player->morphTics)
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
	player->readyweapon = player->pendingweapon;
	P_BringUpWeapon (player);
}

//---------------------------------------------------------------------------
//
// PROC A_Raise
//
//---------------------------------------------------------------------------

void A_Raise (player_t *player, pspdef_t *psp)
{
	psp->sy -= RAISESPEED;
	if (psp->sy > WEAPONTOP)
	{ // Not raised all the way yet
		return;
	}
	psp->sy = WEAPONTOP;
	if (player->powers[pw_weaponlevel2] ||
		(player->readyweapon == wp_faxe && player->ammo[MANA_1]))
	{
		P_SetPsprite (player, ps_weapon,
			wpnlev2info[player->readyweapon]->readystate);
	}
	else
	{
		P_SetPsprite (player, ps_weapon,
			wpnlev1info[player->readyweapon]->readystate);
	}
}




//
// A_GunFlash
//
void A_GunFlash (player_t *player, pspdef_t *psp)
{
	player->mo->PlayAttacking2 ();
	P_SetPsprite (player, ps_flash, wpnlev1info[player->readyweapon]->flashstate);
}



//
// WEAPON ATTACKS
//

//
// P_BulletSlope
// Sets a slope so a near miss is at aproximately
// the height of the intended target
//

void P_BulletSlope (AActor *mo)
{
	static const int angdiff[3] = { -1<<26, 1<<26, 0 };
	int i;
	angle_t an;

	// see which target is to be aimed at
	i = 2;
	do
	{
		an = mo->angle + angdiff[i];
		bulletpitch = P_AimLineAttack (mo, an, 16*64*FRACUNIT);
	} while (linetarget == NULL && --i >= 0);
}


//
// P_GunShot
//
void P_GunShot (AActor *mo, BOOL accurate)
{
	angle_t 	angle;
	int 		damage;
		
	damage = 5*(P_Random (pr_gunshot)%3+1);
	angle = mo->angle;

	if (!accurate)
	{
		angle += PS_Random (pr_gunshot) << 18;
	}

	P_LineAttack (mo, angle, MISSILERANGE, bulletpitch, damage);
}

void A_Light0 (player_t *player, pspdef_t *psp)
{
	player->extralight = 0;
}

void A_Light1 (player_t *player, pspdef_t *psp)
{
	player->extralight = 1;
}

void A_Light2 (player_t *player, pspdef_t *psp)
{
	player->extralight = 2;
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
	player->pendingweapon = player->readyweapon;
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
					P_SetPsprite (player, i, psp->state->nextstate);
				}
			}
		}
	}
	player->psprites[ps_flash].sx = player->psprites[ps_weapon].sx;
	player->psprites[ps_flash].sy = player->psprites[ps_weapon].sy;
}

FArchive &operator<< (FArchive &arc, pspdef_t &def)
{
	return arc << def.state << def.tics << def.sx << def.sy;
}
