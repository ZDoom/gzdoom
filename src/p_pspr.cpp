
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

// [SO] 1=Weapons states are all 1 tick
//		2=states with a function 1 tick, others 0 ticks.
CVAR(Int, sv_fastweapons, false, CVAR_SERVERINFO);

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static weapontype_t WeaponPrefs[] =
{
	wp_plasma, wp_skullrod, wp_maulerscatter,
	wp_supershotgun, wp_blaster, wp_assaultgun,
	wp_chaingun, wp_crossbow, wp_fhammer, wp_cfire, wp_mlightning, wp_electricxbow,
	wp_shotgun, wp_mace, wp_faxe, wp_cstaff, wp_mfrost, wp_minimissile,
	wp_pistol, wp_goldwand, wp_flamethrower,
	wp_chainsaw, wp_gauntlets, wp_hegrenadelauncher,
	wp_missile, wp_phoenixrod, wp_poisonxbow,
	wp_bfg, wp_fsword, wp_choly, wp_mstaff, wp_phgrenadelauncher, wp_maulertorpedo,
	wp_ffist, wp_cmace, wp_mwand, wp_fist, wp_staff, wp_dagger,
	NUMWEAPONS
};

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
			psp->tics = (state->Action.acvoid == NULL) ? 0 : 1;
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
		if (state->GetAction().acp2)
		{ // Call action routine.
			state->GetAction().acp2 (player->mo, psp);
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
		weapon = wpnlev2info[player->pendingweapon];
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
	player->psprites[ps_weapon].sy = player->cheats & CF_INSTANTWEAPSWITCH
		? WEAPONTOP : WEAPONBOTTOM;
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
	ammotype_t ammo;
	FWeaponInfo **wpinfo;
	int count;

	if (player->powers[pw_weaponlevel2] && !deathmatch)
	{
		wpinfo = wpnlev2info;
	}
	else
	{
		wpinfo = wpnlev1info;
	}
	if (wpinfo[player->readyweapon]->flags & WIF_AMMO_OPTIONAL)
	{
		return true;
	}
	ammo = wpinfo[player->readyweapon]->ammo;
	count = wpinfo[player->readyweapon]->GetMinAmmo();
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
	P_PickNewWeapon (player);
	return false;
}

//---------------------------------------------------------------------------
//
// FUNC P_PickNewWeapon
//
// Changes the player's weapon to something different. Used mostly for
// running out of ammo, but it also works when an ACS script explicitly
// takes the ready weapon away.
//
//---------------------------------------------------------------------------

weapontype_t P_PickNewWeapon (player_t *player)
{
	FWeaponInfo **wpinfo;
	weapontype_t *prefs;

	if (player->powers[pw_weaponlevel2] && !deathmatch)
	{
		wpinfo = wpnlev2info;
	}
	else
	{
		wpinfo = wpnlev1info;
	}

	prefs = WeaponPrefs;
	player->pendingweapon = wp_nochange;
	do
	{
		if (player->weaponowned[*prefs])
		{
			int count = wpinfo[*prefs]->GetMinAmmo();
			ammotype_t ammo = wpinfo[*prefs]->ammo;
			if (wpinfo[*prefs]->flags & WIF_AMMO_OPTIONAL)
			{
				player->pendingweapon = *prefs;
			}
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
	return player->pendingweapon;
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

	// [SO] 9/2/02: People were able to do an awful lot of damage
	// when they were observers...
	if (!player->isbot && bot_observer)
	{
		return;
	}

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
	if (!(wpinfo[player->readyweapon]->flags & WIF_NOALERT))
	{
		P_NoiseAlert (player->mo, player->mo);
	}
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

	FWeaponInfo *weapon;
	fixed_t bobtarget;

	weapon = player->powers[pw_weaponlevel2] ?
		wpnlev2info[player->readyweapon] : wpnlev1info[player->readyweapon];

	if (weapon->flags & WIF_DONTBOB)
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

void A_WeaponReady(AActor *actor, pspdef_t *psp)
{
	FWeaponInfo *weapon;
	player_t *player;

	if (NULL == (player = actor->player))
	{
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
		if (!(weapon->flags & WIF_READYSNDHALF) || pr_wpnreadysnd() < 128)
		{
			S_Sound (actor, CHAN_WEAPON, weapon->readysound, 1, ATTN_NORM);
		}
	}

	// Prepare for bobbing and firing action.
	player->cheats |= CF_WEAPONREADY;
	psp->sx = 0;
	psp->sy = WEAPONTOP;
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
	FWeaponInfo *weapon;

	weapon = player->powers[pw_weaponlevel2] ?
		wpnlev2info[player->readyweapon] : wpnlev1info[player->readyweapon];

	// Put the weapon away if the player has a pending weapon or has
	// died.
	if ((player->morphTics == 0 && player->pendingweapon != wp_nochange) || player->health <= 0)
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
}

//---------------------------------------------------------------------------
//
// PROC A_ReFire
//
// The player can re fire the weapon without lowering it entirely.
//
//---------------------------------------------------------------------------

void A_ReFire (AActor *actor, pspdef_t *psp)
{
	player_t *player;

	if (NULL == (player = actor->player))
	{
		return;
	}
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
// PROC A_CheckReload
//
// Present in Doom, but unused. Also present in Strife, and actually used.
// This and what I call A_XBowReFire are actually the same thing in Strife,
// not two separate functions as I have them here.
//
//---------------------------------------------------------------------------

void A_CheckReload (AActor *actor, pspdef_t *psp)
{
	if (actor->player != NULL)
	{
		P_CheckAmmo (actor->player);
	}
}

//---------------------------------------------------------------------------
//
// PROC A_Lower
//
//---------------------------------------------------------------------------

void A_Lower (AActor *actor, pspdef_t *psp)
{
	player_t *player;

	if (NULL == (player = actor->player))
	{
		return;
	}
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
	if (player->pendingweapon == wp_nochange)
	{ // [RH] Make sure we're actually changing weapons.
		player->pendingweapon = player->readyweapon;
	}
	else
	{
		player->readyweapon = player->pendingweapon;
		// [RH] Clear the flash state. Only needed for Strife.
		P_SetPsprite (player, ps_flash, NULL);
	}
	P_BringUpWeapon (player);
}

//---------------------------------------------------------------------------
//
// PROC A_Raise
//
//---------------------------------------------------------------------------

void A_Raise (AActor *actor, pspdef_t *psp)
{
	player_t *player;

	if (NULL == (player = actor->player))
	{
		return;
	}
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
void A_GunFlash (AActor *actor, pspdef_t *psp)
{
	player_t *player;

	if (NULL == (player = actor->player))
	{
		return;
	}
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
		
	damage = 5*(pr_gunshot()%3+1);
	angle = mo->angle;

	if (!accurate)
	{
		angle += pr_gunshot.Random2 () << 18;
	}

	P_LineAttack (mo, angle, PLAYERMISSILERANGE, bulletpitch, damage);
}

void A_Light0 (AActor *actor, pspdef_t *psp)
{
	if (actor->player != NULL)
	{
		actor->player->extralight = 0;
	}
}

void A_Light1 (AActor *actor, pspdef_t *psp)
{
	if (actor->player != NULL)
	{
		actor->player->extralight = 1;
	}
}

void A_Light2 (AActor *actor, pspdef_t *psp)
{
	if (actor->player != NULL)
	{
		actor->player->extralight = 2;
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

FArchive &operator<< (FArchive &arc, pspdef_t &def)
{
	return arc << def.state << def.tics << def.sx << def.sy;
}
