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




#include "doomdef.h"
#include "d_event.h"
#include "p_local.h"
#include "doomstat.h"
#include "s_sound.h"



// Index of the special effects (INVUL inverse) map.
#define INVERSECOLORMAP 		32


//
// Movement.
//

// 16 pixels of bob
#define MAXBOB	0x100000		

// [RH] Now a pointer since it uses P_FindFloor()
mobj_t 		*onground;


//
// P_Thrust
// Moves the given origin along a given angle.
//
void P_Thrust (player_t *player, angle_t angle, fixed_t move)
{
	angle >>= ANGLETOFINESHIFT;

	player->mo->momx += FixedMul(move,finecosine[angle]);
	player->mo->momy += FixedMul(move,finesine[angle]);
}




//
// P_CalcHeight
// Calculate the walking / running height adjustment
//
void P_CalcHeight (player_t* player) 
{
	int 		angle;
	fixed_t 	bob;

	// Regular movement bobbing
	// (needs to be calculated for gun swing
	// even if not on ground)
	// OPTIMIZE: tablify angle
	// Note: a LUT allows for effects
	//	like a ramp with low health.
	player->bob = FixedMul(player->mo->momx,player->mo->momx)
				+ FixedMul(player->mo->momy,player->mo->momy);
	player->bob >>= 2;

	// phares 9/9/98: If player is standing on ice, reduce his bobbing.

	if (player->mo->friction > ORIG_FRICTION) // ice?
	{
		if (player->bob > (MAXBOB>>2))
			player->bob = MAXBOB>>2;
	}
	else													//   ^
		if (player->bob > MAXBOB)							//   |
			player->bob = MAXBOB;							// phares 2/26/98

	if ((player->cheats & CF_NOMOMENTUM) || !onground)
	{
		player->viewz = player->mo->z + VIEWHEIGHT;

		if (player->viewz > player->mo->ceilingz-4*FRACUNIT)
			player->viewz = player->mo->ceilingz-4*FRACUNIT;

// The following line was in the Id source and appears		// phares 2/25/98
// to be a bug. player->viewz is checked in a similar
// manner at a different exit below.

//		player->viewz = player->mo->z + player->viewheight;
		return;
	}
				
	angle = (FINEANGLES/20*level.time)&FINEMASK;
	bob = FixedMul ( player->bob/2, finesine[angle]);


	// move viewheight
	if (player->playerstate == PST_LIVE)
	{
		player->viewheight += player->deltaviewheight;

		if (player->viewheight > VIEWHEIGHT)
		{
			player->viewheight = VIEWHEIGHT;
			player->deltaviewheight = 0;
		}

		if (player->viewheight < VIEWHEIGHT/2)
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
	player->viewz = player->mo->z + player->viewheight + bob;

	if (player->viewz > player->mo->ceilingz-4*FRACUNIT)
		player->viewz = player->mo->ceilingz-4*FRACUNIT;
}



//
// P_MovePlayer
//
void P_MovePlayer (player_t *player)
{
	ticcmd_t *cmd = &player->cmd;
	int		  movefactor;       // movement factor                    // phares
	fixed_t	  forwardmove;
	fixed_t	  sidemove;

	player->mo->angle += (cmd->ucmd.yaw<<16);

	onground = P_FindFloor (player->mo);

	movefactor = P_GetMoveFactor (player->mo);
	if (!onground) {
		if (!olddemo) {
			// [RH] allow very limited movement if not on ground.
			movefactor >>= 8;
		} else {
			movefactor = 0;
		}
	}

	forwardmove = (cmd->ucmd.forwardmove * movefactor) >> 8;
	sidemove = (cmd->ucmd.sidemove * movefactor) >> 8;

	if (forwardmove)
		P_Thrust (player, player->mo->angle, forwardmove);

	if (sidemove)
		P_Thrust (player, player->mo->angle-ANG90, sidemove);

	if ( (cmd->ucmd.forwardmove || cmd->ucmd.sidemove) 
		 && player->mo->state == &states[S_PLAY] )
	{
		P_SetMobjState (player->mo, S_PLAY_RUN1);
	}
}		

// [RH] (Adapted from Q2)
// P_FallingDamage
//
void P_FallingDamage (mobj_t *ent)
{
	mobj_t *groundentity;
	float	delta;
	int		damage;

	if (!ent->player)
		return;		// not a player

	if (ent->flags & MF_NOCLIP)
		return;

	groundentity = P_FindFloor (ent);

	if ((ent->player->oldvelocity[2] < 0) && (ent->momz > ent->player->oldvelocity[2]) && (!groundentity))
	{
		delta = (float)ent->player->oldvelocity[2];
	}
	else
	{
		if (!groundentity)
			return;
		delta = (float)(ent->momz - ent->player->oldvelocity[2]);
	}
	delta = delta*delta * 2.03904313e-11f;

	if (delta < 1)
		return;

	if (delta < 15)
	{
		//ent->s.event = EV_FOOTSTEP;
		return;
	}

	if (delta > 30)
	{
		damage = (int)((delta-30)/2);
		if (damage < 1)
			damage = 1;

		if (dmflags & DF_YES_FALLING)
			P_DamageMobj (ent, NULL, NULL, damage, MOD_FALLING);
	}
	else
	{
		//ent->s.event = EV_FALLSHORT;
		return;
	}

}


//
// P_DeathThink
// Fall on your face when dying.
// Decrease POV height to floor height.
//
#define ANG5	(ANG90/18)

void P_DeathThink (player_t* player)
{
	angle_t 			angle;
	angle_t 			delta;

	P_MovePsprites (player);
		
	// fall to the ground
	if (player->viewheight > 6*FRACUNIT)
		player->viewheight -= FRACUNIT;

	if (player->viewheight < 6*FRACUNIT)
		player->viewheight = 6*FRACUNIT;

	player->deltaviewheight = 0;
	onground = P_FindFloor (player->mo);
	P_CalcHeight (player);
		
	if (player->attacker && player->attacker != player->mo)
	{
		angle = R_PointToAngle2 (player->mo->x,
								 player->mo->y,
								 player->attacker->x,
								 player->attacker->y);
		
		delta = angle - player->mo->angle;
		
		if (delta < ANG5 || delta > (unsigned)-ANG5)
		{
			// Looking at killer,
			//	so fade damage flash down.
			player->mo->angle = angle;

			if (player->damagecount)
				player->damagecount--;
		}
		else if (delta < ANG180)
			player->mo->angle += ANG5;
		else
			player->mo->angle -= ANG5;
	}
	else if (player->damagecount)
		player->damagecount--;
		

	// [RH] Delay rebirth slightly
	if (level.time >= player->respawn_time) {
		if (player->cmd.ucmd.buttons & BT_USE ||
			(deathmatch->value && dmflags & DF_FORCE_RESPAWN))
			player->playerstate = PST_REBORN;
	}
}



//
// P_PlayerThink
//
void P_PlayerThink (player_t *player)
{
	ticcmd_t *cmd;
	weapontype_t newweapon;

	player->xviewshift = 0;		// [RH] Make sure view is in right place

	// fixme: do this in the cheat code
	if (player->cheats & CF_NOCLIP)
		player->mo->flags |= MF_NOCLIP;
	else
		player->mo->flags &= ~MF_NOCLIP;

	// chain saw run forward
	cmd = &player->cmd;
	if (player->mo->flags & MF_JUSTATTACKED)
	{
		cmd->ucmd.yaw = 0;
		cmd->ucmd.forwardmove = 0xc800/2;
		cmd->ucmd.sidemove = 0;
		player->mo->flags &= ~MF_JUSTATTACKED;
	}


	if (player->playerstate == PST_DEAD)
	{
		P_DeathThink (player);
		return;
	}

	// [RH] Look up/down stuff
	if (dmflags & DF_NO_FREELOOK) {
		player->mo->pitch = 0;
	} else {
		int look = cmd->ucmd.pitch;

		if (look) {
			if (look == -32768) {
				// center view
				player->mo->pitch = 0;
			} else if (look > 0) {
				// look up
				player->mo->pitch -= look;
				if (player->mo->pitch < -FRACUNIT)
					player->mo->pitch = -FRACUNIT;
			} else {
				// look down
				player->mo->pitch -= look;
				if (player->mo->pitch > (FRACUNIT<<1))
					player->mo->pitch = FRACUNIT<<1;
			}
		}
	}

	// Move around.
	// Reactiontime is used to prevent movement
	//	for a bit after a teleport.
	if (player->mo->reactiontime)
		player->mo->reactiontime--;
	else
		P_MovePlayer (player);

	P_CalcHeight (player);

	if (player->mo->subsector->sector->special || player->mo->subsector->sector->damage)
		P_PlayerInSpecialSector (player);

	// Check for weapon change.

	// A special event has no other buttons.
	if (cmd->ucmd.buttons & BT_SPECIAL)
		cmd->ucmd.buttons = 0;						
	
	if ((cmd->ucmd.buttons & BT_CHANGE) || cmd->ucmd.impulse >= 50)
	{
		// [RH] Support direct weapon changes
		if (cmd->ucmd.impulse) {
			newweapon = cmd->ucmd.impulse - 50;
		} else {
			// The actual changing of the weapon is done
			//	when the weapon psprite can do it
			//	(read: not in the middle of an attack).
			newweapon = (cmd->ucmd.buttons&BT_WEAPONMASK)>>BT_WEAPONSHIFT;
			
			if (newweapon == wp_fist
				&& player->weaponowned[wp_chainsaw]
				&& !(player->readyweapon == wp_chainsaw
					 && player->powers[pw_strength]))
			{
				newweapon = wp_chainsaw;
			}
			
			if ( (gamemode == commercial)
				&& newweapon == wp_shotgun 
				&& player->weaponowned[wp_supershotgun]
				&& player->readyweapon != wp_supershotgun)
			{
				newweapon = wp_supershotgun;
			}
		}

		if (newweapon >= 0 && newweapon < NUMWEAPONS) {
			if (player->weaponowned[newweapon]
				&& newweapon != player->readyweapon)
			{
				// Do not go to plasma or BFG in shareware,
				//	even if cheated.
				if ((newweapon != wp_plasma
					 && newweapon != wp_bfg)
					|| (gamemode != shareware) )
				{
					player->pendingweapon = newweapon;
				}
			}
		}
	}

	// [RH] check for jump
	if ((cmd->ucmd.buttons & BT_JUMP) == BT_JUMP) {
		if (!player->jumpdown && !(dmflags & DF_NO_JUMP)) {
			// only jump if we are on the ground
			if (P_FindFloor (player->mo)) {
				player->mo->momz += 8 * FRACUNIT;
				player->mo->flags2 |= MF2_JUMPING;
				S_StartSound (player->mo, "*jump1", 100);
			}
			player->jumpdown = true;
		}
	} else player->jumpdown = false;

	// check for use
	if (cmd->ucmd.buttons & BT_USE)
	{
		if (!player->usedown)
		{
			P_UseLines (player);
			player->usedown = true;
		}
	}
	else
		player->usedown = false;

	// cycle psprites
	P_MovePsprites (player);

	// Counters, time dependend power ups.

	// Strength counts up to diminish fade.
	if (player->powers[pw_strength])
		player->powers[pw_strength]++;	
				
	if (player->powers[pw_invulnerability])
		player->powers[pw_invulnerability]--;

	if (player->powers[pw_invisibility])
		if (! --player->powers[pw_invisibility] )
			player->mo->flags &= ~MF_SHADOW;
						
	if (player->powers[pw_infrared])
		player->powers[pw_infrared]--;
				
	if (player->powers[pw_ironfeet])
		player->powers[pw_ironfeet]--;
				
	if (player->damagecount)
		player->damagecount--;
				
	if (player->bonuscount)
		player->bonuscount--;


	// Handling colormaps.
	if (player->powers[pw_invulnerability])
	{
		if (player->powers[pw_invulnerability] > 4*32
			|| (player->powers[pw_invulnerability]&8) )
			player->fixedcolormap = INVERSECOLORMAP;
		else
			player->fixedcolormap = 0;
	}
	else if (player->powers[pw_infrared])		
	{
		if (player->powers[pw_infrared] > 4*32
			|| (player->powers[pw_infrared]&8) )
		{
			// almost full bright
			player->fixedcolormap = 1;
		}
		else
			player->fixedcolormap = 0;
	}
	else
		player->fixedcolormap = 0;
}

