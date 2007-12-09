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

#include "templates.h"
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
#include "thingdef/thingdef.h"

#include "gi.h"

static FRandom pr_checkmissilerange ("CheckMissileRange");
static FRandom pr_opendoor ("OpenDoor");
static FRandom pr_trywalk ("TryWalk");
static FRandom pr_newchasedir ("NewChaseDir");
static FRandom pr_lookformonsters ("LookForMonsters");
static FRandom pr_lookforplayers ("LookForPlayers");
static FRandom pr_scaredycat ("Anubis");
	   FRandom pr_chase ("Chase");
static FRandom pr_facetarget ("FaceTarget");
static FRandom pr_railface ("RailFace");
static FRandom pr_dropitem ("DropItem");
static FRandom pr_look2 ("LookyLooky");
static FRandom pr_look3 ("IGotHooky");
static FRandom pr_slook ("SlooK");

static FRandom pr_skiptarget("SkipTarget");

// movement interpolation is fine for objects that are moved by their own
// momentum. But for monsters it is problematic.
// 1. They don't move every tic
// 2. Their animation is not designed for movement interpolation
// The result is that they tend to 'glide' across the floor
// so this CVAR allows to switch it off.
CVAR(Bool, nomonsterinterpolation, false, CVAR_GLOBALCONFIG|CVAR_ARCHIVE)

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

fixed_t xspeed[8] = {FRACUNIT,46341,0,-46341,-FRACUNIT,-46341,0,46341};
fixed_t yspeed[8] = {0,46341,FRACUNIT,46341,0,-46341,-FRACUNIT,-46341};

void P_RandomChaseDir (AActor *actor);


//
// ENEMY THINKING
// Enemies are always spawned
// with targetplayer = -1, threshold = 0
// Most monsters are spawned unaware of all players,
// but some can be made preaware
//


//----------------------------------------------------------------------------
//
// PROC P_RecursiveSound
//
// Called by P_NoiseAlert.
// Recursively traverse adjacent sectors,
// sound blocking lines cut off traversal.
//----------------------------------------------------------------------------

void P_RecursiveSound (sector_t *sec, AActor *soundtarget, bool splash, int soundblocks)
{
	int 		i;
	line_t* 	check;
	sector_t*	other;
	AActor*		actor;
		
	// wake up all monsters in this sector
	if (sec->validcount == validcount
		&& sec->soundtraversed <= soundblocks+1)
	{
		return; 		// already flooded
	}
	
	sec->validcount = validcount;
	sec->soundtraversed = soundblocks+1;
	sec->SoundTarget = soundtarget;

	// [RH] Set this in the actors in the sector instead of the sector itself.
	for (actor = sec->thinglist; actor != NULL; actor = actor->snext)
	{
		if (actor != soundtarget && (!splash || !(actor->flags4 & MF4_NOSPLASHALERT)))
		{
			actor->LastHeard = soundtarget;
		}
	}

	for (i = 0; i < sec->linecount; i++)
	{
		check = sec->lines[i];
		if (check->sidenum[1] == NO_SIDE ||
			!(check->flags & ML_TWOSIDED))
		{
			continue;
		}
		
		// Early out for intra-sector lines
		if (sides[check->sidenum[0]].sector==sides[check->sidenum[1]].sector) continue;

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
			 sec->ceilingplane.ZatPoint (check->v2->x, check->v2->y))
		 || (other->floorplane.ZatPoint (check->v1->x, check->v1->y) >=
			 other->ceilingplane.ZatPoint (check->v1->x, check->v1->y) &&
			 other->floorplane.ZatPoint (check->v2->x, check->v2->y) >=
			 other->ceilingplane.ZatPoint (check->v2->x, check->v2->y)))
		{
			continue;
		}

		if (check->flags & ML_SOUNDBLOCK)
		{
			if (!soundblocks)
				P_RecursiveSound (other, soundtarget, splash, 1);
		}
		else
		{
			P_RecursiveSound (other, soundtarget, splash, soundblocks);
		}
	}
}



//----------------------------------------------------------------------------
//
// PROC P_NoiseAlert
//
// If a monster yells at a player, it will alert other monsters to the
// player.
//
//----------------------------------------------------------------------------

void P_NoiseAlert (AActor *target, AActor *emmiter, bool splash)
{
	if (emmiter == NULL)
		return;

	if (target != NULL && target->player && (target->player->cheats & CF_NOTARGET))
		return;

	validcount++;
	P_RecursiveSound (emmiter->Sector, target, splash, 0);
}




//----------------------------------------------------------------------------
//
// AActor :: CheckMeleeRange
//
//----------------------------------------------------------------------------

bool AActor::CheckMeleeRange ()
{
	AActor *pl;
	fixed_t dist;
		
	if (!target)
		return false;
				
	pl = target;
	dist = P_AproxDistance (pl->x - x, pl->y - y);

	if (dist >= meleerange + pl->radius)
		return false;

	// [RH] If moving toward goal, then we've reached it.
	if (target == goal)
		return true;

	// [RH] Don't melee things too far above or below actor.
	if (pl->z > z + height)
		return false;
	if (pl->z + pl->height < z)
		return false;
		
	if (!P_CheckSight (this, pl, 0))
		return false;
														
	return true;				
}

//----------------------------------------------------------------------------
//
// FUNC P_CheckMeleeRange2
//
//----------------------------------------------------------------------------

bool P_CheckMeleeRange2 (AActor *actor)
{
	AActor *mo;
	fixed_t dist;

	if (!actor->target)
	{
		return false;
	}
	mo = actor->target;
	dist = P_AproxDistance (mo->x-actor->x, mo->y-actor->y);
	if (dist >= MELEERANGE*2 || dist < MELEERANGE-20*FRACUNIT + mo->radius)
	{
		return false;
	}
	if (mo->z > actor->z+actor->height)
	{ // Target is higher than the attacker
		return false;
	}
	else if (actor->z > mo->z+mo->height)
	{ // Attacker is higher
		return false;
	}
	if (!P_CheckSight(actor, mo))
	{
		return false;
	}
	return true;
}


//=============================================================================
//
// P_CheckMissileRange
//
//=============================================================================
bool P_CheckMissileRange (AActor *actor)
{
	fixed_t dist;
		
	if (!P_CheckSight (actor, actor->target, 4))
		return false;
		
	if (actor->flags & MF_JUSTHIT)
	{
		// the target just hit the enemy, so fight back!
		actor->flags &= ~MF_JUSTHIT;
		return true;
	}
		
	if (actor->reactiontime)
		return false;	// do not attack yet
				
	// OPTIMIZE: get this from a global checksight
	// [RH] What?
	dist = P_AproxDistance (actor->x-actor->target->x,
							actor->y-actor->target->y) - 64*FRACUNIT;
	
	if (actor->MeleeState == NULL)
		dist -= 128*FRACUNIT;	// no melee attack, so fire more

	return actor->SuggestMissileAttack (dist);
}

bool AActor::SuggestMissileAttack (fixed_t dist)
{
	// new version encapsulates the different behavior in flags instead of virtual functions
	// The advantage is that this allows inheriting the missile attack attributes from the
	// various Doom monsters by custom monsters
	
	if (maxtargetrange > 0 && dist > maxtargetrange)
		return false;	// The Arch Vile's special behavior turned into a property
		
	if (MeleeState != NULL && dist < meleethreshold)
		return false;	// From the Revenant: close enough for fist attack

	if (flags4 & MF4_MISSILEMORE) dist >>= 1;
	if (flags4 & MF4_MISSILEEVENMORE) dist >>= 3;
	
	int mmc = FixedMul(MinMissileChance, G_SkillProperty(SKILLP_Aggressiveness));
	return pr_checkmissilerange() >= MIN<int> (dist >> FRACBITS, mmc);
}

//=============================================================================
//
// P_HitFriend()
//
// killough 12/98
// This function tries to prevent shooting at friends that get in the line of fire
//
// [GrafZahl] Taken from MBF but this has been cleaned up to make it readable.
//
//=============================================================================

bool P_HitFriend(AActor * self)
{
	if (self->flags&MF_FRIENDLY && self->target != NULL)
	{
		angle_t angle = R_PointToAngle2 (self->x, self->y, self->target->x, self->target->y);
		fixed_t dist = P_AproxDistance (self->x-self->target->x, self->y-self->target->y);
		P_AimLineAttack (self, angle, dist, 0, true);
		if (linetarget != NULL && linetarget != self->target)
		{
			return self->IsFriend (linetarget);
		}
	}
	return false;
}

//
// P_Move
// Move in the current direction,
// returns false if the move is blocked.
//
bool P_Move (AActor *actor)
{

	fixed_t tryx, tryy, deltax, deltay, origx, origy;
	bool try_ok;
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
		if (actor->z > actor->floorz + actor->MaxStepHeight)
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

	speed = actor->Speed;

#if 0	// [RH] I'm not so sure this is such a good idea
	// killough 10/98: make monsters get affected by ice and sludge too:
	movefactor = P_GetMoveFactor (actor, &friction);

	if (friction < ORIG_FRICTION)
	{ // sludge
		speed = ((ORIG_FRICTION_FACTOR - (ORIG_FRICTION_FACTOR-movefactor)/2)
		   * speed) / ORIG_FRICTION_FACTOR;
		if (speed == 0)
		{ // always give the monster a little bit of speed
			speed = ksgn(actor->Speed);
		}
	}
#endif

	tryx = (origx = actor->x) + (deltax = FixedMul (speed, xspeed[actor->movedir]));
	tryy = (origy = actor->y) + (deltay = FixedMul (speed, yspeed[actor->movedir]));

	// Like P_XYMovement this should do multiple moves if the step size is too large

	fixed_t maxmove = actor->radius - FRACUNIT;
	int steps = 1;

	if (maxmove > 0)
	{ 
		const fixed_t xspeed = abs (deltax);
		const fixed_t yspeed = abs (deltay);

		if (xspeed > yspeed)
		{
			if (xspeed > maxmove)
			{
				steps = 1 + xspeed / maxmove;
			}
		}
		else
		{
			if (yspeed > maxmove)
			{
				steps = 1 + yspeed / maxmove;
			}
		}
	}
	try_ok = true;
	for(int i=1; i < steps; i++)
	{
		try_ok = P_TryMove(actor, origx + Scale(deltax, i, steps), origy + Scale(deltay, i, steps), false);
		if (!try_ok) break;
	}

	// killough 3/15/98: don't jump over dropoffs:
	if (try_ok) try_ok = P_TryMove (actor, tryx, tryy, false);

	// [GrafZahl] Interpolating monster movement as it is done here just looks bad
	// so make it switchable!
	if (nomonsterinterpolation)
	{
		actor->PrevX = actor->x;
		actor->PrevY = actor->y;
		actor->PrevZ = actor->z;
	}

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
				actor->z += actor->FloatSpeed;
			else
				actor->z -= actor->FloatSpeed;

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
		int good = 0;
		
		while (spechit.Pop (ld))
		{
			// [RH] let monsters push lines, as well as use them
			if (((actor->flags4 & MF4_CANUSEWALLS) && P_ActivateLine (ld, actor, 0, SPAC_USE)) ||
				((actor->flags2 & MF2_PUSHWALL) && P_ActivateLine (ld, actor, 0, SPAC_PUSH)))
			{
				good |= ld == BlockingLine ? 1 : 2;
			}
		}
		return good && ((pr_opendoor() >= 203) ^ (good & 1));
	}
	else
	{
		actor->flags &= ~MF_INFLOAT;
	}
	return true; 
}


//=============================================================================
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
//=============================================================================

bool P_TryWalk (AActor *actor)
{
	if (!P_Move (actor))
	{
		return false;
	}
	actor->movecount = pr_trywalk() & 15;
	return true;
}

//=============================================================================
//
// P_DoNewChaseDir
//
// killough 9/8/98:
//
// Most of P_NewChaseDir(), except for what
// determines the new direction to take
//
//=============================================================================

void P_DoNewChaseDir (AActor *actor, fixed_t deltax, fixed_t deltay)
{
	dirtype_t	d[3];
	int			tdir;
	dirtype_t	olddir, turnaround;

	olddir = (dirtype_t)actor->movedir;
	turnaround = opposite[olddir];

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
	if (pr_newchasedir() > 200 || abs(deltay) > abs(deltax))
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
	if (pr_newchasedir() & 1)	
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


//=============================================================================
//
// killough 11/98:
//
// Monsters try to move away from tall dropoffs.
//
// In Doom, they were never allowed to hang over dropoffs,
// and would remain stuck if involuntarily forced over one.
// This logic, combined with p_map.c (P_TryMove), allows
// monsters to free themselves without making them tend to
// hang over dropoffs.
//=============================================================================

struct avoiddropoff_t
{
	AActor * thing;
	fixed_t deltax;
	fixed_t deltay;
	fixed_t floorx;
	fixed_t floory;
	fixed_t floorz;
	fixed_t t_bbox[4];
} a;

static bool PIT_AvoidDropoff(line_t *line)
{
	if (line->backsector                          && // Ignore one-sided linedefs
		a.t_bbox[BOXRIGHT]  > line->bbox[BOXLEFT]   &&
		a.t_bbox[BOXLEFT]   < line->bbox[BOXRIGHT]  &&
		a.t_bbox[BOXTOP]    > line->bbox[BOXBOTTOM] && // Linedef must be contacted
		a.t_bbox[BOXBOTTOM] < line->bbox[BOXTOP]    &&
		P_BoxOnLineSide(a.t_bbox, line) == -1)
    {
		fixed_t front = line->frontsector->floorplane.ZatPoint(a.floorx,a.floory);
		fixed_t back  = line->backsector->floorplane.ZatPoint(a.floorx,a.floory);
		angle_t angle;
		
		// The monster must contact one of the two floors,
		// and the other must be a tall dropoff.
		
		if (back == a.floorz && front < a.floorz - a.thing->MaxDropOffHeight)
		{
			angle = R_PointToAngle2(0,0,line->dx,line->dy);   // front side dropoff
		}
		else if (front == a.floorz && back < a.floorz - a.thing->MaxDropOffHeight)
		{
			angle = R_PointToAngle2(line->dx,line->dy,0,0); // back side dropoff
		}
		else return true;
		
		// Move away from dropoff at a standard speed.
		// Multiple contacted linedefs are cumulative (e.g. hanging over corner)
		a.deltax -= finesine[angle >> ANGLETOFINESHIFT]*32;
		a.deltay += finecosine[angle >> ANGLETOFINESHIFT]*32;
    }
	return true;
}

//=============================================================================
//
// P_NewChaseDir
//
// killough 9/8/98: Split into two functions
//
//=============================================================================

void P_NewChaseDir(AActor * actor)
{
	fixed_t deltax;
	fixed_t deltay;

	if ((actor->flags5&MF5_CHASEGOAL || actor->goal == actor->target) && actor->goal!=NULL)
	{
		deltax = actor->goal->x - actor->x;
		deltay = actor->goal->y - actor->y;
	}
	else if (actor->target != NULL)
	{
		deltax = actor->target->x - actor->x;
		deltay = actor->target->y - actor->y;

		if ((actor->target->player != NULL && (actor->target->player->cheats & CF_FRIGHTENING)) || 
			(actor->flags4 & MF4_FRIGHTENED))
		{
			deltax = -deltax;
			deltay = -deltay;
		}
	}
	else
	{
		// Don't abort if this happens.
		Printf ("P_NewChaseDir: called with no target\n");
		P_RandomChaseDir(actor);
		return;
	}
	
	// Try to move away from a dropoff
	if (actor->floorz - actor->dropoffz > actor->MaxDropOffHeight && 
		actor->z <= actor->floorz && !(actor->flags & MF_DROPOFF) && 
		!(actor->flags2 & MF2_ONMOBJ) &&
		!(actor->flags & MF_FLOAT) && !(i_compatflags & COMPATF_DROPOFF))
	{
		a.thing = actor;
		a.deltax = a.deltay = 0;
		a.floorx = actor->x;
		a.floory = actor->y;
		a.floorz = actor->z;

		int yh=((a.t_bbox[BOXTOP]   = actor->y+actor->radius)-bmaporgy)>>MAPBLOCKSHIFT;
		int yl=((a.t_bbox[BOXBOTTOM]= actor->y-actor->radius)-bmaporgy)>>MAPBLOCKSHIFT;
		int xh=((a.t_bbox[BOXRIGHT] = actor->x+actor->radius)-bmaporgx)>>MAPBLOCKSHIFT;
		int xl=((a.t_bbox[BOXLEFT]  = actor->x-actor->radius)-bmaporgx)>>MAPBLOCKSHIFT;
		int bx, by;
		
		// check lines
		
		validcount++;
		for (bx=xl ; bx<=xh ; bx++)
			for (by=yl ; by<=yh ; by++)
				P_BlockLinesIterator(bx, by, PIT_AvoidDropoff);  // all contacted lines

		if (a.deltax || a.deltay) 
		{
			// [Graf Zahl] I have changed P_TryMove to only apply this logic when
			// being called from here. AVOIDINGDROPOFF activates the code that
			// allows monsters to move away from a dropoff. This is different from
			// MBF which requires unconditional use of the altered logic and therefore
			// forcing a massive change in the monster behavior to use this.

			// use different dropoff movement logic in P_TryMove
			actor->flags5|=MF5_AVOIDINGDROPOFF;
			P_DoNewChaseDir(actor, a.deltax, a.deltay);
			actor->flags5&=~MF5_AVOIDINGDROPOFF;
		
			// If moving away from dropoff, set movecount to 1 so that 
			// small steps are taken to get monster away from dropoff.
			actor->movecount = 1;
			return;
		}
	}
	P_DoNewChaseDir(actor, deltax, deltay);
}




//=============================================================================
//
// P_RandomChaseDir
//
//=============================================================================

void P_RandomChaseDir (AActor *actor)
{
	dirtype_t olddir, turnaround;
	int tdir, i;

	olddir = (dirtype_t)actor->movedir;
	turnaround = opposite[olddir];
	int turndir;

	// Friendly monsters like to head toward a player
	if (actor->flags & MF_FRIENDLY)
	{
		AActor *player;
		fixed_t deltax, deltay;
		dirtype_t d[3];

		if (actor->FriendPlayer != 0)
		{
			player = players[actor->FriendPlayer - 1].mo;
		}
		else
		{
			if (!multiplayer)
			{
				i = 0;
			}
			else for (i = pr_newchasedir() & (MAXPLAYERS-1); !playeringame[i]; i = (i+1) & (MAXPLAYERS-1))
			{
			}
			player = players[i].mo;
		}

		if (pr_newchasedir() & 1 || !P_CheckSight (actor, player))
		{
			deltax = player->x - actor->x;
			deltay = player->y - actor->y;

			if (deltax>128*FRACUNIT)
				d[1]= DI_EAST;
			else if (deltax<-128*FRACUNIT)
				d[1]= DI_WEST;
			else
				d[1]=DI_NODIR;

			if (deltay<-128*FRACUNIT)
				d[2]= DI_SOUTH;
			else if (deltay>128*FRACUNIT)
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
			if (pr_newchasedir() > 200 || abs(deltay) > abs(deltax))
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
		}
	}

	// If the actor elects to continue in its current direction, let it do
	// so unless the way is blocked. Then it must turn.
	if (pr_newchasedir() < 150)
	{
		if (P_TryWalk (actor))
			return;
	}

	turndir = (pr_newchasedir() & 1) ? -1 : 1;

	if (olddir == DI_NODIR)
	{
		olddir = (dirtype_t)(pr_newchasedir() & 7);
	}
	for (tdir = (olddir + turndir) & 7; tdir != olddir; tdir = (tdir + turndir) & 7)
	{
		if (tdir != turnaround)
		{
			actor->movedir = tdir;
			if (P_TryWalk (actor))
				return;
		}
	}
/*
	if (pr_newchasedir() & 1)
	{
		for (tdir = olddir; tdir <= DI_SOUTHEAST; ++tdir)
		{
			if (tdir != turnaround)
			{
				actor->movedir = tdir;
				if (P_TryWalk (actor))
					return;
			}
		}
	}
	else
	{
		for (tdir = DI_SOUTHEAST; tdir >= DI_EAST; --tdir)
		{
			if (tdir != turnaround)
			{
				actor->movedir = tdir;
				if (P_TryWalk (actor))
					return;
			}
		}
	}
*/
	if (turnaround != DI_NODIR)
	{
		actor->movedir = turnaround;
		if (P_TryWalk (actor))
		{
			actor->movecount = pr_newchasedir() & 15;
			return;
		}
	}

	actor->movedir = DI_NODIR;	// cannot move
}

//---------------------------------------------------------------------------
//
// FUNC P_LookForMonsters
//
//---------------------------------------------------------------------------

#define MONS_LOOK_RANGE (20*64*FRACUNIT)
#define MONS_LOOK_LIMIT 64

bool P_LookForMonsters (AActor *actor)
{
	int count;
	AActor *mo;
	TThinkerIterator<AActor> iterator;

	if (!P_CheckSight (players[0].mo, actor, 2))
	{ // Player can't see monster
		return false;
	}
	count = 0;
	while ( (mo = iterator.Next ()) )
	{
		if (!(mo->flags3 & MF3_ISMONSTER) || (mo == actor) || (mo->health <= 0))
		{ // Not a valid monster
			continue;
		}
		if (P_AproxDistance (actor->x-mo->x, actor->y-mo->y)
			> MONS_LOOK_RANGE)
		{ // Out of range
			continue;
		}
		if (pr_lookformonsters() < 16)
		{ // Skip
			continue;
		}
		if (++count >= MONS_LOOK_LIMIT)
		{ // Stop searching
			return false;
		}
		if (mo->IsKindOf (RUNTIME_TYPE(actor)) ||
			actor->IsKindOf (RUNTIME_TYPE(mo)))
		{ // [RH] Don't go after same species
			continue;
		}
		if (!P_CheckSight (actor, mo, 2))
		{ // Out of sight
			continue;
		}
		// Found a target monster
		actor->target = mo;
		return true;
	}
	return false;
}

//============================================================================
//
// LookForTIDinBlock
//
// Finds a target with the specified TID in a mapblock. Alternatively, it
// can find a target with a specified TID if something in this mapblock is
// already targetting it.
//
//============================================================================

AActor *LookForTIDinBlock (AActor *lookee, int index)
{
	FBlockNode *block;
	AActor *link;
	AActor *other;
	
	for (block = blocklinks[index]; block != NULL; block = block->NextActor)
	{
		link = block->Me;

        if (!(link->flags & MF_SHOOTABLE))
			continue;			// not shootable (observer or dead)

		if (link == lookee)
			continue;

		if (link->health <= 0)
			continue;			// dead

		if (link->flags2 & MF2_DORMANT)
			continue;			// don't target dormant things

		if (link->tid == lookee->TIDtoHate)
		{
			other = link;
		}
		else if (link->target != NULL && link->target->tid == lookee->TIDtoHate)
		{
			other = link->target;
			if (!(other->flags & MF_SHOOTABLE) ||
				other->health <= 0 ||
				(other->flags2 & MF2_DORMANT))
			{
				continue;
			}
		}
		else
		{
			continue;
		}

		if (!(lookee->flags3 & MF3_NOSIGHTCHECK))
		{
			if (!P_CheckSight (lookee, other, 2))
				continue;			// out of sight
	/*						
			if (!allaround)
			{
				angle_t an = R_PointToAngle2 (actor->x, actor->y, 
											other->x, other->y)
					- actor->angle;
				
				if (an > ANG90 && an < ANG270)
				{
					fixed_t dist = P_AproxDistance (other->x - actor->x,
											other->y - actor->y);
					// if real close, react anyway
					if (dist > MELEERANGE)
						continue;	// behind back
				}
			}
	*/
		}

		return other;
	}
	return NULL;
}

//============================================================================
//
// P_LookForTID
//
// Selects a live monster with the given TID
//
//============================================================================

bool P_LookForTID (AActor *actor, INTBOOL allaround)
{
	AActor *other;
	bool reachedend = false;

	other = P_BlockmapSearch (actor, 0, LookForTIDinBlock);

	if (other != NULL)
	{
		if (actor->goal && actor->target == actor->goal)
			actor->reactiontime = 0;

		actor->target = other;
		actor->LastLook.Actor = other;
		return true;
	}

	// The actor's TID could change because of death or because of
	// Thing_ChangeTID. If it's not what we expect, then don't use
	// it as a base for the iterator.
	if (actor->LastLook.Actor != NULL &&
		actor->LastLook.Actor->tid != actor->TIDtoHate)
	{
		actor->LastLook.Actor = NULL;
	}

	FActorIterator iterator (actor->TIDtoHate, actor->LastLook.Actor);
	int c = (pr_look3() & 31) + 7;	// Look for between 7 and 38 hatees at a time
	while ((other = iterator.Next()) != actor->LastLook.Actor)
	{
		if (other == NULL)
		{
			if (reachedend)
			{
				// we have cycled through the entire list at least once
				// so let's abort because even if we continue nothing can
				// be found.
				break;
			}
			reachedend = true;
			continue;
		}

		if (!(other->flags & MF_SHOOTABLE))
			continue;			// not shootable (observer or dead)

		if (other == actor)
			continue;			// don't hate self

		if (other->health <= 0)
			continue;			// dead

		if (other->flags2 & MF2_DORMANT)
			continue;			// don't target dormant things

		if (--c == 0)
			break;

		if (!(actor->flags3 & MF3_NOSIGHTCHECK))
		{
			if (!P_CheckSight (actor, other, 2))
				continue;			// out of sight
							
			if (!allaround)
			{
				angle_t an = R_PointToAngle2 (actor->x, actor->y, 
											other->x, other->y)
					- actor->angle;
				
				if (an > ANG90 && an < ANG270)
				{
					fixed_t dist = P_AproxDistance (other->x - actor->x,
											other->y - actor->y);
					// if real close, react anyway
					if (dist > MELEERANGE)
						continue;	// behind back
				}
			}
		}
		
		// [RH] Need to be sure the reactiontime is 0 if the monster is
		//		leaving its goal to go after something else.
		if (actor->goal && actor->target == actor->goal)
			actor->reactiontime = 0;

		actor->target = other;
		actor->LastLook.Actor = other;
		return true;
	}
	actor->LastLook.Actor = other;
	if (actor->target == NULL)
	{
		// [RH] use goal as target
		if (actor->goal != NULL)
		{
			actor->target = actor->goal;
			return true;
		}
		// Use last known enemy if no hatee sighted -- killough 2/15/98:
		if (actor->lastenemy != NULL && actor->lastenemy->health > 0)
		{
			if (!actor->IsFriend(actor->lastenemy))
			{
				actor->target = actor->lastenemy;
				actor->lastenemy = NULL;
				return true;
			}
			else
			{
				actor->lastenemy = NULL;
			}
		}
	}
	return false;
}

//============================================================================
//
// LookForTIDinBlock
//
// Finds a non-friendly monster in a mapblock. It can also use targets of
// friendlies in this mapblock to find non-friendlies in other mapblocks.
//
//============================================================================

AActor *LookForEnemiesInBlock (AActor *lookee, int index)
{
	FBlockNode *block;
	AActor *link;
	AActor *other;
	
	for (block = blocklinks[index]; block != NULL; block = block->NextActor)
	{
		link = block->Me;

        if (!(link->flags & MF_SHOOTABLE))
			continue;			// not shootable (observer or dead)

		if (link == lookee)
			continue;

		if (link->health <= 0)
			continue;			// dead

		if (link->flags2 & MF2_DORMANT)
			continue;			// don't target dormant things

		if (!(link->flags3 & MF3_ISMONSTER))
			continue;			// don't target it if it isn't a monster (could be a barrel)

		other = NULL;
		if (link->flags & MF_FRIENDLY)
		{
			if (deathmatch &&
				lookee->FriendPlayer != 0 && link->FriendPlayer != 0 &&
				lookee->FriendPlayer != link->FriendPlayer)
			{
				// This is somebody else's friend, so go after it
				other = link;
			}
			else if (link->target != NULL && !(link->target->flags & MF_FRIENDLY))
			{
				other = link->target;
				if (!(other->flags & MF_SHOOTABLE) ||
					other->health <= 0 ||
					(other->flags2 & MF2_DORMANT))
				{
					other = NULL;;
				}
			}
		}
		else
		{
			other = link;
		}

		// [MBF] If the monster is already engaged in a one-on-one attack
		// with a healthy friend, don't attack around 60% the time.
		
		// [GrafZahl] This prevents friendlies from attacking all the same 
		// target.
		
		if (other)
		{
			AActor *targ = other->target;
			if (targ && targ->target == other && pr_skiptarget() > 100 && lookee->IsFriend (targ) &&
				targ->health*2 >= targ->GetDefault()->health)
			{
				continue;
			}
		}

		if (other == NULL || !P_CheckSight (lookee, other, 2))
			continue;			// out of sight
	/*						
			if (!allaround)
			{
				angle_t an = R_PointToAngle2 (actor->x, actor->y, 
											other->x, other->y)
					- actor->angle;
				
				if (an > ANG90 && an < ANG270)
				{
					fixed_t dist = P_AproxDistance (other->x - actor->x,
											other->y - actor->y);
					// if real close, react anyway
					if (dist > MELEERANGE)
						continue;	// behind back
				}
			}
	*/

		return other;
	}
	return NULL;
}

//============================================================================
//
// P_LookForEnemies
//
// Selects a live enemy monster
//
//============================================================================

bool P_LookForEnemies (AActor *actor, INTBOOL allaround)
{
	AActor *other;

	other = P_BlockmapSearch (actor, 10, LookForEnemiesInBlock);

	if (other != NULL)
	{
		if (actor->goal && actor->target == actor->goal)
			actor->reactiontime = 0;

		actor->target = other;
//		actor->LastLook.Actor = other;
		return true;
	}

	if (actor->target == NULL)
	{
		// [RH] use goal as target
		if (actor->goal != NULL)
		{
			actor->target = actor->goal;
			return true;
		}
		// Use last known enemy if no hatee sighted -- killough 2/15/98:
		if (actor->lastenemy != NULL && actor->lastenemy->health > 0)
		{
			if (!actor->IsFriend(actor->lastenemy))
			{
				actor->target = actor->lastenemy;
				actor->lastenemy = NULL;
				return true;
			}
			else
			{
				actor->lastenemy = NULL;
			}
		}
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

bool P_LookForPlayers (AActor *actor, INTBOOL allaround)
{
	int 		c;
	int 		stop;
	int			pnum;
	player_t*	player;
	angle_t 	an;
	fixed_t 	dist;

	if (actor->TIDtoHate != 0)
	{
		if (P_LookForTID (actor, allaround))
		{
			return true;
		}
		if (!(actor->flags3 & MF3_HUNTPLAYERS))
		{
			return false;
		}
	}
	else if (actor->flags & MF_FRIENDLY)
	{
		return P_LookForEnemies (actor, allaround);
	}

	if (!(gameinfo.gametype & (GAME_Doom|GAME_Strife)) &&
		!multiplayer &&
		players[0].health <= 0)
	{ // Single player game and player is dead; look for monsters
		return P_LookForMonsters (actor);
	}

	c = 0;
	if (actor->TIDtoHate != 0)
	{
		pnum = pr_look2() & (MAXPLAYERS-1);
	}
	else
	{
		pnum = actor->LastLook.PlayerNumber;
	}
	stop = (pnum - 1) & (MAXPLAYERS-1);
		
	for (;;)
	{
		pnum = (pnum + 1) & (MAXPLAYERS-1);
		if (!playeringame[pnum])
			continue;

		if (actor->TIDtoHate == 0)
		{
			actor->LastLook.PlayerNumber = pnum;
		}

		if (++c == MAXPLAYERS-1 || pnum == stop)
		{
			// done looking
			if (actor->target == NULL)
			{
				// [RH] use goal as target
				if (actor->goal != NULL)
				{
					actor->target = actor->goal;
					return true;
				}
				// Use last known enemy if no players sighted -- killough 2/15/98:
				if (actor->lastenemy != NULL && actor->lastenemy->health > 0)
				{
					if (!actor->IsFriend(actor->lastenemy))
					{
						actor->target = actor->lastenemy;
						actor->lastenemy = NULL;
						return true;
					}
					else
					{
						actor->lastenemy = NULL;
					}
				}
			}
			return actor->target == actor->goal && actor->goal != NULL;
		}

		player = &players[pnum];

		if (!(player->mo->flags & MF_SHOOTABLE))
			continue;			// not shootable (observer or dead)

		if (player->cheats & CF_NOTARGET)
			continue;			// no target

		if (player->health <= 0)
			continue;			// dead

		if (!P_CheckSight (actor, player->mo, 2))
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
		if ((player->mo->flags & MF_SHADOW && !(i_compatflags & COMPATF_INVISIBILITY)) ||
			player->mo->flags3 & MF3_GHOST)
		{
			if ((P_AproxDistance (player->mo->x - actor->x,
					player->mo->y - actor->y) > 2*MELEERANGE)
				&& P_AproxDistance (player->mo->momx, player->mo->momy)
				< 5*FRACUNIT)
			{ // Player is sneaking - can't detect
				return false;
			}
			if (pr_lookforplayers() < 225)
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
		NActorIterator iterator (NAME_PatrolPoint, actor->args[1]);
		actor->special = 0;
		actor->goal = iterator.Next ();
		actor->reactiontime = actor->args[2] * TICRATE + level.maptime;
		if (actor->args[3] == 0) actor->flags5 &=~ MF5_CHASEGOAL;
		else actor->flags5 |= MF5_CHASEGOAL;
	}

	actor->threshold = 0;		// any shot will wake up

	if (actor->TIDtoHate != 0)
	{
		targ = actor->target;
	}
	else
	{
		targ = (i_compatflags & COMPATF_SOUNDTARGET || actor->flags & MF_NOSECTOR)? 
			actor->Sector->SoundTarget : actor->LastHeard;

		// [RH] If the soundtarget is dead, don't chase it
		if (targ != NULL && targ->health <= 0)
		{
			targ = NULL;
		}

		if (targ && targ->player && (targ->player->cheats & CF_NOTARGET))
		{
			return;
		}
	}

	// [RH] Andy Baker's stealth monsters
	if (actor->flags & MF_STEALTH)
	{
		actor->visdir = -1;
	}

	if (targ && (targ->flags & MF_SHOOTABLE))
	{
		if (actor->IsFriend (targ))	// be a little more precise!
		{
			// If we find a valid target here, the wandering logic should *not*
			// be activated! If would cause the seestate to be set twice.
			if (P_LookForPlayers (actor, actor->flags4 & MF4_LOOKALLAROUND))
				goto seeyou;

			// Let the actor wander around aimlessly looking for a fight
			if (actor->SeeState != NULL)
			{
				if (!(actor->flags & MF_INCHASE))
				{
					actor->SetState (actor->SeeState);
				}
			}
			else
			{
				A_Wander (actor);
			}
		}
		else
		{
			actor->target = targ;

			if (actor->flags & MF_AMBUSH)
			{
				if (P_CheckSight (actor, actor->target, 2))
					goto seeyou;
			}
			else
				goto seeyou;
		}
	}
		
		
	if (!P_LookForPlayers (actor, actor->flags4 & MF4_LOOKALLAROUND))
		return;
				
	// go into chase state
  seeyou:
	// [RH] Don't start chasing after a goal if it isn't time yet.
	if (actor->target == actor->goal)
	{
		if (actor->reactiontime > level.maptime)
			actor->target = NULL;
	}
	else if (actor->SeeSound)
	{
		if (actor->flags2 & MF2_BOSS)
		{ // full volume
			S_SoundID (actor, CHAN_VOICE, actor->SeeSound, 1, ATTN_SURROUND);
		}
		else
		{
			S_SoundID (actor, CHAN_VOICE, actor->SeeSound, 1, ATTN_NORM);
		}
	}

	if (actor->target && !(actor->flags & MF_INCHASE))
	{
		actor->SetState (actor->SeeState);
	}
}


//==========================================================================
//
// A_Wander
//
//==========================================================================
void A_Wander (AActor *self)
{
	int delta;

	// [RH] Strife probably clears this flag somewhere, but I couldn't find where.
	// This seems as good a place as any.
	self->flags4 &= ~MF4_INCOMBAT;

	if (self->flags4 & MF4_STANDSTILL)
		return;

	if (self->threshold != 0)
	{
		self->threshold--;
		return;
	}

	// turn towards movement direction if not there yet
	if (self->movedir < DI_NODIR)
	{
		self->angle &= (angle_t)(7<<29);
		delta = self->angle - (self->movedir << 29);
		if (delta > 0)
		{
			self->angle -= ANG90/2;
		}
		else if (delta < 0)
		{
			self->angle += ANG90/2;
		}
	}

	if (--self->movecount < 0 || !P_Move (self))
	{
		P_RandomChaseDir (self);
		self->movecount += 5;
	}
}


//==========================================================================
//
// A_Look2
//
//==========================================================================
void A_Look2 (AActor *self)
{
	AActor *targ;

	self->threshold = 0;
	targ = self->LastHeard;

	if (targ != NULL && targ->health <= 0)
	{
		targ = NULL;
	}

	if (targ && (targ->flags & MF_SHOOTABLE))
	{
		if ((level.flags & LEVEL_NOALLIES) ||
			(self->flags & MF_FRIENDLY) != (targ->flags & MF_FRIENDLY))
		{
			if (self->flags & MF_AMBUSH)
			{
				if (!P_CheckSight (self, targ, 2))
					goto nosee;
			}
			self->target = targ;
			self->threshold = 10;
			self->SetState (self->SeeState);
			return;
		}
		else
		{
			if (!P_LookForPlayers (self, self->flags4 & MF4_LOOKALLAROUND))
				goto nosee;
			self->SetState (self->SeeState);
			self->flags4 |= MF4_INCOMBAT;
			return;
		}
	}
nosee:
	if (pr_look2() < 30)
	{
		self->SetState (self->SpawnState + (pr_look2() & 1) + 1);
	}
	if (!(self->flags4 & MF4_STANDSTILL) && pr_look2() < 40)
	{
		self->SetState (self->SpawnState + 3);
	}
}

//=============================================================================
//
// A_Chase
//
// Actor has a melee attack, so it tries to close as fast as possible
//
// [GrafZahl] integrated A_FastChase, A_SerpentChase and A_SerpentWalk into this
// to allow the monsters using those functions to take advantage of the
// enhancements.
//
//=============================================================================
#define CLASS_BOSS_STRAFE_RANGE	64*10*FRACUNIT

void A_DoChase (AActor *actor, bool fastchase, FState *meleestate, FState *missilestate, bool playactive, bool nightmarefast, bool dontmove)
{
	int delta;

	actor->flags |= MF_INCHASE;

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
	if (actor->target != NULL &&
		actor->target->renderflags & RF_INVISIBLE &&
		actor->target != actor->goal)
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

	if (nightmarefast && G_SkillProperty(SKILLP_FastMonsters))
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

	// [RH] If the target is dead or a friend (and not a goal), stop chasing it.
	if (actor->target && actor->target != actor->goal && (actor->target->health <= 0 || actor->IsFriend(actor->target)))
		actor->target = NULL;

	// [RH] Friendly monsters will consider chasing whoever hurts a player if they
	// don't already have a target.
	if (actor->flags & MF_FRIENDLY && actor->target == NULL)
	{
		player_t *player;

		if (actor->FriendPlayer != 0)
		{
			player = &players[actor->FriendPlayer - 1];
		}
		else
		{
			int i;
			if (!multiplayer)
			{
				i = 0;
			}
			else for (i = pr_newchasedir() & (MAXPLAYERS-1); !playeringame[i]; i = (i+1) & (MAXPLAYERS-1))
			{
			}
			player = &players[i];
		}
		if (player->attacker && player->attacker->health > 0 && player->attacker->flags & MF_SHOOTABLE && pr_newchasedir() < 80)
		{
			if (!(player->attacker->flags & MF_FRIENDLY) ||
				(deathmatch && actor->FriendPlayer != 0 && player->attacker->FriendPlayer != 0 &&
				actor->FriendPlayer != player->attacker->FriendPlayer))
			{
				actor->target = player->attacker;
			} 
		}
	}
	if (!actor->target || !(actor->target->flags & MF_SHOOTABLE))
	{ // look for a new target
		if (P_LookForPlayers (actor, true) && actor->target != actor->goal)
		{ // got a new target
			actor->flags &= ~MF_INCHASE;
			return;
		}
		if (actor->target == NULL)
		{
			if (actor->flags & MF_FRIENDLY)
			{
				A_Look (actor);
				if (actor->target == NULL)
				{
					if (!dontmove) A_Wander (actor);
					actor->flags &= ~MF_INCHASE;
					return;
				}
			}
			else
			{
				actor->SetState (actor->SpawnState);
				actor->flags &= ~MF_INCHASE;
				return;
			}
		}
	}
	
	// do not attack twice in a row
	if (actor->flags & MF_JUSTATTACKED)
	{
		actor->flags &= ~MF_JUSTATTACKED;
		if (!actor->isFast())
		{
			P_NewChaseDir (actor);
		}
		actor->flags &= ~MF_INCHASE;
		return;
	}
	
	// [RH] Don't attack if just moving toward goal
	if (actor->target == actor->goal || (actor->flags5&MF5_CHASEGOAL && actor->goal != NULL))
	{
		AActor * savedtarget = actor->target;
		actor->target = actor->goal;
		bool result = actor->CheckMeleeRange();
		actor->target = savedtarget;

		if (result)
		{
			// reached the goal
			NActorIterator iterator (NAME_PatrolPoint, actor->goal->args[0]);
			NActorIterator specit (NAME_PatrolSpecial, actor->goal->tid);
			AActor *spec;

			// Execute the specials of any PatrolSpecials with the same TID
			// as the goal.
			while ( (spec = specit.Next()) )
			{
				LineSpecials[spec->special] (NULL, actor, false, spec->args[0],
					spec->args[1], spec->args[2], spec->args[3], spec->args[4]);
			}

			angle_t lastgoalang = actor->goal->angle;
			int delay;
			AActor * newgoal = iterator.Next ();
			if (newgoal != NULL && actor->goal == actor->target)
			{
				delay = newgoal->args[1];
				actor->reactiontime = delay * TICRATE + level.maptime;
			}
			else
			{
				delay = 0;
				actor->reactiontime = actor->GetDefault()->reactiontime;
				actor->angle = lastgoalang;		// Look in direction of last goal
			}
			if (actor->target == actor->goal) actor->target = NULL;
			actor->flags |= MF_JUSTATTACKED;
			if (newgoal != NULL && delay != 0)
			{
				actor->flags4 |= MF4_INCOMBAT;
				actor->SetState (actor->SpawnState);
			}
			actor->flags &= ~MF_INCHASE;
			actor->goal = newgoal;
			return;
		}
		if (actor->goal == actor->target) goto nomissile;
	}

	// Strafe	(Hexen's class bosses)
	// This was the sole reason for the separate A_FastChase function but
	// it can be just as easily handled by a simple flag so the monsters
	// can take advantage of all the other enhancements of A_Chase.

	if (fastchase && !dontmove)
	{
		if (actor->FastChaseStrafeCount > 0)
		{
			actor->FastChaseStrafeCount--;
		}
		else
		{
			actor->FastChaseStrafeCount = 0;
			actor->momx = 0;
			actor->momy = 0;
			fixed_t dist = P_AproxDistance (actor->x - actor->target->x, actor->y - actor->target->y);
			if (dist < CLASS_BOSS_STRAFE_RANGE)
			{
				if (pr_chase() < 100)
				{
					angle_t ang = R_PointToAngle2(actor->x, actor->y, actor->target->x, actor->target->y);
					if (pr_chase() < 128) ang += ANGLE_90;
					else ang -= ANGLE_90;
					actor->momx = 13 * finecosine[ang>>ANGLETOFINESHIFT];
					actor->momy = 13 * finesine[ang>>ANGLETOFINESHIFT];
					actor->FastChaseStrafeCount = 3;		// strafe time
				}
			}
		}

	}

	// [RH] Scared monsters attack less frequently
	if (((actor->target->player == NULL ||
		!(actor->target->player->cheats & CF_FRIGHTENING)) &&
		!(actor->flags4 & MF4_FRIGHTENED)) ||
		pr_scaredycat() < 43)
	{
		// check for melee attack
		if (meleestate && actor->CheckMeleeRange ())
		{
			if (actor->AttackSound)
				S_SoundID (actor, CHAN_WEAPON, actor->AttackSound, 1, ATTN_NORM);

			actor->SetState (meleestate);
			actor->flags &= ~MF_INCHASE;
			return;
		}
		
		// check for missile attack
		if (missilestate)
		{
			if (!actor->isFast() && actor->movecount)
			{
				goto nomissile;
			}
			
			if (!P_CheckMissileRange (actor))
				goto nomissile;
			
			actor->SetState (missilestate);
			actor->flags |= MF_JUSTATTACKED;
			actor->flags4 |= MF4_INCOMBAT;
			actor->flags &= ~MF_INCHASE;
			return;
		}
	}

 nomissile:
	// possibly choose another target
	if ((multiplayer || actor->TIDtoHate)
		&& !actor->threshold
		&& !P_CheckSight (actor, actor->target, 0) )
	{
		bool lookForBetter = false;
		bool gotNew;
		if (actor->flags3 & MF3_NOSIGHTCHECK)
		{
			actor->flags3 &= ~MF3_NOSIGHTCHECK;
			lookForBetter = true;
		}
		AActor * oldtarget = actor->target;
		gotNew = P_LookForPlayers (actor, true);
		if (lookForBetter)
		{
			actor->flags3 |= MF3_NOSIGHTCHECK;
		}
		if (gotNew && actor->target != oldtarget)
		{
			actor->flags &= ~MF_INCHASE;
			return; 	// got a new target
		}
	}

	//
	// chase towards player
	//

	// class bosses don't do this when strafing
	if ((!fastchase || !actor->FastChaseStrafeCount) && !dontmove)
	{
		// CANTLEAVEFLOORPIC handling was completely missing in the non-serpent functions.
		fixed_t oldX = actor->x;
		fixed_t oldY = actor->y;
		int oldFloor = actor->floorpic;

		// chase towards player
		if (--actor->movecount < 0 || !P_Move (actor))
		{
			P_NewChaseDir (actor);
		}
		
		// if the move was illegal, reset it 
		// (copied from A_SerpentChase - it applies to everything with CANTLEAVEFLOORPIC!)
		if (actor->flags2&MF2_CANTLEAVEFLOORPIC && actor->floorpic != oldFloor )
		{
			if (P_TryMove (actor, oldX, oldY, false))
			{
				if (nomonsterinterpolation)
				{
					actor->PrevX = oldX;
					actor->PrevY = oldY;
				}
			}
			P_NewChaseDir (actor);
		}
	}
	else if (dontmove && actor->movecount > 0) actor->movecount--;
	
	// make active sound
	if (playactive && pr_chase() < 3)
	{
		actor->PlayActiveSound ();
	}

	actor->flags &= ~MF_INCHASE;
}


//==========================================================================
//
// PIT_VileCheck
//
//==========================================================================

static AActor *corpsehit;
static AActor *vileobj;
static fixed_t viletryx;
static fixed_t viletryy;
static FState *raisestate;

static bool PIT_VileCheck (AActor *thing)
{
	int maxdist;
	bool check;
		
	if (!(thing->flags & MF_CORPSE) )
		return true;	// not a monster
	
	if (thing->tics != -1)
		return true;	// not lying still yet
	
	raisestate = thing->FindState(NAME_Raise);
	if (raisestate == NULL)
		return true;	// monster doesn't have a raise state
	
  	// This may be a potential problem if this is used by something other
	// than an Arch Vile.	
	//maxdist = thing->GetDefault()->radius + GetDefault<AArchvile>()->radius;
	
	// use the current actor's radius instead of the Arch Vile's default.
	maxdist = thing->GetDefault()->radius + vileobj->radius; 
		
	if ( abs(thing->x - viletryx) > maxdist
		 || abs(thing->y - viletryy) > maxdist )
		return true;			// not actually touching
				
	corpsehit = thing;
	corpsehit->momx = corpsehit->momy = 0;
	// [RH] Check against real height and radius

	fixed_t oldheight = corpsehit->height;
	fixed_t oldradius = corpsehit->radius;
	int oldflags = corpsehit->flags;

	corpsehit->flags |= MF_SOLID;
	corpsehit->height = corpsehit->GetDefault()->height;
	check = P_CheckPosition (corpsehit, corpsehit->x, corpsehit->y);
	corpsehit->flags = oldflags;
	corpsehit->radius = oldradius;
	corpsehit->height = oldheight;

	return !check;
}



//==========================================================================
//
// P_CheckForResurrection (formerly part of A_VileChase)
// Check for ressurecting a body
//
//==========================================================================

static bool P_CheckForResurrection(AActor *self, bool usevilestates)
{
	static TArray<AActor *> vilebt;
	int xl, xh, yl, yh;
	int bx, by;

	const AActor *info;
	AActor *temp;
		
	if (self->movedir != DI_NODIR)
	{
		const fixed_t absSpeed = abs (self->Speed);

		// check for corpses to raise
		viletryx = self->x + FixedMul (absSpeed, xspeed[self->movedir]);
		viletryy = self->y + FixedMul (absSpeed, yspeed[self->movedir]);

		xl = (viletryx - bmaporgx - MAXRADIUS*2)>>MAPBLOCKSHIFT;
		xh = (viletryx - bmaporgx + MAXRADIUS*2)>>MAPBLOCKSHIFT;
		yl = (viletryy - bmaporgy - MAXRADIUS*2)>>MAPBLOCKSHIFT;
		yh = (viletryy - bmaporgy + MAXRADIUS*2)>>MAPBLOCKSHIFT;
		
		vileobj = self;
		validcount++;
		vilebt.Clear();

		for (bx = xl; bx <= xh; bx++)
		{
			for (by = yl; by <= yh; by++)
			{
				// Call PIT_VileCheck to check
				// whether object is a corpse
				// that can be raised.
				if (!P_BlockThingsIterator (bx, by, PIT_VileCheck, vilebt))
				{
					// got one!
					temp = self->target;
					self->target = corpsehit;
					A_FaceTarget (self);
					if (self->flags & MF_FRIENDLY)
					{
						// If this is a friendly Arch-Vile (which is turning the resurrected monster into its friend)
						// and the Arch-Vile is currently targetting the resurrected monster the target must be cleared.
						if (self->lastenemy == temp) self->lastenemy = NULL;
						if (temp == self->target) temp = NULL;
						
					}
					self->target = temp;
										
					// Make the state the monster enters customizable.
					FState * state = self->FindState(NAME_Heal);
					if (state != NULL)
					{
						self->SetState (state);
					}
					else if (usevilestates)
					{
						// For Dehacked compatibility this has to use the Arch Vile's
						// heal state as a default if the actor doesn't define one itself.
						const PClass *archvile = PClass::FindClass("Archvile");
						if (archvile != NULL)
						{
							self->SetState (archvile->ActorInfo->FindState(NAME_Heal));
						}
					}
					S_Sound (corpsehit, CHAN_BODY, "vile/raise", 1, ATTN_IDLE);
					info = corpsehit->GetDefault ();
					
					corpsehit->SetState (raisestate);
					corpsehit->height = info->height;	// [RH] Use real mobj height
					corpsehit->radius = info->radius;	// [RH] Use real radius
					/*
					// Make raised corpses look ghostly
					if (corpsehit->alpha > TRANSLUC50)
						corpsehit->alpha /= 2;
					*/
					corpsehit->flags = info->flags;
					corpsehit->flags2 = info->flags2;
					corpsehit->flags3 = info->flags3;
					corpsehit->flags4 = info->flags4;
					corpsehit->health = info->health;
					corpsehit->target = NULL;
					corpsehit->lastenemy = NULL;

					// [RH] If it's a monster, it gets to count as another kill
					if (corpsehit->CountsAsKill())
					{
						level.total_monsters++;
					}

					// You are the Archvile's minion now, so hate what it hates
					corpsehit->CopyFriendliness (self, false);


					return true;
				}
			}
		}
	}
	return false;
}

//==========================================================================
//
// A_Chase and variations
//
//==========================================================================

enum ChaseFlags
{
	CHF_FASTCHASE = 1,
	CHF_NOPLAYACTIVE = 2,
	CHF_NIGHTMAREFAST = 4,
	CHF_RESURRECT = 8,
	CHF_DONTMOVE = 16,
};

void A_Chase (AActor *actor)
{
	int index=CheckIndex(3, &CallingState);
	if (index>=0)
	{
		int flags = EvalExpressionI (StateParameters[index+2], actor);
		if (flags & CHF_RESURRECT && P_CheckForResurrection(actor, false)) return;

		FState *melee = P_GetState(actor, CallingState, StateParameters[index]);
		FState *missile = P_GetState(actor, CallingState, StateParameters[index+1]);
		
		A_DoChase(actor, !!(flags&CHF_FASTCHASE), melee, missile, !(flags&CHF_NOPLAYACTIVE), 
					!!(flags&CHF_NIGHTMAREFAST), !!(flags&CHF_DONTMOVE));
	}
	else // this is the old default A_Chase
	{
		A_DoChase (actor, false, actor->MeleeState, actor->MissileState, true, !!(gameinfo.gametype & GAME_Raven), false);
	}
}

void A_FastChase (AActor *actor)
{
	A_DoChase (actor, true, actor->MeleeState, actor->MissileState, true, true, false);
}

void A_VileChase (AActor *actor)
{
	if (!P_CheckForResurrection(actor, true))
		A_DoChase (actor, false, actor->MeleeState, actor->MissileState, true, !!(gameinfo.gametype & GAME_Raven), false);
}

void A_ExtChase(AActor * self)
{
	// Now that A_Chase can handle state label parameters, this function has become rather useless...
	int index=CheckIndex(4, &CallingState);
	if (index<0) return;

	A_DoChase(self, false,
		EvalExpressionI (StateParameters[index], self) ? self->MeleeState:NULL,
		EvalExpressionI (StateParameters[index+1], self) ? self->MissileState:NULL,
		EvalExpressionN (StateParameters[index+2], self),
		!!EvalExpressionI (StateParameters[index+3], self), false);
}

//=============================================================================
//
// A_FaceTarget
//
//=============================================================================
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
		actor->angle += pr_facetarget.Random2() << 21;
    }
}


//===========================================================================
//
// [RH] A_MonsterRail
//
// New function to let monsters shoot a railgun
//
//===========================================================================
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
		actor->angle += pr_railface.Random2() << 21;
    }

	P_RailAttack (actor, actor->GetMissileDamage (0, 1), 0);
}

void A_Scream (AActor *actor)
{
	if (actor->DeathSound)
	{
		// Check for bosses.
		if (actor->flags2 & MF2_BOSS)
		{
			// full volume
			S_SoundID (actor, CHAN_VOICE, actor->DeathSound, 1, ATTN_SURROUND);
		}
		else
		{
			S_SoundID (actor, CHAN_VOICE, actor->DeathSound, 1, ATTN_NORM);
		}
	}
}

void A_XScream (AActor *actor)
{
	if (actor->player)
		S_Sound (actor, CHAN_VOICE, "*gibbed", 1, ATTN_NORM);
	else
		S_Sound (actor, CHAN_VOICE, "misc/gibbed", 1, ATTN_NORM);
}

// Strife's version of A_XScrem
void A_XXScream (AActor *actor)
{
	if (!(actor->flags & MF_NOBLOOD) || actor->DeathSound == 0)
	{
		A_XScream (actor);
	}
	else
	{
		S_SoundID (actor, CHAN_VOICE, actor->DeathSound, 1, ATTN_NORM);
	}
}

//---------------------------------------------------------------------------
//
// PROC P_DropItem
//
//---------------------------------------------------------------------------
CVAR(Int, sv_dropstyle, 0, CVAR_SERVERINFO | CVAR_ARCHIVE);

AInventory *P_DropItem (AActor *source, const PClass *type, int special, int chance)
{
	if (type != NULL && pr_dropitem() <= chance)
	{
		AActor *mo;
		fixed_t spawnz;

		spawnz = source->z;
		if (!(i_compatflags & COMPATF_NOTOSSDROPS))
		{
			int style = sv_dropstyle;
			if (style==0) style= (gameinfo.gametype == GAME_Strife)? 2:1;
			
			if (style==2)
			{
				spawnz += 24*FRACUNIT;
			}
			else
			{
				spawnz += source->height / 2;
			}
		}
		mo = Spawn (type, source->x, source->y, spawnz, ALLOW_REPLACE);
		mo->flags |= MF_DROPPED;
		mo->flags &= ~MF_NOGRAVITY;	// [RH] Make sure it is affected by gravity
		if (mo->IsKindOf (RUNTIME_CLASS(AInventory)))
		{
			AInventory * inv = static_cast<AInventory *>(mo);
			if (special > 0)
			{
				inv->Amount = special;
			}
			else if (mo->IsKindOf (RUNTIME_CLASS(AAmmo)))
			{
				// Half ammo when dropped by bad guys.
				inv->Amount = inv->GetClass()->Meta.GetMetaInt (AIMETA_DropAmount, MAX(1, inv->Amount / 2 ));
			}
			else if (mo->IsKindOf (RUNTIME_CLASS(AWeapon)))
			{
				// The same goes for ammo from a weapon.
				static_cast<AWeapon *>(mo)->AmmoGive1 /= 2;
				static_cast<AWeapon *>(mo)->AmmoGive2 /= 2;
			}
			if (inv->SpecialDropAction (source))
			{
				return NULL;
			}
		}
		if (!(i_compatflags & COMPATF_NOTOSSDROPS))
		{
			P_TossItem (mo);
		}
		return static_cast<AInventory *>(mo);
	}
	return NULL;
}

//============================================================================
//
// P_TossItem
//
//============================================================================

void P_TossItem (AActor *item)
{
	int style = sv_dropstyle;
	if (style==0) style= (gameinfo.gametype == GAME_Strife)? 2:1;
	
	if (style==2)
	{
		item->momx += pr_dropitem.Random2(7) << FRACBITS;
		item->momy += pr_dropitem.Random2(7) << FRACBITS;
	}
	else
	{
		item->momx = pr_dropitem.Random2() << 8;
		item->momy = pr_dropitem.Random2() << 8;
		item->momz = FRACUNIT*5 + (pr_dropitem() << 10);
	}
}

void A_Pain (AActor *actor)
{
	// [RH] Vary player pain sounds depending on health (ala Quake2)
	if (actor->player && actor->player->morphTics == 0)
	{
		const char *pain_amount;
		int sfx_id = 0;

		if (actor->health < 25)
			pain_amount = "*pain25";
		else if (actor->health < 50)
			pain_amount = "*pain50";
		else if (actor->health < 75)
			pain_amount = "*pain75";
		else
			pain_amount = "*pain100";

		// Try for damage-specific sounds first.
		if (actor->player->LastDamageType != NAME_None)
		{
			FString pain_sound = pain_amount;
			pain_sound += '-';
			pain_sound += actor->player->LastDamageType;
			sfx_id = S_FindSound (pain_sound);
			if (sfx_id == 0)
			{
				// Try again without a specific pain amount.
				pain_sound = "*pain-";
				pain_sound += actor->player->LastDamageType;
				sfx_id = S_FindSound (pain_sound);
			}
		}
		if (sfx_id == 0)
		{
			sfx_id = S_FindSound (pain_amount);
		}

		S_SoundID (actor, CHAN_VOICE, sfx_id, 1, ATTN_NORM);
	}
	else if (actor->PainSound)
	{
		S_SoundID (actor, CHAN_VOICE, actor->PainSound, 1, ATTN_NORM);
	}
}

// killough 11/98: kill an object
void A_Die (AActor *actor)
{
	P_DamageMobj (actor, NULL, NULL, actor->health, NAME_None);
}

//
// A_Detonate
// killough 8/9/98: same as A_Explode, except that the damage is variable
//

void A_Detonate (AActor *mo)
{
	int damage = mo->GetMissileDamage (0, 1);
	P_RadiusAttack (mo, mo->target, damage, damage, mo->DamageType, true);
	if (mo->z <= mo->floorz + (damage << FRACBITS))
	{
		P_HitFloor (mo);
	}
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
	P_RadiusAttack (thing, thing->target, damage, distance, thing->DamageType, hurtSource);
	if (thing->z <= thing->floorz + (distance<<FRACBITS))
	{
		P_HitFloor (thing);
	}
}

void A_ExplodeAndAlert (AActor *thing)
{
	A_Explode (thing);
	if (thing->target != NULL && thing->target->player != NULL)
	{
		validcount++;
		P_RecursiveSound (thing->Sector, thing->target, false, 0);
	}
}

bool CheckBossDeath (AActor *actor)
{
	int i;

	// make sure there is a player alive for victory
	for (i = 0; i < MAXPLAYERS; i++)
		if (playeringame[i] && players[i].health > 0)
			break;
	
	if (i == MAXPLAYERS)
		return false; // no one left alive, so do not end game
	
	// Make sure all bosses are dead
	TThinkerIterator<AActor> iterator;
	AActor *other;

	while ( (other = iterator.Next ()) )
	{
		if (other != actor &&
			(other->health > 0 || (other->flags & MF_ICECORPSE))
			&& other->GetClass() == actor->GetClass())
		{ // Found a living boss
		  // [RH] Frozen bosses don't count as dead until they shatter
			return false;
		}
	}
	// The boss death is good
	return true;
}

//
// A_BossDeath
// Possibly trigger special effects if on a boss level
//
void A_BossDeath (AActor *actor)
{
	FName mytype = actor->GetClass()->TypeName;

	// Ugh...
	FName type = actor->GetClass()->ActorInfo->GetReplacee()->Class->TypeName;
	
	// Do generic special death actions first
	bool checked = false;
	FSpecialAction *sa = level.info->specialactions;
	while (sa)
	{
		if (type == sa->Type || mytype == sa->Type)
		{
			if (!checked && !CheckBossDeath(actor))
			{
				return;
			}
			checked = true;

			LineSpecials[sa->Action](NULL, actor, false, 
				sa->Args[0], sa->Args[1], sa->Args[2], sa->Args[3], sa->Args[4]);
		}
		sa = sa->Next;
	}

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

	if (
		((level.flags & LEVEL_MAP07SPECIAL) && (type == NAME_Fatso || type == NAME_Arachnotron)) ||
		((level.flags & LEVEL_BRUISERSPECIAL) && (type == NAME_BaronOfHell)) ||
		((level.flags & LEVEL_CYBORGSPECIAL) && (type == NAME_Cyberdemon)) ||
		((level.flags & LEVEL_SPIDERSPECIAL) && (type == NAME_SpiderMastermind)) ||
		((level.flags & LEVEL_HEADSPECIAL) && (type == NAME_Ironlich)) ||
		((level.flags & LEVEL_MINOTAURSPECIAL) && (type == NAME_Minotaur)) ||
		((level.flags & LEVEL_SORCERER2SPECIAL) && (type == NAME_Sorcerer2))
	   )
		;
	else
		return;

	if (!CheckBossDeath (actor))
	{
		return;
	}

	// victory!
	if (level.flags & LEVEL_SPECKILLMONSTERS)
	{ // Kill any remaining monsters
		P_Massacre ();
	}
	if (level.flags & LEVEL_MAP07SPECIAL)
	{
		if (type == NAME_Fatso)
		{
			EV_DoFloor (DFloor::floorLowerToLowest, NULL, 666, FRACUNIT, 0, 0, 0);
			return;
		}
		
		if (type == NAME_Arachnotron)
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
			EV_DoDoor (DDoor::doorOpen, NULL, NULL, 666, 8*FRACUNIT, 0, 0, 0);
			return;
		}
	}

	// [RH] If noexit, then don't end the level.
	if ((deathmatch || alwaysapplydmflags) && (dmflags & DF_NO_EXIT))
		return;

	G_ExitLevel (0, false);
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
		if (!(actor->flags2 & MF2_DORMANT) && (actor->flags3 & MF3_ISMONSTER))
		{
			killcount += actor->Massacre();
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

void A_ClassBossHealth (AActor *actor)
{
	if (multiplayer && !deathmatch)		// co-op only
	{
		if (!actor->special1)
		{
			actor->health *= 5;
			actor->special1 = true;   // has been initialized
		}
	}
}
