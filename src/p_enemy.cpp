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

#include "gi.h"

CVAR (testgibs, "0", 0)

enum dirtype_t
{
	DI_EAST,
	DI_NORTHEAST,
	DI_NORTH,
	DI_NORTHWEST,
	DI_WEST,
	DI_SOUTHWEST,
	DI_SOUTH,
	DI_SOUTHEAST,
	DI_NODIR,
	NUMDIRS
};

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



void A_Fall (AActor *actor);


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
		
	for (i=0 ;i<sec->linecount ; i++)
	{
		check = sec->lines[i];
		if (! (check->flags & ML_TWOSIDED) )
			continue;
		
		P_LineOpening (check);

		if (openrange <= 0)
			continue;	// closed door
		
		if ( sides[ check->sidenum[0] ].sector == sec)
			other = sides[ check->sidenum[1] ] .sector;
		else
			other = sides[ check->sidenum[0] ].sector;
		
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

	if (dist >= MELEERANGE-20*FRACUNIT+pl->info->radius)
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
	
	if (!actor->info->meleestate)
		dist -= 128*FRACUNIT;	// no melee attack, so fire more

	dist >>= 16;

	if (actor->type == MT_VILE)
	{
		if (dist > 14*64)		
			return false;		// too far away
	}
		

	if (actor->type == MT_UNDEAD)
	{
		if (dist < 196) 
			return false;		// close for fist attack
		dist >>= 1;
	}
		

	if (actor->type == MT_CYBORG
		|| actor->type == MT_SPIDER
		|| actor->type == MT_SKULL)
	{
		dist >>= 1;
	}
	
	if (dist > 200)
		dist = 200;
				
	if (actor->type == MT_CYBORG && dist > 160)
		dist = 160;
				
	if (P_Random (pr_checkmissilerange) < dist)
		return false;
				
	return true;
}


//
// P_Move
// Move in the current direction,
// returns false if the move is blocked.
//
extern	line_t	**spechit;
extern	int 	numspechit;

BOOL P_Move (AActor *actor)
{
	fixed_t tryx, tryy, deltax, deltay, origx, origy;
	BOOL try_ok;
	int good;
	int speed;
	int movefactor = ORIG_FRICTION_FACTOR;
	int friction = ORIG_FRICTION;

	if (actor->movedir == DI_NODIR)
		return false;

	if ((unsigned)actor->movedir >= 8)
		I_Error ("Weird actor->movedir!");
	
	speed = actor->info->speed;

#if 0	// [RH] I'm not so sure this is such a good idea
	// killough 10/98: make monsters get affected by ice and sludge too:
	movefactor = P_GetMoveFactor (actor, &friction);

	if (friction < ORIG_FRICTION &&		// sludge
		!(speed = ((ORIG_FRICTION_FACTOR - (ORIG_FRICTION_FACTOR-movefactor)/2)
		   * speed) / ORIG_FRICTION_FACTOR))
		speed = 1;	// always give the monster a little bit of speed
#endif

	tryx = (origx = actor->x) + (deltax = speed * xspeed[actor->movedir]);
	tryy = (origy = actor->y) + (deltay = speed * yspeed[actor->movedir]);

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
		// open any specials
		if (actor->flags & MF_FLOAT && floatok)
		{
			// must adjust height
			if (actor->z < tmfloorz)
				actor->z += FLOATSPEED;
			else
				actor->z -= FLOATSPEED;

			actor->flags |= MF_INFLOAT;
			return true;
		}

		if (!numspechit)
			return false;

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

		for (good = 0; numspechit > 0; )
		{
			line_t *ld = spechit[--numspechit];

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
		
	if (!(actor->flags & MF_FLOAT))
	{
		if (actor->z > actor->floorz)
		{
			P_HitFloor (actor);
		}
		actor->z = actor->floorz;
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




void P_NewChaseDir (AActor *actor)
{
	fixed_t 	deltax;
	fixed_t 	deltay;
	
	dirtype_t	d[3];
	
	int			tdir;
	dirtype_t	olddir;
	
	dirtype_t	turnaround;

	if (!actor->target)
		I_Error ("P_NewChaseDir: called with no target");
				
	olddir = (dirtype_t)actor->movedir;
	turnaround = opposite[olddir];

	deltax = actor->target->x - actor->x;
	deltay = actor->target->y - actor->y;

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
		tdir = d[1];
		d[1] = d[2];
		d[2] = (dirtype_t)tdir;
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

	// there is no direct path to the player,
	// so pick another direction.
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



//
// P_LookForPlayers
// If allaround is false, only look 180 degrees in front.
// Returns true if a player is targeted.
//
BOOL P_LookForPlayers (AActor *actor, BOOL allaround)
{
	int 		c;
	int 		stop;
	player_t*	player;
	sector_t*	sector;
	angle_t 	an;
	fixed_t 	dist;
				
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
			if (!actor->target && actor->goal) {
				actor->target = actor->goal;
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
		
		// [RH] Need to be sure the reactiontime is 0 if the monster is
		//		leaving its goal to go after a player.
		if (actor->goal && actor->target == actor->goal)
			actor->reactiontime = 0;

		actor->target = player->mo;
		return true;
	}

	// Note if using cxx: Despite what the warning claims, the following
	// code really is reachable.

	// Use last known enemy if no players sighted -- killough 2/15/98:
	if (actor->lastenemy && actor->lastenemy->health > 0)
	{
		actor->target = actor->lastenemy;
		actor->lastenemy = NULL;
		return true;
	}

	// [RH] Use goal as a last resort
	if (!actor->target && actor->goal) {
		actor->target = actor->goal;
		return true;
	}
	return false;

}


//
// A_KeenDie
// DOOM II special, map 32.
// Uses special tag 666.
//
void A_KeenDie (AActor *actor)
{
	A_Fall (actor);
	
	// scan the remaining thinkers
	// to see if all Keens are dead
	AActor *other;
	TThinkerIterator<AActor> iterator;

	while ( (other = iterator.Next ()) )
	{
		if (other != actor && other->type == actor->type && other->health > 0)
		{
			// other Keen not dead
			return;
		}
	}

	EV_DoDoor (DDoor::doorOpen, NULL, NULL, 666, 2*FRACUNIT, 0, NoKey);
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
		actor->special = 0;
		actor->goal = AActor::FindGoal (NULL, actor->args[1], MT_PATHNODE);
		actor->reactiontime = actor->args[2] * TICRATE + level.time;
	}

	actor->threshold = 0;		// any shot will wake up
	targ = actor->subsector->sector->soundtarget;

	if (targ && targ->player && (targ->player->cheats & CF_NOTARGET))
		return;

	// [RH] Andy Baker's stealth monsters
	if (actor->flags & MF_STEALTH)
	{
		actor->visdir = 1;
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
	else if (actor->info->seesound)
	{
		char sound[MAX_SNDNAME];

		strcpy (sound, actor->info->seesound);

		if (sound[strlen(sound)-1] == '1')
		{
			sound[strlen(sound)-1] = P_Random(pr_look)%3 + '1';
			if (S_FindSound (sound) == -1)
				sound[strlen(sound)-1] = '1';
		}

		if (actor->flags2 & MF2_BOSS)
		{
			// full volume
			S_Sound (actor, CHAN_VOICE, sound, 1, ATTN_SURROUND);
		}
		else
			S_Sound (actor, CHAN_VOICE, sound, 1, ATTN_NORM);
	}

	if (actor->target)
		P_SetMobjState (actor, actor->info->seestate);
}


//
// A_Chase
// Actor has a melee attack,
// so it tries to close as fast as possible
//
void A_Chase (AActor *actor)
{
	int delta;

	// [RH] Andy Baker's stealth monsters
	if (actor->flags & MF_STEALTH)
	{
		actor->visdir = -1;
	}

	if (actor->reactiontime)
		actor->reactiontime--;
								
	// modify target threshold
	if (actor->threshold)
	{
		if (!actor->target || actor->target->health <= 0)
		{
			actor->threshold = 0;
		}
		else
			actor->threshold--;
	}
	
	// turn towards movement direction if not there yet
	if (actor->movedir < 8)
	{
		actor->angle &= (angle_t)(7<<29);
		delta = actor->angle - (actor->movedir << 29);
		
		if (delta > 0)
			actor->angle -= ANG90/2;
		else if (delta < 0)
			actor->angle += ANG90/2;
	}

	// [RH] If the target is dead (and not a goal), stop chasing it.
	if (actor->target && actor->target != actor->goal && actor->target->health <= 0)
		actor->target = NULL;

	if (!actor->target || !(actor->target->flags & MF_SHOOTABLE))
	{
		// look for a new target
		if (P_LookForPlayers (actor, true) && actor->target != actor->goal)
			return; 	// got a new target
		
		if (actor->target == NULL) {
			P_SetMobjState (actor, actor->info->spawnstate);
			return;
		}
	}
	
	// do not attack twice in a row
	if (actor->flags & MF_JUSTATTACKED)
	{
		actor->flags &= ~MF_JUSTATTACKED;
		if ((gameskill.value != sk_nightmare) && !(dmflags & DF_FAST_MONSTERS))
			P_NewChaseDir (actor);
		return;
	}
	
	// [RH] Don't attack if just moving toward goal
	if (actor->target == actor->goal) {
		if (P_CheckMeleeRange (actor)) {
			// reached the goal
			actor->reactiontime = actor->goal->args[1] * TICRATE + level.time;
			actor->goal = AActor::FindGoal (NULL, actor->goal->args[0], MT_PATHNODE);
			actor->target = NULL;
			P_SetMobjState (actor, actor->info->spawnstate);
			return;
		}
		goto nomissile;
	}

	// check for melee attack
	if (actor->info->meleestate && P_CheckMeleeRange (actor))
	{
		if (actor->info->attacksound)
			S_Sound (actor, CHAN_WEAPON, actor->info->attacksound, 1, ATTN_NORM);

		P_SetMobjState (actor, actor->info->meleestate);
		return;
	}
	
	// check for missile attack
	if (actor->info->missilestate)
	{
		if (gameskill.value < sk_nightmare
			&& actor->movecount && !(dmflags & DF_FAST_MONSTERS))
		{
			goto nomissile;
		}
		
		if (!P_CheckMissileRange (actor))
			goto nomissile;
		
		P_SetMobjState (actor, actor->info->missilestate);
		actor->flags |= MF_JUSTATTACKED;
		return;
	}

	// ?
  nomissile:
	// possibly choose another target
	if (multiplayer
		&& !actor->threshold
		&& !P_CheckSight (actor, actor->target, false) )
	{
		if (P_LookForPlayers(actor,true))
			return; 	// got a new target
	}
	
	// chase towards player
	if (--actor->movecount < 0 || !P_Move (actor))
	{
		P_NewChaseDir (actor);
	}
	
	// make active sound
	if (actor->info->activesound && P_Random (pr_chase) < 3)
	{
		S_Sound (actor, CHAN_VOICE, actor->info->activesound, 1, ATTN_IDLE);
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
		
	actor->angle = R_PointToAngle2 (actor->x,
									actor->y,
									actor->target->x,
									actor->target->y);
	
	if (actor->target->flags & MF_SHADOW)
    {
		int t = P_Random(pr_facetarget);
		actor->angle += (t-P_Random(pr_facetarget))<<21;
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

	//actor->pitch = tantoangle[P_AimLineAttack (actor, actor->angle, MISSILERANGE) >> DBITS];
	actor->pitch = -(int)(tan ((float)P_AimLineAttack (actor, actor->angle, MISSILERANGE)/65536.0f)*ANGLE_180/PI);

	// Let the aim trail behind the player
	actor->angle = R_PointToAngle2 (actor->x,
									actor->y,
									actor->target->x - actor->target->momx * 3,
									actor->target->y - actor->target->momy * 3);

	if (actor->target->flags & MF_SHADOW)
    {
		int t = P_Random(pr_facetarget);
		actor->angle += (t-P_Random(pr_facetarget))<<21;
    }

	P_RailAttack (actor, actor->info->damage, 0);
}

//
// A_PosAttack
//
void A_PosAttack (AActor *actor)
{
	int angle;
	int damage;
	int slope;
	int t;
		
	if (!actor->target)
		return;
				
	A_FaceTarget (actor);
	angle = actor->angle;
	slope = P_AimLineAttack (actor, angle, MISSILERANGE);

	S_Sound (actor, CHAN_WEAPON, "grunt/attack", 1, ATTN_NORM);
	t = P_Random(pr_posattack);
	angle += (t - P_Random (pr_posattack))<<20;
	damage = ((P_Random (pr_posattack)%5)+1)*3;
	P_LineAttack (actor, angle, MISSILERANGE, slope, damage);
}

void A_SPosAttack (AActor *actor)
{
	int i;
	int bangle;
	int slope;
		
	if (!actor->target)
		return;

	S_Sound (actor, CHAN_WEAPON, "shotguy/attack", 1, ATTN_NORM);
	A_FaceTarget (actor);
	bangle = actor->angle;
	slope = P_AimLineAttack (actor, bangle, MISSILERANGE);

	for (i=0 ; i<3 ; i++)
    {
		int t = P_Random (pr_sposattack);
		int angle = bangle + ((t - P_Random (pr_sposattack))<<20);
		int damage = ((P_Random (pr_sposattack)%5)+1)*3;
		P_LineAttack(actor, angle, MISSILERANGE, slope, damage);
    }
}

void A_CPosAttack (AActor *actor)
{
	int angle;
	int bangle;
	int damage;
	int slope;
	int t;
		
	if (!actor->target)
		return;

	// [RH] Andy Baker's stealth monsters
	if (actor->flags & MF_STEALTH)
	{
		actor->visdir = 1;
	}

	S_Sound (actor, CHAN_WEAPON, "chainguy/attack", 1, ATTN_NORM);
	A_FaceTarget (actor);
	bangle = actor->angle;
	slope = P_AimLineAttack (actor, bangle, MISSILERANGE);

	t = P_Random (pr_cposattack);
	angle = bangle + ((t - P_Random (pr_cposattack))<<20);
	damage = ((P_Random (pr_cposattack)%5)+1)*3;
	P_LineAttack (actor, angle, MISSILERANGE, slope, damage);
}

void A_CPosRefire (AActor *actor)
{		
	// keep firing unless target got out of sight
	A_FaceTarget (actor);

	if (P_Random (pr_cposrefire) < 40)
		return;

	if (!actor->target
		|| actor->target->health <= 0
		|| !P_CheckSight (actor, actor->target, false) )
	{
		P_SetMobjState (actor, actor->info->seestate);
	}
}


void A_SpidRefire (AActor *actor)
{		
	// keep firing unless target got out of sight
	A_FaceTarget (actor);

	if (P_Random (pr_spidrefire) < 10)
		return;

	if (!actor->target
		|| actor->target->health <= 0
		|| !P_CheckSight (actor, actor->target, false) )
	{
		P_SetMobjState (actor, actor->info->seestate);
	}
}

void A_BspiAttack (AActor *actor)
{		
	if (!actor->target)
		return;

	// [RH] Andy Baker's stealth monsters
	if (actor->flags & MF_STEALTH)
	{
		actor->visdir = 1;
	}

	A_FaceTarget (actor);

	// launch a missile
	P_SpawnMissile (actor, actor->target, MT_ARACHPLAZ);
}


//
// A_TroopAttack
//
void A_TroopAttack (AActor *actor)
{
	if (!actor->target)
		return;
				
	A_FaceTarget (actor);
	if (P_CheckMeleeRange (actor))
	{
		int damage = (P_Random (pr_troopattack)%8+1)*3;
		S_Sound (actor, CHAN_WEAPON, "imp/melee", 1, ATTN_NORM);
		P_DamageMobj (actor->target, actor, actor, damage, MOD_HIT);
		return;
	}
	
	// launch a missile
	P_SpawnMissile (actor, actor->target, MT_TROOPSHOT);
}


void A_SargAttack (AActor *actor)
{
	if (!actor->target)
		return;
				
	A_FaceTarget (actor);
	if (P_CheckMeleeRange (actor))
	{
		int damage = ((P_Random (pr_sargattack)%10)+1)*4;
		P_DamageMobj (actor->target, actor, actor, damage, MOD_HIT);
	}
}

void A_HeadAttack (AActor *actor)
{
	if (!actor->target)
		return;
				
	A_FaceTarget (actor);
	if (P_CheckMeleeRange (actor))
	{
		int damage = (P_Random (pr_headattack)%6+1)*10;
		P_DamageMobj (actor->target, actor, actor, damage, MOD_HIT);
		return;
	}
	
	// launch a missile
	P_SpawnMissile (actor, actor->target, MT_HEADSHOT);
}

void A_CyberAttack (AActor *actor)
{
	AActor *rocket;

	if (!actor->target)
		return;
				
	A_FaceTarget (actor);
	rocket = P_SpawnMissile (actor, actor->target, MT_ROCKET);
	if (rocket)
		rocket->effects = FX_ROCKET;		// [RH] leave a trail behind the rocket
}


void A_BruisAttack (AActor *actor)
{
	if (!actor->target)
		return;
				
	if (P_CheckMeleeRange (actor))
	{
		int damage = (P_Random (pr_bruisattack)%8+1)*10;
		S_Sound (actor, CHAN_WEAPON, "baron/melee", 1, ATTN_NORM);
		P_DamageMobj (actor->target, actor, actor, damage, MOD_HIT);
		return;
	}
	
	// launch a missile
	P_SpawnMissile (actor, actor->target, MT_BRUISERSHOT);
}


//
// A_SkelMissile
//
void A_SkelMissile (AActor *actor)
{		
	AActor *mo;
		
	if (!actor->target)
		return;
				
	A_FaceTarget (actor);
	actor->z += 16*FRACUNIT;	// so missile spawns higher
	mo = P_SpawnMissile (actor, actor->target, MT_TRACER);
	actor->z -= 16*FRACUNIT;	// back to normal

	mo->x += mo->momx;
	mo->y += mo->momy;
	mo->tracer = actor->target;
}

#define TRACEANGLE (0xc000000)

void A_Tracer (AActor *actor)
{
	angle_t 	exact;
	fixed_t 	dist;
	fixed_t 	slope;
	AActor* 	dest;
	AActor* 	th;
				
	// killough 1/18/98: this is why some missiles do not have smoke
	// and some do. Also, internal demos start at random gametics, thus
	// the bug in which revenants cause internal demos to go out of sync.
	//
	// killough 3/6/98: fix revenant internal demo bug by subtracting
	// levelstarttic from gametic:
	//
	// [RH] level.time is always 0-based, so nothing special to do here.

	if (level.time & 3)
		return;
	
	// spawn a puff of smoke behind the rocket			
	P_SpawnPuff (actor->x, actor->y, actor->z, 0, 3);
		
	th = new AActor (actor->x - actor->momx,
					 actor->y - actor->momy,
					 actor->z, MT_SMOKE);
	
	th->momz = FRACUNIT;
	th->tics -= P_Random (pr_tracer)&3;
	if (th->tics < 1)
		th->tics = 1;
	
	// adjust direction
	dest = actor->tracer;
		
	if (!dest || dest->health <= 0)
		return;
	
	// change angle 	
	exact = R_PointToAngle2 (actor->x,
							 actor->y,
							 dest->x,
							 dest->y);

	if (exact != actor->angle)
	{
		if (exact - actor->angle > 0x80000000)
		{
			actor->angle -= TRACEANGLE;
			if (exact - actor->angle < 0x80000000)
				actor->angle = exact;
		}
		else
		{
			actor->angle += TRACEANGLE;
			if (exact - actor->angle > 0x80000000)
				actor->angle = exact;
		}
	}
		
	exact = actor->angle>>ANGLETOFINESHIFT;
	actor->momx = FixedMul (actor->info->speed, finecosine[exact]);
	actor->momy = FixedMul (actor->info->speed, finesine[exact]);
	
	// change slope
	dist = P_AproxDistance (dest->x - actor->x,
							dest->y - actor->y);
	
	dist = dist / actor->info->speed;

	if (dist < 1)
		dist = 1;
	slope = (dest->z+40*FRACUNIT - actor->z) / dist;

	if (slope < actor->momz)
		actor->momz -= FRACUNIT/8;
	else
		actor->momz += FRACUNIT/8;
}


void A_SkelWhoosh (AActor *actor)
{
	if (!actor->target)
		return;
	A_FaceTarget (actor);
	S_Sound (actor, CHAN_WEAPON, "skeleton/swing", 1, ATTN_NORM);
}

void A_SkelFist (AActor *actor)
{
	if (!actor->target)
		return;
				
	A_FaceTarget (actor);
		
	if (P_CheckMeleeRange (actor))
	{
		int damage = ((P_Random (pr_skelfist)%10)+1)*6;
		S_Sound (actor, CHAN_WEAPON, "skeleton/melee", 1, ATTN_NORM);
		P_DamageMobj (actor->target, actor, actor, damage, MOD_HIT);
	}
}



//
// PIT_VileCheck
// Detect a corpse that could be raised.
//
AActor* 		corpsehit;
AActor* 		vileobj;
fixed_t 		viletryx;
fixed_t 		viletryy;

BOOL PIT_VileCheck (AActor *thing)
{
	int 	maxdist;
	BOOL 	check;
		
	if (!(thing->flags & MF_CORPSE) )
		return true;	// not a monster
	
	if (thing->tics != -1)
		return true;	// not lying still yet
	
	if (thing->info->raisestate == S_NULL)
		return true;	// monster doesn't have a raise state
	
	maxdist = thing->info->radius + mobjinfo[MT_VILE].radius;
		
	if ( abs(thing->x - viletryx) > maxdist
		 || abs(thing->y - viletryy) > maxdist )
		return true;			// not actually touching
				
	corpsehit = thing;
	corpsehit->momx = corpsehit->momy = 0;
	// [RH] Check against real height and radius
	{
		fixed_t oldheight = corpsehit->height;
		fixed_t oldradius = corpsehit->radius;
		int oldflags = corpsehit->flags;

		corpsehit->flags |= MF_SOLID;
		corpsehit->height = corpsehit->info->height;
		check = P_CheckPosition (corpsehit, corpsehit->x, corpsehit->y);
		corpsehit->flags = oldflags;
		corpsehit->radius = oldradius;
		corpsehit->height = oldheight;
	}

	return !check;
}



//
// A_VileChase
// Check for ressurecting a body
//
void A_VileChase (AActor *actor)
{
	int 				xl;
	int 				xh;
	int 				yl;
	int 				yh;
	
	int 				bx;
	int 				by;

	mobjinfo_t* 		info;
	AActor* 			temp;
		
	if (actor->movedir != DI_NODIR)
	{
		// check for corpses to raise
		viletryx =
			actor->x + actor->info->speed*xspeed[actor->movedir];
		viletryy =
			actor->y + actor->info->speed*yspeed[actor->movedir];

		xl = (viletryx - bmaporgx - MAXRADIUS*2)>>MAPBLOCKSHIFT;
		xh = (viletryx - bmaporgx + MAXRADIUS*2)>>MAPBLOCKSHIFT;
		yl = (viletryy - bmaporgy - MAXRADIUS*2)>>MAPBLOCKSHIFT;
		yh = (viletryy - bmaporgy + MAXRADIUS*2)>>MAPBLOCKSHIFT;
		
		vileobj = actor;
		for (bx=xl ; bx<=xh ; bx++)
		{
			for (by=yl ; by<=yh ; by++)
			{
				// Call PIT_VileCheck to check
				// whether object is a corpse
				// that canbe raised.
				if (!P_BlockThingsIterator(bx,by,PIT_VileCheck))
				{
					// got one!
					temp = actor->target;
					actor->target = corpsehit;
					A_FaceTarget (actor);
					actor->target = temp;
										
					P_SetMobjState (actor, S_VILE_HEAL1);
					S_Sound (corpsehit, CHAN_BODY, "vile/raise", 1, ATTN_IDLE);
					info = corpsehit->info;
					
					P_SetMobjState (corpsehit,info->raisestate);
					corpsehit->height = info->height;	// [RH] Use real mobj height
					corpsehit->radius = info->radius;	// [RH] Use real radius
					if (corpsehit->translucency > TRANSLUC50)
						corpsehit->translucency /= 2;
					corpsehit->flags = info->flags;
					corpsehit->flags2 = info->flags2;
					corpsehit->health = info->spawnhealth;
					corpsehit->target = NULL;

					return;
				}
			}
		}
	}

	// Return to normal attack.
	A_Chase (actor);
}


//
// A_VileStart
//
void A_VileStart (AActor *actor)
{
	S_Sound (actor, CHAN_VOICE, "vile/start", 1, ATTN_NORM);
}


//
// A_Fire
// Keep fire in front of player unless out of sight
//
void A_Fire (AActor *actor);

void A_StartFire (AActor *actor)
{
	S_Sound(actor, CHAN_BODY, "vile/firestrt", 1, ATTN_NORM);
	A_Fire(actor);
}

void A_FireCrackle (AActor *actor)
{
	S_Sound(actor, CHAN_BODY, "vile/firecrkl", 1, ATTN_NORM);
	A_Fire(actor);
}

void A_Fire (AActor *actor)
{
	AActor* 	dest;
	unsigned	an;
				
	dest = actor->tracer;
	if (!dest)
		return;
				
	// don't move it if the vile lost sight
	if (!P_CheckSight (actor->target, dest, false) )
		return;

	an = dest->angle >> ANGLETOFINESHIFT;

	actor->SetOrigin (dest->x + FixedMul (24*FRACUNIT, finecosine[an]),
					  dest->y + FixedMul (24*FRACUNIT, finesine[an]),
					  dest->z);
}



//
// A_VileTarget
// Spawn the hellfire
//
void A_VileTarget (AActor *actor)
{
	AActor *fog;
		
	if (!actor->target)
		return;

	A_FaceTarget (actor);

	fog = new AActor (actor->target->x,
					  actor->target->x,
					  actor->target->z, MT_FIRE);
	
	actor->tracer = fog;
	fog->target = actor;
	fog->tracer = actor->target;
	A_Fire (fog);
}




//
// A_VileAttack
//
void A_VileAttack (AActor *actor)
{		
	AActor *fire;
	int an;
		
	if (!actor->target)
		return;
	
	A_FaceTarget (actor);

	if (!P_CheckSight (actor, actor->target, false) )
		return;

	S_Sound (actor, CHAN_WEAPON, "vile/stop", 1, ATTN_NORM);
	P_DamageMobj (actor->target, actor, actor, 20, MOD_UNKNOWN);
	actor->target->momz = 1000*FRACUNIT/actor->target->info->mass;
		
	an = actor->angle >> ANGLETOFINESHIFT;

	fire = actor->tracer;

	if (!fire)
		return;
				
	// move the fire between the vile and the player
	fire->x = actor->target->x - FixedMul (24*FRACUNIT, finecosine[an]);
	fire->y = actor->target->y - FixedMul (24*FRACUNIT, finesine[an]);	
	P_RadiusAttack (fire, actor, 70, MOD_UNKNOWN);
}




//
// Mancubus attack,
// firing three missiles (bruisers) in three different directions?
// Doesn't look like it. 
//
#define FATSPREAD		(ANG90/8)

void A_FatRaise (AActor *actor)
{
	A_FaceTarget (actor);
	S_Sound (actor, CHAN_WEAPON, "fatso/raiseguns", 1, ATTN_NORM);
}


void A_FatAttack1 (AActor *actor)
{
	AActor* 	mo;
	int 		an;
		
	A_FaceTarget (actor);
	// Change direction  to ...
	actor->angle += FATSPREAD;
	P_SpawnMissile (actor, actor->target, MT_FATSHOT);

	mo = P_SpawnMissile (actor, actor->target, MT_FATSHOT);
	mo->angle += FATSPREAD;
	an = mo->angle >> ANGLETOFINESHIFT;
	mo->momx = FixedMul (mo->info->speed, finecosine[an]);
	mo->momy = FixedMul (mo->info->speed, finesine[an]);
}

void A_FatAttack2 (AActor *actor)
{
	AActor* 	mo;
	int 		an;

	A_FaceTarget (actor);
	// Now here choose opposite deviation.
	actor->angle -= FATSPREAD;
	P_SpawnMissile (actor, actor->target, MT_FATSHOT);

	mo = P_SpawnMissile (actor, actor->target, MT_FATSHOT);
	mo->angle -= FATSPREAD*2;
	an = mo->angle >> ANGLETOFINESHIFT;
	mo->momx = FixedMul (mo->info->speed, finecosine[an]);
	mo->momy = FixedMul (mo->info->speed, finesine[an]);
}

void A_FatAttack3 (AActor *actor)
{
	AActor* 	mo;
	int 		an;

	A_FaceTarget (actor);
	
	mo = P_SpawnMissile (actor, actor->target, MT_FATSHOT);
	mo->angle -= FATSPREAD/2;
	an = mo->angle >> ANGLETOFINESHIFT;
	mo->momx = FixedMul (mo->info->speed, finecosine[an]);
	mo->momy = FixedMul (mo->info->speed, finesine[an]);

	mo = P_SpawnMissile (actor, actor->target, MT_FATSHOT);
	mo->angle += FATSPREAD/2;
	an = mo->angle >> ANGLETOFINESHIFT;
	mo->momx = FixedMul (mo->info->speed, finecosine[an]);
	mo->momy = FixedMul (mo->info->speed, finesine[an]);
}


//
// SkullAttack
// Fly at the player like a missile.
//
#define SKULLSPEED (20*FRACUNIT)

void A_SkullAttack (AActor *actor)
{
	AActor* 			dest;
	angle_t 			an;
	int 				dist;

	if (!actor->target)
		return;
				
	dest = actor->target;		
	actor->flags |= MF_SKULLFLY;

	S_Sound (actor, CHAN_VOICE, actor->info->attacksound, 1, ATTN_NORM);
	A_FaceTarget (actor);
	an = actor->angle >> ANGLETOFINESHIFT;
	actor->momx = FixedMul (SKULLSPEED, finecosine[an]);
	actor->momy = FixedMul (SKULLSPEED, finesine[an]);
	dist = P_AproxDistance (dest->x - actor->x, dest->y - actor->y);
	dist = dist / SKULLSPEED;
	
	if (dist < 1)
		dist = 1;
	actor->momz = (dest->z+(dest->height>>1) - actor->z) / dist;
}


//
// A_PainShootSkull
// Spawn a lost soul and launch it at the target
//
void A_PainShootSkull (AActor *actor, angle_t angle)
{
	fixed_t 	x;
	fixed_t 	y;
	fixed_t 	z;
	
	AActor* 	other;
	angle_t 	an;
	int 		prestep;
	int 		count;

	// count total number of skull currently on the level
	count = 0;

	TThinkerIterator<AActor> iterator;

	while ( (other = iterator.Next ()) )
	{
		if (other->type == MT_SKULL)
			count++;
	}

	// if there are already 20 skulls on the level,
	// don't spit another one
	if (count > 20)
		return;

	// okay, there's room for another one
	an = angle >> ANGLETOFINESHIFT;
	
	prestep = 4*FRACUNIT + 3*(actor->info->radius + mobjinfo[MT_SKULL].radius)/2;
	
	x = actor->x + FixedMul (prestep, finecosine[an]);
	y = actor->y + FixedMul (prestep, finesine[an]);
	z = actor->z + 8*FRACUNIT;
				
	// Check whether the Lost Soul is being fired through a 1-sided	// phares
	// wall or an impassible line, or a "monsters can't cross" line.//   |
	// If it is, then we don't allow the spawn.						//   V

	if (Check_Sides(actor,x,y))
		return;

	other = new AActor (x, y, z, MT_SKULL);

	// Check to see if the new Lost Soul's z value is above the
	// ceiling of its new sector, or below the floor. If so, kill it.

	if ((other->z >
         (other->subsector->sector->ceilingheight - other->height)) ||
        (other->z < other->subsector->sector->floorheight))
	{
		// kill it immediately
		P_DamageMobj (other, actor, actor, 10000, MOD_UNKNOWN);		//   ^
		return;														//   |
	}																// phares

	// Check for movements.

	if (!P_CheckPosition (other, other->x, other->y))
	{
		// kill it immediately
		P_DamageMobj (other, actor, actor, 10000, MOD_UNKNOWN);		
		return;
	}
				
	other->target = actor->target;
	A_SkullAttack (other);
}


//
// A_PainAttack
// Spawn a lost soul and launch it at the target
// 
void A_PainAttack (AActor *actor)
{
	if (!actor->target)
		return;

	A_FaceTarget (actor);
	A_PainShootSkull (actor, actor->angle);
}

void A_PainDie (AActor *actor)
{
	A_Fall (actor);
	A_PainShootSkull (actor, actor->angle+ANG90);
	A_PainShootSkull (actor, actor->angle+ANG180);
	A_PainShootSkull (actor, actor->angle+ANG270);
}


void A_Scream (AActor *actor)
{
	if (actor->info->deathsound)
	{
		char sound[MAX_SNDNAME];

		strcpy (sound, actor->info->deathsound);

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

void A_Pain (AActor *actor)
{
	// [RH] Have some variety in player pain sounds
	if (actor->player) {
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
	} else if (actor->info->painsound)
		S_Sound (actor, CHAN_VOICE, actor->info->painsound, 1, ATTN_NORM);	
}



void A_Fall (AActor *actor)
{
	// [RH] Andy Baker's stealth monsters
	if (actor->flags & MF_STEALTH)
	{
		actor->translucency = FRACUNIT;
		actor->visdir = 0;
	}

	// actor is on ground, it can be walked over
	actor->flags &= ~MF_SOLID;

	// So change this if corpse objects
	// are meant to be obstacles.

#if 0
	// [RH] Toss some gibs
	//if (actor->health < -80)
	{
		int n;

		if (testgibs.value)
			for (n = 0; n < 6; n++)
				ThrowGib (actor, MT_GIB0 + (int)(P_Random(pr_gengib) >> 5), -actor->health);
	}
#endif
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
	P_RadiusAttack (mo, mo->target, mo->info->damage, MOD_UNKNOWN);
}

//
// A_Explode
//
void A_Explode (AActor *thing)
{
	// [RH] figure out means of death;
	int mod;

	switch (thing->type) {
		case MT_BARREL:
			mod = MOD_BARREL;
			break;
		case MT_ROCKET:
			mod = MOD_ROCKET;
			break;
		default:
			mod = MOD_UNKNOWN;
			break;
	}

	P_RadiusAttack (thing, thing->target, 128, mod);
}

//
// killough 9/98: a mushroom explosion effect, sorta :)
// Original idea: Linguica
//

void A_Mushroom (AActor *actor)
{
	int i, j, n = actor->info->damage;

	A_Explode (actor);	// First make normal explosion

	// Now launch mushroom cloud
	for (i = -n; i <= n; i += 8)
	{
		for (j = -n; j <= n; j += 8)
		{
			AActor target = *actor, *mo;
			target.x += i << FRACBITS; // Aim in many directions from source
			target.y += j << FRACBITS;
			target.z += P_AproxDistance(i,j) << (FRACBITS+2); // Aim up fairly high
			mo = P_SpawnMissile (actor, &target, MT_FATSHOT); // Launch fireball
			mo->momx >>= 1;
			mo->momy >>= 1;									  // Slow it down a bit
			mo->momz >>= 1;
			mo->flags &= ~MF_NOGRAVITY;   // Make debris fall under gravity
		}
	}
}

//
// A_BossDeath
// Possibly trigger special effects if on a boss level
//
void A_BossDeath (AActor *actor)
{
	int i;

	// [RH] These all depend on the presence of level flags now
	//		rather than being hard-coded to specific levels.

	if ((level.flags & (LEVEL_MAP07SPECIAL|
						LEVEL_BRUISERSPECIAL|
						LEVEL_CYBORGSPECIAL|
						LEVEL_SPIDERSPECIAL)) == 0)
		return;

	if (
		((level.flags & LEVEL_MAP07SPECIAL) && (actor->type == MT_FATSO || actor->type == MT_BABY)) ||
		((level.flags & LEVEL_BRUISERSPECIAL) && (actor->type == MT_BRUISER)) ||
		((level.flags & LEVEL_CYBORGSPECIAL) && (actor->type == MT_CYBORG)) ||
		((level.flags & LEVEL_SPIDERSPECIAL) && (actor->type == MT_SPIDER))
	   )
		;
	else return;

	// make sure there is a player alive for victory
	for (i = 0; i < MAXPLAYERS; i++)
		if (playeringame[i] && players[i].health > 0)
			break;
	
	if (i == MAXPLAYERS)
		return; // no one left alive, so do not end game
	
	// scan the remaining thinkers to see if all bosses are dead
	TThinkerIterator<AActor> iterator;
	AActor *other;

	while ( (other = iterator.Next ()) )
	{
		if (other != actor && other->type == actor->type && other->health > 0)
		{
			// other boss not dead
			return;
		}
	}
		
	// victory!
	if (level.flags & LEVEL_MAP07SPECIAL)
	{
		if (actor->type == MT_FATSO)
		{
			EV_DoFloor (DFloor::floorLowerToLowest, NULL, 666, FRACUNIT, 0, 0, 0);
			return;
		}
		
		if (actor->type == MT_BABY)
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
	if ((deathmatch.value || alwaysapplydmflags.value) && (dmflags & DF_NO_EXIT))
		return;

	G_ExitLevel (0);
}


void A_Hoof (AActor *mo)
{
	S_Sound (mo, CHAN_BODY, "cyber/hoof", 1, ATTN_IDLE);
	A_Chase (mo);
}

void A_Metal (AActor *mo)
{
	S_Sound (mo, CHAN_BODY, "spider/walk", 1, ATTN_IDLE);
	A_Chase (mo);
}

void A_BabyMetal (AActor *mo)
{
	S_Sound (mo, CHAN_BODY, "baby/walk", 1, ATTN_IDLE);
	A_Chase (mo);
}

void A_OpenShotgun2 (player_t *player, pspdef_t *psp)
{
	S_Sound (player->mo, CHAN_WEAPON, "weapons/sshoto", 1, ATTN_NORM);
}

void A_LoadShotgun2 (player_t *player, pspdef_t *psp)
{
	S_Sound (player->mo, CHAN_WEAPON, "weapons/sshotl", 1, ATTN_NORM);
}

void A_ReFire (player_t *player, pspdef_t *psp);

void A_CloseShotgun2 (player_t *player, pspdef_t *psp)
{
	S_Sound (player->mo, CHAN_WEAPON, "weapons/sshotc", 1, ATTN_NORM);
	A_ReFire(player,psp);
}



// killough 2/7/98: Remove limit on icon landings:
AActor **braintargets;
int    numbraintargets_alloc;
int    numbraintargets;

struct brain_s brain;   // killough 3/26/98: global state of boss brain

// killough 3/26/98: initialize icon landings at level startup,
// rather than at boss wakeup, to prevent savegame-related crashes

void P_SpawnBrainTargets (void)	// killough 3/26/98: renamed old function
{
	AActor *other;
	TThinkerIterator<AActor> iterator;

	// find all the target spots
	numbraintargets = 0;
	brain.targeton = 0;
	brain.easy = 0;				// killough 3/26/98: always init easy to 0

	while ( (other = iterator.Next ()) )
	{
		if (other->type == MT_BOSSTARGET)
		{	// killough 2/7/98: remove limit on icon landings:
			if (numbraintargets >= numbraintargets_alloc)
			{
				braintargets = (AActor **)Realloc (braintargets,
					(numbraintargets_alloc = numbraintargets_alloc ?
					 numbraintargets_alloc*2 : 32) *sizeof *braintargets);
			}
			braintargets[numbraintargets++] = other;
		}
	}
}

void A_BrainAwake (AActor *mo)
{
	// killough 3/26/98: only generates sound now
	S_Sound (mo, CHAN_VOICE, "brain/sight", 1, ATTN_SURROUND);
}


void A_BrainPain (AActor *mo)
{
	S_Sound (mo, CHAN_VOICE, "brain/pain", 1, ATTN_SURROUND);
}


void A_BrainScream (AActor *mo)
{
	int 		x;
	int 		y;
	int 		z;
	AActor* 	th;
		
	for (x=mo->x - 196*FRACUNIT ; x< mo->x + 320*FRACUNIT ; x+= FRACUNIT*8)
	{
		y = mo->y - 320*FRACUNIT;
		z = 128 + (P_Random (pr_brainscream) << (FRACBITS + 1));
		th = new AActor (x,y,z, MT_ROCKET);
		th->momz = P_Random (pr_brainscream) << 9;

		P_SetMobjState (th, S_BRAINEXPLODE1);

		th->tics -= P_Random (pr_brainscream) & 7;
		if (th->tics < 1)
			th->tics = 1;
	}
		
	S_Sound (mo, CHAN_VOICE, "brain/death", 1, ATTN_SURROUND);
}



void A_BrainExplode (AActor *mo)
{
	int t = P_Random (pr_brainexplode);
	int x = mo->x + (t - P_Random (pr_brainexplode))*2048;
	int y = mo->y;
	int z = 128 + P_Random (pr_brainexplode)*2*FRACUNIT;
	AActor *th = new AActor (x,y,z, MT_ROCKET);
	th->momz = P_Random (pr_brainexplode) << 9;

	P_SetMobjState (th, S_BRAINEXPLODE1);

	th->tics -= P_Random (pr_brainexplode) & 7;
	if (th->tics < 1)
		th->tics = 1;
}


void A_BrainDie (AActor *mo)
{
	// [RH] If noexit, then don't end the level.
	if ((deathmatch.value || alwaysapplydmflags.value) && (dmflags & DF_NO_EXIT))
		return;

	G_ExitLevel (0);
}

void A_BrainSpit (AActor *mo)
{
	AActor* 	targ;
	AActor* 	newmobj;
	
	// [RH] Do nothing if there are no brain targets.
	if (numbraintargets == 0)
		return;

	brain.easy ^= 1;		// killough 3/26/98: use brain struct
	if (gameskill.value <= sk_easy && (!brain.easy))
		return;
				
	// shoot a cube at current target
	targ = braintargets[brain.targeton++];	// killough 3/26/98:
	brain.targeton %= numbraintargets;		// Use brain struct for targets

	// spawn brain missile
	newmobj = P_SpawnMissile (mo, targ, MT_SPAWNSHOT);

	newmobj->target = targ;
	newmobj->reactiontime =
		((targ->y - mo->y)/newmobj->momy) / newmobj->state->tics;

	S_Sound (mo, CHAN_WEAPON, "brain/spit", 1, ATTN_SURROUND);
}



void A_SpawnFly (AActor *mo);

// travelling cube sound
void A_SpawnSound (AActor *mo)	
{
	S_Sound (mo, CHAN_BODY, "brain/cube", 1, ATTN_IDLE);
	A_SpawnFly(mo);
}

void A_SpawnFly (AActor *mo)
{
	AActor* 	newmobj;
	AActor* 	fog;
	AActor* 	targ;
	int 		r;
	mobjtype_t	type;
		
	if (--mo->reactiontime)
		return; // still flying
		
	targ = mo->target;

	// First spawn teleport fog.
	fog = new AActor (targ->x, targ->y, targ->z, MT_SPAWNFIRE);
	S_Sound (fog, CHAN_BODY, "misc/teleport", 1, ATTN_NORM);

	// Randomly select monster to spawn.
	r = P_Random (pr_spawnfly);

	// Probability distribution (kind of :),
	// decreasing likelihood.
	if ( r<50 )
		type = MT_TROOP;
	else if (r<90)
		type = MT_SERGEANT;
	else if (r<120)
		type = MT_SHADOWS;
	else if (r<130)
		type = MT_PAIN;
	else if (r<160)
		type = MT_HEAD;
	else if (r<162)
		type = MT_VILE;
	else if (r<172)
		type = MT_UNDEAD;
	else if (r<192)
		type = MT_BABY;
	else if (r<222)
		type = MT_FATSO;
	else if (r<246)
		type = MT_KNIGHT;
	else
		type = MT_BRUISER;

	newmobj = new AActor (targ->x, targ->y, targ->z, type);
	if (P_LookForPlayers (newmobj, true))
		P_SetMobjState (newmobj, newmobj->info->seestate);
		
	// telefrag anything in this spot
	P_TeleportMove (newmobj, newmobj->x, newmobj->y, newmobj->z, true);

	// remove self (i.e., cube).
	mo->Destroy ();
}



void A_PlayerScream (AActor *mo)
{
	char nametemp[128];
	char *sound;

	if (!(gameinfo.flags & GI_NOCRAZYDEATH) && (mo->health < -50))
	{
		// IF THE PLAYER DIES LESS THAN -50% WITHOUT GIBBING
		sound = "*xdeath1";
	} else {
		// [RH] More variety in death sounds
		sprintf (nametemp, "*death%d", (P_Random (pr_playerscream)&3) + 1);
		sound = nametemp;
	}
	
	S_Sound (mo, CHAN_VOICE, sound, 1, ATTN_NORM);
}
