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
#include "i_system.h"



// Index of the special effects (INVUL inverse) map.
#define INVERSECOLORMAP 		32


//
// Movement.
//

// 16 pixels of bob
#define MAXBOB			0x100000

BOOL onground;

// This function supplements the pointer cleanup in dobject.cpp, because
// player_s is not derived from DObject. (I tried it, and DestroyScan was
// unable to properly determine the player object's type--possibly
// because it gets staticly allocated in an array.)
//
// This function checks all the DObject pointers in a player_s and NULLs any
// that match the pointer passed in. If you add any pointers that point to
// DObject (or a subclass), add them here too.

void player_s::FixPointers (const DObject *obj)
{
	if (mo == obj)
		mo = NULL;
	if (attacker == obj)
		attacker = NULL;
	if (camera == obj)
		camera = NULL;
	if (dest == obj)
		dest = NULL;
	if (prev == obj)
		prev = NULL;
	if (enemy == obj)
		enemy = NULL;
	if (missile == obj)
		missile = NULL;
	if (mate == obj)
		mate = NULL;
	if (last_mate == obj)
		last_mate = NULL;
}


//
// P_Thrust
// Moves the given origin along a given angle.
//
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

//
// P_CalcHeight
// Calculate the walking / running height adjustment
//
void P_CalcHeight (player_t *player) 
{
	int 		angle;
	fixed_t 	bob;

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
		player->bob = FixedMul (player->momx, player->momx)
					+ FixedMul (player->momy, player->momy);
		player->bob >>= 2;

		if (player->bob > MAXBOB)
			player->bob = MAXBOB;
	}

	if (!onground || (player->cheats & CF_NOMOMENTUM))
	{
		player->viewz = player->mo->z + VIEWHEIGHT;

		if (player->viewz > player->mo->ceilingz-4*FRACUNIT)
			player->viewz = player->mo->ceilingz-4*FRACUNIT;

		return;
	}
				
	angle = (FINEANGLES/20*level.time) & FINEMASK;
	bob = FixedMul (player->bob>>(player->mo->waterlevel > 1 ? 2 : 1), finesine[angle]);

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
	AActor *mo = player->mo;

	mo->angle += cmd->ucmd.yaw << 16;

	onground = (mo->z <= mo->floorz) || (mo->flags2 & MF2_ONMOBJ);

	// [RH] Don't let frozen players move
	if (player->cheats & CF_FROZEN)
		return;

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

		movefactor = P_GetMoveFactor (mo, &friction);
		bobfactor = friction < ORIG_FRICTION ? movefactor : ORIG_FRICTION_FACTOR;
		if (!onground && !(player->mo->flags2 & MF2_FLY) && !player->mo->waterlevel)
		{
			// [RH] allow very limited movement if not on ground.
			movefactor >>= 8;
			bobfactor >>= 8;
		}
		forwardmove = (cmd->ucmd.forwardmove * movefactor) >> 8;
		sidemove = (cmd->ucmd.sidemove * movefactor) >> 8;

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

		if (mo->state == &states[S_PLAY])
		{
			P_SetMobjState (player->mo, S_PLAY_RUN1);
		}

		if (player->cheats & CF_REVERTPLEASE)
		{
			player->cheats &= ~CF_REVERTPLEASE;
			player->camera = player->mo;
		}
	}
}		

// [RH] (Adapted from Q2)
// P_FallingDamage
//
void P_FallingDamage (AActor *ent)
{
	float	delta;
	int		damage;

	if (!ent->player)
		return;		// not a player

	if (ent->flags & MF_NOCLIP)
		return;

	if ((ent->player->oldvelocity[2] < 0)
		&& (ent->momz > ent->player->oldvelocity[2])
		&& (!(ent->flags2 & MF2_ONMOBJ)
			|| !(ent->z <= ent->floorz)))
	{
		delta = (float)ent->player->oldvelocity[2];
	}
	else
	{
		if (!(ent->flags2 & MF2_ONMOBJ))
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

void P_DeathThink (player_t *player)
{
	angle_t 			angle;
	angle_t 			delta;

	P_MovePsprites (player);
	onground = (player->mo->z <= player->mo->floorz);
		
	// fall to the ground
	if (player->viewheight > 6*FRACUNIT)
		player->viewheight -= FRACUNIT;

	if (player->viewheight < 6*FRACUNIT)
		player->viewheight = 6*FRACUNIT;

	player->deltaviewheight = 0;
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
			((deathmatch.value || alwaysapplydmflags.value) && dmflags & DF_FORCE_RESPAWN))
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

	// [RH] Error out if player doesn't have an mobj, but just make
	//		it a warning if the player trying to spawn is a bot
	if (!player->mo)
	{
		if (player->isbot)
		{
			Printf (PRINT_HIGH, "%s left: No player %d start\n",
				player->userinfo.netname, player - players + 1);
			bglobal.ClearPlayer (player - players);
			return;
		}
		else
			I_Error ("No player %ld start\n", player - players + 1);
	}

	player->xviewshift = 0;		// [RH] Make sure view is in right place

	// fixme: do this in the cheat code
	if (player->cheats & CF_NOCLIP)
		player->mo->flags |= MF_NOCLIP;
	else
		player->mo->flags &= ~MF_NOCLIP;
	if (player->cheats & CF_FLY)
		player->mo->flags |= MF_NOGRAVITY, player->mo->flags2 |= MF2_FLY;
	else
		player->mo->flags &= ~MF_NOGRAVITY, player->mo->flags2 &= ~MF2_FLY;

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
	if (player->jumpTics)
	{
		player->jumpTics--;
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
			else if (look > 0)
			{ // look up
				player->mo->pitch -= look;
				if (player->mo->pitch < -ANGLE_1*32)
					player->mo->pitch = -ANGLE_1*32;
			}
			else
			{ // look down
				player->mo->pitch -= look;
				if (player->mo->pitch > ANGLE_1*56)
					player->mo->pitch = ANGLE_1*56;
			}
		}
	}

	// Move around.
	// Reactiontime is used to prevent movement
	//	for a bit after a teleport.
	if (player->mo->reactiontime)
		player->mo->reactiontime--;
	else
	{
		// [RH] check for jump
		if ((cmd->ucmd.buttons & BT_JUMP) == BT_JUMP)
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
				player->mo->momz += 8*FRACUNIT;
				S_Sound (player->mo, CHAN_BODY, "*jump1", 1, ATTN_NORM);
				player->mo->flags2 &= ~MF2_ONMOBJ;
				player->jumpTics = 18;
			}
		}

		if (cmd->ucmd.upmove &&
			(player->mo->waterlevel >= 2 || player->mo->flags2 & MF2_FLY))
		{
			player->mo->momz = cmd->ucmd.upmove << 8;
		}

		P_MovePlayer (player);
	}

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
			newweapon = (weapontype_t)(cmd->ucmd.impulse - 50);
		} else {
			// The actual changing of the weapon is done
			//	when the weapon psprite can do it
			//	(read: not in the middle of an attack).
			newweapon = (weapontype_t)((cmd->ucmd.buttons&BT_WEAPONMASK)>>BT_WEAPONSHIFT);
			
			if (newweapon == wp_fist
				&& player->weaponowned[wp_chainsaw]
				&& !(player->readyweapon == wp_chainsaw
					 && player->powers[pw_strength]))
			{
				newweapon = wp_chainsaw;
			}
			
			if (newweapon == wp_shotgun 
				&& player->weaponowned[wp_supershotgun]
				&& player->readyweapon != wp_supershotgun)
			{
				newweapon = wp_supershotgun;
			}
		}

		if ((newweapon >= 0 && newweapon < NUMWEAPONS)
			&& player->weaponowned[newweapon]
			&& newweapon != player->readyweapon)
		{
			player->pendingweapon = newweapon;
		}
	}

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

	// Counters, time dependent power ups.

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

	// Handle air supply
	if (player->mo->waterlevel < 3 || player->powers[pw_ironfeet])
	{
		player->air_finished = level.time + 10*TICRATE;
	}
	else if (player->air_finished <= level.time && !(level.time & 31))
	{
		P_DamageMobj (player->mo, NULL, NULL, 2 + 2*((level.time-player->air_finished)/TICRATE), MOD_WATER, DMG_NO_ARMOR);
	}
}

void player_s::Serialize (FArchive &arc)
{
	int i;

	if (arc.IsStoring ())
	{ // saving to archive
		arc << mo
			<< playerstate
			<< cmd
			<< userinfo
			<< viewz
			<< viewheight
			<< deltaviewheight
			<< bob
			<< momx
			<< momy
			<< health
			<< armorpoints
			<< armortype
			<< backpack
			<< fragcount
			<< readyweapon
			<< pendingweapon
			<< attackdown
			<< usedown
			<< cheats
			<< refire
			<< inconsistant
			<< killcount
			<< itemcount
			<< secretcount
			<< damagecount
			<< bonuscount
			<< attacker
			<< extralight
			<< fixedcolormap
			<< xviewshift
			<< jumpTics
			<< respawn_time
			<< air_finished
			<< isbot;
		for (i = 0; i < NUMPOWERS; i++)
			arc << powers[i];
		for (i = 0; i < NUMCARDS; i++)
			arc << cards[i];
		for (i = 0; i < MAXPLAYERS; i++)
			arc << frags[i];
		for (i = 0; i < NUMWEAPONS; i++)
			arc << weaponowned[i];
		for (i = 0; i < NUMAMMO; i++)
			arc << ammo[i] << maxammo[i];
		for (i = 0; i < NUMPSPRITES; i++)
			arc << psprites[i];
		for (i = 0; i < 3; i++)
			arc << oldvelocity[i];

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
				<< oldy
				<< chat;
			arc.Write (c_target, 256);
		}
	}
	else
	{ // Restoring from archive
		userinfo_t dummyuserinfo;

		arc >> mo
			>> playerstate
			>> cmd
			>> dummyuserinfo // Q: Would it be better to restore the userinfo from the archive?
			>> viewz
			>> viewheight
			>> deltaviewheight
			>> bob
			>> momx
			>> momy
			>> health
			>> armorpoints
			>> armortype
			>> backpack
			>> fragcount
			>> readyweapon
			>> pendingweapon
			>> attackdown
			>> usedown
			>> cheats
			>> refire
			>> inconsistant
			>> killcount
			>> itemcount
			>> secretcount
			>> damagecount
			>> bonuscount
			>> attacker
			>> extralight
			>> fixedcolormap
			>> xviewshift
			>> jumpTics
			>> respawn_time
			>> air_finished
			>> isbot;
		for (i = 0; i < NUMPOWERS; i++)
			arc >> powers[i];
		for (i = 0; i < NUMCARDS; i++)
			arc >> cards[i];
		for (i = 0; i < MAXPLAYERS; i++)
			arc >> frags[i];
		for (i = 0; i < NUMWEAPONS; i++)
			arc >> weaponowned[i];
		for (i = 0; i < NUMAMMO; i++)
			arc >> ammo[i] >> maxammo[i];
		for (i = 0; i < NUMPSPRITES; i++)
			arc >> psprites[i];
		for (i = 0; i < 3; i++)
			arc >> oldvelocity[i];

		camera = mo;

		if (consoleplayer != this - players)
			userinfo = dummyuserinfo;

		if (isbot)
		{
			arc	>> angle
				>> dest
				>> prev
				>> enemy
				>> missile
				>> mate
				>> last_mate
				>> skill
				>> t_active
				>> t_respawn
				>> t_strafe
				>> t_react
				>> t_fight
				>> t_roam
				>> t_rocket
				>> first_shot
				>> sleft
				>> allround
				>> redteam
				>> oldx
				>> oldy
				>> chat;
			arc.Read (c_target, 256);

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
}
