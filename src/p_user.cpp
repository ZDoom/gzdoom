// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//		Player related stuff.
//		Bobbing POV/weapon, movement.
//		Pending weapon.
//
//-----------------------------------------------------------------------------

#include "templates.h"
#include "doomdef.h"
#include "d_event.h"
#include "p_local.h"
#include "doomstat.h"
#include "s_sound.h"
#include "i_system.h"
#include "r_draw.h"
#include "gi.h"
#include "m_random.h"
#include "p_pspr.h"
#include "p_enemy.h"
#include "p_effect.h"
#include "s_sound.h"
#include "a_sharedglobal.h"
#include "statnums.h"
#include "v_palette.h"
#include "v_video.h"

static FRandom pr_morphplayerthink ("MorphPlayerThink");
static FRandom pr_torch ("Torch");
static FRandom pr_healradius ("HealRadius");

extern void P_UpdateBeak (player_t *, pspdef_t *);

// [RH] # of ticks to complete a turn180
#define TURN180_TICKS	9

//
// Movement.
//

// 16 pixels of bob
#define MAXBOB			0x100000

BOOL onground;
int newtorch; // used in the torch flicker effect.
int newtorchdelta;


// This function supplements the pointer cleanup in dobject.cpp, because
// player_s is not derived from DObject. (I tried it, and DestroyScan was
// unable to properly determine the player object's type--possibly
// because it gets staticly allocated in an array.)
//
// This function checks all the DObject pointers in a player_s and NULLs any
// that match the pointer passed in. If you add any pointers that point to
// DObject (or a subclass), add them here too.

void player_s::FixPointers (const DObject *old, DObject *rep)
{
	APlayerPawn *replacement = static_cast<APlayerPawn *>(rep);
	if (mo == old)			mo = replacement;
	if (poisoner == old)	poisoner = replacement;
	if (attacker == old)	attacker = replacement;
	if (rain1 == old)		rain1 = replacement;
	if (rain2 == old)		rain2 = replacement;
	if (camera == old)		camera = replacement;
	if (dest == old)		dest = replacement;
	if (prev == old)		prev = replacement;
	if (enemy == old)		enemy = replacement;
	if (missile == old)		missile = replacement;
	if (mate == old)		mate = replacement;
	if (last_mate == old)	last_mate = replacement;
}

// Reduce the ammo used by the current weapon. Returns true
// if there was enough ammo to use.
bool player_s::UseAmmo (bool doCheck)
{
	if (!(dmflags & DF_INFINITE_AMMO))
	{
		FWeaponInfo *info;

		if (deathmatch || !powers[pw_weaponlevel2])
			info = wpnlev1info[readyweapon];
		else
			info = wpnlev2info[readyweapon];
		if (info->ammo < NUMAMMO)
		{
			if (doCheck && ammo[info->ammo] < info->ammouse)
				return false;
			ammo[info->ammo] -= info->ammouse;
		}
		else if (info->ammo == MANA_BOTH)
		{
			if (doCheck)
			{
				if (ammo[MANA_1] < info->ammouse || ammo[MANA_2] < info->ammouse)
					return false;
			}
			ammo[MANA_1] -= info->ammouse;
			ammo[MANA_2] -= info->ammouse;
		}
	}
	return true;
}

IMPLEMENT_ABSTRACT_ACTOR (APlayerPawn)
IMPLEMENT_ABSTRACT_ACTOR (APlayerChunk)

void APlayerPawn::BeginPlay ()
{
	Super::BeginPlay ();
	ChangeStatNum (STAT_PLAYER);
}

const char *APlayerPawn::GetSoundClass ()
{
	if (player != NULL &&
		player->userinfo.skin != 0 &&
		(unsigned)player->userinfo.skin < numskins)
	{
		return skins[player->userinfo.skin].name;
	}
	return "player";
}

void APlayerPawn::PlayIdle ()
{
	if (state >= SeeState && state < MissileState)
		SetState (SpawnState);
}

void APlayerPawn::PlayRunning ()
{
	if (state == SpawnState)
		SetState (SeeState);
}

void APlayerPawn::PlayAttacking ()
{
	SetState (MissileState);
}

void APlayerPawn::PlayAttacking2 ()
{
	SetState (MissileState+1);
}

void APlayerPawn::ThrowPoisonBag ()
{
}

void APlayerPawn::GiveDefaultInventory ()
{
}

int APlayerPawn::GetAutoArmorSave ()
{
	return 0;
}

int APlayerPawn::GetArmorMax ()
{
	return 0;
}

fixed_t APlayerPawn::GetArmorIncrement (int armortype)
{
	return 10*FRACUNIT;
}

fixed_t APlayerPawn::GetJumpZ ()
{
	return 8*FRACUNIT;
}

const TypeInfo *APlayerPawn::GetDropType ()
{
	if (player == NULL ||
		player->readyweapon >= NUMWEAPONS)
	{
		return NULL;
	}
	else
	{
		return wpnlev1info[player->readyweapon]->droptype;
	}
}

void APlayerPawn::NoBlockingSet ()
{
	if (dmflags2 & DF2_YES_WEAPONDROP)
	{
		const TypeInfo *droptype = GetDropType ();
		if (droptype)
		{
			P_DropItem (this, droptype, -1, 256);
		}
	}
}

void APlayerPawn::TweakSpeeds (int &forward, int &side)
{
	if (player->powers[pw_speed] && !player->morphTics)
	{ // Adjust for a player with a speed artifact
		forward = (3*forward)>>1;
		side = (3*side)>>1;
	}
}

// The standard healing radius behavior is the cleric's
bool APlayerPawn::DoHealingRadius (APlayerPawn *other)
{
	if (P_GiveBody (other->player, 50 + (pr_healradius()%50)))
	{
		S_Sound (other, CHAN_AUTO, "MysticIncant", 1, ATTN_NORM);
		return true;
	}
	return false;
}

void APlayerPawn::SpecialInvulnerabilityHandling (EInvulState setting)
{
}

class APlayerSpeedTrail : public AActor
{
	DECLARE_STATELESS_ACTOR (APlayerSpeedTrail, AActor)
public:
	void Tick ();
};

IMPLEMENT_STATELESS_ACTOR (APlayerSpeedTrail, Any, -1, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_Alpha (FRACUNIT*6/10)
	PROP_RenderStyle (STYLE_Translucent)
END_DEFAULTS

void APlayerSpeedTrail::Tick ()
{
	const int fade = OPAQUE*6/10/8;
	if (alpha <= fade)
	{
		Destroy ();
		return;
	}
	alpha -= fade;
}

/*
==================
=
= P_Thrust
=
= moves the given origin along a given angle
=
==================
*/

void P_SideThrust (player_t *player, angle_t angle, fixed_t move)
{
	angle = (angle - ANGLE_90) >> ANGLETOFINESHIFT;

	player->mo->momx += FixedMul (move, finecosine[angle]);
	player->mo->momy += FixedMul (move, finesine[angle]);
}

void P_ForwardThrust (player_t *player, angle_t angle, fixed_t move)
{
	angle >>= ANGLETOFINESHIFT;

	if ((player->mo->waterlevel || (player->mo->flags2 & MF2_FLY))
		&& player->mo->pitch != 0)
	{
		angle_t pitch = (angle_t)player->mo->pitch >> ANGLETOFINESHIFT;
		fixed_t zpush = FixedMul (move, finesine[pitch]);
		if (player->mo->waterlevel && player->mo->waterlevel < 2 && zpush < 0)
			zpush = 0;
		player->mo->momz -= zpush;
		move = FixedMul (move, finecosine[pitch]);
	}
	player->mo->momx += FixedMul (move, finecosine[angle]);
	player->mo->momy += FixedMul (move, finesine[angle]);
}

//
// P_Bob
// Same as P_Thrust, but only affects bobbing.
//
// killough 10/98: We apply thrust separately between the real physical player
// and the part which affects bobbing. This way, bobbing only comes from player
// motion, nothing external, avoiding many problems, e.g. bobbing should not
// occur on conveyors, unless the player walks on one, and bobbing should be
// reduced at a regular rate, even on ice (where the player coasts).
//

void P_Bob (player_t *player, angle_t angle, fixed_t move)
{
	angle >>= ANGLETOFINESHIFT;

	player->momx += FixedMul(move,finecosine[angle]);
	player->momy += FixedMul(move,finesine[angle]);
}

/*
==================
=
= P_CalcHeight
=
=
Calculate the walking / running height adjustment
=
==================
*/

void P_CalcHeight (player_t *player) 
{
	int 		angle;
	fixed_t 	bob;
	bool		still = false;

	// Regular movement bobbing
	// (needs to be calculated for gun swing even if not on ground)
	// OPTIMIZE: tablify angle

	// killough 10/98: Make bobbing depend only on player-applied motion.
	//
	// Note: don't reduce bobbing here if on ice: if you reduce bobbing here,
	// it causes bobbing jerkiness when the player moves from ice to non-ice,
	// and vice-versa.

	if ((player->mo->flags2 & MF2_FLY) && !onground)
	{
		player->bob = FRACUNIT / 2;
	}
	else
	{
		player->bob = DMulScale16 (player->momx, player->momx, player->momy, player->momy);
		if (player->bob == 0)
		{
			still = true;
		}
		else
		{
			player->bob = FixedMul (player->bob, player->userinfo.MoveBob);

			if (player->bob > MAXBOB)
				player->bob = MAXBOB;
		}
	}

	if (player->cheats & CF_NOMOMENTUM)
	{
		player->viewz = player->mo->z + VIEWHEIGHT;

		if (player->viewz > player->mo->ceilingz-4*FRACUNIT)
			player->viewz = player->mo->ceilingz-4*FRACUNIT;

		return;
	}

	if (still)
	{
		if (player->health > 0)
		{
			angle = DivScale13 (level.time, 120*TICRATE/35) & FINEMASK;
			bob = FixedMul (player->userinfo.StillBob, finesine[angle]);
		}
		else
		{
			bob = 0;
		}
	}
	else
	{
		// DivScale 13 because FINEANGLES == (1<<13)
		angle = DivScale13 (level.time, 20*TICRATE/35) & FINEMASK;
		bob = FixedMul (player->bob>>(player->mo->waterlevel > 1 ? 2 : 1), finesine[angle]);
	}

	// move viewheight
	if (player->playerstate == PST_LIVE)
	{
		player->viewheight += player->deltaviewheight;

		if (player->viewheight > VIEWHEIGHT)
		{
			player->viewheight = VIEWHEIGHT;
			player->deltaviewheight = 0;
		}
		else if (player->viewheight < VIEWHEIGHT/2)
		{
			player->viewheight = VIEWHEIGHT/2;
			if (player->deltaviewheight <= 0)
				player->deltaviewheight = 1;
		}
		
		if (player->deltaviewheight)	
		{
			player->deltaviewheight += FRACUNIT/4;
			if (!player->deltaviewheight)
				player->deltaviewheight = 1;
		}
	}

	if (player->morphTics)
	{
		player->viewz = player->mo->z + player->viewheight - (20 * FRACUNIT);
	}
	else
	{
		player->viewz = player->mo->z + player->viewheight + bob;
	}
	if (player->mo->floorclip && player->playerstate != PST_DEAD
		&& player->mo->z <= player->mo->floorz)
	{
		player->viewz -= player->mo->floorclip;
	}
	if (player->viewz > player->mo->ceilingz - 4*FRACUNIT)
	{
		player->viewz = player->mo->ceilingz - 4*FRACUNIT;
	}
	if (player->viewz < player->mo->floorz + 4*FRACUNIT)
	{
		player->viewz = player->mo->floorz + 4*FRACUNIT;
	}
}

/*
=================
=
= P_MovePlayer
=
=================
*/
CUSTOM_CVAR (Float, sv_aircontrol, 0.00390625f, CVAR_SERVERINFO|CVAR_NOSAVE)
{
	level.aircontrol = (fixed_t)(self * 65536.f);
	G_AirControlChanged ();
}

void P_MovePlayer (player_t *player)
{
	ticcmd_t *cmd = &player->cmd;
	APlayerPawn *mo = player->mo;

	// [RH] 180-degree turn overrides all other yaws
	if (player->turnticks)
	{
		player->turnticks--;
		mo->angle += (ANGLE_180 / TURN180_TICKS);
	}
	else
	{
		mo->angle += cmd->ucmd.yaw << 16;
	}

	onground = (mo->z <= mo->floorz) || (mo->flags2 & MF2_ONMOBJ);

	// killough 10/98:
	//
	// We must apply thrust to the player and bobbing separately, to avoid
	// anomalies. The thrust applied to bobbing is always the same strength on
	// ice, because the player still "works just as hard" to move, while the
	// thrust applied to the movement varies with 'movefactor'.

	if (cmd->ucmd.forwardmove | cmd->ucmd.sidemove)
	{
		fixed_t forwardmove, sidemove;
		int bobfactor;
		int friction, movefactor;
		int fm, sm;

		movefactor = P_GetMoveFactor (mo, &friction);
		if (player->morphTics && gameinfo.gametype == GAME_Heretic)
		{ // Chicken speed
			movefactor = movefactor * 2500 / 2048;
		}
		bobfactor = friction < ORIG_FRICTION ? movefactor : ORIG_FRICTION_FACTOR;
		if (!onground && !(player->mo->flags2 & MF2_FLY) && !player->mo->waterlevel)
		{
			// [RH] allow very limited movement if not on ground.
			movefactor = FixedMul (movefactor, level.aircontrol);
			bobfactor = FixedMul (bobfactor, level.aircontrol);
		}

		fm = cmd->ucmd.forwardmove;
		sm = cmd->ucmd.sidemove;
		mo->TweakSpeeds (fm, sm);

		forwardmove = Scale (fm, movefactor * 35, TICRATE << 8);
		sidemove = Scale (sm, movefactor * 35, TICRATE << 8);

		if (forwardmove)
		{
			P_Bob (player, mo->angle, (cmd->ucmd.forwardmove * bobfactor) >> 8);
			P_ForwardThrust (player, mo->angle, forwardmove);
		}
		if (sidemove)
		{
			P_Bob (player, mo->angle-ANG90, (cmd->ucmd.sidemove * bobfactor) >> 8);
			P_SideThrust (player, mo->angle, sidemove);
		}

		player->mo->PlayRunning ();

		if (player->cheats & CF_REVERTPLEASE)
		{
			player->cheats &= ~CF_REVERTPLEASE;
			player->camera = player->mo;
		}
	}
}		

//==========================================================================
//
// P_FallingDamage
//
//==========================================================================

void P_FallingDamage (AActor *actor)
{
	int damagestyle;
	int damage;
	fixed_t mom;

	damagestyle = ((level.flags >> 15) | (dmflags)) &
		(DF_FORCE_FALLINGZD | DF_FORCE_FALLINGHX);

	if (damagestyle == 0)
		return;

	mom = abs (actor->momz);

	// Since Hexen falling damage is stronger than ZDoom's, it takes
	// precedence. ZDoom falling damage may not be as strong, but it
	// gets felt sooner.

	if (damagestyle & DF_FORCE_FALLINGHX)
	{ // Hexen falling damage

		if (mom <= 23*FRACUNIT)
		{ // Not fast enough to hurt
			return;
		}
		if (mom >= 63*FRACUNIT)
		{ // automatic death
			damage = 10000;
		}
		else
		{
			mom = FixedMul (mom, 16*FRACUNIT/23);
			damage = ((FixedMul (mom, mom) / 10) >> FRACBITS) - 24;
			if (actor->momz > -39*FRACUNIT && damage > actor->health
				&& actor->health != 1)
			{ // No-death threshold
				damage = actor->health-1;
			}
		}
	}
	else
	{ // ZDoom falling damage
		if (mom <= 19*FRACUNIT)
		{ // Not fast enough to hurt
			return;
		}
		if (mom >= 84*FRACUNIT)
		{ // automatic death
			damage = 10000;
		}
		else
		{
			damage = ((MulScale23 (mom, mom*11) >> FRACBITS) - 30) / 2;
			if (damage < 1)
			{
				damage = 1;
			}
		}
	}

	if (actor->player)
	{
		S_Sound (actor, CHAN_AUTO, "*land", 1, ATTN_NORM);
		P_NoiseAlert (actor, actor);
		if (damage == 10000 && (actor->player->cheats & CF_GODMODE))
		{
			damage = 999;
		}
	}
	P_DamageMobj (actor, NULL, NULL, damage, MOD_FALLING);
}

//==========================================================================
//
// P_DeathThink
//
//==========================================================================

void P_DeathThink (player_t *player)
{
	int dir;
	angle_t delta;
	int lookDelta;

	P_MovePsprites (player);

	onground = (player->mo->z <= player->mo->floorz);
	if (player->mo->IsKindOf (RUNTIME_CLASS(APlayerChunk)))
	{ // Flying bloody skull or flying ice chunk
		player->viewheight = 6 * FRACUNIT;
		player->deltaviewheight = 0;
		if (onground)
		{
			if (player->mo->pitch > -(int)ANGLE_1*19)
			{
				lookDelta = (-(int)ANGLE_1*19 - player->mo->pitch) / 8;
				player->mo->pitch += lookDelta;
			}
		}
	}
	else if (!(player->mo->flags2 & MF2_ICEDAMAGE))
	{ // Fall to ground (if not frozen)
		player->deltaviewheight = 0;
		if (player->viewheight > 6*FRACUNIT)
		{
			player->viewheight -= FRACUNIT;
		}
		if (player->viewheight < 6*FRACUNIT)
		{
			player->viewheight = 6*FRACUNIT;
		}
		if (player->mo->pitch < 0)
		{
			player->mo->pitch += ANGLE_1*3;
		}
		else if (player->mo->pitch > 0)
		{
			player->mo->pitch -= ANGLE_1*3;
		}
		if (abs(player->mo->pitch) < ANGLE_1*3)
		{
			player->mo->pitch = 0;
		}
	}
	P_CalcHeight (player);
		
	if (player->attacker && player->attacker != player->mo)
	{ // Watch killer
		dir = P_FaceMobj (player->mo, player->attacker, &delta);
		if (delta < ANGLE_1*10)
		{ // Looking at killer, so fade damage and poison counters
			if (player->damagecount)
			{
				player->damagecount--;
			}
			if (player->poisoncount)
			{
				player->poisoncount--;
			}
		}
		delta /= 8;
		if (delta > ANGLE_1*5)
		{
			delta = ANGLE_1*5;
		}
		if (dir)
		{ // Turn clockwise
			player->mo->angle += delta;
		}
		else
		{ // Turn counter clockwise
			player->mo->angle -= delta;
		}
	}
	else
	{
		if (player->damagecount)
		{
			player->damagecount--;
		}
		if (player->poisoncount)
		{
			player->poisoncount--;
		}
	}		

	if (player->cmd.ucmd.buttons & BT_USE ||
		((deathmatch || alwaysapplydmflags) && (dmflags & DF_FORCE_RESPAWN)))
	{
		if (level.time >= player->respawn_time || ((player->cmd.ucmd.buttons & BT_USE) && !player->isbot))
		{
			player->cls = NULL;		// Force a new class if the player is using a random class
			player->playerstate = multiplayer ? PST_REBORN : PST_ENTER;
			if (player->mo->special1 > 2)
			{
				player->mo->special1 = 0;
			}
		}
	}
}


//----------------------------------------------------------------------------
//
// PROC P_MorphPlayerThink
//
//----------------------------------------------------------------------------

void P_MorphPlayerThink (player_t *player)
{
	APlayerPawn *pmo;

	if (gameinfo.gametype == GAME_Heretic)
	{ // Chicken think
		if (player->health > 0)
		{ // Handle beak movement
			P_UpdateBeak (player, &player->psprites[ps_weapon]);
		}
		if (player->morphTics & 15)
		{
			return;
		}
		pmo = player->mo;
		if (!(pmo->momx + pmo->momy) && pr_morphplayerthink () < 160)
		{ // Twitch view angle
			pmo->angle += pr_morphplayerthink.Random2 () << 19;
		}
		if ((pmo->z <= pmo->floorz) && (pr_morphplayerthink() < 32))
		{ // Jump and noise
			pmo->momz += pmo->GetJumpZ ();
			pmo->SetState (pmo->PainState);
		}
		if (pr_morphplayerthink () < 48)
		{ // Just noise
			S_Sound (pmo, CHAN_VOICE, "chicken/active", 1, ATTN_NORM);
		}
	}
	else
	{ // Pig think
		if (player->morphTics&15)
		{
			return;
		}
		pmo = player->mo;
		if(!(pmo->momx + pmo->momy) && pr_morphplayerthink() < 64)
		{ // Snout sniff
			P_SetPspriteNF (player, ps_weapon, wpnlev1info[wp_snout]->atkstate + 1);
			S_Sound (pmo, CHAN_VOICE, "PigActive1", 1, ATTN_NORM); // snort
			return;
		}
		if (pr_morphplayerthink() < 48)
		{
			S_Sound (pmo, CHAN_VOICE, "PigActive", 1, ATTN_NORM);
		}
	}
}

//----------------------------------------------------------------------------
//
// PROC P_PlayerThink
//
//----------------------------------------------------------------------------

void P_PlayerThink (player_t *player)
{
	ticcmd_t *cmd;

	if (player->mo == NULL)
	{
		I_Error ("No player %d start\n", player - players + 1);
	}

	player->xviewshift = 0;		// [RH] Make sure view is in right place

	// [RH] Zoom the player's FOV
	if (player->FOV != player->DesiredFOV)
	{
		if (fabsf (player->FOV - player->DesiredFOV) < 7.f)
		{
			player->FOV = player->DesiredFOV;
		}
		else
		{
			float zoom = MAX(7.f, fabsf (player->FOV - player->DesiredFOV) * 0.025f);
			if (player->FOV > player->DesiredFOV)
			{
				player->FOV = player->FOV - zoom;
			}
			else
			{
				player->FOV = player->FOV + zoom;
			}
		}
	}
	if (player->inventorytics)
	{
		player->inventorytics--;
	}
	// No-clip cheat
	if (player->cheats & CF_NOCLIP)
	{
		player->mo->flags |= MF_NOCLIP;
	}
	else
	{
		player->mo->flags &= ~MF_NOCLIP;
	}
	if (player->cheats & CF_FLY)
	{
		player->mo->flags |= MF_NOGRAVITY, player->mo->flags2 |= MF2_FLY;
	}
	else if (player->powers[pw_flight] == 0)
	{
		player->mo->flags &= ~MF_NOGRAVITY, player->mo->flags2 &= ~MF2_FLY;
	}
	cmd = &player->cmd;
	if (player->mo->flags & MF_JUSTATTACKED)
	{ // Chainsaw/Gauntlets attack auto forward motion
		cmd->ucmd.yaw = 0;
		cmd->ucmd.forwardmove = 0xc800/2;
		cmd->ucmd.sidemove = 0;
		player->mo->flags &= ~MF_JUSTATTACKED;
	}

	// [RH] Being totally frozen zeros out most input parameters.
	if (player->cheats & CF_TOTALLYFROZEN)
	{
		cmd->ucmd.buttons &= BT_USE;
		cmd->ucmd.pitch = 0;
		cmd->ucmd.yaw = 0;
		cmd->ucmd.roll = 0;
		cmd->ucmd.forwardmove = 0;
		cmd->ucmd.sidemove = 0;
		cmd->ucmd.upmove = 0;
		player->turnticks = 0;
	}
	else if (player->cheats & CF_FROZEN)
	{
		cmd->ucmd.forwardmove = 0;
		cmd->ucmd.sidemove = 0;
		cmd->ucmd.upmove = 0;
	}

	if (player->playerstate == PST_DEAD)
	{
		P_DeathThink (player);
		return;
	}
	if (player->jumpTics)
	{
		player->jumpTics--;
	}
	if (player->morphTics)
	{
		P_MorphPlayerThink (player);
	}

	// [RH] Look up/down stuff
	if (dmflags & DF_NO_FREELOOK)
	{
		player->mo->pitch = 0;
	}
	else
	{
		int look = cmd->ucmd.pitch << 16;

		// The player's view pitch is clamped between -32 and +56 degrees,
		// which translates to about half a screen height up and (more than)
		// one full screen height down from straight ahead when view panning
		// is used.
		if (look)
		{
			if (look == -32768 << 16)
			{ // center view
				player->mo->pitch = 0;
			}
			else
			{
				player->mo->pitch -= look;
				if (look > 0)
				{ // look up
					if (player->mo->pitch < -ANGLE_1*32)
						player->mo->pitch = -ANGLE_1*32;
				}
				else
				{ // look down
					if (player->mo->pitch > ANGLE_1*56)
						player->mo->pitch = ANGLE_1*56;
				}
			}
		}
	}

	// [RH] Check for fast turn around
	if (cmd->ucmd.buttons & BT_TURN180 && !(player->oldbuttons & BT_TURN180))
	{
		player->turnticks = TURN180_TICKS;
	}

	// Handle movement
	if (player->mo->reactiontime)
	{ // Player is frozen
		player->mo->reactiontime--;
	}
	else
	{
		P_MovePlayer (player);
		AActor *pmo = player->mo;

		if (player->powers[pw_speed] && !(level.time & 1)
			&& P_AproxDistance (pmo->momx, pmo->momy) > 12*FRACUNIT)
		{
			AActor *speedMo;

			speedMo = Spawn<APlayerSpeedTrail> (pmo->x, pmo->y, pmo->z);
			if (speedMo)
			{
				speedMo->angle = pmo->angle;
				speedMo->Translation = pmo->Translation;
				speedMo->target = pmo;
				speedMo->sprite = pmo->sprite;
				speedMo->frame = pmo->frame;
				speedMo->floorclip = pmo->floorclip;
				if (player->mo == players[consoleplayer].camera &&
					!(player->cheats & CF_CHASECAM))
				{
					speedMo->renderflags |= RF_INVISIBLE;
				}
			}
		}

		// [RH] check for jump
		if (cmd->ucmd.buttons & BT_JUMP)
		{
			if (player->mo->waterlevel >= 2)
			{
				player->mo->momz = 4*FRACUNIT;
			}
			else if (player->mo->flags2 & MF2_FLY)
			{
				player->mo->momz = 3*FRACUNIT;
			}
			else if (!(dmflags & DF_NO_JUMP) && onground && !player->jumpTics)
			{
				player->mo->momz += player->mo->GetJumpZ ()*35/TICRATE;
				S_Sound (player->mo, CHAN_BODY, "*jump", 1, ATTN_NORM);
				player->mo->flags2 &= ~MF2_ONMOBJ;
				player->jumpTics = 18*TICRATE/35;
			}
		}

		if (cmd->ucmd.upmove == -32768)
		{ // Only land if in the air
			if ((player->mo->flags2 & MF2_FLY) && player->mo->waterlevel < 2)
			{
				player->mo->flags2 &= ~MF2_FLY;
				player->mo->flags &= ~MF_NOGRAVITY;
			}
		}
		else if (cmd->ucmd.upmove != 0)
		{
			if (player->mo->waterlevel >= 2 || player->powers[pw_flight])
			{
				player->mo->momz = cmd->ucmd.upmove << 9;
				if (player->mo->waterlevel < 2 && !(player->mo->flags2 & MF2_FLY))
				{
					player->mo->flags2 |= MF2_FLY;
					player->mo->flags |= MF_NOGRAVITY;
					if (player->mo->momz <= -39*FRACUNIT)
					{ // Stop falling scream
						S_StopSound (player->mo, CHAN_VOICE);
					}
				}
			}
			else if (cmd->ucmd.upmove > 0)
			{
				P_PlayerUseArtifact (player, arti_fly);
			}
		}
	}

	P_CalcHeight (player);

	if (player->mo->Sector->special || player->mo->Sector->damage)
	{
		P_PlayerInSpecialSector (player);
	}
	P_PlayerOnSpecialFlat (player, P_GetThingFloorType (player->mo));
	if (player->mo->momz <= -35*FRACUNIT
		&& player->mo->momz >= -40*FRACUNIT && !player->morphTics)
	{
		int id = S_FindSkinnedSound (player->mo, "*falling");
		if (id != 0 && !S_GetSoundPlayingInfo (player->mo, id))
		{
			S_SoundID (player->mo, CHAN_VOICE, id, 1, ATTN_NORM);
		}
	}
	// check for use
	if ((cmd->ucmd.buttons & BT_USE) && !(player->oldbuttons & BT_USE))
	{
		P_UseLines (player);
	}
	// Morph counter
	if (player->morphTics)
	{
		if (player->chickenPeck)
		{ // Chicken attack counter
			player->chickenPeck -= 3;
		}
		if (!--player->morphTics)
		{ // Attempt to undo the chicken/pig
			P_UndoPlayerMorph (player);
		}
	}
	// Cycle psprites
	P_MovePsprites (player);
	// Other Counters
	if (player->powers[pw_invulnerability])
	{
		player->mo->SpecialInvulnerabilityHandling (APlayerPawn::INVUL_Active);
		if (!(--player->powers[pw_invulnerability]))
		{
			player->mo->flags2 &= ~MF2_INVULNERABLE;
			player->mo->effects &= ~FX_RESPAWNINVUL;
			player->mo->SpecialInvulnerabilityHandling (APlayerPawn::INVUL_Stop);
		}
	}
	// Strength counts up to diminish fade.
	if (player->powers[pw_strength])
		player->powers[pw_strength]++;

	if (player->powers[pw_invisibility])
	{
		if (!--player->powers[pw_invisibility])
		{
			player->mo->flags &= ~MF_SHADOW;
			player->mo->flags3 &= ~MF3_GHOST;
			player->mo->RenderStyle = STYLE_Normal;
			player->mo->alpha = OPAQUE;
		}
	}

	if (player->powers[pw_infrared])
		player->powers[pw_infrared]--;

	if (player->powers[pw_ironfeet])
		player->powers[pw_ironfeet]--;

	if (player->powers[pw_minotaur])
		player->powers[pw_minotaur]--;

	if (player->powers[pw_speed])
		player->powers[pw_speed]--;

	if (player->powers[pw_flight] && (multiplayer || !(level.clusterflags & CLUSTER_HUB)))
	{
		// [RH] Methinks this check is not right.
		//if ((player->mo->flags2 & MF2_FLY) && (player->mo->waterlevel < 2))))
		{
			if (!--player->powers[pw_flight])
			{
				if (player->mo->z != player->mo->floorz)
				{
					player->centering = true;
				}
				player->mo->flags2 &= ~MF2_FLY;
				player->mo->flags &= ~MF_NOGRAVITY;
				BorderTopRefresh = screen->GetPageCount (); //make sure the sprite's cleared out
			}
		}
	}

	if (player->powers[pw_weaponlevel2])
	{
		if (!--player->powers[pw_weaponlevel2])
		{
			if ((player->readyweapon == wp_phoenixrod)
				&& (player->psprites[ps_weapon].state
					!= wpnlev2info[wp_phoenixrod]->readystate)
				&& (player->psprites[ps_weapon].state
					!= wpnlev2info[wp_phoenixrod]->upstate))
			{
				P_SetPsprite (player, ps_weapon,
					wpnlev1info[wp_phoenixrod]->readystate);
				player->ammo[am_phoenixrod] -=
					wpnlev2info[wp_phoenixrod]->ammouse;
				player->refire = 0;
				S_StopSound (player->mo, CHAN_WEAPON);
			}
			else if ((player->readyweapon == wp_gauntlets)
				|| (player->readyweapon == wp_staff))
			{
				player->pendingweapon = player->readyweapon;
			}
			BorderTopRefresh = screen->GetPageCount ();
		}
	}

	if (player->damagecount)
		player->damagecount--;

	if (player->bonuscount)
		player->bonuscount--;

	if (player->poisoncount && !(level.time & 15))
	{
		player->poisoncount -= 5;
		if (player->poisoncount < 0)
		{
			player->poisoncount = 0;
		}
		P_PoisonDamage (player, player->poisoner, 1, true);
	}

	// Handling colormaps.
	if (player->powers[pw_invulnerability] && gameinfo.gametype != GAME_Hexen)
	{
		if ((player->powers[pw_invulnerability] > BLINKTHRESHOLD
			|| (player->powers[pw_invulnerability] & 8)) &&
			!(player->mo->effects & FX_RESPAWNINVUL))
		{
			player->fixedcolormap = NUMCOLORMAPS;
		}
		else
		{
			player->fixedcolormap = 0;
		}
	}
	else if (player->powers[pw_infrared])		
	{
		if (gameinfo.gametype == GAME_Doom)
		{
			if (player->powers[pw_infrared] > BLINKTHRESHOLD
				|| (player->powers[pw_infrared] & 8))
			{	// almost full bright
				player->fixedcolormap = 1;
			}
			else
			{
				player->fixedcolormap = 0;
			}
		}
		else
		{
			if (player->powers[pw_infrared] <= BLINKTHRESHOLD)
			{
				if (player->powers[pw_infrared] & 8)
				{
					player->fixedcolormap = 0;
				}
				else
				{
					player->fixedcolormap = 1;
				}
			}
			else if (!(level.time & 16) &&
				player->mo == players[consoleplayer].camera)
			{
				if (newtorch)
				{
					if (player->fixedcolormap + newtorchdelta > 7
						|| player->fixedcolormap + newtorchdelta < 1
						|| newtorch == player->fixedcolormap)
					{
						newtorch = 0;
					}
					else
					{
						player->fixedcolormap += newtorchdelta;
					}
				}
				else
				{
					newtorch = (pr_torch() & 7) + 1;
					newtorchdelta = (newtorch == player->fixedcolormap) ?
						0 : ((newtorch > player->fixedcolormap) ? 1 : -1);
				}
			}
		}
	}
	else
	{
		player->fixedcolormap = 0;
	}

	// Handle air supply
	if (player->mo->waterlevel < 3 || player->powers[pw_ironfeet])
	{
		player->air_finished = level.time + 10*TICRATE;
	}
	else if (player->air_finished <= level.time && !(level.time & 31))
	{
		P_DamageMobj (player->mo, NULL, NULL, 2 + 2*((level.time-player->air_finished)/TICRATE), MOD_WATER, DMG_NO_ARMOR);
	}

	// Save buttons
	player->oldbuttons = cmd->ucmd.buttons;
}

void player_s::Serialize (FArchive &arc)
{
	int i;
	userinfo_t *ui;
	userinfo_t dummy;

	if (arc.IsStoring ())
	{
		arc.UserWriteClass (cls);
		ui = &userinfo;
	}
	else
	{
		arc.UserReadClass (cls);
		ui = &dummy;
	}

	arc << mo
		<< camera
		<< playerstate
		<< cmd
		<< *ui
		<< DesiredFOV << FOV
		<< viewz
		<< viewheight
		<< deltaviewheight
		<< bob
		<< momx
		<< momy
		<< centering
		<< health
		<< armortype
		<< readyArtifact
		<< artifactCount
		<< inventorytics;

	if (SaveVersion < 206)
	{ // Skip inventorySlotNum field
		int foo;
		arc << foo;
	}

	arc << pieces
		<< backpack
		<< fragcount
		<< spreecount
		<< multicount
		<< lastkilltime
		<< readyweapon
		<< pendingweapon
		<< cheats
		<< refire
		<< inconsistant
		<< killcount
		<< itemcount
		<< secretcount
		<< damagecount
		<< bonuscount
		<< flamecount
		<< poisoncount
		<< poisoner
		<< attacker
		<< extralight
		<< fixedcolormap
		<< xviewshift
		<< morphTics
		<< chickenPeck
		<< rain1
		<< rain2
		<< jumpTics
		<< respawn_time
		<< air_finished
		<< turnticks
		<< oldbuttons
		<< isbot
		<< BlendR
		<< BlendG
		<< BlendB
		<< BlendA;
	for (i = 0; i < NUMARMOR; i++)
		arc << armorpoints[i];
	if (SaveVersion < 206)
	{ // versions before 205 do not have arti_pork
		static const BYTE compatArtiNums[] =
		{
			arti_none,
			arti_invulnerability,
			arti_invisibility,
			arti_health,
			arti_superhealth,
			arti_tomeofpower,
			arti_healingradius,
			arti_summon,
			arti_torch,
			arti_firebomb,
			arti_egg,
			arti_pork,
			arti_fly,
			arti_blastradius,
			arti_poisonbag1,
			arti_poisonbag2,
			arti_poisonbag3,
			arti_teleportother,
			arti_speed,
			arti_boostmana,
			arti_boostarmor,
			arti_teleport,
			arti_firstpuzzitem,
			arti_puzzskull,
			arti_puzzgembig,
			arti_puzzgemred,
			arti_puzzgemgreen1,
			arti_puzzgemgreen2,
			arti_puzzgemblue1,
			arti_puzzgemblue2,
			arti_puzzbook1,
			arti_puzzbook2,
			arti_puzzskull2,
			arti_puzzfweapon,
			arti_puzzcweapon,
			arti_puzzmweapon,
			arti_puzzgear1,
			arti_puzzgear2,
			arti_puzzgear3,
			arti_puzzgear4
		};

		for (i = 0; i < 11; ++i)
			arc << inventory[compatArtiNums[i]];
		if (SaveVersion == 205)
			arc << inventory[compatArtiNums[12]];
		for (i = 13; (size_t)i < sizeof(compatArtiNums); ++i)
			arc << inventory[compatArtiNums[i]];
	}
	else
	{ // Post-205 stores inventory names with their counts,
	  // so new artifacts can be added freely without breaking
	  // savegames.
		if (arc.IsStoring())
		{
			for (i = 0; i < NUMINVENTORYSLOTS; ++i)
			{
				if (inventory[i] != 0)
				{
					arc.WriteString (ArtifactNames[i]);
					arc.WriteCount (inventory[i]);
				}
			}
			arc.WriteString (NULL);
		}
		else
		{
			char *str = NULL;

			arc << str;
			while (str != NULL)
			{
				artitype_t arti;
				DWORD count;

				arti = P_FindNamedInventory (str);
				count = arc.ReadCount ();
				if (arti != arti_none)
				{
					inventory[arti] = (WORD)count;
				}
				arc << str;
			}
		}
	}
	for (i = 0; i < NUMPOWERS; i++)
		arc << powers[i];
	for (i = 0; i < NUMKEYS; i++)
		arc << keys[i];
	for (i = 0; i < MAXPLAYERS; i++)
		arc << frags[i];
	for (i = 0; i < NUMWEAPONS; i++)
		arc << weaponowned[i];
	for (i = 0; i < NUMAMMO; i++)
		arc << ammo[i] << maxammo[i];
	for (i = 0; i < NUMPSPRITES; i++)
		arc << psprites[i];

	if (SaveVersion < 204)
	{ // I'm the only one who would have been playing with some class
	  // other than the Fighter with an earlier version savegame, so
	  // setting this to zero is okay.
		CurrentPlayerClass = 0;
	}
	else
	{
		arc << CurrentPlayerClass;
	}

	if (SaveVersion < 202)
	{
		mstaffcount = cholycount = 0;
	}
	else
	{
		arc << mstaffcount << cholycount;
	}

	if (isbot)
	{
		arc	<< angle
			<< dest
			<< prev
			<< enemy
			<< missile
			<< mate
			<< last_mate
			<< skill
			<< t_active
			<< t_respawn
			<< t_strafe
			<< t_react
			<< t_fight
			<< t_roam
			<< t_rocket
			<< first_shot
			<< sleft
			<< allround
			<< redteam
			<< oldx
			<< oldy;
		if (arc.IsLoading ())
		{
			if (consoleplayer != this - players)
			{
				userinfo = *ui;
			}
			botinfo_t *thebot = bglobal.botinfo;
			while (thebot && stricmp (userinfo.netname, thebot->name))
				thebot = thebot->next;
			if (thebot)
			{
				thebot->inuse = true;
			}
			bglobal.botnum++;
			bglobal.botingame[this - players] = true;
		}
	}
	else
	{
		dest = prev = enemy = missile = mate = last_mate = NULL;
	}
}
