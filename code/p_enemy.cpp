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
//		Enemy thinking, AI.
//		Action Pointer Functions
//		that are associated with states/frames. 
//
//-----------------------------------------------------------------------------


#include <stdlib.h>

#include "m_random.h"
#include "m_alloc.h"
#include "i_system.h"
#include "doomdef.h"
#include "p_local.h"
#include "p_lnspec.h"
#include "p_effect.h"
#include "s_sound.h"
#include "g_game.h"
#include "doomstat.h"
#include "r_state.h"
#include "c_cvars.h"
#include "p_enemy.h"
#include "a_sharedglobal.h"
#include "a_doomglobal.h"
#include "a_action.h"

#include "gi.h"

//
// P_NewChaseDir related LUT.
//

dirtype_t opposite[9] =
{
	DI_WEST, DI_SOUTHWEST, DI_SOUTH, DI_SOUTHEAST,
	DI_EAST, DI_NORTHEAST, DI_NORTH, DI_NORTHWEST, DI_NODIR
};

dirtype_t diags[4] =
{
	DI_NORTHWEST, DI_NORTHEAST, DI_SOUTHWEST, DI_SOUTHEAST
};

fixed_t xspeed[8] = {FRACUNIT,47000,0,-47000,-FRACUNIT,-47000,0,47000};
fixed_t yspeed[8] = {0,47000,FRACUNIT,47000,0,-47000,-FRACUNIT,-47000};



//
// ENEMY THINKING
// Enemies are always spawned
// with targetplayer = -1, threshold = 0
// Most monsters are spawned unaware of all players,
// but some can be made preaware
//


//
// Called by P_NoiseAlert.
// Recursively traverse adjacent sectors,
// sound blocking lines cut off traversal.
//

AActor *soundtarget;

void P_RecursiveSound (sector_t *sec, int soundblocks)
{
	int 		i;
	line_t* 	check;
	sector_t*	other;
		
	// wake up all monsters in this sector
	if (sec->validcount == validcount
		&& sec->soundtraversed <= soundblocks+1)
	{
		return; 		// already flooded
	}
	
	sec->validcount = validcount;
	sec->soundtraversed = soundblocks+1;
	sec->soundtarget = soundtarget;

	for (i = 0; i < sec->linecount; i++)
	{
		check = sec->lines[i];
		if (! (check->flags & ML_TWOSIDED) )
			continue;

		if ( sides[ check->sidenum[0] ].sector == sec)
			other = sides[ check->sidenum[1] ].sector;
		else
			other = sides[ check->sidenum[0] ].sector;

		// check for closed door
		if ((sec->floorplane.ZatPoint (check->v1->x, check->v1->y) >=
			 other->ceilingplane.ZatPoint (check->v1->x, check->v1->y) &&
			 sec->floorplane.ZatPoint (check->v2->x, check->v2->y) >=
			 other->ceilingplane.ZatPoint (check->v2->x, check->v2->y))
		 || (other->floorplane.ZatPoint (check->v1->x, check->v1->y) >=
			 sec->ceilingplane.ZatPoint (check->v1->x, check->v1->y) &&
			 other->floorplane.ZatPoint (check->v2->x, check->v2->y) >=
			 sec->ceilingplane.ZatPoint (check->v2->x, check->v2->y)))
		{
			continue;
		}
		
		if (check->flags & ML_SOUNDBLOCK)
		{
			if (!soundblocks)
				P_RecursiveSound (other, 1);
		}
		else
			P_RecursiveSound (other, soundblocks);
	}
}



//
// P_NoiseAlert
// If a monster yells at a player,
// it will alert other monsters to the player.
//
void P_NoiseAlert (AActor *target, AActor *emmiter)
{
	if (target->player && (target->player->cheats & CF_NOTARGET))
		return;

	soundtarget = target;
	validcount++;
	P_RecursiveSound (emmiter->subsector->sector, 0);
}




//
// P_CheckMeleeRange
//
BOOL P_CheckMeleeRange (AActor *actor)
{
	AActor *pl;
	fixed_t dist;
		
	if (!actor->target)
		return false;
				
	pl = actor->target;
	dist = P_AproxDistance (pl->x-actor->x, pl->y-actor->y);

	if (dist >= MELEERANGE-20*FRACUNIT + pl->radius)
		return false;

	// [RH] If moving toward goal, then we've reached it.
	if (actor->target == actor->goal)
		return true;

	// [RH] Don't melee things too far above or below actor.
	if (pl->z > actor->z + actor->height)
		return false;
	if (pl->z + pl->height < actor->z)
		return false;
		
	if (!P_CheckSight (actor, pl, false))
		return false;
														
	return true;				
}

//
// P_CheckMissileRange
//
BOOL P_CheckMissileRange (AActor *actor)
{
	fixed_t dist;
		
	if (!P_CheckSight (actor, actor->target, false))
		return false;
		
	if (actor->flags & MF_JUSTHIT)
	{
		// the target just hit the enemy,
		// so fight back!
		actor->flags &= ~MF_JUSTHIT;
		return true;
	}
		
	if (actor->reactiontime)
		return false;	// do not attack yet
				
	// OPTIMIZE: get this from a global checksight
	dist = P_AproxDistance ( actor->x-actor->target->x,
							 actor->y-actor->target->y) - 64*FRACUNIT;
	
	if (actor->MeleeState == NULL)
		dist -= 128*FRACUNIT;	// no melee attack, so fire more

	return actor->SuggestMissileAttack (dist);
}

bool AActor::SuggestMissileAttack (fixed_t dist)
{
	return P_Random (pr_checkmissilerange) >= MIN (dist >> FRACBITS, 200);
}

//
// P_Move
// Move in the current direction,
// returns false if the move is blocked.
//
BOOL P_Move (AActor *actor)
{
	fixed_t tryx, tryy, deltax, deltay, origx, origy;
	BOOL try_ok;
	int good;
	int speed;
	int movefactor = ORIG_FRICTION_FACTOR;
	int friction = ORIG_FRICTION;

	if (actor->flags2 & MF2_BLASTED)
		return true;

	if (actor->movedir == DI_NODIR)
		return false;

	// [RH] Instead of yanking non-floating monsters to the ground,
	// let gravity drop them down, unless they're moving down a step.
	if (!(actor->flags & MF_NOGRAVITY) && actor->z > actor->floorz
		&& !(actor->flags2 & MF2_ONMOBJ))
	{
		if (actor->z > actor->floorz + 24*FRACUNIT)
		{
			return false;
		}
		else
		{
			actor->z = actor->floorz;
		}
	}

	if ((unsigned)actor->movedir >= 8)
		I_Error ("Weird actor->movedir!");

	speed = actor->GetDefault()->Speed;

#if 0	// [RH] I'm not so sure this is such a good idea
	// killough 10/98: make monsters get affected by ice and sludge too:
	movefactor = P_GetMoveFactor (actor, &friction);

	if (friction < ORIG_FRICTION &&		// sludge
		!(speed = ((ORIG_FRICTION_FACTOR - (ORIG_FRICTION_FACTOR-movefactor)/2)
		   * speed) / ORIG_FRICTION_FACTOR))
		speed = 1;	// always give the monster a little bit of speed
#endif

	tryx = (origx = actor->x) + (deltax = FixedMul (speed, xspeed[actor->movedir]));
	tryy = (origy = actor->y) + (deltay = FixedMul (speed, yspeed[actor->movedir]));

	// killough 3/15/98: don't jump over dropoffs:
	try_ok = P_TryMove (actor, tryx, tryy, false);

	if (try_ok && friction > ORIG_FRICTION)
	{
		actor->x = origx;
		actor->y = origy;
		movefactor *= FRACUNIT / ORIG_FRICTION_FACTOR / 4;
		actor->momx += FixedMul (deltax, movefactor);
		actor->momy += FixedMul (deltay, movefactor);
	}

	if (!try_ok)
	{
		if ((actor->flags & MF_FLOAT) && floatok)
		{ // must adjust height
			fixed_t savedz = actor->z;

			if (actor->z < tmfloorz)
				actor->z += FLOATSPEED;
			else
				actor->z -= FLOATSPEED;

			// [RH] Check to make sure there's nothing in the way of the float
			if (P_TestMobjZ (actor))
			{
				actor->flags |= MF_INFLOAT;
				return true;
			}
			actor->z = savedz;
		}

		if (!spechit.Size ())
			return false;

		// open any specials
		actor->movedir = DI_NODIR;

		// if the special is not a door that can be opened, return false
		//
		// killough 8/9/98: this is what caused monsters to get stuck in
		// doortracks, because it thought that the monster freed itself
		// by opening a door, even if it was moving towards the doortrack,
		// and not the door itself.
		//
		// killough 9/9/98: If a line blocking the monster is activated,
		// return true 90% of the time. If a line blocking the monster is
		// not activated, but some other line is, return false 90% of the
		// time. A bit of randomness is needed to ensure it's free from
		// lockups, but for most cases, it returns the correct result.
		//
		// Do NOT simply return false 1/4th of the time (causes monsters to
		// back out when they shouldn't, and creates secondary stickiness).

		line_t *ld;
		while (spechit.Pop (ld))
		{
			// [RH] let monsters push lines, as well as use them
			if (P_ActivateLine (ld, actor, 0, SPAC_USE) ||
				P_ActivateLine (ld, actor, 0, SPAC_PUSH))
			{
				good |= ld == BlockingLine ? 1 : 2;
			}
		}
		return good && ((P_Random (pr_opendoor) >= 203) ^ (good & 1));
	}
	else
	{
		actor->flags &= ~MF_INFLOAT;
	}
	return true; 
}


//
// TryWalk
// Attempts to move actor on
// in its current (ob->moveangle) direction.
// If blocked by either a wall or an actor
// returns FALSE
// If move is either clear or blocked only by a door,
// returns TRUE and sets...
// If a door is in the way,
// an OpenDoor call is made to start it opening.
//
BOOL P_TryWalk (AActor *actor)
{		
	if (!P_Move (actor))
	{
		return false;
	}

	actor->movecount = P_Random (pr_trywalk) & 15;
	return true;
}


/*
================
=
= P_NewChaseDir
=
================
*/

void P_NewChaseDir (AActor *actor)
{
	fixed_t 	deltax, deltay;
	dirtype_t	d[3];
	int			tdir;
	dirtype_t	olddir, turnaround;

	if (actor->target == NULL)
		I_Error ("P_NewChaseDir: called with no target");
				
	olddir = (dirtype_t)actor->movedir;
	turnaround = opposite[olddir];

	deltax = actor->target->x - actor->x;
	deltay = actor->target->y - actor->y;

	// [RH] Make monsters run away from frightening players
	if (actor->target->player != NULL &&
		(actor->target->player->cheats & CF_FRIGHTENING))
	{
		deltax = -deltax;
		deltay = -deltay;
	}

	if (deltax>10*FRACUNIT)
		d[1]= DI_EAST;
	else if (deltax<-10*FRACUNIT)
		d[1]= DI_WEST;
	else
		d[1]=DI_NODIR;

	if (deltay<-10*FRACUNIT)
		d[2]= DI_SOUTH;
	else if (deltay>10*FRACUNIT)
		d[2]= DI_NORTH;
	else
		d[2]=DI_NODIR;

	// try direct route
	if (d[1] != DI_NODIR && d[2] != DI_NODIR)
	{
		actor->movedir = diags[((deltay<0)<<1) + (deltax>0)];
		if (actor->movedir != turnaround && P_TryWalk(actor))
			return;
	}

	// try other directions
	if (P_Random (pr_newchasedir) > 200 || abs(deltay) > abs(deltax))
	{
		swap (d[1], d[2]);
	}

	if (d[1] == turnaround)
		d[1] = DI_NODIR;
	if (d[2] == turnaround)
		d[2] = DI_NODIR;
		
	if (d[1] != DI_NODIR)
	{
		actor->movedir = d[1];
		if (P_TryWalk (actor))
		{
			// either moved forward or attacked
			return;
		}
	}

	if (d[2] != DI_NODIR)
	{
		actor->movedir = d[2];

		if (P_TryWalk (actor))
			return;
	}

	// there is no direct path to the player, so pick another direction.
	if (olddir != DI_NODIR)
	{
		actor->movedir = olddir;

		if (P_TryWalk (actor))
			return;
	}

	// randomly determine direction of search
	if (P_Random (pr_newchasedir) & 1)	
	{
		for (tdir = DI_EAST; tdir <= DI_SOUTHEAST; tdir++)
		{
			if (tdir != turnaround)
			{
				actor->movedir = tdir;
				
				if ( P_TryWalk(actor) )
					return;
			}
		}
	}
	else
	{
		for (tdir = DI_SOUTHEAST; tdir != (DI_EAST-1); tdir--)
		{
			if (tdir != turnaround)
			{
				actor->movedir = tdir;
				
				if ( P_TryWalk(actor) )
					return;
			}
		}
	}

	if (turnaround != DI_NODIR)
	{
		actor->movedir =turnaround;
		if ( P_TryWalk(actor) )
			return;
	}

	actor->movedir = DI_NODIR;	// can not move
}


//---------------------------------------------------------------------------
//
// FUNC P_LookForMonsters
//
//---------------------------------------------------------------------------

#define MONS_LOOK_RANGE (20*64*FRACUNIT)
#define MONS_LOOK_LIMIT 64

BOOL P_LookForMonsters (AActor *actor)
{
	int count;
	AActor *mo;
	TThinkerIterator<AActor> iterator;

	if (!P_CheckSight (players[0].mo, actor))
	{ // Player can't see monster
		return false;
	}
	count = 0;
	while ( (mo = iterator.Next ()) )
	{
		if (!(mo->flags&MF_COUNTKILL) || (mo == actor) || (mo->health <= 0))
		{ // Not a valid monster
			continue;
		}
		if (P_AproxDistance (actor->x-mo->x, actor->y-mo->y)
			> MONS_LOOK_RANGE)
		{ // Out of range
			continue;
		}
		if (P_Random() < 16)
		{ // Skip
			continue;
		}
		if (count++ > MONS_LOOK_LIMIT)
		{ // Stop searching
			return false;
		}
		if (mo->IsKindOf (RUNTIME_TYPE(actor)) ||
			actor->IsKindOf (RUNTIME_TYPE(mo)))
		{ // [RH] Don't go after same species
			continue;
		}
		if (!P_CheckSight (actor, mo))
		{ // Out of sight
			continue;
		}
		// Found a target monster
		actor->target = mo;
		return true;
	}
	return false;
}

/*
================
=
= P_LookForPlayers
=
= If allaround is false, only look 180 degrees in front
= returns true if a player is targeted
================
*/

BOOL P_LookForPlayers (AActor *actor, BOOL allaround)
{
	int 		c;
	int 		stop;
	player_t*	player;
	sector_t*	sector;
	angle_t 	an;
	fixed_t 	dist;

	if (!multiplayer && players[0].health <= 0)
	{ // Single player game and player is dead; look for monsters
		return P_LookForMonsters (actor);
	}

	sector = actor->subsector->sector;	
	c = 0;
	stop = (actor->lastlook-1) & (MAXPLAYERS-1);
		
	for ( ; ; actor->lastlook = (actor->lastlook+1)&(MAXPLAYERS-1) )
	{
		if (!playeringame[actor->lastlook])
			continue;
						
		if (c++ == 2 || actor->lastlook == stop)
		{
			// done looking		// [RH] use goal as target
			if (!actor->target && actor->goal)
			{
				actor->target = actor->goal;
				return true;
			}
			// Use last known enemy if no players sighted -- killough 2/15/98:
			if (!actor->target && actor->lastenemy && actor->lastenemy->health > 0)
			{
				actor->target = actor->lastenemy;
				actor->lastenemy = NULL;
				return true;
			}
			return false;
		}
		
		player = &players[actor->lastlook];

		if (player->cheats & CF_NOTARGET)
			continue;			// no target

		if (player->health <= 0)
			continue;			// dead

		if (!P_CheckSight (actor, player->mo, false))
			continue;			// out of sight
						
		if (!allaround)
		{
			an = R_PointToAngle2 (actor->x,
								  actor->y, 
								  player->mo->x,
								  player->mo->y)
				- actor->angle;
			
			if (an > ANG90 && an < ANG270)
			{
				dist = P_AproxDistance (player->mo->x - actor->x,
										player->mo->y - actor->y);
				// if real close, react anyway
				if (dist > MELEERANGE)
					continue;	// behind back
			}
		}
		if (player->powers[pw_invisibility])
		{
			if ((P_AproxDistance (player->mo->x - actor->x,
					player->mo->y - actor->y) > 2*MELEERANGE)
				&& P_AproxDistance (player->mo->momx, player->mo->momy)
				< 5*FRACUNIT)
			{ // Player is sneaking - can't detect
				return false;
			}
			if (P_Random() < 225)
			{ // Player isn't sneaking, but still didn't detect
				return false;
			}
		}
		
		// [RH] Need to be sure the reactiontime is 0 if the monster is
		//		leaving its goal to go after a player.
		if (actor->goal && actor->target == actor->goal)
			actor->reactiontime = 0;

		actor->target = player->mo;
		return true;
	}
}

//
// ACTION ROUTINES
//

//
// A_Look
// Stay in state until a player is sighted.
// [RH] Will also leave state to move to goal.
//
void A_Look (AActor *actor)
{
	AActor *targ;

	// [RH] Set goal now if appropriate
	if (actor->special == Thing_SetGoal && actor->args[0] == 0) 
	{
		TActorIterator<APatrolPoint> iterator (actor->args[1]);
		actor->special = 0;
		actor->goal = iterator.Next ();
		actor->reactiontime = actor->args[2] * TICRATE + level.time;
	}

	actor->threshold = 0;		// any shot will wake up
	targ = actor->subsector->sector->soundtarget;

	if (targ && targ->player && (targ->player->cheats & CF_NOTARGET))
		return;

	// [RH] Andy Baker's stealth monsters
	if (actor->flags & MF_STEALTH)
	{
		actor->visdir = -1;
	}

	if (targ && (targ->flags & MF_SHOOTABLE))
	{
		actor->target = targ;

		if (actor->flags & MF_AMBUSH)
		{
			if (P_CheckSight (actor, actor->target, false))
				goto seeyou;
		}
		else
			goto seeyou;
	}
		
		
	if (!P_LookForPlayers (actor, false))
		return;
				
	// go into chase state
  seeyou:
	// [RH] Don't start chasing after a goal if it isn't time yet.
	if (actor->target == actor->goal)
	{
		if (actor->reactiontime > level.time)
			actor->target = NULL;
	}
	else if (actor->SeeSound)
	{
		char sound[MAX_SNDNAME];

		strcpy (sound, actor->SeeSound);

		if (sound[strlen(sound)-1] == '1')
		{
			sound[strlen(sound)-1] = P_Random(pr_look)%3 + '1';
			if (S_FindSound (sound) == -1)
				sound[strlen(sound)-1] = '1';
		}

		if (actor->flags2 & MF2_BOSS)
		{ // full volume
			S_Sound (actor, CHAN_VOICE, sound, 1, ATTN_SURROUND);
		}
		else
			S_Sound (actor, CHAN_VOICE, sound, 1, ATTN_NORM);
	}

	if (actor->target)
		actor->SetState (actor->SeeState);
}


/*
==============
=
= A_Chase
=
= Actor has a melee attack, so it tries to close as fast as possible
=
==============
*/

void A_Chase (AActor *actor)
{
	int delta;

	// [RH] Andy Baker's stealth monsters
	if (actor->flags & MF_STEALTH)
	{
		actor->visdir = -1;
	}

	if (actor->reactiontime)
	{
		actor->reactiontime--;
	}

	// [RH] Don't chase invisible targets
	if (actor->target != NULL && actor->target->renderflags & RF_INVISIBLE)
	{
		actor->target = NULL;
	}

	// modify target threshold
	if (actor->threshold)
	{
		if (actor->target == NULL || actor->target->health <= 0)
		{
			actor->threshold = 0;
		}
		else
		{
			actor->threshold--;
		}
	}

	if (gameinfo.gametype == GAME_Heretic &&
		(*gameskill == sk_nightmare || (*dmflags & DF_FAST_MONSTERS)))
	{ // Monsters move faster in nightmare mode
		actor->tics -= actor->tics / 2;
		if (actor->tics < 3)
		{
			actor->tics = 3;
		}
	}

	// turn towards movement direction if not there yet
	if (actor->movedir < 8)
	{
		actor->angle &= (angle_t)(7<<29);
		delta = actor->angle - (actor->movedir << 29);
		if (delta > 0)
		{
			actor->angle -= ANG90/2;
		}
		else if (delta < 0)
		{
			actor->angle += ANG90/2;
		}
	}

	// [RH] If the target is dead (and not a goal), stop chasing it.
	if (actor->target && actor->target != actor->goal && actor->target->health <= 0)
		actor->target = NULL;

	if (!actor->target || !(actor->target->flags & MF_SHOOTABLE))
	{ // look for a new target
		if (P_LookForPlayers (actor, true) && actor->target != actor->goal)
		{ // got a new target
			return;
		}
		if (actor->target == NULL)
		{
			actor->SetState (actor->SpawnState);
			return;
		}
	}
	
	// do not attack twice in a row
	if (actor->flags & MF_JUSTATTACKED)
	{
		actor->flags &= ~MF_JUSTATTACKED;
		if ((*gameskill != sk_nightmare) && !(*dmflags & DF_FAST_MONSTERS))
			P_NewChaseDir (actor);
		return;
	}
	
	// [RH] Don't attack if just moving toward goal
	if (actor->target == actor->goal)
	{
		if (P_CheckMeleeRange (actor))
		{
			// reached the goal
			TActorIterator<APatrolPoint> iterator (actor->goal->args[0]);
			actor->reactiontime = actor->goal->args[1] * TICRATE + level.time;
			actor->goal = iterator.Next ();
			actor->target = NULL;
			actor->flags |= MF_JUSTATTACKED;
			actor->SetState (actor->SpawnState);
			return;
		}
		goto nomissile;
	}

	// [RH] Scared monsters attack less frequently
	if (actor->target->player == NULL ||
		!(actor->target->player->cheats & CF_FRIGHTENING) ||
		P_Random (pr_scaredycat) < 43)
	{
		// check for melee attack
		if (actor->MeleeState && P_CheckMeleeRange (actor))
		{
			if (actor->AttackSound)
				S_Sound (actor, CHAN_WEAPON, actor->AttackSound, 1, ATTN_NORM);

			actor->SetState (actor->MeleeState);
			return;
		}
		
		// check for missile attack
		if (actor->MissileState)
		{
			if (*gameskill < sk_nightmare
				&& actor->movecount && !(*dmflags & DF_FAST_MONSTERS))
			{
				goto nomissile;
			}
			
			if (!P_CheckMissileRange (actor))
				goto nomissile;
			
			actor->SetState (actor->MissileState);
			actor->flags |= MF_JUSTATTACKED;
			return;
		}
	}

 nomissile:
	// possibly choose another target
	if (multiplayer
		&& !actor->threshold
		&& !P_CheckSight (actor, actor->target, false) )
	{
		if (P_LookForPlayers (actor, true))
			return; 	// got a new target
	}
	
	// chase towards player
	if (--actor->movecount < 0 || !P_Move (actor))
	{
		P_NewChaseDir (actor);
	}
	
	// make active sound
	if (actor->ActiveSound && P_Random (pr_chase) < 3)
	{
		const char *sound;

		if ((actor->flags3 & MF3_SEEISALSOACTIVE) && P_Random (pr_chase) < 128)
		{
			sound = actor->SeeSound;
		}
		else
		{
			sound = actor->ActiveSound;
		}
		if (S_CheckSound (sound))
		{ // [RH] Only play this sound if it won't cut out another of the same type.
			S_Sound (actor, CHAN_VOICE, sound, 1,
				(actor->flags3 & MF3_FULLVOLACTIVE) ? ATTN_NONE : ATTN_IDLE);
		}
	}
}


//
// A_FaceTarget
//
void A_FaceTarget (AActor *actor)
{
	if (!actor->target)
		return;

	// [RH] Andy Baker's stealth monsters
	if (actor->flags & MF_STEALTH)
	{
		actor->visdir = 1;
	}

	actor->flags &= ~MF_AMBUSH;
	actor->angle = R_PointToAngle2 (actor->x, actor->y,
									actor->target->x, actor->target->y);
	
	if (actor->target->flags & MF_SHADOW)
    {
		actor->angle += PS_Random(pr_facetarget) << 21;
    }
}

//
// [RH] A_MonsterRail
//
// New function to let monsters shoot a railgun
//
void A_MonsterRail (AActor *actor)
{
	if (!actor->target)
		return;

	// [RH] Andy Baker's stealth monsters
	if (actor->flags & MF_STEALTH)
	{
		actor->visdir = 1;
	}

	actor->flags &= ~MF_AMBUSH;
		
	actor->angle = R_PointToAngle2 (actor->x,
									actor->y,
									actor->target->x,
									actor->target->y);

	actor->pitch = P_AimLineAttack (actor, actor->angle, MISSILERANGE);

	// Let the aim trail behind the player
	actor->angle = R_PointToAngle2 (actor->x,
									actor->y,
									actor->target->x - actor->target->momx * 3,
									actor->target->y - actor->target->momy * 3);

	if (actor->target->flags & MF_SHADOW)
    {
		actor->angle += PS_Random(pr_facetarget) << 21;
    }

	P_RailAttack (actor, actor->damage, 0);
}

void A_Scream (AActor *actor)
{
	if (actor->DeathSound)
	{
		char sound[MAX_SNDNAME];

		strcpy (sound, actor->DeathSound);

		if (sound[strlen(sound)-1] == '1')
		{
			sound[strlen(sound)-1] = P_Random(pr_look)%3 + '1';
			if (S_FindSound (sound) == -1)
				sound[strlen(sound)-1] = '1';
		}

		// Check for bosses.
		if (actor->flags2 & MF2_BOSS)
		{
			// full volume
			S_Sound (actor, CHAN_VOICE, sound, 1, ATTN_SURROUND);
		}
		else
			S_Sound (actor, CHAN_VOICE, sound, 1, ATTN_NORM);
	}
}

void A_XScream (AActor *actor)
{
	if (actor->player)
		S_Sound (actor, CHAN_VOICE, "*gibbed", 1, ATTN_NORM);
	else
		S_Sound (actor, CHAN_VOICE, "misc/gibbed", 1, ATTN_NORM);
}

//---------------------------------------------------------------------------
//
// PROC P_DropItem
//
//---------------------------------------------------------------------------

void P_DropItem (AActor *source, const TypeInfo *type, int special, int chance)
{
	if (P_Random() <= chance)
	{
		AActor *mo = Spawn (type, source->x, source->y,
			source->z + (source->height>>1));
		mo->momx = PS_Random() << 8;
		mo->momy = PS_Random() << 8;
		mo->momz = FRACUNIT*5 + (P_Random() << 10);
		mo->flags |= MF_DROPPED;
		if (special >= 0)
		{
			mo->health = special;
		}
	}
}

void A_Pain (AActor *actor)
{
	// [RH] Have some variety in player pain sounds
	if (actor->player && actor->player->morphTics == 0)
	{
		int l, r = (P_Random (pr_playerpain) & 1) + 1;
		char nametemp[128];

		if (actor->health < 25)
			l = 25;
		else if (actor->health < 50)
			l = 50;
		else if (actor->health < 75)
			l = 75;
		else
			l = 100;

		sprintf (nametemp, "*pain%d_%d", l, r);
		S_Sound (actor, CHAN_VOICE, nametemp, 1, ATTN_NORM);
	}
	else if (actor->PainSound)
	{
		S_Sound (actor, CHAN_VOICE, actor->PainSound, 1, ATTN_NORM);
	}
}

// killough 11/98: kill an object
void A_Die (AActor *actor)
{
	P_DamageMobj (actor, NULL, NULL, actor->health, MOD_UNKNOWN);
}

//
// A_Detonate
// killough 8/9/98: same as A_Explode, except that the damage is variable
//

void A_Detonate (AActor *mo)
{
	P_RadiusAttack (mo, mo->target, mo->damage, mo->damage, true, MOD_UNKNOWN);
}

//
// A_Explode
//
void A_Explode (AActor *thing)
{
	int damage = 128;
	int distance = 128;
	bool hurtSource = true;

	thing->PreExplode ();
	thing->GetExplodeParms (damage, distance, hurtSource);
	P_RadiusAttack (thing, thing->target, damage, distance, hurtSource, thing->GetMOD ());
	if (thing->z <= thing->floorz + (distance<<FRACBITS))
	{
		P_HitFloor (thing);
	}
}

//
// A_BossDeath
// Possibly trigger special effects if on a boss level
//
void A_BossDeath (AActor *actor)
{
	int i;
	enum
	{
		MT_FATSO,
		MT_BABY,
		MT_BRUISER,
		MT_CYBORG,
		MT_SPIDER,

		MT_HEAD,
		MT_MINOTAUR,
		MT_SORCERER2
	} type;

	// [RH] These all depend on the presence of level flags now
	//		rather than being hard-coded to specific levels/episodes.

	if ((level.flags & (LEVEL_MAP07SPECIAL|
						LEVEL_BRUISERSPECIAL|
						LEVEL_CYBORGSPECIAL|
						LEVEL_SPIDERSPECIAL|
						LEVEL_HEADSPECIAL|
						LEVEL_MINOTAURSPECIAL|
						LEVEL_SORCERER2SPECIAL)) == 0)
		return;

	if (strcmp (RUNTIME_TYPE(actor)->Name+1, "Fatso") == 0)
		type = MT_FATSO;
	else if (strcmp (RUNTIME_TYPE(actor)->Name+1, "Arachnotron") == 0)
		type = MT_BABY;
	else if (strcmp (RUNTIME_TYPE(actor)->Name+1, "BaronOfHell") == 0)
		type = MT_BRUISER;
	else if (strcmp (RUNTIME_TYPE(actor)->Name+1, "Cyberdemon") == 0)
		type = MT_CYBORG;
	else if (strcmp (RUNTIME_TYPE(actor)->Name+1, "SpiderMastermind") == 0)
		type = MT_SPIDER;
	else if (strcmp (RUNTIME_TYPE(actor)->Name+1, "Ironlich") == 0)
		type = MT_HEAD;
	else if (strcmp (RUNTIME_TYPE(actor)->Name+1, "Minotaur") == 0)
		type = MT_MINOTAUR;
	else if (strcmp (RUNTIME_TYPE(actor)->Name+1, "Sorcerer2") == 0)
		type = MT_SORCERER2;
	else
		return;

	if (
		((level.flags & LEVEL_MAP07SPECIAL) && (type == MT_FATSO || type == MT_BABY)) ||
		((level.flags & LEVEL_BRUISERSPECIAL) && (type == MT_BRUISER)) ||
		((level.flags & LEVEL_CYBORGSPECIAL) && (type == MT_CYBORG)) ||
		((level.flags & LEVEL_SPIDERSPECIAL) && (type == MT_SPIDER)) ||
		((level.flags & LEVEL_HEADSPECIAL) && (type == MT_HEAD)) ||
		((level.flags & LEVEL_MINOTAURSPECIAL) && (type == MT_MINOTAUR)) ||
		((level.flags & LEVEL_SORCERER2SPECIAL) && (type == MT_SORCERER2))
	   )
		;
	else
		return;

	// make sure there is a player alive for victory
	for (i = 0; i < MAXPLAYERS; i++)
		if (playeringame[i] && players[i].health > 0)
			break;
	
	if (i == MAXPLAYERS)
		return; // no one left alive, so do not end game
	
	// Make sure all bosses are dead
	TThinkerIterator<AActor> iterator;
	AActor *other;

	while ( (other = iterator.Next ()) )
	{
		if (other != actor && other->health > 0 && other->IsKindOf (RUNTIME_TYPE(actor)))
		{ // Found a living boss
			return;
		}
	}
		
	// victory!
	if (level.flags & LEVEL_SPECKILLMONSTERS)
	{ // Kill any remaining monsters
		P_Massacre ();
	}
	if (level.flags & LEVEL_MAP07SPECIAL)
	{
		if (type == MT_FATSO)
		{
			EV_DoFloor (DFloor::floorLowerToLowest, NULL, 666, FRACUNIT, 0, 0, 0);
			return;
		}
		
		if (type == MT_BABY)
		{
			EV_DoFloor (DFloor::floorRaiseByTexture, NULL, 667, FRACUNIT, 0, 0, 0);
			return;
		}
	}
	else
	{
		switch (level.flags & LEVEL_SPECACTIONSMASK)
		{
			case LEVEL_SPECLOWERFLOOR:
				EV_DoFloor (DFloor::floorLowerToLowest, NULL, 666, FRACUNIT, 0, 0, 0);
				return;
			
			case LEVEL_SPECOPENDOOR:
				EV_DoDoor (DDoor::doorOpen, NULL, NULL, 666, 8*TICRATE, 0, NoKey);
				return;
		}
	}

	// [RH] If noexit, then don't end the level.
	if ((*deathmatch || *alwaysapplydmflags) && (*dmflags & DF_NO_EXIT))
		return;

	G_ExitLevel (0);
}

//----------------------------------------------------------------------------
//
// PROC P_Massacre
//
// Kills all monsters.
//
//----------------------------------------------------------------------------

int P_Massacre ()
{
	// jff 02/01/98 'em' cheat - kill all monsters
	// partially taken from Chi's .46 port
	//
	// killough 2/7/98: cleaned up code and changed to use dprintf;
	// fixed lost soul bug (LSs left behind when PEs are killed)

	int killcount = 0;
	AActor *actor;
	TThinkerIterator<AActor> iterator;

	while ( (actor = iterator.Next ()) )
	{
		if ((actor->flags & MF_SHOOTABLE) && (actor->flags & MF_COUNTKILL ||
			(gameinfo.gametype == GAME_Doom && actor->IsKindOf (RUNTIME_CLASS(ALostSoul)))))
		{
			// killough 3/6/98: kill even if PE is dead
			if (actor->health > 0)
			{
				killcount++;
				actor->flags2 &= ~(MF2_DORMANT|MF2_INVULNERABLE);
				P_DamageMobj (actor, NULL, NULL, 10000, MOD_UNKNOWN);
			}
			if (actor->IsKindOf (RUNTIME_CLASS(APainElemental)))
			{
				FState *deadstate;
				A_NoBlocking (actor);	// [RH] Use this instead of A_PainDie
				deadstate = actor->DeathState;
				if (deadstate != NULL)
				{
					while (deadstate->nextstate != NULL)
						deadstate = deadstate->nextstate;
					actor->SetState (deadstate);
				}
			}
		}
	}
	return killcount;
}

//
// A_SinkMobj
// Sink a mobj incrementally into the floor
//

bool A_SinkMobj (AActor *actor)
{
	if (actor->floorclip < actor->height)
	{
		actor->floorclip += actor->GetSinkSpeed ();
		return false;
	}
	return true;
}

//
// A_RaiseMobj
// Raise a mobj incrementally from the floor to 
// 

bool A_RaiseMobj (AActor *actor)
{
	bool done = true;

	// Raise a mobj from the ground
	if (actor->floorclip > 0)
	{
		actor->floorclip -= actor->GetRaiseSpeed ();
		if (actor->floorclip <= 0)
		{
			actor->floorclip = 0;
			done = true;
		}
		else
		{
			done = false;
		}
	}
	return done;		// Reached target height
}
