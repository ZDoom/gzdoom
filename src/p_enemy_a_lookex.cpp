#include <stdlib.h>

#include "templates.h"
#include "m_random.h"
#include "i_system.h"
#include "doomdef.h"
#include "p_local.h"
#include "p_lnspec.h"
#include "s_sound.h"
#include "g_game.h"
#include "doomstat.h"
#include "r_state.h"
#include "c_cvars.h"
#include "p_enemy.h"
#include "a_sharedglobal.h"
#include "a_action.h"
#include "thingdef/thingdef.h"
#include "g_level.h"

#include "gi.h"

bool P_LookForMonsters (AActor *actor);

static FRandom pr_look2 ("LookyLookyEx");
static FRandom pr_look3 ("IGotHookyEx");
static FRandom pr_lookforplayers ("LookForPlayersEx");
static FRandom pr_skiptarget("SkipTargetEx");


// [KS] *** Start additions by me - p_enemy.cpp ***

// LookForTIDinBlockEx
// LookForEnemiesInBlockEx
// P_NewLookTID (copied from P_LookForTID)
// P_NewLookPlayers (copied from P_LookForPlayers)
// Takes FOV and sight distances into account when acquiring a target.
// TODO: Not sure if using actor properties to pass parameters around indirectly is such a good idea. If there's a cleaner method, do it that way instead.
//  ~Kate S.  Last updated on 12/23/2007

AActor *LookForTIDinBlockEx (AActor *lookee, int index)
{
	FBlockNode *block;
	AActor *link;
	AActor *other;
	fixed_t 	dist;
	angle_t		an;
	
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

			dist = P_AproxDistance (other->x - lookee->x,
									other->y - lookee->y);

			if (lookee->LookExMaxDist && dist > lookee->LookExMaxDist)
				continue;			// [KS] too far

			if (lookee->LookExMinDist && dist < lookee->LookExMinDist)
				continue;			// [KS] too close

			if (lookee->LookExFOV && lookee->LookExFOV < ANGLE_MAX)
			{
				an = R_PointToAngle2 (lookee->x,
									  lookee->y, 
									  other->x,
									  other->y)
					- lookee->angle;

				if (an > (lookee->LookExFOV / 2) && an < (ANGLE_MAX - (lookee->LookExFOV / 2)))
				{
					// if real close, react anyway
					// [KS] but respect minimum distance rules
					if (lookee->LookExMinDist || dist > MELEERANGE)
						continue;	// outside of fov
				}
			}
			else
			{
				if(!(lookee->flags4 & MF4_LOOKALLAROUND) || (lookee->LookExFOV >= ANGLE_MAX)) // Just in case angle_t doesn't clamp stuff, because I can't be too sure.
				{
					an = R_PointToAngle2 (lookee->x,
										  lookee->y, 
										  other->x,
										  other->y)
						- lookee->angle;

					if (an > ANG90 && an < ANG270)
					{
						// if real close, react anyway
						// [KS] but respect minimum distance rules
						if (lookee->LookExMinDist || dist > MELEERANGE)
							continue;	// behind back
					}
				}
			}
		}

		return other;
	}
	return NULL;
}

AActor *LookForEnemiesInBlockEx (AActor *lookee, int index)
{
	FBlockNode *block;
	AActor *link;
	AActor *other;
	fixed_t 	dist;
	angle_t		an;
	
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
				targ->health*2 >= targ->SpawnHealth())
			{
				continue;
			}
		}

		// [KS] Hey, shouldn't there be a check for MF3_NOSIGHTCHECK here?

		if (other == NULL || !P_CheckSight (lookee, other, 2))
			continue;			// out of sight

		dist = P_AproxDistance (other->x - lookee->x,
								other->y - lookee->y);

		if (lookee->LookExMaxDist && dist > lookee->LookExMaxDist)
			continue;			// [KS] too far

		if (lookee->LookExMinDist && dist < lookee->LookExMinDist)
			continue;			// [KS] too close

		if (lookee->LookExFOV && lookee->LookExFOV < ANGLE_MAX)
		{
			an = R_PointToAngle2 (lookee->x,
								  lookee->y, 
								  other->x,
								  other->y)
				- lookee->angle;

			if (an > (lookee->LookExFOV / 2) && an < (ANGLE_MAX - (lookee->LookExFOV / 2)))
			{
				// if real close, react anyway
				// [KS] but respect minimum distance rules
				if (lookee->LookExMinDist || dist > MELEERANGE)
					continue;	// outside of fov
			}
		}
		else
		{
			if(!(lookee->flags4 & MF4_LOOKALLAROUND) || (lookee->LookExFOV >= ANGLE_MAX)) // Just in case angle_t doesn't clamp stuff, because I can't be too sure.
			{
				an = R_PointToAngle2 (lookee->x,
									  lookee->y, 
									  other->x,
									  other->y)
					- lookee->angle;

				if (an > ANG90 && an < ANG270)
				{
					// if real close, react anyway
					// [KS] but respect minimum distance rules
					if (lookee->LookExMinDist || dist > MELEERANGE)
						continue;	// behind back
				}
			}
		}

		return other;
	}
	return NULL;
}

bool P_NewLookTID (AActor *actor, angle_t fov, fixed_t mindist, fixed_t maxdist, bool chasegoal)
{
	AActor *other;

	actor->LookExMinDist = mindist;
	actor->LookExMaxDist = maxdist;
	actor->LookExFOV = fov;
	other = P_BlockmapSearch (actor, 0, LookForTIDinBlockEx);

	bool reachedend = false;
	fixed_t 	dist;
	angle_t		an;

	if (other != NULL)
	{
		if (!(actor->flags3 & MF3_NOSIGHTCHECK))
		{
			dist = P_AproxDistance (other->x - actor->x,
									other->y - actor->y);

			if (maxdist && dist > maxdist)
			{
				other = NULL;			// [KS] too far
				goto endcheck;
			}

			if (mindist && dist < mindist)
			{
				other = NULL;			// [KS] too close
				goto endcheck;
			}

			if (fov && fov < ANGLE_MAX)
			{
				an = R_PointToAngle2 (actor->x,
									  actor->y, 
									  other->x,
									  other->y)
					- actor->angle;

				if (an > (fov / 2) && an < (ANGLE_MAX - (fov / 2)))
				{
					// if real close, react anyway
					// [KS] but respect minimum distance rules
					if (mindist || dist > MELEERANGE)
					{
						other = NULL;	// outside of fov
						goto endcheck;
					}
				}
			}
			else
			{
				if(!((actor->flags4 & MF4_LOOKALLAROUND) || (fov >= ANGLE_MAX))) // Just in case angle_t doesn't clamp stuff, because I can't be too sure.
				{
					an = R_PointToAngle2 (actor->x,
										  actor->y, 
										  other->x,
										  other->y)
						- actor->angle;

					if (an > ANG90 && an < ANG270)
					{
						// if real close, react anyway
						// [KS] but respect minimum distance rules
						if (mindist || dist > MELEERANGE)
						{
							other = NULL;	// behind back
							goto endcheck;
						}
					}
				}
			}
		}
	}

  endcheck:

	if (other != NULL)
	{
		if (actor->goal && actor->target == actor->goal)
			actor->reactiontime = 0;

		actor->target = other;
		actor->LastLookActor = other;
		return true;
	}

	// The actor's TID could change because of death or because of
	// Thing_ChangeTID. If it's not what we expect, then don't use
	// it as a base for the iterator.
	if (actor->LastLookActor != NULL &&
		actor->LastLookActor->tid != actor->TIDtoHate)
	{
		actor->LastLookActor = NULL;
	}

	FActorIterator iterator (actor->TIDtoHate, actor->LastLookActor);
	int c = (pr_look3() & 31) + 7;	// Look for between 7 and 38 hatees at a time
	while ((other = iterator.Next()) != actor->LastLookActor)
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

			dist = P_AproxDistance (other->x - actor->x,
									other->y - actor->y);

			if (maxdist && dist > maxdist)
				continue;			// [KS] too far

			if (mindist && dist < mindist)
				continue;			// [KS] too close

			if (fov && fov < ANGLE_MAX)
			{
				an = R_PointToAngle2 (actor->x,
									  actor->y, 
									  other->x,
									  other->y)
					- actor->angle;

				if (an > (fov / 2) && an < (ANGLE_MAX - (fov / 2)))
				{
					// if real close, react anyway
					// [KS] but respect minimum distance rules
					if (mindist || dist > MELEERANGE)
						continue;	// outside of fov
				}
			}
			else
			{
				if(!((actor->flags4 & MF4_LOOKALLAROUND) || (fov >= ANGLE_MAX))) // Just in case angle_t doesn't clamp stuff, because I can't be too sure.
				{
					an = R_PointToAngle2 (actor->x,
										  actor->y, 
										  other->x,
										  other->y)
						- actor->angle;

					if (an > ANG90 && an < ANG270)
					{
						// if real close, react anyway
						// [KS] but respect minimum distance rules
						if (mindist || dist > MELEERANGE)
							continue;	// behind back
					}
				}
			}
		}
		
		// [RH] Need to be sure the reactiontime is 0 if the monster is
		//		leaving its goal to go after something else.
		if (actor->goal && actor->target == actor->goal)
			actor->reactiontime = 0;

		actor->target = other;
		actor->LastLookActor = other;
		return true;
	}
	actor->LastLookActor = other;
	if (actor->target == NULL)
	{
		// [RH] use goal as target
		if (actor->goal != NULL && chasegoal)
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

bool P_NewLookEnemies (AActor *actor, angle_t fov, fixed_t mindist, fixed_t maxdist, bool chasegoal)
{
	AActor *other;

	actor->LookExMinDist = mindist;
	actor->LookExMaxDist = maxdist;
	actor->LookExFOV = fov;
	other = P_BlockmapSearch (actor, 10, LookForEnemiesInBlockEx);

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

bool P_NewLookPlayers (AActor *actor, angle_t fov, fixed_t mindist, fixed_t maxdist, bool chasegoal)
{
	int 		c;
	int 		stop;
	int			pnum;
	player_t*	player;
	angle_t 	an;
	fixed_t 	dist;

	if (actor->TIDtoHate != 0)
	{
		if (P_NewLookTID (actor, fov, mindist, maxdist, chasegoal))
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
		if (!deathmatch) // [SP] If you don't see any enemies in deathmatch, look for players.
			return P_NewLookEnemies (actor, fov, mindist, maxdist, chasegoal);
		else if ( P_NewLookEnemies (actor, fov, mindist, maxdist, chasegoal) )
			return true;
	}	// [SP] if false, and in deathmatch, intentional fall-through

	if (!(gameinfo.gametype & (GAME_DoomStrifeChex)) &&
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
		pnum = actor->LastLookPlayerNumber;
	}
	stop = (pnum - 1) & (MAXPLAYERS-1);
		
	for (;;)
	{
		pnum = (pnum + 1) & (MAXPLAYERS-1);
		if (!playeringame[pnum])
			continue;

		if (actor->TIDtoHate == 0)
		{
			actor->LastLookPlayerNumber = pnum;
		}

		if (++c == MAXPLAYERS-1 || pnum == stop)
		{
			// done looking
			if (actor->target == NULL)
			{
				// [RH] use goal as target
				// [KS] ...unless we're ignoring goals and we don't already have one
				if (actor->goal != NULL && chasegoal)
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

		dist = P_AproxDistance (player->mo->x - actor->x,
								player->mo->y - actor->y);

		if (maxdist && dist > maxdist)
			continue;			// [KS] too far

		if (mindist && dist < mindist)
			continue;			// [KS] too close

		// [SP] Deathmatch fixes - if we have MF_FRIENDLY we're definitely in deathmatch
		// We're going to ignore our master, but go after his enemies.
		if ( actor->flags & MF_FRIENDLY )
		{
			if ( actor->FriendPlayer == 0 )
				continue; // I have no friends, I will ignore players.
			if ( actor->FriendPlayer == player->mo->FriendPlayer )
				continue; // This is my master.
		}

		if (fov)
		{
			an = R_PointToAngle2 (actor->x,
								  actor->y, 
								  player->mo->x,
								  player->mo->y)
				- actor->angle;

			if (an > (fov / 2) && an < (ANGLE_MAX - (fov / 2)))
			{
				// if real close, react anyway
				// [KS] but respect minimum distance rules
				if (mindist || dist > MELEERANGE)
					continue;	// outside of fov
			}
		}
		else
		{
			if(!((actor->flags4 & MF4_LOOKALLAROUND) || (fov >= ANGLE_MAX)))
			{
				an = R_PointToAngle2 (actor->x,
									  actor->y, 
									  player->mo->x,
									  player->mo->y)
					- actor->angle;

				if (an > ANG90 && an < ANG270)
				{
					// if real close, react anyway
					// [KS] but respect minimum distance rules
					if (mindist || dist > MELEERANGE)
						continue;	// behind back
				}
			}
		}
		if ((player->mo->flags & MF_SHADOW && !(i_compatflags & COMPATF_INVISIBILITY)) ||
			player->mo->flags3 & MF3_GHOST)
		{
			if ((P_AproxDistance (player->mo->x - actor->x,
					player->mo->y - actor->y) > 2*MELEERANGE)
				&& P_AproxDistance (player->mo->velx, player->mo->vely)
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
// A_LookEx (int flags, fixed minseedist, fixed maxseedist, fixed maxheardist, fixed fov, state wakestate)
// [KS] Borrowed the A_Look code to make a parameterized version.
//

enum LO_Flags
{
	LOF_NOSIGHTCHECK = 1,
	LOF_NOSOUNDCHECK = 2,
	LOF_DONTCHASEGOAL = 4,
	LOF_NOSEESOUND = 8,
	LOF_FULLVOLSEESOUND = 16,
};

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_LookEx)
{
	ACTION_PARAM_START(6);
	ACTION_PARAM_INT(flags, 0);
	ACTION_PARAM_FIXED(minseedist, 1);
	ACTION_PARAM_FIXED(maxseedist, 2);
	ACTION_PARAM_FIXED(maxheardist, 3);
	ACTION_PARAM_ANGLE(fov, 4);
	ACTION_PARAM_STATE(seestate, 5);

	AActor *targ = NULL; // Shuts up gcc
	fixed_t dist;

	if (self->flags5 & MF5_INCONVERSATION)
		return;

	// [RH] Set goal now if appropriate
	if (self->special == Thing_SetGoal && self->args[0] == 0) 
	{
		NActorIterator iterator (NAME_PatrolPoint, self->args[1]);
		self->special = 0;
		self->goal = iterator.Next ();
		self->reactiontime = self->args[2] * TICRATE + level.maptime;
		if (self->args[3] == 0) self->flags5 &=~ MF5_CHASEGOAL;
		else self->flags5 |= MF5_CHASEGOAL;
	}

	self->threshold = 0;		// any shot will wake up

	if (self->TIDtoHate != 0)
	{
		targ = self->target;
	}
	else
	{
		if (!(flags & LOF_NOSOUNDCHECK))
		{
			targ = self->LastHeard;
			if (targ != NULL)
			{
				// [RH] If the soundtarget is dead, don't chase it
				if (targ->health <= 0)
				{
					targ = NULL;
				}
				else
				{
					dist = P_AproxDistance (targ->x - self->x,
											targ->y - self->y);

					// [KS] If the target is too far away, don't respond to the sound.
					if (maxheardist && dist > maxheardist)
					{
						targ = NULL;
						self->LastHeard = NULL;
					}
				}
			}
        }
        
        if (targ && targ->player && (targ->player->cheats & CF_NOTARGET))
        {
            return;
        }
	}

	// [RH] Andy Baker's stealth monsters
	if (self->flags & MF_STEALTH)
	{
		self->visdir = -1;
	}

	if (targ && (targ->flags & MF_SHOOTABLE))
	{
		if (self->IsFriend (targ))	// be a little more precise!
		{
			if (!(self->flags4 & MF4_STANDSTILL))
			{
				if (!(flags & LOF_NOSIGHTCHECK))
				{
					// If we find a valid target here, the wandering logic should *not*
					// be activated! If would cause the seestate to be set twice.
					if (P_NewLookPlayers(self, fov, minseedist, maxseedist, !(flags & LOF_DONTCHASEGOAL)))
						goto seeyou;
				}

				// Let the self wander around aimlessly looking for a fight
                if (!(self->flags & MF_INCHASE))
                {
                    if (seestate)
                    {
                        self->SetState (seestate);
                    }
                    else
                    {
                        if (self->SeeState != NULL)
                        {
                            self->SetState (self->SeeState);
                        }
                        else
                        {
                            CALL_ACTION(A_Wander, self);
                        }
                    }
                }
			}
		}
		else
		{
			self->target = targ; //We already have a target?
            
            // [KS] The target can become ourselves in rare circumstances (like
            // if we committed suicide), so if that's the case, just ignore it.
            if (self->target == self) self->target = NULL;

			if (self->target != NULL)
			{
				if (self->flags & MF_AMBUSH)
				{
					dist = P_AproxDistance (self->target->x - self->x,
											self->target->y - self->y);
					if (P_CheckSight (self, self->target, 2) &&
						(!minseedist || dist > minseedist) &&
						(!maxseedist || dist < maxseedist))
					{
						goto seeyou;
					}
				}
				else
					goto seeyou;
			}
		}
	}

	if (!(flags & LOF_NOSIGHTCHECK))
	{
		if (!P_NewLookPlayers(self, fov, minseedist, maxseedist, !(flags & LOF_DONTCHASEGOAL)))
			return;
	}
	else
	{
		return;
	}
				
	// go into chase state
  seeyou:
	// [RH] Don't start chasing after a goal if it isn't time yet.
	if (self->target == self->goal)
	{
		if (self->reactiontime > level.maptime)
			self->target = NULL;
	}
	else if (self->SeeSound && !(flags & LOF_NOSEESOUND))
	{
		if (flags & LOF_FULLVOLSEESOUND)
		{ // full volume
			S_Sound (self, CHAN_VOICE, self->SeeSound, 1, ATTN_NONE);
		}
		else
		{
			S_Sound (self, CHAN_VOICE, self->SeeSound, 1, ATTN_NORM);
		}
	}

	if (self->target && !(self->flags & MF_INCHASE))
	{
		if (seestate)
		{
			self->SetState (seestate);
		}
		else
		{
			self->SetState (self->SeeState);
		}
	}
}

// [KS] *** End additions by me ***
